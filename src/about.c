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

#include "about.h"
#include "keys.h"
#include "terminal.h"
#include "utils.h"

cwin_t about;


/* =========================================================================
 = ABOUT_INIT
 =
 = Initialize about dialog
 ======================================================================== */

void about_init (void)
{
	memset (&about, 0, sizeof (cwin_t));

	about.width = 67;
	about.height = 16;

	about.xpos = (COLS - about.width) / 2;
	about.ypos = (LINES - about.height) / 2;

	about.w = newwin (about.height,
					  about.width,
					  about.ypos,
					  about.xpos);

	about.p = new_panel (about.w);
	about.visible = FALSE;
	hide_panel (about.p);

	about.attr = ATTR_WINDOW | A_BOLD;
	wbkgdset (about.w, ATTR_WINDOW);

	leaveok (about.w, TRUE);
}


/* =========================================================================
 = ABOUT_CLEANUP
 =
 = Clean up after about dialog
 ======================================================================== */

void about_cleanup (void)
{
	del_panel (about.p);
	delwin (about.w);
}


/* =========================================================================
 = ABOUT_UPDATE
 =
 = Update about dialog
 ======================================================================== */

void about_update (void)
{
	werase (about.w);
	wattrset (about.w, ATTR_BORDER | A_BOLD);

	wborder (about.w,
		window_ui.vline, window_ui.vline, window_ui.hline, window_ui.hline,
		window_ui.nw,    window_ui.ne,    window_ui.sw,    window_ui.se);

	border_draw_bracket (&about, '[');
	wattrset (about.w, ATTR_WHITE | A_BOLD);
	waddstr (about.w, " About MudPRO ");
	border_draw_bracket (&about, ']');

	wattrset (about.w, CP (COLOR_WHITE, COLOR_BLACK));
	mvwaddstr (about.w, 2, 3,
		" ‹‹‹‹‹‹‹‹‹ ‹‹‹‹ ‹‹‹‹ ‹‹‹‹‹‹‹‹‹ ‹‹‹‹‹‹‹‹‹ ‹‹‹‹‹‹‹‹‹ ‹‹‹‹‹‹‹‹‹ ");
	mvwaddstr (about.w, 3, 3,
		" ﬂﬂﬂﬂ€ﬂﬂﬂﬂ ﬂﬂﬂﬂ ﬂﬂﬂﬂ ﬂﬂﬂﬂﬂﬂﬂﬂﬂ ﬂﬂﬂﬂ ﬂﬂﬂﬂ ﬂﬂﬂﬂ ﬂﬂﬂﬂ ﬂﬂﬂﬂ ﬂﬂﬂﬂ ");

	wattrset (about.w, CP (COLOR_RED, COLOR_BLACK) | A_BOLD);
	mvwaddstr (about.w, 4, 3, " €€€‹ ‹€€€ €€€€‹€€€€ €€€€‹€€€€");

	wattrset (about.w, CP (COLOR_BLACK, COLOR_BLACK) | A_BOLD);
	waddstr (about.w, " €€€€‹€€€€ €€€€‹€€€ﬂ €€€€‹€€€€ ");

	wattrset (about.w, CP (COLOR_RED, COLOR_BLACK) | A_BOLD);
	mvwaddstr (about.w, 5, 3, " ﬂﬂﬂﬂ ﬂﬂﬂﬂ ﬂﬂﬂﬂﬂﬂﬂﬂﬂ ﬂﬂﬂﬂﬂﬂﬂﬂﬂ");

	wattrset (about.w, CP (COLOR_BLACK, COLOR_BLACK) | A_BOLD);
	waddstr (about.w, " €€€€      ﬂﬂﬂﬂ €€€€ ﬂﬂﬂﬂﬂﬂﬂﬂﬂ ");

	wattrset (about.w, CP (COLOR_RED, COLOR_BLACK));
	mvwaddstr (about.w, 5, 39, "‹‹‹‹");
	mvwaddstr (about.w, 6, 33, "‹‹‹‹‹‹");
	mvwaddstr (about.w, 6, 48, "‹‹‹‹‹‹");

	wattrset (about.w, CP (COLOR_YELLOW, COLOR_RED) | A_BOLD);
	mvwprintw (about.w, 7, 5, "Release: %s", RELEASE);

	wattrset (about.w, CP (COLOR_WHITE, COLOR_RED) | A_BOLD);
	mvwprintw (about.w, 9,  3, "Copyright (c) 2002-2015 by David Slusky");

	mvwprintw (about.w, 11, 3,
		"This program is free software; you can redistribute it and/or");
		
	mvwprintw (about.w, 12, 3,
		"modify it under the terms of the GNU General Public License");
		
	mvwprintw (about.w, 13, 3,
		"version 2, as published by the Free Software Foundation.");
}


/* =========================================================================
 = ABOUT_MAP
 =
 = Map about dialog on the screen
 ======================================================================== */

void about_map (void)
{
	if (about.visible)
		return;

	show_panel (about.p);
	about.visible = TRUE;
	about_update ();
	update_display ();
}


/* =========================================================================
 = ABOUT_UNMAP
 =
 = Unmap about dialog from the screen
 ======================================================================== */

void about_unmap (void)
{
	if (!about.visible)
		return;

	hide_panel (about.p);
	about.visible = FALSE;
	update_display ();
}


/* =========================================================================
 = ABOUT_KEY_HANDLER
 =
 = Handle about dialog user input
 ======================================================================== */

gboolean about_key_handler (gint ch)
{
	about_unmap ();
	return TRUE;
}
