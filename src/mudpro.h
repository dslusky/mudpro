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

#ifndef __MUDPRO_H__
#define __MUDPRO_H__

#include "defs.h"
#include "widgets.h"

typedef struct /* command-line args */
{
	gchar *hostname;   /* hostname to use (overrides profile) */
	gchar *profile;    /* path to character profile */
	gint port;         /* remote port to use (overrides profile) */
	gint line_style;   /* line style to use */
	gboolean connect;  /* connect at startup */
	gboolean no_poll;  /* disable config polling */
	gboolean capture;  /* begin capturing the session immediately */
} args_t;

typedef struct /* database info */
{
	db_t automap;
	db_t strategy;
	db_t guidebook;
	db_t items;
	db_t monsters;
	db_t players;
	db_t profile;
	db_t spells;
} mudpro_db_t;

extern args_t args;
extern mudpro_db_t mudpro_db;
extern gboolean capture_active;
extern gboolean notice_visible;

void mudpro_create_report_log (void);
void mudpro_capture_log_toggle (void);
void mudpro_capture_log_append (const gchar *str);
void mudpro_audit_log_append (gchar *fmt, ...);
void mudpro_reset_state (gboolean disconnected);
void mudpro_load_data (void);
void mudpro_save_data (void);
void mudpro_connect (void);
void mudpro_disconnect (void);
void temporary_chatlog_append (gchar *str);
void mudpro_redraw_display (void);
void mudpro_shutdown (void);

#endif /* __MUDPRO_H__ */
