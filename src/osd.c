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

#include <string.h>

#include "character.h"
#include "defs.h"
#include "combat.h"
#include "monster.h"
#include "mapview.h"
#include "menubar.h"
#include "mudpro.h"
#include "osd.h"
#include "stats.h"
#include "terminal.h"
#include "timers.h"
#include "utils.h"

cwin_t osd_vitals;
cwin_t osd_stats;
gint   osd_stats_mode = OSD_STATS_NONE;

/* used to lock onto one set of stats */
gboolean osd_stats_lock_exp;
gboolean osd_stats_lock_combat;
gboolean osd_stats_lock_player;
gboolean osd_stats_lock_session;

static void osd_stats_experience_update (void);
static void osd_stats_combat_update (void);
static void osd_stats_player_update (void);
static void osd_stats_session_update (void);
static void osd_status_gauge (cwin_t *cwin, gint percent);


/* =========================================================================
 = OSD_VITALS_INIT
 =
 = Initializes vitals OSD
 ======================================================================== */

void osd_vitals_init (gint y, gint x)
{
	memset (&osd_vitals, 0, sizeof (cwin_t));

	osd_vitals.width  = OSD_VITALS_WIDTH;
	osd_vitals.height = OSD_VITALS_HEIGHT;

	osd_vitals.ypos = y;
	osd_vitals.xpos = x;

	osd_vitals.w = newwin (osd_vitals.height,
						   osd_vitals.width,
						   osd_vitals.ypos,
						   osd_vitals.xpos);

	osd_vitals.p = new_panel (osd_vitals.w);
	osd_vitals.visible = FALSE;
	hide_panel (osd_vitals.p);

	osd_vitals.attr = ATTR_WINDOW | A_BOLD;
	wbkgdset (osd_vitals.w, ATTR_WINDOW);

	leaveok (osd_vitals.w, TRUE);
}


/* =========================================================================
 = OSD_VITALS_CLEANUP
 =
 = Cleans up after vitals OSD
 ======================================================================== */

void osd_vitals_cleanup (void)
{
	if (osd_vitals.p) del_panel (osd_vitals.p);
	if (osd_vitals.w) delwin (osd_vitals.w);
	g_assert (osd_vitals.data == NULL);
	memset (&osd_vitals, 0, sizeof (cwin_t));
}


/* =========================================================================
 = OSD_VITALS_UPDATE
 =
 = Update the vitals OSD
 ======================================================================== */

void osd_vitals_update (void)
{
	gint percent;

	if (!osd_vitals.visible)
		return;

	werase (osd_vitals.w);
	wattrset (osd_vitals.w, ATTR_BORDER | A_BOLD);

	wborder (osd_vitals.w,
		window_ui.vline, window_ui.vline, window_ui.hline, window_ui.hline,
		window_ui.nw,    window_ui.ne,    window_ui.sw,    window_ui.se);

	border_draw_bracket (&osd_vitals, '[');
	wattrset (osd_vitals.w, ATTR_WHITE | A_BOLD);
	waddstr (osd_vitals.w, " Vitals ");
	border_draw_bracket (&osd_vitals, ']');

	wattrset (osd_vitals.w, osd_vitals.attr);
	mvwprintw (osd_vitals.w, 1, 2, "%s %s",
		character.first_name ? character.first_name : "N/A",
		character.last_name  ? character.last_name  : "");

	percent = get_percent (character.hp.now, character.hp.max);
	mvwprintw (osd_vitals.w, 2, 3, "HP: %5d (%3d%%) ",
		character.hp.now, percent);
	osd_status_gauge (&osd_vitals, percent);

	if (character.ma.max) /* has mana */
	{
		percent = get_percent (character.ma.now, character.ma.max);
		mvwprintw (osd_vitals.w, 3, 3, "MA: %5d (%3d%%) ",
			character.ma.now, percent);
		osd_status_gauge (&osd_vitals, percent);
	}
}


/* =========================================================================
 = OSD_VITALS_MAP
 =
 = Map vitals OSD on the screen
 ======================================================================== */

