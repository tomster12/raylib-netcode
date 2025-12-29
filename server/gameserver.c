#include "gameserver.h"
#include "shared/gameimpl.h"
#include "shared/globals.h"
#include "shared/protocol.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int game_server_init(GameServer *server, int port)
{
    // Initialize game server state
    atomic_init(&server->to_shutdown, 0);

    server->socket_fd = -1;
    server->simulation_thread = 0;
    server->client_accept_thread = 0;
    pthread_mutex_init(&server->clients_lock, NULL);
    pthread_mutex_init(&server->state_lock, NULL);
    pthread_cond_init(&server->simulation_loop_cond, NULL);

    server->client_count = 0;
    server->server_frame = 0;
    memset(server->client_data, 0, sizeof(server->client_data));
    memset(server->game_states, 0, sizeof(server->game_states));
    memset(server->game_events, 0, sizeof(server->game_events));

    // Create listening socket on localhost:PORT
    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd < 0)
    {
        perror("socket()");
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    int ret = bind(server->socket_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret != 0)
    {
        perror("bind()");
        close(server->socket_fd);
        return 1;
    }

    // Listen on this main thread
    ret = listen(server->socket_fd, SERVER_LISTEN_BACKLOG);
    if (ret != 0)
    {
        perror("listen()");
        close(server->socket_fd);
        return 1;
    }

    // Start client handling thread
    ret = pthread_create(&server->client_accept_thread, NULL, game_server_accept_thread, server);
    if (ret != 0)
    {
        perror("pthread_create() client_accept_thread");
        game_server_shutdown(server);
        return 1;
    }

    // Start simulation thread
    ret = pthread_create(&server->simulation_thread, NULL, game_simulation_thread, server);
    if (ret != 0)
    {
        perror("pthread_create() simulation_thread");
        game_server_shutdown(server);
        return 1;
    }

    printf("Server listening on localhost:%d (fd=%d, accept=%lu, sim=%lu)\n", port, server->socket_fd, server->client_accept_thread, server->simulation_thread);
    return 0;
}

void game_server_shutdown(GameServer *server)
{
    // Signal shutdown, close server socket, close client sockets, and signal conditions
    // This should cause each of the threads to exit out
    atomic_store(&server->to_shutdown, true);
    if (server->socket_fd > 0)
    {
        printf("Closing server socket\n");
        shutdown(server->socket_fd, SHUT_RDWR);
        close(server->socket_fd);
        server->socket_fd = -1;
    }
    if (server->client_count > 0)
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (server->client_data[i].is_connected)
            {
                shutdown(server->client_data[i].fd, SHUT_RDWR);
                close(server->client_data[i].fd);
            }
        }
    }

    pthread_cond_signal(&server->simulation_loop_cond);

    // Now wait for all the threads to exit correctly
    if (server->simulation_thread)
    {
        printf("Waiting for simulation loop\n");
        pthread_join(server->simulation_thread, NULL);
    }
    if (server->client_accept_thread)
    {
        printf("Waiting for client accept loop\n");
        pthread_join(server->client_accept_thread, NULL);
    }
    if (server->client_count > 0)
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (server->client_data[i].is_connected)
            {
                printf("Waiting for client %d\n", i);
                pthread_join(server->client_data[i].thread_id, NULL);
                server->client_data[i].fd = -1;
                server->client_data[i].is_connected = false;
            }
        }
    }

    // Finally cleanup open client sockets
    pthread_mutex_destroy(&server->clients_lock);
    pthread_mutex_destroy(&server->state_lock);
    pthread_cond_destroy(&server->simulation_loop_cond);

    printf("Game server shutdown\n");
}

