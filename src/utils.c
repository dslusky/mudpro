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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <curses.h>
#include <panel.h>

#include "automap.h"
#include "character.h"
#include "defs.h"
#include "menubar.h"
#include "mudpro.h"
#include "sock.h"
#include "sockbuf.h"
#include "terminal.h"
#include "utils.h"

#define ACTION_AREA(x)	(x->height-3)


/* =========================================================================
 = SUBSTR
 =
 = Allocates substring from given positions
 ======================================================================== */

gchar *substr (const gchar *pos1, const gchar *pos2)
{
	gchar *tmp;

	g_assert (pos1 != NULL);
	g_assert (pos2 != NULL);
	g_assert (pos1 <= pos2);

	tmp = g_strdup (pos1);
	tmp[pos2 - pos1] = '\0';

	return tmp;
}


/* =========================================================================
 = STRCHOMP
 =
 = Removes trailing CR/LF from string
 ======================================================================== */

void strchomp (gchar *str)
{
	gchar *p;

	if (!str) return;

	if ((p = index (str, '\r')) != NULL) *p = '\0';
	if ((p = index (str, '\n')) != NULL) *p = '\0';
}


/* =========================================================================
 = CONVERT_HEALTH
 =
 = Find health value from percentage
 ======================================================================== */

gint convert_health (gint percent)
{
	return (gint) (character.hp.max * 1.0) * (percent * 0.01);
}


/* =========================================================================
 = CONVERT_MANA
 =
 = Find mana value from percentage
 ======================================================================== */

gint convert_mana (gint percent)
{
	return (gint) (character.ma.max * 1.0) * (percent * 0.01);
}


/* ==========================================================================
 = GET_PERCENT
 =
 = Find the percent of two numbers
 ========================================================================= */

gint get_percent(gint x, gint y)
{
	if (y) return ((x * 1.0) / (y * 1.0)) * 100;
	return 0;
}


/* =========================================================================
 = UPDATE_DISPLAY
 =
 = Refresh all windows
 ======================================================================== */

void update_display (void)
{
	gint y, x;

	update_panels ();
	getyx (terminal.w, y, x);
	setsyx (MENUBAR_OFFSET (y), x);
	/* doupdate (); */
}


/* =========================================================================
 = SEND_LINE
 =
 = Sends message to server with CR/LF
 ======================================================================== */

void send_line (gchar *str)
{
	gchar *buf = g_strdup_printf ("%s\r\n", str);
	putSockN (buf, strlen (buf));
	g_free (buf);
}


/* =========================================================================
 = SEND_LINE_VA
 =
 = Sends message to server with CR/LF, supports variable argument list
 ======================================================================== */

void send_line_va (gchar *fmt, ...)
{
	va_list argptr;
	gchar buf[STD_STRBUF];

	va_start (argptr, fmt);
	vsnprintf (buf, sizeof (buf), fmt, argptr);
	va_end (argptr);

	send_line (buf);
}


/* =========================================================================
 = SEND_RAW
 =
 = Sends message to server without CR/LF
 ======================================================================== */

void send_raw (gchar *str)
{
	putSockN (str, strlen (str));
}


/* ==========================================================================
 = COMMIFIED_GSTRING
 =
 = Convert integer into commified GString (must be g_string_free'd)
 ========================================================================= */

GString *commified_g_string (gint value)
{
	GString *str = g_string_new ("");
	gint pos;

	g_string_sprintf (str, "%d", value);

	if (str->len < 4)
		return str;

	pos = ((str->len - 1) % 3) + 1;
	while (pos < str->len)
	{
		str = g_string_insert_c (str, pos, ',');
		pos += 4; /* 3 + inserted character */
    }

	return str;
}


/* ==========================================================================
 = CAPTIALIZED_GSTRING
 =
 = Capitalizes all words in a GString
 ========================================================================= */

GString *capitalized_g_string (gchar *str)
{
	GString *tmp;
	gchar *pos = str;

	g_assert (pos != NULL);

	tmp = g_string_new ("");
	tmp = g_string_append_c (tmp, toupper (*pos));
	pos++;

	while (*pos)
	{
		if (*(pos-1) == ' ')
			tmp = g_string_append_c (tmp, toupper (*pos));
		else
			tmp = g_string_append_c (tmp, *pos);

		pos++;
	}

	return tmp;
}


/* ==========================================================================
 = GET_TIME_AS_G_STRING
 =
 = Return GString of the time_t value (current time if NULL), must be free'd
 ========================================================================= */

GString *get_time_as_g_string (time_t *t)
{
	GString *s;
	time_t gmtime;

	time (&gmtime);

	s = g_string_new (ctime (t ? t : &gmtime));
	s = g_string_erase (s, 0, 11);
	s = g_string_truncate (s, 8);

	return s;
}


/* =========================================================================
 = GET_ELAPSED_AS_G_STRING
 =
 = Return GString of elapsed time as HH:MM:SS, must be free'd
 ======================================================================== */