void osd_vitals_map (void)
{
	if (osd_vitals.visible)
		return;

	show_panel (osd_vitals.p);
	osd_vitals.visible = TRUE;
	osd_vitals_update ();
	menubar_raise ();
}


/* =========================================================================
 = OSD_VITALS_UNMAP
 =
 = Unmaps the vitals OSD from the screen
 ======================================================================== */

void osd_vitals_unmap (void)
{
	if (!osd_vitals.visible)
		return;

	hide_panel (osd_vitals.p);
	osd_vitals.visible = FALSE;
	update_display ();
}


/* =========================================================================
 = OSD_STATS_INIT
 =
 = Initialize the stats OSD
 ======================================================================== */

void osd_stats_init (gint y, gint x)
{
	memset (&osd_stats, 0, sizeof (cwin_t));

	osd_stats.width  = OSD_STATS_WIDTH;
	osd_stats.height = OSD_STATS_HEIGHT;

	osd_stats.ypos = y;
	osd_stats.xpos = x;

	osd_stats.w = newwin (osd_stats.height,
						  osd_stats.width,
						  osd_stats.ypos,
						  osd_stats.xpos);

	osd_stats.p = new_panel (osd_stats.w);
	osd_stats.visible = FALSE;
	hide_panel (osd_stats.p);

	osd_stats.attr = ATTR_WINDOW | A_BOLD;
	wbkgdset (osd_stats.w, ATTR_WINDOW);

	leaveok (osd_stats.w, TRUE);

	/* default to context switching */
	character.option.osd_context = TRUE;
}


/* =========================================================================
 = OSD_STATS_CLEANUP
 =
 = Clean up after stats OSD
 ======================================================================== */

void osd_stats_cleanup (void)
{
	if (osd_stats.p) del_panel (osd_stats.p);
	if (osd_stats.w) delwin (osd_stats.w);
	g_assert (osd_stats.data == NULL);
	memset (&osd_stats, 0, sizeof (cwin_t));
}


/* =========================================================================
 = OSD_STATS_UPDATE
 =
 = Update the stats OSD
 ======================================================================== */

void osd_stats_update (void)
{
	if (osd_stats_mode == OSD_STATS_NONE)
		return;

	if (!timer_elapsed (timers.osd_stats, NULL) && !OSD_LOCK_ENABLED)
	{
		/* automatically set stats display */

		if (!character.flag.ready)
			osd_stats_set_mode (OSD_STATS_SESSION);

		else if (character.targets)
			osd_stats_set_mode (OSD_STATS_COMBAT);

		else if (character.state == STATE_RESTING
			|| character.state == STATE_MEDITATING)
			osd_stats_set_mode (OSD_STATS_PLAYER);

		else
			osd_stats_set_mode (OSD_STATS_EXP);
	}

	werase (osd_stats.w);
	wattrset (osd_stats.w, ATTR_BORDER | A_BOLD);

	wborder (osd_stats.w,
		window_ui.vline, window_ui.vline, window_ui.hline, window_ui.hline,
		window_ui.nw,    window_ui.ne,    window_ui.sw,    window_ui.se);

	border_draw_bracket (&osd_stats, '[');
	wattrset (osd_stats.w, ATTR_WHITE | A_BOLD);
	waddstr (osd_stats.w, " Statistics ");
	border_draw_bracket (&osd_stats, ']');

	switch (osd_stats_mode)
	{
	case OSD_STATS_EXP:     osd_stats_experience_update (); break;
	case OSD_STATS_COMBAT:  osd_stats_combat_update ();     break;
	case OSD_STATS_PLAYER:  osd_stats_player_update ();     break;
	case OSD_STATS_SESSION: osd_stats_session_update ();    break;
	default:
		g_assert_not_reached ();
	}

	update_display ();
}


/* =========================================================================
 = OSD_STATS_EXPERIENCE_UPDATE
 =
 = Update stats OSD in experience mode
 ======================================================================== */

