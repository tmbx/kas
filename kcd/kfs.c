/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#include <mhash.h>
#include "common.h"

/* Preferred maximum size of a download message. */
#define MAX_DOWNLOAD_SIZE       (256*1024)

/* Preferred minimum size of a download chunk. */
#define MIN_DOWNLOAD_CHUNK_SIZE (64*1024)

/* This structure contains the data required to process an upload request. */
struct kcd_kfs_mode_upload {
    
    /* Request info. */
    uint32_t share_id;
    uint64_t commit_id;
    uint64_t public_email_id;
    
    /* Total file size and quota of the workspace. */
    uint64_t kws_total_size;
    uint64_t kws_quota;
    
    /* Total size of the commited files in this upload. This does not account
     * for the file currently being uploaded since it is not yet commited.
     */
    uint64_t commited_total_size;
    
    /* True if the phase 2 is active. */
    int phase_2_active;
    
    /* Number of files to upload. */
    uint32_t nb_upload;
    
    /* Number of files to delete permanently. */
    uint32_t nb_perm_delete;
    
    /* Index in the upload arrays corresponding to the file currently being
     * uploaded.
     */
    uint32_t upload_index;
    
    /* Number of files commited. */
    uint32_t nb_commit;
    
    /* Path on the storage filesystem to the file being uploaded. */
    kstr uploaded_path;
    
    /* Pointer to the file object corresponding to the file currently being
     * uploaded.
     */
    FILE *uploaded_file;
    
    /* Hash context corresponding to the file being uploaded. */
    MHASH hash_context;
    
    /* Hash of the uploaded file. */
    unsigned char uploaded_hash[16];
    
    /* Size of the uploaded file. */
    uint64_t uploaded_size;
    
    /* Array containing the files to upload. */
    karray upload_array;
    
    /* Array containing the files commited. */
    karray commit_array;
    
    /* Array containing the path of the files to delete permanently. */
    karray perm_delete_array;
    
    /* Ticket mode state. */
    struct kcd_ticket_mode_state *tms;
};

/* This structure contains the data required to process a download request. */
struct kcd_kfs_mode_download {
    
    /* Request info. */
    uint32_t share_id;
    
    /* Number of files to download. */
    uint32_t nb_download;
    
    /* Index in the download arrays corresponding to the file currently being
     * downloaded.
     */
    uint32_t download_index;
    
    /* Pointer to the file object corresponding to the file currently being
     * downloaded.
     */
    FILE *downloaded_file;
    
    /* Size of the downloaded file. */
    uint64_t downloaded_size;
    
    /* Size of the remaining data to read in the downloaded file. */
    uint64_t remaining_size;
    
    /* Array containing the inode ID of the files to download. */
    karray download_inode_array;
    
    /* Array containing the offset of the files to download. */
    karray download_offset_array;
    
    /* Array containing the commit ID of the files to download. */
    karray download_commit_array;
    
    /* Array containing the permanent path of the files to download. */
    karray download_path_array;
    
    /* Ticket mode state. */
    struct kcd_ticket_mode_state *tms;
};

static void kcd_kfs_mode_upload_init(struct kcd_kfs_mode_upload *self, struct kcd_ticket_mode_state *tms) {
    memset(self, 0, sizeof(struct kcd_kfs_mode_upload));
    kstr_init(&self->uploaded_path);
    karray_init(&self->upload_array);
    karray_init(&self->commit_array);
    karray_init(&self->perm_delete_array);
    self->tms = tms;
}

static void kcd_kfs_mode_upload_clean(struct kcd_kfs_mode_upload *self) {
    uint32_t i;
    
    kstr_clean(&self->uploaded_path);
    kfs_fclose(&self->uploaded_file, 1);
    if (self->hash_context) mhash_deinit(self->hash_context, NULL);
    
    for (i = 0; i < self->nb_upload; i++) kcd_kfs_uploaded_file_destroy(self->upload_array.data[i]);
    karray_clean(&self->upload_array);
    karray_clean(&self->commit_array);
    karray_clear_kstr(&self->perm_delete_array);
    karray_clean(&self->perm_delete_array);
}

