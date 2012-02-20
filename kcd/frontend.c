/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* Keepalive parameters. See the Linux documentation for details. Note that the
 * keepalive time must be set under 5 minutes since some stupid routers reset
 * the connection after that delay if there is no network activity.
 */
#define KCD_TCP_KEEPALIVE_TIME              4*60
#define KCD_TCP_KEEPALIVE_INTVl             10
#define KCD_TCP_KEEPALIVE_PROBES            9

/* Size of the buffer used when proxying. */
#define KCD_PROXY_TRANS_BUF_SIZE            (256*1024)

/* Number of bytes read to identify the protocol spoken by the client connecting
 * to us.
 */
#define KCD_PROTO_NB_ID_BYTE                4

/* Length of the credentials file name for VNC. */
#define KCD_VNC_BEGIN_STRING_LENGTH         32

/* Offset where the port is written in the VNC file name. */
#define KCD_VNC_PORT_OFFSET                 27

/* String that is used for doing a VNC test. */
#define KCD_VNC_TEST_KCD_VNC_BEGIN_STRING   "VNC__META__PROXY__LOCAL__TESTING"

/* Response when doing a VNC test. */
#define KCD_VNC_TEST_RESPONSE               "VNC__META__PROXY__LOCAL__TESTING__OK\n"

/* Length of the test response when doing a VNC test. */
#define KCD_VNC_TEST_RESPONSE_LENGTH        strlen(KCD_VNC_TEST_RESPONSE) 

struct kcd_client* kcd_client_new() {
    struct kcd_client *self = (struct kcd_client *) kcalloc(sizeof(struct kcd_client));
    self->sock = -1;
    kstr_init(&self->addr);
    ktls_init(&self->conn);
    return self;
}

void kcd_client_destroy(struct kcd_client *self) {
    if (self) {
	ksock_close(&self->sock);
	kstr_clean(&self->addr);
	ktls_clean(&self->conn);
	kfree(self);
    }
}

/* This function loops in one of the proxy modes. */
static int kcd_frontend_proxy_loop(struct ktls_conn *conn, int proxy_sock, char *service) {
    int error = 0;
    struct kproxy_end client_end, proxy_end;

    kmod_log_msg(KCD_LOG_BRIEF, "kcd_frontend_proxy_loop() called.\n");
    
    kproxy_end_init(&client_end, conn->sock, conn, "client", KCD_PROXY_TRANS_BUF_SIZE);
    kproxy_end_init(&proxy_end, proxy_sock, NULL, service, KCD_PROXY_TRANS_BUF_SIZE);
    
    error = (kproxy_loop(&client_end, &proxy_end) == -1) ? -1 : 0;
    
    kproxy_end_clean(&client_end);
    kproxy_end_clean(&proxy_end);
    
    kmod_log_msg(KCD_LOG_BRIEF, "kcd_proxy_comm_loop(): exiting: %s.\n", kmod_strerror());
    
    return error;
}

/* This function transfers the buffer specified using the TLS connection
 * specified.
 */
static int kcd_frontend_tls_xfer(struct ktls_conn *conn, char *buf, int len, int read_flag) {
    int error = 0;
    int pos = 0;
    struct kselect sel;
    
    kmod_log_msg(KCD_LOG_MISC, "kcd_frontend_tls_xfer() called.\n");
    
    while (1) {
        if (read_flag) error = ktls_recv(conn, buf + pos, len - pos);
        else error = ktls_send(conn, buf + pos, len - pos);
        
        if (error == -1) break;
        else if (error >= 0) pos += error;
        error = 0;
        if (pos == len) break;

        kdaemon_prepare_select(&sel);
        if (read_flag) kselect_add_read(&sel, conn->sock);
        else kselect_add_write(&sel, conn->sock);
        error = kdaemon_do_select(&sel);
        if (error) break;
    }
    
    return error;
}

/* This function transfers the buffer specified using the socket specified. */
static int kcd_frontend_sock_xfer(int sock, char *buf, int len, int read_flag) {
    int error = 0;
    int pos = 0;
    struct kselect sel;
    
    kmod_log_msg(KCD_LOG_MISC, "kcd_frontend_sock_xfer() called.\n");
    
    while (1) {
        uint32_t l = len - pos;
            
        if (read_flag) error = ksock_read(sock, buf + pos, &l);
        else error = ksock_write(sock, buf + pos, &l);
        
        if (error == -1) break;
        else if (error == 0) pos += l;
        error = 0;
        if (pos == len) break;

        kdaemon_prepare_select(&sel);
        if (read_flag) kselect_add_read(&sel, sock);
        else kselect_add_write(&sel, sock);
        error = kdaemon_do_select(&sel);
        if (error) break;
    }
    
    return error;
}

