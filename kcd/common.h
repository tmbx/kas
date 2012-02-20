/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#ifndef _COMMON_H
#define _COMMON_H

#include <gnutls/gnutls.h>

#include "log_level.h"
#include "kmod_base.h"
#include "misc.h"
#include "kdaemon.h"
#include "pg.h"
#include "ktls.h"
#include "anp.h"
#include "anp_tls.h"
#include "kmod_transfer.h"
#include "k3p.h"
#include "proxy.h"
#include "kanp_core_defs.h"
#include "pg_common.h"
#include "kcd_misc.h"
#include "frontend.h"
#include "kws.h"
#include "ticket.h"
#include "mgt.h"
#include "misc_cmd.h"
#include "kfs.h"
#include "vnc.h"
#include "mail.h"
#include "notif.h"

/* Global variables accessible to all files. */
struct kdaemon_opts {
    
    /* Current logging level. */
    int log_level;
    
    /* True if quitting has been requested. This flag is set in the SIGTERM
     * signal handler.
     */
    volatile char quit_flag;
    
    /* This field if incremented whenever SIGUSR1 is received. */
    volatile uint64_t sigusr1_count;
    
    /* This field if incremented whenever SIGCHLD is received. */
    volatile uint64_t sigchld_count;
    
    /* Pair of sockets used to unblock calls to select() when SIGTERM is
     * received. The first socket is written to by the signal handler. All
     * instances of the daemon share the same socket pair. Do not drain it.
     */
    int quit_sock[2];
    
    /* Pair of sockets used to unblock calls to select() when other signals are
     * received. The first socket is written to by the signal handler. Each
     * process gets its own sockets.
     */
    int signal_sock[2];
    
    /* KCD startup action. */
    kstr startup_action;
    
    /* KCD startup mode. */
    kstr startup_mode;
    
    /* Standard configuration parameters. */
    int kanp_mode;
    int knp_mode;
    int http_mode;
    int vnc_mode;
    int listen_port;
    int knp_port;
    int web_port;
    uint64_t default_kfs_quota;
    karray org_key_id_array;
    karray org_name_array;
    kstr listen_addr;
    kstr config_path;
    kstr kfs_ini_path;
    kstr ssl_cert_path;
    kstr ssl_key_path;
    kstr kmod_binary_path;
    kstr kmod_db_path;
    kstr vnc_cred_path;
    kstr kcd_host;
    kstr web_host;
    kstr web_link;
    kstr invite_mail_kcd_html;
    int use_kfs_dir;
    kstr kfs_dir_path;
    kstr sendmail_path;
    int sendmail_timeout;
    kstr mail_sender;
};

extern struct kdaemon_opts global_opts;

int kdaemon_load_config(int silent_flag);
void kmod_log_msg(int level, const char *format, ...);
void kdaemon_set_task(const char *format, ...);
int kcd_fork(char *task, int *pid, int sig_flag);
int main(int argc, char **argv);

#endif

