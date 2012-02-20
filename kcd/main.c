/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#include "common.h"
#include "iniparser.h"
#include "syslog.h"

#define KCD_MAX_LABEL_SIZE          50
#define KCD_DEBUG_FILE_PATH         "/etc/teambox/kcd_debug"

/* KCD log name to log flags table. */
struct kcd_log_name_entry {
    char *name;
    uint32_t flags;
};

static struct kcd_log_name_entry kcd_log_name_table[] = {
    { "crit", KCD_LOG_CRIT },
    { "brief", KCD_LOG_CRIT | KCD_LOG_BRIEF },
    { "cmd", KCD_LOG_BRIEF | KCD_LOG_CMD },
    { "kws", KCD_LOG_BRIEF | KCD_LOG_KWS },
    { "kfs", KCD_LOG_BRIEF | KCD_LOG_KFS },
    { "vnc", KCD_LOG_BRIEF | KCD_LOG_VNC },
    { "pg", KCD_LOG_BRIEF | KCD_LOG_PG },
    { "mail", KCD_LOG_BRIEF | KCD_LOG_MAIL },
    { "notif", KCD_LOG_BRIEF | KCD_LOG_NOTIF },
    { "kmod", KCD_LOG_BRIEF | KCD_LOG_KMOD },
    { "misc", KCD_LOG_BRIEF | KCD_LOG_MISC },
    { "none", 0 },
    { "debug", ~0 },
};

/* Global options. */
struct kdaemon_opts global_opts;

/* Startup logging level. */
static int startup_log_level = KCD_LOG_CRIT | KCD_LOG_BRIEF;

/* True if the system log can be used. */
static int syslog_flag = 0;

/* True if KCD should detach from the terminal. */
static int detach_flag = 1;

/* Labels shown in ps, syslog and the terminal. */
static char *ps_label = NULL;
static char syslog_label[KCD_MAX_LABEL_SIZE] = "kcd";
static char tty_label[KCD_MAX_LABEL_SIZE] = "kcd";

/* Lock file of KCD. */
static struct kdaemon_lock_file lock_file;

static void kdaemon_reset_org_array() {
    int i;
    
    for (i = 0; i < global_opts.org_key_id_array.size; i++) {
	uint64_t *key_id = global_opts.org_key_id_array.data[i];
	kstr *name = global_opts.org_name_array.data[i];
	kfree(key_id);
	kstr_destroy(name);
    }
    
    karray_reset(&global_opts.org_key_id_array);
    karray_reset(&global_opts.org_name_array);
}

static void kdaemon_init() {
    kdaemon_lock_file_init(&lock_file);
    kdaemon_open_socket_pair(global_opts.quit_sock);
    kdaemon_open_socket_pair(global_opts.signal_sock);
    kstr_init(&global_opts.startup_action);
    kstr_init(&global_opts.startup_mode);
    karray_init(&global_opts.org_key_id_array);
    karray_init(&global_opts.org_name_array);
    kstr_init(&global_opts.listen_addr);
    kstr_init(&global_opts.config_path);
    kstr_assign_cstr(&global_opts.config_path, "/etc/teambox/kcd/kcd.ini");
    kstr_init(&global_opts.kfs_ini_path);
    kstr_assign_cstr(&global_opts.kfs_ini_path, "/etc/teambox/kcd/kfs.ini");
    kstr_init(&global_opts.ssl_cert_path);
    kstr_init(&global_opts.ssl_key_path);
    kstr_init(&global_opts.kmod_binary_path);
    kstr_init(&global_opts.kmod_db_path);
    kstr_init(&global_opts.vnc_cred_path);
    kstr_init(&global_opts.kcd_host);
    kstr_init(&global_opts.web_host);
    kstr_init(&global_opts.web_link);
    kstr_init(&global_opts.invite_mail_kcd_html);
    kstr_init(&global_opts.kfs_dir_path);
    kstr_init(&global_opts.sendmail_path);
    kstr_init(&global_opts.mail_sender);
}