void *game_server_accept_thread(void *arg)
{
    GameServer *server = (GameServer *)arg;

    while (!atomic_load(&server->to_shutdown))
    {
        // Accept a new client connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server->socket_fd, (struct sockaddr *)&client_addr, &client_len);

        if (atomic_load(&server->to_shutdown)) break;

        if (client_fd < 0)
        {
            printf("Failed to accept client connection");
            continue;
        }

        pthread_mutex_lock(&server->clients_lock);
        {
            // Do not allow more than MAX_CLIENTS clients
            if (server->client_count >= MAX_CLIENTS)
            {
                printf("Maximum client limit reached (%d), rejecting connection (fd=%d)\n", MAX_CLIENTS, client_fd);
                close(client_fd);
                pthread_mutex_unlock(&server->clients_lock);
                continue;
            }

            // Find first available slot
            ClientData *client_data = NULL;
            int client_index = -1;
            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (!server->client_data[i].is_connected)
                {
                    client_data = &server->client_data[i];
                    client_index = i;
                    pthread_mutex_unlock(&server->clients_lock);
                    break;
                }
            }

            // If no slot was available then reject the client
            if (client_data == NULL)
            {
                printf("No available client slots, rejecting connection (fd=%d)\n", client_fd);
                close(client_fd);
                continue;
            }

            // Assign to slot and start client thread
            client_data->is_connected = true;
            client_data->fd = client_fd;
            client_data->index = client_index;
            server->client_count++;

            ClientThreadArgs *args = malloc(sizeof(ClientThreadArgs));
            args->server = server;
            args->index = client_index;
            if (pthread_create(&client_data->thread_id, NULL, game_server_client_thread, args) != 0)
            {
                perror("Failed to create client thread");
                client_data->is_connected = false;
                client_data->fd = -1;
                client_data->index = -1;
                server->client_count--;
                pthread_mutex_unlock(&server->clients_lock);
                close(client_fd);
                free(args);
                continue;
            }

            printf("Accepted new client from %s:%d in slot %d (fd=%d, client=%zu)\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                   client_data->index, client_data->fd, client_data->thread_id);
        }
        pthread_mutex_unlock(&server->clients_lock);
    }

    printf("Client accept thread shutdown\n");
    return NULL;
}

void *game_server_client_thread(void *arg)
{
    ClientThreadArgs *args = (ClientThreadArgs *)arg;
    GameServer *server = args->server;
    int client_index = args->index;
    ClientData *client_data = &server->client_data[client_index];
    free(args);

    // Initialise player with MSG_S2P_INIT_PLAYER
    uint8_t msg_buffer[MAX_MESSAGE_SIZE];
    size_t msg_size = serialize_init_player(msg_buffer, client_index);
    ssize_t sent = send(client_data->fd, msg_buffer, msg_size, 0);
    if (sent < 0)
    {
        printf("Failed to send MSG_S2P_INIT_PLAYER to client %d\n", client_index);
        goto cleanup;
    }

    printf("Sent MSG_S2P_INIT_PLAYER to player %u\n", client_index);

    // Update local player events with PLAYER_EVENT_JOIN
    pthread_mutex_lock(&server->state_lock);
    {
        GameEvents *current_events = &server->game_events[server->server_frame % FRAME_BUFFER_SIZE];
        current_events->player_events[client_index] = PLAYER_EVENT_JOIN;
    }
    pthread_mutex_unlock(&server->state_lock);

    // Listen and wait for client input
    uint8_t buffer[MAX_MESSAGE_SIZE];
    while (&client_data->is_connected && !atomic_load(&server->to_shutdown))
    {
        // TODO: TCP receive into a receive buffer
        ssize_t received = recv(client_data->fd, buffer, sizeof(buffer), 0);
        if (received <= 0) break;

        // Assume that it is a MSG_P2S_GAME_EVENTS
        uint32_t client_frame;
        uint32_t recv_index;
        PlayerInput input;
        deserialize_game_events(buffer, received, &client_frame, &recv_index, &input);
        assert(recv_index == client_index);
        printf("Received MSG_P2S_GAME_EVENTS for frame %u from player %u\n", client_frame, client_index);

        // Lock client and state while we handle storing the clients frame
        pthread_mutex_lock(&server->state_lock);
        {
            pthread_mutex_lock(&server->clients_lock);
            {
                // Error if client is behind the server
                if (client_frame < server->server_frame)
                {
                    printf("WARN: Client frame %u is behind the server frame %u, IGNORING DATA", client_frame, server->server_frame);
                    pthread_mutex_unlock(&server->clients_lock);
                    pthread_mutex_unlock(&server->state_lock);
                    continue;
                }

                // Error if client is too far ahead of server
                if (client_frame >= server->server_frame + FRAME_BUFFER_SIZE)
                {
                    printf("WARN: Client frame %u further than buffer size %d from server frame %u, IGNORING DATA", client_frame, FRAME_BUFFER_SIZE, server->server_frame);
                    pthread_mutex_unlock(&server->clients_lock);
                    pthread_mutex_unlock(&server->state_lock);
                    continue;
                }

                // Write the clients frame data
                GameEvents *events = &server->game_events[client_frame % FRAME_BUFFER_SIZE];
                events->player_inputs[client_index] = input;
                client_data->client_frame = client_frame;
            }
            pthread_mutex_unlock(&server->clients_lock);

            // And everything is up to date we can try simulate
            if (game_server_can_simulate(server))
            {
                pthread_cond_signal(&server->simulation_loop_cond);
            }
        }
        pthread_mutex_unlock(&server->state_lock);
    }

cleanup:
    printf("Client disconnecting (thread=%lu fd=%d player=%u)\n",
           client_data->thread_id, client_data->fd, client_index);

    // Close client socket
    if (client_data->fd >= 0)
    {
        close(client_data->fd);
        client_data->fd = -1;
    }

    // Remove player from local game state
    pthread_mutex_lock(&server->state_lock);
    {
        GameEvents *current_events = &server->game_events[server->server_frame % FRAME_BUFFER_SIZE];
        current_events->player_events[client_index] = PLAYER_EVENT_LEAVE;
    }
    pthread_mutex_unlock(&server->state_lock);

    // Remove from the client data list
    pthread_mutex_lock(&server->clients_lock);
    {
        atomic_store(&client_data->is_connected, false);
        server->client_count--;
    }
    pthread_mutex_unlock(&server->clients_lock);

    printf("Client thread finished for player %u\n", client_index);
    return NULL;
}

