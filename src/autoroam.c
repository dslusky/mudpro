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

#include <string.h>

#include "automap.h"
#include "autoroam.h"
#include "client_ai.h"
#include "defs.h"
#include "keys.h"
#include "mudpro.h"
#include "navigation.h"
#include "terminal.h"
#include "utils.h"
#include "widgets.h"

#define CHECKBOX_WIDTH	24

enum /* autoroam dialog frames */
{
	FRAME_NONE,		/* used for selections */
	FRAME_OPTIONS,
	FRAME_DIRECTIONS
};

enum /* autoroam options */
{
	OPTION_ENABLE_ROAM,
	OPTION_ROAM_BLIND,
	OPTION_USE_DOORS,
	OPTION_USE_SECRETS,
	OPTION_USE_SPECIAL
};

static struct /* autoroam dialog selections */
{
	gint frame;
	gint option;
	gint direction;
} selected;

cwin_t autoroam;
autoroam_opts_t autoroam_opts;
static action_area_t action_area;

#define OPTION_TABLE_SIZE 5
key_value_t option_table[OPTION_TABLE_SIZE+1] = {
	{ "Enable Roaming",    OPTION_ENABLE_ROAM },
	{ "Roam While Blind",  OPTION_ROAM_BLIND },
	{ "Use Doors",         OPTION_USE_DOORS },
	{ "Use Secret Exits",  OPTION_USE_SECRETS },
	{ "Use Special Exits", OPTION_USE_SPECIAL },
	{ NULL, 0 }
};

#define DIRECTION_TABLE_SIZE 10
key_value_t direction_table[DIRECTION_TABLE_SIZE+1] = {
	{ "North",     EXIT_NORTH },     { "South",     EXIT_SOUTH },
	{ "East",      EXIT_EAST },      { "West",      EXIT_WEST },
	{ "Northeast", EXIT_NORTHEAST }, { "Northwest", EXIT_NORTHWEST },
	{ "Southeast", EXIT_SOUTHEAST }, { "Southwest", EXIT_SOUTHWEST },
	{ "Up",        EXIT_UP },        { "Down",      EXIT_DOWN },
	{ NULL, 0 }
};

key_value_t autoroam_actions[] = {
	{ "[ Dismiss ]", ACTION_DISMISS },
	{ NULL, 0 }
};


/* =========================================================================
 = AUTOROAM_INIT
 =
 = Initialize autoroam dialog
 ======================================================================== */

void autoroam_init (void)
{
	memset (&autoroam, 0, sizeof (cwin_t));

	action_area.cwin = &autoroam;
	action_area.actions = autoroam_actions;

	autoroam.width = CHECKBOX_WIDTH*2 + 4;
	autoroam.height = 3  /* options */
					+ 5  /* directions */
					+ 12 /* decorations */;

	autoroam.xpos = 2;
	autoroam.ypos = 2;

	autoroam.w = newwin (autoroam.height,
						 autoroam.width,
						 autoroam.ypos,
						 autoroam.xpos);

	autoroam.p = new_panel (autoroam.w);
	autoroam.visible = FALSE;
	hide_panel (autoroam.p);

	autoroam.attr = ATTR_WINDOW | A_BOLD;
	wbkgdset (autoroam.w, ATTR_WINDOW);
	leaveok (autoroam.w, TRUE);
}


/* =========================================================================
 = AUTOROAM_CLEANUP
 =
 = Clean up autoroam data
 ======================================================================== */

void autoroam_cleanup (void)
{
	del_panel (autoroam.p);
	delwin (autoroam.w);
}


/* =========================================================================
 = AUTOROAM_REPORT
 =
 = Report current status of autoroam module to specified file
 ======================================================================== */

void autoroam_report (FILE *fp)
{
	fprintf (fp, "\nAUTOROAM MODULE\n"
				 "===============\n\n");

	fprintf (fp, "  AutoRoam Status: %s\n\n",
		autoroam_opts.enabled ? "Enabled" : "Disabled");

	fprintf (fp, "  AutoRoam While Blind .... %s\n",
		autoroam_opts.roam_blind ? "YES" : "NO");

	fprintf (fp, "  AutoRoam Using Doors .... %s\n",
		autoroam_opts.use_doors ? "YES" : "NO");

	fprintf (fp, "  AutoRoam Secret Exits ... %s\n",
		autoroam_opts.use_secrets ? "YES" : "NO");

	fprintf (fp, "  AutoRoam Special Exits .. %s\n\n",
		(autoroam_opts.exits & EXIT_SECRET) ? "YES" : "NO");

	fprintf (fp, "  AutoRoam Exits: ");

	if (autoroam_opts.exits)
	{
		exit_table_t *et = exit_table;

		for (et = exit_table; et->short_str; et++)
			if (autoroam_opts.exits & et->direction)
				fprintf (fp, "%s ", et->short_str);

		fprintf (fp, "\n\n");
	}
	else
		fprintf (fp, "None\n\n");
}