static void kcd_kfs_mode_download_init(struct kcd_kfs_mode_download *self, struct kcd_ticket_mode_state *tms) {
    memset(self, 0, sizeof(struct kcd_kfs_mode_download));
    karray_init(&self->download_inode_array);
    karray_init(&self->download_offset_array);
    karray_init(&self->download_commit_array);
    karray_init(&self->download_path_array);
    self->tms = tms;
}

static void kcd_kfs_mode_download_clean(struct kcd_kfs_mode_download *self) {
    ssize_t i;
    
    kfs_fclose(&self->downloaded_file, 1);
    
    for (i = 0; i < self->download_inode_array.size; i++) kfree(self->download_inode_array.data[i]);
    karray_clean(&self->download_inode_array);
    
    for (i = 0; i < self->download_offset_array.size; i++) kfree(self->download_offset_array.data[i]);
    karray_clean(&self->download_offset_array);
    
    for (i = 0; i < self->download_commit_array.size; i++) kfree(self->download_commit_array.data[i]);
    karray_clean(&self->download_commit_array);
    
    for (i = 0; i < self->download_path_array.size; i++) kstr_destroy(self->download_path_array.data[i]);
    karray_clean(&self->download_path_array);
}

/* Obtain the total file size and quota of the workspace. */
static int kcd_kfs_get_kws_total_file_size_and_quota(struct kcd_ticket_mode_state *tms, uint64_t *total_size, 
                                                     uint64_t *quota) {
    int error = 0;
    PGresult *pg_res = NULL;
    kstr *query = &tms->query;
    
    do {
        kstr_sf(query, "SELECT file_size, file_quota FROM kcd_kws_kfs_limit WHERE kws_id = "PRINTF_64"u", tms->kws_id);
        error = kcd_exec_pg_query(&tms->db_conn, query->data, &pg_res, "obtain quota");
        if (error) break;
        
        if (PQntuples(pg_res) != 1) {
            kmod_set_error("cannot obtain quota: no such workspace");
            error = -2;
            break;
        }
        
        *total_size = pg_db_get_uint64(pg_res, 0, 0);
        *quota = pg_db_get_uint64(pg_res, 0, 1);
        
    } while (0);
        
    pg_db_destroy_res(&pg_res);
    
    return error;
}

/* Obtain the share ID from the ticket provided by the user. */
static int kcd_kfs_get_share_id_from_ticket(struct kcd_ticket_mode_state *tms, uint32_t *share_id) {
    if (anp_read_uint32(&tms->ticket.ext, share_id)) return -1;
    return 0;
}

/* This function refreshes the upload entry in phase 2. */
static int kcd_kfs_refresh_phase_2_upload(struct kcd_kfs_mode_upload *mu) {
    struct kcd_ticket_mode_state *tms = mu->tms;
    kbuffer *kbb = &tms->kws_bound_buf;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_refresh_phase_2_upload() called.\n");
    
    anp_write_uint32(kbb, mu->share_id);
    anp_write_uint64(kbb, mu->commit_id);
    return kcd_ticket_mode_kws_bound_query(tms, "refresh_upload", ktime_now_sec(), NULL);
}

