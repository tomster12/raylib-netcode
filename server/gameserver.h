#pragma once

#include "shared/gameimpl.h"
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <unistd.h>

typedef struct
{
    bool is_connected;
    int fd;
    int index;
    pthread_t thread_id;
    uint32_t client_frame;
} ClientData;

typedef struct
{
    atomic_bool to_shutdown;

    int socket_fd;
    pthread_t simulation_thread;
    pthread_t client_accept_thread;
    pthread_mutex_t clients_lock;
    pthread_mutex_t state_lock;
    pthread_cond_t simulation_loop_cond;

    int client_count;
    uint32_t server_frame;
    ClientData client_data[MAX_CLIENTS];
    GameState game_states[FRAME_BUFFER_SIZE];
    GameEvents game_events[FRAME_BUFFER_SIZE];
} GameServer;

typedef struct
{
    GameServer *server;
    int index;
} ClientThreadArgs;

int game_server_init(GameServer *server, int port);
void game_server_shutdown(GameServer *server);
void *game_server_accept_thread(void *arg);
void *game_server_client_thread(void *arg);
void *game_simulation_thread(void *arg);

ssize_t game_server_broadcast(GameServer *server, const uint8_t *buffer, size_t size, int exclude_fd);
bool game_server_can_simulate(GameServer *server);
