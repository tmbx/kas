/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* This function returns the name corresponding to the type specified. */
char* anp_type_name(enum anp_type type) {
    switch (type) {
        case ANP_UINT32: return "UINT32";
        case ANP_UINT64: return "UINT64";
        case ANP_STR: return "STR";
        case ANP_BIN: return "BIN";
        default: return "unknown type";
    }
}

struct anp_element* anp_element_new() {
    return (struct anp_element *) kcalloc(sizeof(struct anp_element));
}

void anp_element_clean(struct anp_element *self) {
    switch (self->type) {
        case ANP_STR:
            kstr_destroy(self->str);
            break;
        case ANP_BIN:
            kbuffer_destroy(self->bin);
            break;
        default: break;
    }
}

void anp_element_destroy(struct anp_element *self) {
    if (self) {
        anp_element_clean(self);
        kfree(self);
    }
}

/* This function adds a 32 bits unsigned integer to the buffer. */
void anp_write_uint32(kbuffer *buf, uint32_t i) {
    kbuffer_write8(buf, ANP_UINT32);
    kbuffer_write32(buf, i);
}

/* This function adds a 64 bits unsigned integer to the buffer. */
void anp_write_uint64(kbuffer *buf, uint64_t i) {
    kbuffer_write8(buf, ANP_UINT64);
    kbuffer_write64(buf, i);
}

/* This function adds a textual kstr to the buffer. */
void anp_write_kstr(kbuffer *buf, kstr *str) {
    kbuffer_write8(buf, ANP_STR);
    kbuffer_write32(buf, str->slen);
    kbuffer_write(buf, str->data, str->slen);
}

/* This function adds a C string to the buffer. */
void anp_write_cstr(kbuffer *buf, char *str) {
    int len = strlen(str);
    kbuffer_write8(buf, ANP_STR);
    kbuffer_write32(buf, len);
    kbuffer_write(buf, str, len);
}

/* This function adds a binary buffer to the buffer. 'bin' can be NULL. */
void anp_write_bin(kbuffer *buf, kbuffer *bin) {
    kbuffer_write8(buf, ANP_BIN);
    
    if (bin) {
        kbuffer_write32(buf, bin->len);
        kbuffer_write(buf, bin->data, bin->len);
    }
    
    else {
        kbuffer_write32(buf, 0);
    }
}

/* Helper method for anp_read_element(). */
static int anp_read_element_string(kbuffer *buf, kstr *str) {
    int error = 0;
    uint32_t len;
    
    do {
        error = kbuffer_read32(buf, &len);
        if (error) break;
        kstr_grow(str, len);
        error = kbuffer_read(buf, str->data, len);
        str->data[len] = 0;
        str->slen = len;
    } while (0);

    return error;
}
	
/* Helper method for anp_read_element(). */
static int anp_read_element_bin(kbuffer *buf, kbuffer *bin) {
    int error = 0;
    uint32_t len;
    
    do {
        error = kbuffer_read32(buf, &len);
        if (error) break;
        kbuffer_grow(bin, len);
        error = kbuffer_read(buf, bin->data, len);
        bin->pos = 0;
        bin->len = len;
    } while (0);

    return error;
}

/* This function reads the next element in the buffer. This function sets the
 * KMOD error string. It returns -1 on failure. If el is null just skip the
 * element.
 */
int anp_read_element(kbuffer *buf, struct anp_element *el) {
    int error = 0;
    uint8_t type;
    struct anp_element dummy;
    struct anp_element *work_el = &dummy;
    if (el != NULL)
        work_el = el;
	
    if (kbuffer_read8(buf, &type)) return -1;
    work_el->type = type;

    switch (work_el->type) {
        case ANP_UINT32:
            error = kbuffer_read32(buf, &work_el->uint32);
            break;
        case ANP_UINT64:
            error = kbuffer_read64(buf, &work_el->uint64);
            break;
        case ANP_STR:
            work_el->str = kstr_new();
            error = anp_read_element_string(buf, work_el->str);
            if (error) { kstr_destroy(work_el->str); work_el->str = NULL; }
            break;
        case ANP_BIN:
            work_el->bin = kbuffer_new();
            error = anp_read_element_bin(buf, work_el->bin);
            if (error) { kbuffer_destroy(work_el->bin); work_el->bin = NULL; }
            break;
    }

    if (el == NULL) anp_element_clean(&dummy);
    
    return error;
}

