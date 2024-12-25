include "libekio.h"
#include <liburing.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>  // For POLLIN, POLLOUT
#include <fcntl.h> // For TFD_NONBLOCK
#include <sys/timerfd.h>

    // Backend-specific structure
    struct ev_backend
{
    struct io_uring ring;
    struct io_uring_cqe **cqe;
    int max_events;
    int active_watcher_count;
};

// Initialize backend
ev_backend_t *ev_backend_init()
{
    ev_backend_t *backend = (ev_backend_t *)malloc(sizeof(ev_backend_t));
    if (!backend)
        return NULL;

    int ret = io_uring_queue_init(64, &backend->ring, 0); // Initialize io_uring with max_events (64)
    if (ret)
    {
        perror("Failed to create io_uring");
        free(backend);
        return NULL;
    }

    backend->max_events = 64; // Default event size

    backend->cqe = (struct io_uring_cqe **)malloc(sizeof(struct io_uring_cqe *) * backend->max_events);
    backend->active_watcher_count = 0;
    if (!backend->cqe)
    {
        perror("Failed to allocate CQE array");
        io_uring_queue_exit(&backend->ring);
        free(backend);
        return NULL;
    }

    return backend;
}

// Destroy backend
void ev_backend_destroy(ev_backend_t *backend)
{
    if (!backend)
        return;

    io_uring_queue_exit(&backend->ring);
    free(backend->cqe);
    free(backend);
}

// Prepare backend (queue pending tasks, etc.)
void ev_backend_prepare(ev_backend_t *backend)
{
    // Prepare logic if necessary (e.g., fork watchers)
}

// Poll backend for events
int ev_backend_poll(ev_backend_t *backend, int flags)
{
    int ret = io_uring_peek_batch_cqe(&backend->ring, backend->cqe, backend->max_events);
    if (ret > 0)
    {
        // Handle events if there are any
        return ret;
    }
    return 0; // No events
}

// Dispatch events
void ev_backend_dispatch(ev_backend_t *backend)
{
    for (int i = 0; i < backend->max_events; i++)
    {
        if (backend->cqe)
        {
            struct io_uring_cqe *cqe = backend->cqe[i];
            if (cqe && cqe->user_data)
            {
                int type = ((ev_io_t *)cqe->user_data)->type;
                if (type == IO_EVENT)
                {
                    ev_io_t *watcher = (ev_io_t *)cqe->user_data;
                    watcher->callback(watcher, cqe->res); // Call user callback
                }
                else if (type == TIMER_EVENT)
                {
                    ev_timer_t *timer = (ev_timer_t *)cqe->user_data;
                    // Skip if the timer is no longer active
                    if (!timer->active)
                    {
                        continue;
                    }

                    uint64_t expirations;
                    read(timer->ident, &expirations, sizeof(expirations)); // Clear the timer

                    timer->callback(timer, 0); // Call timer callback

                    if (timer->repeat == 0)
                    {
                        ev_backend_unregister_timer(backend, timer); // Stop one-shot timer
                    }
                }
            }
        }
    }
}

// Check if backend has pending tasks
int ev_backend_is_empty(ev_backend_t *backend)
{
    if (backend->active_watcher_count == 0)
    {
        return 1;
    }
    // Check if any watchers are active
    return 0; // For now, assume not empty
}

// Register I/O event
void ev_backend_register_io(ev_backend_t *backend, ev_io_t *watcher)
{
    if (!backend || !watcher)
        return;

    struct io_uring_sqe *sqe = io_uring_get_sqe(&backend->ring);
    if (!sqe)
    {
        perror("Failed to get SQE");
        return;
    }

    // Set up I/O event
    if (watcher->events & EV_READ)
    {
        io_uring_prep_poll_add(sqe, watcher->fd, POLLIN);
    }
    if (watcher->events & EV_WRITE)
    {
        io_uring_prep_poll_add(sqe, watcher->fd, POLLOUT);
    }

    sqe->user_data = watcher; // Store user data

    int ret = io_uring_submit(&backend->ring);
    if (ret < 0)
    {
        perror("io_uring_submit ADD IO event");
    }
    backend->active_watcher_count++;
}

void ev_backend_unregister_io(ev_backend_t *backend, ev_io_t *watcher)
{
    if (!backend || !watcher)
        return;

    struct io_uring_sqe *sqe = io_uring_get_sqe(&backend->ring);
    if (!sqe)
    {
        perror("Failed to get SQE");
        return;
    }

    // Remove the I/O event
    io_uring_prep_poll_remove(sqe, watcher->fd);

    int ret = io_uring_submit(&backend->ring);
    if (ret < 0)
    {
        perror("io_uring_submit DEL IO event");
    }
    backend->active_watcher_count--;
}

int ev_backend_register_timer(ev_backend_t *backend, ev_timer_t *timer)
{
    struct itimerspec ts;
    ts.it_value.tv_sec = (time_t)timer->after;
    ts.it_value.tv_nsec = (long)((timer->after - ts.it_value.tv_sec) * 1e9);
    ts.it_interval.tv_sec = (time_t)timer->repeat;
    ts.it_interval.tv_nsec = (long)((timer->repeat - ts.it_interval.tv_sec) * 1e9);

    timer->ident = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    if (timer->ident == -1)
    {
        perror("timerfd_create");
        return -1;
    }
    if (timerfd_settime(timer->ident, 0, &ts, NULL) == -1)
    {
        perror("timerfd_settime");
        close(timer->ident);
        return -1;
    }
    struct io_uring_sqe *sqe = io_uring_get_sqe(&backend->ring);
    if (!sqe)
    {
        perror("Failed to get SQE for timer");
        return -1;
    }
    io_uring_prep_poll_add(sqe, timer->ident, POLLIN);
    sqe->user_data = (uintptr_t)timer;
    int ret = io_uring_submit(&backend->ring);
    if (ret < 0)
    {
        perror("io_uring_submit ADD timer");
        close(timer->ident);
        return -1;
    }
    backend->active_watcher_count++;
    timer->active = 1;
    return 0;
}

int ev_backend_unregister_timer(ev_backend_t *backend, ev_timer_t *timer)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&backend->ring);
    if (!sqe)
    {
        perror("Failed to get SQE for timer removal");
        return -1;
    }

    io_uring_prep_poll_remove(sqe, timer->ident);
    int ret = io_uring_submit(&backend->ring);
    if (ret < 0)
    {
        perror("io_uring_submit DEL timer");
        return -1;
    }

    close(timer->ident);
    timer->active = 0;
    backend->active_watcher_count--;
    return 0;
}
