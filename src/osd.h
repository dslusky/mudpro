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

#ifndef __OSD_H__
#define __OSD_H__

#include "widgets.h"

#define OSD_MAPVIEW_WIDTH   33
#define OSD_MAPVIEW_HEIGHT  11
#define OSD_VITALS_WIDTH    33
#define OSD_VITALS_HEIGHT   5
#define OSD_STATS_WIDTH     33
#define OSD_STATS_HEIGHT    6

#define OSD_LOCK_ENABLED \
	(osd_stats_lock_exp | osd_stats_lock_combat | \
	 osd_stats_lock_player | osd_stats_lock_session)

#define OSD_LOCK_RESET \
	osd_stats_lock_exp = osd_stats_lock_combat = \
	osd_stats_lock_player = osd_stats_lock_session = 0

enum /* stat osd modes */
{
	OSD_STATS_NONE,
	OSD_STATS_EXP,
	OSD_STATS_COMBAT,
	OSD_STATS_PLAYER,
	OSD_STATS_SESSION
};

extern cwin_t osd_vitals;
extern cwin_t osd_stats;
extern gint osd_stats_mode;

extern gboolean osd_stats_lock_exp;
extern gboolean osd_stats_lock_combat;
extern gboolean osd_stats_lock_player;
extern gboolean osd_stats_lock_session;

void osd_vitals_init (gint y, gint x);
void osd_vitals_cleanup (void);
void osd_vitals_update (void);
void osd_vitals_map (void);
void osd_vitals_unmap (void);

void osd_stats_init (gint y, gint x);
void osd_stats_cleanup (void);
void osd_stats_update (void);
void osd_stats_reset_current (void);
void osd_stats_reset_all (void);
void osd_stats_toggle_context (void);
void osd_stats_set_mode (gint mode);
void osd_stats_exp_set_lock (void);
void osd_stats_combat_set_lock (void);
void osd_stats_player_set_lock (void);
void osd_stats_session_set_lock (void);
void osd_stats_map (void);
void osd_stats_unmap (void);

void osd_dock_toggle_mapview (void);
void osd_dock_toggle_vitals (void);
void osd_dock_toggle_stats (void);
void osd_dock_restack (void);

#endif /* __OSD_H__ */
