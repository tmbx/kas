#include "common.h"

#ifdef __UNIX__
#include <execinfo.h>
typedef void (*sighandler_t)(int);
#endif

#define KDAEMON_LOCK_FILE_SIZE 47
#define KDAEMON_LOCK_FILE_STATUS_OFFSET 0
#define KDAEMON_LOCK_FILE_PID_OFFSET 8
#define KDAEMON_LOCK_FILE_RANDOM_OFFSET 14

/* This function makes the daemon part away forever from the terminal.
 * Beware, there is no turning back!
 */
void kdaemon_detach() {
    freopen("/dev/null", "rb", stdin);
    freopen("/dev/null", "wb", stdout);
    freopen("/dev/null", "wb", stderr);
}

/* This function closes all file descriptors but 0, 1 and 2. This is meant to be
 * executed in a completely uninitialized environment.
 */
void kdaemon_closefd_check(int *argc, char **argv) {
    int i, max_fd;
    
    if (*argc < 2 || strcmp(argv[1], "closefd")) return;
    
    /* Close our descriptors. Ignore the function return code. If the call
     * fails, we won't loop long.
     */
    max_fd = getdtablesize();
    for (i = 3; i < max_fd; i++) close(i);
}

/* Create a socket pair. Die on error. */
void kdaemon_open_socket_pair(int pair[2]) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) || ksock_set_unblocking(pair[0]) || ksock_set_unblocking(pair[1]))
        kerror_fatal("cannot create socket pair: %s", kerror_syserror());
}

/* Close a socket pair, if required. */
void kdaemon_close_socket_pair(int pair[2]) {
    int i;
    for (i = 0; i < 2; i++) {
        if (pair[i] != -1) {
            close(pair[i]);   
            pair[i] = -1;
        }
    }
}

/* Block all signals. */
void kdaemon_block_signals() {
    struct sigaction sa;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL)) kerror_fatal("unable to block signals");
}

/* Unblock all signals. */
void kdaemon_unblock_signals() {
    struct sigaction sa;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL)) kerror_fatal("unable to unblock signals");
}

/* This function handles SIGSEGV. */
static void kdaemon_segfault_handler(int sig_id) {
#ifdef __UNIX__
    void *array[20];
    int i, size = 20;
    char **strings = NULL;
    kstr out_string;
    static int currently_handling = 0;
    
    sig_id = 0;

    /* Handle recursive segfault. */
    if (currently_handling) _exit(1);
    currently_handling = 1;
    
    /* Get the stack trace. This thing is nearly useless to debug, but it's
     * better than nothing I guess.
     */
    kstr_init(&out_string);
    kstr_sf(&out_string, "Process %d segfaulted.\n\nStack trace:\n\n", getpid());
    
    size = backtrace(array, size);
    strings = backtrace_symbols(array, size);
    if (strings == NULL) return;

    for (i = 0; i < size; i++) {
        kstr_append_cstr(&out_string, strings[i]);
        kstr_append_char(&out_string, '\n');
    }
    
    /* Try to log... */
    kmod_log_msg(KCD_LOG_CRIT, "%s", out_string.data);
    
    free(strings);
    kstr_clean(&out_string);
    _exit(1);
#endif
}

/* Generic signal handler. */
static void kdaemon_signal_handler(int sig_id) {
    char a = 0;
    
    if (sig_id == SIGTERM) global_opts.quit_flag = 1;
    else if (sig_id == SIGCHLD) global_opts.sigchld_count++;
    else if (sig_id == SIGUSR1) global_opts.sigusr1_count++;
    
    if (sig_id == SIGTERM && global_opts.quit_sock[0] != -1) write(global_opts.quit_sock[0], &a, 1);
    if (global_opts.signal_sock[0] != -1) write(global_opts.signal_sock[0], &a, 1);
}

/* Register a single signal handler. */
static void kdaemon_register_one_signal(int sig_id, sighandler_t handler) {
    struct sigaction sa;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    if (sigaction(sig_id, &sa, NULL) < 0) kerror_fatal("unable to register signal handler"); 
}

