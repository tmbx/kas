/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* This function sets the KMOD error string based on the error that occurred on
 * the TLS connection object.
 */
static void ktls_import_tls_err(int error) {
    kmod_set_error("%s", gnutls_strerror(error));
}

void ktls_init(struct ktls_conn *self) {
    self->session = NULL;
    self->cert_cred = NULL;
    self->client_anon_cred = NULL;
    self->server_anon_cred = NULL;
    self->server_dh_params = NULL;
}

void ktls_clean(struct ktls_conn *self) {
    if (self) ktls_reset(self);
}

/* This function resets the state of the connection to the state it had
 * immediately after its initialization.
 */
void ktls_reset(struct ktls_conn *self) {
    self->sock = -1;
    
    if (self->session) {
    	gnutls_deinit(self->session);
	self->session = NULL;
    }
    
    if (self->cert_cred) {
    	gnutls_certificate_free_credentials(self->cert_cred);
	self->cert_cred = NULL;
    }
    
    if (self->client_anon_cred) {
    	gnutls_anon_free_client_credentials(self->client_anon_cred);
	self->client_anon_cred = NULL;
    }
    
    if (self->server_anon_cred) {
    	gnutls_anon_free_server_credentials(self->server_anon_cred);
	self->server_anon_cred = NULL;
    }
    
    if (self->server_dh_params) {
        gnutls_dh_params_deinit(self->server_dh_params);
        self->server_dh_params = NULL;
    }
}
 
/* This function setups the connection for use on the server side. The function
 * accepts unauthenticated connections if requested, and authenticated
 * connections if 'cert_path' is non-null.
 */
int ktls_setup_server(struct ktls_conn *self, int sock, char *cert_path, char *key_path, int anon_flag) {
    int error = -1;
    
    ktls_reset(self);
    self->sock = sock;
    
    /* Cipher suites reported by gnutls_cipher_suite_get_name() after handshake:
     * - With cert: RSA_AES_256_CBC_SHA1.
     * - Without cert: ANON_DH_AES_128_CBC_SHA1.
     */
    do {
    	int r;
        
        /* Cipher suites we want to enable. */
        int cert_flag = (cert_path != NULL);
        
        /* Supported key exchange algorithms. Since ANON_DH is not included by
         * default, we have to specify it explicitly. Since we're specifying
         * algorithms explicitly, we also have to specify RSA explicitly,
         * otherwise the call to gnutls_kx_set_priority() will disable it.
         */
	int kx_prio[3];
        int nb_kx_prio = 0;
        
        if (cert_flag) kx_prio[nb_kx_prio++] = GNUTLS_KX_RSA;
        if (anon_flag) kx_prio[nb_kx_prio++] = GNUTLS_KX_ANON_DH;
        kx_prio[nb_kx_prio++] = 0;
	
	r = gnutls_init(&self->session, GNUTLS_SERVER);
	if (r) { ktls_import_tls_err(r); break; }
	
	gnutls_transport_set_ptr(self->session, (gnutls_transport_ptr_t) sock);
        
        /* This call must be done *first* when multiple cipher suites are
         * supported.
         */
        r = gnutls_set_default_priority(self->session);
        if (r) { ktls_import_tls_err(r); break; }
        
        r = gnutls_kx_set_priority(self->session, kx_prio);
        if (r) { ktls_import_tls_err(r); break; }
        
        if (anon_flag) {
            r = gnutls_anon_allocate_server_credentials(&self->server_anon_cred);
            if (r) { ktls_import_tls_err(r); break; }
            
            r = gnutls_dh_params_init(&self->server_dh_params);
            if (r) { ktls_import_tls_err(r); break; }
            
            r = gnutls_dh_params_generate2(self->server_dh_params, 768);
            if (r) { ktls_import_tls_err(r); break; }
            
            gnutls_anon_set_server_dh_params(self->server_anon_cred, self->server_dh_params);
            
            r = gnutls_credentials_set(self->session, GNUTLS_CRD_ANON, self->server_anon_cred);
            if (r) { ktls_import_tls_err(r); break; }
            
            gnutls_dh_set_prime_bits(self->session, 768);
    	}
        
	if (cert_flag) {
	    r = gnutls_certificate_allocate_credentials(&self->cert_cred);
	    if (r) { ktls_import_tls_err(r); break; }
	    
	    r = gnutls_certificate_set_x509_trust_file(self->cert_cred, cert_path, GNUTLS_X509_FMT_PEM);
	    if (r < 0) { ktls_import_tls_err(r); break; }
	    
	    r = gnutls_certificate_set_x509_key_file(self->cert_cred, cert_path, key_path, GNUTLS_X509_FMT_PEM);
	    if (r) { ktls_import_tls_err(r); break; }
	    
	    r = gnutls_credentials_set(self->session, GNUTLS_CRD_CERTIFICATE, self->cert_cred);
    	    if (r) { ktls_import_tls_err(r); break; }
	}
	
	error = 0;
	
    } while (0); 
    
    if (error) kmod_append_error("cannot setup TLS connection");
    
    return error;
}

