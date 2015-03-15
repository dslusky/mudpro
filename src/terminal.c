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

#include <signal.h>
#include <string.h>

#include "about.h"
#include "character.h"
#include "menubar.h"
#include "mudpro.h"
#include "osd.h"
#include "parse.h"
#include "terminal.h"
#include "utils.h"

cwin_t terminal;
ui_charset_t window_ui;
ui_charset_t menu_ui;
ui_charset_t widget_ui;

gint palette[FG_COLORS][BG_COLORS];

static sequence_t seq;

static void terminal_sequence_dispatch (void);


/* =========================================================================
 = TERMINAL_INIT
 =
 = Initialize the terminal window
 ======================================================================== */

void terminal_init (void)
{
	memset (&terminal, 0, sizeof (cwin_t));
	memset (&seq, 0, sizeof (sequence_t));

	terminal.w = newwin (LINES-1, 0, 1, 0);
	terminal.p = new_panel (terminal.w);
	terminal.visible = TRUE;

	scrollok (terminal.w, TRUE);
	idlok (terminal.w, TRUE);
	nodelay (terminal.w, TRUE);
	intrflush (terminal.w, FALSE);
	keypad (terminal.w, TRUE);

	terminal_clear ();
}


/* =========================================================================
 = TERMINAL_CLEANUP
 =
 = Cleanup after the terminal window
 ======================================================================== */

void terminal_cleanup (void)
{
	del_panel (terminal.p);
	delwin (terminal.w);
}


/* =========================================================================
 = TERMINAL_CLEAR
 =
 = Clear the terminal area
 ======================================================================== */

void terminal_clear (void)
{
	notice_visible = FALSE;
	werase (terminal.w);
	update_display ();
}


/* =========================================================================
 = TERMINAL_PALETTE_INIT
 =
 = Initialize terminal color palette
 ======================================================================== */

void terminal_palette_init (void)
{
	gint fg, bg;
	gint pair = 1;

	for (fg = 0; fg <= 7; fg++)
	{
		for (bg = 0; bg <= 7; bg++)
		{
#ifdef __linux__
			/* so we can have nice transparent background :)*/
			init_pair (pair, fg, (bg == COLOR_BLACK) ? -1 : bg);
#elif _WIN32
			init_pair (pair, fg, bg);
#endif
			palette[fg][bg] = pair++;
		}
	}
}


/* =========================================================================
 = TERMINAL_CHARSET_INIT
 =
 = Initialize terminal character set
 ======================================================================== */

void terminal_charset_init (void)
{
	memset (&window_ui, 0, sizeof (ui_charset_t));
	memset (&menu_ui,   0, sizeof (ui_charset_t));
	memset (&widget_ui, 0, sizeof (ui_charset_t));

	switch (character.line_style)
	{
	case 0: /* default line style */
	case 1:

		window_ui.gauge  = 0xF0;
		window_ui.bullet = 0xC4;
		window_ui.vline  = 0xB3;
		window_ui.hline  = 0xC4;
		window_ui.nw     = 0xDA;
		window_ui.ne     = 0xBF;
		window_ui.sw     = 0xC0;
		window_ui.se     = 0xD9;

		memcpy (&menu_ui,   &window_ui, sizeof (ui_charset_t));
		memcpy (&widget_ui, &window_ui, sizeof (ui_charset_t));
		break;

	case 2: /* alternate line style */

		window_ui.gauge  = 0xF0;
		window_ui.bullet = 0xFE;
		window_ui.vline  = 0xBA;
		window_ui.hline  = 0xCD;
		window_ui.nw     = 0xC9;
		window_ui.ne     = 0xBB;
		window_ui.sw     = 0xC8;
		window_ui.se     = 0xBC;

		menu_ui.gauge    = 0xF0;
		menu_ui.bullet   = 0xFE;
		menu_ui.vline    = 0xB3;
		menu_ui.hline    = 0xCD;
		menu_ui.nw       = 0xD5;
		menu_ui.ne       = 0xB8;
		menu_ui.sw       = 0xD4;
		menu_ui.se       = 0xBE;

		widget_ui.gauge  = 0xF0;
		widget_ui.bullet = 0xFE;
		widget_ui.vline  = 0xB3;
		widget_ui.hline  = 0xC4;
		widget_ui.nw     = 0xDA;
		widget_ui.ne     = 0xBF;
		widget_ui.sw     = 0xC0;
		widget_ui.se     = 0xD9;
		break;

	case 3: /* ascii-only line style */

		window_ui.gauge  = '=';
		window_ui.bullet = '-';
		window_ui.vline  = '|';
		window_ui.hline  = '-';
		window_ui.nw     = '+';
		window_ui.ne     = '+';
		window_ui.sw     = '+';
		window_ui.se     = '+';

		memcpy (&menu_ui,   &window_ui, sizeof (ui_charset_t));
		memcpy (&widget_ui, &window_ui, sizeof (ui_charset_t));
		break;
		
	case 4: /* alternate ascii-only line style */
	
		window_ui.gauge  = '=';
		window_ui.bullet = '-';
		window_ui.vline  = '|';
		window_ui.hline  = '-';
		window_ui.nw     = ' ';
		window_ui.ne     = ' ';
		window_ui.sw     = ' ';
		window_ui.se     = ' ';

		memcpy (&menu_ui,   &window_ui, sizeof (ui_charset_t));
		memcpy (&widget_ui, &window_ui, sizeof (ui_charset_t));
		break;

	default:
		g_assert_not_reached ();
	}
}


