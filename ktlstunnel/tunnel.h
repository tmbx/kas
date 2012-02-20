/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _TUNNEL_H
#define _TUNNEL_H

struct ktun {
    int remote_sock;
    int local_sock;
    char *remote_host;
    char *local_host;
    int remote_port;
    int local_port;
    struct ktls_conn remote_conn;
    kbuffer in_buf;
    kbuffer out_buf;
};

void ktun_init(struct ktun *self);
void ktun_clean(struct ktun *self);
int ktun_run_tunnel(struct ktun *self, char *remote_host, int remote_port, char *local_host, int local_port,
    	    	    int local_first);

#endif
