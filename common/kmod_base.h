/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _KMOD_BASE_H
#define _KMOD_BASE_H

/* System includes. */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

/* Platform-specific includes. */
#ifdef __WINDOWS__
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "ktools.h"

/* This macro sets the thread-local error message. All previously stacked
 * messages are cleared.
 */
#define kmod_set_error(...) \
    kerror_reset(); \
    kerror_push(kerror_node_new(__FILE__, __LINE__, __FUNCTION__, 1, 0, __VA_ARGS__))
    
/* This macro appends an error message to the thread-local error messages
 * already set.
 */
#define kmod_append_error(...) \
    kerror_push(kerror_node_new(__FILE__, __LINE__, __FUNCTION__, 1, 0, __VA_ARGS__))
    
/* This function returns true if the character specified is a whitespace. */
static inline int kmod_is_whitespace(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

void kmod_base_init();
void kmod_base_clean();
char * kmod_strerror();
kstr * kmod_kstrerror();
char * kmod_syserror();
char * kmod_neterror();
void kmod_clear_kstr_array(karray *array);
int kmod_generate_random(char *buf, int len);
void kmod_dump_buf_ascii(unsigned char *buf, int n, FILE *stream);
void kmod_bin_to_hex(unsigned char *in, int n, kstr *out);
char * portable_strcasestr(const char *haystack, const char *needle);
char * reverse_strcasestr(const char *start, const char *haystack, const char *needle);

#endif