static void osd_stats_experience_update (void)
{
	GString *buf;

	stats_exp_update ();

	wattrset (osd_stats.w, osd_stats.attr);

	buf = commified_g_string (stats.exp.gained);
	mvwprintw (osd_stats.w, 1, 2, "Exp Made:   %s", buf->str);
	g_string_free (buf, TRUE);

	buf = commified_g_string (stats.exp.needed);
	mvwprintw (osd_stats.w, 2, 2, "Exp Needed: %s", buf->str);
	g_string_free (buf, TRUE);

	mvwprintw (osd_stats.w, 3, 2, "Rate: %s", stats.exp.rate_str->str);
	mvwprintw (osd_stats.w, 3, 20, "Kills: %d", CLAMP (stats.exp.kills, 0, 9999));

	buf = g_string_new ("");
	g_string_sprintf (buf, "Will Level in: %s", stats.exp.eta_str->str);
	mvwaddstr (osd_stats.w, 4, 2, buf->str);
	g_string_free (buf, TRUE);
}


/* =========================================================================
 = OSD_STATS_COMBAT_UPDATE
 =
 = Update stats OSD in combat mode
 ======================================================================== */

static void osd_stats_combat_update (void)
{
	gdouble total, percent;

	wattrset (osd_stats.w, osd_stats.attr);

	total = stats.misses
		+ stats.backstab.hits
		+ stats.normal.hits
		+ stats.magical.hits
		+ stats.critical.hits
		+ stats.extra.hits;

	percent = (total > 0) ? stats.backstab.hits / (total * 0.01) : 0;
	mvwprintw (osd_stats.w, 1,  2, "BS:   %3.0f%%", percent);
	mvwprintw (osd_stats.w, 1, 14, "R: %ld-%ld",
		stats.backstab.damage.min, stats.backstab.damage.max);
	mvwprintw (osd_stats.w, 1, 24, "A: %ld", stats.backstab.damage.avg);

	percent = (total > 0) ? stats.critical.hits / (total * 0.01) : 0;
	mvwprintw (osd_stats.w, 2,  2, "Crit: %3.0f%%", percent);
	mvwprintw (osd_stats.w, 2, 14, "R: %ld-%ld",
		stats.critical.damage.min, stats.critical.damage.max);
	mvwprintw (osd_stats.w, 2, 24, "A: %ld", stats.critical.damage.avg);

	if (combat.strategy.current &&
		combat.strategy.current->type == STRATEGY_SPELL_ATTACK)
	{
		percent = (total > 0) ? stats.magical.hits / (total * 0.01) : 0;
		mvwprintw (osd_stats.w, 3,  2, "Cast: %3.0f%%", percent);
		mvwprintw (osd_stats.w, 3, 14, "R: %ld-%ld",
			stats.magical.damage.min, stats.magical.damage.max);
		mvwprintw (osd_stats.w, 3, 24, "A: %ld", stats.magical.damage.avg);
	}
	else
	{
		percent = (total > 0) ? stats.normal.hits / (total * 0.01) : 0;
		mvwprintw (osd_stats.w, 3,  2, "Hit:  %3.0f%%", percent);
		mvwprintw (osd_stats.w, 3, 14, "R: %ld-%ld",
			stats.normal.damage.min, stats.normal.damage.max);
		mvwprintw (osd_stats.w, 3, 24, "A: %ld", stats.normal.damage.avg);
	}

	percent = (total > 0) ? stats.misses / (total * 0.01) : 0;
	mvwprintw (osd_stats.w, 4,  2, "Miss: %3.0f%%", percent);
}


/* =========================================================================
 = OSD_STATS_PLAYER_UPDATE
 =
 = Update stats OSD in player mode
 ======================================================================== */

static void osd_stats_player_update (void)
{
	stats_player_update ();

	wattrset (osd_stats.w, osd_stats.attr);

	/* TODO: implement this */
	mvwprintw (osd_stats.w, 1,  2, "Cash Flow:  0 copper/hr");

	mvwprintw (osd_stats.w, 2,  2, "Health Low: %d", stats.lowhp);
	mvwprintw (osd_stats.w, 2, 22, "%d%%",
		get_percent (stats.lowhp, character.hp.max));

	mvwprintw (osd_stats.w, 3,  2, "Mana Rate:  %d-%d",
		stats.mana_tick.min, stats.mana_tick.max);
	mvwprintw (osd_stats.w, 3, 22, "Avg: %d", stats.mana_tick.avg);
}


