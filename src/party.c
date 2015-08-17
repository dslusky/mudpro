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

#include <string.h>
#include <stdlib.h>

#include "automap.h"
#include "character.h"
#include "client_ai.h"
#include "command.h"
#include "mudpro.h"
#include "osd.h"
#include "party.h"
#include "stats.h"
#include "terminal.h"
#include "utils.h"

party_request_t request_table[]=
{
	{ PARTY_REQ_NONE,     "@none",     { 0, 0 } },
	{ PARTY_REQ_JOIN,     "@join",     { 0, 0 } },
	{ PARTY_REQ_WAIT,     "@wait",     { 0, 0 } },
	{ PARTY_REQ_HEAL,     "@heal",     { 0, 0 } },
	{ PARTY_REQ_BLIND,    "@blind",    { 0, 0 } },
	{ PARTY_REQ_POISONED, "@poisoned", { 0, 0 } },
	{ PARTY_REQ_DISEASED, "@diseased", { 0, 0 } },
	{ PARTY_REQ_HELD,     "@held",     { 0, 0 } },
	{ 0, NULL, { 0, 0 } }
};

GSList *party_members; /* list of players within party */

static void party_auto_toggle (player_t *player, gint type,
	const gchar *label, const gchar *state, gboolean *toggle);
static gint party_member_list_prioritize (gconstpointer a,
    gconstpointer b);


/* =========================================================================
 = PARTY_INIT
 =
 = Initialize party data
 ======================================================================== */

void party_init (void)
{
	party_members = NULL;
}


/* =========================================================================
 = PARTY_CLEANUP
 =
 = Cleanup party data
 ======================================================================== */

void party_cleanup (void)
{
	party_follower_list_free ();
	party_member_list_free ();
}


/* =========================================================================
 = PARTY_SEND_REQUEST
 =
 = Send request for party assistance if not already pending
 ======================================================================== */

gboolean party_send_request (gint req_type, const gchar *reason,
	gint msg_type, player_t *player)
{
	party_request_t *request = NULL;
	GString *buf;

	g_assert (req_type >= PARTY_REQ_JOIN);
	g_assert (req_type <= PARTY_REQ_HELD);

	request = &request_table[req_type];

	if (request->pending.tv_sec)
		return FALSE;

	/* send request and mark as pending */

	g_get_current_time (&request->pending);

	/* set timeout period */
	request->pending.tv_sec += (req_type == PARTY_REQ_WAIT)? 45 : 15;

	switch (msg_type)
	{
	case PARTY_MSG_SPOKEN:

		buf = g_string_new ("");

		if (player)
			g_string_printf (buf, ">%s %s", player->name, request->command);
		else
			g_string_printf (buf, "%c%s", character.prefix, request->command);

		if (reason)
			g_string_append_printf (buf, " (%s)", reason);

		command_send_va (CMD_SPEAK, buf->str);
		g_string_free (buf, TRUE);

		return TRUE;

	case PARTY_MSG_TELEPATH:

		if (!player) return FALSE;

		buf = g_string_new ("");

		g_string_printf (buf, "%s %s", player->name, request->command);

		if (reason)
			g_string_append_printf (buf, " (%s)", reason);

		command_send_va (CMD_TELEPATH, buf->str);
		g_string_free (buf, TRUE);

		return TRUE;
	default:
		g_assert_not_reached ();
	}

	return FALSE;
}


/* =========================================================================
 = PARTY_UPDATE_REQUEST
 =
 = Update pending party requests
 ======================================================================== */

void party_request_update (void)
{
	party_request_t *request;
	GTimeVal now;

	g_get_current_time (&now);

	for (request = request_table; request->command; request++)
	{
		if (request->pending.tv_sec &&
			now.tv_sec >= request->pending.tv_sec)
			memset (&request->pending, 0, sizeof (GTimeVal));
	}
}


