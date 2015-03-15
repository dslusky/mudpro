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
#include <string.h>

#include "about.h"
#include "automap.h"
#include "autoroam.h"
#include "character.h"
#include "defs.h"
#include "guidebook.h"
#include "keys.h"
#include "item.h"
#include "mapview.h"
#include "menubar.h"
#include "mudpro.h"
#include "navigation.h"
#include "osd.h"
#include "sock.h"
#include "terminal.h"
#include "utils.h"

#define MENU_ACCESS_RESTRICTED \
	(mapview_adjust_visible \
	|| mapview_flags_visible \
	|| autoroam.visible \
	|| guidebook.visible)

cwin_t menubar;

static cwin_t menu;
static gint selected_menu;
static gint selected_item;

static gboolean menubar_mudpro_access (gpointer data);
static gboolean menubar_automap_access (gpointer data);
static gboolean menubar_character_access (gpointer data);
static gboolean menubar_toggle_access (gpointer data);
static gboolean menubar_stats_access (gpointer data);
static gboolean menubar_navigation_access (gpointer data);
static gboolean menubar_window_access (gpointer data);

static menu_item_t mudpro_menu_items[]=
{
	{ "_Connect         CTRL-C", 'c', MENU_ITEM_NORMAL,
		menubar_mudpro_access,        NULL,
		mudpro_connect,	              NULL },

	{ "_Disconnect      CTRL-D", 'd', MENU_ITEM_NORMAL,
		menubar_mudpro_access,        NULL,
		mudpro_disconnect,            NULL },

	{ "Re_Load All Data", 'l',        MENU_ITEM_NORMAL,
		menubar_mudpro_access,        NULL,
		mudpro_load_data,             NULL },

	{ "_Save All Data", 's',          MENU_ITEM_NORMAL,
		menubar_mudpro_access,        NULL,
		mudpro_save_data,             NULL },

	{ "_Redraw Display  CTRL-L", 'r', MENU_ITEM_NORMAL,
		NULL,                         NULL,
		mudpro_redraw_display,        NULL },

	{ NULL, 0, MENU_ITEM_SEPARATOR, NULL, NULL, NULL, NULL },

	{ "_Quit            CTRL-Q", 'q', MENU_ITEM_NORMAL,
		menubar_mudpro_access,        NULL,
		mudpro_shutdown,              NULL },

	{ NULL, 0, 0, NULL, NULL, NULL, NULL }
};

static menu_item_t automap_menu_items[]=
{
	{ "_Automapping", 'a',          MENU_ITEM_TOGGLE,
		menubar_automap_access,     &automap.enabled,
		automap_enable,             automap_disable },

	{ "_Mapview OSD", 'm',          MENU_ITEM_TOGGLE,
		menubar_automap_access,     &character.option.osd_mapview,
		osd_dock_toggle_mapview,    osd_dock_toggle_mapview },

	{ "Edit Room _Flags", 'f',      MENU_ITEM_TOGGLE,
		menubar_automap_access,     &mapview_flags_visible,
		mapview_toggle_room_flags,  mapview_toggle_room_flags },

	{ "Adjust _Coordinates", 'c',   MENU_ITEM_TOGGLE,
		menubar_automap_access,     &mapview_adjust_visible,
		mapview_toggle_xyz_adjust,  mapview_toggle_xyz_adjust },

	{ "Coordinates _Display", 'd',  MENU_ITEM_TOGGLE,
		menubar_automap_access,     &mapview_xyz_visible,
		mapview_toggle_xyz_display, mapview_toggle_xyz_display },

#if 0 /* TODO: implement this */
	{ "Mapview Free-Mo_ve",   'v',  MENU_ITEM_TOGGLE,
		menubar_automap_access,     NULL,
		NULL,                       NULL },
#endif

	{ NULL, 0, MENU_ITEM_SEPARATOR, NULL, NULL, NULL, NULL },

	{ "Reset Database", 0,          MENU_ITEM_NORMAL,
		menubar_automap_access,     NULL,
		automap_db_reset,           NULL },

	{ "Add Room Manually", 0,       MENU_ITEM_NORMAL,
		menubar_automap_access,     NULL,
		automap_insert_location,    NULL },

	{ "Remove Current Room", 0,     MENU_ITEM_NORMAL,
		menubar_automap_access,     NULL,
		automap_remove_location,    NULL },

	{ NULL, 0, 0, NULL, NULL, NULL, NULL }
};