static void kdaemon_clean() {
    kstr_clean(&global_opts.startup_action);
    kstr_clean(&global_opts.startup_mode);
    kdaemon_reset_org_array();
    karray_clean(&global_opts.org_key_id_array);
    karray_clean(&global_opts.org_name_array);
    kstr_clean(&global_opts.listen_addr);
    kstr_clean(&global_opts.config_path);
    kstr_clean(&global_opts.kfs_ini_path);
    kstr_clean(&global_opts.ssl_cert_path);
    kstr_clean(&global_opts.ssl_key_path);
    kstr_clean(&global_opts.kmod_binary_path);
    kstr_clean(&global_opts.kmod_db_path);
    kstr_clean(&global_opts.vnc_cred_path);
    kstr_clean(&global_opts.kcd_host);
    kstr_clean(&global_opts.web_host);
    kstr_clean(&global_opts.web_link);
    kstr_clean(&global_opts.invite_mail_kcd_html);
    kstr_clean(&global_opts.kfs_dir_path);
    kstr_clean(&global_opts.sendmail_path);
    kstr_clean(&global_opts.mail_sender);
    
    /* This is technically incorrect (we may be signaled here), but it's no big
     * deal.
     */
    kdaemon_close_socket_pair(global_opts.quit_sock);
    kdaemon_close_socket_pair(global_opts.signal_sock);
    kdaemon_lock_file_clean(&lock_file);
}

/* Obtain the KCD log flags corresponding to the name specified, if any. Return
 * true if the name was found.
 */
static int kcd_log_name_to_flags(char *name, uint32_t *flags) {
    uint32_t i;
    
    for (i = 0; i < sizeof(kcd_log_name_table) / sizeof(struct kcd_log_name_entry); i++) {
        struct kcd_log_name_entry *entry = kcd_log_name_table + i;
        if (!strcmp(name, entry->name)) {
            *flags = entry->flags;
            return 1;
        }
    }
    
    return 0;
}

/* Return the flags written in the debug file. */
static uint32_t get_kcd_debug_flags() {
    int i;
    kbuffer buf;
    kstr word;
    uint32_t word_flags, ret_flags = 0, have_flags = 0;
    
    kbuffer_init(&buf);
    kstr_init(&word);
    
    do {
        if (kfs_read_file(KCD_DEBUG_FILE_PATH, &buf)) break;
        kbuffer_write8(&buf, 0);
    
        for (i = 0; buf.data[i]; i++) {
            char c = buf.data[i];
            
            if (isspace(c)) {
                if (word.slen) {
                    have_flags = 1;
                    
                    if (kcd_log_name_to_flags(word.data, &word_flags)) {
                        ret_flags |= word_flags;
                    }
                
                    else {
                        kmod_log_msg(KCD_LOG_CRIT, "Invalid log level (%s).\n", word.data);
                    }
                    
                    kstr_reset(&word);
                }
            }

            else if (c == '\n') break;
            else kstr_append_char(&word, c);
        }
        
        if (!have_flags) ret_flags = ~0;
        
    } while (0);
    
    kbuffer_clean(&buf);
    kstr_clean(&word);
    
    return ret_flags;
}

/* Update the log level according to the content of the debug file. */
static void kcd_update_log_level() {
    if (kfs_regular(KCD_DEBUG_FILE_PATH)) global_opts.log_level = get_kcd_debug_flags();
    else global_opts.log_level = startup_log_level;
}

