#include <argp.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
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
	REQ_BYTES,
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
	{ "msg-type", 'x', "type", 0, "PING_PONG = 0 | REQ_BYTES = 1"},
	{ "req-bytes", 'r', "bytes", 0, "Request n bytes from server"},
	{ "messages", 'm', "msgs", 0, "Client: Number of bytes to send"},
	{ "time", 't', "seconds", 0, "Client: Seconds to transmit data"},
	{ 0 }
};

struct arguments {
	enum { SERVER, CLIENT } mode;
	int length;
	int type;
	int rqbytes;
	enum { DEFAULT, MSGS, SECONDS} mors;
	int limit;
	char device[64];
};

struct msg_header {
	int len;
	int type;
	unsigned int crc;
};

struct msg {
	struct msg_header header;
	unsigned char payload[1024];
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;
	switch (key) {
	case 's': arguments->mode = SERVER; break;
	case 'c': arguments->mode = CLIENT; break;
	case 'l': arguments->length = strtol(arg, NULL, 10); break;
	case 'x': arguments->type = strtol(arg, NULL, 10); break;
	case 'r': arguments->rqbytes = strtol(arg, NULL, 10); break;
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

static unsigned char const crc8_table[] = {
	0xea, 0xd4, 0x96, 0xa8, 0x12, 0x2c, 0x6e, 0x50, 0x7f, 0x41, 0x03, 0x3d,
	0x87, 0xb9, 0xfb, 0xc5, 0xa5, 0x9b, 0xd9, 0xe7, 0x5d, 0x63, 0x21, 0x1f,
	0x30, 0x0e, 0x4c, 0x72, 0xc8, 0xf6, 0xb4, 0x8a, 0x74, 0x4a, 0x08, 0x36,
        0x8c, 0xb2, 0xf0, 0xce, 0xe1, 0xdf, 0x9d, 0xa3, 0x19, 0x27, 0x65, 0x5b,
	0x3b, 0x05, 0x47, 0x79, 0xc3, 0xfd, 0xbf, 0x81, 0xae, 0x90, 0xd2, 0xec,
        0x56, 0x68, 0x2a, 0x14, 0xb3, 0x8d, 0xcf, 0xf1,	0x4b, 0x75, 0x37, 0x09,
	0x26, 0x18, 0x5a, 0x64, 0xde, 0xe0, 0xa2, 0x9c, 0xfc, 0xc2, 0x80, 0xbe,
        0x04, 0x3a, 0x78, 0x46, 0x69, 0x57, 0x15, 0x2b, 0x91, 0xaf, 0xed, 0xd3,
	0x2d, 0x13, 0x51, 0x6f, 0xd5, 0xeb, 0xa9, 0x97, 0xb8, 0x86, 0xc4, 0xfa,
	0x40, 0x7e, 0x3c, 0x02, 0x62, 0x5c, 0x1e, 0x20, 0x9a, 0xa4, 0xe6, 0xd8,
	0xf7, 0xc9, 0x8b, 0xb5, 0x0f, 0x31, 0x73, 0x4d, 0x58, 0x66, 0x24, 0x1a,
	0xa0, 0x9e, 0xdc, 0xe2, 0xcd, 0xf3, 0xb1, 0x8f,	0x35, 0x0b, 0x49, 0x77,
	0x17, 0x29, 0x6b, 0x55, 0xef, 0xd1, 0x93, 0xad, 0x82, 0xbc, 0xfe, 0xc0,
	0x7a, 0x44, 0x06, 0x38, 0xc6, 0xf8, 0xba, 0x84, 0x3e, 0x00, 0x42, 0x7c,
	0x53, 0x6d, 0x2f, 0x11, 0xab, 0x95, 0xd7, 0xe9,	0x89, 0xb7, 0xf5, 0xcb,
	0x71, 0x4f, 0x0d, 0x33,	0x1c, 0x22, 0x60, 0x5e, 0xe4, 0xda, 0x98, 0xa6,
	0x01, 0x3f, 0x7d, 0x43, 0xf9, 0xc7, 0x85, 0xbb, 0x94, 0xaa, 0xe8, 0xd6,
	0x6c, 0x52, 0x10, 0x2e, 0x4e, 0x70, 0x32, 0x0c,	0xb6, 0x88, 0xca, 0xf4,
	0xdb, 0xe5, 0xa7, 0x99, 0x23, 0x1d, 0x5f, 0x61, 0x9f, 0xa1, 0xe3, 0xdd,
	0x67, 0x59, 0x1b, 0x25, 0x0a, 0x34, 0x76, 0x48, 0xf2, 0xcc, 0x8e, 0xb0,
	0xd0, 0xee, 0xac, 0x92, 0x28, 0x16, 0x54, 0x6a, 0x45, 0x7b, 0x39, 0x07,
	0xbd, 0x83, 0xc1, 0xff};

unsigned int crc8(unsigned int crc, unsigned char const *data, int len)
{
	if (data == NULL)
		return 0;
	crc &= 0xff;
	unsigned char const *end = data + len;
	while (data < end)
		crc = crc8_table[crc ^ *data++];
	return crc;
}

int check_crc(struct msg *msg)
{
	if (msg->header.crc != crc8(0, msg->payload, msg->header.len))
		return 1;

	return 0;
}

static void receive_reply(int fd, struct msg *msg)
{
	int ret, err;

	ret = read(fd, &msg->header, sizeof(struct msg_header));
	err = errno;
	if (ret > 0 && ret < (int) sizeof(struct msg_header)) {
		printf("server: read %d bytes expected %u\n", ret, sizeof(struct msg_header));
		exit(1);
	} else if (ret < 0  && err != ETIMEDOUT) {
		/* TODO: Handle timeout. If timeout just continue. */
		perror("server read error: ");
		exit(1);
	}

	ret = read(fd, msg->payload, msg->header.len);
	if (ret > 0 && ret < (int) msg->header.len) {
		printf("server: read %d bytes expected %d\n", ret, msg->header.len);
		exit(1);
	} else if (ret < 0) {
		perror("read error: ");
		exit(1);
	}
}

static void send_msg(int fd, int len, int type, int rqbytes)
{
	struct msg msg;
	int ret;

	msg.header.type = type;
	if (type == REQ_BYTES){
		memcpy(msg.payload, &rqbytes, 4);
		msg.header.len = 4;
	} else {
		msg.header.len = len;
		memset(msg.payload, 0x55, len);
	}
	msg.header.crc = crc8(0, msg.payload, msg.header.len);

	/* This should be just one write() */
	ret = write(fd, &msg.header, sizeof(struct msg_header));
	if (ret > 0 && ret < (int) sizeof(struct msg_header)) {
		printf("server: write %d bytes expected %u\n", ret, sizeof(struct msg_header));
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit(1);
	}

	printf("Gonna write payload %d\n", msg.header.len);
	ret = write(fd, msg.payload, msg.header.len);
	if (ret > 0 && ret < (int) msg.header.len) {
		printf("server: write %d bytes expected %d\n", ret, msg.header.len);
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit(1);
	}
	printf("%s finished\n", __func__);
}

static void do_check(const unsigned char *payload, char *cmp, int msglen, int type,
		     int rqbytes)
{
	switch (type) {
	case REQ_BYTES:
		if (msglen != rqbytes) {
			printf("client: server reply does not have match with"
			       " the bytes requested (requested = %d) (got = %d)\n",
			       rqbytes, msglen);
			exit(1);
		}
		break;
	case PING_PONG:
		if (memcmp(payload, cmp, msglen)) {
			printf("client: server reply is %s not the same\n", payload);
			exit(1);
		}
	}

}

static void run_client_msgs(int fd, int len, int type, int rqbytes,
			    char *cmp, int limit)
{
	struct msg msg;
	int count = 0;

	while (count < limit) {
		/* TODO: Timeout not fatal everywhere! */
		send_msg(fd, len, type, rqbytes);
		count++;
		receive_reply(fd, &msg);
		do_check(msg.payload, cmp, len, type, rqbytes);
	}
	printf("Finished sending %d messages\n", limit);
}

static void run_client_seconds(int fd, int len, int type, int rqbytes,
			       char *cmp, int limit)
{
	struct msg msg;
	long int start = (long int) time(NULL);
	long int count = (long int) (time(NULL) - start);

	while (count < limit) {
		/* TODO: Timeout not fatal everywhere! */
		send_msg(fd, len, type, rqbytes);
		receive_reply(fd, &msg);
		do_check(msg.payload, cmp, len, type, rqbytes);
		count = (long int) (time(NULL) - start);
	}
	printf("Finished sending data for %d seconds\n", limit);
}

static void run_client(int fd, int len, int type, int rqbytes,
		       int mors, int limit)
{
	struct msg msg;
	char cmp[len];

	memset(cmp, 0x55, len);
	switch (mors) {
	case MSGS:
		run_client_msgs(fd, len, type, rqbytes, cmp, limit);
		break;
	case SECONDS:
		run_client_seconds(fd, len, type, rqbytes, cmp, limit);
		break;
	default:
		while (1) {
			/* TODO: Timeout not fatal everywhere! */
			send_msg(fd, len, type, rqbytes);
			receive_reply(fd, &msg);
			do_check(msg.payload, cmp, msg.header.len, type, rqbytes);
		}
	}
}

static void ping_pong(int fd, const unsigned char *payload, int len)
{
	struct msg msg;
	int ret;

	memcpy(msg.payload, payload, len);
	msg.header.type = PING_PONG;
	msg.header.len = len;
	msg.header.crc = crc8(0, msg.payload, msg.header.len);

	/* Make it just one write() */
	ret = write(fd, &msg.header, sizeof(struct msg_header));
	printf("Written\n");
	if (ret > 0 && ret < (int) sizeof(struct msg_header)) {
		printf("server: write %d bytes expected %u\n", ret, sizeof(struct msg_header));
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit(1);
	}

	ret = write(fd, msg.payload, msg.header.len);
	if (ret > 0 && ret < (int) msg.header.len) {
		printf("server: write %d bytes expected %d\n", ret, msg.header.len);
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit(1);
	}
}

static void req_bytes(int fd, const unsigned char *payload, int len)
{
	struct msg msg;
	int ret;

	memcpy(&msg.header.len, payload, len);
	memset(msg.payload, 0x55, msg.header.len);
	msg.header.type = REQ_BYTES;
	msg.header.crc = crc8(0, msg.payload, msg.header.len);

	/* Make it just one write() */
	ret = write(fd, &msg.header, sizeof(struct msg_header));
	if (ret > 0 && ret < (int) sizeof(struct msg_header)) {
		printf("server: write %d bytes expected %u\n", ret, sizeof(struct msg_header));
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit(1);
	}

	ret = write(fd, msg.payload, msg.header.len);
	if (ret > 0 && ret < (int) msg.header.len) {
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
		printf("Gonna read header\n");
		ret = read(fd, &msg.header, sizeof(struct msg_header));
		printf("READ header\n");
		err = errno;
		if (ret > 0 && ret < (int) sizeof(struct msg_header)) {
			printf("server: read %d bytes expected %u\n", ret, sizeof(struct msg_header));
			exit(1);
		} else if (ret < 0 && err != ETIMEDOUT) {
			/* TODO: Handle timeout. If timeout just continue. */
			perror("server read error: ");
			exit(1);
		}

		printf("Message header length: %d\n", msg.header.len);
		printf("Gonna READ payload\n");
		ret = read(fd, msg.payload, msg.header.len);
		printf("Read payload\n");
		err = errno;
		if (ret > 0 && ret < msg.header.len) {
			printf("server: read %d bytes expected %d\n", ret, msg.header.len);
			exit(1);
		} else if (ret < 0 && err != -60) {
			perror("read error: ");
			exit(1);
		}

		printf("BEFORE CRC\n");
		ret = check_crc(&msg);
		if (ret) {
			printf("Bad CRC\n");
			exit(1);
		}

		printf("BEFORE SWITCH\n");
		switch (msg.header.type) {
		case PING_PONG:
			ping_pong(fd, msg.payload, msg.header.len);
			break;
		case REQ_BYTES:
			req_bytes(fd, msg.payload, msg.header.len);
			break;
		default:
			printf("oops, unknown message type (%d). something is wrong!\n", msg.header.type);
			exit(1);
		}
	}
}

int main(int argc, char *argv[])
{
	struct arguments arguments;
	struct stat dev_stat;
	int fd, ret;

	/* Default options */
	arguments.length = 1024;
	arguments.rqbytes = 0;
	arguments.mors = DEFAULT;
	arguments.type = PING_PONG;

	/* Add arguments to client mode to stop after i) N bytes or ii) M seconds. */
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	printf ("Device = %s\nMode = %s\n",
			arguments.device,
			arguments.mode ? "Client" : "Server");
	if (arguments.mode)
		printf ("Message length = %d\nMessage type = %s\n",
			arguments.length,
			arguments.type ? "REQ_BYTES" : "PING_PONG");
	if (arguments.type == REQ_BYTES)
		printf ("Bytes requested to server = %d\n",
			arguments.rqbytes);
	if (arguments.mors)
		printf ("Send %s%d %s\n",
			arguments.mors - 1 ? "for " : "",
			arguments.limit,
			arguments.mors - 1 ? "seconds" : "messages");
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
		printf ("%s is not a character device/n", arguments.device);
		exit(1);
	}

	if (arguments.length < 0 || arguments.length > 1024) {
		printf ("%d bytes: not a valid length (0 < length <= 1024)\n",
			arguments.length);
		exit(1);
	}

	if (arguments.mode == SERVER)
		run_server(fd);
	else
		run_client(fd, arguments.length, arguments.type,
			   arguments.rqbytes, arguments.mors, arguments.limit);
	return 0;
}
