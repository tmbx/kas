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
 * $Id: decode_cursor.c,v 1.6 2004/11/10 16:29:04 grolloj Exp $
 * Connecting to a VNC host
 */

#include <stdlib.h>
#include <sys/types.h>
#include <zlib.h>
#include <string.h>

#include "rfblib.h"
#include "logging.h"
#include "async_io.h"
#include "translate.h"
#include "client_io.h"
#include "host_io.h"
#include "reflector.h"


static void rf_host_xcursor_color(void);
static void rf_host_xcursor_bmps(void);
static void rf_host_richcursor_bmps(void);
static void rf_host_cursor(void);
static void rf_host_pointerpos(void);

static FB_RECT s_curs_rect;
static FB_RECT s_pos_rect;
static CARD8 s_xcursor_colors[sz_rfbXCursorColors];
static CARD8 *s_bmps = NULL;
static FB_RECT s_new_curs_rect;
static CARD8 s_new_xcursor_colors[sz_rfbXCursorColors];
static CARD8 *s_new_bmps = NULL;
static CARD16 s_curs_x = 0;
static CARD16 s_curs_y = 0;
static int s_type = 0;
static int s_read_size = 0;
static int s_has_pos = 0;


/*
 * Public functions
 */
void setread_decode_xcursor(FB_RECT *r)
{
  s_new_curs_rect = *r;

  if (s_new_curs_rect.w * s_new_curs_rect.h) {
    aio_setread(rf_host_xcursor_color, s_new_xcursor_colors, sz_rfbXCursorColors);
  }
}

void setread_decode_richcursor(FB_RECT *r)
{
  CARD32 size;

  s_new_curs_rect = *r;

  if (s_new_curs_rect.w * s_new_curs_rect.h) {
    /* calculate size of two cursor bitmaps to follow */
    /* cursor image in client pixel format */
    /* transparency data for cursor */
    size = s_new_curs_rect.w * s_new_curs_rect.h * (g_screen_info.pixformat.bits_pixel / 8);
    size += ((s_new_curs_rect.w + 7) / 8) * s_new_curs_rect.h;
    s_new_bmps = realloc(s_new_bmps, size);
    if (!s_new_bmps) {
      log_write(LL_ERROR, "Failed to allocate memory for cursor pixmaps");
      aio_close(0);
      return;
    }
    s_read_size = size;
    aio_setread(rf_host_richcursor_bmps, s_new_bmps, size);
  }
}

void setread_decode_pointerpos(FB_RECT *r)
{
  s_pos_rect = *r;
  s_curs_x = s_pos_rect.x;
  s_curs_y = s_pos_rect.y;
  if (!s_has_pos)
    s_has_pos = 1;
  rf_host_pointerpos();
}

void set_pointerpos(CARD16 x, CARD16 y)
{
  SET_RECT(&s_pos_rect, x, y, 0, 0);  
  s_curs_x = x;
  s_curs_y = y;
  if (!s_has_pos)
    s_has_pos = 1;
  rf_host_pointerpos();
}

FB_RECT *crsr_get_rect(void)
{
  return &s_curs_rect;
}

int crsr_has_pos_rect(void)
{
  return s_has_pos;
}

FB_RECT *crsr_get_pos_rect(void)
{
  return &s_pos_rect;
}

CARD8 *crsr_get_col(void)
{
  return s_xcursor_colors;
}

CARD8 *crsr_get_bmps(void)
{
  return s_bmps;
}

int crsr_get_type(void)
{
  return s_type;
}


/*
 * Private functions
 */

static void rf_host_xcursor_color(void)
{
  CARD32 size;

  fbs_spool_data(cur_slot->readbuf, sz_rfbXCursorColors);

  /* calculate size of two cursor bitmaps to follow */
  size = ((s_new_curs_rect.w + 7) / 8) * s_new_curs_rect.h * 2;
  s_new_bmps = realloc(s_new_bmps, size);
  if (!s_new_bmps) {
    log_write(LL_ERROR, "Failed to allocate memory for cursor pixmaps");
    aio_close(0);
    return;
  }
  s_read_size = size;
  aio_setread(rf_host_xcursor_bmps, s_new_bmps, size);
}

static void rf_host_xcursor_bmps(void)
{
  fbs_spool_data(cur_slot->readbuf, s_read_size);
  s_curs_rect = s_new_curs_rect;
  memcpy(s_xcursor_colors, s_new_xcursor_colors, sz_rfbXCursorColors);
  s_bmps = realloc(s_bmps, s_read_size);
  if (!s_bmps) {
    log_write(LL_ERROR, "Failed to allocate memory for cursor pixmaps");
    aio_close(0);
    return;
  }
  memcpy(s_bmps, s_new_bmps, s_read_size);
  s_type = RFB_ENCODING_XCURSOR;
  rf_host_cursor();
  fbupdate_rect_done();
}

static void rf_host_richcursor_bmps(void)
{
  fbs_spool_data(cur_slot->readbuf, s_read_size);
  s_curs_rect = s_new_curs_rect;
  s_bmps = realloc(s_bmps, s_read_size);
  if (!s_bmps) {
    log_write(LL_ERROR, "Failed to allocate memory for cursor pixmaps");
    aio_close(0);
    return;
  }
  memcpy(s_bmps, s_new_bmps, s_read_size);
  s_type = RFB_ENCODING_RICHCURSOR;
  rf_host_cursor();
  fbupdate_rect_done();
}

/***********************************/
/* Handling XCursor and RichCursor */
/***********************************/
static void rf_host_cursor(void)
{
  aio_walk_slots(fn_client_send_xcursor, TYPE_CL_SLOT);
}

/***********************************/
/* Handling PointerPos messages    */
/***********************************/
static void rf_host_pointerpos(void)
{
  aio_walk_slots(fn_client_send_pointerpos, TYPE_CL_SLOT);
}

