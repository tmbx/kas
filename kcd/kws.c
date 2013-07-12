/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* Conventions used in KANP workspace mode to handle user commands:
 * - The error code 0 indicates that the command has succeeded. The result 
 *   provided will be returned to the user.
 * - The error code -1 indicates that an internal error has occurred. A backend
 *   error will be sent to the client.
 * - The error code -2 indicates that the command has failed with a generic 
 *   error. The KANP_RES_FAIL_GEN result will be reported to the user.
 * - The error code -3 indicates that the command has failed with a specific
 *   error result. The result provided will be returned to the user.
 *
 * Race conditions:
 * 
 * There is a race condition where the client receives events before he got the
 * reply to the listen to workspace command. There is also a race condition
 * where the KCD can send events from a workspace which has been unlistened to.
 * We won't fix those races until the whole model is reimplemented to make it
 * more scalable.
 *
 * General suckiness:
 *
 * We don't use linked lists for lists; there are race conditions; the code is
 * generally ugly. The whole thing must be rewritten.
 */
 
/* Maximum size of each message queue of a client. */
#define KCD_KWS_MAX_CLIENT_QUEUE_SIZE           (2*1024*1024)

/* How much data we can be put in a single outgoing packet. */
#define KCD_KWS_MAX_CLIENT_OUT_PACKET_SIZE	(1*1024*1024)


/******************************************************************************/
/* Dispatch table for KANP commands. */

struct kcd_kws_cmd_dispatch_entry {

    /* Command type. */
    uint32_t type;
    
    /* Command name. */
    char *name;
    
    /* True if this is a workspace-bound command. */
    int kws_bound_flag;
    
    /* Command handler. */
    int (*handler)(struct kcd_kws_cmd_exec_state *);
};

static struct kcd_kws_cmd_dispatch_entry kcd_kws_cmd_dispatch_table[] = {
       { KANP_CMD_MGT_CREATE_KWS,       "create workspace",             0, kcd_mgt_create_kws },
       { KANP_CMD_MGT_FREEMIUM_CONFIRM, "send Freemium confirm email",  0, kcd_mgt_freemium_confirm },
       { KANP_CMD_KWS_CONNECT_KWS,      "connect to workspace",         0, kcd_mgt_connect_kws },
       { KANP_CMD_KWS_DISCONNECT_KWS,   "disconnect from workspace",    0, kcd_mgt_disconnect_kws },
       { KANP_CMD_KWS_INVITE_KWS,       "invite to workspace",          1, kcd_mgt_invite_kws },
       
       { KANP_CMD_KWS_SET_USER_PWD,     "set kws user password",        1, kcd_misc_cmd_kws_prop_change },
       { KANP_CMD_KWS_SET_USER_NAME,    "set kws user name",            1, kcd_misc_cmd_kws_prop_change },
       { KANP_CMD_KWS_SET_USER_ADMIN,   "set kws user admin",           1, kcd_misc_cmd_kws_prop_change },
       { KANP_CMD_KWS_SET_USER_MANAGER, "set kws user manager",         1, kcd_misc_cmd_kws_prop_change },
       { KANP_CMD_KWS_SET_USER_LOCK,    "set kws user lock",            1, kcd_misc_cmd_kws_prop_change },
       { KANP_CMD_KWS_SET_USER_BAN,     "set kws user ban",             1, kcd_misc_cmd_kws_prop_change },
       { KANP_CMD_KWS_SET_NAME,         "set workspace name",           1, kcd_misc_cmd_kws_prop_change },
       { KANP_CMD_KWS_SET_FREEZE,       "set workspace freeze",         1, kcd_misc_cmd_kws_prop_change },
       { KANP_CMD_KWS_SET_DEEP_FREEZE,  "set workspace deep freeze",    1, kcd_misc_cmd_kws_prop_change },
       { KANP_CMD_KWS_SET_SECURE,       "set workspace secure",         1, kcd_misc_cmd_kws_prop_change },
       { KANP_CMD_KWS_SET_THIN_KFS,     "set workspace thin KFS",       1, kcd_misc_cmd_kws_prop_change },
       
       { KANP_CMD_KFS_DOWNLOAD_REQ,     "grant download ticket",        1, kcd_misc_cmd_grant_kcd_ticket },
       { KANP_CMD_KFS_UPLOAD_REQ,       "grant upload ticket",          1, kcd_misc_cmd_grant_kcd_ticket },
       { KANP_CMD_VNC_CONNECT_TICKET,   "grant VNC client ticket",      1, kcd_misc_cmd_grant_kcd_ticket },
       { KANP_CMD_VNC_START_TICKET,     "grant VNC server ticket",      1, kcd_misc_cmd_grant_kcd_ticket },
       
       { KANP_CMD_CHAT_MSG,             "post chat message",            1, kcd_misc_cmd_chat_msg },
       { KANP_CMD_KWS_GET_UURL,         "get SKURL",                    1, kcd_misc_cmd_kws_get_uurl },
       { KANP_CMD_PB_ACCEPT_CHAT,       "accept public chat",           1, kcd_misc_cmd_pb_accept_chat },
};


/******************************************************************************/
/* Miscellaneous workspace functions */

struct kcd_kws_cmd_kws* kcd_kws_cmd_kws_new() {
    struct kcd_kws_cmd_kws *self = kcalloc(sizeof(struct kcd_kws_cmd_kws));
    return self;
}

void kcd_kws_cmd_kws_destroy(struct kcd_kws_cmd_kws *self) {
    if (self) {
        kfree(self);
    }
}

static struct kcd_kws_evt_kws* kcd_kws_evt_kws_new() {
    struct kcd_kws_evt_kws *self = kcalloc(sizeof(struct kcd_kws_evt_kws));
    return self;
}

static void kcd_kws_evt_kws_destroy(struct kcd_kws_evt_kws *self) {
    if (self) {
        kfree(self);
    }
}

static void kcd_kws_state_destroy_thread_msg(struct kcd_thread_msg *msg) {

    /* Free the memory associated to the thread message. For now the following
     * code works, but it will have to be updated if more complex messages are
     * passed.
     */
    if (msg) {
	kfree(msg->data);
	kfree(msg);
    }
}

