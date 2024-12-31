#include "../config.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "libekio.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>

#if HAVE_KQUEUE
#include "event_notification/kqueue.c"
#endif

#if HAVE_EPOLL
#include "event_notification/epoll.c"
#endif

#if HAVE_IO_URING
#include "event_notification/io_uring.c"
#endif

// Event loop structure
struct ev_loop
{
    ev_backend_t *backend;  // Backend-specific operations
    unsigned int iteration; // Current iteration count
    unsigned int depth;     // Recursion depth
    bool running;           // Is the loop running
    int break_status;       // EVBREAK_*
};

// Default event loop
static ev_loop_t *default_loop = NULL;

// initialize a default loop
struct ev_loop *ev_default_loop()
{
    if (!default_loop)
    {
        default_loop = ev_loop_create();
    }
    return default_loop;
}

// create a new loop
struct ev_loop *ev_loop_create()
{
    ev_loop_t *loop = (ev_loop_t *)malloc(sizeof(ev_loop_t));
    if (!loop)
        return NULL;

    // Backend initialization
    loop->backend = ev_backend_init();
    if (!loop->backend)
    {
        free(loop);
        return NULL;
    }

    loop->iteration = 0;
    loop->depth = 0;
    loop->running = false;
    loop->break_status = EVBREAK_NONE;

    return loop;
};

// destroy a loop
void ev_loop_destroy(struct ev_loop *loop)
{
    if (!loop)
        return;

    // Destroy backend-specific data
    ev_backend_destroy(loop->backend);

    free(loop);
    if (loop == default_loop)
    {
        default_loop = NULL;
    }
};

// run the event loop
int ev_run(struct ev_loop *loop, int flags)
{
    // printf("Loop ev run called");
    if (!loop || !loop->backend)
        return 0;

    loop->depth++;
    loop->break_status = EVBREAK_NONE;
    loop->running = true;

    // printf("Loop starting working");

    while (loop->running)
    {
        // printf("Loop started working");
        //  Queue and call pending watchers (prepare, fork, etc.)
        ev_backend_prepare(loop->backend);

        if (loop->break_status != EVBREAK_NONE)
            break;

        // printf("Getting new event using backend poll");

        // Block and wait for events
        int new_events = ev_backend_poll(loop->backend, flags);

        // printf("New Events %d Running %d\n", new_events, loop->running);

        if (new_events < 0)
        {
            if (errno != EINTR)
            {
                perror("ev_backend_poll failed");
                break;
            }
            continue;
        }

        // printf("Active Dount %d", loop->backend->active_watcher_count);

        if (new_events == 0)
        {
            continue;
        }

        // printf("EV Backend Dispatch Event");
        //  Handle new events
        ev_backend_dispatch(loop->backend);

        // Break if necessary
        if (loop->break_status == EVBREAK_ONE ||
            (flags & EVRUN_ONCE) ||
            ev_backend_is_empty(loop->backend))
        {
            break;
        }

        // Increment iteration count
        loop->iteration++;
    }

    if (loop->break_status == EVBREAK_ONE)
    {
        loop->break_status = EVBREAK_NONE;
    }

    loop->depth--;
    return ev_backend_is_empty(loop->backend) ? 0 : 1;
}

// Function to break the loop
void ev_break(struct ev_loop *loop, int how)
{
    if (loop)
    {
        loop->break_status = how;
    }
}

// Helper functions for loop state
unsigned int ev_iteration(struct ev_loop *loop)
{
    return loop ? loop->iteration : 0;
}

unsigned int ev_depth(struct ev_loop *loop)
{
    return loop ? loop->depth : 0;
}

void ev_suspend(struct ev_loop *loop)
{
    // Optionally implement backend-specific suspend logic
}

void ev_resume(struct ev_loop *loop)
{
    // Optionally implement backend-specific resume logic
}

/*****
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 * Event IO Realated Implementation
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

// Initialize an I / O watcher

void ev_io_init(ev_io_t *watcher, ev_io_cb callback, int fd, int events)
{
    watcher->callback = callback;
    watcher->active = false;
    watcher->data = NULL;

    ev_io_set(watcher, fd, events);
}

// Set events for an existing I/O watcher
void ev_io_set(ev_io_t *watcher, int fd, int events)
{
    watcher->fd = fd;
    watcher->events = events;
    watcher->type = IO_EVENT;

    // Set non-blocking mode for the fd
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl");
        return;
    }
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Start monitoring an I/O watcher
void ev_io_start(ev_loop_t *loop, ev_io_t *watcher)
{
    if (!watcher->active)
    {
        watcher->active = true;
        ev_backend_register_io(loop->backend, watcher);
    }
}

// Stop monitoring an I/O watcher
void ev_io_stop(ev_loop_t *loop, ev_io_t *watcher)
{
    if (watcher->active)
    {
        watcher->active = false;
        ev_backend_unregister_io(loop->backend, watcher);
    }
}

// Handle SIGPIPE for pipe/socket writing errors
void ev_io_handle_sigpipe(int signo)
{
    // Do nothing on SIGPIPE to prevent termination
    (void)signo;
}

void ev_io_setup_sigpipe_handling()
{
    struct sigaction sa;
    sa.sa_handler = ev_io_handle_sigpipe;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, NULL);
}

/*****
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 * EV Timer Realated Implementation
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */
static uintptr_t timer_id_counter = 0; // To generate unique timer IDs

void ev_timer_init(ev_timer_t *timer, ev_timer_cb callback, double after, double repeat)
{
    // printf("Timer Initilize\n");
    timer->after = after;
    timer->repeat = repeat;
    timer->callback = callback;
    timer->data = NULL;
    timer->active = 0;
    timer->ident = ++timer_id_counter;
    timer->type = TIMER_EVENT;
    // printf("Timer Completed\n");
}

void ev_timer_set(ev_timer_t *timer, double after, double repeat)
{
    timer->after = after;
    timer->repeat = repeat;
}

void ev_timer_start(ev_loop_t *loop, ev_timer_t *timer)
{
    // printf("Ev timer start called\n");
    if (timer->active)
        return; // Prevent duplicate starts

    ev_backend_t *backend = (ev_backend_t *)loop->backend;

    int backend_register = ev_backend_register_timer(backend, timer);

    // printf("Backend Register %d\n", backend_register);

    // Register the timer with the backend
    if (backend_register != 0)
    {
        fprintf(stderr, "Failed to register timer with backend\n");
        return;
    }

    timer->active = 1;
    // printf("Ev timer start completed\n");
}

void ev_timer_stop(ev_loop_t *loop, ev_timer_t *timer)
{
    if (!timer->active)
        return;

    ev_backend_t *backend = (ev_backend_t *)loop->backend;

    if (ev_backend_unregister_timer(backend, timer) != 0)
    {
        fprintf(stderr, "Failed to unregister timer from backend\n");
        return;
    }
    timer->active = false;
}

void ev_timer_again(ev_loop_t *loop, ev_timer_t *timer)
{
    if (timer->repeat > 0)
    {
        ev_timer_set(timer, timer->repeat, timer->repeat);
        ev_timer_stop(loop, timer);
        ev_timer_start(loop, timer);
    }
    else
    {
        fprintf(stderr, "Cannot restart a non-repeating timer\n");
    }
}
