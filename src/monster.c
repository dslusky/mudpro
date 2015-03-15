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

#include "autoroam.h"
#include "character.h"
#include "combat.h"
#include "command.h"
#include "monster.h"
#include "mudpro.h"
#include "navigation.h"
#include "osd.h"
#include "timers.h"
#include "terminal.h"
#include "utils.h"

static GHashTable *monster_db;

static gchar *mob_prefix[] = {
	"happy ",  "angry ",  "fierce ",
	"thin ",   "small ",  "large ",
	"big ",    "fat ",    "short ",
	"tall ",   "nasty ",  "slim ",
	NULL
};

static void monster_db_free (void);
static void monster_record_deallocate (gpointer key, gpointer value, gpointer user_data);
static gint monster_target_list_prioritize (gconstpointer a, gconstpointer b);


/* =========================================================================
 = MONSTER_DB_INIT
 =
 = Initialize monster database
 ======================================================================== */

void monster_db_init (void)
{
	mudpro_db.monsters.filename = g_strdup_printf (
		"%s%cmonsters.db", character.data_path, G_DIR_SEPARATOR);

	monster_db = NULL;
	monster_db_load ();
}


/* =========================================================================
 = MONSTER_DB_CLEANUP
 =
 = Cleanup after monster database
 ======================================================================== */

void monster_db_cleanup (void)
{
	monster_db_free ();
	monster_target_list_free ();

	g_free (mudpro_db.monsters.filename);
}


/* =========================================================================
 = MONSTER_DB_LOAD
 =
 = (Re)load monster database from file
 ======================================================================== */

void monster_db_load (void)
{
	gchar buf[STD_STRBUF];
	gchar *offset;
	monster_t *monster;
	FILE *fp;

	if ((fp = fopen (mudpro_db.monsters.filename, "r")) == NULL)
	{
		printt ("Unable to open monster database");
		return;
	}

	g_get_current_time (&mudpro_db.monsters.access);

	if (character.targets)
		monster_target_list_free ();

	if (monster_db != NULL)
		monster_db_free ();

	monster_db = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_freeze (monster_db);

	while (fgets (buf, sizeof (buf), fp))
	{
		strchomp (buf);

		if (buf[0] == '#' || buf[0] == '\0')
			continue;

		monster = g_malloc0 (sizeof (monster_t));

		offset = buf;
		monster->name  = get_token_as_str (&offset);
		monster->hp    = get_token_as_long (&offset);
		monster->flags = get_token_as_long (&offset);

		g_hash_table_insert (monster_db, monster->name, monster);
	}

	g_hash_table_thaw (monster_db);

	fclose (fp);
}


/* =========================================================================
 = MONSTER_DB_FREE
 =
 = Free memory allocated to monster database
 ======================================================================== */

static void monster_db_free (void)
{
	g_hash_table_foreach (monster_db, monster_record_deallocate, NULL);
	g_hash_table_destroy (monster_db);
	monster_db = NULL;
}


/* =========================================================================
 = MONSTER_RECORD_DEALLOCATE
 =
 = Free memory allocated to monster record
 ======================================================================== */

static void monster_record_deallocate (gpointer key, gpointer value,
	gpointer user_data)
{
	g_free (key); /* monster->name */
	g_free (value);
}


/* =========================================================================
 = MONSTER_LOOKUP
 =
 = Find monster in database, also return prefix
 ======================================================================== */

monster_t *monster_lookup (const gchar *str, gchar **prefix)
{
	gint len, pos;
	gchar buf[STD_STRBUF];

	snprintf (buf, sizeof (buf), "%s", str);
	g_strdown (buf);
	str = buf;

	if (!strncasecmp (str, "a ", 2))		str += 2;
	else if (!strncasecmp (str, "an ", 3))	str += 3;

	pos = 0;
	while (mob_prefix[pos] != NULL)
	{
		len = strlen (mob_prefix[pos]);

		if (!strncmp (str, mob_prefix[pos], len))
		{
			if (prefix) *prefix = mob_prefix[pos];
			str += len;
			break;
		}
		else
			pos++;
	}

	return g_hash_table_lookup (monster_db, str);
}


/* =========================================================================
 = MONSTER_TARGET_LIST_BUILD
 =
 = Parse string and add monsters to target list
 ======================================================================== */

void monster_target_list_build (gchar *str)
{
	monster_t *monster;
	monster_t *monster_avoid = NULL;
	player_t *player;
	gchar *offset, *tmp, *prefix;

	if (character.targets)
		monster_target_list_free ();

	character.flag.double_back = FALSE;

	offset = str + strlen ("Also here:");

	while ((tmp = get_token_as_str (&offset)) != NULL)
	{
		prefix = NULL;

		if ((monster = monster_lookup (tmp, &prefix)) != NULL)
		{
			if ((monster->flags & MONSTER_FLAG_AVOID) && AUTO_MOVEMENT)
			{
				character.flag.avoid = TRUE;
				monster_avoid = monster;
			}
			monster_target_add (monster, prefix);
		}
		else if ((player = player_db_lookup (tmp)) != NULL)
		{
			/* TODO: support player relations avoid, hangup, etc.. */
			
			if ((player->flags & PLAYER_FLAG_INVITE) &&
				!(player->party_flags & PARTY_FLAG_INVITED) &&
				!(player->party_flags & PARTY_FLAG_JOINED))
				command_send_va (CMD_INVITE, player->name);
		}
		else
			navigation.npc_present = TRUE;

		g_free (tmp);
	}

	if (monster_avoid && character.flag.avoid)
	{
		printt ("Avoiding %s", monster_avoid->name);
		monster_target_list_free ();
	}
}