/* This function posts the phase 2 event. */
static int kcd_kfs_post_phase_2_event(struct kcd_kfs_mode_upload *mu) {
    int error = 0;
    uint32_t i;
    uint64_t date = ktime_now_sec();
    kbuffer evt;
    kbuffer notif;
    struct kcd_ticket_mode_state *tms = mu->tms;
    kbuffer *kbb = &tms->kws_bound_buf;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_post_phase_2_event() called.\n");
    
    kbuffer_init(&evt);
    kbuffer_init(&notif);
    
    do {
        anp_write_uint64(&evt, tms->kws_id);
	anp_write_uint64(&evt, date);
	anp_write_uint32(&evt, tms->user_id);
	anp_write_uint32(&evt, mu->share_id);
	anp_write_uint64(&evt, mu->commit_id);
	anp_write_uint32(&evt, mu->nb_commit);
        
        anp_write_uint64(&notif, mu->public_email_id);
        anp_write_uint32(&notif, mu->nb_commit);
        
        for (i = 0; i < mu->nb_commit; i++) {
            struct kcd_kfs_uploaded_file *f = mu->commit_array.data[i];
            
            anp_write_uint64(&evt, f->inode);
            anp_write_uint64(&evt, f->size);
            anp_write_bin(&evt, &f->hash);
            
            anp_write_uint32(&notif, f->create_flag);
            anp_write_kstr(&notif, &f->share_path);
        }
        
        anp_write_uint32(kbb, mu->share_id);
        anp_write_uint64(kbb, mu->commit_id);
        anp_write_uint64(kbb, mu->public_email_id);
        anp_write_bin(kbb, &evt);
        anp_write_bin(kbb, &notif);
        anp_write_uint32(kbb, mu->nb_commit);
        
        for (i = 0; i < mu->nb_commit; i++) {
            struct kcd_kfs_uploaded_file *f = mu->commit_array.data[i];
            anp_write_uint64(kbb, f->inode);
            anp_write_uint64(kbb, f->size);
        }
        
        error = kcd_ticket_mode_kws_bound_query(tms, "upload_phase_two", date, NULL);
        if (error) break;
        
    } while (0);
    
    kbuffer_clean(&evt);
    kbuffer_clean(&notif);
    
    return error;
}

/* This function opens the file currently being uploaded if it is not already
 * open. The hash context is also initialized.
 */
static int kcd_open_phase_2_file_if_needed(struct kcd_kfs_mode_upload *mu) {
    int error = 0;
    int i;
    karray components;
    kstr *rel_path = &((struct kcd_kfs_uploaded_file *)mu->upload_array.data[mu->upload_index])->perm_path;
    kstr final_name;
    struct kcd_ticket_mode_state *tms = mu->tms;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_open_phase_2_file_if_needed() called.\n");
    
    karray_init(&components);
    kstr_init(&final_name);
    
    do {
        /* The file is already open. */
        if (mu->uploaded_file) break;
        
        /* Create the missing directories. Ignore failures since concurrent
         * operations may be taking place. We will detect that something is
         * wrong when we try to open the file.
         */
        kcd_kfs_split_path(rel_path, &components, &final_name);
        kstr_sf(&mu->uploaded_path, "%s"PRINTF_64"u/", global_opts.kfs_dir_path.data, tms->kws_id);
        kfs_mkdir(mu->uploaded_path.data);
        
        for (i = 0; i < components.size; i++) {
            kstr *c = components.data[i];
            kstr_append_kstr(&mu->uploaded_path, c);
            kstr_append_char(&mu->uploaded_path, '/');
            kfs_mkdir(mu->uploaded_path.data);
        }
        
        kstr_append_kstr(&mu->uploaded_path, &final_name);
        
        /* Open the file. */
        error = kfs_fopen(&mu->uploaded_file, mu->uploaded_path.data, "wb");
        if (error) break;
        
        /* Open the hash context. */
        assert(mu->hash_context == NULL);
        mu->hash_context = mhash_init(MHASH_MD5);
        
        /* Reset the uploaded size. */
        mu->uploaded_size = 0;
        
    } while (0);
    
    karray_clear_kstr(&components);
    karray_clean(&components);
    kstr_clean(&final_name);
    
    return error;
}

/* This function closes the file currently being uploaded if it is open and
 * deletes it if requested. The hash context is also closed.
 */
static int kcd_close_phase_2_file_if_needed(struct kcd_kfs_mode_upload *mu, int delete_flag) {
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_close_phase_2_file_if_needed() called.\n");
    
    if (mu->hash_context) {
        mhash_deinit(mu->hash_context, mu->uploaded_hash);
        mu->hash_context = NULL;
    }
    
    if (!mu->uploaded_file) return 0;
    
    if (delete_flag) {
        kfs_fclose(&mu->uploaded_file, 1);
        return kfs_delete(mu->uploaded_path.data, 1);
    }
    
    return kfs_fclose(&mu->uploaded_file, 0);
}

