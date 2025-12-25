// cbuild: -I../libs/raylib/include -L../libs/raylib/lib -I../
// cbuild: -lraylib -lm ../shared/gameimpl.c ../shared/protocol.c gameimpl.c gameclient.c

#include "gameclient.h"
#include "gameimpl.h"
#include "raylib.h"
#include "shared/gameimpl.h"
#include "shared/globals.h"
#include <arpa/inet.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

volatile sig_atomic_t to_shutdown_app = 0;

void handle_sigaction(int signum)
{
    (void)signum;
    to_shutdown_app = 1;
}

void init_sigaction_handler(struct sigaction *sa)
{
    memset(sa, 0, sizeof(*sa));

    sa->sa_handler = handle_sigaction;
    sa->sa_flags = 0;

    sigaction(SIGINT, sa, NULL);
    sigaction(SIGTERM, sa, NULL);
}

int main()
{
    printf("Client application started\n");

    struct sigaction sa;
    init_sigaction_handler(&sa);

    SetTraceLogLevel(LOG_WARNING);
    SetTargetFPS(SIMULATION_TICK_RATE);
    InitWindow(800, 800, "Raylib Netcode");

    GameClient client;
    if (game_client_init(&client, "127.0.0.1", PORT) != 0)
    {
        perror("game_client_init");
        CloseWindow();
        return 1;
    }

    while (!WindowShouldClose() && !to_shutdown_app && atomic_load(&client.is_connected))
    {
        if (!atomic_load_explicit(&client.is_initialised, memory_order_acquire))
        {
            continue;
        }

        pthread_mutex_lock(&client.state_lock);
        {
            GameState *game_state = &client.states[client.client_frame % FRAME_BUFFER_SIZE];
            GameEvents *game_events = &client.events[client.client_frame % FRAME_BUFFER_SIZE];
            memset(game_events, 0, sizeof(GameEvents));

            game_handle_events(game_state, game_events, client.client_player_id);

            GameState *game_state_next = &client.states[client.client_frame + 1 % FRAME_BUFFER_SIZE];
            game_simulate(game_state, game_events, game_state_next);

            game_client_update_server(&client);

            BeginDrawing();
            ClearBackground(RAYWHITE);
            game_render(game_state_next, client.client_player_id);
            DrawFPS(10, 10);
            EndDrawing();
        }
        pthread_mutex_unlock(&client.state_lock);
    }

    printf("\nShutting down the game client\n");
    game_client_shutdown(&client);
    CloseWindow();
    return 0;
}
