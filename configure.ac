AC_INIT([libekio], [1.0], [thakararkeval1010@gmail.com])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign])
AC_CANONICAL_HOST
AC_CONFIG_SRCDIR([src/core/event_loop.c])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_PROG_CXX
LT_INIT

#### Determining the operating system and setting platform-specific flags
case "$host_os" in 
    linux*)
        AC_DEFINE([HAVE_LINUX], 1, [Define if building on Linux])

        # Checking for epoll support EPOLL is manadatory to have
        AC_CHECK_LIB([epoll], [epoll_create1], 
            [AC_DEFINE([HAVE_EPOLL], 1, [Define if epoll is available])], 
            [AC_MSG_ERROR([epoll library not found. Required on Linux.])])
        
        # Checking for io_uring support (optional)
        AC_CHECK_LIB([uring], [io_uring_queue_init], 
            [AC_DEFINE([HAVE_IO_URING], 1, [Define if io_uring is available])],
            [AC_MSG_NOTICE([io_uring support not available. will use epoll instead.])])
        ;;
    darwin*)
        AC_DEFINE([HAVE_MACOS], 1, [Define if building on macOS])

        # Check for kqueue support by checking the header and function
        AC_CHECK_HEADERS([sys/event.h], [], 
            [AC_MSG_ERROR([kqueue header not found. Required on macOS.])])
        
        AC_CHECK_FUNCS([kqueue],  
            [AC_DEFINE([HAVE_KQUEUE], 1, [Define if kqueue is available])], 
            [AC_MSG_ERROR([kqueue function not found. Required on macOS.])])
        ;;
    *)
        AC_MSG_ERROR([Unsupported operating system: $host_os])
        ;;
esac

#### Checking for threading support (common to all platforms)

AC_CHECK_HEADERS([pthread.h], [], [AC_MSG_ERROR([pthread.h header not found.])])
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread library not found.])])

#### Check for POSIX timer support
AC_CHECK_HEADERS([time.h], [], [AC_MSG_ERROR([time.h header not found.])])

# Check for networking libraries
AC_CHECK_FUNCS([socket], 
    [AC_DEFINE([HAVE_SOCKET], 1, [Define if socket operations are supported])], 
    [AC_MSG_ERROR([Socket operations not supported.])])

AC_CHECK_LIB([resolv], [res_query], 
    [AC_DEFINE([HAVE_DNS], 1, [Define if DNS resolution is supported])], 
    [AC_MSG_ERROR([DNS resolution library not found.])])

# Create output files
AC_CONFIG_FILES([
    Makefile
    src/Makefile
    tests/Makefile
])
AC_OUTPUT