/* This function handles a chunk in phase 2. */
static int kcd_kfs_handle_phase_2_chunk(struct kcd_kfs_mode_upload *mu) {
    int error = 0;
    struct kcd_ticket_mode_state *tms = mu->tms;
    uint64_t upload_total_size;
    kbuffer chunk;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_handle_phase_2_chunk() called.\n");
    
    kbuffer_init(&chunk);
    
    do {
        /* Get the chunk data. */
        if (anp_read_bin(&tms->in_msg->payload, &chunk)) {
            error = -2;
            break;
        }
        
        /* Open the current file, if needed. */
        error = kcd_open_phase_2_file_if_needed(mu);
        if (error) break;
        
        /* Hash the data. */
        mhash(mu->hash_context, chunk.data, chunk.len);
        
        /* Update the file size. */
        mu->uploaded_size += chunk.len;
        
        /* Compute the current total size of the upload. */
        upload_total_size = mu->commited_total_size + mu->uploaded_size;
        
        /* Debugging. Remove me eventually. */
        kmod_log_msg(KCD_LOG_KFS, "Upload file size %llu, upload total size %llu, "
                                  "kws usage %llu, kws quota %llu, license usage %llu, license quota %llu\n",
                                  mu->uploaded_size, upload_total_size,
                                  mu->kws_total_size, mu->kws_quota,
                                  tms->usage_info.kfs_usage, tms->license_info.kfs_usage);
        
        /* Check if we're busting the per-workspace quota. */
        if (upload_total_size + mu->kws_total_size > mu->kws_quota) {
            kmod_set_error(KCD_KWS_NAME " file quota exceeded");
            error = kcd_ticket_mode_set_failure(tms, KANP_RES_FAIL_FILE_QUOTA_EXCEEDED);
            break;
        }
        
        /* Check if we're busting the license quota. */
        if (upload_total_size + tms->usage_info.kfs_usage > tms->license_info.kfs_usage) {
            kmod_set_error("license file quota exceeded");
            kcd_kanp_resource_quota_failure(kcd_ticket_mode_failure(tms), KANP_RESOURCE_QUOTA_GENERAL);
            error = -3;
            break;
        }
        
        /* Write the chunk data in the file. */
        error = kfs_fwrite(mu->uploaded_file, chunk.data, chunk.len);
        if (error) break;
        
    } while (0);
    
    kbuffer_clean(&chunk);
    
    return error;
}

/* This function handles a commit in phase 2. */
static int kcd_kfs_handle_phase_2_commit(struct kcd_kfs_mode_upload *mu) {
    int error = 0;
    kbuffer user_hash;
    struct kcd_kfs_uploaded_file *f;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_handle_phase_2_commit() called.\n");
    
    kbuffer_init(&user_hash);
    
    do {
        /* Get the user hash. */
        if (anp_read_bin(&mu->tms->in_msg->payload, &user_hash)) {
            error = -2;
            break;
        }
        
        /* Open the current file, if needed. This case can happen legally if the
         * user uploaded an empty file.
         */
        error = kcd_open_phase_2_file_if_needed(mu);
        if (error) break;
        
        /* Close the uploaded file. */
        error = kcd_close_phase_2_file_if_needed(mu, 0);
        if (error) break;
        
        /* Verify if the hashes match. */
        if (user_hash.len != 16 || memcmp(user_hash.data, mu->uploaded_hash, 16)) {
            kmod_set_error("the computed file hash does not match");
            error = -2;
            break;
        }
        
        /* Remember the information about this file. */
        f =  mu->upload_array.data[mu->upload_index];
        karray_push(&mu->commit_array, f);
        f->size = mu->uploaded_size;
        kbuffer_write(&f->hash, mu->uploaded_hash, 16);
        mu->nb_commit++;
        
        /* Update the commited total size. */
        mu->commited_total_size += mu->uploaded_size;
        
        /* Pass to the next file. */
        mu->upload_index++;
    
    } while (0);
    
    kbuffer_clean(&user_hash);
    
    return error;
}

