/*  MudPRO: An advanced client for the online game MajorMUD
 *  Copyright (C) 2002-2018  David Slusky
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __MAPVIEW_H__
#define __MAPVIEW_H__

#include "widgets.h"

#define MAPVIEW_CENTER_X	(mapview.width >> 1)
#define MAPVIEW_CENTER_Y	(mapview.height >> 1)
#define MAPVIEW_X(x)		(MAPVIEW_CENTER_X + x)
#define MAPVIEW_Y(y)		(MAPVIEW_CENTER_Y + y)

/* virtal grid size */
#define MAPVIEW_VX(x)		(abs (x) < 5) /* -4 to 4 */
#define MAPVIEW_VY(y)		(abs (y) < 2) /* -1 to 1 */

/* propagation grid size (relative to virtual grid) */
#define MAPVIEW_PROP_X(x)	(abs (x) < 16) /* -15 to 15 */
#define MAPVIEW_PROP_Y(y)	(abs (y) < 16) /* -15 to 15 */

#define MAPVIEW_GRID_CELL(x)	(x * 3) /* cell offset */

#define MAPVIEW_HISTORY_X		31
#define MAPVIEW_HISTORY_Y		31
#define MAPVIEW_HISTORY(x, y)	history[x+15][y+15]

typedef struct
{
	guint direction; /* direction of exit */
	guchar ch;       /* character used to draw exit */
	gint x, y;       /* graph XY offset */
} mapview_graph_table_t;

extern cwin_t mapview;
extern gboolean mapview_adjust_visible;
extern gboolean mapview_xyz_visible;
extern gboolean mapview_flags_visible;

void mapview_init (gint y, gint x);
void mapview_cleanup (void);
void mapview_update (void);
void mapview_map (void);
void mapview_unmap (void);
void mapview_toggle_xyz_adjust (void);
void mapview_toggle_xyz_display (void);
void mapview_toggle_room_flags (void);
void mapview_animate_player (guint reps);
gboolean mapview_key_handler (gint ch);
gboolean mapview_animate_player_frame (void);

#endif /* __MAPVIEW_H__ */
