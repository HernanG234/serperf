#include <argp.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

enum msg_types {
	PING_PONG,
	MSG_TYPES
};

const char *argp_program_version = "serperf 0.1.0";
const char *argp_program_bug_address = "<ezequiel@vanguardiasur.com.ar>";
static char doc[] = "Test /dev/serial performance.";
static char args_doc[] = "DEVICE";

static struct argp_option options[] = {
	{ "server", 's', 0, 0, "Server mode"},
	{ "client", 'c', 0, 0, "Client mode"},
	{ 0 }
};

struct arguments {
	enum { SERVER, CLIENT } mode;
	char device[64];
};

struct msg_header {
	int type;
	int len;
	int crc;
};

struct msg {
	struct msg_header header;
	char payload[1024];
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;
	switch (key) {
	case 's': arguments->mode = SERVER; break;
	case 'c': arguments->mode = CLIENT; break;
	case ARGP_KEY_ARG:
		  if (state->arg_num >= 1) {
			  argp_usage(state);
		  }
		  snprintf(arguments->device, 63, arg);
		  break;
	case ARGP_KEY_END:
		  if (state->arg_num < 1)
			  argp_usage (state);
		  break;
	default: return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

int check_crc(struct msg *msg)
{
	return 0;
}

static void receive_reply(int fd, struct msg *msg)
{
	int ret;

	ret = read(fd, &msg->header, sizeof(struct msg_header));
	if (ret > 0 && ret < sizeof(struct msg_header)) {
		printf("server: read %d bytes expected %d\n", ret, sizeof(struct msg_header));
		exit(1);
	} else if (ret < 0) {
		/* TODO: Handle timeout. If timeout just continue. */
		perror("server read error: ");
		exit(1);
	}

	ret = read(fd, msg->payload, msg->header.len);
	if (ret > 0 && ret < msg->header.len) {
		printf("server: read %d bytes expected %d\n", ret, msg->header.len);
		exit(1);
	} else if (ret < 0) {
		perror("read error: ");
		exit(1);
	}
}

static void send_msg(int fd)
{
	struct msg msg;
	int ret;

	sprintf(msg.payload, "HELLO");
	msg.header.type = PING_PONG;
	msg.header.len = strlen(msg.payload) + 1;
	msg.header.crc = 0;//get_crc(msg);

	ret = write(fd, &msg.header, sizeof(struct msg_header));
	if (ret > 0 && ret < sizeof(struct msg_header)) {
		printf("server: write %d bytes expected %d\n", ret, sizeof(struct msg_header));
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit(1);
	}

	ret = write(fd, msg.payload, msg.header.len);
	if (ret > 0 && ret < msg.header.len) {
		printf("server: write %d bytes expected %d\n", ret, msg.header.len);
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit(1);
	}
}

static void run_client(int fd)
{
	struct msg msg;

	while (1) {
		/* TODO: Timeout not fatal everywhere! */
		send_msg(fd);
		receive_reply(fd, &msg);

		if (strcmp(msg.payload, "HELLO")) {
			printf("client: server reply is %s not HELLO\n", msg.payload);
			exit(1);
		}
	}
}

static void ping_pong(int fd, const char *payload, int len)
{
	struct msg msg;
	int ret;

	memcpy(msg.payload, payload, len);
	msg.type = PING_PONG;
	msg.len = len;
	msg.crc = 0;//get_crc(msg);

	ret = write(fd, &msg.header, sizeof(struct msg_header));
	if (ret > 0 && ret < sizeof(struct msg_header)) {
		printf("server: write %d bytes expected %d\n", ret, sizeof(struct msg_header));
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit(1);
	}

	ret = write(fd, msg.payload, msg.header.len);
	if (ret > 0 && ret < msg.header.len) {
		printf("server: write %d bytes expected %d\n", ret, msg.header.len);
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit(1);
	}
}

static void run_server(int fd)
{
	struct msg msg;
	int ret;

	while (1) {
		ret = read(fd, &msg.header, sizeof(struct msg_header));
		if (ret > 0 && ret < sizeof(struct msg_header)) {
			printf("server: read %d bytes expected %d\n", ret, sizeof(struct msg_header));
			exit(1);
		} else if (ret < 0) {
			/* TODO: Handle timeout. If timeout just continue. */
			perror("server read error: ");
			exit(1);
		}

		ret = read(fd, msg.payload, msg.header.len);
		if (ret > 0 && ret < msg.header.len) {
			printf("server: read %d bytes expected %d\n", ret, msg.header.len);
			exit(1);
		} else if (ret < 0) {
			perror("read error: ");
			exit(1);
		}

		ret = check_crc(&msg);
		if (!ret) {
			printf("bad crc\n");
			exit(1);
		}

		switch (msg.type) {
		case PING_PONG:
			ping_pong(fd, msg.payload, msg.header.len);
			break;
		default:
			printf("oops, unknown message type. something is wrong!\n");
			exit(1);
		}
	}
}

int main(int argc, char *argv[])
{
	struct arguments arguments;
	struct stat dev_stat;
	int fd, ret;

	/* Add arguments to client mode to stop after i) N bytes or ii) M seconds. */
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	fd = open(arguments.device, O_RDWR);
	if (fd < 0) {
		perror("open error: ");
		exit(1);
	}

	ret = fstat(fd, &dev_stat);
	if (ret < 0) {
		perror("fstat error: ");
		exit(1);
	}

	if (!S_ISCHR(dev_stat.st_mode)) {
		printf ("%s (%lu) is not a character device/n", arguments.device, dev_stat.st_dev);
		exit(1);
	}

	if (arguments.mode == SERVER)
		run_server(fd);
	else
		run_client(fd);
	return 0;
}
