/* Copyright (C) 2009-2012 Opersys inc., All rights reserved. */

#ifndef _LOG_LEVEL_H
#define _LOG_LEVEL_H

/* Critical events. */
#define KCD_LOG_CRIT                0x0001

/* Normal events. */
#define KCD_LOG_BRIEF               0x0002

/* Low-level command events. */
#define KCD_LOG_CMD                 0x0004

/* Low-level workspace events. */
#define KCD_LOG_KWS                 0x0008

/* Low-level KFS events. */
#define KCD_LOG_KFS                 0x0010

/* Low-level VNC events. */
#define KCD_LOG_VNC                 0x0020

/* Low-level Postgres events. */
#define KCD_LOG_PG                  0x0040

/* Low-level mail events. */
#define KCD_LOG_MAIL                0x0080

/* Low-level notification events. */
#define KCD_LOG_NOTIF               0x0100

/* Low-level KMOD events. */
#define KCD_LOG_KMOD                0x0200

/* Low-level miscellaneous events. */
#define KCD_LOG_MISC                0x0400

#endif

