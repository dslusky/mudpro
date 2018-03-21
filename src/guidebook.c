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

#include "automap.h"
#include "autoroam.h"
#include "character.h"
#include "defs.h"
#include "guidebook.h"
#include "keys.h"
#include "navigation.h"
#include "mudpro.h"
#include "terminal.h"
#include "timers.h"
#include "utils.h"

cwin_t guidebook;

static GSList *guidebook_db;
static gint guidebook_db_size;
static listview_t *listview;
static action_area_t action_area;

key_value_t guidebook_actions[] = {
	{ "[  Add  ]",   ACTION_ADD },
	{ "[ Remove ]",  ACTION_REMOVE },
	{ "[ Dismiss ]", ACTION_DISMISS },
	{ NULL, 0 }
};

static void guidebook_db_free (void);
static gint guidebook_db_sort_name (gconstpointer a, gconstpointer b);
static void guidebook_listview_update (void);


/* =========================================================================
 = GUIDEBOOK_INIT
 =
 = Initialize the guidebook
 ======================================================================== */

void guidebook_init (void)
{
	memset (&guidebook, 0, sizeof (cwin_t));

	action_area.cwin = &guidebook;
	action_area.actions = guidebook_actions;

	guidebook.width = 40;
	guidebook.height = 16;

	guidebook.xpos = 2;
	guidebook.ypos = 2;

	guidebook.w = newwin (guidebook.height,
						  guidebook.width,
						  guidebook.ypos,
						  guidebook.xpos);

	guidebook.p = new_panel (guidebook.w);
	guidebook.visible = FALSE;
	hide_panel (guidebook.p);

	guidebook.attr = ATTR_WINDOW | A_BOLD;
	wbkgdset (guidebook.w, ATTR_WINDOW);

	leaveok (guidebook.w, TRUE);

	listview = listview_new (&guidebook,
		guidebook.width - 4, GUIDEBOOK_LIST_SIZE, 2, 2);

	mudpro_db.guidebook.filename = g_strdup_printf (
		"%s%cguidebook.db", character.data_path, G_DIR_SEPARATOR);

	guidebook_db = NULL;
	guidebook_db_size = 0;
	guidebook_db_load ();
}


/* =========================================================================
 = GUIDEBOOK_CLEANUP
 =
 = Cleanup after the guidebook
 ======================================================================== */

void guidebook_cleanup (void)
{
	del_panel (guidebook.p);
	delwin (guidebook.w);

	listview_free (listview);

	guidebook_db_save ();
	guidebook_db_free ();
	g_free (mudpro_db.guidebook.filename);
}


/* =========================================================================
 = GUIDEBOOK_DB_LOAD
 =
 = (Re)load the guidebook db from file
 ======================================================================== */

void guidebook_db_load (void)
{
	gchar buf[STD_STRBUF];
	gchar *offset;
	guidebook_record_t *record;
	FILE *fp;

	if ((fp = fopen (mudpro_db.guidebook.filename, "r")) == NULL)
	{
		printt ("Unable to open guidebook database");
		return;
	}

	g_get_current_time (&mudpro_db.guidebook.access);

	if (guidebook_db != NULL)
		guidebook_db_free ();

	while (fgets (buf, sizeof (buf), fp))
	{
		strchomp (buf);

		if (buf[0] == '#' || buf[0] == '\0')
			continue;

		record = g_malloc0 (sizeof (guidebook_record_t));

		offset = buf;
		record->name = get_token_as_str (&offset);
		record->id = get_token_as_str (&offset);

		guidebook_db = g_slist_insert_sorted (guidebook_db, record,
			guidebook_db_sort_name);
	}

	fclose (fp);
}


/* =========================================================================
 = GUIDEBOOK_DB_FREE
 =
 = Free data allocated to guidebook database
 ======================================================================== */

static void guidebook_db_free (void)
{
	GSList *node;
	guidebook_record_t *record;

	for (node = guidebook_db; node; node = node->next)
	{
		record = node->data;
		g_free (record->name);
		g_free (record->id);
		g_free (record);
	}

	g_slist_free (guidebook_db);
	guidebook_db      = NULL;
	guidebook_db_size = 0;
}


/* =========================================================================
 = GUIDEBOOK_DB_SAVE
 =
 = Saves guidebook to file
 ======================================================================== */

void guidebook_db_save (void)
{
	GSList *node;
	guidebook_record_t *record;
	FILE *fp;

	if ((fp = fopen (mudpro_db.guidebook.filename, "w")) == NULL)
	{
		printt ("Unable to open guidebook.db for writing");
		return;
	}

	fprintf (fp, "# GUIDEBOOK.DB\n"
				 "#\n"
				 "# This file contains locations recorded by the GuideBook"
				 "# and should not normally be modified manually\n\n");

	for (node = guidebook_db; node; node = node->next)
	{
		record = node->data;
		if (record->name && record->id)
			fprintf (fp, "\"%s\", \"%s\"\n", record->name, record->id);
	}

	fclose (fp);

	g_get_current_time (&mudpro_db.guidebook.access);
}


/* =========================================================================
 = GUIDEBOOK_DB_RESET
 =
 = Reset the guidebook database and start anew
 ======================================================================== */

void guidebook_db_reset (void)
{
	guidebook_db_free ();
	guidebook_db_save ();
}


/* =========================================================================
 = GUIDEBOOK_DB_ADD
 =
 = Adds location to the guidebook
 ======================================================================== */

