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

#include "character.h"
#include "combat.h"
#include "command.h"
#include "item.h"
#include "monster.h"
#include "mudpro.h"
#include "navigation.h"
#include "timers.h"
#include "terminal.h"
#include "utils.h"

combat_t combat;

static key_value_t strategy_types[] = {
	{ "NOOP",         STRATEGY_NOOP },
	{ "Backstab",     STRATEGY_BACKSTAB },
	{ "WeaponAttack", STRATEGY_WEAPON_ATTACK },
	{ "SpellAttack",  STRATEGY_SPELL_ATTACK },
	{ "Break",        STRATEGY_BREAK },
	{ "Punch",        STRATEGY_PUNCH },
	{ "Jumpkick",     STRATEGY_JUMPKICK },
	{ NULL, 0 }
};

static key_value_t criteria_types[] = {
	{ "None",    CRITERIA_NONE },
	{ "Highest", CRITERIA_HIGHEST },
	{ "Lowest",  CRITERIA_LOWEST },
	{ "Average", CRITERIA_AVERAGE },
	{ NULL, 0 }
};

combat_t combat;
strategy_t default_strategy;

static void combat_strategy_parse_options (strategy_t *strategy, gchar *str);
static gint get_strategy_type (gchar *str);
static gint get_criteria_type (gchar *str);


/* =========================================================================
 = COMBAT_INIT
 =
 = Initialize the combat module
 ======================================================================== */

void combat_init (void)
{
	memset (&combat, 0, sizeof (combat_t));
	memset (&default_strategy, 0, sizeof (strategy_t));

	default_strategy.type   = STRATEGY_WEAPON_ATTACK;
	default_strategy.rounds = 9999;

	mudpro_db.strategy.filename = g_strdup_printf (
		"%s%cstrategy.db", character.data_path, G_DIR_SEPARATOR);

	combat_strategy_list_load ();
}


/* =========================================================================
 = COMBAT_CLEANUP
 =
 = Cleanup after combat module
 ======================================================================== */

void combat_cleanup (void)
{
	combat_strategy_list_free ();
	g_free (mudpro_db.strategy.filename);
}


/* =========================================================================
 = COMBAT_REPORT
 =
 = Report current status of combat module to the specified file
 ======================================================================== */

void combat_report (FILE *fp)
{
	monster_t *monster = monster_target_get ();

	fprintf (fp, "\nCOMBAT MODULE\n"
				 "=============\n\n");

	fprintf (fp, "  Target ....... %s\n", monster ? monster->name : "None");
	fprintf (fp, "  Rounds ....... %d\n", combat.rounds);
	fprintf (fp, "  Sync Flag .... %s\n", combat.sync ? "TRUE" : "FALSE");
	fprintf (fp, "  Force Break .. %s\n\n", combat.force_break ? "TRUE" : "FALSE");

	fprintf (fp, "  Monster Tracking:\n\n");
	fprintf (fp, "    Count ...... %d\n", combat.monster.count);
	fprintf (fp, "    Strongest .. %d\n", combat.monster.strongest);
	fprintf (fp, "    Weakest .... %d\n", combat.monster.weakest);
	fprintf (fp, "    Average .... %d\n\n", combat.monster.average);

	fprintf (fp, "  Damage Tracking:\n\n");
	fprintf (fp, "    To enemy .. %d\n", combat.damage.enemy);
	fprintf (fp, "    To room ... %d\n", combat.damage.room);
	fprintf (fp, "    Received .. %d\n\n", combat.damage.received);

	if (combat.strategy.current)
	{
		strategy_t *current = combat.strategy.current;

		fprintf (fp, "  Current Strategy: %s\n\n", strategy_types[current->type].key);
		fprintf (fp, "    Min Mon HP .... %d\n", current->min.mon_hp);
		fprintf (fp, "    Min Monsters .. %d\n", current->min.monsters);
		fprintf (fp, "    Min Mana ...... %d\n", current->min.mana);
		fprintf (fp, "    Min Tick ...... %d\n\n", current->min.tick);
		fprintf (fp, "    Target ........ %s\n",
			current->target ? current->target : "Any");
		fprintf (fp, "    Weapon ........ %s\n",
			current->weapon ? current->weapon : "Any");
		fprintf (fp, "    Spell ......... %s\n",
			current->spell ?  current->spell : "None");
		fprintf (fp, "    Rounds ........ %d\n", current->rounds);
		fprintf (fp, "    Criteria ...... %s\n", criteria_types[current->criteria].key);
		fprintf (fp, "    Using Bash .... %s\n", current->bash ? "TRUE" : "FALSE");
		fprintf (fp, "    Using Smash ... %s\n", current->smash ? "TRUE" : "FALSE");
		fprintf (fp, "    Affects Room .. %s\n\n", current->room ? "TRUE" : "FALSE");
	}
	else
		fprintf (fp, "  Current Strategy: None\n\n");
}


