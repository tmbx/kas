/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _K3P_H
#define _K3P_H

#include "k3p_core_defs.h"

/* K3P object identifiers. */
#define K3P_INS     	    	    	    	1
#define K3P_INT     	    	    	    	2
#define K3P_STR     	    	    	    	3

/* Prefered size of the K3P data buffer. */
#define K3P_DATA_BUF_SIZE (64*1024u)

/* State of the protocol. */
enum {
    /* K3P protocol has been initialized (initial state). Do not transfer
     * K3P elements in this state.
     */
    K3P_INITIALIZED,

    /* Connection is active, i.e waiting for one of KPP_CONNECT_KMO,
     * KPP_DISCONNECT_KMO or KPP_BEG_SESSION.
     */
    K3P_ACTIVE,

    /* Connection is interactive, i.e. waiting for commands that can
     * happen after KPP_BEG_SESSION.
     */
    K3P_INTERACTING,

    /* Connection to the plugin has been lost and has not been cleaned up
     * yet. After clean up, the state becomes K3P_INITIALIZED. Note:
     * KMOD is expected to disconnect from the plugin as soon as an error
     * occur at the protocol level. This state indicates that an error
     * occurred, that the connection has been closed, and that a high-level
     * routine is expected to clean up eventually.
     */
    K3P_DISCONNECTED
};

/* K3P data element. */
struct k3p_element {
    
    /* Type of element. */
    int type;
    
    /* Instruction or integer value, if applicable, or the string length. */
    uint32_t value;
    
    /* String data, if applicable. */
    char *str;
};

/* This object handles the communication between the plugin and KMOD through
 * the K3P protocol.
 */
struct k3p_proto {
    
    /* State of the protocol. */
    int state;

    /* True if the timeout is currently used. */
    int timeout_enabled;

    /* Time to wait for the other side, in milliseconds. */
    uint32_t timeout;
    
    /* Array of incoming K3P elements. */
    karray element_array;
    
    /* Read position in the element array. We do not add new elements in the
     * array until all elements have been consumed.
     */
    int element_array_pos;
    
    /* Data buffer. When receiving, this buffer contains the data received from
     * the remote side that is being converted to K3P elements (in
     * element_array). When sending, this buffer contains the data to send to
     * the remote side. It is an error to use this buffer to send data when it
     * is already used to contain data being converted to K3P elements, or
     * vice-versa. The size of this buffer is shrunk after each transfer
     * operation.
     */
    kbuffer data_buf;
    
    /* This object describes the current transfer operation. */
    struct kmod_data_transfer transfer;
    
    /* Pointer to the transfer hub of KMOD. */
    struct kmod_transfer_hub *hub;
};

void k3p_proto_init(struct k3p_proto *k3p);
void k3p_proto_clean(struct k3p_proto *k3p);
void k3p_proto_disconnect(struct k3p_proto *k3p);
int k3p_perform_transfer(struct k3p_proto *k3p);
int k3p_receive_element(struct k3p_proto *k3p);
int k3p_read_inst(struct k3p_proto *k3p, uint32_t *i);
int k3p_read_uint32(struct k3p_proto *k3p, uint32_t *i);
int k3p_read_kstr(struct k3p_proto *k3p, kstr *str);
void k3p_write_inst(struct k3p_proto *k3p, uint32_t i);
void k3p_write_uint32(struct k3p_proto *k3p, uint32_t i);
void k3p_write_kstr(struct k3p_proto *k3p, kstr *str);
int k3p_send_data(struct k3p_proto *k3p);

/* This function writes a cstr to the remote side.
 * The data is not sent until a send operation is requested.
 */
static inline void k3p_write_cstr(struct k3p_proto *k3p, char *str) {
    kstr tmp;
    kstr_init_cstr(&tmp, str);
    k3p_write_kstr(k3p, &tmp);
    kstr_clean(&tmp);
}

/* This function shrinks the data buffer. */
static inline void k3p_shrink_data_buf(struct k3p_proto *k3p) {
    kbuffer_shrink(&k3p->data_buf, K3P_DATA_BUF_SIZE);
}

#endif

