/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#include "common.h"

void kcd_pg_anp_query_init(struct kcd_pg_anp_query *self) {
    kbuffer_init(&self->input_buf);
    kbuffer_init(&self->output_buf);
}

void kcd_pg_anp_query_clean(struct kcd_pg_anp_query *self) {
    kbuffer_clean(&self->input_buf);
    kbuffer_clean(&self->output_buf);
}

/* This function loops until the connection to the database is opened. */
int kcd_open_pg_conn(struct pg_db_conn *conn, char *conn_info) {
    int error = 0;
    
    kmod_log_msg(KCD_LOG_PG, "kcd_open_pg_conn() called.\n");
   
    do {
	error = pg_db_connect_start(conn, conn_info);
	if (error) break;
	
	while (1) {
	    struct kselect sel;
    	    int r = pg_db_connect_check(conn);
	    
	    if (! r) break;
	    
	    if (r == -1) {
	    	error = -1;
		break;
	    }
	    
	    kdaemon_prepare_select(&sel);
	    if (r == -2) kselect_add_read(&sel, conn->sock);
	    if (r == -3) kselect_add_write(&sel, conn->sock);
	    
	    error = kdaemon_do_select(&sel);
	    if (error) break;
	}
	
	if (error) break;
	
    } while (0);
    
    return error;
}

/* This function executes a query on the server. The function assumes that only
 * one result will be returned by the server. This result is set in *res, if it
 * is wanted. The result must be freed by the caller with PQclear(). The result
 * is verified by calling pg_db_verify_result(). 'err_str' is used to format the
 * error message if the verification of the result fails.
 */
int kcd_exec_pg_query(struct pg_db_conn *conn, char *query, PGresult **db_res, char *err_str) {
    int error = 0;
    struct kselect sel;
    PGresult *res = NULL;
    
    kmod_log_msg(KCD_LOG_PG, "kcd_kws_cmd_pg_query: executing |%s|.\n", query);
    if (db_res) *db_res = NULL;

    do {
    	error = pg_db_query_start(conn, query, 1);
	if (error) break;
	
	while (1) {
	    
	    /* Allow postgres to consume its input, if any. */
	    error = pg_db_consume(conn);
	    if (error) break;

	    /* Finish sending the query to the server. */
	    if (conn->query_state == 1) {
		int r = pg_db_query_check(conn);

		if (r == -1) {
	    	    error = -1;
		    break;
		}
	    }

	    /* Get the next result, if any. */
	    if (conn->query_state == 2) {
		res = pg_db_result_check(conn);
		if (res) break;
	    }

            /* Wait for the postgres connection to become ready. */
	    kdaemon_prepare_select(&sel);
	    kselect_add_read(&sel, conn->sock);
	    if (conn->query_state == 1) kselect_add_write(&sel, conn->sock);
	    
	    error = kdaemon_do_select(&sel);
	    if (error) break;
	}
	
	if (error) break;
	
	error = pg_db_verify_result(res, err_str);
	if (error) break;
	
    } while (0);
    
    if (db_res) *db_res = res;
    else pg_db_destroy_res(&res);
    
    return error;
}

/* Execute an ANP query against the KCD Postgres backend. If the query succeeds,
 * the output buffer has the read position set to the position of the output
 * parameters. The return value is 0 if the query succeeded, -1 if an internal
 * error occurred and -2 if a generic or user error has occurred. The input
 * buffer is cleared in all cases.
 */
int kcd_exec_pg_anp_query(struct pg_db_conn *conn, struct kcd_pg_anp_query *anp_query, char *query_name) {
    int error = 0;
    uint32_t query_code;
    kstr tmp;
    PGresult *pg_res = NULL;
    
    kstr_init(&tmp);

    do {
        /* Execute the query. */
        kstr_sf(&tmp, "SELECT %s(", query_name);
        pg_db_add_bytea(conn, &tmp, &anp_query->input_buf);
        kstr_append_cstr(&tmp, ")");
        error = kcd_exec_pg_query(conn, tmp.data, &pg_res, query_name);
        if (error) break;

        /* Get the query code. */
        pg_db_get_bytea(pg_res, 0, 0, &anp_query->output_buf);
        error = anp_read_uint32(&anp_query->output_buf, &query_code);
        if (error) break;

        /* User error. */
        if (query_code) {
            error = anp_read_kstr(&anp_query->output_buf, &tmp);
            if (error) break;

            kmod_set_error("%s", tmp.data);
            error = -2;
            break;
        }

    } while (0);

    kbuffer_reset(&anp_query->input_buf);
    kstr_clean(&tmp);
    pg_db_destroy_res(&pg_res);

    return error; 
}

