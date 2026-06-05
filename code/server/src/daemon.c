#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

int make_daemon(void) /* Detach the server from the terminal. */
{
    pid_t pid;

    /* Create a child process and terminate the parent. */
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "[SERVER] fork(): %s\n", strerror(errno));
        return -1;
    }

    if (pid != 0) {
        return 1;
    }

    /* Detach from the controlling terminal. */
    if (setsid() < 0) {
        fprintf(stderr, "[SERVER] setsid(): %s\n", strerror(errno));
        return -1;
    }

    /* Ignore hangup signal for daemon process. */
    signal(SIGHUP, SIG_IGN);

    /* Allow daemon to create files with requested permissions. */
    umask(0);

    return 0;
}
