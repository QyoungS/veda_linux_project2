#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define TCP_PORT 5100

int make_daemon(void);

typedef int (*func_void_t)(void);
typedef int (*func_int_t)(int);

struct sensor_arg {
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
static FILE *g_log_file = NULL;

static FILE *open_log_file(void) /* Open docs/running.txt from root or exec/. */
{
    const char *paths[] = {
        "docs/running.txt",
        "../docs/running.txt"
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        FILE *fp = fopen(paths[i], "a");
        if (fp != NULL) {
            return fp;
        }
    }

    return NULL;
}

static void server_log(const char *format, ...) /* Append one server log line. */
{
    va_list args;

    if (g_log_file == NULL) {
        return;
    }

    fprintf(g_log_file, "[SERVER] ");
    va_start(args, format);
    vfprintf(g_log_file, format, args);
    va_end(args);
    fflush(g_log_file);
}

static void server_error(const char *format, ...) /* Append one server error line. */
{
    va_list args;

    if (g_log_file == NULL) {
        return;
    }

    fprintf(g_log_file, "[SERVER] ");
    va_start(args, format);
    vfprintf(g_log_file, format, args);
    va_end(args);
    fflush(g_log_file);
}

static void server_perror(const char *msg) /* Append errno detail to the server log. */
{
    server_error("%s: %s\n", msg, strerror(errno));
}

static void *dlopen_device(const char *name, const char *paths[], int flags) /* Try library paths from root or exec/. */
{
    void *handle = NULL;

    for (size_t i = 0; paths[i] != NULL; i++) {
        handle = dlopen(paths[i], flags);
        if (handle != NULL) {
            return handle;
        }
    }

    server_error("dlopen error for %s: %s\n", name, dlerror());
    return NULL;
}

static FILE *open_status_file(const char *mode) /* Open status.txt from common run paths. */
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

static void read_status_value(const char *key, char *value, size_t size) /* Read one value from status.txt. */
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

static const char *brightness_label(int value) /* Convert PWM value to a display label. */
{
    if (value >= 255) {
        return "MAX";
    }
    if (value > 0) {
        return "MEDIUM";
    }
    return "LOW/OFF";
}

static void status_load_previous(void) /* Restore previous FND value if available. */
{
    char value[64];

    read_status_value("FND", value, sizeof(value));
    if (value[0] >= '0' && value[0] <= '9') {
        g_status.fnd_value = atoi(value);
    }
}

static void write_status_locked(void) /* Write current device status while locked. */
{
    FILE *fp = open_status_file("w");
    time_t now;
    struct tm *tm_now;
    char updated[32];

    if (fp == NULL) {
        server_perror("fopen(status.txt)");
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

static void status_write(void) /* Write current device status safely. */
{
    pthread_mutex_lock(&g_status_lock);
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_led(int is_on, int brightness) /* Update LED status and brightness. */
{
    pthread_mutex_lock(&g_status_lock);
    g_status.led_on = is_on;
    g_status.led_brightness = brightness;
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_client(int is_connected) /* Update client connection status. */
{
    pthread_mutex_lock(&g_status_lock);
    g_status.client_connected = is_connected;
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_buzzer(int is_on) /* Update buzzer status. */
{
    pthread_mutex_lock(&g_status_lock);
    g_status.buzzer_on = is_on;
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_sensor(int is_on) /* Update sensor monitor status. */
{
    pthread_mutex_lock(&g_status_lock);
    g_status.sensor_on = is_on;
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void status_set_light(int value) /* Update light value and derived LED state. */
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

static void status_set_fnd(int value) /* Update the last FND digit. */
{
    pthread_mutex_lock(&g_status_lock);
    g_status.fnd_value = value;
    write_status_locked();
    pthread_mutex_unlock(&g_status_lock);
}

static void *sensor_thread_fn(void *arg) /* Poll the light sensor in the background. */
{
    struct sensor_arg *sa = (struct sensor_arg *)arg;

    while (!sa->stop) {
        int value = sa->light_read_fn();
        status_set_light(value);
        usleep(500000);
    }

    /* do not free here; main thread will free the struct after pthread_join */
    return NULL;
}

static void remove_newline(char *buf) /* Trim CR/LF from socket input. */
{
    buf[strcspn(buf, "\r\n")] = '\0';
}

static void to_uppercase(char *buf) /* Format command text for logs. */
{
    while (*buf != '\0') {
        *buf = (char)toupper((unsigned char)*buf);
        buf++;
    }
}

int main(int argc, char **argv) /* Device server entry point. */
{
    int ssock;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clen;
    char mesg[BUFSIZ];

    void *led_handle;
    void *buzzer_handle;
    void *light_handle;
    void *fnd_handle;
    const char *led_paths[] = {"./lib/libled.so", "./exec/lib/libled.so", NULL};
    const char *buzzer_paths[] = {"./lib/libbuzzer.so", "./exec/lib/libbuzzer.so", NULL};
    const char *light_paths[] = {"./lib/liblight.so", "./exec/lib/liblight.so", NULL};
    const char *fnd_paths[] = {"./lib/libfnd.so", "./exec/lib/libfnd.so", NULL};
    func_void_t led_on;
    func_void_t led_off;
    func_void_t buzzer_on;
    func_void_t buzzer_off;
    func_void_t light_read;
    func_int_t led_brightness;
    func_int_t fnd_display;

    setvbuf(stdout, NULL, _IONBF, 0);

    /* Run as daemon only when -d option is given */
    if (argc >= 2 && strcmp(argv[1], "-d") == 0) {
        int ret = make_daemon();
        if (ret != 0) {
            return ret == 1 ? 0 : -1;
        }
    }

    g_log_file = open_log_file();

    /* Load device shared libraries at runtime */
    led_handle = dlopen_device("LED", led_paths, RTLD_NOW | RTLD_GLOBAL);
    if (!led_handle) {
        return -1;
    }

    buzzer_handle = dlopen_device("BUZZER", buzzer_paths, RTLD_NOW);
    if (!buzzer_handle) {
        dlclose(led_handle);
        return -1;
    }

    light_handle = dlopen_device("LIGHT", light_paths, RTLD_NOW);
    if (!light_handle) {
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    fnd_handle = dlopen_device("FND", fnd_paths, RTLD_NOW);
    if (!fnd_handle) {
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
        server_error("dlsym error: %s\n", dlerror());
        dlclose(fnd_handle);
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    /* Create TCP server socket */
    if ((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        server_perror("socket()");
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
            server_perror("setsockopt()");
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
        server_perror("bind()");
        close(ssock);
        dlclose(fnd_handle);
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    /* Wait for client connections */
    if (listen(ssock, 8) < 0) {
        server_perror("listen()");
        close(ssock);
        dlclose(fnd_handle);
        dlclose(light_handle);
        dlclose(buzzer_handle);
        dlclose(led_handle);
        return -1;
    }

    server_log("Server started\n");
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
            server_perror("accept()");
            continue;
        }

        server_log("Client connected\n");
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
                server_log("Client disconnected\n");
                break;
            }

            remove_newline(mesg);
            {
                char log_mesg[BUFSIZ];
                snprintf(log_mesg, sizeof(log_mesg), "%s", mesg);
                to_uppercase(log_mesg);
                server_log("Command received: %s\n", log_mesg);
            }

            /* Dispatch command to device library function */
            if (strcmp(mesg, "led on") == 0) {
                led_on();
                status_set_led(1, 255);
                server_log("[LED] Device result: success\n");
                write(csock, "Result: LED ON\n", 15);
            } else if (strcmp(mesg, "led off") == 0) {
                led_off();
                status_set_led(0, 0);
                server_log("[LED] Device result: success\n");
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
                server_log("[LED] Device result: success\n");
                snprintf(mesg, sizeof(mesg), "Result: LED BRIGHT %d\n", value);
                write(csock, mesg, strlen(mesg));
            } else if (strcmp(mesg, "buzzer on") == 0) {
                buzzer_on();
                status_set_buzzer(1);
                server_log("[BUZZER] Device result: success\n");
                write(csock, "Result: BUZZER ON\n", 18);
            } else if (strcmp(mesg, "buzzer off") == 0) {
                buzzer_off();
                status_set_buzzer(0);
                server_log("[BUZZER] Device result: success\n");
                write(csock, "Result: BUZZER OFF\n", 19);
            } else if (strcmp(mesg, "light read") == 0) {
                if (sensor_tid == 0) {
                    sensor_argp = malloc(sizeof(struct sensor_arg));
                    if (sensor_argp != NULL) {
                        int value = light_read();
                        const char *light_status =
                            (value == 0) ? "DETECTED" : "NOT DETECTED";
                        status_set_light(value);
                        sensor_argp->stop = 0;
                        sensor_argp->light_read_fn = light_read;
                        if (pthread_create(&sensor_tid, NULL, sensor_thread_fn, sensor_argp) == 0) {
                            status_set_sensor(1);
                            server_log("[LIGHT] Sensor monitor started\n");
                            snprintf(mesg, sizeof(mesg),
                                     "Result: SENSOR ON\n"
                                     "Result: LIGHT %s (value=%d)\n",
                                     light_status, value);
                            write(csock, mesg, strlen(mesg));
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
                    int value = light_read();
                    const char *light_status =
                        (value == 0) ? "DETECTED" : "NOT DETECTED";
                    status_set_light(value);
                    snprintf(mesg, sizeof(mesg),
                             "Result: SENSOR ALREADY ON\n"
                             "Result: LIGHT %s (value=%d)\n",
                             light_status, value);
                    write(csock, mesg, strlen(mesg));
                }
            } else if (strcmp(mesg, "sensor on") == 0) {
                if (sensor_tid == 0) {
                    sensor_argp = malloc(sizeof(struct sensor_arg));
                    if (sensor_argp != NULL) {
                        int value = light_read();
                        const char *light_status =
                            (value == 0) ? "DETECTED" : "NOT DETECTED";
                        status_set_light(value);
                        sensor_argp->stop = 0;
                        sensor_argp->light_read_fn = light_read;
                        if (pthread_create(&sensor_tid, NULL, sensor_thread_fn, sensor_argp) == 0) {
                            status_set_sensor(1);
                            server_log("[LIGHT] Sensor monitor started\n");
                            snprintf(mesg, sizeof(mesg),
                                     "Result: SENSOR ON\n"
                                     "Result: LIGHT %s (value=%d)\n",
                                     light_status, value);
                            write(csock, mesg, strlen(mesg));
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
                    int value = light_read();
                    const char *light_status =
                        (value == 0) ? "DETECTED" : "NOT DETECTED";
                    status_set_light(value);
                    snprintf(mesg, sizeof(mesg),
                             "Result: SENSOR ALREADY ON\n"
                             "Result: LIGHT %s (value=%d)\n",
                             light_status, value);
                    write(csock, mesg, strlen(mesg));
                }
            } else if (strcmp(mesg, "sensor off") == 0) {
                if (sensor_tid != 0 && sensor_argp != NULL) {
                    sensor_argp->stop = 1;
                    pthread_join(sensor_tid, NULL);
                    free(sensor_argp);
                    sensor_tid = 0;
                    sensor_argp = NULL;
                    status_set_sensor(0);
                    server_log("[LIGHT] Sensor monitor stopped\n");
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
                server_log("[FND] Device result: success\n");
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
                server_log("Client disconnected\n");
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
