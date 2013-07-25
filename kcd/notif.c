/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* Number of seconds between attempts to reconnect to the database. */
#define KCD_NOTIF_RECONNECT_DELAY           10

/* Number of seconds between chat notifications. */
#define KCD_NOTIF_CHAT_DELAY                (5*60)

/* Maximum length of a chat message. */
#define KCD_NOTIF_MAX_CHAT_LENGTH           256

/* Maximum number of file uploads in a notification. */
#define KCD_NOTIF_KCD_NOTIF_MAX_FILE_UPLOAD 10

/* Maximum number of chat messages, file uploads, file downloads and VNC sessions
 * in a single workspace of a summary.
 */
#define KCD_NOTIF_KWS_MAX_CHAT              20
#define KCD_NOTIF_KWS_MAX_FILE_UPLOAD       20
#define KCD_NOTIF_KWS_MAX_FILE_DOWNLOAD     20
#define KCD_NOTIF_KWS_MAX_VNC               20

/* Directory where the summary messages are stored. Must end with a slash. */
#define KCD_NOTIF_SUMMARY_DIR               "/tmp/kws_summary/"

/* State used in notification mode. */
struct kcd_notif_state {

    /* True if the database connection is open. */
    int conn_flag;

    /* Time in seconds at which a database connection attempt was made. */
    int64_t conn_time;

    /* Time in seconds at which the next summary must be sent. */
    int64_t summary_time;

    /* ID of the last workspace fetched. */
    uint64_t last_kws_id;

    /* Tree of workspaces indexed by workspace ID. */
    krb_tree kws_tree;

    /* Template for notifications and summaries. */
    struct kcd_mail_template notif_tmpl;

    /* Connection to the database. */
    struct pg_db_conn conn;
};

/* Represent the view of a workspace used in notification mode. */
struct kcd_notif_kws {

    /* ID of the workspace. */
    uint64_t kws_id;

    /* Last notification ID received from that workspace. */
    uint64_t last_notif_id;

    /* Time at which the last chat notification was sent. */
    int64_t last_chat_time;

    /* Flags of the workspace. */
    uint32_t flags;
    
    /* Total number of events. Used during summary generation. */
    uint32_t event_total;

    /* Name of the workspace. */
    kstr name;

    /* Array of users of the workspace. The system user is not included. */
    karray user_array;
};

/* Represent the view of a user of a workspace. */
struct kcd_notif_kws_user {

    /* User ID. */
    uint32_t user_id;
    
    /* User flags. */
    uint32_t flags;

    /* Workspace email notification flags. */
    uint32_t notif_policy;

    /* User email address. */
    kstr email;

    /* Name of the user. Empty if not provided. */
    kstr name;

    /* System invitation email ID of the user if this is not a public workspace,
     * if any.
     */
    kstr system_email_id;
};

/* Represent a notification associated to a workspace. */
struct kcd_notif_kws_notif {

    /* Workspace ID. */
    uint64_t kws_id;

    /* Notification ID. */
    uint64_t notif_id;

    /* Date at which the notification was generated (seconds since UNIX epoch). */
    uint64_t date;

    /* ID of the user who triggered the notification. */
    uint32_t user_id;

    /* Notification type. */
    uint32_t type;

    /* Notification payload. */
    kbuffer payload;
};

/* Represent the payload-specific data of a notification. The fields are
 * allocated as needed.
 */
struct kcd_notif_kws_notif_data {

    /* Email ID of the public workspace. */
    uint64_t public_email_id;

    /* Path to the files that have been created or updated. */
    karray *upload_array;

    /* Path to the file that has been downloaded. */
    kstr *download_path;

    /* Subject of the email referenced in a public workspace. */
    kstr *mail_subject;

    /* Subject of the VNC session. */
    kstr *vnc_subject;

    /* Chat message. */
    kstr *chat_msg;

    /* Pointer to the user who has generated the notification, if any. */
    struct kcd_notif_kws_user *user;
    
    /* HTML version of the workspace name. */
    kstr html_kws_name;

    /* Name of the user who has generated the notification. */
    kstr user_name;
    
    /* Nice HTML version of user_name. */
    kstr html_user_name;
    
    /* Timestamp. */
    kstr timestamp;
};

/* Represent a global user of the KCD. */
struct kcd_notif_global_user {
    
    /* Address of the user. */
    kstr email;
    
    /* Array of workspaces of the user for which summaries must be sent. */
    karray kws_array;
    
    /* Email ID of the user for each workspace. */
    karray email_id_array;
};

static char *notif_body_format =
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
"\n"
"<html>\n"
"  <head>\n"
"%s"
"    <style type=\"text/css\">\n"
"      body\n"
"      {\n"
"      font: 12px Verdana, Arial, Helvetica;\n"
"      font-weight: normal;\n"
"      background-color: #ffffff;\n"
"      }\n"
"      .bounding_box\n"
"      {\n"
"      font: 16px Verdana, Arial, Helvetica;\n"
"      font-weight: bold;\n"
"      border: 1px solid #000000;\n"
"      background: #cccccc;\n"
"      min-height: 40px;\n"
"      min-width: 60%;\n"
"      max-width: 90%;\n"
"      }\n"
"      .event_text\n"
"      {\n"
"      text-align: left;\n"
"      margin: 3px;\n"
"      }\n"
"      .content\n"
"      {\n"
"      font-size: 14px;\n"
"      min-height: 60px;\n"
"      }\n"
"      .content_text\n"
"      {\n"
"      margin: 5px;\n"
"      }\n"
"      .teambox_text\n"
"      {\n"
"      font-size: 14px;\n"
"      margin: 3px;\n"
"      }\n"
"      .footer\n"
"      {\n"
"      font-size: 10px;\n"
"      margin: 5px;\n"
"      }\n"
"    </style>\n"
"  </head>\n"
"  <body>\n"
"    <div class=\"bounding_box\">\n"
"      <div class=\"event_text\">\n"
"%s"
"      </div>\n"
"    </div>\n"
"\n"
"    <div class=\"content\">\n"
"      <div class=\"content_text\">\n"
"	Details:\n"
"%s"
"      </div>\n"
"    </div>\n"
"\n"
"    <div class=\"bounding_box\">\n"
"      <div class=\"teambox_text\">\n"
"	Access the Teambox and/or change your notification settings here:\n"
"	<br>\n"
"%s"
"      </div>\n"
"    </div>\n"
"\n"
"    <div class=\"footer\">\n"
"      This email automatically sent by Teambox&trade;. &copy; 2009-2012, <a href=\"http://www.teambox.co\">Opersys inc.</a>\n"
"    </div>\n"
"  </body>\n"
"</html>\n";

static char *summary_body_format = 
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
"\n"
"<html>\n"
"  <head>\n"
"    <title>Teambox Activity Summary</title>\n"
"    <style type=\"text/css\">\n"
"      body\n"
"      {\n"
"      font: 12px Verdana, Arial, Helvetica;\n"
"      font-weight: normal;\n"
"      background-color: #ffffff;\n"
"      }\n"
"      .bounding_box\n"
"      {\n"
"      font: 16px Verdana, Arial, Helvetica;\n"
"      font-weight: bold;\n"
"      border: 1px solid #000000;\n"
"      background: #cccccc;\n"
"      min-height: 40px;\n"
"      min-width: 60%;\n"
"      max-width: 90%;\n"
"      }\n"
"      .event_text\n"
"      {\n"
"      text-align: left;\n"
"      margin: 3px;\n"
"      }\n"
"      .content\n"
"      {\n"
"      font-size: 14px;\n"
"      min-height: 60px;\n"
"      }\n"
"      .content_text\n"
"      {\n"
"      margin: 5px;\n"
"      }\n"
"      .teambox_text\n"
"      {\n"
"      font-size: 14px;\n"
"      margin: 3px;\n"
"      }\n"
"      .footer\n"
"      {\n"
"      font-size: 10px;\n"
"      margin: 5px;\n"
"      }\n"
"    </style>\n"
"  </head>\n"
"  <body>\n"
"    <div class=\"bounding_box\">\n"
"      <div class=\"event_text\">\n"
"	Teambox&trade; activity summary for the past 24 hours\n"
"      </div>\n"
"    </div>\n"
"\n"
"    <div class=\"content\">\n"
"      <div class=\"content_text\">\n"
"%s"
"      </div>\n"
"    </div>\n"
"\n"
"    <hr size=1 width=\"90%\" align=\"left\" color=\"#000000\">\n"
"    <br>\n"
"%s"
"    <div class=\"footer\">\n"
"      This email automatically sent by Teambox&trade;. &copy; 2009-2012, <a href=\"http://www.teambox.co\">Opersys inc.</a>\n"
"    </div>\n"
"  </body>\n"
"</html>\n";

