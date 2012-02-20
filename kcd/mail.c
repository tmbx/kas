#include "common.h"

/* Parse the SSMTP error code and update the KMOD error string if possible.
 * Return true if the error code was updated.
 */
static int kcd_process_parse_ssmtp_error(struct kcd_process *self) {
    char *pa, *pb, *p;
    kstr e;
    kstr *s = &self->err_str;
    
    if (!s->slen) return 0;

    /* This functions scans the string s and moves the pointer pa, the
       start of the good error message, to pb, the end of it. */

    pa = s->data;
    pb = s->data + s->slen;

    /* Skip the first "ssmtp:", it's always there. */
    p = strstr(s->data, ": ");
    if (p) pa = p + 2;
    else return 0;

    /*
      The kind of thing we want to make more friendly:

      ssmtp: RCPT TO:<blarg@teambox.co> (550 5.1.1
      <blarg@teambox.co>: Recipient address rejected: User unknown
      in local recipient table)
    */
    if ((p = strstr(pa, "RCPT TO"))) {
        /* Find '(' */
        p = strstr(p, "(");        
        if (p) {
            pa = p + 1;

            /* Find ')' */
            p = strstr(pa, ")");
            if (p) pb = p;

            /* Fucked up case? Reset.  Will happen if we find "()". */
            if (pb <= pa) {
                pa = s->data;
                pb = s->data + s->slen;
            }
        }
    }

    /* Create a new string starting from pa and ending at pb. */
    kstr_init_buf(&e, pa, pb - pa);
    kmod_set_error(e.data);
    kstr_clean(&e);
    
    return 1;
}

struct kcd_mail_template_element* kcd_mail_template_element_new() {
    struct kcd_mail_template_element *self = kcalloc(sizeof(struct kcd_mail_template_element));
    kstr_init(&self->value);
    return self;
}

void kcd_mail_template_element_destroy(struct kcd_mail_template_element *self) {
    if (self) {
        kstr_clean(&self->value);
        kfree(self);
    }
}

void kcd_mail_var_tree_init(krb_tree *self) {
    krb_tree_init_func(self, krb_tree_str_cmp);
}

void kcd_mail_var_tree_clean(krb_tree *self) {
    int i, size = krb_tree_size(self);
    struct krb_node *iter = krb_tree_iter_start(self);
    for (i = 0; i < size; i++) {
        krb_tree_iter_next(self, &iter);
        kfree(iter->key);
        kstr_destroy(iter->value);
    }
    krb_tree_clean(self);
}

/* Add or clobber a variable in the variable tree. */
void kcd_mail_var_tree_add(krb_tree *self, char *key, kstr *value) {
    struct krb_node *node = krb_tree_get(self, key);
    if (!node) node = krb_tree_add_fast(self, kutil_strdup(key), kstr_new());
    kstr_assign_kstr(node->value, value);
}

/* Same as above, but the value is HTML-escaped. */
void kcd_mail_var_tree_add_escaped(krb_tree *self, char *key, kstr *value) {
    kstr v;
    kstr_init(&v);
    kcd_mail_escapeHTML(value, &v);
    kcd_mail_var_tree_add(self, key, &v);
    kstr_clean(&v);
}

void kcd_mail_template_init(struct kcd_mail_template *self) {
    karray_init(&self->el_array);
}

void kcd_mail_template_clean(struct kcd_mail_template *self) {
    int i;
    for (i = 0; i < self->el_array.size; i++) kcd_mail_template_element_destroy(self->el_array.data[i]);
    karray_clean(&self->el_array);
}

/* Helper method for kcd_mail_template_read(). */
static void kcd_mail_template_read_helper(struct kcd_mail_template *self, int var_flag, kstr *value) {
    if (value->slen) {
        struct kcd_mail_template_element *el = kcd_mail_template_element_new();
        el->var_flag = var_flag;
        kstr_assign_kstr(&el->value, value);
        karray_push(&self->el_array, el);
        kstr_reset(value);
    }
}

