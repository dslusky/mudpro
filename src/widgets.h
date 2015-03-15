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

#ifndef __WIDGETS_H__
#define __WIDGETS_H__

#include "defs.h"

#define LISTVIEW_YPOS(y)		(listview->ypos + y + 1)
#define LISTVIEW_SELECTION(x)	(x->offset + x->position)

typedef struct /* curses window */
{
	WINDOW *w;          /* curses window structure */
	PANEL *p;           /* curses panel structure (for overlapping windows) */
	gpointer data;      /* misc user data */
	gboolean visible;   /* window visibility flag */
	gint xpos, ypos;    /* current XY position */
	gint width, height; /* width/height of window */
	attr_t attr;        /* text attributes */
} cwin_t;

typedef struct
{
	cwin_t *cwin;       /* window containing listview */
	GSList *data;       /* list data */
	gint position;      /* listview cursor position */
	gint offset;        /* offset in list data */
	gint width, height; /* width/height of listview */
	gint xpos, ypos;    /* XY position relative to window */
} listview_t;

typedef struct
{
	cwin_t *cwin;         /* window containing action area */
	key_value_t *actions; /* list of actions */
	gint focus;           /* selected action (if any) */
} action_area_t;

typedef struct
{
	cwin_t *cwin;      /* window containing checkbox */
	gchar *str;        /* checkbox label */
	gint width;        /* width of checkbox */
	gint xpos, ypos;   /* XY position */
	gboolean enabled;  /* checkbox toggled status */
	gboolean selected; /* selected checkbox */
	gboolean focused;  /* focused checkbox (if any) */
} checkbox_t;

enum /* action area action types */
{
	ACTION_NONE,
	ACTION_ACCEPT,
	ACTION_DISMISS,
	ACTION_ADD,
	ACTION_REMOVE
};

listview_t *listview_new (cwin_t *cwin, gint width, gint height,
	gint ypos, gint xpos);
void listview_free (listview_t *listview);
void listview_draw (listview_t *listview, gint maxlen, gboolean focused);
void listview_scroll (listview_t *listview, gint lines);
void action_area_draw (action_area_t *action_area);
gint action_area_get_next (action_area_t *aa, key_value_t *data);
void checkbox_draw (checkbox_t *checkbox);

#endif /* __WIDGETS_H__ */
