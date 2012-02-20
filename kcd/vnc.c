/* Copyright (C) 2008-2012 Opersys inc., All rights reserved. */

#include "common.h"

#define TRANS_BUF_SIZE (256*1024)

/* This structure contains the data required to service a client in VNC mode. */
struct kcd_vnc_mode_state {
    
    /* VNC session ID. */
    uint64_t session_id;
    
    /* End session error code. */
    uint32_t end_error_code;
    
    /* End session error message. */
    kstr end_error_msg;
    
    /* Port of the proxy. */
    int proxy_port;
    
    /* Socket of the proxy. */
    int proxy_sock;
    
    /* Proxy process. */
    struct kcd_process process;
    
    /* Ticket mode state. */
    struct kcd_ticket_mode_state *tms;
};

static void kcd_vnc_mode_state_init(struct kcd_vnc_mode_state *self, struct kcd_ticket_mode_state *tms) {
    memset(self, 0, sizeof(struct kcd_vnc_mode_state));
    self->proxy_sock = -1;
    kstr_init(&self->end_error_msg);
    kcd_process_init(&self->process);
    self->tms = tms;
}

static void kcd_vnc_mode_state_clean(struct kcd_vnc_mode_state *self) {
    ksock_close(&self->proxy_sock);
    kstr_clean(&self->end_error_msg);
    kcd_process_clean(&self->process);
}

/* This function loops communicating with the proxy. It returns 0 or -1. */
static int kcd_vnc_proxy_comm_loop(struct kcd_vnc_mode_state *vms) {
    int error = 0;
    struct kcd_ticket_mode_state *tms = vms->tms;
    struct kcd_client *client = tms->client;
    struct kproxy_end client_end, proxy_end;
    
    /* select() doesn't like huge values for time. */
    int64_t deadline = ktime_set_deadline(MIN(tms->license_info.vnc_session_time * 1000llu, 10*24*60*60*1000llu));
    int64_t remaining;
    
    kmod_log_msg(KCD_LOG_BRIEF, "kcd_vnc_proxy_comm_loop() called.\n");
        
    kproxy_end_init(&client_end, client->sock, &client->conn, "client", TRANS_BUF_SIZE);
    kproxy_end_init(&proxy_end, vms->proxy_sock, NULL, "proxy", TRANS_BUF_SIZE);
    
    do {
        /* Receive the initial synchronization byte. */
        kmod_log_msg(KCD_LOG_VNC, "kcd_vnc_proxy_comm_loop: receiving sync byte.\n");
         
        while (1) {
            char byte;
            
            int r = ktls_recv(&client->conn, &byte, 1);
            if (r == 1) break;
            if (r == -1) { error = -4; break; }
            
            kcd_ticket_mode_prepare_wait(tms);
	    kselect_add_read(&tms->sel, client->sock);
            error = kcd_ticket_mode_wait(tms, -1);
	    if (error) break;
        }
        
        if (error) break;
        
        /* Enter the proxy loop. */
        kmod_log_msg(KCD_LOG_VNC, "kcd_vnc_proxy_comm_loop: entering proxy loop.\n");
        
        while (1) {
            
            /* Transfer data as required. */
            kproxy_do_xfer(&client_end, &proxy_end, NULL);
            kcd_process_do_xfer(&vms->process);
            
            /* At least one side of the connection was lost, check if we're done. */
            if (kproxy_is_finished(&client_end, &proxy_end)) {
                error = -4;
                break;
            }
            
            /* Check deadline, compute remaining. */
            if (ktime_check_deadline(deadline, &remaining)) {
                error = -4;
                kmod_set_error("maximum screen sharing session time reached");
                vms->end_error_code = KANP_RES_FAIL_RESOURCE_QUOTA;
                kstr_assign_kstr(&vms->end_error_msg, kmod_kstrerror());
                break;
            }
            
            /* Block in select. */
            kcd_ticket_mode_prepare_wait(tms);
            kproxy_prepare_select(&tms->sel, &client_end, &proxy_end);
            kcd_process_prepare_select(&vms->process, &tms->sel);
            
            kmod_log_msg(KCD_LOG_VNC, "kproxy_loop: about to wait in select().\n");
            error = kcd_ticket_mode_wait(tms, remaining);
            kmod_log_msg(KCD_LOG_VNC, "kproxy_loop: out of select().\n");
            if (error) break;
        }
        
        if (error) break;
        
    } while (0);
    
    kmod_log_msg(KCD_LOG_BRIEF, "kcd_vnc_proxy_comm_loop(): exiting: %s.\n", kmod_strerror());
        
    kproxy_end_clean(&client_end);
    kproxy_end_clean(&proxy_end);
    
    if (error == -4) {
        
        /* Set a generic end error code and message. */
        if (!vms->end_error_code) {
            vms->end_error_code = KANP_RES_FAIL_GEN;
            kstr_assign_kstr(&vms->end_error_msg, kmod_kstrerror());
        }
        
        error = 0;
    }
    
    return error;
}

