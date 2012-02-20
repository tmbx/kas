/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* The connectivity to the online services is unreliable. This value determines
 * how many times KMOD will be launched to contact the KOS.
 */
#define KCD_MGT_NB_KMOD_TICKET_ATTEMPT  3

/* Time to sleep between each attempt above, in microseconds. */
#define KCD_MGT_KMOD_TICKET_SLEEP       30000

/* Call kcd_ask_kmod_about_kws_ticket() in a loop for reliability. */
static int kcd_mgt_loop_ask_kmod_about_kws_ticket(char *ticket_data, int ticket_len, uint64_t key_id, int *valid) {
    int i;
    
    for (i = 0; i < KCD_MGT_NB_KMOD_TICKET_ATTEMPT; i++) {
        int error = kcd_ask_kmod_about_kws_ticket(ticket_data, ticket_len, key_id, valid);
        if (! error) return 0;
        usleep(KCD_MGT_KMOD_TICKET_SLEEP);
    }
    
    return -1;
}

/* Locate the organization specified by the KCD administrator that has the key
 * ID specified, if any.
 */
static kstr* kcd_mgt_get_kcd_org_name_by_key_id(uint64_t key_id) {
    int i = 0;
    for (i = 0; i < global_opts.org_key_id_array.size; i++)
        if (*(uint64_t *) global_opts.org_key_id_array.data[i] == key_id)
            return global_opts.org_name_array.data[i];
    return NULL;
}

/* Get the KCD administrator password. An empty string is set if the password
 * cannot be obtained, e.g. because it wasn't set by the administrator.
 */
static void kcd_mgt_get_kcd_admin_pwd(kstr *pwd) {
    int i;
    kbuffer buf;
    kbuffer_init(&buf);
    kstr_reset(pwd);
    
    if (!kfs_read_file("/etc/teambox/base/admin_pwd", &buf)) {
        for (i = 0; i < (int)buf.len; i++) {
            char c = buf.data[i];
            if (kmod_is_whitespace(c)) break;
            kstr_append_char(pwd, c);
        }
    }
    
    kbuffer_clean(&buf);
}

/* Check if the specified encryption key ID is trusted by the workspace
 * administrator.
 */
static int kcd_mgt_is_kws_key_id_trusted(struct pg_db_conn *conn, uint64_t kws_id, uint64_t key_id, int *trusted_flag) {
    int error = 0;
    kstr query;
    PGresult *pg_res = NULL;
    
    kstr_init(&query);
    
    do {
        kstr_sf(&query, "SELECT key_id FROM kcd_kws_trusted_key "
                        "WHERE kws_id = "PRINTF_64"u AND key_id = "PRINTF_64"u", kws_id, key_id);
        error = kcd_exec_pg_query(conn, query.data, &pg_res, "get trusted key ID");
        if (error) break;
        *trusted_flag = (PQntuples(pg_res) > 0);
        
    } while (0);
    
    kstr_clean(&query);
    pg_db_destroy_res(&pg_res);
    
    return error;
}


/* State used when creating a workspace. */
struct kcd_mgt_create_kws_state {
    kbuffer raw_ticket;
    struct kcd_mgt_user_ticket parsed_ticket;
    kstr *creator_org_name;
    kbuffer reply_buf;
    uint64_t kws_id;
    uint64_t email_id;
};

/* Helper function for kcd_mgt_create_kws(). */
static int kcd_mgt_create_kws_validate_ticket(struct kcd_mgt_create_kws_state *cks) {
    int valid;
    
    /* Parse the raw ticket. */
    if (kcd_mgt_parse_user_ticket(&cks->parsed_ticket, &cks->raw_ticket)) {
        kmod_append_error("malformed ticket");
        return -2;
    }
    
    /* Locate the organization matching the ticket's key ID. */
    cks->creator_org_name = kcd_mgt_get_kcd_org_name_by_key_id(cks->parsed_ticket.key_id);
    
    /* This ticket is REJECTED because it does NOT come from our trusted KPSes. */
    if (!cks->creator_org_name) {
        kmod_set_error("not authorized to create " KCD_KWS_NAME" (ticket has key ID "PRINTF_64"u)",
                       cks->parsed_ticket.key_id);
        return -2;
    }
    
    /* Validate the ticket. */
    if (kcd_mgt_loop_ask_kmod_about_kws_ticket(cks->raw_ticket.data, cks->raw_ticket.len, cks->parsed_ticket.key_id,
                                               &valid)) return -1;
    if (!valid) {
        kmod_set_error("invalid ticket");
        return -2;
    }
    
    return 0;
}