void *game_simulation_thread(void *arg)
{
    GameServer *server = (GameServer *)arg;

    while (!atomic_load(&server->to_shutdown))
    {
        pthread_mutex_lock(&server->state_lock);
        {
            // Wait to have all clients frames
            while (!atomic_load(&server->to_shutdown) && !game_server_can_simulate(server))
            {
                pthread_cond_wait(&server->simulation_loop_cond, &server->state_lock);
            }
            if (atomic_load(&server->to_shutdown))
            {
                pthread_mutex_unlock(&server->state_lock);
                break;
            }

            // Simulate just the next server frame with all clients events
            GameState *current_state = &server->game_states[server->server_frame % FRAME_BUFFER_SIZE];
            GameEvents *current_events = &server->game_events[server->server_frame % FRAME_BUFFER_SIZE];
            GameState *next_state = &server->game_states[(server->server_frame + 1) % FRAME_BUFFER_SIZE];

            printf("Server simulating frame %u\n", server->server_frame);
            game_simulate(current_state, current_events, next_state);
            server->server_frame++;

            // Broadcast out final confirmed events to all clients
            uint8_t buffer[MAX_MESSAGE_SIZE];
            size_t msg_size = serialize_frame_events(buffer, server->server_frame, current_events);
            ssize_t sent = game_server_broadcast(server, buffer, msg_size, -1);
            printf("Broadcasted MSG_S2P_GAME_EVENTS for frame %d\n", server->server_frame);

            // Reset events for this frame for when we rollback around
            memset(&server->game_events[(server->server_frame + 1) % FRAME_BUFFER_SIZE], 0, sizeof(GameEvents));
        }
        pthread_mutex_unlock(&server->state_lock);
    }

    printf("Server simulation thread shutdown\n");
    return NULL;
}

ssize_t game_server_broadcast(GameServer *server, const uint8_t *buffer, size_t size, int exclude_fd)
{
    ssize_t total_sent = 0;
    pthread_mutex_lock(&server->clients_lock);
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (server->client_data[i].is_connected && server->client_data[i].fd != exclude_fd)
            {
                ssize_t sent = send(server->client_data[i].fd, buffer, size, 0);
                if (sent < 0) printf("Failed to broadcast to client %d: %zu", i, sent);
                total_sent += sent;
            }
        }
    }
    pthread_mutex_unlock(&server->clients_lock);
    return total_sent;
}

bool game_server_can_simulate(GameServer *server)
{
    // We can simulate 1 more server frame if:
    // - 1+ client is connected
    // - client_frame >= server_frame for all clients

    pthread_mutex_lock(&server->clients_lock);
    {
        if (server->client_count == 0)
        {
            pthread_mutex_unlock(&server->clients_lock);
            return false;
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (server->client_data[i].is_connected &&
                server->client_data[i].client_frame < server->server_frame)
            {
                pthread_mutex_unlock(&server->clients_lock);
                return false;
            }
        }
    }
    pthread_mutex_unlock(&server->clients_lock);
    return true;
}