/* This function retrieves the version information contained in the message
 * received from the client, stores it in the 'client' structure, and validates
 * that the client is recent enough to interact with the KCD.
 */
static int kcd_frontend_check_kanp_version(struct kcd_client *client, struct anp_msg *msg) {
    client->announced_major = msg->major;
    client->announced_minor = msg->minor;
    client->effective_major = 0;
    client->effective_minor = MIN(client->announced_minor, KANP_MINOR_VERSION);
    
    kmod_log_msg(KCD_LOG_MISC, "Client has KANP minor version %u.\n", client->announced_minor);
    
    /* Cast used to shut up nitpicking GCC. */
    if ((int) client->effective_minor < (int) KANP_LAST_COMP_MINOR_VERSION) {
        kmod_set_error("sorry, your client is too old and must be upgraded");
        return -1;
    }
    
    return 0;
}

/* This function negociates the role of KCD in KANP mode. */
static int kcd_frontend_negociate_kanp_role(struct kcd_client *client, struct anp_tls_xfer *xfer) {
    int error = 0, got_role_flag = 0, must_upgrade_flag = 0;
    struct anp_msg *msg = NULL;
    uint32_t role;
    uint64_t msg_id;
    
    kmod_log_msg(KCD_LOG_MISC, "kcd_frontend_negociate_kanp_role() called.\n");

    do {
        /* Receive the first packet. */
	error = kcd_do_anp_xfer(xfer, &client->conn);
	if (error) break;

	msg = anp_tls_get_recv(xfer);
	msg_id = msg->id;
        
        /* Analyse the first packet. */
	do {
            /* The first packet received should always be a role negociation. */
	    if (msg->type != KANP_CMD_MGT_SELECT_ROLE) {
		kmod_set_error("expected select role packet");
		break;
	    }
            
            /* Check the version number. If the client is too old, do not parse
             * the message further.
             */
            if (kcd_frontend_check_kanp_version(client, msg)) {
                must_upgrade_flag = 1;
                break;
            }

	    if (anp_read_uint32(&msg->payload, &role)) break;

	    if (role != KANP_KCD_ROLE_WORKSPACE &&
		role != KANP_KCD_ROLE_FILE_XFER &&
		role != KANP_KCD_ROLE_APP_SHARE) { 
		kmod_set_error("invalid role");
		break;
	    }
            
            got_role_flag = 1;

	} while (0);
        
        /* Prepare the result. */
	anp_msg_destroy(msg);
        msg = anp_msg_new();
        msg->major = KANP_MAJOR_VERSION;
        msg->minor = KANP_MINOR_VERSION;
        msg->id = msg_id;

	if (!got_role_flag) {
        
            if (must_upgrade_flag) {
                kmod_log_msg(KCD_LOG_BRIEF, "Refusing access to obsolete client with minor version %d.\n",
                                            client->announced_minor);
                kcd_kanp_set_failure(msg, KANP_RES_FAIL_MUST_UPGRADE);
            }
            
            else {
                kmod_log_msg(KCD_LOG_BRIEF, "Bad select role packet: %s.\n", kmod_strerror());
	        kcd_kanp_set_gen_failure(msg);
            }
	}

	else {
	    msg->type = KANP_RES_OK;
	}
        
        /* Send the result with our latest version numbers in all cases. */
	anp_tls_send_msg(xfer, msg);
	anp_msg_destroy(msg);
	msg = NULL;
	
        error = kcd_do_anp_xfer(xfer, &client->conn);
	if (error) break;
        
        /* We don't have a role. Bail out. */
        if (!got_role_flag) break;
        
        /* Dispatch. */
        if (role == KANP_KCD_ROLE_WORKSPACE) error = kcd_kws_handle_conn(client);
        else if (role == KANP_KCD_ROLE_FILE_XFER) error = kcd_ticket_mode_handle_conn(client, KANP_NS_KFS);
        else if (role == KANP_KCD_ROLE_APP_SHARE) error = kcd_ticket_mode_handle_conn(client, KANP_NS_VNC);
        if (error) break;

    } while (0);

    anp_msg_destroy(msg);
    
    return error;
}