/* Clean-up function for closing file descriptors after forking the VNC process. */
static void kcd_vnc_proxy_process_gc(struct kcd_vnc_mode_state *vms) {
    ksock_close(&vms->tms->client->sock);
    ksock_close(&vms->tms->db_conn.sock);
    ksock_close(&vms->proxy_sock);
}

/* This function starts the proxy process and connects to it. */
static int kcd_vnc_start_proxy(struct kcd_vnc_mode_state *vms) {
    int error = 0;
    int sock_pair[2] = { -1, -1 };
    int listen_sock = -1;
    int local_addr_len;
    struct sockaddr_in local_addr;
    char k_opt[100];
    char *argv[] = { "/usr/bin/vncreflector", "-k", k_opt, "-f", "3", "unused", NULL };
    
    do {
	/* Listen to 127.0.0.1 on the first free port. */
	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	local_addr.sin_port = 0;				
		
	error = ksock_create(&listen_sock);
	if (error) break;
	
	if (bind(listen_sock, (struct sockaddr *) &local_addr, sizeof(local_addr)) < 0) {
	    kmod_set_error("cannot bind socket: %s", strerror(errno));
	    error = -1;
	    break;
	}
	
	error = ksock_listen(listen_sock);
	if (error) break;
	
	error = ksock_set_unblocking(listen_sock);
	if (error) break;
	
	local_addr_len = sizeof(local_addr);
	if (getsockname(listen_sock, (struct sockaddr *) &local_addr, &local_addr_len) < 0) {
	    kmod_set_error("getsockname failed: %s", strerror(errno));
	    error = -1;
	    break;
	}
	
	vms->proxy_port = htons(local_addr.sin_port);
	
	/* Create the proxy host socket. */
        kdaemon_open_socket_pair(sock_pair);
	vms->proxy_sock = sock_pair[0];
	sock_pair[0] = -1;
	    
        /* Format the '-k' option. */
        sprintf(k_opt, "%d:%d", sock_pair[1], listen_sock);
	
	/* Start the proxy process. */
        vms->process.log_level = KCD_LOG_VNC;
        error = kcd_process_start(&vms->process, argv, kcd_vnc_proxy_process_gc, vms);
        if (error) break;
    
    } while (0);
    
    kdaemon_close_socket_pair(sock_pair);
    ksock_close(&listen_sock);
    
    return error;
}

/* Check the right to start/join a VNC session. */
static int kcd_vnc_check_right(struct kcd_ticket_mode_state *tms) {
    if (!tms->license_info.vnc_session_time) {
        kmod_set_error("not authorized to start screen sharing session");
        return kcd_ticket_mode_set_failure(tms, KANP_RES_VNC_START_SESSION);
    }
    
    return 0;
}

