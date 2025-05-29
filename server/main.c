// cbuild: -I../ -g
// cbuild: gameserver.c

#include "gameserver.h"
#include "shared/gameimpl.h"
#include "shared/globals.h"
#include <errno.h>   // for errno
#include <pthread.h> // for pthreads
#include <signal.h>  // for signal handling
#include <stdint.h>  // for uint32_t
#include <stdio.h>   // for printf
#include <stdlib.h>  // for exit
#include <string.h>  // for memset
#include <unistd.h>  // for close

volatile sig_atomic_t to_shutdown = 0;

void handle_sigaction(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        printf("Received sigaction signum=%d\n", signum);
        to_shutdown = 1;
    }
    else
    {
        perror("Unexpected signal received");
    }
}

int main()
{
    printf("Server application started\n");

    // Setup signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigaction;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Start game server
    GameServer server;
    if (game_server_init(&server, PORT) != 0)
    {
        perror("Failed to initialize game server");
        return 1;
    }

    // Wait for shutdown signal
    while (!to_shutdown) pause();
    printf("\nShutting down the game server\n");
    game_server_shutdown(&server);
    printf("Server application finished\n");
    return 0;
}
