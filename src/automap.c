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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __linux__
#include <unistd.h>
#endif

#include "automap.h"
#include "client_ai.h"
#include "combat.h"
#include "command.h"
#include "character.h"
#include "defs.h"
#include "guidebook.h"
#include "item.h"
#include "mapview.h"
#include "menubar.h"
#include "monster.h"
#include "mudpro.h"
#include "navigation.h"
#include "terminal.h"
#include "timers.h"
#include "utils.h"


automap_t automap;

static key_value_t exit_type[]=
{
	{ "secret passage ",    EXIT_SECRET },
	{ "open fireplace ",    EXIT_SECRET },
	{ "concealed passage ", EXIT_SECRET },
	{ "huge passage ",      EXIT_SECRET },
	{ "gaping hole ",       EXIT_SECRET },
	{ "rocky passage ",     EXIT_SECRET },
	{ "crack in tree ",     EXIT_SECRET },
	{ "dark passageway ",   EXIT_SECRET },
	{ "cramped passage ",   EXIT_SECRET },
	{ "open door ",         EXIT_DOOR_OPEN },
	{ "open gate ",         EXIT_DOOR_OPEN },
	{ "open trap door ",    EXIT_DOOR_OPEN },
	{ "closed door ",       EXIT_DOOR_CLOSED },
	{ "closed gate ",       EXIT_DOOR_CLOSED },
	{ "closed trap door ",  EXIT_DOOR_CLOSED },
	{ NULL, 0 }
};

exit_table_t exit_table[]=
{
	{ "NONE!!!",    "NONE",  EXIT_NONE,       EXIT_NONE,       0,  0,  0 },
	{ "north",      "n",     EXIT_NORTH,      EXIT_SOUTH,      0,  1,  0 },
	{ "south",      "s",     EXIT_SOUTH,      EXIT_NORTH,      0, -1,  0 },
	{ "east",       "e",     EXIT_EAST,       EXIT_WEST,       1,  0,  0 },
	{ "west",       "w",     EXIT_WEST,       EXIT_EAST,      -1,  0,  0 },
	{ "northeast",  "ne",    EXIT_NORTHEAST,  EXIT_SOUTHWEST,  1,  1,  0 },
	{ "northwest",  "nw",    EXIT_NORTHWEST,  EXIT_SOUTHEAST, -1,  1,  0 },
	{ "southeast",  "se",    EXIT_SOUTHEAST,  EXIT_NORTHWEST,  1, -1,  0 },
	{ "southwest",  "sw",    EXIT_SOUTHWEST,  EXIT_NORTHEAST, -1, -1,  0 },
	{ "up",         "u",     EXIT_UP,         EXIT_DOWN,       0,  0,  1 },
	{ "down",       "d",     EXIT_DOWN,       EXIT_UP,         0,  0, -1 },
	{ "above",      NULL,    EXIT_UP,         EXIT_DOWN,       0,  0,  1 },
	{ "below",      NULL,    EXIT_DOWN,       EXIT_UP,         0,  0, -1 },
	{ "upwards",    NULL,    EXIT_UP,         EXIT_DOWN,       0,  0,  1 },
	{ "downwards",  NULL,    EXIT_DOWN,       EXIT_UP,         0,  0, -1 },
	{ NULL, NULL, 0, 0, 0, 0, 0 }
};

static void automap_report_exit_list (FILE *fp);
static void automap_parse_exit_info (automap_record_t *record, gchar *str);
static void automap_record_save (gpointer key, gpointer value, gpointer user_data);
static void automap_record_deallocate (gpointer key, gpointer value, gpointer user_data);
static void automap_movement_list_free (void);
static automap_record_t *automap_find_location (void);
static void automap_location_search (gpointer key, gpointer value, gpointer user_data);
static void automap_duplicate_search (gpointer key, gpointer value, gpointer user_data);
static void automap_location_dereference (gpointer key, gpointer value, gpointer user_data);
static automap_record_t *automap_location_get_next (automap_record_t *record, exit_table_t *et);
static gchar *automap_get_exit_id (automap_record_t *location, gint direction);
static void automap_set_exit_id (automap_record_t *location, gchar *id, gint direction, gchar *exit_str);
static void automap_join_locations (automap_record_t *first, automap_record_t *second, exit_table_t *et);
static void automap_update_location (void);
static gint automap_get_exit_type (gchar *str, gchar **offset);
static void automap_set_obvious_exit (gint type, gint flag);


/* =========================================================================
 = AUTOMAP_INIT
 =
 = Initialize automapper
 ======================================================================== */

void automap_init (void)
{
	GTimeVal tv;

	memset (&automap, 0, sizeof (automap_t));
	automap.key        = g_string_new ("");
	automap.user_input = g_string_new ("");

	g_get_current_time (&tv);
	srand ((unsigned int) tv.tv_sec);

	automap.room_name = g_string_new ("");
	automap_reset (TRUE /* full reset */);

	mudpro_db.automap.filename = g_strdup_printf (
		"%s%cautomap.db", character.data_path, G_DIR_SEPARATOR);

	automap_db_load (); /* must come after automap.lost has been (un)set */
}


