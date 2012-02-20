/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#ifndef _KCD_MISC_H
#define _KCD_MISC_H

/* This structure is used to send ANP queries to the Postgres backend. */
struct kcd_pg_anp_query {
    kbuffer input_buf;
    kbuffer output_buf;
};

/* Represent the state of a process. */
struct kcd_process {
    
    /* Do not read more than this number of bytes from the process, per file
     * descriptor. By default this value is set to 100MB.
     */
    uint32_t output_cap;
    
    /* True if the process has been killed with SIGKILL. */
    int killed_flag;
    
    /* True if the process has been collected. */
    int collected_flag;
    
    /* True if the process has timed out. */
    int timeout_flag;
    
    /* True if the process has failed. This is set when the process is
     * collected.
     */
    int failed_flag;
    
    /* PID of the process. This is 0 if the process has not been forked. */
    int pid;
    
    /* Current position in the input buffer (see 'in_str' below). */
    int in_pos;
    
    /* Stdin, stdout and stderr socket pairs, in order. The first descriptor in
     * the pair is for the parent, the second is for the child.
     */
    int desc[6];
    
    /* Logging level. Default to 0. */
    int log_level;
    
    /* Name of the process (cached for logging purposes). */
    kstr name;
    
    /* Data to input to the process. */
    kstr in_str;

    /* Data returned by the process. */
    kstr out_str;
    kstr err_str;
};

void kcd_pg_anp_query_init(struct kcd_pg_anp_query *self);
void kcd_pg_anp_query_clean(struct kcd_pg_anp_query *self);
int kcd_open_pg_conn(struct pg_db_conn *conn, char *conn_info);
int kcd_exec_pg_query(struct pg_db_conn *conn, char *query, PGresult **db_res, char *err_str);
int kcd_exec_pg_anp_query(struct pg_db_conn *conn, struct kcd_pg_anp_query *anp_query, char *query_name);
int kcd_exec_safe_pg_anp_query(struct pg_db_conn *conn, struct kcd_pg_anp_query *anp_query, char *query_name);
int kcd_exec_kws_bound_query(struct pg_db_conn *conn,
                             struct kcd_pg_anp_query *anp_query,
                             char *query_name,
                             struct anp_msg *cmd,
                             struct anp_msg *res,
                             uint64_t kws_id,
                             uint64_t date,
                             uint32_t login_type,
                             uint32_t user_id,
                             uint32_t cmd_minor,
                             kbuffer *extra_args);
int kcd_open_pg_serializable_transaction(struct pg_db_conn *conn);
int kcd_commit_pg_transaction(struct pg_db_conn *conn);
int kcd_do_anp_timed_xfer(struct anp_tls_xfer *xfer, struct ktls_conn *conn, int timeout);
int kcd_do_anp_xfer(struct anp_tls_xfer *xfer, struct ktls_conn *conn);
int kcd_ask_kmod_about_kws_ticket(char *ticket_data, int ticket_len, uint64_t key_id, int *valid);
void kcd_log_kanp_msg(uint32_t level, int is_cmd, struct anp_msg *msg);
int kcd_waitpid(int pid, int block_flag, int *failed_flag);
void kcd_process_init(struct kcd_process *self);
void kcd_process_clean(struct kcd_process *self);
void kcd_process_log_stream(struct kcd_process *self, char *what, kstr *stream);
void kcd_process_log_output(struct kcd_process *self, int stdout_flag, int stderr_flag);
void kcd_process_import_error(struct kcd_process *self);
int kcd_process_is_finished(struct kcd_process *self);
int kcd_process_is_socket_ready(struct kcd_process *self, struct kselect *sel);
void kcd_process_prepare_select(struct kcd_process *self, struct kselect *sel);
void kcd_process_do_xfer(struct kcd_process *self);
void kcd_process_kill(struct kcd_process *self);
void kcd_process_try_collect(struct kcd_process *self);
int kcd_process_kill_and_collect(struct kcd_process *self);
int kcd_process_xfer_and_collect(struct kcd_process *self, int64_t timeout);
int kcd_process_start(struct kcd_process *self, char **argv, void *gc_func, void *gc_arg);
int kcd_process_start_and_collect(struct kcd_process *self, char **argv, void *gc_func, void *gc_arg,
                                  int64_t timeout, int critical_flag);
int kcd_exec_kcdhelper(char *task, uint64_t kws_id, int log_level);

#endif

