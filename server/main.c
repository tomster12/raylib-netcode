#include <stdio.h>      // for printf
#include <stdint.h>     // for uint8_t, ssize_t
#include <stdlib.h>     // for malloc, free
#include <string.h>     // for memset
#include <unistd.h>     // for usleep
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket functions
#include <pthread.h>    // for threading

#define PORT 32000
#define MAX_CLIENTS 100

int connected_client_sockets[MAX_CLIENTS] = {0};
int connected_client_count = 0;
pthread_mutex_t client_sockets_mutex = PTHREAD_MUTEX_INITIALIZER;

void *client_thread(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);
    
    printf("Client %d connected\n", client_fd);

    // Keep receiving data from the client until they disconnect
    // TODO: Actually handle the received data
    uint8_t buffer[1024];
    while (1)
    {
        ssize_t received = recv(client_fd, buffer, sizeof(buffer), 0);
        if (received <= 0)
        {
            printf("Client %d disconnected or error occurred\n", client_fd);
            break;
        }

        printf("Received %ld bytes from client %d\n", received, client_fd);
    }

    // Client disconnected, remove from connected clients
    printf("Disconnecting client %d\n", client_fd);
    pthread_mutex_lock(&client_sockets_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (connected_client_sockets[i] == client_fd)
        {
            connected_client_sockets[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&client_sockets_mutex);
    close(client_fd);
    return NULL;
}

void *simulation_thread(void *arg)
{
    while (1)
    {
        // 15hz server tick
        usleep(1000000 / 15);

        // TODO: Simulate current frame using inputs from clients

        // TODO: Serialize game state + events

        // uint8_t update_data[1024];
        // size_t update_size = 0;
        // pthread_mutex_lock(&client_sockets_mutex);
        // for (int i = 0; i < MAX_CLIENTS; ++i)
        // {
        //     if (connected_client_sockets[i] != 0)
        //     {
        //         send(connected_client_sockets[i], update_data, update_size, 0);
        //     }
        // }
        // pthread_mutex_unlock(&client_sockets_mutex);
    }

    return NULL;
}

void accept_clients(int server_fd)
{
    // Start accepting client connections
    pthread_mutex_init(&client_sockets_mutex, NULL);
    while (1)
    {
        // Accept a new client connection
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, NULL, NULL);
        if (*client_fd < 0)
        {
            perror("accept");
            free(client_fd);
            continue;
        }

        if (connected_client_count >= MAX_CLIENTS)
        {
            printf("Maximum client limit reached (%d), rejecting new client %d\n", MAX_CLIENTS, *client_fd);
            close(*client_fd);
            free(client_fd);
            continue;
        }

        // Assign the client socket to the first available slot
        pthread_mutex_lock(&client_sockets_mutex);
        int assigned = 0;
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (connected_client_sockets[i] == 0)
            {
                connected_client_sockets[i] = *client_fd;
                assigned = 1;
                break;
            }
        }
        pthread_mutex_unlock(&client_sockets_mutex);

        if (!assigned)
        {
            printf("No available client slots, rejecting new client %d\n", *client_fd);
            close(*client_fd);
            free(client_fd);
            continue;
        }

        // Start a new detached thread to handle the client
        connected_client_count++;
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_thread, client_fd);
        pthread_detach(thread_id);
    }

    close(server_fd);
}

int main()
{
    setbuf(stdout, NULL);
    printf("Starting server...\n");

    // Initialize and bind the server socket to localhost:PORT
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);
    printf("Server listening on port %d\n", PORT);

    // Create a detached thread for simulation
    pthread_t sim_thread;
    pthread_create(&sim_thread, NULL, simulation_thread, NULL);

    // Begin accepting client connections
    accept_clients(server_fd);

    printf("Shutting down server...\n");
    pthread_join(sim_thread, NULL);
    pthread_mutex_destroy(&client_sockets_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (connected_client_sockets[i] != 0)
        {
            close(connected_client_sockets[i]);
        }
    }
    close(server_fd);
    return 0;
}