GString *get_elapsed_as_g_string (gulong elapsed)
{
	GString *s;
	gint hours, mins, secs;

	hours = mins = secs = 0;

	if ((hours = elapsed / 3600))
		elapsed -= hours * 3600;

	if ((mins = elapsed / 60))
		elapsed -= mins * 60;

	secs = elapsed;

	s = g_string_new ("");
	g_string_sprintf (s, "%02d:%02d:%02d", hours, mins, secs);

	return s;
}


/* =========================================================================
 = GET_OBVIOUS_EXITS_AS_G_STRING
 =
 = Returns obvious exits in g_string format (must be free'd)
 ======================================================================== */

GString *get_obvious_exits_as_g_string (void)
{
	exit_table_t *et;
	GString *buf = g_string_new ("");

	for (et = exit_table; et->short_str; et++)
	{
		if ((VISIBLE_EXITS | automap.obvious.secrets) & et->direction)
		{
			buf = g_string_append (buf, MOVEMENT_STR (et->direction));
			buf = g_string_append_c (buf, ',');
		}
	}

	buf = g_string_truncate (buf, MAX (0, buf->len - 1));
	buf = g_string_up (buf);

	return buf;
}


/* =========================================================================
 = GET_TOKEN_AS_INT
 =
 = Reads token from string and returns its value as an int
 ======================================================================== */

glong get_token_as_long (gchar **offset)
{
	gchar *str;
	glong value;

	if ((str = get_token_as_str (offset)) != NULL)
	{
		if (!strcasecmp (str, "true"))       value = TRUE;
		else if (!strcasecmp (str, "false")) value = FALSE;
		else
			value = atol (str);
		g_free (str);
	}
	else
		value = 0;

	return value;
}


/* =========================================================================
 = GET_TOKEN_AS_STR
 =
 = Extracts the next token from string
 ======================================================================== */

gchar *get_token_as_str (gchar **offset)
{
	gchar *value = NULL;
	gchar *pos1, *pos2;

	pos1 = pos2 = *offset;

	if (isspace (*pos1))
	{
		while (isspace (*pos1) && *pos1 != '\0') pos1++;
		pos2 = pos1;
	}

	if (*pos1 == ':' ||
		*pos1 == '=' ||
		*pos1 == ',' ||
		*pos1 == '.' ||
		*pos1 == '\0')
		return NULL; /* nothing to read */

	if (*pos1 == '"')
	{
		pos1 = ++pos2;
		while (*pos2 != '"' && *pos2 != '\0') pos2++;

		if (*pos2 == '"')
		{
			value = substr (pos1, pos2);
			pos1 = ++pos2;
		}
		else
			return NULL;
	}

	while (*pos2 != ':' &&
		   *pos2 != '=' &&
		   *pos2 != ',' &&
		   *pos2 != '.' &&
		   *pos2 != '\0')
			pos2++;

	if (value == NULL)
	{
		value = substr (pos1, pos2);

		/* remove trailing spaces */
		pos1 = value + strlen (value) - 1;
		while (isspace (*pos1)) *(pos1--) = '\0';
	}

	if (*pos2 == ':' ||
		*pos2 == '=' ||
		*pos2 == ',' ||
		*pos2 == '.')
		pos1 = ++pos2;

	*offset = pos2;

	return value;
}


/* =========================================================================
 = GET_KEY_VALUE_BY_INDEX
 =
 = Returns the value of the key/value by index
 ======================================================================== */

gint get_key_value_by_index (key_value_t *table, gint index)
{
	gint pos = 0;

	g_assert (table != NULL);

	for (; table->key; table++)
	{
		if (pos++ == index)
			return table->value;
	}
	return 0;
}


/* =========================================================================
 = PO2
 =
 = Returns exponent for 2ü
 ======================================================================== */

gint po2 (gint value)
{
	gint i;
	gint table[20] = {
		0,     2,      4,      8,
		16,    32,     64,     128,
		256,   512,    1024,   2048,
		4096,  8192,   16384,  32768,
		65536, 131072, 262144, 524288
	};

	for (i = 1; i < 20; i++)
		if (table[i] == value)
			return i;

	return 0;
}


/* =========================================================================
 = BORDER_DRAW_BRACKET
 =
 = Draws a bracket with border attributes
 ======================================================================== */

void border_draw_bracket (cwin_t *cwin, gint type)
{
	g_assert (cwin != NULL);
	g_assert (cwin->w != NULL);

	switch (type)
	{
	case ']':
		wattrset (cwin->w, ATTR_BORDER | A_BOLD);
		waddch (cwin->w, type);
		break;
	case '[':
		wattrset (cwin->w, ATTR_BORDER | A_BOLD);
		mvwaddch (cwin->w, 0, 3, type);
		break;
	default:
		g_assert_not_reached ();
	}
}


/* =========================================================================
 = DB_DEALLOCATE
 =
 = Free memory allocated to db_t record
 ======================================================================== */

void db_deallocate (gpointer data, gpointer user_data)
{
	db_t *db = data;

	g_assert (db != NULL);
	g_free (db->filename);
	g_free (db);
}