/* This function handles a command to create a workspace. */
int kcd_mgt_create_kws(struct kcd_kws_cmd_exec_state *ces) {
    int error = 0;
    struct kcd_mgt_create_kws_state cks;
    kbuffer *in_buf = &ces->aq.input_buf, *out_buf = &ces->aq.output_buf;
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_create_kws() called.\n");
    
    memset(&cks, 0, sizeof(struct kcd_mgt_create_kws_state));
    kbuffer_init(&cks.raw_ticket);
    kcd_mgt_user_ticket_init(&cks.parsed_ticket);
    kbuffer_init(&cks.reply_buf);
    
    do {
        /* Get the raw ticket. */
        if (anp_msg_get_bin(ces->cmd, 1, &cks.raw_ticket)) {
            error = -2;
            break;
        }
    
        /* Validate the ticket. */
        error = kcd_mgt_create_kws_validate_ticket(&cks);
        if (error) break;
        
        /* Dispatch to the Postgres handler. */
        anp_write_bin(in_buf, &ces->cmd->payload);
        anp_write_uint32(in_buf, ces->client->effective_minor);
        anp_write_kstr(in_buf, cks.creator_org_name);
        anp_write_uint64(in_buf, global_opts.default_kfs_quota);
        anp_write_kstr(in_buf, &global_opts.web_host);
        error = kcd_exec_pg_anp_query(ces->conn, &ces->aq, "cmd_mgt_create_kws");
        if (error) break;
        
        /* Get the reply. */
        if (anp_read_uint32(out_buf, &ces->res->type) ||
            anp_read_bin(out_buf, &ces->res->payload)) {
            error = -1;
            break;
        }
        
    } while (0);
    
    kbuffer_clean(&cks.raw_ticket);
    kcd_mgt_user_ticket_clean(&cks.parsed_ticket);
    kbuffer_clean(&cks.reply_buf);
    
    return error;
}


/* This function handles a command to send the Freemium confirmation email. */
int kcd_mgt_freemium_confirm(struct kcd_kws_cmd_exec_state *ces) {
    int error = 0, failed_flag;
    struct kbuffer *buf = &ces->cmd->payload;
    struct kcd_mail_template tmpl;
    kstr pwd, email, confirm_link, mail_date, mail_id, boundary, mail_content, tmp, tmp2;
    krb_tree var_tree;
    
    kcd_mail_template_init(&tmpl);
    kstr_init(&pwd);
    kstr_init(&email);
    kstr_init(&confirm_link);
    kstr_init(&mail_date);
    kstr_init(&mail_id);
    kstr_init(&boundary);
    kstr_init(&mail_content);
    kstr_init(&tmp);
    kstr_init(&tmp2);
    kcd_mail_var_tree_init(&var_tree);
    
    do {
        /* Retrieve the arguments. */
        if (anp_read_kstr(buf, &pwd) ||
            anp_read_kstr(buf, &email) ||
            anp_read_kstr(buf, &confirm_link)) {
            error = -2;
            break;
        }
        
        /* Validate the password. */
        kcd_mgt_get_kcd_admin_pwd(&tmp);
        
        if (!tmp.slen || !kstr_equal_kstr(&pwd, &tmp)) {
            kmod_set_error("invalid root password");
            error = -2;
            break;
        }
        
        /* Load the template. */
        error = kcd_mail_template_read(&tmpl, "freemium_confirm");
        if (error) break;
        
        /* Generate the system fields. */
        error = kcd_mail_generate_system(&mail_id, &mail_date, &boundary);
        if (error) break;

        /* Generate the notification email content. */
        kcd_mail_var_tree_add(&var_tree, "MailDate", &mail_date);
        kcd_mail_var_tree_add(&var_tree, "MailMessageID", &mail_id);
        
        kstr_assign_cstr(&tmp2, KCD_KWS_NAME " Account Confirmation");
        kcd_mail_fmt_email(&tmp2, &global_opts.mail_sender, &tmp);
        kcd_mail_var_tree_add(&var_tree, "MailFrom", &tmp);
        
        kcd_mail_var_tree_add(&var_tree, "MailTo", &email);
        kcd_mail_var_tree_add(&var_tree, "Boundary", &boundary);
        kcd_mail_var_tree_add(&var_tree, "ConfirmLink", &confirm_link);
        
        error = kcd_mail_template_generate(&tmpl, &var_tree, &mail_content);
        if (error) break;

        /* Send the mail. */
        error = kcd_send_mail(&mail_content, &email, &failed_flag);
        if (error) break;
        
        if (failed_flag) {
            error = -2;
            break;
        }
        
    } while (0);

    kcd_mail_template_clean(&tmpl);
    kstr_clean(&pwd);
    kstr_clean(&email);
    kstr_clean(&confirm_link);
    kstr_clean(&mail_date);
    kstr_clean(&mail_id);
    kstr_clean(&boundary);
    kstr_clean(&mail_content);
    kstr_clean(&tmp);
    kstr_clean(&tmp2);
    kcd_mail_var_tree_clean(&var_tree);

    return error;
}


