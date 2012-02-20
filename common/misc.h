/* Copyright (C) 2006-2012 Opersys inc., All rights reserved. */

#ifndef _MISC_H
#define _MISC_H

struct kselect {
    fd_set read_set;
    fd_set write_set;
    fd_set error_set;
    int max_fd;
    struct timeval tv;
};

static inline void kselect_zero(struct kselect *self) {
    memset(self, 0, sizeof(struct kselect));
}

/* Add a socket in the read and error sets if it is not -1. */
static inline void kselect_add_read(struct kselect *self, int fd) {
    if (fd == -1) return;
    FD_SET((unsigned int) fd, &self->read_set);
    FD_SET((unsigned int) fd, &self->error_set);
    self->max_fd = MAX(fd, self->max_fd);
}

/* Add a socket in the write and error sets if it is not -1. */
static inline void kselect_add_write(struct kselect *self, int fd) {
    if (fd == -1) return;
    FD_SET((unsigned int) fd, &self->write_set);
    FD_SET((unsigned int) fd, &self->error_set);
    self->max_fd = MAX(fd, self->max_fd);
}

/* This function returns true if the socket is present in the read set or in the
 * error set. The function returns false if the socket is -1.
 */
static inline int kselect_in_read(struct kselect *self, int fd) {
    return (fd != -1 && (FD_ISSET(fd, &self->read_set) || FD_ISSET(fd, &self->error_set)));
}

/* This function returns true if the socket is present in the write set or in
 * the error set. The function returns false if the socket is -1.
 */
static inline int kselect_in_write(struct kselect *self, int fd) {
    return (fd != -1 && (FD_ISSET(fd, &self->write_set) || FD_ISSET(fd, &self->error_set)));
}

void kselect_wait(struct kselect *self);

#endif
