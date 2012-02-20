/* Copyright (C) 2008-2012 Opersys inc., All rights reserved. */

#include "common.h"

struct kcd_kfs_uploaded_file* kcd_kfs_uploaded_file_new() {
    struct kcd_kfs_uploaded_file *self = kcalloc(sizeof(struct kcd_kfs_uploaded_file));
    kstr_init(&self->share_path);
    kstr_init(&self->perm_path);
    kbuffer_init(&self->hash);
    return self;
}

void kcd_kfs_uploaded_file_destroy(struct kcd_kfs_uploaded_file *self) {
    if (self) {
        kstr_clean(&self->share_path);
        kstr_clean(&self->perm_path);
        kbuffer_clean(&self->hash);
        kfree(self);
    }
}

void kcd_mgt_user_ticket_init(struct kcd_mgt_user_ticket *self) {
    memset(self, 0, sizeof(struct kcd_mgt_user_ticket));
    kstr_init(&self->name);
    kstr_init(&self->email);
    kstr_init(&self->host);
}

void kcd_mgt_user_ticket_clean(struct kcd_mgt_user_ticket *self) {
    kstr_clean(&self->name);
    kstr_clean(&self->email);
    kstr_clean(&self->host);
}

void kcd_global_user_usage_info_init(struct kcd_global_user_usage_info *self) {
    memset(self, 0, sizeof(struct kcd_global_user_usage_info));
}

void kcd_global_user_usage_info_clean(struct kcd_global_user_usage_info *self) {
    self = NULL;
}

void kcd_global_user_usage_info_reset(struct kcd_global_user_usage_info *self) {
    self->nb_non_pb_kws = self->nb_pb_kws = 0;
    self->kfs_usage = 0;
}

void kcd_global_user_license_info_init(struct kcd_global_user_license_info *self) {
    memset(self, 0, sizeof(struct kcd_global_user_license_info));
    kstr_init(&self->license_name);
}

void kcd_global_user_license_info_clean(struct kcd_global_user_license_info *self) {
    kstr_clean(&self->license_name);
}

void kcd_global_user_license_info_reset(struct kcd_global_user_license_info *self) {
    kstr_reset(&self->license_name);
    self->nb_non_pb_kws = self->nb_pb_kws = 0;
    self->kfs_usage = self->vnc_session_time = 0;
    self->secure_kws_flag = 0;
}

/* Parse a user ticket in ANP format. */
int kcd_mgt_parse_user_ticket(struct kcd_mgt_user_ticket *ticket, kbuffer *buf) {
    buf->pos = 38;

    if (anp_read_kstr(buf, &ticket->name) ||
	anp_read_kstr(buf, &ticket->email) ||
	anp_read_kstr(buf, &ticket->host) ||
	anp_read_uint32(buf, &ticket->port) ||
	anp_read_uint64(buf, &ticket->key_id))
	return -1;
    
    return 0;
}

/* Generate an email ID. The characters a-z, 0-9 are used, yielding an entropy
 * slightly above 5 bits per character. We want about 16 bytes of entropy in the
 * email ID, so we use 25 characters.
 */
void kcd_mgt_generate_email_id(kstr *email_id) {
    const int nb_char = 25;
    int i;
    unsigned char buf[nb_char];
    
    kstr_reset(email_id);
    kutil_generate_random(buf, nb_char);
    
    for (i = 0; i < nb_char; i++) {
        /* We're lowering the entropy a bit here due to rounding issues. */
        int r = MIN(35, (int) ((double) buf[i] / 255.0 * 36.0));
        char c = r < 26 ? 'a' + r : '0' + r - 26;
        kstr_append_char(email_id, c);
    }
}

