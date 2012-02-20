/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#ifndef _KCD_FRONTEND_H
#define _KCD_FRONTEND_H

/* This structure represents a client connected to the KCD. */
struct kcd_client {
    
    /* Socket of the connection with the client. */
    int sock;
    
    /* IP address of the client. */
    kstr addr;
    
    /* Port used by the client to connect to the server. */
    int port;
    
    /* GnuTLS connection. */
    struct ktls_conn conn;
    
    /* Version information advertised by the client. */
    uint32_t announced_major;
    uint32_t announced_minor;
    
    /* Effective version of the protocol used to communicate with the client. */
    uint32_t effective_major;
    uint32_t effective_minor;
};

struct kcd_client* kcd_client_new();
void kcd_client_destroy(struct kcd_client *self);
int kcd_frontend_listener_loop();

#endif

