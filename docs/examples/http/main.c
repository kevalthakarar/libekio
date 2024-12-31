#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "libekio.h"
#include <netdb.h>
#include <sys/errno.h>
#include <sys/event.h>

// Structure to hold connection-specific data
typedef struct
{
    int req_id;
    int sockfd;
    ev_io_t io_watcher;
    char hostname[256];
    char request[1024];
    char response[8192];
    size_t response_len;
} connection_t;

// Callback for reading data from the server
void read_callback(ev_io_t *w, int revents)
{
    connection_t *conn = (connection_t *)w->data;
    ssize_t bytes_received = recv(conn->sockfd, conn->response + conn->response_len,
                                  sizeof(conn->response) - conn->response_len - 1, 0);

    if (bytes_received < 0)
    {
        ev_io_stop(ev_default_loop(), w);
        return;
    }

    if (bytes_received == 0)
    {
        // Connection closed by the server
        ev_io_stop(ev_default_loop(), w);

        // Null-terminate and print the response
        conn->response[conn->response_len] = '\0';
        printf("Response from req id - %d received\n", conn->req_id);
        return;
    }

    conn->response_len += bytes_received;
}

// Callback for sending the HTTP GET request
void write_callback(ev_io_t *w, int revents)
{
    connection_t *conn = (connection_t *)w->data;

    if (send(conn->sockfd, conn->request, strlen(conn->request), 0) < 0)
    {
        perror("Send failed");
        ev_io_stop(ev_default_loop(), w);
        close(conn->sockfd);
        free(conn);
        return;
    }

    // Switch to reading mode
    ev_io_stop(ev_default_loop(), w);
    ev_io_init(&conn->io_watcher, read_callback, conn->sockfd, EV_READ);
    conn->io_watcher.data = conn;
    ev_io_start(ev_default_loop(), &conn->io_watcher);
}

// Initiate a connection and register it with the event loop
void initiate_request(ev_loop_t *loop, const char *hostname, const char *ip_address, int req_id)
{
    connection_t *conn = (connection_t *)malloc(sizeof(connection_t));
    if (!conn)
    {
        perror("Memory allocation failed");
        exit(1);
    }

    memset(conn, 0, sizeof(connection_t));
    strcpy(conn->hostname, hostname);
    conn->req_id = req_id;

    // Create a non-blocking socket
    conn->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->sockfd < 0)
    {
        perror("Socket creation failed");
        free(conn);
        exit(1);
    }

    fcntl(conn->sockfd, F_SETFL, O_NONBLOCK);

    // Setup server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(3000);
    server_addr.sin_addr.s_addr = inet_addr(ip_address);

    // Connect to the server
    if (connect(conn->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 && errno != EINPROGRESS)
    {
        perror("Connection failed");
        close(conn->sockfd);
        free(conn);
        return;
    }

    // Prepare the HTTP GET request
    snprintf(conn->request, sizeof(conn->request),
             "GET / HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             hostname);

    // Initialize and start the write watcher
    ev_io_init(&conn->io_watcher, write_callback, conn->sockfd, EV_WRITE);
    conn->io_watcher.data = conn;
    ev_io_start(loop, &conn->io_watcher);
}

int main()
{
    const char *hostname = "local host";
    // const char *ip_address = resolve_host(hostname);

    const char *ip_address = "0.0.0.0";
    ev_loop_t *loop = ev_default_loop();

    // Initiate 100 connections to the same hostname
    for (int i = 0; i < 4; i++)
    {
        printf("Initiated Request %d\n", i + 1);
        initiate_request(loop, hostname, ip_address, i + 1);
    }

    // Run the custom event loop
    ev_run(loop, 0);

    ev_loop_destroy(loop);
    return 0;
}