/* Same as kcd_exec_pg_anp_query, except that the error code -2 is converted to
 * the error code -1.
 */
int kcd_exec_safe_pg_anp_query(struct pg_db_conn *conn, struct kcd_pg_anp_query *anp_query, char *query_name) {
    return kcd_exec_pg_anp_query(conn, anp_query, query_name) ? -1 : 0;
}

/* Execute a workspace-bound ANP query against the KCD Postgres backend. The ANP
 * result obtained, if any, is set in 'res'. The return value is 0 if the query
 * succeeded, -1 if an internal error has occurred, -2 if a generic error has
 * occurred and -3 if an error result has been set. 'cmd' can be NULL.
 * 'extra_args' is reset once the query has been executed.
 */
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
                             kbuffer *extra_args) {
    int error = 0;
    uint32_t query_code;
    kbuffer *in_buf = &anp_query->input_buf, *out_buf = &anp_query->output_buf;
    
    /* Prepare the query. */
    kbuffer_reset(in_buf);
    anp_write_uint64(in_buf, kws_id);
    anp_write_uint64(in_buf, date);
    anp_write_uint32(in_buf, login_type);
    anp_write_uint32(in_buf, user_id);
    anp_write_uint32(in_buf, cmd_minor);
    anp_write_bin(in_buf, cmd ? &cmd->payload : NULL);
    kbuffer_write_buffer(in_buf, extra_args);
    kbuffer_reset(extra_args);

    /* Execute the query. */
    error = kcd_exec_pg_anp_query(conn, anp_query, query_name);
    if (error) return error;
    
    /* Get the result data, the result type and the second query code. */
    if (anp_read_uint32(out_buf, &res->type) ||
        anp_read_bin(out_buf, &res->payload) ||
        anp_read_uint32(out_buf, &query_code)) {
        return -1;
    }
    
    return query_code ? -3 : 0;
}

/* Open a transaction in serializable isolation level. */
int kcd_open_pg_serializable_transaction(struct pg_db_conn *conn) {
    return kcd_exec_pg_query(conn, "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE", NULL, "open transaction");
}

/* Commit a transaction. */
int kcd_commit_pg_transaction(struct pg_db_conn *conn) {
    return kcd_exec_pg_query(conn, "COMMIT", NULL, "commit transaction");
}

/* This function loops until the specified ANP transfer has finished. The
 * parameter 'timeout' determines the time to wait, in milliseconds, for the
 * transfer to finish. If 'timeout' is zero, the time to wait is infinite. The
 * funtion returns 0 on success, -1 if the transfer failed or -2 if the transfer
 * timed out.
 */
int kcd_do_anp_timed_xfer(struct anp_tls_xfer *xfer, struct ktls_conn *conn, int timeout) {
    struct timeval start;
    
    kmod_log_msg(KCD_LOG_MISC, "kcd_do_anp_timed_xfer() called.\n");
    
    /* Get the start time. */
    if (timeout) ktime_now(&start);
    
    while (1) {
    	struct kselect sel;
	int done = 1;
	
    	if (anp_tls_do_xfer(xfer, conn)) return -1;
	
	kdaemon_prepare_select(&sel);
	
	if (anp_tls_receiving(xfer) && ! anp_tls_done_receiving(xfer)) {
	    done = 0; kselect_add_read(&sel, conn->sock); 
	}
	
	if (anp_tls_sending(xfer) && ! anp_tls_done_sending(xfer)) { done = 0; kselect_add_write(&sel, conn->sock); }
	if (done) return 0;
        
        /* Compute the wait delay and bail out if it is negative. */
        if (timeout) {
            struct timeval elapsed;
            int delay;
            ktime_elapsed(&elapsed, &start);
            delay = timeout - ktime_to_msec(&elapsed);
            if (delay < 0) return -2;
            ktime_from_msec(&sel.tv, delay + 1);
        }
	
	if (kdaemon_do_select(&sel)) return -1;
    }
}

