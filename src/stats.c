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

#include <stdlib.h>
#include <string.h>

#include "character.h"
#include "stats.h"
#include "timers.h"
#include "utils.h"

stats_t stats;


/* ==========================================================================
 = STATS_INIT
 =
 = Initialize stats module
 ========================================================================= */

void stats_init (void)
{
	memset (&stats, 0, sizeof (stats_t));

	stats.exp.rate_str = g_string_new ("");
	stats.exp.eta_str =  g_string_new ("");
}


/* ==========================================================================
 = STATS_CLEANUP
 =
 = Clean up after stats module
 ========================================================================= */

void stats_cleanup (void)
{
	g_string_free (stats.exp.rate_str, TRUE);
	g_string_free (stats.exp.eta_str, TRUE);
}


/* ==========================================================================
 = STATS_EXP_UPDATE
 =
 = Calculate the current exp rate stats
 ========================================================================= */

void stats_exp_update (void)
{
	gdouble elapsed;
	gint x;

	elapsed = timer_elapsed (timers.duration, NULL);

	/* calculate rate/eta */

	stats.exp.rate = (stats.exp.gained / (elapsed / 60.0)) * 60.0;

	if (stats.exp.rate > 10000)
		g_string_sprintf (stats.exp.rate_str, "%.1f k/hr", stats.exp.rate / 1000.0);
	else
		g_string_sprintf (stats.exp.rate_str, "%d/hr", (gint) stats.exp.rate);

	if (stats.exp.rate)
		stats.exp.eta = stats.exp.needed / (stats.exp.rate / 60.0);
	else
		stats.exp.eta = 0;

	/* build stats.exp.eta_str */
	if ((x = stats.exp.eta / 43200.0) >= 1)
		g_string_sprintf (stats.exp.eta_str, "%d-%d Months", x, x+1);

	else if ((x = stats.exp.eta / 1440.0) >= 2)
		g_string_sprintf (stats.exp.eta_str, "%d-%d Days", x, x+1);

	else if ((x = stats.exp.eta / 60.0) >= 1)
		g_string_sprintf (stats.exp.eta_str, "%d-%d Hours", x, x+1);

	else if (stats.exp.eta >= 1)
		g_string_sprintf (stats.exp.eta_str, "%d Minutes", (gint) stats.exp.eta);

	else if (stats.exp.eta > 0)
		g_string_sprintf (stats.exp.eta_str, "Any Moment");

	else if (stats.exp.eta == 0 && stats.exp.rate)
		g_string_sprintf (stats.exp.eta_str, "Now!");

	else
		stats.exp.eta_str = g_string_assign (stats.exp.eta_str, "Eternity!");
}


/* ==========================================================================
 = STATS_EXP_RESET
 =
 = Reset experience stats
 ========================================================================= */

void stats_exp_reset (void)
{
	timer_reset (timers.duration);
	stats.exp.gained = 0;
	stats.exp.kills = 0;
}


/* =========================================================================
 = STATS_COMBAT_ADD_DAMAGE
 =
 = Adds damage to combat stats
 ======================================================================== */

void stats_combat_add_damage (combat_stats_t *s, gint value)
{
	g_assert (s != NULL);

	value = MAX (0, value);

	s->damage.total += value;
	s->damage.avg = s->damage.total / ++s->hits;

	if (!s->damage.min)
		s->damage.min = value;

	s->damage.min = MIN (s->damage.min, value);
	s->damage.max = MAX (s->damage.max, value);
}


/* =========================================================================
 = STATS_COMBAT_RESET
 =
 = Reset combat statistics
 ======================================================================== */

void stats_combat_reset (void)
{
	stats.misses = 0;
	memset (&stats.backstab, 0, sizeof (combat_stats_t));
	memset (&stats.normal, 0, sizeof (combat_stats_t));
	memset (&stats.magical, 0, sizeof (combat_stats_t));
	memset (&stats.critical, 0, sizeof (combat_stats_t));
	memset (&stats.extra, 0, sizeof (combat_stats_t));
}


/* =========================================================================
 = STATS_PLAYER_UPDATE
 =
 = Update player stats
 ======================================================================== */

void stats_player_update (void)
{
	if (character.flag.ready && !stats.lowhp)
		stats.lowhp = character.hp.now;

	if (character.tick.current)
	{
		if (!stats.mana_tick.min)
			stats.mana_tick.min = character.tick.current;

		stats.mana_tick.min = MIN (stats.mana_tick.min, character.tick.current);
		stats.mana_tick.max = MAX (stats.mana_tick.max, character.tick.current);

		stats.mana_tick.avg = stats.mana_tick.count
			? stats.mana_tick.total / stats.mana_tick.count : 0;
	}
}


/* =========================================================================
 = STATS_PLAYER_RESET
 =
 = Reset player stats
 ======================================================================== */

void stats_player_reset (void)
{
	stats.cash = 0;
	stats.lowhp = 0;
	memset (&stats.mana_tick, 0, sizeof (stats.mana_tick));
}


/* =========================================================================
 = STATS_SESSION_UPDATE
 =
 = Update session stats
 ======================================================================== */

void stats_session_update (void)
{
}


/* =========================================================================
 = STATS_SESSION_RESET
 =
 = Reset session stats
 ======================================================================== */

void stats_session_reset (void)
{
	timer_reset (timers.mudpro);
	timer_reset (timers.online);
	stats.online      = 0;
	stats.connects    = 0;
	stats.disconnects = 0;
}
