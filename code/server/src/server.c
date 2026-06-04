#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dlfcn.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
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

#define SERVER_LOG(...)        \
    do {                       \
        printf("[SERVER] ");   \
        printf(__VA_ARGS__);   \
    } while (0)

#define SERVER_ERR(...)              \
    do {                             \
        fprintf(stderr, "[SERVER] "); \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

#define SERVER_PERROR(msg) \
    SERVER_ERR("%s: %s\n", (msg), strerror(errno))

struct sensor_arg {
    int csock;
    volatile int stop;
    func_void_t light_read_fn;
};

struct device_status {
    int client_connected;
    int led_on;
    int led_brightness;
    int buzzer_on;
    int sensor_on;
    int light_value;
    int fnd_value;
};

static struct device_status g_status = {
    0, 0, 0, 0, 0, -1, -1
};
static pthread_mutex_t g_status_lock = PTHREAD_MUTEX_INITIALIZER;

static FILE *open_status_file(const char *mode)
{
    const char *paths[] = {
        "exec/status.txt",
        "status.txt",
        "../exec/status.txt"
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        FILE *fp = fopen(paths[i], mode);
        if (fp != NULL) {
            return fp;
        }
    }

    return NULL;
}

static void read_status_value(const char *key, char *value, size_t size)
{
    FILE *fp = open_status_file("r");
    char line[256];
    size_t key_len = strlen(key);

    value[0] = '\0';

    if (fp == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = line;
        char *end;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (strncmp(p, key, key_len) != 0) {
            continue;
        }

        p += key_len;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p != '=') {
            continue;
        }

        p++;
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        end = p + strcspn(p, "\r\n");
        while (end > p && (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }

        snprintf(value, size, "%.*s", (int)(end - p), p);
        break;
    }

    fclose(fp);
}

static const char *brightness_label(int value)
{
    if (value >= 255) {
        return "MAX";
    }
    if (value > 0) {
        return "MEDIUM";
    }
    return "LOW/OFF";
}

static void status_load_previous(void)
{
    char value[64];

    read_status_value("FND", value, sizeof(value));
    if (value[0] >= '0' && value[0] <= '9') {
        g_status.fnd_value = atoi(value);
    }
}

static void write_status_locked(void)
{
    FILE *fp = open_status_file("w");
    time_t now;
    struct tm *tm_now;
    char updated[32];

    if (fp == NULL) {
        SERVER_PERROR("fopen(status.txt)");
        return;
    }

    now = time(NULL);
    tm_now = localtime(&now);
    if (tm_now != NULL) {
        strftime(updated, sizeof(updated), "%Y-%m-%d %H:%M:%S", tm_now);
    } else {
        snprintf(updated, sizeof(updated), "UNKNOWN");
    }

    fprintf(fp, "%-10s = %s\n", "CLIENT",
            g_status.client_connected ? "CONNECTED" : "DISCONNECTED");
    fprintf(fp, "%-10s = %s\n", "LED", g_status.led_on ? "ON" : "OFF");
    fprintf(fp, "%-10s = %s\n", "BRIGHTNESS",
            brightness_label(g_status.led_brightness));
    fprintf(fp, "%-10s = %s\n", "BUZZER", g_status.buzzer_on ? "ON" : "OFF");
    fprintf(fp, "%-10s = %s\n", "SENSOR", g_status.sensor_on ? "ON" : "OFF");
    if (g_status.light_value < 0) {
        fprintf(fp, "%-10s = %s\n", "LIGHT", "UNKNOWN");
    } else {
        fprintf(fp, "%-10s = %s\n", "LIGHT",
                g_status.light_value == 0 ? "DETECTED" : "NOT_DETECTED");
    }
    if (g_status.fnd_value < 0) {
        fprintf(fp, "%-10s = %s\n", "FND", "UNKNOWN");
    } else {
        fprintf(fp, "%-10s = %d\n", "FND", g_status.fnd_value);
    }
    fprintf(fp, "%-10s = %s\n", "UPDATED", updated);

    fclose(fp);
}

static void status_write(void)
{
    pthread_mutex_lock(&g_status_lock);
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_led(int is_on, int brightness)
{
    pthread_mutex_lock(&g_status_lock);
    g_status.led_on = is_on;
    g_status.led_brightness = brightness;
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_client(int is_connected)
{
    pthread_mutex_lock(&g_status_lock);
    g_status.client_connected = is_connected;
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_buzzer(int is_on)
{
    pthread_mutex_lock(&g_status_lock);
    g_status.buzzer_on = is_on;
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_sensor(int is_on)
{
    pthread_mutex_lock(&g_status_lock);
    g_status.sensor_on = is_on;
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_light(int value)
{
    pthread_mutex_lock(&g_status_lock);
    g_status.light_value = value;
    if (value == 0) {
        g_status.led_on = 0;
        g_status.led_brightness = 0;
    } else {
        g_status.led_on = 1;
        g_status.led_brightness = 255;
    }
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_fnd(int value)
{
    pthread_mutex_lock(&g_status_lock);
    g_status.fnd_value = value;
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void *sensor_thread_fn(void *arg)
{
    struct sensor_arg *sa = (struct sensor_arg *)arg;
    char buf[BUFSIZ];

    while (!sa->stop) {
        int value = sa->light_read_fn();
        const char *status = (value == 0) ? "DETECTED" : "NOT DETECTED";
        status_set_light(value);
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

    /* Redirect stdout/stderr to docs/running.txt for runtime logs */
    {
        const char *log_paths[] = {
            "../docs/running.txt",
            "docs/running.txt"
        };
        FILE *logf = NULL;

        mkdir("../docs", 0777);
        mkdir("docs", 0777);
        for (size_t i = 0; i < sizeof(log_paths) / sizeof(log_paths[0]); i++) {
            logf = fopen(log_paths[i], "a");
            if (logf) {
                break;
            }
        }

        if (logf) {
            /* duplicate file descriptor to stdout and stderr */
            dup2(fileno(logf), STDOUT_FILENO);
            dup2(fileno(logf), STDERR_FILENO);
            /* keep unbuffered for immediate logging */
            setvbuf(stdout, NULL, _IONBF, 0);
        } else {
            SERVER_PERROR("fopen(docs/running.txt)");
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
        SERVER_ERR("dlopen error: %s\n", dlerror());
        return -1;
    }

    buzzer_handle = dlopen(BUZZER_LIB_PATH, RTLD_NOW);
    if (!buzzer_handle) {
        SERVER_ERR("dlopen error: %s\n", dlerror());
        dlclose(led_handle);
        return -1;
    }

    light_handle = dlopen(LIGHT_LIB_PATH, RTLD_NOW);
    if (!light_handle) {
        SERVER_ERR("dlopen error: %s\n", dlerror());
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    fnd_handle = dlopen(FND_LIB_PATH, RTLD_NOW);
    if (!fnd_handle) {
        SERVER_ERR("dlopen error: %s\n", dlerror());
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
        SERVER_ERR("dlsym error: %s\n", dlerror());
        dlclose(fnd_handle);
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    /* Create TCP server socket */
    if ((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        SERVER_PERROR("socket()");
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
            SERVER_PERROR("setsockopt()");
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
        SERVER_PERROR("bind()");
        close(ssock);
        dlclose(fnd_handle);
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    /* Wait for client connections */
    if (listen(ssock, 8) < 0) {
        SERVER_PERROR("listen()");
        close(ssock);
        dlclose(fnd_handle);
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    SERVER_LOG("Server started\n");
    fflush(stdout);
    status_load_previous();
    status_write();

    while (1) {
        int csock;
        int n;

        clen = sizeof(cliaddr);

        /* Accept one client connection */
        csock = accept(ssock, (struct sockaddr *)&cliaddr, &clen);
        if (csock < 0) {
            SERVER_PERROR("accept()");
            continue;
        }

        SERVER_LOG("Client connected\n");
        status_set_client(1);
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
                    status_set_sensor(0);
                }
                status_set_client(0);
                close(csock);
                SERVER_LOG("Client disconnected\n");
                break;
            }

            remove_newline(mesg);
            {
                char log_mesg[BUFSIZ];
                snprintf(log_mesg, sizeof(log_mesg), "%s", mesg);
                to_uppercase(log_mesg);
                SERVER_LOG("Command received: %s\n", log_mesg);
            }

            /* Dispatch command to device library function */
            if (strcmp(mesg, "led on") == 0) {
                led_on();
                status_set_led(1, 255);
                SERVER_LOG("[LED] Device result: success\n");
                write(csock, "Result: LED ON\n", 15);
            } else if (strcmp(mesg, "led off") == 0) {
                led_off();
                status_set_led(0, 0);
                SERVER_LOG("[LED] Device result: success\n");
                write(csock, "Result: LED OFF\n", 16);
            } else if (strncmp(mesg, "led bright ", 11) == 0) {
                int value = atoi(mesg + 11);
                led_brightness(value);
                if (value < 0) {
                    value = 0;
                }
                if (value > 255) {
                    value = 255;
                }
                status_set_led(value > 0, value);
                SERVER_LOG("[LED] Device result: success\n");
                snprintf(mesg, sizeof(mesg), "Result: LED BRIGHT %d\n", value);
                write(csock, mesg, strlen(mesg));
            } else if (strcmp(mesg, "buzzer on") == 0) {
                buzzer_on();
                status_set_buzzer(1);
                SERVER_LOG("[BUZZER] Device result: success\n");
                write(csock, "Result: BUZZER ON\n", 18);
            } else if (strcmp(mesg, "buzzer off") == 0) {
                buzzer_off();
                status_set_buzzer(0);
                SERVER_LOG("[BUZZER] Device result: success\n");
                write(csock, "Result: BUZZER OFF\n", 19);
            } else if (strcmp(mesg, "light read") == 0) {
                if (sensor_tid == 0) {
                    sensor_argp = malloc(sizeof(struct sensor_arg));
                    if (sensor_argp != NULL) {
                        sensor_argp->csock = csock;
                        sensor_argp->stop = 0;
                        sensor_argp->light_read_fn = light_read;
                        if (pthread_create(&sensor_tid, NULL, sensor_thread_fn, sensor_argp) == 0) {
                            status_set_sensor(1);
                            SERVER_LOG("[LIGHT] Sensor push started\n");
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
                            status_set_sensor(1);
                            SERVER_LOG("[LIGHT] Sensor push started\n");
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
                    status_set_sensor(0);
                    SERVER_LOG("[LIGHT] Sensor push stopped\n");
                    write(csock, "Result: SENSOR OFF\n", 17);
                } else {
                    status_set_sensor(0);
                    write(csock, "Result: SENSOR OFF\n", 17);
                }
            } else if (strncmp(mesg, "fnd ", 4) == 0) {
                int num = atoi(mesg + 4);
                fnd_display(num);
                if (num >= 0 && num <= 9) {
                    status_set_fnd(num);
                }
                SERVER_LOG("[FND] Device result: success\n");
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
                    status_set_sensor(0);
                }
                status_set_client(0);
                close(csock);
                SERVER_LOG("Client disconnected\n");
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
