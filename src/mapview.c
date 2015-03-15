/*  MudPRO: An advanced client for the online game MajorMUD
 *  Copyright (C) 2002, 2003, 2004  David Slusky
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __linux__
#include <unistd.h>
#endif

#include "automap.h"
#include "character.h"
#include "defs.h"
#include "keys.h"
#include "mapview.h"
#include "menubar.h"
#include "mudpro.h"
#include "osd.h"
#include "terminal.h"
#include "timers.h"
#include "utils.h"

cwin_t mapview;
gboolean mapview_adjust_visible = FALSE; /* display XYZ axis adjustment */
gboolean mapview_xyz_visible    = FALSE; /* display current XYZ coordinates */
gboolean mapview_flags_visible  = FALSE; /* display flags in mapview osd */
static gint mapview_adjustment  = 0;     /* the axis we are adjusting (if any) */
static guint player_anim_reps   = 0;     /* player animation repetitions */

/* mapview drawing history */
static gint history[MAPVIEW_HISTORY_X][MAPVIEW_HISTORY_Y];

static mapview_graph_table_t gt[] = {
	{ EXIT_NONE,		0,     0,  0 },
	{ EXIT_NORTH,		'|',   0, -1 },
	{ EXIT_SOUTH,		'|',   0,  1 },
	{ EXIT_EAST,		'-',   1,  0 },
	{ EXIT_WEST,		'-',  -1,  0 },
	{ EXIT_NORTHEAST,	'/',   1, -1 },
	{ EXIT_NORTHWEST,	'\\', -1, -1 },
	{ EXIT_SOUTHEAST,	'\\',  1,  1 },
	{ EXIT_SOUTHWEST,	'/',  -1,  1 },
	{ EXIT_UP,			0,     0,  0 },
	{ EXIT_DOWN,		0,     0,  0 },
	{ EXIT_SPECIAL,		0,     0,  0 },
};

static void mapview_draw_xyz_adjust (void);
static void mapview_draw_room_flags (void);
static void mapview_draw_location (automap_record_t *location, gint x, gint y);
static void mapview_propagate (automap_record_t *location, gint x, gint y);


/* =========================================================================
 = MAPVIEW_INIT
 =
 = Initialize mapview window
 ======================================================================== */

void mapview_init (gint y, gint x)
{
	memset (&mapview, 0, sizeof (cwin_t));

	mapview.width  = OSD_MAPVIEW_WIDTH;
	mapview.height = OSD_MAPVIEW_HEIGHT;

	mapview.ypos = y;
	mapview.xpos = x;

	mapview.w = newwin (mapview.height,
						mapview.width,
						mapview.ypos,
						mapview.xpos);

	mapview.p = new_panel (mapview.w);
	mapview.visible = FALSE;
	hide_panel (mapview.p);

	mapview.attr = ATTR_WINDOW | A_BOLD;
	wbkgdset (mapview.w, ATTR_WINDOW);

	leaveok (mapview.w, TRUE);
}


/* =========================================================================
 = MAPVIEW_CLEANUP
 =
 = Cleanup after mapview window
 ======================================================================== */

void mapview_cleanup (void)
{
	if (mapview.p) del_panel (mapview.p);
	if (mapview.w) delwin (mapview.w);
	g_assert (mapview.data == NULL);
	memset (&mapview, 0, sizeof (cwin_t));
}


/* =========================================================================
 = MAPVIEW_UPDATE
 =
 = Update the mapview window
 ======================================================================== */