static menu_item_t character_menu_items[]=
{
	{ "_Deposit Cash", 'd',       MENU_ITEM_NORMAL,
		menubar_character_access, NULL,
		character_deposit_cash,   NULL },

	{ "Sys _Goto ...", 'g',       MENU_ITEM_NORMAL,
		menubar_character_access, NULL,
		character_sys_goto,       NULL },

	{ NULL, 0, MENU_ITEM_SEPARATOR, NULL, NULL, NULL, NULL },

	{ "Follow Attacks", 'f',      MENU_ITEM_TOGGLE,
		menubar_character_access, &character.option.follow_attack,
		NULL,                     NULL },

	{ "Item _Checking", 'c',      MENU_ITEM_TOGGLE,
		menubar_character_access, &character.option.item_check,
		NULL,                     NULL },

	{ "Reserve _Light", 'l',      MENU_ITEM_TOGGLE,
		menubar_character_access, &character.option.reserve_light,
		NULL,                     NULL },

	{ "_Vitals OSD", 'v',         MENU_ITEM_TOGGLE,
		menubar_window_access,    &character.option.osd_vitals,
		osd_dock_toggle_vitals,   osd_dock_toggle_vitals },

	{ NULL, 0, 0, NULL, NULL, NULL, NULL }
};

static menu_item_t toggle_menu_items[]=
{
	{ "Auto-_Response", 'r',       MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &character.option.auto_all,
		NULL,                      NULL },

	{ "Auto-_Attack", 'a',         MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &character.option.attack,
		NULL,                      NULL },

	{ "Auto-R_ecovery", 'e',       MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &character.option.recovery,
		NULL,                      NULL },

	{ "Auto-_Movement", 'm',       MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &character.option.movement,
		NULL,                      NULL },

	{ "Auto-Get _Items", 'i',      MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &character.option.get_items,
		NULL,                      NULL },

	{ "Auto-Get Cas_h", 'h',       MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &character.option.get_cash,
		NULL,                      NULL },

	{ "Auto-S_tash Items", 't',    MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &character.option.stash,
		NULL,                      NULL },

	{ "Auto-_Sneak", 's',          MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &character.option.sneak,
		NULL,                      NULL },

	{ "_Capture Log", 'c',         MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &capture_active,
		mudpro_capture_log_toggle, mudpro_capture_log_toggle },

	{ NULL, 0, MENU_ITEM_SEPARATOR, NULL, NULL, NULL, NULL },

	{ "Hangup on Low HP", 0,       MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &character.option.hangup,
		NULL,                      NULL },

	{ "Run on Low HP", 0,          MENU_ITEM_TOGGLE,
		menubar_toggle_access,     &character.option.run,
		NULL,                      NULL },

	{ NULL, 0, 0, NULL, NULL, NULL, NULL }
};

static menu_item_t stats_menu_items[]=
{
	{ "Statistics _OSD", 'o',       MENU_ITEM_TOGGLE,
		menubar_stats_access,       &character.option.osd_stats,
		osd_dock_toggle_stats,      osd_dock_toggle_stats },

	{ "Display By Conte_xt", 'x',   MENU_ITEM_TOGGLE,
		menubar_stats_access,       &character.option.osd_context,
		osd_stats_toggle_context,   osd_stats_toggle_context },

	{ "Reset _Current Stats", 'c',  MENU_ITEM_NORMAL,
		menubar_stats_access,       NULL,
		osd_stats_reset_current,    NULL },

	{ "Reset _All Stats", 'a',      MENU_ITEM_NORMAL,
		menubar_stats_access,       NULL,
		osd_stats_reset_all,        NULL },

	{ NULL, 0, MENU_ITEM_SEPARATOR, NULL, NULL, NULL, NULL },

	{ "Always Show _Exp", 'e',      MENU_ITEM_RADIO,
		menubar_stats_access,       &osd_stats_lock_exp,
		osd_stats_exp_set_lock,     NULL },

	{ "Always Show Com_bat", 'b',   MENU_ITEM_RADIO,
		menubar_stats_access,       &osd_stats_lock_combat,
		osd_stats_combat_set_lock,  NULL },

	{ "Always Show _Player", 'p',   MENU_ITEM_RADIO,
		menubar_stats_access,       &osd_stats_lock_player,
		osd_stats_player_set_lock,  NULL },

	{ "Always Show _Session", 's',  MENU_ITEM_RADIO,
		menubar_stats_access,       &osd_stats_lock_session,
		osd_stats_session_set_lock, NULL },

	{ NULL, 0, 0, NULL, NULL, NULL, NULL }
};