/* =========================================================================
 = AUTOMAP_CLEANUP
 =
 = Cleanup after automapper
 ======================================================================== */

void automap_cleanup (void)
{
	automap_db_save ();
	automap_db_free ();
	automap_movement_list_free ();

	g_free (mudpro_db.automap.filename);
	g_string_free (automap.room_name, TRUE);

	g_string_free (automap.key, TRUE);
	g_string_free (automap.user_input, TRUE);
}


/* =========================================================================
 = AUTOMAP_REPORT
 =
 = Report current status of automap module to the specified file
 ======================================================================== */

void automap_report (FILE *fp)
{
	exit_table_t *et;

	fprintf (fp, "\nAUTOMAP MODULE\n"
				 "==============\n\n");

	fprintf (fp, "  Automap Status: %s\n",
		automap.enabled ? "Enabled" : "Disabled");

	fprintf (fp, "  Automap Coordinates: (X: %ld) (Y: %ld) (Z: %ld)\n",
		automap.x, automap.y, automap.z);

	if (automap.location)
	{
		automap_record_t *record;
		gchar *buf = NULL;

		record = automap_db_lookup (automap.location->id);

		if (record && record->visited.tv_sec)
		{
			buf = g_strdup_printf ("%s",
				ctime (&record->visited.tv_sec));
			strchomp (buf);
		}

		fprintf (fp, "\n  Automap Location: %s (%s)\n\n",
			automap.location->id, record ? record->name : "N/A");

		fprintf (fp, "    Last Visited .. %s\n", buf ? buf : "N/A");
		fprintf (fp, "    Coordinates ... (X: %ld) (Y: %ld) (Z: %ld)\n",
			automap.location->x, automap.location->y, automap.location->z);
		fprintf (fp, "    Exits ......... %ld\n", automap.location->exits);
		fprintf (fp, "    Flags ......... %ld\n", automap.location->flags);
		fprintf (fp, "    Regen Index ... %d\n", automap.location->regen);
		fprintf (fp, "    Session ....... %ld\n\n", automap.location->session);
		g_free (buf);

		automap_report_exit_list (fp);
	}
	else
		fprintf (fp, "  Automap Location: none\n\n");

	fprintf (fp, "  Automap Lost Index ...... %d\n", automap.lost);

	fprintf (fp, "  Automap Session ......... %ld\n", automap.session);

	fprintf (fp, "  Room Database Size ...... %d\n",
		g_hash_table_size (automap.db));

	fprintf (fp, "  Movement Queue Size ..... %d\n",
		g_slist_length (automap.movement));

	fprintf (fp, "  Current Room Name ....... %s\n",
		automap.room_name->len ? automap.room_name->str : "none");

	fprintf (fp, "  Last Key Used ........... %s\n",
		automap.key->len ? automap.key->str : "none");

	fprintf (fp, "  Last User Input ......... %s\n",
		automap.user_input->len ? automap.user_input->str : "none");

	fprintf (fp, "\n  Obvious Exits ........... ");

	if (automap.obvious.exits)
	{
		for (et = exit_table; et->short_str; et++)
			if (automap.obvious.exits & et->direction)
				fprintf (fp, "%s ", et->short_str);
	}
	else
		fprintf (fp, "none");

	fprintf (fp, "\n  Obvious Secrets ......... ");

	if (automap.obvious.secrets)
	{
		for (et = exit_table; et->short_str; et++)
			if (automap.obvious.secrets & et->direction)
				fprintf (fp, "%s ", et->short_str);
	}
	else
		fprintf (fp, "none");

	fprintf (fp, "\n  Obvious Doors Open ...... ");

	if (automap.obvious.doors_open)
	{
		for (et = exit_table; et->short_str; et++)
			if (automap.obvious.doors_open & et->direction)
				fprintf (fp, "%s ", et->short_str);
	}
	else
		fprintf (fp, "none");

	fprintf (fp, "\n  Obvious Doors Closed .... ");

	if (automap.obvious.doors_closed)
	{
		for (et = exit_table; et->short_str; et++)
			if (automap.obvious.doors_closed & et->direction)
				fprintf (fp, "%s ", et->short_str);
	}
	else
		fprintf (fp, "none");

	fprintf (fp, "\n  Obvious Doors Unlocked .. ");

	if (automap.obvious.doors_unlocked)
	{
		for (et = exit_table; et->short_str; et++)
			if (automap.obvious.doors_unlocked & et->direction)
				fprintf (fp, "%s ", et->short_str);
	}
	else
		fprintf (fp, "none");

	fprintf (fp, "\n\n");
}


/* =========================================================================
 = AUTOMAP_REPORT_EXIT_LIST
 =
 = Report exit list for the current location to the specified file
 ======================================================================== */

