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