/* Same as above, with 'timeout' set to 0. */
int kcd_do_anp_xfer(struct anp_tls_xfer *xfer, struct ktls_conn *conn) {
    return kcd_do_anp_timed_xfer(xfer, conn, 0);
}

static int kcd_start_kmod(int *kmod_sock, int *kmod_pid) {
    int error = 0;
    int sock_pair[2] = { -1, -1 };
    
    kmod_log_msg(KCD_LOG_KMOD, "kcd_start_kmod() called.\n");

    /* Try. */
    do {
	/* Create a socket pair. */
	error = socketpair(AF_UNIX, SOCK_STREAM, 0, sock_pair);
	if (error) {
	    kmod_set_error("cannot create socket pair");
	    error = -1;
	    break;
	}

	/* Launch KMOD. */
	error = fork();

	/* Error. */
	if (error == -1) {
	    kmod_set_error("fork failed: %s");
	    break;
	}

	/* Child. */
	else if (error == 0) {
	
	    /* Assign socket to stdin. */
	    error = dup2(sock_pair[1], 0);
	    if (error) {
		printf("Cannot duplicate socket\n");
		exit(1);
	    }

	    /* Close the socket descriptors. Important, otherwise we'll wait
	     * for data on our own duplicated descriptor.
	     */
	    close(sock_pair[0]);
	    close(sock_pair[1]);

	    /* Execute or die. */
	    execl(global_opts.kmod_binary_path.data, global_opts.kmod_binary_path.data, "-C", "inherited", "-l", "2",
		  "-k", global_opts.kmod_db_path.data, NULL);
	    printf("Cannot execute kmod: %s.\n", strerror(errno));
	    exit(1);
	}

	/* Parent. */
	*kmod_pid = error;
	error = 0;
	*kmod_sock = sock_pair[0];
	close(sock_pair[1]);
	sock_pair[0] = sock_pair[1] = -1;

    } while (0);

    if (sock_pair[0] != -1) close(sock_pair[0]);
    if (sock_pair[1] != -1) close(sock_pair[1]);
    
    return error;
}

static void kcd_close_kmod(int *kmod_sock, int *kmod_pid) {
    ksock_close(kmod_sock);
    
    if (*kmod_pid != -1) {
	kill(*kmod_pid, SIGKILL);
	waitpid(*kmod_pid, NULL, 0);
	*kmod_pid = -1;
    }
}

