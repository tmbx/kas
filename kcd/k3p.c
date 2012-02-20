/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* This function returns a string describing the K3P element specified. */
static inline char * k3p_get_element_desc(int type) {
    switch (type) {
    	case K3P_INS: return "instruction";
	case K3P_INT: return "integer";
	case K3P_STR: return "textual string";
	default: return "unknown element type";
    }
    
    return NULL;
}

/* This function destroys the K3P element specified. */
static inline void k3p_element_destroy(struct k3p_element *el) {
    if (el == NULL) return;
    
    if (el->str != NULL) {
    	kfree(el->str);
    }
    
    kfree(el);
}

/* This function initializes the K3P communication object. */
void k3p_proto_init(struct k3p_proto *k3p) {
    memset(k3p, 0, sizeof(struct k3p_proto));
    k3p->state = K3P_INITIALIZED;
    k3p->timeout_enabled = 1;
    karray_init(&k3p->element_array);
    kbuffer_init(&k3p->data_buf);
    kmod_data_transfer_init(&k3p->transfer);
}

/* This function frees the K3P communication object. */
void k3p_proto_clean(struct k3p_proto *k3p) {
    if (k3p == NULL) return;
    k3p_proto_disconnect(k3p);
    karray_clean(&k3p->element_array);
    kbuffer_clean(&k3p->data_buf);
    kmod_data_transfer_clean(&k3p->transfer);
}

/* This function disconnects KMOD from the remote side, if it is connected. */
void k3p_proto_disconnect(struct k3p_proto *k3p) {
    kmod_log_msg(KCD_LOG_KMOD, "k3p_proto_disconnect() called.\n");
     
    /* If we're clean, return. */
    if (k3p->state == K3P_INITIALIZED) {
    	return;
    }
    
    /* Close connection, if needed. */
    if (k3p->transfer.fd != -1) {
    	k3p->transfer.driver.disconnect(&k3p->transfer.fd);
    }
    
    /* Destroy all incoming K3P elements. */
    while (k3p->element_array_pos < k3p->element_array.size) {
    	k3p_element_destroy((struct k3p_element *) k3p->element_array.data[k3p->element_array_pos]);
	k3p->element_array_pos++;
    }
    
    k3p->element_array_pos = k3p->element_array.size = 0;
    
    /* Shrink the data buffer. */
    k3p_shrink_data_buf(k3p);
    
    /* Look 'ma! All clean! */
    k3p->state = K3P_INITIALIZED;
}

/* This function adds the K3P transfer in the hub, waits for it to complete and
 * removes the transfer from the hub.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
int k3p_perform_transfer(struct k3p_proto *k3p) {
    int error = 0;
    
    kmod_log_msg(KCD_LOG_KMOD, "k3p_perform_transfer() called.\n");
    
    /* Add the transfer. */
    kmod_transfer_hub_add(k3p->hub, &k3p->transfer);
    
    /* Loop until the transfer is done. */
    while (1) {
    	kmod_transfer_hub_wait(k3p->hub);
	
	if (k3p->transfer.status == KMOD_DATA_TRANS_COMPLETED) {
	    break;
	}
	
	else if (k3p->transfer.status == KMOD_DATA_TRANS_ERROR) {
	    kmod_set_error(kmod_data_transfer_err(&k3p->transfer));
	    error = -1;
	    break;
	}
	
	else {
	    assert(k3p->transfer.status == KMOD_DATA_TRANS_PENDING);
	}
    }
    
    /* Remove the transfer. */
    kmod_transfer_hub_remove(k3p->hub, &k3p->transfer);
    return error;
}