/* State used when connecting to a workspace. */
struct kcd_mgt_connect_state {
    
    /* Information provided by the user in the command. The workspace and user
     * ID are set in 'kws' to avoid duplicating the information. Some fields
     * are updated during the execution of the command.
     */
    uint32_t user_delete_kws_flag;
    uint64_t user_last_event_id;
    uint64_t user_last_event_date;
    kstr user_name;
    kstr user_email;
    kstr user_email_id;
    kbuffer nonce;
    kbuffer user_ticket;
    kstr user_pwd;
    
    /* Actual (i.e. correct) user password. */
    kstr actual_pwd;
    
    /* Error string set by the call to cmd_mgt_connect_kws(). */
    kstr error_str;
    
    /* Command execution state. */
    struct kcd_kws_cmd_exec_state *ces;
    
    /* Workspace associated to the command. Memory owned by this object. This
     * field is set to NULL when the workspace object is transferred to the
     * command workspace set.
     */
    struct kcd_kws_cmd_kws *kws;
    
    /* Parsed user ticket. */
    struct kcd_mgt_user_ticket parsed_ticket;
    
    /* ID of the last event received on the KCD. */
    uint64_t kcd_last_event_id;
    
    /* Login result code. 0 if not set yet. */
    uint32_t login_code;
    
    /* True if a choose user ID reply must be sent. */
    uint32_t choose_user_id_flag;
    
    /* True if a permission denied reply must be sent. */
    uint32_t perm_denied_flag;
    
    /* True if the ticket provided was found in the cache. */
    uint32_t ticket_cached_flag;
    
    /* True if the ticket provided was validated. */
    uint32_t ticket_validated_flag;
    
    /* True if the workspace is secure. */
    uint32_t secure_kws_flag;
    
    /* True if the workspace is in V2 compatibility mode. */
    uint32_t compat_v2_flag;
    
    /* True if the user is registered. */
    uint32_t registered_flag;
};
    
static void kcd_mgt_connect_state_init(struct kcd_mgt_connect_state *self, struct kcd_kws_cmd_exec_state *ces) {
    memset(self, 0, sizeof(struct kcd_mgt_connect_state));
    kstr_init(&self->user_name);
    kstr_init(&self->user_email);
    kstr_init(&self->user_email_id);
    kbuffer_init(&self->nonce);
    kbuffer_init(&self->user_ticket);
    kstr_init(&self->user_pwd);
    kstr_init(&self->actual_pwd);
    kstr_init(&self->error_str);
    self->ces = ces;
    self->kws = kcd_kws_cmd_kws_new();
    ces->kws = self->kws;
    kcd_mgt_user_ticket_init(&self->parsed_ticket);
}

static void kcd_mgt_connect_state_clean(struct kcd_mgt_connect_state *self) {
    kstr_clean(&self->user_name);
    kstr_clean(&self->user_email);
    kstr_clean(&self->user_email_id);
    kbuffer_clean(&self->nonce);
    kbuffer_clean(&self->user_ticket);
    kstr_clean(&self->user_pwd);
    kstr_clean(&self->actual_pwd);
    kstr_clean(&self->error_str);
    kcd_kws_cmd_kws_destroy(self->kws);
    kcd_mgt_user_ticket_clean(&self->parsed_ticket);
}
        
/* Parse the connect command arguments. */
static int kcd_mgt_login_parse_cmd(struct kcd_mgt_connect_state *cs) {
    uint32_t m, minor = cs->ces->client->effective_minor;
    struct kbuffer *buf = &cs->ces->cmd->payload;
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_parse_cmd() called.\n");
    
    if (anp_read_uint64(buf, &cs->kws->kws_id) ||
        (minor >= 4 && anp_read_uint32(buf, &cs->user_delete_kws_flag)) ||
        anp_read_uint64(buf, &cs->user_last_event_id) ||
        anp_read_uint64(buf, &cs->user_last_event_date) ||
        anp_read_uint32(buf, &cs->kws->user_id) ||
        anp_read_kstr(buf, &cs->user_name) ||
        anp_read_kstr(buf, &cs->user_email) ||
        (minor <= 2 && anp_read_bin(buf, &cs->nonce)) ||
        (minor <= 2 && anp_read_uint32(buf, &m)) ||
        (minor >= 3 && anp_read_kstr(buf, &cs->user_email_id)) ||
        ((minor >= 3 || m) && anp_read_bin(buf, &cs->user_ticket)) ||
        ((minor >= 3 || !m) && anp_read_kstr(buf, &cs->user_pwd))) {
        return -2;
    }
    
    return 0;
}