/* This function handles an abort in phase 2. */
static int kcd_kfs_handle_phase_2_abort(struct kcd_kfs_mode_upload *mu) {
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_handle_phase_2_abort() called.\n");
    
    /* Close the uploaded file, if needed. */
    if (kcd_close_phase_2_file_if_needed(mu, 1)) return -1;
        
    /* Pass to the next file. */
    mu->upload_index++;
    
    return 0;
}

/* This function handles a command message in phase 2. */
static int kcd_kfs_handle_phase_2_msg(struct kcd_kfs_mode_upload *mu) {
    int error = 0;
    uint32_t i, nb_sub, sub_type, nb_sub_el;
    kbuffer *in_buf = &mu->tms->in_msg->payload;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_handle_phase_2_msg() called.\n");
    
    /* Read the number of submessages. */
    if (anp_read_uint32(in_buf, &nb_sub)) return -2;
    
    /* Handle the submessages. */
    for (i = 0; i < nb_sub; i++) {
        
        /* If all files have been uploaded, complain. */
        if (mu->upload_index == mu->nb_upload) {
            kmod_set_error("too many submessages");
            return -2;
        }
        
        /* Get the number of elements in the submessage and its type. */
        if (anp_read_uint32(in_buf, &nb_sub_el) ||
            anp_read_uint32(in_buf, &sub_type)) {
            return -2;
        }
        
        /* Handle the submessage. */
        if (sub_type == KANP_KFS_SUBMESSAGE_CHUNK) {
            error = kcd_kfs_handle_phase_2_chunk(mu);
            if (error) return error;
        }
        
        else if (sub_type == KANP_KFS_SUBMESSAGE_COMMIT) {
            error = kcd_kfs_handle_phase_2_commit(mu);
            if (error) return error;
        }
        
        else if (sub_type == KANP_KFS_SUBMESSAGE_ABORT) {
            error = kcd_kfs_handle_phase_2_abort(mu);
            if (error) return error;
        }
            
        else {
            kmod_set_error("unexpected submessage type %u", sub_type);
            return -2;
        }
    }
    
    /* Send the result. */
    kcd_ticket_mode_new_out_msg(mu->tms, KANP_RES_OK);
    return kcd_ticket_mode_send_msg(mu->tms);
}

/* This function handles upload phase 2. */
static int kcd_kfs_handle_phase_2(struct kcd_kfs_mode_upload *mu) {
    int error = 0;
    struct kcd_ticket_mode_state *tms = mu->tms;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_handle_phase_2() called.\n");
    
    do {
        /* Receive ANP messages until we've received all the data. */
        while (mu->upload_index != mu->nb_upload) {
        
            /* Refresh the total file size and quota of the workspace. */
            error = kcd_kfs_get_kws_total_file_size_and_quota(tms, &mu->kws_total_size, &mu->kws_quota);
            if (error) break;
        
            /* Refresh the resource and license information and validate. */
            error = kcd_ticket_mode_get_usage_and_license_info(tms);
            if (error) break;
            
            /* Try to receive the next message. */
            error = kcd_ticket_mode_timed_recv_msg(tms, 60*1000);
            if (error) break;
            
            /* We got the next ANP message. Process it. */
            if (tms->in_msg) {
                error = kcd_kfs_handle_phase_2_msg(mu);
                if (error) break;
            }
        
            /* Refresh the upload entry. */
            error = kcd_kfs_refresh_phase_2_upload(mu);
            if (error) break;
        }
        
        if (error) break;
        
    } while (0);
    
    /* Delete the file currently being uploaded, if required. */
    kcd_close_phase_2_file_if_needed(mu, 1);
    
    return error;
}

/* Delete the files permanently, if possible. */
static void kcd_kfs_delete_files_permanently(struct kcd_kfs_mode_upload *mu) {
    uint32_t i;
    kstr full_path;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_delete_files_permanently() called.\n");
    
    kstr_init(&full_path);
    
    /* Delete the files permanently, if possible. We ignore errors. */
    for (i = 0; i < mu->nb_perm_delete; i++) {
        kstr *path = mu->perm_delete_array.data[i];
        kstr_sf(&full_path, "%s"PRINTF_64"u/%s", global_opts.kfs_dir_path.data, mu->tms->kws_id, path->data);
        kfs_delete(full_path.data, 1);
    }
        
    kstr_clean(&full_path);
}

