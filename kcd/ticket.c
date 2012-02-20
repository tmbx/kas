/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* Conventions used in ticket mode:
 * - The error code 0 indicates that no error has occurred.
 * - The error code -1 indicates that an internal error has occurred.
 * - The error code -2 indicates that the command has failed with a generic 
 *   error. The KANP_RES_FAIL_GEN result will be reported to the user.
 * - The error code -3 indicates that the command has failed with a specific
 *   error result. The result provided will be returned to the user.
 * - The error code -4 indicates that the connection with the client has been
 *   lost.
 */
        
/* Dispatch table for the ticket modes. */
static struct kcd_ticket_mode_dispatch_entry kcd_ticket_mode_dispatch_table[] = {
    { KANP_CMD_KFS_DOWNLOAD_REQ, KANP_RES_KFS_DOWNLOAD_REQ, KANP_CMD_KFS_DOWNLOAD_DATA, 
      KANP_KCD_TICKET_DOWNLOAD, kcd_kfs_handle_download },
      
    { KANP_CMD_KFS_UPLOAD_REQ, KANP_RES_KFS_UPLOAD_REQ, KANP_CMD_KFS_PHASE_1,
      KANP_KCD_TICKET_UPLOAD, kcd_kfs_handle_upload },
      
    { KANP_CMD_VNC_CONNECT_TICKET, KANP_RES_VNC_CONNECT_TICKET, KANP_CMD_VNC_CONNECT_SESSION,
      KANP_KCD_TICKET_VNC_CLIENT, kcd_vnc_connect_session },
      
    { KANP_CMD_VNC_START_TICKET, KANP_RES_VNC_START_TICKET, KANP_CMD_VNC_START_SESSION,
      KANP_KCD_TICKET_VNC_SERVER, kcd_vnc_start_session },
};

void kcd_ticket_mode_state_init(struct kcd_ticket_mode_state *self, struct kcd_client *client) {
    memset(self, 0, sizeof(struct kcd_ticket_mode_state));
    self->client = client;
    anp_tls_init(&self->xfer);
    pg_db_conn_init(&self->db_conn);
    kcd_internal_ticket_init(&self->ticket);
    kstr_init(&self->query);
    kcd_pg_anp_query_init(&self->aq);
    kbuffer_init(&self->kws_bound_buf);
    kstr_init(&self->license_email);
    kcd_global_user_usage_info_init(&self->usage_info);
    kcd_global_user_license_info_init(&self->license_info);
}

void kcd_ticket_mode_state_clean(struct kcd_ticket_mode_state *self) {
    anp_tls_clean(&self->xfer);
    pg_db_conn_clean(&self->db_conn);
    anp_msg_destroy(self->in_msg);
    anp_msg_destroy(self->out_msg);
    kcd_internal_ticket_clean(&self->ticket);
    kstr_clean(&self->query);
    kcd_pg_anp_query_clean(&self->aq);
    kbuffer_clean(&self->kws_bound_buf);
    kstr_clean(&self->license_email);
    kcd_global_user_usage_info_clean(&self->usage_info);
    kcd_global_user_license_info_clean(&self->license_info);
}

/* Return the entry corresponding to the command or first message type
 * specified, if any.
 */
struct kcd_ticket_mode_dispatch_entry* kcd_ticket_mode_get_dispatch_entry(uint32_t type) {
    uint32_t i;
    
    for (i = 0; i < sizeof(kcd_ticket_mode_dispatch_table)/sizeof(struct kcd_ticket_mode_dispatch_entry); i++) {
        struct kcd_ticket_mode_dispatch_entry *entry = kcd_ticket_mode_dispatch_table + i;
        if (entry->cmd_type == type || entry->first_msg_type == type) return entry;
    }
    
    return NULL;
}

/* Clear the current input message, if any. */
void kcd_ticket_mode_clear_in_msg(struct kcd_ticket_mode_state *self) {
    anp_msg_destroy(self->in_msg);
    self->in_msg = NULL;
}

