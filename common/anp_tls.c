/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#include "common.h"

void anp_tls_init(struct anp_tls_xfer *self) {
    self->in_state = 0;
    self->in_msg = NULL;
    kbuffer_init(&self->in_buf);
    self->out_state = 0;
    kbuffer_init(&self->out_buf);
}

void anp_tls_clean(struct anp_tls_xfer *self) {
    anp_tls_reset(self);
    kbuffer_clean(&self->in_buf);
    kbuffer_clean(&self->out_buf);
}

void anp_tls_reset(struct anp_tls_xfer *self) {
    anp_tls_flush_recv(self);
    anp_tls_flush_send(self);
}

/* In frontend mode KCD reads 4 bytes to identify the service type. This method
 * adds those four bytes to the buffer of the message being received.
 */
void anp_tls_add_id_buf(struct anp_tls_xfer *self, char *id_buf) {
    assert(self->in_state == 1);
    assert(self->in_buf.len == 0);
    memcpy(self->in_buf.data, id_buf, 4);
    self->in_buf.len = 4;
}

void anp_tls_begin_recv(struct anp_tls_xfer *self) {
    anp_tls_flush_recv(self);
    self->in_state = 1;
    kbuffer_reset(&self->in_buf);
    kbuffer_grow(&self->in_buf, ANP_MSG_HDR_SIZE);
}

struct anp_msg * anp_tls_get_recv(struct anp_tls_xfer *self) {
    assert(self->in_state == 3);
    struct anp_msg *in_msg = self->in_msg;
    self->in_msg = NULL;
    self->in_state = 0;
    return in_msg;
}

void anp_tls_flush_recv(struct anp_tls_xfer *self) {
    self->in_state = 0;
    anp_msg_destroy(self->in_msg);
    self->in_msg = NULL;
}

void anp_tls_send_msg(struct anp_tls_xfer *self, struct anp_msg *msg) {
    anp_tls_flush_send(self);
    self->out_state = 1;
    anp_msg_to_buf(msg, &self->out_buf);
}

void anp_tls_send_many_msg(struct anp_tls_xfer *self, karray *msg_array) {
    int i;
    
    anp_tls_flush_send(self);
    self->out_state = 1;
    
    for (i = 0; i < msg_array->size; i++) {
    	anp_msg_to_buf((struct anp_msg *) msg_array->data[i], &self->out_buf);
    }
}

void anp_tls_flush_send(struct anp_tls_xfer *self) {
    self->out_state = 0;
    kbuffer_shrink(&self->out_buf, 1024);
}

/* Helper method for anp_tls_do_xfer(). */
static int anp_tls_xfer_state_3(struct anp_tls_xfer *self) {
    assert(self->in_msg);
    if (anp_msg_parse(self->in_msg, &self->in_buf)) return -1;
    kbuffer_shrink(&self->in_buf, 1024);
    self->in_state = 3;
    return 0;
}

/* Return true if there is a transfer to perform. */
int anp_tls_has_xfer(struct anp_tls_xfer *self) {
    return ((anp_tls_receiving(self) && !anp_tls_done_receiving(self)) ||
            (anp_tls_sending(self) && !anp_tls_done_sending(self)));
}

int anp_tls_do_xfer(struct anp_tls_xfer *self, struct ktls_conn *conn) {
    int error = 0;
    int loop = 1;
    
    kmod_log_msg(KCD_LOG_MISC, "anp_tls_do_xfer() called.\n");
    
    /* I'm wary of GnuTLS not transferring everything it can on the socket, so
     * I'm looping while stuff move.
     */
    while (loop) {
    	loop = 0;
	
	if (self->in_state == 1) {
    	    kbuffer *buf = &self->in_buf;
    	    int r = ktls_recv(conn, buf->data + buf->len, ANP_MSG_HDR_SIZE - buf->len);

	    if (r == -1) { error = -1; break; }

	    if (r > 0) {
	    	loop = 1;
		buf->len += r;
		
		if (buf->len == ANP_MSG_HDR_SIZE) {
                    assert(self->in_msg == NULL);
                    self->in_msg = anp_msg_new();
		    kbuffer_read32(buf, &self->in_msg->major);
		    kbuffer_read32(buf, &self->in_msg->minor);
		    kbuffer_read32(buf, &self->in_msg->type);
		    kbuffer_read64(buf, &self->in_msg->id);
		    kbuffer_read32(buf, &self->payload_size);

		    if (self->payload_size > ANP_MSG_MAX_SIZE) {
	    		kmod_set_error("ANP message received is too big");
			error = -1;
			break;
		    }
                    
                    kbuffer_reset(buf);
		    kbuffer_grow(buf, self->payload_size);
		    
		    if (self->payload_size) {
    	    	    	self->in_state = 2;
		    }
		    
		    else {
                        if (anp_tls_xfer_state_3(self)) return -1;
		    }
		}
    	    }
	}
    
	if (self->in_state == 2) {
    	    kbuffer *buf = &self->in_buf;
    	    int r = ktls_recv(conn, buf->data + buf->len, self->payload_size - buf->len);

	    if (r == -1) { error = -1; break; }

	    if (r > 0) {
		loop = 1;
		buf->len += r;

		if (buf->len == self->payload_size) {
                    if (anp_tls_xfer_state_3(self)) return -1;
		}
    	    }
	}

	if (self->out_state == 1) {
    	    kbuffer *buf = &self->out_buf;
    	    int r = ktls_send(conn, buf->data + buf->pos, buf->len - buf->pos);
	    
	    if (r == -1) { error = -1; break; }

	    if (r > 0) {
		loop = 1;
		buf->pos += r;

		if (buf->len == buf->pos) {
		    self->out_state = 2;
		}
    	    }
	}
    }
    
    if (error) anp_tls_reset(self);
    
    return error;
}

/* Prepare the call to select by adding the sockets in the select set specified. */
void anp_tls_prepare_select(struct anp_tls_xfer *self, struct ktls_conn *conn, struct kselect *sel) {
    if (anp_tls_receiving(self) && !anp_tls_done_receiving(self)) kselect_add_read(sel, conn->sock); 
    if (anp_tls_sending(self) && !anp_tls_done_sending(self)) kselect_add_write(sel, conn->sock);
}