/* =========================================================================
 = COMBAT_RESET
 =
 = Resets current combat data
 ======================================================================== */

void combat_reset (void)
{
	if (combat.force_break || combat.room)
		command_send (CMD_BREAK);

	combat.strategy.current = NULL;
	combat.sync        = FALSE;
	combat.force_break = FALSE;
	combat.room        = FALSE;
	combat.rounds = 0;

	memset (&combat.damage, 0, sizeof (combat.damage));
	memset (&combat.monster, 0, sizeof (combat.monster));
}


/* =========================================================================
 = COMBAT_SYNC
 =
 = Syncronize combat rounds
 ======================================================================== */

void combat_sync (void)
{
	if (combat.sync && (timer_elapsed (timers.round, NULL) < 3))
		return;

	timer_reset (timers.round);
	combat.sync = TRUE;
	combat.rounds++;
}


/* =========================================================================
 = COMBAT_MONSTERS_UPDATE
 =
 = Updates combat monster tracking data
 ======================================================================== */

void combat_monsters_update (void)
{
	GSList *node;
	monster_t *monster;
	gulong total = 0;

	memset (&combat.monster, 0, sizeof (combat.monster));

	for (node = character.targets; node; node = node->next)
	{
		monster = node->data;
		g_assert (monster != NULL);

		total += monster->hp;

		combat.monster.strongest = MAX
			(combat.monster.strongest, monster->hp);

		if (combat.monster.weakest)
			combat.monster.weakest = MIN
				(combat.monster.weakest, monster->hp);
		else
			combat.monster.weakest = combat.monster.strongest;

		combat.monster.average = total /
			++combat.monster.count;
	}
}


/* =========================================================================
 = COMBAT_STRATEGY_LIST_LOAD
 =
 = (Re)load list of strategies from file
 ======================================================================== */

void combat_strategy_list_load (void)
{
	gchar buf[STD_STRBUF];
	gchar *offset, *tmp;
	strategy_t *strategy = NULL;
	FILE *fp;

	if ((fp = fopen (mudpro_db.strategy.filename, "r")) == NULL)
	{
		printt ("Unable to open spell database");
		return;
	}

	g_get_current_time (&mudpro_db.strategy.access);

	if (combat.strategy.list != NULL)
		combat_strategy_list_free ();

	while (fgets (buf, sizeof (buf), fp))
	{
		strchomp (buf);

		if (buf[0] == '#' || buf[0] == '\0')
		{
			strategy = NULL;
			continue;
		}

		if (isspace (buf[0]))
		{
			if (strategy != NULL)
				combat_strategy_parse_options (strategy, buf);
			continue;
		}

		strategy = g_malloc0 (sizeof (strategy_t));

		offset = buf;
		tmp = get_token_as_str (&offset);
		strategy->type = get_strategy_type (tmp);
		strategy->rounds = 9999; /* set default */
		g_free (tmp);

		if (!strategy->type)
		{
			g_free (strategy);
			strategy = NULL;
			continue;
		}

		if (strategy->type == STRATEGY_SPELL_ATTACK)
			strategy->spell = get_token_as_str (&offset);

		combat.strategy.list = g_slist_append (combat.strategy.list, strategy);
	}

	fclose (fp);
}


/* =========================================================================
 = COMBAT_STRATEGY_LIST_FREE
 =
 = Free data allocated to strategy list
 ======================================================================== */