static menu_item_t navigation_menu_items[]=
{
	{ "_AutoRoam Dialog", 'a',        MENU_ITEM_NORMAL,
		menubar_window_access,        NULL,
		autoroam_map,                 NULL },

	{ "Open _GuideBook", 'g',         MENU_ITEM_NORMAL,
		menubar_window_access,        NULL,
		guidebook_map,                NULL },

	{ "Auto-Attack After _Route", 'r',     MENU_ITEM_TOGGLE,
		menubar_toggle_access,        &character.option.restore_attack,
		NULL,                         NULL },

	{ NULL, 0, MENU_ITEM_SEPARATOR, NULL, NULL, NULL, NULL },

	{ "Clear _Current Route", 'c',    MENU_ITEM_NORMAL,
		menubar_navigation_access,    NULL,
		navigation_reset_route,       NULL },

	{ "Clear _Pending Route(s)", 'p', MENU_ITEM_NORMAL,
		menubar_navigation_access,    NULL,
		navigation_reset_all,         NULL },

	{ NULL, 0, 0, NULL, NULL, NULL, NULL }
};

static menu_item_t help_menu_items[]=
{
	{ "_About MudPRO ...", 'a',   MENU_ITEM_NORMAL,
		menubar_window_access,    NULL,
		about_map,                NULL },

	{ "Create _Report Log", 'r',  MENU_ITEM_NORMAL,
		NULL,                     NULL,
		mudpro_create_report_log, NULL },

	{ NULL, 0, 0, NULL, NULL, NULL, NULL }
};

static void menubar_menu_map (menu_item_t *menu_items, gint column);
static void menubar_menu_unmap (void);
static void menubar_menu_update (menu_item_t *menu_items);
static void menubar_menu_item_toggle (menu_item_t *item);
static void menubar_menu_item_draw_label (menu_item_t *item, gboolean selected);


/* =========================================================================
 = MENUBAR_INIT
 =
 = Intitialize the menubar
 ======================================================================== */

void menubar_init (void)
{
	/* initialize menubar */
	memset (&menubar, 0, sizeof (cwin_t));
	memset (&menu, 0, sizeof (cwin_t));
	selected_menu = MENU_NONE;

	menubar.w = newwin (1, 0, 0, 0);
	menubar.p = new_panel (menubar.w);
	menubar.visible = TRUE;
	menubar.attr = ATTR_WINDOW | A_BOLD;

	leaveok (menubar.w, TRUE);
	wbkgdset (menubar.w, ATTR_WINDOW);
	wclear (menubar.w);

	menubar_update ();
}


/* =========================================================================
 = MENUBAR_CLEANUP
 =
 = Cleanup after menubar
 ======================================================================== */

void menubar_cleanup (void)
{
	/* free menubar */
	del_panel (menubar.p);
	delwin (menubar.w);

	if (menu.visible)
		menubar_menu_unmap ();
}


/* =========================================================================
 = MENUBAR_UPDATE
 =
 = Update the menubar
 ======================================================================== */