/* Destroy the ANP messages in the array specified and clean/reset the array as
 * requested.
 */
static void kcd_kws_clear_anp_msg_array(karray *array, int clean_flag) {
    int i;
    for (i = 0; i < array->size; i++) anp_msg_destroy(array->data[i]);
    if (clean_flag) karray_clean(array);
    else karray_reset(array);
}

/* Destroy the ANP messages in the array specified and clean/reset the array as
 * requested.
 */
static void kcd_kws_clear_thread_msg_array(karray *array, int clean_flag) {
    int i;
    for (i = 0; i < array->size; i++) kcd_kws_state_destroy_thread_msg(array->data[i]);
    if (clean_flag) karray_clean(array);
    else karray_reset(array);
}

void kcd_internal_ticket_init(struct kcd_internal_ticket *self) {
    memset(self, 0, sizeof(struct kcd_internal_ticket));
    kbuffer_init(&self->payload);
    kbuffer_init(&self->ext);
}

void kcd_internal_ticket_clean(struct kcd_internal_ticket *self) {
    kbuffer_clean(&self->payload);
    kbuffer_clean(&self->ext);
}

static void kcd_kws_cmd_exec_state_init(struct kcd_kws_cmd_exec_state *self) {
    memset(self, 0, sizeof(struct kcd_kws_cmd_exec_state));
    kstr_init(&self->query);
    kcd_pg_anp_query_init(&self->aq);
    kbuffer_init(&self->kws_bound_buf);
}

static void kcd_kws_cmd_exec_state_clean(struct kcd_kws_cmd_exec_state *self) {
    kstr_clean(&self->query);
    kcd_pg_anp_query_clean(&self->aq);
    kbuffer_clean(&self->kws_bound_buf);
}

static void kcd_kws_state_init(struct kcd_kws_state *self) {
    memset(self, 0, sizeof(struct kcd_kws_state));
    
    kdaemon_open_socket_pair(self->brk_sock);
    kdaemon_open_socket_pair(self->cmd_sock);
    kdaemon_open_socket_pair(self->evt_sock);
    
    kmutex_init(&self->mutex);
    kstr_init(&self->no_backend_str);
    kstr_init(&self->no_client_str);
    karray_init(&self->in_msg_array);
    karray_init(&self->out_msg_array);
    karray_init(&self->evt_msg_array);
    karray_init(&self->cmd_msg_array);
    
    karray_init(&self->brk_out_msg_array);
    anp_tls_init(&self->brk_xfer);
    
    krb_tree_init_func(&self->evt_kws_tree, kutil_uint64_cmp);
    krb_tree_init_func(&self->evt_kws_active_tree, kutil_uint64_cmp);
    pg_db_conn_init(&self->evt_conn);
    
    krb_tree_init_func(&self->cmd_kws_tree, kutil_uint64_cmp);
    pg_db_conn_init(&self->cmd_conn);
}

static void kcd_kws_state_clean(struct kcd_kws_state *self) {
    int i, size;
    struct krb_node *iter;
    
    kmutex_clean(&self->mutex);
    kstr_clean(&self->no_backend_str);
    kstr_clean(&self->no_client_str);
    
    kcd_kws_clear_anp_msg_array(&self->in_msg_array, 1);
    kcd_kws_clear_anp_msg_array(&self->out_msg_array, 1);
    kcd_kws_clear_thread_msg_array(&self->evt_msg_array, 1);
    kcd_kws_clear_thread_msg_array(&self->cmd_msg_array, 1);
    
    
    kcd_kws_clear_anp_msg_array(&self->brk_out_msg_array, 1);
    anp_tls_clean(&self->brk_xfer);
    
    
    iter = krb_tree_iter_start(&self->evt_kws_tree);
    size = krb_tree_size(&self->evt_kws_tree);
    for (i = 0; i < size; i++) kcd_kws_evt_kws_destroy(krb_tree_iter_next(&self->evt_kws_tree, &iter));
    krb_tree_clean(&self->evt_kws_tree);
    
    krb_tree_clean(&self->evt_kws_active_tree);
    pg_db_conn_clean(&self->evt_conn);
    
    
    iter = krb_tree_iter_start(&self->cmd_kws_tree);
    size = krb_tree_size(&self->cmd_kws_tree);
    for (i = 0; i < size; i++) kcd_kws_cmd_kws_destroy(krb_tree_iter_next(&self->cmd_kws_tree, &iter));
    krb_tree_clean(&self->cmd_kws_tree);
    
    pg_db_conn_clean(&self->cmd_conn);
}

static void kcd_kws_notify_sock(int fd) {
    char a = 0;
    write(fd, &a, 1);
}

static void kcd_kws_notify_brk_thread(struct kcd_kws_state *st) {
    kcd_kws_notify_sock(st->brk_sock[0]);
}

static void kcd_kws_notify_cmd_thread(struct kcd_kws_state *st) {
    kcd_kws_notify_sock(st->cmd_sock[0]);
}

static void kcd_kws_notify_evt_thread(struct kcd_kws_state *st) {
    kcd_kws_notify_sock(st->evt_sock[0]);
}

static void kcd_kws_notify_all_thread(struct kcd_kws_state *st) {
    kcd_kws_notify_brk_thread(st);
    kcd_kws_notify_cmd_thread(st);
    kcd_kws_notify_evt_thread(st);
}

static void kcd_kws_clear_notif_sock(int fd) {
    char buf[1000];
    while (read(fd, buf, 1000) > 0) {}
}

static void kcd_kws_clear_notif_brk(struct kcd_kws_state *st) {
    kcd_kws_clear_notif_sock(st->brk_sock[1]);
}

static void kcd_kws_clear_notif_cmd(struct kcd_kws_state *st) {
    kcd_kws_clear_notif_sock(st->cmd_sock[1]);
}

static void kcd_kws_clear_notif_evt(struct kcd_kws_state *st) {
    kcd_kws_clear_notif_sock(st->evt_sock[1]);
}

/* These functions add/remove an ANP message to the incoming/outgoing message
 * queue and update quenching as needed.
 */
