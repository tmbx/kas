/* Copyright (C) 2008-2012 Opersys inc., All rights reserved. */

/* Note about SPI programming:
 * 
 * The SPI is painful to program with because the SPI functions can longjmp()
 * whenever they want. Although we can catch these errors, it is preferable not
 * to do so, for three reasons:
 * 
 * Firstly, if we PG_CATCH() an error and don't PG_RE_THROW() it immediately, we
 * lose the ability to report the error to Postgres.
 * 
 * Secondly, since there's no real documentation about the proper way to
 * continue the execution after an error has been caught and discarded, we could
 * screw up the memory context of Postgres inadvertently. We're navigating in
 * the murky waters of the Postgres internals and I don't trust them the least
 * bit.
 * 
 * Thirdly, the documentation says the error handling mechanisms are "still in
 * flux". Hence they are liable to change under our feet in the future. The
 * least we rely on them, the better.
 * 
 * The consequence is that we must not store pointers in local variables. All
 * pointers must be stored in a structure to be able to free them after a call
 * to longjmp().
 * 
 * Note about elog():
 * 
 * elog(ERROR, ...) invokes longjmp(). This can be used to report errors to
 * Postgres.
 * 
 * elog(NOTICE, ...) reports a string to the client. This can be used for
 * debugging.
 * 
 * Note that the code below relies heavily on macros to work around the
 * longjmp() situation.
 *
 * All the queries (public functions) we support use the following conventions:
 * - The KCDPG_QUERY_* macros are used to declare the state used by the query
 *   and to execute the query.
 * - The 'arg' buffer is used to retrieve the arguments of the query.
 * - The 'ret' buffer is used to return the output of the query.
 * - The 'ext' buffer is used to store extra data to return to the caller.
 *   The usage varies according to the type of the query being performed.
 * - The 'arg', 'ret' and 'ext' buffers use ANP elements to marshall data.
 * - If the caller provides invalid arguments or if an internal error occurs,
 *   elog(ERROR) is called. This aborts the query.
 * - Otherwise, at the end of the function, the error code is checked.
 * - If the error code is 0, the number 0 is written in the 'ret' buffer.
 *   More data may be added after this, such as the content of the 'ext' buffer.
 * - Else if the error code is -1, the number 1 and the current error string are
 *   written in the 'ret' buffer. This is used as a catch-all error handler.
 * - Else, other query-specific processing is performed.
 * - Queries that cannot fail with the error code -1 are called 'safe' queries.
 *
 * Some queries use the "workspace-bound" convention:
 * - This mode is used to perform a query on the behalf of a user who is logged
 *   in a workspace.
 * - A set of functions is provided to retrieve the standard arguments passed to
 *   the query, to validate the context of the query and to handle the results.
 * - This mode assumes that there is an ANP command result to return to the user.
 *   If this is not actually the case, an empty result can be returned.
 *   The 'res_type' field and the 'res' buffer are provided for this purpose.
 * - The error code -1 is handled using the convention described above.
 * - If the error code is not -1, the number 0, the 'res_type' field and the
 *   'res' buffer are written in the 'ret' buffer. Then:
 *   - If the error code is 0:
 *     - The query succeeded.
 *     - The number 0 and the 'extra' buffer are added to the 'ret' buffer.
 *   - Else:
 *     - It is assumed that the error code is -2. This code indicates that an
 *       ANP failure result must be returned to the user and the caller must
 *       interrupt its processing.
 *     - The number 1 is added to the 'ret' buffer.
 */

#include <dlfcn.h>
#include "common.h"
#include "postgres.h"
#include "fmgr.h"
#include "executor/spi.h"

/* Postgres magic symbol used to detect out-of-sync versions. */
#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/* This macro can be used to output debug statements. Nothing is printed if
 * debugging is disabled.
 */
#define KCDPG_DEBUG(...) do { if (kcdpg_debug_flag) kcdpg_debug_print(__LINE__, __VA_ARGS__); } while (0)

/* Purge stale uploader delay: 30 minutes. */
#define KCDPG_PURGE_UPLOADER_DELAY 30*60

/* List of flags associated to the root user. */
#define KCDPG_ROOT_USER_FLAGS (KANP_USER_FLAG_ROOT | KANP_USER_FLAG_ADMIN | KANP_USER_FLAG_MANAGER |\
                               KANP_USER_FLAG_REGISTER)

/* This macro outputs the code to declare the beginning of the query state
 * structure.
 */
#define KCDPG_QUERY_STRUCT(NAME) \
    struct kcdpg_##NAME##_state {\
        /* Buffer containing the arguments of the query. */\
        kbuffer arg_buf;\
        /* Buffer containing the output of the query. */\
        kbuffer ret_buf;\
        /* Buffer containing the ANP elements to return to the caller. */\
        kbuffer ext_buf;\
        /* Buffer containing the payload of the event to post in the event log. */\
        kbuffer evt_buf;\
        /* Buffer containing the payload of the notification to post in the\
         * notification log.\
         */\
        kbuffer ntf_buf;\
        /* Temporary buffer. */\
        kbuffer tb;\
        /* Temporary strings. */\
        kstr ts, ts2, ts3;\
        /* Pointer to the workspace-bound state, if any. */\
        struct kcdpg_kws_bound_state *wb;

/* This macro outputs the code to perform the beginning of the query state
 * initialization.
 */
#define KCDPG_QUERY_INIT(NAME, KWS_BOUND) \
    };\
    static void kcdpg_##NAME##_state_init(struct kcdpg_##NAME##_state *self) {\
        memset(self, 0, sizeof(struct kcdpg_##NAME##_state));\
        kbuffer_init(&self->arg_buf);\
        kbuffer_init(&self->ret_buf);\
        kbuffer_init(&self->ext_buf);\
        kbuffer_init(&self->evt_buf);\
        kbuffer_init(&self->ntf_buf);\
        kbuffer_init(&self->tb);\
        kstr_init(&self->ts); kstr_init(&self->ts2); kstr_init(&self->ts3);\
        if (KWS_BOUND) self->wb = kcdpg_kws_bound_state_new();

/* This macro outputs the code to peform the beginning of the query state
 * finalization.
 */
#define KCDPG_QUERY_CLEAN(NAME) \
    }\
    static void kcdpg_##NAME##_state_clean(struct kcdpg_##NAME##_state *self) {\
        kbuffer_clean(&self->arg_buf);\
        kbuffer_clean(&self->ret_buf);\
        kbuffer_clean(&self->ext_buf);\
        kbuffer_clean(&self->evt_buf);\
        kbuffer_clean(&self->ntf_buf);\
        kbuffer_clean(&self->tb);\
        kstr_clean(&self->ts); kstr_clean(&self->ts2); kstr_clean(&self->ts3);\
        kcdpg_kws_bound_state_destroy(self->wb);

/* This macro outputs the code to declare some static functions used by the
 * query. Do not put the final brace in the last function.
 */
#define KCDPG_QUERY_STATIC }

/* This macro outputs the beginning of a function that handles a query. */
#define KCDPG_QUERY_START(NAME) \
    }\
    PG_FUNCTION_INFO_V1(kcdpg_##NAME);\
    Datum kcdpg_##NAME(PG_FUNCTION_ARGS) {\
        struct kcdpg_##NAME##_state st;\
        bytea *m_arg, *m_ret;\
        \
        /* Initialize the state used by the handler. */\
        kcdpg_##NAME##_state_init(&st);\
        \
        /* Write the input data. */\
        m_arg = PG_GETARG_BYTEA_P(0);\
        kbuffer_write(&st.arg_buf, VARDATA(m_arg), VARSIZE(m_arg) - VARHDRSZ);\
        \
        /* Call the internal handler. */\
        PG_TRY();\
        {\
            int error = 0;\
            kstr *ts = &st.ts, *ts2 = &st.ts2, *ts3 = &st.ts3;\
            struct kcdpg_kws_bound_state *wb = st.wb;\
            \
            KCDPG_DEBUG(#NAME" called()");\
            \
            if (SPI_connect() < 0) elog(ERROR, "cannot connect to SPI");\
            \
            /* Read the standard workspace-bound arguments. */\
            if (wb) {\
                if (anp_read_uint64(&st.arg_buf, &wb->kws_id) ||\
                    anp_read_uint64(&st.arg_buf, &wb->date) ||\
                    anp_read_uint32(&st.arg_buf, &wb->login_type) ||\
                    anp_read_uint32(&st.arg_buf, &wb->user_id) ||\
                    anp_read_uint32(&st.arg_buf, &wb->cmd_minor) ||\
                    anp_read_bin(&st.arg_buf, &wb->cmd_buf)) {\
                    elog(ERROR, "bad workspace-bound argument: %s", kmod_strerror());\
                }\
            }\
            \
            do {
            
/* This macro outputs the end of a function that handles a query. */
#define KCDPG_QUERY_END(NAME) \
            } while (0); \
            \
            ts = ts2 = ts3 = NULL;\
            \
            if (error == -1) {\
                anp_write_uint32(&st.ret_buf, 1);\
                anp_write_kstr(&st.ret_buf, kmod_kstrerror());\
            }\
            \
            else {\
                anp_write_uint32(&st.ret_buf, 0);\
                if (wb) {\
                    anp_write_uint32(&st.ret_buf, wb->res_type);\
                    anp_write_bin(&st.ret_buf, &wb->res_buf);\
                    anp_write_uint32(&st.ret_buf, error ? 1 : 0);\
                }\
                if (error == 0) kbuffer_write_buffer(&st.ret_buf, &st.ext_buf);\
            }\
            SPI_finish();\
        }\
        /* Clean up our state and rethrow. */\
        PG_CATCH();\
        {\
            kcdpg_##NAME##_state_clean(&st);\
            PG_RE_THROW();\
        }\
        PG_END_TRY();\
        \
        /* Create the reply, clean our state and return the reply. */\
        m_ret = (bytea *) palloc(VARHDRSZ + st.ret_buf.len);\
        SET_VARSIZE(m_ret, VARHDRSZ + st.ret_buf.len);\
        memcpy(VARDATA(m_ret), st.ret_buf.data, st.ret_buf.len);\
        kcdpg_##NAME##_state_clean(&st);\
        PG_RETURN_BYTEA_P(m_ret);\
    }

/* Handler code template. Copy and paste as needed.
KCDPG_QUERY_STRUCT(name)

KCDPG_QUERY_INIT(name, 1)

KCDPG_QUERY_CLEAN(name)

KCDPG_QUERY_START(name)
    
    if (anp_read_uint64(&st.arg_buf, &st.kws_id)) {
        elog(ERROR, "bad name argument: %s", kmod_strerror());
    }
    
KCDPG_QUERY_END(name)
*/

/* State used by a workspace-bound query. */
struct kcdpg_kws_bound_state {

    /* Workspace ID. */
    uint64_t kws_id;

    /* Query date. */
    uint64_t date;

    /* Flags associated to the workspace. */
    uint32_t kws_flags;

    /* ID of the user doing the query. */
    uint32_t user_id;

    /* Login type of the user doing the query. */
    uint32_t login_type;

    /* Flags of the user doing the query in the workspace. */
    uint32_t user_flags;

    /* Minor version of the command sent by the client. */
    uint32_t cmd_minor;

    /* Type of the result to send to the client. */
    uint32_t res_type;

    /* Buffer containing the command payload. */
    kbuffer cmd_buf;

    /* Buffer containing the payload of the result to send to the client. */
    kbuffer res_buf;
};

/* This structure is used to format a workspace property change event. */
struct kcdpg_kws_prop_change_state {
    
    /* Workspace ID. */
    uint64_t kws_id;
    
    /* Event user ID. */
    uint32_t user_id;
    
    /* Number of changes. */
    uint32_t nb_change;
    
    /* Temporary strings. */
    kstr ts, ts2;
    
    /* Buffer containing the changes. */
    kbuffer change_buf;
    
    /* Buffer containing the event. */
    kbuffer evt_buf;
};

/* True if debugging is enabled. */
static int kcdpg_debug_flag = 0;

/* This structure contains pointers to the functions we import from libpq. */
struct {
     void *handle;
     unsigned char *(*PQescapeBytea)(const unsigned char *from, size_t from_length, size_t *to_length);
     unsigned char *(*PQunescapeBytea)(const unsigned char *strtext, size_t *retbuflen);
     size_t (*PQescapeString)(char *to, const char *from, size_t length);
     void (*PQfreemem)(void *ptr);
} kcdpg_libpq;

static struct kcdpg_kws_bound_state* kcdpg_kws_bound_state_new() {
    struct kcdpg_kws_bound_state *self = kcalloc(sizeof(struct kcdpg_kws_bound_state));
    self->res_type = KANP_RES_OK;
    kbuffer_init(&self->cmd_buf);
    kbuffer_init(&self->res_buf);
    return self;
}

static void kcdpg_kws_bound_state_destroy(struct kcdpg_kws_bound_state *self) {
    if (self) {
        kbuffer_clean(&self->cmd_buf);
        kbuffer_clean(&self->res_buf);
        kfree(self);
    }
}

/* This funtion prints a debug statement in the Postgres logs. */
static void kcdpg_debug_print(int line, char *format, ...) {
    kstr s, t;
    va_list args;
    va_start(args, format);
    kstr_init_sfv(&s, format, args);
    kstr_init_sf(&t, "DEBUG libkcdpg line %d: %s", line, s.data);
    elog(NOTICE, "%s", t.data);
    kstr_clean(&s);
    kstr_clean(&t);
    va_end(args);
}

/* Helper for kcdpg_libpq_init(). */
static void* kcdpg_libpq_load_sym(char *name) {
    void *val = dlsym(kcdpg_libpq.handle, name);
    if (!val) { elog(ERROR, "cannot load %s from libpq: %s", name, dlerror()); }
    return val;
}

/* This function loads the functions from libpq. */
static void kcdpg_libpq_init() {
     kcdpg_libpq.handle = dlopen("/usr/lib/libpq.so.5", RTLD_LAZY | RTLD_LOCAL);
     if (!kcdpg_libpq.handle) { elog(ERROR, "cannot load libpq: %s", dlerror()); }
     
     kcdpg_libpq.PQescapeBytea = kcdpg_libpq_load_sym("PQescapeBytea");
     kcdpg_libpq.PQunescapeBytea = kcdpg_libpq_load_sym("PQunescapeBytea");
     kcdpg_libpq.PQescapeString = kcdpg_libpq_load_sym("PQescapeString");
     kcdpg_libpq.PQfreemem = kcdpg_libpq_load_sym("PQfreemem");
}

/* This function unloads the functions from libpq. */
static void kcdpg_libpq_clean() {
     if (kcdpg_libpq.handle) dlclose(kcdpg_libpq.handle);
}
    
/* This function is called automatically by Postgres when this shared library is
 * loaded.
 */
void _PG_init() {
    ktools_initialize();
    kmod_base_init();
    kcdpg_libpq_init();
    
    /* Enable debugging if required. */
    if (kfs_regular("/etc/teambox/pg_debug")) kcdpg_debug_flag = 1;
    KCDPG_DEBUG("libkcdpg.so loaded");
}

/* This function is called automatically by Postgres when this shared library is
 * unloaded.
 */
void _PG_fini() {
    KCDPG_DEBUG("libkcdpg.so unloading");
    kcdpg_libpq_clean();
    kmod_base_clean();
    ktools_finalize();
}

/* Return a string containing the value in the row and column specified. */
static char *kcdpg_row_val(int row, int col) {
    char *val = SPI_getvalue(SPI_tuptable->vals[row], SPI_tuptable->tupdesc, col + 1);
    return val == NULL ? "" : val;
}

/* This function adds a uint32 value to a query string. */
static void kcdpg_add_uint32(kstr *to, uint32_t value) {
    char buf[100];
    sprintf(buf, "%u", value);
    kstr_append_cstr(to, buf);
}

/* This function adds a uint64 value to a query string. */
static void kcdpg_add_uint64(kstr *to, uint64_t value) {
    char buf[100];
    sprintf(buf, PRINTF_64"u", value);
    kstr_append_cstr(to, buf);
}

/* This function escapes and adds a string to a query string. */
static void kcdpg_add_str(kstr *to, kstr *from) {
     kstr tmp;
     kstr_init(&tmp);

     kstr_grow(&tmp, from->slen * 2 + 1);
     tmp.slen = kcdpg_libpq.PQescapeString(tmp.data, from->data, from->slen);

     kstr_append_char(to, '\'');
     kstr_append_kstr(to, &tmp);
     kstr_append_char(to, '\'');

     kstr_clean(&tmp);
}

/* This function escapes and adds a binary string to a query string. */
static void kcdpg_add_bytea(kstr *to, kbuffer *from) {
     size_t tmp_len;
     unsigned char *tmp = kcdpg_libpq.PQescapeBytea(from->data, from->len, &tmp_len);
     if (!tmp) { elog(ERROR, "out of memory"); }
     kstr_append_cstr(to, "E'");
     kstr_append_buf(to, tmp, tmp_len - 1);
     kstr_append_char(to, '\'');
     kcdpg_libpq.PQfreemem(tmp);
}

/* This function returns the 32 bits integer at the specified row/col. */
static uint32_t kcdpg_get_uint32(int row, int col) {
    return atoi(kcdpg_row_val(row, col));
}

/* This function returns the 64 bits integer at the specified row/col. */
static uint64_t kcdpg_get_uint64(int row, int col) {
    return atoll(kcdpg_row_val(row, col));
}

/* This function fetches the string at the specified row/col. */
static void kcdpg_get_str(int row, int col, kstr *to) {
    kstr_assign_cstr(to, kcdpg_row_val(row, col));
}

/* This function fetches the binary string at the specified row/col. */
static void kcdpg_get_bytea(int row, int col, kbuffer *to) {
    size_t tmp_len;
    char *tmp = kcdpg_libpq.PQunescapeBytea(kcdpg_row_val(row, col), &tmp_len);
    if (!tmp) { elog(ERROR, "cannot unescape binary string"); }
    kbuffer_reset(to);
    kbuffer_write(to, tmp, tmp_len);
    kcdpg_libpq.PQfreemem(tmp);
}

/* This function executes the SQL query specified. If the query fails, an
 * exception is thrown.
 */
static void kcdpg_exec_query(char *query) {
    int res = SPI_execute(query, 0, 0);
    if (res <= 0) elog(ERROR, "query '%s' failed", query);
}

/* Shortcut functions to query workspace and user properties. */
static int kcdpg_is_kws_public(uint32_t kws_flags) { return (kws_flags & KANP_KWS_FLAG_PUBLIC) > 0; }
static int kcdpg_is_kws_secure(uint32_t kws_flags) { return (kws_flags & KANP_KWS_FLAG_SECURE) > 0; }
static int kcdpg_is_kws_frozen(uint32_t kws_flags) { return (kws_flags & KANP_KWS_FLAG_FREEZE) > 0; }
static int kcdpg_is_kws_frozen_deep(uint32_t kws_flags) { return (kws_flags & KANP_KWS_FLAG_DEEP_FREEZE) > 0; }
static int kcdpg_is_kws_deleted(uint32_t kws_flags) { return (kws_flags & KANP_KWS_FLAG_DELETE) > 0; }
static int kcdpg_is_kws_compatv2(uint32_t kws_flags) { return (kws_flags & KANP_KWS_FLAG_COMPAT_V2) > 0; }
static int kcdpg_is_kws_thin_kfs(uint32_t kws_flags) { return (kws_flags & KANP_KWS_FLAG_THIN_KFS) > 0; }
static int kcdpg_is_login_priv(uint32_t login_type) { return (login_type == KCD_KWS_LOGIN_TYPE_ROOT ||
                                                              login_type == KCD_KWS_LOGIN_TYPE_KWMO); }
static int kcdpg_is_user_root(uint32_t user_flags) { return (user_flags & KANP_USER_FLAG_ROOT) > 0; }
static int kcdpg_is_user_admin(uint32_t user_flags) { return (user_flags & KANP_USER_FLAG_ADMIN) > 0; }
static int kcdpg_is_user_manager(uint32_t user_flags) { return (user_flags & KANP_USER_FLAG_MANAGER) > 0; }
static int kcdpg_is_user_registered(uint32_t user_flags) { return (user_flags & KANP_USER_FLAG_REGISTER) > 0; }
static int kcdpg_is_user_locked(uint32_t user_flags) { return (user_flags & KANP_USER_FLAG_LOCK) > 0; }
static int kcdpg_is_user_banned(uint32_t user_flags) { return (user_flags & KANP_USER_FLAG_BAN) > 0; }

/* Filter the workspace flags that are unpublished to the client. */
static uint32_t kcdpg_filter_unpublished_kws_flags(uint32_t kws_flags) {
    return kws_flags & ~(KANP_KWS_FLAG_DELETE | KANP_KWS_FLAG_COMPAT_V2);
}

/* Set or clear the specified workspace flags. */
static void kcdpg_set_kws_flags(uint32_t *kws_flags, uint32_t update_flags, int set_flag) {
    if (set_flag) *kws_flags |= update_flags;
    else *kws_flags &= ~update_flags;
}

/* Call elog(ERROR) if the user flags specified are not consistent. */
static void kcdpg_check_user_flags_consistency(uint32_t flags) {
    if ((kcdpg_is_user_root(flags) && !kcdpg_is_user_admin(flags)) ||
        (kcdpg_is_user_admin(flags) && !kcdpg_is_user_manager(flags)) ||
        (kcdpg_is_user_banned(flags) && (kcdpg_is_user_manager(flags) ||
                                         kcdpg_is_user_registered(flags) ||
                                         kcdpg_is_user_locked(flags)))) {
        elog(ERROR, "inconsistent workspace user flags");
    }
}

/* Set or clear the specified flags in the user flags specified while ensuring
 * consistency. The request itself must be consistent.
 */
static void kcdpg_set_kws_user_flags(uint32_t *user_flags, uint32_t update_flags, int set_flag) {
    uint32_t set_add_flags = 0, set_rem_flags = 0, clear_add_flags = 0, clear_rem_flags = 0;
    
    kcdpg_check_user_flags_consistency(*user_flags);
    
    if (kcdpg_is_user_root(update_flags)) {
        set_add_flags |= KANP_USER_FLAG_ROOT | KANP_USER_FLAG_ADMIN | KANP_USER_FLAG_MANAGER;
        clear_rem_flags |= KANP_USER_FLAG_ROOT;
    }
    
    if (kcdpg_is_user_admin(update_flags)) {
        set_add_flags |= KANP_USER_FLAG_ADMIN | KANP_USER_FLAG_MANAGER;
        clear_rem_flags |= KANP_USER_FLAG_ROOT | KANP_USER_FLAG_ADMIN;
    }
    
    if (kcdpg_is_user_manager(update_flags)) {
        set_add_flags |= KANP_USER_FLAG_MANAGER;
        clear_rem_flags |= KANP_USER_FLAG_ROOT | KANP_USER_FLAG_ADMIN | KANP_USER_FLAG_MANAGER;
    }
    
    if (kcdpg_is_user_registered(update_flags)) {
        set_add_flags |= KANP_USER_FLAG_REGISTER;
        clear_rem_flags |= KANP_USER_FLAG_REGISTER;
    }
    
    if (kcdpg_is_user_locked(update_flags)) {
        set_add_flags |= KANP_USER_FLAG_LOCK;
        clear_rem_flags |= KANP_USER_FLAG_LOCK;
    }
    
    if (kcdpg_is_user_banned(update_flags)) {
        set_add_flags |= KANP_USER_FLAG_BAN;
        set_rem_flags |= KANP_USER_FLAG_ROOT | KANP_USER_FLAG_ADMIN | KANP_USER_FLAG_MANAGER |
                         KANP_USER_FLAG_REGISTER | KANP_USER_FLAG_LOCK;
        clear_rem_flags |= KANP_USER_FLAG_BAN;
    }
    
    if (set_flag) {
        *user_flags |= set_add_flags;
        *user_flags &= ~set_rem_flags;
    }
    
    else {
        *user_flags |= clear_add_flags;
        *user_flags &= ~clear_rem_flags;
    }
    
    kcdpg_check_user_flags_consistency(*user_flags);
}

/* Internal helper for queries that fetch a single row containing a single
 * string.
 */
static void kcdpg_get_row_str(kstr *ts, kstr *out) {
    kcdpg_exec_query(ts->data);
    if (SPI_processed) kcdpg_get_str(0, 0, out);
    else kstr_reset(out);
}

/* Internal helper for queries that fetch a single row containing a single
 * uint32.
 */
static uint32_t kcdpg_get_row_uint32(kstr *ts) {
    kcdpg_exec_query(ts->data);
    if (!SPI_processed) return 0;
    return kcdpg_get_uint32(0, 0);
}

/* Internal helper for queries that fetch a single row containing a single
 * uint64.
 */
static uint64_t kcdpg_get_row_uint64(kstr *ts) {
    kcdpg_exec_query(ts->data);
    if (!SPI_processed) return 0;
    return kcdpg_get_uint64(0, 0);
}

/* Obtain the name associated to the workspace specified, if any. */
static void kcdpg_get_kws_name(kstr *ts, uint64_t kws_id, kstr *name) {
    kstr_sf(ts, "SELECT name FROM kcd_kws_list WHERE kws_id = "PRINTF_64"u", kws_id);
    kcdpg_get_row_str(ts, name);
}

/* Obtain the flags associated to the workspace specified, if any. */
static uint32_t kcdpg_get_kws_flags(kstr *ts, uint64_t kws_id) {
    kstr_sf(ts, "SELECT flags FROM kcd_kws_list WHERE kws_id = "PRINTF_64"u", kws_id);
    return kcdpg_get_row_uint32(ts);
}

/* Obtain the name_admin property of the workspace user specified. */
static void kcdpg_get_kws_user_name_admin(kstr *ts, uint64_t kws_id, uint32_t user_id, kstr *name) {
    if (!user_id) {
        kstr_assign_cstr(name, "System Administrator");
        return;
    }
    kstr_sf(ts, "SELECT name_admin FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u AND user_id = %u", kws_id, user_id);
    kcdpg_get_row_str(ts, name);
}

/* Obtain the name_user property of the workspace user specified. */
static void kcdpg_get_kws_user_name_user(kstr *ts, uint64_t kws_id, uint32_t user_id, kstr *name) {
    if (!user_id) {
        kstr_assign_cstr(name, "System Administrator");
        return;
    }
    kstr_sf(ts, "SELECT name_user FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u AND user_id = %u", kws_id, user_id);
    kcdpg_get_row_str(ts, name);
}

/* Obtain the flags associated to the workspace user specified. */
static uint32_t kcdpg_get_kws_user_flags(kstr *ts, uint64_t kws_id, uint32_t user_id) {
    if (!user_id) return KCDPG_ROOT_USER_FLAGS;
    kstr_sf(ts, "SELECT flags FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u AND user_id = %u", kws_id, user_id);
    return kcdpg_get_row_uint32(ts);
}

/* Return the notification policy associated to the global user specified. */
static uint32_t kcdpg_get_global_user_notif_policy(kstr *ts, kstr *email) {
    kstr_sf(ts, "SELECT notif_policy FROM kcd_global_users WHERE lower(email) = lower(");
    kcdpg_add_str(ts, email);
    kstr_append_cstr(ts, ")");
    return kcdpg_get_row_uint32(ts);
}

/* Return the license associated to the global user specified. */
static void kcdpg_get_global_user_license(kstr *ts, kstr *email, kstr *license) {
    kstr_sf(ts, "SELECT license FROM kcd_global_users WHERE lower(email) = lower(");
    kcdpg_add_str(ts, email);
    kstr_append_cstr(ts, ")");
    kcdpg_get_row_str(ts, license);
}

/* Return the usage information associated to the global user specified. */
static void kcdpg_get_global_user_usage_info(kstr *ts, kstr *email, struct kcd_global_user_usage_info *info) {
    uint32_t i;
    
    kcd_global_user_usage_info_reset(info);
    
    kstr_sf(ts, "SELECT flags, file_size FROM kcd_kws_list INNER JOIN kcd_kws_kfs_limit USING (kws_id) WHERE "
                "kws_id IN (SELECT kws_id FROM kcd_kws_users WHERE user_id = 1 AND lower(email) = lower(");
    kcdpg_add_str(ts, email);
    kstr_append_cstr(ts, "))");
    kcdpg_exec_query(ts->data);
    
    for (i = 0; i < SPI_processed; i++) {
        uint32_t kws_flags = kcdpg_get_uint32(i, 0);
        uint64_t kws_file_size = kcdpg_get_uint64(i, 1);
        if (kcdpg_is_kws_public(kws_flags)) info->nb_pb_kws++;
        else info->nb_non_pb_kws++;
        info->kfs_usage += kws_file_size;
    }
}

/* Return the license information associated to the global user specified. */
static void kcdpg_get_global_user_license_info(kstr *ts, kstr *email,
                                               struct kcd_global_user_license_info *info) {
    int enforce_flag = kfs_regular("/etc/freemium");
    kstr *name = &info->license_name;
    
    /* Get the license name, if any. */
    kcdpg_get_global_user_license(ts, email, name);
    
    /* Unlimited. */
    if (!enforce_flag || kstr_equal_cstr(name, "gold")) {
        info->nb_non_pb_kws = info->nb_pb_kws = INT32_MAX;
        info->kfs_usage = info->vnc_session_time = INT64_MAX;
        info->secure_kws_flag = 1;
    }
    
    /* Freemium. */
    else if (kstr_equal_cstr(name, "freemium")) {
        info->nb_non_pb_kws = 5;
        info->nb_pb_kws = 1;
        info->kfs_usage = 1024*1024*1024;
        info->vnc_session_time = 15*60;
        info->secure_kws_flag = 0;
    }
    
    /* Something else or not found -- no rights. */
    else {
        kcd_global_user_license_info_reset(info);
    }
}


/* Return the usage and license information associated to the workspace
 * specified.
 */
static void kcdpg_get_kws_usage_and_license_info(kstr *ts, kstr *ts2, uint64_t kws_id,
                                                 struct kcd_global_user_usage_info *u,
                                                 struct kcd_global_user_license_info *l) {
    kstr_sf(ts, "SELECT email FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u AND user_id = 1", kws_id);
    kcdpg_get_row_str(ts, ts2);
    
    if (!ts2->slen) {
        kcd_global_user_usage_info_reset(u);
        kcd_global_user_license_info_reset(l);
    }
    
    else {
        kcdpg_get_global_user_usage_info(ts, ts2, u);
        kcdpg_get_global_user_license_info(ts, ts2, l);
    }
}
    
/* Return true if the ticket specified is present in the workspace login ticket
 * table.
 */
static int kcdpg_is_kws_login_ticket_stored(kstr *ts, uint64_t kws_id, uint32_t user_id, kbuffer *ticket) {
    kstr_sf(ts, "SELECT kws_id FROM kcd_kws_login_ticket WHERE kws_id = "PRINTF_64"u AND user_id = %u",
                 kws_id, user_id);
    kstr_append_cstr(ts, " AND ticket = ");
    kcdpg_add_bytea(ts, ticket);
    kcdpg_exec_query(ts->data);
    return (SPI_processed > 0);
}

/* This function returns true if the upload entry specified could be found. If
 * timestamp is non-NULL, the entry must match the timestamp.
 */
static int kcdpg_uploader_exist(kstr *ts, uint64_t kws_id, uint32_t share_id, uint32_t user_id, uint64_t commit_id,
                                uint64_t *timestamp) {
    kstr_sf(ts, "SELECT kws_id FROM kcd_kws_kfs_upload WHERE kws_id = "PRINTF_64"u AND share_id = %u AND user_id = %u "
                "AND commit_id = "PRINTF_64"u", kws_id, share_id, user_id, commit_id);
    if (timestamp) kstr_append_sf(ts, " AND timestamp = "PRINTF_64"u", *timestamp);
    kcdpg_exec_query(ts->data);
    return (SPI_processed > 0);
}

/* Create the specified lock. */
static void kcdpg_create_lock(kstr *ts, uint64_t kws_id, char *name) {
    kstr_sf(ts, "INSERT INTO kcd_locks (kws_id, name) VALUES ("PRINTF_64"u, '%s')", kws_id, name);
    kcdpg_exec_query(ts->data);
}

/* Create the specified sequence. */
static void kcdpg_create_seq(kstr *ts, uint64_t kws_id, char *name) {
    kstr_sf(ts, "INSERT INTO kcd_sequences (kws_id, name, value) VALUES ("PRINTF_64"u, '%s', 0)", kws_id, name);
    kcdpg_exec_query(ts->data);
}

/* Lock the specified lock. Return true if the lock was obtained, i.e. if the
 * target row was present in the lock table.
 */
static int kcdpg_get_lock(kstr *ts, uint64_t kws_id, char *name, int write_flag) {
    kstr_sf(ts, "SELECT NULL FROM kcd_locks WHERE kws_id = "PRINTF_64"u AND name = '%s' FOR %s",
                kws_id, name, write_flag ? "UPDATE" : "SHARE");
    kcdpg_exec_query(ts->data);
    return (SPI_processed > 0);
}

/* Shortcut functions for locks. */
static void kcdpg_lock_kws_list(kstr *ts) { kcdpg_get_lock(ts, 0, "kws_list", 1); }
static int kcdpg_lock_kws(kstr *ts, uint64_t kws_id, int write_flag)
    { return kcdpg_get_lock(ts, kws_id, "kws", write_flag); }
static void kcdpg_lock_kfs(kstr *ts, uint64_t kws_id, int write_flag) { kcdpg_get_lock(ts, kws_id, "kfs", write_flag); }
static void kcdpg_lock_evt(kstr *ts, uint64_t kws_id) { kcdpg_get_lock(ts, kws_id, "event_log", 1); }

/* Return the next ID in the specified sequence. */
static uint64_t kcdpg_get_next_seq_id(kstr *ts, uint64_t kws_id, char *name) {
    kstr_sf(ts, "UPDATE kcd_sequences SET value = value + 1 WHERE kws_id = "PRINTF_64"u AND name = '%s'", kws_id, name);
    kcdpg_exec_query(ts->data);
    kstr_sf(ts, "SELECT value FROM kcd_sequences WHERE kws_id = "PRINTF_64"u AND name = '%s'", kws_id, name);
    kcdpg_exec_query(ts->data);
    if (SPI_processed != 1) elog(ERROR, "sequence '%s' for workspace "PRINTF_64"u not found", name, kws_id);
    return kcdpg_get_uint64(0, 0);
}

/* Insert the user specified in the global user table if he is not already in
 * the table.
 */
static void kcdpg_insert_global_user(kstr *ts, kstr *email, uint32_t notif_policy) {
    kstr_sf(ts, "SELECT email FROM kcd_global_users WHERE email = ");
    kcdpg_add_str(ts, email);
    kcdpg_exec_query(ts->data);
    if (SPI_processed > 0) return;
    
    kstr_sf(ts, "INSERT INTO kcd_global_users (email, notif_policy, license) VALUES (");
    kcdpg_add_str(ts, email); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, notif_policy); kstr_append_cstr(ts, ", ");
    kstr_append_cstr(ts, "'')");
    kcdpg_exec_query(ts->data);
}

