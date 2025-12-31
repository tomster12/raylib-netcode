#pragma once

#include "../shared/globals.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
    bool movements_held[4];
} PlayerInput;

typedef enum class
{
    PLAYER_EVENT_NONE,
    PLAYER_EVENT_JOIN,
    PLAYER_EVENT_LEAVE
} PlayerEvent;

typedef struct
{
    PlayerEvent player_events[MAX_CLIENTS];
    PlayerInput player_inputs[MAX_CLIENTS];
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
