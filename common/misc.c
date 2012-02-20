/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#include "common.h"

/* Helper function for kselect_wait(). It turns out the kernel doesn't clear the
 * select sets when select() returns EINTR.
 */
static void kselect_interrupt_clear(struct kselect *self) {
    FD_ZERO(&self->read_set);
    FD_ZERO(&self->write_set);
    FD_ZERO(&self->error_set);
}

void kselect_wait(struct kselect *self) {
    int error = select(self->max_fd + 1, &self->read_set, &self->write_set, &self->error_set, &self->tv);

    if (error < 0) {
	
	#ifdef __UNIX__
	/* Ignore EINTR. */
	if (errno == EINTR) {
	    kselect_interrupt_clear(self);
	    return;
	}
	#else
	if (WSAGetLastError() == WSAEINTR || WSAGetLastError() == WSAEINPROGRESS) {
	    kselect_interrupt_clear(self);
	    return;
        }
	#endif
	
	/* We can't handle other errors. */
	kerror_fatal("select() failed: %s", kmod_neterror());
    }
}