/* Set the license associated to the global user specified. */
static void kcdpg_set_global_user_license(kstr *ts, kstr *email, kstr *license) {
    kstr_sf(ts, "UPDATE kcd_global_users SET license = ");
    kcdpg_add_str(ts, license);
    kstr_append_cstr(ts, " WHERE email = ");
    kcdpg_add_str(ts, email);
    kcdpg_exec_query(ts->data);
}

/* Insert a user in the workspace user table. */
static void kcdpg_insert_kws_user(kstr *ts, uint64_t kws_id, uint32_t user_id, uint32_t flags, uint32_t notif_policy,
                                  kstr *email, kstr *name_admin, kstr *name_user, kstr *org_name, kstr *pwd) {
    kstr_sf(ts, "INSERT INTO kcd_kws_users (kws_id, user_id, flags, notif_policy, email, name_admin, "
                                           "name_user, org_name, pwd) VALUES (");
    kcdpg_add_uint64(ts, kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, user_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, flags); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, notif_policy); kstr_append_cstr(ts, ", ");
    kcdpg_add_str(ts, email); kstr_append_cstr(ts, ", ");
    kcdpg_add_str(ts, name_admin); kstr_append_cstr(ts, ", ");
    kcdpg_add_str(ts, name_user); kstr_append_cstr(ts, ", ");
    kcdpg_add_str(ts, org_name); kstr_append_cstr(ts, ", ");
    kcdpg_add_str(ts, pwd); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
}

/* Update the flags of the workspace specified. */
static void kcdpg_update_kws_flags(kstr *ts, uint64_t kws_id, uint32_t flags) {
    kstr_sf(ts, "UPDATE kcd_kws_list SET flags = %u WHERE kws_id = "PRINTF_64"u", flags, kws_id);
    kcdpg_exec_query(ts->data);
}

/* Update the name of the workspace. */
static void kcdpg_update_kws_name(kstr *ts, uint64_t kws_id, kstr *name) {
    kstr_sf(ts, "UPDATE kcd_kws_list SET name = ");
    kcdpg_add_str(ts, name);
    kstr_append_sf(ts, " WHERE kws_id = "PRINTF_64"u", kws_id);
    kcdpg_exec_query(ts->data);
}
    
/* Update the flags of the workspace user specified. */
static void kcdpg_update_kws_user_flags(kstr *ts, uint64_t kws_id, uint32_t user_id, uint32_t flags) {
    kcdpg_check_user_flags_consistency(flags);
    kstr_sf(ts, "UPDATE kcd_kws_users SET flags = %u WHERE kws_id = "PRINTF_64"u AND user_id = %u",
                 flags, kws_id, user_id);
    kcdpg_exec_query(ts->data);
}

/* Update the name_admin of the workspace user specified. */
static void kcdpg_update_kws_user_name_admin(kstr *ts, uint64_t kws_id, uint32_t user_id, kstr *name) {
    kstr_sf(ts, "UPDATE kcd_kws_users SET name_admin = ");
    kcdpg_add_str(ts, name);
    kstr_append_sf(ts, " WHERE kws_id = "PRINTF_64"u AND user_id = %u", kws_id, user_id);
    kcdpg_exec_query(ts->data);
}

/* Update the name_user of the workspace user specified. */
static void kcdpg_update_kws_user_name_user(kstr *ts, uint64_t kws_id, uint32_t user_id, kstr *name) {
    kstr_sf(ts, "UPDATE kcd_kws_users SET name_user = ");
    kcdpg_add_str(ts, name);
    kstr_append_sf(ts, " WHERE kws_id = "PRINTF_64"u AND user_id = %u", kws_id, user_id);
    kcdpg_exec_query(ts->data);
}

/* Update the password of the workspace user specified. */
static void kcdpg_update_kws_user_pwd(kstr *ts, uint64_t kws_id, uint32_t user_id, kstr *pwd) {
    kstr_sf(ts, "UPDATE kcd_kws_users SET pwd = ");
    kcdpg_add_str(ts, pwd);
    kstr_append_sf(ts, " WHERE kws_id = "PRINTF_64"u AND user_id = %u", kws_id, user_id);
    kcdpg_exec_query(ts->data);
}

/* Insert an invitation in the invitation table. */
static void kcdpg_insert_invitation(kstr *ts, kstr *email_id, uint64_t kws_id, uint32_t inviting_user_id,
                                    uint32_t user_id, uint64_t date, uint64_t key_id, kstr *pwd) {
    kstr_sf(ts, "INSERT INTO kcd_kws_user_invitation (email_id, kws_id, inviting_user_id, user_id, date, key_id, "
                                                     "pwd) VALUES (");
    kcdpg_add_str(ts, email_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, inviting_user_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, user_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, date); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, key_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_str(ts, pwd); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
}

/* Generate and insert a system email ID in the invitation table. */
static void kcdpg_insert_system_email_id(kstr *ts, kstr *ts2, kstr *email_id, uint64_t kws_id, uint32_t user_id,
                                         uint64_t date) {
    kstr_reset(ts2);
    kcd_mgt_generate_email_id(email_id);
    kcdpg_insert_invitation(ts, email_id, kws_id, 0, user_id, date, 0, ts2);
}

/* Insert the ticket specified if it is not already in the table. */
static void kcdpg_insert_login_ticket(kstr *ts, uint64_t kws_id, uint32_t user_id, kbuffer *ticket) {
    kstr_sf(ts, "SELECT kws_id FROM kcd_kws_login_ticket WHERE kws_id = "PRINTF_64"u AND user_id = %u AND ticket = ",
            kws_id, user_id);
    kcdpg_add_bytea(ts, ticket);
    kcdpg_exec_query(ts->data);
    if (SPI_processed > 0) return;
    
    kstr_sf(ts, "INSERT INTO kcd_kws_login_ticket (kws_id, user_id, ticket) VALUES (");
    kcdpg_add_uint64(ts, kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, user_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_bytea(ts, ticket); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
}

/* Insert the trusted key ID specified if it is not already in the table. */
static void kcdpg_insert_trusted_key_id(kstr *ts, uint64_t kws_id, uint64_t key_id) {
    kstr_sf(ts, "SELECT kws_id FROM kcd_kws_trusted_key WHERE kws_id = "PRINTF_64"u AND key_id = "PRINTF_64"u", 
            kws_id, key_id);
    kcdpg_exec_query(ts->data);
    if (SPI_processed > 0) return;
    
    kstr_sf(ts, "INSERT INTO kcd_kws_trusted_key (kws_id, key_id) VALUES (");
    kcdpg_add_uint64(ts, kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, key_id); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
}

/* This function removes the uploader specified. */
static void kcdpg_remove_uploader(kstr *ts, uint64_t kws_id, uint32_t share_id, uint32_t user_id, uint64_t commit_id) {
    kstr_sf(ts, "DELETE FROM kcd_kws_kfs_upload WHERE kws_id = "PRINTF_64"u AND share_id = %u AND user_id = %u "
                "AND commit_id = "PRINTF_64"u", kws_id, share_id, user_id, commit_id);
    kcdpg_exec_query(ts->data);
}

/* Notify the listeners of the event log. */
static void kcdpg_notify_evt(kstr *ts, uint64_t kws_id) { 
    kstr_sf(ts, "NOTIFY kws_"PRINTF_64"u_event_log", kws_id);
    kcdpg_exec_query(ts->data);
}

/* Notify the listeners of the perm_check relation. */
static void notify_perm_check(kstr *ts, uint64_t kws_id) { 
    kstr_sf(ts, "NOTIFY kws_"PRINTF_64"u_perm_check", kws_id);
    kcdpg_exec_query(ts->data);
}

/* This function posts an event in the event log. The event log is locked and
 * notified. The event ID is returned.
 */
static uint64_t kcdpg_post_event_internal(kstr *ts, uint64_t kws_id, uint32_t evt_minor, uint32_t evt_type,
                                          kbuffer *evt_payload) {
    uint64_t evt_id;
    
    KCDPG_DEBUG("Posting event of type %u in workspace "PRINTF_64"u.", evt_type, kws_id);
     
    /* Lock the event log. */
    kcdpg_lock_evt(ts, kws_id);
    
    /* Get the next event ID. */
    evt_id = kcdpg_get_next_seq_id(ts, kws_id, "event_log");
    
    /* Insert the event in the log. */
    kstr_sf(ts, "INSERT INTO kcd_kws_event_log (kws_id, evt_id, major, minor, type, event) VALUES (");
    kcdpg_add_uint64(ts, kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, evt_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, 0); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, evt_minor); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, evt_type); kstr_append_cstr(ts, ", ");
    kcdpg_add_bytea(ts, evt_payload); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    
    /* Notify the listeners. */
    kcdpg_notify_evt(ts, kws_id);
    
    return evt_id;
}

/* This function posts a notification in the notification log. The notification
 * ID is returned. The function assumes that the event log has been locked, and
 * has been or will be notified. This is the case when an event has been posted.
 */
static uint64_t kcdpg_post_notif(kstr *ts, uint64_t kws_id, uint64_t evt_id, uint64_t date, uint32_t user_id,
                                 uint32_t type, kbuffer *payload) {
    uint64_t notif_id;
    
    KCDPG_DEBUG("Posting notification of type %u in workspace "PRINTF_64"u.", type, kws_id);
     
    /* Get the next notification ID. */
    notif_id = kcdpg_get_next_seq_id(ts, kws_id, "notif_log");
    
    /* Insert the notification in the log. */
    kstr_sf(ts, "INSERT INTO kcd_kws_notif_log (kws_id, notif_id, evt_id, date, user_id, type, payload) "
                "VALUES (");
    kcdpg_add_uint64(ts, kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, notif_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, evt_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, date); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, user_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, type); kstr_append_cstr(ts, ", ");
    kcdpg_add_bytea(ts, payload); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    
    return notif_id;
}

/* Helper for kcdpg_perm_check_*() functions. */
static int kcdpg_internal_perm_check(int pred, char *what) {
    if (!pred) {
        kmod_set_error("%s", what);
        return -1;
    }
    
    return 0;
}

static int kcdpg_perm_check_kws_public(uint32_t kws_flags) {
    return kcdpg_internal_perm_check(kcdpg_is_kws_public(kws_flags), "not a public "KCD_KWS_NAME);
}

static int kcdpg_perm_check_user_root(uint32_t user_flags) {
    return kcdpg_internal_perm_check(kcdpg_is_user_root(user_flags), "system administrator power required");
}

static int kcdpg_perm_check_user_admin(uint32_t user_flags) {
    return kcdpg_internal_perm_check(kcdpg_is_user_admin(user_flags), "workspace administrator power required");
}

static int kcdpg_perm_check_user_manager(uint32_t user_flags) {
    return kcdpg_internal_perm_check(kcdpg_is_user_manager(user_flags), "workspace manager power required");
}

static int kcdpg_perm_check_target_not_self(uint32_t user_id, uint32_t target_user_id) {
    return kcdpg_internal_perm_check(user_id != target_user_id, "cannot apply action to yourself");
}

static int kcdpg_perm_check_target_not_root(uint32_t user_flags) {
    return kcdpg_internal_perm_check(!kcdpg_is_user_root(user_flags), "cannot apply action to system administrator");
}

static int kcdpg_perm_check_target_not_banned(uint32_t user_flags) {
    return kcdpg_internal_perm_check(!kcdpg_is_user_banned(user_flags), "cannot apply action to banned user");
}

/* Verify if the user privilege level is higher or equal to the target privilege
 * level.
 */
static int kcdpg_perm_check_priv_level(uint32_t user_flags, uint32_t target_flags) {
    if ((kcdpg_is_user_root(target_flags) && kcdpg_perm_check_user_root(user_flags)) ||
        (kcdpg_is_user_admin(target_flags) && kcdpg_perm_check_user_admin(user_flags)) ||
        (kcdpg_is_user_manager(target_flags) && kcdpg_perm_check_user_manager(user_flags))) return -1;
    return 0;
}

/* Read lock or write lock the workspace specified. Validate that the workspace
 * still exists. The corresponding login error code is obtained if requested.
 */
static int kcdpg_perm_check_lock_kws(kstr *ts, uint64_t kws_id, int write_flag, uint32_t *login_code) {
    if (!kcdpg_lock_kws(ts, kws_id, write_flag)) {
        if (kcdpg_is_kws_deleted(kcdpg_get_kws_flags(ts, kws_id))) {
            kmod_set_error("the "KCD_KWS_NAME" has been deleted from the server");
            if (login_code) *login_code = KANP_KWS_LOGIN_DELETED_KWS;
        }
        else {
            kmod_set_error("no such "KCD_KWS_NAME);
            if (login_code) *login_code = KANP_KWS_LOGIN_BAD_KWS_ID;
        }
        
        return -1;
    }
    
    return 0;
}

/* Validate that the user can perform an action in a workspace. */
static int kcdpg_perm_check_kws_not_frozen(uint32_t kws_flags, uint32_t user_flags) {
    if ((kcdpg_is_kws_frozen_deep(kws_flags) && !kcdpg_is_user_root(user_flags)) ||
        (kcdpg_is_kws_frozen(kws_flags) && !kcdpg_is_user_admin(user_flags))) {
        kmod_set_error("the workspace state has been frozen");
        return -1;
    }
    
    return 0;
}

/* Verify that the user specified exists. If 'flags' is non-null, obtain the
 * user flags.
 */
