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

#ifndef __GUIDEBOOK_H__
#define __GUIDEBOOK_H__

#include "widgets.h"

#define GUIDEBOOK_LIST_SIZE		(guidebook.height - 5)
#define GUIDEBOOK_UPPER_SIZE	2
#define GUIDEBOOK_LOWER_SIZE	3

typedef struct
{
	gchar *name; /* location room name */
	gchar *id;   /* location ID */
} guidebook_record_t;

extern cwin_t guidebook;
extern gchar *guidebook_last_id;

void guidebook_init (void);
void guidebook_cleanup (void);
void guidebook_db_load (void);
void guidebook_db_save (void);
void guidebook_db_reset (void);
void guidebook_db_add (gchar *str, gchar *id);
void guidebook_db_del (guidebook_record_t *record);
void guidebook_map (void);
void guidebook_unmap (void);
void guidebook_update (void);
gboolean guidebook_key_handler (gint ch);

#endif /* __GUIDEBOOK_H__ */