/* This function handles upload phase 1. */
static int kcd_kfs_handle_phase_1(struct kcd_kfs_mode_upload *mu) {
    int error = 0;
    uint32_t i;
    struct kcd_ticket_mode_state *tms = mu->tms;
    kbuffer *kbb = &tms->kws_bound_buf, *out_buf = &tms->aq.output_buf;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_handle_phase_1() called.\n");
    
    /* Call the phase 1 handler in Postgres. */
    anp_write_uint32(kbb, mu->share_id);
    error = kcd_ticket_mode_kws_bound_query(mu->tms, "upload_phase_one", ktime_now_sec(), tms->in_msg);
    if (error) return error;

    if (anp_read_uint64(out_buf, &mu->commit_id) ||
        anp_read_uint64(out_buf, &mu->public_email_id) ||
        anp_read_uint32(out_buf, &mu->nb_upload)) {
        return -1;
    }
    
    for (i = 0; i < mu->nb_upload; i++) {
        struct kcd_kfs_uploaded_file *f = kcd_kfs_uploaded_file_new();
        karray_push(&mu->upload_array, f);
    
        if (anp_read_uint32(out_buf, &f->create_flag) ||
            anp_read_uint64(out_buf, &f->inode) ||
            anp_read_kstr(out_buf, &f->share_path) ||
            anp_read_kstr(out_buf, &f->perm_path)) {
            return -1;
        }
    }
    
    if (anp_read_uint32(out_buf, &mu->nb_perm_delete)) return -1;
    
    for (i = 0; i < mu->nb_perm_delete; i++) {
        kstr *path = kstr_new();
        karray_push(&mu->perm_delete_array, path);
        if (anp_read_kstr(out_buf, path)) return -1;
    }
    
    /* Remember whether the phase 2 is active. */
    mu->phase_2_active = (mu->nb_upload > 0);
    
    /* Delete the files permanently, if possible. */
    kcd_kfs_delete_files_permanently(mu);

    /* Send the phase 1 result to the client. */
    return kcd_ticket_mode_send_msg(tms);
}

/* Perform upload phases 1 and 2, as required. */
static int kcd_kfs_handle_upload_phases(struct kcd_kfs_mode_upload *mu) {
    int error;

    /* Handle upload phase 1. */
    error = kcd_kfs_handle_phase_1(mu);
    if (error) return error;

    /* Handle upload phase 2. */
    if (mu->phase_2_active) {
        error = kcd_kfs_handle_phase_2(mu);
        if (error) return error;
    }
    
    return 0;
}

/* Helper function for kcd_kfs_handle_upload(). The function sends an error
 * result if required and updates user_error_flag.
 */
static int kcd_kfs_handle_upload_error(struct kcd_ticket_mode_state *tms, int r, int *user_error_flag) {
    
    /* No error. */
    if (r == 0) return 0;
    
    /* Internal error. */
    if (r == -1) return -1;
    
    /* New communication error. */
    if (r == -4) {
        kmod_log_msg(KCD_LOG_BRIEF, "KFS upload error: %s.\n", kmod_strerror());
        *user_error_flag = 1;
    }
    
    /* Pending communication error. */
    if (*user_error_flag) return 0;
    
    /* Remember we have a user error and send the error message. */
    *user_error_flag = 1;
    return kcd_ticket_mode_handle_user_error(tms, r) == -1 ? -1 : 0;
}

