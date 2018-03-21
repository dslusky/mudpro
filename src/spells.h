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

#ifndef __SPELLS_H__
#define __SPELLS_H__

#include <glib.h>

#define SPELL_FLAG(x)	(spell->flags & x)

typedef struct
{
	gchar *name;     /* spell name */
	gint tick;       /* recast when mana tick below this mark */
	gint duration;   /* max duration for spell */
	gulong flags;    /* misc spell flags */
	glong lastcast;  /* timestamp of last cast */
	gboolean active; /* spell currently active */

	struct /* beginning/ending spell messages */
	{
		gchar *start;
		gchar *end;
	} msg;

	struct
	{
		gint thresh;      /* HP/MA casting threshold */
		gint test;        /* test to perform (eg: < > = !) */
		gboolean percent; /* treat value as percentage */
	} hp, ma;
} spell_t;

enum /* spell flags */
{
	SPELL_FLAG_HEAL			= 1 << 0, /* restores health */
	SPELL_FLAG_ANTIDOTE		= 1 << 1, /* cures poison */
	SPELL_FLAG_CDISEASE     = 1 << 2, /* cures disease */
	SPELL_FLAG_LIGHT        = 1 << 3, /* provides light */
	SPELL_FLAG_VISION		= 1 << 4, /* restores vision */
	SPELL_FLAG_MOVEMENT		= 1 << 5, /* restores movement */
	SPELL_FLAG_INT_ATTACK	= 1 << 6, /* interrupts attacking */
	SPELL_FLAG_INT_REST		= 1 << 7, /* interrupts resting */
	SPELL_FLAG_INT_MEDITATE = 1 << 8, /* interrupts meditation */
	SPELL_FLAG_INT_SNEAK	= 1 << 9, /* interrupts sneaking */
};

void spell_db_init (void);
void spell_db_cleanup (void);
void spell_db_load (void);
void spell_db_parse (gchar *str);
gboolean spellcasting (void);

#endif /* __SPELLS_H__ */
