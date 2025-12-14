#include "game_client.h"
#include "net_messages.h"
#include "network_utils.h"

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>
#include <steam/steamtypes.h>
#include <string_view>
#include <thread>

namespace {
SteamNetworkingMicroseconds g_logTimeZero;
}

void GameClient::run()
{
    init();
    connect();
    poll_loop();
}

void GameClient::poll_loop()
{
    while (!m_is_quitting) {
        poll_incoming_messages();
        poll_connection_state_changes();
        poll_local_user_input();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    shutdown();
}

void GameClient::connect()
{
    m_sockets = SteamNetworkingSockets();
    if (m_sockets == nullptr) {
        fatal_error("Failed to create net socket.");
    }

    char sz_addr[SteamNetworkingIPAddr::k_cchMaxString];

    SteamNetworkingIPAddr server_addr;
    server_addr.Clear();

    if (!server_addr.ParseString("127.0.0.1:7776")) {
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
}

void GameClient::disconnect_from_server()
{

    if (m_net_connection != k_HSteamNetConnection_Invalid) {
        m_sockets->CloseConnection(m_net_connection, 0, "Client disconnecting.", true);
        m_net_connection = k_HSteamNetConnection_Invalid;
    }
    m_is_connected = false;
    m_is_quitting  = true;
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
    m_is_quitting  = true;
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

void GameClient::parse_incoming_messages()
{
    ISteamNetworkingMessage* msg      = nullptr;
    int                      num_msgs = m_sockets->ReceiveMessagesOnConnection(m_net_connection, &msg, 1);
    if (num_msgs == 0)
        return;
    if (num_msgs < 0)
        fatal_error("Client received Error checking messages.");

    int   size = msg->m_cbSize;
    void* data = msg->m_pData;

    if (size < sizeof(MsgHeader)) {
        msg->Release();
        fatal_error("Client received Invalid packet (too small)\n");
    }

    MsgHeader header;
    memcpy(&header, data, sizeof(header));

    if (size < sizeof(MsgHeader) + header.size) {
        msg->Release();
        printt("Conn: %u\n", msg->m_conn);
        printt("Header type: %d\n", header.type);
        printt("Client received Malformed packet (wrong size)\n");
        return;
    }

    uint8_t* payload = (uint8_t*)data + sizeof(MsgHeader);

    switch (header.type) {
    case MsgType::Direction: {
        if (header.size != sizeof(Direction)) {
            printt("Client received Invalid Direction packet size\n");
            break;
        }

        Direction dir;
        memcpy(&dir, payload, sizeof(dir));
        // printt("Direction x=%f y=%f\n", dir.x, dir.y);
    } break;

    case MsgType::ChatMessage: {
        std::string text((char*)payload, header.size);
        std::cout << "user_msg: " << text << "\n"; // DEBUG_PRINT

    } break;

    case MsgType::Position: {
        if (header.size != sizeof(Position)) {
            printt("Client received Invalid Position packet size\n");
            break;
        }

        Position pos;
        memcpy(&pos, payload, sizeof(pos));
        printt("Position x=%f y=%f\n", pos.x, pos.y);

    } break;

    case MsgType::MsgPlayerJoined: {
        if (header.size != sizeof(MsgPlayerJoined)) {
            printt("Client received Invalid MsgPlayerJoined packet size\n");
            break;
        }

        MsgPlayerJoined joined_msg;
        memcpy(&joined_msg, payload, sizeof(joined_msg));
        on_player_joined(joined_msg.id, joined_msg.position);

        printt("Player '%d' joined x=%f y=%f\n",
            joined_msg.id, joined_msg.position.x, joined_msg.position.x);
    } break;

    case MsgType::MsgPlayerLeft: {
        if (header.size != sizeof(MsgPlayerLeft)) {
            printt("Client received Invalid MsgPlayerLeft packet size\n");
            break;
        }

        MsgPlayerLeft left_msg;
        memcpy(&left_msg, payload, sizeof(left_msg));
        on_player_left(left_msg.id);

        printt("Player '%d' left.\n", left_msg.id);
    } break;

    case MsgType::MsgPlayerIdAssign: {
        if (header.size != sizeof(MsgPlayerIdAssign)) {
            printt("Client received Invalid MsgPlayerIdAssign packet size\n");
            break;
        }

        MsgPlayerIdAssign id_assign_msg;
        memcpy(&id_assign_msg, payload, sizeof(id_assign_msg));
        on_player_id_assigned(id_assign_msg.id);

        printt("Player assigned id '%d'.\n", id_assign_msg.id);
    } break;

    case MsgType::MsgPlayerPositionChanged: {
        if (header.size != sizeof(MsgPlayerPositionChanged)) {
            printt("Client received Invalid MsgPlayerPositionChanged packet size\n");
            break;
        }

        MsgPlayerPositionChanged position_changed_msg;
        memcpy(&position_changed_msg, payload, sizeof(position_changed_msg));
        on_player_position_changed(position_changed_msg.id, position_changed_msg.position);

        // printt("Player '%d' position changed x: '%f' y: '%f'.\n",
        //     position_changed_msg.id, position_changed_msg.position.x, position_changed_msg.position.y);
    } break;

    case MsgType::MsgInitialState: {
        if (header.size != sizeof(MsgInitialState)) {
            printt("Client received Invalid MsgInitialState packet size\n");
            break;
        }

        MsgInitialState position_changed_msg;
        memcpy(&position_changed_msg, payload, sizeof(position_changed_msg));
        on_players_initial_state_sent(position_changed_msg.count, position_changed_msg.clients);

    } break;

    case MsgType::MsgSpawnBullet: {
            if (header.size != sizeof(MsgSpawnBullet)) {
                printt("Client received Invalid MsgSpawnBullet packet size\n");
                break;
            }

            MsgSpawnBullet spawn_bullet_msg;
            memcpy(&spawn_bullet_msg, payload, sizeof(spawn_bullet_msg));

            on_players_spawn_bullet(spawn_bullet_msg);
            // printt("Player '%d' position changed x: '%f' y: '%f'.\n",
            //     spawn_bullet_msg.id, spawn_bullet_msg.position.x, spawn_bullet_msg.position.y);
        } break;

    default:
        printt("Client received Unknown message type\n");
    }

    // fwrite(msg->m_pData, 1, msg->m_cbSize, stdout);
    // fputc('\n', stdout);

    msg->Release();
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

        // m_sockets->SendMessageToConnection(m_net_connection, input.data(),
        // input.size(), k_nSteamNetworkingSend_Reliable, nullptr);

        send_string_data_to_server(input);
    }
}

void GameClient::send_string_data_to_server(std::string_view msg)
{
    MsgHeader header;
    header.type = MsgType::ChatMessage;
    header.size = static_cast<uint32_t>(msg.size());

    std::vector<uint8_t> buffer(sizeof(MsgHeader) + msg.size());

    memcpy(buffer.data(), &header, sizeof(MsgHeader));
    memcpy(buffer.data() + sizeof(MsgHeader), msg.data(), msg.size());

    m_sockets->SendMessageToConnection(m_net_connection, buffer.data(),
        buffer.size(), k_nSteamNetworkingSend_Reliable,
        nullptr);
}

void GameClient::send_data(const void* data, uint32 data_size, int k_n_flag)
{
    m_sockets->SendMessageToConnection(m_net_connection, data,
        data_size, k_n_flag, nullptr);
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
