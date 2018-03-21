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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "command.h"
#include "character.h"
#include "defs.h"
#include "mudpro.h"
#include "navigation.h"
#include "party.h"
#include "spells.h"
#include "timers.h"
#include "terminal.h"
#include "utils.h"

key_value_t spell_flags[] = {
	{ "Heal",        SPELL_FLAG_HEAL },
	{ "Antidote",    SPELL_FLAG_ANTIDOTE },
	{ "CureDisease", SPELL_FLAG_CDISEASE },
	{ "Light",       SPELL_FLAG_LIGHT },
	{ "Vision",      SPELL_FLAG_VISION },
	{ "Movement",    SPELL_FLAG_MOVEMENT },
	{ "IntAttack",   SPELL_FLAG_INT_ATTACK },
	{ "IntRest",     SPELL_FLAG_INT_REST },
	{ "IntMeditate", SPELL_FLAG_INT_MEDITATE },
	{ "IntSneak",    SPELL_FLAG_INT_SNEAK },
	{ NULL, 0 }
};

static GSList *spell_db;

static void spell_db_free (GSList *db);
static void spell_db_parse_option (spell_t *spell, gchar *str);
static void spell_db_merge_active (gpointer data, gpointer user_data);
#ifdef NEW_SPELLCASTING
static gboolean spellcasting_select_spell (player_t *player);
#endif
static gboolean spell_within_threshold (gint thresh, gint test, gint percent);


/* =========================================================================
 = SPELL_DB_INIT
 =
 = Initialize the spell database
 ======================================================================== */

void spell_db_init (void)
{
	mudpro_db.spells.filename = g_strdup_printf (
		"%s%cspells.db", character.data_path, G_DIR_SEPARATOR);

	spell_db = NULL;
	spell_db_load ();
}


/* =========================================================================
 = SPELL_DB_CLEANUP
 =
 = Cleanup after spell database
 ======================================================================== */

void spell_db_cleanup (void)
{
	spell_db_free (spell_db);
	spell_db = NULL;

	g_free (mudpro_db.spells.filename);
}


/* =========================================================================
 = SPELL_DB_LOAD
 =
 = (Re)load spell database
 ======================================================================== */

void spell_db_load (void)
{
	gchar buf[STD_STRBUF];
	gchar *offset, *tmp;
	spell_t *spell = NULL;
	GSList *old_db = NULL;
	FILE *fp;

	if ((fp = fopen (mudpro_db.spells.filename, "r")) == NULL)
	{
		printt ("Unable to open spell database");
		return;
	}

	g_get_current_time (&mudpro_db.spells.access);

	if (spell_db != NULL) /* hold data temporarily */
	{
		old_db = spell_db;
		spell_db = NULL;
	}

	while (fgets (buf, sizeof (buf), fp))
	{
		strchomp (buf);

		if (buf[0] == '#' || buf[0] == '\0')
		{
			spell = NULL;
			continue;
		}

		if (isspace (buf[0]))
		{
			if (spell != NULL)
				spell_db_parse_option (spell, buf);
			continue;
		}

		offset = buf;
		if ((tmp = get_token_as_str (&offset)) == NULL)
			continue;

		if (!strcasecmp (tmp, "Spell"))
		{
			spell = g_malloc0 (sizeof (spell_t));

			if ((spell->name = get_token_as_str (&offset)) == NULL)
			{
				g_free (spell);
				spell = NULL;
			}
			else
				spell_db = g_slist_append (spell_db, spell);
		}
		g_free (tmp);
	}
	fclose (fp);

	if (old_db) /* mark active spells and free old data */
	{
		g_slist_foreach (spell_db, spell_db_merge_active, old_db);
		spell_db_free (old_db);
	}
}


/* =========================================================================
 = SPELL_DB_FREE
 =
 = Free memory allocated to spell database
 ======================================================================== */

static void spell_db_free (GSList *db)
{
	GSList *node;
	spell_t *spell;

	for (node = db; node; node = node->next)
	{
		if ((spell = node->data) == NULL)
			continue;

		g_free (spell->name);
		g_free (spell->msg.start);
		g_free (spell->msg.end);
		g_free (spell);
	}
	g_slist_free (db);
}