int kcd_ask_kmod_about_kws_ticket(char *ticket_data, int ticket_len, uint64_t key_id, int *valid) {
    int error = 0;
    uint32_t i;
    int kmod_sock = -1, kmod_pid = -1;
    struct kmod_transfer_hub hub;
    struct k3p_proto k3p, *k = &k3p;
    kstr str, errmsg;
    
    kmod_log_msg(KCD_LOG_KMOD, "kcd_ask_kmod_about_kws_ticket() called.\n");
    
    kmod_transfer_hub_init(&hub);
    
    k3p_proto_init(&k3p);
    k3p.timeout_enabled = 1;
    k3p.timeout = 10000;
    k3p.transfer.driver = kmod_sock_driver;
    k3p.hub = &hub;
    
    kstr_init(&str);
    kstr_init(&errmsg);

    do {
	error = kcd_start_kmod(&kmod_sock, &kmod_pid);
	if (error) break;
	
	k3p.transfer.fd = kmod_sock;
	kmod_sock = -1;
	error = -1;

	k3p_write_inst(k, KPP_CONNECT_KMO);
	k3p_write_uint32(k, 0);
	k3p_write_uint32(k, 0);
	k3p_write_cstr(k, "kcd");
	k3p_write_uint32(k, 0);
	k3p_write_uint32(k, 0);
	k3p_write_uint32(k, 0);
	k3p_write_uint32(k, 0);
	if (k3p_send_data(k)) break;
	
	if (k3p_read_inst(k, &i)) break;
	if (i != KMO_COGITO_ERGO_SUM) {
	    kmod_set_error("kmod negociation error");
	    break;
	}
	if (k3p_read_kstr(k, &str) || k3p_read_kstr(k, &str) || k3p_read_kstr(k, &str)) break;

	k3p_write_inst(k, KPP_BEG_SESSION);
	k3p_write_inst(k, K3P_VALIDATE_TICKET);
	kstr_assign_buf(&str, ticket_data, ticket_len);
	k3p_write_kstr(k, &str);
	kstr_sf(&str, PRINTF_64"u", key_id);
	k3p_write_kstr(k, &str);
	
	if (k3p_send_data(k)) break;
	if (k3p_read_inst(k, &i)) break;
	
	if (i != K3P_COMMAND_OK) {
        if (i == KMO_SERVER_ERROR) {
            if (k3p_read_uint32(k, &i)) break;
            if (k3p_read_uint32(k, &i)) break;
            if (k3p_read_kstr(k, &errmsg)) break;
	    kmod_set_error("kmod replied %u to validate ticket query: '%s'", i, errmsg.data);
        }
        else {
            kmod_set_error("kmod replied %u to validate ticket query", i);
        }
	    break;
	}
	
	if (k3p_read_uint32(k, &i)) break;
	if (k3p_read_kstr(k, &str)) break;
	
	if (! i) {
	    *valid = 1;
	}
	
	else {
	    *valid = 0;
	    kmod_set_error("invalid "KCD_KWS_NAME" ticket: %s", str.data);
	}
	
	error = 0;
	
    } while (0);

    kcd_close_kmod(&kmod_sock, &kmod_pid);
    kstr_clean(&str);
    kstr_clean(&errmsg);
    k3p_proto_clean(&k3p);
    kmod_transfer_hub_clean(&hub);
    
    return error;
}

/* Log the content of the command or result specified. This function trashes the
 * current error message.
 */
void kcd_log_kanp_msg(uint32_t level, int is_cmd, struct anp_msg *msg) {
    kbuffer *buf = &msg->payload;
    int cur_buf_pos = buf->pos;
    uint32_t type = msg->type;
    uint32_t proto = (type & (3 << 28)) >> 28;
    uint32_t role = (type & (3 << 26)) >> 26;
    uint32_t ns = (type & (1023 << 16)) >> 16;
    uint32_t sub = (type & (255 << 8)) >> 8;
    kstr str;
    kstr tmp;
    
    kstr_init(&str);
    kstr_init(&tmp);
    buf->pos = 0;

    if (is_cmd) kstr_append_cstr(&str, ">>> Command: ");
    else kstr_append_cstr(&str, "<<< Result: ");
    
    if (proto != 1) kstr_append_sf(&str, "bad proto (%d) ", proto);
    
    if (is_cmd && role != 0) kstr_append_sf(&str, "bad role (%d) ", role);
    else if (!is_cmd && role != 1) kstr_append_sf(&str, "bad role (%d) ", role);
    
    kstr_append_sf(&str, "ns %u, subtype %u, ID "PRINTF_64"u.\n", ns, sub, msg->id);
    
    if (!is_cmd && type == KANP_RES_FAIL) {
        uint32_t fail_type = 0;
        if (anp_read_uint32(buf, &fail_type) || anp_read_kstr(buf, &tmp)) 
            kstr_assign_cstr(&tmp, "invalid RES_FAIL format");
        kstr_append_sf(&str, "RES_FAIL type %u: %s.\n", fail_type, tmp.data);
    }
    
    if (is_cmd) kmod_log_msg(level, "");
    kmod_log_msg(level, str.data);
    
    kstr_clean(&str);
    kstr_clean(&tmp);
    buf->pos = cur_buf_pos;
}

/* Wait for the child specified. Return true if the child has been collected. If
 * block_flag is true, the call will block in waitpid(), though EINTR may cause
 * an early return. failed_flag will be set to true if the process has failed.
 */
int kcd_waitpid(int pid, int block_flag, int *failed_flag) {
    int status;
    int r = waitpid(pid, &status, block_flag ? 0 : WNOHANG);
    
    if (r == -1 && errno == EINTR) return 0;
    else if (r == -1) kerror_fatal("waitpid() failed: %s", kerror_syserror());
    else if (r) {
        *failed_flag = (!WIFEXITED(status) || WEXITSTATUS(status) != 0);
        return 1;
    }
    
    return 0;
}