/* Clear the current output message, if any. */
void kcd_ticket_mode_clear_out_msg(struct kcd_ticket_mode_state *self) {
    anp_msg_destroy(self->out_msg);
    self->out_msg = NULL;
}

/* Create a new output message with the correct major/minor, the message type
 * specified and ID 0. For convenience, a pointer to the output buffer is
 * returned.
 */
kbuffer* kcd_ticket_mode_new_out_msg(struct kcd_ticket_mode_state *self, uint32_t type) {
    anp_msg_destroy(self->out_msg);
    self->out_msg = anp_msg_new();
    self->out_msg->minor = self->client->effective_minor;
    self->out_msg->type = type;
    return &self->out_msg->payload;
}

/* Create a new output message and call kcd_kanp_set_failure(). Return -3 for
 * convenience.
 */
int kcd_ticket_mode_set_failure(struct kcd_ticket_mode_state *self, uint32_t error_type) {
    kcd_ticket_mode_new_out_msg(self, 0);
    kcd_kanp_set_failure(self->out_msg, error_type);
    return -3;
}

/* Create a new output message, set the result type to failure and return the
 * result payload.
 */
kbuffer* kcd_ticket_mode_failure(struct kcd_ticket_mode_state *self) {
    return kcd_ticket_mode_new_out_msg(self, KANP_RES_FAIL);
}

/* Execute a workspace-bound query in Postgres. 'cmd' can be NULL. */
int kcd_ticket_mode_kws_bound_query(struct kcd_ticket_mode_state *self, char *query_name,
                                    uint64_t date, struct anp_msg *cmd) {
    kcd_ticket_mode_new_out_msg(self, 0);
    return kcd_exec_kws_bound_query(&self->db_conn, &self->aq, query_name, cmd, self->out_msg, self->kws_id,
                                    date, self->login_type, self->user_id, self->client->effective_minor,
                                    &self->kws_bound_buf);
}

/* Perform a permission check. Return 0, -1 or -4. */
int kcd_ticket_mode_do_perm_check(struct kcd_ticket_mode_state *self) {
    kbuffer *in_buf = &self->aq.input_buf, *out_buf = &self->aq.output_buf;
    int error = 0;
    uint32_t res, login_code;
    kstr msg;
    
    kmod_log_msg(self->log_level, "kcd_ticket_mode_do_perm_check() called.\n");
    
    kstr_init(&msg);

    do {
        anp_write_uint64(in_buf, self->kws_id);
        anp_write_uint32(in_buf, self->login_type);
        anp_write_uint32(in_buf, self->user_id);
        error = kcd_exec_safe_pg_anp_query(&self->db_conn, &self->aq, "check_kws_login");
        if (error) break;
        
        if (anp_read_uint32(out_buf, &res) ||
            anp_read_uint32(out_buf, &login_code) ||
            anp_read_kstr(out_buf, &msg)) {
            error = -1;
            break;
        }
        
        if (res) {
            kmod_set_error("permission check failed: %s", msg.data);
            error = -4;
            break;
        }
        
    } while (0);
    
    kstr_clean(&msg);

    return error;
}

/* Process the notifications received from Postgres, if any. Return 0, -1 or -4. */
int kcd_ticket_mode_process_notif(struct kcd_ticket_mode_state *self) {
    int notif_flag = 0;
    
    kmod_log_msg(self->log_level, "kcd_ticket_mode_process_notif() called.\n");

    /* Allow Postgres to consume its input, if any. */
    if (pg_db_consume(&self->db_conn)) return -1;
	
    /* Process all notifications. */
    while (1) {
        PGnotify *notif = pg_db_notify_check(&self->db_conn);
        if (!notif) break;
        notif_flag = 1;
        PQfreemem(notif);
    }
    
    if (!notif_flag) return 0;
    
    /* Perform a permission check. */
    return kcd_ticket_mode_do_perm_check(self);
}