static void automap_report_exit_list (FILE *fp)
{
	GSList *node;
	automap_record_t *record;
	exit_info_t *exit_info;
	gint count = 1;

	g_assert (automap.location != NULL);

	fprintf (fp, "    Exit List:\n\n");

	if (!automap.location->exit_list)
	{
		fprintf (fp, "      None\n\n");
		return;
	}

	for (node = automap.location->exit_list; node; node = node->next)
	{
		exit_info = node->data;

		if ((record = automap_db_lookup (exit_info->id)) != NULL)
		{
			gchar *buf = NULL;

			if (record->visited.tv_sec)
			{
				buf = g_strdup_printf ("%s", ctime (&record->visited.tv_sec));
				strchomp (buf);
			}

			fprintf (fp, "      [%2d] ID: %s (%s)\n", count++,
				record->id, record->name);
			fprintf (fp, "           Last Visited: %s\n", buf ? buf : "N/A");
			fprintf (fp, "           Coordinates: (X: %ld) (Y: %ld) (Z: %ld)\n",
				record->x, record->y, record->z);

			g_free (buf);
		}
		else
			fprintf (fp, "      [%2d] ID: %s\n", count++, exit_info->id);

		if (exit_info->str && exit_info->str[0] != '\0')
			fprintf (fp, "           String: '%s'\n", exit_info->str);

		fprintf (fp, "           Direction: %s\n",
			exit_info->direction == EXIT_SPECIAL ?
			"Special" : DIRECTION_STR (exit_info->direction));

		fprintf (fp, "           Flags: %ld\n\n", exit_info->flags);
	}
}


/* =========================================================================
 = AUTOMAP_ENABLE
 =
 = Enable the automapper
 ======================================================================== */

void automap_enable (void)
{
	if (automap.enabled)
		return;

	if (g_hash_table_size (automap.db) == 0)
	{
		/* database is empty, zero everything */
		automap.x       = 0;
		automap.y       = 0;
		automap.z       = 0;
		automap.session = 0;
		automap.lost    = FALSE;
	}

	if (automap.lost)
	{
		printt ("Automap: Cannot activate while lost!");
		return;
	}

	automap.enabled = TRUE;
	automap.session++;

	if (mapview.visible)
	{
		mapview_update ();
		update_display ();
	}
}


/* =========================================================================
 = AUTOMAP_DISABLE
 =
 = Disable the automapper
 ======================================================================== */

void automap_disable (void)
{
	automap.enabled = FALSE;

	if (mapview.visible)
	{
		mapview_update ();
		update_display ();
	}
}


/* =========================================================================
 = AUTOMAP_DB_LOAD
 =
 = (Re)load automap database
 ======================================================================== */

void automap_db_load (void)
{
	gchar buf[STD_STRBUF], *offset;
	automap_record_t *record = NULL;
	FILE *fp;

	if (automap.db != NULL)
		automap_db_free ();

	automap.db = g_hash_table_new (g_str_hash, g_str_equal);
	g_get_current_time (&mudpro_db.automap.access);

	if ((fp = fopen (mudpro_db.automap.filename, "r")) == NULL)
	{
		/* no db, start mapping ASAP */
		automap_enable ();
		return;
	}

	g_hash_table_freeze (automap.db);

	while (fgets (buf, sizeof (buf), fp))
	{
		strchomp (buf);

		if (buf[0] == '#' || buf[0] == '\0')
		{
			record = NULL;
			continue;
		}

		if (isspace (buf[0]))
		{
			if (record != NULL)
				automap_parse_exit_info (record, buf);
			continue;
		}

		record = g_malloc0 (sizeof (automap_record_t));

		offset = buf;
		record->id      = get_token_as_str (&offset);
		record->name    = get_token_as_str (&offset);
		record->exits   = get_token_as_long (&offset);
		record->flags   = get_token_as_long (&offset);
		record->x       = get_token_as_long (&offset);
		record->y       = get_token_as_long (&offset);
		record->z       = get_token_as_long (&offset);
		record->session = get_token_as_long (&offset);

		automap.session = MAX (automap.session, record->session);

		if (record->flags & ROOM_FLAG_REGEN)
			record->regen = REGEN_RECHARGE;

		g_hash_table_insert (automap.db, record->id, record);
	}

	g_hash_table_thaw (automap.db);

	if (g_hash_table_size (automap.db) == 0)
		automap_enable (); /* no rooms defined, start mapping ASAP */

	fclose (fp);
}


/* =========================================================================
 = AUTOMAP_PARSE_EXIT_INFO
 =
 = Reads exit info data and appends it to the automap record
 ======================================================================== */

static void automap_parse_exit_info (automap_record_t *record, gchar *str)
{
	exit_info_t *exit_info;
	gchar *offset;

	g_assert (record != NULL);
	g_assert (str != NULL);

	exit_info = g_malloc0 (sizeof (exit_info_t));

	offset = str;
	exit_info->id        = get_token_as_str (&offset);
	exit_info->str       = get_token_as_str (&offset);
	exit_info->required  = get_token_as_str (&offset);
	exit_info->direction = get_token_as_long (&offset);
	exit_info->flags     = get_token_as_long (&offset);

	record->exit_list = g_slist_append (record->exit_list, exit_info);
}


/* =========================================================================
 = AUTOMAP_DB_LOOKUP
 =
 = Lookup room ID within automap database
 ======================================================================== */

automap_record_t *automap_db_lookup (gchar *id)
{
	return g_hash_table_lookup (automap.db, id);
}