/* =========================================================================
 = MONSTER_TARGET_LIST_FREE
 =
 = Free data allocated to target list
 ======================================================================== */

void monster_target_list_free (void)
{
	monster_t *monster;
	GSList *node;

	for (node = character.targets; node; node = node->next)
	{
		monster = node->data;
		g_free (monster->name);
		g_free (monster);
	}
	g_slist_free (character.targets);
	character.targets = NULL;
}


/* =========================================================================
 = MONSTER_TARGET_LIST_PRIORITIZE
 =
 = Sort the target list by priority
 ======================================================================== */

static gint monster_target_list_prioritize (gconstpointer a,
	gconstpointer b)
{
	monster_t *mon1, *mon2;

	mon1 = (monster_t *) a;
	mon2 = (monster_t *) b;

	g_assert (mon1 != NULL);
	g_assert (mon2 != NULL);

	/* MON1 = contender    / MON2  = current champ */
	/* TRUE = insert after / FALSE = insert before */

	if ((mon1->flags & MONSTER_FLAG_PRIO) &&
	   !(mon2->flags & MONSTER_FLAG_PRIO))
	   return FALSE;

	if ((mon2->flags & MONSTER_FLAG_PRIO) &&
	   !(mon1->flags & MONSTER_FLAG_PRIO))
	   return TRUE;

	if (mon1->hp > mon2->hp)
		return FALSE;

	return TRUE;
}


/* =========================================================================
 = MONSTER_TARGET_ADD
 =
 = Adds monster to target list
 ======================================================================== */

void monster_target_add (monster_t *record, const gchar *prefix)
{
	monster_t *monster;

	g_assert (record != NULL);

#ifdef DEBUG
	if (character.flag.running
		|| !character.option.auto_all
		|| !character.option.attack)
		g_assert_not_reached ();
#endif

	monster = g_malloc0 (sizeof (monster_t));
	monster->name   = g_strdup (record->name);
	monster->prefix = (gchar *) prefix;
	monster->hp     = record->hp;
	monster->flags  = record->flags;

	character.targets = g_slist_insert_sorted (character.targets, monster,
		monster_target_list_prioritize);

	combat_monsters_update ();

	if (combat.strategy.current != combat_strategy_get_next ())
		combat.strategy.current = NULL;
}


/* =========================================================================
 = MONSTER_TARGET_GET
 =
 = Returns the current target
 ======================================================================== */

monster_t *monster_target_get (void)
{
	if (character.targets)
		return g_slist_nth_data (character.targets, 0);

	return NULL;
}


/* =========================================================================
 = MONSTER_TARGET_DEL
 =
 = Removes the current target
 ======================================================================== */

void monster_target_del (void)
{
	monster_t *monster;

	if ((monster = monster_target_get()) == NULL)
		return;

	character.targets = g_slist_remove (character.targets, monster);
	g_free (monster->name);
	g_free (monster);

	if (!character.targets)
	{
		combat_reset ();

		if (character.option.item_check
			&& !character.option.monster_check
			&& !CANNOT_SEE)
			command_send (CMD_TARGET);
	}
	else
	{
		combat.rounds = 0;
		combat.damage.enemy = 0;
		combat.strategy.current = NULL;
		combat_monsters_update ();
	}
}


/* =========================================================================
 = MONSTER_TARGET_LOOKUP
 =
 = Checks to see if monster is in target list
 ======================================================================== */

monster_t *monster_target_lookup (monster_t *record, const gchar *prefix)
{
	monster_t *monster;
	GSList *node;

	g_assert (record != NULL);

	for (node = character.targets; node; node = node->next)
	{
		monster = node->data;

		if (!strcasecmp (monster->name, record->name))
		{
			if (!prefix && !monster->prefix)
				return monster;

			if (prefix && monster->prefix &&
				!strcasecmp (monster->prefix, prefix))
				return monster;
		}
	}
	return NULL;
}


/* =========================================================================
 = MONSTER_TARGET_OVERRIDE
 =
 = Overrides the current target and optionally re-engages the new target
 ======================================================================== */

void monster_target_override (monster_t *monster, gboolean re_engage)
{
	monster_t *current;

	if ((current = monster_target_get ()) == NULL)
		return;

	/* determine if override is necessary */
	if (!strcasecmp (current->name, monster->name))
	{
		if (!current->prefix && !monster->prefix)
			return;

		if (current->prefix && monster->prefix
			&& !strcasecmp (current->prefix, monster->prefix))
			return;
	}

	character.targets = g_slist_remove  (character.targets, monster);
	character.targets = g_slist_prepend (character.targets, monster);

	if (re_engage)
		combat.strategy.current = NULL;
	else
		printt ("Target override");
}