/* This function prints the usage on the stream specified. */
static void kdaemon_print_usage(FILE *stream) {
    fprintf(stream, "Usage: kcd [closefd] <action> [options]\n"
                    "\n"
                    "Actions:\n"
                    "  start <frontend|notif>\n"
                    "  stop\n"
                    "  query\n"
                    "  sigterm\n"
                    "  sigusr1\n"
		    "\n"
                    "Options:\n"
		    "-l <level>       Set a log level. Default to 'brief'.\n"
		    "-c <path>        Path to the main configuration file. Default to\n"
		    "                   /etc/teambox/kcd/kcd.ini.\n"
                    "-f <path>        Path to the KFS configuration file. Default to\n"
                    "                   /etc/teambox/kcd/kfs.ini.\n"
		    "-P <lock path>   Set the path to the lock file. If not specified, no\n"
		    "                   lock file is created.\n"
		    "-h               Show this help message and exit.\n"
		    "-v               Show the version number and exit.\n"
		    "-t               Stay attached to the terminal.\n"
                    "\n"
                    "Log levels:\n"
                    "  crit            Critical events.\n"
                    "  brief           Normal events.\n"
                    "  cmd             Low-level command events.\n"
                    "  kws             Low-level workspace events.\n"
                    "  kfs             Low-level KFS events.\n"
                    "  vnc             Low-level VNC events.\n"
                    "  pg              Low-level Postgres events.\n"
                    "  mail            Low-level mail events.\n"
                    "  notif           Low-level notification events.\n"
                    "  kmod            Low-level KMOD events.\n"
                    "  misc            Low-level miscellaneous events.\n"
                    "  debug           All events.\n"
                    "  none            No event.\n"
                    );
}

static void kdaemon_print_version(FILE *stream) {
    fprintf(stream, "Teambox Collaboration Daemon (KCD) version %s.\n", BUILD_ID);
    fprintf(stream, "Copyright (C) 2005-2012 Opersys inc., All rights reserved.\n\n");
}

/* This function parses the command line arguments. It returns 0 if the program
 * should keep going, -1 if the program should exit with a failure code and -2
 * if the program should exit with a success code.
 */