static int kcdpg_perm_check_user_exist(kstr *ts, uint64_t kws_id, uint32_t user_id, uint32_t *flags) {
    if (!user_id) {
        if (flags) *flags = KCDPG_ROOT_USER_FLAGS;
        return 0;
    }
    
    kstr_sf(ts, "SELECT flags FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u AND user_id = %u", kws_id, user_id);
    kcdpg_exec_query(ts->data);
    
    if (!SPI_processed) {
        kmod_set_error("no such user");
        return -1;
    }
    
    if (flags) *flags = kcdpg_get_uint32(0, 0);
    return 0;
}

/* Validate that the user can log in the workspace specified. The corresponding
 * login error code is obtained if requested. The login type '0' can be
 * specified if no check should be done on the login type.
 */
static int kcdpg_perm_check_kws_login(uint32_t kws_flags, uint32_t login_type, uint32_t user_flags, int *login_code) {
    if (login_type == KCD_KWS_LOGIN_TYPE_NORMAL && kcdpg_is_kws_secure(kws_flags)) {
        kmod_set_error("secure login credentials required");
        if (login_code) *login_code = KANP_KWS_LOGIN_BAD_PWD_OR_TICKET;
        return -1;
    }
    
    if (kcdpg_is_user_locked(user_flags)) {
        kmod_set_error("your workspace account has been locked");
        if (login_code) *login_code = KANP_KWS_LOGIN_ACCOUNT_LOCKED;
        return -1;
    }
    
    if (kcdpg_is_user_banned(user_flags)) {
        kmod_set_error("you have been banned from the workspace");
        if (login_code) *login_code = KANP_KWS_LOGIN_BANNED;
        return -1;
    }
    
    return 0;
}

/* Verify if the property change requested can be performed on the target user
 * specified. The target user flags are obtained.
 */
static int kcdpg_perm_check_modify_target_user_prop(kstr *ts,
                                                    uint64_t kws_id,
                                                    uint32_t invoker_user_id,
                                                    uint32_t invoker_user_flags,
                                                    uint32_t target_user_id,
                                                    uint32_t *target_user_flags,
                                                    uint32_t self_ok_flag,
                                                    uint32_t ban_ok_flag,
                                                    uint32_t priv_level) {
                                               
    if (kcdpg_perm_check_user_exist(ts, kws_id, target_user_id, target_user_flags) ||
        kcdpg_perm_check_target_not_root(*target_user_flags)) return -1;
    
    if (self_ok_flag && invoker_user_id == target_user_id) return 0;
    if (!self_ok_flag && kcdpg_perm_check_target_not_self(invoker_user_id, target_user_id)) return -1;
    
    if (!ban_ok_flag && kcdpg_perm_check_target_not_banned(*target_user_flags)) return -1;
    
    if (kcdpg_perm_check_priv_level(invoker_user_flags, *target_user_flags) ||
        kcdpg_perm_check_priv_level(invoker_user_flags, priv_level)) return -1;
    
    return 0;
}

/* Validate that the workspace specified exists, lock the workspace as
 * requested, obtain the workspace flags, verify that the user can log in if
 * requested and perform the workspace freeze check if requested.
 */
static int kcdpg_perm_check_kws_bound(kstr *ts, struct kcdpg_kws_bound_state *wb,
                                      int write_lock_kws_flag, int freeze_check_flag, int login_check_flag) {
                                
    /* Lock the workspace, if any. */
    if (kcdpg_perm_check_lock_kws(ts, wb->kws_id, write_lock_kws_flag, NULL)) return -1;
    wb->kws_flags = kcdpg_get_kws_flags(ts, wb->kws_id);
    
    /* Get the user. */
    if (kcdpg_perm_check_user_exist(ts, wb->kws_id, wb->user_id, &wb->user_flags)) return -1;
    
    /* Perform the login check. */
    if (login_check_flag && kcdpg_perm_check_kws_login(wb->kws_flags, wb->login_type, wb->user_flags, NULL)) return -1;
    
    /* Perform the freeze check. */
    if (freeze_check_flag && kcdpg_perm_check_kws_not_frozen(wb->kws_flags, wb->user_flags)) return -1;
    
    return 0;
}

/* Same as above for KFS-related queries. */
static int kcpdg_perm_check_kws_bound_kfs(kstr *ts, struct kcdpg_kws_bound_state *wb,
                                          int write_lock_kfs_flag, int freeze_check_flag, int login_check_flag) {
    if (kcdpg_perm_check_kws_bound(ts, wb, 0, freeze_check_flag, login_check_flag)) return -1;
    kcdpg_lock_kfs(ts, wb->kws_id, write_lock_kfs_flag);
    return 0;
}

/* Handle a workspace-bound failure. Set the result type to failure and return
 * the result payload.
 */
static kbuffer* kcdpg_kws_bound_failure(struct kcdpg_kws_bound_state *self) {
    self->res_type = KANP_RES_FAIL;
    return &self->res_buf;
}

/* Handle a workspace-bound permission error. This function assumes that the
 * KMOD error string has been set. The value -2 is returned for convenience.
 */
static int kcdpg_handle_kws_bound_perm_error(struct kcdpg_kws_bound_state *self) {
    self->res_type = KANP_RES_FAIL;
    kbuffer_reset(&self->res_buf);
    anp_write_uint32(&self->res_buf, KANP_RES_FAIL_PERM_DENIED);
    anp_write_kstr(&self->res_buf, kmod_kstrerror());
    return -2;
}

static struct kcdpg_kws_prop_change_state* kcdpg_kws_prop_change_state_new(uint64_t kws_id, uint32_t user_id) {
    struct kcdpg_kws_prop_change_state *self = kcalloc(sizeof(struct kcdpg_kws_prop_change_state));
    self->kws_id = kws_id;
    self->user_id = user_id;
    kstr_init(&self->ts);
    kstr_init(&self->ts2);
    kbuffer_init(&self->change_buf);
    kbuffer_init(&self->evt_buf);
    return self;
}

static void kcdpg_kws_prop_change_state_destroy(struct kcdpg_kws_prop_change_state *self) {
    if (self) {
        kstr_clean(&self->ts);
        kstr_clean(&self->ts2);
        kbuffer_clean(&self->change_buf);
        kbuffer_clean(&self->evt_buf);
        kfree(self);
    }
}

/* Register a workspace property change. */
static void kcdpg_kws_prop_change_add_prop(struct kcdpg_kws_prop_change_state *self, uint32_t prop) {
    self->nb_change++;
    anp_write_uint32(&self->change_buf, prop);
}

/* Register a workspace user property change. */
static void kcdpg_kws_prop_change_add_user_prop(struct kcdpg_kws_prop_change_state *self, uint32_t user_id,
                                                uint32_t prop) {
    kcdpg_kws_prop_change_add_prop(self, prop);
    anp_write_uint32(&self->change_buf, user_id);
}

static void kcdpg_kws_prop_change_kws_name(struct kcdpg_kws_prop_change_state *self, kstr *name) {
    kcdpg_get_kws_name(&self->ts, self->kws_id, &self->ts2);
    if (kstr_equal_kstr(name, &self->ts2)) return;
    kcdpg_update_kws_name(&self->ts, self->kws_id, name);
    kcdpg_kws_prop_change_add_prop(self, KANP_PROP_KWS_NAME);
    anp_write_kstr(&self->change_buf, name);
}

static void kcdpg_kws_prop_change_kws_flags(struct kcdpg_kws_prop_change_state *self, uint32_t flags) {
    if (kcdpg_get_kws_flags(&self->ts, self->kws_id) == flags) return;
    kcdpg_update_kws_flags(&self->ts, self->kws_id, flags);
    kcdpg_kws_prop_change_add_prop(self, KANP_PROP_KWS_FLAGS);
    anp_write_uint32(&self->change_buf, flags);
}

static void kcdpg_kws_prop_change_user_name_admin(struct kcdpg_kws_prop_change_state *self, uint32_t user_id,
                                                  kstr *name) {
    kcdpg_get_kws_user_name_admin(&self->ts, self->kws_id, user_id, &self->ts2);
    if (kstr_equal_kstr(name, &self->ts2)) return;
    kcdpg_update_kws_user_name_admin(&self->ts, self->kws_id, user_id, name);
    kcdpg_kws_prop_change_add_user_prop(self, user_id, KANP_PROP_USER_NAME_ADMIN);
    anp_write_kstr(&self->change_buf, name);
}

static void kcdpg_kws_prop_change_user_name_user(struct kcdpg_kws_prop_change_state *self, uint32_t user_id,
                                                 kstr *name) {
    kcdpg_get_kws_user_name_user(&self->ts, self->kws_id, user_id, &self->ts2);
    if (kstr_equal_kstr(name, &self->ts2)) return;
    kcdpg_update_kws_user_name_user(&self->ts, self->kws_id, user_id, name);
    kcdpg_kws_prop_change_add_user_prop(self, user_id, KANP_PROP_USER_NAME_USER);
    anp_write_kstr(&self->change_buf, name);
}

static void kcdpg_kws_prop_change_user_flags(struct kcdpg_kws_prop_change_state *self, uint32_t user_id,
                                            uint32_t flags) {
    if (kcdpg_get_kws_user_flags(&self->ts, self->kws_id, user_id) == flags) return;
    kcdpg_update_kws_user_flags(&self->ts, self->kws_id, user_id, flags);
    kcdpg_kws_prop_change_add_user_prop(self, user_id, KANP_PROP_USER_FLAGS);
    anp_write_uint32(&self->change_buf, flags);
}

/* Post the event and return the event ID, if any. */
static uint64_t kcdpg_kws_prop_change_post_event(struct kcdpg_kws_prop_change_state *self, uint64_t date) {
    if (!self->nb_change) return 0;
    anp_write_uint64(&self->evt_buf, self->kws_id);
    anp_write_uint64(&self->evt_buf, date);
    anp_write_uint32(&self->evt_buf, self->user_id);
    anp_write_uint32(&self->evt_buf, self->nb_change);
    kbuffer_write_buffer(&self->evt_buf, &self->change_buf);
    return kcdpg_post_event_internal(&self->ts, self->kws_id, 4, KANP_EVT_KWS_PROP_CHANGE, &self->evt_buf);
}


/* Create a new workspace:
 *   BIN    Command buffer.
 *   UINT32 Command minor version.
 *   STR    Organization name.
 *   UINT64 File quota.
 *   STR    KWMO host name.
 *
 * Output:
 *   UINT32 Result type.
 *   BIN    Result buffer.
 */
KCDPG_QUERY_STRUCT(cmd_mgt_create_kws)
    uint32_t cmd_minor;
    uint32_t user_id;
    uint32_t public_flag;
    uint32_t secure_flag;
    uint32_t thin_kfs_flag;
    uint32_t kws_flags;
    uint32_t notif_policy;
    uint32_t user_flags;
    uint64_t kws_id;
    uint64_t creation_date;
    uint64_t file_quota;
    uint32_t res_type;
    kbuffer cmd_buf;
    kbuffer res_buf;
    kbuffer raw_ticket;
    kbuffer nonce;
    kstr kws_name;
    kstr org_name;
    kstr email_id;
    kstr pwd;
    kstr kwmo_host;
    struct kcd_mgt_user_ticket parsed_ticket;
    struct kcd_global_user_usage_info usage_info;
    struct kcd_global_user_license_info license_info;

KCDPG_QUERY_INIT(cmd_mgt_create_kws, 0)
    kbuffer_init(&self->cmd_buf);
    kbuffer_init(&self->res_buf);
    kbuffer_init(&self->raw_ticket);
    kbuffer_init(&self->nonce);
    kstr_init(&self->kws_name);
    kstr_init(&self->org_name);
    kstr_init(&self->email_id);
    kstr_init(&self->pwd);
    kstr_init(&self->kwmo_host);
    kcd_mgt_user_ticket_init(&self->parsed_ticket);
    kcd_global_user_usage_info_init(&self->usage_info);
    kcd_global_user_license_info_init(&self->license_info);

KCDPG_QUERY_CLEAN(cmd_mgt_create_kws)
    kbuffer_clean(&self->cmd_buf);
    kbuffer_clean(&self->res_buf);
    kbuffer_clean(&self->raw_ticket);
    kbuffer_clean(&self->nonce);
    kstr_clean(&self->kws_name);
    kstr_clean(&self->org_name);
    kstr_clean(&self->email_id);
    kstr_clean(&self->pwd);
    kstr_clean(&self->kwmo_host);
    kcd_mgt_user_ticket_clean(&self->parsed_ticket);
    kcd_global_user_usage_info_clean(&self->usage_info);
    kcd_global_user_license_info_clean(&self->license_info);

KCDPG_QUERY_STATIC

/* Validate the user's right to create a workspace. Return true on success,
 * otherwise return false and handle the failure.
 */
static int kcdpg_mgt_cmd_create_kws_validate(struct kcdpg_cmd_mgt_create_kws_state *st) {
    int code = KANP_RESOURCE_QUOTA_GENERAL;
    kstr *ts = &st->ts;
    kstr *email = &st->parsed_ticket.email;
    struct kcd_global_user_usage_info *u = &st->usage_info;
    struct kcd_global_user_license_info *l = &st->license_info;
    
    kcdpg_get_global_user_usage_info(ts, email, u);
    kcdpg_get_global_user_license_info(ts, email, l);
    
    /* Break on first failure, or return if none. */
    do {
        if (st->public_flag && u->nb_pb_kws >= l->nb_pb_kws) {
            kmod_set_error("public " KCD_KWS_NAME " quota exceeded");
            break;
        }
        
        if (!st->public_flag && u->nb_non_pb_kws >= l->nb_non_pb_kws) {
            kmod_set_error(KCD_KWS_NAME " quota exceeded");
            break;
        }
        
        if (st->secure_flag && !l->secure_kws_flag) {
            code = KANP_RESOURCE_QUOTA_NO_SECURE;
            kmod_set_error("not authorized to create secure " KCD_KWS_NAME);
            break;
        }
        
        return 1;
    
    } while (0);
        
    st->res_type = KANP_RES_FAIL;
    kcd_kanp_resource_quota_failure(&st->res_buf, code);
    return 0;
}

/* Create the workspace. */
static void kcdpg_mgt_cmd_create_kws_create(struct kcdpg_cmd_mgt_create_kws_state *st) {
    uint32_t i;
    kstr *ts = &st->ts, *ts2 = &st->ts2, *ts3 = &st->ts3;
    struct kcd_mgt_user_ticket *pt = &st->parsed_ticket;
    static char *lock_list[] = { "kws", "kfs", "event_log" };
    static char *seq_list[] = { "user", "event_log", "notif_log", "kfs_commit", "kfs_inode", "vnc_session",
                                "skurl_req_id" };
    
    /* Get the workspace ID. */
    st->kws_id = kcdpg_get_next_seq_id(ts, 0, "kws_list");
    
    /* Initialize the locks and the sequences of the workspace. */
    for (i = 0; i < KUTIL_ARRAY_SIZE(lock_list); i++) kcdpg_create_lock(ts, st->kws_id, lock_list[i]);
    for (i = 0; i < KUTIL_ARRAY_SIZE(seq_list); i++) kcdpg_create_seq(ts, st->kws_id, seq_list[i]);
    
    /* Create the root KFS diretory. */
    kstr_sf(ts, "INSERT INTO kcd_kws_kfs_current_view "
                "(kws_id, share_id, inode, parent_inode, commit_id, inode_type, entry_name, email_id) "
                "VALUES ("PRINTF_64"u, 0, 0, 0, 0, 2, '', 0)", st->kws_id);
    kcdpg_exec_query(ts->data);
    
    /* Set some workspace information. */
    st->creation_date = (uint64_t) time(NULL);
    if (st->public_flag) st->kws_flags |= KANP_KWS_FLAG_PUBLIC;
    if (st->secure_flag) st->kws_flags |= KANP_KWS_FLAG_SECURE;
    if (st->thin_kfs_flag) st->kws_flags |= KANP_KWS_FLAG_THIN_KFS;
    if (st->cmd_minor <= 2) st->kws_flags |= KANP_KWS_FLAG_COMPAT_V2;
    st->user_id = kcdpg_get_next_seq_id(ts, st->kws_id, "user");
    kcd_mgt_generate_email_id(&st->email_id);
    
    /* Insert the workspace information in the workspace list-> */
    kstr_sf(ts, "INSERT INTO kcd_kws_list (kws_id, creation_date, name, flags) "
                "VALUES (");
    kcdpg_add_uint64(ts, st->kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, st->creation_date); kstr_append_cstr(ts, ", ");
    kcdpg_add_str(ts, &st->kws_name); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, st->kws_flags); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);

    /* Insert the workspace information in the KFS limit table. */
    kstr_sf(ts, "INSERT INTO kcd_kws_kfs_limit (kws_id, file_quota, file_size) "
                "VALUES ("PRINTF_64"u, "PRINTF_64"u, 0)", st->kws_id, st->file_quota);
    kcdpg_exec_query(ts->data);
    
    /* Insert the creator in the global user list-> */
    kcdpg_insert_global_user(ts, &pt->email, KANP_EMAIL_SUMMARY_FLAG);
    
    /* Insert the creator in the workspace user list-> */
    st->notif_policy = kcdpg_get_global_user_notif_policy(ts, &pt->email);
    if (st->public_flag) st->notif_policy |= KANP_EMAIL_NOTIF_FLAG;
    st->user_flags = KANP_USER_FLAG_ADMIN | KANP_USER_FLAG_MANAGER | KANP_USER_FLAG_REGISTER;
    kcdpg_insert_kws_user(ts, st->kws_id, st->user_id, st->user_flags, st->notif_policy, &pt->email, &pt->name,
                          &pt->name, &st->org_name, &st->pwd);
    
    /* Make an invitation from the creator himself. */
    kcdpg_insert_invitation(ts, &st->email_id, st->kws_id, st->user_id, st->user_id, st->creation_date, pt->key_id,
                            &st->pwd);
    
    /* Make a system invitation. */
    kcdpg_insert_system_email_id(ts, ts2, ts3, st->kws_id, st->user_id, st->creation_date);
    
    /* Insert the ticket in the ticket list-> */
    kcdpg_insert_login_ticket(ts, st->kws_id, st->user_id, &st->raw_ticket);
    
    /* Insert the key ID in the list of trusted key IDs. */
    kcdpg_insert_trusted_key_id(ts, st->kws_id, pt->key_id);
    
    /* Post the workspace created event. */
    anp_write_uint64(&st->evt_buf, st->kws_id);
    anp_write_uint64(&st->evt_buf, st->creation_date);
    anp_write_uint32(&st->evt_buf, st->user_id);
    anp_write_kstr(&st->evt_buf, &pt->name);
    anp_write_kstr(&st->evt_buf, &pt->email);
    if (st->cmd_minor <= 2) {
        anp_write_uint32(&st->evt_buf, 1);
        anp_write_uint32(&st->evt_buf, 1);
    }
    anp_write_kstr(&st->evt_buf, &st->org_name);
    if (st->cmd_minor >= 3) {
        anp_write_kstr(&st->evt_buf, &st->kws_name);
        anp_write_uint32(&st->evt_buf, kcdpg_filter_unpublished_kws_flags(st->kws_flags));
        anp_write_kstr(&st->evt_buf, &st->kwmo_host);
    }
    kcdpg_post_event_internal(ts, st->kws_id, st->cmd_minor <= 2 ? 2 : 3, KANP_EVT_KWS_CREATED, &st->evt_buf);
    
    /* Notify the listeners. */
    kcdpg_exec_query("NOTIFY kws_list");

KCDPG_QUERY_START(cmd_mgt_create_kws)
    
    /* Retrieve the arguments. */
    if (anp_read_bin(&st.arg_buf, &st.cmd_buf) ||
        anp_read_uint32(&st.arg_buf, &st.cmd_minor) ||
        anp_read_kstr(&st.arg_buf, &st.org_name) ||
        anp_read_uint64(&st.arg_buf, &st.file_quota) ||
        anp_read_kstr(&st.arg_buf, &st.kwmo_host)) {
        elog(ERROR, "bad cmd_mgt_create_kws argument: %s", kmod_strerror());
    }
    
    /* Parse the command arguments. */
    st.secure_flag = 1;
    if (anp_read_kstr(&st.cmd_buf, &st.kws_name) ||
        anp_read_bin(&st.cmd_buf, &st.raw_ticket) ||
        anp_read_uint32(&st.cmd_buf, &st.public_flag) ||
        (st.cmd_minor >= 3 && anp_read_uint32(&st.cmd_buf, &st.secure_flag)) ||
        (st.cmd_minor >= 4 && anp_read_uint32(&st.cmd_buf, &st.thin_kfs_flag))) {
        error = -2;
        break;
    }
    
    /* Parse the user ticket. */
    if (kcd_mgt_parse_user_ticket(&st.parsed_ticket, &st.raw_ticket)) {
        error = -2;
        break;
    }
    
    /* Lock the workspace list. */
    kcdpg_lock_kws_list(ts);
    
    /* Validate and create the workspace as needed. */
    if (kcdpg_mgt_cmd_create_kws_validate(&st)) {
        kcdpg_mgt_cmd_create_kws_create(&st);
        
        st.res_type = KANP_RES_MGT_KWS_CREATED;
        anp_write_uint64(&st.res_buf, st.kws_id);
        if (st.cmd_minor <= 2) anp_write_bin(&st.res_buf, &st.nonce);
        if (st.cmd_minor >= 3) anp_write_kstr(&st.res_buf, &st.email_id);
    }
    
    /* Write the output parameters. */
    anp_write_uint32(&st.ext_buf, st.res_type);
    anp_write_bin(&st.ext_buf, &st.res_buf);

KCDPG_QUERY_END(cmd_mgt_create_kws)


/* Check if the user is allowed to login to the workspace specified using the
 * information provided in the MGT_CONNECT_KWS command:
 *   UINT64 Workspace ID.
 *   UINT32 Delete workspace flag.
 *   UINT32 Login type.
 *   UINT32 User ID.
 *   STR    User email.
 *   STR    user email ID.
 *   BIN    User ticket.
 *   UINT64 User last event ID.
 *   UINT64 User last event date.
 *
 * Output:
 *   UINT64 KCD last event ID.
 *   UINT32 Login code to return. 0 if no error occurred.
 *   UINT32 True if a choose user ID result must be returned.
 *   UINT32 True if a permission denied result must be returned.
 *   UINT32 True if the ticket provided was retrieved from the cache.
 *   UINT32 True if the workspace is secure.
 *   UINT32 True if the workspace is in V2 compatibility mode.
 *   UINT32 True if the user is registered.
 *   UINT32 Actual user ID.
 *   STR    Actual user email address.
 *   STR    Actual user password, if any.
 *   STR    Error string, if any.
 */
KCDPG_QUERY_STRUCT(cmd_mgt_connect_kws)
    uint64_t kws_id;
    uint64_t user_last_event_id;
    uint64_t user_last_event_date;
    uint64_t kcd_last_event_id;
    uint32_t delete_kws_flag;
    uint32_t login_type;
    uint32_t user_id;
    uint32_t kws_flags;
    uint32_t user_flags;
    uint32_t login_code;
    uint32_t choose_user_id_flag;
    uint32_t perm_denied_flag;
    kstr email;
    kstr email_id;
    kstr pwd;
    kstr error_str;
    kbuffer ticket;
    kbuffer last_evt;

KCDPG_QUERY_INIT(cmd_mgt_connect_kws, 0)
    kstr_init(&self->email);
    kstr_init(&self->email_id);
    kstr_init(&self->pwd);
    kstr_init(&self->error_str);
    kbuffer_init(&self->ticket);
    kbuffer_init(&self->last_evt);

KCDPG_QUERY_CLEAN(cmd_mgt_connect_kws)
    kstr_clean(&self->email);
    kstr_clean(&self->email_id);
    kstr_clean(&self->pwd);
    kstr_clean(&self->error_str);
    kbuffer_clean(&self->ticket);
    kbuffer_clean(&self->last_evt);

KCDPG_QUERY_STATIC