static void kcd_kws_push_msg_queue(struct kcd_kws_state *st, struct karray *queue, int *queue_size, int *quench,
			           struct anp_msg *msg) {
    karray_push(queue, msg);
    *queue_size += msg->payload.len + 50;
    
    if (*queue_size > KCD_KWS_MAX_CLIENT_QUEUE_SIZE && ! *quench) {
	*quench = 1;
	kcd_kws_notify_all_thread(st);
    }
}
    
static struct anp_msg * kcd_kws_pop_msg_queue(struct kcd_kws_state *st, struct karray *queue, int *queue_size,
					      int *quench) {
    assert(queue->size);
    struct anp_msg *msg = (struct anp_msg *) queue->data[0];
    int i;
    
    /* FIXME: implement a doubly-linked list. */
    for (i = 0; i < queue->size - 1; i++) {
	queue->data[i] = queue->data[i + 1];
    }
    
    queue->size--;
    *queue_size -= msg->payload.len + 50;
    
    if (*queue_size <= KCD_KWS_MAX_CLIENT_QUEUE_SIZE && *quench) {
	*quench = 0;
	kcd_kws_notify_all_thread(st);
    }
    
    return msg;
}

static void kcd_kws_push_in_msg(struct kcd_kws_state *st, struct anp_msg *msg) {
    kcd_kws_push_msg_queue(st, &st->in_msg_array, &st->in_msg_array_size, &st->in_quenched, msg);
}

static struct anp_msg * kcd_kws_pop_in_msg(struct kcd_kws_state *st) {
    return kcd_kws_pop_msg_queue(st, &st->in_msg_array, &st->in_msg_array_size, &st->in_quenched);
}

static void kcd_kws_push_out_msg(struct kcd_kws_state *st, struct anp_msg *msg) {
    kcd_kws_push_msg_queue(st, &st->out_msg_array, &st->out_msg_array_size, &st->out_quenched, msg);
}

static struct anp_msg * kcd_kws_pop_out_msg(struct kcd_kws_state *st) {
    return kcd_kws_pop_msg_queue(st, &st->out_msg_array, &st->out_msg_array_size, &st->out_quenched);
}

/* This function should be called when a client error occurs. */
static void kcd_kws_set_client_error(struct kcd_kws_state *st) {
    kmutex_lock(&st->mutex);
    
    if (! st->no_client_flag) {
    	st->no_client_flag = 1;
	kstr_assign_kstr(&st->no_client_str, kmod_kstrerror());
	kcd_kws_notify_all_thread(st);
    }
    
    kmutex_unlock(&st->mutex);
}

/* This function should be called when a backend error occurs. */
static void kcd_kws_set_backend_error(struct kcd_kws_state *st) {
    kmutex_lock(&st->mutex);
    
    if (! st->no_backend_flag && ! global_opts.quit_flag) {
    	
	/* Post an event message to inform the client that a backend error 
	 * has occurred.
	 */
	struct anp_msg *msg = anp_msg_new();
	msg->id = 0;
        msg->minor = st->client->effective_minor;
	kcd_kanp_set_failure(msg, KANP_RES_FAIL_BACKEND);
	kcd_kws_push_out_msg(st, msg);
	
    	st->no_backend_flag = 1;
	kstr_assign_kstr(&st->no_backend_str, kmod_kstrerror());
	kmod_log_msg(KCD_LOG_BRIEF, "Backend error: %s.\n", st->no_backend_str.data);
	kcd_kws_notify_all_thread(st);
    }
    
    kmutex_unlock(&st->mutex);
}

/* This function should be called to determine whether the calling thread should
 * bail out.
 */
static int kcd_kws_should_bail_out(struct kcd_kws_state *st) {
    return (global_opts.quit_flag || st->no_client_flag);
}


/******************************************************************************/
/* Broker thread functions */

/* This function pops some messages off the main outgoing message queue, and
 * adds them to the broker outgoing message queue.
 */
static void kcd_kws_brk_pop_outgoing_msg(struct kcd_kws_state *st) {
    int cur_size = 0;
    
    assert(st->out_msg_array.size);
    assert(! st->brk_out_msg_array.size);
    
    do {
	struct anp_msg *msg = kcd_kws_pop_out_msg(st);
	karray_push(&st->brk_out_msg_array, msg);
	cur_size += msg->payload.len + 50;
	
    } while (st->out_msg_array.size && cur_size < KCD_KWS_MAX_CLIENT_OUT_PACKET_SIZE);
}

/* This function checks the state of the client broker thread and performs short
 * adjustments as required in mutual exclusion.
 */
static void kcd_kws_brk_check_state_mutex(struct kcd_kws_state *st) {

    /* We are not currently receiving a new message. */
    if (! anp_tls_receiving(&st->brk_xfer)) {
    
	/* But we can receive one now, so start an incoming transfer. */
	if (! st->in_quenched) {
	    anp_tls_begin_recv(&st->brk_xfer);
	}
    }
    
    /* We are not currently sending a packet. */
    if (! anp_tls_sending(&st->brk_xfer)) {
	
	/* But there is a message to send. Pop some messages off the
	 * outgoing queue.
	 */
	if (st->out_msg_array.size) {
	    kcd_kws_brk_pop_outgoing_msg(st);
	}
    }
}

static int kcd_kws_brk_do_xfer(struct kcd_kws_state *st, int *check_state) {
    int error = 0;
    *check_state = 0;
    
    /* We are not currently sending a packet, but we have popped some messages
     * to send, so build a packet.
     */
    if (! anp_tls_sending(&st->brk_xfer) && st->brk_out_msg_array.size) {
	int i;
	
	anp_tls_send_many_msg(&st->brk_xfer, &st->brk_out_msg_array);
	
	for (i = 0; i < st->brk_out_msg_array.size; i++) {
	    anp_msg_destroy((struct anp_msg *) st->brk_out_msg_array.data[i]);
	}
	
	karray_reset(&st->brk_out_msg_array);
    }
    
    do {
	error = anp_tls_do_xfer(&st->brk_xfer, &st->client->conn);
	if (error) break;
	
	if (anp_tls_done_receiving(&st->brk_xfer)) { 
	    *check_state = 1;
	    
	    kmutex_lock(&st->mutex);
	    kcd_kws_push_in_msg(st, anp_tls_get_recv(&st->brk_xfer));
	    kcd_kws_notify_cmd_thread(st);
	    kmutex_unlock(&st->mutex);
	}
	
	if (anp_tls_done_sending(&st->brk_xfer)) {
	    *check_state = 1;
	    anp_tls_flush_send(&st->brk_xfer);
	}
    	
    } while (0);
    
    return error;
}

