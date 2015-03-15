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

#ifndef __DEFS_H__
#define __DEFS_H__

#include <curses.h>
#include <glib.h>
#include <panel.h>
#include <time.h>
#include <stdio.h>

typedef struct /* general key/value pair */
{
	gchar *key;
	gint value;
} key_value_t;

typedef struct /* db access tracking */
{
	gchar *filename;
	GTimeVal access;
} db_t;

#define RELEASE "0.91q (05-02-2004)"

#define STD_STRBUF 1024

#define FlagON(x, y)		(x |= y)
#define FlagOFF(x, y)		(x &= ~y)
#define Toggle(x, y)		(x ^= y)

#endif /* __DEFS_H__ */