/* Close all six file descriptors. */
static void kcd_process_close_all_desc(struct kcd_process *self) {
    int i;
    for (i = 0; i < 6; i++) ksock_close(self->desc + i);
}

void kcd_process_init(struct kcd_process *self) {
    int i;
    memset(self, 0, sizeof(struct kcd_process));
    self->output_cap = 100*1024*1024;
    for (i = 0; i < 3; i++) kdaemon_open_socket_pair(self->desc + i*2);
    kstr_init(&self->name);
    kstr_init(&self->in_str);
    kstr_init(&self->out_str);
    kstr_init(&self->err_str);
}

void kcd_process_clean(struct kcd_process *self) {
    kcd_process_close_all_desc(self);
    kstr_clean(&self->name);
    kstr_clean(&self->in_str);
    kstr_clean(&self->out_str);
    kstr_clean(&self->err_str);
}

/* Helper function for kcd_process_log_output(). */
void kcd_process_log_stream(struct kcd_process *self, char *what, kstr *stream) {
    char *c = stream->data;
    kstr line;
    
    if (stream->slen <= 1) return;
    
    kstr_init(&line);
    
    kmod_log_msg(self->log_level, "%s %s output:", self->name.data, what);
    
    while (1) {
        if (*c == '\n' || *c == 0) {
            if (line.slen) kmod_log_msg(self->log_level, "%s", line.data);
            if (*c == 0) break;
            kstr_reset(&line);
        }
        
        else kstr_append_char(&line, *c);
        
        c++;
    }
    
    kstr_clean(&line);
}

/* Log the output of the process, if any. */
void kcd_process_log_output(struct kcd_process *self, int stdout_flag, int stderr_flag) {
    if (stdout_flag) kcd_process_log_stream(self, "stdout", &self->out_str);
    if (stderr_flag) kcd_process_log_stream(self, "stderr", &self->err_str);
}

/* Set the KMOD error string to the outcome of the process. */
void kcd_process_import_error(struct kcd_process *self) {
    if (self->timeout_flag) {
        kmod_set_error("process %s timed out", self->name.data);
    }
    
    else if (self->failed_flag) {
        char *src = NULL;
        
        if (self->err_str.slen > 1) src = self->err_str.data;
        else if (self->out_str.slen > 1) src = self->out_str.data;
        
        if (src) {
            char buf[100];
            snprintf(buf, 100, "%s", src);
            kmod_set_error("process %s failed: %s", self->name.data, buf);
        }
        
        else {
            kmod_set_error("process %s failed", self->name.data);
        }
    }
    
    else {
        kmod_set_error("process %s succeeded", self->name.data);
    }
}

/* Handle writing to stdin of the process. */
static void kcd_process_handle_input(struct kcd_process *self) {
    int sock = self->desc[0], done_flag = 0;
    uint32_t len = self->in_str.slen - self->in_pos;
    
    if (sock == -1) return;
    
    if (!len) {
        kmod_log_msg(self->log_level, "No data to send to %s.\n", self->name.data);
        done_flag = 1;
    }
    
    else {
        int r = ksock_write(sock, self->in_str.data + self->in_pos, &len);
    
        /* We've written data. */
        if (!r) {
            self->in_pos += len;
            kmod_log_msg(self->log_level, "Wrote %u bytes to %s stdin.\n", len, self->name.data);
        
            if (self->in_str.slen == self->in_pos) {
                kmod_log_msg(self->log_level, "Sent all data to %s.\n", self->name.data);
                done_flag = 1;
            }
        }
        
        /* Socket closed or an error occurred. */
        else if (r == -1) {
            kmod_log_msg(self->log_level, "Error writing to %s: %s.\n", self->name.data, kmod_strerror());
            done_flag = 1;
        }
    }
    
    /* We must close the socket. */
    if (done_flag) {
        ksock_close(self->desc + 0);
    }
}

