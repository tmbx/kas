/* Copyright (C) 2008-2012 Opersys inc., All rights reserved. */

#ifndef _PG_COMMON_H
#define _PG_COMMON_H

/* Current marketing name for a workspace. */
#define KCD_KWS_NAME                "Teambox"
#define KCD_KWSES_NAME              "Teamboxes"

/* Supported workspace login types. */
#define KCD_KWS_LOGIN_TYPE_NORMAL   1
#define KCD_KWS_LOGIN_TYPE_SECURE   2
#define KCD_KWS_LOGIN_TYPE_ROOT     3
#define KCD_KWS_LOGIN_TYPE_KWMO     4

/* Represent a file being uploaded. */
struct kcd_kfs_uploaded_file {
    
    /* True if this is a file creation, false if this is an update. */
    uint32_t create_flag;
    
    /* File inode. */
    uint64_t inode;
    
    /* Size of the file. */
    uint64_t size;
    
    /* Path to the file in the share directory. */
    kstr share_path;
    
    /* Path to the file in the storage area. */
    kstr perm_path;
    
    /* Hash of the file. */
    kbuffer hash;
};

/* Ticket authorizing the creation / joining of a workspace. */
struct kcd_mgt_user_ticket {
    kstr name;
    kstr email;
    kstr host;
    uint32_t port;
    uint64_t key_id;
};

/* Resource usage information of a global user. */
struct kcd_global_user_usage_info {
    
    /* Number of non-public and public workspaces. */
    uint32_t nb_non_pb_kws;
    uint32_t nb_pb_kws;
    
    /* Total KFS storage usage, in bytes. */
    uint64_t kfs_usage;
};

/* License information about a global user. */
struct kcd_global_user_license_info {
    
    /* License name. */
    kstr license_name;
    
    /* Allowed number of non-public and public workspaces. */
    uint32_t nb_non_pb_kws;
    uint32_t nb_pb_kws;
    
    /* Allowed total KFS storage usage, in bytes. */
    uint64_t kfs_usage;
    
    /* True if public workspaces can be created. */
    uint32_t secure_kws_flag;
    
    /* Allowed screen sharing session length, in seconds. */
    uint64_t vnc_session_time;
};

struct kcd_kfs_uploaded_file* kcd_kfs_uploaded_file_new();
void kcd_kfs_uploaded_file_destroy(struct kcd_kfs_uploaded_file *self);
void kcd_mgt_user_ticket_init(struct kcd_mgt_user_ticket *self);
void kcd_mgt_user_ticket_clean(struct kcd_mgt_user_ticket *self);
void kcd_global_user_usage_info_init(struct kcd_global_user_usage_info *self);
void kcd_global_user_usage_info_clean(struct kcd_global_user_usage_info *self);
void kcd_global_user_usage_info_reset(struct kcd_global_user_usage_info *self);
void kcd_global_user_license_info_init(struct kcd_global_user_license_info *self);
void kcd_global_user_license_info_clean(struct kcd_global_user_license_info *self);
void kcd_global_user_license_info_reset(struct kcd_global_user_license_info *self);
int kcd_mgt_parse_user_ticket(struct kcd_mgt_user_ticket *ticket, kbuffer *buf);
void kcd_mgt_generate_email_id(kstr *email_id);
int kcd_kfs_is_file_name_valid(kstr *name);
int kcd_kfs_is_path_valid(kstr *path);
void kcd_kfs_split_path(kstr *path, karray *components, kstr *final_name);
void kcd_kanp_set_failure(struct anp_msg *msg, uint32_t error_type);
void kcd_kanp_set_gen_failure(struct anp_msg *msg);
void kcd_kanp_set_perm_failure(struct anp_msg *msg);
void kcd_kanp_format_failure(kbuffer *payload, uint32_t error_type);
void kcd_kanp_resource_quota_failure(kbuffer *payload, uint32_t sub_error_type);

#endif

