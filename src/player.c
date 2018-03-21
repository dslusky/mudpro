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

#include <ctype.h>
#include <string.h>

#include "character.h"
#include "client_ai.h"
#include "mudpro.h"
#include "player.h"
#include "terminal.h"
#include "utils.h"

key_value_t player_relation[]=
{
	{ "Unknown", PLAYER_RELATION_UNKNOWN },
	{ "Friend",  PLAYER_RELATION_FRIEND },
	{ "Enemy",   PLAYER_RELATION_ENEMY },
	{ "Avoid",   PLAYER_RELATION_AVOID },
	{ "Hangup",  PLAYER_RELATION_HANGUP },
	{ NULL, 0 }
};

key_value_t player_flags[]=
{
	{ "Online", PLAYER_FLAG_ONLINE },
	{ "Invite", PLAYER_FLAG_INVITE },
	{ "Join",   PLAYER_FLAG_JOIN },
	{ NULL, 0 }
};

key_value_t remote_flags[]=
{
	{ "Trusted",  REMOTE_FLAG_TRUSTED },
	{ "Info",     REMOTE_FLAG_INFO },
	{ "Assist",   REMOTE_FLAG_ASSIST },
	{ "Commands", REMOTE_FLAG_COMMANDS },
	{ "Settings", REMOTE_FLAG_SETTINGS },
	{ "Control",  REMOTE_FLAG_CONTROL },
	{ NULL, 0 }
};

static GHashTable *player_db;

static void player_db_free (void);
static void player_record_deallocate (gpointer key, gpointer value, gpointer user_data);
static void player_deallocate (player_t *player);
static void player_db_parse_option (player_t *player, const gchar *str);


/* =========================================================================
 = PLAYER_DB_INIT
 =
 = Initialize player database
 ======================================================================== */

void player_db_init (void)
{
	mudpro_db.players.filename = g_strdup_printf (
		"%s%cplayers.db", character.data_path, G_DIR_SEPARATOR);

	player_db = NULL;
	player_db_load ();
}


/* =========================================================================
 = PLAYER_DB_CLEANUP
 =
 = Cleanup after player database
 ======================================================================== */

void player_db_cleanup (void)
{
	player_db_free ();
	g_free (mudpro_db.players.filename);
}


/* =========================================================================
 = PLAYER_DB_LOAD
 =
 = (Re)load player database from file
 ======================================================================== */

void player_db_load (void)
{
	gchar buf[STD_STRBUF];
	gchar *offset, *tmp;
	player_t *player = NULL;
	FILE *fp;

	if ((fp = fopen (mudpro_db.players.filename, "r")) == NULL)
	{
		printt ("Unable to open player database");
		return;
	}

	g_get_current_time (&mudpro_db.players.access);

	if (player_db != NULL)
		player_db_free ();

	player_db = g_hash_table_new (g_str_hash, g_str_equal);

	while (fgets (buf, sizeof (buf), fp))
	{
		strchomp (buf);

		if (buf[0] == '#' || buf[0] == '\0')
		{
			player = NULL;
			continue;
		}

		if (isspace (buf[0]))
		{
			if (player)
				player_db_parse_option (player, buf);
			continue;
		}

		offset = buf;
		if ((tmp = get_token_as_str (&offset)) == NULL)
			continue;

		if (!strcasecmp (tmp, "Player"))
		{
			player = g_malloc0 (sizeof (player_t));
			player->relation = PLAYER_RELATION_UNKNOWN;

			if ((player->name = get_token_as_str (&offset)) == NULL)
			{
				g_free (player);
				player = NULL;
			}
			else
				g_hash_table_insert (player_db, player->name, player);
		}
		g_free (tmp);
	}
}


/* =========================================================================
 = PLAYER_DB_SAVE
 =
 = Save player database to file
 ======================================================================== */

void player_db_save (void)
{
}


/* =========================================================================
 = PLAYER_DB_FREE
 =
 = Free memory allocated to player database
 ======================================================================== */

static void player_db_free (void)
{
	if (player_db)
	{
		g_hash_table_foreach (player_db, player_record_deallocate, NULL);
		g_hash_table_destroy (player_db);
		player_db = NULL;
	}
}


/* =========================================================================
 = PLAYER_RECORD_DEALLOCATE
 =
 = Free memory allocated to player record
 ======================================================================== */

static void player_record_deallocate (gpointer key, gpointer value,
	gpointer user_data)
{
	player_deallocate (value);
}


/* =========================================================================
 = PLAYER_DEALLOCATE
 =
 = Deallocate the player
 ======================================================================== */

static void player_deallocate (player_t *player)
{
	g_assert (player != NULL);
	g_free (player->name);
	g_free (player);
}


/* =========================================================================
 = PLAYER_DB_PARSE_OPTION
 =
 = Parses player option and adds data to record
 ======================================================================== */

static void player_db_parse_option (player_t *player, const gchar *str)
{
	gchar *offset, *option, *arg;

	if (!player || !str)
		return;

	offset = option = arg = NULL;

	offset = (gchar *) str;
	if ((option = get_token_as_str (&offset)) == NULL)
		return;

	if (!strcasecmp (option, "Relation"))
	{
		key_value_t *p;

		g_free (option);

		if ((arg = get_token_as_str (&offset)) == NULL)
			return;

		for (p = player_relation; p->key; p++)
		{
			if (strcasecmp (arg, p->key))
				continue;

			player->relation = p->value;
			g_free (arg);
			return;
		}
	}

	else if (!strcasecmp (option, "RemoteAccess"))
	{
		key_value_t *r;

		g_free (option);

		if ((arg = get_token_as_str (&offset)) == NULL)
			return;

		for (r = remote_flags; r->key; r++)
		{
			if (strcasecmp (arg, r->key))
				continue;

			FlagON (player->remotes, r->value);
			g_free (arg);
			return;
		}
	}

	else if (!strcasecmp (option, "SetFlag"))
	{
		key_value_t *p;

		g_free (option);

		if ((arg = get_token_as_str (&offset)) == NULL)
			return;

		for (p = player_flags; p->key; p++)
		{
			if (strcasecmp (arg, p->key))
				continue;

			FlagON (player->flags, p->value);
			g_free (arg);
			return;
		}
	}

	g_free (arg);
}


/* =========================================================================
 = PLAYER_DB_LOOKUP
 =
 = Lookup player in database
 ======================================================================== */

player_t *player_db_lookup (const gchar *name)
{
	if (player_db)
		return g_hash_table_lookup (player_db, name);

	return NULL;
}


/* =========================================================================
 = PLAYER_DB_ADD
 =
 = Adds player to database and returns a pointer
 ======================================================================== */

player_t *player_db_add (const gchar *name)
{
	player_t *player = NULL;

	/* only add player if not already in database */

	if (player_db && !player_db_lookup (name))
	{
		player = g_malloc0 (sizeof (player_t));
		player->name = g_strdup (name);
		player->relation = PLAYER_RELATION_UNKNOWN;

		g_hash_table_insert (player_db, player->name, player);
	}

	return player;
}


/* =========================================================================
 = PLAYER_SET_WAIT
 =
 = (Re)sets player wait status
 ======================================================================== */

void player_set_wait (player_t *player, gboolean activate)
{
	g_assert (player != NULL);

	if (activate)
	{
		FlagON (player->party_flags, PARTY_FLAG_WAIT);
		g_get_current_time (&player->party_wait);
		client_ai_movement_reset ();
	}
	else
	{
		FlagOFF (player->party_flags, PARTY_FLAG_WAIT);
		memset (&player->party_wait, 0, sizeof (GTimeVal));
	}
}