/* Main loop of the broker thread. */
static void kcd_kws_brk_main_loop(struct kthread *thread, struct kcd_kws_state *st) {    
    int error = 0;
    thread = NULL;
    
    kmod_log_msg(KCD_LOG_KWS, "kcd_kws_brk_main_loop() called.\n");
    
    do {
	while (1) {
	    int stop_flag = 0;
	    int check_state = 0;

	    /* Check our state in mutual exclusion. */
	    kmutex_lock(&st->mutex);

	    kcd_kws_clear_notif_brk(st);
	    stop_flag = kcd_kws_should_bail_out(st);

	    if (! stop_flag) {
		kcd_kws_brk_check_state_mutex(st);
	    }

	    kmutex_unlock(&st->mutex);

	    /* Stop. */
	    if (stop_flag) break;

	    error = kcd_kws_brk_do_xfer(st, &check_state);
	    if (error) break;
    	    
	    /* Wait for the transfers. */
	    if (! check_state) {
		struct kselect sel;
		kdaemon_prepare_select(&sel);
		kselect_add_read(&sel, st->brk_sock[1]);
		if (anp_tls_receiving(&st->brk_xfer)) kselect_add_read(&sel, st->client->sock);
		if (anp_tls_sending(&st->brk_xfer)) kselect_add_write(&sel, st->client->sock);
		error = kdaemon_do_select(&sel);
		if (error) break;
	    }
	}
	
	if (error) break;
	
    } while (0);
    
    if (error) {
    	kmod_log_msg(KCD_LOG_BRIEF, "Lost client connection: %s.\n", kmod_strerror());
    	kcd_kws_set_client_error(st);
    }
}


/******************************************************************************/
/* Event thread functions */

/* Return the workspace having the specified ID, if any. */
static struct kcd_kws_evt_kws* kcd_kws_evt_get_kws_by_id(struct kcd_kws_state *st, uint64_t id) {
    return krb_tree_get(&st->evt_kws_tree, &id);
}

/* Mark the workspace specified active. */
static void kcd_kws_evt_mark_kws_active(struct kcd_kws_state *st, struct kcd_kws_evt_kws *kws) {
    krb_tree_add(&st->evt_kws_active_tree, &kws->kws_id, kws);
}

/* Mark the workspace specified inactive. */
static void kcd_kws_evt_mark_kws_inactive(struct kcd_kws_state *st, struct kcd_kws_evt_kws *kws) {
    krb_tree_remove(&st->evt_kws_active_tree, &kws->kws_id);
}

/* Notify the command thread to check the status of the workspace specified. */
static void kcd_kws_evt_notify_check_kws(struct kcd_kws_state *st, struct kcd_kws_evt_kws *kws) {
    struct kcd_thread_msg *m = kcalloc(sizeof(struct kcd_thread_msg));
    struct kcd_thread_msg_check_kws *c = kcalloc(sizeof(struct kcd_thread_msg_check_kws));

    m->type = KCD_THREAD_MSG_CHECK_KWS;
    m->data = c;
    c->kws_id = kws->kws_id;

    kmutex_lock(&st->mutex);
    karray_push(&st->cmd_msg_array, m);
    kcd_kws_notify_cmd_thread(st);
    kmutex_unlock(&st->mutex);
}

/* This function removes the workspace having the specified ID, if any. */
static void kcd_kws_evt_remove_kws(struct kcd_kws_state *st, struct kcd_kws_evt_kws *kws) {
    kcd_kws_evt_mark_kws_inactive(st, kws);
    kcd_kws_evt_kws_destroy(krb_tree_remove(&st->evt_kws_tree, &kws->kws_id));
}

/* Return true if there is work to do in the event thread. */
static int kcd_kws_evt_has_work(struct kcd_kws_state *st) {
    return (krb_tree_size(&st->evt_kws_active_tree) > 0);
}

/* This function handles a request to listen to a workspace. */
static void kcd_kws_evt_handle_listen_request(struct kcd_kws_state *st, struct kcd_thread_msg_listen_kws *msg) {
    struct kcd_kws_evt_kws *kws = kcd_kws_evt_get_kws_by_id(st, msg->kws_id);
      
    if (!kws) {
	kws = kcd_kws_evt_kws_new();
	kws->kws_id = msg->kws_id;
	kws->poll_event_flag = 1;
	kws->last_event_id = msg->last_event_id;
	krb_tree_add(&st->evt_kws_tree, &kws->kws_id, kws);
    }
    
    kws->wanted_flag = 1;
    kcd_kws_evt_mark_kws_active(st, kws);
}

/* This function handles a request to stop listening to a workspace. */
static void kcd_kws_evt_handle_unlisten_request(struct kcd_kws_state *st, struct kcd_thread_msg_unlisten_kws *msg) {
    struct kcd_kws_evt_kws *kws = kcd_kws_evt_get_kws_by_id(st, msg->kws_id);
    
    if (kws) {
	kws->wanted_flag = 0;
        if (kws->listening_flag) kcd_kws_evt_mark_kws_active(st, kws);
        else kcd_kws_evt_remove_kws(st, kws);
    }
}

/* This function checks the state of the event thread and performs short
 * adjustments as required in mutual exclusion.
 */