/* Get the login type and validate the privileged login credentials, if any. */
static int kcd_mgt_login_get_login_type(struct kcd_mgt_connect_state *cs) {
    int *login_type = &cs->kws->login_type;
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_get_login_type() called.\n");
    
    /* Get the login type. */
    if (kstr_equal_cstr(&cs->user_email_id, "admin")) *login_type = KCD_KWS_LOGIN_TYPE_ROOT;
    else if (kstr_equal_cstr(&cs->user_email_id, "kwmo")) *login_type = KCD_KWS_LOGIN_TYPE_KWMO;
    else *login_type = KCD_KWS_LOGIN_TYPE_NORMAL;
    
    /* Verify the privileged login password. */
    if (*login_type == KCD_KWS_LOGIN_TYPE_ROOT || *login_type == KCD_KWS_LOGIN_TYPE_KWMO) {
        kstr *pwd = &cs->actual_pwd;
        kcd_mgt_get_kcd_admin_pwd(pwd);
        
        if (!pwd->slen) {
            kmod_set_error("the administration password is not set");
            return -2;
        }

        if (!kstr_equal_kstr(pwd, &cs->user_pwd)) {
            kmod_set_error("invalid administration password");
            return -2;
        }
    }
    
    return 0;
}

/* Validate the login password. */
static void kcd_mgt_login_check_pwd(struct kcd_mgt_connect_state *cs, int *valid) {
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_check_pwd() called.\n");
    
    if (kstr_equal_kstr(&cs->user_pwd, &cs->actual_pwd)) {
        kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_check_pwd: password accepted.\n");
        *valid = 1;
    }
    
    else {
        kmod_set_error("bad password");
        kmod_log_msg(KCD_LOG_BRIEF, "kcd_mgt_login_check_pwd: bad password.\n");
        *valid = 0;
    }
}

/* Validate the login ticket. */
static int kcd_mgt_login_check_ticket(struct kcd_mgt_connect_state *cs) {
    int valid;
    uint64_t key_id;
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_check_ticket() called.\n");
    
    /* Ticket was cached. */
    if (cs->ticket_cached_flag) {
        kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_check_ticket: ticket already verified.\n");
        return 0;
    }
    
    /* Parse the ticket. */
    if (kcd_mgt_parse_user_ticket(&cs->parsed_ticket, &cs->user_ticket)) {
        kmod_append_error("invalid ticket format");
        kmod_log_msg(KCD_LOG_BRIEF, "kcd_mgt_login_check_ticket: %s.\n", kmod_strerror());
        return 0;
    }
    
    key_id = cs->parsed_ticket.key_id;
    
    /* Validate the email address. */
    if (strcasecmp(cs->user_email.data, cs->parsed_ticket.email.data)) {
        kmod_set_error("ticket email address is %s, expected %s", cs->parsed_ticket.email.data,
                                                                  cs->user_email.data);
        kmod_log_msg(KCD_LOG_BRIEF, "kcd_mgt_login_check_ticket: %s.\n", kmod_strerror());
        return 0;
    }
    
    /* We trust the key ID because it was specified by the KCD administrator. */
    if (kcd_mgt_get_kcd_org_name_by_key_id(key_id)) {
        kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_check_ticket: trusting key ID (KCD).\n");
    }
    
    /* Check if the key ID is trusted by the workspace administrators. */
    else {
        int trusted_flag;
        
        if (kcd_mgt_is_kws_key_id_trusted(cs->ces->conn, cs->kws->kws_id, key_id, &trusted_flag))
            return -1;
    
        else if (trusted_flag) {
            kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_check_ticket: trusting key ID (workspace).\n");
        }
            
        else {
            kmod_set_error("untrusted key ID");
            kmod_log_msg(KCD_LOG_BRIEF, "kcd_mgt_login_check_ticket: untrusted key ID ("PRINTF_64"u).\n", key_id);
            return 0;
        }
    }

    /* Validate the ticket. */
    if (kcd_mgt_loop_ask_kmod_about_kws_ticket(cs->user_ticket.data, cs->user_ticket.len, key_id, &valid)) {
        return -1;
    }
    
    else if (!valid) {
        kmod_set_error("invalid ticket");
        kmod_log_msg(KCD_LOG_BRIEF, "kcd_mgt_login_check_ticket: invalid ticket.\n");
        return 0;
    }
    
    else {
        kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_check_ticket: ticket validated.\n");
        cs->ticket_validated_flag = 1;
        return 0;
    }
}
        
