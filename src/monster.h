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

#ifndef __MONSTER_H__
#define __MONSTER_H__

#include <glib.h>

/* find monsters current HP */
#define MON_HP(x) \
	(MAX (0, x - combat.damage.enemy - combat.damage.room))

typedef struct
{
	gchar *name;   /* name of monster */
	gchar *prefix; /* prefix if any */
	gint hp;       /* monster HP */
	gint flags;    /* monster flags */
} monster_t;

enum /* monster flags */
{
	MONSTER_FLAG_PRIO   = 1 << 0, /* give monster highest priority */
	MONSTER_FLAG_NOBS   = 1 << 1, /* do not attempt backstab */
	MONSTER_FLAG_UNDEAD = 1 << 2, /* monster is undead */
	MONSTER_FLAG_AVOID  = 1 << 3, /* avoid monster if possible */
};

enum
{
    TARGET_MODE_DEFAULT = 1 << 0, /* use default selection method (health) */
    TARGET_MODE_HEALTH  = 1 << 1, /* select targets by health */
    TARGET_MODE_LEVEL   = 1 << 2, /* select targets by level */
    TARGET_MODE_EXP     = 1 << 3, /* select targets by exp */
    TARGET_MODE_FORWARD = 1 << 4, /* select targets as they appear in ascending order */
    TARGET_MODE_REVERSE = 1 << 5, /* select targets as they appear in descending order */
};

void monster_db_init (void);
void monster_db_cleanup (void);
void monster_db_load (void);
monster_t *monster_lookup (const gchar *str, gchar **prefix);
void monster_target_list_build (gchar *str);
void monster_target_list_free (void);
void monster_target_add (monster_t *monster, const gchar *prefix);
monster_t *monster_target_get (void);
void monster_target_del (void);
monster_t *monster_target_lookup (monster_t *monster, const gchar *prefix);
void monster_target_override (monster_t *monster, gboolean re_engage);

#endif /* __MONSTER_H__ */
