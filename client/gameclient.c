#include "gameclient.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int game_client_init(GameClient *client, const char *server_ip, int port)
{
    // Initialize game client state
    atomic_init(&client->to_shutdown, 0);
    client->is_connected = false;
    client->socket_fd = -1;
    client->recv_thread = 0;
    client->client_player_id = 0;

    atomic_init(&client->is_initialised, 0);
    client->last_confirmed_frame = 0;
    client->current_frame = 0;
    memset(client->states, 0, sizeof(client->states));
    memset(client->events, 0, sizeof(client->events));
    memset(client->frame_confirmed, 0, sizeof(client->frame_confirmed));

    // Ceate socket and connect to localhost:PORT
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) goto fail;

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    int ret = connect(client->socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret != 0) goto fail;

    // Start server listening thread
    ret = pthread_create(&client->recv_thread, NULL, game_client_recv_thread, client);
    if (ret != 0) goto fail;

    printf("Game client connected to %s:%d (fd=%d, listen=%lu)\n", server_ip, port, client->socket_fd, client->recv_thread);
    client->is_connected = true;
    return 0;

fail:
    perror("game_client_init");
    if (client->socket_fd >= 0) close(client->socket_fd);
    return 1;
}

void game_client_shutdown(GameClient *client)
{
    // Signal to threads to stop
    atomic_store(&client->to_shutdown, 1);
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

    // Keep receiving until signalled to shutdown
    while (!atomic_load(&client->to_shutdown))
    {
        char buf[MAX_MESSAGE_SIZE];
        ssize_t n = recv(client->socket_fd, buf, sizeof(buf), 0);

        // Handle socket recv failures
        if (n == 0)
        {
            // Socket closed intentionally
            client->is_connected = false;
            break;
        }
        else if (n < 0)
        {
            // signal interruption, retry
            if (errno == EINTR)
            {
                printf("Retry on errono=EINTR\n");
                continue;
            }

            // non-blocking socket case
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                printf("Retry on errono=EAGAIN or errono=EWOULDBLOCK\n");
                continue;
            }

            // Real error
            perror("recv");
            client->is_connected = false;
            break;
        }

        // Read in the message header then switch on message type
        MessageHeader header;
        assert(n >= sizeof(MessageHeader));
        memcpy(&header, buf, sizeof(header));
        game_client_handle_payload(client, &header, buf, (size_t)n);
    }

    printf("Client recv_thread shutting down\n");
    return NULL;
}

void game_client_handle_payload(GameClient *client, MessageHeader *header, char *buf, size_t n)
{
    assert(client->is_initialised);

    switch (header->type)
    {
    case MSG_S2P_ASSIGN_ID:
    {
        uint32_t assigned_id;
        deserialize_assign_id((uint8_t *)buf, n, &assigned_id);
        client->client_player_id = assigned_id;
        printf("Received MSG_S2P_ASSIGN_ID (%u)\n", assigned_id);

        // Initialize frame 0 with this player spawned in
        GameState *initial_state = game_client_get_state(client, 0);
        memset(initial_state, 0, sizeof(GameState));
        game_player_join(initial_state, assigned_id);

        atomic_store(&client->is_initialised, 1);
        client->last_confirmed_frame = 0;
        client->current_frame = 0;
        client->frame_confirmed[0] = true;
        break;
    }

    case MSG_SB_PLAYER_JOINED:
    {
        uint32_t joined_player_id;
        deserialize_player_joined_left((uint8_t *)buf, n, MSG_SB_PLAYER_JOINED, &joined_player_id);
        printf("Received MSG_SB_PLAYER_JOINED (%u)\n", joined_player_id);

        GameState *current_state = game_client_get_state(client, client->current_frame);
        game_player_join(current_state, joined_player_id);
        break;
    }

    case MSG_SB_PLAYER_LEFT:
    {
        uint32_t left_player_id;
        deserialize_player_joined_left((uint8_t *)buf, n, MSG_SB_PLAYER_LEFT, &left_player_id);
        printf("Received MSG_SB_PLAYER_LEFT (%u)\n", left_player_id);

        GameState *current_state = game_client_get_state(client, client->current_frame);
        game_player_leave(current_state, left_player_id);
        break;
    }

    case MSG_S2P_FRAME_EVENTS:
    {
        uint32_t server_frame;
        GameEvents server_events;
        deserialize_frame_events((uint8_t *)buf, n, &server_frame, &server_events);
        printf("Received MSG_S2P_FRAME_EVENTS (%u)\n", server_frame);

        GameEvents *local_events = game_client_get_events(client, server_frame);
        *local_events = server_events;

        // TODO: Rollback

        client->frame_confirmed[server_frame % MAX_ROLLBACK] = true;
        if (server_frame > client->last_confirmed_frame)
        {
            client->last_confirmed_frame = server_frame;
        }
        break;
    }

    default:
        printf("Unknown message type: %d\n", header->type);
        break;
    }
}

void game_client_update_server(GameClient *client)
{
    assert(client->is_initialised);

    // Get the events for this frame
    GameEvents *events = game_client_get_events(client, client->current_frame);

    // Serialize and send to server
    uint8_t buffer[MAX_MESSAGE_SIZE];
    size_t msg_size = serialize_player_events(
        buffer,
        client->current_frame,
        client->client_player_id,
        events);

    ssize_t sent = send(client->socket_fd, buffer, msg_size, 0);
    if (sent < 0)
    {
        printf("Failed to send player input: %u", client->current_frame);
        client->is_connected = false;
        return;
    }

    printf("Client updated server, frame=%u, sent %zd bytes\n", client->current_frame, sent);
    client->current_frame++;
}
