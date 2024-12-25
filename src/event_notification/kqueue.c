#include "libekio.h"
#include <sys/event.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

// Backend-specific structure
struct ev_backend
{
    int kqueue_fd;
    struct kevent *events;
    int max_events;
    int active_watcher_count;
};

// Initialize backend
ev_backend_t *ev_backend_init()
{
    ev_backend_t *backend = (ev_backend_t *)malloc(sizeof(ev_backend_t));
    if (!backend)
        return NULL;

    backend->kqueue_fd = kqueue();
    if (backend->kqueue_fd == -1)
    {
        perror("Failed to create kqueue");
        free(backend);
        return NULL;
    }

    backend->max_events = 64; // Default event size
    backend->events = (struct kevent *)malloc(sizeof(struct kevent) * backend->max_events);
    backend->active_watcher_count = 0;
    if (!backend->events)
    {
        perror("Failed to allocate events array");
        close(backend->kqueue_fd);
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

    close(backend->kqueue_fd);
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
    // printf("EV backend Polll");
    struct timespec timeout = {0, 0};
    if (!(flags & EVRUN_NOWAIT))
    {
        timeout.tv_sec = 1; // Default 1-second timeout
    }

    return kevent(backend->kqueue_fd, NULL, 0, backend->events, backend->max_events, &timeout);
}

// Dispatch events
void ev_backend_dispatch(ev_backend_t *backend)
{
    // printf("EV Max Events %d\n", backend->kqueue_fd);
    for (int i = 0; i < backend->max_events; i++)
    {
        struct kevent *ev = &backend->events[i];
        // printf("Kevent ident %d\n", ev->ident);

        if (ev->udata)
        {
            // printf("ev->filter %d\n", ev->filter);
            // here need to handle event type based on filter

            if (ev->filter == EVFILT_READ || ev->filter == EVFILT_WRITE)
            {
                ev_io_t *watcher = (ev_io_t *)ev->udata;
                watcher->callback(watcher, ev->filter); // Call user callback
            }
            else if (ev->filter == EVFILT_TIMER)
            {
                // printf("Timer Callback");
                ev_timer_t *timer = (ev_timer_t *)ev->udata;

                // Skip if the timer is no longer active
                if (!timer->active)
                {
                    // printf("Skipping inactive timer\n");
                    continue;
                }

                // printf("Dispatching Timer event\n");
                timer->callback(timer, 0); // Call timer callback

                //("Timer Repeat %d\n", timer->repeat);

                // If it's a one-shot timer, deactivate it
                if (timer->repeat == 0)
                {
                    // printf("Stopping one-shot timer\n");
                    ev_backend_unregister_timer(backend, timer);
                }
            }
        }
    }

    // printf("EV backend Dispatch end");
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

// Handle I/O events in the backend for kqueue
void ev_backend_register_io(ev_backend_t *backend, ev_io_t *watcher)
{
    if (!backend || !watcher)
        return;

    // Backend-specific code to register I/O events
    // For kqueue
    struct kevent ke;
    EV_SET(&ke, watcher->fd, (watcher->events & EV_READ) ? EVFILT_READ : EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, watcher);

    int event_add = kevent(backend->kqueue_fd, &ke, 1, NULL, 0, NULL);

    if (event_add == -1)
    {
        perror("kevent EV_ADD");
    }
    backend->active_watcher_count++;
}

void ev_backend_unregister_io(ev_backend_t *backend, ev_io_t *watcher)
{
    if (!backend || !watcher)
        return;

    // Backend-specific code to unregister I/O events
    // For kqueue
    struct kevent ke;
    EV_SET(&ke, watcher->fd, (watcher->events & EV_READ) ? EVFILT_READ : EVFILT_WRITE, EV_DELETE, 0, 0, watcher);
    int event_remove = kevent(backend->kqueue_fd, &ke, 1, NULL, 0, NULL);
    if (event_remove == -1)
    {
        perror("kevent EV_DELETE");
    }
    backend->active_watcher_count--;
}

int ev_backend_register_timer(ev_backend_t *backend, ev_timer_t *timer)
{
    // printf("timer->ident %d \n", timer->ident);
    //  printf("Ev Backend Register");
    struct kevent ke;
    struct timespec ts;

    ts.tv_sec = (time_t)timer->after;
    ts.tv_nsec = (long)((timer->after - ts.tv_sec) * 1e9);

    EV_SET(&ke, timer->ident, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_USECONDS,
           ts.tv_sec * 1e6 + ts.tv_nsec / 1e3, timer);

    int timer_register = kevent(backend->kqueue_fd, &ke, 1, NULL, 0, NULL);

    if (timer_register == -1)
    {
        perror("kevent EVFILT_TIMER EV_ADD");
        return -1;
    }
    backend->active_watcher_count++;
    return 0;
}

int ev_backend_unregister_timer(ev_backend_t *backend, ev_timer_t *timer)
{
    // printf("Ev Backend Unregister");
    struct kevent ke;

    // printf("timer->active %d\n", timer->active);

    EV_SET(&ke, timer->ident, EVFILT_TIMER, EV_DELETE, 0, 0, timer);

    int timer_unregister = kevent(backend->kqueue_fd, &ke, 1, NULL, 0, NULL);

    if (timer_unregister == -1)
    {
        perror("kevent EVFILT_TIMER EV_DELETE");
        return -1;
    }

    timer->active = 0;

    // printf("Timer Deleted\n");
    backend->active_watcher_count--;
    return 0;
}