/* Read the content of the template file 'name' and store the template strings
 * and variables in the template specified. Return 0 on success, -1 on error.
 */
int kcd_mail_template_read(struct kcd_mail_template *self, char *name) {
    int error = 0;
    char *c;
    kbuffer buf;
    kstr path, content, cur_str;
    
    kbuffer_init(&buf);
    kstr_init_sf(&path, "/etc/teambox/kcd/mail/%s.tmpl", name);
    kstr_init(&content);
    kstr_init(&cur_str);
    
    do {
        /* Read the template file. */
        error = kfs_read_file(path.data, &buf);
        if (error) break;
        kstr_assign_buf(&content, buf.data, buf.len);
        
        /* Process the template content. */
        for (c = content.data; *c; c++) {
        
            /* Template variable. */
            if (*c == '%') {
            
                /* Store the current template string, if any. */
                kcd_mail_template_read_helper(self, 0, &cur_str);
                
                /* Read the variable name. */
                for (c++; *c != '%'; c++) {
                    if (!isalpha(*c)) {
                        kmod_set_error("unterminated variable in kcd_mail_template");
                        error = -1;
                        break;
                    }
                    
                    kstr_append_char(&cur_str, *c);
                }
                
                if (error) break;
                
                /* Store the variable, if any. */
                kcd_mail_template_read_helper(self, 1, &cur_str);
            }
            
            /* Template string character. */
            else {
                char v = *c;
                
                /* Escape sequence. */
                if (v == '\\') {
                    c++;
                    v = *c;
                    
                    if (v != '\\' && v != '%') {
                        kmod_set_error("unexpected escape sequence in kcd_mail_template");
                        error = -1;
                        break;
                    }
                }
                
                kstr_append_char(&cur_str, v);
            }
        }
        
        if (error) break;
            
        /* Store the current template string, if any. */
        kcd_mail_template_read_helper(self, 0, &cur_str);
        
    } while (0);
    
    kbuffer_clean(&buf);
    kstr_clean(&path);
    kstr_clean(&content);
    kstr_clean(&cur_str);
    
    return error;
}

/* Expand the template specified in the string specified. */
int kcd_mail_template_generate(struct kcd_mail_template *self, krb_tree *var_tree, kstr *s) {
    int i;
    
    kstr_reset(s);

    for (i = 0; i < self->el_array.size; i++) {
        struct kcd_mail_template_element *el = karray_get(&self->el_array, i);

        if (!el->var_flag)
            kstr_append_kstr(s, &el->value);
        
        else {
            kstr *var = krb_tree_get(var_tree, el->value.data);
            
            if (!var) {
                kmod_set_error("undefined template variable %s", el->value.data);
                return -1;
            }
        
            kstr_append_kstr(s, var);
        }
    }

    return 0;
}

/* Send the mail specified. */
int kcd_send_mail(kstr *mail, kstr *to, int *failed_flag) {
    int error = 0;
    char *argv[]  = { global_opts.sendmail_path.data, to->data, NULL };
    struct kcd_process process;
    
    kmod_log_msg(KCD_LOG_MAIL, "kcd_send_mail() called");
    
    kcd_process_init(&process);
    process.log_level = KCD_LOG_MAIL;
    
    kstr_assign_kstr(&process.in_str, mail);
    *failed_flag = 0;
    
    do {
        error = kcd_process_start_and_collect(&process, argv, NULL, NULL, global_opts.sendmail_timeout * 1000, 0);
        if (error) break;
    
        kcd_process_log_output(&process, 1, 1);
    
        if (process.timeout_flag || process.failed_flag) {
            *failed_flag = 1;
            if (!kcd_process_parse_ssmtp_error(&process)) kcd_process_import_error(&process);
        }
        
    } while (0);
    
    kcd_process_clean(&process);
    
    return error;
}
    