void mapview_update (void)
{
	werase (mapview.w);
	wattrset (mapview.w, ATTR_BORDER | A_BOLD);

	wborder (mapview.w,
		window_ui.vline, window_ui.vline, window_ui.hline, window_ui.hline,
		window_ui.nw,    window_ui.ne,    window_ui.sw,    window_ui.se);

	if (mapview_adjustment) /* NOTE: keep this above display XYZ */
		mapview_draw_xyz_adjust ();
	else if (mapview_xyz_visible)
	{
		/* display XYZ coordinates */
		border_draw_bracket (&mapview, '[');
		wattrset (mapview.w, ATTR_WHITE | A_BOLD);
		wprintw (mapview.w, " X: %d Y: %d Z: %d ",
			automap.x, automap.y, automap.z);
		border_draw_bracket (&mapview, ']');
	}
	else
	{
		border_draw_bracket (&mapview, '[');
		wattrset (mapview.w, ATTR_WHITE | A_BOLD);
		if (mapview_flags_visible) waddstr (mapview.w, " Room Flags ");
		else if (automap.enabled)  waddstr (mapview.w, " Mapview (Automapping) ");
		else                       waddstr (mapview.w, " Mapview ");

		border_draw_bracket (&mapview, ']');
	}

	if (mapview_flags_visible)
		mapview_draw_room_flags ();
	else if (automap.lost > 2 || !automap.location)
	{
		wattrset (mapview.w, ATTR_HILITE | A_BOLD);
		mvwaddch (mapview.w, MAPVIEW_Y (0), MAPVIEW_X (0), '?');
	}
	else
	{
		memset (&history, 0, sizeof (history));
		mapview_propagate (automap.location, 0, 0);
	}
}


/* =========================================================================
 = MAPVIEW_MAP
 =
 = Maps the mapview display
 ======================================================================== */

void mapview_map (void)
{
	if (mapview.visible)
		return;

	show_panel (mapview.p);
	mapview.visible = TRUE;
	mapview_update ();
	menubar_raise ();
}


/* =========================================================================
 = MAPVIEW_UNMAP
 =
 = Unmaps the mapview display
 ======================================================================== */

void mapview_unmap (void)
{
	if (!mapview.visible)
		return;

	hide_panel (mapview.p);
	mapview.visible = FALSE;
	update_display ();
}


/* =========================================================================
 = MAPVIEW_TOGGLE_XYZ_ADJUST
 =
 = Toggle XYZ adjustment display
 ======================================================================== */

void mapview_toggle_xyz_adjust (void)
{
	if (mapview_adjust_visible)
	{
		mapview_adjust_visible = FALSE;
		mapview_adjustment     = 0;
		automap_duplicate_merge ();
	}
	else
	{
		mapview_adjust_visible = TRUE;
		mapview_adjustment     = '?'; /* query for axis */
	}
	mapview_update ();
	update_display ();
}


/* =========================================================================
 = MAPVIEW_TOGGLE_XYZ_DISPLAY
 =
 = Toggle current XYZ coordinate display
 ======================================================================== */

void mapview_toggle_xyz_display ()
{
	mapview_xyz_visible = !mapview_xyz_visible;
	mapview_update ();
	update_display ();
}


/* =========================================================================
 = MAPVIEW_TOGGLE_ROOM_FLAGS
 =
 = Toggle room flag display
 ======================================================================== */

void mapview_toggle_room_flags (void)
{
	if (automap.lost || !automap.location)
	{
		printt ("Cannot do that while lost!");
		return;
	}
	mapview_flags_visible = !mapview_flags_visible;
	mapview_update ();
	update_display ();
}


/* =========================================================================
 = MAPVIEW_DRAW_XYZ_ADJUST
 =
 = Draw XYZ axis adjustments
 ======================================================================== */

static void mapview_draw_xyz_adjust (void)
{
	switch (mapview_adjustment)
	{
	case 'x': /* display X axis */
		border_draw_bracket (&mapview, '[');
		wattrset (mapview.w, ATTR_WHITE | A_BOLD);
		wprintw (mapview.w, " Adjusting X: %d ", automap.x);
		border_draw_bracket (&mapview, ']');
		break;
	case 'y': /* display Y axis */
		border_draw_bracket (&mapview, '[');
		wattrset (mapview.w, ATTR_WHITE | A_BOLD);
		wprintw (mapview.w, " Adjusting Y: %d ", automap.y);
		border_draw_bracket (&mapview, ']');
		break;
	case 'z': /* display Z axis */
		border_draw_bracket (&mapview, '[');
		wattrset (mapview.w, ATTR_WHITE | A_BOLD);
		wprintw (mapview.w, " Adjusting Z: %d ", automap.z);
		border_draw_bracket (&mapview, ']');
		break;
	case '?': /* display axis selection query */
		border_draw_bracket (&mapview, '[');
		wattrset (mapview.w, ATTR_WHITE | A_BOLD);
		wprintw (mapview.w, " Adjusting: ... ");
		border_draw_bracket (&mapview, ']');
		break;
	default:
		g_assert_not_reached ();
	}
}


