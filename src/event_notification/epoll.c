#include "libekio.h"
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

// Backend-specific structure
struct ev_backend
{
    int epoll_fd;
    struct epoll_event *events;
    int max_events;
    int active_watcher_count;
};

// Initialize backend
ev_backend_t *ev_backend_init()
{
    ev_backend_t *backend = (ev_backend_t *)malloc(sizeof(ev_backend_t));
    if (!backend)
        return NULL;

    backend->epoll_fd = epoll_create1(0);
    if (backend->epoll_fd == -1)
    {
        perror("Failed to create epoll");
        free(backend);
        return NULL;
    }

    backend->max_events = 64; // Default event size
    backend->events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * backend->max_events);
    backend->active_watcher_count = 0;
    if (!backend->events)
    {
        perror("Failed to allocate events array");
        close(backend->epoll_fd);
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

    close(backend->epoll_fd);
    free(backend->events);
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
    int timeout = (!(flags & EVRUN_NOWAIT)) ? 0 : 1000; // Default 1-second timeout
    return epoll_wait(backend->epoll_fd, backend->events, backend->max_events, timeout);
}

// Dispatch events
void ev_backend_dispatch(ev_backend_t *backend)
{
    for (int i = 0; i < backend->max_events; i++)
    {
        struct epoll_event *ev = &backend->events[i];

        if (ev->data.ptr)
        {
            if (((ev_io_t *)ev->data.ptr)->type == IO_EVENT)
            {
                ev_io_t *watcher = (ev_io_t *)ev->data.ptr;
                watcher->callback(watcher, ev->events); // Call user callback
            }
            else if (((ev_timer_t *)ev->data.ptr)->type == TIMER_EVENT)
            {
                ev_timer_t *timer = (ev_timer_t *)ev->data.ptr;

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

    struct epoll_event ev = {0};
    ev.events = (watcher->events & EV_READ ? EPOLLIN : 0) | (watcher->events & EV_WRITE ? EPOLLOUT : 0);
    ev.data.ptr = watcher;

    if (epoll_ctl(backend->epoll_fd, EPOLL_CTL_ADD, watcher->fd, &ev) == -1)
    {
        perror("epoll_ctl ADD");
    }
    backend->active_watcher_count++;
}

void ev_backend_unregister_io(ev_backend_t *backend, ev_io_t *watcher)
{
    if (!backend || !watcher)
        return;

    if (epoll_ctl(backend->epoll_fd, EPOLL_CTL_DEL, watcher->fd, NULL) == -1)
    {
        perror("epoll_ctl DEL");
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

    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.ptr = timer;

    if (epoll_ctl(backend->epoll_fd, EPOLL_CTL_ADD, timer->ident, &ev) == -1)
    {
        perror("epoll_ctl ADD timer");
        close(timer->ident);
        return -1;
    }

    backend->active_watcher_count++;
    timer->active = 1;
    return 0;
}

int ev_backend_unregister_timer(ev_backend_t *backend, ev_timer_t *timer)
{
    if (epoll_ctl(backend->epoll_fd, EPOLL_CTL_DEL, timer->ident, NULL) == -1)
    {
        perror("epoll_ctl DEL timer");
        return -1;
    }

    close(timer->ident);
    timer->active = 0;
    backend->active_watcher_count--;
    return 0;
}