void menubar_update (void)
{
	werase (menubar.w);
	wattrset (menubar.w, menubar.attr);

	mvwaddstr (menubar.w, 0, MENU_COLUMN_MUDPRO,     "MudPRO");
	mvwaddstr (menubar.w, 0, MENU_COLUMN_AUTOMAP,    "Automap");
	mvwaddstr (menubar.w, 0, MENU_COLUMN_CHARACTER,  "Character");
	mvwaddstr (menubar.w, 0, MENU_COLUMN_TOGGLE,     "Toggles");
	mvwaddstr (menubar.w, 0, MENU_COLUMN_STATS,      "Statistics");
	mvwaddstr (menubar.w, 0, MENU_COLUMN_NAVIGATION, "Navigation");
	mvwaddstr (menubar.w, 0, MENU_COLUMN_HELP,       "Help");

	if (selected_menu)
		wattrset (menubar.w, ATTR_MENU);

	switch (selected_menu)
	{
	case MENU_MUDPRO:
		mvwaddstr (menubar.w, 0, MENU_COLUMN_MUDPRO-1, " MudPRO ");
		menubar_menu_map (&mudpro_menu_items[0], MENU_COLUMN_MUDPRO);
		break;
	case MENU_AUTOMAP:
		mvwaddstr (menubar.w, 0, MENU_COLUMN_AUTOMAP-1, " Automap ");
		menubar_menu_map (&automap_menu_items[0], MENU_COLUMN_AUTOMAP);
		break;
	case MENU_CHARACTER:
		mvwaddstr (menubar.w, 0, MENU_COLUMN_CHARACTER-1, " Character ");
		menubar_menu_map (&character_menu_items[0], MENU_COLUMN_CHARACTER);
		break;
	case MENU_TOGGLE:
		mvwaddstr (menubar.w, 0, MENU_COLUMN_TOGGLE-1, " Toggles ");
		menubar_menu_map (&toggle_menu_items[0], MENU_COLUMN_TOGGLE);
		break;
	case MENU_STATS:
		mvwaddstr (menubar.w, 0, MENU_COLUMN_STATS-1, " Statistics ");
		menubar_menu_map (&stats_menu_items[0], MENU_COLUMN_STATS);
		break;
	case MENU_NAVIGATION:
		mvwaddstr (menubar.w, 0, MENU_COLUMN_NAVIGATION-1, " Navigation ");
		menubar_menu_map (&navigation_menu_items[0], MENU_COLUMN_NAVIGATION);
		break;
	case MENU_HELP:
		mvwaddstr (menubar.w, 0, MENU_COLUMN_HELP-1, " Help ");
		menubar_menu_map (&help_menu_items[0], MENU_COLUMN_HELP);
		break;
	}
	update_display ();
}


/* =========================================================================
 = MENUBAR_RAISE
 =
 = Raise menbar and menu above all other panels
 ======================================================================== */

void menubar_raise (void)
{
	if (menubar.visible) top_panel (menubar.p);
	if (menu.visible)    top_panel (menu.p);
	update_display ();
}


/* =========================================================================
 = MENUBAR_MENU_MAP
 =
 = Draws menubar menu
 ======================================================================== */

static void menubar_menu_map (menu_item_t *menu_items, gint column)
{
	menu_item_t *item;
	gboolean toggles = FALSE;
	gchar *pos;
	gint width;

	if (menu.visible)
		menubar_menu_unmap ();

	selected_item = 0;

	/* calculate width of longest menu item */
	for (item = menu_items; item->type; item++)
	{
		/* check for toggle menu item */
		if (item->type == MENU_ITEM_TOGGLE) toggles = TRUE;

		width = 0; /* get label width */
		for (pos = item->label; pos && *pos; pos++)
			if (*pos != '_') width++;

		menu.width = MAX (menu.width, width);
		menu.height++;
	}

	if (toggles) menu.width += 8; /* border and toggle area */
	else         menu.width += 4; /* border */

	/* impose minimum width */
	menu.width = MAX (menu.width, MENU_WIDTH_MIN);

	/* get/set menu geometry */
	menu.xpos = column-1;
	menu.ypos = 1;

	/* keep menu within screen */
	if ((menu.xpos + menu.width) > COLS)
		menu.xpos = COLS - menu.width;

	/* create the menu */
	menu.w = newwin (menu.height+2, menu.width, menu.ypos, menu.xpos);
	menu.p = new_panel (menu.w);
	menu.attr = ATTR_MENU;
	menu.data = menu_items;
	menu.visible = TRUE;

	wattrset (menu.w, menu.attr);
	wbkgdset (menu.w, ATTR_MENU);
	leaveok (menu.w, TRUE);

	/* draw menu items */
	menubar_menu_update (menu_items);
}


/* =========================================================================
 = MENUBAR_MENU_UNMAP
 =
 = Close/hide the menubar menu
 ======================================================================== */

static void menubar_menu_unmap (void)
{
	if (!menu.visible)
		return; /* already unmapped */

	del_panel (menu.p);
	delwin (menu.w);
	memset (&menu, 0, sizeof (cwin_t));
	update_display ();
}


/* =========================================================================
 = MENUBAR_MENU_UPDATE
 =
 = Update the menu
 ======================================================================== */

