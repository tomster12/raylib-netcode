#include "gameclient.h"
#include "shared/protocol.h"
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
    client->is_connected = false;
    client->to_shutdown = 0;
    client->socket_fd = -1;
    client->recv_thread = 0;
    client->client_player_id = 0;

    client->last_confirmed_frame = 0;
    client->current_frame = 0;
    memset(client->states, 0, sizeof(client->states));
    memset(client->events, 0, sizeof(client->events));
    memset(client->frame_confirmed, 0, sizeof(client->frame_confirmed));

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
    client->is_connected = true;

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
    client->is_connected = false;

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
        char buf[MAX_MESSAGE_SIZE];
        ssize_t n = recv(client->socket_fd, buf, sizeof(buf), 0);
        if (n == 0)
        {
            printf("Server closed connection\n");
            client->is_connected = false;
            break;
        }
        if (n < 0)
        {
            perror("recv");
            client->is_connected = false;
            break;
        }

        // Read in the message header
        MessageHeader header;
        memcpy(&header, buf, sizeof(header));
        switch (header.type)
        {
        case MSG_S2P_ASSIGN_ID:
        {
            uint32_t assigned_id;
            deserialize_assign_id((uint8_t *)buf, n, &assigned_id);
            client->client_player_id = assigned_id;
            printf("Assigned player ID: %u\n", assigned_id);

            // Initialize frame 0 with this player spawned in
            GameState *initial_state = game_client_get_state(client, 0);
            memset(initial_state, 0, sizeof(GameState));
            game_player_join(initial_state, assigned_id);

            client->last_confirmed_frame = 0;
            client->current_frame = 0;
            client->frame_confirmed[0] = true;

            printf("Client initialized at frame 0\n");
            break;
        }

        case MSG_SB_PLAYER_JOINED:
        {
            uint32_t joined_player_id;
            deserialize_player_joined_left((uint8_t *)buf, n, MSG_SB_PLAYER_JOINED, &joined_player_id);
            printf("Player %u joined\n", joined_player_id);

            // Add this player to the current state
            GameState *current_state = game_client_get_state(client, client->current_frame);
            game_player_join(current_state, joined_player_id);
            break;
        }

        case MSG_SB_PLAYER_LEFT:
        {
            uint32_t left_player_id;
            deserialize_player_joined_left((uint8_t *)buf, n, MSG_SB_PLAYER_LEFT, &left_player_id);
            printf("Player %u left\n", left_player_id);

            // Remove this player from the current state
            GameState *current_state = game_client_get_state(client, client->current_frame);
            game_player_leave(current_state, left_player_id);
            break;
        }

        case MSG_S2P_FRAME_EVENTS:
        {
            uint32_t server_frame;
            GameEvents server_events;
            deserialize_frame_events((uint8_t *)buf, n, &server_frame, &server_events);
            printf("Received authoritative frame %u from server\n", server_frame);

            // Store the authoritative inputs
            GameEvents *local_events = game_client_get_events(client, server_frame);
            *local_events = server_events;
            client->frame_confirmed[server_frame % MAX_ROLLBACK] = true;

            // TODO: Check if prediction was wrong, rollback if needed
            // For now, just update last confirmed
            if (server_frame > client->last_confirmed_frame)
            {
                client->last_confirmed_frame = server_frame;
            }
            break;
        }

        default:
            printf("Unknown message type: %d\n", header.type);
            break;
        }
    }

    printf("Client receive thread shutdown\n");
    return NULL;
}

void game_client_update_server(GameClient *client)
{
    // Don't send anything until we have a player ID
    if (client->client_player_id == -1)
    {
        return;
    }

    // Get the events for this frame
    GameEvents *events = game_client_get_events(client, client->current_frame);
    PlayerInput *input = &events->player_inputs[client->client_player_id];

    // Serialize and send to server
    uint8_t buffer[MAX_MESSAGE_SIZE];
    size_t msg_size = serialize_player_events(
        buffer,
        client->current_frame,
        client->client_player_id,
        input);

    ssize_t sent = send(client->socket_fd, buffer, msg_size, 0);
    if (sent < 0)
    {
        perror("Failed to send player input");
        client->is_connected = false;
        return;
    }

    client->current_frame++;

    printf("Client tick: %u (sent %zd bytes)\n", client->current_frame, sent);
}