static int kdaemon_handle_cmd_line(int argc, char **argv, kstr *action, kstr *mode) {
    char *action_list[] = { "start", "stop", "query", "sigterm", "sigusr1" };
    int nb_arg_list[] = {   1,       0,      0,       0,          0 };
    char *mode_list[] = { "frontend", "notif" };
    int i, j, log_arg_flag = 0, found_flag, nb_arg_left;
    
    while (1) {
	int cmd = getopt(argc, argv, "l:c:f:P:hvt");

	if (cmd == '?' || cmd == ':') {
	    kdaemon_print_usage(stderr);
	    return -1;
	}
	
	else if (cmd == 'l') {
            uint32_t flags;
            
            if (!log_arg_flag) {
                log_arg_flag = 1;
                startup_log_level = 0;
            }
            
	    if (!kcd_log_name_to_flags(optarg, &flags)) {
		fprintf(stderr, "Invalid log level (%s).\n", optarg);
		return -1;
	    }
            
            startup_log_level |= flags;
	}

	else if (cmd == 'c') {
	    kstr_assign_cstr(&global_opts.config_path, optarg);
	}
        
	else if (cmd == 'f') {
	    kstr_assign_cstr(&global_opts.kfs_ini_path, optarg);
	}

	else if (cmd == 'h') {
	    kdaemon_print_usage(stdout);
	    return -2;
	}

	else if (cmd == 'P') {
	    kstr_assign_cstr(&lock_file.path, optarg);
	}
	
	else if (cmd == 'v') {
	    kdaemon_print_version(stdout);
	    return -2;
	}
	
	else if (cmd == 't') {
	    detach_flag = 0;
	}

	/* Out of args. */
	else if (cmd == -1) {
	    break;
	}

	else {
	    assert(0);
	}
        
    }
    
    nb_arg_left = argc - optind;
    
    /* Remove the closefd argument. */
    if (nb_arg_left && !strcmp(argv[optind], "closefd")) {
        optind++;
        nb_arg_left--;
    }
    
    /* Get the action and its eventual argument. */
    if (!nb_arg_left) {
        kdaemon_print_usage(stderr);
        return -1;
    }
    
    kstr_assign_cstr(action, argv[optind++]);
    nb_arg_left--;
    found_flag = 0;
    
    for (i = 0; (unsigned)i < sizeof(action_list) / sizeof(char *); i++) {
        if (kstr_equal_cstr(action, action_list[i])) {
            if (nb_arg_left != nb_arg_list[i]) {
                kdaemon_print_usage(stderr);
                return -1;
            }
            found_flag = 1;
            break;
        }
    }
    
    if (!found_flag) {
        kdaemon_print_usage(stderr);
        return -1;
    }
    
    if (kstr_equal_cstr(action, "start")) {
        kstr_assign_cstr(mode, argv[optind++]);
        nb_arg_left--;
        found_flag = 0;
        
        for (i = 0; (unsigned)i < sizeof(mode_list) / sizeof(char *); i++) {
            if (kstr_equal_cstr(mode, mode_list[i])) {
                found_flag = 1;
                break;
            }
        }
        
        if (!found_flag) {
            kdaemon_print_usage(stderr);
            return -1;
        }
    }
    
    /* And now, thanks to Linux and its enlightened kernel developers, we are
     * about to prepare the way to change the process name. For years there was
     * no API to do this properly, but kernel 2.6.9 has brought us the useful
     * PR_SET_NAME API, which allows to set the process name to an arbitrary
     * string having as much as FIFTEEN characters! Great job, Andi!!
     * 
     * My tests indicate that the arguments are all layed out after each other
     * in memory, separated by a zero. If you write past argv[0], you actually
     * clobber the other arguments. ps(1) ignores 0 as a string terminator and
     * converts zeroes to spaces. How ps(1) detects the end of the argument list
     * is unknown.
     * 
     * To work around this crap, we preemptively nullify every argument and we
     * set the process name in argv[0]. We assume correctly or incorrectly that
     * the buffer allocated by Linux to hold the arguments is at least 50 bytes
     * long. One post I've seen indicated that the buffer was about 1500
     * characters, since a memory page is used for it. It seems to work fine for
     * now so let's live dangerously.
     * 
     * Obviously all these manipulations trash the argument array.
     */
    for (i = 0; i < argc; i++)
        for (j = 0; argv[i][j]; j++)
            argv[i][j] = 0;
    ps_label = argv[0];
    strcpy(ps_label, "kcd [Startup]");
    
    return 0;
}

static void kdaemon_get_ini_str(dictionary *d, char *key, kstr *val) {
    kstr_assign_cstr(val, iniparser_getstring(d, key, ""));
}

static void kdaemon_get_ini_int(dictionary *d, char *key, uint32_t default_val, uint32_t *val) {
    *val = iniparser_getint(d, key, default_val);
}

