#include "libekio.h"
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * nc 127.0.0.1 8080
 * telnet 127.0.0.1 8080
 */

static void accept_cb(ev_io_t *watcher, int revents)
{
    int client_fd = accept(watcher->fd, NULL, NULL);
    printf("Events %d", revents);
    if (client_fd > 0)
    {
        const char *response = "Hello from TCP Server!\n";
        write(client_fd, response, strlen(response));
        close(client_fd);
    }
}

int main()
{
    struct ev_loop *loop = ev_default_loop();
    ev_io_t tcp_watcher;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 10);

    ev_io_init(&tcp_watcher, accept_cb, server_fd, EV_READ);
    ev_io_start(loop, &tcp_watcher);

    printf("Server is running on port 8080\n");
    ev_run(loop, 0);
    close(server_fd);
    return 0;
}