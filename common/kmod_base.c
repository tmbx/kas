#include "kmod_base.h"

/* This variable contains the last formatted error string obtained through
 * kmod_strerror().
 */
static kstr last_error_msg;

/* This function initializes the KMOD base module. */
void kmod_base_init() {
    kstr_init(&last_error_msg);
}

/* This function cleans up the KMOD base module. */
void kmod_base_clean() {
    kstr_clean(&last_error_msg);
}

/* This function updates the last_error_msg string from the current error stack. */
static void kmod_update_last_error_msg(kstr *err_str) {
    struct kerror *error_instance = kerror_get_current();
    int i;
    
    kstr_reset(err_str);

    if (error_instance->stack.size == 0) {
        kstr_assign_cstr(err_str, "unknown error");
    }
    
    else {
	for (i = error_instance->stack.size - 1; i >= 0; i--) {
	    struct kerror_node *node = (struct kerror_node *) karray_get(&error_instance->stack, i);
	    
	    if (i != error_instance->stack.size - 1) {
		kstr_append_cstr(err_str, ": ");
	    }
	    
	    kstr_append_kstr(err_str, &node->text);
	}
    }
}

/* This function formats the current error stack into an arguably pretty error
 * string and returns that string. The pointer to the string returned is always
 * valid, but the underlying string is modified every time this function is
 * called.
 */
char * kmod_strerror() {
    return kmod_kstrerror()->data;
}

/* Same as above, but a pointer to a kstr is returned instead. */
kstr * kmod_kstrerror() {
    
    if (ktools_use_mt) {
    	struct kthread_specific *s = kthread_get_specific();
	kmod_update_last_error_msg(&s->err_str);
	return &s->err_str;
    }
    
    else {
    	kmod_update_last_error_msg(&last_error_msg);
	return &last_error_msg;
    }
}

/* This function returns the system error string that is currently set, or NULL
 * if there is none.
 */
char * kmod_syserror() {
    return (errno ? strerror(errno) : NULL);
}

/* This function returns a string corresponding to the last network error. */
char * kmod_neterror() {
    #ifdef __WINDOWS__
    return strerror(WSAGetLastError());
    #else
    return kmod_syserror();
    #endif
}

/* This function deletes the kstr present in the array specified, then it resets
 * the size of the array.
 */
void kmod_clear_kstr_array(karray *array) {
    int i;

    for (i = 0; i < array->size; i++) {
    	kstr_destroy((kstr *) array->data[i]);
    }
	
    array->size = 0;
}

/* This function dumps the content of a buffer on the stream specified, in
 * ASCII. A newline is inserted after 20 characters have been printed on a line.
 */
void kmod_dump_buf_ascii(unsigned char *buf, int n, FILE *stream) {
    int i;
    
    for (i = 0; i < n; i++) {
	if (i > 0 && i % 20 == 0) fprintf(stream, "\n");
	else if (i % 20) fprintf(stream, " ");
	
	if (buf[i] == '\n') fprintf(stream, "\\n");
	else if (buf[i] == '\r') fprintf(stream, "\\r");
	else fprintf(stream, "%c ", buf[i]);
    }
}
	
/* This function converts a binary buffer to an hexadecimal kstr. */
void kmod_bin_to_hex(unsigned char *in, int n, kstr *out) {
    static unsigned char hex_table[16] =
    	{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    
    int i;
    
    kstr_grow(out, 2*n);
    
    for (i = 0; i < n; i++) {
	out->data[i*2] = hex_table[in[i] >> 4];
	out->data[i*2 + 1] = hex_table[in[i] & 0xf];
    }
    
    out->data[2*n] = 0;
}

/* This function implements a portable version of strcasestr(). */
char * portable_strcasestr(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    const char *last_start = haystack + strlen(haystack) - needle_len;

    while (haystack <= last_start) {
	if (portable_strncasecmp(haystack, needle, needle_len) == 0)
	    return (char *) haystack;
    
    	haystack++;
    }

    return NULL;
}

/* This function looks for 'needle' inside 'haystack' like in
 * portable_strcasestr(), except that the function scans backward inside
 * 'haystack' until either 'needle' is found or 'start' is passed.
 */
char * reverse_strcasestr(const char *start, const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    
    while (haystack >= start) {
	if (portable_strncasecmp(haystack, needle, needle_len) == 0)
	    return (char *) haystack;
    
    	haystack--;
    }
    
    return NULL;
}

