#include <argp.h>
#include <stdbool.h>

const char *argp_program_version = "serperf 0.1.0";
const char *argp_program_bug_address = "<ezequiel@vanguardiasur.com.ar>";
static char doc[] = "Test /dev/serial performance.";
static char args_doc[] = "[FILENAME]...";
static struct argp_option options[] = {
    { "server", 's', 0, 0, "Server mode"},
    { "client", 'c', 0, 0, "Client mode"},
    { "device", 'd', 0, 0, "Device path"},
    { 0 }
};

struct arguments {
    enum { SERVER, CLIENT } mode;
    char device[64];
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;
    switch (key) {
    case 's': arguments->mode = SERVER; break;
    case 'c': arguments->mode = CLIENT; break;
    case 'd': arguments->isCaseInsensitive = true; break;
    case ARGP_KEY_ARG: return 0;
    default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

int main(int argc, char *argv[])
{
    struct arguments arguments;
    struct stat dev_stat;
    int fd, ret;

    /* Add arguments to client mode to stop after i) N bytes or ii) M seconds. */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    fd = open(arguments.device, O_RDWR);
    if (fd < 0) {
        /* TODO: Use perror() or error() */
    }

    ret = fstat(fd, &dev_stat);
    /* TODO: Check all return paths. */

    /* TODO check in dev_stat that this is a char device and fail otherwise. */

    /* Fill an array of callbacks, run_client and run_server,
     * to run each mode. */

    /* server mode: listens to commands and executes them.
     * for now, only a single command is supported ping/pong with CRC check
     * and payload mirror.
     */

    /* client mode: sends commands until stop. Stop is N bytes or M bytes.
     */
}
