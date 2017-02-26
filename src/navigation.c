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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "automap.h"
#include "autoroam.h"
#include "character.h"
#include "client_ai.h"
#include "combat.h"
#include "command.h"
#include "navigation.h"
#include "monster.h"
#include "mudpro.h"
#include "terminal.h"
#include "utils.h"

navigation_t navigation;
exit_info_t *destination;
static exit_info_t exit_info_lost;

static GNode *navigation_create_route_propagate (automap_record_t *anchor, GSList *rooms);
static automap_record_t *navigation_create_route_propagate_verify (exit_info_t *exit_info);
static void navigation_clear_route_flag (gpointer key, gpointer value, gpointer user_data);


/* =========================================================================
 = NAVIGATION_INIT
 =
 = Initialize navigation
 ======================================================================== */

void navigation_init (void)
{
	memset (&navigation, 0, sizeof (navigation_t));
	memset (&exit_info_lost, 0, sizeof (exit_info_t));
	client_ai_movement_reset ();
}


/* =========================================================================
 = NAVIGATION_CLEANUP
 =
 = Cleanup navigation data
 ======================================================================== */

void navigation_cleanup (void)
{
	if (navigation.route)
		navigation_route_free ();

	if (navigation.anchors)
		navigation_anchor_list_free ();
}


/* =========================================================================
 = NAVIGATION_REPORT
 =
 = Report current status of navigation module to specified file
 ======================================================================== */

void navigation_report (FILE *fp)
{
	automap_record_t *record;


	fprintf (fp, "\nNAVIGATION MODULE\n"
				 "=================\n\n");

	if (destination)
	{
		record = automap_db_lookup (destination->id);

		fprintf (fp, "  Current Destination: %s (%s)\n",
			destination->id,
			record ? record->name : "N/A");
	}
	else
		fprintf (fp, "  Current Destination: None\n");

	if (navigation.route)
		fprintf (fp, "  Current Route: %p (%d locations)\n\n",
			navigation.route, g_slist_length (navigation.route));
	else
		fprintf (fp, "  Current Route: None\n\n");

	if (navigation.anchors)
	{
		GSList *node;
		gint count = 1;

		fprintf (fp, "  Anchors Pending:\n\n");

		for (node = navigation.anchors; node; node = node->next)
		{
			record = automap_db_lookup ((gchar *) node->data);

			fprintf (fp, "    [%2d] %s (%s)\n",
				count++, record->id, record->name);
		}
		fprintf (fp, "\n");
	}
	else
		fprintf (fp, "  Anchors Pending: None\n\n");

	fprintf (fp, "  Collisions ... %d\n", navigation.collisions);
	fprintf (fp, "  Steps Ran .... %d\n", navigation.steps_ran);
	fprintf (fp, "  Flags ........ %ld\n", navigation.flags);
	fprintf (fp, "  Room Dark .... %s\n",
		navigation.room_dark ? "YES" : "NO");
	fprintf (fp, "  PC Present ... %s\n",
        navigation.pc_present ? "YES" : "NO");
	fprintf (fp, "  NPC Present .. %s\n\n",
		navigation.npc_present ? "YES" : "NO");
}


/* =========================================================================
 = NAVIGATION_AUTOROAM
 =
 = Wander the realm
 ======================================================================== */

exit_info_t *navigation_autoroam (gint mode)
{
	exit_info_t *exit_info, *retval = NULL;
	automap_record_t *record;
	GTimeVal tv;
	GSList *node;
	gulong flags = 0;

	if (automap.lost || !automap.location)
		return navigation_autoroam_lost ();

	if (mode == NAVIGATE_BACKWARD)
		memset (&tv, 0, sizeof (GTimeVal));
	else
		g_get_current_time (&tv);

	for (node = automap.location->exit_list; node; node = node->next)
	{
		exit_info = node->data;

		if (!strcmp (exit_info->id, "0") ||
			((exit_info->flags & EXIT_FLAG_DOOR) && !autoroam_opts.use_doors) ||
			((exit_info->flags & EXIT_FLAG_SECRET) && !autoroam_opts.use_secrets) ||
			 (exit_info->flags & EXIT_FLAG_BLOCKED) ||
			 !(autoroam_opts.exits & exit_info->direction))
			continue;

		if ((record = automap_db_lookup (exit_info->id)) == NULL)
			continue;

		if ((record->flags & ROOM_FLAG_NOROAM) ||
			(record->flags & ROOM_FLAG_NOENTER))
			continue; /* do not enter room */

		if ((mode == NAVIGATE_BACKWARD && (record->visited.tv_sec >= tv.tv_sec)) ||
			(mode == NAVIGATE_FORWARD  && (record->visited.tv_sec <= tv.tv_sec)))
		{
			tv.tv_sec = record->visited.tv_sec;
			retval    = exit_info;
			flags     = record->flags;
		}
	}

	if (retval && retval->direction > EXIT_NONE)
	{
		navigation.flags = flags;
		return retval;
	}

	printt ("Navigation: autoroam failed");
    mudpro_audit_log_append ("Navigation halted - autoroam failed");
	autoroam_opts.enabled  = FALSE;
	client_ai_movement_reset ();

	return NULL;
}