static void menubar_menu_update (menu_item_t *menu_items)
{
	menu_item_t *item;
	gboolean selected;
	gint ypos;

	g_assert (menu_items != NULL);

	werase (menu.w);
	wattrset (menu.w, menu.attr | A_BOLD);

	wborder (menu.w,
		menu_ui.vline, menu_ui.vline, menu_ui.hline, menu_ui.hline,
		menu_ui.nw,    menu_ui.ne,    menu_ui.sw,    menu_ui.se);

	ypos = 1;
	for (item = menu_items; item->type; item++) /* draw menu items */
	{
		/* draw separator */
		if (item->type == MENU_ITEM_SEPARATOR)
		{
			wmove (menu.w, ypos++, 1);
			wattrset (menu.w, ATTR_MENU | A_BOLD);
			whline (menu.w, widget_ui.hline, menu.width-2);
			continue;
		}

		/* mark selected row */
		if ((selected = (selected_item == ypos-1) ? TRUE : FALSE))
		{
			wattrset (menu.w, ATTR_MENU_S | A_BOLD);
			wmove (menu.w, ypos, 1);
			whline (menu.w, ' ', menu.width-2);
		}

		wmove (menu.w, ypos, 2);
		menubar_menu_item_draw_label (item, selected);

		if (item->type == MENU_ITEM_TOGGLE || item->type == MENU_ITEM_RADIO)
		{
			/* set required attributes */
			attr_t attr = selected ? ATTR_MENU_S | A_BOLD : ATTR_MENU;
			wattrset (menu.w, MENU_ITEM_ACCESS (item) ? attr : attr ^ A_BOLD);

			/* draw toggle button */
			if (item->state && *item->state)
				mvwprintw (menu.w, ypos, menu.width-5, "(*)");
			else
				mvwprintw (menu.w, ypos, menu.width-5, "( )");
		}
		ypos++;
	}
	update_display ();
}


/* =========================================================================
 = MENUBAR_MENU_ITEM_TOGGLE
 =
 = Callback to toggle menu item state
 ======================================================================== */

static void menubar_menu_item_toggle (menu_item_t *item)
{
	GString *tmp;
	gchar *pos;

	g_assert (item != NULL);

	/* use custom callbacks if defined */

	if (*item->state && item->deactivate)
	{
		(*item->deactivate) ();
		return;
	}
	else if (!*item->state && item->activate)
	{
		(*item->activate) ();
		return;
	}

	/* toggle state and display the result in terminal */
	*item->state = !*item->state;

	tmp = g_string_new ("");

	for (pos = item->label; *pos; pos++) /* remove accelerator from label */
		if (*pos != '_') tmp = g_string_append_c (tmp, *pos);

	printt ("%s: %s", tmp->str, *item->state ? "ON" : "OFF");
	g_string_free (tmp, TRUE);
}


/* =========================================================================
 = MENUBAR_MENU_ITEM_DRAW_LABEL
 =
 = Draws menu item label
 ======================================================================== */

static void menubar_menu_item_draw_label (menu_item_t *item, gboolean selected)
{
	gboolean access = MENU_ITEM_ACCESS (item);
	gchar *pos;
	attr_t attr;

	attr = selected ? ATTR_MENU_S | A_BOLD : ATTR_MENU;

	for (pos = item->label; pos && *pos; pos++)
	{
		if (*pos == '_') /* highlight accelerator */
		{
			if (access)
				attr = (selected ? ATTR_ACCEL_S : ATTR_ACCEL) | A_BOLD;
			continue;
		}

		wattrset (menu.w, access ? attr : attr ^ A_BOLD);
		waddch (menu.w, *pos);

		attr = selected ? ATTR_MENU_S | A_BOLD : ATTR_MENU;
	}
}


/* =========================================================================
 = MENUBAR_MUDPRO_ACCESS
 =
 = Determine if mudpro menu items are accessable
 ======================================================================== */

static gboolean menubar_mudpro_access (gpointer data)
{
	menu_item_t *item = data;

	if (MENU_ACCESS_RESTRICTED)
		return FALSE;

	if (sock.alive && item->activate == mudpro_connect)
		return FALSE;

	if (!sock.alive && item->activate == mudpro_disconnect)
		return FALSE;

	return TRUE;
}


/* =========================================================================
 = MENUBAR_AUTOMAP_ACCESS
 =
 = Determine if automap menu items are accessable
 ======================================================================== */

