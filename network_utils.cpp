
#include "network_utils.h"

#include <steam/isteamnetworkingutils.h>
#include <unistd.h>
#include <signal.h>

void print_usage_and_exit(int rc = 1)
{
    fflush(stderr);
    printf(
        R"usage(Usage:
    example_chat client SERVER_ADDR
    example_chat server [--port PORT]
)usage");
    fflush(stdout);
    exit(rc);
}

void nuke_process(int rc)
{
#ifdef _WIN32
    ExitProcess(rc);
#else
    (void)rc; // Unused formal parameter
    kill(getpid(), SIGKILL);
#endif
}