/* Handle reading from stdout or stderr of the process. */
static void kcd_process_handle_output(struct kcd_process *self, int *sock, kstr *out_str, char *desc) {
    int r;
    char buf[BUFSIZ];
    uint32_t len = BUFSIZ;
    
    if (*sock == -1) return;
    
    r = ksock_read(*sock, buf, &len);
    
    /* We've read data. */
    if (!r) {
        kmod_log_msg(self->log_level, "Read %u bytes from %s %s.\n", len, self->name.data, desc);
        kstr_append_buf(out_str, buf, MIN(len, self->output_cap - out_str->slen));
    }
    
    /* Socket closed or an error occurred. */
    else if (r == -1) {
        kmod_log_msg(self->log_level, "Error reading from %s %s: %s.\n", self->name.data, desc, kmod_strerror());
        ksock_close(sock);
    }
}

/* Return true if the process is finished. */
int kcd_process_is_finished(struct kcd_process *self) {
    return (self->collected_flag && self->desc[2] == -1 && self->desc[4] == -1);
}

/* Return true if a socket is ready according to the last call to select. */
int kcd_process_is_socket_ready(struct kcd_process *self, struct kselect *sel) {
    return kselect_in_write(sel, self->desc[0]) ||
           kselect_in_read(sel, self->desc[2]) ||
           kselect_in_read(sel, self->desc[4]);
}

/* Prepare the call to select by adding the sockets in the select set specified. */
void kcd_process_prepare_select(struct kcd_process *self, struct kselect *sel) {
    kselect_add_write(sel, self->desc[0]);
    kselect_add_read(sel, self->desc[2]);
    kselect_add_read(sel, self->desc[4]);
}

/* Perform the transfers with the process. */
void kcd_process_do_xfer(struct kcd_process *self) {
    kcd_process_handle_input(self);
    kcd_process_handle_output(self, self->desc + 2, &self->out_str, "stdout");
    kcd_process_handle_output(self, self->desc + 4, &self->err_str, "stderr");
}

/* Kill the process. */
void kcd_process_kill(struct kcd_process *self) {
    if (!self->pid || self->collected_flag || self->killed_flag) return;
    kill(self->pid, SIGKILL);
    self->killed_flag = 1;
    kmod_log_msg(self->log_level, "Killed process %s.\n", self->name.data);
}

/* Try to collect the process. */
void kcd_process_try_collect(struct kcd_process *self) {
    if (!self->pid || self->collected_flag) return; 
    if (kcd_waitpid(self->pid, 0, &self->failed_flag)) {
        kmod_log_msg(self->log_level, "Collected process %s.\n", self->name.data);
        self->collected_flag = 1;
    }
}

/* Kill the process and block until the process has been collected or an error
 * occurs.
 */
int kcd_process_kill_and_collect(struct kcd_process *self) {
    kmod_log_msg(self->log_level, "kcd_process_kill_and_collect() called.\n");
    
    if (!self->pid || self->collected_flag) return 0;
    
    kcd_process_kill(self);
    kcd_process_close_all_desc(self);
    
    while (1) {
        struct kselect sel;
        
        kcd_process_try_collect(self);
        if (self->collected_flag) return 0;
        
        kdaemon_prepare_select(&sel);
        if (kdaemon_do_select(&sel)) return -1;
    }
}

/* Perform transfers with the process until a timeout occurs or all the output
 * is collected. The process is collected in all cases unless an error occurs.
 * The timeout is in milliseconds. The value -1 means infinite.
 */
int kcd_process_xfer_and_collect(struct kcd_process *self, int64_t timeout) {
    int64_t remaining, deadline = (timeout == -1) ? 0 : ktime_set_deadline(timeout);
    uint64_t last_sigchld_count = 0;
    struct kselect sel;
    
    kmod_log_msg(self->log_level, "kcd_process_xfer_and_collect() called.\n");
    
    while (1) {
        
        /* The process is finished. */
        if (kcd_process_is_finished(self)) {
            kmod_log_msg(self->log_level, "Collected and received all data from %s.\n", self->name.data);
            return 0;
        }
        
        /* Timeout. Kill and collect. */
        if (deadline && ktime_check_deadline(deadline, &remaining)) {
            kmod_log_msg(self->log_level, "Process %s timed out.\n", self->name.data);
            self->timeout_flag = 1;
            return kcd_process_kill_and_collect(self);
        }
	
        /* Perform the select() call. */
        kdaemon_prepare_select(&sel);
        kcd_process_prepare_select(self, &sel);
        if (deadline) ktime_from_msec(&sel.tv, remaining + 1);
	if (kdaemon_do_select(&sel)) return -1;
        
        /* Perform transfers. */
        if (kcd_process_is_socket_ready(self, &sel)) kcd_process_do_xfer(self);
        
        /* Collect the process, if required. */
        if (global_opts.sigchld_count != last_sigchld_count) {
            kdaemon_block_signals();
            last_sigchld_count = global_opts.sigchld_count;
            kcd_process_try_collect(self);
            kdaemon_unblock_signals();
        }
    }
}

