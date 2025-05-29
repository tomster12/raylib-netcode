// cbuild: -I../libs/raylib/include -L../libs/raylib/lib -I../
// cbuild: -lraylib -lm ../shared/gameimpl.c gameimpl.c gameclient.c

#include "gameclient.h"
#include "gameimpl.h"
#include "raylib.h"
#include "shared/gameimpl.h"
#include "shared/globals.h"
#include <arpa/inet.h>  // For inet_ntop, inet_pton
#include <stdio.h>      // For printf
#include <stdlib.h>     // For malloc, free
#include <string.h>     // For memset
#include <sys/socket.h> // For socket functions
#include <unistd.h>     // For usleep

int main()
{
    printf("Client application started\n");

    // Setup Raylib window
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(SIMULATION_TICK_RATE);
    InitWindow(800, 800, "Raylib Netcode");

    // Setup game client
    GameClient client;
    if (game_client_init(&client, "127.0.0.1", PORT) != 0)
    {
        perror("Failed to initialize game client");
        CloseWindow();
        return 1;
    }

    // Game simulation tick and rendering loop
    while (!WindowShouldClose())
    {
        // Grab current frame and events
        GameState *game_state = game_client_get_state(&client, client.client_frame);
        GameEvents *game_events = game_client_get_events(&client, client.client_frame);
        memset(game_events, 0, sizeof(GameEvents));

        // Handle game events
        game_handle_events(game_state, game_events, client.client_player_id);

        // Perform game simulation
        GameState *game_state_next = game_client_get_state(&client, client.client_frame + 1);
        game_simulate(game_state, game_events, game_state_next);

        // Draw the game state
        BeginDrawing();
        ClearBackground(RAYWHITE);
        game_render(game_state_next, client.client_player_id);
        DrawFPS(10, 10);
        EndDrawing();

        // Send this frames events to the server
        game_client_tick(&client);
    }

    game_client_shutdown(&client);
    CloseWindow();

    printf("Client application finished\n");
    return 0;
}
