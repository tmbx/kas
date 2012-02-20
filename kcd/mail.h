/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _MAIL_H
#define _MAIL_H

/* A template element is either a plain string that is pasted as-is or the name
 * of a variable that is going to be substituted.
 */
struct kcd_mail_template_element {
    
    /* True if the element is a variable. */
    int var_flag;
    
    /* Template string or variable name. */
    kstr value;
};

/* Template definition read from a file. */
struct kcd_mail_template {

    /* Array of mail_template elements. */
    karray el_array;    
};

/* State used to send invitation emails. */
struct kcd_mail_invite_user_state {

    /* Name and email address of the sender. */
    kstr from_name;
    kstr from_email;
    
    /* Name of the workspace. */
    kstr kws_name;
    
    /* User invitation message. */
    kstr user_msg;
    
    /* Mail formatting information. */
    kstr mail_id;
    kstr mail_date;
    kstr mail_boundary;
    
    /* Content of the mail to send. */
    kstr mail_content;
    
    /* Name and email address of the invited user. */
    kstr *to_name;
    kstr *to_email;
    
    /* Invitation URL. */
    kstr *url;
    
    /* Invitation error string. */
    kstr *invite_error;
};

struct kcd_mail_template_element* kcd_mail_template_element_new();
void kcd_mail_template_element_destroy(struct kcd_mail_template_element *self);
void kcd_mail_var_tree_init(krb_tree *self);
void kcd_mail_var_tree_clean(krb_tree *self);
void kcd_mail_var_tree_add(krb_tree *self, char *key, kstr *value);
void kcd_mail_var_tree_add_escaped(krb_tree *self, char *key, kstr *value);
void kcd_mail_template_init(struct kcd_mail_template *self);
void kcd_mail_template_clean(struct kcd_mail_template *self);
int kcd_mail_template_read(struct kcd_mail_template *self, char *name);
int kcd_mail_template_generate(struct kcd_mail_template *self, krb_tree *var_tree, kstr *s);
int kcd_send_mail(kstr *mail, kstr *to, int *failed_flag);
void kcd_mail_escapeHTML(kstr *in, kstr *out);
void kcd_mail_fmt_email(kstr *name, kstr *email, kstr *out);
void kcd_mail_fmt_mailto(kstr *name, kstr *email, kstr *out);
int kcd_mail_generate_system(kstr *mail_id, kstr *mail_date, kstr *boundary);
void kcd_mail_invite_user_state_init(struct kcd_mail_invite_user_state *self);
void kcd_mail_invite_user_state_clean(struct kcd_mail_invite_user_state *self);
int kcd_mail_invite_user(struct kcd_mail_invite_user_state *st, int *failed_flag);

#endif

