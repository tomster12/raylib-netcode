#pragma once

#include "shared/gameimpl.h"
#include <pthread.h>
#include <signal.h>

typedef struct
{
    volatile sig_atomic_t to_shutdown;
    bool is_connected;
    int socket_fd;
    pthread_t recv_thread;
    uint32_t client_player_id;
    uint32_t server_frame;
    uint32_t client_frame;
    GameState states[MAX_ROLLBACK];
    GameEvents events[MAX_ROLLBACK];
} GameClient;

int game_client_init(GameClient *client, const char *server_ip, int port);

void game_client_shutdown(GameClient *client);

GameState *game_client_get_state(GameClient *client, uint32_t frame);

GameEvents *game_client_get_events(GameClient *client, uint32_t frame);

void *game_client_recv_thread(void *arg);

void game_client_update_server(GameClient *client);