/* This function setups the connection for use on the client side. The function
 * accepts unauthenticated and authenticated connections as requested.
 */
int ktls_setup_client(struct ktls_conn *self, int sock, int cert_flag, int anon_flag) {
    int error = -1;
    
    ktls_reset(self);
    self->sock = sock;
    
    do {
        int r;
	int kx_prio[3];
        int nb_kx_prio = 0;
        
        if (cert_flag) kx_prio[nb_kx_prio++] = GNUTLS_KX_RSA;
        if (anon_flag) kx_prio[nb_kx_prio++] = GNUTLS_KX_ANON_DH;
        kx_prio[nb_kx_prio++] = 0;
	
	r = gnutls_init(&self->session, GNUTLS_CLIENT);
	if (r) { ktls_import_tls_err(r); break; }
	
	gnutls_transport_set_ptr(self->session, (gnutls_transport_ptr_t) sock);
        
        r = gnutls_set_default_priority(self->session);
        if (r) { ktls_import_tls_err(r); break; }
        
        r = gnutls_kx_set_priority(self->session, kx_prio);
        if (r) { ktls_import_tls_err(r); break; }
        
        if (anon_flag) {
            r = gnutls_anon_allocate_client_credentials(&self->client_anon_cred);
            if (r) { ktls_import_tls_err(r); break; }
            
            r = gnutls_credentials_set(self->session, GNUTLS_CRD_ANON, self->client_anon_cred);
            if (r) { ktls_import_tls_err(r); break; }
    	}
        
	if (cert_flag) {
	    const int cert_type_priority[2] = { GNUTLS_CRT_X509, 0 };
            
	    r = gnutls_certificate_allocate_credentials(&self->cert_cred);
	    if (r) { ktls_import_tls_err(r); break; }
	    
	    r = gnutls_certificate_type_set_priority(self->session, cert_type_priority);
	    if (r) { ktls_import_tls_err(r); break; }
	    
	    r = gnutls_credentials_set(self->session, GNUTLS_CRD_CERTIFICATE, self->cert_cred);
    	    if (r) { ktls_import_tls_err(r); break; }
	}
	
	error = 0;
	
    } while (0); 
    
    if (error) kmod_append_error("cannot setup TLS connection");
    
    return error;
}

/* This function performs the handshake of the TLS connection. It returns 0 on
 * success, -1 on failure, -2 if the function should be called again when the
 * socket is ready for reading or -3 if the function should be called again when
 * the socket is ready for writing.
 */
int ktls_perform_handshake(struct ktls_conn *self) {
    int r = gnutls_handshake(self->session);
    
    if (! r) {
    	return 0;
    }
    
    else if (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED) {
    	return gnutls_record_get_direction(self->session) ? -3 : -2;
    }
    
    else {
    	ktls_import_tls_err(r);
	kmod_append_error("TLS handshake failed");
	return -1;
    }
}

/* This function receives data from the TLS connection. It returns the number of
 * bytes received on success, -1 on failure or -2 if the function should be
 * called again when the socket is ready for reading.
 */
int ktls_recv(struct ktls_conn *self, char *buf, int len) {
    assert(len);
    
    int r = gnutls_record_recv(self->session, buf, (size_t) len);
    
    if (! r) {
    	kmod_set_error("remote host closed TLS connection");
    	return -1;
    }
    
    else if (r > 0) {
    	return r;
    }
    
    else if (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED) {
    	assert(gnutls_record_get_direction(self->session) == 0);
    	return -2;
    }
    
    else {
    	ktls_import_tls_err(r);
	kmod_append_error("cannot receive TLS data");
	return -1;
    }
}

/* This function sends data on the TLS connection. It returns the number of
 * bytes sent on success, -1 on failure or -2 if the function should be
 * called again when the socket is ready for writing.
 */    
int ktls_send(struct ktls_conn *self, char *buf, int len) {
    assert(len);
    
    int r = gnutls_record_send(self->session, buf, (size_t) len);
    
    if (r >= 0) {
    	return r;
    }
    
    else if (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED) {
    	assert(gnutls_record_get_direction(self->session) == 1);
    	return -2;
    }
    
    else {
    	ktls_import_tls_err(r);
	kmod_append_error("cannot send TLS data");
	return -1;
    }
}

/* Utility method that loops until the handshake is performed. */
int ktls_handshake_loop(struct ktls_conn *self) {
    kmod_log_msg(KCD_LOG_MISC, "ktls_handshake_loop() called.\n");
    
    while (1) {
    	struct kselect sel;
    	int r = ktls_perform_handshake(self);
	
	if (! r) return 0;
	if (r == -1) return -1;
	
	kdaemon_prepare_select(&sel);
	
	if (r == -2) kselect_add_read(&sel, self->sock);
	if (r == -3) kselect_add_write(&sel, self->sock);
	
	if (kdaemon_do_select(&sel)) return -1;
    }
}

