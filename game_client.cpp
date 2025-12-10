

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>
#include <string_view>
#include <thread>

#include "game_client.h"
#include "network_utils.h"

namespace {
SteamNetworkingMicroseconds g_logTimeZero;
}

void GameClient::run()
{

    // init();

    m_sockets = SteamNetworkingSockets();
    if (m_sockets == nullptr) {
        fatal_error("Failed to create net socket.");
    }

    char sz_addr[SteamNetworkingIPAddr::k_cchMaxString];

    SteamNetworkingIPAddr server_addr;
    server_addr.Clear();

    if (!server_addr.ParseString("127.0.0.1:7777")) {
        fatal_error("Invalid server adress.");
    }

    server_addr.ToString(sz_addr, sizeof(sz_addr), true);

    printt("Connecting to chat server at %s", sz_addr);

    SteamNetworkingConfigValue_t options;
    options.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
        (void*)net_connection_status_changed_callback);

    m_net_connection = m_sockets->ConnectByIPAddress(server_addr, 1, &options);
    if (m_net_connection == k_HSteamNetConnection_Invalid) {
        fatal_error("Failed to create connection.");
    }

    m_is_connected = true;

    // while (!m_is_quitting) {
        // poll_incoming_messages();
        // poll_connection_state_changes();
        // poll_local_user_input();
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // }

    // shutdown();
}

void GameClient::disconnect_from_server() {

    if (m_net_connection != k_HSteamNetConnection_Invalid) {
        m_sockets->CloseConnection(m_net_connection, 0, nullptr, false);
        m_net_connection = k_HSteamNetConnection_Invalid;
    }
    m_is_connected = false;
    m_is_quitting = true;
    // GameNetworkingSockets_Kill();
}

void GameClient::shutdown()
{
    // Step 5: destroy the library
    if (m_net_connection != k_HSteamNetConnection_Invalid) {
        m_sockets->CloseConnection(m_net_connection, 0, nullptr, false);
        m_net_connection = k_HSteamNetConnection_Invalid;
    }
    m_is_connected = false;
    m_is_quitting = true;
    GameNetworkingSockets_Kill();
    nuke_process(0);
}

void GameClient::poll_incoming_messages()
{
    while (!m_is_quitting) {
        ISteamNetworkingMessage* msg = nullptr;

        int num_msgs = m_sockets->ReceiveMessagesOnConnection(m_net_connection, &msg, 1);
        if (num_msgs == 0)
            break;
        if (num_msgs < 0)
            fatal_error("Error checking messages.");

        fwrite(msg->m_pData, 1, msg->m_cbSize, stdout);
        fputc('\n', stdout);

        msg->Release();
    }
}

void GameClient::init()
{
    SteamDatagramErrMsg errMsg;
    if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
        fatal_error("GameNetworkingSockets_Init failed! %s", errMsg);
    }

    g_logTimeZero = SteamNetworkingUtils()->GetLocalTimestamp();
    SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, debug_output);

    local_user_input_init();
}

void GameClient::poll_local_user_input()
{
    std::string cmd;
    while (!m_is_quitting && local_user_input_get_next(cmd)) {

        std::string_view input { cmd };

        if (input == "/quit") {
            m_is_quitting = true;
            printt("Disconnecting from chat server.");

            m_sockets->CloseConnection(m_net_connection, 0, "Goodbye.", true);
            break;
        }

        m_sockets->SendMessageToConnection(m_net_connection, input.data(),
            input.size(), k_nSteamNetworkingSend_Reliable, nullptr);
    }
}

void GameClient::on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* p_info)
{
    assert(p_info->m_hConn == m_net_connection || m_net_connection == k_HSteamNetConnection_Invalid);

    switch (p_info->m_info.m_eState) {

    case k_ESteamNetworkingConnectionState_None:
        break;

    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
        m_is_quitting = true;

        if (p_info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting) {
            printt("Connection failed. (%s)", p_info->m_info.m_szEndDebug);
        } else if (p_info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
            printt("Connection failed due to local problem. (%s)", p_info->m_info.m_szEndDebug);
        } else {
            printt("The conection is closed.", p_info->m_info.m_szEndDebug);
        }

        m_sockets->CloseConnection(m_net_connection, 0, nullptr, false);
        m_net_connection = k_HSteamNetConnection_Invalid;
        break;
    }

    case k_ESteamNetworkingConnectionState_Connecting:
        break;

    case k_ESteamNetworkingConnectionState_Connected:
        m_is_connected = true;
        printt("Connected to server. OK.");
        break;

    default:
        break;
    }
}

void GameClient::poll_connection_state_changes()
{
    m_instance = this;
    m_sockets->RunCallbacks();
}

void GameClient::local_user_input_init()
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

bool GameClient::local_user_input_get_next(std::string& result)
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
        // nuke_process(1);
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

GameClient* GameClient::m_instance = nullptr;

// int main(int argc, char* argv[])
// {
//     GameClient game_client;
//     game_client.run();
//     return 0;
// }
