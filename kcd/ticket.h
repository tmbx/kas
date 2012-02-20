/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#ifndef _TICKET_H
#define _TICKET_H

/* State used when the user reconnects to the KCD using a ticket. */
struct kcd_ticket_mode_state {
    
    /* Logging level. */
    int log_level;
    
    /* Workspace ID. */
    uint64_t kws_id;
    
    /* Login type. */
    uint32_t login_type;
    
    /* ID of the user in the workspace. */
    uint32_t user_id;
    
    /* Pointer to the client to service. Read-only. */
    struct kcd_client *client;
    
    /* ANP message transfer with the client. */
    struct anp_tls_xfer xfer;
    
    /* Connection to the database. */
    struct pg_db_conn db_conn;
    
    /* Select state. */
    struct kselect sel;
    
    /* Ticket received from the client. */
    struct kcd_internal_ticket ticket;
    
    /* Last ANP message received from the client, if any. */
    struct anp_msg *in_msg;
    
    /* ANP message to send to the client. */
    struct anp_msg *out_msg;
    
    /* Query string for generic Postgres queries. */
    kstr query;
    
    /* Postgres ANP query. */
    struct kcd_pg_anp_query aq;
    
    /* Extra argument buffer for workspace-bound queries. This buffer is reset
     * every time a workspace-bound query is executed.
     */
    kbuffer kws_bound_buf;
    
    /* Email address of the user to which the workspace is licensed. This is
     * empty if the user cannot be found.
     */
    kstr license_email;
    
    /* Resource usage and license information. */
    struct kcd_global_user_usage_info usage_info;
    struct kcd_global_user_license_info license_info;
};

/* Dispatch table entry for the ticket modes. */
struct kcd_ticket_mode_dispatch_entry {

    /* Ticket request command type. */
    uint32_t cmd_type;
    
    /* Result type of the ticket request. */
    uint32_t res_type;
    
    /* Type of the first message to receive in ticket mode. */
    uint32_t first_msg_type;
    
    /* Ticket type. */
    uint32_t ticket_type;
    
    /* Command handler. */
    int (*handler)(struct kcd_ticket_mode_state *);
};

void kcd_ticket_mode_state_init(struct kcd_ticket_mode_state *self, struct kcd_client *client);
void kcd_ticket_mode_state_clean(struct kcd_ticket_mode_state *self);
struct kcd_ticket_mode_dispatch_entry* kcd_ticket_mode_get_dispatch_entry(uint32_t type);
void kcd_ticket_mode_clear_in_msg(struct kcd_ticket_mode_state *self);
void kcd_ticket_mode_clear_out_msg(struct kcd_ticket_mode_state *self);
kbuffer* kcd_ticket_mode_new_out_msg(struct kcd_ticket_mode_state *self, uint32_t type);
int kcd_ticket_mode_set_failure(struct kcd_ticket_mode_state *self, uint32_t error_type);
kbuffer* kcd_ticket_mode_failure(struct kcd_ticket_mode_state *self);
int kcd_ticket_mode_kws_bound_query(struct kcd_ticket_mode_state *self, char *query_name,
                                    uint64_t date, struct anp_msg *cmd);
int kcd_ticket_mode_do_perm_check(struct kcd_ticket_mode_state *self);
int kcd_ticket_mode_process_notif(struct kcd_ticket_mode_state *self);
void kcd_ticket_mode_prepare_wait(struct kcd_ticket_mode_state *self);
int kcd_ticket_mode_wait(struct kcd_ticket_mode_state *self, int64_t delay);
int kcd_ticket_mode_timed_tls_xfer(struct kcd_ticket_mode_state *self, int64_t delay);
int kcd_ticket_mode_send_msg(struct kcd_ticket_mode_state *self);
int kcd_ticket_mode_recv_msg(struct kcd_ticket_mode_state *self);
int kcd_ticket_mode_timed_recv_msg(struct kcd_ticket_mode_state *self, int64_t delay);
int kcd_ticket_mode_get_license_email(struct kcd_ticket_mode_state *self);
int kcd_ticket_mode_get_usage_and_license_info(struct kcd_ticket_mode_state *self);
int kcd_ticket_mode_handle_user_error(struct kcd_ticket_mode_state *self, int r);
int kcd_ticket_mode_validate_ticket(struct kcd_ticket_mode_state *self, uint32_t expected_type);
int kcd_ticket_mode_handle_conn(struct kcd_client *client, uint32_t app);

#endif