/* This function handles a file upload. */
int kcd_kfs_handle_upload(struct kcd_ticket_mode_state *tms) {
    int error = 0, user_error_flag = 0;
    struct kcd_kfs_mode_upload mu;
    
    kcd_kfs_mode_upload_init(&mu, tms);
    
    do {
        /* Get the extra information from the ticket. */
        error = kcd_kfs_get_share_id_from_ticket(tms, &mu.share_id);
        if (error) break;
        
        /* Perform upload phases 1 and 2, as required. */
        error = kcd_kfs_handle_upload_error(tms, kcd_kfs_handle_upload_phases(&mu), &user_error_flag);
        if (error) break;
        
        /* Post the phase 2 event if the phase 2 is active. */
        if (mu.phase_2_active) {
            error = kcd_kfs_handle_upload_error(tms, kcd_kfs_post_phase_2_event(&mu), &user_error_flag);
            if (error) break;
        }
        
        /* Handle user errors. */
        if (user_error_flag) {
            kmod_set_error("closed client connection after handling upload error");
            error = -4;
            break;
        }
        
        /* Send the phase 2 result. */
        if (mu.phase_2_active) {
            kcd_ticket_mode_new_out_msg(tms, KANP_RES_OK);
            error = kcd_ticket_mode_send_msg(tms);
            if (error) break;
        }
	
    } while (0);
    
    kcd_kfs_mode_upload_clean(&mu);
    
    return error;
}


/* This function passes to the next file to download. */
static void kcd_kfs_download_next_file(struct kcd_kfs_mode_download *md) {
    assert(md->download_index < md->nb_download);
    md->download_index++;
    kfs_fclose(&md->downloaded_file, 1);
}

/* Send the next message to the user. */
static int kcd_kfs_download_send_msg(struct kcd_kfs_mode_download *md) {
    int error = 0;
    uint32_t nb_sub = 0;
    kbuffer payload, data_buf, *buf;
    kstr full_path;
    struct kcd_ticket_mode_state *tms = md->tms;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_download_send_msg() called.\n");
    
    kbuffer_init(&payload);
    kbuffer_init(&data_buf);
    kstr_init(&full_path);
    
    do {
        /* Loop until the message is full or we run out of files to download. */
        while (payload.len < MAX_DOWNLOAD_SIZE && md->download_index != md->nb_download) {

            /* The current file is closed. */
            if (md->downloaded_file == NULL) {
                uint64_t size;
                uint64_t *inode = md->download_inode_array.data[md->download_index];
                uint64_t *offset = md->download_offset_array.data[md->download_index];
                kstr *path = md->download_path_array.data[md->download_index];
                kstr_sf(&full_path, "%s"PRINTF_64"u/%s", global_opts.kfs_dir_path.data, tms->kws_id, path->data);
                
                error = kfs_fopen(&md->downloaded_file, full_path.data, "rb");
                if (error) break;

                /* Get the file size. */
                error = kfs_fsize(md->downloaded_file, &size);
                if (error) break;
                md->downloaded_size = size;
                
                /* If the offset is bigger than the file size, complain. */
                if (*offset > md->downloaded_size) {
                    kmod_set_error("offset "PRINTF_64"u is bigger than file size "PRINTF_64"u for inode "PRINTF_64"u",
                                   *offset, md->downloaded_size, *inode);
                    error = -2;
                    break;
                }

                /* Compute the remaining size. */
                md->remaining_size = md->downloaded_size - *offset;

                /* Add the 'file' submessage. */
                nb_sub++;
                anp_write_uint32(&payload, 4);
                anp_write_uint32(&payload, KANP_KFS_SUBMESSAGE_FILE);
                anp_write_uint64(&payload, md->downloaded_size);
                anp_write_uint64(&payload, md->remaining_size);

                /* The remaining size is non-zero, there will be a chunk. */
                if (md->remaining_size) {

                    /* Seek at the specified offset. */
                    error = kfs_fseek(md->downloaded_file, *offset, SEEK_SET);
                    if (error) break;
                }

                /* Pass to the next file. */
                else {
                    kcd_kfs_download_next_file(md);
                }
            }

            /* The current file is open. */
            else {
                uint64_t chunk_size;

                /* Compute the chunk size. */
                assert(md->remaining_size);
                chunk_size = MAX(MIN_DOWNLOAD_CHUNK_SIZE, MAX_DOWNLOAD_SIZE - (int32_t) payload.len);
                chunk_size = MIN(chunk_size, md->remaining_size);
                md->remaining_size -= chunk_size;

                /* Read the chunk. */
                kbuffer_reset(&data_buf);
                error = kfs_fread(md->downloaded_file, kbuffer_write_nbytes(&data_buf, chunk_size), chunk_size);
                if (error) break;

                /* Add the 'chunk' submessage. */
                nb_sub++;
                anp_write_uint32(&payload, 3);
                anp_write_uint32(&payload, KANP_KFS_SUBMESSAGE_CHUNK);
                anp_write_bin(&payload, &data_buf);

                /* Pass to the next file. */
                if (!md->remaining_size) {
                    kcd_kfs_download_next_file(md);
                }
            }
        }
        
        if (error) break;
    
        /* Create and send the message. */
        buf = kcd_ticket_mode_new_out_msg(tms, KANP_RES_KFS_DOWNLOAD_DATA);
        anp_write_uint32(buf, nb_sub);
        kbuffer_write_buffer(buf, &payload);
        error = kcd_ticket_mode_send_msg(tms);
        if (error) break;
        
    } while (0);
    
    kbuffer_clean(&payload);
    kbuffer_clean(&data_buf);
    kstr_clean(&full_path);
    
    return error;
}