/* Verify the login security credentials. */
static int kcd_mgt_login_check_security(struct kcd_mgt_connect_state *cs) {
    int error = 0, valid = 0;
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_check_security() called.\n");
    
    /* Validate the login password. */
    if (!valid && cs->user_pwd.slen) {
        kcd_mgt_login_check_pwd(cs, &valid);
    }
    
    /* Validate the login ticket. */
    if (!valid && cs->user_ticket.len) {
        error = kcd_mgt_login_check_ticket(cs);
        if (error) return error;
        valid = cs->ticket_cached_flag || cs->ticket_validated_flag;
    }
    
    /* Refuse the login. */
    if (!valid) {
        
        /* Format a decent error string, if required. */
        int nb_option = (cs->user_pwd.slen > 0) + (cs->user_ticket.len > 0);
        if (nb_option == 0) { kmod_set_error("security credentials required"); }
        else if (nb_option == 2) { kmod_set_error("security credentials refused"); }
        cs->login_code = KANP_KWS_LOGIN_BAD_PWD_OR_TICKET;
        
        kmod_log_msg(KCD_LOG_BRIEF, "kcd_mgt_login_check_security(): all authentication options failed.\n");
    }
    
    return 0;
}

/* Check the login credentials and perform the relevant modifications to the
 * database.
 */
static int kcd_mgt_login_job(struct kcd_mgt_connect_state *cs) {
    int error = 0;
    struct kcd_kws_cmd_exec_state *ces = cs->ces;
    kbuffer *in_buf = &ces->aq.input_buf, *out_buf = &ces->aq.output_buf;
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_check() called.\n");
    
    /* Parse the connect command arguments. */
    error = kcd_mgt_login_parse_cmd(cs);
    if (error) return error;
    
    /* Check if the user is already logged in if the workspace is not being
     * deleted.
     */
    if (!cs->user_delete_kws_flag && kcd_kws_cmd_get_kws_by_id(ces->kws_tree, cs->kws->kws_id)) {
        kmod_set_error("already logged to "KCD_KWS_NAME" "PRINTF_64"u", cs->kws->kws_id);
        return -2;
    }

    /* Get the login type. */
    error = kcd_mgt_login_get_login_type(cs);
    if (error) return error;
    
    /* Check the login credentials in Postgres. */
    anp_write_uint64(in_buf, cs->kws->kws_id);
    anp_write_uint32(in_buf, cs->user_delete_kws_flag);
    anp_write_uint32(in_buf, cs->kws->login_type);
    anp_write_uint32(in_buf, cs->kws->user_id);
    anp_write_kstr(in_buf, &cs->user_email);
    anp_write_kstr(in_buf, &cs->user_email_id);
    anp_write_bin(in_buf, &cs->user_ticket);
    anp_write_uint64(in_buf, cs->user_last_event_id);
    anp_write_uint64(in_buf, cs->user_last_event_date);
    
    error = kcd_exec_pg_anp_query(ces->conn, &ces->aq, "cmd_mgt_connect_kws");
    if (error) return error;
    
    if (anp_read_uint64(out_buf, &cs->kcd_last_event_id) ||
        anp_read_uint32(out_buf, &cs->login_code) ||
        anp_read_uint32(out_buf, &cs->choose_user_id_flag) ||
        anp_read_uint32(out_buf, &cs->perm_denied_flag) ||
        anp_read_uint32(out_buf, &cs->ticket_cached_flag) ||
        anp_read_uint32(out_buf, &cs->secure_kws_flag) ||
        anp_read_uint32(out_buf, &cs->compat_v2_flag) ||
        anp_read_uint32(out_buf, &cs->registered_flag) ||
        anp_read_uint32(out_buf, &cs->kws->user_id) ||
        anp_read_kstr(out_buf, &cs->user_email_id) ||
        anp_read_kstr(out_buf, &cs->actual_pwd) ||
        anp_read_kstr(out_buf, &cs->error_str)) {
        return -1;
    }
    
    /* Import the error string. */
    kmod_set_error("%s", cs->error_str.data);
    
    /* We're done. */
    if (cs->login_code || cs->choose_user_id_flag || cs->perm_denied_flag) return 0;
    
    /* Verify the login security credentials of normal users. */
    if (cs->secure_kws_flag && cs->kws->login_type == KCD_KWS_LOGIN_TYPE_NORMAL) {
    
        /* Upgrade the login type. */
        cs->kws->login_type = KCD_KWS_LOGIN_TYPE_SECURE;
        
        /* Check the credentials. */
        error = kcd_mgt_login_check_security(cs);
        if (error || cs->login_code) return error;
    }
    
    /* Delete the workspace if requested. */
    if (cs->user_delete_kws_flag) {
        cs->login_code = KANP_KWS_LOGIN_DELETED_KWS;
        return kcd_exec_kcdhelper("--delete-kws", cs->kws->kws_id, KCD_LOG_CMD);
    }
    
    /* Store the ticket if we have validated it and the user is a regular user. */
    if (cs->ticket_validated_flag && cs->kws->user_id) {
        anp_write_bin(&ces->kws_bound_buf, &cs->user_ticket);
        error = kcd_kws_cmd_kws_bound_query(ces, "store_kws_user_ticket");
        if (error) return error;
    }
    
    /* Register the user if he is unregistered and the user is regular user. */
    if (!cs->registered_flag && cs->kws->user_id) {
        anp_write_kstr(&ces->kws_bound_buf, &cs->user_name);
        error = kcd_kws_cmd_kws_bound_query(ces, "register_kws_user");
        if (error) return error;
    }
    
    /* Login credentials accepted. */
    cs->login_code = KANP_KWS_LOGIN_OK;
    kmod_set_error("login successful");
    return 0;
}