/* =========================================================================
 = NAVIGATION_AUTOROAM_LOST
 =
 = Wander the realm while lost
 ======================================================================== */

exit_info_t *navigation_autoroam_lost (void)
{
	static gulong origin = EXIT_NONE;
	exit_table_t *et;

	for (et = exit_table; et->direction && et->direction <= EXIT_DOWN; et++)
	{
		if (!(automap.obvious.exits & et->direction) ||
			!(autoroam_opts.exits & et->direction))
			continue;

		/* try to autoroam someplace other than we came */
		if (origin == et->direction &&
			(automap.obvious.exits & (autoroam_opts.exits & ~origin)))
			continue;

		origin = et->opposite;
		exit_info_lost.id        = NULL;
		exit_info_lost.str       = NULL;
		exit_info_lost.direction = et->direction;
		exit_info_lost.flags     = 0;
		navigation.flags         = 0;

		return &exit_info_lost;
	}

	return NULL;
}


/* =========================================================================
 = NAVIGATION_ROUTE_NEXT
 =
 = Returns exit_info_t for next location in route
 ======================================================================== */

exit_info_t *navigation_route_next (gint mode)
{
	GSList *node;
	automap_record_t *dest;
	exit_info_t *exit_info;

	if (!navigation.route)
		return NULL;

	if (!automap.location || automap.lost)
		return navigation_autoroam_lost ();

	g_assert (navigation.route->data != NULL);
	dest = navigation.route->data;

	for (node = automap.location->exit_list; node; node = node->next)
	{
		exit_info = node->data;
		if (!strcmp (exit_info->id, dest->id))
		{
			navigation.flags = dest->flags;
			return exit_info;
		}
	}

	printt ("Navigation: no matching exit in route!");
	mudpro_audit_log_append ("Navigation failed: no matching exit in route!");

	if (navigation.route)
		navigation_route_free ();

	return NULL;
}


/* =========================================================================
 = NAVIGATION_CREATE_ROUTE
 =
 = Creates path to next anchor
 ======================================================================== */

void navigation_create_route (void)
{
	automap_record_t *anchor;
	GSList *rooms = NULL;
	GNode *dest, *root = NULL;

	if (!navigation.anchors)
		return; /* cannot create route without an anchor */

	if ((anchor = automap_db_lookup ((gchar *) navigation.anchors->data)) == NULL)
		return; /* anchor is invalid */

	if (navigation.route)
		navigation_route_free ();

	g_hash_table_foreach (automap.db, navigation_clear_route_flag, NULL);

	automap.location->_route_flag = TRUE;
	root = g_node_new (automap.location);
	rooms = g_slist_prepend (rooms, root);

	dest = navigation_create_route_propagate (anchor, rooms);

	for (GNode *node = dest; node->parent; node = node->parent) {
		navigation.route = g_slist_prepend (navigation.route, node->data);
	}

	g_node_destroy (root);

	if (navigation.route)
    {
		printt ("Walking to %s", anchor->name);
		mudpro_audit_log_append ("Walking to %s", anchor->name);
    }
	else
	{
		printt ("Route creation failed for %s!", anchor->name);
		mudpro_audit_log_append ("Route creation failed for %s!", anchor->name);
		navigation_anchor_del ();
	}
}


/* =========================================================================
 = NAVIGATION_CREATE_ROUTE_PROPAGATE
 =
 = Propagate through locations and determine shortest path
 ======================================================================== */

static GNode *navigation_create_route_propagate (automap_record_t *anchor, GSList *rooms) {
	GSList *adjacent_rooms = NULL;

	for (GSList *r = rooms; r; r = r->next)
	{
		GNode *node = r->data;
		automap_record_t *room = node->data;

		if (!strcmp (room->id, anchor->id)) {
			return node; // found shortest path to destination
		}

		for (GSList *e = room->exit_list; e; e = e->next) {
			exit_info_t *ei = e->data;
			automap_record_t *adjacent;

			adjacent = navigation_create_route_propagate_verify (ei);

			if (adjacent) {
				adjacent->_route_flag = TRUE;
				adjacent_rooms = g_slist_prepend (adjacent_rooms,
					g_node_prepend_data (node, adjacent));
			}
		}
	}

	g_slist_free(rooms);

	if (adjacent_rooms)
		return navigation_create_route_propagate (anchor, adjacent_rooms);

	return NULL;
}


/* =========================================================================
 = NAVIGATION_CREATE_ROUTE_PROPAGATE_VERIFY
 =
 = Verify exit is valid and location can be added to route
 ======================================================================== */

