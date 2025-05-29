#include <stdio.h>     // For perror
#include <pthread.h>   // For threading
#include <unistd.h>    // For close
#include <arpa/inet.h> // For socket functions
#include <string.h>    // For memset
#include <stdio.h>     // For printf
#include <stdlib.h>    // For exit
#include <errno.h>     // For errno

#include "netclient.h"

static int sockfd;
static pthread_t recv_thread;
static volatile int running = 1;
static NetBuffer *net_buf = NULL;

void net_buffer_init(NetBuffer *buf)
{
    memset(buf, 0, sizeof(NetBuffer));
    buf->server_frame = 0;
    buf->client_frame = 0;
}

GameState *net_buffer_get_state(NetBuffer *buf, uint32_t frame)
{
    return &buf->states[frame % MAX_ROLLBACK];
}

GameEvents *net_buffer_get_events(NetBuffer *buf, uint32_t frame)
{
    return &buf->events[frame % MAX_ROLLBACK];
}

void *net_client_recv_loop(void *arg)
{
    // Receive loop for the client
    // TODO: Actually handle the received data
    while (running)
    {
        uint32_t frame;
        ssize_t r = recv(sockfd, &frame, sizeof(frame), MSG_WAITALL);
        if (r <= 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                continue;
            }
            perror("recv frame");
            break;
        }

        if (frame >= MAX_ROLLBACK)
        {
            continue;
        }

        GameState *state = net_buffer_get_state(net_buf, frame);
        GameEvents *events = net_buffer_get_events(net_buf, frame);

        recv(sockfd, state, sizeof(GameState), MSG_WAITALL);
        recv(sockfd, events, sizeof(GameEvents), MSG_WAITALL);

        net_buf->server_frame = frame;
    }

    return NULL;
}

int net_client_init(const char *server_ip, int port, NetBuffer *net_buf)
{
    net_buf = net_buf;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Connected to server %s:%d\n", server_ip, port);

    pthread_create(&recv_thread, NULL, net_client_recv_loop, NULL);
    pthread_detach(recv_thread);

    return 0;
}

void net_client_shutdown()
{
    running = 0;
    shutdown(sockfd, SHUT_RDWR);
    pthread_join(recv_thread, NULL);
    close(sockfd);
}

void net_client_send_events(uint32_t frame, const GameEvents *events)
{
    // TODO: Handle sending events properly
    send(sockfd, &frame, sizeof(uint32_t), 0);
    send(sockfd, events, sizeof(GameEvents), 0);
}
