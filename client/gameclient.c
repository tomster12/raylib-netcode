#include "gameclient.h"
#include "../shared/log.h"
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

    client->client_index = -1;
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

    log_printf("Game client connected to %s:%d (fd=%d, listen=%lu)\n", server_ip, port, client->socket_fd, client->recv_thread);
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
        log_printf("Closing client socket\n");
        shutdown(client->socket_fd, SHUT_RDWR);
        close(client->socket_fd);
    }

    // Wait for the listening thread
    if (client->recv_thread)
    {
        log_printf("Waiting for receive thread\n");
        pthread_join(client->recv_thread, NULL);
    }

    log_printf("Game client shutdown\n");
}

void *game_client_recv_thread(void *arg)
{
    GameClient *client = (GameClient *)arg;

    // Keep receiving until signalled to shutdown
    uint8_t buffer[MAX_MESSAGE_SIZE];
    while (!atomic_load(&client->to_shutdown))
    {
        ssize_t message_size = recv(client->socket_fd, buffer, sizeof(buffer), 0);
        if (message_size <= 0) break;

        // Peek and parse just the header so we can switch on the type
        // We make the assumption at this point 1 TCP recv = 1 game message
        assert(message_size >= sizeof(MessageHeader));

        MessageHeader header;
        memcpy(&header, buffer, sizeof(header));

        uint16_t payload_size = ntohs(header.payload_size);
        assert(message_size == (sizeof(header) + payload_size));

        // Now hand this over to be handled
        game_client_handle_payload(client, &header, buffer, (size_t)message_size);
    }

    atomic_store(&client->is_connected, false);
    log_printf("Client recv_thread shutdown\n");
    return NULL;
}

void game_client_handle_payload(GameClient *client, MessageHeader *header, char *buffer, size_t message_size)
{
    switch (header->type)
    {
    case MSG_S2P_INIT_PLAYER:
    {
        int frame;
        int client_index;
        GameState current_state;
        GameEvents current_events;
        deserialize_init_player((uint8_t *)buffer, message_size, &frame, &current_state, &current_events, &client_index);

        log_printf("Received MSG_S2P_INIT_PLAYER as player %u\n", client_index);

        // Initialize player with the given frame, events, state
        pthread_mutex_lock(&client->state_lock);
        {
            int frame = ntohl(header->frame);
            client->client_index = client_index;
            client->sync_frame = frame;
            client->server_frame = frame;
            client->client_frame = frame;
            client->states[client->sync_frame % FRAME_BUFFER_SIZE] = current_state;
            client->events[client->sync_frame % FRAME_BUFFER_SIZE] = current_events;
        }
        pthread_mutex_unlock(&client->state_lock);

        atomic_store_explicit(&client->is_initialised, true, memory_order_release);
        break;
    }

    case MSG_S2P_FRAME_GAME_EVENTS:
    {
        int frame;
        GameEvents server_frame_events;
        deserialize_s2p_frame_game_events((uint8_t *)buffer, message_size, &frame, &server_frame_events);

        log_printf("Received MSG_S2P_FRAME_GAME_EVENTS for frame %u\n", frame);

        pthread_mutex_lock(&client->state_lock);
        {
            // Expect to receive the servers next frame for now
            // If server was on frame 0 when we joined it will send that out again
            if (frame != client->server_frame + 1 && frame != 0)
            {
                log_printf("WARN: Server frame %u unexpected, expected %d or 0\n", frame, client->server_frame + 1);
                pthread_mutex_unlock(&client->state_lock);
                break;
            }

            // Overwrite local game events with servers
            client->server_frame = frame;
            client->events[frame % FRAME_BUFFER_SIZE] = server_frame_events;

            game_client_reconcile_frames(client);
        }
        pthread_mutex_unlock(&client->state_lock);
        break;
    }

    default:
        log_printf("Unknown message type: %d\n", header->type);
        break;
    }
}

void game_client_reconcile_frames(GameClient *client)
{
    // EXPECTS state_lock to be locked

    if (client->client_frame > client->server_frame || client->server_frame > client->sync_frame)
    {
        log_printf("Reconciling with rollback (sync %d <= server %d <= client %d)\n", client->sync_frame, client->server_frame, client->client_frame);
    }

    // Reconcile from sync_frame -> server_frame with new real data
    if (client->server_frame > client->sync_frame)
    {
        for (int i = client->sync_frame; i < client->server_frame; ++i)
        {
            GameState *current_state = &client->states[i % FRAME_BUFFER_SIZE];
            GameEvents *current_events = &client->events[i % FRAME_BUFFER_SIZE];
            GameState *next_state = &client->states[(i + 1) % FRAME_BUFFER_SIZE];
            game_simulate(current_state, current_events, next_state);
        }
        client->sync_frame = client->server_frame;
    }

    // Then from server_frame -> client_frame with local data
    if (client->client_frame > client->server_frame)
    {
        for (int i = client->server_frame; i < client->client_frame; ++i)
        {
            GameState *current_state = &client->states[i % FRAME_BUFFER_SIZE];
            GameEvents *current_events = &client->events[i % FRAME_BUFFER_SIZE];
            GameState *next_state = &client->states[(i + 1) % FRAME_BUFFER_SIZE];
            game_simulate(current_state, current_events, next_state);
        }
    }
}

void game_client_send_game_events(GameClient *client, int frame, GameEvents *events)
{
    // Serialize and send to server the players inputs
    uint8_t buffer[MAX_MESSAGE_SIZE];
    size_t msg_size = serialize_p2s_frame_inputs(
        buffer,
        frame,
        client->client_index,
        &events->player_inputs[client->client_index]);

    ssize_t sent = send(client->socket_fd, buffer, msg_size, 0);
    if (sent < 0)
    {
        log_printf("Client failed to send frame %u", frame);
        atomic_store(&client->is_connected, false);
        return;
    }

    log_printf("Sent MSG_P2S_FRAME_INPUTS for frame %u\n", frame);
}