static void kcd_kws_evt_check_state_mutex(struct kcd_kws_state *st) {
    int i;
    
    /* Process the messages we have received. */
    for (i = 0; i < st->evt_msg_array.size; i++) {
	struct kcd_thread_msg *thread_msg = st->evt_msg_array.data[i];
        
	if (thread_msg->type == KCD_THREAD_MSG_LISTEN_KWS)
            kcd_kws_evt_handle_listen_request(st, thread_msg->data);
	else if (thread_msg->type == KCD_THREAD_MSG_UNLISTEN_KWS)
            kcd_kws_evt_handle_unlisten_request(st, thread_msg->data);
	else assert(0);
        
        kcd_kws_state_destroy_thread_msg(thread_msg);
    }
    
    karray_reset(&st->evt_msg_array);
}

/* Listen to the workspace specified. */
static int kcd_kws_evt_listen_to_kws(struct kcd_kws_state *st, struct kcd_kws_evt_kws *kws, kstr *query) {

    kmod_log_msg(KCD_LOG_KWS, "Starting to listen to workspace " PRINTF_64"u.\n", kws->kws_id);
    
    kstr_sf(query, "LISTEN kws_"PRINTF_64"u_event_log", kws->kws_id);
    if (kcd_exec_pg_query(&st->evt_conn, query->data, NULL, "listen to workspace")) return -1;
    
    kstr_sf(query, "LISTEN kws_"PRINTF_64"u_perm_check", kws->kws_id);
    if (kcd_exec_pg_query(&st->evt_conn, query->data, NULL, "listen to workspace")) return -1;
    
    /* Mark the workspace active to poll it now that we are listening to it. */
    kws->listening_flag = 1;
    kcd_kws_evt_mark_kws_active(st, kws);
    
    /* Notify the command thread to poll the workspace. */   
    kcd_kws_evt_notify_check_kws(st, kws);
    
    return 0;
}

/* Unlisten from the workspace specified. */
static int kcd_kws_evt_unlisten_from_kws(struct kcd_kws_state *st, struct kcd_kws_evt_kws *kws, kstr *query) {
    kmod_log_msg(KCD_LOG_KWS, "Stopping to listen to workspace " PRINTF_64"u.\n", kws->kws_id);
    
    kstr_sf(query, "UNLISTEN kws_"PRINTF_64"u_event_log", kws->kws_id);
    if (kcd_exec_pg_query(&st->evt_conn, query->data, NULL, "unlisten to workspace")) return -1;
    
    kstr_sf(query, "UNLISTEN kws_"PRINTF_64"u_perm_check", kws->kws_id);
    if (kcd_exec_pg_query(&st->evt_conn, query->data, NULL, "unlisten to workspace")) return -1;
    
    /* Remove the workspace. */
    kws->listening_flag = 0;
    kcd_kws_evt_remove_kws(st, kws);
    
    return 0;
}
    
/* Poll the workspace specified for events. */
static int kcd_kws_evt_poll_kws(struct kcd_kws_state *st, struct kcd_kws_evt_kws *kws, kstr *query) {
    int error = 0, limit = 100, i, nrow;
    PGresult *pg_res = NULL;
    karray msg_array;
        
    karray_init(&msg_array);
        
    do {
        /* Get the events. */
        kstr_sf(query, "SELECT evt_id, minor, type, event FROM kcd_kws_event_log "
                       "WHERE kws_id = "PRINTF_64"u AND evt_id > " PRINTF_64"u ORDER BY evt_id LIMIT %u",
                       kws->kws_id, kws->last_event_id, limit);
        error = kcd_exec_pg_query(&st->evt_conn, query->data, &pg_res, "poll workspace");
        if (error) break;
        
        nrow = PQntuples(pg_res);
        kmod_log_msg(KCD_LOG_KWS, "Fetched %d events for workspace "PRINTF_64"u.\n", nrow, kws->kws_id);
        
        /* No failures from this point. */
        for (i = 0; i < nrow; i++) {
            struct anp_msg *msg = anp_msg_new();
            karray_push(&msg_array, msg);
            msg->id = pg_db_get_uint64(pg_res, i, 0);
            msg->minor = pg_db_get_uint32(pg_res, i, 1);
            msg->type = pg_db_get_uint32(pg_res, i, 2);
            pg_db_get_bytea(pg_res, i, 3, &msg->payload);
            kws->last_event_id = MAX(kws->last_event_id, msg->id);
        }
        
        /* Push the events to the broker thread. */
        kmutex_lock(&st->mutex);
        for (i = 0; i < nrow; i++) kcd_kws_push_out_msg(st, msg_array.data[i]);
        kcd_kws_notify_brk_thread(st);
        kmutex_unlock(&st->mutex);
        
        /* Update the poll flag and mark the workspace active if needed. */
        kws->poll_event_flag = (nrow == limit);
        if (kws->poll_event_flag) kcd_kws_evt_mark_kws_active(st, kws);
    
    } while (0);
        
    karray_clean(&msg_array);
    pg_db_destroy_res(&pg_res);
    
    return error;
}

/* This function finds a task to do and executes it. */
static int kcd_kws_evt_find_work(struct kcd_kws_state *st) {
    int error = 0;
    struct kcd_kws_evt_kws *kws = NULL;
    kstr query;
    
    kmod_log_msg(KCD_LOG_KWS, "kcd_kws_evt_find_work() called.\n");
    
    kstr_init(&query);
    
    /* Get the workspace to operate on and mark it inactive. */
    kws = krb_tree_get_by_index(&st->evt_kws_active_tree, 0);
    kcd_kws_evt_mark_kws_inactive(st, kws);
        
    /* Dispatch. */
    if (!kws->listening_flag && kws->wanted_flag) error = kcd_kws_evt_listen_to_kws(st, kws, &query);
    else if (kws->listening_flag && !kws->wanted_flag) error = kcd_kws_evt_unlisten_from_kws(st, kws, &query);
    else if (kws->poll_event_flag) error = kcd_kws_evt_poll_kws(st, kws, &query);
    
    kstr_clean(&query);
    
    return error;
}

