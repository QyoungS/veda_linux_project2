#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define TCP_PORT 5100
#define BUF_SIZE 1024
#define DEFAULT_SERVER_IP "127.0.0.1"

static volatile sig_atomic_t running = 1;
static pid_t countdown_pid = -1;
static const char *global_server_ip = DEFAULT_SERVER_IP;
static FILE *log_file;

static void write_log(const char *format, ...)
{
	va_list args;

	if (log_file == NULL) {
		return;
	}

	va_start(args, format);
	fprintf(log_file, "[CLIENT] ");
	vfprintf(log_file, format, args);
	va_end(args);

	fprintf(log_file, "\n");
	fflush(log_file);
}

static void sigint_handler(int signo)
{
	(void)signo;
	running = 0;
}

static void ignore_handler(int signo)
{
	printf("\nSignal %d ignored. Press Ctrl+C to exit.\n", signo);
}

static void setup_signal_handlers(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	signal(SIGTERM, ignore_handler);
	signal(SIGQUIT, ignore_handler);
	signal(SIGHUP, ignore_handler);
}

static void print_menu(void)
{
	printf("\n[ Device Control Menu ]\n");
	printf("1. LED ON\n");
	printf("2. LED OFF\n");
	printf("3. Set Brightness\n");
	printf("4. BUZZER ON (play melody)\n");
	printf("5. BUZZER OFF (stop)\n");
	printf("6. LIGHT SENSOR ON\n");
	printf("7. LIGHT SENSOR OFF\n");
	printf("8. SEGMENT DISPLAY\n");
	printf("9. SEGMENT STOP\n");
	printf("10. Exit\n");
	printf("\nSelect: ");
	fflush(stdout);
}

static int make_command(int menu, char *cmd, size_t size)
{
	int value;

	switch (menu) {
	case 1:
		snprintf(cmd, size, "led on");
		break;
	case 2:
		snprintf(cmd, size, "led off");
		break;
	case 3:
		printf("Brightness (1=low/off, 2=medium, 3=max): ");
		fflush(stdout);
		if (scanf("%d", &value) != 1) {
			return -1;
		}

		switch (value) {
		case 1:
			value = 0;
			break;
		case 2:
			value = 128;
			break;
		case 3:
			value = 255;
			break;
		default:
			return -1;
		}

		snprintf(cmd, size, "led bright %d", value);
		break;
	case 4:
		snprintf(cmd, size, "buzzer on");
		break;
	case 5:
		snprintf(cmd, size, "buzzer off");
		break;
	case 6:
		snprintf(cmd, size, "light read");
		break;
	case 7:
		snprintf(cmd, size, "sensor off");
		break;
	case 8:
		printf("Countdown seconds (0~9): ");
		fflush(stdout);
		if (scanf("%d", &value) != 1) {
			return -1;
		}
		if (value < 0 || value > 9) {
			return -1;
		}
		snprintf(cmd, size, "countdown %d", value);
		break;
	case 9:
		snprintf(cmd, size, "segment stop");
		break;
	case 10:
		snprintf(cmd, size, "exit");
		break;
	default:
		return -1;
	}

	return 0;
}

static int connect_server(const char *server_ip)
{
	int sock;
	struct sockaddr_in servaddr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket()");
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(TCP_PORT);

	if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
		perror("inet_pton()");
		close(sock);
		return -1;
	}

	if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("connect()");
		close(sock);
		return -1;
	}

	write_log("Connected to server %s:%d", server_ip, TCP_PORT);

	return sock;
}

static int send_command_result(int sock, const char *cmd, int print_result)
{
	char recvbuf[BUF_SIZE];
	ssize_t recv_len;

	if (send(sock, cmd, strlen(cmd), 0) < 0) {
		perror("send()");
		write_log("Failed to send command: %s", cmd);
		return -1;
	}

	write_log("Sent command: %s", cmd);

	memset(recvbuf, 0, sizeof(recvbuf));
	recv_len = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);

	if (print_result && recv_len > 0) {
		recvbuf[recv_len] = '\0';
		write_log("Received response: %s", recvbuf);
		if (strcmp(cmd, "light read") == 0 ||
		    strcmp(cmd, "sensor off") == 0) {
			printf("[Sensor] %s", recvbuf);
		} else {
			printf("%s", recvbuf);
		}
		if (recvbuf[recv_len - 1] != '\n') {
			printf("\n");
		}
		fflush(stdout);
	}

	return 0;
}

static int send_command(int sock, const char *cmd)
{
	return send_command_result(sock, cmd, 1);
}

static int send_command_silent(int sock, const char *cmd)
{
	return send_command_result(sock, cmd, 0);
}