/* This function registers a signal handler for SIGTERM and SIGCHLD. */
void kdaemon_register_signal() {
    
    /* Block all signals. This ensures atomicity here and normalizes our state.
     * If we are started in some weird environment (e.g. from Apache), our
     * signals may be blocked.
     */
    kdaemon_block_signals();
    
    kdaemon_register_one_signal(SIGPIPE, SIG_IGN);
    kdaemon_register_one_signal(SIGSEGV, kdaemon_segfault_handler);
    kdaemon_register_one_signal(SIGTERM, kdaemon_signal_handler);
    kdaemon_register_one_signal(SIGCHLD, kdaemon_signal_handler);
    kdaemon_register_one_signal(SIGUSR1, kdaemon_signal_handler);
    
    /* Unblock all signals. */
    kdaemon_unblock_signals();
}

/* This function clears the select sets and sets the timeout to 1000000 seconds. */
void kdaemon_prepare_select(struct kselect *sel) {
    kselect_zero(sel);
    sel->tv.tv_sec = 1000000;
}

/* This function adds the quit and the signal socket in the read set of select()
 * so that a call to select() doesn't block when a signal is received, and then
 * it waits for something to happen in select(). This function returns -1 if
 * it's time to quit, otherwise it returns 0. The KMOD error string is set when
 * it's time to quit. The signal socket, if any, is drained after the call to
 * select() has been made.
 */
int kdaemon_do_select(struct kselect *sel) {
    int quit_sock = global_opts.quit_sock[1];
    int signal_sock = global_opts.signal_sock[1];
    
    if (global_opts.quit_flag) {
	kmod_set_error("must quit");
	return -1;
    }
    
    kselect_add_read(sel, quit_sock);
    kselect_add_read(sel, signal_sock);
    kselect_wait(sel);
    
    if (kselect_in_read(sel, quit_sock)) {
	global_opts.quit_flag = 1;
	kmod_set_error("must quit");
	return -1;
    }
    
    if (kselect_in_read(sel, signal_sock)) {
        while (1) {
            char buf[1000];
            uint32_t len = 1000;
            int error = ksock_read(signal_sock, buf, &len);
            if (error == -1) kerror_fatal("cannot drain signal socket: %s", kerror_syserror());
            if (error == -2) break;
        }
    }
    
    return 0;
}

void kdaemon_lock_file_init(struct kdaemon_lock_file *self) {
    memset(self, 0, sizeof(struct kdaemon_lock_file));
    kstr_init(&self->path);
    kstr_init(&self->content);
}

void kdaemon_lock_file_clean(struct kdaemon_lock_file *self) {
    kfs_fclose(&self->file, 1);
    kstr_clean(&self->path);
    kstr_clean(&self->content);
}

/* Return true if the lock file exists. */
int kdaemon_lock_file_exist(struct kdaemon_lock_file *self) {
    return self->path.slen && kfs_regular(self->path.data);
}

/* Open the daemon lock file, creating it if requested. The function validates
 * that a valid path has been specified.
 */
int kdaemon_lock_file_open(struct kdaemon_lock_file *self, int create_flag) {
    int error = 0;
    FILE *tmp_file = NULL;
    kstr tmp_path;
    
    assert(self->file == NULL);
    kstr_init(&tmp_path);
    
    do {
        if (!self->path.slen) {
            kmod_set_error("no lock file path specified");
            error = -1;
            break;
        }
        
        /* Create the lock file. */
        if (create_flag && !kdaemon_lock_file_exist(self)) {
            char *content = "stopped\n00000\n00000000000000000000000000000000\n";
            assert(strlen(content) == KDAEMON_LOCK_FILE_SIZE);
        
            kstr_sf(&tmp_path, "%s.%d", self->path.data, getpid());
            error = kfs_fopen(&tmp_file, tmp_path.data, "wb");
            if (error) break;
            error = kfs_fwrite(tmp_file, content, strlen(content));
            if (error) break;
            error = kfs_fclose(&tmp_file, 0);
            if (error) break;
            
            error = kfs_rename(tmp_path.data, self->path.data);
            if (error) break;
        }
        
        /* Open the lock file. */
        error = kfs_fopen(&self->file, self->path.data, "r+b");
        if (error) break;
    
    } while (0);
    
    kfs_fclose(&tmp_file, 1);
    kstr_clean(&tmp_path);
    
    return error;
}

