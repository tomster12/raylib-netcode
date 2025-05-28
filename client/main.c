// cbuild: -I../libs/raylib/include -L../libs/raylib/lib -I../shared
// cbuild: -lraylib -lm ../shared/game.c

#include "raylib.h"
#include "gamebuffer.h"
#include "game.h"

void populate_game_events(GameState *game_state, GameEvents *game_events, int local_player_id)
{
    // Join as a new player if this is the first frame
    if (game_state->player_data[local_player_id].active == false)
    {
        PlayerEvent *join_event = &game_events->player_event[game_events->player_event_count++];
        join_event->type = PLAYER_EVENT_PLAYER_JOINED;
        join_event->player_id = local_player_id;
        return;
    }

    // Otherwise handle moving the player
    PlayerControl *controls = &game_events->player_controls[local_player_id];
    controls->movements_held[0] = IsKeyDown(KEY_A);
    controls->movements_held[1] = IsKeyDown(KEY_D);
    controls->movements_held[3] = IsKeyDown(KEY_S);
    controls->movements_held[2] = IsKeyDown(KEY_W);
}

void render_game_state(const GameState *game_state, int local_player_id)
{
    // Render each player as a coloured circle
    for (size_t i = 0; i < MAX_PLAYERS; ++i)
    {
        const PlayerData *player = &game_state->player_data[i];
        if (player->active)
        {
            DrawCircle((int)player->x, (int)player->y, 20, (i == local_player_id) ? BLUE : RED);
        }
    }
}

int main()
{
    // Setup Raylib
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(60);
    InitWindow(800, 800, "Raylib Netcode");

    // Setup game
    uint32_t current_frame = 0;
    int local_player_id = 0;
    GameBuffer game_buffer;
    game_buffer_init(&game_buffer, current_frame);

    while (!WindowShouldClose())
    {
        GameState *game_state = game_buffer_get_state(&game_buffer, current_frame);
        GameEvents *game_events = game_buffer_get_events(&game_buffer, current_frame);
        memset(game_events, 0, sizeof(GameEvents));

        // Events
        populate_game_events(game_state, game_events, local_player_id);

        // Simulate
        GameState *game_state_next = game_buffer_get_state(&game_buffer, current_frame + 1);
        simulate(game_state, game_events, game_state_next);

        // Draw
        BeginDrawing();
        ClearBackground(RAYWHITE);
        render_game_state(game_state_next, local_player_id);
        DrawFPS(10, 10);
        EndDrawing();

        current_frame++;
    }

    CloseWindow();

    return 0;
}
