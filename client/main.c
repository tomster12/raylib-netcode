// cbuild: -I../libs/raylib/include -L../libs/raylib/lib -I../
// cbuild: -lraylib -lm ../shared/gameimpl.c ../shared/protocol.c ../shared/log.c gameimpl.c gameclient.c

#include "gameclient.h"
#include "gameimpl.h"
#include "raylib.h"
#include "../shared/gameimpl.h"
#include "../shared/globals.h"
#include "../shared/log.h"
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
        if (!atomic_load_explicit(&client.is_initialised, memory_order_acquire)) continue;

        // Do the main simulation in a tight lock to allow receiving data
        GameState next_state_copy;
        GameEvents current_events_copy;
        pthread_mutex_lock(&client.state_lock);
        {
            // Error state if client is too far ahead of server
            if (client.client_frame >= client.sync_frame + FRAME_BUFFER_SIZE)
            {
                printf("WARN: Client frame %u further than buffer size %d from sync frame %u", client.client_frame, FRAME_BUFFER_SIZE, client.sync_frame);
                usleep(1000 * 1000);
                pthread_mutex_unlock(&client.state_lock);
                continue;
            }

            // Read in local events and simulate another frame
            GameState *current_state = &client.states[client.client_frame % FRAME_BUFFER_SIZE];
            GameEvents *current_events = &client.events[client.client_frame % FRAME_BUFFER_SIZE];
            GameState *next_state = &client.states[(client.client_frame + 1) % FRAME_BUFFER_SIZE];

            log_timestamp();
            printf("Client simulating frame %u\n", client.client_frame);
            memset(current_events, 0, sizeof(GameEvents));
            game_handle_events(current_state, current_events, client.client_index);

            game_simulate(current_state, current_events, next_state);
            client.client_frame++;

            // Copy immutable state and events
            next_state_copy = *next_state;
            current_events_copy = *current_events;
        }
        pthread_mutex_unlock(&client.state_lock);

        // Send the local events to the server
        game_client_send_game_events(&client, client.client_frame - 1, &current_events_copy);

        // Render the new generated frame
        BeginDrawing();
        ClearBackground(RAYWHITE);
        game_render(&next_state_copy, client.client_index);
        DrawFPS(10, 10);
        EndDrawing();
    }

    printf("\nShutting down the game client\n");
    game_client_shutdown(&client);
    CloseWindow();
    return 0;
}