/* =========================================================================
 = SPELL_DB_PARSE
 =
 = Attempts to match string using spell database
 ======================================================================== */

void spell_db_parse (gchar *str)
{
	GSList *node;
	spell_t *spell;

	for (node = spell_db; node; node = node->next)
	{
		spell = node->data;
		g_assert (spell != NULL);

		if (spell->msg.start && !strcmp (str, spell->msg.start))
			spell->active = TRUE;
		else if (spell->msg.end && !strcmp (str, spell->msg.end))
			spell->active = FALSE;
	}
}


/* =========================================================================
 = SPELL_DB_PARSE_OPTION
 =
 = Parses optional spell data and updates record
 ======================================================================== */

static void spell_db_parse_option (spell_t *spell, gchar *str)
{
	gchar *offset, *option, *tmp;
	glong value;

	offset = option = tmp = NULL;

	offset = str;
	if ((option = get_token_as_str (&offset)) == NULL)
		return;

	if (!strcasecmp (option, "HPThreshold"))
	{
		g_free (option);
		tmp = get_token_as_str (&offset);

		if (tmp && tmp[0] != '\0')
		{
			if (!strncasecmp (tmp, "is", 2))
			{
				spell->hp.test   = '=';
				spell->hp.thresh = atoi (tmp+2);
			}
			else if (!strncasecmp (tmp, "not", 3))
			{
				spell->hp.test = '!';
				spell->hp.thresh = atoi (tmp+3);
			}
			else if (tmp[0] == '<' || tmp[0] == '>')
			{
				spell->hp.test = tmp[0];
				spell->hp.thresh = atoi (tmp+1);
			}

			if (tmp[strlen (tmp)-1] == '%')
			{
				spell->hp.percent = TRUE;
				spell->hp.thresh  = CLAMP (spell->hp.thresh, 0, 100);
			}
		}
	}

	else if (!strcasecmp (option, "MAThreshold"))
	{
		g_free (option);
		tmp = get_token_as_str (&offset);

		if (tmp && tmp[0] != '\0')
		{
			if (!strncasecmp (tmp, "is", 2))
			{
				spell->ma.test = '=';
				spell->ma.thresh = atoi (tmp+2);

			}
			else if (!strncasecmp (tmp, "not", 3))
			{
				spell->ma.test = '!';
				spell->ma.thresh = atoi (tmp+3);
			}
			else if (tmp[0] == '<' || tmp[0] == '>')
			{
				spell->ma.test = tmp[0];
				spell->ma.thresh = atoi (tmp+1);
			}

			if (tmp[strlen (tmp)-1] == '%')
			{
				spell->ma.percent = TRUE;
				spell->ma.thresh  = CLAMP (spell->ma.thresh, 0, 100);
			}
		}
	}

	else if (!strcasecmp (option, "MaxDuration"))
	{
		g_free (option);
		value = get_token_as_long (&offset);
		spell->duration = MAX (0, value);
	}

	else if (!strcasecmp (option, "MessageStart"))
	{
		g_free (option);
		spell->msg.start = get_token_as_str (&offset);
	}

	else if (!strcasecmp (option, "MessageEnd"))
	{
		g_free (option);
		spell->msg.end = get_token_as_str (&offset);
	}

	else if (!strcasecmp (option, "RecastTick"))
	{
		g_free (option);
		value = get_token_as_long (&offset);
		spell->tick = MAX (0, value);
	}

	else if (!strcasecmp (option, "SetFlag"))
	{
		key_value_t *sf;

		g_free (option);

		if ((tmp = get_token_as_str (&offset)) == NULL)
			return;

		for (sf = spell_flags; sf->key; sf++)
		{
			if (!strcasecmp (tmp, sf->key))
			{
				FlagON (spell->flags, sf->value);
				g_free (tmp);
				return;
			}
		}
	}

	g_free (tmp);
}


/* =========================================================================
 = SPELL_DB_MERGE_ACTIVE
 =
 = Compare spells, merge those that match and are active (time based)
 ======================================================================== */

