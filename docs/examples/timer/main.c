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