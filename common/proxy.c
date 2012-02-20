/* Copyright (C) 2008-2012 Opersys inc., All rights reserved. */

#include "common.h"

void kproxy_end_init(struct kproxy_end *self, int sock, struct ktls_conn *tls, char *desc, int size) {
    memset(self, 0, sizeof(struct kproxy_end));
    self->sock = sock;
    self->tls = tls;
    self->desc = desc;
    kbuffer_init(&self->buf);
    kbuffer_grow(&self->buf, size);
}

void kproxy_end_clean(struct kproxy_end *self) {
    kbuffer_clean(&self->buf);
}

/* This function opens a TCP connection with the host and port specified. */
int kproxy_connect_tcp(int *sock, char *host, int port) {
    int error = 0;
    struct kselect sel;
    
    kmod_log_msg(KCD_LOG_MISC, "kproxy_connect_tcp() called.\n");
    
    do {
	error = ksock_create(sock);
	if (error) break;

	error = ksock_set_unblocking(*sock);
	if (error) break;

	error = ksock_connect(*sock, host, port);
	if (error) break;
	
        while (1) {
            kdaemon_prepare_select(&sel);
            kselect_add_write(&sel, *sock);
            error = kdaemon_do_select(&sel);
            if (error) break;
            
            if (kselect_in_write(&sel, *sock)) {
                error = ksock_connect_check(*sock, host);
                break;
            }
        }
        
        if (error) break;
	
    } while (0);
    
    return error;
}

/* Helper function for kproxy_read() and kproxy_write(). */
static void kproxy_handle_loss(struct kproxy_end *e, char *op) {
    e->lost = 1;
    kmod_log_msg(KCD_LOG_MISC, "kproxy: %s %s error: %s.\n", op, e->desc, kmod_strerror());
}

/* Helper function for kproxy_loop(). */
static void kproxy_read(struct kproxy_end *e, int *moved) {
    kmod_log_msg(KCD_LOG_MISC, "kproxy_read(): about to read from %s.\n", e->desc);
    
    if (e->tls) {
	int r = ktls_recv(e->tls, e->buf.data, e->buf.allocated);
        if (r != -2) *moved = 1;
	if (r == -1) kproxy_handle_loss(e, "read from");
	if (r >= 0) {
	    kmod_log_msg(KCD_LOG_MISC, "kproxy_read(): read %d bytes from %s.\n", r, e->desc);
	    e->buf.len = r;
	}
    }
    
    else {
	uint32_t len = e->buf.allocated;
	int r = ksock_read(e->sock, e->buf.data, &len);
        if (r != -2) *moved = 1;
	if (r == -1) kproxy_handle_loss(e, "read from");
	if (r == 0) {
	    kmod_log_msg(KCD_LOG_MISC, "kproxy_read(): read %d bytes from %s.\n", len, e->desc);
	    e->buf.len = len;
	}
    }
}

/* Helper function for kproxy_loop(). */
static void kproxy_write(struct kproxy_end *e, kbuffer *buf, int *moved) {
    kmod_log_msg(KCD_LOG_MISC, "kproxy_write(): about to write to %s.\n", e->desc);
    
    if (e->tls) {
	int r = ktls_send(e->tls, buf->data + buf->pos, buf->len - buf->pos);
        if (r != -2) *moved = 1;
	if (r == -1) kproxy_handle_loss(e, "write to");
	if (r >= 0) {
	    kmod_log_msg(KCD_LOG_MISC, "kproxy_write(): wrote %d bytes to %s.\n", r, e->desc);
	    buf->pos += r;
	    if (buf->pos == buf->len) buf->pos = buf->len = 0;
	}
    }
    
    else {
	uint32_t len = buf->len - buf->pos;
	int r = ksock_write(e->sock, buf->data + buf->pos, &len);
        if (r != -2) *moved = 1;
	if (r == -1) kproxy_handle_loss(e, "write to");
	if (r == 0) {
	    kmod_log_msg(KCD_LOG_MISC, "kproxy_write(): wrote %d bytes to %s.\n", len, e->desc);
	    buf->pos += len;
	    if (buf->pos == buf->len) buf->pos = buf->len = 0;
	}
    }
}

/* Return true if the transfers are finished. */
int kproxy_is_finished(struct kproxy_end *e1, struct kproxy_end *e2) {
    return (e1->lost || e2->lost) && ((e1->lost || !e2->buf.len) && (e2->lost || !e1->buf.len));
}

/* Perform one round of transfers. move_flag is set to true if a transfer
 * occurred. move_flag can be NULL.
 */
void kproxy_do_xfer(struct kproxy_end *e1, struct kproxy_end *e2, int *move_flag) {
    int m = 0;
    if (!e1->lost && !e1->buf.len) kproxy_read(e1, &m);
    if (!e2->lost && e1->buf.len) kproxy_write(e2, &e1->buf, &m);
    if (!e2->lost && !e2->buf.len) kproxy_read(e2, &m);
    if (!e1->lost && e2->buf.len) kproxy_write(e1, &e2->buf, &m);
    if (move_flag) *move_flag = m;
}

/* Prepare the call to select by adding the sockets in the select set specified. */
void kproxy_prepare_select(struct kselect *sel, struct kproxy_end *e1, struct kproxy_end *e2) {
    if (!e2->lost && e1->buf.len) kselect_add_write(sel, e2->sock);
    if (!e1->lost && !e1->buf.len) kselect_add_read(sel, e1->sock);
    if (!e1->lost && e2->buf.len) kselect_add_write(sel, e1->sock);
    if (!e2->lost && !e2->buf.len) kselect_add_read(sel, e2->sock);
}
    
/* This function loops exchanging data between the two proxy ends. It returns -1
 * if an error occurs and -2 when a connection is lost on either end. Note that
 * any buffered data is flushed to the other side when one side closes the
 * connection.
 */
int kproxy_loop(struct kproxy_end *e1, struct kproxy_end *e2) {
    int error = 0;
    
    while (1) {
    	int move_flag = 0;
	
        /* Transfer data as required. */
        kproxy_do_xfer(e1, e2, &move_flag);
        
        /* At least one side of the connection was lost, check if we're done. */
        if (kproxy_is_finished(e1, e2)) {
            kmod_set_error("session finished");
            return -2;
        }
        
        /* Block in select if no data was transferred. */
	if (!move_flag) {
	    struct kselect sel;
	    kdaemon_prepare_select(&sel);
            kproxy_prepare_select(&sel, e1, e2);
	    
	    kmod_log_msg(KCD_LOG_MISC, "kproxy_loop: about to wait in select().\n");
	    error = kdaemon_do_select(&sel);
	    kmod_log_msg(KCD_LOG_MISC, "kproxy_loop: out of select().\n");
            
	    if (error) return -1;
	}
    }
}