/* =========================================================================
 = PARTY_SEND_RESPONSE
 =
 = Sends response to party command w/variable argument list
 ======================================================================== */

void party_send_response (player_t *player, gint type, const gchar *fmt, ...)
{
	va_list argptr;
	gchar buf[STD_STRBUF];

	if (!player || !fmt)
		return;

	memset (&buf, 0, sizeof (buf));

	va_start (argptr, fmt);
	vsnprintf (buf, sizeof (buf), fmt, argptr);
	va_end (argptr);

	if (buf[0] == '\0')
		return; /* nothing to send */

	switch (type)
	{
	case PARTY_MSG_GANGPATH:
		command_send_va (CMD_GANGPATH, "{%s}", buf);
		break;
	case PARTY_MSG_SPOKEN:
		command_send_va (CMD_SPEAK, ">%s {%s}", player->name, buf);
		break;
	case PARTY_MSG_TELEPATH:
		command_send_va (CMD_TELEPATH, "%s {%s}", player->name, buf);
		break;
	default:
		g_assert_not_reached ();
	}
}


/* =========================================================================
 = PARTY_AUTO_TOGGLE
 =
 = Handle request to change player auto-toggles
 ======================================================================== */

static void party_auto_toggle (player_t *player, gint type,
	const gchar *label, const gchar *state, gboolean *toggle)
{
	g_assert (player != NULL);
	g_assert (type >= PARTY_MSG_GANGPATH);
	g_assert (type <= PARTY_MSG_TELEPATH);
	g_assert (label != NULL);
	g_assert (state != NULL);
	g_assert (toggle != NULL);

	if (*state == '\0'
		|| (!strcasecmp (state, "on") && !*toggle)
		|| (!strcasecmp (state, "off") && *toggle))
		*toggle = !*toggle;

	party_send_response (player, type, "%s: %s",
		label, *toggle ? "ON" : "OFF");
}


/* =========================================================================
 = PARTY_PARSE_COMMAND
 =
 = Parse incoming party commands
 ======================================================================== */