/* =========================================================================
 = OSD_STATS_SESSION_UPDATE
 =
 = Update stats OSD in session mode
 ======================================================================== */

static void osd_stats_session_update (void)
{
	GString *tmp;

	stats_session_update ();

	wattrset (osd_stats.w, osd_stats.attr);

	tmp = get_elapsed_as_g_string (
		(gulong) timer_elapsed (timers.mudpro, NULL));
	mvwprintw (osd_stats.w, 1, 2, "MudPRO:       %s", tmp->str);
	g_string_free (tmp, TRUE);

	tmp = get_elapsed_as_g_string (
		(gulong) timer_elapsed (timers.online, NULL) + stats.online);
	mvwprintw (osd_stats.w, 2, 2, "Online:       %s", tmp->str);
	g_string_free (tmp, TRUE);

	mvwprintw (osd_stats.w, 3, 2, "Connected:    %d", stats.connects);
	mvwprintw (osd_stats.w, 4, 2, "Disconnected: %d", stats.disconnects);
}


/* =========================================================================
 = OSD_STATS_RESET_CURRENT
 =
 = Reset currently displayed stats
 ======================================================================== */

void osd_stats_reset_current (void)
{
	switch (osd_stats_mode)
	{
	case OSD_STATS_EXP:     stats_exp_reset ();     break;
	case OSD_STATS_COMBAT:  stats_combat_reset ();  break;
	case OSD_STATS_PLAYER:  stats_player_reset ();  break;
	case OSD_STATS_SESSION: stats_session_reset (); break;
	default:
		g_assert_not_reached ();
	}
}


/* =========================================================================
 = OSD_STATS_RESET_ALL
 =
 = Reset all stats
 ======================================================================== */

void osd_stats_reset_all (void)
{
	stats_exp_reset ();
	stats_combat_reset ();
	stats_player_reset ();
	stats_session_reset ();
}


/* =========================================================================
 = OSD_STATS_TOGGLE_CONTEXT
 =
 = Toggle automatic context switching of stats OSD
 ======================================================================== */

void osd_stats_toggle_context (void)
{
	OSD_LOCK_RESET;
	character.option.osd_context = !character.option.osd_context;

	if (!character.option.osd_context)
		osd_stats_exp_set_lock ();
	else
		osd_stats_update ();
}


/* =========================================================================
 = OSD_STATS_SET_MODE
 =
 = Set the stats display mode
 ======================================================================== */

void osd_stats_set_mode (gint mode)
{
	if (timer_elapsed (timers.osd_stats, NULL) || osd_stats_mode == mode)
		return;

	g_assert (mode >= OSD_STATS_EXP);
	g_assert (mode <= OSD_STATS_SESSION);

	osd_stats_mode = mode;
	timer_start (timers.osd_stats);
}


/* =========================================================================
 = OSD_STATS_EXP_SET_LOCK
 =
 = Lock experience stats display
 ======================================================================== */

void osd_stats_exp_set_lock (void)
{
	OSD_LOCK_RESET;
	character.option.osd_context = FALSE;
	osd_stats_lock_exp = TRUE;
	osd_stats_mode = OSD_STATS_EXP;
	osd_stats_update ();
}


/* =========================================================================
 = OSD_STATS_COMBAT_SET_LOCK
 =
 = Lock combat stats display
 ======================================================================== */

void osd_stats_combat_set_lock (void)
{
	OSD_LOCK_RESET;
	character.option.osd_context = FALSE;
	osd_stats_lock_combat = TRUE;
	osd_stats_mode = OSD_STATS_COMBAT;
	osd_stats_update ();
}


/* =========================================================================
 = OSD_STATS_PLAYER_SET_LOCK
 =
 = Lock player stats display
 ======================================================================== */

void osd_stats_player_set_lock (void)
{
	OSD_LOCK_RESET;
	character.option.osd_context = FALSE;
	osd_stats_lock_player = TRUE;
	osd_stats_mode = OSD_STATS_PLAYER;
	osd_stats_update ();
}