void combat_strategy_list_free (void)
{
	GSList *node;
	strategy_t *strategy;

	for (node = combat.strategy.list; node; node = node->next)
	{
		strategy = node->data;
		g_assert (strategy != NULL);

		g_free (strategy->target);
		g_free (strategy->weapon);
		g_free (strategy->spell);
		g_free (strategy);
	}

	g_slist_free (combat.strategy.list);
	combat.strategy.list    = NULL;
	combat.strategy.current = NULL;
}


/* =========================================================================
 = COMBAT_STRATEGY_PARSE_OPTIONS
 =
 = Read strategy option data
 ======================================================================== */

static void combat_strategy_parse_options (strategy_t *strategy, gchar *str)
{
	gchar *offset, *option;
	gulong value;

	g_assert (strategy != NULL);
	g_assert (str != NULL);

	offset = str;
	option = get_token_as_str (&offset);

	if (!strcasecmp (option, "Bash"))
	{
		value = get_token_as_long (&offset);
		strategy->bash = CLAMP (value, 0, 1);
	}
	else if (!strcasecmp (option, "Criteria"))
	{
		gchar *tmp = get_token_as_str (&offset);
		strategy->criteria = get_criteria_type (tmp);
	}
	else if (!strcasecmp (option, "Monsters"))
	{
		value = get_token_as_long (&offset);
		strategy->min.monsters = MAX (0, value);
	}
	else if (!strcasecmp (option, "NPCNotPresent"))
	{
        value = get_token_as_long (&offset);
        strategy->npc_not_present = CLAMP (value, 0, 1);
	}
	else if (!strcasecmp (option, "PCNotPresent"))
	{
        value = get_token_as_long (&offset);
        strategy->pc_not_present = CLAMP (value, 0, 1);
	}
	else if (!strcasecmp (option, "ReqMana"))
	{
		value = get_token_as_long (&offset);
		strategy->min.mana = CLAMP (value, 0, 100);
	}
	else if (!strcasecmp (option, "ReqMonHP"))
	{
		value = get_token_as_long (&offset);
		strategy->min.mon_hp = MAX (0, value);
	}
	else if (!strcasecmp (option, "ReqTick"))
	{
		value = get_token_as_long (&offset);
		strategy->min.tick = MAX (0, value);
	}
	else if (!strcasecmp (option, "RoomSpell"))
	{
		value = get_token_as_long (&offset);
		strategy->room = CLAMP (value, 0, 1);
	}
	else if (!strcasecmp (option, "Rounds"))
	{
		value = get_token_as_long (&offset);
		strategy->rounds = MAX (0, value);
	}
	else if (!strcasecmp (option, "Smash"))
	{
		value = get_token_as_long (&offset);
		strategy->smash = MAX (0, value);
	}
	else if (!strcasecmp (option, "Target"))
	{
		strategy->target = get_token_as_str (&offset);
	}
	else if (!strcasecmp (option, "Weapon"))
	{
		strategy->weapon = get_token_as_str (&offset);
	}

	g_free (option);
}


/* =========================================================================
 = COMBAT_STRATEGY_GET_NEXT
 =
 = Returns the combat strategy within current thresholds
 ======================================================================== */

