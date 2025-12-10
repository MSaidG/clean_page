#include <atomic>
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
    void shutdown();
    void disconnect_from_server();
    bool m_is_connected { false };

private:
    static GameClient* m_instance;

    HSteamNetConnection      m_net_connection;
    ISteamNetworkingSockets* m_sockets;

    std::jthread            m_threadUserInput;
    std::queue<std::string> m_queueUserInput;
    std::atomic<bool>       m_is_quitting { false };
    std::mutex              m_mutexUserInputQueue;

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