/* Process the pending database notifications. */
static int kcd_kws_evt_process_db_notif(struct kcd_kws_state *st) {
    
    /* Allow postgres to consume its input, if any. */
    if (pg_db_consume(&st->evt_conn)) return -1;
	
    /* Process all notifications. */
    while (1) {
        int what = 0; /* 0: nothing, 1: event log, 2: perm check. */
        PGnotify *notif = pg_db_notify_check(&st->evt_conn);
        if (!notif) return 0;

        if (strstr(notif->relname, "_event_log")) what = 1;
        else if (strstr(notif->relname, "_perm_check")) what = 2;

        if (what) {
            uint64_t kws_id = strtoll(notif->relname + 4, NULL, 10);
            struct kcd_kws_evt_kws *kws = kcd_kws_evt_get_kws_by_id(st, kws_id);
            
            if (kws) {

                /* Event log. */
                if (what == 1) {
                    kmod_log_msg(KCD_LOG_KWS, "Got event notification for workspace "PRINTF_64"u.\n", kws_id);
                    kws->poll_event_flag = 1;
                    kcd_kws_evt_mark_kws_active(st, kws);
                }

                /* Permission check. */
                else {
                    kmod_log_msg(KCD_LOG_KWS, "Got perm check notification for workspace "PRINTF_64"u.\n",
                                 kws_id);
                    kcd_kws_evt_notify_check_kws(st, kws);
                }
            }
        }

        PQfreemem(notif);
    }
}

/* Main loop of the event thread. */
static void kcd_kws_evt_main_loop(struct kthread *thread, struct kcd_kws_state *st) {
    int error = 0;
    thread = NULL;
    kstr conn_str;

    kstr_init(&conn_str);
    kmod_log_msg(KCD_LOG_KWS, "kcd_kws_evt_main_loop() called.\n");
    
    do {
        kstr_sf(&conn_str, "dbname=%s user=%s password=%s host=%s port=%s", 
                global_opts.db_name.data, global_opts.db_user.data,
                global_opts.db_password.data, global_opts.db_host.data,
                global_opts.db_port.data);

	error = kcd_open_pg_conn(&st->evt_conn, conn_str.data);
	if (error) break;

	while (1) {
	    int stop_flag = 0;

	    /* Check our state in mutual exclusion. */
	    kmutex_lock(&st->mutex);

	    kcd_kws_clear_notif_evt(st);
	    stop_flag = kcd_kws_should_bail_out(st);
	    if (!stop_flag) kcd_kws_evt_check_state_mutex(st);

	    kmutex_unlock(&st->mutex);

	    /* Stop. */
	    if (stop_flag) break;
            
            /* Execute work. */
            if (kcd_kws_evt_has_work(st)) {
		error = kcd_kws_evt_find_work(st);
		if (error) break;
            }

	    /* Process the pending database notifications. */
            error = kcd_kws_evt_process_db_notif(st);
            if (error) break;
            
            /* Sleep until something happens. */
            if (!kcd_kws_evt_has_work(st)) {
                struct kselect sel;
                kdaemon_prepare_select(&sel);
                kselect_add_read(&sel, st->evt_sock[1]);
                kselect_add_read(&sel, st->evt_conn.sock);
                error = kdaemon_do_select(&sel);
                if (error) break;
            }
	}
	
	if (error) break;
    
    } while (0);
    
    kstr_clean(&conn_str);

    if (error) kcd_kws_set_backend_error(st);
}


/******************************************************************************/
/* Command thread functions */
        
/* Return the workspace having the specified ID, if any. */
struct kcd_kws_cmd_kws* kcd_kws_cmd_get_kws_by_id(krb_tree *kws_tree, uint64_t id) {
    return krb_tree_get(kws_tree, &id);
}

/* Add a workspace to the command workspace set of the client. */
void kcd_kws_cmd_add_kws(struct kcd_kws_state *st, struct kcd_kws_cmd_kws *kws, uint64_t last_event_id) {
    krb_tree_add_fast(&st->cmd_kws_tree, &kws->kws_id, kws);

    if (kws->login_type != KCD_KWS_LOGIN_TYPE_KWMO) {
        struct kcd_thread_msg *m = kcalloc(sizeof(struct kcd_thread_msg));
        struct kcd_thread_msg_listen_kws *l = kcalloc(sizeof(struct kcd_thread_msg_listen_kws));
        
        m->type = KCD_THREAD_MSG_LISTEN_KWS;
        m->data = l;
        l->kws_id = kws->kws_id;
        l->user_id = kws->user_id;
        l->last_event_id = last_event_id;
        
        kmutex_lock(&st->mutex);
        karray_push(&st->evt_msg_array, m);
        kcd_kws_notify_evt_thread(st);
        kmutex_unlock(&st->mutex);
    }
}
    
/* Remove a workspace from the command workspace set of the client. */
void kcd_kws_cmd_remove_kws(struct kcd_kws_state *st, struct kcd_kws_cmd_kws *kws) {
    struct kcd_thread_msg *m = kcalloc(sizeof(struct kcd_thread_msg));
    struct kcd_thread_msg_unlisten_kws *l = kcalloc(sizeof(struct kcd_thread_msg_unlisten_kws));

    m->type = KCD_THREAD_MSG_UNLISTEN_KWS;
    m->data = l;
    l->kws_id = kws->kws_id;

    kmutex_lock(&st->mutex);
    karray_push(&st->evt_msg_array, m);
    kcd_kws_notify_evt_thread(st);
    kmutex_unlock(&st->mutex);

    kcd_kws_cmd_kws_destroy(krb_tree_remove(&st->cmd_kws_tree, &kws->kws_id));
}

/* Prepare and execute a workspace-bound query in Postgres. */
int kcd_kws_cmd_kws_bound_query(struct kcd_kws_cmd_exec_state *ces, char *query_name) {
    return kcd_exec_kws_bound_query(ces->conn, &ces->aq, query_name, ces->cmd, ces->res, ces->kws->kws_id, ces->date,
                                    ces->kws->login_type, ces->kws->user_id, ces->client->effective_minor,
                                    &ces->kws_bound_buf);
}

/* Generate a ticket for reconnecting to KCD in a special mode and insert the
 * ticket in the database.
 */