/* =========================================================================
 = AUTOMAP_DB_ADD_LOCATION
 =
 = Adds the current location to the database
 ======================================================================== */

automap_record_t *automap_db_add_location (void)
{
	automap_record_t *record, *tmp;
	exit_table_t *et;
	gchar *buf;

	record = g_malloc0 (sizeof (automap_record_t));

	do /* get unique room ID */
	{
		buf = g_strdup_printf ("%d", rand ());

		if ((tmp = automap_db_lookup (buf)) != NULL)
		{
			g_free (buf);
			buf = NULL;
		}
	} while (buf == NULL);

	record->id      = buf;
	record->name    = g_strdup (automap.room_name->str);
	record->x       = automap.x;
	record->y       = automap.y;
	record->z       = automap.z;
	record->session = automap.session;

	for (et = exit_table; et->long_str; et++)
	{
		if (VISIBLE_EXITS & et->direction)
			automap_add_exit_info (record, et->direction);

		else if (automap.obvious.secrets & et->direction)
		{
			exit_info_t *exit_info = automap_add_exit_info (
				record, et->direction);
			if (exit_info)
				FlagON (exit_info->flags, EXIT_FLAG_SECRET);
		}
	}

	g_hash_table_insert (automap.db, record->id, record);

	return record;
}


/* =========================================================================
 = AUTOMAP_DB_SAVE
 =
 = Write the automap database out to file
 ======================================================================== */

void automap_db_save (void)
{
	FILE *fp;

	if ((fp = fopen (mudpro_db.automap.filename, "w")) == NULL)
	{
		printt ("Unable to open automap db for writing");
		return;
	}

	fprintf (fp, "# AUTOMAP.DB\n"
				 "#\n"
				 "# This file contains the rooms recorded by the automapper\n"
				 "# and should not normally be modified by hand\n\n");

	if (automap.db)
		g_hash_table_foreach (automap.db, automap_record_save, fp);

	fclose (fp);

	g_get_current_time (&mudpro_db.automap.access);
}


/* =========================================================================
 = AUTOMAP_RECORD_SAVE
 =
 = Saves the automap record to file
 ======================================================================== */

static void automap_record_save (gpointer key, gpointer value,
	gpointer user_data)
{
	automap_record_t *record = value;
	exit_info_t *exit_info;
	GSList *node;
	FILE *fp = user_data;

	g_assert (record != NULL);
	g_assert (fp != NULL);

	fprintf (fp, "%s, \"%s\", %ld, %ld, %ld, %ld, %ld, %ld\n",
		record->id,
		record->name,
		record->exits,
		record->flags,
		record->x, record->y, record->z,
		record->session);

	for (node = record->exit_list; node; node = node->next)
	{
		exit_info = node->data;
		FlagOFF (exit_info->flags, EXIT_FLAG_BLOCKED);

 		fprintf (fp, "\t%s, \"%s\", \"%s\", %ld, %ld\n", exit_info->id,
			(exit_info->str) ? exit_info->str : "",
			(exit_info->required) ? exit_info->required : "",
			exit_info->direction, exit_info->flags);
	}
	fprintf (fp, "\n");
}


/* =========================================================================
 = AUTOMAP_DB_FREE
 =
 = Free memory allocated to automap database
 ======================================================================== */

void automap_db_free (void)
{
	g_hash_table_foreach (automap.db, automap_record_deallocate,
		GINT_TO_POINTER (1) /* free key/value */);
	g_hash_table_destroy (automap.db);
	automap.db = NULL;
}


/* =========================================================================
 = AUTOMAP_DB_RESET
 =
 = Resets the current automap database and starts anew
 ======================================================================== */

void automap_db_reset (void)
{
	automap_reset (TRUE /* full reset */);

	automap_db_free (); /* clear current database */
	automap_db_save ();
	guidebook_db_reset ();

	automap_db_load (); /* start new database */
	command_send (CMD_TARGET);
}


/* =========================================================================
 = AUTOMAP_RECORD_DEALLOCATE
 =
 = Free memory allocated to automap record
 ======================================================================== */

static void automap_record_deallocate (gpointer key, gpointer value,
	gpointer user_data)
{
	automap_record_t *record = value;
	exit_info_t *exit_info;
	GSList *node;

	g_assert (key != NULL);
	g_assert (record != NULL);

	for (node = record->exit_list; node; node = node->next)
	{
		exit_info = node->data;
		g_free (exit_info->id);
		g_free (exit_info->str);
		g_free (exit_info->required);
		g_free (exit_info);
	}
	g_slist_free (record->exit_list);
	g_free (record->name);

	if (GPOINTER_TO_INT (user_data))
	{
		g_free (key); /* record->id */
		g_free (value);
	}
}


/* =========================================================================
 = AUTOMAP_PARSE_EXITS
 =
 = Parse and classify visible exits
 ======================================================================== */