static void spell_db_merge_active (gpointer data, gpointer user_data)
{
	spell_t *spell, *old_spell;
	GSList *node;

	g_assert (data != NULL);
	spell = data;

	if (!spell->duration)
		return; /* only compare time based spells */

	for (node = user_data; node; node = node->next)
	{
		old_spell = node->data;

		if (!old_spell->duration ||
			 old_spell->duration != spell->duration)
			continue; /* duration mismatch */

		if (strcmp (spell->name, old_spell->name))
			continue; /* name mismatch */

		if (spell->flags != old_spell->flags)
			continue; /* flags mismatch */

		if (memcmp (&spell->hp, &old_spell->hp, sizeof (spell->hp)) ||
			memcmp (&spell->ma, &old_spell->ma, sizeof (spell->ma)))
			continue; /* threshold mismatch */

		if (spell->msg.start && old_spell->msg.start &&
			strcmp (spell->msg.start, old_spell->msg.start))
			continue; /* start message mismatch */

		if (spell->msg.end && old_spell->msg.end &&
			strcmp (spell->msg.end, old_spell->msg.end))
			continue; /* end message mismatch */

		/* looks like we got a winner */
		spell->lastcast = old_spell->lastcast;
		spell->active   = old_spell->active;
	}
}

#ifdef NEW_SPELLCASTING

/* =========================================================================
 = SPELLCASTING
 =
 = Perform spellcasting
 ======================================================================== */

gboolean spellcasting (void)
{
	player_t *player;
	GSList *node;

	if (!party_members)
	{
		player = player_db_lookup (character.first_name);

		printt ("DEBUG: player -> '%s'", player->name);

		if (player &&
			spellcasting_select_spell (player))
			return TRUE;

		return FALSE;
	}

	for (node = party_members; node; node = node->next)
	{
		player = node->data;

		if (!player)
		{
			printt ("DEBUG: spellcasting party_member is NULL!");
			continue;
		}

		if (spellcasting_select_spell (player))
			return TRUE;
	}

	return FALSE;
}


/* =========================================================================
 = SPELLCASTING_SELECT_SPELL
 =
 = Select spell to cast
 ======================================================================== */

static gboolean spellcasting_select_spell (player_t *player)
{
	GSList *node;
	spell_t *spell;
	GTimeVal now;

	if (!player->hp.max)
		return FALSE; /* HP/MA not read yet */

	for (node = spell_db; node; node = node->next)
	{
		spell = node->data;

		if (!spell)
		{
			printt ("DEBUG: spellcasting_select_spell spell has NULL data!");

			/* try reloading the database */
			spell_db_load ();
			return FALSE;
		}

		if ((SPELL_FLAG (SPELL_FLAG_HEAL)        && !character.option.recovery) ||
			(SPELL_FLAG (SPELL_FLAG_ANTIDOTE)    && !player->status.poisoned) ||
			(SPELL_FLAG (SPELL_FLAG_CDISEASE)    && !character.status.diseased) ||
			(SPELL_FLAG (SPELL_FLAG_VISION)      && !character.status.blind) ||
			(SPELL_FLAG (SPELL_FLAG_LIGHT)       && !navigation.room_dark) ||
			(character.flag.sneaking             && !SPELL_FLAG (SPELL_FLAG_INT_SNEAK)) ||
			(character.targets                   && !SPELL_FLAG (SPELL_FLAG_INT_ATTACK)) ||
			(character.state == STATE_RESTING    && !SPELL_FLAG (SPELL_FLAG_INT_REST)) ||
			(character.state == STATE_MEDITATING && !SPELL_FLAG (SPELL_FLAG_INT_MEDITATE)))
			continue;

		if (spell->hp.test)
		{
			gint value = spell->hp.percent ? player->hp.p : player->hp.now;

			if (!spell_within_threshold (spell->hp.thresh, spell->hp.test, value))
				continue;
		}

		if (spell->ma.test)
		{
			gint value = spell->ma.percent ?
				get_percent (character.ma.now, character.ma.max) : character.ma.now;

			if (!spell_within_threshold (spell->ma.thresh, spell->ma.test, value))
				continue;
		}

		if (strcmp (player->name, character.first_name))
			continue; /* FIXME: handle following spells for followers */

		if ((spell->tick && character.tick.current) &&
			(character.tick.current < spell->tick) &&
			(character.tick.current != character.tick.meditate))
		{
			spell->active = FALSE;
			character.tick.current = 0; /* require another reading */
		}

		g_get_current_time (&now);

		if ((spell->duration && !spell->active) ||
			((now.tv_sec - spell->lastcast) > spell->duration))
		{
			spell->lastcast = now.tv_sec;
			spell->active   = FALSE;

			if (!strcmp (character.first_name, player->name))
				command_send_va (CMD_CAST, "%s", spell->name);
			else
				command_send_va (CMD_CAST, "%s %s", spell->name, player->name);
			return TRUE;
		}
	}

	return FALSE; /* no spell was cast */
}