/* =========================================================================
 = AUTOROAM_MAP
 =
 = Maps the autoroam dialog
 ======================================================================== */

void autoroam_map (void)
{
	if (autoroam.visible)
		return;

	show_panel (autoroam.p);
	memset (&selected, 0, sizeof (selected));
	selected.frame = FRAME_OPTIONS;
	action_area.focus = 0;
	autoroam.visible = TRUE;
	autoroam_update ();
	update_display ();
}


/* =========================================================================
 = AUTOROAM_UNMAP
 =
 = Unmaps the autoroam dialog
 ======================================================================== */

void autoroam_unmap (void)
{
	if (!autoroam.visible)
		return;

	hide_panel (autoroam.p);
	autoroam.visible = FALSE;
	update_display ();
}


/* =========================================================================
 = AUTOROAM_UPDATE
 =
 = Update the autoroam dialog
 ======================================================================== */

void autoroam_update (void)
{
	key_value_t *kv;
	checkbox_t checkbox;
	gint pos;

	werase (autoroam.w);
	wattrset (autoroam.w, ATTR_BORDER | A_BOLD);

	wborder (autoroam.w,
		window_ui.vline, window_ui.vline, window_ui.hline, window_ui.hline,
		window_ui.nw,    window_ui.ne,    window_ui.sw,    window_ui.se);

	border_draw_bracket (&autoroam, '[');
	wattrset (autoroam.w, ATTR_WHITE | A_BOLD);
	waddstr (autoroam.w, " Auto Roam ");
	border_draw_bracket (&autoroam, ']');

	/* initialize checkboxes */
	checkbox.cwin = &autoroam;
	checkbox.width = CHECKBOX_WIDTH;
	checkbox.xpos = 2;
	checkbox.ypos = 2;

	/* == options frame ================================================= */

	wattrset (autoroam.w, autoroam.attr);
	wmove (autoroam.w, checkbox.ypos, checkbox.xpos);
	whline (autoroam.w, widget_ui.hline, checkbox.width*2);
	mvwaddstr (autoroam.w, checkbox.ypos, checkbox.xpos+1, " Options ");
	checkbox.ypos += 2;

	pos = 0;
	for (kv = option_table; kv->key; kv++)
	{
		checkbox.str = kv->key;
		checkbox.selected = (pos == selected.option);
		checkbox.focused = (selected.frame == FRAME_OPTIONS);

		switch (pos)
		{
		case OPTION_ENABLE_ROAM: checkbox.enabled = autoroam_opts.enabled;     break;
		case OPTION_ROAM_BLIND:  checkbox.enabled = autoroam_opts.roam_blind;  break;
		case OPTION_USE_DOORS:   checkbox.enabled = autoroam_opts.use_doors;   break;
		case OPTION_USE_SECRETS: checkbox.enabled = autoroam_opts.use_secrets; break;
		case OPTION_USE_SPECIAL:
			checkbox.enabled = (autoroam_opts.exits & EXIT_SPECIAL);
			break;
		default:
			g_assert_not_reached ();
		}

		checkbox_draw (&checkbox);

		if (pos % 2)
		{
			checkbox.xpos = 2;
			checkbox.ypos++;
		}
		else
			checkbox.xpos += checkbox.width;

		pos++;
	}
	checkbox.ypos += 2;
	checkbox.xpos = 2;

	/* == directions frame ============================================== */

	wattrset (autoroam.w, autoroam.attr);
    wmove (autoroam.w, checkbox.ypos, checkbox.xpos);
    whline (autoroam.w, widget_ui.hline, checkbox.width*2);
	mvwaddstr (autoroam.w, checkbox.ypos, checkbox.xpos+1, " Directions ");
	checkbox.ypos += 2;

	pos = 0;
	for (kv = direction_table; kv->key; kv++)
	{
		checkbox.str = kv->key;
		checkbox.enabled = (autoroam_opts.exits & kv->value);
		checkbox.selected = (pos == selected.direction);
		checkbox.focused = (selected.frame == FRAME_DIRECTIONS);
		checkbox_draw (&checkbox);

		if (pos % 2)
		{
			checkbox.xpos = 2;
			checkbox.ypos++;
		}
		else
			checkbox.xpos += checkbox.width;

		pos++;
	}
	checkbox.ypos++;

	wattrset (autoroam.w, autoroam.attr);
	wmove (autoroam.w, checkbox.ypos, checkbox.xpos);
	whline (autoroam.w, widget_ui.hline, checkbox.width * 2);

	/* == action area =================================================== */

	action_area_draw (&action_area);

	update_display ();
}