static char *summary_workspace_format =
"    <div class=\"bounding_box\">\n"
"      <div class=\"event_text\">\n"
"%s"
"      </div>\n"
"    </div>\n"
"\n"
"    <div class=\"content\">\n"
"      <div class=\"content_text\">\n"
"%s"
"      </div>\n"
"    </div>\n"
"\n"
"    <div class=\"bounding_box\">\n"
"      <div class=\"teambox_text\">\n"
"	Access this Teambox and/or change its notification settings here:\n"
"	<br>\n"
"%s"
"      </div>\n"
"    </div>\n"
"\n"
"    <br clear=\"all\">\n"
"    <hr size=1 width=\"90%\" align=\"left\" color=\"#000000\">\n";
    
/* Contain the summary information of a workspace. */
struct kcd_notif_kws_summary {
    
    /* Total number of chat messages, file uploads and VNC sessions.
     */
    uint32_t chat_total, upload_total, vnc_total;
    
    /* Chat, upload, download and VNC lines. */
    kstr chat_lines, upload_lines, download_lines, vnc_lines;
};

static struct kcd_notif_kws_user* kcd_notif_kws_user_new() {
    struct kcd_notif_kws_user *self = kcalloc(sizeof(struct kcd_notif_kws_user));
    kstr_init(&self->email);
    kstr_init(&self->name);
    kstr_init(&self->system_email_id);
    return self;
}

static void kcd_notif_kws_user_destroy(struct kcd_notif_kws_user *self) {
    if (self) {
        kstr_clean(&self->email);
        kstr_clean(&self->name);
        kstr_clean(&self->system_email_id);
        kfree(self);
    }
}

static struct kcd_notif_kws* kcd_notif_kws_new() {
    struct kcd_notif_kws *self = kcalloc(sizeof(struct kcd_notif_kws));
    kstr_init(&self->name);
    karray_init(&self->user_array);
    return self;
}

/* Clear the users of the workspace. */
static void kcd_notif_kws_clear_user_array(struct kcd_notif_kws *self) {
    int i;
    for (i = 0; i < self->user_array.size; i++) kcd_notif_kws_user_destroy(self->user_array.data[i]);
    karray_reset(&self->user_array);
}

static void kcd_notif_kws_destroy(struct kcd_notif_kws *self) {
    if (self) {
        kstr_clean(&self->name);
        kcd_notif_kws_clear_user_array(self);
        karray_clean(&self->user_array);
        kfree(self);
    }
}

static void kcd_notif_state_init(struct kcd_notif_state *self) {
    memset(self, 0, sizeof(struct kcd_notif_state));
    krb_tree_init_func(&self->kws_tree, krb_tree_uint64_cmp);
    kcd_mail_template_init(&self->notif_tmpl);
    pg_db_conn_init(&self->conn);
}

/* Clear the workspace tree and the last workspace ID. */
static void kcd_notif_state_clear_kws_tree(struct kcd_notif_state *self) {
    int i, size = krb_tree_size(&self->kws_tree);
    struct krb_node *iter = krb_tree_iter_start(&self->kws_tree);
    for (i = 0; i < size; i++) kcd_notif_kws_destroy(krb_tree_iter_next(&self->kws_tree, &iter));
    krb_tree_reset(&self->kws_tree);
    self->last_kws_id = 0;
}

static void kcd_notif_state_clean(struct kcd_notif_state *self) {
    kcd_notif_state_clear_kws_tree(self);
    krb_tree_clean(&self->kws_tree);
    kcd_mail_template_clean(&self->notif_tmpl);
    pg_db_conn_clean(&self->conn);
}

static struct kcd_notif_kws_notif* kcd_notif_kws_notif_new() {
    struct kcd_notif_kws_notif *self = kcalloc(sizeof(struct kcd_notif_kws_notif));
    kbuffer_init(&self->payload);
    return self;
}

static void kcd_notif_kws_notif_destroy(struct kcd_notif_kws_notif *self) {
    if (self) {
        kbuffer_clean(&self->payload);
        kfree(self);
    }
}

/* Free the notifications contained in the array specified and reset the array. */
static void kcd_notif_clear_notif_array(karray *array) {
    int i;
    for (i = 0; i < array->size; i++) kcd_notif_kws_notif_destroy(array->data[i]);
    karray_reset(array);
}

static void kcd_notif_kws_notif_data_init(struct kcd_notif_kws_notif_data *self) {
    memset(self, 0, sizeof(struct kcd_notif_kws_notif_data));
    kstr_init(&self->html_kws_name);
    kstr_init(&self->user_name);
    kstr_init(&self->html_user_name);
    kstr_init(&self->timestamp);
}

static void kcd_notif_kws_notif_data_clean(struct kcd_notif_kws_notif_data *self) {
    int i;

    if (self->upload_array) {
        for (i = 0; i < self->upload_array->size; i++) kstr_destroy(self->upload_array->data[i]);
        karray_destroy(self->upload_array);
    }

    kstr_destroy(self->download_path);
    kstr_destroy(self->mail_subject);
    kstr_destroy(self->vnc_subject);
    kstr_destroy(self->chat_msg);
    kstr_clean(&self->html_kws_name);
    kstr_clean(&self->user_name);
    kstr_clean(&self->html_user_name);
    kstr_clean(&self->timestamp);
}

static struct kcd_notif_global_user* kcd_notif_global_user_new() {
    struct kcd_notif_global_user *self = kcalloc(sizeof(struct kcd_notif_global_user));
    kstr_init(&self->email);
    karray_init(&self->kws_array);
    karray_init(&self->email_id_array);
    return self;
}

static void kcd_notif_global_user_destroy(struct kcd_notif_global_user *self) {
    if (self) {
        int i;
        kstr_clean(&self->email);
        karray_clean(&self->kws_array);
        for (i = 0; i < self->email_id_array.size; i++) kstr_destroy(self->email_id_array.data[i]);
        karray_clean(&self->email_id_array);
        kfree(self);
    }
}

void kcd_notif_kws_summary_init(struct kcd_notif_kws_summary *self) {
    memset(self, 0, sizeof(struct kcd_notif_kws_summary));
    kstr_init(&self->chat_lines);
    kstr_init(&self->upload_lines);
    kstr_init(&self->download_lines);
    kstr_init(&self->vnc_lines);
}

void kcd_notif_kws_summary_clean(struct kcd_notif_kws_summary *self) {
    kstr_clean(&self->chat_lines);
    kstr_clean(&self->upload_lines);
    kstr_clean(&self->download_lines);
    kstr_clean(&self->vnc_lines);
}

/* Return true if the workspace specified is public. */
static int kcd_notif_is_public_kws(struct kcd_notif_kws *kws) { return (kws->flags & KANP_KWS_FLAG_PUBLIC) > 0; }

/* Return true if the workspace delete flag is set. */
static int kcd_notif_is_kws_deleted(uint32_t kws_flags) {
    return (kws_flags & KANP_KWS_FLAG_DELETE) > 0;
}

/* Return true if the user ban or lock flag is set. */
static int kcd_notif_is_user_out(uint32_t user_flags) {
    return (user_flags & (KANP_USER_FLAG_BAN | KANP_USER_FLAG_LOCK)) > 0;
}

/* Return true if the user wants to receive the notification specified. */
static int kcd_notif_is_notif_wanted(struct kcd_notif_kws_notif *notif, struct kcd_notif_kws_user *user) {
    return (notif->user_id != user->user_id &&
            !kcd_notif_is_user_out(user->flags) &&
            (user->notif_policy & KANP_EMAIL_NOTIF_FLAG) > 0);
}

/* Return true if the user wants to receive the workspace summary. */
static int kcd_notif_is_summary_wanted(struct kcd_notif_kws_user *user) {
    return (!kcd_notif_is_user_out(user->flags) &&
            (user->notif_policy & KANP_EMAIL_SUMMARY_FLAG) > 0);
}

/* Return the HTML-escaped version of the string specified. The second argument
 * is used as the escaping buffer.
 */
static char* _h(kstr *in, kstr *out) {
    kcd_mail_escapeHTML(in, out);
    return out->data;
}