/* Prepare the state to wait for something to happen. */
void kcd_ticket_mode_prepare_wait(struct kcd_ticket_mode_state *self) {
    kdaemon_prepare_select(&self->sel);
    kselect_add_read(&self->sel, self->db_conn.sock);
}

/* Wait for something to happen for up to 'delay' milliseconds (-1 is infinite).
 * Postgres permission checks are performed as necessary. Return 0, -1 or -4.
 */
int kcd_ticket_mode_wait(struct kcd_ticket_mode_state *self, int64_t delay) {
    int error = 0;
    
    kmod_log_msg(self->log_level, "kcd_ticket_mode_wait() called.\n");
    
    if (delay == -1) delay = 1000000000;
    ktime_from_msec(&self->sel.tv, delay + 1);
    
    error = kdaemon_do_select(&self->sel);
    kmod_log_msg(self->log_level, "kcd_ticket_mode_wait(): out of select().\n");
    if (error) return error;
    
    if (kselect_in_read(&self->sel, self->db_conn.sock)) {
        error = kcd_ticket_mode_process_notif(self);
        if (error) return error;
    }
    
    return 0;
}

/* Wait for the current transfer to complete. Return 0, -1 or -4. */
int kcd_ticket_mode_timed_tls_xfer(struct kcd_ticket_mode_state *self, int64_t delay) {
    kmod_log_msg(self->log_level, "kcd_ticket_mode_timed_tls_xfer() called.\n");
    if (!anp_tls_has_xfer(&self->xfer)) return 0;
    kcd_ticket_mode_prepare_wait(self);
    anp_tls_prepare_select(&self->xfer, &self->client->conn, &self->sel);
    return kcd_ticket_mode_wait(self, delay);
}

/* Send and clear the current output message to the client. Return 0, -1, or -4. */
int kcd_ticket_mode_send_msg(struct kcd_ticket_mode_state *self) {
    int error = 0;
    
    kmod_log_msg(self->log_level, "kcd_ticket_mode_send_msg() called.\n");
    
    assert(self->out_msg);
    anp_tls_send_msg(&self->xfer, self->out_msg);
    kcd_ticket_mode_clear_out_msg(self);
    
    while (1) {
        error = kcd_ticket_mode_process_notif(self);
        if (error) return error;
        if (anp_tls_do_xfer(&self->xfer, &self->client->conn)) return -4;
        if (anp_tls_done_sending(&self->xfer)) return 0;
        error = kcd_ticket_mode_timed_tls_xfer(self, -1);
        if (error) return error;
    }
}
    
/* Receive the next message from the client. Return 0, -1, or -4. */
int kcd_ticket_mode_recv_msg(struct kcd_ticket_mode_state *self) {
    return kcd_ticket_mode_timed_recv_msg(self, -1);
}

/* Receive the next message from the client. Return 0 if a timeout occurs.
 * 'in_msg' is NULL if no message was received. Return 0, -1, or -4.
 */
int kcd_ticket_mode_timed_recv_msg(struct kcd_ticket_mode_state *self, int64_t delay) {
    int error = 0;
    int64_t remaining = -1, deadline = (delay == -1) ? 0 : ktime_set_deadline(delay);
    
    kmod_log_msg(self->log_level, "kcd_ticket_mode_timed_recv_msg() called.\n");
    
    kcd_ticket_mode_clear_in_msg(self);
    if (!anp_tls_receiving(&self->xfer)) anp_tls_begin_recv(&self->xfer);
    
    if (!anp_tls_done_receiving(&self->xfer)) {
        while (1) {
            error = kcd_ticket_mode_process_notif(self);
            if (error) return error;
            if (anp_tls_do_xfer(&self->xfer, &self->client->conn)) return -4;
            if (anp_tls_done_receiving(&self->xfer)) break;
            if (deadline && ktime_check_deadline(deadline, &remaining)) return 0;
            error = kcd_ticket_mode_timed_tls_xfer(self, remaining);
            if (error) return error;
       }
    }
    
    self->in_msg = anp_tls_get_recv(&self->xfer);
    
    return 0;
}

