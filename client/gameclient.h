#pragma once

#include "shared/gameimpl.h"
#include "shared/protocol.h"
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>

typedef struct
{
    atomic_bool to_shutdown;
    atomic_bool is_connected;
    atomic_bool is_initialised;

    int socket_fd;
    pthread_t recv_thread;
    pthread_mutex_t state_lock;

    uint32_t client_player_id;
    uint32_t last_confirmed_frame;
    uint32_t current_frame;
    GameState states[MAX_ROLLBACK];
    GameEvents events[MAX_ROLLBACK];
    bool frame_confirmed[MAX_ROLLBACK];
} GameClient;

int game_client_init(GameClient *client, const char *server_ip, int port);

void game_client_shutdown(GameClient *client);

GameState *game_client_get_state(GameClient *client, uint32_t frame);

GameEvents *game_client_get_events(GameClient *client, uint32_t frame);

void *game_client_recv_thread(void *arg);

void game_client_handle_payload(GameClient *client, MessageHeader *header, char *buf, size_t n);

void game_client_update_server(GameClient *client);