void automap_parse_exits (gchar *str)
{
	gulong previous_exits = VISIBLE_EXITS;
	gchar *pos1, *pos2, *tmp;
	exit_table_t *et;
	gint exit_type;

	g_assert (str != NULL);

	memset (&automap.obvious, 0, sizeof (automap.obvious));

	pos1 = pos2 = str + strlen ("Obvious exits:");

	while (*pos1 != '\0')
	{
		tmp = get_token_as_str (&pos1);
		exit_type = automap_get_exit_type (tmp, &pos2);

		/* tmp  = full exit string (eg: 'west' or 'open gate north') */
		/* pos1 = updated position in the initial obvious exits string */
		/* pos2 = index to the exit in string (eg: open gate ^east) */

		for (et = exit_table; et->long_str; et++)
		{
			if (!strcasecmp (pos2, et->long_str))
			{
				automap_set_obvious_exit (exit_type, et->direction);
				break;
			}
		}
		g_free (tmp);
	}

	if (automap.movement || (VISIBLE_EXITS != previous_exits))
		automap_movement_update (); /* we have moved */
}


/* =========================================================================
 = AUTOMAP_PARSE_USER_INPUT
 =
 = Parse user input for movement
 ======================================================================== */

void automap_parse_user_input (gchar *str)
{
	exit_table_t *et;

	g_assert (str != NULL);

	/* keep a copy of user input for use with special exits */
	automap.user_input = g_string_assign (automap.user_input, str);

	if (!strncasecmp (str, "sys goto", 8) && automap.enabled)
	{
		item_visible_list_free ();
		automap_reset (FALSE /* full reset */);
		return;
	}

	for (et = exit_table; et->long_str; et++)
	{
		if ((et->short_str && !strcmp (str, et->short_str)) ||
			(et->long_str  && !strcmp (str, et->long_str)))
		{
			/* OK, user has moved. assume these are false until confirmed */
			character.flag.sneaking = FALSE;
			character.flag.avoid    = FALSE;
			navigation.pc_present   = FALSE;
			navigation.npc_present  = FALSE;

			item_visible_list_free ();
			automap_movement_add (et);
			return;
		}
	}
}


/* =========================================================================
 = AUTOMAP_MOVEMENT_ADD
 =
 = Add movement to queue
 ======================================================================== */

void automap_movement_add (exit_table_t *et)
{
	g_assert (et != NULL);
	automap.movement = g_slist_append (automap.movement, et);
}


/* =========================================================================
 = AUTOMAP_MOVEMENT_DEL
 =
 = Remove movement from the queue
 ======================================================================== */

void automap_movement_del (void)
{
	exit_table_t *et;

	/* normal movements are pointers to the exit_table */
	if ((et = g_slist_nth_data (automap.movement, 0)) != NULL)
		automap.movement = g_slist_remove (automap.movement, et);

	/* special movements are dynamically allocated and must be free'd */
	if (et && et->direction == EXIT_SPECIAL)
	{
		g_free (et->long_str);
		g_free (et);
	}
}


/* =========================================================================
 = AUTOMAP_MOVEMENT_GET_NEXT
 =
 = Returns the next movement in the queue
 ======================================================================== */

exit_table_t *automap_movement_get_next (void)
{
	return g_slist_nth_data (automap.movement, 0);
}


/* =========================================================================
 = AUTOMAP_MOVEMENT_LIST_FREE
 =
 = Free memory allocated to automap movement list
 ======================================================================== */

static void automap_movement_list_free (void)
{
	GSList *node;
	exit_table_t *et;

	for (node = automap.movement; node; node = node->next)
	{
		et = node->data;

		if (et && et->direction == EXIT_SPECIAL)
		{
			g_free (et->long_str);
			g_free (et);
		}
	}
	g_slist_free (automap.movement);
	automap.movement = NULL;
}


/* =========================================================================
 = AUTOMAP_MOVEMENT_UPDATE
 =
 = Update automap movement tracking
 ======================================================================== */

void automap_movement_update (void)
{
	exit_table_t *et;

	/* update XYZ if known */
	if ((et = automap_movement_get_next ()) != NULL)
	{
		automap.x += et->x;
		automap.y += et->y;
		automap.z += et->z;
	}

	/* update the rest */

	if (character.state == STATE_ENGAGED)
	{
		monster_target_list_free ();
		combat.force_break = TRUE;
		combat_reset ();
	}

	if (character.state == STATE_RESTING)
		character.state = STATE_NONE;

	character.flag.no_sneak = FALSE;

	automap_update_location ();
	automap_movement_del ();
	client_ai_movement_reset ();
	client_ai_open_door_reset ();

	if (navigation.route && !automap.lost)
		navigation_route_step ();
}


/* =========================================================================
 = AUTOMAP_ADD_EXIT_INFO
 =
 = Adds exit info to location
 ======================================================================== */

exit_info_t *automap_add_exit_info (automap_record_t *record, gint direction)
{
	exit_info_t *exit_info;

	g_assert (record != NULL);
	g_assert (direction > 0);

	if (direction == EXIT_NONE)
		return NULL; /* nothing to add */

	if (automap_get_exit_info (record, direction))
		return NULL; /* exit already defined */

	exit_info = g_malloc0 (sizeof (exit_info_t));

	exit_info->id        = g_strdup ("0");
	exit_info->str       = NULL;
	exit_info->required  = NULL;
	exit_info->direction = direction;

    if (VISIBLE_EXITS & direction)
        FlagON (record->exits, direction);

	if (VISIBLE_DOORS & direction)
		FlagON (exit_info->flags, EXIT_FLAG_DOOR);

	record->exit_list = g_slist_append (record->exit_list, exit_info);

	return exit_info;
}


