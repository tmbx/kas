/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* Handle all workspace property change commands. */
int kcd_misc_cmd_kws_prop_change(struct kcd_kws_cmd_exec_state *ces) {
    int error = 0;
    uint32_t sync_kfs_flag;
    
    anp_write_uint32(&ces->kws_bound_buf, ces->cmd->type);
    error = kcd_kws_cmd_kws_bound_query(ces, "handle_kws_prop_change");
    if (error) return error;
    
    if (anp_read_uint32(&ces->aq.output_buf, &sync_kfs_flag) ||
        anp_read_uint32(&ces->aq.output_buf, &ces->kws->login_type)) return -1;
        
    if (sync_kfs_flag && kcd_exec_kcdhelper("--sync-kfs", ces->kws->kws_id, KCD_LOG_CMD)) return -1;
    
    return 0;
}

/* Handle all grant KCD ticket requests. */
int kcd_misc_cmd_grant_kcd_ticket(struct kcd_kws_cmd_exec_state *ces) {
    int error = 0;
    uint32_t share_id;
    uint64_t session_id;
    struct anp_msg *cmd = ces->cmd, *res = ces->res;
    struct kcd_ticket_mode_dispatch_entry *entry;
    struct kcd_internal_ticket ticket;
    
    kcd_internal_ticket_init(&ticket);
    
    do {
        /* Get the arguments. */
        if (cmd->type == KANP_CMD_KFS_DOWNLOAD_REQ || cmd->type == KANP_CMD_KFS_UPLOAD_REQ) {
            if (anp_read_uint32(&cmd->payload, &share_id)) {
                error = -2;
                break;
            }
            
            anp_write_uint32(&ticket.ext, share_id);
        }
        
        else if (cmd->type == KANP_CMD_VNC_CONNECT_TICKET) {
            if (anp_read_uint64(&cmd->payload, &session_id)) {
                error = -2;
                break;
            }
            
            anp_write_uint64(&ticket.ext, session_id);
        }
        
        /* Get the result type and the ticket type. */
        entry = kcd_ticket_mode_get_dispatch_entry(cmd->type);
        assert(entry);
        res->type = entry->res_type;
        ticket.type = entry->ticket_type;
	
        /* Create the ticket. */
        error = kcd_kws_cmd_create_kcd_ticket(ces, &ticket);
        if (error) break;
        
        /* Write the ticket in the result. */
        anp_write_bin(&res->payload, &ticket.payload);
	
    } while (0);
    
    kcd_internal_ticket_clean(&ticket);
    
    return error;
}

int kcd_misc_cmd_chat_msg(struct kcd_kws_cmd_exec_state *ces) {
    return kcd_kws_cmd_kws_bound_query(ces, "cmd_chat_msg");
}

int kcd_misc_cmd_kws_get_uurl(struct kcd_kws_cmd_exec_state *ces) {
    anp_write_kstr(&ces->kws_bound_buf, &global_opts.web_host);
    return kcd_kws_cmd_kws_bound_query(ces, "cmd_kws_get_uurl");
}

int kcd_misc_cmd_pb_accept_chat(struct kcd_kws_cmd_exec_state *ces) {
    return kcd_kws_cmd_kws_bound_query(ces, "cmd_pb_accept_chat");
}

