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

#ifndef __MENUBAR_H__
#define __MENUBAR_H__

#include "defs.h"
#include "widgets.h"

#define MENUBAR_HEIGHT 1
#define MENUBAR_OFFSET(x)   (MENUBAR_HEIGHT+x)

#define MENU_WIDTH_MIN 20

#define MENU_ITEM(x,y)  ((void *)x + y * sizeof (menu_item_t))

/* call access funtion if defined, othwise assume true */
#define MENU_ITEM_ACCESS(x) (x->access ? (*x->access)(x) : TRUE)

typedef struct
{
	gchar *label;
	gchar accel;
	gint type;
	gboolean (*access)(gpointer data);
	gboolean *state;

	/* custom toggle callbacks */
	void (*activate) (void);
	void (*deactivate) (void);
} menu_item_t;

enum /* menus */
{
	MENU_NONE,
	MENU_MUDPRO,
	MENU_AUTOMAP,
	MENU_CHARACTER,
	MENU_TOGGLE,
	MENU_STATS,
	MENU_NAVIGATION,
	MENU_HELP
};

enum /* menu item types */
{
	MENU_ITEM_NONE,
	MENU_ITEM_NORMAL,
	MENU_ITEM_SEPARATOR,
	MENU_ITEM_TOGGLE,
	MENU_ITEM_RADIO,
};

enum /* menu column positions */
{
	MENU_COLUMN_MUDPRO	   = 2,
	MENU_COLUMN_AUTOMAP    = 10,
	MENU_COLUMN_CHARACTER  = 19,
	MENU_COLUMN_TOGGLE     = 30,
	MENU_COLUMN_STATS      = 39,
	MENU_COLUMN_NAVIGATION = 51,
	MENU_COLUMN_HELP       = 63,
};

extern cwin_t menubar;

void menubar_init (void);
void menubar_cleanup (void);
void menubar_update (void);
void menubar_raise (void);
gboolean menubar_key_handler (gint ch);

#endif /* __MENUBAR_H__ */