/* Handle the compatibility login mode. */
static int kcdpg_mgt_cmd_connect_kws_compat_mode(struct kcdpg_cmd_mgt_connect_kws_state *st) {
    uint32_t real_user_id = 0;
    
    /* Locate the user entry by the user ID. */
    if (st->user_id) {
        kstr_sf(&st->ts, "SELECT user_id FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u AND user_id = %u",
                         st->kws_id, st->user_id);
    }
    
    /* Locate the user entry by the email address. */
    else {
        kstr_sf(&st->ts, "SELECT user_id FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u AND email = ", st->kws_id);
        kcdpg_add_str(&st->ts, &st->email);
    }
    
    kcdpg_exec_query(st->ts.data);
    
    /* User entry not found. */
    if (!SPI_processed) {
        
        /* This is fatal. */
        if (st->user_id) {
            kmod_set_error("no such user");
            return -1;
        }
        
        /* Send a choose user ID reply. */
        else {
            st->choose_user_id_flag = 1;
            return 0;
        }
    }
    
    /* Get the real user ID. */
    real_user_id = kcdpg_get_uint32(0, 0);
    
    /* Get the system email ID associated to the user entry. */
    kstr_sf(&st->ts, "SELECT email_id FROM kcd_kws_user_invitation WHERE "
                     "kws_id = "PRINTF_64"u AND user_id = %u and inviting_user_id = 0", st->kws_id, real_user_id);
    kcdpg_exec_query(st->ts.data);
    
    /* System email ID not found. */
    if (!SPI_processed) {
        kmod_set_error("system email ID not found");
        return -1;
    }
    
    /* Update the email ID. */
    kcdpg_get_str(0, 0, &st->email_id);
    
    return 0;
}

/* Locate the user entry using the email ID and obtain the relevant information. */
static int kcdpg_mgt_cmd_connect_kws_locate_user(struct kcdpg_cmd_mgt_connect_kws_state *st) {
    kstr *ts = &st->ts;
    uint32_t real_user_id = st->user_id;
 
    /* Normal login. */
    if (!kcdpg_is_login_priv(st->login_type)) {
        uint64_t real_kws_id;
    
        /* Locate the email ID. */
        kstr_sf(ts, "SELECT kws_id, user_id FROM kcd_kws_user_invitation WHERE email_id = ");
        kcdpg_add_str(ts, &st->email_id);
        kcdpg_exec_query(ts->data);
        
        /* Email ID not found. */
        if (!SPI_processed) {
            kmod_set_error("email ID not found (you might have been banned)");
            st->login_code = KANP_KWS_LOGIN_BAD_EMAIL_ID;
            return 0;
        }
        
        real_kws_id = kcdpg_get_uint64(0, 0);
        real_user_id = kcdpg_get_uint64(0, 1);
        
        /* Validate the workspace ID. */
        if (real_kws_id != st->kws_id) {
            kmod_set_error("no such " KCD_KWS_NAME);
            st->login_code = KANP_KWS_LOGIN_BAD_KWS_ID;
            return 0;
        }
        
        /* Validate the user ID if it is provided. */
        if (st->user_id && real_user_id != st->user_id) {
            kmod_set_error("invalid user ID");
            return -1;
        }
        
        /* Get the user's password, if any. */
        kstr_sf(ts, "SELECT pwd FROM kcd_kws_users "
                    "WHERE kws_id = "PRINTF_64"u AND user_id = %u", st->kws_id, real_user_id);
        kcdpg_get_row_str(ts, &st->pwd);
    }
    
    /* Update the user ID. */
    st->user_id = real_user_id;
    
    /* Get the user flags. */
    if (kcdpg_perm_check_user_exist(ts, st->kws_id, st->user_id, &st->user_flags)) return -1;
    
    return 0;
}

/* Check if the view of the events of the user is consistent and obtain the KCD
 * last event ID.
 */
