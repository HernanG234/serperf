#include <argp.h>
#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* ioctl flags */
#define SERIAL_IOC_MAGIC        'h'
#define SERIAL_GET_PARAMS       _IOR(SERIAL_IOC_MAGIC, 1, struct serial_params)
#define SERIAL_SET_PARAMS       _IOW(SERIAL_IOC_MAGIC, 2, struct serial_params)
#define SERIAL_RX_BUFFER_CLEAR  _IO(SERIAL_IOC_MAGIC, 3)
#define SERIAL_READ_IOC         _IOWR(SERIAL_IOC_MAGIC, 4, struct serial_rw_msg)
#define SERIAL_WRITE_IOC        _IOWR(SERIAL_IOC_MAGIC, 5, struct serial_rw_msg)

/* params flags */
#define BIT(nr)         (1UL << (nr))
#define SERIAL_PARAMS_BAUDRATE          BIT(0)
#define SERIAL_PARAMS_DATABITS          BIT(1)
#define SERIAL_PARAMS_RCV_TIMEOUT       BIT(2)
#define SERIAL_PARAMS_XMIT_TIMEOUT      BIT(3)
#define SERIAL_PARAMS_PARITY            BIT(4)
#define SERIAL_PARAMS_STOPBITS          BIT(5)
#define SERIAL_PARAMS_FIFO_TRIGGER      BIT(6)

#define SERIAL_WAIT_FOR_XMIT            BIT(0)

#define MAX_PAYLOAD_LEN 131072

struct timeval tval_before, tval_after, tval_result;
bool verb = false;
int rmsgs = 0;
int wmsgs = 0;
bool serial_ioctl = false;
bool w4xmit = false;

struct serial_rw_msg {
	__u16 flags;
	__u32 count;
	char *buf;
};

enum msg_types {
	PING_PONG,
	REQ_BYTES,
	MSG_TYPES,
	LOOPBACK
};

const char *argp_program_version = "serperf 3.0.0";
const char *argp_program_bug_address = "<ezequiel@vanguardiasur.com.ar>";
static char doc[] = "Test /dev/serial performance.";
static char args_doc[] = "-s|c (-l bytes) (-r bytes) (-t seconds) DEVICE";

static struct argp_option options[] = {
	{ "server", 's', 0, 0, "Server mode"},
	{ "client", 'c', 0, 0, "Client mode"},
	{ "loopback", 'p', 0, 0, "Loopback mode"},
	{ "msg-length", 'l', "length", 0, "Set message length"},
	{ "msg-type", 'x', "type", 0, "PING_PONG = 0 | REQ_BYTES = 1"},
	{ "req-bytes", 'r', "bytes", 0, "Request n bytes from server"},
	{ "messages", 'm', "msgs", 0, "Client: Number of msgs to send"},
	{ "time", 't', "seconds", 0, "Client: Seconds to transmit data"},
	{ "verbose", 'v', 0, 0, "Verbose mode"},
	{ "wait-4-xmit", 'w', 0, 0, "Wait for transmit to finish (only ioctl)"},
	{ "ioctl", 'i', 0, 0, "Use IOCtl interface instead of read/write()" },
	{ 0 }
};

struct arguments {
	enum { SERVER, CLIENT, LOOP } mode;
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
	unsigned char payload[MAX_PAYLOAD_LEN]; /* alloc dynamically */
};