strategy_t *combat_strategy_get_next (void)
{
	GSList *node;
	strategy_t *strategy;
	monster_t *target;

	target = monster_target_get ();
	g_assert (target != NULL);
	g_assert (target->name != NULL);

	node = (combat.strategy.current) ?
		g_slist_find (combat.strategy.list, combat.strategy.current) :
		combat.strategy.list;

	for (; node; node = node->next)
	{
		strategy = node->data;

		/* attempt to disqualify strategy */

        if (strategy->pc_not_present && navigation.pc_present)
            continue; /* Player is present */

		if (strategy->npc_not_present && navigation.npc_present)
            continue; /* NPC is present */

		if (strategy->target && strcasecmp (target->name, strategy->target))
			continue; /* target defined, but doesn't match current target */

		if (strategy->weapon &&
			!item_list_lookup (character.inventory, strategy->weapon))
			continue; /* weapon not in inventory */

		if (combat.rounds >= strategy->rounds)
			continue; /* round limit exceeded */

		if (combat.monster.count < strategy->min.monsters)
			continue; /* not enough monsters */

		if (get_percent (character.ma.now, character.ma.max) < strategy->min.mana)
			continue; /* not enough mana */

		if (strategy->type == STRATEGY_BACKSTAB &&
			(!character.flag.sneaking || (target->flags & MONSTER_FLAG_NOBS)))
			continue; /* cannot backstab monster */

		if (character.tick.current < strategy->min.tick)
			continue; /* mana tick too low */

		switch (strategy->criteria)
		{
		case CRITERIA_HIGHEST:
			if (MON_HP (combat.monster.strongest) < strategy->min.mon_hp)
				continue;
			break;
		case CRITERIA_LOWEST:
			if (MON_HP (combat.monster.weakest) < strategy->min.mon_hp)
				continue;
			break;
		case CRITERIA_AVERAGE:
			if (MON_HP (combat.monster.average) < strategy->min.mon_hp)
				continue;
			break;
		default:
			/* only consider the current target */
			if (MON_HP (target->hp) < strategy->min.mon_hp)
				continue;
		}

		return strategy; /* looks like we got a winner */
	}

	/* when all else fails, use default strategy */
	return &default_strategy;
}


/* =========================================================================
 = COMBAT_STRATEGY_EXECUTE
 =
 = Executes the specified strategy
 ======================================================================== */

void combat_strategy_execute (strategy_t *strategy)
{
	item_t *item;
	monster_t *monster;
	gchar *target;

	g_assert (strategy != NULL);

	monster = monster_target_get ();
	g_assert (monster != NULL);

	combat.strategy.current = strategy;
	combat.room = strategy->room;

	if (strategy->target)
		target = g_strdup (strategy->target);
	else if (monster->prefix)
		target = g_strdup_printf ("%s%s", monster->prefix, monster->name);
	else
		target = g_strdup (monster->name);

	if (strategy->weapon &&
		(item = item_list_lookup (character.inventory, strategy->weapon)))
	{
		/* arm specified weapon while for this strategy */
		if (!item->armed && (item->equip & EQUIP_FLAG_WEAPON))
			command_send_va (CMD_ARM, "%s", strategy->weapon);
	}

	switch (strategy->type)
	{
	case STRATEGY_BACKSTAB:
		command_send_va (CMD_ATTACK, "bs %s", target);
		break;
	case STRATEGY_WEAPON_ATTACK:
		if (strategy->smash)
			command_send_va (CMD_ATTACK, "sma %s", target);
		else if (strategy->bash)
			command_send_va (CMD_ATTACK, "aa %s", target);
		else
			command_send_va (CMD_ATTACK, "a %s", target);
		break;
	case STRATEGY_SPELL_ATTACK:
		if (strategy->room)
			command_send_va (CMD_ATTACK, "%s", strategy->spell);
		else
			command_send_va (CMD_ATTACK, "%s %s", strategy->spell, target);
		break;
	case STRATEGY_BREAK:
		/* FIXME: need to verify this works OK */
		command_send (CMD_BREAK);
		break;
	case STRATEGY_PUNCH:
		command_send_va (CMD_ATTACK, "pu %s", target);
		break;
	case STRATEGY_JUMPKICK:
		command_send_va (CMD_ATTACK, "ju %s", target);
		break;
	default:
		g_assert_not_reached ();
	}

	g_free (target);
}


/* =========================================================================
 = GET_STRATEGY_TYPE
 =
 = Returns strategy type as int
 ======================================================================== */

static gint get_strategy_type (gchar *str)
{
	key_value_t *st;

	for (st = strategy_types; st->key; st++)
	{
		if (!strcasecmp (str, st->key))
			return st->value;
	}

	return STRATEGY_NOOP; /* invalid strategy, do nothing */
}


/* =========================================================================
 = GET_CRITERIA_TYPE
 =
 = Returns the criteria type as int
 ======================================================================== */

static gint get_criteria_type (gchar *str)
{
	key_value_t *ct;

	for (ct = criteria_types; ct->key; ct++)
	{
		if (!strcasecmp (str, ct->key))
			return ct->value;
	}

	return CRITERIA_NONE; /* unknown criteria */
}
