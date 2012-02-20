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
 * $Id: region_more.c,v 1.2 2004/08/08 15:23:35 const_k Exp $
 * A routine to join neighboring rectangles in a region, to reduce the
 * total number of rectangles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "rfblib.h"
#include "region.h"
#include "logging.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

static void report_bad_rect_order(void);

/* FIXME: The algorithm still can be improved. */

void region_pack(RegionPtr pregion, int threshold)
{
  int i, num_rects;
  int height, overhead, area1, area2, area3;
  int joins = 0, sum_overhead = 0;
  BoxRec prev_rect, this_rect, tmp_rect;
  RegionRec tmp_region, add_region;

  num_rects = REGION_NUM_RECTS(pregion);

  if (num_rects < 2) {
    return;                     /* nothing to optimize */
  }

  REGION_INIT(&add_region, NullBox, 16);
  prev_rect = REGION_RECTS(pregion)[0];

  for (i = 1; i < num_rects; i++) {
    this_rect = REGION_RECTS(pregion)[i];

    if (this_rect.y1 == prev_rect.y1 && this_rect.y2 == prev_rect.y2) {

      /* Try to join two rectangles of the same "band" */

      if (prev_rect.x2 > this_rect.x1) {
        report_bad_rect_order();
        REGION_UNINIT(&add_region);
        return;
      }
      height = this_rect.y2 - this_rect.y1;
      overhead = (this_rect.x1 - prev_rect.x2) * height;
      if (overhead < threshold) {
        tmp_rect.y1 = prev_rect.y1;
        tmp_rect.y2 = prev_rect.y2;
        tmp_rect.x1 = prev_rect.x2;
        tmp_rect.x2 = this_rect.x1;
        REGION_INIT(&tmp_region, &tmp_rect, 1);
        REGION_UNION(&add_region, &add_region, &tmp_region);
        REGION_UNINIT(&tmp_region);
        joins++;
        sum_overhead += overhead;
      }

    } else {

      /* Try to join two rectangles of neighboring "bands" */

      area1 = (prev_rect.x2 - prev_rect.x1) * (prev_rect.y2 - prev_rect.y1);
      area2 = (this_rect.x2 - this_rect.x1) * (this_rect.y2 - this_rect.y1);
      tmp_rect.x1 = min(prev_rect.x1, this_rect.x1);
      tmp_rect.x2 = max(prev_rect.x2, this_rect.x2);
      tmp_rect.y1 = min(prev_rect.y1, this_rect.y1);
      tmp_rect.y2 = max(prev_rect.y2, this_rect.y2);
      area3 = (tmp_rect.x2 - tmp_rect.x1) * (tmp_rect.y2 - tmp_rect.y1);
      overhead = area3 - area2 - area1;
      if (overhead < threshold || overhead < (area1 + area2) / 100) {
        REGION_INIT(&tmp_region, &tmp_rect, 1);
        REGION_UNION(&add_region, &add_region, &tmp_region);
        REGION_UNINIT(&tmp_region);
        joins++;
        sum_overhead += overhead;
        this_rect = tmp_rect;   /* copy the joined one to prev_rect */
      }

    }

    prev_rect = this_rect;
  }

  if (sum_overhead) {
    REGION_UNION(pregion, pregion, &add_region);
    log_write(LL_DEBUG, "Joined rectangles: %d -> %d, overhead %d",
              num_rects, (int)(REGION_NUM_RECTS(pregion)), sum_overhead);
  }

  REGION_UNINIT(&add_region);
}

#ifndef MIN
# define MIN(a,b)\
    ({  typeof(a) _a = (a);\
        typeof(b) _b = (b);\
        a < _b ? _a : _b;  })
#endif
#ifndef NULL
# define NULL 0
#endif

/* Split the region in multiple square tiles.
 * box        is the rectangle to split in tiles
 * tile_size  is the size of the tile in pixels
 * threshold  is the number of extra pixels that can be added or removed from a
 *            tile so that there's no 1x1 tiles or the like.*/
BoxListPtr region_split_in_tile(BoxPtr box, int tile_size, int threshold)
{
  BoxListPtr retBoxList = NULL;
  BoxListPtr tmpBoxList;
  int nbTiles = 0;

  /* Initialize the ybox to iterate over the y axis without altering box. */
  BoxRec ybox;
  ybox.x1 = box->x1;
  ybox.x2 = box->x2;
  ybox.y1 = box->y1;
  ybox.y2 = box->y2;

  log_write(LL_INFO, ">> Splitting the rect (%hd,%hd),(%hdx%hd) in %d+-%d wide tiles", box->x1, box->y1, box->x2 - box->x1, box->y2 - box->y1, tile_size, threshold);

  while (ybox.y2 - ybox.y1 > 0)
  {
      /* Get a band of tile_size height */
      BoxRec xbox;
      xbox.x1 = ybox.x1;
      xbox.y1 = ybox.y1;
      xbox.x2 = ybox.x2;
      xbox.y2 = MIN(ybox.y1 + tile_size + threshold, ybox.y2) == ybox.y2 ?
          ybox.y2 :
          ybox.y1 + tile_size;

      /* Advance to the the next band */
      ybox.y1 = xbox.y2;

      while (xbox.x2 - xbox.x1 > 0)
      {
          nbTiles++;
          /* Add a tile to the list */
          tmpBoxList = (BoxListPtr)malloc(sizeof(BoxList));
          tmpBoxList->rect.x1 = xbox.x1;
          tmpBoxList->rect.y1 = xbox.y1;
          tmpBoxList->rect.x2 = MIN(xbox.x1 + tile_size + threshold, xbox.x2) == xbox.x2 ?
              xbox.x2 :
              xbox.x1 + tile_size;
          tmpBoxList->rect.y2 = xbox.y2;
          tmpBoxList->next = retBoxList;
          retBoxList = tmpBoxList;
          log_write(LL_INFO, "Adding tile (%hd,%hd),(%hdx%hd)", tmpBoxList->rect.x1, tmpBoxList->rect.y1, tmpBoxList->rect.x2 - tmpBoxList->rect.x1, tmpBoxList->rect.y2 - tmpBoxList->rect.y1);

          /* Advance to the next tile. */
          xbox.x1 = tmpBoxList->rect.x2;
      }
  }
  log_write(LL_INFO, ">> Splitted in %d tiles.", nbTiles);
  return retBoxList;
}

void BoxList_Destroy(BoxListPtr list)
{
    if (list != NULL)
    {
        BoxListPtr next = list->next;
        free(list);
        BoxList_Destroy(next);
    }
}

/*
 * Error reporting.
 */

static int warned = 0;

static void report_bad_rect_order(void) {
  if (!warned) {
    log_write(LL_WARN, "Bad rectangle order in regions - not packing");
    warned = 1;
  }
}