void party_parse_command (player_t *player, gint type, const gchar *str)
{
	const gchar *pos = NULL;

	if (!player || !str || str[0] == '\0' || !character.first_name)
		return;

	if (!strcasecmp (player->name, character.first_name))
		return; /* ignore our own commands */

	if (!strcasecmp (str, "@ok"))
	{
		if (party_follower_verify (player))
		{
			printt ("DEBUG: %s's wait flag cleared", player->name);
			player_set_wait (player, FALSE /* activate */);
		}

		return;
	}

	else if (!strncasecmp (str, "@auto-", 6))
	{
		if (!REMOTE_ACCESS (player, REMOTE_FLAG_SETTINGS))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		pos = str + 6;

		if (!strncasecmp (pos, "all", 3))
			party_auto_toggle (player, type,
				"Auto-All", pos+4, &character.option.auto_all);

		else if (!strncasecmp (pos, "attack", 6))
			party_auto_toggle (player, type,
				"Auto-Attack", pos+7, &character.option.attack);

		else if (!strncasecmp (pos, "recovery", 8))
			party_auto_toggle (player, type,
				"Auto-Recovery", pos+9, &character.option.recovery);

		else if (!strncasecmp (pos, "movement", 8))
			party_auto_toggle (player, type,
				"Auto-Movement", pos+9, &character.option.movement);

		else if (!strncasecmp (pos, "items", 5))
			party_auto_toggle (player, type,
				"Auto-GetItems", pos+6, &character.option.get_items);

		else if (!strncasecmp (pos, "cash", 4))
			party_auto_toggle (player, type,
				"Auto-GetCash", pos+5, &character.option.get_cash);

		else if (!strncasecmp (pos, "stash", 5))
			party_auto_toggle (player, type,
				"Auto-Stash", pos+6, &character.option.stash);

		else if (!strncasecmp (pos, "sneak", 5))
			party_auto_toggle (player, type,
				"Auto-Sneak", pos+6, &character.option.sneak);

		else
			party_send_response (player, type, PARTY_COMMAND_INVALID);

		return;
	}

	if (!character.option.auto_all)
		return; /* == auto-all required beyond this point =============== */

	if (!strncasecmp (str, "@do ", 4) || !strncasecmp (str, "@party ", 7))
	{
		if (!REMOTE_ACCESS (player, REMOTE_FLAG_CONTROL))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		pos = index (str, ' ')+1;

		if (*pos != '\0')
		{
			send_line ((gchar *) pos);
			party_send_response (player, type, PARTY_COMMAND_OK);
		}
		else
			party_send_response (player, type, PARTY_COMMAND_INVALID);
	}

	else if (!strcasecmp (str, "@exp"))
	{
		GString *gained, *needed;

		if (!REMOTE_ACCESS (player, REMOTE_FLAG_INFO))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		stats_exp_update ();

		gained = commified_g_string (stats.exp.gained);
		needed = commified_g_string (stats.exp.needed);

		party_send_response (player, type,
			"Made: %s  Needed: %s  Rate: %s  Will level in: %s",
			gained->str, needed->str,
			stats.exp.rate_str->str, stats.exp.eta_str->str);

		g_string_free (gained, TRUE);
		g_string_free (needed, TRUE);
	}

	else if (!strcasecmp (str, "@health"))
	{
		if (!REMOTE_ACCESS (player, REMOTE_FLAG_INFO))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		if (character.ma.max)
			party_send_response (player, type, "HP=%d/%d,MA=%d/%d",
				character.hp.now, character.hp.max,
				character.ma.now, character.ma.max);
		else
			party_send_response (player, type, "HP=%d/%d",
				character.hp.now, character.hp.max);
	}

	else if (!strcasecmp (str, "@join"))
	{
		if (!REMOTE_ACCESS (player, REMOTE_FLAG_COMMANDS))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		command_send_va (CMD_JOIN, player->name);
		party_send_response (player, type, PARTY_COMMAND_OK);
	}

	else if (!strcasecmp (str, "@level"))
	{
		GString *needed;

		if (!REMOTE_ACCESS (player, REMOTE_FLAG_INFO))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		stats_exp_update ();

		needed = commified_g_string (stats.exp.needed);

		party_send_response (player, type,
			"Level: %d  Needed: %s  Will level in: %s",
			character.stat.level, needed->str, stats.exp.eta_str->str);

		g_string_free (needed, TRUE);
	}

	else if (!strcasecmp (str, "@rego"))
	{
		if (!REMOTE_ACCESS (player, REMOTE_FLAG_COMMANDS))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		character.option.movement = TRUE;
		party_send_response (player, type, PARTY_COMMAND_OK);
	}

	else if (!strcasecmp (str, "@reset"))
	{
		if (!REMOTE_ACCESS (player, REMOTE_FLAG_COMMANDS))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		mudpro_reset_state (FALSE /* disconnected */);
		osd_stats_reset_all ();
		party_send_response (player, type, PARTY_COMMAND_OK);
	}

	else if (!strcasecmp (str, "@status"))
	{
		if (!REMOTE_ACCESS (player, REMOTE_FLAG_INFO))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		/* TODO implement this */
	}

	else if (!strcasecmp (str, "@stop"))
	{
		if (!REMOTE_ACCESS (player, REMOTE_FLAG_COMMANDS))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		character.option.movement = FALSE;
		party_send_response (player, type, PARTY_COMMAND_OK);
	}

	else if (!strncasecmp (str, "@ver", 3))
	{
		if (!REMOTE_ACCESS (player, REMOTE_FLAG_INFO))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		party_send_response (player, type, "MudPRO %s", RELEASE);
	}

	else if (!strncasecmp (str, "@wait", 5))
	{
		if (!REMOTE_ACCESS (player, REMOTE_FLAG_COMMANDS))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		if (party_follower_verify (player) &&
			!(player->party_flags & PARTY_FLAG_WAIT))
		{
			printt ("DEBUG: %s's wait flag set", player->name);
			player_set_wait (player, TRUE /* activate */);
			/* client_ai_movement_reset (); */
		}
	}

	else if (!strcasecmp (str, "@where"))
	{
		GString *buf;

		if (!REMOTE_ACCESS (player, REMOTE_FLAG_INFO))
		{
			party_send_response (player, type, PARTY_COMMAND_INVALID);
			return;
		}

		if (!(VISIBLE_EXITS | automap.obvious.secrets))
		{
			party_send_response (player, type, "N/A");
			return;
		}

		buf = get_obvious_exits_as_g_string ();
		buf = g_string_prepend (buf, " (Exits: ");
		buf = g_string_prepend (buf, automap.room_name->str);
		buf = g_string_append  (buf, ")");

		party_send_response (player, type, buf->str);
		g_string_free (buf, TRUE);
	}
}


