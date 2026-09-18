/*
 * Stub <signal.h> for the koppi-os Lua port. There is no job control, so
 * signal() is a no-op and the frontend's Ctrl-C handler never fires.
 */
#ifndef LUA_SHIM_SIGNAL_H
#define LUA_SHIM_SIGNAL_H

typedef void (*__sighandler_t)(int);
typedef int sig_atomic_t;

#define SIG_DFL ((__sighandler_t) 0)
#define SIG_IGN ((__sighandler_t) 1)
#define SIG_ERR ((__sighandler_t) -1)

#define SIGINT  2
#define SIGTERM 15

static inline __sighandler_t signal(int sig, __sighandler_t h) {
    (void) sig;
    (void) h;
    return SIG_DFL;
}

#endif
