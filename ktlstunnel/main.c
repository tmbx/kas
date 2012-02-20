/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */
#include "common.h"

struct kdaemon_opts global_opts;

static void kdaemon_init() {
    global_opts.quit_flag = 0;
    global_opts.log_level = KCD_LOG_CRIT;
    global_opts.log_file = stderr;
    global_opts.remote_host = NULL;
    global_opts.local_host = NULL;
    global_opts.second_host = NULL;
    global_opts.remote_port = 0;
    global_opts.second_port = 0;
    global_opts.local_port = 0;
    global_opts.local_first = 0;
    kstr_init(&global_opts.ssl_cert_path);
}

static void kdaemon_clean() {
    kstr_clean(&global_opts.ssl_cert_path);
}

/* This function prints the usage on the stream specified. */
static void kdaemon_print_usage(FILE *stream) {
    fprintf(stream, "Usage: ktlstunnel [-l <log level>] [-L <log file>] [-r host:port] [-h -v -f]\n"
    	    	    "                  <local host> <local port> <remote host> <remote port>\n"
		    "\n"
		    "-l <level>       Log level: 'minimal', 'debug', or bitmask.\n"
		    "                   Default to 'minimal'.\n"
		    "-L <file>        If specified, the daemon will log in this file.\n"
		    "-r <host:port>   If specified, the daemon will reconnect to this host and port\n"
		    "                   when the local connection is lost\n"
		    "-f               If specified, the daemon will connect to the local process\n"
		    "                   first.\n"
		    "-h               Show this help message and exit.\n"
		    "-v               Show the version number and exit.\n");
}

static void kdaemon_print_version(FILE *stream) {
    fprintf(stream, "Teambox TLS Tunnel Daemon (ktlstunnel) version %s.\n", BUILD_ID);
    fprintf(stream, "Copyright (C) 2005-2012 Opersys inc., All rights reserved.\n\n");
}

/* This function parses the command line arguments. It returns 0 if the program
 * should keep going, -1 if the program should exit with a failure code and -2
 * if the program should exit with a success code.
 */
static int kdaemon_handle_cmd_line(int argc, char **argv) {

    while (1) {
	int cmd = getopt(argc, argv, "l:L:r:hvf");

	if (cmd == '?' || cmd == ':') {
	    kdaemon_print_usage(stderr);
	    return -1;
	}
	
	else if (cmd == 'l') {
	    global_opts.log_level = 0;
	    
	    if (! strcmp(optarg, "minimal")) global_opts.log_level = KCD_LOG_CRIT;
	    else if (! strcmp(optarg, "debug")) global_opts.log_level = 0xffff;
	    else if (sscanf(optarg, "%x", &global_opts.log_level) != 1) {
		fprintf(stderr, "Invalid log level (%s).\n", optarg);
		return -1;
	    }
	}
	
	else if (cmd == 'L') {
	    global_opts.log_file = fopen(optarg, "wb");
	    
	    if (! global_opts.log_file) {
		fprintf(stderr, "Cannot open %s: %s.\n", optarg, strerror(errno));
		return -1;
	    }
	    
	    /* Make the logs unbuffered. */
    	    if (setvbuf(global_opts.log_file, NULL, _IONBF, 0)) {
	    	fprintf(stderr, "Failed to make the logs unbuffered.\n");
	    	return -1;
	    }
	}
	
	else if (cmd == 'r') {
	    char *colon = strstr(optarg, ":");
	    
	    if (! colon) {
		fprintf(stderr, "Invalid host:port string.\n");
		return -1;
	    }
	    
	    *colon = 0;
	    global_opts.second_host = optarg;
	    global_opts.second_port = atoi(colon + 1);
	}
	
	else if (cmd == 'h') {
	    kdaemon_print_usage(stdout);
	    return -2;
	}

	else if (cmd == 'v') {
	    kdaemon_print_version(stdout);
	    return -2;
	}
	
	else if (cmd == 'f') {
	    global_opts.local_first = 1;
	}

	/* Out of args. */
	else if (cmd == -1) {
	    break;
	}

	else {
	    assert(0);
	}
    }
    
    if (argc - optind != 4) {
    	kdaemon_print_usage(stderr);
	return -1;
    }
    
    global_opts.local_host = argv[optind];
    global_opts.local_port = atoi(argv[optind + 1]);
    global_opts.remote_host = argv[optind + 2];
    global_opts.remote_port = atoi(argv[optind + 3]);
    
    return 0;
}

/* This function should be called to log a message in the KMOD log.
 * Arguments:
 * Message logging level (1, 2, 3, 4) (higher number is lower priority).
 * Format is the usual printf() format, and the following args are the args that
 *   printf() takes.
 */
void kmod_log_msg(int level, const char *format, ...) {
    va_list arg;
    char date[256];
    kstr str, fmt;
    time_t now;

    if ((level & global_opts.log_level) != level) return;
    
    va_start(arg, format);
    
    kstr_init(&fmt);
    kstr_init(&str);

    time(&now);
    strftime(date, 256, "%Y/%m/%d %H:%M:%S", localtime(&now));
    
    kstr_sf(&fmt, "%s : %s", date, format);
    kstr_sfv(&str, fmt.data, arg);
    
    fprintf(global_opts.log_file, "%s", str.data);
    
    va_end(arg);
    kstr_clean(&str);
    kstr_clean(&fmt);
}

void kdaemon_prepare_select(struct kselect *sel) {
    kselect_zero(sel);
    sel->tv.tv_sec = 100;
}

int kdaemon_do_select(struct kselect *sel) {
    if (global_opts.quit_flag) {
	kmod_set_error("must quit");
	return -1;
    }
    
    kselect_wait(sel);
    
    return 0;
}
    
int main(int argc, char **argv) {
    int error = 0;
    struct ktun tun;
    
    ktools_initialize();
    kmod_base_init();
    gnutls_global_init();
    kdaemon_init();
    ktun_init(&tun);
    
    do {
    	error = kdaemon_handle_cmd_line(argc, argv);
	if (error) break;
	
	do {
	    kmod_log_msg(KCD_LOG_CRIT, "ktlstunnel starting.\n");
	    
	    error = ktun_run_tunnel(&tun, global_opts.remote_host, global_opts.remote_port,
	    	    	    	    global_opts.local_host, global_opts.local_port, global_opts.local_first);
	    if (error) break;

	} while (0);

	if (error == -1) {
	    kmod_log_msg(KCD_LOG_CRIT, "ktlstunnel error: %s.\n", kmod_strerror());
	}

	kmod_log_msg(KCD_LOG_CRIT, "ktlstunnel shutting down.\n");
	
	if (global_opts.log_file != stderr) {
	    fclose(global_opts.log_file);
	    global_opts.log_file = stderr;
	}

    } while (0);
    
    ktun_clean(&tun);
    kdaemon_clean();
    gnutls_global_deinit();
    kmod_base_clean();
    ktools_finalize();
    
    return (error == -1) ? 1 : 0;
}

