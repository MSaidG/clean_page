#include "game_server.h"
#include "network_utils.h"

#include <assert.h>
#include <cstdio>
#include <format>
#include <iostream>
#include <stdarg.h>

#include <sstream>
#include <steam/isteamnetworkingmessages.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steam_api_common.h>
#include <steam/steamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>
#include <steam/steamtypes.h>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
SteamNetworkingMicroseconds g_logTimeZero;
}

void GameServer::run()
{
    init();

    // ISteamNetworkingSockets* pInterface = SteamNetworkingSockets();
    m_sockets = SteamNetworkingSockets();

    SteamNetworkingIPAddr server_addr;
    server_addr.Clear();
    server_addr.m_port = m_port;

    SteamNetworkingConfigValue_t options;
    options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)net_connection_status_changed_callback);

    m_listen_socket = m_sockets->CreateListenSocketIP(server_addr, 1, &options);
    if (m_listen_socket == k_HSteamListenSocket_Invalid) {
        fatal_error("Failed to listen on port %d", m_port);
    }

    m_poll_group = m_sockets->CreatePollGroup();
    if (m_poll_group == k_HSteamNetPollGroup_Invalid) {
        fatal_error("Failed to create poll group on port %d", m_port);
    }

    printt("\nServer listening on port %d\n", m_port);

    while (!m_is_quitting) {
        poll_incoming_messages();
        poll_connection_state_changes();
        poll_local_user_input();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    shutdown_server();
}

void GameServer::init()
{
    SteamDatagramErrMsg errMsg;
    if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
        fatal_error("GameNetworkingSockets_Init failed! %s", errMsg);
    }

    g_logTimeZero = SteamNetworkingUtils()->GetLocalTimestamp();
    SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, debug_output);

    local_user_input_init();
}

void GameServer::shutdown_server()
{
    // Step 1: notify clients
    printt("Close connections... \n");
    for (const auto& [conn, client] : m_map_clients) {
        send_message_to_client(conn, "Server is shutting down. Goodbye.");
    }

    // Step 2: wait until reliable messages are delivered (or timeout)
    auto       start   = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(2);
    while (!is_all_reliable_messages_sent(m_sockets, m_map_clients)) {
        // Poll network events to flush messages
        m_sockets->ReceiveMessagesOnPollGroup(m_poll_group, nullptr, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (std::chrono::steady_clock::now() - start > timeout) {
            break; // timed out
        }
    }

    // Step 3: close all connections
    for (const auto& [conn, client] : m_map_clients) {
        m_sockets->CloseConnection(conn, 0, "Server Shutdown", true);
    }
    m_map_clients.clear();

    // Step 4: cleanup listen socket and poll group
    if (m_listen_socket != k_HSteamListenSocket_Invalid) {
        m_sockets->CloseListenSocket(m_listen_socket);
        m_listen_socket = k_HSteamListenSocket_Invalid;
    }

    if (m_poll_group != k_HSteamNetPollGroup_Invalid) {
        m_sockets->DestroyPollGroup(m_poll_group);
        m_poll_group = k_HSteamNetPollGroup_Invalid;
    }

    // Step 5: destroy the library
    GameNetworkingSockets_Kill();
    nuke_process(0);
}

void GameServer::send_message_to_all_clients(const std::string_view msg, HSteamNetConnection except)
{
    for (const auto& [conn, client] : m_map_clients) {
        if (conn != except) {
            std::cout << "nick, msg: " << client.nick << " : " << msg << "\n"; // DEBUG_PRINT
            send_message_to_client(conn, msg);
        }
    }
}

void GameServer::send_message_to_client(HSteamNetConnection conn, const std::string_view msg) noexcept
{
    m_sockets->SendMessageToConnection(
        conn,
        msg.data(),
        static_cast<uint32_t>(msg.size()),
        k_nSteamNetworkingSend_Reliable,
        nullptr);
}

void GameServer::poll_local_user_input()
{
    std::string cmd;

    while (!m_is_quitting && local_user_input_get_next(cmd)) {

        std::string_view input { cmd };

        if (input == "/quit") {
            m_is_quitting = true;
            printt("Shutting down server.");
            break;
        }

        printt("The server only knows one command: '/quit'");
    }
}

void GameServer::poll_incoming_messages()
{
    while (!m_is_quitting) {
        SteamNetworkingMessage_t* msg = nullptr;

        int num_msgs = m_sockets->ReceiveMessagesOnPollGroup(m_poll_group, &msg, 1);

        if (num_msgs == 0)
            break;
        if (num_msgs < 0)
            fatal_error("Error checking for messages!");

        assert(num_msgs == 1 && msg);

        auto it_client = m_map_clients.find(msg->m_conn);
        assert(it_client != m_map_clients.end());

        std::string cmd(reinterpret_cast<const char*>(msg->m_pData), msg->m_cbSize);
        msg->Release();

        std::string outgoing_msg = std::format("{}: {}", it_client->second.nick, cmd);

        std::cout << "user_msg: " << outgoing_msg << "\n"; // DEBUG_PRINT

        send_message_to_all_clients(outgoing_msg, it_client->first);
    }
}

bool GameServer::is_all_reliable_messages_sent(ISteamNetworkingSockets* sockets, const std::unordered_map<HSteamNetConnection, Client>& clients)
{
    for (const auto& [conn, client] : clients) {
        SteamNetConnectionInfo_t info;
        if (!sockets->GetConnectionInfo(conn, &info)) {
            continue; // treat as closed
        }
        if (info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
            return false; // still pending reliable data
        }
    }
    return true;
}

void GameServer::set_client_nick(HSteamNetConnection hConn, std::string_view nick)
{
    // Update the client's nick in the map
    m_map_clients[hConn].nick = std::string(nick);

    // Also set the connection name for debugging
    m_sockets->SetConnectionName(hConn, nick.data());
}

void GameServer::on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* pInfo)
{
    assert(pInfo);
    const auto& info     = pInfo->m_info;
    const auto  oldState = pInfo->m_eOldState;

    switch (info.m_eState) {
    case k_ESteamNetworkingConnectionState_None:
        // Ignore callbacks from destroyed connections
        return;

    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
        // Ignore if the client was not previously connected
        if (oldState != k_ESteamNetworkingConnectionState_Connected) {
            assert(oldState == k_ESteamNetworkingConnectionState_Connecting);
            return;
        }

        auto itClient = m_map_clients.find(pInfo->m_hConn);
        assert(itClient != m_map_clients.end());

        std::string      reasonMessage;
        std::string_view logAction;
        if (info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
            logAction     = "problem detected locally";
            reasonMessage = std::format("Alas, {} hath fallen into shadow.  ({})",
                itClient->second.nick, info.m_szEndDebug);
        } else {
            logAction     = "closed by peer";
            reasonMessage = std::format("{} hath departed", itClient->second.nick);
        }

        std::cout << "Connection " << info.m_szConnectionDescription
                  << " " << logAction
                  << ", reason " << info.m_eEndReason
                  << ": " << info.m_szEndDebug << '\n';

        m_map_clients.erase(itClient);

        // Notify everyone else
        send_message_to_all_clients(reasonMessage);

        // Close locally
        m_sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
        return;
    }

    case k_ESteamNetworkingConnectionState_Connecting: {
        assert(m_map_clients.find(pInfo->m_hConn) == m_map_clients.end());

        std::cout << "Connection request from " << info.m_szConnectionDescription << '\n';

        if (m_sockets->AcceptConnection(pInfo->m_hConn) != k_EResultOK) {
            m_sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
            std::cout << "Can't accept connection. It was already closed?\n";
            return;
        }

        if (!m_sockets->SetConnectionPollGroup(pInfo->m_hConn, m_poll_group)) {
            m_sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
            std::cout << "Failed to set poll group\n";
            return;
        }

        // Generate a temporary nickname
        std::string        nick = "BraveWarrior" + std::to_string(10000 + (rand() % 100000));
        std::ostringstream welcomeMsg;
        welcomeMsg << "Welcome, stranger. Thou art known as '" << nick
                   << "'; use '/nick' to change.";
        send_message_to_client(pInfo->m_hConn, welcomeMsg.str().c_str());

        if (m_map_clients.empty()) {
            send_message_to_client(pInfo->m_hConn, "Thou art utterly alone.");
        } else {
            send_message_to_client(pInfo->m_hConn,
                std::format("{} companions greet you:", m_map_clients.size()).c_str());
            for (const auto& [conn, client] : m_map_clients) {
                send_message_to_client(pInfo->m_hConn, client.nick.c_str());
            }
        }

        send_message_to_all_clients(
            std::format("Hark! A stranger hath joined: '{}'", nick),
            pInfo->m_hConn);

        m_map_clients[pInfo->m_hConn]; // default-construct client entry
        set_client_nick(pInfo->m_hConn, nick);
        return;
    }

    case k_ESteamNetworkingConnectionState_Connected:
        // Immediately after accepting; server can ignore
        return;

    default:
        return;
    }
}

