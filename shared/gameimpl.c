#include "gameimpl.h"
#include <assert.h>

void game_simulate(const GameState *current, const GameEvents *events, GameState *out)
{
    *out = *current;

    uint32_t active_players = 0;
    for (size_t i = 0; i < MAX_CLIENTS; ++i)
    {
        const PlayerEvent *player_event = &events->player_events[i];
        const PlayerInput *player_input = &events->player_inputs[i];
        PlayerData *player_data = &out->player_data[i];

        // Handle events
        if (*player_event == PLAYER_EVENT_JOIN)
        {
            player_data->active = true;
            player_data->x = 400.0f;
            player_data->y = 400.0f;
        }
        if (*player_event == PLAYER_EVENT_LEAVE)
        {
            player_data->active = false;
        }

        if (!player_data->active) continue;

        // Handle movement
        if (player_input->movements_held[0]) player_data->x -= 1.0f;
        if (player_input->movements_held[1]) player_data->x += 1.0f;
        if (player_input->movements_held[2]) player_data->y -= 1.0f;
        if (player_input->movements_held[3]) player_data->y += 1.0f;
    }
}