static int kdaemon_parse_kcd_ini() {
    int i;
    int error = 0;
    dictionary *d = NULL;
    
    kdaemon_reset_org_array();
    
    do {
	d = iniparser_load(global_opts.config_path.data);
	if (d == NULL) {
	    kmod_set_error("error parsing %s", global_opts.config_path.data);
	    error = -1;
	    break;
	}
	
        kdaemon_get_ini_int(d, "config:kanp_mode", 10, &global_opts.kanp_mode);
        kdaemon_get_ini_int(d, "config:knp_mode", 10, &global_opts.knp_mode);
        kdaemon_get_ini_int(d, "config:http_mode", 10, &global_opts.http_mode);
        kdaemon_get_ini_int(d, "config:vnc_mode", 10, &global_opts.vnc_mode);
	kdaemon_get_ini_str(d, "config:listen_addr", &global_opts.listen_addr);
	kdaemon_get_ini_int(d, "config:listen_port", 443, &global_opts.listen_port);
	kdaemon_get_ini_int(d, "config:knp_port", 5000, &global_opts.knp_port);
	kdaemon_get_ini_str(d, "config:ssl_cert_path", &global_opts.ssl_cert_path);
	kdaemon_get_ini_str(d, "config:ssl_key_path", &global_opts.ssl_key_path);
	kdaemon_get_ini_str(d, "config:kmod_binary_path", &global_opts.kmod_binary_path);
	kdaemon_get_ini_str(d, "config:kmod_db_path", &global_opts.kmod_db_path);
	kdaemon_get_ini_str(d, "config:vnc_cred_path", &global_opts.vnc_cred_path);
	kdaemon_get_ini_int(d, "config:web_port", 80, &global_opts.web_port);
	kdaemon_get_ini_str(d, "config:kcd_host", &global_opts.kcd_host);
	kdaemon_get_ini_str(d, "config:web_host", &global_opts.web_host);
	kdaemon_get_ini_str(d, "config:web_link", &global_opts.web_link);
	kdaemon_get_ini_str(d, "config:invite_mail_kcd_html", &global_opts.invite_mail_kcd_html);
        kdaemon_get_ini_str(d, "config:sendmail_path", &global_opts.sendmail_path);
        kdaemon_get_ini_int(d, "config:sendmail_timeout", 10, &global_opts.sendmail_timeout);
        kdaemon_get_ini_str(d, "config:mail_sender", &global_opts.mail_sender);
        
	/* Switch '\n' for real newlines. */
        kstr_replace(&global_opts.web_link, "\\n", "\n");

	for (i = 0; i < d->size; i++) {
	    if (d->key[i] != NULL && d->val[i] != NULL && strstr(d->key[i], "organizations:")) {
		uint64_t *key_id = kmalloc(sizeof(uint64_t));
		kstr *name = kstr_new();
		
		*key_id = atoll(strstr(d->key[i], ":") + 1);
		kstr_assign_cstr(name, d->val[i]);
		
		karray_push(&global_opts.org_key_id_array, key_id);
		karray_push(&global_opts.org_name_array, name);
	    }
	}
	
    } while (0);
    
    iniparser_freedict(d);
    
    return error;
}

static int kcd_parse_kfs_ini() {
    int error = 0;
    int64_t i64;
    dictionary *d = NULL;
    char *str;
    
    do {
	d = iniparser_load(global_opts.kfs_ini_path.data);
	if (d == NULL) {
	    kmod_set_error("Error parsing %s", global_opts.kfs_ini_path.data);
	    error = -1;
	    break;
	}
	
	str = iniparser_getstring(d, "config:kfs_mode", NULL);
	if (str == NULL) {
	    kmod_set_error("the specified value for config:kfs_mode is invalid");
	    error = -1;
	    break;
	}
        
        if (! strcmp(str, "pg")) {
            kmod_set_error("the postgres option is no longer supported");
            error = -1;
            break;
        }
        
        else if (! strcmp(str, "local") || ! strcmp(str, "samba")) global_opts.use_kfs_dir = 1;
        
        else {
	    kmod_set_error("the specified value for config:kfs_mode is invalid");
            error = -1;
            break;
        }
	
	str = iniparser_getstring(d, "config:kfs_dir", NULL);
	if (str == NULL) {
	    kmod_set_error("the specified value for config:kfs_dir is invalid");
	    error = -1;
	    break;
	}
	
	kstr_assign_cstr(&global_opts.kfs_dir_path, str);
        kpath_add_delim(&global_opts.kfs_dir_path, KPATH_FORMAT_UNIX);
        
	i64 = iniparser_getint(d, "config:default_kfs_quota", -1);
	if (i64 < 0) {
	    kmod_set_error("the specified value for config:default_kfs_quota is invalid");
	    error = -1;
	    break;
	}
        
        global_opts.default_kfs_quota = i64*1024*1024;
	
    } while (0);
    
    iniparser_freedict(d);
    
    return error;
}

