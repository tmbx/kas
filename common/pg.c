/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* It must be noted that Postgres' documentation leaves a lot of things
 * unspecified. The correctness of the code below is therefore unknown.
 *
 * UPDATE: the documentation is quite misleading, but the source code is well
 * commented. See src/interfaces/libpq/ in Postgres' source tree.
 */

/* Helper function. */
static void pg_db_import_err(char *msg) {
    kstr tmp;
    
    kstr_init(&tmp);
    kstr_assign_cstr(&tmp, msg);
    
    if (! tmp.slen) {
	kstr_assign_cstr(&tmp, "unknown postgres error");
    }
    
    /* Postgres error messages always end with a newline. Remove it. */
    else {
	tmp.slen--;
	tmp.data[tmp.slen] = 0;
    }
    
    kmod_set_error("%s", tmp.data);
    kstr_clean(&tmp);
}

/* This function sets the KMOD error string based on the last error that
 * occurred on the postgres connection object.
 */
static void pg_db_import_conn_err(struct pg_db_conn *self) {
    pg_db_import_err(PQerrorMessage(self->pg_conn));
}

/* This function sets the KMOD error string based on the error associated to the
 * result specified.
 */
static void pg_db_import_result_err(PGresult *res) {
    pg_db_import_err(PQresultErrorMessage(res));
}

void pg_db_conn_init(struct pg_db_conn *self) {
    self->pg_conn = NULL;
    self->sock = -1;
    self->query_state = 0;
    self->nb_result_left = 0;
    self->buf_res = NULL;
}

void pg_db_conn_clean(struct pg_db_conn *self) {
    if (self) pg_db_reset(self);
}

/* This function resets the state of the connection to the state it had
 * immediately after its initialization.
 */
void pg_db_reset(struct pg_db_conn *self) {
    if (self->pg_conn) {
    	PQfinish(self->pg_conn);
	self->pg_conn = NULL;
	self->sock = -1;
	self->query_state = 0;
    	self->nb_result_left = 0;
	
	if (self->buf_res) {
	    PQclear(self->buf_res);
	    self->buf_res = NULL;
	}
    }
}

/* This function sets the connection blocking mode. */
int pg_db_set_blocking_mode(struct pg_db_conn *self, int blocking) {
    if (PQsetnonblocking(self->pg_conn, !blocking)) {
	pg_db_import_conn_err(self);
	return -1;
    }
    
    return 0;
}

/* This function opens a connection using the parameters specified. On success,
 * the caller should wait for the socket of the connection to become writable
 * then call the function pg_db_connect_check().
 */
int pg_db_connect_start(struct pg_db_conn *self, char *conn_info) {
    int error = 0;
    
    pg_db_reset(self);
    
    do {
	/* Obtain a postgres connection object. */
	self->pg_conn = PQconnectStart(conn_info);   
	
	if (self->pg_conn == NULL) {
	    kmod_set_error("out of memory");
	    error = -1;
	    break;
	}
	
	if (PQstatus(self->pg_conn) == CONNECTION_BAD) {
	    pg_db_import_conn_err(self);
	    error = -1;
	    break;
	}
	
	error = pg_db_set_blocking_mode(self, 0);
	if (error) break;
	
	self->sock = PQsocket(self->pg_conn);
	
    } while (0);
    
    return error;
}

/* This function checks if Postgres has managed to establish its connection. It
 * returns 0 on success, -1 on failure, -2 if the function should be called
 * again when the socket is ready for reading or -3 if the function should be
 * called again when the socket is ready for writing.
 */
int pg_db_connect_check(struct pg_db_conn *self) {
    int r;
	
    /* Poll the connection. */
    r = PQconnectPoll(self->pg_conn);
    
    if (r == PGRES_POLLING_OK) {
	return 0;
    }
    
    else if (r == PGRES_POLLING_FAILED) {
	pg_db_import_conn_err(self);
	return -1;
    }
    
    else if (r == PGRES_POLLING_READING) {
	return -2;
    }
    
    else if (r == PGRES_POLLING_WRITING) {
    	return -3;
    }
    
    else assert(0);
    
    return 0;
}

/* This function sends a query to the server. On success, the caller should wait
 * for the socket of the connection to become writable then call the function 
 * pg_db_query_check().
 */
int pg_db_query_start(struct pg_db_conn *self, char *query, int nb_result) {
    assert(! self->query_state);
    self->query_state = 1;
    self->nb_result_left = nb_result;
    
    if (! PQsendQuery(self->pg_conn, query)) {
    	pg_db_import_conn_err(self);
    	kmod_append_error("cannot execute query");
	return -1;
    }
    
    return 0;
}

