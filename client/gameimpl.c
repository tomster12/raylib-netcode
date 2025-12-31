#include "../shared/gameimpl.h"
#include "gameimpl.h"
#include "raylib.h"

void game_handle_events(GameState *game_state, GameEvents *game_events, int client_index)
{
    // Otherwise handle moving the player
    PlayerInput *controls = &game_events->player_inputs[client_index];
    controls->movements_held[0] = IsKeyDown(KEY_A);
    controls->movements_held[1] = IsKeyDown(KEY_D);
    controls->movements_held[3] = IsKeyDown(KEY_S);
    controls->movements_held[2] = IsKeyDown(KEY_W);
}

void game_render(const GameState *game_state, int client_index)
{
    // Render each player as a coloured circle
    for (size_t i = 0; i < MAX_CLIENTS; ++i)
    {
        const PlayerData *player = &game_state->player_data[i];
        if (player->active)
        {
            DrawCircle((int)player->x, (int)player->y, 20, (i == client_index) ? BLUE : RED);
        }
    }
}
