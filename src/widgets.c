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

#include "character.h"
#include "defs.h"
#include "utils.h"
#include "mudpro.h"
#include "terminal.h"
#include "widgets.h"

#define ACTION_AREA(x)	(x->height-3)


/* =========================================================================
 = LISTVIEW_NEW
 =
 = Creates a new listview widget
 ======================================================================== */

listview_t *listview_new (cwin_t *cwin, gint width, gint height,
	gint ypos, gint xpos)
{
	listview_t *listview;

	g_assert (cwin != NULL);

	listview = g_malloc0 (sizeof (listview_t));
	listview->cwin = cwin;
	listview->width = width;
	listview->height = height;
	listview->xpos = xpos;
	listview->ypos = ypos;

	return listview;
}


/* =========================================================================
 = LISTVIEW_FREE
 =
 = Free data allocated to listview
 ======================================================================== */

void listview_free (listview_t *listview)
{
	GSList *node;

	g_assert (listview != NULL);

	for (node = listview->data; node; node = node->next)
		g_free (node->data);

	g_slist_free (listview->data);
	g_free (listview);
}


/* =========================================================================
 = LISTVIEW_DRAW
 =
 = Draw listview widget
 ======================================================================== */

void listview_draw (listview_t *listview, gint maxlen, gboolean focused)
{
	WINDOW *w;
	GSList *node;
	gchar *p;
	gint ypos;

	g_assert (listview != NULL);
	w = listview->cwin->w;

	wattrset (w, listview->cwin->attr);
	wmove (w, listview->ypos, listview->xpos);
	whline (w, widget_ui.hline, listview->width);

	node = g_slist_nth (listview->data, listview->offset);

	ypos = 0;
	while (node && ypos < (listview->height-2))
	{
		g_assert (node->data != NULL);
		p = (gchar *) node->data;

		wmove (w, LISTVIEW_YPOS (ypos), listview->xpos);

		if (ypos == listview->position)
		{
			if (focused)
				wattrset (w, ATTR_SELECTED);
			else
				wattrset (w, ATTR_INVERTED);
			whline (w, ' ', listview->width);
		}
		else
			wattrset (w, listview->cwin->attr);

		wprintw (w, " %c ", widget_ui.bullet);

		if (strlen (p) <= maxlen)
			wprintw (w, "%s", p);
		else
		{
			for (; (p - (gchar *) node->data) < (maxlen - 4); p++)
				wprintw (w, "%c", *p);

			wprintw (w, " ...");
		}

		node = g_slist_next (node);
		ypos++;
	}

	wattrset (w, listview->cwin->attr);
	wmove (w, (listview->ypos + listview->height) - 1,
		listview->xpos);
	whline (w, widget_ui.hline, listview->width);
}


/* =========================================================================
 = LISTVIEW_SCROLL
 =
 = Scroll the listview
 ======================================================================== */

void listview_scroll (listview_t *listview, gint lines)
{
	if (!lines || !g_slist_nth_data (listview->data,
		listview->offset + listview->position + lines))
		return;

	if ((listview->position + lines) >= 0 &&
		(listview->position + lines) <= (listview->height - 3))
		listview->position += lines;
	else
		listview->offset += lines;
}


/* =========================================================================
 = ACTION_AREA_DRAW
 =
 = Draw buttons in action area
 ======================================================================== */

void action_area_draw (action_area_t *action_area)
{
	WINDOW *w;
	key_value_t *pos;
	gint len = 0;

	g_assert (action_area != NULL);
	g_assert (action_area->cwin != NULL);
	g_assert (action_area->actions != NULL);
	w = action_area->cwin->w;

	for (pos = action_area->actions; pos->key; pos++)
		len += (strlen (pos->key) + 2);

	wmove (w, ACTION_AREA (action_area->cwin),
		action_area->cwin->width - (len + 1));

	for (pos = action_area->actions; pos->key; pos++)
	{
		if (pos->value == action_area->focus)
			wattrset (w, ATTR_SELECTED);
		else
			wattrset (w, action_area->cwin->attr);

		waddstr (w, pos->key);
		wattrset (w, action_area->cwin->attr);
		waddstr (w, "  ");
	}
}


/* =========================================================================
 = ACTION_AREA_GET_NEXT
 =
 = Gets next action in action area
 ======================================================================== */

gint action_area_get_next (action_area_t *aa, key_value_t *data)
{
	key_value_t *dd;

	g_assert (aa != NULL);
	g_assert (data != NULL);

	/* if nothing is focused, return the first action */
	if (aa->focus == 0)
		return data->value;

	/* otherwise return the next in list */
	for (dd = data; dd->key; dd++)
	{
		if (dd->value == aa->focus)
			return (dd+1)->value;
	}

	return 0;
}


/* =========================================================================
 = CHECKBOX_DRAW
 =
 = Draw a checkbox widget
 ======================================================================== */

void checkbox_draw (checkbox_t *checkbox)
{
	WINDOW *w;

	g_assert (checkbox != NULL);
	g_assert (checkbox->cwin != NULL);
	g_assert (checkbox->str != NULL);

	w = checkbox->cwin->w;
	wmove (w, checkbox->ypos, checkbox->xpos);

	if (checkbox->selected)
	{
		if (checkbox->focused)
			wattrset (w, ATTR_SELECTED | A_BOLD);
		/* else
			wattrset (w, ATTR_INVERTED | A_BOLD); */

		whline (w, ' ', checkbox->width-1);
	}
	else
		wattrset (w, checkbox->cwin->attr);

	if (checkbox->enabled)
		waddstr (w, "[*] ");
	else
		waddstr (w, "[ ] ");

	waddstr (w, checkbox->str);
}