/* Convert newlines in a string to HTML <br> */
void kcd_mail_escapeHTML(kstr *in, kstr *out) {
    char *c;
    kstr_reset(out);

    for (c = in->data; c < in->data + in->slen; c++) {
        switch (*c) {
        case '\n': kstr_append_cstr(out, "<br />"); break;
        case '<':  kstr_append_cstr(out, "&lt;");   break;
        case '>':  kstr_append_cstr(out, "&gt;");   break;
        case '&':  kstr_append_cstr(out, "&amp;");  break;
        case '\'': kstr_append_cstr(out, "&#39;");  break;
        case '"':  kstr_append_cstr(out, "&quot;"); break;
        default: kstr_append_char(out, *c);
        }
    }
}

/* Generate a string of the form 'User Name <Email>' or just 'Email' for the
 * user name and email specified. The user name can be empty. The string is not
 * HTML-escaped.
 */
void kcd_mail_fmt_email(kstr *name, kstr *email, kstr *out) {
    if (name->slen) kstr_sf(out, "%s <%s>", name->data, email->data);
    else kstr_assign_kstr(out, email);
}

/* Generate a string of the form <a href="mailto:Email">User Name</a>. The email
 * address replaces the user name if the user name is empty. The string is
 * HTML-escaped.
 */
void kcd_mail_fmt_mailto(kstr *name, kstr *email, kstr *out) {
    kstr tmp;
    kstr_init(&tmp);
    
    kcd_mail_escapeHTML(email, &tmp);
    kstr_sf(out, "<a href=\"mailto:%s\">", tmp.data);
    
    kcd_mail_escapeHTML(name->slen ? name : email, &tmp);
    kstr_append_sf(out, "%s</a>", tmp.data);
    
    kstr_clean(&tmp);
}

/* Generate the fields that are pulled from the OS and that may fail to be
 * generated for some reason.
 */
int kcd_mail_generate_system(kstr *mail_id, kstr *mail_date, kstr *boundary) {
    char buf[33], time_fmt[] = "%a, %d %b %Y %H:%M:%S %z";
    time_t t;
    struct tm *tm;
    buf[32] = 0;

    /* Get and format the local date. */
    time(&t);
    kstr_grow(mail_date, 256);
    if ((tm = localtime(&t)) == NULL || strftime(mail_date->data, mail_date->mlen, time_fmt, tm) <= 0) {
        kmod_set_error("failed to obtain and format local date");
        return -1;
    }
    mail_date->slen = strlen(mail_date->data);

    /* Generate a random message ID. */
    if (kutil_generate_alpha_random(buf, sizeof(buf) - 1) < 0) {
        kmod_set_error("failed to generate random message ID");
        return -1;
    }
    kstr_sf(mail_id, "<%s@teambox>", buf);
    
    /* Generate a random message boundary. */
    if (kutil_generate_alpha_random(buf, sizeof(buf) - 1) < 0) {
        kmod_set_error("failed to generate message boundary");
        return -1;
    }
    kstr_sf(boundary, "-----=_%s", buf);

    return 0;
}

void kcd_mail_invite_user_state_init(struct kcd_mail_invite_user_state *self) {
    memset(self, 0, sizeof(struct kcd_mail_invite_user_state));
    kstr_init(&self->from_name);
    kstr_init(&self->from_email);
    kstr_init(&self->kws_name);
    kstr_init(&self->user_msg);
    kstr_init(&self->mail_id);
    kstr_init(&self->mail_date);
    kstr_init(&self->mail_boundary);
    kstr_init(&self->mail_content);
}

void kcd_mail_invite_user_state_clean(struct kcd_mail_invite_user_state *self) {
    kstr_clean(&self->from_name);
    kstr_clean(&self->from_email);
    kstr_clean(&self->kws_name);
    kstr_clean(&self->user_msg);
    kstr_clean(&self->mail_id);
    kstr_clean(&self->mail_date);
    kstr_clean(&self->mail_boundary);
    kstr_clean(&self->mail_content);
}