/* This function connects to a VNC session. */
int kcd_vnc_connect_session(struct kcd_ticket_mode_state *tms) {
    int error = 0;
    struct kcd_vnc_mode_state vms;
    kstr *query = &tms->query;
    PGresult *pg_res = NULL;
    
    kcd_vnc_mode_state_init(&vms, tms);
    
    do {
        /* Check the right to start/join a VNC session. */
        error = kcd_vnc_check_right(tms);
        if (error) break;
    
        /* Get the extra information from the ticket. */
        error = anp_read_uint64(&tms->ticket.ext, &vms.session_id);
        if (error) break;
    
	/* Get the port of the session. */
	kstr_sf(query, "SELECT port FROM kcd_kws_vnc_session "
                       "WHERE kws_id = "PRINTF_64"u AND session_id = "PRINTF_64"u", tms->kws_id, vms.session_id);
	error = kcd_exec_pg_query(&tms->db_conn, query->data, &pg_res, "select from VNC map");
    	if (error) break;
	
	/* No such session. */
	if (!PQntuples(pg_res)) {
	    kmod_set_error("the sharing session has been closed");
	    error = -2;
	    break;
	}
	
	vms.proxy_port = pg_db_get_uint32(pg_res, 0, 0);
	pg_db_destroy_res(&pg_res);
	
	/* Connect to the proxy. */
	error = ksock_create(&vms.proxy_sock);
	if (error) break;
	
	error = ksock_set_unblocking(vms.proxy_sock);
	if (error) break;
	
	if (ksock_connect(vms.proxy_sock, "127.0.0.1", vms.proxy_port)) {
	    kmod_set_error("the sharing session has been closed");
	    error = -2;
	    break;
	}
	
	/* Send the "OK" to the client. */
        kcd_ticket_mode_new_out_msg(tms, KANP_RES_OK);
        error = kcd_ticket_mode_send_msg(tms);
	if (error) break;
        
	/* Loop communicating with the proxy. */
	error = kcd_vnc_proxy_comm_loop(&vms);
	if (error) break;
	
    } while (0);
    
    kcd_vnc_mode_state_clean(&vms);
    pg_db_destroy_res(&pg_res);
    
    return error;
}

/* This function starts the VNC session. */
int kcd_vnc_start_session(struct kcd_ticket_mode_state *tms) {
    int error = 0;
    struct kcd_vnc_mode_state vms;
    kstr subject;
    kbuffer *kbb = &tms->kws_bound_buf, *in_buf = &tms->aq.input_buf, *out_buf = &tms->aq.output_buf;
    
    kcd_vnc_mode_state_init(&vms, tms);
    kstr_init(&subject);
    
    do {
        /* Check the right to start/join a VNC session. */
        error = kcd_vnc_check_right(tms);
        if (error) break;
        
        /* Get the extra information from the command. */
        if (anp_read_kstr(&tms->in_msg->payload, &subject)) {
            error = -2;
            break;
        }
        
	/* Start the proxy server. */
	error = kcd_vnc_start_proxy(&vms);
	if (error) break;
        
        /* Start the session in Postgres. This has to be done after the proxy
         * has been started so that the session does not get collected early.
         */
        anp_write_kstr(kbb, &subject);
        anp_write_uint32(kbb, vms.proxy_port);
        error = kcd_ticket_mode_kws_bound_query(tms, "start_vnc", ktime_now_sec(), NULL);
        if (error) break;
        
        error = anp_read_uint64(out_buf, &vms.session_id);
        if (error) break;
	
	/* Send the "OK" to the client. */
        kcd_ticket_mode_new_out_msg(tms, KANP_RES_OK);
        
        if (tms->client->effective_minor > 2) {
            tms->out_msg->type = KANP_RES_VNC_START_SESSION;
            anp_write_uint64(&tms->out_msg->payload, vms.session_id);
        }
	    
        error = kcd_ticket_mode_send_msg(tms);
	if (error) break;
	
	/* Loop communicating with the proxy. */
	error = kcd_vnc_proxy_comm_loop(&vms);
	if (error) break;
        
        /* Terminate the session. */
        anp_write_uint64(in_buf, tms->kws_id);
        anp_write_uint32(in_buf, tms->user_id);
        anp_write_uint64(in_buf, vms.session_id);
        anp_write_uint32(in_buf, tms->client->effective_minor >= 5 ? 5 : 2);
        anp_write_uint32(in_buf, vms.end_error_code);
        anp_write_kstr(in_buf, &vms.end_error_msg);
        error = kcd_exec_safe_pg_anp_query(&tms->db_conn, &tms->aq, "end_vnc");
        if (error) break;
    
    } while (0);
    
    /* Kill and collect the proxy server. */
    if (error != -1) {
        if (kcd_process_kill_and_collect(&vms.process)) {
            error = -1;
        }
    }
    
    /* Log the proxy output. */
    kcd_process_log_output(&vms.process, 1, 1);
    
    kcd_vnc_mode_state_clean(&vms);
    kstr_clean(&subject);
    
    return error;
}