/* Helper function for the anp_read_* functions. It ensures that the next
 * value has the expected type.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
static int anp_read_ensure_type(kbuffer *buf, uint8_t expected_type) {
    uint8_t actual_type;
	
    if (kbuffer_read8(buf, &actual_type)) return -1;
    
    if (actual_type != expected_type) {
	kmod_set_error("expected type %s, got type %s (%d)", anp_type_name(expected_type), anp_type_name(actual_type), actual_type);
	return -1;
    }
    
    return 0;
}

/* This function reads a 32 bits unsigned integer from the buffer.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
int anp_read_uint32(kbuffer *buf, uint32_t *i) {
    int error = 0;
    
    do {
	error = anp_read_ensure_type(buf, ANP_UINT32);
	if (error) break;
	
	error = kbuffer_read32(buf, i);
	if (error) break;
	
    } while (0);
    
    if (error) {
    	kmod_append_error("cannot read UINT32 value in message");
    	return -1;
    }
    
    return 0;
}

/* This function reads a 64 bits unsigned integer from the buffer.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
int anp_read_uint64(kbuffer *buf, uint64_t *i) {
    int error = 0;
    
    do {
	error = anp_read_ensure_type(buf, ANP_UINT64);
	if (error) break;
	
	error = kbuffer_read64(buf, i);
	if (error) break;
	
    } while (0);
    
    if (error) {
    	kmod_append_error("cannot read UINT64 value in message");
    	return -1;
    }
    
    return 0;
}

/* This function reads a kstr of type STR from the buffer.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
int anp_read_kstr(kbuffer *buf, kstr *str) {
    int error = 0;
    uint32_t len;
    
    do {
	error = anp_read_ensure_type(buf, ANP_STR);
	if (error) break;
	
	error = kbuffer_read32(buf, &len);
	if (error) break;
	
	kstr_grow(str, len);
	error = kbuffer_read(buf, str->data, len);
	str->data[len] = 0;
	str->slen = len;
    	if (error) break;
	
    } while (0);
    
    if (error) {
    	kmod_append_error("cannot read STR value in message");
    	return -1;
    }
    
    return 0;
}

/* This function reads a buffer of type BIN from the buffer.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
int anp_read_bin(kbuffer *buf, kbuffer *bin) {
    int error = 0;
    uint32_t len;
    
    do {
	error = anp_read_ensure_type(buf, ANP_BIN);
	if (error) break;
	
	error = kbuffer_read32(buf, &len);
	if (error) break;
	
	kbuffer_grow(bin, len);
	error = kbuffer_read(buf, bin->data, len);
	bin->pos = 0;
	bin->len = len;
	if (error) break;

    } while (0);
    
    if (error) {
    	kmod_append_error("cannot read BIN value in message");
    	return -1;
    }
    
    return 0;
}

/* This function dumps the content of a ANP message buffer in the string
 * specified. This function sets the KMOD error string when it encounters an
 * error in the buffer. It returns -1 on failure.
 */
int anp_dump(kbuffer *buf, kstr *dump_str) {
    int error = 0;
    kstr work_str;
    kstr data_str;
    kbuffer bin_buf;
    
    kstr_init(&work_str);
    kstr_init(&data_str);
    kbuffer_init(&bin_buf);
    kstr_reset(dump_str);

    while (! kbuffer_eof(buf)) {
	uint8_t type = buf->data[buf->pos];
	
	if (type == ANP_UINT32) {
	    uint32_t val;
	    error = anp_read_uint32(buf, &val);
	    if (error) break;
	    
	    kstr_sf(&work_str, "uint32> %u\n", val);
	    kstr_append_kstr(dump_str, &work_str);
	}
	
	else if (type == ANP_UINT64) {
	    uint64_t val;
	    error = anp_read_uint64(buf, &val);
	    if (error) break;
	    
	    kstr_sf(&work_str, "uint64> "PRINTF_64"u\n", val);
	    kstr_append_kstr(dump_str, &work_str);
	}
	
	else if (type == ANP_STR) {
	    error = anp_read_kstr(buf, &data_str);
	    if (error) break;
	
	    kstr_sf(&work_str, "string %u> ", data_str.slen);
	    kstr_append_kstr(dump_str, &work_str);
	    kstr_append_kstr(dump_str, &data_str);
	    kstr_append_cstr(dump_str, "\n");
	}
	
	else if (type == ANP_BIN) {
	    error = anp_read_bin(buf, &bin_buf);
	    if (error) break;
	
	    kstr_sf(&work_str, "binary %u> ", bin_buf.len);
	    kstr_append_kstr(dump_str, &work_str);
	    kstr_append_cstr(dump_str, "\n");
	}
	
	else {
	    kmod_set_error("invalid ANP identifier (%u)\n", type);
	    error = -1;
	    break;
	}
    }
    
    kstr_clean(&work_str);
    kstr_clean(&data_str);
    kbuffer_clean(&bin_buf);
    
    /* Reset the buffer position to 0. */
    buf->pos = 0;
    
    return error;
}

