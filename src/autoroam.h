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

#ifndef __AUTOROAM_H__
#define __AUTOROAM_H__

#include "defs.h"
#include "widgets.h"

typedef struct
{
	gboolean enabled;     /* roaming is enabled */
	gboolean roam_blind;  /* roam while blind */
	gboolean use_doors;   /* pass through doors */
	gboolean use_secrets; /* pass through secret exits */
	gulong exits;         /* selected exits */
} autoroam_opts_t;

extern cwin_t autoroam;
extern autoroam_opts_t autoroam_opts;

void autoroam_init (void);
void autoroam_cleanup (void);
void autoroam_report (FILE *fp);
void autoroam_map (void);
void autoroam_unmap (void);
void autoroam_update (void);
gboolean autoroam_key_handler (gint ch);

#endif /* __AUTOROAM_H__ */