/* This function increases the data buffer size by at least 'n' bytes.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
static int k3p_extend_data_buf(struct k3p_proto *k3p, uint32_t n) {
    kmod_log_msg(KCD_LOG_KMOD, "k3p_extend_data_buf() called.\n");
    
    k3p->transfer.read_flag = 1;
    k3p->transfer.min_len = n;
    k3p->transfer.max_len = MAX(n, K3P_DATA_BUF_SIZE);
    k3p->transfer.op_timeout = k3p->timeout_enabled ? k3p->timeout : 0;
    k3p->transfer.buf = kbuffer_begin_write(&k3p->data_buf, k3p->transfer.max_len);
    
    if (k3p_perform_transfer(k3p)) {
	kmod_append_error("cannot read incoming data from plugin");
    	return -1;
    }
    
    kbuffer_end_write(&k3p->data_buf, k3p->transfer.trans_len);
    return 0;
}

/* This function parses a K3P instruction. 
 * This function sets the KMOD error string. It returns -1 on failure.
 */
static int k3p_parse_ins(struct k3p_proto *k3p) {
    struct k3p_element *el = (struct k3p_element *) kcalloc(sizeof(struct k3p_element));
    el->type = K3P_INS;
    
    kmod_log_msg(KCD_LOG_KMOD, "k3p_parse_ins() called.\n");
    
    /* Try. */
    do {
    	char buf[9];

	/* Fetch the 8 bytes for the number. */
	if (k3p->data_buf.pos + 8 > k3p->data_buf.len)
    	    if (k3p_extend_data_buf(k3p, k3p->data_buf.pos + 8 - k3p->data_buf.len))
	    	break;
	
	kbuffer_read(&k3p->data_buf, buf, 8);
	buf[8] = 0;
	
	/* Try to parse the content. */
	if (sscanf(buf, "%x", &el->value) < 1) {
    	    kmod_set_error("bad instruction format");
	    break;
	}
    	
	/* Queue the element. */
	karray_push(&k3p->element_array, el);
	return 0;
    
    } while (0);
    
    kmod_append_error("cannot parse instruction");
    kfree(el);
    return -1;
}

/* This function parses a number up to the '>' delimiter.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
static int k3p_parse_number(struct k3p_proto *k3p, uint32_t *num) {
    int i = 0;
    
    kmod_log_msg(KCD_LOG_KMOD, "k3p_parse_number() called.\n");
    
    /* Try to find the delimiter. */
    while (1) {
    	char c;
	
	if (k3p->data_buf.pos + i == k3p->data_buf.len && k3p_extend_data_buf(k3p, 1))
    	    return -1;
	
	assert(k3p->data_buf.pos + i < k3p->data_buf.len);
	c = k3p->data_buf.data[k3p->data_buf.pos + i];
	
	if (c == '>') {
	    if (i == 0) {
	    	kmod_set_error("expected number before '>'");
		return -1;
	    }
	
	    break;
	}
	
	else if (i == 10) {
	    kmod_set_error("expected '>' after number");
	    return -1;
	}
	
	else if (c < '0' || c > '9') {
	    kmod_set_error("unexpected character (%d) in number", c);
	    return -1;
	}
	
	i++;
    }
    
    /* Replace the delimiter by a zero, to get a NULL-terminated string. */
    k3p->data_buf.data[k3p->data_buf.pos + i] = 0;
    
    /* Try to parse the number. */
    if (sscanf(k3p->data_buf.data + k3p->data_buf.pos, "%u", num) < 1) {
    	kmod_set_error("bad number format");
	return -1;
    }
    
    /* Skip the number and the delimiter. */
    k3p->data_buf.pos += i + 1;
    return 0;
}

/* This function parses a K3P integer. 
 * This function sets the KMOD error string. It returns -1 on failure.
 */
static int k3p_parse_int(struct k3p_proto *k3p) {
    struct k3p_element *el = (struct k3p_element *) kcalloc(sizeof(struct k3p_element));
    el->type = K3P_INT;
    
    kmod_log_msg(KCD_LOG_KMOD, "k3p_parse_int() called.\n");
    
    if (k3p_parse_number(k3p, &el->value)) {
	kmod_append_error("cannot parse integer");
    	kfree(el);
	return -1;
    }
    
    karray_push(&k3p->element_array, el);
    return 0;
}