struct anp_msg* anp_msg_new() {
    struct anp_msg *self = (struct anp_msg *) kcalloc(sizeof(struct anp_msg));
    kbuffer_init(&self->payload);
    karray_init(&self->element_array);
    return self;
}

void anp_msg_destroy(struct anp_msg *self) {
    if (self) {
        anp_msg_clear_payload(self);
        karray_clean(&self->element_array);
	kbuffer_clean(&self->payload);
	kfree(self);
    }
}

/* This function clears the payload of this message. */
void anp_msg_clear_payload(struct anp_msg *self) {
    struct karray_iter iter;
    struct anp_element *el;
    karray_iter_init(&iter, &self->element_array);
    while (kiter_next(&iter, &el) == 0) anp_element_destroy(el);
    karray_reset(&self->element_array);
    kbuffer_reset(&self->payload);
}

/* This function parses the elements contained in 'buf' and replaces the
 * payload of this message with those elements.
 */
int anp_msg_parse(struct anp_msg *self, kbuffer *buf) {
    int error = 0;
    
    do {
        /* Reset the payload and the element array. */
        anp_msg_clear_payload(self);
        
        /* Parse the elements. */
        buf->pos = 0;
        
        while (buf->pos < buf->len) {
            struct anp_element el;
            
            error = anp_read_element(buf, &el);
            if (error) break;
            
            switch (el.type) {
                case ANP_UINT32:
                    anp_msg_write_uint32(self, el.uint32);
                    break;
                case ANP_UINT64:
                    anp_msg_write_uint64(self, el.uint64);
                    break;
                case ANP_STR:
                    anp_msg_write_kstr(self, el.str);
                    break;
                case ANP_BIN:
                    anp_msg_write_bin(self, el.bin);
                    break;
            }
            
            anp_element_clean(&el);
        }
        
        if (error) break;
        
    } while (0);

    return error;
}

/* This function adds a message to the buffer specified. The buffer is not
 * cleared prior to the message being added.
 */
void anp_msg_to_buf(struct anp_msg *msg, kbuffer *buf) {
    kbuffer_write32(buf, msg->major);
    kbuffer_write32(buf, msg->minor);
    kbuffer_write32(buf, msg->type);
    kbuffer_write64(buf, msg->id);
    kbuffer_write32(buf, msg->payload.len);
    kbuffer_write(buf, msg->payload.data, msg->payload.len);
}

/* Same as anp_dump(), but the header of the message is also dumped. */
int anp_msg_dump(struct anp_msg *self, kstr *dump_str) {
    int error = 0;
    kstr work_str;
    
    kstr_init(&work_str);
    kstr_reset(dump_str);

    kstr_sf(&work_str, "major> %u\n", self->major);
    kstr_append_kstr(dump_str, &work_str);

    kstr_sf(&work_str, "minor> %u\n", self->minor);
    kstr_append_kstr(dump_str, &work_str);

    kstr_sf(&work_str, "type> %u\n", self->minor);
    kstr_append_kstr(dump_str, &work_str);

    kstr_sf(&work_str, "id> "PRINTF_64"u\n", self->minor);
    kstr_append_kstr(dump_str, &work_str);

    if (anp_dump(&self->payload, &work_str)) {
        error = -1;
    } else {
        kstr_append_kstr(dump_str, &work_str);
    }

    kstr_clean(&work_str);
    return error;
}

/* The following functions perform the same job as their anp_write_* and
 * anp_read_* counterparts.
 */
void anp_msg_write_uint32(struct anp_msg *self, uint32_t i) {
    struct anp_element *el = anp_element_new();
    el->type = ANP_UINT32;
    el->uint32 = i;
    karray_push(&self->element_array, el);
    anp_write_uint32(&self->payload, el->uint32);
}