/* This function (re)loads the configuration of the daemon. */
int kdaemon_load_config(int silent_flag) {
    int error = 0;
    
    if (!silent_flag) kmod_log_msg(KCD_LOG_BRIEF, "kdaemon_load_config() called.\n");
    
    do {
        error = kdaemon_parse_kcd_ini();
        if (error) break;
        
        error = kcd_parse_kfs_ini();
        if (error) break;
        
        kcd_update_log_level();
        
    } while (0);
    
    return error;
}

/* This function should be called to log a message in the KMOD log.
 * Arguments:
 * Message logging level.
 * Format is the usual printf() format, and the following args are the args that
 *   printf() takes.
 */
void kmod_log_msg(int level, const char *format, ...) {
    va_list arg;
    kstr str;

    if ((level & global_opts.log_level) == 0) return;
    
    va_start(arg, format);
    
    kstr_init(&str);
    kstr_sfv(&str, format, arg);
    
    if (syslog_flag) {
	syslog(LOG_INFO, "%s", str.data);
    }
    
    else {
	if (!strcmp(format, "")) printf("\n\n");
	else {
            if (str.slen == 0 || str.data[str.slen - 1] != '\n') kstr_append_char(&str, '\n');
            printf("%s %s", tty_label, str.data);
        }
    }
    
    va_end(arg);
    kstr_clean(&str);
}

/* This function sets the daemon task for display on ps, syslog, and tty. Do not
 * call this function until the daemon has been properly initialized.
 */
void kdaemon_set_task(const char *format, ...) {
    int pid = getpid();
    char *c;
    kstr task;
    va_list args;
    
    va_start(args, format);
    kstr_init_sfv(&task, format, args);
    
    for (c = ps_label; *c; c++) *c = 0;
    snprintf(ps_label, KCD_MAX_LABEL_SIZE, "kcd [%s]", task.data);
    snprintf(syslog_label, KCD_MAX_LABEL_SIZE, "kcd[%d] [%s]", pid, task.data);
    snprintf(tty_label, KCD_MAX_LABEL_SIZE, "kcd[%d] [%s]", pid, task.data);
    
    /* Set the label for syslog. */ 
    if (detach_flag) {
        syslog_flag = 1;
        openlog(syslog_label, 0, LOG_LOCAL0);
    }
    
    kstr_clean(&task);
    va_end(args);
}

/* Fork a new instance of the KCD. 'pid' is filled up with the PID returned by
 * fork(). For the child, the function calls setsid(), sets the process task to
 * the task requested, clones the signal socket if requested and performs the
 * lock file registration if required. Signals are blocked during this function
 * call.
 */
int kcd_fork(char *task, int *pid, int sig_flag) {
    int error = 0;
    
    kdaemon_block_signals();
    
    do {
	*pid = fork();
	
	if (*pid == -1) {
	    kmod_set_error("fork() failed");
            error = -1;
	    break;
	}
        
        /* Child. */
        if (!*pid) {
	    if (setsid() == -1) {
                kmod_set_error("setsid() failed");
                error = -1;
                break;
            }
         
            kdaemon_set_task("%s", task);
            
            kdaemon_close_socket_pair(global_opts.signal_sock);
            if (sig_flag) kdaemon_open_socket_pair(global_opts.signal_sock);
            
            if (lock_file.path.slen) {
                error = kdaemon_lock_file_register_child(&lock_file);
                if (error) break;
            }
        }
	
    } while (0);
    
    kdaemon_unblock_signals();
    
    return error;
}

/* Helper for handle_start_action(), handle_query_action() and handle_signal_action(). */
static int kcd_internal_query() {

    /* It's marked stopped. */
    if (lock_file.status_code == 's')
        return 's';
    
    /* Explicitly marked halfway or marked running but the PID isn't set or no
     * process is holding a lock on the PID area.
     */
    if (lock_file.status_code == 'h' || lock_file.pid == 0 || !kdaemon_lock_file_get_pid_child(&lock_file))
        return 'h';
    
    /* It's running. */
    return 'r';
}

