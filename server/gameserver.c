#include "gameserver.h"
#include "shared/gameimpl.h"
#include "shared/globals.h"
#include "shared/protocol.h"
#include <arpa/inet.h>
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
    pthread_cond_init(&server->frame_ready_cond, NULL);

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
    atomic_store(&server->to_shutdown, true);

    // Close the server socket
    if (server->socket_fd > 0)
    {
        printf("Closing server socket\n");
        shutdown(server->socket_fd, SHUT_RDWR);
        close(server->socket_fd);
        server->socket_fd = -1;
    }

    // Wait for threads to finish
    if (server->simulation_thread)
    {
        printf("Waiting for simulation loop to finish\n");
        pthread_join(server->simulation_thread, NULL);
    }

    if (server->client_accept_thread)
    {
        printf("Waiting for client accept loop to finish\n");
        pthread_join(server->client_accept_thread, NULL);
    }

    // Close all client sockets
    if (server->client_count > 0)
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (server->client_data[i].is_connected)
            {
                printf("Waiting for client %d to finish\n", i);

                shutdown(server->client_data[i].fd, SHUT_RDWR);
                pthread_join(server->client_data[i].thread_id, NULL);
                close(server->client_data[i].fd);

                server->client_data[i].fd = -1;
                server->client_data[i].is_connected = false;
            }
        }
    }

    pthread_mutex_destroy(&server->clients_lock);
    pthread_mutex_destroy(&server->state_lock);
    pthread_cond_destroy(&server->frame_ready_cond);

    printf("Game server shutdown\n");
}

void broadcast_to_all_clients(GameServer *server, const uint8_t *buffer, size_t size, int exclude_fd)
{
    pthread_mutex_lock(&server->clients_lock);
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (server->client_data[i].is_connected && server->client_data[i].fd != exclude_fd)
            {
                ssize_t sent = send(server->client_data[i].fd, buffer, size, 0);
                if (sent < 0)
                {
                    printf("Failed to broadcast to client %d: %d", i, sent);
                }
            }
        }
    }
    pthread_mutex_unlock(&server->clients_lock);
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

        printf("Accepted new connection from %s:%d (fd=%d)\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);

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

            client_data->is_connected = true;
            client_data->fd = client_fd;
            client_data->index = client_index;
            server->client_count++;

            printf("Assigned client to slot %d (fd=%d)\n", client_fd, client_index);

            // Start client thread to handle this connection
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
                close(client_fd);
                free(args);
                continue;
            }

            printf("Client thread started (thread=%lu)\n", client_data->thread_id);
        }
        pthread_mutex_unlock(&server->clients_lock);
    }

    printf("Client accept thread shutting down\n");
    return NULL;
}

