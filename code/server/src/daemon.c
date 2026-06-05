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

    /* Detach standard input/output/error from the terminal. */
    {
        int fd = open("/dev/null", O_RDWR);

        if (fd < 0) {
            fprintf(stderr, "[SERVER] open(/dev/null): %s\n", strerror(errno));
            return -1;
        }

        if (dup2(fd, STDIN_FILENO) < 0 ||
            dup2(fd, STDOUT_FILENO) < 0 ||
            dup2(fd, STDERR_FILENO) < 0) {
            fprintf(stderr, "[SERVER] dup2(/dev/null): %s\n", strerror(errno));
            close(fd);
            return -1;
        }

        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }

    return 0;
}