/* =========================================================================
 = PARTY_PARSE_RESPONSE
 =
 = Parse incoming party responses
 ======================================================================== */

void party_parse_response (player_t *player, gint type, const gchar *str)
{
	gchar *pos;

	if (!strcmp (str, "{ok}"))
	{
		/* this is not currently being used for anything ... */
	}

	if (!strncmp (str, "{HP=", 4) && str[strlen(str)-1] == '}')
	{
		if (!(pos = index (str, '/')))
			return; /* invalid response */

		player->hp.max = atoi (++pos);

		if ((pos = strstr (pos, ",MA=")))
		{
			if (!(pos = index (pos, '/')))
				return; /* invalid response */

			player->ma.max = atoi (++pos);
		}
	}
	printt ("DEBUG: (HP: %d/%d) (MA: %d/%d)",
		player->hp.now, player->hp.max,
		player->ma.now, player->ma.max);
}


/* =========================================================================
 = PARTY_FOLLOWER_LIST_FREE
 =
 = Free list of followers
 ======================================================================== */

void party_follower_list_free (void)
{
	printt ("DEBUG: follower list free");
	g_slist_free (character.followers);
	character.followers = NULL;
}


/* =========================================================================
 = PARTY_FOLLOWER_ADD
 =
 = Adds player to follower list
 ======================================================================== */

void party_follower_add (player_t *player)
{
	g_assert (player != NULL);

	if (g_slist_find (character.followers, player))
		return; /* already in list */

	character.leader = NULL;
	character.followers = g_slist_prepend (character.followers, player);

	player_set_wait (player, TRUE /* activate */);

	FlagON (player->party_flags, PARTY_FLAG_PRESENT);

	player->hp.p = 100; /* assume 100% hp/ma */
	player->ma.p = 100;

	player->hp.max = 0; /* require new reading */
	player->hp.now = 0;

	player->ma.max = 0;
	player->ma.now = 0;

	printt ("DEBUG: added follower -> %s", player->name);
}


/* =========================================================================
 = PARTY_FOLLOWER_DEL
 =
 = Remove player from follower list
 ======================================================================== */

void party_follower_del (player_t *player)
{
	g_assert (player != NULL);

	character.followers = g_slist_remove (character.followers, player);
	printt ("DEBUG: removed follower -> %s", player->name);

	player->party_flags = 0;
	player->hp.p = player->hp.max = player->hp.now = 0;
	player->ma.p = player->ma.max = player->ma.now = 0;
	player_set_wait (player, FALSE /* activate */);
}


/* =========================================================================
 = PARTY_FOLLOWER_SEARCH_FLAGS
 =
 = Searches follower flags
 ======================================================================== */