/* =========================================================================
 = MAPVIEW_DRAW_ROOM_FLAGS
 =
 = Display menu for editing room flags
 ======================================================================== */

static void mapview_draw_room_flags (void)
{
	gint xpos = 2, ypos = 2;

	/* <----------- 29 ------------> */
	/* <----------- 23 ------>       */
	/* Full (H)ealth Needed  [ NO  ] */

	wattrset (mapview.w, mapview.attr);
#if 0
	mvwaddstr (mapview.w, ypos++, xpos, "Regen (P)oint ....... [");
	wattrset (mapview.w, ATTR_HILITE | A_BOLD);
	wprintw (mapview.w, " %-3s ",
		(automap.location->flags & ROOM_FLAG_REGEN) ? "YES" : "NO");
	wattrset (mapview.w, mapview.attr);
	waddch (mapview.w, ']');
#endif
	mvwaddstr (mapview.w, ypos++, xpos, "Full (H)ealth Needed  [");
	wattrset (mapview.w, ATTR_HILITE | A_BOLD);
	wprintw (mapview.w, " %-3s ",
		(automap.location->flags & ROOM_FLAG_FULL_HP) ? "YES" : "NO");
	wattrset (mapview.w, mapview.attr);
	waddch (mapview.w, ']');

	mvwaddstr (mapview.w, ypos++, xpos, "Full (M)ana Needed .. [");
	wattrset (mapview.w, ATTR_HILITE | A_BOLD);
	wprintw (mapview.w, " %-3s ",
		(automap.location->flags & ROOM_FLAG_FULL_MA) ? "YES" : "NO");
	wattrset (mapview.w, mapview.attr);
	waddch (mapview.w, ']');

	mvwaddstr (mapview.w, ypos++, xpos, "Do Not (R)est ....... [");
	wattrset (mapview.w, ATTR_HILITE | A_BOLD);
	wprintw (mapview.w, " %-3s ",
		(automap.location->flags & ROOM_FLAG_NOREST) ? "YES" : "NO");
	wattrset (mapview.w, mapview.attr);
	waddch (mapview.w, ']');

	mvwaddstr (mapview.w, ypos++, xpos, "Do Not (A)utoroam ... [");
	wattrset (mapview.w, ATTR_HILITE | A_BOLD);
	wprintw (mapview.w, " %-3s ",
		(automap.location->flags & ROOM_FLAG_NOROAM) ? "YES" : "NO");
	wattrset (mapview.w, mapview.attr);
	waddch (mapview.w, ']');

	mvwaddstr (mapview.w, ypos++, xpos, "Do Not (E)nter ...... [");
	wattrset (mapview.w, ATTR_HILITE | A_BOLD);
	wprintw (mapview.w, " %-3s ",
		(automap.location->flags & ROOM_FLAG_NOENTER) ? "YES" : "NO");
	wattrset (mapview.w, mapview.attr);
	waddch (mapview.w, ']');

	mvwaddstr (mapview.w, ypos++, xpos, "Stash (G)oods ....... [");
	wattrset (mapview.w, ATTR_HILITE | A_BOLD);
	wprintw (mapview.w, " %-3s ",
		(automap.location->flags & ROOM_FLAG_STASH) ? "YES" : "NO");
	wattrset (mapview.w, mapview.attr);
	waddch (mapview.w, ']');

	mvwaddstr (mapview.w, ypos++, xpos, "(S)ync Automap ...... [");
	wattrset (mapview.w, ATTR_HILITE | A_BOLD);
	wprintw (mapview.w, " %-3s ",
		(automap.location->flags & ROOM_FLAG_SYNC) ? "YES" : "NO");
	wattrset (mapview.w, mapview.attr);
	waddch (mapview.w, ']');
}


/* =========================================================================
 = MAPVIEW_DRAW_LOCATION
 =
 = Displays the current location
 ======================================================================== */

