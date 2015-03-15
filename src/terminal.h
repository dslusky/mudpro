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

#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include "defs.h"
#include "widgets.h"

#define TERMINAL_TITLE_DEFAULT "MudPRO"

#define CP(x,y)        COLOR_PAIR (palette[x][y])
#define ATTR_NORMAL    COLOR_PAIR (0)

#define ATTR_BLACK     CP (COLOR_BLACK,   COLOR_BLACK)
#define ATTR_RED       CP (COLOR_RED,     COLOR_BLACK)
#define ATTR_GREEN     CP (COLOR_GREEN,   COLOR_BLACK)
#define ATTR_YELLOW    CP (COLOR_YELLOW,  COLOR_BLACK)
#define ATTR_BLUE      CP (COLOR_BLUE,    COLOR_BLACK)
#define ATTR_MAGENTA   CP (COLOR_MAGENTA, COLOR_BLACK)
#define ATTR_CYAN      CP (COLOR_CYAN,    COLOR_BLACK)
#define ATTR_WHITE     CP (COLOR_WHITE,   COLOR_BLACK)

/* interface colors */
#define ATTR_BORDER    CP (COLOR_BLACK,  COLOR_BLACK)
#define ATTR_WINDOW    CP (COLOR_WHITE,  COLOR_RED)
#define ATTR_MENU      CP (COLOR_BLACK,  COLOR_WHITE)
#define ATTR_MENU_S    CP (COLOR_WHITE,  COLOR_BLACK)
#define ATTR_ACCEL     CP (COLOR_YELLOW, COLOR_WHITE)
#define ATTR_ACCEL_S   CP (COLOR_YELLOW, COLOR_BLACK)
#define ATTR_DIM       CP (COLOR_BLACK,  COLOR_RED)
#define ATTR_SUBTLE    CP (COLOR_RED,    COLOR_RED)
#define ATTR_HILITE    CP (COLOR_YELLOW, COLOR_RED)
#define ATTR_SELECTED  CP (COLOR_BLACK,  COLOR_WHITE)
#define ATTR_INVERTED  CP (COLOR_WHITE,  COLOR_BLACK)
#define ATTR_GAUGE     CP (COLOR_RED,    COLOR_RED)
#define ATTR_NOTICE    CP (COLOR_WHITE,  COLOR_BLUE)

/* palette size */
#define FG_COLORS  8
#define BG_COLORS  8

#define MAX_PARAMS 8

typedef struct
{
	gint state;
	gint type;
	gint param[MAX_PARAMS];
	gint params;
} sequence_t;

typedef struct
{
	gint gauge, bullet;
	gint vline, hline;
	gint nw, ne, sw, se;
} ui_charset_t;

enum /* sequence types */
{
	SEQ_NOOP, /* NOOP */
	SEQ_CUB,  /* Cursor Backward */
	SEQ_CUD,  /* Cursor Down */
	SEQ_CUF,  /* Cursor Forward */
	SEQ_CUP,  /* Cursor Position */
	SEQ_CUU,  /* Cursor Up */
	SEQ_DSR,  /* Device Status Report */
	SEQ_ED,   /* Erase in Display */
	SEQ_EL,   /* Erase in Line */
	SEQ_HVP,  /* Horz & Vertical Position */
	SEQ_SGR,  /* Select Graphic Rendition */
};

extern cwin_t terminal;
extern ui_charset_t window_ui;
extern ui_charset_t menu_ui;
extern ui_charset_t widget_ui;
extern gint palette[FG_COLORS][BG_COLORS];

void terminal_init (void);
void terminal_cleanup (void);
void terminal_clear (void);
void terminal_palette_init (void);
void terminal_charset_init (void);
void terminal_set_title (gchar *fmt, ...);
void terminal_put (gint ch);
void terminal_sequence_read (gint ch);
void terminal_resize (void);
void printt (gchar *fmt, ...);

#endif /* __TERMINAL_H__ */