/* =========================================================================
 = TERMINAL_SET_TITLE
 =
 = Sets the terminal title
 ======================================================================== */

void terminal_set_title (gchar *fmt, ...)
{
	va_list argptr;
	gchar buf[STD_STRBUF];

	if (!character.option.set_title)
		return;

	va_start (argptr, fmt);
	vsnprintf (buf, sizeof (buf), fmt, argptr);
	va_end (argptr);

#ifdef __linux__
	/* exit curses momentarily */
	endwin ();

	/* update terminal title (is this widely supported?) */
	printf ("[21]l%s\\", buf);

	/* restore curses */
	update_display ();
#elif _WIN32
	SetConsoleTitle(buf);
#endif
}


/* =========================================================================
 = TERMINAL_PUT
 =
 = Prints character to the terminal
 ======================================================================== */

void terminal_put (gint ch)
{
	waddch (terminal.w, ch | terminal.attr);
	parse_line_buffer (ch);
}


/* =========================================================================
 = TERMINAL_SEQUENCE_READ
 =
 = Reads ansi sequences in data stream
 ======================================================================== */

void terminal_sequence_read (gint ch)
{
	static enum { S_NORM, S_ESC, S_CSI } state = S_NORM;

	switch (state)
	{
	case S_ESC:
		if (ch == '[')
		{
			memset (&seq, 0, sizeof (sequence_t));
			seq.params = 0;
			state = S_CSI;
		}
		else
			state = S_NORM;
		break;

	case S_CSI:
		switch (ch)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			seq.param[seq.params] = seq.param[seq.params] * 10 + ch - '0';
			return; /* sequence incomplete */
		case 'A': seq.type = SEQ_CUU;  break;
		case 'B': seq.type = SEQ_CUD;  break;
		case 'C': seq.type = SEQ_CUF;  break;
		case 'D': seq.type = SEQ_CUB;  break;
		case 'f': seq.type = SEQ_HVP;  break;
		case 'H': seq.type = SEQ_CUP;  break;
		case 'J': seq.type = SEQ_ED;   break;
		case 'K': seq.type = SEQ_EL;   break;
		case 'm': seq.type = SEQ_SGR;  break;
		case 'n': seq.type = SEQ_DSR;  break;
		case '!': seq.type = SEQ_NOOP; break;
		case ';':
			if (seq.params < (MAX_PARAMS-1))
				seq.params++;
			return; /* sequence incomplete */
#ifdef DEBUG
		default:
			printt ("DEBUG: Unhandled sequence: %d (0x%x)", ch, ch);
#endif
		}
		state = S_NORM;
		terminal_sequence_dispatch ();
		break;

	case S_NORM:
		if (ch == 0x1B) /* escape */
			state = S_ESC;
		else if (ch == 0x11) /* XON */
			/* for some reason MMUD sends these while drawing 'train stats' */
			/* the space is put so that everything is displayed correctly */
			terminal_put (' ');
		else if (ch > 31 || ch == '\b' || ch == '\n')
			terminal_put (ch);
		break;
	}
}


/* =========================================================================
 = TERMINAL_SEQUENCE_DISPATCH
 =
 = Executes the current sequence
 ======================================================================== */

