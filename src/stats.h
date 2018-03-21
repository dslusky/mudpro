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

#ifndef __STATS_H__
#define __STATS_H__

#include <glib.h>

typedef struct
{
	gulong hits;

	struct {
		gulong total;
		gulong min;
		gulong max;
		gulong avg;
	} damage;
} combat_stats_t;

typedef struct
{
	/* misc stats */
	gulong online;
	gulong cash;
	gint lowhp;
	gint connects;
	gint disconnects;

	struct /* mana stats */
	{
		gulong total;
		gulong count;
		gint min;
		gint max;
		gint avg;
	} mana_tick;

	struct /* experience stats */
	{
		GString *rate_str;
		GString *eta_str;
		gdouble total;
		gdouble gained;
		gdouble needed;
		gdouble rate;
		gdouble eta;
		gint kills;
	} exp;

	/* combat stats */
	gulong misses;
	combat_stats_t backstab;
	combat_stats_t normal;
	combat_stats_t magical;
	combat_stats_t critical;
	combat_stats_t extra;
} stats_t;

extern stats_t stats;

void stats_init (void);
void stats_cleanup (void);
void stats_exp_update (void);
void stats_exp_reset (void);
void stats_combat_add_damage (combat_stats_t *s, gint value);
void stats_combat_reset (void);
void stats_player_update (void);
void stats_player_reset (void);
void stats_session_update (void);
void stats_session_reset (void);

#endif /* __STATS_H__ */