/* =========================================================================
 = AUTOROAM_KEY_HANDLER
 =
 = Handle autoroam key strokes
 ======================================================================== */

gboolean autoroam_key_handler (gint ch)
{
	switch (ch)
	{
	case ' ': /* toggle selected checkbox */
		if (selected.frame == FRAME_OPTIONS)
		{
			if (selected.option == OPTION_ENABLE_ROAM)
				autoroam_opts.enabled = !autoroam_opts.enabled;

			else if (selected.option == OPTION_ROAM_BLIND)
				autoroam_opts.roam_blind = !autoroam_opts.roam_blind;

			else if (selected.option == OPTION_USE_DOORS)
				autoroam_opts.use_doors = !autoroam_opts.use_doors;

			else if (selected.option == OPTION_USE_SECRETS)
				autoroam_opts.use_secrets = !autoroam_opts.use_secrets;

			else if (selected.option == OPTION_USE_SPECIAL)
				Toggle (autoroam_opts.exits, EXIT_SPECIAL);
		}
		else if (selected.frame == FRAME_DIRECTIONS)
		{
			Toggle (autoroam_opts.exits,
				get_key_value_by_index (direction_table, selected.direction));
		}
		autoroam_update ();
		break;

	case KEY_CR: /* activate selected action */
		if (action_area.focus == ACTION_DISMISS)
			autoroam_unmap ();
		break;

	case KEY_DOWN:
		if (selected.frame == FRAME_OPTIONS)
		{
			if ((selected.option + 2) < OPTION_TABLE_SIZE)
				selected.option += 2;
		}
		else if (selected.frame == FRAME_DIRECTIONS)
		{
			if ((selected.direction + 2) < DIRECTION_TABLE_SIZE)
				selected.direction += 2;
		}
		autoroam_update ();
		break;

	case KEY_LEFT:
		if (selected.frame == FRAME_OPTIONS)
		{
			if (selected.option % 2)
				selected.option--;
		}
		else if (selected.frame == FRAME_DIRECTIONS)
		{
			if (selected.direction % 2)
				selected.direction--;
		}
		autoroam_update ();
		break;

	case KEY_RIGHT:
		if (selected.frame == FRAME_OPTIONS)
		{
			if (!(selected.option % 2)
				&& selected.option+1 < OPTION_TABLE_SIZE)
				selected.option++;
		}
		else if (selected.frame == FRAME_DIRECTIONS)
		{
			if (!(selected.direction % 2))
				selected.direction++;
		}
		autoroam_update ();
		break;

	case KEY_TAB:
		selected.option = 0;
		selected.direction = 0;

		if (selected.frame == FRAME_NONE)
		{
			action_area.focus = action_area_get_next (&action_area,
				autoroam_actions);
			if (!action_area.focus)
				selected.frame = FRAME_OPTIONS;
		}

		else if (selected.frame == FRAME_OPTIONS)
			selected.frame = FRAME_DIRECTIONS;

		else if (selected.frame == FRAME_DIRECTIONS)
		{
			selected.frame = FRAME_NONE;
			autoroam_key_handler (KEY_TAB);
		}
		autoroam_update ();
		break;

	case KEY_UP:
		if (selected.frame == FRAME_OPTIONS)
		{
			if ((selected.option - 2) > -1)
				selected.option -= 2;
		}
		else if (selected.frame == FRAME_DIRECTIONS)
		{
			if ((selected.direction - 2) > -1)
				selected.direction -= 2;
		}
		autoroam_update ();
		break;

	case KEY_ALT_W:
	case KEY_CTRL_X:
		autoroam_unmap ();
		break;
	}

	if (destination && !autoroam_opts.enabled)
		client_ai_movement_reset ();

	return TRUE;
}
