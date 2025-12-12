#include "net_messages.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <steam/isteamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>
#include <string>
#include <thread>

class GameClient {

public:
    void init();
    void run();
    void connect();
    void poll_loop();
    void shutdown();
    void disconnect_from_server();
    void send_data(const void* data, uint32 data_size, int k_n_flag);
    bool m_is_connected { false };
    void parse_incoming_messages();


    std::function<void(uint32_t id, Position pos)> on_player_position_changed;
    std::function<void(uint32_t id, Position pos)> on_player_joined;
    std::function<void(uint32_t id, Client clients[8])> on_players_initial_state_sent;
    std::function<void(uint32_t id)> on_player_id_assigned;
    std::function<void(uint32_t id)> on_player_left;


private:
    static GameClient* m_instance;

    ISteamNetworkingSockets* m_sockets;
    HSteamNetConnection      m_net_connection;

    std::jthread            m_threadUserInput;
    std::queue<std::string> m_queueUserInput;
    std::atomic<bool>       m_is_quitting { false };
    std::mutex              m_mutexUserInputQueue;

    void send_string_data_to_server(std::string_view msg);
    void poll_incoming_messages();
    void poll_local_user_input();
    void local_user_input_init();
    void on_net_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* p_info);
    void poll_connection_state_changes();
    bool local_user_input_get_next(std::string& result);

    static void net_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t* p_info)
    {
        m_instance->on_net_connection_status_changed(p_info);
    }
};

void fatal_error(const char* fmt, ...);
void debug_output(ESteamNetworkingSocketsDebugOutputType eType, const char* pszMsg);
void printt(const char* fmt, ...);
