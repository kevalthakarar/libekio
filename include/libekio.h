#ifndef LIB_EKIO_H
#define LIB_EKIO_H

#include <stdbool.h>
#include <stdint.h>

/*
 *
 * Comman Macros
 *
 *
 */
// Flags for `ev_run`
#define EVRUN_NOWAIT 0x01
#define EVRUN_ONCE 0x02

// Break statuses
#define EVBREAK_NONE 0
#define EVBREAK_ONE 1
#define EVBREAK_ALL 2

// event type
#define TIMER_EVENT 1
#define IO_EVENT 2

enum
{
    EV_READ = 0x1,
    EV_WRITE = 0x2
};

// IO watcher structure
typedef struct ev_io ev_io_t;
// Backend-specific structure
typedef struct ev_backend ev_backend_t;
// timeout related structure
typedef struct ev_timer ev_timer_t;
// Event loop structure
typedef struct ev_loop ev_loop_t;

/**
 *
 *
 *  Main Event Loop Functions
 *
 *
 */
struct ev_loop *ev_default_loop();
struct ev_loop *ev_loop_create();
void ev_loop_destroy(struct ev_loop *loop);
int ev_run(struct ev_loop *loop, int flags);
void ev_break(struct ev_loop *loop, int how);
unsigned int ev_iteration(struct ev_loop *loop);
unsigned int ev_depth(struct ev_loop *loop);
void ev_suspend(struct ev_loop *loop);
void ev_resume(struct ev_loop *loop);

/**
 *
 *
 * Event I/O Related Functions
 *
 *
 */
typedef void (*ev_io_cb)(ev_io_t *watcher, int revents);
struct ev_io
{
    int type;
    int fd;            // File descriptor to watch
    int events;        // Events to watch (e.g., EV_READ, EV_WRITE)
    ev_io_cb callback; // Callback function
    void *data;        // User data associated with this watcher
    bool active;       // if io is active or not
};

void ev_io_init(ev_io_t *watcher, ev_io_cb callback, int fd, int events);
void ev_io_set(ev_io_t *watcher, int fd, int events);
void ev_io_start(ev_loop_t *loop, ev_io_t *watcher);
void ev_io_stop(ev_loop_t *loop, ev_io_t *watcher);
void ev_io_handle_sigpipe(int signo);
void ev_io_setup_sigpipe_handling();

/**
 *
 *
 * Timeout Related Functions
 *
 *
 *
 *
 */

typedef void (*ev_timer_cb)(ev_timer_t *timer, int revents);
struct ev_timer
{
    int type;
    double after;         // Initial timeout in seconds
    double repeat;        // Repeat interval in seconds (0 for one-shot)
    ev_timer_cb callback; // Callback function
    void *data;           // User data
    int active;           // Internal flag for active status
    uintptr_t ident;      // Unique identifier for the timer
};

void ev_timer_init(ev_timer_t *timer, ev_timer_cb callback, double after, double repeat);
void ev_timer_set(ev_timer_t *timer, double after, double repeat);
void ev_timer_start(ev_loop_t *loop, ev_timer_t *timer);
void ev_timer_stop(ev_loop_t *loop, ev_timer_t *timer);
void ev_timer_again(ev_loop_t *loop, ev_timer_t *timer);

/**
 *
 *
 * Backend Related Functions (Epoll , Kqueue , Io_Uring)
 *
 *
 */
ev_backend_t *ev_backend_init();
void ev_backend_destroy(ev_backend_t *backend);
void ev_backend_prepare(ev_backend_t *backend);
int ev_backend_poll(ev_backend_t *backend, int flags);
void ev_backend_dispatch(ev_backend_t *backend);
int ev_backend_is_empty(ev_backend_t *backend);
void ev_backend_register_io(ev_backend_t *backend, ev_io_t *watcher);
void ev_backend_unregister_io(ev_backend_t *backend, ev_io_t *watcher);
int ev_backend_register_timer(ev_backend_t *backend, ev_timer_t *timer);
int ev_backend_unregister_timer(ev_backend_t *backend, ev_timer_t *timer);

#endif // LIB_EKIO_H