/* Trim the string specified to the number of characters specified. The function
 * is careful not to cut words. If the string is cut, the string '...' is
 * appended. For convenience, the function returns a pointer to the trimmed
 * string.
 */
static char* trim_str(kstr *in, kstr *out, int nb_char) {
    int i;
    kstr_reset(out);

    for (i = 0; i < in->slen; i++) {
        char c = in->data[i];

        if (isspace(c)) {
            if (i >= nb_char) {
                kstr_append_cstr(out, " ...");
                break;
            }
        }

        kstr_append_char(out, c);
    }

    return out->data;
}

/* Set and return the path to the summary file specified. */
static char* kcd_notif_get_summary_file_path(kstr *path, uint64_t kws_id) {
    kstr_sf(path, "%skws_"PRINTF_64"u", KCD_NOTIF_SUMMARY_DIR, kws_id);
    return path->data;
}

/* Add a notification detail line. */
static void kcd_notif_add_notif_detail(kstr *content, kstr *detail) {
    kstr_append_sf(content, "<br>%s\n", detail->data);
}

/* Append a (X more) line to the HTML list specified if the number of actual
 * entries is higher than the limit.
 */
static void kcd_notif_append_more_line(kstr *to, char *before, int actual, int limit) {
    kstr detail;
    if (actual <= limit) return;
    kstr_init_sf(&detail, "%s%d more", before, actual - limit);
    kcd_notif_add_notif_detail(to, &detail);
    kstr_clean(&detail);
}

/* Add an application section (e.g. chat) in a summary email. */
static void kcd_notif_summary_format_app_section(kstr *content,
                                                 char *single,
                                                 char *multi,
                                                 int actual,
                                                 int limit,
                                                 kstr *lines) {
    if (!actual) return;
    kstr_append_sf(content, "<u>%s (%d):</u>\n%s", actual > 1 ? multi : single, actual, lines->data);
    kcd_notif_append_more_line(content, "&nbsp;&nbsp;&nbsp;", actual, limit);
    kstr_append_cstr(content, "<br>\n<br>\n");
}

/* Return the workspace having the specified ID, if any. */
static struct kcd_notif_kws* kcd_notif_get_kws_by_id(struct kcd_notif_state *st, uint64_t id) {
    return krb_tree_get(&st->kws_tree, &id);
}

/* Format and return the SKURL specified. */
static char* kcd_notif_get_skurl(kstr *url, int64_t kws_id, uint64_t email_id) {
    kstr_sf(url, "https://%s/teambox/public/"PRINTF_64"u/"PRINTF_64"u?notif=1",
            global_opts.web_host.data, kws_id, email_id);
    return url->data;
}

/* Format and return the invitation URL specified. */
static char* kcd_notif_get_invitation_url(kstr *url, int64_t kws_id, kstr *email_id) {
    kstr_sf(url, "https://%s/teambox/i/"PRINTF_64"u/%s?notif=1", global_opts.web_host.data, kws_id, email_id->data);
    return url->data;
}

/* Return the user having the ID specified, if any. */
static struct kcd_notif_kws_user* kcd_notif_get_kws_user_by_id(struct kcd_notif_kws *kws, uint32_t user_id) {
    if (user_id != 0 && user_id - 1 < (uint32_t)kws->user_array.size) return kws->user_array.data[user_id - 1];
    return NULL;
}

/* Return the name of the user specified, if any. If the name is empty, the
 * email address is used. If no user is provided, the function assumes that the
 * user is the system administrator.
 */
static void kcd_notif_get_kws_user_name(struct kcd_notif_kws_user *user, kstr *name) {
    if (!user) kstr_assign_cstr(name, "System Admin");
    else if (user->name.slen) kstr_assign_kstr(name, &user->name);
    else kstr_assign_kstr(name, &user->email);
}

/* Return a nice HTML name representation of the user specified, if any. The
 * name is clickable (mailto) if the user has an email address.
 */
static void kcd_notif_get_kws_html_user_name(struct kcd_notif_kws_user *user, kstr *html_user_name) {
    kstr user_name;
    kstr_init(&user_name);
    
    kcd_notif_get_kws_user_name(user, &user_name);
    if (!user || !user->email.slen) _h(&user_name, html_user_name);
    else kcd_mail_fmt_mailto(&user_name, &user->email, html_user_name);
    
    kstr_clean(&user_name);
}

/* Set the summary time to one second past midnight today in local time. */
static void kcd_notif_set_summary_time(struct kcd_notif_state *st) {
    time_t now = time(NULL);
    float diff;
    
    if (kfs_regular(CONFIG_PATH"/summary_debug")) {
        st->summary_time = now + 20;
    }
    
    else {
        struct tm ltime = *localtime(&now);
        ltime.tm_sec = 1;
        ltime.tm_min = 0;
        ltime.tm_hour = 0;
        ltime.tm_mday++;
        st->summary_time = (int64_t)mktime(&ltime);
    }
    
    diff = ((float)st->summary_time - (float)now) / 3600.0;
    kmod_log_msg(KCD_LOG_BRIEF, "Sending next summary in %.2f hours.\n", diff);
}

/* Send an email to the recipient specified. */
static int kcd_notif_send_email(struct kcd_notif_state *st, kstr *to, kstr *subject, kstr *html_body) {
    int error = 0, failed_flag;
    kstr mail_date, mail_id, boundary, mail_content, tmp, tmp2;
    krb_tree var_tree;

    kstr_init(&mail_date);
    kstr_init(&mail_id);
    kstr_init(&boundary);
    kstr_init(&mail_content);
    kstr_init(&tmp);
    kstr_init(&tmp2);
    kcd_mail_var_tree_init(&var_tree);

    kmod_log_msg(KCD_LOG_NOTIF, "Sending email to %s.\n", to->data);

    do {
        /* Generate the system fields. */
        error = kcd_mail_generate_system(&mail_id, &mail_date, &boundary);
        if (error) break;

        /* Generate the notification email content. */
        kcd_mail_var_tree_add(&var_tree, "MailDate", &mail_date);
        kcd_mail_var_tree_add(&var_tree, "MailMessageID", &mail_id);
        
        kstr_assign_cstr(&tmp2, KCD_KWS_NAME " Notification");
        kcd_mail_fmt_email(&tmp2, &global_opts.mail_sender, &tmp);
        kcd_mail_var_tree_add(&var_tree, "MailFrom", &tmp);
        
        kcd_mail_var_tree_add(&var_tree, "MailTo", to);
        kcd_mail_var_tree_add(&var_tree, "Subject", subject);
        kcd_mail_var_tree_add(&var_tree, "Boundary", &boundary);
        kcd_mail_var_tree_add(&var_tree, "HtmlBody", html_body);
        error = kcd_mail_template_generate(&st->notif_tmpl, &var_tree, &mail_content);
        if (error) break;

        /* Send the mail. */
        error = kcd_send_mail(&mail_content, to, &failed_flag);
        if (error) break;
        
        if (failed_flag) {
            kmod_log_msg(KCD_LOG_BRIEF, "Error sending notification email to %s: %s.\n", to->data, kmod_strerror());
        }

    } while (0);

    kstr_clean(&mail_date);
    kstr_clean(&mail_id);
    kstr_clean(&boundary);
    kstr_clean(&mail_content);
    kstr_clean(&tmp);
    kstr_clean(&tmp2);
    kcd_mail_var_tree_clean(&var_tree);

    return error;
}

/* Check if the workspace specified has been deleted. */
static int kcd_notif_check_kws_deleted(struct kcd_notif_state *st, uint64_t kws_id, int *deleted_flag) {
    int error = 0;
    PGresult *pg_res = NULL;
    kstr query;

    kstr_init(&query);

    do {
        kstr_sf(&query, "SELECT flags FROM kcd_kws_list WHERE kws_id = "PRINTF_64"u", kws_id);
        error = kcd_exec_pg_query(&st->conn, query.data, &pg_res, "get workspace flags");
        if (error) break;
        
        if (!PQntuples(pg_res)) {
            kmod_set_error("workspace not found");
            error = -1;
            break;
        }
        
        *deleted_flag = kcd_notif_is_kws_deleted(pg_db_get_uint32(pg_res, 0, 0));
        
    } while (0);

    kstr_clean(&query);
    pg_db_destroy_res(&pg_res);

    return error;
}