void *game_server_client_thread(void *arg)
{
    ClientThreadArgs *args = (ClientThreadArgs *)arg;
    GameServer *server = args->server;
    int client_index = args->index;
    ClientData *client_data = &server->client_data[client_index];
    free(args);

    printf("Client thread started for slot %d (fd=%d)\n", client_data->index, client_data->fd);

    // Send player ID assignment
    uint8_t msg_buffer[MAX_MESSAGE_SIZE];
    size_t msg_size = serialize_assign_id(msg_buffer, client_index);
    if (send(client_data->fd, msg_buffer, msg_size, 0) < 0)
    {
        perror("Failed to send player ID");
        goto cleanup;
    }
    printf("Sent player ID %u to client (fd=%d)\n", client_index, client_data->fd);

    // Add player to game state
    pthread_mutex_lock(&server->state_lock);
    {
        GameState *current_state = &server->game_states[server->server_frame % MAX_ROLLBACK];
        game_player_join(current_state, client_index);
    }
    pthread_mutex_unlock(&server->state_lock);

    // Broadcast to all other clients that this player joined
    msg_size = serialize_player_joined(msg_buffer, client_index);
    broadcast_to_all_clients(server, msg_buffer, msg_size, client_data->fd);
    printf("Broadcasted player %u joined\n", client_index);

    // Listen and wait for client input
    uint8_t buffer[MAX_MESSAGE_SIZE];
    while (client_data->is_connected && !atomic_load(&server->to_shutdown))
    {
        ssize_t received = recv(client_data->fd, buffer, sizeof(buffer), 0);
        if (received <= 0) break;

        // Assume that it is a MSG_P2S_PLAYER_EVENTS
        uint32_t frame;
        uint32_t recv_index;
        PlayerInput input;
        deserialize_player_events(buffer, received, &frame, &recv_index, &input);

        // Validate player ID matches
        if (recv_index != client_index)
        {
            printf("Warning: Client %u sent input for player %u\n", client_index, recv_index);
            continue;
        }

        printf("Received input from player %u for frame %u: [%d,%d,%d,%d]\n",
               client_index, frame,
               input.movements_held[0], input.movements_held[1],
               input.movements_held[2], input.movements_held[3]);

        pthread_mutex_lock(&server->state_lock);
        {
            // Store the input
            GameEvents *events = &server->game_events[frame % MAX_ROLLBACK];
            events->player_inputs[client_index] = input;

            // Mark this client as ready
            client_data->ready_for_frame = true;
            client_data->last_received_frame = frame;

            // Signal simulation thread if all clients are now ready
            if (all_clients_ready_for_frame(server))
            {
                pthread_cond_signal(&server->frame_ready_cond);
            }
        }
        pthread_mutex_unlock(&server->state_lock);
    }

cleanup:
    // Client disconnected
    printf("Client disconnecting (thread=%lu fd=%d player=%u)\n",
           client_data->thread_id, client_data->fd, client_index);

    // Remove from connected clients list
    pthread_mutex_lock(&server->clients_lock);
    {
        client_data->is_connected = false;
        server->client_count--;
    }
    pthread_mutex_unlock(&server->clients_lock);

    // Remove player from game state
    pthread_mutex_lock(&server->state_lock);
    {
        GameState *current_state = &server->game_states[server->server_frame % MAX_ROLLBACK];
        game_player_leave(current_state, client_index);
    }
    pthread_mutex_unlock(&server->state_lock);

    // Broadcast player left
    msg_size = serialize_player_left(msg_buffer, client_index);
    broadcast_to_all_clients(server, msg_buffer, msg_size, -1);
    printf("Broadcasted player %u left\n", client_index);

    if (client_data->fd >= 0)
    {
        close(client_data->fd);
        client_data->fd = -1;
    }

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
            while (!atomic_load(&server->to_shutdown) && !all_clients_ready_for_frame(server))
            {
                pthread_cond_wait(&server->frame_ready_cond, &server->state_lock);
            }

            if (atomic_load(&server->to_shutdown))
            {
                pthread_mutex_unlock(&server->state_lock);
                break;
            }

            // Simulate the frame with all clients events
            uint32_t current_frame = server->server_frame;
            uint32_t next_frame = current_frame + 1;
            GameState *current_state = &server->game_states[current_frame % MAX_ROLLBACK];
            GameEvents *current_events = &server->game_events[current_frame % MAX_ROLLBACK];
            GameState *next_state = &server->game_states[next_frame % MAX_ROLLBACK];

            printf("Server simulating frame %u\n", current_frame);

            game_simulate(current_state, current_events, next_state);

            uint8_t buffer[MAX_MESSAGE_SIZE];
            size_t msg_size = serialize_frame_events(buffer, current_frame, current_events);
            broadcast_to_all_clients(server, buffer, msg_size, -1);

            server->server_frame = next_frame;
            memset(&server->game_events[next_frame % MAX_ROLLBACK], 0, sizeof(GameEvents));
        }
        pthread_mutex_unlock(&server->state_lock);
    }

    printf("Client simulation_thread shutting down\n");
    return NULL;
}

bool all_clients_ready_for_frame(GameServer *server)
{
    pthread_mutex_lock(&server->clients_lock);
    {
        if (server->client_count == 0) return false;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (server->client_data[i].is_connected && !server->client_data[i].ready_for_frame)
            {
                pthread_mutex_unlock(&server->clients_lock);
                return false;
            }
        }
    }
    pthread_mutex_unlock(&server->clients_lock);
    return true;
}