static void mapview_draw_location (automap_record_t *location, gint x, gint y)
{
	exit_info_t *exit_info;
	gint attr, ch, i;
	GSList *node;

	g_assert (location != NULL);

	if (location == automap.location) /* draw player's location */
	{
		wattrset (mapview.w, ATTR_HILITE | A_BOLD);
		mvwaddch (mapview.w, MAPVIEW_Y (y), MAPVIEW_X (x),
			(automap.lost) ? '?' : 'X');
	}
	else
	{
		if (location->flags & ROOM_FLAG_REGEN)
			wattrset (mapview.w, ATTR_HILITE | A_BOLD);
		else if ((location->flags & ROOM_FLAG_NOROAM) ||
				 (location->flags & ROOM_FLAG_NOENTER))
			wattrset (mapview.w, ATTR_SUBTLE | A_BOLD);
		else
			wattrset (mapview.w, mapview.attr);

		/* room has a secret (non-searchable) exit */
		if (automap_get_exit_info (location, EXIT_SPECIAL))
			mvwaddch (mapview.w, MAPVIEW_Y (y), MAPVIEW_X (x), '&');

		else if (location->flags & ROOM_FLAG_STASH)
			mvwaddch (mapview.w, MAPVIEW_Y (y), MAPVIEW_X (x), '$');

		/* room has an exit leading up */
		else if ((location->exits & EXIT_UP) && (location->exits ^ EXIT_DOWN))
			mvwaddch (mapview.w, MAPVIEW_Y (y), MAPVIEW_X (x), 'U');

		/* room has an exit leading down */
		else if ((location->exits & EXIT_DOWN) && (location->exits ^ EXIT_UP))
			mvwaddch (mapview.w, MAPVIEW_Y (y), MAPVIEW_X (x), 'D');

		/* room has exits leading both up and down */
		else if (location->exits & (EXIT_UP | EXIT_DOWN))
			mvwaddch (mapview.w, MAPVIEW_Y (y), MAPVIEW_X (x), '@');

		/* regular room, '#' seems to work best with most fonts */
		else
			mvwaddch (mapview.w, MAPVIEW_Y (y), MAPVIEW_X (x), '#');
	}

	/* draw exit paths */
	for (node = location->exit_list; node; node = node->next)
	{
		g_assert (node->data != NULL);
		exit_info = node->data;

		/* only show adjacent exits */
		if (exit_info->direction < EXIT_NORTH ||
			exit_info->direction > EXIT_SOUTHWEST)
			continue;

		/* select exits attributes */
		if (!strcmp (exit_info->id, "0"))
			attr = ATTR_SUBTLE | A_BOLD;
		else if (exit_info->flags & EXIT_FLAG_BLOCKED)
			attr = ATTR_DIM | A_BOLD;
		else
			attr = mapview.attr;

		i = po2 (exit_info->direction);

		/* mark doors and secret exits specially */
		if (exit_info->flags & EXIT_FLAG_DOOR) ch = '%';
		else if (exit_info->flags & EXIT_FLAG_SECRET) ch = '+';
		else ch = gt[i].ch; /* otherwise refer to table */

		if (ch) mvwaddch (mapview.w,
			MAPVIEW_Y (gt[i].y) + y,
			MAPVIEW_X (gt[i].x) + x,
			ch | attr);
	}
}


/* =========================================================================
 = MAPVIEW_PROPAGATE
 =
 = Propagate to surrounding rooms and draw those visible to the mapview
 ======================================================================== */

static void mapview_propagate (automap_record_t *location, gint x, gint y)
{
	exit_info_t *exit_info;
	automap_record_t *record;
	GSList *node;
	gint i, xx, yy;

	g_assert (location != NULL);

	/* draw room if it's within the boundaries of the virtial grid */
	if (MAPVIEW_VX (x) && MAPVIEW_VY (y))
		mapview_draw_location (location,
			MAPVIEW_GRID_CELL (x), MAPVIEW_GRID_CELL (y));

	/* mark the rooms we've visited */
	MAPVIEW_HISTORY (x, y) = 1;

	/* propagate through available exits */
	for (node = location->exit_list; node; node = node->next)
	{
		g_assert (node->data != NULL);
		exit_info = node->data;

		i = po2 (exit_info->direction);

		if (!strcmp (exit_info->id, "0") || (exit_info->flags & EXIT_FLAG_ONEWAY))
			continue;

		xx = x + gt[i].x;
		yy = y + gt[i].y;

		if (!MAPVIEW_PROP_X (xx) || !MAPVIEW_PROP_Y (yy) ||
			 MAPVIEW_HISTORY (xx, yy))
			 continue;

		if ((record = automap_db_lookup (exit_info->id)) != NULL)
			mapview_propagate (record, xx, yy);
	}
}