/* Listen or unlisten to the workspace specified. */
static int kcd_notif_listen_kws(struct kcd_notif_state *st, kstr *query, uint64_t kws_id, int listen_flag) {
    char *keyword = listen_flag ? "LISTEN" : "UNLISTEN";
    char *what = listen_flag ? "listen to workspace" : "unlisten from workspace";
    
    kstr_sf(query, "%s kws_"PRINTF_64"u_event_log", keyword, kws_id);
    if (kcd_exec_pg_query(&st->conn, query->data, NULL, what)) return -1;
    
    kstr_sf(query, "%s kws_"PRINTF_64"u_perm_check", keyword, kws_id);
    if (kcd_exec_pg_query(&st->conn, query->data, NULL, what)) return -1;
    
    return 0;
}

/* Check if the workspace still exists. If not, remove it from the workspace
 * tree.
 */
static int kcd_notif_perm_check(struct kcd_notif_state *st, struct kcd_notif_kws *kws, int *deleted_flag) {
    int error = 0;
    kstr query;

    kstr_init(&query);

    do {
        error = kcd_notif_check_kws_deleted(st, kws->kws_id, deleted_flag);
        if (error) break;
        
        if (*deleted_flag) {
            error = kcd_notif_listen_kws(st, &query, kws->kws_id, 0);
            if (error) break;
            
            kcd_notif_kws_destroy(krb_tree_remove(&st->kws_tree, &kws->kws_id));
        }

    } while (0);

    kstr_clean(&query);

    return error;
}

/* Fetch the data for the workspace specified, if possible. The function assumes
 * that the workspace ID is set.
 */
static int kcd_notif_fetch_kws_data(struct kcd_notif_state *st, struct kcd_notif_kws *kws, int *deleted_flag) {
    int error = 0, i;
    kstr query;
    PGresult *pg_res = NULL;

    kstr_init(&query);

    assert(kws->kws_id);
    kcd_notif_kws_clear_user_array(kws);

    do {
        /* Get the kcd_kws_list information. */
        kstr_sf(&query, "SELECT flags, name FROM kcd_kws_list WHERE kws_id = "PRINTF_64"u", kws->kws_id);
        error = kcd_exec_pg_query(&st->conn, query.data, &pg_res, "get workspace information");
        if (error) break;

        if (!PQntuples(pg_res)) {
            kmod_set_error("workspace not found");
            error = -1;
            break;
        }

        kws->flags = pg_db_get_uint32(pg_res, 0, 0);
        *deleted_flag = kcd_notif_is_kws_deleted(kws->flags);
        kstr_assign_cstr(&kws->name, PQgetvalue(pg_res, 0, 1));
        pg_db_destroy_res(&pg_res);
        
        /* The workspace has been deleted. */
        if (*deleted_flag) break;

        /* Get the kcd_kws_users information. */
        kstr_sf(&query, "SELECT user_id, flags, notif_policy, email, name_admin, name_user "
                        "FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u ORDER BY user_id", kws->kws_id);
        error = kcd_exec_pg_query(&st->conn, query.data, &pg_res, "get user information");
        if (error) break;

        for (i = 0; i < PQntuples(pg_res); i++) {
            struct kcd_notif_kws_user *user = kcd_notif_kws_user_new();
            karray_push(&kws->user_array, user);

            user->user_id = pg_db_get_uint32(pg_res, i, 0);
            if (user->user_id != (uint32_t)i + 1) {
                kmod_set_error("invalid user ID in user table");
                error = -1;
                break;
            }

            user->flags = pg_db_get_uint32(pg_res, i, 1);
            user->notif_policy = pg_db_get_uint32(pg_res, i, 2);
            kstr_assign_cstr(&user->email, PQgetvalue(pg_res, i, 3));
            kstr_assign_cstr(&user->name, PQgetvalue(pg_res, i, 4));
            if (!user->name.slen) kstr_assign_cstr(&user->name, PQgetvalue(pg_res, i, 5));
        }

        if (error) break;
        pg_db_destroy_res(&pg_res);

        /* Get the system email ID of the users, if possible. */
        if (!kcd_notif_is_public_kws(kws)) {
            for (i = 0; i < kws->user_array.size; i++) {
                struct kcd_notif_kws_user *user = kws->user_array.data[i];
                if (kcd_notif_is_user_out(user->flags)) continue;

                kstr_sf(&query, "SELECT email_id FROM kcd_kws_user_invitation "
                                "WHERE kws_id = "PRINTF_64"u AND user_id = %u AND inviting_user_id = 0",
                        kws->kws_id, user->user_id);
                error = kcd_exec_pg_query(&st->conn, query.data, &pg_res, "get system email ID");
                if (error) break;

                if (!PQntuples(pg_res)) {
                    kmod_set_error("system email ID not found");
                    error = -1;
                    break;
                }

                kstr_assign_cstr(&user->system_email_id, PQgetvalue(pg_res, 0, 0));
                pg_db_destroy_res(&pg_res);
            }

            if (error) break;
        }

    } while (0);

    kstr_clean(&query);
    pg_db_destroy_res(&pg_res);

    return error;
}

/* Fetch the subject corresponding to the email ID specified in the public
 * workspace specified. If the subject isn't found, "unknown subject" is set.
 */
static int kcd_notif_fetch_public_email_subject(struct kcd_notif_state *st, uint64_t kws_id, uint64_t email_id,
                                                kstr *subject) {
    int error = 0;
    kstr query;
    PGresult *pg_res = NULL;

    kstr_init(&query);

    do {
        kstr_sf(&query, "SELECT subject FROM kcd_kws_pub_email_info WHERE "
                        "kws_id = "PRINTF_64"u AND email_id = "PRINTF_64"u", kws_id, email_id);
        error = kcd_exec_pg_query(&st->conn, query.data, &pg_res, "get email subject");
        if (error) break;

        if (PQntuples(pg_res)) kstr_assign_cstr(subject, PQgetvalue(pg_res, 0, 0));
        else kstr_assign_cstr(subject, "unknown subject");

        pg_db_destroy_res(&pg_res);

    } while (0);

    kstr_clean(&query);
    pg_db_destroy_res(&pg_res);

    return error;
}

/* Retrieve the list of new workspaces, starting from last_kws_id, listen to
 * them and get their last notification ID. Only the 'kws_id' and the
 * 'last_notif_id' fields are set in the workspaces fetched.
 */
static int kcd_notif_fetch_new_kws(struct kcd_notif_state *st) {
    int error = 0, i;
    kstr query;
    karray kws_array;
    PGresult *pg_res = NULL;

    kstr_init(&query);
    karray_init(&kws_array);

    do {
        /* Get the new workspaces. We skip deleted workspaces here. Since the
         * database state has been frozen, we cannot accidentally start to
         * listen to deleted workspaces at this point.
         */
        kstr_sf(&query, "SELECT kws_id FROM kcd_kws_list WHERE kws_id > "PRINTF_64"u AND (flags & %u = 0)",
                st->last_kws_id, KANP_KWS_FLAG_DELETE);
        error = kcd_exec_pg_query(&st->conn, query.data, &pg_res, "get new workspaces");
        if (error) break;

        for (i = 0; i < PQntuples(pg_res); i++) {
            struct kcd_notif_kws *kws = kcd_notif_kws_new();
            kws->kws_id = pg_db_get_uint64(pg_res, i, 0);
            st->last_kws_id = MAX(st->last_kws_id, kws->kws_id);
            karray_push(&kws_array, kws);
            krb_tree_add_fast(&st->kws_tree, &kws->kws_id, kws);
        }

        pg_db_destroy_res(&pg_res);

        /* Process the new workspaces. */
        for (i = 0; i < kws_array.size; i++) {
            struct kcd_notif_kws *kws = kws_array.data[i];
            
            /* Listen to the workspace. */
            error = kcd_notif_listen_kws(st, &query, kws->kws_id, 1);
            if (error) break;
            
            /* Fetch the last notification ID. */
            kstr_sf(&query, "SELECT max(notif_id) FROM kcd_kws_notif_log WHERE kws_id = "PRINTF_64"u", kws->kws_id);
            error = kcd_exec_pg_query(&st->conn, query.data, &pg_res, "get last notification ID");
            if (error) break;
            kws->last_notif_id = pg_db_get_uint64(pg_res, 0, 0);
            pg_db_destroy_res(&pg_res);
        }

        if (error) break;

    } while (0);

    kstr_clean(&query);
    karray_clean(&kws_array);
    pg_db_destroy_res(&pg_res);

    return error;
}

/* Fetch an array of workspace notifications with the WHERE clause specified.
 * The notification array is not emptied on error.
 */
