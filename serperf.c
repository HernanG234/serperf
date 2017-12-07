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
#include <time.h>
#include <unistd.h>

enum msg_types {
	PING_PONG,
	MSG_TYPES
};

const char *argp_program_version = "serperf 0.1.0";
const char *argp_program_bug_address = "<ezequiel@vanguardiasur.com.ar>";
static char doc[] = "Test /dev/serial performance.";
static char args_doc[] = "-s|c (-l bytes) (-b bytes) (-t seconds) DEVICE";

static struct argp_option options[] = {
	{ "server", 's', 0, 0, "Server mode"},
	{ "client", 'c', 0, 0, "Client mode"},
	{ "msg-length", 'l', "length", 0, "Set message length"},
	{ "messages", 'm', "msgs", 0, "Client: Number of bytes to send"},
	{ "time", 't', "seconds", 0, "Client: Seconds to transmit data"},
	{ 0 }
};

struct arguments {
	enum { SERVER, CLIENT } mode;
	int length;
	enum { DEFAULT, MSGS, SECONDS} mors;
	int limit;
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
	case 'l': arguments->length = strtol(arg, NULL, 10); break;
	case 'm': arguments->mors = MSGS;
		  arguments->limit = strtol(arg, NULL, 10); break;
	case 't': arguments->mors = SECONDS;
		  arguments->limit = strtol(arg, NULL, 10); break;
	case ARGP_KEY_ARG:
		  if (state->arg_num >= 1) {
			  argp_usage(state);
		  }
		  snprintf(arguments->device, 63, "%s", arg);
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
	return 1;
}

static void receive_reply(int fd, struct msg *msg)
{
	int ret, err;

	ret = read(fd, &msg->header, sizeof(struct msg_header));
	err = errno;
	if (ret > 0 && ret < (int) sizeof(struct msg_header)) {
		printf("server: read %d bytes expected %lu\n", ret, sizeof(struct msg_header));
		exit(1);
	} else if (ret < 0  && err != ETIMEDOUT) {
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

static void send_msg(int fd, int len)
{
	struct msg msg;
	int ret;

	memset(msg.payload, 0x55, len);
	msg.header.type = PING_PONG;
	msg.header.len = len;
	msg.header.crc = 0;//get_crc(msg);

	ret = write(fd, &msg.header, sizeof(struct msg_header));
	if (ret > 0 && ret < (int) sizeof(struct msg_header)) {
		printf("server: write %d bytes expected %lu\n", ret, sizeof(struct msg_header));
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

static void run_client_msgs(int fd, int len, char *cmp, int limit)
{
	struct msg msg;
	int count = 0;

	while (count < limit) {
		/* TODO: Timeout not fatal everywhere! */
		send_msg(fd, len);
		count++;
		receive_reply(fd, &msg);

		if (memcmp(msg.payload, cmp, len)) {
			printf("client: server reply is %s not the same\n", msg.payload);
			exit(1);
		}
	}
	printf("Finished sending %d messages\n", limit);
}

static void run_client_seconds(int fd, int len, char *cmp, int limit)
{
	struct msg msg;
	long int start = (long int) time(NULL);
	long int count = (long int) (time(NULL) - start);

	while (count < limit) {
		/* TODO: Timeout not fatal everywhere! */
		send_msg(fd, len);
		receive_reply(fd, &msg);

		if (memcmp(msg.payload, cmp, len)) {
			printf("client: server reply is %s not the same\n", msg.payload);
			exit(1);
		}
		count = (long int) (time(NULL) - start);
	}
	printf("Finished sending data for %d seconds\n", limit);
}

static void run_client(int fd, int len, int mors, int limit)
{
	struct msg msg;
	char cmp[len];
	int count = 0;

	memset(cmp, 0x55, len);
	switch (mors) {
	case MSGS:
		run_client_msgs(fd, len, cmp, limit);
		break;
	case SECONDS:
		run_client_seconds(fd, len, cmp, limit);
		break;
	default:
		while (1) {
			/* TODO: Timeout not fatal everywhere! */
			send_msg(fd, len);
			count += (len + sizeof(msg.header));
			receive_reply(fd, &msg);

			if (memcmp(msg.payload, cmp, len)) {
				printf("client: server reply is %s not the same\n", msg.payload);
				exit(1);
			}
		}
	}
}

static void ping_pong(int fd, const char *payload, int len)
{
	struct msg msg;
	int ret;

	memcpy(msg.payload, payload, len);
	msg.header.type = PING_PONG;
	msg.header.len = len;
	msg.header.crc = 0;//get_crc(msg);

	ret = write(fd, &msg.header, sizeof(struct msg_header));
	if (ret > 0 && ret < (int) sizeof(struct msg_header)) {
		printf("server: write %d bytes expected %lu\n", ret, sizeof(struct msg_header));
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
	int ret, err;

	while (1) {
		ret = read(fd, &msg.header, sizeof(struct msg_header));
		err = errno;
		if (ret > 0 && ret < (int) sizeof(struct msg_header)) {
			printf("server: read %d bytes expected %lu\n", ret, sizeof(struct msg_header));
			exit(1);
		} else if (ret < 0 && err != ETIMEDOUT) {
			/* TODO: Handle timeout. If timeout just continue. */
			perror("server read error: ");
			exit(1);
		}

		ret = read(fd, msg.payload, msg.header.len);
		err = errno;
		if (ret > 0 && ret < msg.header.len) {
			printf("server: read %d bytes expected %d\n", ret, msg.header.len);
			exit(1);
		} else if (ret < 0 && err != -60) {
			perror("read error: ");
			exit(1);
		}

		ret = check_crc(&msg);
		if (!ret) {
			printf("bad crc\n");
			exit(1);
		}

		switch (msg.header.type) {
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
	int fd, ret, len, mors, limit;

	/* Default options */
	arguments.length = 1024;
	arguments.mors = DEFAULT;

	/* Add arguments to client mode to stop after i) N bytes or ii) M seconds. */
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	printf ("Device = %s\nMode = %s\nMessage length = %d\n",
			arguments.device,
			arguments.mode ? "Client" : "Server",
			arguments.length);

	if (arguments.mors) {
		printf ("Send %s%d %s\n",
			arguments.mors - 1 ? "for " : "",
			arguments.limit,
			arguments.mors -1 ? "seconds" : "messages");
	}

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

	if (arguments.length < 0 || arguments.length > 1024) {
		printf ("%d bytes: not a valid length (0 < length <= 1024)\n",
			arguments.length);
		exit(1);
	}
	len = arguments.length;
	mors = arguments.mors;
	limit = arguments.limit;

	if (arguments.mode == SERVER)
		run_server(fd);
	else
		run_client(fd, len, mors, limit);
	return 0;
}