static int read_sensor_once(int sock)
{
	char recvbuf[BUF_SIZE];
	char *line;
	ssize_t recv_len;

	if (send(sock, "light read", strlen("light read"), 0) < 0) {
		perror("send()");
		write_log("Failed to send command: light read");
		return -1;
	}

	write_log("Sent command: light read");

	memset(recvbuf, 0, sizeof(recvbuf));
	recv_len = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);

	if (recv_len <= 0) {
		write_log("Failed to receive sensor response");
		return -1;
	}

	recvbuf[recv_len] = '\0';
	write_log("Received response: %s", recvbuf);
	line = strtok(recvbuf, "\r\n");
	while (line != NULL) {
		if (strncmp(line, "Result: LIGHT ", 14) == 0) {
			printf("[Sensor] %s\n", line + 8);
			write_log("[Sensor] %s", line + 8);
		} else {
			printf("[Sensor] %s\n", line);
			write_log("[Sensor] %s", line);
		}
		line = strtok(NULL, "\r\n");
	}
	fflush(stdout);

	return 0;
}

static void reap_countdown(void)
{
	if (countdown_pid > 0 && waitpid(countdown_pid, NULL, WNOHANG) == countdown_pid) {
		countdown_pid = -1;
	}
}

static void stop_countdown(void)
{
	if (countdown_pid <= 0) {
		return;
	}

	kill(countdown_pid, SIGTERM);
	waitpid(countdown_pid, NULL, 0);
	countdown_pid = -1;
}

static int start_countdown(int sock, int seconds)
{
	pid_t pid;

	stop_countdown();

	pid = fork();
	if (pid < 0) {
		perror("fork()");
		return -1;
	}

	if (pid == 0) {
		char cmd[BUF_SIZE];
		int value;

		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);

		for (value = seconds; value >= 0; value--) {
			snprintf(cmd, sizeof(cmd), "fnd %d", value);
			send_command_silent(sock, cmd);

			if (value == 0) {
				send_command_silent(sock, "buzzer on");
				break;
			}

			sleep(1);
		}

		_exit(0);
	}

	countdown_pid = pid;
	return 0;
}

static void stop_all_devices(int sock)
{
	const char *commands[] = {
		"led off",
		"buzzer off",
	};
	size_t i;

	for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		if (send_command(sock, commands[i]) < 0) {
			printf("Failed to send command: %s\n", commands[i]);
		}
	}
}

int main(int argc, char **argv)
{
	int menu;
	int sock;
	char cmd[BUF_SIZE];

	{
		const char *log_paths[] = {
			"../docs/running.txt",
			"docs/running.txt"
		};
		size_t i;

		mkdir("../docs", 0777);
		mkdir("docs", 0777);
		for (i = 0; i < sizeof(log_paths) / sizeof(log_paths[0]); i++) {
			log_file = fopen(log_paths[i], "a");
			if (log_file != NULL) {
				break;
			}
		}
	}
	if (log_file == NULL) {
		perror("docs/running.txt");
	}
	write_log("Client started");

	if (argc >= 2) {
		global_server_ip = argv[1];
	}

	setup_signal_handlers();

	sock = connect_server(global_server_ip);
	if (sock < 0) {
		return -1;
	}

	while (running) {
		print_menu();

		if (scanf("%d", &menu) != 1) {
			if (!running) {
				break;
			}
			printf("Invalid input\n");
			write_log("Invalid input");
			break;
		}
		write_log("Selected menu: %d", menu);

		reap_countdown();
		memset(cmd, 0, sizeof(cmd));

		if (make_command(menu, cmd, sizeof(cmd)) < 0) {
			printf("Invalid menu\n");
			write_log("Invalid menu: %d", menu);
			continue;
		}

		if (strncmp(cmd, "countdown ", 10) == 0) {
			if (start_countdown(sock, atoi(cmd + 10)) < 0) {
				printf("Failed to start countdown\n");
				write_log("Failed to start countdown");
			}
			printf("Countdown started. Select 9 to stop.\n");
			write_log("Countdown started: %s", cmd);
			continue;
		}

		if (strcmp(cmd, "segment stop") == 0) {
			stop_countdown();
			printf("Segment countdown stopped\n");
			write_log("Segment countdown stopped");
			continue;
		}

		if (strcmp(cmd, "light read") == 0) {
			if (read_sensor_once(sock) < 0) {
				printf("Failed to send command\n");
				write_log("Failed to read sensor");
			}
			continue;
		}

		if (strcmp(cmd, "exit") == 0) {
			stop_countdown();
			stop_all_devices(sock);
			if (send_command(sock, cmd) < 0) {
				printf("Failed to send command\n");
				write_log("Failed to send command: %s", cmd);
			}
			break;
		}

		if (send_command(sock, cmd) < 0) {
			printf("Failed to send command\n");
			write_log("Failed to send command: %s", cmd);
			break;
		}
	}

	stop_countdown();
	close(sock);
	printf("\nClient terminated\n");
	write_log("Client terminated");
	if (log_file != NULL) {
		fclose(log_file);
	}

	return 0;
}