static int kcd_notif_fetch_kws_notif(struct kcd_notif_state *st, kstr *where, karray *notif_array) {
    int error = 0, i;
    kstr query;
    PGresult *pg_res = NULL;

    kstr_init(&query);

    karray_reset(notif_array);

    do {
        kstr_sf(&query, "SELECT notif_id, date, user_id, type, payload FROM kcd_kws_notif_log WHERE %s", where->data);
        error = kcd_exec_pg_query(&st->conn, query.data, &pg_res, "get workspace notifications");
        if (error) break;

        for (i = 0; i < PQntuples(pg_res); i++) {
            struct kcd_notif_kws_notif *notif = kcd_notif_kws_notif_new();
            karray_push(notif_array, notif);
            notif->notif_id = pg_db_get_uint64(pg_res, i, 0);
            notif->date = pg_db_get_uint64(pg_res, i, 1);
            notif->user_id = pg_db_get_uint32(pg_res, i, 2);
            notif->type = pg_db_get_uint32(pg_res, i, 3);
            pg_db_get_bytea(pg_res, i, 4, &notif->payload);
        }

        pg_db_destroy_res(&pg_res);

    } while (0);

    kstr_clean(&query);
    pg_db_destroy_res(&pg_res);

    return error;
}

/* Purge the notifications of the workspace specified up the last notification. */
static int kcd_notif_purge_notif(struct kcd_notif_state *st, struct kcd_notif_kws *kws) {
    int error = 0;
    struct kcd_pg_anp_query aq;
    
    kmod_log_msg(KCD_LOG_NOTIF, "kcd_notif_purge_notif() called.\n");
    
    kcd_pg_anp_query_init(&aq);
    anp_write_uint64(&aq.input_buf, kws->kws_id);
    anp_write_uint64(&aq.input_buf, kws->last_notif_id);
    error = kcd_exec_safe_pg_anp_query(&st->conn, &aq, "purge_notif");
    kcd_pg_anp_query_clean(&aq);
    
    return error;
}

/* Obtain the data of the notification specified. */
static int kcd_notif_get_notif_data(struct kcd_notif_state *st, struct kcd_notif_kws *kws,
                                    struct kcd_notif_kws_notif *notif, struct kcd_notif_kws_notif_data *nd) {
    int error = 0;
    uint32_t i, nb_file;
    kbuffer *payload = &notif->payload;
    time_t date = (time_t)notif->date;
    struct tm *tm = localtime(&date);

    payload->pos = 0;

    do {
        /* Extract the fields. */
        if (notif->type == KANP_EVT_CHAT_MSG) {
            nd->chat_msg = kstr_new();
            error = anp_read_kstr(payload, nd->chat_msg);
            if (error) break;
        }

        else if (notif->type == KANP_EVT_KFS_PHASE_2) {
            nd->upload_array = karray_new();

            if (anp_read_uint64(payload, &nd->public_email_id) || anp_read_uint32(payload, &nb_file)) {
                error = -1;
                break;
            }

            for (i = 0; i < nb_file; i++) {
                uint32_t create_flag;   
                kstr *path;

                error = anp_read_uint32(payload, &create_flag);
                if (error) break;

                path = kstr_new();
                karray_push(nd->upload_array, path);
                error = anp_read_kstr(payload, path);
                if (error) break;
            }

            if (error) break;
        }

        else if (notif->type == KANP_EVT_KFS_DOWNLOAD) {
            nd->download_path = kstr_new();

            if (anp_read_uint64(payload, &nd->public_email_id) || anp_read_kstr(payload, nd->download_path)) {
                error = -1;
                break;
            }
        }

        else if (notif->type == KANP_EVT_VNC_START) {
            nd->vnc_subject = kstr_new();
            error = anp_read_kstr(payload, nd->vnc_subject);
            if (error) break;
        }

        else {
            kmod_set_error("unknown notification type "PRINTF_64"u", notif->type);
            error = -1;
            break;
        }

        /* Get the mail subject. */
        if (kcd_notif_is_public_kws(kws)) {
            nd->mail_subject = kstr_new();

            if (nd->public_email_id) {
                error = kcd_notif_fetch_public_email_subject(st, kws->kws_id, nd->public_email_id, nd->mail_subject);
                if (error) break;
            }
        }
        
        /* Get the workspace name. */
        _h(&kws->name, &nd->html_kws_name);

        /* Get the notification user object and user name. */
        nd->user = kcd_notif_get_kws_user_by_id(kws, notif->user_id);
        kcd_notif_get_kws_user_name(nd->user, &nd->user_name);
        kcd_notif_get_kws_html_user_name(nd->user, &nd->html_user_name);
        
        /* Format the timestamp. */
        kstr_sf(&nd->timestamp, "%.2d:%.2d", tm->tm_hour, tm->tm_min);

    } while (0);

    if (error) kmod_append_error("notification payload error");

    return error;
}

/* Get the notification HTML title line. */
static void kcd_notif_get_notif_title(kstr *content, kstr *user, char *event, kstr *kws) {
    kstr_sf(content, "<title>%s %s in \"%s\" Teambox</title>\n", user->data, event, kws->data);
}

/* Get a notification description line. */
static void kcd_notif_get_notif_desc(kstr *content, kstr *user, char *desc, kstr *kws) {
    kstr_sf(content, "%s %s in the \"%s\" Teambox&trade;\n", user->data, desc, kws->data);
}

/* Process a new notification for the workspace specified. Note: split this
 * function when it gets bigger.
 */