/* This function checks whether a query has been sent completely to the server. 
 * It returns 0 on success, -1 on failure or -2 if the function should be
 * called again when the socket is ready for writing.
 */
int pg_db_query_check(struct pg_db_conn *self) {
    assert(self->query_state == 1);
    int r = PQflush(self->pg_conn);
    
    if (r == 0) {
    	self->query_state = 2;
	return 0;
    }
    
    else if (r == -1) {
	pg_db_import_conn_err(self);
	return -1;
    }
    
    else if (r == 1) {
	return -2;
    }
    
    else assert(0);
    
    return 0;
}
 
/* This function calls PQconsumeInput() to allow Postgres to receive the data it
 * is waiting for. This function should be called prior to a call to
 * pg_db_result_check() and pg_db_notify_check() (but not *between* the call to
 * these two functions, otherwise a subsequent call to select() could hang).
 */
int pg_db_consume(struct pg_db_conn *self) {
    
    if (! PQconsumeInput(self->pg_conn)) {
	pg_db_import_conn_err(self);
	return -1;
    }

    return 0;
}

/* This function checks whether a query sent to the server has produced a result.
 * If so, this function returns the result, which must be freed with PQclear().
 * If no result is ready, the function returns NULL.
 */
PGresult * pg_db_result_check(struct pg_db_conn *self) {
    assert(self->query_state == 2);
    
    /* PQisBusy() can return true even if only one result should be
     * returned for our query and we already got this result. To
     * work-around this idiocy, we buffer the last result, and we wait
     * for PQisBusy() to return false and PQgetResult() to return NULL.
     */
    if (! PQisBusy(self->pg_conn)) {
	PGresult *res = PQgetResult(self->pg_conn);
	
        /* We should have gotten all our results. */
	if (res == NULL) {
	    if (self->nb_result_left) {
		kerror_fatal("PG database did not return enough results.\n");
	    }
	    
	    assert(self->buf_res);
	    
	    res = self->buf_res;
	    self->buf_res = NULL;
	    self->query_state = 0;
	    return res;
	}
	
	else {
	    if (! self->nb_result_left) {
		kerror_fatal("PG database returned too many results.\n");
	    }
	    
	    self->nb_result_left--;
	    
	    /* This should be the last result. */
	    if (! self->nb_result_left) {
		
		/* Great, the DB is not busy. We should be done. */
		if (! PQisBusy(self->pg_conn)) {
		    if (PQgetResult(self->pg_conn)) kerror_fatal("PG database sent a result after last result.\n");
		    self->query_state = 0;
		    return res;
		}
		
		/* So, it's busy. Enable workaround... */
		else {
		    self->buf_res = res;
		    return NULL;
		}
	    }
	    
	    /* Return the current result. */
	    else {
		return res;
	    }
	}
    }
    
    /* The result is not ready. */
    return NULL;
}

/* This function checks if a notification has been received. If so, this
 * function returns this notification, which must be freed with PQfreemem(). If
 * no notification was received, this function returns NULL. This function must
 * be called in a loop until all notifications have been read.
 */
PGnotify * pg_db_notify_check(struct pg_db_conn *self) {
    return PQnotifies(self->pg_conn);
}

/* Utility function to destroy a Postgres result and set its pointer to NULL, as
 * needed.
 */
void pg_db_destroy_res(PGresult **res) {
    if (*res) {
	PQclear(*res);
	*res = NULL;
    }
}

/* Utility function to destroy a Postgres notification and set its pointer to
 * NULL, as needed.
 */
void pg_db_destroy_notif(PGnotify **notif) {
    if (*notif) {
        PQfreemem(*notif);
	*notif = NULL;
    }
}

/* This function checks if a query was successful. If not, this function sets
 * the KMOD error string based on the string specified.
 */
int pg_db_verify_result(PGresult *res, char *err_str) {
    int res_status = PQresultStatus(res);
    
    if (res_status != PGRES_COMMAND_OK && res_status != PGRES_TUPLES_OK) {
    	pg_db_import_result_err(res);
	kmod_append_error("'%s' query failed", err_str);
	return -1;
    }
    
    return 0;
}

/* This function adds a uint32 value to a query string. */
void pg_db_add_uint32(kstr *to, uint32_t value) {
    char buf[100];
    sprintf(buf, "%u", value);
    kstr_append_cstr(to, buf);
}

/* This function adds a uint64 value to a query string. */
void pg_db_add_uint64(kstr *to, uint64_t value) {
    char buf[100];
    sprintf(buf, PRINTF_64"u", value);
    kstr_append_cstr(to, buf);
}