/* =========================================================================
 = AUTOMAP_GET_EXIT_INFO
 =
 = Returns exit info for the given direction at location
 ======================================================================== */

exit_info_t *automap_get_exit_info (automap_record_t *record, gint direction)
{
	GSList *node;
	exit_info_t *exit_info;

	if (!record)
		return NULL;

	for (node = record->exit_list; node; node = node->next)
	{
		exit_info = node->data;
		if (exit_info->direction == direction)
			return exit_info;
	}
	return NULL;
}


/* =========================================================================
 = AUTOMAP_FIND_LOCATION
 =
 = Attempt to find current location within automap database
 ======================================================================== */

static automap_record_t *automap_find_location (void)
{
	automap_record_t *location = NULL;
	g_hash_table_foreach (automap.db, automap_location_search, &location);
	return location;
}


/* =========================================================================
 = AUTOMAP_LOCATION_SEARCH
 =
 = Performs search for automap_find_location, do not call directly
 ======================================================================== */

static void automap_location_search (gpointer key, gpointer value,
	gpointer user_data)
{
	automap_record_t *record = value, **location = user_data;

	g_assert (record != NULL);
	g_assert (location != NULL);

	if (automap.lost) /* if lost, allow match without XYZ */
	{
		if (!strcmp (record->name, automap.room_name->str)
			&& record->exits == VISIBLE_EXITS)
			*location = record;
	}

	/* otherwise require a complete match */

	if (!strcmp (record->name, automap.room_name->str)
		&& record->exits == VISIBLE_EXITS
		&& record->x == automap.x
		&& record->y == automap.y
		&& record->z == automap.z)
		*location = record;
}


/* =========================================================================
 = AUTOMAP_DUPLICATE_SEARCH
 =
 = Performs search for automap_merge_duplicates, do not call directly
 ======================================================================== */

static void automap_duplicate_search (gpointer key, gpointer value,
	gpointer user_data)
{
	automap_record_t *record = value, **location = user_data;

	g_assert (record != NULL);
	g_assert (location != NULL);

	if (!strcmp (record->name, automap.room_name->str)
		&& strcmp (record->id, automap.location->id)
		&& record->exits == VISIBLE_EXITS
		&& record->x == automap.x
		&& record->y == automap.y
		&& record->z == automap.z)
		*location = record;
}


/* =========================================================================
 = AUTOMAP_LOCATION_DEREFERENCE
 =
 = Remove any reference to location
 ======================================================================== */

static void automap_location_dereference (gpointer key, gpointer value,
	gpointer user_data)
{
	automap_record_t *record = value;
	exit_info_t *exit_info;
	gchar *id = user_data;
	GSList *node;

	g_assert (record != NULL);
	g_assert (id != NULL);

	for (node = record->exit_list; node; node = node->next)
	{
		exit_info = node->data;

		if (strcmp (exit_info->id, id))
			continue;

		g_free (exit_info->id);
		exit_info->id = g_strdup ("0");
	}
}


/* =========================================================================
 = AUTOMAP_LOCATION_GET_NEXT
 =
 = Returns the next location for a given direction
 ======================================================================== */

static automap_record_t *automap_location_get_next (automap_record_t *record,
	exit_table_t *et)
{
	gchar *id;

	if (record == NULL)
		return NULL;

	if (et->direction == EXIT_SPECIAL)
	{
		GSList *node;
		exit_info_t *exit_info;

		for (node = record->exit_list; node; node = node->next)
		{
			exit_info = node->data;
			if (exit_info->str &&
				!strcmp (exit_info->str, et->long_str))
				return automap_db_lookup (exit_info->id);
		}

		return NULL;
	}

	if ((id = automap_get_exit_id (record, et->direction)) != NULL)
	{
		if (id[0] != '\0') /* if connected to room, look it up */
			return automap_db_lookup (id);
	}

	return NULL;
}


/* =========================================================================
 = AUTOMAP_LOCATION_MERGE
 =
 = Perform location merging
 ======================================================================== */

void automap_location_merge (automap_record_t *original,
	automap_record_t *duplicate)
{
	GSList *node1, *node2;
	automap_record_t *record;

	g_assert (original != NULL);
	g_assert (duplicate != NULL);

	/* scan exits defined for the original location */
	for (node1 = original->exit_list; node1; node1 = node1->next)
	{
		exit_info_t *exit_info = node1->data;

		if ((record = automap_db_lookup (exit_info->id)) == NULL)
			continue;

		/* replace all references to duplicate record with original */
		for (node2 = record->exit_list; node2; node2 = node2->next)
		{
			exit_info_t *exit_info = node2->data;

			if (strcmp (exit_info->id, duplicate->id))
				continue;

			g_free (exit_info->id);
			exit_info->id = g_strdup (original->id);
		}
	}

	/* remove duplicate from database */
	g_hash_table_remove (automap.db, duplicate->id);
	automap_record_deallocate (duplicate->id, duplicate,
		GINT_TO_POINTER (1));

	/* update automap location */
	automap_set_location (original);
}