/* This function parses a K3P textual string.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
static int k3p_parse_str(struct k3p_proto *k3p) {
    struct k3p_element *el = (struct k3p_element *) kcalloc(sizeof(struct k3p_element));
    el->type = K3P_STR;
    
    kmod_log_msg(KCD_LOG_KMOD, "k3p_parse_str() called.\n");
    
    /* Try. */
    do {
    	if (k3p_parse_number(k3p, &el->value))
	    break;
	
	/* Larger than 100 MB? Ouch! */
	if (el->value > 100*1024*1024) {
	    kmod_set_error("textual string is too large (%d bytes)", el->value);
	    break;
	}
	
	if (el->value) {	    
	    if (k3p->data_buf.pos + el->value > k3p->data_buf.len)
    		if (k3p_extend_data_buf(k3p, k3p->data_buf.pos + el->value - k3p->data_buf.len))
	    	    break;

	    el->str = (char *) kmalloc(el->value);
	    kbuffer_read(&k3p->data_buf, el->str, el->value);
	}
	
	karray_push(&k3p->element_array, el);
	return 0;
    
    } while (0);
    
    kmod_append_error("cannot parse textual string");
    k3p_element_destroy(el);
    return -1;
}

/* This function receives at least one element from the remote side. The
 * function stops receiving elements when there is no more data left in the data
 * buffer.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
int k3p_receive_element(struct k3p_proto *k3p) {
    int error = 0;
    
    kmod_log_msg(KCD_LOG_KMOD, "k3p_receive_element() called.\n");
    
    /* Normally, in this context, we have consumed all the elements in the
     * element array. Anything else means the logic is wrong.
     */
    assert(k3p->element_array_pos == k3p->element_array.size);
    k3p->element_array_pos = k3p->element_array.size = 0;
    
    /* Try. */
    do {
	/* If the data buffer is empty, obtain some data for the first element. */
	if (k3p->data_buf.len == 0) {
    	    error = k3p_extend_data_buf(k3p, 5);
	    if (error) break;
	}

	/* Read the elements. */
	while (1) {
	    char elem_type[4];
    	    
	    /* If we're at the end of the data buffer, we're done. */
	    if (k3p->data_buf.pos == k3p->data_buf.len) {
		assert(k3p->element_array.size > 0);
		break;
	    }

	    /* Complete the element, if needed. */
	    if (k3p->data_buf.pos + 5 > k3p->data_buf.len) {
		error = k3p_extend_data_buf(k3p, k3p->data_buf.pos + 5 - k3p->data_buf.len);
		if (error) break;
	    }

	    /* Read one element. */
	    kbuffer_read(&k3p->data_buf, elem_type, 3);			
	    elem_type[3] = 0;

	    if (strcmp(elem_type, "INT") == 0) {
		error = k3p_parse_int(k3p);
		if (error) break;
	    }

	    else if (strcmp(elem_type, "INS") == 0) {
		error = k3p_parse_ins(k3p);
		if (error) break;
	    }

	    else if (strcmp(elem_type, "STR") == 0) {
		error = k3p_parse_str(k3p);
		if (error) break;
	    }
	    
	    else {
		kmod_set_error("invalid K3P element type (%s)", elem_type);
		error = -1;
		break;
	    }
	}
	
	if (error) break;
	
    } while (0);
    
    /* Disconnect if an error occurred. */
    if (error) k3p_proto_disconnect(k3p);
    
    /* Shrink the data buffer. */
    k3p_shrink_data_buf(k3p);
    
    return error;
}