/* This function handles a connection in the KANP mode. */
static int kcd_frontend_handle_kanp(struct kcd_client *client, char *id_buf) {
    int error = 0;
    struct anp_tls_xfer xfer;

    anp_tls_init(&xfer);
        
    kdaemon_set_task("KANP | %s", client->addr.data);
    kmod_log_msg(KCD_LOG_MISC, "kcd_frontend_handle_kanp() called.\n");
    
    do {
        if (!global_opts.kanp_mode) {
            kmod_set_error("KANP mode disabled");
            error = -1;
            break;
        }
    
        anp_tls_begin_recv(&xfer);
        anp_tls_add_id_buf(&xfer, id_buf);
    
        error = kcd_frontend_negociate_kanp_role(client, &xfer);
        if (error) break;
        
    } while (0);
    
    anp_tls_clean(&xfer);
    
    return error;
}

/* This function handles a connection in VNC proxy mode. */
static int kcd_frontend_handle_vnc(struct kcd_client *client) {
    int error = 0;
    int proxy_sock = -1;
    char begin_string[KCD_VNC_BEGIN_STRING_LENGTH + 1];
    kstr cred_path;
    
    kdaemon_set_task("VNC proxy | %s", client->addr.data);
    kmod_log_msg(KCD_LOG_BRIEF, "kcd_frontend_handle_vnc() called.\n");
    
    assert(strlen(KCD_VNC_TEST_KCD_VNC_BEGIN_STRING) == KCD_VNC_BEGIN_STRING_LENGTH);
    
    begin_string[KCD_VNC_BEGIN_STRING_LENGTH] = 0;
    kstr_init(&cred_path);
    
    do {
        int port;
        
        if (!global_opts.vnc_mode) {
            kmod_set_error("VNC proxy mode disabled");
            error = -1;
            break;
        }
        
        /* Receive the file name. */
        error = kcd_frontend_tls_xfer(&client->conn, begin_string, KCD_VNC_BEGIN_STRING_LENGTH, 1);
        if (error) break;

        /* This is a test. */
        if (! strncmp(begin_string, KCD_VNC_TEST_KCD_VNC_BEGIN_STRING, KCD_VNC_BEGIN_STRING_LENGTH)) {
            kmod_log_msg(KCD_LOG_MISC, "kcd_proxy_handle_vnc(): test connection.\n");
            error = kcd_frontend_tls_xfer(&client->conn, KCD_VNC_TEST_RESPONSE, KCD_VNC_TEST_RESPONSE_LENGTH, 0);
            if (error) break;
        }
        
        /* This is a genuine VNC connection. */
        else {
            
            /* 'begin_string' is a credential file. */
            char *file_name = begin_string;

            /* Check if that file exists. */
            kstr_sf(&cred_path, "%s/%s", global_opts.vnc_cred_path.data, file_name);
        
            if (! kfs_regular(cred_path.data)) {
                kmod_set_error("credential file %s does not exist", file_name);
                error = -1;
                break;
            }
        
            /* Get the port. */
            port = atoi(file_name + KCD_VNC_PORT_OFFSET);
        
            /* Note: don't delete the credential file, IE sucks and connects
             * twice.
             */
        
            /* Connect to the VNC proxy. */
            error = kproxy_connect_tcp(&proxy_sock, global_opts.kcd_host.data, port);
            if (error) break;
        
            /* Loop within the proxy. */
            error = kcd_frontend_proxy_loop(&client->conn, proxy_sock, "VNC proxy");
            if (error) break;
        }
        
    } while (0);
    
    ksock_close(&proxy_sock);
    kstr_clean(&cred_path); 
    
    return error;
}

/* This function handles a connection in HTTP proxy mode. */
static int kcd_frontend_handle_http(struct kcd_client *client, char *id_buf) {
    int error = 0;
    int proxy_sock = -1;
    
    kdaemon_set_task("HTTP proxy | %s", client->addr.data);
    kmod_log_msg(KCD_LOG_BRIEF, "kcd_frontend_handle_http() called.\n");
    
    do {
        if (!global_opts.http_mode) {
            kmod_set_error("HTTP proxy mode disabled");
            error = -1;
            break;
        }
        
        /* Connect to the web server. */
        error = kproxy_connect_tcp(&proxy_sock, "127.0.0.1", global_opts.web_port);
        if (error) break;
        
        /* Write back the identification bytes. */
        error = kcd_frontend_sock_xfer(proxy_sock, id_buf, KCD_PROTO_NB_ID_BYTE, 0);
        if (error) break;
    
        /* Loop within the proxy. */
        error = kcd_frontend_proxy_loop(&client->conn, proxy_sock, "web server");
        if (error) break;
    
    } while (0);
    
    ksock_close(&proxy_sock);
    
    return error;
}