/* =========================================================================
 = OSD_STATS_SESSION_SET_LOCK
 =
 = Lock session stats display
 ======================================================================== */

void osd_stats_session_set_lock (void)
{
	OSD_LOCK_RESET;
	character.option.osd_context = FALSE;
	osd_stats_lock_session = TRUE;
	osd_stats_mode = OSD_STATS_SESSION;
	osd_stats_update ();
}


/* =========================================================================
 = OSD_STATS_MAP
 =
 = Map stats OSD on the screen
 ======================================================================== */

void osd_stats_map (void)
{
	if (osd_stats.visible)
		return;

	show_panel (osd_stats.p);
	osd_stats.visible = TRUE;
	osd_stats_update ();
	menubar_raise ();
}


/* =========================================================================
 = OSD_STATS_UNMAP
 =
 = Unmap the stats OSD from the screen
 ======================================================================== */

void osd_stats_unmap (void)
{
	if (!osd_stats.visible)
		return;

	hide_panel (osd_stats.p);
	osd_stats.visible = FALSE;
	update_display ();
}


/* =========================================================================
 = OSD_STATUS_GAUGE
 =
 = Displays a status gauge
 ======================================================================== */

static void osd_status_gauge (cwin_t *cwin, gint percent)
{
	gint i;

	percent = CLAMP (percent, 0, 100);

	for (i = 0; i < 100; i += 10)
	{
		if (percent == 0 || (i && percent < i+10))
			wattrset (cwin->w, ATTR_GAUGE | A_BOLD);
		else
			wattrset (cwin->w, ATTR_HILITE | A_BOLD);

		waddch (cwin->w, widget_ui.gauge);
	}
	wattrset (cwin->w, cwin->attr);
}


/* =========================================================================
 = OSD_DOCK_TOGGLE_MAPVIEW
 =
 = Toggle mapview OSD in dock
 ======================================================================== */

void osd_dock_toggle_mapview (void)
{
	character.option.osd_mapview = !character.option.osd_mapview;
	osd_dock_restack ();
}


/* =========================================================================
 = OSD_DOCK_TOGGLE_VITALS
 =
 = Toggle vitals OSD in dock
 ======================================================================== */

void osd_dock_toggle_vitals (void)
{
	character.option.osd_vitals = !character.option.osd_vitals;
	osd_dock_restack ();
}


/* =========================================================================
 = OSD_DOCK_TOGGLE_STATS
 =
 = Toggle stats OSD in dock
 ======================================================================== */

void osd_dock_toggle_stats (void)
{
	character.option.osd_stats = !character.option.osd_stats;
	osd_dock_restack ();
}


/* =========================================================================
 = OSD_DOCK_RESTACK
 =
 = Restack dock OSD panels
 ======================================================================== */

void osd_dock_restack (void)
{
	gint ypos = 2;

	mapview_cleanup ();
	osd_vitals_cleanup ();
	osd_stats_cleanup ();

	if (character.flag.ready &&
		character.option.osd_mapview)
	{
		if (LINES < (OSD_MAPVIEW_HEIGHT + ypos) ||
			COLS  < (OSD_MAPVIEW_WIDTH  + 4))
			return;

		mapview_init (ypos, COLS - (OSD_MAPVIEW_WIDTH + 2));
		ypos += OSD_MAPVIEW_HEIGHT;
		mapview_map ();
	}

	if (character.flag.ready &&
		character.option.osd_vitals)
	{
		if (LINES < (OSD_VITALS_HEIGHT + ypos) ||
			COLS  < (OSD_VITALS_WIDTH  + 4))
			return;

		osd_vitals_init (ypos, COLS - (OSD_VITALS_WIDTH + 2));
		ypos += OSD_VITALS_HEIGHT;
		osd_vitals_map ();
	}

	if (character.option.osd_stats)
	{
		if (LINES < (OSD_STATS_HEIGHT + ypos) ||
			COLS  < (OSD_STATS_WIDTH  + 4))
			return;

		osd_stats_init (ypos, COLS - (OSD_STATS_WIDTH + 2));
		ypos += OSD_STATS_HEIGHT;
		osd_stats_map ();
	}
}
