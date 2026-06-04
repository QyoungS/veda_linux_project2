#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define TCP_PORT 5100
#define LED_LIB_PATH "./lib/libled.so"
#define BUZZER_LIB_PATH "./lib/libbuzzer.so"
#define LIGHT_LIB_PATH "./lib/liblight.so"
#define FND_LIB_PATH "./lib/libfnd.so"

int make_daemon(void);

typedef int (*func_void_t)(void);
typedef int (*func_int_t)(int);

struct sensor_arg {
    int csock;
    volatile int stop;
    func_void_t light_read_fn;
};

static void *sensor_thread_fn(void *arg)
{
    struct sensor_arg *sa = (struct sensor_arg *)arg;
    char buf[BUFSIZ];

    while (!sa->stop) {
        int value = sa->light_read_fn();
        const char *status = (value == 0) ? "DETECTED" : "NOT DETECTED";
        snprintf(buf, sizeof(buf), "Result: LIGHT %s (value=%d)\n", status, value);
        if (write(sa->csock, buf, strlen(buf)) < 0) {
            break;
        }
        usleep(500000);
    }

    /* do not free here; main thread will free the struct after pthread_join */
    return NULL;
}

static void remove_newline(char *buf)
{
    buf[strcspn(buf, "\r\n")] = '\0';
}

static void to_uppercase(char *buf)
{
    while (*buf != '\0') {
        *buf = (char)toupper((unsigned char)*buf);
        buf++;
    }
}

