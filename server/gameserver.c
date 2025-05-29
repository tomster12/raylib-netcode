#include "gameserver.h"
#include "shared/globals.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int game_server_init(GameServer *server, int port)
{
    // Initialize game client state
    server->to_shutdown = false;
    server->socket_fd = -1;
    server->simulation_thread = 0;
    server->client_accept_thread = 0;
    server->client_sockets_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    memset(server->client_data, 0, sizeof(server->client_data));
    server->client_count = 0;
    server->server_frame = 0;

    // Start a detached thread for simulation
    if (pthread_create(&server->simulation_thread, NULL, game_simulation_thread, server) != 0)
    {
        perror("Failed to create simulation thread");
        return 1;
    }

    printf("Simulation thread started (thread=%lu)\n", server->simulation_thread);

    // Bind server socket to localhost:PORT
    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd < 0)
    {
        perror("Failed to create server socket");
        return 1;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(server->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Failed to bind server socket");
        close(server->socket_fd);
        return 1;
    }
    if (listen(server->socket_fd, SERVER_LISTEN_BACKLOG) < 0)
    {
        perror("Failed to listen on server socket");
        close(server->socket_fd);
        return 1;
    }

    printf("Server listening on ANY:%d (fd=%d)\n", port, server->socket_fd);

    // Begin accepting client connections
    pthread_mutex_init(&server->client_sockets_mutex, NULL);
    if (pthread_create(&server->client_accept_thread, NULL, game_server_accept_thread, server) != 0)
    {
        perror("Failed to create client accept thread");
        game_server_shutdown(server);
        return 1;
    }

    printf("Client accept thread started (thread=%lu)\n", server->client_accept_thread);

    return 0;
}

void game_server_shutdown(GameServer *server)
{
    server->to_shutdown = true;

    // Close the server socket
    if (server->socket_fd > 0)
    {
        printf("Closing server socket (fd=%d)\n", server->socket_fd);
        shutdown(server->socket_fd, SHUT_RDWR);
        close(server->socket_fd);
        server->socket_fd = -1;
    }

    // Wait for threads to finish
    if (server->simulation_thread)
    {
        printf("Waiting for simulation loop to finish (thread=%lu)\n", server->simulation_thread);
        pthread_join(server->simulation_thread, NULL);
    }
    if (server->client_accept_thread)
    {
        printf("Waiting for client accept loop to finish (thread=%lu)\n", server->client_accept_thread);
        pthread_join(server->client_accept_thread, NULL);
    }

    // Close all client sockets
    if (server->client_count > 0)
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (server->client_data[i].is_connected)
            {
                printf("Waiting for client to finish (thread=%lu fd=%d)\n", server->client_data[i].thread_id, server->client_data[i].fd);
                shutdown(server->client_data[i].fd, SHUT_RDWR);
                pthread_join(server->client_data[i].thread_id, NULL);
            }
        }
    }
    pthread_mutex_destroy(&server->client_sockets_mutex);
    server->client_count = 0;

    printf("Game server shutdown\n");
}

void *game_server_accept_thread(void *arg)
{
    GameServer *server = (GameServer *)arg;

    while (!server->to_shutdown)
    {
        // Accept a new client connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server->socket_fd, NULL, NULL);
        if (client_fd < 0)
        {
            perror("Failed to accept client connection");
            continue;
        }

        // Do not allow more than MAX_CLIENTS clients
        if (server->client_count >= MAX_CLIENTS)
        {
            printf("Maximum client limit reached (%d), rejecting new client (fd=%d)\n", MAX_CLIENTS, client_fd);
            close(client_fd);
            continue;
        }

        // Find the first available slot for a new client
        int client_index = -1;
        pthread_mutex_lock(&server->client_sockets_mutex);
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (server->client_data[i].is_connected == false)
            {
                client_index = i;
                break;
            }
        }
        pthread_mutex_unlock(&server->client_sockets_mutex);

        // If no slot was available then reject the client
        if (client_index < 0)
        {
            printf("No available client slots, rejecting new client (fd=%d)\n", client_fd);
            close(client_fd);
            continue;
        }

        // Assign client to this new slot
        pthread_mutex_lock(&server->client_sockets_mutex);
        ClientData *client_data = &server->client_data[client_index];
        client_data->is_connected = true;
        client_data->fd = client_fd;
        client_data->index = client_index;
        server->client_count++;
        pthread_mutex_unlock(&server->client_sockets_mutex);

        // Start a new detached thread to handle the client
        ClientThreadArgs *args = malloc(sizeof(ClientThreadArgs));
        args->server = server;
        args->index = client_index;
        if (pthread_create(&client_data->thread_id, NULL, game_server_client_thread, args) != 0)
        {
            perror("Failed to create client thread");
            close(client_fd);
            free(args);
            continue;
        }

        printf("Accepted new client from %s:%d (fd=%d, index=%d, thread=%lu)\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd, client_index, client_data->thread_id);
    }

    printf("Client accept thread shutting down\n");
}

void *game_server_client_thread(void *arg)
{
    ClientThreadArgs *args = (ClientThreadArgs *)arg;
    GameServer *server = args->server;
    ClientData *client_data = &server->client_data[args->index];
    free(args);

    // Listen for client data until they disconnect
    uint8_t buffer[MAX_MESSAGE_SIZE];
    while (client_data->is_connected && !server->to_shutdown)
    {
        ssize_t received = recv(client_data->fd, buffer, sizeof(buffer), 0);
        if (received <= 0) break;

        printf("Received %ld bytes from client %d\n", received, client_data->fd);

        // TODO
    }

    // Client disconnected, remove from connected clients
    printf("Client finished (thread=%lu fd=%d)\n", client_data->thread_id, client_data->fd);

    pthread_mutex_lock(&server->client_sockets_mutex);
    client_data->is_connected = false;
    server->client_count--;
    pthread_mutex_unlock(&server->client_sockets_mutex);

    if (client_data->fd)
    {
        close(client_data->fd);
        client_data->fd = -1;
    }

    return NULL;
}

void *game_simulation_thread(void *arg)
{
    GameServer *server = (GameServer *)arg;

    while (!server->to_shutdown)
    {
        usleep(1000000 / 15);

        // TODO
    }

    printf("Simulation thread shutting down\n");
    return NULL;
}
