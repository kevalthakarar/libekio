# **Libekio**  
*Asynchronous Event Loop Library* for **High-Performance Applications**


## Overview
This C library provides an event loop that supports asynchronous programming using `kqueue`, `io_uring`, and `epoll`. These I/O event notification systems allow efficient non-blocking I/O operations, enabling asynchronous handling of events such as timers, file descriptors, and network requests.


The library provides a unified interface for interacting with multiple event notification systems, making it easy to develop asynchronous programs in C.

## Features
- **Cross-platform support**: Uses `kqueue` (macOS), `io_uring` (Linux), and `epoll` (Linux) for efficient I/O event handling.
- **Asynchronous Event Loop**: Handle timers, file I/O

### Building Examples
```bash
git clone https://github.com/kevalthakarar/libekio.git
cd libekio
autoconf
./configure
make -C docs
```

## Examples

Below are two examples demonstrating the functionality of the library.

### 1. Timer Example (Similar to `setTimeout` in node js)

The following code shows how to use the event loop to implement a simple timer that triggers an action after a delay

```c
#include <libekio.h>
#include <stdio.h>

void stop_loop(ev_timer_t *timer, int revents)
{
    printf("Hello World!");

    ev_break(ev_default_loop(), EVBREAK_ALL);
    return;
}

int main()
{
    struct ev_loop *loop = ev_default_loop();
    ev_timer_t timer_watcher;

    ev_timer_init(&timer_watcher, stop_loop, 4, 0);
    ev_timer_start(loop, &timer_watcher);

    // Start the event loop
    ev_run(loop, 0);

    return 0;
}
```


### 2. Using Timer to understand Async Code

The following example demonstrates how to write async code without blocking the event loop. This showcases the non-blocking nature of async operations. 

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "libekio.h"
#include <fcntl.h>

#define TASK_COUNT 5

// Simulated synchronous I/O task
void sync_task(int task_id)
{
    printf("Starting synchronous I/O task %d\n", task_id);
    sleep(1); // Simulate I/O latency (1 second)
    printf("Completed synchronous I/O task %d\n", task_id);
}

void async_response(ev_timer_t *timer, int revents)
{
    printf("Completed Aynchronus Task %d\n", timer->ident);
    return;
}

void run_synchronous()
{
    printf("=== Running Synchronous Tasks ===\n");
    time_t start = time(NULL);

    for (int i = 0; i < TASK_COUNT; i++)
    {
        sync_task(i);
    }

    time_t end = time(NULL);
    printf("Synchronous tasks completed in %.2f seconds.\n\n", (double)(end - start));
}

void run_asynchronous()
{
    printf("=== Running Asynchronous Tasks ===\n");
    ev_loop_t *loop = ev_default_loop();

    time_t start = time(NULL);

    ev_timer_t timer_watcher[TASK_COUNT];

    for (int i = 0; i < TASK_COUNT; i++)
    {
        // expecting delay of 1 second to respond
        ev_timer_init(&timer_watcher[i], async_response, 1, 0);
        printf("Started Aynchronus Task %d\n", timer_watcher[i].ident);
        ev_timer_start(loop, &timer_watcher[i]);
    }

    // Run the event loop
    ev_run(loop, 0);

    time_t end = time(NULL);
    printf("Asynchronous tasks completed in %.2f seconds.\n\n", (double)(end - start));
}

int main()
{
    run_synchronous();
    run_asynchronous();
    return 0;
}
```

Output
```output
=== Running Synchronous Tasks ===

Starting synchronous I/O task 0
Completed synchronous I/O task 0
Starting synchronous I/O task 1
Completed synchronous I/O task 1
Starting synchronous I/O task 2
Completed synchronous I/O task 2
Starting synchronous I/O task 3
Completed synchronous I/O task 3
Starting synchronous I/O task 4
Completed synchronous I/O task 4

Synchronous tasks completed in 5.00 seconds.

=== Running Asynchronous Tasks ===
Started Aynchronus Task 1
Started Aynchronus Task 2
Started Aynchronus Task 3
Started Aynchronus Task 4
Started Aynchronus Task 5

Completed Aynchronus Task 1
Completed Aynchronus Task 2
Completed Aynchronus Task 3
Completed Aynchronus Task 4
Completed Aynchronus Task 5

Asynchronous tasks completed in 1.00 seconds.
```

here first we will run run_synchronous() function that completed it's task in 1 second  (used sleep to simulate IO operations delay)

async task we just initialize ev_timer_init and then using start ev_timer_start we just add our event to event notification based on config.h file and to start and process all timer and event in last we just start ev_run;


### 3. Asynchronous GET Request Example

This example demonstrates how to send an HTTP GET request asynchronously without blocking the event loop.

> **Note**:  for get request i have spin up one small local server that just accept get request and respond with hello world after 5 seconds

```c
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
```
Output

```output
Initiated Request 1
Initiated Request 2
Initiated Request 3
Initiated Request 4

#received this all 4 response after 5 seconds delay
Response from req id - 1 received
Response from req id - 4 received
Response from req id - 2 received
Response from req id - 3 received
```