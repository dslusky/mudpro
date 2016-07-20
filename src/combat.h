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

#ifndef __COMBAT_H__
#define __COMBAT_H__

#include "defs.h"

typedef struct
{
	gchar *target;  /* specific target */
	gchar *weapon;  /* weapon to use */
	gchar *spell;   /* spell name */
	gint type;      /* type of strategy */
	gint rounds;    /* rounds to use strategy */
	gint criteria;  /* monster HP comparison method */
	gboolean bash;  /* use bash when attacking */
	gboolean smash; /* use smash when attacking (overrides bash) */
	gboolean room;  /* strategy affects whole room */

    gboolean pc_not_present;  /* no players are present */
	gboolean npc_not_present; /* no NPCs are present */

	struct /* strategy minimum requirements */
	{
		gint mon_hp;   /* monster HP */
		gint monsters; /* monsters present */
		gint mana;     /* player MA */
		gint tick;     /* player MA tick */
	} min;
} strategy_t;

typedef struct /* combat tracking */
{
	guint rounds;         /* duration in rounds (for current strategy) */
	gboolean sync;        /* round sync flag */
	gboolean force_break; /* force break on reset */
	gboolean room;        /* combat affects whole room */

	struct /* strategy list and current position */
	{
		GSList *list;
		strategy_t *current;
	} strategy;

	struct /* damage tracking */
	{
		gint enemy;
		gint room;
		gint received;
	} damage;

	struct /* monster tracking */
	{
		gint count;
		gint strongest;
		gint weakest;
		gint average;
	} monster;
} combat_t;

enum /* strategy types */
{
	STRATEGY_NOOP,
	STRATEGY_BACKSTAB,
	STRATEGY_WEAPON_ATTACK,
	STRATEGY_SPELL_ATTACK,
	STRATEGY_BREAK,
	STRATEGY_PUNCH,
	STRATEGY_JUMPKICK
};

enum /* criteria types */
{
	CRITERIA_NONE,
	CRITERIA_HIGHEST,
	CRITERIA_LOWEST,
	CRITERIA_AVERAGE
};

extern combat_t combat;
extern strategy_t default_strategy;

void combat_init (void);
void combat_cleanup (void);
void combat_report (FILE *fp);
void combat_reset (void);
void combat_sync (void);
void combat_monsters_update (void);
void combat_strategy_list_load (void);
void combat_strategy_list_free (void);
strategy_t *combat_strategy_get_next (void);
void combat_strategy_execute (strategy_t *strategy);

#endif /* __COMBAT_H__ */