/* =========================================================================
 = AUTOMAP_DUPLICATE_MERGE
 =
 = Merges together duplicate automap database entries
 ======================================================================== */

void automap_duplicate_merge (void)
{
	automap_record_t *original = NULL;

	if (!automap.location)
		return;

	g_hash_table_foreach (automap.db, automap_duplicate_search, &original);

	if (original)
		automap_location_merge (original, automap.location);
}


/* =========================================================================
 = AUTOMAP_GET_EXIT_ID
 =
 = Returns the exit ID for the given exit at location
 ======================================================================== */

static gchar *automap_get_exit_id (automap_record_t *record, gint direction)
{
	exit_info_t *exit_info;
	GSList *node;

	if (record == NULL)
		return NULL;

	g_assert (direction > 0);

	/* see if the ID is available */
	for (node = record->exit_list; node; node = node->next)
	{
		exit_info = node->data;

		if (exit_info->direction == direction)
			return exit_info->id;
	}

	return NULL;
}


/* =========================================================================
 = AUTOMAP_SET_EXIT_ID
 =
 = Sets the exit ID for the given direction
 ======================================================================== */

static void automap_set_exit_id (automap_record_t *record, gchar *id,
	gint direction, gchar *exit_str)
{
	exit_info_t *exit_info = NULL;
	GSList *node;

	if (direction == 0)
		return;

	g_assert (record != NULL);
	g_assert (id != NULL);

	if (direction == EXIT_SPECIAL)
	{
		g_assert (exit_str != NULL);
		if ((exit_info = automap_add_exit_info (record, direction)) == NULL)
		{
			printt ("SET_EXIT_ID: Failed to set exit '%s' for %s", exit_str, id);
			return;
		}
		g_free (exit_info->id);
		exit_info->id  = g_strdup (id);
		exit_info->str = g_strdup (exit_str);
		FlagON (exit_info->flags, EXIT_FLAG_EXITSTR);
		return;
	}

	/* set ID for direction */
	for (node = record->exit_list; node; node = node->next)
	{
		exit_info = node->data;

		if (exit_info->direction != direction)
			continue;

		g_free (exit_info->id);
		exit_info->id = g_strdup (id);
		return;
	}
}


/* =========================================================================
 = AUTOMAP_SET_LOCATION
 =
 = Sets the current automap location
 ======================================================================== */

void automap_set_location (automap_record_t *location)
{
	/* update automap location data */

	g_assert (location != NULL);

	automap.location = location;
	automap.x = location->x;
	automap.y = location->y;
	automap.z = location->z;

	if (location->flags & ROOM_FLAG_SYNC)
		automap.lost = 0;

	if (!automap.lost)
	{
		g_get_current_time (&location->visited);
		location->regen = MAX (0, location->regen - 1);
		if (!location->regen && (location->flags & ROOM_FLAG_REGEN))
			FlagOFF (location->flags, ROOM_FLAG_REGEN);
	}

	if (mapview.visible)
		mapview_update ();
}


/* =========================================================================
 = AUTOMAP_JOIN_LOCATIONS
 =
 = Joins two locations together
 ======================================================================== */

static void automap_join_locations (automap_record_t *first,
	automap_record_t *second, exit_table_t *et)
{
	if (!first || !second)
		return;

	/* exchange room IDs */
	automap_set_exit_id (first, second->id, et->direction, et->long_str);

	if (second->exits & et->opposite)
		automap_set_exit_id (second, first->id, et->opposite, NULL);

	/* do animation for visual confirmation */
	if (mapview.visible)
		mapview_animate_player (1 /* reps */);
}


/* =========================================================================
 = AUTOMAP_UPDATE_LOCATION
 =
 = Update the current position within the automap database
 ======================================================================== */

static void automap_update_location (void)
{
	automap_record_t *location = NULL;
	exit_table_t *et = NULL;

	/* attempt to navigate to the next location */

	if ((et = automap_movement_get_next ()) != NULL)
		location = automap_location_get_next (automap.location, et);

	if (location != NULL)
	{
		/* verify location matches the room we're in (unless blind) */
		if ((!strcmp (automap.room_name->str, location->name) &&
			(VISIBLE_EXITS == location->exits)) || CANNOT_SEE)
		{
			automap.lost = MAX (0, automap.lost - 1);
			automap_set_location (location);
			if ((automap.obvious.secrets & et->direction) && !CANNOT_SEE)
				automap_set_secret (automap.location, et->direction);
			return;
		}
		else /* hmm... looks like we're lost */
			automap_reset (FALSE /* full reset */);
	}

	/* unable to navigate successfully, attempt to orient self */

	if ((location = automap_find_location ()) != NULL)
	{
		/* just set our position and continue */
		if (automap.lost || !automap.enabled)
		{
			automap_set_location (location);
			return;
		}

		/* our bearings are correct, close the path */
		if ((location->x == automap.x) &&
			(location->y == automap.y) &&
			(location->z == automap.z) &&
			(et != NULL))
		{
			automap_join_locations (automap.location, location, et);
			automap_set_location (location);
			return;
		}
		else /* OK, now we're REALLY lost... */
			automap_reset (FALSE /* full reset */);
	}

	if (!automap.enabled || automap.lost)
	{
		automap_reset (TRUE /* full reset */);
		return;
	}

	/* add current location to database */

	location = automap_db_add_location ();

	if (automap.location != location && et)
		automap_join_locations (automap.location, location, et);

	automap_set_location (location);
}