/* This function escapes and adds a string to a query string. */
void pg_db_add_str(struct pg_db_conn *self, kstr *to, kstr *from) {
    int error;
    kstr tmp;
    kstr_init(&tmp);
    
    kstr_grow(&tmp, from->slen * 2 + 1);
    tmp.slen = PQescapeStringConn(self->pg_conn, tmp.data, from->data, from->slen, &error);
    if (error) kstr_reset(&tmp);
    
    kstr_append_char(to, '\'');
    kstr_append_kstr(to, &tmp);
    kstr_append_char(to, '\'');
    
    kstr_clean(&tmp);
}

/* This function escapes and adds a binary string to a query string. */
void pg_db_add_bytea(struct pg_db_conn *self, kstr *to, kbuffer *from) {
    size_t tmp_len;
    char *tmp = PQescapeByteaConn(self->pg_conn, from->data, from->len, &tmp_len);
    if (! tmp) kerror_fatal("out of memory");
    kstr_append_cstr(to, "E'");
    kstr_append_buf(to, tmp, tmp_len - 1);
    kstr_append_char(to, '\'');
    PQfreemem(tmp);
}

/* This function returns the 32 bits integer at the specified row/col in the
 * result specified.
 */
uint32_t pg_db_get_uint32(PGresult *res, int row, int col) {
    return atoi(PQgetvalue(res, row, col));
}

/* This function returns the 64 bits integer at the specified row/col in the
 * result specified.
 */
uint64_t pg_db_get_uint64(PGresult *res, int row, int col) {
    return atoll(PQgetvalue(res, row, col));
}

/* This function fetches the binary string at the specified row/col in the
 * result specified.
 */
void pg_db_get_bytea(PGresult *res, int row, int col, kbuffer *to) {
    size_t tmp_len;
    char *tmp = PQunescapeBytea(PQgetvalue(res, row, col), &tmp_len);
    if (! tmp) kerror_fatal("cannot unescape binary string");
    kbuffer_reset(to);
    kbuffer_write(to, tmp, tmp_len);
    PQfreemem(tmp);
}

/* This function creates a new large object. */
int pg_db_lo_create(struct pg_db_conn *self, int *oid) {

    *oid = lo_creat(self->pg_conn, INV_READ | INV_WRITE);
    
    if (! *oid) {
	pg_db_import_conn_err(self);
	return -1;
    }
    
    return 0;
}

/* This function opens a large object. File descriptor -1 means the large object
 * is not presently open.
 */
int pg_db_lo_open(struct pg_db_conn *self, int oid, int *fd) {
    assert(*fd == -1);
    
    *fd = lo_open(self->pg_conn, oid, INV_READ | INV_WRITE);
    
    if (*fd == -1) {
	pg_db_import_conn_err(self);
	return -1;
    }
    
    return 0;
}

/* This function closes a large object, if required. */
int pg_db_lo_close(struct pg_db_conn *self, int *fd) {
    if (*fd == -1) return 0;
    
    if (lo_close(self->pg_conn, *fd)) {
	*fd = -1;
	pg_db_import_conn_err(self);
	return -1;
    }
    
    *fd = -1;
    return 0;
}

/* This function obtains the size of a large object. The read/write cursor is
 * reset to 0.
 */
int pg_db_lo_size(struct pg_db_conn *self, int fd, int *size) {
    *size = lo_lseek(self->pg_conn, fd, 0, SEEK_END);
    
    if (*size == -1) {
	pg_db_import_conn_err(self);
	return -1;
    }
    
    if (lo_lseek(self->pg_conn, fd, 0, SEEK_SET) == -1) {
	pg_db_import_conn_err(self);
	return -1;
    }
    
    return 0;
}

/* This function reads the number of bytes specified from a large object. */
int pg_db_lo_read(struct pg_db_conn *self, int fd, char *buf, int len) {
    int nb = 0;
    
    while (nb != len) {
	int trans = lo_read(self->pg_conn, fd, buf + nb, (size_t) (len - nb));
	
	if (trans < 0) {
	    pg_db_import_conn_err(self);
	    return -1;
	}
	
	nb += trans;
    }
    
    return 0;
}

/* This function writes the number of bytes specified to a large object. */
int pg_db_lo_write(struct pg_db_conn *self, int fd, char *buf, int len) {
    int nb = 0;
    
    while (nb != len) {
	int trans = lo_write(self->pg_conn, fd, buf + nb, (size_t) (len - nb));
	
	if (trans < 0) {
	    pg_db_import_conn_err(self);
	    return -1;
	}
	
	nb += trans;
    }
    
    return 0;
}