static int kcd_notif_process_new_notif(struct kcd_notif_state *st, struct kcd_notif_kws *kws,
                                       struct kcd_notif_kws_notif *notif) {
    int error = 0, i, j;
    struct kcd_notif_kws_notif_data nd;
    kstr notif_subject;
    kstr notif_link;
    char *notif_event_name;
    char *notif_event_desc;
    kstr notif_detail_lines;
    kstr c;                                 /* Mail content. */
    kstr *t = kstr_new(), *t2 = kstr_new(); /* Scratch strings. */

    kcd_notif_kws_notif_data_init(&nd);
    kstr_init(&notif_subject);
    kstr_init(&notif_link);
    kstr_init(&notif_detail_lines);
    kstr_init(&c);

    kmod_log_msg(KCD_LOG_NOTIF, "kcd_notif_process_new_notif() called.\n");

    do {
        /* Get the notification data. */
        error = kcd_notif_get_notif_data(st, kws, notif, &nd);
        if (error) break;

        kmod_log_msg(KCD_LOG_NOTIF, "%s workspace "PRINTF_64"u, notif "PRINTF_64"u type %u, user %s.\n",
                                    kcd_notif_is_public_kws(kws) ? "Public" : "Non-public",
                                    kws->kws_id,
                                    notif->notif_id,
                                    notif->type,
                                    nd.user ? nd.user->email.data : "System Admin");

        /* This is a public workspace. */
        if (kcd_notif_is_public_kws(kws)) {
            struct kcd_notif_kws_user *owner;
            
            /* Get the owner, if any. */
            if (!kws->user_array.size) break;
            owner = kws->user_array.data[0];
                
            if (!kcd_notif_is_notif_wanted(notif, owner)) {
                kmod_log_msg(KCD_LOG_NOTIF, "User %s doesn't want the notification.\n", owner->email.data);
                break;
            }
            
            if (notif->type == KANP_EVT_KFS_PHASE_2) {
                if (!nd.upload_array->size) break;
                
                kstr_sf(&notif_subject, "File received from %s", nd.user_name.data);
                kstr_append_sf(&c, "You have received a file from %s in response to your email %s.<br/><br/>\n",
                                   nd.html_user_name.data, _h(nd.mail_subject, t));
                
                kstr_append_cstr(&c, "<hr/>\n");
                kpath_basename(nd.upload_array->data[0], t, 0);
                kstr_append_cstr(&c, _h(t, t2));
                kstr_append_cstr(&c, "\n<hr/>\n");
            
                kcd_notif_get_skurl(t, kws->kws_id, nd.public_email_id);
                kstr_append_sf(&c, "<br/>\n"
                                   "Click on the link below to access the file or "
                                   "to change your notification settings:<br/>\n"
                                   "<a href=\"%s\">%s</a><br/>\n", t->data, t->data);
                kstr_append_cstr(&c, "<br/>\n"
                                     "This file is also available in your Teambox Manager "
                                     "under \"My Public Teambox\".<br/>\n");
            }

            else if (notif->type == KANP_EVT_KFS_DOWNLOAD) {
                kstr_sf(&notif_subject, "File accessed by %s", nd.user_name.data);
                kstr_append_sf(&c, "%s has accessed a file included in your email %s.<br/><br/>\n",
                                   nd.html_user_name.data, _h(nd.mail_subject, t));
                
                kstr_append_cstr(&c, "<hr/>\n");
                kpath_basename(nd.download_path, t, 0);
                kstr_append_cstr(&c, _h(t, t2));
                kstr_append_cstr(&c, "\n<hr/>\n");
            
                kcd_notif_get_skurl(t, kws->kws_id, nd.public_email_id);
                kstr_append_sf(&c, "<br/>\n"
                                   "Click on the link below to view the file "
                                   "or to change your notification settings:<br/>\n"
                                   "<a href=\"%s\">%s</a><br/>\n", t->data, t->data);
                kstr_append_cstr(&c, "<br/>\n"
                                     "This file is also available in your Teambox Manager "
                                     "under \"My Public Teambox\".<br/>\n");
            }
                
            else break;
            
            /* Send the email. */
            error = kcd_notif_send_email(st, &owner->email, &notif_subject, &c);
            if (error) break;
        }

        /* This is a regular workspace. */
        else {

            /* Quench chat if required. */
            if (notif->type == KANP_EVT_CHAT_MSG) {
                int64_t prev_chat_time = kws->last_chat_time;
                kws->last_chat_time = ktime_now_sec();
                if (prev_chat_time + KCD_NOTIF_CHAT_DELAY > kws->last_chat_time) {
                    kmod_log_msg(KCD_LOG_NOTIF, "Chat quenched.\n");
                    break;
                }
            }

            /* Send the notifications to the users. */
            for (i = 0; i < kws->user_array.size; i++) {
                struct kcd_notif_kws_user *user = kws->user_array.data[i];
                kstr_reset(&notif_subject);
                kstr_reset(&notif_detail_lines);

                /* The user isn't interested in this notification. */
                if (!kcd_notif_is_notif_wanted(notif, user)) {
                    kmod_log_msg(KCD_LOG_NOTIF, "User %s doesn't want the notification.\n", user->email.data);
                    continue;
                }

                if (notif->type == KANP_EVT_CHAT_MSG) {
                    kstr_sf(&notif_subject, "Chat in " KCD_KWS_NAME " %s", kws->name.data);
                    notif_event_name = "chat";
                    notif_event_desc = "is chatting";
                    trim_str(nd.chat_msg, t, KCD_NOTIF_MAX_CHAT_LENGTH);
                    _h(t, t2);
                    kcd_notif_add_notif_detail(&notif_detail_lines, t2);
                }
                
                else if (notif->type == KANP_EVT_KFS_PHASE_2) {
                    int actual = nd.upload_array->size;
                    int limit = MIN(actual, KCD_NOTIF_KCD_NOTIF_MAX_FILE_UPLOAD);
                    int plural = actual > 1;
                    kstr_sf(&notif_subject, "%s added to " KCD_KWS_NAME " %s", plural ? "Files" : "File",
                                            kws->name.data);
                    notif_event_name = plural ? "files created or modified" : "file created or modified";
                    notif_event_desc = plural ? "just added or modified the following files" :
                                                "just added or modified the following file";
                    for (j = 0; j < limit; j++) {
                        kstr *path = nd.upload_array->data[j];
                        _h(path, t);
                        kcd_notif_add_notif_detail(&notif_detail_lines, t);
                    }
                    kcd_notif_append_more_line(&notif_detail_lines, "", actual, limit);
                }
                
                else if (notif->type == KANP_EVT_VNC_START) {
                    kstr_sf(&notif_subject, "Screen sharing in " KCD_KWS_NAME " %s", kws->name.data);
                    notif_event_name = "screen sharing session";
                    notif_event_desc = "started a screen sharing session";
                    _h(nd.vnc_subject, t);
                    kcd_notif_add_notif_detail(&notif_detail_lines, t);
                }
                
                else continue;
   
                /* Add the link to access the workspace. */
                kcd_notif_get_invitation_url(t, kws->kws_id, &user->system_email_id);
                kstr_sf(&notif_link, "<a href=\"%s\">%s</a>\n", t->data, t->data);
                
                /* Format the mail body. */
                kcd_notif_get_notif_title(t, &nd.html_user_name, notif_event_name, &nd.html_kws_name);
                kcd_notif_get_notif_desc(t2, &nd.html_user_name, notif_event_desc, &nd.html_kws_name);
                kstr_sf(&c, notif_body_format, t->data, t2->data, notif_detail_lines.data, notif_link.data);
                
                /* Send the email. */
                error = kcd_notif_send_email(st, &user->email, &notif_subject, &c);
                if (error) break;
            }

            if (error) break;
        }

    } while (0);

    kcd_notif_kws_notif_data_clean(&nd);
    kstr_clean(&notif_subject);
    kstr_clean(&notif_link);
    kstr_clean(&notif_detail_lines);
    kstr_clean(&c);
    kstr_destroy(t);
    kstr_destroy(t2);

    return error;
}

/* Fetch real-time notifications for the workspace specified and process them. */
static int kcd_notif_fetch_new_notif(struct kcd_notif_state *st, struct kcd_notif_kws *kws) {
    int error = 0, i, deleted_flag;
    kstr where;
    karray notif_array;

    kstr_init(&where);
    karray_init(&notif_array);

    kmod_log_msg(KCD_LOG_NOTIF, "kcd_notif_fetch_new_notif() called.\n");

    do {
        /* Fetch the notifications. */
        kstr_sf(&where, "kws_id = "PRINTF_64"u AND notif_id > "PRINTF_64"u ORDER BY notif_id",
                        kws->kws_id, kws->last_notif_id);
        error = kcd_notif_fetch_kws_notif(st, &where, &notif_array);
        if (error) break;

        /* No new notifications. */
        if (!notif_array.size) break;

        /* Fetch the workspace data, if possible. */
        error = kcd_notif_fetch_kws_data(st, kws, &deleted_flag);
        if (error || deleted_flag) break;

        /* Process each notification. */
        for (i = 0; i < notif_array.size; i++) {
            struct kcd_notif_kws_notif *notif = notif_array.data[i];
            kws->last_notif_id = MAX(kws->last_notif_id, notif->notif_id);
            error = kcd_notif_process_new_notif(st, kws, notif);
            if (error) break;
        }

        if (error) break;

        /* Free the workspace users to save memory. */
        kcd_notif_kws_clear_user_array(kws);

    } while (0);

    kstr_clean(&where);
    kcd_notif_clear_notif_array(&notif_array);
    karray_clean(&notif_array);

    return error;
}

/* Process a workspace summary notification. */
static int kcd_notif_summary_process_notif(struct kcd_notif_state *st,
                                           struct kcd_notif_kws_summary *ks,
                                           struct kcd_notif_kws *kws,
                                           struct kcd_notif_kws_notif *notif) {
    int error = 0, i;
    char *sp = "&nbsp;&nbsp;&nbsp;";
    struct kcd_notif_kws_notif_data nd;
    kstr *t = kstr_new(), *t2 = kstr_new(); /* Scratch strings. */
    
    kcd_notif_kws_notif_data_init(&nd);

    do {
        error = kcd_notif_get_notif_data(st, kws, notif, &nd);
        if (error) break;

        if (notif->type == KANP_EVT_CHAT_MSG) {
            if (ks->chat_total++ >= KCD_NOTIF_KWS_MAX_CHAT) break;
            trim_str(nd.chat_msg, t, KCD_NOTIF_MAX_CHAT_LENGTH);
            kstr_append_sf(&ks->chat_lines, "<br>%s<b>%s</b>:%s%s<br>%s%s%s\n",
                           sp, nd.html_user_name.data, nd.timestamp.data, sp, sp, sp, _h(t, t2));
        }
        
        else if (notif->type == KANP_EVT_KFS_PHASE_2) {
            for (i = 0; i < nd.upload_array->size; i++) {
                if (ks->upload_total++ < KCD_NOTIF_KWS_MAX_FILE_UPLOAD) {
                    kstr *path = nd.upload_array->data[i];
                    kstr_append_sf(&ks->upload_lines, "<br>%s<b>%s</b>%sby%s%s%sat%s%s\n",
                                   sp, _h(path, t), sp, sp, nd.html_user_name.data, sp, sp, nd.timestamp.data);
                }
            }
        }
        
        else if (notif->type == KANP_EVT_VNC_START) {
            if (ks->vnc_total++ >= KCD_NOTIF_KWS_MAX_VNC) break;
            kstr_append_sf(&ks->vnc_lines, "<br>%s<b>%s</b>:%s%s\n", 
                           sp, nd.html_user_name.data, sp, _h(nd.vnc_subject, t));
        }

    } while (0);

    kcd_notif_kws_notif_data_clean(&nd);
    kstr_destroy(t);
    kstr_destroy(t2);

    return error;
}