/* Helper for handle_start_action() and handle_stop_action(). */
static int kcd_internal_stop_daemon() {
    
    /* The daemon is already stopped. */
    if (lock_file.status_code == 's') return 0;
    
    /* Set the status to 'halfway' while we get rid of the KCD processes. */
    if (kdaemon_lock_file_write_status(&lock_file, "halfway")) return -1;
    
    /* Loop killing processes. */
    while (1) {
        int pid = kdaemon_lock_file_get_random_child(&lock_file);
        if (!pid) break;
        if (kill(-pid, 9) && errno != ESRCH) {
            kmod_set_error("cannot kill pid %d", pid);
            return -1;
        }
        
        /* Your process has been killed. Please reboot for the change to take
         * effect.
         * 
         * Sleep to prevent repeated kill of dead processes still griping the
         * lock.
         */
        usleep(1000);
    }
        
    /* Set the status to 'stopped'. */
    if (kdaemon_lock_file_write_status(&lock_file, "stopped")) return -1;
    
    return 0;
}

static int handle_start_action() {
    int error = 0;
    int start_flag = 1;
    int lock_flag = lock_file.path.slen;
    int fork_flag = lock_flag || detach_flag;
    int fork_pid;
    
    do {
        /* Sanity check. */
        if (!PQisthreadsafe()) {
            kmod_set_error("postgres is not compiled with multi-threading support.");
            error = -1;
            break;
        }
        
        /* Load the configuration before all else. We want to bail out on errors
         * early.
         */
        error = kdaemon_load_config(1);
	if (error) break;
    
        /* We are using a lock file. */
        if (lock_flag) {
    
            /* Open or create the lock file, lock the status to prevent other
             * init scripts from interfering and parse it.
             */
            error = kdaemon_lock_file_open_lock_parse(&lock_file, 1, 1);
            if (error) break;
            
            /* The daemon is already running. Don't start a new instance. */
            if (kcd_internal_query() == 'r')
                start_flag = 0;
            
            /* The daemon is half running or stopped. */
            else {
                
                /* Stop everything if required. */
                error = kcd_internal_stop_daemon();
                if (error) break;
                
                /* Generate new random data and clear the pid. */
                error = kutil_generate_alpha_random(lock_file.random_string, 32);
                if (error) break;
                lock_file.pid = 0;
                
                /* Order matters here. An init script query ignores our status
                 * lock.
                 */
                if (kdaemon_lock_file_write_random(&lock_file) ||
                    kdaemon_lock_file_write_pid(&lock_file) ||
                    kdaemon_lock_file_write_status(&lock_file, "running")) {
                    error = -1;
                    break;
                }
            }
        }
        
        if (start_flag) {
            
            /* Register our signals. */
	    kdaemon_register_signal();
	    
            /* Detach from the terminal if requested. */
            if (detach_flag) kdaemon_detach();
            
            /* Fork if requested. */
            if (fork_flag) {
                
                error = kcd_fork("Initializing", &fork_pid, 1);
                if (error) break;
                
                /* The parent exits. */
                if (fork_pid) break;
                
                /* We're the child and we have a lock file. */
                if (lock_flag) {
                    
                    /* Obtain a write lock on the PID area, for init script queries. */
                    error = kdaemon_lock_file_write_lock_pid(&lock_file);
                    if (error) break;
                    
                    /* Write the daemon PID in the PID area. */
                    lock_file.pid = getpid();
                    error = kdaemon_lock_file_write_pid(&lock_file);
                    if (error) break;
                }
            }
            
            /* Otherwise just set the process task. */
	    else kdaemon_set_task("Initializing");
    
            /* Dispatch to the appropriate handler. */
            kmod_log_msg(KCD_LOG_CRIT, "KCD starting.\n");
            
            if (kstr_equal_cstr(&global_opts.startup_mode, "frontend")) {
                error = kcd_frontend_listener_loop();
                if (error == -1) {
                    kmod_log_msg(KCD_LOG_CRIT, "Listener mode error: %s.\n", kmod_strerror());
                    error = 0;
                }
            }
            
            else if (kstr_equal_cstr(&global_opts.startup_mode, "notif")) {
                error = kcd_notif_entry();
                if (error == -1) {
                    kmod_log_msg(KCD_LOG_CRIT, "Notification mode error: %s.\n", kmod_strerror());
                    error = 0;
                }
            }
            
            kmod_log_msg(KCD_LOG_CRIT, "KCD shutting down.\n");
        }

    } while (0);
   
    return error;
}