void GameServer::poll_connection_state_changes()
{
    m_instance = this;
    m_sockets->RunCallbacks();
}

void GameServer::local_user_input_init()
{
    m_threadUserInput = std::jthread([this]() {
        while (!m_is_quitting) {
            char szLine[4000];
            if (!fgets(szLine, sizeof(szLine), stdin)) {
                if (m_is_quitting)
                    return;
                m_is_quitting = true;
                printt("Failed to read on stdin, quitting\n");
                break;
            }

            std::lock_guard<std::mutex> lock(m_mutexUserInputQueue);
            m_queueUserInput.push(std::string(szLine));
        }
    });
}

bool GameServer::local_user_input_get_next(std::string& result)
{
    std::lock_guard<std::mutex> lock(m_mutexUserInputQueue); // RAII locking

    while (!m_queueUserInput.empty()) {
        result = std::move(m_queueUserInput.front());
        m_queueUserInput.pop();

        ltrim(result);
        rtrim(result);

        if (!result.empty())
            return true; // valid line found
    }

    return false; // no meaningful input
}

void debug_output(ESteamNetworkingSocketsDebugOutputType eType, const char* pszMsg)
{
    SteamNetworkingMicroseconds time = SteamNetworkingUtils()->GetLocalTimestamp() - g_logTimeZero;
    printf("%10.6f %s\n", time * 1e-6, pszMsg);
    fflush(stdout);
    if (eType == k_ESteamNetworkingSocketsDebugOutputType_Bug) {
        fflush(stdout);
        fflush(stderr);
        nuke_process(1);
    }
}

void fatal_error(const char* fmt, ...)
{
    char    text[2048];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(text, fmt, ap);
    va_end(ap);
    char* nl = strchr(text, '\0') - 1;
    if (nl >= text && *nl == '\n')
        *nl = '\0';
    debug_output(k_ESteamNetworkingSocketsDebugOutputType_Bug, text);
}

void printt(const char* fmt, ...)
{
    char    text[2048];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(text, fmt, ap);
    va_end(ap);
    char* nl = strchr(text, '\0') - 1;
    if (nl >= text && *nl == '\n')
        *nl = '\0';
    debug_output(k_ESteamNetworkingSocketsDebugOutputType_Msg, text);
}

GameServer* GameServer::m_instance = nullptr;
int         main(int argc, char* argv[])
{
    GameServer game_server;
    game_server.run();
    return 0;
}