/* Send a workspace summary to the user specified. */
static int kcd_notif_summary_process_user(struct kcd_notif_state *st, struct kcd_notif_global_user *user) {
    int error = 0, i;
    kstr *t = kstr_new(), *t1 = kstr_new(), *t2 = kstr_new(), *t3 = kstr_new();
    kstr content, sec1, sec2;
    kstr subject;
    kbuffer buf;
    
    kstr_init(&content);
    kstr_init(&sec1);
    kstr_init(&sec2);
    kstr_init(&subject);
    kbuffer_init(&buf);

    do {
        kstr_assign_cstr(&subject, KCD_KWS_NAME " activity summary");
        kstr_append_cstr(&content, "<p>"KCD_KWS_NAME " activity summary for the last 24 hours.</p><br/>\n");
        
        /* Add the workspace data. */
        for (i = 0; i < user->kws_array.size; i++) {
            struct kcd_notif_kws *kws = user->kws_array.data[i];
            kstr *email_id = user->email_id_array.data[i];
            
	    kstr_append_sf(&sec1, "&nbsp;\"%s\" (%u events)<br>\n", _h(&kws->name, t), kws->event_total);
            
            kstr_sf(t1, "\"%s\" Teambox&trade;\n", _h(&kws->name, t));
            
            error = kfs_read_file(kcd_notif_get_summary_file_path(t, kws->kws_id), &buf);
            if (error) break;
            kstr_assign_buf(t2, buf.data, buf.len);
            if (i) kstr_append_cstr(t2, "<br>\n");
            
            kcd_notif_get_invitation_url(t, kws->kws_id, email_id);
            kstr_sf(t3, "<a href=\"%s\">%s</a>\n", t->data, t->data);
            
            kstr_append_sf(&sec2, summary_workspace_format, t1->data, t2->data, t3->data);
        }
        
        if (error) break;
        
        kstr_sf(&content, summary_body_format, sec1.data, sec2.data);
        
        /* Send the email. */
        error = kcd_notif_send_email(st, &user->email, &subject, &content);
        if (error) break;
        
    } while (0);
    
    kstr_destroy(t);
    kstr_destroy(t1);
    kstr_destroy(t2);
    kstr_destroy(t3);
    kstr_clean(&content);
    kstr_clean(&sec1);
    kstr_clean(&sec2);
    kstr_clean(&subject);
    kbuffer_clean(&buf);

    return error;
}
            
/* Process the notifications and users of the workspace specified to prepare to
 * send the summaries for that workspace.
 */
static int kcd_notif_summary_process_kws(struct kcd_notif_state *st, struct kcd_notif_kws *kws, krb_tree *user_tree) {
    int error = 0, i, fetch_limit = 500, transaction_flag = 0, deleted_flag;
    uint64_t cur_notif_id = 0;
    kstr content;
    kstr where;
    karray notif_array;
    struct kcd_notif_kws_summary ks;
    kbuffer buf;
    
    kstr_init(&content);
    kstr_init(&where);
    karray_init(&notif_array);
    kcd_notif_kws_summary_init(&ks);
    kbuffer_init(&buf);

    do {
        /* Freeze the database state. */
        transaction_flag = 1;
        error = kcd_open_pg_serializable_transaction(&st->conn);
        if (error) break;
        
        /* Fetch the workspace data, if possible. */
        error = kcd_notif_fetch_kws_data(st, kws, &deleted_flag);
        if (error || deleted_flag) break;
        
        /* No summary for public workspaces. */
        if (kcd_notif_is_public_kws(kws)) break;
        
        /* Fetch the notifications. */
        while (1) {
            kstr_sf(&where, "kws_id = "PRINTF_64"u AND notif_id > "PRINTF_64"u AND notif_id <= "PRINTF_64"u "
                            "ORDER BY notif_id LIMIT %d",
                             kws->kws_id, cur_notif_id, kws->last_notif_id, fetch_limit);
            error = kcd_notif_fetch_kws_notif(st, &where, &notif_array);
            if (error) break;
            if (!notif_array.size) break;
            
            /* Process the notifications. */
            for (i = 0; i < notif_array.size; i++) {
                struct kcd_notif_kws_notif *notif = notif_array.data[i];
                cur_notif_id = MAX(cur_notif_id, notif->notif_id);
                error = kcd_notif_summary_process_notif(st, &ks, kws, notif);
                if (error) break;
            }
            
            if (error) break;
            kcd_notif_clear_notif_array(&notif_array);
        }
        
        if (error) break;
        
        /* Thaw the database state. This needs to be done before we purge the
         * notifications below.
         */
        transaction_flag = 0;
        error = kcd_commit_pg_transaction(&st->conn);
        if (error) break;
        
        /* Set the event total. */
        kws->event_total = ks.chat_total + ks.upload_total + ks.vnc_total;
        
        /* No notifications. */
        if (!kws->event_total) break;
        
        
        /* Format the message. */
        kcd_notif_summary_format_app_section(&content, "File Created or Modified", "Files Created or Modified",
                                             ks.upload_total, KCD_NOTIF_KWS_MAX_FILE_UPLOAD, &ks.upload_lines);
        kcd_notif_summary_format_app_section(&content, "Message Board Activity", "Message Board Activity",
                                             ks.chat_total, KCD_NOTIF_KWS_MAX_CHAT, &ks.chat_lines);
        kcd_notif_summary_format_app_section(&content, "Screen Sharing Session", "Screen Sharing Sessions",
                                             ks.vnc_total, KCD_NOTIF_KWS_MAX_VNC, &ks.vnc_lines);

        /* Write the message in a file. */
        kbuffer_write_kstr(&buf, &content);
        error = kfs_write_file(kcd_notif_get_summary_file_path(&where, kws->kws_id), &buf);
        if (error) break;
        
        /* Purge the old notifications up to the last notification we have
         * received.
         */
        error = kcd_notif_purge_notif(st, kws);
        if (error) break;
        
        /* Register the workspace to the users who want to receive a
         * notification.
         */
        for (i = 0; i < kws->user_array.size; i++) {
            struct kcd_notif_kws_user *kws_user = kws->user_array.data[i];
            struct kcd_notif_global_user *global_user = NULL;
            
            /* The user doesn't want the summary. */
            if (!kcd_notif_is_summary_wanted(kws_user)) continue;
            
            /* Get or register the corresponding global user. */
            global_user = krb_tree_get(user_tree, kws_user->email.data);
            if (!global_user) {
                global_user = kcd_notif_global_user_new();
                kstr_assign_kstr(&global_user->email, &kws_user->email);
                krb_tree_add_fast(user_tree, global_user->email.data, global_user);
            }
            
            /* Associate the workspace and the email ID to the global user. */
            karray_push(&global_user->kws_array, kws);
            karray_push(&global_user->email_id_array, kstr_new_kstr(&kws_user->system_email_id));
        }
        
        /* Free the workspace users to save memory. */
        kcd_notif_kws_clear_user_array(kws);
        
    } while (0);
    
    /* Thaw the database state. */
    if (!error && transaction_flag) {
        error = kcd_commit_pg_transaction(&st->conn);
    }
    
    kstr_clean(&content);
    kstr_clean(&where);
    kcd_notif_clear_notif_array(&notif_array);
    karray_clean(&notif_array);
    kcd_notif_kws_summary_clean(&ks);
    kbuffer_clean(&buf);
        
    return error;
}