/* This function handles a connection in KNP proxy mode. */
static int kcd_frontend_handle_knp(struct kcd_client *client, char *id_buf) {
    int error = 0;
    int proxy_sock = -1;
    
    kdaemon_set_task("KNP proxy | %s", client->addr.data);
    kmod_log_msg(KCD_LOG_BRIEF, "kcd_frontend_handle_knp() called.\n");
    
    do {
        if (!global_opts.knp_mode) {
            kmod_set_error("KNP proxy mode disabled");
            error = -1;
            break;
        }
        
        /* Connect to the tbxsosd daemon. */
        error = kproxy_connect_tcp(&proxy_sock, "127.0.0.1", global_opts.knp_port);
        if (error) break;
        
        /* Write back the identification bytes. */
        error = kcd_frontend_sock_xfer(proxy_sock, id_buf, KCD_PROTO_NB_ID_BYTE, 0);
        if (error) break;
    
        /* Loop within the proxy. */
        error = kcd_frontend_proxy_loop(&client->conn, proxy_sock, "tbxsosd");
        if (error) break;
    
    } while (0);
    
    ksock_close(&proxy_sock);
    
    return error;
}

/* This function handles a connection in frontend mode. */
static void kcd_frontend_handle_conn(struct kcd_client *client) {
    int error = 0;
    struct anp_tls_xfer xfer;
    char recv_id_buf[KCD_PROTO_NB_ID_BYTE];
    char vnc_id_buf[KCD_PROTO_NB_ID_BYTE] = { 'V', 'N', 'C', '!' };
    char knp_id_buf[KCD_PROTO_NB_ID_BYTE] = { 0, 0, 0, 4 };
    char anp_id_buf[KCD_PROTO_NB_ID_BYTE] = { 0, 0, 0, 0 };
    
    anp_tls_init(&xfer);
    
    kdaemon_set_task("Frontend | %s", client->addr.data);
    kmod_log_msg(KCD_LOG_MISC, "kcd_frontend_handle_conn() called.\n");

    do {
        /* Establish the SSL connection. Using /etc/kcd_noanon to disable
         * support old ktlstunnel programs if desired.
         */
	error = ktls_setup_server(&client->conn,
                                  client->sock,
                                  global_opts.ssl_cert_path.slen ? global_opts.ssl_cert_path.data : NULL,
                                  global_opts.ssl_key_path.data,
                                  global_opts.kanp_mode && !kfs_regular("/etc/kcd_noanon"));
	if (error) break;

	error = ktls_handshake_loop(&client->conn);
	if (error) break;
        
        /* Read the identification bytes. */
        error = kcd_frontend_tls_xfer(&client->conn, recv_id_buf, KCD_PROTO_NB_ID_BYTE, 1);
	if (error) break;
        
        /* Dispatch. */
        if (!memcmp(recv_id_buf, vnc_id_buf, KCD_PROTO_NB_ID_BYTE))
            error = kcd_frontend_handle_vnc(client);
            
        else if (!memcmp(recv_id_buf, knp_id_buf, KCD_PROTO_NB_ID_BYTE))
            error = kcd_frontend_handle_knp(client, recv_id_buf);
            
        else if (!memcmp(recv_id_buf, anp_id_buf, KCD_PROTO_NB_ID_BYTE))
            error = kcd_frontend_handle_kanp(client, recv_id_buf);
            
        else error = kcd_frontend_handle_http(client, recv_id_buf);
        
        if (error) break;
        
    } while(0);
    
    if (error) {
        kmod_log_msg(KCD_LOG_BRIEF, "Error in client connection: %s.\n", kmod_strerror());
    }
    
    anp_tls_clean(&xfer);
}

/* This function collects zombies in the frontend listener loop. If 'wait_flag'
 * is true, all children are collected, otherwise only zombies are collected.
 */
static void kcd_frontend_loop_collect_zombie(int *nb_child, int wait_flag) {
    while (*nb_child) {
        int ignored;
        if (!kcd_waitpid(-1, wait_flag, &ignored) && !wait_flag) return;
	(*nb_child)--;
    }
}

