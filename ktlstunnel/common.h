/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _COMMON_H
#define _COMMON_H

#include <gnutls/gnutls.h>

#include "log_level.h"
#include "kmod_base.h"
#include "misc.h"
#include "ktls.h"
#include "proxy.h"
#include "tunnel.h"

struct kdaemon_opts {
    char quit_flag;
    int log_level;
    FILE *log_file;
    char *remote_host;
    char *local_host;
    char *second_host;
    int remote_port;
    int local_port;
    int second_port;
    int local_first;
    kstr ssl_cert_path;
};

extern struct kdaemon_opts global_opts;

void kmod_log_msg(int level, const char *format, ...);
void kdaemon_prepare_select(struct kselect *sel);
int kdaemon_do_select(struct kselect *sel);

#endif

