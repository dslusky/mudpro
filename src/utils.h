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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <glib.h>

#include "defs.h"
#include "widgets.h"


gchar *substr (const gchar *pos1, const gchar *pos2);
void strchomp (gchar *str);
gint convert_health (gint percent);
gint convert_mana (gint percent);
gint get_percent (gint x, gint y);
void update_display (void);
void send_line (gchar *str);
void send_line_va (gchar *fmt, ...);
void send_raw (gchar *str);
GString *commified_g_string(gint value);
GString *capitalized_g_string (gchar *str);
GString *get_time_as_g_string (time_t *t);
GString *get_elapsed_as_g_string (gulong elapsed);
GString *get_obvious_exits_as_g_string (void);
glong get_token_as_long (gchar **offset);
gchar *get_token_as_str (gchar **offset);
gint get_key_value_by_index (key_value_t *table, gint index);
gint po2 (gint value);
void border_draw_bracket (cwin_t *cwin, gint type);
void db_deallocate (gpointer data, gpointer user_data);

#endif /* __UTILS_H__ */