#else

/* =========================================================================
 = SPELLCASTING
 =
 = Perform spellcasting
 ======================================================================== */

gboolean spellcasting (void)
{
	GSList *node;
	GTimeVal now;
	spell_t *spell;

	for (node = spell_db; node; node = node->next)
	{
		/* if spell has null data, try reloading the database */
		if ((spell = node->data) == NULL)
			spell_db_load ();

		/* attempt to disqualify spell */

		if ((SPELL_FLAG (SPELL_FLAG_HEAL)        && !character.option.recovery) ||
			(SPELL_FLAG (SPELL_FLAG_ANTIDOTE)    && !character.status.poisoned) ||
			(SPELL_FLAG (SPELL_FLAG_CDISEASE)    && !character.status.diseased) ||
			(SPELL_FLAG (SPELL_FLAG_VISION)      && !character.status.blind) ||
			(SPELL_FLAG (SPELL_FLAG_LIGHT)       && !navigation.room_dark) ||
			(character.flag.sneaking             && !SPELL_FLAG (SPELL_FLAG_INT_SNEAK)) ||
			(character.targets                   && !SPELL_FLAG (SPELL_FLAG_INT_ATTACK)) ||
			(character.state == STATE_RESTING    && !SPELL_FLAG (SPELL_FLAG_INT_REST)) ||
			(character.state == STATE_MEDITATING && !SPELL_FLAG (SPELL_FLAG_INT_MEDITATE)))
			continue;

		if (spell->hp.test) /* test if spell is within HP threshold */
		{
			gint value = spell->hp.percent ?
				get_percent (character.hp.now, character.hp.max) : character.hp.now;

			if (!spell_within_threshold (spell->hp.thresh, spell->hp.test, value))
				continue;
		}

		if (spell->ma.test) /* test if spell is within MA threshold */
		{
			gint value = spell->ma.percent ?
				get_percent (character.ma.now, character.ma.max) : character.ma.now;

			if (!spell_within_threshold (spell->ma.thresh, spell->ma.test, value))
				continue;
		}

		if ((spell->tick && character.tick.current) &&
			(character.tick.current < spell->tick) &&
			(character.tick.current != character.tick.meditate))
		{
			spell->active = FALSE;
			character.tick.current = 0; /* require another reading */
		}

		g_get_current_time (&now);

		if ((spell->duration && !spell->active) ||
			(now.tv_sec - spell->lastcast > spell->duration))
		{
			spell->lastcast = now.tv_sec;
			spell->active   = FALSE;
			command_send_va (CMD_CAST, "%s", spell->name);
			return TRUE;
		}
	}

	return FALSE; /* no spell was cast */
}

#endif


/* =========================================================================
 = SPELL_WITHIN_THRESHOLD
 =
 = Checks if spell is within specified threshold
 ======================================================================== */

static gboolean spell_within_threshold (gint thresh, gint test, gint value)
{
	switch (test)
	{
	case '=': if (value == thresh) return TRUE; break;
	case '!': if (value != thresh) return TRUE; break;
	case '>': if (value  > thresh) return TRUE; break;
	case '<': if (value  < thresh) return TRUE; break;
	}
	return FALSE;
}
