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

#ifndef __PARSE_H__
#define __PARSE_H__

#include <glib.h>
#include <pcre.h>

#define PARSE_SUBSTR_NUM	10
#define ASSIGNED_DIRECTION	100 /* offset for assigned direction */

typedef struct
{
	GSList *regexp_list; /* list of regexp_parse_t patterns/actions */
	GSList *db_list;     /* list of parse database files */
	GString *line_buf;   /* line buffered data from the server */
	GString *wrap_buf;   /* buffer to handle wrapped lines */
	GString *room_name;  /* room name buffer */
	gboolean line_wrap;  /* line wrapping flag */
} parse_t;

typedef struct
{
	gchar *tag;    /* user-friendly parse tag */
	gchar *regexp; /* substitution regexp */
} parse_tag_t;

typedef struct
{
	gchar *type; /* action type */
	gchar *arg;  /* optional action argument */
	gint value;  /* optional argument value */
} parse_action_t;

#define PARSE_VALUE_DIRECTION 100 /* parse_action_t value offsets */

typedef struct
{
	gchar *pattern;      /* original regexp pattern */
	pcre *compiled;      /* compiled regexp */
	pcre_extra *studied; /* studied regexp (optimized for speed) */
	GSList *actions;     /* actions to execute when pattern matched */
} parse_regexp_t;

extern parse_t parse;

void parse_init (void);
void parse_cleanup (void);
void parse_list_compile (void);
void parse_list_free (void);
void parse_db_update (void);
void parse_line_buffer (guchar ch);
void parse_line (gchar *line);

#endif /* __PARSE_H__ */
