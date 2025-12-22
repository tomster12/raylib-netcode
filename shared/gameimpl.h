#pragma once

#include "shared/globals.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
    bool movements_held[4];
} PlayerInput;

typedef struct
{
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

void game_player_join(GameState *state, uint32_t player_id);
void game_player_leave(GameState *state, uint32_t player_id);
void game_simulate(const GameState *current, const GameEvents *input, GameState *out);