int kcd_kws_cmd_create_kcd_ticket(struct kcd_kws_cmd_exec_state *ces, struct kcd_internal_ticket *ticket) {
    int error = 0;
    char rnd[16];
    kstr query;
    kbuffer nonce;
    kbuffer *payload = &ticket->payload;
    
    kmod_log_msg(KCD_LOG_KWS,  "kcd_create_kcd_ticket() called.\n");
    
    kstr_init(&query);
    kbuffer_init(&nonce);
    
    do {
        /* Generate a random nonce. */
        error = kutil_generate_random(rnd, 16);
	if (error) break;
	kbuffer_write(&nonce, rnd, 16);
        
	/* Generate the ticket. */
	kbuffer_reset(payload);
        anp_write_uint32(payload, ticket->type);
        anp_write_uint64(payload, ces->kws->kws_id);
        anp_write_uint32(payload, ces->kws->login_type);
        anp_write_uint32(payload, ces->kws->user_id);
        anp_write_bin(payload, &ticket->ext);
        anp_write_bin(payload, &nonce);
	
	/* Write the ticket in the database. */
	kstr_sf(&query, "INSERT INTO kcd_ticket (creation_date, ticket) VALUES (");
	pg_db_add_uint64(&query, ktime_now_sec());
	kstr_append_cstr(&query, ", ");
	pg_db_add_bytea(ces->conn, &query, payload);
	kstr_append_cstr(&query, ")");
	error = kcd_exec_pg_query(ces->conn, query.data, NULL, "insert KCD ticket");
	if (error) break;
	
    } while (0);
    
    kstr_clean(&query);
    kbuffer_clean(&nonce);
    
    return error;
}

/* Obtain the workspace specified by the user in the command and validate that
 * the user is logged to that workspace. The command buffer is advanced to the
 * parameter following the workspace ID.
 */
static int kcd_kws_cmd_validate_kws(struct kcd_kws_cmd_exec_state *ces) {
    int error = 0;
    uint64_t kws_id;

    do {
        /* Get the workspace ID */
        if (anp_read_uint64(&ces->cmd->payload, &kws_id)) {
            error = -2;
            break;
        }
        
        /* Get the workspace. */
        ces->kws = kcd_kws_cmd_get_kws_by_id(ces->kws_tree, kws_id);
        if (!ces->kws) {
            kmod_set_error("not logged to "KCD_KWS_NAME" "PRINTF_64"u", kws_id);
            kcd_kanp_set_failure(ces->res, KANP_RES_FAIL_PERM_DENIED);
            error = -3;
            break;
        }
        
    } while (0);

    return error;
}

/* Execute a command from the client. */
static int kcd_kws_cmd_exec_cmd(struct kcd_kws_state *st, krb_tree *dispatch_tree, struct anp_msg *cmd) {
    int error = 0;
    struct kcd_kws_cmd_exec_state ces;
    struct anp_msg *res = anp_msg_new();
    
    kcd_kws_cmd_exec_state_init(&ces);
    ces.date = ktime_now_sec();
    ces.cmd = cmd;
    ces.res = res;
    ces.kws_tree = &st->cmd_kws_tree;
    ces.conn = &st->cmd_conn;
    ces.st = st;
    ces.client = st->client;
    	
    res->minor = st->client->effective_minor;
    res->id = cmd->id;
    res->type = KANP_RES_OK;
    
    kcd_log_kanp_msg(KCD_LOG_BRIEF, 1, cmd);
    
    do {
	/* Locate the dispatch entry, if any. */
        struct kcd_kws_cmd_dispatch_entry *entry = krb_tree_get(dispatch_tree, &cmd->type);
        
        /* No such command. */
        if (!entry) {
            kmod_set_error("command %u is not supported", cmd->type);
            error = -2;
            break;
        }
        
        kmod_log_msg(KCD_LOG_BRIEF, "Executing \"%s\" command.\n", entry->name);
        
        /* If this is a workspace-bound query, validate the workspace ID. */
        if (entry->kws_bound_flag) {
            error = kcd_kws_cmd_validate_kws(&ces);
            if (error) break;
        }
        
        /* Dispatch. */
        error = entry->handler(&ces);
        if (error) break;
    
    } while (0);
    
    /* Handle the generic failure. */
    if (error == -2) {
        kcd_kanp_set_gen_failure(res);
        error = 0;
    }
    
    /* A specifiec error result has been set. Ignore the error. */
    else if (error == -3) {
        error = 0;
    }
    
    assert(error == 0 || error == -1);
    
    /* A result has been obtained. */
    if (!error) {
        kcd_log_kanp_msg(KCD_LOG_BRIEF, 0, res);
	
	kmutex_lock(&st->mutex);
    	kcd_kws_push_out_msg(st, res);
	res = NULL;
    	kcd_kws_notify_brk_thread(st);
	kmutex_unlock(&st->mutex);
    }
    
    kcd_kws_cmd_exec_state_clean(&ces);
    anp_msg_destroy(res);
    
    return error;
}

/* Validate that the user can log in the workspace specified. */
static int kcd_kws_cmd_check_kws(struct kcd_kws_state *st, struct kcd_kws_cmd_kws *kws) {
    int error = 0;
    uint32_t query_res;
    uint32_t login_code;
    kstr error_str;
    struct kcd_pg_anp_query aq;
    kbuffer *in_buf = &aq.input_buf, *out_buf = &aq.output_buf;
    
    kmod_log_msg(KCD_LOG_KWS, "kcd_kws_cmd_check_kws() called.\n");
    
    kstr_init(&error_str);
    kcd_pg_anp_query_init(&aq);
    
    do {
        anp_write_uint64(in_buf, kws->kws_id);
        anp_write_uint32(in_buf, kws->login_type);
        anp_write_uint32(in_buf, kws->user_id);
        error = kcd_exec_safe_pg_anp_query(&st->cmd_conn, &aq, "check_kws_login");
        if (error) break;
        
        if (anp_read_uint32(out_buf, &query_res) ||
            anp_read_uint32(out_buf, &login_code) ||
            anp_read_kstr(out_buf, &error_str)) {
            error = -1;
            break;
        }
        
        /* The client cannot log in the workspace anymore. */
        if (query_res) {
            
            /* Notify the client if its minor is above 3. */
            if (st->client->effective_minor > 3) {
                struct anp_msg *evt = anp_msg_new();
                evt->minor = 4;
                evt->type = KANP_EVT_KWS_LOG_OUT;
                anp_write_uint64(&evt->payload, kws->kws_id);
                anp_write_uint64(&evt->payload, ktime_now_sec());
                anp_write_uint32(&evt->payload, login_code);
                anp_write_kstr(&evt->payload, &error_str);

                kmutex_lock(&st->mutex);
                kcd_kws_push_out_msg(st, evt);
                kcd_kws_notify_brk_thread(st);
                kmutex_unlock(&st->mutex);
            }
            
            /* Remove the workspace from the command workspace set. */
            kcd_kws_cmd_remove_kws(st, kws);
        }
            
    } while (0);
    
    kcd_pg_anp_query_clean(&aq);
    kstr_clean(&error_str);
    
    return error;
}

