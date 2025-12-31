// cbuild: -I../ -g
// cbuild: gameserver.c ../shared/gameimpl.c ../shared/protocol.c ../shared/log.c

#include "gameserver.h"
#include "../shared/gameimpl.h"
#include "../shared/globals.h"
#include "../shared/log.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

volatile sig_atomic_t to_shutdown_app = 0;

void handle_sigaction(int signum)
{
    log_printf("Received sigaction signum=%d\n", signum);
    to_shutdown_app = 1;
}

void init_sigaction_handler(struct sigaction *sa)
{
    memset(sa, 0, sizeof(*sa));

    sa->sa_handler = handle_sigaction;
    sa->sa_flags = 0;

    sigaction(SIGINT, sa, NULL);
    sigaction(SIGTERM, sa, NULL);
}

int main()
{
    log_printf("Server application started\n");

    struct sigaction sa;
    init_sigaction_handler(&sa);

    GameServer server;
    if (game_server_init(&server, PORT) != 0)
    {
        perror("game_server_init");
        return 1;
    }

    while (!to_shutdown_app) pause();

    log_printf("Shutting down the game server\n");
    game_server_shutdown(&server);
    return 0;
}
