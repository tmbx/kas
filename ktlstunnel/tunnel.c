/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#include "common.h"

#define TRANS_BUF_SIZE 65000

/* Keepalive parameters. See the Linux documentation for details. */
#define TCP_KEEPALIVE_TIME      10*60
#define TCP_KEEPALIVE_INTVl     10
#define TCP_KEEPALIVE_PROBES    9

static int ktun_connect_remote_host(struct ktun *self) {
    int error = 0;
    
    kmod_log_msg(KCD_LOG_MISC, "ktun_connect_remote_host() called.\n");
    
    do {
    	error = kproxy_connect_tcp(&self->remote_sock, self->remote_host, self->remote_port);
	if (error) break;
        
        error = ksock_enable_keepalive(self->remote_sock,
                                       TCP_KEEPALIVE_TIME, TCP_KEEPALIVE_INTVl, TCP_KEEPALIVE_PROBES);
	if (error) break;
        
	error = ktls_setup_client(&self->remote_conn, self->remote_sock, 1, 1);
	if (error) break;
	
    	error = ktls_handshake_loop(&self->remote_conn);
	if (error) break;
	
    } while (0);
    
    if (error) kmod_append_error("cannot connect to remote host");
    
    return error;
}

static int ktun_connect_local_host(struct ktun *self) {
    int error = 0;
    
    kmod_log_msg(KCD_LOG_MISC, "ktun_connect_local_host() called.\n");
    
    do {
    	error = kproxy_connect_tcp(&self->local_sock, self->local_host, self->local_port);
	if (error) break;
        
        error = ksock_enable_keepalive(self->remote_sock,
                                       TCP_KEEPALIVE_TIME, TCP_KEEPALIVE_INTVl, TCP_KEEPALIVE_PROBES);
	if (error) break;
	
    } while (0);
    
    if (error) kmod_append_error("cannot connect to local host");
    
    return error;
}

static int ktun_connect_second_host(struct ktun *self) {
    int error = 0;
    
    kmod_log_msg(KCD_LOG_MISC, "ktun_connect_second_host() called.\n");
    
    do {
    	error = kproxy_connect_tcp(&self->local_sock, global_opts.second_host, global_opts.second_port);
	if (error) break;
        
        error = ksock_enable_keepalive(self->remote_sock,
                                       TCP_KEEPALIVE_TIME, TCP_KEEPALIVE_INTVl, TCP_KEEPALIVE_PROBES);
	if (error) break;
	
    } while (0);
    
    if (error) kmod_append_error("cannot connect to second host");
    
    return error;
}

/* This function returns -1 on error, -2 if the remote connection is lost and -3
 * if the local connection is lost. If 'send_sync_byte_flag' is true, an initial
 * byte is sent to the remote host to synchronize.
 */
static int ktun_exchange_packet(struct ktun *self, int send_sync_byte_flag) {
    int error = -1;
    struct kproxy_end local_end, remote_end;
    
    kmod_log_msg(KCD_LOG_MISC, "ktun_exchange_packet() called.\n");
    
    kproxy_end_init(&local_end, self->local_sock, NULL, "local host", TRANS_BUF_SIZE);
    kproxy_end_init(&remote_end, self->remote_sock, &self->remote_conn, "remote host", TRANS_BUF_SIZE);
    
    if (send_sync_byte_flag) {
        kmod_log_msg(KCD_LOG_MISC, "ktun_exchange_packet(): adding sync byte.\n");
        kbuffer_write8(&local_end.buf, 'a');
    }
   
    if (kproxy_loop(&local_end, &remote_end) == -2) {
	error = local_end.lost ? -3 : -2;
    }
    
    kproxy_end_clean(&local_end);
    kproxy_end_clean(&remote_end);
    
    kmod_log_msg(KCD_LOG_MISC, "ktun_exchange_packet(): exiting: %s.\n", kmod_strerror());
    
    return error;
}
    
void ktun_init(struct ktun *self) {
    self->remote_sock = -1;
    self->local_sock = -1;
    ktls_init(&self->remote_conn);
    kbuffer_init(&self->in_buf);
    kbuffer_init(&self->out_buf);
    kbuffer_grow(&self->in_buf, TRANS_BUF_SIZE);
    kbuffer_grow(&self->out_buf, TRANS_BUF_SIZE);
}
 
void ktun_clean(struct ktun *self) {
    ksock_close(&self->remote_sock);
    ksock_close(&self->local_sock);
    ktls_clean(&self->remote_conn);
    kbuffer_clean(&self->in_buf);
    kbuffer_clean(&self->out_buf);
}
  
int ktun_run_tunnel(struct ktun *self, char *remote_host, int remote_port, char *local_host, int local_port,
		    int local_first) { 
    int error = 0;

    kmod_log_msg(KCD_LOG_MISC, "ktun_run_tunnel() called.\n");

    self->remote_host = remote_host;
    self->local_host = local_host;
    self->remote_port = remote_port;
    self->local_port = local_port;

    do {
	if (local_first) {
	    error = ktun_connect_local_host(self);
	    if (error) break;

	    error = ktun_connect_remote_host(self);
	    if (error) break;
	}

	else {
	    error = ktun_connect_remote_host(self);
	    if (error) break;

	    error = ktun_connect_local_host(self);
	    if (error) break;
	}

	error = ktun_exchange_packet(self, 0);
	if (error == -1) break;

	/* Reconnect to second host, if required. */
	if (error == -3 && global_opts.second_host) {
	    ksock_close(&self->local_sock);

	    error = ktun_connect_second_host(self);
	    if (error) break;

	    error = ktun_exchange_packet(self, 1);
	    if (error == -1) break;
	}
	
	error = 0;
	break;

    } while (0);

    return error;
}

