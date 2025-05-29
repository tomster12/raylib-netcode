#pragma once

#include "implcore.h"

typedef struct
{
    GameState states[MAX_ROLLBACK];
    GameEvents events[MAX_ROLLBACK];
    uint32_t server_frame;
    uint32_t client_frame;
} NetBuffer;

void net_buffer_init(NetBuffer *buf);

GameState *net_buffer_get_state(NetBuffer *buf, uint32_t frame);

GameEvents *net_buffer_get_events(NetBuffer *buf, uint32_t frame);

typedef struct
{
    uint32_t frame;
    uint32_t frame_progress;
    GameState state;
    GameEvents events[MAX_ROLLBACK];
} ServerUpdate;

void *net_client_recv_loop(void *arg);

int net_client_init(const char *server_ip, int port, NetBuffer *buf);

void net_client_shutdown();

void net_client_send_events(uint32_t frame, const GameEvents *events);