/* This function reads the element specified in the memory location specified.
 * It is used to compact the code.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
static int k3p_consume_next_element(struct k3p_proto *k3p, int type, void *loc) {
    int error = 0;
    struct k3p_element *el = NULL;
    
    kmod_log_msg(KCD_LOG_KMOD, "k3p_consume_next_element() called.\n");
    
    /* Try. */
    do {
    	/* Read more elements, if needed. */
    	if (k3p->element_array_pos == k3p->element_array.size) {
	    k3p->element_array_pos = k3p->element_array.size = 0;
	    error = k3p_receive_element(k3p);
	    if (error) break;
	}
	
	el = (struct k3p_element *) k3p->element_array.data[k3p->element_array_pos];
	k3p->element_array_pos++; 
    	
	if (el->type != type) {
	    kmod_set_error("expected K3P type %s, got %s", k3p_get_element_desc(type), k3p_get_element_desc(el->type));
	    error = -1;
	    break;
	}
	
	if (type == K3P_INS || type == K3P_INT) {
	    *(uint32_t *) loc = el->value;
	}
	
	else {
	    kstr_assign_buf((kstr *) loc, el->str, el->value);
	}
	
    } while (0);
    
    if (error) k3p_proto_disconnect(k3p);

    k3p_element_destroy(el);
    return error;
}

/* This function reads an instruction from the remote side.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
int k3p_read_inst(struct k3p_proto *k3p, uint32_t *i) {
    kmod_log_msg(KCD_LOG_KMOD, "k3p_read_inst() called.\n");
    return k3p_consume_next_element(k3p, K3P_INS, i);
}

/* This function reads a 32 bit unsigned integer from the remote side.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
int k3p_read_uint32(struct k3p_proto *k3p, uint32_t *i) {
    kmod_log_msg(KCD_LOG_KMOD, "k3p_read_uint32() called.\n");
    return k3p_consume_next_element(k3p, K3P_INT, i);
}

/* This function reads a textual kstr from the remote side.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
int k3p_read_kstr(struct k3p_proto *k3p, kstr *str) {
    kmod_log_msg(KCD_LOG_KMOD, "k3p_read_kstr() called.\n");
    return k3p_consume_next_element(k3p, K3P_STR, str);
}

/* This function writes an instruction to the remote side.
 * The data is not sent until a send operation is requested.
 */
void k3p_write_inst(struct k3p_proto *k3p, uint32_t i) {
    char buf[12];
    int len = sprintf(buf, "INS%08x", i);
    
    kmod_log_msg(KCD_LOG_KMOD, "k3p_write_inst() called.\n");
    
    assert(len == 11);
    len = 0;
    kbuffer_write(&k3p->data_buf, buf, 11);
}

/* This function writes a 32 bit unsigned integer to the remote side.
 * The data is not sent until a send operation is requested.
 */
void k3p_write_uint32(struct k3p_proto *k3p, uint32_t i) {
    char buf[15];
    int len = sprintf(buf, "INT%u>", i);
    
    kmod_log_msg(KCD_LOG_KMOD, "k3p_write_uint32() called.\n");
    
    assert(len <= 14);
    kbuffer_write(&k3p->data_buf, buf, len);
}

/* This function writes a textual kstr to the remote side.
 * The data is not sent until a send operation is requested.
 */
void k3p_write_kstr(struct k3p_proto *k3p, kstr *str) {
    char buf[15];
    int len = sprintf(buf, "STR%u>", str->slen);
    
    kmod_log_msg(KCD_LOG_KMOD, "k3p_write_kstr() called.\n");
    
    assert(len <= 14);
    kbuffer_write(&k3p->data_buf, buf, len);
    kbuffer_write(&k3p->data_buf, str->data, str->slen);
}

/* This function sends the data written to the remote side.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
int k3p_send_data(struct k3p_proto *k3p) {
    kmod_log_msg(KCD_LOG_KMOD, "k3p_send_data() called.\n");
    
    k3p->transfer.read_flag = 0;
    k3p->transfer.buf = k3p->data_buf.data;
    k3p->transfer.min_len = k3p->data_buf.len;
    k3p->transfer.max_len = k3p->data_buf.len;
    k3p->transfer.op_timeout = k3p->timeout_enabled ? k3p->timeout : 0;
    
    if (k3p_perform_transfer(k3p)) {
	kmod_append_error("cannot write outgoing data to plugin");
    	k3p_proto_disconnect(k3p);
    	return -1;
    }
    
    k3p_shrink_data_buf(k3p);
    return 0;
}