/* Retrieve the permanent path to the files specified by the user. */
static int kcd_kfs_download_get_path(struct kcd_kfs_mode_download *md) {
    int error = 0;
    uint32_t i;
    struct kcd_ticket_mode_state *tms = md->tms;
    kbuffer *kbb = &tms->kws_bound_buf, *in_buf = &tms->in_msg->payload, *out_buf = &tms->aq.output_buf;
    
    kmod_log_msg(KCD_LOG_KFS, "kcd_kfs_download_get_path() called.\n");
    
    /* Get the list of files to download. */
    if (anp_read_uint32(in_buf, &md->nb_download)) return -2;
    
    for (i = 0; i < md->nb_download; i++) {
        uint64_t *inode = kmalloc(8), *offset = kmalloc(8), *commit_id = kmalloc(8);
        karray_push(&md->download_inode_array, inode);
        karray_push(&md->download_offset_array, offset);
        karray_push(&md->download_commit_array, commit_id);
        
        if (anp_read_uint64(in_buf, inode) ||
            anp_read_uint64(in_buf, offset) ||
            anp_read_uint64(in_buf, commit_id)) {
            return -2;
        }
    }
    
    if (!md->nb_download) {
        kmod_set_error("the number of files to download is 0");
        return -2;
    }
    
    /* Get the permanent paths from Postgres. */
    anp_write_uint32(kbb, md->share_id);
    anp_write_uint32(kbb, md->nb_download);
    
    for (i = 0; i < md->nb_download; i++) {
        uint64_t *inode = md->download_inode_array.data[i];
        uint64_t *commit_id = md->download_commit_array.data[i];
        anp_write_uint64(kbb, *inode);
        anp_write_uint64(kbb, *commit_id);
    }
    
    error = kcd_ticket_mode_kws_bound_query(tms, "download_file", ktime_now_sec(), NULL);
    if (error) return error;

    for (i = 0; i < md->nb_download; i++) {
        kstr *path = kstr_new();
        karray_push(&md->download_path_array, path);
        error = anp_read_kstr(out_buf, path);
        if (error) return error;
    }
        
    return 0;
}

/* This function handles a file download. */
int kcd_kfs_handle_download(struct kcd_ticket_mode_state *tms) {
    int error = 0;
    struct kcd_kfs_mode_download md;
    
    kcd_kfs_mode_download_init(&md, tms);
    
    do {
        /* Get the extra information from the ticket. */
        error = kcd_kfs_get_share_id_from_ticket(tms, &md.share_id);
        if (error) break;
        
        /* Retrieve the file paths. */
        error = kcd_kfs_download_get_path(&md);
        if (error) break;
        
        /* Send the files. */
        while (md.download_index != md.nb_download) {
            error = kcd_kfs_download_send_msg(&md);
            if (error) break;
        }
        
        if (error) break;
    
    } while (0);
    
    kcd_kfs_mode_download_clean(&md);
    
    return error;
}