static int handle_stop_action() {
    int error = 0;
    
    do {
        /* No lock file, bail out. */
        if (!kdaemon_lock_file_exist(&lock_file)) break;
        
        /* Open the lock file, lock the status to prevent other init scripts
         * from interfering and parse it.
         */
        error = kdaemon_lock_file_open_lock_parse(&lock_file, 0, 1);
        if (error) break;
        
        /* Stop everything if required. */
        error = kcd_internal_stop_daemon();
        if (error) break;
        
    } while (0);
   
    return error;
}

static void handle_query_action() {
    int error = 0;
    int code = 's';
        
    do {
        /* No lock file, bail out. */
        if (!kdaemon_lock_file_exist(&lock_file)) break;
        
        /* Open the lock file and parse it. */
        error = kdaemon_lock_file_open_lock_parse(&lock_file, 0, 0);
        if (error) break;
        
        /* Determine the status code. */
        code = kcd_internal_query();
        
    } while (0);
    
    if (error) kmod_log_msg(KCD_LOG_MISC, "%s", kmod_strerror());
    
    printf("%s\n", code == 's' ? "stopped" : code == 'h' ? "halfway" : "running");
}

static int handle_signal_action() {
    int error = 0;
    
    do {
        /* No lock file specified. */
        if (!lock_file.path.slen) {
            kmod_set_error("no lock file specified");
            error = -1;
            break;
        }
        
        /* No lock file, bail out. */
        if (!kdaemon_lock_file_exist(&lock_file)) break;
        
        /* Open the lock file and parse it. */
        error = kdaemon_lock_file_open_lock_parse(&lock_file, 0, 0);
        if (error) break;
        
        /* The daemon is running. */
        if (kcd_internal_query() == 'r') {
            int sig = kstr_equal_cstr(&global_opts.startup_action, "sigterm") ? SIGTERM : SIGUSR1;
            
            /* Signal the process group. */
            error = kill(-lock_file.pid, sig);
            if (error) break;
            
            printf("Sent %s to process group %d.\n", global_opts.startup_action.data, lock_file.pid);
        }
    
    } while (0);
    
    return error;
}
        
int main(int argc, char **argv) {
    int error = 0;
    
    kdaemon_closefd_check(&argc, argv);
    
    ktools_initialize();
    kmod_base_init();
    kdaemon_init();
    gnutls_global_init();
    kthread_enter_mt_mode();
    
    do {
        kstr *action = &global_opts.startup_action, *mode = &global_opts.startup_mode;
        
    	error = kdaemon_handle_cmd_line(argc, argv, action, mode);
        if (error) break;
        
        kcd_update_log_level();
	
        if (kstr_equal_cstr(action, "start")) error = handle_start_action(mode);
        else if (kstr_equal_cstr(action, "stop")) error = handle_stop_action();
        else if (kstr_equal_cstr(action, "query")) handle_query_action();
        else if (kstr_equal_cstr(action, "sigterm") || kstr_equal_cstr(action, "sigusr1"))
            error = handle_signal_action();
        if (error) fprintf(stderr, "kcd error: %s\n", kmod_strerror());
    
    } while (0);
    
    kthread_exit_mt_mode();
    gnutls_global_deinit();
    kdaemon_clean();
    kmod_base_clean();
    ktools_finalize();
    
    return (error == -1) ? 1 : 0;
}
 