/* Set the common variables used by the invitation mail templates. */
static void kcd_mail_set_invitation_variables(krb_tree *var_tree, struct kcd_mail_invite_user_state *st) {
    kstr tmp, tmp2;
    kstr_init(&tmp);
    kstr_init(&tmp2);
    
    kcd_mail_var_tree_add(var_tree, "MailDate", &st->mail_date);
    
    kstr_assign_cstr(&tmp2, KCD_KWS_NAME " Invitation");
    kcd_mail_fmt_email(&tmp2, &global_opts.mail_sender, &tmp);
    kcd_mail_var_tree_add(var_tree, "MailFrom", &tmp);
    
    kcd_mail_fmt_email(st->to_name, st->to_email, &tmp);
    kcd_mail_var_tree_add(var_tree, "MailTo", &tmp);
    
    kcd_mail_fmt_mailto(&st->from_name, &st->from_email, &tmp);
    kcd_mail_var_tree_add(var_tree, "SenderEmailText", &st->from_email);
    kcd_mail_var_tree_add(var_tree, "SenderName", &tmp);
    
    kcd_mail_var_tree_add(var_tree, "TeamboxName", &st->kws_name);
    kcd_mail_var_tree_add_escaped(var_tree, "EscTeamboxName", &st->kws_name);
    kcd_mail_var_tree_add(var_tree, "TeamboxURL", st->url);
    
    kstr_clean(&tmp);
    kstr_clean(&tmp2);
}

/* Generate the invitation email of a user. */
static int kcd_mail_generate_invite_msg(struct kcd_mail_invite_user_state *st) {
    int error = 0;
    kstr user_output;
    krb_tree mail_tree, user_tree;
    struct kcd_mail_template mail_tmpl, user_tmpl;

    kmod_log_msg(KCD_LOG_MAIL, "kcd_mail_generate_invite_msg() called.\n");
    
    kstr_init(&user_output);
    kcd_mail_var_tree_init(&mail_tree);
    kcd_mail_var_tree_init(&user_tree);
    kcd_mail_template_init(&mail_tmpl);
    kcd_mail_template_init(&user_tmpl);
    
    do {
        /* Generate the system fields. */
        error = kcd_mail_generate_system(&st->mail_id, &st->mail_date, &st->mail_boundary);
        if (error) break;
        
        /* Generate the user message output. */
        error = kcd_mail_template_read(&user_tmpl, st->user_msg.slen ? "invite_custom_msg" : "invite_default_msg");
        if (error) break;
        kcd_mail_set_invitation_variables(&user_tree, st);
        kcd_mail_var_tree_add_escaped(&user_tree, "MessageText", &st->user_msg);
        error = kcd_mail_template_generate(&user_tmpl, &user_tree, &user_output);
        if (error) break;
        
        /* Generate the invitation email content. */
        error = kcd_mail_template_read(&mail_tmpl, "invite");
        if (error) break;
        kcd_mail_set_invitation_variables(&mail_tree, st);
        kcd_mail_var_tree_add(&mail_tree, "MailMessageID", &st->mail_id);
        kcd_mail_var_tree_add(&mail_tree, "Boundary", &st->mail_boundary);
        kcd_mail_var_tree_add(&mail_tree, "UserMsg", &user_output);
        error = kcd_mail_template_generate(&mail_tmpl, &mail_tree, &st->mail_content);
        if (error) break;
    
    } while (0);
        
    kstr_clean(&user_output);
    kcd_mail_var_tree_clean(&mail_tree);
    kcd_mail_var_tree_clean(&user_tree);
    kcd_mail_template_clean(&mail_tmpl);
    kcd_mail_template_clean(&user_tmpl);

    return error;
}

/* Invite a user to a workspace. */
int kcd_mail_invite_user(struct kcd_mail_invite_user_state *st, int *failed_flag) {
    int error = 0;
    *failed_flag = 0;
    
    do {
        error = kcd_mail_generate_invite_msg(st);
        if (error) break;
        
        error = kcd_send_mail(&st->mail_content, st->to_email, failed_flag);
        if (error) break;
    
        if (*failed_flag) kstr_sf(st->invite_error, "%s", kmod_strerror());
    
    } while (0);
    
    return error;
}