/* Main loop of the command thread. */
static void kcd_kws_cmd_main_loop(struct kthread *thread, struct kcd_kws_state *st) {
    int error = 0, i;
    krb_tree dispatch_tree;
    karray thread_msg_array;
    struct anp_msg *cmd = NULL;
    kstr conn_str;
    thread = NULL;
    
    krb_tree_init_func(&dispatch_tree, kutil_uint32_cmp);
    karray_init(&thread_msg_array);
    kstr_init(&conn_str);
    
    kmod_log_msg(KCD_LOG_KWS, "kcd_kws_cmd_main_loop() called.\n");
    
    /* Populate the dispatch tree. */
    for (i = 0; (uint32_t)i < sizeof(kcd_kws_cmd_dispatch_table)/sizeof(struct kcd_kws_cmd_dispatch_entry); i++) {
        struct kcd_kws_cmd_dispatch_entry *entry = kcd_kws_cmd_dispatch_table + i;
        krb_tree_add_fast(&dispatch_tree, &entry->type, entry);
    }
    
    do {
        kstr_sf(&conn_str, "dbname=%s user=%s password=%s host=%s port=%s", 
                global_opts.db_name.data, global_opts.db_user.data,
                global_opts.db_password.data, global_opts.db_host.data,
                global_opts.db_port.data);

	error = kcd_open_pg_conn(&st->cmd_conn, conn_str.data);
	if (error) break;

	while (1) {
    	    int stop_flag = 0;
    	    int select_flag = 1;
    	    
	    /* Check our state in mutual exclusion. */
	    kmutex_lock(&st->mutex);

	    kcd_kws_clear_notif_cmd(st);
	    stop_flag = kcd_kws_should_bail_out(st);

	    if (!stop_flag) {
            
                /* Get the thread messages to process. */
                for (i = 0; i < st->cmd_msg_array.size; i++) karray_push(&thread_msg_array, st->cmd_msg_array.data[i]);
                karray_reset(&st->cmd_msg_array);
                
                /* Get the next command to process. */
		if (!st->in_quenched && st->in_msg_array.size) {
                    cmd = kcd_kws_pop_in_msg(st);
		    select_flag = 0;
		}
	    }

	    kmutex_unlock(&st->mutex);

	    /* Stop. */
	    if (stop_flag) break;
            
            /* Process the check-workspace thread messages. */
            for (i = 0; i < thread_msg_array.size; i++) {
                struct kcd_thread_msg *msg = thread_msg_array.data[i];
                struct kcd_kws_cmd_kws *kws;
                uint64_t kws_id;
                
                assert(msg->type == KCD_THREAD_MSG_CHECK_KWS);
                kws_id = ((struct kcd_thread_msg_check_kws*)msg->data)->kws_id;
                kws = kcd_kws_cmd_get_kws_by_id(&st->cmd_kws_tree, kws_id);
                
                if (kws) {
                    error = kcd_kws_cmd_check_kws(st, kws);
                    if (error) break;
                }
            }
            
            if (error) break;
            
            kcd_kws_clear_thread_msg_array(&thread_msg_array, 0);

	    /* We have a command to process. */
	    if (cmd) {
                error = kcd_kws_cmd_exec_cmd(st, &dispatch_tree, cmd);
		if (error) break;
                
                anp_msg_destroy(cmd);
                cmd = NULL;
	    }
	    
	    if (select_flag) {
	    	struct kselect sel;
		kdaemon_prepare_select(&sel);
	    	kselect_add_read(&sel, st->cmd_sock[1]);
	    	error = kdaemon_do_select(&sel);
		if (error) break;
	    }
	}
	
	if (error) break;
    
    } while (0);
    
    krb_tree_clean(&dispatch_tree);
    kcd_kws_clear_thread_msg_array(&thread_msg_array, 1);
    anp_msg_destroy(cmd);
    kstr_clean(&conn_str);
    
    if (error) kcd_kws_set_backend_error(st);
}


/******************************************************************************/
/* KANP workspace mode connection handler. */

int kcd_kws_handle_conn(struct kcd_client *client) {
    struct kthread brk_thread, evt_thread, cmd_thread;
    struct kcd_kws_state st;
    
    kdaemon_set_task("Workspace | %s", client->addr.data);
    kmod_log_msg(KCD_LOG_BRIEF, "kcd_kws_handle_conn() called.\n");
    
    kcd_kws_state_init(&st);
    st.client = client;
    
    kthread_init(&brk_thread);
    kthread_init(&evt_thread);
    kthread_init(&cmd_thread);
    
    kthread_start(&brk_thread, (void (*)(struct kthread *, void *)) kcd_kws_brk_main_loop, &st);
    kthread_start(&evt_thread, (void (*)(struct kthread *, void *)) kcd_kws_evt_main_loop, &st);
    kthread_start(&cmd_thread, (void (*)(struct kthread *, void *)) kcd_kws_cmd_main_loop, &st);
    
    kthread_join(&brk_thread);
    kthread_join(&evt_thread);
    kthread_join(&cmd_thread);
    
    kthread_clean(&brk_thread);
    kthread_clean(&evt_thread);
    kthread_clean(&cmd_thread);
    
    kcd_kws_state_clean(&st);
    
    kmod_log_msg(KCD_LOG_BRIEF, "kcd_kws_handle_conn(): exiting.\n");
    
    return 0;
}

