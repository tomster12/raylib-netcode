#include "gameclient.h"
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int game_client_init(GameClient *client, const char *server_ip, int port)
{
    // Initialize game client state
    client->to_shutdown = 0;
    client->socket_fd = -1;
    client->recv_thread = 0;
    client->client_player_id = 0;
    client->server_frame = 0;
    client->client_frame = 0;
    memset(client->states, 0, sizeof(client->states));
    memset(client->events, 0, sizeof(client->events));

    // Connect to the server
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    if (connect(client->socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connect");
        close(client->socket_fd);
        return 1;
    }

    printf("Game client connected to %s:%d (fd=%d)\n", server_ip, port, client->socket_fd);

    // Start the server listening thread
    if (pthread_create(&client->recv_thread, NULL, game_client_recv_thread, client) != 0)
    {
        perror("Failed to create receive thread");
        close(client->socket_fd);
        return 1;
    }

    printf("Receive thread started (thread=%lu)\n", client->recv_thread);

    return 0;
}

void game_client_shutdown(GameClient *client)
{
    client->to_shutdown = 1;

    // Close the connection socket
    if (client->socket_fd >= 0)
    {
        printf("Closing client socket (fd=%d)\n", client->socket_fd);
        shutdown(client->socket_fd, SHUT_RDWR);
        close(client->socket_fd);
        client->socket_fd = -1;
    }

    // Wait for the listening thread to finish
    if (client->recv_thread)
    {
        printf("Waiting for receive thread to finish (thread=%lu)\n", client->recv_thread);
        pthread_join(client->recv_thread, NULL);
    }

    printf("Game client shutdown\n");
}

GameState *game_client_get_state(GameClient *client, uint32_t frame)
{
    return &client->states[frame % MAX_ROLLBACK];
}

GameEvents *game_client_get_events(GameClient *client, uint32_t frame)
{
    return &client->events[frame % MAX_ROLLBACK];
}

void *game_client_recv_thread(void *arg)
{
    GameClient *client = (GameClient *)arg;

    while (!client->to_shutdown)
    {
        // TODO
    }

    printf("Client receive thread shutdown\n");
    return NULL;
}

void game_client_tick(GameClient *client)
{
    // TODO

    client->client_frame++;
    client->server_frame++;

    printf("Client tick: %u, Server frame: %u\n", client->client_frame, client->server_frame);
}