/* =========================================================================
 = AUTOMAP_INSERT_LOCATION
 =
 = Manually add location to database
 ======================================================================== */

void automap_insert_location (void)
{
	if (automap.location)
		automap_remove_location ();

	automap_set_location (automap_db_add_location ());
	automap.lost = 0;

	if (mapview.visible)
		mapview_animate_player (1);
}


/* =========================================================================
 = AUTOMAP_REMOVE_LOCATION
 =
 = Removes the current location
 ======================================================================== */

void automap_remove_location (void)
{
	if (automap.lost || !automap.location)
		return; /* cannot remove location */

	g_hash_table_foreach (automap.db, automap_location_dereference,
		automap.location->id);

	automap_record_deallocate (automap.location->id, automap.location,
		GINT_TO_POINTER (0) /* do not free key/value yet */);
	g_hash_table_remove (automap.db, automap.location->id);
	automap.location = NULL;
	automap_disable ();
}


/* =========================================================================
 = AUTOMAP_INCREMENT_XYZ
 =
 = Adjust automap XYZ coordinates
 ======================================================================== */

void automap_adjust_xyz (gint xyz, gint adj)
{
	switch (xyz)
	{
	case 'x':
		if (adj == '+') automap.x++;
		else if (adj == '-') automap.x--;
		automap.location->x = automap.x;
		break;
	case 'y':
		if (adj == '+') automap.y++;
		else if (adj == '-') automap.y--;
		automap.location->y = automap.y;
		break;
	case 'z':
		if (adj == '+') automap.z++;
		else if (adj == '-') automap.z--;
		automap.location->z = automap.z;
		break;
	}
}


/* =========================================================================
 = AUTOMAP_RESET
 =
 = Reset automapper
 ======================================================================== */

void automap_reset (gboolean full_reset)
{
	automap.lost = SYNC_STEPS;
	automap_disable ();
	navigation_route_free ();

	if (full_reset)
	{
		memset (&automap.obvious, 0, sizeof (automap.obvious));
		automap_movement_list_free ();
		automap.location = NULL;
		automap.room_name  = g_string_assign (automap.room_name, "");
		automap.key        = g_string_assign (automap.key, "");
		automap.user_input = g_string_assign (automap.user_input, "");
		automap.x = 0;
		automap.y = 0;
		automap.z = 0;
	}
}


/* =========================================================================
 = AUTOMAP_GET_EXIT_TYPE
 =
 = Searches for exit type and returns as int with offset to exit
 ======================================================================== */

static gint automap_get_exit_type (gchar *str, gchar **offset)
{
	key_value_t *et;
	gint len;

	g_assert (str != NULL);
	g_assert (*offset != NULL);

	for (et = exit_type; et->key; et++)
	{
		len = strlen (et->key);

		if (!strncasecmp (str, et->key, len))
		{
			*offset = str + len;
			return et->value;
		}
	}
	*offset = str;

	return EXIT_NORMAL;
}


/* =========================================================================
 = AUTOMAP_GET_EXIT_AS_INT
 =
 = Returns the value of the given exit as int
 ======================================================================== */

gint automap_get_exit_as_int (gchar *str)
{
	exit_table_t *et;

	for (et = exit_table; et->long_str; et++)
	{
		if (!strcasecmp (str, et->long_str))
			return et->direction;
	}

	return 0;
}

/* =========================================================================
 = AUTOMAP_SET_OBVIOUS_EXIT
 =
 = Set exit flags
 ======================================================================== */

static void automap_set_obvious_exit (gint type, gint flag)
{
	if (flag == EXIT_NONE)
		return;

	switch (type)
	{
	case EXIT_NORMAL:      FlagON (automap.obvious.exits, flag);        break;
	case EXIT_SECRET:      FlagON (automap.obvious.secrets, flag);      break;
	case EXIT_DOOR_OPEN:   FlagON (automap.obvious.doors_open, flag);   break;
	case EXIT_DOOR_CLOSED: FlagON (automap.obvious.doors_closed, flag); break;
	default:
		g_assert_not_reached ();
	}
}


/* =========================================================================
 = AUTOMAP_SET_SECRET
 =
 = Adds secret to location if not already defined
 ======================================================================== */

void automap_set_secret (automap_record_t *record, gint direction)
{
	exit_info_t *exit_info;

	g_assert (record != NULL);
	g_assert (direction > EXIT_NONE);
	g_assert (direction < EXIT_SPECIAL);

	if ((exit_info = automap_add_exit_info (record, direction)) == NULL)
		return; /* exit already defined */

	FlagON (exit_info->flags, EXIT_FLAG_SECRET);

	if (mapview.visible)
		mapview_update ();
}
