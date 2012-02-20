/* Copyright (C) 2008-2012 Opersys inc., All rights reserved. */

#ifndef _PROXY_H
#define _PROXY_H

/* This structure represents one of the two entities connected by the proxy. */
struct kproxy_end {
    
    /* Socket of the connection. */
    int sock;
    
    /* True if the connection has been lost. */
    int lost;
    
    /* If this is non-null, the connection is a TLS connection, otherwise it is
     * a raw TCP connection.
     */
    struct ktls_conn *tls;
    
    /* String describing this end of the connection (for debugging purposes). */
    char *desc;
    
    /* Memory buffer allocated to read data from this connection. */
    kbuffer buf;
};

void kproxy_end_init(struct kproxy_end *self, int sock, struct ktls_conn *tls, char *desc, int size);
void kproxy_end_clean(struct kproxy_end *self);
int kproxy_connect_tcp(int *sock, char *host, int port);
int kproxy_is_finished(struct kproxy_end *e1, struct kproxy_end *e2);
void kproxy_do_xfer(struct kproxy_end *e1, struct kproxy_end *e2, int *move_flag);
void kproxy_prepare_select(struct kselect *sel, struct kproxy_end *e1, struct kproxy_end *e2);
int kproxy_loop(struct kproxy_end *e1, struct kproxy_end *e2);

#endif
