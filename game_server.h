#pragma once

#include "net_messages.h"
#include <atomic>
#include <mutex>
#include <queue>
#include <steam/isteamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>
#include <string>
#include <thread>
#include <unordered_map>

constexpr int PORT = 7776;

class GameServer {
public:
    void run();

private:

    std::unordered_map<HSteamNetConnection, Client> m_map_clients;

    static GameServer* m_instance;
    const uint16       m_port { PORT };

    ISteamNetworkingSockets* m_sockets;
    HSteamNetPollGroup       m_poll_group;
    HSteamListenSocket       m_listen_socket;
    
    std::queue<std::string>  m_queueUserInput;
    std::atomic<bool>        m_is_quitting { false };
    std::mutex               m_mutexUserInputQueue;
    std::jthread             m_threadUserInput;

    void init();
    void shutdown_server();
    void send_message_to_all_clients(const std::string_view msg, HSteamNetConnection except = k_HSteamNetConnection_Invalid);
    void send_message_to_client(HSteamNetConnection conn, std::string_view msg) noexcept;
    void poll_local_user_input();
    void poll_incoming_messages();
    bool is_all_reliable_messages_sent(ISteamNetworkingSockets* sockets, const std::unordered_map<HSteamNetConnection, Client>& clients);
    void set_client_nick(HSteamNetConnection hConn, std::string_view nick);
    void on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* pInfo);
    void poll_connection_state_changes();
    void local_user_input_init();
    bool local_user_input_get_next(std::string& result);

    // void send_direction_data_to_all_other_clients(Direction dir);
    template<typename T>
    void send_data_to_all_clients(const T data, HSteamNetConnection except, const int k_n_flag=k_nSteamNetworkingSend_Unreliable);
    // void send_data_to_client(HSteamNetConnection conn, const Direction dir) noexcept;
    template<typename T>
    void send_data(HSteamNetConnection conn, const T data, uint32 data_size, int k_n_flag);

    // void send_data(HSteamNetConnection conn, const void* data, uint32 data_size, int k_n_flag);


    static void net_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t* pInfo)
    {
        // Retrieve instance from the callback context
        m_instance->on_net_connection_status_changed(pInfo);
    }
};

void fatal_error(const char* fmt, ...);
void debug_output(ESteamNetworkingSocketsDebugOutputType eType, const char* pszMsg);
void printt(const char* fmt, ...);