void guidebook_db_add (gchar *str, gchar *id)
{
	guidebook_record_t *record;

	g_assert (str != NULL);
	g_assert (id != NULL);

	record = g_malloc0 (sizeof (guidebook_record_t));
	record->name = g_strdup (str);
	record->id   = g_strdup (id);

	guidebook_db_size++;
	guidebook_db = g_slist_insert_sorted (guidebook_db, record,
		guidebook_db_sort_name);
}


/* =========================================================================
 = GUIDEBOOK_DB_DEL
 =
 = Removes location from guidebook
 ======================================================================== */

void guidebook_db_del (guidebook_record_t *record)
{
	g_assert (record != NULL);

	g_free (record->name);
	g_free (record->id);

	guidebook_db_size = MIN (0, guidebook_db_size - 1);
	guidebook_db = g_slist_remove (guidebook_db, record);
	g_free (record);
}


/* =========================================================================
 = GUIDEBOOK_DB_SORT_NAME
 =
 = Sort guidebook records by name
 ======================================================================== */

static gint guidebook_db_sort_name (gconstpointer a, gconstpointer b)
{
	guidebook_record_t *record1, *record2;
	gchar *pos1, *pos2;

	g_assert (a != NULL);
	g_assert (b != NULL);

	record1 = (guidebook_record_t *) a;
	record2 = (guidebook_record_t *) b;
	pos1 = record1->name;
	pos2 = record2->name;

	while ((*pos1 && *pos2) && *pos1 == *pos2)
		{ pos1++; pos2++; }

	if (*pos1 < *pos2)
		return FALSE;

	return TRUE;
}


/* =========================================================================
 = GUIDEBOOK_MAP
 =
 = Maps the guidebook window
 ======================================================================== */

void guidebook_map (void)
{
	if (guidebook.visible)
		return;

	show_panel (guidebook.p);
	action_area.focus = 0;
	guidebook.visible = TRUE;
	guidebook_listview_update ();
	guidebook_update ();
}


/* =========================================================================
 = GUIDEBOOK_UNMAP
 =
 = Unmaps the guidebook window
 ======================================================================== */

void guidebook_unmap (void)
{
	if (!guidebook.visible)
		return;

	hide_panel (guidebook.p);
	guidebook.visible = FALSE;
	update_display ();
}


/* =========================================================================
 = GUIDEBOOK_UPDATE
 =
 = Update the guidebook window
 ======================================================================== */

void guidebook_update (void)
{
	werase (guidebook.w);
	wattrset (guidebook.w, ATTR_BORDER | A_BOLD);

	wborder (guidebook.w,
		window_ui.vline, window_ui.vline, window_ui.hline, window_ui.hline,
		window_ui.nw,    window_ui.ne,    window_ui.sw,    window_ui.se);

	border_draw_bracket (&guidebook, '[');
	wattrset (guidebook.w, ATTR_WHITE | A_BOLD);
	waddstr (guidebook.w, " Guide Book ");
	border_draw_bracket (&guidebook, ']');

	listview_draw (listview, (guidebook.width - 7),
		!CLAMP (action_area.focus, 0, 1));

	action_area_draw (&action_area);

	update_display ();
}


/* =========================================================================
 = GUIDEBOOK_LISTVIEW_UPDATE
 =
 = Update guidebook listview
 ======================================================================== */

static void guidebook_listview_update (void)
{
	guidebook_record_t *record;
	GSList *node;

	listview->offset   = 0;
	listview->position = 0;

	/* clear listview data */
	for (node = listview->data; node; node = node->next)
		g_free (node->data);
	g_slist_free (listview->data);
	listview->data = NULL;

	/* load current data */
	for (node = guidebook_db; node; node = node->next)
	{
		record = node->data;
		listview->data = g_slist_append
			(listview->data, g_strdup (record->name));
	}
}


/* =========================================================================
 = GUIDEBOOK_KEY_HANDLER
 =
 = Handle guidebook key strokes
 ======================================================================== */

gboolean guidebook_key_handler (gint ch)
{
	switch (ch)
	{
	case KEY_TAB:
		action_area.focus = action_area_get_next (&action_area,
			guidebook_actions);
		guidebook_update ();
		break;
	case KEY_CR:
		if (action_area.focus == ACTION_ADD)
		{
			automap_record_t *record = automap.location;
			if (record == NULL)
				return TRUE;

			guidebook_db_add (record->name, record->id);
			guidebook_db_save ();
			guidebook_listview_update ();
			guidebook_update ();
		}
		else if (action_area.focus == ACTION_REMOVE)
		{
			guidebook_record_t *record = g_slist_nth_data
				(guidebook_db, LISTVIEW_SELECTION (listview));

			if (record == NULL)
				return TRUE;

			guidebook_db_del (record);
			guidebook_db_save ();
			guidebook_listview_update ();
			guidebook_update ();
		}
		else if (action_area.focus == ACTION_DISMISS)
		{
			guidebook_unmap ();
		}
		else
		{
			guidebook_record_t *record = g_slist_nth_data
				(guidebook_db, LISTVIEW_SELECTION (listview));

			if (record == NULL)
				return TRUE;

			navigation_anchor_add (record->id, TRUE /* clear list */);
			navigation_create_route ();
			autoroam_opts.enabled = FALSE;
		}
		break;
	case KEY_ALT_G:
	case KEY_CTRL_X:
		guidebook_unmap ();
		break;
	case KEY_DOWN:
		if (action_area.focus > 0)
			return TRUE;
		listview_scroll (listview, 1);
		guidebook_update ();
		break;
	case KEY_UP:
		if (action_area.focus > 0)
			return TRUE;

		listview_scroll (listview, -1);
		guidebook_update ();
		break;
	}

	return TRUE;
}