static void terminal_sequence_dispatch (void)
{
	static gint fg = 7;
	static gint bg = 0;
	int x, y;

#if 0 /* display all sequences */
	wprintw (terminal.w, "(seq: %d %d:%d)", seq.type,
		seq.param[0], seq.param[1]);
#endif

	switch (seq.type)
	{
	case SEQ_CUB:
		getyx (terminal.w, y, x);
		wmove (terminal.w, y, CLAMP (x - seq.param[0], 0, COLS-1));
		break;

	case SEQ_CUD:
		getyx (terminal.w, y, x);
		wmove (terminal.w, MIN (y + MAX (1, seq.param[0]), LINES-1), x);
		break;

	case SEQ_CUF:
		getyx (terminal.w, y, x);
		wmove (terminal.w, y, MIN (x + MAX (1, seq.param[0]), COLS-1));
		break;

	case SEQ_CUP:
	case SEQ_HVP:
		getyx (terminal.w, y, x);
		wmove (terminal.w,
			CLAMP (seq.param[0]-1, 0, LINES-1),
			CLAMP (seq.param[1]-1, 0, COLS-1));
		break;

	case SEQ_CUU:
		getyx (terminal.w, y, x);
		wmove (terminal.w, MAX (1, y - 1), x);
		break;

	case SEQ_DSR:
		if (seq.param[0] == 6)
			send_raw ("[45;5R"); /* make WG happy ... */
		break;

	case SEQ_ED:
		switch (seq.param[0])
		{
#ifdef DEBUG
		case 0: /* TODO: erase cursor to end of screen */
		case 1: /* TODO: erase beginning of screen to cursor */
			printt ("SEQ_ED %d/%d unhandled", seq.param[0], seq.param[1]);
			break;
#endif
		case 2:
			werase (terminal.w);
			break;
		}
		break;

	case SEQ_EL:
		switch (seq.param[0])
		{
		case 0:
			wclrtoeol(terminal.w);
			break;
		case 1:
		case 2:
			getyx (terminal.w, y, x);
			wmove (terminal.w, y, 0);
			wclrtoeol (terminal.w);
			break;
		}
		break;

	case SEQ_SGR:
		for (x = 0; x <= seq.params; x++)
		{
			if (seq.param[x] == 0) { fg = 7; bg = 0; terminal.attr &= ~A_BOLD; }
			else if (seq.param[x] == 1) terminal.attr |= A_BOLD;
			else if (seq.param[x] == 5) /* FIXME: implement this */;
			else if (seq.param[x] == 7)	{ gint tmp = fg; fg = bg; bg = tmp; }
			else if (seq.param[x] > 29 && seq.param[x] < 38) { fg = seq.param[x]-30; }
			else if (seq.param[x] > 39 && seq.param[x] < 48) { bg = seq.param[x]-40; }
#ifdef DEBUG
			else printt ("SEQ_SGR: unhandled (param %d: %d)", x, seq.param[x]);
#endif
		}
		terminal.attr = CP (fg, bg) | (terminal.attr & A_BOLD);
		break;
	}
}


/* =========================================================================
 = TERMINAL_RESIZE
 =
 = Handle terminal resizing
 ======================================================================== */

void terminal_resize (void)
{
	gboolean about_visible = about.visible;

	/* get new terminal size */
	endwin ();
	initscr ();

	wresize (menubar.w, MENUBAR_HEIGHT, COLS);
	wresize (terminal.w, MAX (LINES-MENUBAR_HEIGHT, 1), COLS);

	menubar_update ();
	/* terminal_update (); */

	osd_dock_restack ();

	about_cleanup ();
	about_init ();
	if (about_visible) about_map ();

	update_display ();
}


/* =========================================================================
 = PRINTT
 =
 = Print message to the terminal
 ======================================================================== */

void printt (gchar *fmt, ...)
{
	va_list argptr;
	gchar buf[STD_STRBUF];
	gchar *message;
	gint y, x;

	if (!terminal.w)
		return; /* terminal not initialized */

	va_start (argptr, fmt);
	vsnprintf (buf, sizeof (buf), fmt, argptr);
	va_end (argptr);

	message = g_strdup_printf ("[%s]", buf);
	mudpro_capture_log_append (message);

	if (notice_visible)
		terminal_clear ();

	getyx (terminal.w, y, x);
	if (x != 0)
		waddch (terminal.w, '\n');


	wattrset (terminal.w, ATTR_NOTICE | A_BOLD);
	wprintw (terminal.w, "%s\n", message);
	g_free (message);
	wattrset (terminal.w, ATTR_NORMAL);

	update_display ();
}
