#pragma once

#include "shared/globals.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
    bool movements_held[4];
} PlayerControl;

typedef enum
{
    PLAYER_EVENT_PLAYER_JOINED,
    PLAYER_EVENT_PLAYER_LEFT
} PlayerEventType;

typedef struct
{
    PlayerEventType type;
    size_t player_id;
} PlayerEvent;

typedef struct
{
    PlayerControl player_controls[MAX_CLIENTS];
    PlayerEvent player_event[MAX_CLIENTS];
    size_t player_event_count;
} GameEvents;

typedef struct
{
    float x, y;
    bool active;
} PlayerData;

typedef struct
{
    PlayerData player_data[MAX_CLIENTS];
} GameState;

void game_simulate(const GameState *current, const GameEvents *input, GameState *out);
