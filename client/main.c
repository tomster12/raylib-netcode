// cbuild: -I../libs/raylib/include -L../libs/raylib/lib -I../shared
// cbuild: -lraylib -lm ../shared/implcore.c implclient.c netclient.c

#include <stdio.h>      // For printf
#include <stdlib.h>     // For malloc, free
#include <string.h>     // For memset
#include <unistd.h>     // For usleep
#include <arpa/inet.h>  // For inet_ntop, inet_pton
#include <sys/socket.h> // For socket functions

#include "raylib.h"
#include "implcore.h"
#include "implclient.h"
#include "netclient.h"

#define PORT 32000

int main()
{
    setbuf(stdout, NULL);
    printf("Starting client...\n");

    // Setup Raylib window
    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(10);
    InitWindow(800, 800, "Raylib Netcode");

    // Setup game state and buffer
    int local_player_id = 0;
    NetBuffer net_buf;
    net_buffer_init(&net_buf);

    // Setup game client
    if (net_client_init("127.0.0.1", PORT, &net_buf) != 0)
    {
        fprintf(stderr, "Failed to connect to server\n");
        CloseWindow();
        return 1;
    }

    // Game simulation tick and rendering loop
    while (!WindowShouldClose())
    {
        printf("Client frame: %u, Server frame: %u\n", net_buf.client_frame, net_buf.server_frame);

        // Grab current frame and events
        GameState *game_state = net_buffer_get_state(&net_buf, net_buf.client_frame);
        GameEvents *game_events = net_buffer_get_events(&net_buf, net_buf.client_frame);
        memset(game_events, 0, sizeof(GameEvents));

        // Handle game events
        game_handle_events(game_state, game_events, local_player_id);

        // Perform game simulation
        GameState *game_state_next = net_buffer_get_state(&net_buf, net_buf.client_frame + 1);
        game_simulate(game_state, game_events, game_state_next);

        // Draw the game state
        BeginDrawing();
        ClearBackground(RAYWHITE);
        game_render(game_state_next, local_player_id);
        DrawFPS(10, 10);
        EndDrawing();

        net_buf.client_frame++;
        net_buf.server_frame++; // TODO: Remove this line when server updates are handled properly

        // Send this frames events to the server
        net_client_send_events(net_buf.client_frame - 1, game_events);
    }

    printf("Shutting down client...\n");
    net_client_shutdown();
    CloseWindow();
    return 0;
}
