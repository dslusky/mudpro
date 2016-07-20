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

#ifndef __NAVIGATION_H__
#define __NAVIGATION_H__

#include "automap.h"

#define NAVIGATION_COLLISION_MAX 5 /* collisions allowed before stopping */

typedef struct
{
	GSList *anchors;      /* anchor queue */
	GSList *route;        /* current route */
	guint steps_ran;      /* number of steps we've ran */
	guint collisions;     /* number of failed movements */
	gulong flags;	      /* room flags for _destination_ */
	gboolean room_dark;   /* current room too dark to see */
	gboolean pc_present;  /* Another player at location */
	gboolean npc_present; /* NPC at location */
} navigation_t;

enum /* navigation type */
{
	NAVIGATE_NONE,
	NAVIGATE_FORWARD,
	NAVIGATE_BACKWARD,
};

extern navigation_t navigation;
extern exit_info_t *destination;

void navigation_init (void);
void navigation_cleanup (void);
void navigation_report (FILE *fp);
exit_info_t *navigation_autoroam (gint mode);
exit_info_t *navigation_autoroam_lost (void);
exit_info_t *navigation_route_next (gint mode);
void navigation_create_route (void);
void navigation_route_free (void);
void navigation_route_step (void);
void navigation_anchor_list_free (void);
void navigation_anchor_add (gchar *id, gboolean clear);
void navigation_anchor_del (void);
void navigation_detour (void);
void navigation_reset_route (void);
void navigation_reset_all (void);
void navigation_sys_goto (void);

#endif /* __NAVIGATION_H__ */