/* Send the workspace summaries. */
static int kcd_notif_send_summary(struct kcd_notif_state *st) {
    int error = 0, i, size;
    struct krb_node *iter;
    krb_tree user_tree;
    
    kmod_log_msg(KCD_LOG_BRIEF, "Sending workspace summaries.\n");
    
    krb_tree_init_func(&user_tree, krb_tree_str_cmp);
    
    do {
        /* Create the summary directory if required. */
        if (!kfs_isdir(KCD_NOTIF_SUMMARY_DIR)) {
            error = kfs_mkdir(KCD_NOTIF_SUMMARY_DIR);
            if (error) break;
        }
        
        /* Process the workspaces. */
        size = krb_tree_size(&st->kws_tree);
        iter = krb_tree_iter_start(&st->kws_tree);
        for (i = 0; i < size; i++) {
            struct kcd_notif_kws *kws = krb_tree_iter_next(&st->kws_tree, &iter);
            error = kcd_notif_summary_process_kws(st, kws, &user_tree);
            if (error) break;
        }
        if (error) break;
        
        /* Process the users. */
        size = krb_tree_size(&user_tree);
        iter = krb_tree_iter_start(&user_tree);
        for (i = 0; i < size; i++) {
            struct kcd_notif_global_user *user = krb_tree_iter_next(&user_tree, &iter);
            error = kcd_notif_summary_process_user(st, user);
            if (error) break;
        }
        if (error) break;
        
    } while (0);
        
    size = krb_tree_size(&user_tree);
    iter = krb_tree_iter_start(&user_tree);
    for (i = 0; i < size; i++) kcd_notif_global_user_destroy(krb_tree_iter_next(&user_tree, &iter));
    krb_tree_clean(&user_tree);
    
    return error;
}

/* Handle the case where we have a connection in the main loop. */
static int kcd_notif_loop_have_conn(struct kcd_notif_state *st, struct kselect *sel, int *skip_select_flag) {
    int error = 0;
    int64_t now_sec = ktime_now_sec();
    int64_t delay = st->summary_time - now_sec;
    PGnotify *notif = NULL;

    kmod_log_msg(KCD_LOG_NOTIF, "kcd_notif_loop_have_conn() called.\n");

    do {
        /* It's time to send the summary. */
        if (delay <= 0) {

            /* Set the next summary time. */
            kcd_notif_set_summary_time(st);
            
            /* Send the summaries. */
            error = kcd_notif_send_summary(st);
            if (error) break;

            /* Skip the select() call. */
            *skip_select_flag = 1;
            break;
        }

        /* Wait until it is time to send the summary. */
        ktime_from_msec(&sel->tv, delay*1000 + 1);
        
        /* Freeze the database state. */
        error = kcd_open_pg_serializable_transaction(&st->conn);
        if (error) break;
        
        /* Allow postgres to consume its input, if any. */
        error = pg_db_consume(&st->conn);
        if (error) break;
        
        /* Process all Postgres notifications. */
        while (1) {
            notif = pg_db_notify_check(&st->conn);
            if (!notif) break;

            /* Handle the new workspaces. */
            if (!strcmp(notif->relname, "kws_list")) {
                kmod_log_msg(KCD_LOG_NOTIF, "Got postgres notification for kws_list.\n");
                error = kcd_notif_fetch_new_kws(st);
                if (error) break;
            }

            /* Handle a workspace notification. */
            else if (!strncmp(notif->relname, "kws_", 4)) {
                uint64_t kws_id = strtoll(notif->relname + 4, NULL, 10);
                struct kcd_notif_kws *kws = kcd_notif_get_kws_by_id(st, kws_id);

                if (kws) {
                    if (strstr(notif->relname, "_event_log") != NULL) {
                        kmod_log_msg(KCD_LOG_NOTIF, "Got event log notification for workspace "PRINTF_64"u.\n", kws_id);
                        error = kcd_notif_fetch_new_notif(st, kws);
                        if (error) break;
                    }
                    
                    else {
                        int ignored;
                        kmod_log_msg(KCD_LOG_NOTIF, "Got perm check notification for workspace "PRINTF_64"u.\n",
                                                    kws_id);
                        error = kcd_notif_perm_check(st, kws, &ignored);
                        if (error) break;
                    }
                }

                else {
                    kmod_log_msg(KCD_LOG_BRIEF, "Got postgres notification for unknown workspace "PRINTF_64"u.\n",
                                 kws_id);
                }
            }

            pg_db_destroy_notif(&notif);
        }
        
        if (error) break;

        /* Thaw the database state. */
        error = kcd_commit_pg_transaction(&st->conn);
        if (error) break;
        
        /* Add the database socket to the read set to receive notifications. */
        kselect_add_read(sel, st->conn.sock);

    } while (0);

    pg_db_destroy_notif(&notif);

    return error;
}

/* Reconnect to the database and listen to the workspaces. */
static int kcd_notif_attempt_connect(struct kcd_notif_state *st) {
    int error = 0;
    kstr query, conn_str;

    kstr_init(&query);
    kstr_init(&conn_str);

    kmod_log_msg(KCD_LOG_NOTIF, "kcd_notif_attempt_connect() called.\n");

    do {
        /* Connect to the database. */
        kstr_sf(&conn_str, "dbname=%s user=%s password=%s host=%s port=%s", 
                global_opts.db_name.data, global_opts.db_user.data,
                global_opts.db_password.data, global_opts.db_host.data,
                global_opts.db_port.data);

        error = kcd_open_pg_conn(&st->conn, conn_str.data);
        if (error) break;

        /* Freeze the database state. */
        error = kcd_open_pg_serializable_transaction(&st->conn);
        if (error) break;
        
        /* Listen to kws_list. */
        kstr_sf(&query, "LISTEN kws_list");
        error = kcd_exec_pg_query(&st->conn, query.data, NULL, "listen to workspace list");
        if (error) break;

        /* Fetch the current workspaces. */
        error = kcd_notif_fetch_new_kws(st);
        if (error) break;
        
        /* Thaw the database state. */
        error = kcd_commit_pg_transaction(&st->conn);
        if (error) break;

        /* Set the summary time. */
        kcd_notif_set_summary_time(st);

    } while (0);

    kstr_clean(&query);
    kstr_clean(&conn_str);

    return error;
}

/* Handle the case where we have no connection in the main loop. */
static int kcd_notif_loop_no_conn(struct kcd_notif_state *st, struct kselect *sel, int *skip_select_flag) {
    int64_t now_sec = ktime_now_sec();
    int64_t delay = st->conn_time + KCD_NOTIF_RECONNECT_DELAY - now_sec;

    kmod_log_msg(KCD_LOG_NOTIF, "kcd_notif_loop_no_conn() called.\n");

    /* It's time to connect. */
    if (delay <= 0) {

        /* Reset the connection time. */
        st->conn_time = now_sec;

        /* Try to connect. */
        if (kcd_notif_attempt_connect(st))
            return -1;

        /* Connection opened. */
        st->conn_flag = 1;

        /* Skip the select() call. */
        *skip_select_flag = 1;
    }

    /* Wait until the reconnection delay has passed. */
    else {
        ktime_from_msec(&sel->tv, delay*1000 + 1);
    }

    return 0;
}

/* Main loop of the notification mode. */
static int kcd_notif_loop(struct kcd_notif_state *st) {
    int error = 0;

    kmod_log_msg(KCD_LOG_NOTIF, "kcd_notif_loop() called.\n");

    while (1) {
        int skip_select_flag = 0;
        struct kselect sel;

        kdaemon_prepare_select(&sel);

        /* We don't have a database connection. */
        if (!st->conn_flag)
            error = kcd_notif_loop_no_conn(st, &sel, &skip_select_flag);

        /* We have a database connection. */
        else
            error = kcd_notif_loop_have_conn(st, &sel, &skip_select_flag);

        /* An error occurred. Enter the no database connection mode. */
        if (error) {
            kmod_log_msg(KCD_LOG_CRIT, "Notification error: %s.\n", kmod_strerror());
            error = 0;
            skip_select_flag = 1;
            st->conn_flag = 0;
            pg_db_reset(&st->conn);
            kcd_notif_state_clear_kws_tree(st);
        }

        /* Perform the select() call. */
        if (!skip_select_flag) {
            kmod_log_msg(KCD_LOG_NOTIF, "kcd_notif_loop(): doing select() call.\n");
            error = kdaemon_do_select(&sel);
            if (error) break;
            kmod_log_msg(KCD_LOG_NOTIF, "kcd_notif_loop(): out of select() call.\n");
        }
    }

    return error;
}

/* This function is called to execute the KCD notification mode. */
int kcd_notif_entry() {
    int error = 0;
    struct kcd_notif_state st;

    kcd_notif_state_init(&st);

    kdaemon_set_task("Notification");
    kmod_log_msg(KCD_LOG_NOTIF, "kcd_notif_entry() called.\n");

    do {
        /* Load the template. */
        error = kcd_mail_template_read(&st.notif_tmpl, "notif");
        if (error) break;

        /* Enter the main loop. */
        error = kcd_notif_loop(&st);
        if (error) break;

    } while (0);

    kcd_notif_state_clean(&st);

    return error;
}

