#include "libekio.h"
#include <stdio.h>

int main()
{
    struct ev_loop *loop = ev_default_loop();

    printf("Hello, World!\n");

    ev_break(loop, EVBREAK_ALL);
}