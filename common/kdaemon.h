#ifndef _KDAEMON_H_
#define _KDAEMON_H_

/* This structure represents the data of the daemon lock file. */
struct kdaemon_lock_file {
    
    /* File pointer. */
    FILE *file;
    
    /* Path to the lock file. */
    kstr path;
    
    /* Content of the lock file. */
    kstr content;
    
    /* Status string. */
    char status_string[8];
    
    /* Status code representing the status of the lock file: 'r' running, 's'
     * stopped, 'h' halfway.
     */
    char status_code;
    
    /* Random string. */
    char random_string[32];
    
    /* PID. */
    int pid;
};

void kdaemon_detach();
void kdaemon_closefd_check(int *argc, char **argv);
void kdaemon_open_socket_pair(int pair[2]);
void kdaemon_close_socket_pair(int pair[2]);
void kdaemon_block_signals();
void kdaemon_unblock_signals();
void kdaemon_register_signal();
void kdaemon_prepare_select(struct kselect *sel);
int kdaemon_do_select(struct kselect *sel);
void kdaemon_lock_file_init(struct kdaemon_lock_file *self);
void kdaemon_lock_file_clean(struct kdaemon_lock_file *self);
int kdaemon_lock_file_exist(struct kdaemon_lock_file *self);
int kdaemon_lock_file_open(struct kdaemon_lock_file *self, int create_flag);
int kdaemon_lock_file_parse(struct kdaemon_lock_file *self);
int kdaemon_lock_file_open_lock_parse(struct kdaemon_lock_file *self, int create_flag, int lock_flag);
int kdaemon_lock_file_write_status(struct kdaemon_lock_file *self, char *status);
int kdaemon_lock_file_write_pid(struct kdaemon_lock_file *self);
int kdaemon_lock_file_write_random(struct kdaemon_lock_file *self);
int kdaemon_lock_file_write_lock_status(struct kdaemon_lock_file *self);
int kdaemon_lock_file_write_lock_pid(struct kdaemon_lock_file *self);
int kdaemon_lock_file_get_pid_child(struct kdaemon_lock_file *self);
int kdaemon_lock_file_read_lock_random(struct kdaemon_lock_file *self);
int kdaemon_lock_file_get_random_child(struct kdaemon_lock_file *self);
int kdaemon_lock_file_register_child(struct kdaemon_lock_file *self);

#endif