/* Execute the program. */
static void kcd_process_call_exec(struct kcd_process *self, char **argv) {
    int i;
    
    /* Close the pipes we don't need. */
    close(0);
    close(1);
    close(2);

    /* Set the pipes to the standard pipes for the program. Make the sockets
     * blocking otherwise some processes will barf.
     */
    dup2(self->desc[1], 0);
    dup2(self->desc[3], 1);
    dup2(self->desc[5], 2);
    ksock_set_blocking(0);
    ksock_set_blocking(1);
    ksock_set_blocking(2);
    for (i = 0; i < 6; i++) ksock_close(self->desc + i);

    /* Execute the process. */
    execv(argv[0], argv);
    kerror_fatal("cannot execute %s: %s", argv[0], kmod_syserror());
}

/* Extract the process name from the process path. */
static void kcd_process_get_process_name(struct kcd_process *self, char *path) {
    kstr tmp;
    kstr_init_cstr(&tmp, path);
    kpath_basename(&tmp, &self->name, 0);
    kstr_clean(&tmp);
}

/* Start the process. 'argv' must be a NULL-terminated array containing the path
 * to the executable and the remaining arguments. 'argv' needs to be valid only
 * during this call. If gc_func is non-NULL, it will be called with the argument
 * 'gc_arg' to give the child a chance to close the inherited UNIX file
 * descriptors.
 */
int kcd_process_start(struct kcd_process *self, char **argv, void *gc_func, void *gc_arg) {
    assert(!self->pid);
    
    /* Cache the process name. */
    kcd_process_get_process_name(self, argv[0]);
    
    kmod_log_msg(self->log_level, "Starting process %s.\n", self->name.data);
    
    /* Fork the process. */
    if (kcd_fork(self->name.data, &self->pid, 0)) return -1;
        
    /* Child. */
    if (!self->pid) {
        void (*casted_gc_func)(void *) = gc_func;
        if (casted_gc_func) casted_gc_func(gc_arg);
        kcd_process_call_exec(self, argv);
    }
    
    /* Close the child file descriptors. */
    ksock_close(self->desc + 1);
    ksock_close(self->desc + 3);
    ksock_close(self->desc + 5);
    
    return 0;
}

/* Call kcd_process_start() and kcd_process_xfer_and_collect(). If critical_flag
 * is true, -1 is returned if the process failed.
 */
int kcd_process_start_and_collect(struct kcd_process *self, char **argv, void *gc_func, void *gc_arg,
                                  int64_t timeout, int critical_flag) {
    
    if (kcd_process_start(self, argv, gc_func, gc_arg) || kcd_process_xfer_and_collect(self, timeout)) return -1;
    
    if (critical_flag && (self->timeout_flag || self->failed_flag)) {
        kcd_process_import_error(self);
        return -1;
    }
    
    return 0;
}

/* Call kcdhelper with the arguments specified. */
int kcd_exec_kcdhelper(char *task, uint64_t kws_id, int log_level) {
    int error;
    char kws_id_buf[100];
    char *argv[] = { "/usr/bin/kcdhelper", task, kws_id_buf, NULL };
    struct kcd_process process;
    
    kmod_log_msg(log_level, "kcd_exec_kcdhelper() called with task %s.\n", task);
    
    kcd_process_init(&process);
    process.log_level = log_level;
    sprintf(kws_id_buf, PRINTF_64"u", kws_id);
    error = kcd_process_start_and_collect(&process, argv, NULL, NULL, -1, 1);
    kcd_process_clean(&process);
    return error;
}