struct loopback {
	struct arguments arguments;
	int fd;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;
	switch (key) {
	case 's': arguments->mode = SERVER; break;
	case 'c': arguments->mode = CLIENT; break;
	case 'v': verb = true; break;
	case 'p': arguments->mode = LOOP; break;
	case 'l': arguments->length = strtol(arg, NULL, 10); break;
	case 'x': arguments->type = strtol(arg, NULL, 10); break;
	case 'r': arguments->rqbytes = strtol(arg, NULL, 10); break;
	case 'm': arguments->mors = MSGS;
		  arguments->limit = strtol(arg, NULL, 10); break;
	case 't': arguments->mors = SECONDS;
		  arguments->limit = strtol(arg, NULL, 10); break;
	case 'i': serial_ioctl = true; break;
	case 'w': w4xmit = true; break;
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

void exit_print()
{
	printf("\nTotal msgs READ: \t%d msgs\n", rmsgs);
	printf("Total msgs WRITTEN: \t%d msgs\n", wmsgs);
	gettimeofday(&tval_after, NULL);
	timersub(&tval_after, &tval_before, &tval_result);
	printf("Time elapsed: %ld.%06ld\n\n",
	       (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
}

void int_handler(int signo)
{
	if (signo == SIGINT) {
		exit_print();
		exit(1);
	}
}

ssize_t serial_read(int fd, void *buf, size_t count)
{
	struct serial_rw_msg rw_msg;
	int ret;

	if (!serial_ioctl)
		ret = read(fd, buf, count);
	else {
		rw_msg.count = count;
		rw_msg.buf = buf;
		rw_msg.flags = w4xmit ? SERIAL_WAIT_FOR_XMIT : 0;
		ret = ioctl(fd, SERIAL_READ_IOC, &rw_msg);
	}

	return ret;
}

ssize_t serial_write(int fd, void *buf, size_t count)
{
	struct serial_rw_msg rw_msg;
	int ret;

	if (!serial_ioctl)
		ret = write(fd, buf, count);
	else {
		rw_msg.count = count;
		rw_msg.buf = buf;
		rw_msg.flags = w4xmit ? SERIAL_WAIT_FOR_XMIT : 0;
		ret = ioctl(fd, SERIAL_WRITE_IOC, &rw_msg);
	}
	return ret;
}

int verbose(const char *format, ...)
{
	va_list args;
	int ret;

	if (!verb)
		return 0;
	va_start(args, format);
	ret = vprintf(format, args);
	va_end(args);

	return ret;
}

static void receive_reply(int fd, struct msg *msg)
{
	int ret, err;

	verbose("Client: %s\n", __func__);
	verbose("Client: Reading header\n");
	ret = serial_read(fd, &msg->header, sizeof(struct msg_header));
	err = errno;
	if (ret > 0 && ret < (int)sizeof(struct msg_header)) {
		printf("server: read %d bytes expected %zu\n", ret, sizeof(struct msg_header));
		exit_print();
		exit(1);
	} else if (ret < 0  && err != ETIMEDOUT) {
		perror("server read error: ");
		exit_print();
		exit(1);
	}

	verbose("Client: Reading payload\n");
	ret = serial_read(fd, msg->payload, msg->header.len);
	if (ret > 0 && ret < (int)msg->header.len) {
		printf("Client: read %d bytes expected %d\n", ret, msg->header.len);
		exit_print();
		exit(1);
	} else if (ret < 0) {
		perror("Client: read error: ");
		exit_print();
		exit(1);
	}
}

static void send_msg(int fd, int len, int type, int rqbytes)
{
	struct msg msg;
	int ret;

	verbose("Client: %s\n", __func__);
	msg.header.type = type;
	if (type == REQ_BYTES){
		memcpy(msg.payload, &rqbytes, 4);
		msg.header.len = 4;
	} else {
		msg.header.len = len;
		memset(msg.payload, 0x55, len);
		msg.payload[msg.header.len - 1] = 0xFF;
	}
	msg.header.crc = crc8(0, msg.payload, msg.header.len);

	verbose("Client: Writing\n");
	ret = serial_write(fd, &msg.header, (sizeof(struct msg_header) + msg.header.len));
	if (ret > 0 && ret < (int)(sizeof(struct msg_header) + msg.header.len)) {
		printf("Client: write %d bytes expected %zu\n", ret, (sizeof(struct msg_header) + msg.header.len));
		exit_print();
		exit(1);
	} else if (ret < 0) {
		perror("Client: write error: ");
		exit_print();
		exit(1);
	}
}

static void do_check(const unsigned char *payload, char *cmp, int msglen, int type,
		     int rqbytes)
{
	verbose("Client: %s\n", __func__);
	switch (type) {
	case REQ_BYTES:
		if (msglen != rqbytes) {
			printf("client: server reply does not have match with"
			       " the bytes requested (requested = %d) (got = %d)\n",
			       rqbytes, msglen);
			exit_print();
			exit(1);
		}
		break;
	case PING_PONG:
		if (memcmp(payload, cmp, msglen)) {
			printf("client: server reply is %s not the same\n", payload);
			exit_print();
			exit(1);
		}
	}

}

static void run_client_msgs(int fd, int len, int type, int rqbytes,
			    char *cmp, int limit, bool loopback)
{
	struct msg msg;
	int count = 0;

	verbose("Client: %s\n", __func__);
	while (count < limit) {
		send_msg(fd, len, type, rqbytes);
		wmsgs++;
		count++;
		if (!loopback) {
			receive_reply(fd, &msg);
			rmsgs++;
			do_check(msg.payload, cmp, msg.header.len, type, rqbytes);
		}
	}
	printf("Finished sending %d messages\n", limit);
}

static void run_client_seconds(int fd, int len, int type, int rqbytes,
			       char *cmp, int limit, bool loopback)
{
	struct msg msg;
	struct timeval tval_before, tval_after, tval_result;
	gettimeofday(&tval_before, NULL);
	gettimeofday(&tval_after, NULL);
	timersub(&tval_after, &tval_before, &tval_result);

	verbose("Client: %s\n", __func__);
	while ((long int)tval_result.tv_sec < limit) {
		send_msg(fd, len, type, rqbytes);
		wmsgs++;
		if (!loopback) {
			receive_reply(fd, &msg);
			rmsgs++;
			do_check(msg.payload, cmp, msg.header.len, type, rqbytes);
		}
		gettimeofday(&tval_after, NULL);
		timersub(&tval_after, &tval_before, &tval_result);
	}
	printf("Finished sending data for %d seconds\n", limit);
}

static void run_client(int fd, int len, int type, int rqbytes,
		       int mors, int limit)
{
	struct msg msg;
	char cmp[len];
	int ret;

	verbose("Erasing buffers\n");
	ret = ioctl(fd, SERIAL_RX_BUFFER_CLEAR);
	if (ret < 0) {
		perror("ioctl error: ");
		exit(1);
	}

	verbose("Starting Client...\n");
	memset(cmp, 0x55, len);
	cmp[len - 1] = 0xFF;
	switch (mors) {
	case MSGS:
		run_client_msgs(fd, len, type, rqbytes, cmp, limit, false);
		break;
	case SECONDS:
		run_client_seconds(fd, len, type, rqbytes, cmp, limit, false);
		break;
	default:
		while (1) {
			send_msg(fd, len, type, rqbytes);
			wmsgs++;
			receive_reply(fd, &msg);
			rmsgs++;
			do_check(msg.payload, cmp, msg.header.len, type, rqbytes);
		}
	}
}

static void ping_pong(int fd, const unsigned char *payload, int len)
{
	struct msg msg;
	int ret;

	verbose("Server: %s\n", __func__);
	memcpy(msg.payload, payload, len);
	msg.header.type = PING_PONG;
	msg.header.len = len;
	msg.header.crc = crc8(0, msg.payload, msg.header.len);

	verbose("Server: Writing\n");
	ret = serial_write(fd, &msg.header, (sizeof(struct msg_header) + msg.header.len));
	if (ret > 0 && ret < (int)(sizeof(struct msg_header) + msg.header.len)) {
		printf("server: write %d bytes expected %zu\n", ret, (sizeof(struct msg_header) + msg.header.len));
		exit_print();
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit_print();
		exit(1);
	}
	wmsgs++;
}

static void req_bytes(int fd, const unsigned char *payload, int len)
{
	struct msg msg;
	int ret;

	verbose("Server: %s\n", __func__);
	memcpy(&msg.header.len, payload, len);
	memset(msg.payload, 0x55, msg.header.len);
	msg.payload[msg.header.len - 1] = 0xFF;
	msg.header.type = REQ_BYTES;
	msg.header.crc = crc8(0, msg.payload, msg.header.len);

	verbose("Server: Writing\n");
	ret = serial_write(fd, &msg.header, (sizeof(struct msg_header) + msg.header.len));
	if (ret > 0 && ret < (int)(sizeof(struct msg_header) + msg.header.len)) {
		printf("server: write %d bytes expected %zu\n", ret, (sizeof(struct msg_header) + msg.header.len));
		exit_print();
		exit(1);
	} else if (ret < 0) {
		perror("server write error: ");
		exit_print();
		exit(1);
	}
	wmsgs++;
}

static int read_header(int fd, struct msg *msg, bool *timeout)
{
	int ret;

	*timeout = false;
	verbose("Server: Reading header\n");
	ret = serial_read(fd, &msg->header, sizeof(struct msg_header));
	if (ret > 0 && ret < (int)sizeof(struct msg_header)) {
		printf("server: read %d bytes expected %zu\n",
		       ret, sizeof(struct msg_header));
		exit_print();
		exit(1);
	} else if (ret < 0 && errno != ETIMEDOUT) {
		perror("server read error: ");
		exit_print();
		exit(1);
	} else if (ret < 0 && errno == ETIMEDOUT) {
		*timeout = true;
	}

	return ret;
}

static int read_payload(int fd, struct msg *msg, bool *timeout)
{
	int ret;

	*timeout = false;
	verbose("Server: Reading payload\n");
	ret = serial_read(fd, &msg->payload, msg->header.len);
	if (ret > 0 && ret < msg->header.len) {
		printf("server: read %d bytes expected %d\n",
		       ret, msg->header.len);
		exit_print();
		exit(1);
	} else if (ret < 0 && errno != ETIMEDOUT) {
		perror("read error: ");
		exit_print();
		exit(1);
	} else if (ret < 0 && errno == ETIMEDOUT) {
		*timeout = true;
	}

	return ret;
}

static int read_msg(int fd, struct msg *msg)
{
	bool timeout = false;
	int ret;

	while (1) {
		ret = read_header(fd, msg, &timeout);
		verbose("Server: Read %d bytes as header\n", ret);
		if (timeout)
			continue;
		if (msg->header.len > MAX_PAYLOAD_LEN) {
			printf("Header.len too large: %x\n", msg->header.len);
			exit(1);
		}
		ret = read_payload(fd, msg, &timeout);
		verbose("Server: Read %d bytes as payload\n", ret);
		if (timeout)
			continue;
		break;
	}

	return ret;
}

static void run_server(int fd)
{
	struct msg msg;
	int ret;

	verbose("Starting Server...\n");
	while (1) {
		ret = read_msg(fd, &msg);
		ret = check_crc(&msg);
		if (ret) {
			printf("Bad CRC\n");
			exit_print();
			exit(1);
		}

		rmsgs++;
		switch (msg.header.type) {
		case PING_PONG:
			ping_pong(fd, msg.payload, msg.header.len);
			break;
		case REQ_BYTES:
			req_bytes(fd, msg.payload, msg.header.len);
			break;
		default:
			printf("oops, unknown message type (%d). something is wrong!\n", msg.header.type);
			exit_print();
			exit(1);
		}
	}
}

void *serial_reader (void *arg) {
	struct timeval tval_before, tval_after, tval_result;
	struct loopback *lb = (struct loopback *)arg;
	struct msg msg;
	int ret, i;

	verbose("Loopback: %s\n", __func__);
	switch (lb->arguments.mors) {
	case MSGS:
		verbose("Loopback: Reader: Msgs mode\n");
		for (i = 0; i < lb->arguments.limit; i++) {
			ret = read_msg(lb->fd, &msg);
			ret = check_crc(&msg);
			if (ret) {
				printf("Bad CRC\n");
				exit_print();
				exit(1);
			}
			rmsgs++;
		}
		break;
	case SECONDS:
		verbose("Loopback: Reader: Seconds mode\n");
		gettimeofday(&tval_before, NULL);
		gettimeofday(&tval_after, NULL);
		timersub(&tval_after, &tval_before, &tval_result);

		while ((long int)tval_result.tv_sec < lb->arguments.limit) {
			ret = read_msg(lb->fd, &msg);
			ret = check_crc(&msg);
			if (ret) {
				printf("Bad CRC\n");
				exit_print();
				exit(1);
			}
			rmsgs++;
			gettimeofday(&tval_after, NULL);
			timersub(&tval_after, &tval_before, &tval_result);
		}
		break;
	default:
		verbose("Loopback: Reader: Infinite mode\n");
		while (1) {
			ret = read_msg(lb->fd, &msg);
			ret = check_crc(&msg);
			if (ret) {
				printf("Bad CRC\n");
				exit_print();
				exit(1);
			}
			rmsgs++;
		}
	}
	return NULL;
}

void *serial_writer (void *arg) {
	struct loopback *lb = (struct loopback *)arg;

	verbose("Loopback: %s\n", __func__);
	switch (lb->arguments.mors) {
	case MSGS:
		verbose("Loopback: Writer: Msgs mode\n");
		run_client_msgs(lb->fd, lb->arguments.length,
				lb->arguments.type, lb->arguments.rqbytes, NULL,
				lb->arguments.limit, true);
		break;
	case SECONDS:
		verbose("Loopback: Writer: Seconds mode\n");
		run_client_seconds(lb->fd, lb->arguments.length,
				   lb->arguments.type, lb->arguments.rqbytes,
				   NULL, lb->arguments.limit, true);
		break;
	default:
		verbose("Loopback: Writer: Infinite mode\n");
		while (1) {
			send_msg(lb->fd, lb->arguments.length,
				 lb->arguments.type, lb->arguments.rqbytes);
			wmsgs++;
		}
	}

	return NULL;
}


static void run_loopback(struct loopback lb)
{
	void *result;
	int ret;

	verbose("Erasing buffers\n");
	ret = ioctl(lb.fd, SERIAL_RX_BUFFER_CLEAR);
	if (ret < 0) {
		perror("ioctl error: ");
		exit(1);
	}


	verbose("Loopback: %s: Launching threads\n", __func__);
	pthread_t s_reader, s_writer;
	pthread_create (&s_reader, NULL, serial_reader, &lb);
	pthread_create (&s_writer, NULL, serial_writer, &lb);
	pthread_join (s_writer, &result);
	pthread_cancel(s_reader);
}

void printstatus(struct arguments arguments)
{
	printf("Device = %s\n", arguments.device);
	printf("Mode = ");
	switch (arguments.mode) {
	case SERVER:
		printf("Server\n"); break;
	case CLIENT:
		printf("Client\n"); break;
	case LOOP:
		printf("Loopback\n"); break;
	default:
		printf("Unknown mode - exit\n");
		exit(1);
	}

	if (arguments.mode)
		printf("Message length = %d\n",
			arguments.type ? 4 : arguments.length);
	if (arguments.mode != LOOP)
		printf("Message type = %s\n",
		       arguments.type ? "REQ_BYTES" : "PING_PONG");
	if (arguments.type == REQ_BYTES && arguments.mode != LOOP)
		printf("Bytes requested to server = %d\n",
			arguments.rqbytes);
	if (arguments.mors)
		printf("Send %s%d %s\n",
			arguments.mors - 1 ? "for " : "",
			arguments.limit,
			arguments.mors - 1 ? "seconds" : "messages");
}

int main(int argc, char *argv[])
{
	struct arguments arguments;
	struct stat dev_stat;
	struct loopback lb;
	int fd, ret;

	gettimeofday(&tval_before, NULL);

	verbose("Initializing...\n");

	signal(SIGINT, int_handler);

	/* Default options */
	arguments.length = 1024;
	arguments.rqbytes = 0;
	arguments.mors = DEFAULT;
	arguments.type = PING_PONG;

	/* Add arguments to client mode to stop after i) N bytes or ii) M seconds. */
	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	printstatus(arguments);

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

	if (arguments.length < 0 || arguments.length > 131072) {
		printf ("%d bytes: not a valid length (0 < length <= 131072)\n",
			arguments.length);
		exit(1);
	}

	lb.arguments = arguments;
	lb.fd = fd;

	switch (arguments.mode) {
	case SERVER:
		run_server(fd); break;
	case CLIENT:
		run_client(fd, arguments.length, arguments.type,
			   arguments.rqbytes, arguments.mors, arguments.limit);
		break;
	case LOOP:
		run_loopback(lb);
		break;
	default:
		printf("oops, unknown mode (%d). something is wrong!\n",
		       arguments.mode);
		exit(1);

	}

	close(fd);
	exit_print();
	return 0;
}