/* This function returns true if the file name specified is valid. */
int kcd_kfs_is_file_name_valid(kstr *name) {
    static char* reserved_words[] = {
        "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
        "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4",
        "LPT5", "LPT6", "LPT7", "LPT8", "LPT9", NULL
    };
    
    static char allowed_chars[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    
    int i;
    
    if (name->slen == 0) {
        kmod_set_error("empty file name");
        return 0;
    }
    
    if (isspace(name->data[0])) {
        kmod_set_error("leading white space");
        return 0;
    }
    
    if (isspace(name->data[name->slen - 1])) {
        kmod_set_error("trailing white space");
        return 0;
    }
    
    if (name->data[name->slen - 1] == '.') {
        kmod_set_error("trailing dot");
        return 0;
    }
    
    i = 0;
    
    while (1) {
        char *word = reserved_words[i++];
        if (! word) break;
        
        if (! strcasecmp(name->data, word)) {
            kmod_set_error("reserved name");
            return 0;
        }
    }
    
    for (i = 0; i < name->slen; i++) {
        unsigned int code = (unsigned char) name->data[i];
        if (! allowed_chars[code]) {
            kmod_set_error("illegal character ('%c', code %u)", (char) code, code);
            return 0;
        }
    }
    
    return 1;
}

/* This function returns true if the KFS path specified is valid. */
int kcd_kfs_is_path_valid(kstr *path) {
    int error = 0;
    ssize_t i;
    kstr abs_path, norm_path, final_name;
    karray components;
    
    kstr_init(&abs_path);
    kstr_init(&norm_path);
    kstr_init(&final_name);
    karray_init(&components);
    
    do {
        /* The path should not be empty. */
        if (! path->slen) {
            kmod_set_error("empty path");
            error = -1;
            break;
        }
        
        /* The path should not be too long. */
        if (path->slen > 200) {
            kmod_set_error("path too long");
            error = -1;
            break;
        }
        
        /* Add a '/' to the original path to make it absolute. */
        kstr_sf(&abs_path, "/%s", path->data);
        
        /* Copy and normalize the path. */
        kstr_assign_kstr(&norm_path, &abs_path);
        kpath_normalize(&norm_path, 0, KPATH_FORMAT_UNIX);
        
        /* If the absolute and the normalized path do not match, the path is
         * invalid.
         */
        if (! kstr_equal_kstr(&abs_path, &norm_path)) {
            kmod_set_error("the path is not normalized");
            error = -1;
            break;
        }
        
        /* Check the individual components. */
        kcd_kfs_split_path(path, &components, &final_name);
        
        for (i = 0; i < components.size; i++) {
            if (! kcd_kfs_is_file_name_valid(components.data[i])) {
                error = -1;
                break;
            }
        }
        
        if (error) break;
        
        if (! kcd_kfs_is_file_name_valid(&final_name)) {
            error = -1;
            break;
        }
        
    } while (0);
    
    if (error) kmod_append_error("path '%s' is invalid", path->data);
    
    kstr_clean(&abs_path);
    kstr_clean(&norm_path);
    kstr_clean(&final_name);
    karray_clear_kstr(&components);
    karray_clean(&components);
    
    return (error == 0);
}

/* This function splits the path specified into an array of directory components
 * and the final file name.
 */
void kcd_kfs_split_path(kstr *path, karray *components, kstr *final_name) {
    ssize_t i;
    struct kpath_dir dir;
    kstr dir_part, name, ext;
    
    kpath_dir_init(&dir);
    kstr_init(&dir_part);
    kstr_init(&name);
    kstr_init(&ext);
    
    karray_reset(components);

    kpath_split(path, &dir_part, &name, &ext, KPATH_FORMAT_UNIX);
    kstr_assign_kstr(final_name, &name);
    if (ext.slen) {
        kstr_append_char(final_name, '.');
        kstr_append_kstr(final_name, &ext);
    }
    
    if (kstr_equal_cstr(&dir_part, "./")) kstr_reset(&dir_part);
    kpath_decompose_dir(&dir_part, &dir, KPATH_FORMAT_UNIX);
    for (i = 0; i < dir.components.size; i++) karray_push(components, dir.components.data[i]);
    karray_reset(&dir.components);
    
    kpath_dir_clean(&dir);
    kstr_clean(&dir_part);
    kstr_clean(&name);
    kstr_clean(&ext);
}

/* Setup the message specified so that it indicates the failure specified. The
 * error message is imported from the last KMOD error message. The message
 * payload is reset by the function prior to it being written.
 */
void kcd_kanp_set_failure(struct anp_msg *msg, uint32_t error_type) {
    msg->type = KANP_RES_FAIL;
    kbuffer_reset(&msg->payload);
    anp_write_uint32(&msg->payload, error_type);
    anp_write_kstr(&msg->payload, kmod_kstrerror());
}

/* Shortcut functions for failures. */
void kcd_kanp_set_gen_failure(struct anp_msg *msg) { kcd_kanp_set_failure(msg, KANP_RES_FAIL_GEN); }
void kcd_kanp_set_perm_failure(struct anp_msg *msg) { kcd_kanp_set_failure(msg, KANP_RES_FAIL_PERM_DENIED); }

/* FIXME: refactor all error handling functions to use the functions below
 * exclusively.
 */
void kcd_kanp_format_failure(kbuffer *payload, uint32_t error_type) {
    kbuffer_reset(payload);
    anp_write_uint32(payload, error_type);
    anp_write_kstr(payload, kmod_kstrerror());
}

void kcd_kanp_resource_quota_failure(kbuffer *payload, uint32_t sub_error_type) {
     kcd_kanp_format_failure(payload, KANP_RES_FAIL_RESOURCE_QUOTA);
     anp_write_uint32(payload, sub_error_type);
}

