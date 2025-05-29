#include "shared/gameimpl.h"
#include "gameimpl.h"
#include "raylib.h"

void game_handle_events(GameState *game_state, GameEvents *game_events, int client_player_id)
{
    // Join as a new player if this is the first frame
    if (game_state->player_data[client_player_id].active == false)
    {
        PlayerEvent *join_event = &game_events->player_event[game_events->player_event_count++];
        join_event->type = PLAYER_EVENT_PLAYER_JOINED;
        join_event->player_id = client_player_id;
        return;
    }

    // Otherwise handle moving the player
    PlayerControl *controls = &game_events->player_controls[client_player_id];
    controls->movements_held[0] = IsKeyDown(KEY_A);
    controls->movements_held[1] = IsKeyDown(KEY_D);
    controls->movements_held[3] = IsKeyDown(KEY_S);
    controls->movements_held[2] = IsKeyDown(KEY_W);
}

void game_render(const GameState *game_state, int client_player_id)
{
    // Render each player as a coloured circle
    for (size_t i = 0; i < MAX_CLIENTS; ++i)
    {
        const PlayerData *player = &game_state->player_data[i];
        if (player->active)
        {
            DrawCircle((int)player->x, (int)player->y, 20, (i == client_player_id) ? BLUE : RED);
        }
    }
}
