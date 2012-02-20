/* VNC Reflector
 * Copyright (C) 2001-2004 HorizonLive.com, Inc.  All rights reserved.
 *
 * This software is released under the terms specified in the file LICENSE,
 * included.  HorizonLive provides e-Learning and collaborative synchronous
 * presentation solutions in a totally Web-based environment.  For more
 * information about HorizonLive, please see our website at
 * http://www.horizonlive.com.
 *
 * This software was authored by Constantin Kaplinsky <const@ce.cctpu.edu.ru>
 * and sponsored by HorizonLive.com, Inc.
 *
 * $Id: control.c,v 1.13 2004/08/08 15:23:35 const_k Exp $
 * Processing signals to control reflector
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <zlib.h>

#include "rfblib.h"
#include "async_io.h"
#include "logging.h"
#include "reflector.h"
#include "host_connect.h"
#include "host_io.h"
#include "translate.h"
#include "client_io.h"

#define FUNC_CL_DISCONNECT   0
#define FUNC_HOST_RECONNECT  1

static void sh_disconnect_clients(int signo);
static void sh_reconnect_noclose(int signo);
static void sh_reconnect_close(int signo);
static void sh_toggle_log_level(int signo);

static void safe_disconnect_clients(void);
static void safe_reconnect_noclose(void);
static void safe_reconnect_close(void);

static void fn_close(AIO_SLOT *slot);
static void fn_stop_listening(AIO_SLOT *slot);
static void fn_reconnect_close(AIO_SLOT *slot);

/*
 * Function visible from outside
 */

void set_control_signals(void)
{
  signal(SIGHUP, sh_disconnect_clients);
  signal(SIGUSR1, sh_reconnect_noclose);
  signal(SIGUSR2, sh_reconnect_close);
  signal(SIGWINCH, sh_toggle_log_level);
}

/*
 * Signal handlers
 */

static void sh_disconnect_clients(int signo)
{
  aio_call_func(safe_disconnect_clients, FUNC_CL_DISCONNECT);
  signal(signo, sh_disconnect_clients);
}

static void sh_reconnect_noclose(int signo)
{
  aio_call_func(safe_reconnect_noclose, FUNC_HOST_RECONNECT);
  signal(signo, sh_reconnect_noclose);
}

static void sh_reconnect_close(int signo)
{
  aio_call_func(safe_reconnect_close, FUNC_HOST_RECONNECT);
  signal(signo, sh_reconnect_close);
}

/*
 * sh_toggle_log_level() is called when the application handles
 * the SIGWINCH signal (see set_control_signals() above).
 *
 * The first time sh_toggle_log_level() is called it will set the
 * file and stderr log levels to LL_DEBUG. Subsequent calls will
 * toggle between the original settings and debug settings.
 *
 * Usage: kill -SIGWINCH <vncreflector_pid>
 *
 */

static void sh_toggle_log_level(int signo)
{
  /* Cache the previous/next level. */
  static int next_file_level = LL_DEBUG;
  static int next_stderr_level = LL_DEBUG;

  /* Log the signal. */
  log_write(LL_WARN, "Toggling log level: file => %d, stderr => %d",
            next_file_level, next_stderr_level);

  /* Set the new log levels, and cache the old ones. */
  next_file_level = log_set_file_level(next_file_level);
  next_stderr_level = log_set_stderr_level(next_stderr_level);

  /* Reset the signal. */
  signal(signo, sh_toggle_log_level);
}

/*
 * Disconnecting all clients on SIGHUP
 */

static void safe_disconnect_clients(void)
{
  log_write(LL_WARN, "Caught SIGHUP signal, disconnecting all clients");
  aio_walk_slots(fn_close, TYPE_CL_SLOT);
}

/*
 * On SIGUSR1: reconnect and close current host connection only when
 * new connection is successful. Note: socket listening for host
 * connections would be closed anyway, otherwise bind(2) on the same
 * port would fail. Also, all non-authenticated host connections
 * would be closed as well.
 */

static void safe_reconnect_noclose(void)
{
  log_write(LL_WARN, "Caught SIGUSR1 signal, trying to (re)connect to host");

  aio_walk_slots(fn_stop_listening, TYPE_HOST_LISTENING_SLOT);
  aio_walk_slots(fn_close, TYPE_HOST_CONNECTING_SLOT);

  connect_to_host(NULL, 0);
}

/*
 * On SIGUSR2: close current host connection and try to reconnect.
 */

static void safe_reconnect_close(void)
{
  log_write(LL_WARN, "Caught SIGUSR2 signal, (re)connecting to host");

  aio_walk_slots(fn_stop_listening, TYPE_HOST_LISTENING_SLOT);
  aio_walk_slots(fn_close, TYPE_HOST_CONNECTING_SLOT);

  /* If host connection is active, aio_walk_slots would return 1 and
     we would request reconnect after current host connection is
     closed (fn_reconnect_to_host function). Otherwise (if there is no
     host connection), just connect immediately. */

  if (aio_walk_slots(fn_reconnect_close, TYPE_HOST_ACTIVE_SLOT) == 0)
    connect_to_host(NULL, 0);
}

/*
 * Do nothing but close a slot
 */

static void fn_close(AIO_SLOT *slot)
{
  aio_close_other(slot, 0);
}

/*
 * Callback function to close an I/O slot for listening connection.
 * Note that aio_close closes listening fd immediately.
 */

static void fn_stop_listening(AIO_SLOT *slot)
{
  log_write(LL_DETAIL, "Closing listening slot");
  aio_close_other(slot, 0);
}

/*
 * Callback function to close I/O slot and reconnect
 */

static void fn_reconnect_close(AIO_SLOT *slot)
{
  aio_close_other(slot, 0);
  connect_to_host(NULL, 0);
}

