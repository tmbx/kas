/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#ifndef _KWS_H
#define _KWS_H

/* This structure represents a message exchanged between KCD threads. */
struct kcd_thread_msg {
    
    /* Type of the message. */
    int type;
    
    /* Data of the message. */
    void *data;
};

/* This message is sent to the event thread to make it listen to the events of a
 * workspace.
 */
struct kcd_thread_msg_listen_kws {
    
    /* ID of the workspace. */
    uint64_t kws_id;
    
    /* ID of the user in the workspace. */
    uint32_t user_id;
    
    /* ID of the last event retrieved from the workspace's event log. */
    uint64_t last_event_id;
};

/* This message is sent to the event thread to make it stop listening to the
 * events of a workspace.
 */
struct kcd_thread_msg_unlisten_kws {

    /* ID of the workspace. */
    uint64_t kws_id;
};

/* This message is sent to the command thread to make it verify the status of
 * the workspace specified.
 */
struct kcd_thread_msg_check_kws {
    
    /* ID of the workspace. */
    uint64_t kws_id;
};

enum {
    KCD_THREAD_MSG_LISTEN_KWS = 1,
    KCD_THREAD_MSG_UNLISTEN_KWS,
    KCD_THREAD_MSG_CHECK_KWS,
};

/* This structure represents a workspace a client is logged in as seen by the
 * event thread.
 */
struct kcd_kws_evt_kws {

    /* ID of the workspace. */
    uint64_t kws_id;
    
    /* True if we still want to listen to that workspace. */
    int wanted_flag;
    
    /* True if we are listening to the workspace. */
    int listening_flag;
    
    /* True if the database should be polled for new events ASAP. */
    int poll_event_flag;
    
    /* ID of the last event retrieved from the workspace's event log. */
    uint64_t last_event_id;
};

/* Represent a workspace to which the user is logged in as seen by the command
 * thread in the KANP workspace mode.
 */
struct kcd_kws_cmd_kws {
    
    /* Workspace ID. */
    uint64_t kws_id;
    
    /* Login type. */
    uint32_t login_type;
    
    /* ID of the user in the workspace. */
    uint32_t user_id;
};

/* Represent a KCD ticket. */
struct kcd_internal_ticket {
    
    /* Ticket type. */
    uint32_t type;
    
    /* Formatted ticket payload. */
    kbuffer payload;
    
    /* Ticket extra data. */
    kbuffer ext;
};

/* Contain the state of a command to execute in the KANP workspace mode. */
struct kcd_kws_cmd_exec_state {
    
    /* Command date. */
    uint64_t date;
    
    /* Command to process. */
    struct anp_msg *cmd;
    
    /* Result of the command to process. */
    struct anp_msg *res;

    /* Tree of workspaces (kcd_kws_cmd_kws) the user is logged to indexed by
     * workspace ID.
     */
    krb_tree *kws_tree;
    
    /* Workspace associated to the command, if any. */
    struct kcd_kws_cmd_kws *kws;
    
    /* Postgres connection. */
    struct pg_db_conn *conn;
    
    /* Query string for generic Postgres queries. */
    kstr query;
    
    /* Postgres ANP query. */
    struct kcd_pg_anp_query aq;
    
    /* Extra argument buffer for workspace-bound queries. This buffer is reset
     * every time a workspace-bound query is executed.
     */
    kbuffer kws_bound_buf;
    
    /* Pointer to the KANP workspace mode state. */
    struct kcd_kws_state *st;
    
    /* Pointer to the client to service. Read-only. */
    struct kcd_client *client;
};

/* This structure contains the data required to service a client in workspace
 * mode.
 */
struct kcd_kws_state {
    
    /* Pointer to the client to service. Read-only. */
    struct kcd_client *client;
    
    /* Notifier socket arrays. This is used to wake up waiting threads. The
     * first socket must be written to, the second must be read from.
     */
    int brk_sock[2];
    int cmd_sock[2];
    int evt_sock[2];
    
    
    /* Mutex protecting the following fields in this structure. */
    struct kmutex mutex;
    
    /* True if the backend encountered an error. Protected by mutex. */
    int no_backend_flag;
    
    /* Backend error string. Protected by mutex. */
    kstr no_backend_str;
    
    /* True if we lost the connection with the client. Protected by mutex. */
    int no_client_flag;
    
    /* Client error string. Protected by mutex. */    
    kstr no_client_str;
    
    /* Array of ANP messages received. Protected by mutex. The command thread
     * must be notified when a new message is received, so that it can process
     * it.
     */
    karray in_msg_array;
    
    /* Total size of the data in the array above. Protected by mutex. */
    int in_msg_array_size;
    
    /* Array of ANP messages to send. Protected by mutex. The broker thread must
     * be notified when a new message is added to this queue, so that it can
     * send it.
     */
    karray out_msg_array;
    
    /* Total size of the data in the array above. Protected by mutex. */
    int out_msg_array_size;
    
    /* True if the incoming message queue is quenched because it is full. In
     * that case, the broker thread will stop receiving messages from the
     * client. Protected by mutex. Whenever this status changes, all threads
     * must be notified.
     */
    int in_quenched; 
    
    /* True if the outgoing message queue is quenched because it is full. In
     * that case, the command and event threads should refrain from posting
     * further messages in the queue. Protected by mutex. Whenever this status
     * changes, all threads must be notified.
     */
    int out_quenched;
    
    /* Array of thread messages to be processed by the event thread. Protected
     * by mutex.
     */
    karray evt_msg_array;
    
    /* Array of thread messages to be processed by the command thread. Protected
     * by mutex.
     */
    karray cmd_msg_array;
    
    
    /* Data specific to the broker thread. */
    
    /* Array containing the messages to send to the client in the next packet. */
    karray brk_out_msg_array;
    
    /* ANP message transfer with the client. */
    struct anp_tls_xfer brk_xfer;
    
    
    /* Data specific to the event thread. */
    
    /* Tree of workspaces the user is connected to. */
    krb_tree evt_kws_tree;
    
    /* Tree of workspaces for which a query must be done. Memory not owned by
     * this object.
     */
    krb_tree evt_kws_active_tree;
    
    /* Connection to the database used by the event thread. */
    struct pg_db_conn evt_conn;
    
    
    /* Data specific to the command thread. */
    
    /* Tree of workspaces (kcd_kws_cmd_kws) the user is logged to indexed by
     * workspace ID.
     */
    struct krb_tree cmd_kws_tree;
    
    /* Connection to the database used by the command thread. */
    struct pg_db_conn cmd_conn;
};

struct kcd_kws_cmd_kws* kcd_kws_cmd_kws_new();
void kcd_kws_cmd_kws_destroy(struct kcd_kws_cmd_kws *self);
void kcd_internal_ticket_init(struct kcd_internal_ticket *self);
void kcd_internal_ticket_clean(struct kcd_internal_ticket *self);
struct kcd_kws_cmd_kws* kcd_kws_cmd_get_kws_by_id(krb_tree *kws_tree, uint64_t id);
void kcd_kws_cmd_add_kws(struct kcd_kws_state *st, struct kcd_kws_cmd_kws *kws, uint64_t last_event_id);
void kcd_kws_cmd_remove_kws(struct kcd_kws_state *st, struct kcd_kws_cmd_kws *kws);
int kcd_kws_cmd_kws_bound_query(struct kcd_kws_cmd_exec_state *ces, char *query_name);
int kcd_kws_cmd_create_kcd_ticket(struct kcd_kws_cmd_exec_state *ces, struct kcd_internal_ticket *ticket);
int kcd_kws_handle_conn(struct kcd_client *client);

#endif