int main(int argc, char **argv)
{
    int ssock;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clen;
    char mesg[BUFSIZ];

    void *led_handle;
    void *buzzer_handle;
    void *light_handle;
    void *fnd_handle;
    func_void_t led_on;
    func_void_t led_off;
    func_void_t buzzer_on;
    func_void_t buzzer_off;
    func_void_t light_read;
    func_int_t led_brightness;
    func_int_t fnd_display;

    setvbuf(stdout, NULL, _IONBF, 0);

    /* Redirect stdout/stderr to running.txt for runtime logs */
    {
        FILE *logf = fopen("running.txt", "a");
        if (logf) {
            /* duplicate file descriptor to stdout and stderr */
            dup2(fileno(logf), STDOUT_FILENO);
            dup2(fileno(logf), STDERR_FILENO);
            /* keep unbuffered for immediate logging */
            setvbuf(stdout, NULL, _IONBF, 0);
        } else {
            perror("fopen(running.txt)");
        }
    }

    /* Run as daemon only when -d option is given */
    if (argc >= 2 && strcmp(argv[1], "-d") == 0) {
        int ret = make_daemon();
        if (ret != 0) {
            return ret == 1 ? 0 : -1;
        }
    }

    /* Load device shared libraries at runtime */
    led_handle = dlopen(LED_LIB_PATH, RTLD_NOW | RTLD_GLOBAL);
    if (!led_handle) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        return -1;
    }

    buzzer_handle = dlopen(BUZZER_LIB_PATH, RTLD_NOW);
    if (!buzzer_handle) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        dlclose(led_handle);
        return -1;
    }

    light_handle = dlopen(LIGHT_LIB_PATH, RTLD_NOW);
    if (!light_handle) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    fnd_handle = dlopen(FND_LIB_PATH, RTLD_NOW);
    if (!fnd_handle) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    /* Load device control functions from shared libraries */
    led_on = (func_void_t)dlsym(led_handle, "led_on");
    led_off = (func_void_t)dlsym(led_handle, "led_off");
    led_brightness = (func_int_t)dlsym(led_handle, "led_brightness");
    buzzer_on = (func_void_t)dlsym(buzzer_handle, "buzzer_on");
    buzzer_off = (func_void_t)dlsym(buzzer_handle, "buzzer_off");
    light_read = (func_void_t)dlsym(light_handle, "light_read");
    fnd_display = (func_int_t)dlsym(fnd_handle, "fnd_display");

    if (!led_on || !led_off || !led_brightness || !buzzer_on ||
        !buzzer_off || !light_read || !fnd_display) {
        fprintf(stderr, "dlsym error: %s\n", dlerror());
        dlclose(fnd_handle);
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    /* Create TCP server socket */
    if ((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        dlclose(fnd_handle);
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    {
        int optval = 1;
        if (setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR,
                       &optval, sizeof(optval)) < 0) {
            perror("setsockopt()");
            close(ssock);
            dlclose(fnd_handle);
            dlclose(light_handle);
            dlclose(buzzer_handle);
            dlclose(led_handle);
            return -1;
        }
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(TCP_PORT);

    /* Bind server socket to TCP port */
    if (bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind()");
        close(ssock);
        dlclose(fnd_handle);
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    /* Wait for client connections */
    if (listen(ssock, 8) < 0) {
        perror("listen()");
        close(ssock);
        dlclose(fnd_handle);
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    printf("Server started\n");
    fflush(stdout);

    while (1) {
        int csock;
        int n;

        clen = sizeof(cliaddr);

        /* Accept one client connection */
        csock = accept(ssock, (struct sockaddr *)&cliaddr, &clen);
        if (csock < 0) {
            perror("accept()");
            continue;
        }

        printf("Client connected\n");
        pthread_t sensor_tid = 0;
        struct sensor_arg *sensor_argp = NULL;

        while (1) {
            memset(mesg, 0, sizeof(mesg));
            n = read(csock, mesg, sizeof(mesg) - 1);
            if (n <= 0) {
                /* stop sensor thread if running */
                if (sensor_tid != 0 && sensor_argp != NULL) {
                    sensor_argp->stop = 1;
                    pthread_join(sensor_tid, NULL);
                    free(sensor_argp);
                    sensor_tid = 0;
                    sensor_argp = NULL;
                }
                close(csock);
                printf("Client disconnected\n");
                break;
            }

            remove_newline(mesg);
            {
                char log_mesg[BUFSIZ];
                snprintf(log_mesg, sizeof(log_mesg), "%s", mesg);
                to_uppercase(log_mesg);
                printf("Command received: %s\n", log_mesg);
            }

            /* Dispatch command to device library function */
            if (strcmp(mesg, "led on") == 0) {
                led_on();
                printf("[LED] Device result: success\n");
                write(csock, "Result: LED ON\n", 15);
            } else if (strcmp(mesg, "led off") == 0) {
                led_off();
                printf("[LED] Device result: success\n");
                write(csock, "Result: LED OFF\n", 16);
            } else if (strncmp(mesg, "led bright ", 11) == 0) {
                int value = atoi(mesg + 11);
                led_brightness(value);
                printf("[LED] Device result: success\n");
                snprintf(mesg, sizeof(mesg), "Result: LED BRIGHT %d\n", value);
                write(csock, mesg, strlen(mesg));
            } else if (strcmp(mesg, "buzzer on") == 0) {
                buzzer_on();
                printf("[BUZZER] Device result: success\n");
                write(csock, "Result: BUZZER ON\n", 18);
            } else if (strcmp(mesg, "buzzer off") == 0) {
                buzzer_off();
                printf("[BUZZER] Device result: success\n");
                write(csock, "Result: BUZZER OFF\n", 19);
            } else if (strcmp(mesg, "light read") == 0) {
                if (sensor_tid == 0) {
                    sensor_argp = malloc(sizeof(struct sensor_arg));
                    if (sensor_argp != NULL) {
                        sensor_argp->csock = csock;
                        sensor_argp->stop = 0;
                        sensor_argp->light_read_fn = light_read;
                        if (pthread_create(&sensor_tid, NULL, sensor_thread_fn, sensor_argp) == 0) {
                            printf("[LIGHT] Sensor push started\n");
                            write(csock, "Result: SENSOR ON\n", 17);
                        } else {
                            free(sensor_argp);
                            sensor_argp = NULL;
                            sensor_tid = 0;
                            write(csock, "ERROR: failed to start sensor\n", 30);
                        }
                    } else {
                        write(csock, "ERROR: failed to allocate sensor\n", 33);
                    }
                } else {
                    write(csock, "Result: SENSOR ALREADY ON\n", 27);
                }
            } else if (strcmp(mesg, "sensor on") == 0) {
                if (sensor_tid == 0) {
                    sensor_argp = malloc(sizeof(struct sensor_arg));
                    if (sensor_argp != NULL) {
                        sensor_argp->csock = csock;
                        sensor_argp->stop = 0;
                        sensor_argp->light_read_fn = light_read;
                        if (pthread_create(&sensor_tid, NULL, sensor_thread_fn, sensor_argp) == 0) {
                            printf("[LIGHT] Sensor push started\n");
                            write(csock, "Result: SENSOR ON\n", 17);
                        } else {
                            free(sensor_argp);
                            sensor_argp = NULL;
                            sensor_tid = 0;
                            write(csock, "ERROR: failed to start sensor\n", 30);
                        }
                    } else {
                        write(csock, "ERROR: failed to allocate sensor\n", 33);
                    }
                } else {
                    write(csock, "Result: SENSOR ALREADY ON\n", 27);
                }
            } else if (strcmp(mesg, "sensor off") == 0) {
                if (sensor_tid != 0 && sensor_argp != NULL) {
                    sensor_argp->stop = 1;
                    pthread_join(sensor_tid, NULL);
                    free(sensor_argp);
                    sensor_tid = 0;
                    sensor_argp = NULL;
                    printf("[LIGHT] Sensor push stopped\n");
                    write(csock, "Result: SENSOR OFF\n", 17);
                } else {
                    write(csock, "Result: SENSOR OFF\n", 17);
                }
            } else if (strncmp(mesg, "fnd ", 4) == 0) {
                int num = atoi(mesg + 4);
                fnd_display(num);
                printf("[FND] Device result: success\n");
                snprintf(mesg, sizeof(mesg), "Result: FND %d\n", num);
                write(csock, mesg, strlen(mesg));
            } else if (strcmp(mesg, "exit") == 0) {
                write(csock, "Result: EXIT\n", 13);
                /* stop sensor thread if running */
                if (sensor_tid != 0 && sensor_argp != NULL) {
                    sensor_argp->stop = 1;
                    pthread_join(sensor_tid, NULL);
                    free(sensor_argp);
                    sensor_tid = 0;
                    sensor_argp = NULL;
                }
                close(csock);
                printf("Client disconnected\n");
                break;
            } else {
                write(csock, "ERROR: unknown command\n", 23);
            }
        }
    }

    close(ssock);
    dlclose(fnd_handle);
    dlclose(light_handle);
    dlclose(buzzer_handle);
    dlclose(led_handle);

    return 0;
}
