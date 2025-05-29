#pragma once

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define MAX_ROLLBACK 256
#define MAX_PLAYERS 50

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
    PlayerControl player_controls[MAX_PLAYERS];
    PlayerEvent player_event[MAX_PLAYERS];
    size_t player_event_count;
} GameEvents;

typedef struct
{
    float x, y;
    bool active;
} PlayerData;

typedef struct
{
    PlayerData player_data[MAX_PLAYERS];
} GameState;

void game_simulate(const GameState *current, const GameEvents *input, GameState *out);
