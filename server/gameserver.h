#pragma once

#include "shared/gameimpl.h"
#include <pthread.h>
#include <signal.h>

typedef struct
{
    bool is_connected;
    int fd;
    int index;
    pthread_t thread_id;
} ClientData;

typedef struct
{
    volatile sig_atomic_t to_shutdown;
    int socket_fd;
    pthread_t simulation_thread;
    pthread_t client_accept_thread;
    pthread_mutex_t client_sockets_mutex;
    ClientData client_data[MAX_CLIENTS];
    int client_count;
    uint32_t server_frame;
    pthread_mutex_t game_state_mutex;
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
