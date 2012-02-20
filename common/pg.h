/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _PG_H
#define _PG_H

#include <postgresql/libpq-fe.h>
#include <postgresql/libpq/libpq-fs.h>

/* This structure represents a connection to a postgres database. */
struct pg_db_conn {
    
    /* Postgres connection object. */
    PGconn *pg_conn;
    
    /* Connection socket, valid after a call to pg_db_connect_start(). */
    int sock;
    
    /* State of the current query:
     * 0: no query.
     * 1: query being sent to the server.
     * 2. query sent, waiting for results.
     */
    int query_state;
    
    /* Number of results left to receive. */
    int nb_result_left;
    
    /* Buffered last result. See pg_db_result_check() for details. */
    PGresult *buf_res;
};

void pg_db_conn_init(struct pg_db_conn *self);
void pg_db_conn_clean(struct pg_db_conn *self);
void pg_db_reset(struct pg_db_conn *self);
int pg_db_set_blocking_mode(struct pg_db_conn *self, int blocking);
int pg_db_connect_start(struct pg_db_conn *self, char *conn_info);
int pg_db_connect_check(struct pg_db_conn *self);
int pg_db_query_start(struct pg_db_conn *self, char *query, int nb_result);
int pg_db_query_check(struct pg_db_conn *self);
int pg_db_consume(struct pg_db_conn *self);
PGresult * pg_db_result_check(struct pg_db_conn *self);
PGnotify * pg_db_notify_check(struct pg_db_conn *self);
void pg_db_destroy_res(PGresult **res);
void pg_db_destroy_notif(PGnotify **notif);
int pg_db_verify_result(PGresult *res, char *err_str);
void pg_db_add_uint32(kstr *to, uint32_t value);
void pg_db_add_uint64(kstr *to, uint64_t value);
void pg_db_add_str(struct pg_db_conn *self, kstr *to, kstr *from);
void pg_db_add_bytea(struct pg_db_conn *self, kstr *to, kbuffer *from);
uint32_t pg_db_get_uint32(PGresult *res, int row, int col);
uint64_t pg_db_get_uint64(PGresult *res, int row, int col);
void pg_db_get_bytea(PGresult *res, int row, int col, kbuffer *to);
int pg_db_lo_create(struct pg_db_conn *self, int *oid);
int pg_db_lo_open(struct pg_db_conn *self, int oid, int *fd);
int pg_db_lo_close(struct pg_db_conn *self, int *fd);
int pg_db_lo_size(struct pg_db_conn *self, int fd, int *size);
int pg_db_lo_read(struct pg_db_conn *self, int fd, char *buf, int len);
int pg_db_lo_write(struct pg_db_conn *self, int fd, char *buf, int len);

#endif