void anp_msg_write_uint64(struct anp_msg *self, uint64_t i) {
    struct anp_element *el = anp_element_new();
    el->type = ANP_UINT64;
    el->uint64 = i;
    karray_push(&self->element_array, el);
    anp_write_uint64(&self->payload, el->uint64);
}

void anp_msg_write_kstr(struct anp_msg *self, kstr *str) {
    struct anp_element *el = anp_element_new();
    el->type = ANP_STR;
    el->str = kstr_new();
    kstr_assign_kstr(el->str, str);
    karray_push(&self->element_array, el);
    anp_write_kstr(&self->payload, el->str);
}

void anp_msg_write_cstr(struct anp_msg *self, char *str) {
    struct anp_element *el = anp_element_new();
    el->type = ANP_STR;
    el->str = kstr_new();
    kstr_assign_cstr(el->str, str);
    karray_push(&self->element_array, el);
    anp_write_kstr(&self->payload, el->str);
}

/* 'bin' can be NULL'. */
void anp_msg_write_bin(struct anp_msg *self, kbuffer *bin) {
    struct anp_element *el = anp_element_new();
    el->type = ANP_BIN;
    el->bin = kbuffer_new();
    if (bin) kbuffer_write_buffer(el->bin, bin);
    karray_push(&self->element_array, el);
    anp_write_bin(&self->payload, el->bin);
}

int anp_msg_read_uint32(struct anp_msg *self, uint32_t *i) {
    return anp_msg_get_uint32(self, self->pos++, i);
}

int anp_msg_read_uint64(struct anp_msg *self, uint64_t *i) {
    return anp_msg_get_uint64(self, self->pos++, i);
}

int anp_msg_read_kstr(struct anp_msg *self, kstr *str) {
    return anp_msg_get_kstr(self, self->pos++, str);
}

int anp_msg_read_bin(struct anp_msg *self, kbuffer *bin) {
    return anp_msg_get_bin(self, self->pos++, bin);
}

/* Helper function for the anp_msg_get_* functions. It ensures that the value
 * has the expected type.
 * This function sets the KMOD error string. It returns -1 on failure.
 */
static int anp_msg_get_ensure_type(struct anp_element *el, int pos, uint8_t expected_type) {
    if (! el) {
        kmod_set_error("there is no %s element at position %d", anp_type_name(expected_type), pos);
        return -1;
    }
    
    if (el->type != expected_type) {
        kmod_set_error("expected element of type %s at position %d, got type %s", anp_type_name(expected_type), pos,
                                                                                  anp_type_name(el->type));
        return -1;
    }
    
    return 0;
}

/* This function gets the element at pos in an anp_msg as a 32 bits unsigned
 * integer. This function sets the KMOD error string. It returns -1 on failure.
 */
int anp_msg_get_uint32(struct anp_msg *self, int pos, uint32_t *i) {
    struct anp_element *el = (struct anp_element *)karray_get(&self->element_array, pos);
    if (anp_msg_get_ensure_type(el, pos, ANP_UINT32)) return -1;
    *i = el->uint32;
    return 0;
}

/* This function gets the element at pos in an anp_msg as a 64 bits unsigned
 * integer. This function sets the KMOD error string. It returns -1 on failure.
 */
int anp_msg_get_uint64(struct anp_msg *self, int pos, uint64_t *i) {
    struct anp_element *el = (struct anp_element *) karray_get(&self->element_array, pos);
    if (anp_msg_get_ensure_type(el, pos, ANP_UINT64)) return -1;
    *i = el->uint64;
    return 0;
}

/* This function gets the element at pos in an anp_msg as a string. This
 * function sets the KMOD error string. It returns -1 on failure.
 */
int anp_msg_get_kstr(struct anp_msg *self, int pos, kstr *str) {
    struct anp_element *el = (struct anp_element *) karray_get(&self->element_array, pos);
    if (anp_msg_get_ensure_type(el, pos, ANP_STR)) return -1;
    kstr_assign_kstr(str, el->str);
    return 0;
}

/* This function gets the element at pos in an anp_msg as a bin. This
 * function sets the KMOD error string. It returns -1 on failure.
 */
int anp_msg_get_bin(struct anp_msg *self, int pos, kbuffer *bin) {
    struct anp_element *el = (struct anp_element *) karray_get(&self->element_array, pos);
    if (anp_msg_get_ensure_type(el, pos, ANP_BIN)) return -1;
    bin->pos = 0;
    kbuffer_write_buffer(bin, el->bin);
    return 0;
}