/* =========================================================================
 = MAPVIEW_ANIMATE_PLAYER
 =
 = Initiates player animation
 ======================================================================== */

void mapview_animate_player (guint reps)
{
	if (reps)
	{
		/* set the first frame */
		/* mapview_animate_player_frame (); */

		/* execute animation */
		player_anim_reps = reps;
		g_timer_start (timers.player_anim);
	}
}


/* =========================================================================
 = MAPVIEW_ANIMATE_PLAYER_FRAME
 =
 = Draws one frame of player animation
 ======================================================================== */

gboolean mapview_animate_player_frame (void)
{
	static enum { DIM, BRIGHT, NORMAL } state = DIM;
	static gint count = 0;
	gboolean anim_done = FALSE;

	switch (state)
	{
	case NORMAL:
		wattrset (mapview.w, ATTR_HILITE | A_BOLD);
		if (count < player_anim_reps)
			count++;
		else
		{
			count = 0;
			anim_done = TRUE;
		}
		state = DIM;
		break;
	case DIM:
		wattrset (mapview.w, ATTR_SUBTLE | A_BOLD);
		state = BRIGHT;
		break;
	case BRIGHT:
		wattrset (mapview.w, ATTR_WINDOW | A_BOLD);
		state = NORMAL;
		break;
	}
	mvwaddch (mapview.w, MAPVIEW_Y (0), MAPVIEW_X (0), 'X');
	g_timer_reset (timers.player_anim);
	update_display ();

	return anim_done;
}


/* =========================================================================
 = MAPVIEW_KEY_HANDLER
 =
 = Handle mapview user input
 ======================================================================== */

gboolean mapview_key_handler (gint ch)
{
	if (!automap.location || automap.lost)
		return FALSE; /* ignore input until we are oriented */

	if (mapview_adjustment && mapview_adjust_visible)
	{
		switch (tolower (ch))
		{
		case 'x': mapview_adjustment = 'x'; break;
		case 'y': mapview_adjustment = 'y'; break;
		case 'z': mapview_adjustment = 'z'; break;

		case '+':
		case '=':
			automap_adjust_xyz (mapview_adjustment, '+');
			break;

		case '-':
		case '_':
			automap_adjust_xyz (mapview_adjustment, '-');
			break;

		default:
			return TRUE; /* don't bother updating display */
		}
		mapview_update ();
		update_display ();
		return TRUE;
	}

	else if (mapview_flags_visible)
	{
		g_assert (automap.location != NULL);

		switch (tolower (ch))
		{
		case 'p': /* Regen (P)oint */
			automap.location->flags ^= ROOM_FLAG_REGEN;
			break;

		case 's': /* (S)ync Automap */
			automap.location->flags ^= ROOM_FLAG_SYNC;
			break;

		case 'h': /* Full (H)ealth Needed */
			automap.location->flags ^= ROOM_FLAG_FULL_HP;
			break;

		case 'm': /* Full (M)ana Needed */
			automap.location->flags ^= ROOM_FLAG_FULL_MA;
			break;

		case 'r': /* Do Not (R)est */
			automap.location->flags ^= ROOM_FLAG_NOREST;
			break;

		case 'a': /* Do Not (A)utoroam */
			automap.location->flags ^= ROOM_FLAG_NOROAM;
			break;

		case 'e': /* Do Not (E)nter */
			automap.location->flags ^= ROOM_FLAG_NOENTER;
			break;

		case 'g': /* Stash (G)oods */
			automap.location->flags ^= ROOM_FLAG_STASH;
			break;
		default:
			return TRUE;
		}
		mapview_update ();
		update_display ();
		return TRUE;
	}
	return FALSE;
}