static gboolean menubar_automap_access (gpointer data)
{
	menu_item_t *item = data;

	if (!character.flag.ready)
		return FALSE;

	if (item->type == MENU_ITEM_NORMAL && !item->activate)
		return FALSE; /* no action defined */

	if (autoroam.visible || guidebook.visible)
		return FALSE;

	if (!mapview.visible)
	{
		if (item->state == &mapview_flags_visible ||
			item->state == &mapview_adjust_visible ||
			item->state == &mapview_xyz_visible)
			return FALSE;

		if (item->activate == automap_insert_location ||
			item->activate == automap_remove_location)
			return FALSE;
	}

	if (mapview_adjust_visible &&
		(item->state != &mapview_adjust_visible))
		return FALSE;

	if (mapview_flags_visible &&
		(item->state != &mapview_flags_visible))
		return FALSE;

	if ((autoroam_opts.enabled || navigation.route) &&
		(item->activate == automap_enable ||
		 item->activate == mapview_toggle_xyz_adjust ||
		 item->activate == automap_db_reset ||
		 item->activate == automap_insert_location ||
		 item->activate == automap_remove_location))
		 return FALSE;

	if (!automap.location || automap.lost)
	{
		if (item->state == &mapview.visible)
			return TRUE;

		if (item->activate == automap_db_reset)
			return TRUE;

		return FALSE;
	}

	return TRUE;
}


/* =========================================================================
 = MENUBAR_CHARACTER_ACCESS
 =
 = Determine if character menu items are accessable
 ======================================================================== */

static gboolean menubar_character_access (gpointer data)
{
	menu_item_t *item = data;

	if (MENU_ACCESS_RESTRICTED || !character.flag.ready)
		return FALSE;

	if (item->activate == character_deposit_cash)
	{
		gulong amount = item_inventory_get_cash_amount ();
		return CLAMP (amount, 0, 1);
	}

	if (item->activate == character_sys_goto
		&& !character.option.sys_goto)
		return FALSE;

	return TRUE;
}


/* =========================================================================
 = MENUBAR_TOGGLE_ACCESS
 =
 = Determine if toggle menu items are accessable
 ======================================================================== */

static gboolean menubar_toggle_access (gpointer data)
{
	if (MENU_ACCESS_RESTRICTED)
		return FALSE;

	return TRUE;
}


/* =========================================================================
 = MENUBAR_STATS_ACCESS
 =
 = Determine if stats menu item is accessable
 ======================================================================== */

static gboolean menubar_stats_access (gpointer data)
{
	menu_item_t *item = data;

	if (MENU_ACCESS_RESTRICTED || !character.flag.ready)
		return FALSE;

	if (!osd_stats.visible && item->state != &character.option.osd_stats)
		return FALSE;

	return TRUE;
}


/* =========================================================================
 = MENUBAR_NAVIGATION_ACCESS
 =
 = Determine if navigation menu items are accessable
 ======================================================================== */

static gboolean menubar_navigation_access (gpointer data)
{
	menu_item_t *item = data;

	if (MENU_ACCESS_RESTRICTED)
		return FALSE;

	if (automap.enabled)
		return FALSE;

	if (item->activate == navigation_reset_route && !navigation.route)
		return FALSE;

	if (item->activate == navigation_reset_all &&
		!navigation.route && !navigation.anchors)
		return FALSE;

	return TRUE;
}


/* =========================================================================
 = MENUBAR_WINDOW_ACCESS
 =
 = Determine if window menu items are accessable
 ======================================================================== */

static gboolean menubar_window_access (gpointer data)
{
	menu_item_t *item = data;

	if (MENU_ACCESS_RESTRICTED)
		return FALSE;

	if (!character.flag.ready
		|| automap.enabled)
	{
		/* exclude about window */
		if (item->activate == about_map)
			return TRUE;

		return FALSE;
	}

	return TRUE;
}


/* =========================================================================
 = MENUBAR_KEY_HANDLER
 =
 = Handle menubar user input
 ======================================================================== */