static automap_record_t *navigation_create_route_propagate_verify (
	exit_info_t *exit_info)
{
	automap_record_t *record = NULL;

	if (!strcmp (exit_info->id, "0"))
		return NULL;

	if ((record = automap_db_lookup (exit_info->id)) == NULL)
		return NULL;

	if (record->_route_flag || (record->flags & ROOM_FLAG_NOENTER))
		return NULL;

	return record; /* location is ok */
}


/* =========================================================================
 = NAVIGATION_CLEAR_ROUTE_FLAG
 =
 = Clears route_flag for location in automap database
 ======================================================================== */

static void navigation_clear_route_flag (gpointer key, gpointer value,
	gpointer user_data)
{
	automap_record_t *record = value;
	record->_route_flag = 0;
}


/* =========================================================================
 = NAVIGATION_ROUTE_FREE
 =
 = Clear the current route
 ======================================================================== */

void navigation_route_free (void)
{
	client_ai_movement_reset ();
	g_slist_free (navigation.route);
	navigation.route = NULL;
}


/* =========================================================================
 = NAVIGATION_ROUTE_STEP
 =
 = Remove last step from route
 ======================================================================== */

void navigation_route_step (void)
{
	navigation.route = g_slist_remove (navigation.route,
		navigation.route->data);

	if (!navigation.route)
	{
		navigation_anchor_del ();

		if (navigation.anchors)
			navigation_create_route ();
		else
		{
			printt ("Destination reached");
			mudpro_audit_log_append ("Destination reached");

			if (character.option.restore_attack)
				character.option.attack = TRUE;
		}
	}
}


/* =========================================================================
 = NAVIGATION_ANCHOR_LIST_FREE
 =
 = Clear list of anchors
 ======================================================================== */

void navigation_anchor_list_free (void)
{
	g_slist_free (navigation.anchors);
	navigation.anchors = NULL;
}

/* =========================================================================
 = NAVIGATION_ANCHOR_ADD
 =
 = Add anchor to queue
 ======================================================================== */

void navigation_anchor_add (gchar *id, gboolean clear)
{
	automap_record_t *record;

	g_assert (id != NULL);

	if (clear) /* clear any other anchors set */
		navigation_anchor_list_free ();

	if (automap.lost || !automap.location)
	{
		printt ("Cannot create route while lost!");
		return;
	}

	if ((record = automap_db_lookup (id)) == NULL)
		return; /* not in database */

	if (automap.enabled)
	{
		printt ("Automapping disabled");
		automap.enabled = FALSE;
	}

	navigation.anchors = g_slist_prepend (navigation.anchors, record->id);
}


/* =========================================================================
 = NAVIGATION_ANCHOR_DEL
 =
 = Remove anchor from queue
 ======================================================================== */

void navigation_anchor_del (void)
{
	if (!navigation.anchors)
		return; /* nothing to do */

	/* remove the last anchor */
	navigation.anchors = g_slist_remove (navigation.anchors,
		navigation.anchors->data);
}


/* =========================================================================
 = NAVIGATION_DETOUR
 =
 = Add detour locations to anchor queue
 ======================================================================== */

void navigation_detour (void)
{
	gchar *id, *offset;

	if (!destination || !destination->str || destination->str[0] == '\0')
	{
		printt ("No detour locations defined!");
		mudpro_audit_log_append ("No detour locations defined!");
		client_ai_movement_reset ();
		return;
	}

	/* place detour location(s) in anchor queue */

	offset = destination->str;
	while ((id = get_token_as_str (&offset)) != NULL)
	{
		printt ("DEBUG adding %s to anchor queue", id);
		navigation_anchor_add (id, FALSE /* clear list */);
		g_free (id);
	}

	/* reset current route */
	navigation_route_free ();
}


/* =========================================================================
 = NAVIGATION_RESET_ROUTE
 =
 = Reset the current route and display a status message
 ======================================================================== */

void navigation_reset_route (void)
{
	printt ("Navigation: Route Reset");
	mudpro_audit_log_append ("Navigation route reset");
	navigation_route_free ();
	navigation_anchor_del ();
}


/* =========================================================================
 = NAVIGATION_RESET_ALL
 =
 = Reset all navigation data and display a status message
 ======================================================================== */

void navigation_reset_all (void)
{
	printt ("Navigation: Full Reset");
	mudpro_audit_log_append ("Navigation full reset");
	navigation_cleanup ();
}


/* =========================================================================
 = NAVIGATION_SYS_GOTO
 =
 = Handle using sys goto
 ======================================================================== */

void navigation_sys_goto (void)
{
	if (!character.option.sys_goto)
	{
		printt ("Navigation: sys goto option not enabled!");
		return;
	}

	if (character.targets)
		monster_target_list_free ();

	if (character.state == STATE_ENGAGED)
	{
		combat.force_break = TRUE;
		combat_reset ();
	}

	if (character.sys_goto &&
		character.sys_goto[0] != '\0')
		send_line_va ("sys goto %s", character.sys_goto);
	else
		send_line ("sys goto silv");

	character.flag.sys_goto = TRUE;

	automap_reset (TRUE /* full reset */);
	command_send (CMD_TARGET);
}