/* Format a choose user ID reply. */
static int kcd_mgt_login_choose_user_id_reply(struct kcd_mgt_connect_state *cs) {
    int error = 0;
    uint32_t i, nb_user;
    kstr *query = &cs->ces->query;
    PGresult *pg_res = NULL;
    struct anp_msg *res = cs->ces->res;
    kbuffer *res_buf = &res->payload;
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_choose_user_id_reply() called.\n");
    
    do {
	kmod_set_error("cannot identify user");
	kcd_kanp_set_failure(res, KANP_RES_FAIL_CHOOSE_USER_ID);
    
	kstr_sf(query, "SELECT user_id, name_admin, email FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u",
                       cs->kws->kws_id);
	error = kcd_exec_pg_query(cs->ces->conn, query->data, &pg_res, "get workspace users");
	if (error) break;
        
        nb_user = PQntuples(pg_res);
	anp_write_uint32(res_buf, nb_user);
        
        for (i = 0; i < nb_user; i++) {
	    anp_write_uint32(res_buf, pg_db_get_uint32(pg_res, i, 0));
	    anp_write_cstr(res_buf, PQgetvalue(pg_res, i, 1));
	    anp_write_cstr(res_buf, PQgetvalue(pg_res, i, 2));
        }
	
    } while (0);
    
    pg_db_destroy_res(&pg_res);
    
    return error;
}

/* Format the compatibility login reply. */
static int kcd_mgt_login_handle_compat_reply(struct kcd_mgt_connect_state *cs) {
    int c = cs->login_code;
    struct anp_msg *res = cs->ces->res;
    kbuffer *res_buf = &res->payload;
    assert(c);
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_handle_compat_reply() called.\n");
    
    if (c == KANP_KWS_LOGIN_OK) {
	res->type = KANP_RES_KWS_CONNECT_KWS;
        anp_write_uint32(res_buf, cs->kws->user_id);
        anp_write_uint64(res_buf, cs->kcd_last_event_id);
    }
    
    else if (c == KANP_KWS_LOGIN_OOS)
	kcd_kanp_set_failure(res, KANP_RES_FAIL_EVT_OUT_OF_SYNC);
    
    else
	kcd_kanp_set_failure(res, KANP_RES_FAIL_GEN);
    
    return 0;
}