gboolean menubar_key_handler (gint ch)
{
	menu_item_t *item;

	if (menu.visible && menu.data) /* handle accelerators */
	{
		for (item = menu.data; item->type; item++)
		{
			if (tolower (ch) != item->accel || !MENU_ITEM_ACCESS (item))
				continue;

			if (item->activate &&
				(item->type == MENU_ITEM_NORMAL || item->type == MENU_ITEM_RADIO))
				(*item->activate) ();

			else if (item->type == MENU_ITEM_TOGGLE)
				menubar_menu_item_toggle (item);

			selected_menu = MENU_NONE;
			menubar_menu_unmap ();
			menubar_update ();
			return TRUE;
		}
	}

	switch (ch) /* handle normal key events */
	{
	case ' ': /* toggle selected checkbox */
		if (!menu.visible || !menu.data)
			return FALSE;

		item = MENU_ITEM (menu.data, selected_item);

		if (!MENU_ITEM_ACCESS (item))
			return TRUE; /* action not allowed */

		if (item->activate && item->type == MENU_ITEM_RADIO)
			(*item->activate) ();

		if (item->type == MENU_ITEM_TOGGLE)
			menubar_menu_item_toggle (item);

		menubar_menu_update (menu.data);

		return TRUE; /* skip menubar_update () */

	case KEY_CR: /* active selected item */
		if (!menu.visible || !menu.data)
			return FALSE;

		item = MENU_ITEM (menu.data, selected_item);

		if (!MENU_ITEM_ACCESS (item))
			return TRUE; /* action now allowed */

		if (item->activate &&
			(item->type == MENU_ITEM_NORMAL || item->type == MENU_ITEM_RADIO))
			(*item->activate) ();

		else if (item->type == MENU_ITEM_TOGGLE)
			menubar_menu_item_toggle (item);

		selected_menu = MENU_NONE;
		menubar_menu_unmap ();
		break;

	case KEY_ALT_A:
		if (menu.visible)
			menubar_menu_unmap ();

		selected_menu = (selected_menu == MENU_AUTOMAP)
			? MENU_NONE : MENU_AUTOMAP;
		break;

	case KEY_ALT_C:
		if (menu.visible)
			menubar_menu_unmap ();

		selected_menu = (selected_menu == MENU_CHARACTER)
			? MENU_NONE : MENU_CHARACTER;
		break;

	case KEY_ALT_N:
		if (menu.visible)
			menubar_menu_unmap ();

		selected_menu = (selected_menu == MENU_NAVIGATION)
			? MENU_NONE : MENU_NAVIGATION;
		break;

	case KEY_ALT_M:
		if (menu.visible)
			menubar_menu_unmap ();

		selected_menu = (selected_menu == MENU_MUDPRO)
			? MENU_NONE : MENU_MUDPRO;
		break;

	case KEY_ALT_S:
		if (menu.visible)
			menubar_menu_unmap ();

		selected_menu = (selected_menu == MENU_STATS)
			? MENU_NONE : MENU_STATS;
		break;

	case KEY_ALT_T:
		if (menu.visible)
			menubar_menu_unmap ();

		selected_menu = (selected_menu == MENU_TOGGLE)
			? MENU_NONE : MENU_TOGGLE;
		break;

	case KEY_ALT_H:
		if (menu.visible)
			menubar_menu_unmap ();

		selected_menu = (selected_menu == MENU_HELP)
			? MENU_NONE : MENU_HELP;
		break;

	case KEY_ALT_X:
		if (selected_menu)
		{
			selected_menu = MENU_NONE;
			menubar_menu_unmap ();
		}
		else return FALSE;
		break;

	case KEY_DOWN:
		if (!menu.visible && !menu.data)
			return FALSE;

		selected_item = (selected_item < menu.height-1) ? selected_item+1 : 0;

		item = MENU_ITEM (menu.data, selected_item);

		/* attempt to skip over separator */
		if (item->type == MENU_ITEM_SEPARATOR)
			return menubar_key_handler (KEY_DOWN);

		menubar_menu_update (menu.data);
		return TRUE; /* skip menubar_update () */

	case KEY_LEFT:
		if (!menu.visible)
			return FALSE;

		menubar_menu_unmap ();
		selected_menu = (selected_menu > MENU_MUDPRO) ? selected_menu-1 : MENU_HELP;
		break;

	case KEY_RIGHT:
		if (!menu.visible)
			return FALSE;

		menubar_menu_unmap ();
		selected_menu = (selected_menu < MENU_HELP) ? selected_menu+1 : MENU_MUDPRO;
		break;

	case KEY_UP:
		if (!menu.visible)
			return FALSE;

		selected_item = (selected_item > 0) ? selected_item-1 : menu.height-1;

		item = MENU_ITEM (menu.data, selected_item);

		/* attempt to skip over separator */
		if (item->type == MENU_ITEM_SEPARATOR)
			return menubar_key_handler (KEY_UP);

		menubar_menu_update (menu.data);
		return TRUE; /* skip menubar_update () */

	default:
		if (menu.visible) return TRUE;
		else              return FALSE;
	}
	menubar_update ();

	return TRUE;
}
