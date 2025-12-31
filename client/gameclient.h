#pragma once

#include "../shared/gameimpl.h"
#include "../shared/protocol.h"
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

    uint32_t client_index;
    uint32_t sync_frame;
    uint32_t server_frame;
    uint32_t client_frame;
    GameState states[FRAME_BUFFER_SIZE];
    GameEvents events[FRAME_BUFFER_SIZE];
} GameClient;

int game_client_init(GameClient *client, const char *server_ip, int port);
void game_client_shutdown(GameClient *client);
void *game_client_recv_thread(void *arg);

void game_client_handle_payload(GameClient *client, MessageHeader *header, char *buf, size_t n);
void game_client_reconcile_frames(GameClient *client);
void game_client_send_game_events(GameClient *client, uint32_t frame, GameEvents *events);