/* Format the login reply. */
static void kcd_mgt_login_handle_reply(struct kcd_mgt_connect_state *cs) {
    int c = cs->login_code;
    struct anp_msg *res = cs->ces->res;
    kbuffer *res_buf = &res->payload;
    assert(c);
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_login_handle_reply() called.\n");
    
    res->type = KANP_RES_KWS_CONNECT_KWS;
    anp_write_uint32(res_buf, c);
    anp_write_kstr(res_buf, kmod_kstrerror());
    
    if (c == KANP_KWS_LOGIN_OK || c == KANP_KWS_LOGIN_OOS || c == KANP_KWS_LOGIN_BAD_PWD_OR_TICKET) {
        anp_write_uint32(res_buf, cs->kws->user_id);
        if (c == KANP_KWS_LOGIN_OK) anp_write_kstr(res_buf, &cs->user_email_id);
        else anp_write_cstr(res_buf, "");
        anp_write_uint64(res_buf, cs->kcd_last_event_id);
        anp_write_uint32(res_buf, cs->secure_kws_flag);
        anp_write_uint32(res_buf, (cs->actual_pwd.slen > 0));
        anp_write_kstr(res_buf, &global_opts.web_host);
    }
    
    else {
        anp_write_uint32(res_buf, 0);
        anp_write_cstr(res_buf, "");
        anp_write_uint64(res_buf, 0);
        anp_write_uint32(res_buf, 0);
        anp_write_uint32(res_buf, 0);
        anp_write_cstr(res_buf, "");
    }
}

/* This function handles a command to connect to a workspace. */
int kcd_mgt_connect_kws(struct kcd_kws_cmd_exec_state *ces) {
    int error = 0;
    struct kcd_mgt_connect_state cs;
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_connect_kws() called.\n");
    
    kcd_mgt_connect_state_init(&cs, ces);
    
    do {
        /* Check the login credentials and perform the relevant modifications to
         * the database.
         */
        error = kcd_mgt_login_job(&cs);
        if (error) break;

        /* Format the login reply. */
        if (cs.choose_user_id_flag) {
            error = kcd_mgt_login_choose_user_id_reply(&cs);
            if (error) break;
        }
        
        else if (cs.perm_denied_flag) {
            kmod_set_error("administrator privilege required");
            kcd_kanp_set_perm_failure(ces->res);
        }
    
        else if (ces->client->effective_minor <= 2) {
            error = kcd_mgt_login_handle_compat_reply(&cs);
            if (error) break;
        }
        
        else {
            kcd_mgt_login_handle_reply(&cs);
        }

        /* Accept the login. */
        if (cs.login_code == KANP_KWS_LOGIN_OK) {
            kcd_kws_cmd_add_kws(ces->st, cs.kws, cs.user_last_event_id);
            cs.kws = NULL;
        }
        
    } while (0);
    
    kcd_mgt_connect_state_clean(&cs);
    
    return error;
}


/* This function handles a command to disconnect the user from a workspace. */
int kcd_mgt_disconnect_kws(struct kcd_kws_cmd_exec_state *ces) {
    int error = 0;
    uint64_t kws_id;
    struct kcd_kws_cmd_kws *kws;
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_disconnect_kws() called.\n");
    
    do {
	/* Parse the command payload. */
    	if (anp_read_uint64(&ces->cmd->payload, &kws_id)) {
	    error = -2;
	    break;
	}
    
        /* Get the workspace the user is connected to, if any. */
        kws = kcd_kws_cmd_get_kws_by_id(ces->kws_tree, kws_id);
        if (!kws) break;
	
	/* Disconnect the user from the workspace. */
        kcd_kws_cmd_remove_kws(ces->st, kws);
	
    } while (0);
	
    return error;
}


/* Represent a user being invited to a workspace. */
struct kcd_mgt_invite_kws_user {
    uint32_t send_email_flag;
    kstr name;
    kstr email;
    kstr email_id;
    kstr url;
    kstr invite_error;
};

static struct kcd_mgt_invite_kws_user* kcd_mgt_invite_kws_user_new() {
    struct kcd_mgt_invite_kws_user *self = kcalloc(sizeof(struct kcd_mgt_invite_kws_user));
    kstr_init(&self->name);
    kstr_init(&self->email);
    kstr_init(&self->email_id);
    kstr_init(&self->url);
    kstr_init(&self->invite_error);
    return self;
}

static void kcd_mgt_invite_kws_user_destroy(struct kcd_mgt_invite_kws_user *self) {
    if (self) {
        kstr_clean(&self->name);
        kstr_clean(&self->email);
        kstr_clean(&self->email_id);
        kstr_clean(&self->url);
        kstr_clean(&self->invite_error);
        free(self);
    }
}

