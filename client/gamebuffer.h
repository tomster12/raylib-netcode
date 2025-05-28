#pragma once

#include "game.h"

typedef struct
{
    GameState states[MAX_FRAMES];
    GameEvents events[MAX_FRAMES];
    uint32_t start_frame;
} GameBuffer;

void game_buffer_init(GameBuffer *buf, uint32_t start_frame)
{
    memset(buf, 0, sizeof(GameBuffer));
    buf->start_frame = start_frame;
}

GameState *game_buffer_get_state(GameBuffer *buf, uint32_t frame)
{
    return &buf->states[frame % MAX_FRAMES];
}

GameEvents *game_buffer_get_events(GameBuffer *buf, uint32_t frame)
{
    return &buf->events[frame % MAX_FRAMES];
}