player_t *party_follower_search_flags (gulong flags)
{
	GSList *node;
	player_t *player;

	g_assert (flags != 0);

	for (node = character.followers; node; node = node->next)
	{
		player = node->data;

		if (player->party_flags & flags)
			return player;
	}

	return NULL;
}


/* =========================================================================
 = PARTY_FOLLOWER_VERIFY
 =
 = Verify that player is in follower list
 ======================================================================== */

gboolean party_follower_verify (player_t *player)
{
	return g_slist_find (character.followers, player) == NULL;
}


/* =========================================================================
 = PARTY_FOLLOWER_MANAGE
 =
 = Manage followers
 ======================================================================== */

void party_follower_manage (void)
{
	GSList *node;
	player_t *player;

	for (node = character.followers; node; node = node->next)
	{
		player = node->data;

		if ((player->party_flags & PARTY_FLAG_INVITED) &&
			!(player->party_flags & PARTY_FLAG_JOINED))
			party_send_request (PARTY_REQ_JOIN, NULL,
				PARTY_MSG_TELEPATH, player);
	}
}


/* =========================================================================
 = PARTY_FOLLOWER_GROUP_READY
 =
 = Determine if all followers are ready
 ======================================================================== */

gboolean party_follower_group_ready (void)
{
	GSList *node;
	player_t *player;
	GTimeVal now;

	g_get_current_time (&now);

	for (node = character.followers; node; node = node->next)
	{
		player = node->data;

		if (player->party_flags & PARTY_FLAG_WAIT)
		{
			if (now.tv_sec >= (player->party_wait.tv_sec + character.wait.party))
			{
				if ((player->flags & PLAYER_FLAG_ONLINE) &&
					(player->party_flags & PARTY_FLAG_PRESENT))
				{
					printt ("%s's wait flag expired", player->name);
					player_set_wait (player, FALSE /* activate */);
				}
				else
				{
					printt ("%s's wait flag expired, removing from party", player->name);
					party_follower_del (player);
				}
			}
			/* printt ("DEBUG %s's wait flag set", player->name); */
			return FALSE;
		}

		/* FIXME: are ONLINE/PRESENT checks redundant? do some testing ... */

		if (!(player->flags & PLAYER_FLAG_ONLINE))
		{
			printt ("DEBUG: %s's not online", player->name);
			return FALSE;
		}

		if (!(player->party_flags & PARTY_FLAG_PRESENT))
		{
			printt ("DEBUG: %s's not present", player->name);
			return FALSE;
		}

		if (!(player->party_flags & PARTY_FLAG_JOINED))
		{
			printt ("DEBUG: %s's not joined", player->name);
			return FALSE;
		}
	}
	return TRUE;
}


/* =========================================================================
 = PARTY_MEMBER_ADD
 =
 = Add player to party member list
 ======================================================================== */

void party_member_add (player_t *player)
{
	g_assert (player != NULL);

	if (g_slist_find (party_members, player))
		return; /* already in list */

	party_members = g_slist_insert_sorted (party_members, player,
		party_member_list_prioritize);

	player = g_slist_nth_data (party_members, 0);

	printt ("DEBUG: %s has the lowest HP (%d%%)", player->name, player->hp.p);
}


/* =========================================================================
 = PARTY_MEMBER_LIST_FREE
 =
 = Free list of party members
 ======================================================================== */

void party_member_list_free (void)
{
	g_slist_free (party_members);
	party_members = NULL;
}


/* =========================================================================
 = PARTY_MEMBER_LIST_PRIORITIZE
 =
 = Sort the party member list by health
 ======================================================================== */

static gint party_member_list_prioritize (gconstpointer a,
	gconstpointer b)
{
	player_t *player1, *player2;

	g_assert (a != NULL);
	g_assert (b != NULL);

	player1 = (player_t *) a; /* contender */
	player2 = (player_t *) b; /* current champ */

	if (player1->hp.p < player2->hp.p)
		return FALSE; /* insert before */
	else
		return TRUE; /* insert after */
}