/* Parse and validate the daemon lock file. */
int kdaemon_lock_file_parse(struct kdaemon_lock_file *self) {
    char content_buf[KDAEMON_LOCK_FILE_SIZE];
    char pid_buf[6];
    
    assert(self->file);
    
    /* Read the content. */
    if (kfs_fseek(self->file, 0, SEEK_SET)) return -1;
    if (kfs_fread(self->file, content_buf, KDAEMON_LOCK_FILE_SIZE)) return -1;
    kstr_assign_buf(&self->content, content_buf, KDAEMON_LOCK_FILE_SIZE);
    if (strlen(self->content.data) != KDAEMON_LOCK_FILE_SIZE) {
        kmod_set_error("invalid daemon lock file size");
        return -1;
    }
    
    /* Get the status. */
    memcpy(self->status_string, content_buf + KDAEMON_LOCK_FILE_STATUS_OFFSET, 7);
    if (!strcmp(self->status_string, "running")) self->status_code = 'r';
    else if (!strcmp(self->status_string, "stopped")) self->status_code = 's';
    else if (!strcmp(self->status_string, "halfway")) self->status_code = 'h';
    else {
        kmod_set_error("invalid daemon lock file status");
        return -1;
    }
    
    /* Get the PID. */
    memcpy(pid_buf, content_buf + KDAEMON_LOCK_FILE_PID_OFFSET, 5);
    pid_buf[5] = 0;
    self->pid = atoi(pid_buf);
    
    /* Get the random string. */
    memcpy(self->random_string, content_buf + KDAEMON_LOCK_FILE_RANDOM_OFFSET, 32);
    
    return 0;
}

/* Create, open, lock and parse the lock file as requested. */
int kdaemon_lock_file_open_lock_parse(struct kdaemon_lock_file *self, int create_flag, int lock_flag) {
    if (kdaemon_lock_file_open(self, create_flag) ||
        (lock_flag && kdaemon_lock_file_write_lock_status(self)) ||
        kdaemon_lock_file_parse(self))
        return -1;
    return 0;
}

/* Write a field in the lock file. The function assumes 'data' is
 * null-terminated.
 */
static int kdaemon_lock_file_write_field(struct kdaemon_lock_file *self, char *data, int offset, int len) {
    assert(self->file);
    assert(strlen(data) == (uint32_t)len);
    if (kfs_fseek(self->file, offset, SEEK_SET) || kfs_fwrite(self->file, data, len) || kfs_flush(self->file))
        return -1;
    return 0;
}

/* Write the status in the lock file. */
int kdaemon_lock_file_write_status(struct kdaemon_lock_file *self, char *status) {
    assert(strlen(status) == 7);
    memcpy(self->status_string, status, 7);
    return kdaemon_lock_file_write_field(self, self->status_string, KDAEMON_LOCK_FILE_STATUS_OFFSET, 7);
}

/* Write the pid in the lock file. */
int kdaemon_lock_file_write_pid(struct kdaemon_lock_file *self) {
    char buf[6];
    sprintf(buf, "%05d", self->pid);
    return kdaemon_lock_file_write_field(self, buf, KDAEMON_LOCK_FILE_PID_OFFSET, 5);
}

/* Write the random string in the lock file. */
int kdaemon_lock_file_write_random(struct kdaemon_lock_file *self) {
    return kdaemon_lock_file_write_field(self, self->random_string, KDAEMON_LOCK_FILE_RANDOM_OFFSET, 32);
}

