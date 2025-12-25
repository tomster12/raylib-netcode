#pragma once

#include "shared/gameimpl.h"
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>

typedef struct
{
    atomic_bool is_connected;
    int fd;
    int index;
    pthread_t thread_id;
    uint32_t last_received_frame;
    bool ready_for_frame;
} ClientData;

typedef struct
{
    atomic_bool to_shutdown;

    int socket_fd;
    pthread_t simulation_thread;
    pthread_t client_accept_thread;
    pthread_mutex_t clients_lock;
    pthread_mutex_t state_lock;
    pthread_cond_t frame_ready_cond;

    int client_count;
    uint32_t server_frame;
    ClientData client_data[MAX_CLIENTS];
    GameState game_states[MAX_ROLLBACK];
    GameEvents game_events[MAX_ROLLBACK];
} GameServer;

typedef struct
{
    GameServer *server;
    int index;
} ClientThreadArgs;

int game_server_init(GameServer *server, int port);

void game_server_shutdown(GameServer *server);

void broadcast_to_all_clients(GameServer *server, const uint8_t *buffer, size_t size, int exclude_fd);

void *game_server_accept_thread(void *arg);

void *game_server_client_thread(void *arg);

void *game_simulation_thread(void *arg);

bool all_clients_ready_for_frame(GameServer *server);

void reset_client_ready_flags(GameServer *server);