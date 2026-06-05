#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define TCP_PORT 5100
#define BUF_SIZE 1024
#define DEFAULT_SERVER_IP "127.0.0.1"

static volatile sig_atomic_t running = 1;
static pid_t countdown_pid = -1;
static const char *global_server_ip = DEFAULT_SERVER_IP;

static void sigint_handler(int signo) /* Allow Ctrl+C to stop the main loop. */
{
	(void)signo;
	running = 0;
}

static void ignore_handler(int signo) /* Keep the client alive for non-INT signals. */
{
	printf("\nSignal %d ignored. Press Ctrl+C to exit.\n", signo);
}

static void setup_signal_handlers(void) /* Register client signal behavior. */
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

static void print_menu(void) /* Show the device control menu. */
{
	printf("\n");
	printf("+------------------------------------------------+\n");
	printf("|            VEDA DEVICE CONTROL CLIENT          |\n");
	printf("+------------------------------------------------+\n");
	printf("| LED        1) ON              2) OFF           |\n");
	printf("|            3) Brightness                       |\n");
	printf("|                                                |\n");
	printf("| BUZZER     4) Play Melody      5) Stop         |\n");
	printf("| SENSOR     6) Start Monitor    7) Stop Monitor |\n");
	printf("| FND        8) Countdown        9) Stop         |\n");
	printf("|                                                |\n");
	printf("| SYSTEM    10) Exit                             |\n");
	printf("+------------------------------------------------+\n");
	printf("Select menu > ");
	fflush(stdout);
}

static int make_command(int menu, char *cmd, size_t size) /* Convert menu input to a server command. */
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

static int connect_server(const char *server_ip) /* Open a TCP connection to the device server. */
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

	return sock;
}

static int send_command_result(int sock, const char *cmd, int print_result) /* Send one command and read its response. */
{
	char recvbuf[BUF_SIZE];
	ssize_t recv_len;

	if (send(sock, cmd, strlen(cmd), 0) < 0) {
		perror("send()");
		return -1;
	}

	memset(recvbuf, 0, sizeof(recvbuf));
	recv_len = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);

	if (print_result && recv_len > 0) {
		recvbuf[recv_len] = '\0';
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

static int send_command(int sock, const char *cmd) /* Send a command and print the response. */
{
	return send_command_result(sock, cmd, 1);
}

static int send_command_silent(int sock, const char *cmd) /* Send a command without printing the response. */
{
	return send_command_result(sock, cmd, 0);
}

static int read_sensor_once(int sock) /* Request and print one light sensor reading. */
{
	char recvbuf[BUF_SIZE];
	char *line;
	ssize_t recv_len;

	if (send(sock, "light read", strlen("light read"), 0) < 0) {
		perror("send()");
		return -1;
	}

	memset(recvbuf, 0, sizeof(recvbuf));
	recv_len = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);

	if (recv_len <= 0) {
		return -1;
	}

	recvbuf[recv_len] = '\0';
	line = strtok(recvbuf, "\r\n");
	while (line != NULL) {
		if (strncmp(line, "Result: LIGHT ", 14) == 0) {
			printf("[Sensor] %s\n", line + 8);
		} else {
			printf("[Sensor] %s\n", line);
		}
		line = strtok(NULL, "\r\n");
	}
	fflush(stdout);

	return 0;
}

static void reap_countdown(void) /* Reap a finished countdown child process. */
{
	if (countdown_pid > 0 && waitpid(countdown_pid, NULL, WNOHANG) == countdown_pid) {
		countdown_pid = -1;
	}
}

static void stop_countdown(void) /* Stop the active countdown child process. */
{
	if (countdown_pid <= 0) {
		return;
	}

	kill(countdown_pid, SIGTERM);
	waitpid(countdown_pid, NULL, 0);
	countdown_pid = -1;
}

static int start_countdown(int sock, int seconds) /* Run the FND countdown in a child process. */
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

static void stop_all_devices(int sock) /* Turn off devices before client exit. */
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

int main(int argc, char **argv) /* Client entry point. */
{
	int menu;
	int sock;
	char cmd[BUF_SIZE];

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
			break;
		}

		reap_countdown();
		memset(cmd, 0, sizeof(cmd));

		if (make_command(menu, cmd, sizeof(cmd)) < 0) {
			printf("Invalid menu\n");
			continue;
		}

		if (strncmp(cmd, "countdown ", 10) == 0) {
			if (start_countdown(sock, atoi(cmd + 10)) < 0) {
				printf("Failed to start countdown\n");
			}
			printf("Countdown started. Select 9 to stop.\n");
			continue;
		}

		if (strcmp(cmd, "segment stop") == 0) {
			stop_countdown();
			printf("Segment countdown stopped\n");
			continue;
		}

		if (strcmp(cmd, "light read") == 0) {
			if (read_sensor_once(sock) < 0) {
				printf("Failed to send command\n");
			}
			continue;
		}

		if (strcmp(cmd, "exit") == 0) {
			stop_countdown();
			stop_all_devices(sock);
			if (send_command(sock, cmd) < 0) {
				printf("Failed to send command\n");
			}
			break;
		}

		if (send_command(sock, cmd) < 0) {
			printf("Failed to send command\n");
			break;
		}
	}

	stop_countdown();
	close(sock);
	printf("\nClient terminated\n");

	return 0;
}
