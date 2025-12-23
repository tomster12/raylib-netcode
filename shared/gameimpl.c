#include "gameimpl.h"
#include <assert.h>

void game_player_join(GameState *state, uint32_t player_id)
{
    assert(player_id < MAX_CLIENTS);
    state->player_data[player_id].active = true;
    state->player_data[player_id].x = 400.0f;
    state->player_data[player_id].y = 400.0f;
}

void game_player_leave(GameState *state, uint32_t player_id)
{
    assert(player_id < MAX_CLIENTS);
    state->player_data[player_id].active = false;
}

void game_simulate(const GameState *current, const GameEvents *events, GameState *out)
{
    *out = *current;

    uint32_t active_players = 0;
    for (size_t i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!out->player_data[i].active) continue;

        // Handle movement
        if (events->player_inputs[i].movements_held[0])
        {
            out->player_data[i].x -= 1.0f;
        }
        if (events->player_inputs[i].movements_held[1])
        {
            out->player_data[i].x += 1.0f;
        }
        if (events->player_inputs[i].movements_held[2])
        {
            out->player_data[i].y -= 1.0f;
        }
        if (events->player_inputs[i].movements_held[3])
        {
            out->player_data[i].y += 1.0f;
        }
    }
}