/* Retrieve the email address of the user to which the workspace is licensed.
 * The result is empty if the email address cannot be found.
 */
int kcd_ticket_mode_get_license_email(struct kcd_ticket_mode_state *self) {
    int error = 0;
    PGresult *pg_res = NULL;
    
    kmod_log_msg(self->log_level, "kcd_ticket_mode_get_license_email() called.\n");
    
    do {
        kstr_sf(&self->query, "SELECT email FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u AND user_id = 1",
                              self->kws_id);
	error = kcd_exec_pg_query(&self->db_conn, self->query.data, &pg_res, "get license email");
        if (error) break;
        
        if (PQntuples(pg_res)) kstr_assign_cstr(&self->license_email, PQgetvalue(pg_res, 0, 0));
        else kstr_reset(&self->license_email);
	
    } while (0);
    
    pg_db_destroy_res(&pg_res);
    
    return error;
}

/* Retrieve the resource usage and license information associated to the license
 * email, if any.
 */
int kcd_ticket_mode_get_usage_and_license_info(struct kcd_ticket_mode_state *self) {
    kbuffer *in_buf = &self->aq.input_buf, *out_buf = &self->aq.output_buf;
    struct kcd_global_user_usage_info *u = &self->usage_info;
    struct kcd_global_user_license_info *l = &self->license_info;
    
    kmod_log_msg(self->log_level, "kcd_ticket_mode_get_usage_and_license_info() called.\n");
    
    if (!self->license_email.slen) {
        kcd_global_user_usage_info_reset(u);
        kcd_global_user_license_info_reset(l);
        return 0;
    }
    
    anp_write_kstr(in_buf, &self->license_email);
    if (kcd_exec_safe_pg_anp_query(&self->db_conn, &self->aq, "get_usage_and_license_info")) return -1;
    
    if (anp_read_uint32(out_buf, &u->nb_non_pb_kws) ||
        anp_read_uint32(out_buf, &u->nb_pb_kws) ||
        anp_read_uint64(out_buf, &u->kfs_usage) ||
        anp_read_kstr(out_buf, &l->license_name) ||
        anp_read_uint32(out_buf, &l->nb_non_pb_kws) ||
        anp_read_uint32(out_buf, &l->nb_pb_kws) ||
        anp_read_uint64(out_buf, &l->kfs_usage) ||
        anp_read_uint32(out_buf, &l->secure_kws_flag) ||
        anp_read_uint64(out_buf, &l->vnc_session_time)) {
        return -1;
    }
    
    return 0;
}

/* Handle the error codes -2 and -3, sending back an error message to the
 * client. Return -1 or -4.
 */
int kcd_ticket_mode_handle_user_error(struct kcd_ticket_mode_state *self, int r) {
    int error = 0;
    
    kmod_log_msg(self->log_level, "kcd_ticket_mode_handle_user_error() called.\n");
    
    /* Handle generic failure. */
    if (r == -2) {
        kcd_ticket_mode_new_out_msg(self, 0);
        kcd_kanp_set_gen_failure(self->out_msg);
    }
    
    /* We should already have a failure message set. */
    else {
        assert(self->out_msg);
    }
    
    /* Log the message. */
    kcd_log_kanp_msg(KCD_LOG_BRIEF, 0, self->out_msg);
    
    /* Send back the failure message. */
    error = kcd_ticket_mode_send_msg(self);
    if (error == 0) {
        kmod_set_error("closing connection after sending error message");
        error = -4;
    }
    
    return error;
}