/* This function handles a command to invite users in a workspace. */
int kcd_mgt_invite_kws(struct kcd_kws_cmd_exec_state *ces) {
    int error = 0, i;
    uint32_t nb_user;
    uint64_t kws_id = ces->kws->kws_id;
    struct kcd_mail_invite_user_state is;
    karray user_array;
    kstr web_link;
    kstr compat_web_link;
    kstr wleu_email_id;
    kbuffer nonce;
    kbuffer *kbb = &ces->kws_bound_buf, *out_buf = &ces->aq.output_buf;
    
    kmod_log_msg(KCD_LOG_CMD, "kcd_mgt_invite_kws() called.\n");
    
    kcd_mail_invite_user_state_init(&is);
    karray_init(&user_array);
    kstr_init(&web_link);
    kstr_init(&compat_web_link);
    kstr_init(&wleu_email_id);
    kbuffer_init(&nonce);
    
    do {
        /* Dispatch to the Postgres handler. */
        anp_write_kstr(kbb, &global_opts.mail_sender);
        error = kcd_kws_cmd_kws_bound_query(ces, "cmd_mgt_invite_kws");
        if (error) break;
        
        if (anp_read_kstr(out_buf, &is.kws_name) ||
            anp_read_kstr(out_buf, &is.from_name) ||
            anp_read_kstr(out_buf, &is.from_email) ||
            anp_read_kstr(out_buf, &is.user_msg) ||
            anp_read_uint32(out_buf, &nb_user)) {
            error = -1;
            break;
        }
        
        for (i = 0; i < (int)nb_user; i++) {
            struct kcd_mgt_invite_kws_user *iu = kcd_mgt_invite_kws_user_new();
            karray_push(&user_array, iu);
            
            if (anp_read_uint32(out_buf, &iu->send_email_flag) ||
                anp_read_kstr(out_buf, &iu->name) ||
                anp_read_kstr(out_buf, &iu->email) ||
                anp_read_kstr(out_buf, &iu->email_id)) {
                error = -1;
                break;
            }
        }
            
        if (error) break;
        
        /* Process the users. */
        for (i = 0; i < (int)nb_user; i++) {
            struct kcd_mgt_invite_kws_user *iu = user_array.data[i];
            
            /* Format the invitation URL. */
            if (ces->client->effective_minor >= 3) {
                kstr_sf(&iu->url, "https://%s/teambox/i/"PRINTF_64"u/%s",
                        global_opts.web_host.data, kws_id, iu->email_id.data);
            }
            
            /* Send the invitation email. */
            if (iu->send_email_flag) {
                int failed_flag;
                
                is.to_name = &iu->name;
                is.to_email = &iu->email;
                is.url = &iu->url;
                is.invite_error = &iu->invite_error;
                
                error = kcd_mail_invite_user(&is, &failed_flag);
                if (error) break;
                
                if (failed_flag) {
                    kmod_log_msg(KCD_LOG_BRIEF, "Failed to send invitation mail for user %s: %s\n",
                                                iu->email.data, iu->invite_error.data);
                }
            }
        }
        
        if (error) break;
        
        /* Format the reply. */
        ces->res->type = KANP_RES_KWS_INVITE_KWS;
        out_buf = &ces->res->payload;
        
        if (ces->client->effective_minor <= 2) {
            anp_write_bin(out_buf, &nonce);
	    kstr_sf(&web_link, "https://%s/?kws_id="PRINTF_64"u", global_opts.web_host.data, kws_id);
	    kstr_sf(&compat_web_link, global_opts.web_link.data, web_link.data);
            anp_write_kstr(out_buf, &compat_web_link);
        }
        
        else {
            /* We do not store the WLEU email ID at this time since we don't need it. */
            kcd_mgt_generate_email_id(&wleu_email_id);
            kstr_sf(&web_link, "https://%s/teambox/s/"PRINTF_64"u/%s", global_opts.web_host.data, kws_id,
                                                                       wleu_email_id.data);
            anp_write_kstr(out_buf, &web_link);
            anp_write_uint32(out_buf, nb_user);
            
            for (i = 0; i < (int)nb_user; i++) {
                struct kcd_mgt_invite_kws_user *iu = user_array.data[i];
                anp_write_kstr(out_buf, &iu->email_id);
                anp_write_kstr(out_buf, &iu->url);
                anp_write_kstr(out_buf, &iu->invite_error);
            }
        }
        
    } while (0);
    
    kcd_mail_invite_user_state_clean(&is);
    for (i = 0; i < user_array.size; i++) kcd_mgt_invite_kws_user_destroy(user_array.data[i]);
    karray_clean(&user_array);
    kstr_clean(&web_link);
    kstr_clean(&compat_web_link);
    kstr_clean(&wleu_email_id);
    kbuffer_clean(&nonce);
    
    return error;
}