static void kcdpg_mgt_cmd_connect_kws_check_last_event(struct kcdpg_cmd_mgt_connect_kws_state *st) {

    /* Check the consistency if required. */
    if (!st->delete_kws_flag && st->user_last_event_id) {
        uint64_t u;
        
	/* Retrieve the event. */
	kstr_sf(&st->ts, "SELECT event FROM kcd_kws_event_log WHERE kws_id = "PRINTF_64"u AND evt_id="PRINTF_64"u",
                         st->kws_id, st->user_last_event_id);
        kcdpg_exec_query(st->ts.data);
        
	if (!SPI_processed) {
            kmod_set_error("the " KCD_KWS_NAME " information is out of date");
            st->login_code = KANP_KWS_LOGIN_OOS;
	    return;
	}
        
	kcdpg_get_bytea(0, 0, &st->last_evt);
	
	/* Check the timestamp. */
	if (anp_read_uint64(&st->last_evt, &u) || anp_read_uint64(&st->last_evt, &u)) {
            elog(ERROR, "%s", kmod_strerror());
        }
	
	if (u != st->user_last_event_date) {
            kmod_set_error("the " KCD_KWS_NAME " information is out of date");
            st->login_code = KANP_KWS_LOGIN_OOS;
	    return;
	}
    }
        
    /* Get the last KCD event ID. */
    kstr_sf(&st->ts, "SELECT max(evt_id) FROM kcd_kws_event_log WHERE kws_id = "PRINTF_64"u", st->kws_id);
    kcdpg_exec_query(st->ts.data);
    st->kcd_last_event_id = kcdpg_get_uint64(0, 0);

KCDPG_QUERY_START(cmd_mgt_connect_kws)
    
    if (anp_read_uint64(&st.arg_buf, &st.kws_id) ||
        anp_read_uint32(&st.arg_buf, &st.delete_kws_flag) ||
        anp_read_uint32(&st.arg_buf, &st.login_type) ||
        anp_read_uint32(&st.arg_buf, &st.user_id) ||
        anp_read_kstr(&st.arg_buf, &st.email) ||
        anp_read_kstr(&st.arg_buf, &st.email_id) ||
        anp_read_bin(&st.arg_buf, &st.ticket) ||
        anp_read_uint64(&st.arg_buf, &st.user_last_event_id) ||
        anp_read_uint64(&st.arg_buf, &st.user_last_event_date)) {
        elog(ERROR, "bad cmd_mgt_connect_kws argument: %s", kmod_strerror());
    }
    
    do {
        /* Lock the workspace, if any. */
        if (kcdpg_perm_check_lock_kws(ts, st.kws_id, 0, &st.login_code)) break;
        st.kws_flags = kcdpg_get_kws_flags(ts, st.kws_id);
        
        /* The email ID is empty. */
        if (!st.email_id.slen) {
            
            /* We're not in compatibility mode. Refuse the login. */
            if (!kcdpg_is_kws_compatv2(st.kws_flags)) {
                kmod_set_error("empty email ID provided");
                st.login_code = KANP_KWS_LOGIN_BAD_EMAIL_ID;
                break;
            }
        
            /* Handle the compatibility login mode. */
            error = kcdpg_mgt_cmd_connect_kws_compat_mode(&st);
            if (error || st.choose_user_id_flag) break;
        }
        
        /* Locate the user from the email ID. */
        error = kcdpg_mgt_cmd_connect_kws_locate_user(&st);
        if (error || st.login_code) break;
        
        /* Check if the user can log in. */
        if (kcdpg_perm_check_kws_login(st.kws_flags, 0, st.user_flags, &st.login_code)) break;
        
        /* Check if the user can delete the workspace. */
        if (st.delete_kws_flag && (kcdpg_perm_check_kws_not_frozen(st.kws_flags, st.user_flags) ||
                                   kcdpg_perm_check_user_admin(st.user_flags))) {
            st.perm_denied_flag = 1;
            break;
        }
        
        /* Check if the view of the events of the user is consistent and obtain
         * the KCD last event ID.
         */
        kcdpg_mgt_cmd_connect_kws_check_last_event(&st);
        if (st.login_code) break;
        
    } while (0);
    
    /* Bail out if a user error occurred. */
    if (error) break;
    
    /* Cache the error string, to avoid clobbering it accidentally. */
    kstr_assign_kstr(&st.error_str, kmod_kstrerror());
    
    /* Send back the reply. */
    anp_write_uint64(&st.ext_buf, st.kcd_last_event_id);
    anp_write_uint32(&st.ext_buf, st.login_code);
    anp_write_uint32(&st.ext_buf, st.choose_user_id_flag);
    anp_write_uint32(&st.ext_buf, st.perm_denied_flag);
    anp_write_uint32(&st.ext_buf, kcdpg_is_kws_secure(st.kws_flags) &&
                                  kcdpg_is_kws_login_ticket_stored(ts, st.kws_id, st.user_id, &st.ticket));
    anp_write_uint32(&st.ext_buf, kcdpg_is_kws_secure(st.kws_flags));
    anp_write_uint32(&st.ext_buf, kcdpg_is_kws_compatv2(st.kws_flags));
    anp_write_uint32(&st.ext_buf, kcdpg_is_user_registered(st.user_flags));
    anp_write_uint32(&st.ext_buf, st.user_id);
    anp_write_kstr(&st.ext_buf, &st.email_id);
    anp_write_kstr(&st.ext_buf, &st.pwd);
    anp_write_kstr(&st.ext_buf, &st.error_str);
    
KCDPG_QUERY_END(cmd_mgt_connect_kws)


/* Mark a user as registered in a workspace if required (workspace-bound query):
 *   STR    User name.
 */
KCDPG_QUERY_STRUCT(register_kws_user)
    kstr user_name;
    
KCDPG_QUERY_INIT(register_kws_user, 1)
    kstr_init(&self->user_name);
    
KCDPG_QUERY_CLEAN(register_kws_user)
    kstr_clean(&self->user_name);
    
KCDPG_QUERY_START(register_kws_user)
    
    if (anp_read_kstr(&st.arg_buf, &st.user_name)) {
        elog(ERROR, "bad register_kws_user argument: %s", kmod_strerror());
    }
    
    if (kcdpg_perm_check_kws_bound(ts, wb, 1, 0, 1)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* No need to register the user. */
    if (kcdpg_is_user_root(wb->user_flags) ||
        kcdpg_is_user_banned(wb->user_flags) ||
        kcdpg_is_user_registered(wb->user_flags)) break;
       
    /* Update the flags and the user name in the user table. */
    kcdpg_set_kws_user_flags(&wb->user_flags, KANP_USER_FLAG_REGISTER, 1);
    kcdpg_update_kws_user_flags(ts, wb->kws_id, wb->user_id, wb->user_flags);
    kcdpg_update_kws_user_name_user(ts, wb->kws_id, wb->user_id, &st.user_name);
    
    /* Post an event announcing the registration of the user. */
    anp_write_uint64(&st.evt_buf, wb->kws_id);
    anp_write_uint64(&st.evt_buf, wb->date);
    anp_write_uint32(&st.evt_buf, wb->user_id);
    anp_write_kstr(&st.evt_buf, &st.user_name);
    kcdpg_post_event_internal(ts, wb->kws_id, 2, KANP_EVT_KWS_USER_REGISTERED, &st.evt_buf);
    
KCDPG_QUERY_END(register_kws_user)


/* Store the workspace user ticket specified if required (workspace-bound query):
 *   BIN    Ticket.
 */
KCDPG_QUERY_STRUCT(store_kws_user_ticket)
    kbuffer ticket;

KCDPG_QUERY_INIT(store_kws_user_ticket, 1)
    kbuffer_init(&self->ticket);

KCDPG_QUERY_CLEAN(store_kws_user_ticket)
    kbuffer_clean(&self->ticket);

KCDPG_QUERY_START(store_kws_user_ticket)
    
    if (anp_read_bin(&st.arg_buf, &st.ticket)) {
        elog(ERROR, "bad store_kws_user_ticket argument: %s", kmod_strerror());
    }
    
    if (kcdpg_perm_check_kws_bound(ts, wb, 1, 0, 1)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* Abort if the ticket doesn't need to be stored. */
    if (kcdpg_is_user_root(wb->user_flags) ||
        kcdpg_is_user_banned(wb->user_flags) ||
        kcdpg_is_kws_login_ticket_stored(ts, wb->kws_id, wb->user_id, &st.ticket)) break;
    
    /* Store the ticket. */
    kstr_sf(ts, "INSERT INTO kcd_kws_login_ticket (kws_id, user_id, ticket) VALUES (");
    kcdpg_add_uint64(ts, wb->kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, wb->user_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_bytea(ts, &st.ticket); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    
KCDPG_QUERY_END(store_kws_user_ticket)


/* Represent a user being invited to a workspace. */
struct kcdpg_invited_user {
    uint32_t new_flag;
    uint64_t key_id;
    uint32_t user_id;
    uint32_t user_flags;
    uint32_t send_email_flag;
    kstr name;
    kstr email;
    kstr email_id;
    kstr pwd;
    kstr org_name;
};

static struct kcdpg_invited_user* kcdpg_invited_user_new() {
    struct kcdpg_invited_user *self = kcalloc(sizeof(struct kcdpg_invited_user));
    kstr_init(&self->name);
    kstr_init(&self->email);
    kstr_init(&self->email_id);
    kstr_init(&self->pwd);
    kstr_init(&self->org_name);
    return self;
}

static void kcdpg_invited_user_destroy(struct kcdpg_invited_user *self) {
    if (self) {
        kstr_clean(&self->name);
        kstr_clean(&self->email);
        kstr_clean(&self->email_id);
        kstr_clean(&self->pwd);
        kstr_clean(&self->org_name);
        kfree(self);
    }
}

/* Invite users to a workspace (workspace-bound query):
 *   STR    Root email address.
 *   
 * Output:
 *   STR    Workspace name.
 *   STR    Sender name.
 *   STR    Sender email address.
 *   STR    Invitation message.
 *   UINT32 Number of users.
 *     UINT32 Send email flag.
 *     STR    Name.
 *     STR    Email address.
 *     STR    Email ID.
 */
KCDPG_QUERY_STRUCT(cmd_mgt_invite_kws)
    karray user_array;
    karray new_array;
    karray banned_array;
    kstr invitation_msg;
    kstr root_email;
    kstr sender_name;
    kstr sender_email;
    struct kcdpg_kws_prop_change_state *pcs;
    
KCDPG_QUERY_INIT(cmd_mgt_invite_kws, 1)
    karray_init(&self->user_array);
    karray_init(&self->new_array);
    karray_init(&self->banned_array);
    kstr_init(&self->invitation_msg);
    kstr_init(&self->root_email);
    kstr_init(&self->sender_name);
    kstr_init(&self->sender_email);
    
KCDPG_QUERY_CLEAN(cmd_mgt_invite_kws)
    int i;
    for (i = 0; i < self->user_array.size; i++) kcdpg_invited_user_destroy(self->user_array.data[i]);
    karray_clean(&self->user_array);
    karray_clean(&self->new_array);
    karray_clean(&self->banned_array);
    kstr_clean(&self->invitation_msg);
    kstr_clean(&self->root_email);
    kstr_clean(&self->sender_name);
    kstr_clean(&self->sender_email);
    kcdpg_kws_prop_change_state_destroy(self->pcs);
    
KCDPG_QUERY_STATIC
    
/* Generate the name and email address of the user specified. */
static void kcdpg_cmd_mgt_invite_get_name_and_email(struct kcdpg_cmd_mgt_invite_kws_state *st, uint32_t user_id,
                                                    kstr *name, kstr *email) {
    kstr *ts = &st->ts, *ts2 = &st->ts2;
    
    if (!user_id) {
        kstr_assign_cstr(name, KCD_KWS_NAME " Administrator");
        kstr_assign_kstr(email, &st->root_email);
        return;
    }

    kstr_sf(ts, "SELECT name_admin, name_user, email FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u AND user_id = %u",
                st->wb->kws_id, user_id);
    kcdpg_exec_query(ts->data);
    if (!SPI_processed) { elog(ERROR, "user not found"); }
    kcdpg_get_str(0, 0, ts);
    kcdpg_get_str(0, 1, ts2);
    if (ts->slen) kstr_assign_kstr(name, ts);
    else if (ts2->slen) kstr_assign_kstr(name, ts2);
    else kstr_reset(name);
    kcdpg_get_str(0, 2, email);
}

/* Parse the command arguments. */
static int kcdpg_cmd_mgt_invite_kws_parse_cmd(struct kcdpg_cmd_mgt_invite_kws_state *st) {
    uint32_t i, nb_user;
    uint64_t u;
    struct kcdpg_kws_bound_state *wb = st->wb;
    
    /* Parse the command arguments. */
    if (anp_read_uint64(&wb->cmd_buf, &u) ||
        (wb->cmd_minor >= 3 && anp_read_kstr(&wb->cmd_buf, &st->invitation_msg)) ||
        anp_read_uint32(&wb->cmd_buf, &nb_user)) {
        return -1;
    }
    
    for (i = 0; i < nb_user; i++) {
        struct kcdpg_invited_user *iu = kcdpg_invited_user_new();
        karray_push(&st->user_array, iu);
        
        if (anp_read_kstr(&wb->cmd_buf, &iu->name) || anp_read_kstr(&wb->cmd_buf, &iu->email))
            return -1;
        
        if (wb->cmd_minor <= 2) {
            uint32_t m, a;
            
            if (anp_read_uint32(&wb->cmd_buf, &m) || anp_read_uint32(&wb->cmd_buf, &a))
                return -1;
            
            if (m) {
                if (anp_read_uint64(&wb->cmd_buf, &iu->key_id) || anp_read_kstr(&wb->cmd_buf, &iu->org_name))
                    return -1;
            }
            
            else {
                if (anp_read_kstr(&wb->cmd_buf, &iu->pwd))
                    return -1;
            }
        }
        
        else {
            if (anp_read_uint64(&wb->cmd_buf, &iu->key_id) ||
                anp_read_kstr(&wb->cmd_buf, &iu->org_name) ||
                anp_read_kstr(&wb->cmd_buf, &iu->pwd) ||
                anp_read_uint32(&wb->cmd_buf, &iu->send_email_flag)) {
                return -1;
            }
        }
    }
    
    return 0;
}

/* Return true if the user is inviting himself. */
static int kcdpg_cmd_mgt_invite_kws_is_self_invite(struct kcdpg_cmd_mgt_invite_kws_state *st) {
    kstr *ts = &st->ts;
    struct kcdpg_kws_bound_state *wb = st->wb;
    struct kcdpg_invited_user *iu;
    
    if (st->user_array.size != 1) return 0;
    iu = st->user_array.data[0];
    
    kstr_sf(ts, "SELECT user_id FROM kcd_kws_users "
                "WHERE kws_id = "PRINTF_64"u AND user_id = %u AND lower(email) = lower(", wb->kws_id, wb->user_id);
    kcdpg_add_str(ts, &iu->email);
    kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    return (SPI_processed > 0);
}

/* Find out whether the user specified is a new or an existing user and validate
 * the inviter's right to invite the user specified.
 */
static int kcdpg_cmd_mgt_invite_kws_find_user(struct kcdpg_cmd_mgt_invite_kws_state *st,
                                              struct kcdpg_invited_user *iu) {
    struct kcdpg_kws_bound_state *wb = st->wb;
    kstr *ts = &st->ts;
    
    /* Check if the user is in the user table. */
    kstr_sf(ts, "SELECT user_id, flags FROM kcd_kws_users WHERE kws_id = "PRINTF_64"u AND lower(email) = lower(",
                wb->kws_id);
    kcdpg_add_str(ts, &iu->email);
    kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    
    /* The user is not in the user table. */
    if (!SPI_processed) {
        iu->new_flag = 1;
    }
    
    /* The user already exists. */
    else {
    
        /* Retrieve the user information. */
        iu->user_id = kcdpg_get_uint32(0, 0);
        iu->user_flags = kcdpg_get_uint32(0, 1);
        
        /* Validate that the inviter has the right to invite the user. */
        if (kcdpg_perm_check_priv_level(wb->user_flags, iu->user_flags)) return -1;
    }
    
    return 0;
}

/* Invite the user specified. */
static void kcdpg_cmd_mgt_invite_kws_invite_user(struct kcdpg_cmd_mgt_invite_kws_state *st,
                                                 struct kcdpg_invited_user *iu) {
    int system_invite_flag = 0;
    uint32_t notif_policy;
    struct kcdpg_kws_bound_state *wb = st->wb;
    kstr *ts = &st->ts, *ts2 = &st->ts2, *ts3 = &st->ts3;
    
    /* This is a new user. */
    if (iu->new_flag) {
        
        /* Add the user to the new user array. */
        karray_push(&st->new_array, iu);
        
        /* Insert the user in the global user list. */
        kcdpg_insert_global_user(ts, &iu->email, KANP_EMAIL_SUMMARY_FLAG);
        
        /* Add the user in the workspace user table. */
        iu->user_id = kcdpg_get_next_seq_id(ts, wb->kws_id, "user");
        kstr_reset(ts2);
        
        notif_policy = kcdpg_get_global_user_notif_policy(ts, &iu->email);
        kcdpg_insert_kws_user(ts, wb->kws_id, iu->user_id, iu->user_flags, notif_policy, &iu->email, &iu->name,
                              ts2, &iu->org_name, &iu->pwd);
       
        /* A system invitation may be required. */
        system_invite_flag = 1;
    }
    
    /* The user already exists. */
    else {
    
        /* The user is banned. */
        if (kcdpg_is_user_banned(iu->user_flags)) {
            
            /* A system invitation may be required. */
            system_invite_flag = 1;
            
            /* Add the user to the banned user array. */
            karray_push(&st->banned_array, iu);
        }
    }
    
    /* Generate an email ID. */
    kcd_mgt_generate_email_id(&iu->email_id);
    
    /* Add the invitation in the invitation table. */
    kcdpg_insert_invitation(ts, &iu->email_id, wb->kws_id, wb->user_id, iu->user_id, wb->date, iu->key_id, &iu->pwd);
    
    /* Make a system invitation if required. */
    if (system_invite_flag && wb->user_id)
        kcdpg_insert_system_email_id(ts, ts2, ts3, wb->kws_id, iu->user_id, wb->date);
            
    /* Insert the key ID in the list of trusted key IDs. */
    if (iu->key_id) kcdpg_insert_trusted_key_id(ts, wb->kws_id, iu->key_id);
    
    /* Generate the name and email address of the user. */
    kcdpg_cmd_mgt_invite_get_name_and_email(st, iu->user_id, &iu->name, &iu->email);
}

/* Post the required events. */
static void kcdpg_cmd_mgt_invite_kws_process_post_event(struct kcdpg_cmd_mgt_invite_kws_state *st) {
    int i;
    struct kcdpg_kws_bound_state *wb = st->wb;
    
    /* Process the new users. */
    if (st->new_array.size) {
    
        /* Determine the maximum minor version of the invitation event to post.
         * This is 3 unless the command is sent by the KWMO and the workspace is
         * in compatibility mode or the client has version 2. In that case, we
         * post the invitation in version 2 to avoid breaking compatibility with
         * old clients.
         */
        uint32_t evt_minor = 3;
        if (wb->cmd_minor < 3 || (wb->login_type == KCD_KWS_LOGIN_TYPE_KWMO && kcdpg_is_kws_compatv2(wb->kws_flags))) {
            evt_minor = 2;
        }
        
        anp_write_uint64(&st->evt_buf, wb->kws_id);
        anp_write_uint64(&st->evt_buf, wb->date);
        if (evt_minor >= 3) anp_write_uint32(&st->evt_buf, wb->user_id);
        anp_write_uint32(&st->evt_buf, st->new_array.size);
        
        for (i = 0; i < st->new_array.size; i++) {
            struct kcdpg_invited_user *iu = st->new_array.data[i];
            anp_write_uint32(&st->evt_buf, iu->user_id);
            anp_write_kstr(&st->evt_buf, &iu->name);
            anp_write_kstr(&st->evt_buf, &iu->email);
            if (evt_minor == 2) {
                anp_write_uint32(&st->evt_buf, 0);
                anp_write_uint32(&st->evt_buf, 0);
            }
            anp_write_kstr(&st->evt_buf, &iu->org_name);
        }
        
        kcdpg_post_event_internal(&st->ts, wb->kws_id, evt_minor, KANP_EVT_KWS_INVITED, &st->evt_buf);
    }
    
    /* Process the banned users. */
    if (st->banned_array.size) {
        for (i = 0; i < st->banned_array.size; i++) {
            struct kcdpg_invited_user *iu = st->banned_array.data[i];
            kcdpg_set_kws_user_flags(&iu->user_flags, KANP_USER_FLAG_BAN, 0);
            kcdpg_kws_prop_change_user_flags(st->pcs, iu->user_id, iu->user_flags);
        }
        
        kcdpg_kws_prop_change_post_event(st->pcs, wb->date);
    }

KCDPG_QUERY_START(cmd_mgt_invite_kws)
    int i;
    
    if (anp_read_kstr(&st.arg_buf, &st.root_email)) {
        elog(ERROR, "bad cmd_mgt_invite_kws argument: %s", kmod_strerror());
    }
            
    /* Parse the command arguments. */
    error = kcdpg_cmd_mgt_invite_kws_parse_cmd(&st);
    if (error) break;
    
    /* Lock and validate. */
    kcdpg_lock_kws_list(ts);
    
    if (kcdpg_perm_check_kws_bound(ts, wb, 1, 1, 1) ||
        (!kcdpg_cmd_mgt_invite_kws_is_self_invite(&st) && kcdpg_perm_check_user_manager(wb->user_flags))) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* Find and validate the users. */
    for (i = 0; i < st.user_array.size; i++) {
        error = kcdpg_cmd_mgt_invite_kws_find_user(&st, st.user_array.data[i]);
        if (error) break;
    }
    
    if (error) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* Create the property change state object. */
    st.pcs = kcdpg_kws_prop_change_state_new(wb->kws_id, wb->user_id);
    
    /* Invite the invited users. */
    for (i = 0; i < st.user_array.size; i++)
        kcdpg_cmd_mgt_invite_kws_invite_user(&st, st.user_array.data[i]);
    
    /* Post the required events. */
    kcdpg_cmd_mgt_invite_kws_process_post_event(&st);
    
    /* Success. Send back the invitation information. */
    kcdpg_get_kws_name(ts, wb->kws_id, ts2);
    anp_write_kstr(&st.ext_buf, ts2);
    
    kcdpg_cmd_mgt_invite_get_name_and_email(&st, wb->user_id, &st.sender_name, &st.sender_email);
    anp_write_kstr(&st.ext_buf, &st.sender_name);
    anp_write_kstr(&st.ext_buf, &st.sender_email);
    
    anp_write_kstr(&st.ext_buf, &st.invitation_msg);
    anp_write_uint32(&st.ext_buf, st.user_array.size);
    for (i = 0; i < st.user_array.size; i++) {
        struct kcdpg_invited_user *iu = st.user_array.data[i];
        anp_write_uint32(&st.ext_buf, iu->send_email_flag);
        anp_write_kstr(&st.ext_buf, &iu->name);
        anp_write_kstr(&st.ext_buf, &iu->email);
        anp_write_kstr(&st.ext_buf, &iu->email_id);
    }
 
KCDPG_QUERY_END(cmd_mgt_invite_kws)


/* Delete a workspace (safe query):
 *   UINT64 Workspace ID.
 */
KCDPG_QUERY_STRUCT(delete_kws)
    uint64_t kws_id;
    uint32_t kws_flags;

KCDPG_QUERY_INIT(delete_kws, 0)

KCDPG_QUERY_CLEAN(delete_kws)

KCDPG_QUERY_START(delete_kws)
    uint32_t i;
    static char *table_array[] = { "kcd_locks",
                                   "kcd_sequences",
                                   "kcd_kws_event_log",
                                   "kcd_kws_notif_log",
                                   "kcd_kws_trusted_key",
                                   "kcd_kws_users",
                                   "kcd_kws_user_invitation",
                                   "kcd_kws_login_ticket", 
                                   "kcd_kws_kfs_upload",
                                   "kcd_kws_kfs_download",
                                   "kcd_kws_kfs_limit",
                                   "kcd_kws_kfs_current_view",
                                   "kcd_kws_kfs_file_map",
                                   "kcd_kws_vnc_session",
                                   "kcd_kws_pub_email_info",
                                   "kcd_kws_pub_email_recipient_info" };
    
    do {
        /* Retrieve the arguments. */
        if (anp_read_uint64(&st.arg_buf, &st.kws_id)) {
            elog(ERROR, "bad delete_kws argument: %s", kmod_strerror());
        }
        
        /* Lock the workspace list. */
        kcdpg_lock_kws_list(ts);
        
        /* Lock the workspace for writing or bail out if it is already deleted. */
        if (kcdpg_perm_check_lock_kws(ts, st.kws_id, 1, NULL)) break;
        
        /* Mark the workspace as deleted. */
        st.kws_flags = kcdpg_get_kws_flags(ts, st.kws_id);
        st.kws_flags |= KANP_KWS_FLAG_DELETE;
        kcdpg_update_kws_flags(ts, st.kws_id, st.kws_flags);
        
        /* Purge the information from the workspace tables. */
        for (i = 0; i < KUTIL_ARRAY_SIZE(table_array); i++) {
            kstr_sf(ts, "DELETE FROM %s WHERE kws_id = "PRINTF_64"u", table_array[i], st.kws_id);
            kcdpg_exec_query(ts->data);
        }

        /* Notify the listeners of the perm_check relation. */
        notify_perm_check(ts, st.kws_id);
        
    } while (0);
        
KCDPG_QUERY_END(delete_kws)


/* Post a chat message (workspace-bound query). */
KCDPG_QUERY_STRUCT(cmd_chat_msg)
    uint32_t chat_id;
    kstr chat_msg;

KCDPG_QUERY_INIT(cmd_chat_msg, 1)
    kstr_init(&self->chat_msg);

KCDPG_QUERY_CLEAN(cmd_chat_msg)
    kstr_clean(&self->chat_msg);

KCDPG_QUERY_START(cmd_chat_msg)
    uint64_t u, evt_id;
    
    if (anp_read_uint64(&wb->cmd_buf, &u) ||
        anp_read_uint32(&wb->cmd_buf, &st.chat_id) ||
        anp_read_kstr(&wb->cmd_buf, &st.chat_msg)) {
        error = -1;
        break;
    }
    
    if (kcdpg_perm_check_kws_bound(ts, wb, 1, 1, 1)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    anp_write_uint64(&st.evt_buf, wb->kws_id);
    anp_write_uint64(&st.evt_buf, wb->date);
    anp_write_uint32(&st.evt_buf, st.chat_id);
    anp_write_uint32(&st.evt_buf, wb->user_id);
    anp_write_kstr(&st.evt_buf, &st.chat_msg);
    evt_id = kcdpg_post_event_internal(ts, wb->kws_id, 2, KANP_EVT_CHAT_MSG, &st.evt_buf);
        
    /* Post the notification only if this is not a public workspace. */
    if (!kcdpg_is_kws_public(wb->kws_flags)) {
        anp_write_kstr(&st.ntf_buf, &st.chat_msg);
        kcdpg_post_notif(ts, wb->kws_id, evt_id, wb->date, wb->user_id, KANP_EVT_CHAT_MSG, &st.ntf_buf);
    }
     
KCDPG_QUERY_END(cmd_chat_msg)


/* Get a unique URL representing an email (workspace-bound query).
 *   STR    Web host.
 */
KCDPG_QUERY_STRUCT(cmd_kws_get_uurl)
    uint32_t nb_attach;
    uint32_t nb_recipient;
    uint32_t nb_item;
    uint64_t email_id;
    uint64_t att_expire_delay;
    uint64_t att_expire_date;
    kstr web_host;
    kstr from_name;
    kstr from_email;
    kstr subject;
    karray name_array;
    karray email_array;

KCDPG_QUERY_INIT(cmd_kws_get_uurl, 1)
    kstr_init(&self->web_host);
    kstr_init(&self->from_name);
    kstr_init(&self->from_email);
    kstr_init(&self->subject);
    karray_init(&self->name_array);
    karray_init(&self->email_array);

KCDPG_QUERY_CLEAN(cmd_kws_get_uurl)
    kstr_clean(&self->web_host);
    kstr_clean(&self->from_name);
    kstr_clean(&self->from_email);
    kstr_clean(&self->subject);
    karray_clear_kstr(&self->name_array);
    karray_clean(&self->name_array);
    karray_clear_kstr(&self->email_array);
    karray_clean(&self->email_array);

KCDPG_QUERY_START(cmd_kws_get_uurl)
    uint64_t u;
    uint32_t i;
    
    /* Retrieve the arguments. */
    if (anp_read_kstr(&st.arg_buf, &st.web_host)) {
        elog(ERROR, "bad cmd_kws_get_uurl argument: %s", kmod_strerror());
    }
    
    /* Parse the command arguments. */
    if (anp_read_uint64(&wb->cmd_buf, &u) ||
        anp_read_kstr(&wb->cmd_buf, &st.from_name) ||
        anp_read_kstr(&wb->cmd_buf, &st.from_email) ||
        anp_read_kstr(&wb->cmd_buf, &st.subject) ||
        (wb->cmd_minor >= 3 && anp_read_uint32(&wb->cmd_buf, &st.nb_attach)) ||
        anp_read_uint32(&wb->cmd_buf, &st.nb_recipient) ||
        anp_read_uint32(&wb->cmd_buf, &st.nb_item)) {
        error = -1;
        break;
    }
    
    for (i = 0; i < st.nb_recipient; i++) {
        kstr *rec_name = kstr_new(), *rec_email = kstr_new();
        karray_push(&st.name_array, rec_name);
        karray_push(&st.email_array, rec_email);
        
        if (anp_read_kstr(&wb->cmd_buf, rec_name) ||
            anp_read_kstr(&wb->cmd_buf, rec_email)) {
            error = -1;
            break;
        }
    }
    
    if (error) break;
    
    if (wb->cmd_minor >= 6 && anp_read_uint64(&wb->cmd_buf, &st.att_expire_delay)) {
        error = -1;
        break;
    }
    
    if (st.nb_recipient == 0) {
        kmod_set_error("no recipient specified");
        error = -1;
        break;
    }
    
    /* Validate the context. */
    if (kcdpg_perm_check_kws_bound(ts, wb, 1, 1, 1) ||
        kcdpg_perm_check_kws_public(wb->kws_flags)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* Get a unique email ID. */
    while (1) {
        unsigned char buf[7];
        
        /* Generate 7 bytes of random data. The leftmost byte must be 0
         * since Postgres does not accept unsigned big ints.
         */
        if (kutil_generate_random(buf, 7)) {
            elog(ERROR, "cannot generate email ID: %s", kmod_strerror());
        }
        
        /* Incredibly, the C compiler will convert from uint8_t to int32_t
         * (change signedness!) if you don't use proper casts.
         */
        st.email_id = 0;
        for (i = 0; i < 7; i++) st.email_id += ((uint64_t)buf[i] << (uint64_t)(i*8));
        
        kstr_sf(ts, "SELECT email_id FROM kcd_kws_pub_email_info WHERE "
                    "kws_id = "PRINTF_64"u AND email_id = "PRINTF_64"u", wb->kws_id, st.email_id);
        kcdpg_exec_query(ts->data);
        if (!SPI_processed) break;
    }
    
    /* Compute the expiration date. */
    if (st.att_expire_delay) st.att_expire_date = wb->date + st.att_expire_delay;
    else st.att_expire_date = INT64_MAX;
    
    /* Insert the data in kcd_kws_pub_email_info. */
    kstr_sf(ts, "INSERT INTO kcd_kws_pub_email_info "
                "(kws_id, email_id, subject, date, att_expire_date, att_expire_flag, name, address, nb_attachment)"
                " VALUES ( ");
    kcdpg_add_uint64(ts, wb->kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, st.email_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_str(ts, &st.subject); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, wb->date); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, st.att_expire_date); kstr_append_cstr(ts, ", ");
    kstr_append_cstr(ts, "0, ");
    kcdpg_add_str(ts, &st.from_name); kstr_append_cstr(ts, ", ");
    kcdpg_add_str(ts, &st.from_email); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, st.nb_attach); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    
    /* Insert the data in kcd_kws_pub_email_recipient_info. */
    for (i = 0; i < st.nb_recipient; i++) {
        kstr *rec_name = st.name_array.data[i];
        kstr *rec_email = st.email_array.data[i];
        kstr_sf(ts, "INSERT INTO kcd_kws_pub_email_recipient_info (kws_id, email_id, name, address) VALUES ( ");
        kcdpg_add_uint64(ts, wb->kws_id); kstr_append_cstr(ts, ", ");
        kcdpg_add_uint64(ts, st.email_id); kstr_append_cstr(ts, ", ");
        kcdpg_add_str(ts, rec_name); kstr_append_cstr(ts, ", ");
        kcdpg_add_str(ts, rec_email); kstr_append_cstr(ts, ")");
        kcdpg_exec_query(ts->data);
    }
    
    /* Format the reply. */
    wb->res_type = KANP_RES_KWS_UURL;
    kstr_sf(ts, "https://%s/teambox/public/"PRINTF_64"u/"PRINTF_64"u", st.web_host.data, wb->kws_id, st.email_id);
    anp_write_kstr(&wb->res_buf, ts);
    if (wb->cmd_minor >= 3) anp_write_uint64(&wb->res_buf, wb->date);
    if (wb->cmd_minor >= 6) anp_write_uint64(&wb->res_buf, st.email_id);

KCDPG_QUERY_END(cmd_kws_get_uurl)


/* Request workspace creation in a public workspace (workspace-bound query):
 *   UINT32 Version 2 compatibility (1 means yes, 0 means no).
 *   UINT32 Request ID (if compatible with version 2).
 *   UINT64 Request ID (if not compatible with version 2).
 *   STR    Subject.
 */
KCDPG_QUERY_STRUCT(pb_request_workspace)
    uint32_t compat_v2;
    uint32_t req_id_32;
    uint64_t req_id_64;
    kstr subject;

KCDPG_QUERY_INIT(pb_request_workspace, 1)
    kstr_init(&self->subject);

KCDPG_QUERY_CLEAN(pb_request_workspace)
    kstr_clean(&self->subject);

KCDPG_QUERY_START(pb_request_workspace)
    
    /* Retrieve the arguments. */
    if (anp_read_uint32(&st.arg_buf, &st.compat_v2) ||
        (st.compat_v2 && anp_read_uint32(&st.arg_buf, &st.req_id_32)) ||
        (!st.compat_v2 && anp_read_uint64(&st.arg_buf, &st.req_id_64)) ||
        anp_read_kstr(&st.arg_buf, &st.subject)) {
        elog(ERROR, "bad pb_request_workspace argument: %s", kmod_strerror());
        break;
    }

    /* Validate the context. */
    if (kcdpg_perm_check_kws_bound(ts, wb, 0, 1, 1) ||
        kcdpg_perm_check_kws_public(wb->kws_flags)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* Post an event to publish the request. */
    anp_write_uint64(&st.evt_buf, wb->kws_id);
    anp_write_uint64(&st.evt_buf, wb->date);
    if (st.compat_v2) anp_write_uint32(&st.evt_buf, st.req_id_32);
    else anp_write_uint64(&st.evt_buf, st.req_id_64);
    anp_write_uint32(&st.evt_buf, wb->user_id);
    anp_write_kstr(&st.evt_buf, &st.subject);
    kcdpg_post_event_internal(ts, wb->kws_id, st.compat_v2 ? 2 : 3, KANP_EVT_PB_TRIGGER_KWS, &st.evt_buf);

KCDPG_QUERY_END(pb_request_workspace)


/* Request chat in a public workspace (workspace-bound query):
 *   UINT32 Version 2 compatibility (1 means yes, 0 means no).
 *   UINT32 Request ID (if compatible with version 2).
 *   UINT64 Request ID (if not compatible with version 2).
 *   UINT32 Timeout (in seconds).
 *   STR    Subject.
 */
KCDPG_QUERY_STRUCT(pb_request_chat)
    uint32_t compat_v2;
    uint32_t req_id_32;
    uint64_t req_id_64;
    uint32_t timeout;
    kstr subject;

KCDPG_QUERY_INIT(pb_request_chat, 1)
    kstr_init(&self->subject);

KCDPG_QUERY_CLEAN(pb_request_chat)
    kstr_clean(&self->subject);

KCDPG_QUERY_START(pb_request_chat)
    
    /* Retrieve the arguments. */
    if (anp_read_uint32(&st.arg_buf, &st.compat_v2) ||
        (st.compat_v2 && anp_read_uint32(&st.arg_buf, &st.req_id_32)) ||
        (!st.compat_v2 && anp_read_uint64(&st.arg_buf, &st.req_id_64)) ||
        anp_read_uint32(&st.arg_buf, &st.timeout) ||
        anp_read_kstr(&st.arg_buf, &st.subject)) {
        elog(ERROR, "bad pb_request_chat argument: %s", kmod_strerror());
    }

    /* Validate the context. */
    if (kcdpg_perm_check_kws_bound(ts, wb, 0, 1, 1) ||
        kcdpg_perm_check_kws_public(wb->kws_flags)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* Post an event to publish the request. */
    anp_write_uint64(&st.evt_buf, wb->kws_id);
    anp_write_uint64(&st.evt_buf, wb->date);
    if (st.compat_v2) anp_write_uint32(&st.evt_buf, st.req_id_32);
    else anp_write_uint64(&st.evt_buf, st.req_id_64);
    anp_write_uint32(&st.evt_buf, wb->user_id);
    anp_write_kstr(&st.evt_buf, &st.subject);
    anp_write_uint32(&st.evt_buf, st.timeout);
    kcdpg_post_event_internal(ts, wb->kws_id, st.compat_v2 ? 2 : 3, KANP_EVT_PB_TRIGGER_CHAT, &st.evt_buf);

KCDPG_QUERY_END(pb_request_chat)


/* Accept a requested chat in a public workspace (workspace-bound query). */
KCDPG_QUERY_STRUCT(cmd_pb_accept_chat)
    uint32_t req_id_32;
    uint64_t req_id_64;
    uint32_t user_id;
    uint32_t channel_id;

KCDPG_QUERY_INIT(cmd_pb_accept_chat, 1)
KCDPG_QUERY_CLEAN(cmd_pb_accept_chat)

KCDPG_QUERY_START(cmd_pb_accept_chat)
    uint64_t u;
    
    /* Parse the command arguments. */
    if (anp_read_uint64(&wb->cmd_buf, &u) ||
        (wb->cmd_minor <= 2 && anp_read_uint32(&wb->cmd_buf, &st.req_id_32)) ||
        (wb->cmd_minor >= 3 && anp_read_uint64(&wb->cmd_buf, &st.req_id_64)) ||
        anp_read_uint32(&wb->cmd_buf, &st.user_id) ||
        anp_read_uint32(&wb->cmd_buf, &st.channel_id)) {
        error = -1;
        break;
    }

    /* Validate the context. */
    if (kcdpg_perm_check_kws_bound(ts, wb, 1, 1, 1) ||
        kcdpg_perm_check_kws_public(wb->kws_flags)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* Post an event to publish the acceptation. */
    anp_write_uint64(&st.evt_buf, wb->kws_id);
    anp_write_uint64(&st.evt_buf, wb->date);
    if (wb->cmd_minor <= 2) anp_write_uint32(&st.evt_buf, st.req_id_32);
    else anp_write_uint64(&st.evt_buf, st.req_id_64);
    anp_write_uint32(&st.evt_buf, st.user_id);
    anp_write_uint32(&st.evt_buf, st.channel_id);
    kcdpg_post_event_internal(ts, wb->kws_id, wb->cmd_minor <= 2 ? 2 : 3, KANP_EVT_PB_CHAT_ACCEPTED, &st.evt_buf);

KCDPG_QUERY_END(cmd_pb_accept_chat)


/* Phase one operation to perform. */
struct kcdpg_phase_one_op {
    
    /* Operation arguments. */
    void *arg;
    
    /* Operation handler. */
    int (*handler)(void *st, void *arg);
    
    /* Argument clean-up function. */
    void (*cleaner)(void *arg);
};

static struct kcdpg_phase_one_op* kcdpg_phase_one_op_new() {
    struct kcdpg_phase_one_op *self = kcalloc(sizeof(struct kcdpg_phase_one_op));
    return self;
}

static void kcdpg_phase_one_op_destroy(struct kcdpg_phase_one_op *self) {
    if (self) {
        self->cleaner(self->arg);
        kfree(self);
    }
}

/* Phase one operation arguments. */
struct kcdpg_phase_one_create_arg {
    uint32_t op;
    uint64_t parent_inode, parent_commit_id;
    kstr entry_path;
};

static struct kcdpg_phase_one_create_arg* kcdpg_phase_one_create_arg_new(uint32_t op) {
    struct kcdpg_phase_one_create_arg *self = kcalloc(sizeof(struct kcdpg_phase_one_create_arg));
    self->op = op;
    kstr_init(&self->entry_path);
    return self;
}

static void kcdpg_phase_one_create_arg_destroy(struct kcdpg_phase_one_create_arg *self) {
    if (self) {
        kstr_clean(&self->entry_path);
        kfree(self);
    }
}

struct kcdpg_phase_one_update_arg {
    uint64_t inode, commit_id;
};

static struct kcdpg_phase_one_update_arg* kcdpg_phase_one_update_arg_new() {
    struct kcdpg_phase_one_update_arg *self = kcalloc(sizeof(struct kcdpg_phase_one_update_arg));
    return self;
}

static void kcdpg_phase_one_update_arg_destroy(struct kcdpg_phase_one_update_arg *self) {
    if (self) {
        kfree(self);
    }
}
    
struct kcdpg_phase_one_delete_arg {
    uint32_t op;
    uint64_t inode, commit_id;
};

static struct kcdpg_phase_one_delete_arg* kcdpg_phase_one_delete_arg_new(uint32_t op) {
    struct kcdpg_phase_one_delete_arg *self = kcalloc(sizeof(struct kcdpg_phase_one_delete_arg));
    self->op = op;
    return self;
}

static void kcdpg_phase_one_delete_arg_destroy(struct kcdpg_phase_one_delete_arg *self) {
    if (self) {
        kfree(self);
    }
}

struct kcdpg_phase_one_move_arg {
    uint32_t op;
    uint64_t move_inode, move_commit_id, parent_inode, parent_commit_id;
    kstr entry_path;
};

static struct kcdpg_phase_one_move_arg* kcdpg_phase_one_move_arg_new(uint32_t op) {
    struct kcdpg_phase_one_move_arg *self = kcalloc(sizeof(struct kcdpg_phase_one_move_arg));
    self->op = op;
    kstr_init(&self->entry_path);
    return self;
}

static void kcdpg_phase_one_move_arg_destroy(struct kcdpg_phase_one_move_arg *self) {
    if (self) {
        kstr_clean(&self->entry_path);
        kfree(self);
    }
}

/* Handle KFS upload phase 1 (workspace-bound query):
 *   UINT32 Share ID.
 *
 * Output:
 *   UINT64 Commit ID.
 *   UINT64 Public email ID.
 *   UINT32 Number of files to upload.
 *     UINT32 Create flag.
 *     UINT64 Inode.
 *     STR    Path in KFS share.
 *     STR    Permanent path on storage filesystem.
 *   UINT32 Number of files to permanently delete.
 *     STR    Permanent path on storage filesystem.
 */
KCDPG_QUERY_STRUCT(upload_phase_one)

    /* KFS share ID. */
    uint32_t share_id;
    
    /* Public email ID related to the upload. */
    uint64_t public_email_id;
    
    /* Commit ID used for this transaction. */
    uint64_t local_commit_id;
    
    /* Number of changes requested. */
    uint32_t nb_change_req;
    
    /* Number of changes accepted. */
    uint32_t nb_change_ok;
    
    /* Number of files to upload. */
    uint32_t nb_upload;
    
    /* Number of files to delete permanently. */
    uint32_t nb_perm_delete;
    
    /* Size of the files permanently deleted. */
    uint64_t perm_delete_size;
    
    /* Buffer that contains the changes to put in the event buffer. */
    kbuffer evt_change_buf;
    
    /* Buffer that contains the information about the files to upload. */
    kbuffer upload_buf;
    
    /* Buffer that contains the information about the files to delete
     * permanently.
     */
    kbuffer perm_delete_buf;
    
    /* Entry path in create op / move op. */
    kstr entry_path;
    
    /* Entry name in create op / move op. */
    kstr entry_name;
    
    /* Share path to the file or directory. */
    kstr share_path;
    
    /* Permanent path to the file. */
    kstr perm_path;
    
    /* Array of entry path components in create op / move op. */
    karray components;
    
    /* Array of operations to perform. */
    karray op_array;

KCDPG_QUERY_INIT(upload_phase_one, 1)
    kbuffer_init(&self->evt_change_buf);
    kbuffer_init(&self->upload_buf);
    kbuffer_init(&self->perm_delete_buf);
    kstr_init(&self->entry_path);
    kstr_init(&self->entry_name);
    kstr_init(&self->share_path);
    kstr_init(&self->perm_path);
    karray_init(&self->components);
    karray_init(&self->op_array);

KCDPG_QUERY_CLEAN(upload_phase_one)
    int i;
    kbuffer_clean(&self->evt_change_buf);
    kbuffer_clean(&self->upload_buf);
    kbuffer_clean(&self->perm_delete_buf);
    kstr_clean(&self->entry_path);
    kstr_clean(&self->entry_name);
    kstr_clean(&self->share_path);
    kstr_clean(&self->perm_path);
    karray_clear_kstr(&self->components);
    karray_clean(&self->components);
    for (i = 0; i < self->op_array.size; i++) kcdpg_phase_one_op_destroy(self->op_array.data[i]);
    karray_clean(&self->op_array);
    
KCDPG_QUERY_STATIC

/* Set the KMOD error string to the string specified and return -3. */
static int kcdpg_phase_one_reject_op(char *what) {
    kmod_set_error("%s", what);
    return -3;
}

/* This function handles a file upload in phase 1. */
static void kcdpg_phase_one_handle_upload(struct kcdpg_upload_phase_one_state *st, int create_flag,
                                          uint64_t cur_parent, uint64_t new_inode) {
    kstr *ts = &st->ts;
    
    KCDPG_DEBUG("kcdpg_phase_one_handle_upload() called");
    
    kstr_reset(&st->share_path);
    kstr_reset(&st->perm_path);
    
    /* Get the share and permanent path. */
    while (cur_parent) {
        kstr_sf(ts, "SELECT parent_inode, entry_name FROM kcd_kws_kfs_current_view "
                    "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u",
                    st->wb->kws_id, st->share_id, cur_parent);
        kcdpg_exec_query(ts->data);
        if (SPI_processed != 1) elog(ERROR, "permanent path inode not found");
        kstr_sf(ts, "%s/%s", kcdpg_row_val(0, 1), st->share_path.data);
        kstr_assign_kstr(&st->share_path, ts);
        kstr_sf(ts, "d.%s/%s", kcdpg_row_val(0, 1), st->perm_path.data);
        kstr_assign_kstr(&st->perm_path, ts);
        cur_parent = kcdpg_get_uint64(0, 0);
    }
    
    kstr_append_kstr(&st->share_path, &st->entry_name);
    kstr_append_sf(&st->perm_path, "f.%s."PRINTF_64"u", st->entry_name.data, st->local_commit_id);
    
    /* Convert the permanent path to UTF8 to avoid troubles with accented
     * characters.
     */
    kutil_latin1_to_utf8(&st->perm_path);
    
    /* Insert the permanent path in the file map table. */
    kstr_sf(ts, "INSERT INTO kcd_kws_kfs_file_map (kws_id, share_id, inode, commit_id, size, path) "
                "VALUES ("PRINTF_64"u, %u, "PRINTF_64"u, "PRINTF_64"u, 0, ",
                st->wb->kws_id, st->share_id, new_inode, st->local_commit_id);
    kcdpg_add_str(ts, &st->perm_path);
    kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    
    /* Write the inode number and the paths in the upload buffer. */
    st->nb_upload++;
    anp_write_uint32(&st->upload_buf, create_flag);
    anp_write_uint64(&st->upload_buf, new_inode);
    anp_write_kstr(&st->upload_buf, &st->share_path);
    anp_write_kstr(&st->upload_buf, &st->perm_path);
}

/* Handle the permanent deletion of the files updated or deleted as required. */
static void kcdpg_phase_one_handle_perm_delete(struct kcdpg_upload_phase_one_state *st, uint64_t inode,
                                               uint64_t commit_id) {
    kstr *ts = &st->ts;
    
    /* Permanent deletion is disabled. */
    if (!kcdpg_is_kws_thin_kfs(st->wb->kws_flags)) return;
    
    /* Get the size and path of the file. */
    kstr_sf(ts, "SELECT size, path FROM kcd_kws_kfs_file_map WHERE kws_id = "PRINTF_64"u AND "
                "share_id = %u AND inode = "PRINTF_64"u AND commit_id = "PRINTF_64"u",
                st->wb->kws_id, st->share_id, inode, commit_id);
    kcdpg_exec_query(ts->data);
    if (SPI_processed != 1) { elog(ERROR, "file path not found"); }
    
    /* Update the delete information. */
    st->nb_perm_delete++;
    st->perm_delete_size += kcdpg_get_uint32(0, 0);
    kcdpg_get_str(0, 1, ts);
    anp_write_kstr(&st->perm_delete_buf, ts);
    
    /* Remove the entry from the file map table. */
    kstr_sf(ts, "DELETE FROM kcd_kws_kfs_file_map WHERE kws_id = "PRINTF_64"u AND "
                "share_id = %u AND inode = "PRINTF_64"u AND commit_id = "PRINTF_64"u",
                st->wb->kws_id, st->share_id, inode, commit_id);
    kcdpg_exec_query(ts->data);
}

/* This function validates the entry path provided by the user in a create or
 * move operation and updates the current parent.
 */
static int kcdpg_phase_one_validate_entry_path(struct kcdpg_upload_phase_one_state *st, uint64_t *cur_parent,
                                               int move_flag) {
    ssize_t i;
    uint32_t row_inode_type;
    uint64_t row_inode;
    kstr *ts = &st->ts;
    
    KCDPG_DEBUG("kcdpg_phase_one_validate_entry_path() called");
    
    karray_clear_kstr(&st->components);
    
    /* Add a '/' to the path to make it absolute. */
    kstr_sf(ts, "/%s", st->entry_path.data);
    
    /* Split the path into components. */
    kcd_kfs_split_path(ts, &st->components, &st->entry_name);
    
    /* Validate the components. */
    for (i = 0; i < st->components.size; i++) {
        kstr *path = st->components.data[i];
        
        /* Locate the current entry. */
        kstr_sf(ts, "SELECT inode, inode_type FROM kcd_kws_kfs_current_view "
                    "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND parent_inode = "PRINTF_64"u "
                    "AND entry_name = ", st->wb->kws_id, st->share_id, *cur_parent);
        kcdpg_add_str(ts, path);
        kcdpg_exec_query(ts->data);
        if (SPI_processed != 1) return kcdpg_phase_one_reject_op("intermediate inode not found");
        row_inode = kcdpg_get_uint64(0, 0);
        row_inode_type = kcdpg_get_uint32(0, 1);
        if (row_inode_type != KANP_KFS_ENTRY_DIR)
            return kcdpg_phase_one_reject_op("intermediate inode is not a directory");
        *cur_parent = row_inode;
    }
    
    /* Verify if the entry name already exists. */
    kstr_sf(ts, "SELECT entry_name FROM kcd_kws_kfs_current_view "
                "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND parent_inode = "PRINTF_64"u "
                "AND lower(entry_name) = lower(", st->wb->kws_id, st->share_id, *cur_parent);
    kcdpg_add_str(ts, &st->entry_name);
    kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    if (SPI_processed) {
        kcdpg_get_str(0, 0, ts);
        if (!move_flag || strcasecmp(ts->data, st->entry_name.data))
            return kcdpg_phase_one_reject_op("destination exists");
    }
    
    return 0;
}

/* Validate the command commit ID specified. Reject the operation and return -3
 * on error.
 */
static int kcdpg_phase_one_validate_commit_id(struct kcdpg_upload_phase_one_state *st, uint32_t cmd_commit_id,
                                              uint32_t actual_commit_id, char *what) {
    if (cmd_commit_id != actual_commit_id) return kcdpg_phase_one_reject_op(what);
    if (cmd_commit_id == st->local_commit_id) return kcdpg_phase_one_reject_op("improper use of local commit ID");
    return 0;
}

/* This function handles a create operation in phase 1. */
static int kcdpg_phase_one_op_create(struct kcdpg_upload_phase_one_state *st, struct kcdpg_phase_one_create_arg *a) {
    uint32_t new_inode_type = (a->op == KANP_KFS_OP_CREATE_FILE ? KANP_KFS_ENTRY_FILE : KANP_KFS_ENTRY_DIR); 
    uint32_t row_inode_type;
    uint64_t row_commit_id, cur_parent, new_inode;
    kstr *ts = &st->ts;
    struct kcdpg_kws_bound_state *wb = st->wb;
    
    KCDPG_DEBUG("kcdpg_phase_one_op_create() called");
    
    /* Set the entry path. */
    kstr_assign_kstr(&st->entry_path, &a->entry_path);
    
    /* Validate the parent inode. */
    kstr_sf(ts, "SELECT inode_type, commit_id FROM kcd_kws_kfs_current_view "
                "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u",
                wb->kws_id, st->share_id, a->parent_inode);
    kcdpg_exec_query(ts->data);
    if (SPI_processed != 1) return kcdpg_phase_one_reject_op("parent inode not found");
    row_inode_type = kcdpg_get_uint32(0, 0);
    row_commit_id = kcdpg_get_uint64(0, 1);
    if (row_inode_type != KANP_KFS_ENTRY_DIR) return kcdpg_phase_one_reject_op("parent inode is not a directory");
    if (kcdpg_phase_one_validate_commit_id(st, a->parent_commit_id, row_commit_id, "parent inode is stale")) return -3;
    
    /* Validate the entry path. */
    cur_parent = a->parent_inode;
    if (kcdpg_phase_one_validate_entry_path(st, &cur_parent, 0)) return -3;
    
    /* Accept the change. */
    st->nb_change_ok++;
    
    /* Obtain a new inode. */
    new_inode = kcdpg_get_next_seq_id(ts, wb->kws_id, "kfs_inode");
    
    /* Insert the inode in the table. */
    kstr_sf(ts, "INSERT INTO kcd_kws_kfs_current_view "
                "(kws_id, share_id, inode, parent_inode, commit_id, inode_type, entry_name, email_id) "
                "VALUES ("PRINTF_64"u, %u, "PRINTF_64"u, "PRINTF_64"u, "PRINTF_64"u, %u, ",
                wb->kws_id, st->share_id, new_inode, cur_parent, st->local_commit_id, new_inode_type);
    kcdpg_add_str(ts, &st->entry_name); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, st->public_email_id); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    
    /* Write the event data. */
    anp_write_uint32(&st->evt_change_buf, 5);
    anp_write_uint32(&st->evt_change_buf, a->op);
    anp_write_uint64(&st->evt_change_buf, new_inode);
    anp_write_uint64(&st->evt_change_buf, cur_parent);
    anp_write_kstr(&st->evt_change_buf, &st->entry_name);
    
    /* Handle file upload. */
    if (a->op == KANP_KFS_OP_CREATE_FILE) kcdpg_phase_one_handle_upload(st, 1, cur_parent, new_inode);

    return 0;
}

/* This function handles an update operation in phase 1. */
static int kcdpg_phase_one_op_update(struct kcdpg_upload_phase_one_state *st, struct kcdpg_phase_one_update_arg *a) {
    uint32_t row_inode_type;
    uint64_t row_commit_id, row_parent_inode;
    kstr *ts = &st->ts;
    struct kcdpg_kws_bound_state *wb = st->wb;
    
    KCDPG_DEBUG("kcdpg_phase_one_op_update() called");
    
    /* Validate the inode. */
    kstr_sf(ts, "SELECT inode_type, commit_id, parent_inode, entry_name FROM kcd_kws_kfs_current_view "
                "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u",
                wb->kws_id, st->share_id, a->inode);
    kcdpg_exec_query(ts->data);
    if (SPI_processed != 1) return kcdpg_phase_one_reject_op("inode not found");
    row_inode_type = kcdpg_get_uint32(0, 0);
    row_commit_id = kcdpg_get_uint64(0, 1);
    row_parent_inode = kcdpg_get_uint64(0, 2);
    kcdpg_get_str(0, 3, &st->entry_name);
    if (row_inode_type != KANP_KFS_ENTRY_FILE) return kcdpg_phase_one_reject_op("inode is not a file");
    if (kcdpg_phase_one_validate_commit_id(st, a->commit_id, row_commit_id, "inode is stale")) return -3;
    
    /* Accept the change. */
    st->nb_change_ok++;
    
    /* Permanetly delete the file being updated if required. */
    kcdpg_phase_one_handle_perm_delete(st, a->inode, a->commit_id);
    
    /* Update the entry in the table. */
    kstr_sf(ts, "UPDATE kcd_kws_kfs_current_view SET commit_id = "PRINTF_64"u "
                "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u",
                st->local_commit_id, wb->kws_id, st->share_id, a->inode);
    kcdpg_exec_query(ts->data);
   
    /* Write the event data. */
    anp_write_uint32(&st->evt_change_buf, 3);
    anp_write_uint32(&st->evt_change_buf, KANP_KFS_OP_UPDATE_FILE);
    anp_write_uint64(&st->evt_change_buf, a->inode);
    
    /* Handle file upload. */
    kcdpg_phase_one_handle_upload(st, 0, row_parent_inode, a->inode);
    
    return 0;
}

/* This function handles a delete operation in phase 1. */
static int kcdpg_phase_one_op_delete(struct kcdpg_upload_phase_one_state *st, struct kcdpg_phase_one_delete_arg *a) {
    uint32_t op_inode_type = (a->op == KANP_KFS_OP_DELETE_FILE ? KANP_KFS_ENTRY_FILE : KANP_KFS_ENTRY_DIR);
    uint32_t row_inode_type;
    uint64_t row_commit_id;
    kstr *ts = &st->ts;
    struct kcdpg_kws_bound_state *wb = st->wb;
    
    KCDPG_DEBUG("kcdpg_phase_one_op_delete() called");
    
    /* Refuse to delete the root. */
    if (!a->inode) return kcdpg_phase_one_reject_op("cannot delete root directory");
    
    /* Validate the inode. */
    kstr_sf(ts, "SELECT inode_type, commit_id FROM kcd_kws_kfs_current_view "
                "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u",
                wb->kws_id, st->share_id, a->inode);
    kcdpg_exec_query(ts->data);
    if (SPI_processed != 1) return kcdpg_phase_one_reject_op("inode not found");
    row_inode_type = kcdpg_get_uint32(0, 0);
    row_commit_id = kcdpg_get_uint64(0, 1);
    if (row_inode_type != op_inode_type) return kcdpg_phase_one_reject_op("inode is not of expected type");
    if (kcdpg_phase_one_validate_commit_id(st, a->commit_id, row_commit_id, "inode is stale")) return -3;
    
    /* If we're deleting a directory, validate that it is empty. */
    if (a->op == KANP_KFS_OP_DELETE_DIR) {
        kstr_sf(ts, "SELECT inode_type FROM kcd_kws_kfs_current_view "
                    "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND parent_inode = "PRINTF_64"u",
                    wb->kws_id, st->share_id, a->inode);
        kcdpg_exec_query(ts->data);
        if (SPI_processed) return kcdpg_phase_one_reject_op("directory is not empty");
    }
    
    /* Accept the change. */
    st->nb_change_ok++;
    
    /* Permanetly delete the file if required. */
    if (op_inode_type == KANP_KFS_ENTRY_FILE) kcdpg_phase_one_handle_perm_delete(st, a->inode, a->commit_id);
    
    /* Update the entry in the table. */
    kstr_sf(ts, "DELETE FROM kcd_kws_kfs_current_view WHERE kws_id = "PRINTF_64"u AND share_id = %u AND "
                "inode = "PRINTF_64"u", wb->kws_id, st->share_id, a->inode);
    kcdpg_exec_query(ts->data);
   
    /* Write the event data. */
    anp_write_uint32(&st->evt_change_buf, 3);
    anp_write_uint32(&st->evt_change_buf, a->op);
    anp_write_uint64(&st->evt_change_buf, a->inode);
    
    return 0;
}

/* This function handles a move operation in phase 1. */
static int kcdpg_phase_one_op_move(struct kcdpg_upload_phase_one_state *st, struct kcdpg_phase_one_move_arg *a) {
    uint32_t op_inode_type = (a->op == KANP_KFS_OP_MOVE_FILE ? KANP_KFS_ENTRY_FILE : KANP_KFS_ENTRY_DIR);
    uint32_t row_inode_type;
    uint64_t row_commit_id, cur_parent;
    kstr *ts = &st->ts;
    struct kcdpg_kws_bound_state *wb = st->wb;
    
    KCDPG_DEBUG("kcdpg_phase_one_op_move() called");
    
    /* Set the entry path. */
    kstr_assign_kstr(&st->entry_path, &a->entry_path);
    
    /* Refuse to move the root. */
    if (!a->move_inode) return kcdpg_phase_one_reject_op("cannot move root directory");
    
    /* Validate the move inode. */
    kstr_sf(ts, "SELECT inode_type, commit_id FROM kcd_kws_kfs_current_view "
                "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u",
                wb->kws_id, st->share_id, a->move_inode);
    kcdpg_exec_query(ts->data);
    if (SPI_processed != 1) return kcdpg_phase_one_reject_op("move inode not found");
    row_inode_type = kcdpg_get_uint32(0, 0);
    row_commit_id = kcdpg_get_uint64(0, 1);
    if (row_inode_type != op_inode_type) return kcdpg_phase_one_reject_op("move inode is not of expected type");
    if (kcdpg_phase_one_validate_commit_id(st, a->move_commit_id, row_commit_id, "move inode is stale")) return -3;
    
    /* Validate the parent inode. */
    kstr_sf(ts, "SELECT inode_type, commit_id FROM kcd_kws_kfs_current_view "
                "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u",
                wb->kws_id, st->share_id, a->parent_inode);
    kcdpg_exec_query(ts->data);
    if (SPI_processed != 1) return kcdpg_phase_one_reject_op("parent inode not found");
    row_inode_type = kcdpg_get_uint32(0, 0);
    row_commit_id = kcdpg_get_uint64(0, 1);
    if (row_inode_type != KANP_KFS_ENTRY_DIR) return kcdpg_phase_one_reject_op("parent inode is not a directory");
    if (kcdpg_phase_one_validate_commit_id(st, a->parent_commit_id, row_commit_id, "parent inode is stale")) return -3;
    
    /* Validate the entry path. */
    cur_parent = a->parent_inode;
    if (kcdpg_phase_one_validate_entry_path(st, &cur_parent, 1)) return -3;
    
    /* Make sure we aren't making a directory a child of itself. */
    if (a->op == KANP_KFS_OP_MOVE_DIR) {
        uint64_t cur_dir = cur_parent;
        
        while (cur_dir) {
            if (cur_dir == a->move_inode) return kcdpg_phase_one_reject_op("making directory child of itself");
            kstr_sf(ts, "SELECT parent_inode FROM kcd_kws_kfs_current_view "
                        "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u",
                        wb->kws_id, st->share_id, cur_dir);
            kcdpg_exec_query(ts->data);
            if (SPI_processed != 1) return kcdpg_phase_one_reject_op("directory child check: parent inode not found");
            cur_dir = kcdpg_get_uint64(0, 0);
        }
    }
    
    /* Accept the change. */
    st->nb_change_ok++;
    
    /* Update the entry in the table. */
    kstr_sf(ts, "UPDATE kcd_kws_kfs_current_view SET parent_inode = "PRINTF_64"u, entry_name = ", cur_parent);
    kcdpg_add_str(ts, &st->entry_name);
    kstr_append_sf(ts, " WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u", 
                   wb->kws_id, st->share_id, a->move_inode);
    kcdpg_exec_query(ts->data);
   
    /* Write the event data. */
    anp_write_uint32(&st->evt_change_buf, 5);
    anp_write_uint32(&st->evt_change_buf, a->op);
    anp_write_uint64(&st->evt_change_buf, a->move_inode);
    anp_write_uint64(&st->evt_change_buf, cur_parent);
    anp_write_kstr(&st->evt_change_buf, &st->entry_name);

    return 0;
}

/* Add a phase one operation to perform. */
static void kcdpg_phase_one_add_op(struct kcdpg_upload_phase_one_state *st, void *arg, void *handler, void *cleaner) {
    struct kcdpg_phase_one_op *op = kcdpg_phase_one_op_new();
    op->arg = arg;
    op->handler = handler;
    op->cleaner = cleaner;
    karray_push(&st->op_array, op);
}
    
/* Parse the command arguments. */
static int kcdpg_phase_one_parse_cmd(struct kcdpg_upload_phase_one_state *st) {
    uint32_t i;
    struct kcdpg_kws_bound_state *wb = st->wb;
    kbuffer *buf = &wb->cmd_buf;
    
    if (anp_read_bin(buf, &st->tb) ||
        (wb->cmd_minor >= 3 && anp_read_uint64(buf, &st->public_email_id)) ||
        anp_read_uint32(buf, &st->nb_change_req)) return -1;
    
    for (i = 0; i < st->nb_change_req; i++) {
        uint32_t op, ignored;
        
        if (anp_read_uint32(buf, &ignored) ||
            anp_read_uint32(buf, &op)) return -1;
        
        if (op == KANP_KFS_OP_CREATE_FILE || op == KANP_KFS_OP_CREATE_DIR) {
            struct kcdpg_phase_one_create_arg *a = kcdpg_phase_one_create_arg_new(op);
            kcdpg_phase_one_add_op(st, a, kcdpg_phase_one_op_create, kcdpg_phase_one_create_arg_destroy);
            if (anp_read_uint64(buf, &a->parent_inode) ||
                anp_read_uint64(buf, &a->parent_commit_id) ||
                anp_read_kstr(buf, &a->entry_path) ||
                !kcd_kfs_is_path_valid(&a->entry_path)) return -1;
        }
        
        else if (op == KANP_KFS_OP_UPDATE_FILE) {
            struct kcdpg_phase_one_update_arg *a = kcdpg_phase_one_update_arg_new();
            kcdpg_phase_one_add_op(st, a, kcdpg_phase_one_op_update, kcdpg_phase_one_update_arg_destroy);
            if (anp_read_uint64(buf, &a->inode) ||
                anp_read_uint64(buf, &a->commit_id)) return -1;
        }
        
        else if (op == KANP_KFS_OP_DELETE_FILE || op == KANP_KFS_OP_DELETE_DIR) {
            struct kcdpg_phase_one_delete_arg *a = kcdpg_phase_one_delete_arg_new(op);
            kcdpg_phase_one_add_op(st, a, kcdpg_phase_one_op_delete, kcdpg_phase_one_delete_arg_destroy);
            if (anp_read_uint64(buf, &a->inode) ||
                anp_read_uint64(buf, &a->commit_id)) return -1;
        }
        
        else if (op == KANP_KFS_OP_MOVE_FILE || op == KANP_KFS_OP_MOVE_DIR) {
            struct kcdpg_phase_one_move_arg *a = kcdpg_phase_one_move_arg_new(op);
            kcdpg_phase_one_add_op(st, a, kcdpg_phase_one_op_move, kcdpg_phase_one_move_arg_destroy);
            if (anp_read_uint64(buf, &a->move_inode) ||
                anp_read_uint64(buf, &a->move_commit_id) ||
                anp_read_uint64(buf, &a->parent_inode) ||
                anp_read_uint64(buf, &a->parent_commit_id) ||
                anp_read_kstr(buf, &a->entry_path) ||
                !kcd_kfs_is_path_valid(&a->entry_path)) return -1;
        }
        
        else {
            kmod_set_error("invalid operation %u", op);
            return -1;
        }
    }
    
    return  0;

KCDPG_QUERY_START(upload_phase_one)
    uint32_t i;
    
    /* Retrieve the arguments. */
    if (anp_read_uint32(&st.arg_buf, &st.share_id)) {
        elog(ERROR, "bad upload_phase_one argument: %s", kmod_strerror());
    }
    
    /* Read the command arguments. */
    error = kcdpg_phase_one_parse_cmd(&st);
    if (error) break;
    
    /* Lock the KFS application. */
    if (kcpdg_perm_check_kws_bound_kfs(ts, wb, 1, 1, 1)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* Get a new commit ID. */
    st.local_commit_id = kcdpg_get_next_seq_id(ts, wb->kws_id, "kfs_commit");
    
    /* Add the commit ID and the number of changes in the reply buffer. */
    anp_write_uint64(&wb->res_buf, st.local_commit_id);
    anp_write_uint32(&wb->res_buf, st.nb_change_req);
    
    /* Process the changes. In the helper functions, the error code -3 is used
     * to indicate that the operation could not be applied.
     */
    for (i = 0; i < st.nb_change_req; i++) {
        struct kcdpg_phase_one_op *op = st.op_array.data[i];
        
        KCDPG_DEBUG("KFS operation %u.\n", i);
        
        /* Call the operation handler. */
        error = op->handler(&st, op->arg);
        
        /* Write the outcome of the operation in the reply buffer. */
        assert(error == 0 || error == -3);
        
        if (!error) {
            anp_write_uint32(&wb->res_buf, 1);
            anp_write_cstr(&wb->res_buf, "");
        }
        
        else {
            KCDPG_DEBUG("Operation %u rejected: %s", i, kmod_strerror());
            anp_write_uint32(&wb->res_buf, 0);
            anp_write_kstr(&wb->res_buf, kmod_kstrerror());
        }
        
        error = 0;
    }
    
    assert(!error);
    
    /* Update the total file size. */
    if (st.perm_delete_size) {
        kstr_sf(ts, "UPDATE kcd_kws_kfs_limit SET file_size = file_size - "PRINTF_64"u WHERE kws_id = "PRINTF_64"u",
                    st.perm_delete_size, wb->kws_id);
        kcdpg_exec_query(ts->data);
    }
    
    /* Post the phase 1 event. */
    if (st.nb_change_ok) {
        anp_write_uint64(&st.evt_buf, wb->kws_id);
        anp_write_uint64(&st.evt_buf, wb->date);
        anp_write_uint32(&st.evt_buf, wb->user_id);
        anp_write_uint32(&st.evt_buf, st.share_id);
        anp_write_uint64(&st.evt_buf, st.local_commit_id);
        anp_write_uint32(&st.evt_buf, st.nb_change_ok);
        kbuffer_write_buffer(&st.evt_buf, &st.evt_change_buf);
        kcdpg_post_event_internal(ts, wb->kws_id, 1, KANP_EVT_KFS_PHASE_1, &st.evt_buf);
    }
    
    /* We must update the uploader table. */
    if (st.nb_upload) {
        kstr_sf(ts, "INSERT INTO kcd_kws_kfs_upload (kws_id, share_id, user_id, commit_id, timestamp) "
                    "VALUES ("PRINTF_64"u, %u, %u, "PRINTF_64"u, "PRINTF_64"u)",
                    wb->kws_id, st.share_id, wb->user_id, st.local_commit_id, wb->date);
        kcdpg_exec_query(ts->data);
    }
    
    /* Set the result type. */
    wb->res_type = KANP_RES_KFS_PHASE_1;
    
    /* Write the output parameters. */
    anp_write_uint64(&st.ext_buf, st.local_commit_id);
    anp_write_uint64(&st.ext_buf, st.public_email_id);
    anp_write_uint32(&st.ext_buf, st.nb_upload);
    kbuffer_write_buffer(&st.ext_buf, &st.upload_buf);
    anp_write_uint32(&st.ext_buf, st.nb_perm_delete);
    kbuffer_write_buffer(&st.ext_buf, &st.perm_delete_buf);

KCDPG_QUERY_END(upload_phase_one)


/* Handle KFS upload phase 2 (workspace-bound query):
 *   UINT32 Share ID.
 *   UINT64 Commit ID.
 *   UINT32 Public email ID.
 *   BIN    Event buffer.
 *   BIN    Notification buffer.
 *   UINT32 Number of commits.
 *     UINT64 Inode.
 *     UINT64 Size.
 */
KCDPG_QUERY_STRUCT(upload_phase_two)
    
    /* KFS share and user information. */
    uint64_t total_size;
    uint64_t commit_id;
    uint64_t public_email_id;
    uint32_t share_id;
    
    /* Array of uploaded files. */
    karray upload_array;
    
KCDPG_QUERY_INIT(upload_phase_two, 1)
    karray_init(&self->upload_array);
    
KCDPG_QUERY_CLEAN(upload_phase_two)
    uint32_t i;
    for (i = 0; i < (uint32_t) self->upload_array.size; i++) kcd_kfs_uploaded_file_destroy(self->upload_array.data[i]);
    karray_clean(&self->upload_array);
    
KCDPG_QUERY_START(upload_phase_two)
    uint32_t i, nb_file;
    uint64_t evt_id;
    
    /* Retrieve the arguments. */
    if (anp_read_uint32(&st.arg_buf, &st.share_id) ||
        anp_read_uint64(&st.arg_buf, &st.commit_id) ||
        anp_read_uint64(&st.arg_buf, &st.public_email_id) ||
        anp_read_bin(&st.arg_buf, &st.evt_buf) ||
        anp_read_bin(&st.arg_buf, &st.ntf_buf) ||
        anp_read_uint32(&st.arg_buf, &nb_file)) {
        elog(ERROR, "bad upload_phase_two argument: %s", kmod_strerror());
    }
    
    for (i = 0; i < nb_file; i++) {
        struct kcd_kfs_uploaded_file *f = kcd_kfs_uploaded_file_new();
        karray_push(&st.upload_array, f);
        anp_read_uint64(&st.arg_buf, &f->inode);
        anp_read_uint64(&st.arg_buf, &f->size);
        st.total_size += f->size;
    }
    
    /* Lock the KFS application. We skip some permission checks since the event
     * must be posted unconditionally.
     */
    if (kcpdg_perm_check_kws_bound_kfs(ts, wb, 1, 0, 0)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* The uploader no longer exists. Report failure. */
    if (!kcdpg_uploader_exist(ts, wb->kws_id, st.share_id, wb->user_id, st.commit_id, NULL)) {
        kmod_set_error("uploader entry no longer exist");
        error = -1;
        break;
    }
    
    KCDPG_DEBUG("Uploader still exists, removing entry and posting event.");
    
    /* Update the individual file sizes. */
    for (i = 0; i < nb_file; i++) {
        struct kcd_kfs_uploaded_file *f = st.upload_array.data[i];
        kstr_sf(ts, "UPDATE kcd_kws_kfs_file_map SET size = "PRINTF_64"u "
                    "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u",
                    f->size, wb->kws_id, st.share_id, f->inode);
        kcdpg_exec_query(ts->data);
    }
    
    /* Update the total file size. */
    kstr_sf(ts, "UPDATE kcd_kws_kfs_limit SET file_size = file_size + "PRINTF_64"u WHERE kws_id = "PRINTF_64"u",
                st.total_size, wb->kws_id);
    kcdpg_exec_query(ts->data);
    
    /* Remove the entry. */
    kcdpg_remove_uploader(ts, wb->kws_id, st.share_id, wb->user_id, st.commit_id);
    
    /* Post the event and the notification. */
    evt_id = kcdpg_post_event_internal(ts, wb->kws_id, 1, KANP_EVT_KFS_PHASE_2, &st.evt_buf);
    if (nb_file) kcdpg_post_notif(ts, wb->kws_id, evt_id, wb->date, wb->user_id, KANP_EVT_KFS_PHASE_2, &st.ntf_buf);

KCDPG_QUERY_END(upload_phase_two)


/* Refresh a KFS upload entry (workspace-bound query):
 *   UINT32 Share ID.
 *   UINT64 Commit ID.
 */
KCDPG_QUERY_STRUCT(refresh_upload)
    uint64_t commit_id;
    uint32_t share_id;

KCDPG_QUERY_INIT(refresh_upload, 1)
KCDPG_QUERY_CLEAN(refresh_upload)

KCDPG_QUERY_START(refresh_upload)
    
    /* Retrieve the arguments. */
    if (anp_read_uint32(&st.arg_buf, &st.share_id) ||
        anp_read_uint64(&st.arg_buf, &st.commit_id)) {
        elog(ERROR, "bad refresh_upload argument: %s", kmod_strerror());
    }
    
    /* Lock the KFS application. */
    if (kcpdg_perm_check_kws_bound_kfs(ts, wb, 1, 1, 1)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* The uploader no longer exists. Report failure. */
    if (!kcdpg_uploader_exist(ts, wb->kws_id, st.share_id, wb->user_id, st.commit_id, NULL)) {
        kmod_set_error("uploader entry no longer exist");
        error = -1;
        break;
    }
    
    KCDPG_DEBUG("Uploader still exists, refreshing entry.");
    
    /* Refresh the entry. */
    kstr_sf(ts, "UPDATE kcd_kws_kfs_upload SET timestamp = "PRINTF_64"u "
                "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND user_id = %u AND commit_id = "PRINTF_64"u",
                wb->date, wb->kws_id, st.share_id, wb->user_id, st.commit_id);
    kcdpg_exec_query(ts->data);

KCDPG_QUERY_END(refresh_upload)


/* This structure represents an expired uploader entry. */
struct expired_uploader {
    uint64_t kws_id;
    uint32_t share_id;
    uint32_t user_id;
    uint64_t commit_id;
    uint64_t timestamp;
};

/* Purge stale upload entries (safe query). */
KCDPG_QUERY_STRUCT(purge_upload)

    /* Tree containing the list of expired uploaders. The tree is indexed by
     * workspace ID and its values are karrays containing expired_uploader
     * having that workspace ID.
     */
    struct krb_tree expired_tree;
    
KCDPG_QUERY_INIT(purge_upload, 0)
    krb_tree_init_func(&self->expired_tree, kutil_uint64_cmp);

KCDPG_QUERY_CLEAN(purge_upload)
    uint32_t i, j, size; 
    struct krb_node *iter;
    
    iter = krb_tree_iter_start(&self->expired_tree);
    size = krb_tree_size(&self->expired_tree);
        
    for (i = 0; i < size; i++) {
        karray *expired_array = krb_tree_iter_next(&self->expired_tree, &iter);
        for (j = 0; j < (uint32_t) expired_array->size; j++) kfree(expired_array->data[j]);
        karray_destroy(expired_array);
    }
    
    krb_tree_clean(&self->expired_tree);

KCDPG_QUERY_START(purge_upload)
    uint32_t i, j, size;
    uint64_t now = ktime_now_sec(), expired_time;
    struct krb_node *iter, *node;
    karray *expired_array;
    
    /* Compute the expired time. */
    expired_time = now - KCDPG_PURGE_UPLOADER_DELAY;
    
    /* Retrieve the expired uploaders. It is _important_ to retrieve them
     * sorted by workspace ID to impose a total ordering on the locks
     * acquired by Postgres. We're doing everything in a single transaction
     * and this is a situation ripe for deadlocks.
     */
    kstr_sf(ts, "SELECT kws_id, share_id, user_id, commit_id, timestamp FROM kcd_kws_kfs_upload "
                "WHERE timestamp < "PRINTF_64"u ORDER BY kws_id", expired_time);
    kcdpg_exec_query(ts->data);
    
    for (i = 0; i < SPI_processed; i++) {
        struct expired_uploader *e = kmalloc(sizeof(struct expired_uploader));
        e->kws_id = kcdpg_get_uint64(i, 0);
        e->share_id = kcdpg_get_uint32(i, 1);
        e->user_id = kcdpg_get_uint32(i, 2);
        e->commit_id = kcdpg_get_uint64(i, 3);
        e->timestamp = kcdpg_get_uint64(i, 4);
        
        node = krb_tree_get_node(&st.expired_tree, &e->kws_id);
        
        if (node == NULL) {
            expired_array = karray_new();
            krb_tree_add(&st.expired_tree, &e->kws_id, expired_array);
        }
        
        else expired_array = node->value;
        
        karray_push(expired_array, e);
    }
    
    /* Purge the expired uploaders. */
    iter = krb_tree_iter_start(&st.expired_tree);
    size = krb_tree_size(&st.expired_tree);
    
    for (i = 0; i < size; i++) {
        expired_array = krb_tree_iter_next(&st.expired_tree, &iter);
    
        for (j = 0; j < (uint32_t) expired_array->size; j++) {
            struct expired_uploader *e = expired_array->data[j];
            
            /* Lock the KFS application, if possible. */
            if (j == 0) {
                if (!kcdpg_lock_kws(ts, e->kws_id, 0)) break;
                kcdpg_lock_kfs(ts, e->kws_id, 1);
            }
        
            /* The uploader still exists. */
            if (kcdpg_uploader_exist(ts, e->kws_id, e->share_id, e->user_id, e->commit_id, &e->timestamp)) {
                KCDPG_DEBUG("Stale uploader exists, purging entry.");

                /* Remove the entry. */
                kcdpg_remove_uploader(ts, e->kws_id, e->share_id, e->user_id, e->commit_id);

                /* Post the event. */
                kbuffer_reset(&st.evt_buf);
                anp_write_uint64(&st.evt_buf, e->kws_id);
                anp_write_uint64(&st.evt_buf, now);
                anp_write_uint32(&st.evt_buf, e->user_id);
                anp_write_uint32(&st.evt_buf, e->share_id);
                anp_write_uint64(&st.evt_buf, e->commit_id);
                anp_write_uint32(&st.evt_buf, 0);
                kcdpg_post_event_internal(ts, e->kws_id, 1, KANP_EVT_KFS_PHASE_2, &st.evt_buf);
            }
        }
    }

KCDPG_QUERY_END(purge_upload)


/* Purge the attachments in a public workspace (safe query):
 *   UINT64 Workspace ID.
 *   UINT64 Email ID.
 */
KCDPG_QUERY_STRUCT(purge_att)
    uint64_t kws_id;
    uint64_t email_id;

KCDPG_QUERY_INIT(purge_att, 0)
KCDPG_QUERY_CLEAN(purge_att)

KCDPG_QUERY_START(purge_att)
    
    if (anp_read_uint64(&st.arg_buf, &st.kws_id) ||
        anp_read_uint64(&st.arg_buf, &st.email_id)) {
        elog(ERROR, "bad purge_att argument: %s", kmod_strerror());
    }
    
    /* Lock the workspace. */
    if (!kcdpg_lock_kws(ts, st.kws_id, 1)) break;
    
    /* Set the attachment expiration flag. */
    kstr_sf(ts, "UPDATE kcd_kws_pub_email_info SET att_expire_flag = 1 WHERE "
                "kws_id = "PRINTF_64"u AND email_id = "PRINTF_64"u", st.kws_id, st.email_id);
    kcdpg_exec_query(ts->data);
    
KCDPG_QUERY_END(purge_att)


/* Get the path of the files to download (workspace-bound query):
 *   UINT32 Share ID.
 *   UINT32 Number of files.
 *     UINT64 Inode.
 *     UINT64 Commit ID.
 *
 * Output:
 *   Array of paths.
 */
KCDPG_QUERY_STRUCT(download_file)
    uint32_t share_id;
    uint32_t nb_download;
    kbuffer path_buf;

KCDPG_QUERY_INIT(download_file, 1)
    kbuffer_init(&self->path_buf);

KCDPG_QUERY_CLEAN(download_file)
    kbuffer_clean(&self->path_buf);

KCDPG_QUERY_START(download_file)
    uint32_t i;
        
    /* Lock the KFS application. */
    if (kcpdg_perm_check_kws_bound_kfs(ts, wb, 0, 0, 1)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }

    if (anp_read_uint32(&st.arg_buf, &st.share_id) ||
        anp_read_uint32(&st.arg_buf, &st.nb_download)) {
        elog(ERROR, "bad download_file() argument: %s", kmod_strerror());
    }
    
    for (i = 0; i < st.nb_download; i++) {
        uint64_t inode, commit_id;
        
        if (anp_read_uint64(&st.arg_buf, &inode) ||
            anp_read_uint64(&st.arg_buf, &commit_id)) {
            elog(ERROR, "bad download_file() argument: %s", kmod_strerror());
        }
        
        kstr_sf(ts, "SELECT path FROM kcd_kws_kfs_file_map WHERE kws_id = "PRINTF_64"u AND "
                    "share_id = %u AND inode = "PRINTF_64"u AND commit_id = "PRINTF_64"u",
                    wb->kws_id, st.share_id, inode, commit_id);
        kcdpg_exec_query(ts->data);
        
        if (SPI_processed != 1) {
            kmod_set_error("file with inode "PRINTF_64"u and commit ID "PRINTF_64"u does not exist", inode, commit_id);
            error = -1;
            break;
        }
        
        anp_write_cstr(&st.ext_buf, kcdpg_row_val(0, 0));
    }
    
    if (error) break;
     
KCDPG_QUERY_END(download_file)


/* Post a notification when a file is downloaded (workspace-bound query):
 *   UINT32 Share ID.
 *   UINT64 Inode.
 *   UINT64 Commit ID.
 *   UINT64 Public email ID.
 */
KCDPG_QUERY_STRUCT(notify_file_download)
    uint32_t share_id;
    uint64_t inode;
    uint64_t commit_id;
    uint64_t public_email_id;
    kstr share_path;

KCDPG_QUERY_INIT(notify_file_download, 1)
    kstr_init(&self->share_path);
    
KCDPG_QUERY_CLEAN(notify_file_download)
    kstr_clean(&self->share_path);

KCDPG_QUERY_START(notify_file_download)
    uint64_t cur_inode, evt_id;

    if (anp_read_uint32(&st.arg_buf, &st.share_id) ||
        anp_read_uint64(&st.arg_buf, &st.inode) ||
        anp_read_uint64(&st.arg_buf, &st.commit_id) ||
        anp_read_uint64(&st.arg_buf, &st.public_email_id)) {
        elog(ERROR, "bad notify_file_download argument: %s", kmod_strerror());
    }
    
    /* Lock the KFS application. */
    if (kcpdg_perm_check_kws_bound_kfs(ts, wb, 0, 0, 1)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* Check if the file has already been downloaded by this user. */
    kstr_sf(ts, "SELECT user_id FROM kcd_kws_kfs_download "
                "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u "
                "AND commit_id = "PRINTF_64"u AND user_id = %u",
                wb->kws_id, st.share_id, st.inode, st.commit_id, wb->user_id);
    kcdpg_exec_query(ts->data);
    if (SPI_processed) break;
    
    /* Get the path to the file. */
    cur_inode = st.inode;
    while (cur_inode) {
        kstr_sf(ts, "SELECT parent_inode, entry_name FROM kcd_kws_kfs_current_view "
                    "WHERE kws_id = "PRINTF_64"u AND share_id = %u AND inode = "PRINTF_64"u",
                    wb->kws_id, st.share_id, cur_inode);
        kcdpg_exec_query(ts->data);
        if (SPI_processed != 1) break;
        kcdpg_get_str(0, 1, ts);
        if (st.share_path.slen) kstr_append_sf(ts, "/%s", st.share_path.data);
        kstr_assign_kstr(&st.share_path, ts);
        cur_inode = kcdpg_get_uint64(0, 0);
    }
    
    /* Get out if the file is gone. */
    if (!st.share_path.slen) break;
    
    /* Mark the file as downloaded. We don't care if the version no longer
     * exists.
     */
    kstr_sf(ts, "INSERT INTO kcd_kws_kfs_download "
                "(kws_id, share_id, inode, commit_id, user_id) VALUES ( ");
    kcdpg_add_uint64(ts, wb->kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, st.share_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, st.inode); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, st.commit_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, wb->user_id); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    
    /* Post the event and the notification if the workspace is not in
     * compatibility mode.
     */
    if (!kcdpg_is_kws_compatv2(wb->kws_flags)) {
        anp_write_uint64(&st.evt_buf, wb->kws_id);
        anp_write_uint64(&st.evt_buf, wb->date);
        anp_write_uint32(&st.evt_buf, wb->user_id);
        anp_write_uint32(&st.evt_buf, st.share_id);
        anp_write_uint64(&st.evt_buf, st.inode);
        anp_write_uint64(&st.evt_buf, st.commit_id);
        evt_id = kcdpg_post_event_internal(ts, wb->kws_id, 3, KANP_EVT_KFS_DOWNLOAD, &st.evt_buf);
        
        anp_write_uint64(&st.ntf_buf, st.public_email_id);
        anp_write_kstr(&st.ntf_buf, &st.share_path);
        kcdpg_post_notif(ts, wb->kws_id, evt_id, wb->date, wb->user_id, KANP_EVT_KFS_DOWNLOAD, &st.ntf_buf);
    }
    
KCDPG_QUERY_END(notify_file_download)


/* Start a VNC session (workspace-bound query):
 *   STR    Subject.
 *   UINT32 Port.
 *
 * Output:
 *   UINT64 Session ID.
 */
KCDPG_QUERY_STRUCT(start_vnc)
    uint64_t session_id;
    uint32_t port;
    kstr subject;

KCDPG_QUERY_INIT(start_vnc, 1)
    kstr_init(&self->subject);
    
KCDPG_QUERY_CLEAN(start_vnc)
    kstr_clean(&self->subject);

KCDPG_QUERY_START(start_vnc)
    uint64_t evt_id;
    
    /* Get the arguments. */
    if (anp_read_kstr(&st.arg_buf, &st.subject) ||
        anp_read_uint32(&st.arg_buf, &st.port)) {
        elog(ERROR, "bad start_vnc argument: %s", kmod_strerror());
    }
    
    /* Lock and validate. */
    if (kcdpg_perm_check_kws_bound(ts, wb, 1, 1, 1)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    /* Get a session ID. */
    st.session_id = kcdpg_get_next_seq_id(ts, wb->kws_id, "vnc_session");
    
    /* Insert the data in kcd_kws_vnc_session. */
    kstr_sf(ts, "INSERT INTO kcd_kws_vnc_session (kws_id, session_id, port, date) VALUES (");
    kcdpg_add_uint64(ts, wb->kws_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, st.session_id); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint32(ts, st.port); kstr_append_cstr(ts, ", ");
    kcdpg_add_uint64(ts, wb->date); kstr_append_cstr(ts, ")");
    kcdpg_exec_query(ts->data);
    
    /* Post an event announcing that the user is ready to share. */
    anp_write_uint64(&st.evt_buf, wb->kws_id);
    anp_write_uint64(&st.evt_buf, wb->date);
    anp_write_uint32(&st.evt_buf, wb->user_id);
    anp_write_uint64(&st.evt_buf, st.session_id);
    anp_write_kstr(&st.evt_buf, &st.subject);
    evt_id = kcdpg_post_event_internal(ts, wb->kws_id, 2, KANP_EVT_VNC_START, &st.evt_buf);
    
    anp_write_kstr(&st.ntf_buf, &st.subject);
    kcdpg_post_notif(ts, wb->kws_id, evt_id, wb->date, wb->user_id, KANP_EVT_VNC_START, &st.ntf_buf);
    
    /* Write the output parameters. */
    anp_write_uint64(&st.ext_buf, st.session_id);

KCDPG_QUERY_END(start_vnc)


/* End a VNC session (safe query):
 *   UINT64 Workspace ID.
 *   UINT32 User ID.
 *   UINT64 Session ID.
 *   UINT32 Event minor version.
 *   UINT32 Error code.
 *   STR    Error message.
 */
KCDPG_QUERY_STRUCT(end_vnc)
    uint64_t kws_id;
    uint64_t session_id;
    uint32_t user_id;
    uint32_t evt_minor;
    uint32_t error_code;
    kstr error_msg;

KCDPG_QUERY_INIT(end_vnc, 0)
    kstr_init(&self->error_msg);
    
KCDPG_QUERY_CLEAN(end_vnc)
    kstr_clean(&self->error_msg);

KCDPG_QUERY_START(end_vnc)

    /* Retrieve the arguments. */
    if (anp_read_uint64(&st.arg_buf, &st.kws_id) ||
        anp_read_uint32(&st.arg_buf, &st.user_id) ||
        anp_read_uint64(&st.arg_buf, &st.session_id) ||
        anp_read_uint32(&st.arg_buf, &st.evt_minor) ||
        anp_read_uint32(&st.arg_buf, &st.error_code) ||
        anp_read_kstr(&st.arg_buf, &st.error_msg)) {
        elog(ERROR, "bad end_vnc argument: %s", kmod_strerror());
    }
    
    /* Lock the workspace. */
    if (!kcdpg_lock_kws(ts, st.kws_id, 1)) break;
    
    /* Bail out if the session does not exist. */
    kstr_sf(ts, "SELECT session_id FROM kcd_kws_vnc_session WHERE "
                "kws_id = "PRINTF_64"u AND session_id = "PRINTF_64"u", st.kws_id, st.session_id);
    kcdpg_exec_query(ts->data);
    if (!SPI_processed) break;
    
    /* Remove the session ID from the VNC table. */
    kstr_sf(ts, "DELETE FROM kcd_kws_vnc_session WHERE "
                "kws_id = "PRINTF_64"u AND session_id = "PRINTF_64"u", st.kws_id, st.session_id);
    kcdpg_exec_query(ts->data);
    
    /* Post an event announcing that the user is done sharing. */
    anp_write_uint64(&st.evt_buf, st.kws_id);
    anp_write_uint64(&st.evt_buf, ktime_now_sec());
    anp_write_uint32(&st.evt_buf, st.user_id);
    anp_write_uint64(&st.evt_buf, st.session_id);
    if (st.evt_minor > 2) {
        anp_write_uint32(&st.evt_buf, st.error_code);
        anp_write_kstr(&st.evt_buf, &st.error_msg);
    }
    kcdpg_post_event_internal(ts, st.kws_id, st.evt_minor, KANP_EVT_VNC_END, &st.evt_buf);

KCDPG_QUERY_END(end_vnc)


/* Obtain a SKURL request ID (workspace-bound query):
 *
 * Output:
 *   UINT64 Request ID.
 */
KCDPG_QUERY_STRUCT(get_next_skurl_req_id)
    uint64_t req_id;   
    
KCDPG_QUERY_INIT(get_next_skurl_req_id, 1)
KCDPG_QUERY_CLEAN(get_next_skurl_req_id)

KCDPG_QUERY_START(get_next_skurl_req_id)
    
    if (kcdpg_perm_check_kws_bound(ts, wb, 1, 1, 1)) {
        error = kcdpg_handle_kws_bound_perm_error(wb);
        break;
    }
    
    st.req_id = kcdpg_get_next_seq_id(ts, wb->kws_id, "skurl_req_id");
    anp_write_uint64(&st.ext_buf, st.req_id);

KCDPG_QUERY_END(get_next_skurl_req_id)


/* Change the notification policy of the user of a workspace (safe query):
 *   UINT64 Workspace ID.
 *   UINT32 User ID.
 *   UINT32 Notification policy.
 */
KCDPG_QUERY_STRUCT(do_notif_mgt)
    uint64_t kws_id;
    uint32_t user_id;
    uint32_t notif_policy;

KCDPG_QUERY_INIT(do_notif_mgt, 0)
KCDPG_QUERY_CLEAN(do_notif_mgt)

KCDPG_QUERY_START(do_notif_mgt)

    if (anp_read_uint64(&st.arg_buf, &st.kws_id) ||
        anp_read_uint32(&st.arg_buf, &st.user_id) ||
        anp_read_uint32(&st.arg_buf, &st.notif_policy)) {
        elog(ERROR, "bad do_notif_mgt argument: %s", kmod_strerror());
    }
        
    /* Lock the workspace. */
    if (!kcdpg_lock_kws(ts, st.kws_id, 1)) break;
    
    /* Update the policy. */
    kstr_sf(ts, "UPDATE kcd_kws_users SET notif_policy = %u WHERE kws_id = "PRINTF_64"u AND user_id = %u",
                st.notif_policy, st.kws_id, st.user_id);
    kcdpg_exec_query(ts->data);
    
KCDPG_QUERY_END(do_notif_mgt)


/* Purge the notifications of a workspace (safe query):
 *   UINT64 Workspace ID.
 *   UINT64 Last notification ID.
 */
KCDPG_QUERY_STRUCT(purge_notif)
    uint64_t kws_id;
    uint64_t last_notif;

KCDPG_QUERY_INIT(purge_notif, 0)
KCDPG_QUERY_CLEAN(purge_notif)

KCDPG_QUERY_START(purge_notif)
    
    if (anp_read_uint64(&st.arg_buf, &st.kws_id) ||
        anp_read_uint64(&st.arg_buf, &st.last_notif)) {
        elog(ERROR, "bad purge_notif argument: %s", kmod_strerror());
    }
    
    /* Lock the workspace and the event log. */
    if (!kcdpg_lock_kws(ts, st.kws_id, 0)) break;
    kcdpg_lock_evt(ts, st.kws_id);
    
    /* Delete the old notifications. */
    kstr_sf(ts, "DELETE FROM kcd_kws_notif_log WHERE kws_id = "PRINTF_64"u AND notif_id <= "PRINTF_64"u",
                st.kws_id, st.last_notif);
    kcdpg_exec_query(ts->data);
    
KCDPG_QUERY_END(purge_notif)


/* Handle a workspace property change command (workspace-bound query):
 *   UINT32 Command type.
 *
 * Output:
 *   UINT32 True if the KFS files need to be synchronized.
 *   UINT32 New login type.
 */
KCDPG_QUERY_STRUCT(handle_kws_prop_change)
    
    /* ID of the property change event posted. */
    uint64_t evt_id;
    
    /* Type of the property change command being handled. */
    uint32_t cmd_type;
    
    /* Value type of the property being modified:
     * 1: flags.
     * 2: string.
     */
    uint32_t prop_value_type;
    
    /* True if the property being modified is associated to a user. */
    uint32_t kws_user_flag;
    
    /* Target user being modified, if any. */
    uint32_t target_user_id;
    uint32_t target_user_flags;
    
    /* True if the command can be applied to the invoking user. */
    uint32_t target_self_ok_flag;
    
    /* True if the command can be applied to a banned user. */
    uint32_t target_ban_ok_flag;
    
    /* Required privilege level. */
    uint32_t priv_level;
    
    /* True if a security flag is being modified. */
    uint32_t security_flag;
    
    /* True if the KFS files need to be synchronized. */
    uint32_t sync_kfs_flag;
    
    /* Property flag to set/clear. */
    uint32_t prop_value_flag;
    
    /* True if the property flag is to be set. */
    uint32_t prop_value_flags_set_flag;
    
    /* Property string to set. */
    kstr prop_value_str;
    
    /* Property change state. */
    struct kcdpg_kws_prop_change_state *pcs;
    
    /* License and usage information. */
    struct kcd_global_user_usage_info usage_info;
    struct kcd_global_user_license_info license_info;

KCDPG_QUERY_INIT(handle_kws_prop_change, 1)
    kstr_init(&self->prop_value_str);
    kcd_global_user_usage_info_init(&self->usage_info);
    kcd_global_user_license_info_init(&self->license_info);

KCDPG_QUERY_CLEAN(handle_kws_prop_change)
    kstr_clean(&self->prop_value_str);
    kcdpg_kws_prop_change_state_destroy(self->pcs);
    kcd_global_user_usage_info_clean(&self->usage_info);
    kcd_global_user_license_info_clean(&self->license_info);

KCDPG_QUERY_STATIC

/* Translate a command type to a property type flag. */
static uint32_t kcdpg_handle_kws_prop_change_cmd_type_to_flag(uint32_t cmd_type) {
    switch (cmd_type) {
        case KANP_CMD_KWS_SET_USER_ADMIN: return KANP_USER_FLAG_ADMIN;
        case KANP_CMD_KWS_SET_USER_MANAGER: return KANP_USER_FLAG_MANAGER;
        case KANP_CMD_KWS_SET_USER_LOCK: return KANP_USER_FLAG_LOCK;
        case KANP_CMD_KWS_SET_USER_BAN: return KANP_USER_FLAG_BAN;
        case KANP_CMD_KWS_SET_SECURE: return KANP_KWS_FLAG_SECURE;
        case KANP_CMD_KWS_SET_FREEZE: return KANP_KWS_FLAG_FREEZE;
        case KANP_CMD_KWS_SET_DEEP_FREEZE: return KANP_KWS_FLAG_DEEP_FREEZE;
        case KANP_CMD_KWS_SET_THIN_KFS: return KANP_KWS_FLAG_THIN_KFS;
    }
            
    assert(0);
    return 0;
}

/* Obtain some characteristics of the command. */
static void kcdpg_handle_kws_prop_change_get_static_info(struct kcdpg_handle_kws_prop_change_state *st) {
    uint32_t cmd_type = st->cmd_type;
    
    switch (cmd_type) {
        case KANP_CMD_KWS_SET_USER_PWD:
        case KANP_CMD_KWS_SET_USER_NAME:
        case KANP_CMD_KWS_SET_NAME:
            st->prop_value_type = 2;
            break;
         
        default:
            st->prop_value_type = 1;
            st->prop_value_flag = kcdpg_handle_kws_prop_change_cmd_type_to_flag(cmd_type);
    }
    
    switch (cmd_type) {
        case KANP_CMD_KWS_SET_USER_PWD:
        case KANP_CMD_KWS_SET_USER_NAME:
        case KANP_CMD_KWS_SET_USER_ADMIN:
        case KANP_CMD_KWS_SET_USER_MANAGER:
        case KANP_CMD_KWS_SET_USER_LOCK:
        case KANP_CMD_KWS_SET_USER_BAN:
            st->kws_user_flag = 1;
    }
    
    switch (cmd_type) {
        case KANP_CMD_KWS_SET_USER_PWD:
        case KANP_CMD_KWS_SET_USER_NAME:
            st->target_self_ok_flag = 1;
    }
    
    switch (cmd_type) {
        case KANP_CMD_KWS_SET_USER_PWD:
        case KANP_CMD_KWS_SET_USER_NAME:
        case KANP_CMD_KWS_SET_USER_BAN:
            st->target_ban_ok_flag = 1;
    }
    
    switch (cmd_type) {
        case KANP_CMD_KWS_SET_USER_PWD:
        case KANP_CMD_KWS_SET_USER_NAME:
        case KANP_CMD_KWS_SET_USER_LOCK:
        case KANP_CMD_KWS_SET_USER_BAN:
            st->priv_level = KANP_USER_FLAG_MANAGER;
            break;
        
        case KANP_CMD_KWS_SET_USER_MANAGER:
        case KANP_CMD_KWS_SET_SECURE:
        case KANP_CMD_KWS_SET_FREEZE:
        case KANP_CMD_KWS_SET_NAME:
        case KANP_CMD_KWS_SET_THIN_KFS:
            st->priv_level = KANP_USER_FLAG_ADMIN;
            break;
        
        case KANP_CMD_KWS_SET_USER_ADMIN:
        case KANP_CMD_KWS_SET_DEEP_FREEZE:
            st->priv_level = KANP_USER_FLAG_ROOT;
            break;
    }
    
    switch (cmd_type) {
        case KANP_CMD_KWS_SET_USER_MANAGER:
        case KANP_CMD_KWS_SET_USER_ADMIN:
        case KANP_CMD_KWS_SET_USER_LOCK:
        case KANP_CMD_KWS_SET_USER_BAN:
        case KANP_CMD_KWS_SET_SECURE:
            st->security_flag = 1;
    }
}

/* Obtain and validate the command information. */
static int kcdpg_handle_kws_prop_change_check_cmd_info(struct kcdpg_handle_kws_prop_change_state *st) {
    uint64_t u;
    kstr *ts = &st->ts, *ts2 = &st->ts2;
    struct kcdpg_kws_bound_state *wb = st->wb;
    struct kcd_global_user_usage_info *ui = &st->usage_info;
    struct kcd_global_user_license_info *li = &st->license_info;
    
    /* Read the workspace ID. */
    if (anp_read_uint64(&wb->cmd_buf, &u)) return -1;
    
    /* Read the target user ID. */
    if (st->kws_user_flag && anp_read_uint32(&wb->cmd_buf, &st->target_user_id)) return -1;
    
    /* Read the property value. */
    if (st->prop_value_type == 1) {
        if (anp_read_uint32(&wb->cmd_buf, &st->prop_value_flags_set_flag)) return -1;
    }
    
    else if (st->prop_value_type == 2) {
        if (anp_read_kstr(&wb->cmd_buf, &st->prop_value_str)) return -1;
    }
    
    /* Obtain the usage and license information. */
    kcdpg_get_kws_usage_and_license_info(ts, ts2, wb->kws_id, ui, li);
    
    /* Validate that we are not setting the user ban flag to false. This is not
     * supported at this time.
     */
    if (st->cmd_type == KANP_CMD_KWS_SET_USER_BAN && !st->prop_value_flags_set_flag) {
        kmod_set_error("cannot clear the user ban flag");
        return -1;
    }
    
    /* Perform the permission checks. Mind the order here, we want an accurate
     * error message.
     */
    if (kcdpg_perm_check_kws_bound(ts, wb, 1, 1, 1) ||
        (st->kws_user_flag && kcdpg_perm_check_modify_target_user_prop(ts,
                                                                       wb->kws_id,
                                                                       wb->user_id,
                                                                       wb->user_flags,
                                                                       st->target_user_id,
                                                                       &st->target_user_flags,
                                                                       st->target_self_ok_flag,
                                                                       st->target_ban_ok_flag,
                                                                       st->priv_level)) ||
        (!st->kws_user_flag && kcdpg_perm_check_priv_level(wb->user_flags, st->priv_level))) {
        return kcdpg_handle_kws_bound_perm_error(wb);
    }
    
    /* Validate that the user has the license rights to set the workspace secure
     * flag.
     */
    if (st->cmd_type == KANP_CMD_KWS_SET_SECURE && st->prop_value_flags_set_flag) {
        if (!li->secure_kws_flag) {
            kmod_set_error("not authorized to create secure " KCD_KWS_NAME);
            kcd_kanp_resource_quota_failure(kcdpg_kws_bound_failure(wb), KANP_RESOURCE_QUOTA_NO_SECURE);
            return -2;
        }
    }
    
    /* Lock the KFS if we're modifying the thin KFS flag. */
    if (st->cmd_type == KANP_CMD_KWS_SET_THIN_KFS) kcdpg_lock_kfs(ts, wb->kws_id, 1);
    
    return 0;
}

/* Perform the requested modifications. */
static void kcdpg_handle_kws_prop_change_perform_change(struct kcdpg_handle_kws_prop_change_state *st) {
    kstr *ts = &st->ts;
    uint32_t cmd_type = st->cmd_type;
    struct kcdpg_kws_bound_state *wb = st->wb;
    struct kcdpg_kws_prop_change_state *pcs = st->pcs;
    
    if (cmd_type == KANP_CMD_KWS_SET_USER_PWD) {
        kcdpg_update_kws_user_pwd(ts, wb->kws_id, st->target_user_id, &st->prop_value_str);
    }
    
    else if (cmd_type == KANP_CMD_KWS_SET_USER_NAME) {
        if (kcdpg_is_user_manager(wb->user_flags))
            kcdpg_kws_prop_change_user_name_admin(pcs, st->target_user_id, &st->prop_value_str);
        else
            kcdpg_kws_prop_change_user_name_user(pcs, st->target_user_id, &st->prop_value_str);
    }
    
    else if (cmd_type == KANP_CMD_KWS_SET_USER_ADMIN ||
             cmd_type == KANP_CMD_KWS_SET_USER_MANAGER ||
             cmd_type == KANP_CMD_KWS_SET_USER_LOCK ||
             cmd_type == KANP_CMD_KWS_SET_USER_BAN) {
        kcdpg_set_kws_user_flags(&st->target_user_flags, st->prop_value_flag, st->prop_value_flags_set_flag);
        kcdpg_kws_prop_change_user_flags(pcs, st->target_user_id, st->target_user_flags);
        
        /* Delete the tickets and the email IDs of the user being banned. The
         * password is unaffected.
         */
        if (cmd_type == KANP_CMD_KWS_SET_USER_BAN && kcdpg_is_user_banned(st->target_user_flags)) {
            kstr_sf(ts, "DELETE FROM kcd_kws_login_ticket WHERE kws_id = "PRINTF_64"u AND user_id = %u",
                        wb->kws_id, st->target_user_id);
            kcdpg_exec_query(ts->data);
            
            kstr_sf(ts, "DELETE FROM kcd_kws_user_invitation WHERE kws_id = "PRINTF_64"u AND user_id = %u",
                        wb->kws_id, st->target_user_id);
            kcdpg_exec_query(ts->data);
        }
    }
    
    else if (cmd_type == KANP_CMD_KWS_SET_NAME) {
        kcdpg_kws_prop_change_kws_name(pcs, &st->prop_value_str);
    }
    
    else if (cmd_type == KANP_CMD_KWS_SET_SECURE ||
             cmd_type == KANP_CMD_KWS_SET_FREEZE ||
             cmd_type == KANP_CMD_KWS_SET_DEEP_FREEZE ||
             cmd_type == KANP_CMD_KWS_SET_THIN_KFS) {
        kcdpg_set_kws_flags(&wb->kws_flags, st->prop_value_flag, st->prop_value_flags_set_flag);
        kcdpg_kws_prop_change_kws_flags(pcs, wb->kws_flags);
        
        if (cmd_type == KANP_CMD_KWS_SET_SECURE && st->prop_value_flags_set_flag) {
            if (wb->login_type == KCD_KWS_LOGIN_TYPE_NORMAL) wb->login_type = KCD_KWS_LOGIN_TYPE_SECURE;
        }
        
        if (cmd_type == KANP_CMD_KWS_SET_THIN_KFS && st->prop_value_flags_set_flag) {
            uint64_t total_size;
        
            /* Get rid of the stale files in the file map table. */
            kstr_sf(ts, "DELETE FROM kcd_kws_kfs_file_map WHERE kws_id = "PRINTF_64"u AND share_id = 0 AND "
                        "(kws_id, share_id, inode, commit_id) NOT IN "
                        "(SELECT kws_id, share_id, inode, commit_id FROM kcd_kws_kfs_current_view "
                        " WHERE kws_id = "PRINTF_64"u AND share_id = 0)", wb->kws_id, wb->kws_id);
            kcdpg_exec_query(ts->data);
            
            /* Recompute the total size. */
            kstr_sf(ts, "SELECT sum(size) FROM kcd_kws_kfs_file_map WHERE kws_id = "PRINTF_64"u", wb->kws_id);
            total_size = kcdpg_get_row_uint64(ts);
            
            kstr_sf(ts, "UPDATE kcd_kws_kfs_limit SET file_size = "PRINTF_64"u WHERE kws_id = "PRINTF_64"u",
                        total_size, wb->kws_id);
            kcdpg_exec_query(ts->data);
            
            /* The files must be synchronized. */
            st->sync_kfs_flag = 1;
        }
    }

KCDPG_QUERY_START(handle_kws_prop_change)
    
    /* Get the arguments. */
    if (anp_read_uint32(&st.arg_buf, &st.cmd_type)) {
        elog(ERROR, "bad handle_kws_prop_change argument: %s", kmod_strerror());
    }
    
    /* Create the property change state object. */
    st.pcs = kcdpg_kws_prop_change_state_new(wb->kws_id, wb->user_id);

    /* Obtain some characteristics of the command. */
    kcdpg_handle_kws_prop_change_get_static_info(&st);
    
    /* Obtain and validate the command information. */
    error = kcdpg_handle_kws_prop_change_check_cmd_info(&st);
    if (error) break;
    
    /* Perform the requested modifications. */
    kcdpg_handle_kws_prop_change_perform_change(&st);
    
    /* Post the property change event, if required. */
    st.evt_id = kcdpg_kws_prop_change_post_event(st.pcs, wb->date);
    
    /* Notify the listeners of the perm_check relation if a security flag has
     * been modified.
     */
    if (st.security_flag && st.pcs->nb_change) notify_perm_check(ts, wb->kws_id);
    
    /* Format the reply. */
    if (wb->cmd_minor <= 3) {
        wb->res_type = KANP_RES_OK;
    }
    
    else {
        wb->res_type = KANP_RES_KWS_PROP_CHANGE;
        anp_write_uint64(&wb->res_buf, st.evt_id);
    }
    
    /* Write the output parameters. */
    anp_write_uint32(&st.ext_buf, st.sync_kfs_flag);
    anp_write_uint32(&st.ext_buf, wb->login_type);
    
KCDPG_QUERY_END(handle_kws_prop_change)


/* Check if the user can log in the workspace specified (safe query):
 *   UINT64 Workspace ID.
 *   UINT32 Login type.
 *   UINT32 User ID.
 *
 * Output:
 *   UINT32 Result (0: OK, 1: error).
 *   UINT32 Login code.
 *   STR    Error message.
 */
KCDPG_QUERY_STRUCT(check_kws_login)
    uint64_t kws_id;
    uint32_t kws_flags;
    uint32_t login_type;
    uint32_t user_id;
    uint32_t user_flags;
    uint32_t login_code;

KCDPG_QUERY_INIT(check_kws_login, 0)
KCDPG_QUERY_CLEAN(check_kws_login)

KCDPG_QUERY_START(check_kws_login)
    int r = 0;
    
    if (anp_read_uint64(&st.arg_buf, &st.kws_id) ||
        anp_read_uint32(&st.arg_buf, &st.login_type) ||
        anp_read_uint32(&st.arg_buf, &st.user_id)) {
        elog(ERROR, "bad check_kws_login argument: %s", kmod_strerror());
    }
    
    do {
        /* Lock the workspace, if any. */
        r = kcdpg_perm_check_lock_kws(ts, st.kws_id, 0, &st.login_code);
        if (r) break;
        
        st.kws_flags = kcdpg_get_kws_flags(ts, st.kws_id);
        
        /* Verify that the user can log in. There is no specific login error
         * code for the case where the user ID does not exist, since the user ID
         * should always be valid if the workspace still exists.
         */
        r = kcdpg_perm_check_user_exist(ts, st.kws_id, st.user_id, &st.user_flags);
        if (r) break;
        
        r = kcdpg_perm_check_kws_login(st.kws_flags, st.login_type, st.user_flags, &st.login_code);
        if (r) break;
        
    } while (0);
    
    anp_write_uint32(&st.ext_buf, r ? 1 : 0);
    anp_write_uint32(&st.ext_buf, st.login_code);
    anp_write_kstr(&st.ext_buf, kmod_kstrerror());

KCDPG_QUERY_END(check_kws_login)


/* Set the license of a global user (safe query):
 *   STR    Email.
 *   STR    License. 
 */
KCDPG_QUERY_STRUCT(set_freemium_user)
    kstr email;
    kstr license;

KCDPG_QUERY_INIT(set_freemium_user, 0)
    kstr_init(&self->email);
    kstr_init(&self->license);
    
KCDPG_QUERY_CLEAN(set_freemium_user)
    kstr_clean(&self->email);
    kstr_clean(&self->license);

KCDPG_QUERY_START(set_freemium_user)
    
    if (anp_read_kstr(&st.arg_buf, &st.email) ||
        anp_read_kstr(&st.arg_buf, &st.license)) {
        elog(ERROR, "bad set_freemium_user argument: %s", kmod_strerror());
    }
    
    /* Lock the workspace list. */
    kcdpg_lock_kws_list(ts);
    
    /* Insert the user, if required, and update his license. */
    kcdpg_insert_global_user(ts, &st.email, KANP_EMAIL_SUMMARY_FLAG);
    kcdpg_set_global_user_license(ts, &st.email, &st.license);
    
KCDPG_QUERY_END(set_freemium_user)


/* Retrieve the resource usage and license information about the specified user
 * (safe query).
 *   STR     Email
 *
 * Output:
 *   Resource info fields.
 *   License info fields.
 */
KCDPG_QUERY_STRUCT(get_usage_and_license_info)
    kstr email;
    struct kcd_global_user_usage_info usage_info;
    struct kcd_global_user_license_info license_info;

KCDPG_QUERY_INIT(get_usage_and_license_info, 0)
    kstr_init(&self->email);
    kcd_global_user_usage_info_init(&self->usage_info);
    kcd_global_user_license_info_init(&self->license_info);

KCDPG_QUERY_CLEAN(get_usage_and_license_info)
    kstr_clean(&self->email);
    kcd_global_user_usage_info_clean(&self->usage_info);
    kcd_global_user_license_info_clean(&self->license_info);

KCDPG_QUERY_START(get_usage_and_license_info)
    struct kcd_global_user_usage_info *u = &st.usage_info;
    struct kcd_global_user_license_info *l = &st.license_info;
    kbuffer *b = &st.ext_buf;
    
    if (anp_read_kstr(&st.arg_buf, &st.email)) {
        elog(ERROR, "bad get_usage_and_license_info argument: %s", kmod_strerror());
    }
    
    kcdpg_get_global_user_usage_info(ts, &st.email, u);
    kcdpg_get_global_user_license_info(ts, &st.email, l);
    
    anp_write_uint32(b, u->nb_non_pb_kws);
    anp_write_uint32(b, u->nb_pb_kws);
    anp_write_uint64(b, u->kfs_usage);
    
    anp_write_kstr(b, &l->license_name);
    anp_write_uint32(b, l->nb_non_pb_kws);
    anp_write_uint32(b, l->nb_pb_kws);
    anp_write_uint64(b, l->kfs_usage);
    anp_write_uint32(b, l->secure_kws_flag);
    anp_write_uint64(b, l->vnc_session_time);
    
KCDPG_QUERY_END(get_usage_and_license_info)

