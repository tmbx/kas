/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _ANP_TLS_H
#define _ANP_TLS_H

/* This structure is used to transfer ANP messages with a remote host. */
struct anp_tls_xfer {
    
    /* State of the incoming message.
     * 0: no incoming message.
     * 1: message header being received.
     * 2: message payload being received.
     * 3: incoming message received.
     */
    int in_state;
    
    /* Size of the payload. */
    uint32_t payload_size;
    
    /* Buffer containing the data being received, if any. */
    kbuffer in_buf;
    
    /* Pointer to the message received, if any. */
    struct anp_msg *in_msg;
    
    /* State of the outgoing packet.
     * 0: no outgoing packet.
     * 1: packet being sent.
     * 2: packet sent.
     */
    int out_state;
    
    /* Buffer containing the data of the packet being sent, if any. */
    kbuffer out_buf;
};

static inline int anp_tls_receiving(struct anp_tls_xfer *self) { return (self->in_state > 0); }
static inline int anp_tls_done_receiving(struct anp_tls_xfer *self) { return (self->in_state == 3); }
static inline int anp_tls_sending(struct anp_tls_xfer *self) { return (self->out_state == 1); }
static inline int anp_tls_done_sending(struct anp_tls_xfer *self) { return (self->out_state == 2); }

void anp_tls_init(struct anp_tls_xfer *self);
void anp_tls_clean(struct anp_tls_xfer *self);
void anp_tls_reset(struct anp_tls_xfer *self);
void anp_tls_add_id_buf(struct anp_tls_xfer *self, char *id_buf);
void anp_tls_begin_recv(struct anp_tls_xfer *self);
struct anp_msg * anp_tls_get_recv(struct anp_tls_xfer *self);
void anp_tls_flush_recv(struct anp_tls_xfer *self);
void anp_tls_send_msg(struct anp_tls_xfer *self, struct anp_msg *msg);
void anp_tls_send_many_msg(struct anp_tls_xfer *self, karray *msg_array);
void anp_tls_flush_send(struct anp_tls_xfer *self);
int anp_tls_has_xfer(struct anp_tls_xfer *self);
int anp_tls_do_xfer(struct anp_tls_xfer *self, struct ktls_conn *conn);
void anp_tls_prepare_select(struct anp_tls_xfer *self, struct ktls_conn *conn, struct kselect *sel);

#endif

