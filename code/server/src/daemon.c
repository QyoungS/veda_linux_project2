#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

int make_daemon(void)
{
    pid_t pid;

    /* Create a child process and terminate the parent */
    pid = fork();
    if (pid < 0) {
        perror("fork()");
        return -1;
    }

    if (pid != 0) {
        return 1;
    }

    /* Detach from the controlling terminal */
    if (setsid() < 0) {
        perror("setsid()");
        return -1;
    }

    /* Ignore hangup signal for daemon process */
    signal(SIGHUP, SIG_IGN);

    /* Allow daemon to create files with requested permissions */
    umask(0);

    return 0;
}
