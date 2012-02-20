/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _KTLS_H
#define _KTLS_H

struct ktls_conn {

    /* Socket, when the connection is open. */
    int sock;
    
    /* GnuTLS session. */
    gnutls_session_t session;
    
    /* Certificate data, if any. */
    gnutls_certificate_credentials_t cert_cred;
    
    /* Client anonymous credentials, if any. */
    gnutls_anon_client_credentials_t client_anon_cred;
    
    /* Server anonymous credentials, if any. */
    gnutls_anon_server_credentials_t server_anon_cred;
    gnutls_dh_params_t server_dh_params;
};

void ktls_init(struct ktls_conn *self);
void ktls_clean(struct ktls_conn *self);
void ktls_reset(struct ktls_conn *self);
int ktls_setup_server(struct ktls_conn *self, int sock, char *cert_path, char *key_path, int anon_flag);
int ktls_setup_client(struct ktls_conn *self, int sock, int cert_flag, int anon_flag);
int ktls_perform_handshake(struct ktls_conn *self);
int ktls_recv(struct ktls_conn *self, char *buf, int len);
int ktls_send(struct ktls_conn *self, char *buf, int len);
int ktls_handshake_loop(struct ktls_conn *self);

#endif
