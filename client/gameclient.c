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
    atomic_init(&client->to_shutdown, false);
    atomic_init(&client->is_connected, false);
    atomic_init(&client->is_initialised, false);

    client->socket_fd = -1;
    client->recv_thread = 0;
    pthread_mutex_init(&client->state_lock, NULL);

    client->client_player_id = -1;
    client->sync_frame = -1;
    client->server_frame = -1;
    client->client_frame = -1;
    memset(client->states, 0, sizeof(client->states));
    memset(client->events, 0, sizeof(client->events));

    // Ceate socket and connect to localhost:PORT
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0)
    {
        perror("socket()");
        return 1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    int ret = connect(client->socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret != 0)
    {
        perror("connect()");
        close(client->socket_fd);
        return 1;
    }

    // Start server listening thread
    ret = pthread_create(&client->recv_thread, NULL, game_client_recv_thread, client);
    if (ret != 0)
    {
        perror("pthread_create()");
        game_client_shutdown(client);
        return 1;
    }

    printf("Game client connected to %s:%d (fd=%d, listen=%lu)\n", server_ip, port, client->socket_fd, client->recv_thread);
    atomic_store(&client->is_connected, true);
    return 0;
}

void game_client_shutdown(GameClient *client)
{
    // Signal to threads to stop
    atomic_store(&client->to_shutdown, true);
    atomic_store(&client->is_connected, false);

    // Close the connection socket
    if (client->socket_fd >= 0)
    {
        printf("Closing client socket\n");
        shutdown(client->socket_fd, SHUT_RDWR);
        close(client->socket_fd);
    }

    // Wait for the listening thread
    if (client->recv_thread)
    {
        printf("Waiting for receive thread\n");
        pthread_join(client->recv_thread, NULL);
    }

    printf("Game client shutdown\n");
}

void *game_client_recv_thread(void *arg)
{
    GameClient *client = (GameClient *)arg;

    // Keep receiving until signalled to shutdown
    uint8_t buffer[MAX_MESSAGE_SIZE];
    while (!atomic_load(&client->to_shutdown))
    {
        ssize_t received = recv(client->socket_fd, buffer, sizeof(buffer), 0);
        if (received <= 0) break;

        // Read in the message header then switch on message type
        MessageHeader header;
        assert(received >= sizeof(MessageHeader));
        memcpy(&header, buffer, sizeof(header));

        game_client_handle_payload(client, &header, buffer, (size_t)received);
    }

    atomic_store(&client->is_connected, false);
    printf("Client recv_thread shutdown\n");
    return NULL;
}

void game_client_handle_payload(GameClient *client, MessageHeader *header, char *buf, size_t n)
{
    switch (header->type)
    {
    case MSG_S2P_ASSIGN_ID:
    {
        uint32_t assigned_id;
        deserialize_assign_id((uint8_t *)buf, n, &assigned_id);
        printf("Received MSG_S2P_ASSIGN_ID as player %u\n", assigned_id);

        // Initialize frame 0 with this player spawned in
        pthread_mutex_lock(&client->state_lock);
        {
            GameState *initial_state = &client->states[0];
            memset(initial_state, 0, sizeof(GameState));
            game_player_join(initial_state, assigned_id);

            client->client_player_id = assigned_id;
            client->sync_frame = 0;
            client->server_frame = 0;
            client->client_frame = 0;
        }
        pthread_mutex_unlock(&client->state_lock);

        atomic_store_explicit(&client->is_initialised, true, memory_order_release);
        break;
    }

    case MSG_SB_PLAYER_JOINED:
    {
        uint32_t joined_player_id;
        deserialize_player_joined_left((uint8_t *)buf, n, MSG_SB_PLAYER_JOINED, &joined_player_id);
        printf("Received MSG_SB_PLAYER_JOINED for player %u\n", joined_player_id);

        pthread_mutex_lock(&client->state_lock);
        {
            GameState *current_state = &client->states[client->client_frame];
            game_player_join(current_state, joined_player_id);
        }
        pthread_mutex_unlock(&client->state_lock);
        break;
    }

    case MSG_SB_PLAYER_LEFT:
    {
        uint32_t left_player_id;
        deserialize_player_joined_left((uint8_t *)buf, n, MSG_SB_PLAYER_LEFT, &left_player_id);
        printf("Received MSG_SB_PLAYER_LEFT for player %u\n", left_player_id);

        pthread_mutex_lock(&client->state_lock);
        {
            GameState *current_state = &client->states[client->client_frame];
            game_player_leave(current_state, left_player_id);
        }
        pthread_mutex_unlock(&client->state_lock);
        break;
    }

    case MSG_S2P_FRAME_EVENTS:
    {
        uint32_t server_frame;
        GameEvents server_frame_events;
        deserialize_frame_events((uint8_t *)buf, n, &server_frame, &server_frame_events);
        printf("Received MSG_S2P_FRAME_EVENTS for frame %u\n", server_frame);

        pthread_mutex_lock(&client->state_lock);
        {
            if (server_frame != client->server_frame + 1)
            {
                printf("Server frame %u unexpected, expected %d\n", server_frame, client->server_frame + 1);
                pthread_mutex_unlock(&client->state_lock);
                break;
            }

            // Copy events into the game client for server frame
            GameEvents *frame_events = &client->events[server_frame % FRAME_BUFFER_SIZE];
            *frame_events = server_frame_events;
            client->server_frame = server_frame;

            game_client_reconcile_frames(client);
        }
        pthread_mutex_unlock(&client->state_lock);
        break;
    }

    default:
        printf("Unknown message type: %d\n", header->type);
        break;
    }
}

void game_client_reconcile_frames(GameClient *client)
{
    // Reconcile from sync_frame -> server_frame with new real data
    // Then from server_frame -> client_frame with local data
}

void game_client_send_server_events(GameClient *client, uint32_t frame)
{
    // Serialize and send to server
    GameEvents *events = &client->events[frame % FRAME_BUFFER_SIZE];

    uint8_t buffer[MAX_MESSAGE_SIZE];
    size_t msg_size = serialize_player_events(
        buffer,
        frame,
        client->client_player_id,
        events);

    ssize_t sent = send(client->socket_fd, buffer, msg_size, 0);
    if (sent < 0)
    {
        printf("Client failed to send frame %u", frame);
        atomic_store(&client->is_connected, false);
        return;
    }

    printf("Sent MSG_P2S_PLAYER_EVENTS for frame %u\n", frame);
}