/* Handle the signaled state in the listener loop. */
static int kcd_frontend_loop_handle_signal(int *nb_child) {
    int error = 0;
    
    kmod_log_msg(KCD_LOG_MISC, "kcd_frontend_loop_handle_signal() called.\n");
    
    kdaemon_block_signals();
    
    do {
        if (global_opts.sigchld_count) {
            global_opts.sigchld_count = 0;
            kcd_frontend_loop_collect_zombie(nb_child, 0);
        }
        
        if (global_opts.sigusr1_count) {
            global_opts.sigusr1_count = 0;
            error = kdaemon_load_config(0);
            if (error) break;
        }
        
    } while (0);
    
    kdaemon_unblock_signals();
    
    return error;
}

/* This function accepts a connection from a client in the listener loop. */
static void kcd_frontend_loop_accept_conn(int *nb_child, int listen_sock) {
    int error = 0;
    int pid;
    struct sockaddr_in sock_addr;
    socklen_t sock_len;
    struct kcd_client *client = kcd_client_new();
    
    kmod_log_msg(KCD_LOG_MISC, "kcd_frontend_accept_conn() called.\n");
    
    do {
	/* Try to accept a connection. */
	error = ksock_accept(listen_sock, &client->sock);
	if (error == -2) { error = 0; break; }
	if (error) break;
	
	/* Get the client address. */
	sock_len = sizeof(sock_addr);
	error = getpeername(client->sock, (struct sockaddr *) &sock_addr, &sock_len);
	if (error) {
	    kmod_set_error("cannot get peer name: %s", strerror(errno));
	    break;
	}
	
	kstr_assign_cstr(&client->addr, inet_ntoa(sock_addr.sin_addr));
	client->port = ntohs(sock_addr.sin_port);
	
	/* Set the socket to non-blocking mode. */
	error = ksock_set_unblocking(client->sock);
	if (error) break;
        
        /* Enable keepalives. */
        error = ksock_enable_keepalive(client->sock, KCD_TCP_KEEPALIVE_TIME,
                                       KCD_TCP_KEEPALIVE_INTVl, KCD_TCP_KEEPALIVE_PROBES);
	if (error) break;
	
	/* We got the connection. Dispatch it. */
	kmod_log_msg(KCD_LOG_BRIEF, "Accepted connection from %s on port %u.\n", client->addr.data, client->port);
	
	/* Fork to handle the connection. */
        error = kcd_fork("Service ID", &pid, 1);
        if (error) break;
        
        /* Child. */
        else if (!pid) {
            kcd_frontend_handle_conn(client);
            exit(0);
        }
        
        /* Parent. */
        else {
            (*nb_child)++;
        }
	
    } while (0);
	    
    if (error) {
	kmod_log_msg(KCD_LOG_BRIEF, "Error accepting connection: %s.\n", kmod_strerror());
    }
	    
    kcd_client_destroy(client);
}

/* Loop accepting connections. */
int kcd_frontend_listener_loop() {
    int error = 0;
    int nb_child = 0;
    int listen_sock = -1;
    
    kdaemon_set_task("Listener");
    kmod_log_msg(KCD_LOG_BRIEF, "kcd_frontend_listener_loop() called.\n");
    
    do {
	/* Begin listening for connections. */
	error = ksock_create(&listen_sock);
	if (error) break;
	
	error = ksock_bind(listen_sock, global_opts.listen_addr.data, global_opts.listen_port);
	if (error) break;
	
	error = ksock_listen(listen_sock);
	if (error) break;
	
	error = ksock_set_unblocking(listen_sock);
	if (error) break;
	
	/* Loop accepting connections. */
	while (1) {
	    struct kselect sel;
	    
	    /* Wait for a connection. */
	    kdaemon_prepare_select(&sel);
	    kselect_add_read(&sel, listen_sock);
	    error = kdaemon_do_select(&sel);
	    if (error) break;
            
            /* We've been signaled. */
            if (global_opts.sigusr1_count || global_opts.sigchld_count) {
                error = kcd_frontend_loop_handle_signal(&nb_child);
                if (error) break;
            }
	    
	    /* Try to accept a connection. */
	    if (kselect_in_read(&sel, listen_sock)) {
		kcd_frontend_loop_accept_conn(&nb_child, listen_sock);
	    }
	}
	
	if (error) break;
	
    } while (0);
    
    ksock_close(&listen_sock);
    
    /* Collect all children. */
    kcd_frontend_loop_collect_zombie(&nb_child, 1);
    
    return error;
}