/* Read, parse and consume the user-supplied ticket. Return 0, -1 or -2. */
int kcd_ticket_mode_validate_ticket(struct kcd_ticket_mode_state *self, uint32_t expected_type) {
    int error = 0;
    PGresult *pg_res = NULL;
    kbuffer *payload = &self->ticket.payload;
    
    kmod_log_msg(self->log_level, "kcd_ticket_mode_validate_ticket() called.\n");
    
    do {
        /* Retrieve the ticket payload from the command arguments. */
    	if (anp_read_bin(&self->in_msg->payload, payload)) {
	    error = -2;
	    break;
	}
        
	/* Parse the ticket payload. We don't care about the nonce. */
    	if (anp_read_uint32(payload, &self->ticket.type) ||
            anp_read_uint64(payload, &self->kws_id) ||
            anp_read_uint32(payload, &self->login_type) || 
            anp_read_uint32(payload, &self->user_id) ||
            anp_read_bin(payload, &self->ticket.ext)) {
            error = -2;
	    break;
	}
        
        /* Validate the ticket type. */
        if (self->ticket.type != expected_type) {
	    kmod_set_error("invalid ticket type");
            error = -2;
            break;
        }
        
        /* Consume the ticket. */
	kstr_sf(&self->query, "SELECT consume_kcd_ticket(");
	pg_db_add_bytea(&self->db_conn, &self->query, payload);
	kstr_append_cstr(&self->query, ")");
	error = kcd_exec_pg_query(&self->db_conn, self->query.data, &pg_res, "consume_kcd_ticket");
    	if (error) break;
        
        /* Invalid ticket. */
        if (!pg_db_get_uint32(pg_res, 0, 0)) {
            kmod_set_error("expired KCD ticket");
            error = -2;
            break;
        }
	
    } while (0);
    
    pg_db_destroy_res(&pg_res);
    
    return error;
}

/* Handle a connection in one of the ticket modes. */
int kcd_ticket_mode_handle_conn(struct kcd_client *client, uint32_t app) {
    int error = 0;
    char *task_name = "";
    struct kcd_ticket_mode_dispatch_entry *entry;
    struct kcd_ticket_mode_state st;
    
    kcd_ticket_mode_state_init(&st, client);
    
    do {
        /* Set the task name and the log level. */
        if (app == KANP_NS_KFS) {
            task_name = "KFS";
            st.log_level = KCD_LOG_KFS;
        }
        
        else if (app == KANP_NS_VNC) {
            task_name = "VNC";
            st.log_level = KCD_LOG_VNC;
        }
        
        kdaemon_set_task("%s | %s", task_name, client->addr.data);
        kmod_log_msg(KCD_LOG_BRIEF, "kcd_ticket_mode_handle_conn() called.\n");
        
        /* Connect to the DB. */
	error = kcd_open_pg_conn(&st.db_conn, "dbname=kcd");
	if (error) break;
        
        /* Receive the initial message. */
        error = kcd_ticket_mode_recv_msg(&st);
        if (error) break;
        
        /* Get the dispatch entry. */
        entry = kcd_ticket_mode_get_dispatch_entry(st.in_msg->type);
        if (!entry) {
	    kmod_set_error("invalid request type (%u)", st.in_msg->type);
	    error = -2;
            break;
        }
        
        /* Validate the ticket. */
        error = kcd_ticket_mode_validate_ticket(&st, entry->ticket_type);
        if (error) break;
        
        /* Listen to workspace permission check notifications. */
        kstr_sf(&st.query, "LISTEN kws_"PRINTF_64"u_perm_check", st.kws_id);
        error = kcd_exec_pg_query(&st.db_conn, st.query.data, NULL, "listen to workspace");
        if (error) break;
        
        /* Perform the initial permission check. */      
        error = kcd_ticket_mode_do_perm_check(&st);
        if (error) break;
        
        /* Retrieve the usage and license information.. */
        error = kcd_ticket_mode_get_license_email(&st);
        if (error) break;
        
        error = kcd_ticket_mode_get_usage_and_license_info(&st);
        if (error) break;
        
        /* Dispatch. */
        error = entry->handler(&st);
        if (error) break;
        
    } while (0);
    
    /* Handle user errors.  */
    if (error == -2 || error == -3) error = kcd_ticket_mode_handle_user_error(&st, error);
    
    /* Convert lost connection error to internal error. */
    if (error == -4) error = -1;
        
    assert(error == 0 || error == -1);
    
    kcd_ticket_mode_state_clean(&st);
    
    return error;
}

