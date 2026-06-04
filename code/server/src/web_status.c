#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define DEFAULT_WEB_PORT 8080
#define STATUS_BUF_SIZE 4096

static FILE *open_status_file(void)
{
    const char *paths[] = {
        "status.txt",
        "exec/status.txt",
        "../exec/status.txt"
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        FILE *fp = fopen(paths[i], "r");
        if (fp != NULL) {
            return fp;
        }
    }

    return NULL;
}

static void load_status(char *buf, size_t size)
{
    FILE *fp = open_status_file();
    size_t used = 0;
    char line[256];

    buf[0] = '\0';

    if (fp == NULL) {
        snprintf(buf, size,
                 "STATUS=NOT_READY\n"
                 "MESSAGE=status.txt was not found. Start the device server first.\n");
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        int written;

        if (used >= size - 1) {
            break;
        }

        written = snprintf(buf + used, size - used, "%s", line);
        if (written < 0) {
            break;
        }
        if ((size_t)written >= size - used) {
            used = size - 1;
            break;
        }
        used += (size_t)written;
    }

    fclose(fp);
}

static void html_escape(const char *src, char *dst, size_t size)
{
    size_t used = 0;

    while (*src != '\0' && used + 1 < size) {
        const char *rep = NULL;

        switch (*src) {
        case '&':
            rep = "&amp;";
            break;
        case '<':
            rep = "&lt;";
            break;
        case '>':
            rep = "&gt;";
            break;
        default:
            dst[used++] = *src++;
            continue;
        }

        if (used + strlen(rep) >= size) {
            break;
        }
        strcpy(dst + used, rep);
        used += strlen(rep);
        src++;
    }

    dst[used] = '\0';
}

static void get_status_value(const char *status, const char *key,
                             char *value, size_t size)
{
    const char *line = status;
    size_t key_len = strlen(key);

    snprintf(value, size, "UNKNOWN");

    while (*line != '\0') {
        const char *next = strchr(line, '\n');
        const char *p = line;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (strncmp(p, key, key_len) == 0) {
            const char *end;
            size_t len;

            p += key_len;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (*p == '=') {
                p++;
                while (*p == ' ' || *p == '\t') {
                    p++;
                }
                end = next != NULL ? next : p + strlen(p);
                while (end > p && (end[-1] == ' ' || end[-1] == '\t' ||
                                    end[-1] == '\r')) {
                    end--;
                }
                len = (size_t)(end - p);
                if (len >= size) {
                    len = size - 1;
                }
                memcpy(value, p, len);
                value[len] = '\0';
                return;
            }
        }

        if (next == NULL) {
            break;
        }
        line = next + 1;
    }
}

static void send_status_page(int csock)
{
    char status[STATUS_BUF_SIZE];
    char escaped[STATUS_BUF_SIZE * 2];
    char client_state[64];
    char client_class[32];
    char body[STATUS_BUF_SIZE * 3];
    char header[512];
    int body_len;
    int header_len;

    load_status(status, sizeof(status));
    get_status_value(status, "CLIENT", client_state, sizeof(client_state));
    snprintf(client_class, sizeof(client_class), "%s",
             strcmp(client_state, "CONNECTED") == 0 ? "connected" : "disconnected");
    html_escape(status, escaped, sizeof(escaped));

    body_len = snprintf(
        body, sizeof(body),
        "<!doctype html>"
        "<html><head>"
        "<meta charset=\"utf-8\">"
        "<meta http-equiv=\"refresh\" content=\"0.3\">"
        "<title>Device Status</title>"
        "<style>"
        "body{font-family:Arial,sans-serif;margin:40px;background:#f5f7fb;color:#172033;}"
        "main{position:relative;max-width:760px;margin:0 auto;background:white;border:1px solid #d9e1ee;padding:28px;}"
        "h1{font-size:28px;margin:0 0 18px;}"
        ".client{position:absolute;right:28px;top:28px;padding:8px 12px;border:1px solid #cbd5e1;font-weight:bold;font-size:14px;}"
        ".client.connected{background:#dcfce7;color:#166534;border-color:#86efac;}"
        ".client.disconnected{background:#fee2e2;color:#991b1b;border-color:#fca5a5;}"
        "pre{font-size:18px;line-height:1.7;background:#111827;color:#e5e7eb;padding:20px;overflow:auto;}"
        "p{color:#526071;}"
        "</style>"
        "</head><body><main>"
        "<div class=\"client %s\">CLIENT: %s</div>"
        "<h1>Device Status Dashboard</h1>"
        "<p>Auto refreshes every 0.3 seconds.</p>"
        "<pre>%s</pre>"
        "</main></body></html>",
        client_class,
        client_state,
        escaped);

    if (body_len < 0) {
        return;
    }
    if ((size_t)body_len >= sizeof(body)) {
        body_len = (int)sizeof(body) - 1;
    }

    header_len = snprintf(
        header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        body_len);

    if (header_len > 0) {
        write(csock, header, (size_t)header_len);
        write(csock, body, (size_t)body_len);
    }
}

int main(int argc, char **argv)
{
    int ssock;
    int port = DEFAULT_WEB_PORT;
    struct sockaddr_in servaddr;

    if (argc >= 2) {
        port = atoi(argv[1]);
    }

    ssock = socket(AF_INET, SOCK_STREAM, 0);
    if (ssock < 0) {
        perror("socket()");
        return -1;
    }

    {
        int optval = 1;
        if (setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR,
                       &optval, sizeof(optval)) < 0) {
            perror("setsockopt()");
            close(ssock);
            return -1;
        }
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons((unsigned short)port);

    if (bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind()");
        close(ssock);
        return -1;
    }

    if (listen(ssock, 10) < 0) {
        perror("listen()");
        close(ssock);
        return -1;
    }

    printf("Web status server started on port %d\n", port);

    while (1) {
        int csock = accept(ssock, NULL, NULL);
        char request[1024];

        if (csock < 0) {
            perror("accept()");
            continue;
        }

        read(csock, request, sizeof(request) - 1);
        send_status_page(csock);
        close(csock);
    }

    close(ssock);
    return 0;
}