/* Obtain a read or write lock on the offset specified. The call is blocking if
 * 'block_flag' is set. No lock is acquired if 'test_flag' is set. If a lock
 * cannot be obtained and test_flag is specified, the PID of the process that
 * holds the lock is returned, otherwise 0 is returned. In other cases, the
 * function returns -1 if the lock could not be obtained, 0 otherwise.
 */
static int kdaemon_lock_file_get_lock(struct kdaemon_lock_file *self, int offset, int read_flag, int block_flag,
                                      int test_flag) {
    assert(self->file);
    assert(!block_flag || !test_flag);
    
    /* Loop for handling EINTR. Repopulate the structure at every iteration so
     * that the kernel cannot screw us.
     */
    while (1) {
        int error;
        int cmd = test_flag ? F_GETLK : block_flag ? F_SETLKW : F_SETLK;
        struct flock arg;
        
        memset(&arg, 0, sizeof(struct flock));
        arg.l_type = read_flag ? F_RDLCK : F_WRLCK;
        arg.l_whence = SEEK_SET;
        arg.l_start = offset;
        arg.l_len = 1;
        arg.l_pid = 0;
        
        error = fcntl(fileno(self->file), cmd, &arg);
        if (error == -1 && errno == EINTR) continue;
        
        /* Welcome to the realm of unspecified behavior. Since the return value
         * of fnctl() is not specified in this case, we only use the value of
         * 'pid' to determine whether the lock could be obtained.
         */
        if (test_flag)
            return arg.l_pid;
        
        /* In other cases, we trust the return value. */
        if (error) {
            kmod_set_error("cannot obtain lock: %s", kmod_syserror());
            return -1;
        }
        
        return 0;
    }
}

/* Obtain a blocking write lock on the status offset. */
int kdaemon_lock_file_write_lock_status(struct kdaemon_lock_file *self) {
    return kdaemon_lock_file_get_lock(self, KDAEMON_LOCK_FILE_STATUS_OFFSET, 0, 1, 0);
}

/* Obtain a non-blocking write lock on the PID offset. */
int kdaemon_lock_file_write_lock_pid(struct kdaemon_lock_file *self) {
    return kdaemon_lock_file_get_lock(self, KDAEMON_LOCK_FILE_PID_OFFSET, 0, 0, 0);
}

/* Obtain the PID of a child having a lock on the PID area, or 0 if none. */
int kdaemon_lock_file_get_pid_child(struct kdaemon_lock_file *self) {
    return kdaemon_lock_file_get_lock(self, KDAEMON_LOCK_FILE_PID_OFFSET, 0, 0, 1);
}

/* Obtain a non-blocking read lock on the random offset. */
int kdaemon_lock_file_read_lock_random(struct kdaemon_lock_file *self) {
    return kdaemon_lock_file_get_lock(self, KDAEMON_LOCK_FILE_RANDOM_OFFSET, 1, 0, 0);
}

/* Obtain the PID of a child having a lock on the random area, or 0 if none. */
int kdaemon_lock_file_get_random_child(struct kdaemon_lock_file *self) {
    return kdaemon_lock_file_get_lock(self, KDAEMON_LOCK_FILE_RANDOM_OFFSET, 0, 0, 1);
}

/* Register a child process of the daemon. */
int kdaemon_lock_file_register_child(struct kdaemon_lock_file *self) {
    char cached_random[32];
    
    /* Keep track of the cached random string. */
    memcpy(cached_random, self->random_string, 32);
    
    /* Register in the random string area. */
    if (kdaemon_lock_file_read_lock_random(self)) return -1;
    
    /* Re-parse the lock file. */
    if (kdaemon_lock_file_parse(self)) return -1;
    
    /* The status is not running or the random string doesn't match our cached
     * string.
     */
    if (self->status_code != 'r' || memcmp(cached_random, self->random_string, 32)) {
        kmod_set_error("daemon child process registration failed");
        return -1;
    }
    
    return 0;
}

