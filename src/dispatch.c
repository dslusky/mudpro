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

#include "automap.h"
#include "autoroam.h"
#include "character.h"
#include "combat.h"
#include "command.h"
#include "defs.h"
#include "dispatch.h"
#include "item.h"
#include "mapview.h"
#include "monster.h"
#include "mudpro.h"
#include "navigation.h"
#include "osd.h"
#include "parse.h"
#include "party.h"
#include "player.h"
#include "sock.h"
#include "stats.h"
#include "timers.h"
#include "terminal.h"
#include "utils.h"

#define SUBSTR_SIZE 1024

#define GET_PCRE_SUBSTR(x) \
    pcre_copy_substring (subject, ovector, strings, x, pcre_substr, SUBSTR_SIZE)

player_t *selected_player = NULL;

/* regexp substring buffer */
static gchar pcre_substr[SUBSTR_SIZE];

static gboolean parse_action_automap (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings);
static gboolean parse_action_character (parse_action_t *action, gchar *subject,
	parse_regexp_t *parse_regexp, gint *ovector, gint strings);
static gboolean parse_action_chatlog (parse_action_t *action, gchar *subject,
	parse_regexp_t *parse_regexp, gint *ovector, gint strings);
static gboolean parse_action_command (parse_action_t *action, gchar *subject,
	parse_regexp_t *parse_regexp, gint *ovector, gint strings);
static gboolean parse_action_item (parse_action_t *action, gchar *subject,
	parse_regexp_t *parse_regexp, gint *ovector, gint strings);
static gboolean parse_action_misc (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings);
static gboolean parse_action_navigation (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings);
static gboolean parse_action_player (parse_action_t *action, gchar *subject,
	parse_regexp_t *parse_regexp, gint *ovector, gint strings);
static gboolean parse_action_require (parse_action_t *action, gchar *subject,
	parse_regexp_t *parse_regexp, gint *ovector, gint strings);
static gboolean parse_action_stats (parse_action_t *action, gchar *subject,
	parse_regexp_t *parse_regexp, gint *ovector, gint strings);
static gboolean parse_action_target (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings);


/* =========================================================================
 = PARSE_ACTION_DISPATCH
 =
 = Dispatch parse actions to be executed
 ======================================================================== */

gboolean parse_action_dispatch (parse_action_t *action, gchar *subject,
	parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	g_assert (action != NULL);

	if (action->type == NULL || action->type[0] == '\0')
		return TRUE; /* invalid parse action */

	memset (&pcre_substr, 0, sizeof (pcre_substr));

	if (!strcasecmp (action->type, "Automap") && action->arg)
		return parse_action_automap (action, subject, parse_regexp, ovector, strings);

	else if (!strcasecmp (action->type, "Character") && action->arg)
		return parse_action_character (action, subject, parse_regexp, ovector, strings);

	else if (!strcasecmp (action->type, "ChatLog") && action->arg)
		return parse_action_chatlog (action, subject, parse_regexp, ovector, strings);

	else if (!strncasecmp (action->type, "Command", 7))
		return parse_action_command (action, subject, parse_regexp, ovector, strings);

	else if (!strcasecmp (action->type, "Item") && action->arg)
		return parse_action_item (action, subject, parse_regexp, ovector, strings);

	else if (!strcasecmp (action->type, "Navigation") && action->arg)
		return parse_action_navigation (action, subject, parse_regexp, ovector, strings);

	else if (!strcasecmp (action->type, "Player") && action->arg)
		return parse_action_player (action, subject, parse_regexp, ovector, strings);

	else if (!strncasecmp (action->type, "Require", 7) && action->arg)
		return parse_action_require (action, subject, parse_regexp, ovector, strings);

	else if (!strcasecmp (action->type, "Stats") && action->arg)
		return parse_action_stats (action, subject, parse_regexp, ovector, strings);

	else if (!strcasecmp (action->type, "Target") && action->arg)
		return parse_action_target (action, subject, parse_regexp, ovector, strings);

	else
		return parse_action_misc (action, subject, parse_regexp, ovector, strings);

	return TRUE;
}


/* =========================================================================
 = PARSE_ACTION_AUTOMAP
 =
 = Execute automap parse action
 ======================================================================== */

static gboolean parse_action_automap (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	exit_table_t *et;

	if (!strcasecmp (action->arg, "MovementDelete"))
	{
		automap_movement_del ();
	}

	else if (!strcasecmp (action->arg, "MovementUpdate"))
	{
		automap_movement_update ();
	}

	else if (!strcasecmp (action->arg, "SpecialMovement"))
	{
		if (automap.user_input->len == 0)
			return TRUE;

		et = g_malloc0 (sizeof (exit_table_t));
		et->long_str  = g_strdup (automap.user_input->str);
		et->direction = EXIT_SPECIAL;
		et->opposite  = EXIT_NONE;

		automap.user_input = g_string_assign (automap.user_input, "");
		automap_movement_add (et);
	}

	/* == automapper must be oriented beyond this point ================= */

	if (!automap.location || automap.lost)
		return TRUE;

	if (!strcasecmp (action->arg, "AddMovement") && action->value)
	{
		exit_table_t *et;

		GET_PCRE_SUBSTR (action->value);

		for (et = exit_table; et->long_str; et++)
		{
			if (!strcasecmp (pcre_substr, et->long_str))
			{
				automap_movement_add (et);
				return TRUE;
			}
		}
	}

	else if (!strcasecmp (action->arg, "AddRegen") && action->value)
	{
		exit_info_t *exit_info;
		automap_record_t *record;
		gint direction;

		if (action->value > PARSE_VALUE_DIRECTION)
			direction = action->value - PARSE_VALUE_DIRECTION;
		else
		{
			GET_PCRE_SUBSTR (action->value);
			direction = automap_get_exit_as_int (pcre_substr);
		}

		/* get record for the specified direction */
		if ((exit_info = automap_get_exit_info (automap.location, direction)) == NULL)
			return TRUE;

		if ((record = automap_db_lookup (exit_info->id)) == NULL)
			return TRUE;

		/* wait to see if there are enemies present */
		if (CANNOT_SEE) character.flag.blind_wait = TRUE;

		/* increment regen index */

		if (record->regen) /* wait until we've seen regen more than once */
			FlagON (record->flags, ROOM_FLAG_REGEN);

		record->regen = REGEN_RECHARGE;
	}

	/* == automapper must be enabled beyond this point ================== */

	if (!automap.enabled)
		return TRUE;

	if (!strcasecmp (action->arg, "AddSecret") && action->value)
	{
		gint direction;

		if (action->value > PARSE_VALUE_DIRECTION)
			direction = action->value - PARSE_VALUE_DIRECTION;
		else
		{
			GET_PCRE_SUBSTR (action->value);
			direction = automap_get_exit_as_int (pcre_substr);
		}

		automap_set_secret (automap.location, direction);
	}

	else if (!strcasecmp (action->arg, "Disabled"))
	{
		automap_disable ();
	}

	else if (!strcasecmp (action->arg, "KeyRequired") && action->value)
	{
		exit_info_t *exit_info;

		GET_PCRE_SUBSTR (action->value);
		exit_info = automap_get_exit_info (automap.location,
			automap_get_exit_as_int (pcre_substr));

		if (!exit_info || automap.key->str[0] == '\0')
		{
			printt ("Automap: Cannot set key!");
			return TRUE;
		}

		g_free (exit_info->str);
		exit_info->str = g_strdup (automap.key->str);
		FlagON (exit_info->flags, EXIT_FLAG_KEYREQ);
	}

	else if (!strcasecmp (action->arg, "KeyUsed") && action->value)
	{
		GET_PCRE_SUBSTR (action->value);

		if (!item_list_lookup (character.inventory, pcre_substr))
			printt ("Warning: Key used not in inventory!?");

		automap.key = g_string_assign (automap.key, pcre_substr);
	}

	else if (!strcasecmp (action->arg, "Reset"))
	{
		automap_reset (TRUE /* full reset */);
	}

	return TRUE;
}


/* =========================================================================
 = PARSE_ACTION_CHARACTER
 =
 = Execute character parse action
 ======================================================================== */

static gboolean parse_action_character (parse_action_t *action, gchar *subject,
	parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	if (!strcasecmp (action->arg, "CalibrateVitals"))
	{
		character_vitals_calibrate ();
	}

	/* ================================================================== */

	if (!action->value)
		return TRUE; /* pcre_substr required beyond this point */

	GET_PCRE_SUBSTR (action->value);

	if (!strcasecmp (action->arg, "Agility"))
		character.stat.agility = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "ArmorClass"))
		character.stat.armor_class = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Charm"))
		character.stat.charm = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Class"))
		character.stat.klass = character_class_as_int (pcre_substr);

	else if (!strcasecmp (action->arg, "DamageRes"))
		character.stat.damage_res = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "FirstName"))
	{
		g_free (character.first_name);
		character.first_name = g_strdup (pcre_substr);
	}

	else if (!strcasecmp (action->arg, "Health"))
		character.stat.health = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "HPMax"))
		character.hp.max = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "HPNow"))
		character.hp.now = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "HPUpdate"))
	{
		static gint last_value = 0;

		character.hp.now = atoi (pcre_substr);

		if (character.hp.now != last_value)
		{
			stats.lowhp = MIN (stats.lowhp, character.hp.now);
			osd_vitals_update ();
			last_value = character.hp.now;
		}
	}

	else if (!strcasecmp (action->arg, "Intellect"))
		character.stat.intellect = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "LastName"))
	{
		g_free (character.last_name);
		character.last_name = g_strdup (pcre_substr);
	}

	else if (!strcasecmp (action->arg, "Level"))
		character.stat.level = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Lives"))
		character.stat.lives = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "MagicRes"))
		character.stat.magic_res = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "MAMax"))
		character.ma.max = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "MANow"))
		character.ma.now = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "MAUpdate"))
	{
		static gint last_value = 0;

		character.ma.now = atoi (pcre_substr);

		if (character.ma.now != last_value)
		{
			if (character.ma.now > last_value
				&& character.ma.now != character.ma.max
				&& character.state != STATE_ENGAGED
				&& !command_pending (CMD_CAST))
			{
				character.tick.current = character.ma.now - last_value;
				stats.mana_tick.total += character.tick.current;
				stats.mana_tick.count++;
			}

			osd_vitals_update ();
			last_value = character.ma.now;
		}
	}

	else if (!strcasecmp (action->arg, "MartialArts"))
		character.stat.martial_arts = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Perception"))
		character.stat.perception = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Picklocks"))
		character.stat.picklocks = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Race"))
		character.stat.race = character_race_as_int (pcre_substr);

	else if (!strcasecmp (action->arg, "Rank") && action->value)
	{
		GET_PCRE_SUBSTR (action->value);

		if (!strcasecmp (pcre_substr, "back"))
			character.rank.current = RANK_BACK;

		else if (!strcasecmp (pcre_substr, "middle"))
			character.rank.current = RANK_MIDDLE;

		else if (!strcasecmp (pcre_substr, "front"))
			character.rank.current = RANK_FRONT;
	}

	else if (!strcasecmp (action->arg, "Spellcasting"))
		character.stat.spellcasting = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Stealth"))
		character.stat.stealth = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Strength"))
		character.stat.strength = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Thievery"))
		character.stat.thievery = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Tracking"))
		character.stat.tracking = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Traps"))
		character.stat.traps = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "Willpower"))
		character.stat.willpower = atoi (pcre_substr);

	return TRUE;
}


/* =========================================================================
 = PARSE_ACTION_CHATLOG
 =
 = Execute chatlog parse action
 ======================================================================== */

static gboolean parse_action_chatlog (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	GET_PCRE_SUBSTR (action->value);

	/* TODO: do something better with communications ... */
	/* perhaps integrate with external IM clients or log to an internal */
	/* conversation box ... or both ? */

	if (strstr (pcre_substr, ": @") ||
		strstr (pcre_substr, ": {") ||
		strstr (pcre_substr, "says \"@"))
		return TRUE; /* FIXME: do something better to filter @commands */

	if (!strcasecmp (action->arg, "Auction"))
	{
		temporary_chatlog_append (pcre_substr);
	}

	else if (!strcasecmp (action->arg, "Gangpath"))
	{
		temporary_chatlog_append (pcre_substr);
	}

	else if (!strcasecmp (action->arg, "Gossip"))
	{
		temporary_chatlog_append (pcre_substr);
	}

	else if (!strcasecmp (action->arg, "Spoken"))
	{
		temporary_chatlog_append (pcre_substr);
	}

	else if (!strcasecmp (action->arg, "Telepath"))
	{
		temporary_chatlog_append (pcre_substr);
	}

	return TRUE;
}


/* =========================================================================
 = PARSE_ACTION_COMMAND
 =
 = Execute command parse action
 ======================================================================== */

static gboolean parse_action_command (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	if (!strcasecmp (action->type, "CommandDel") && action->arg)
	{
		if (!strcasecmp (action->arg, "Arming"))             command_del (CMD_ARM);
		else if (!strcasecmp (action->arg, "Attacking"))     command_del (CMD_ATTACK);
		else if (!strcasecmp (action->arg, "Bashing"))       command_del (CMD_BASH);
		else if (!strcasecmp (action->arg, "Buying"))        command_del (CMD_BUY);
		else if (!strcasecmp (action->arg, "Casting"))       command_del (CMD_CAST);
		else if (!strcasecmp (action->arg, "ChangingRank"))  command_del (CMD_RANKCH);
		else if (!strcasecmp (action->arg, "CheckingParty")) command_del (CMD_PARTY);
		else if (!strcasecmp (action->arg, "Depositing"))    command_del (CMD_DEPOSIT);
		else if (!strcasecmp (action->arg, "Dropping"))      command_del (CMD_DROP);
		else if (!strcasecmp (action->arg, "Exiting"))       command_del (CMD_EXIT);
		else if (!strcasecmp (action->arg, "CheckExp"))      command_del (CMD_EXP);
		else if (!strcasecmp (action->arg, "CheckHealth"))   command_del (CMD_HEALTH);
		else if (!strcasecmp (action->arg, "CheckStats"))    command_del (CMD_STATS);
		else if (!strcasecmp (action->arg, "Hiding"))        command_del (CMD_HIDE);
		else if (!strcasecmp (action->arg, "Gangpathing"))   command_del (CMD_GANGPATH);
		else if (!strcasecmp (action->arg, "Inviting"))      command_del (CMD_INVITE);
		else if (!strcasecmp (action->arg, "Joining"))       command_del (CMD_JOIN);
		else if (!strcasecmp (action->arg, "Meditating"))    command_del (CMD_MEDITATE);
		else if (!strcasecmp (action->arg, "Moving"))        command_del (CMD_MOVE);
		else if (!strcasecmp (action->arg, "Opening"))       command_del (CMD_OPEN);
		else if (!strcasecmp (action->arg, "PickingLock"))   command_del (CMD_PICKLOCK);
		else if (!strcasecmp (action->arg, "PickingUp"))     command_del (CMD_PICKUP);
		else if (!strcasecmp (action->arg, "Removing"))	     command_del (CMD_REMOVE);
		else if (!strcasecmp (action->arg, "Resting"))       command_del (CMD_REST);
		else if (!strcasecmp (action->arg, "Scanning"))      command_del (CMD_SCAN);
		else if (!strcasecmp (action->arg, "Searching"))     command_del (CMD_SEARCH);
		else if (!strcasecmp (action->arg, "Selling"))       command_del (CMD_SELL);
		else if (!strcasecmp (action->arg, "Sneaking"))      command_del (CMD_SNEAK);
		else if (!strcasecmp (action->arg, "Speaking"))      command_del (CMD_SPEAK);
		else if (!strcasecmp (action->arg, "Targeting"))     command_del (CMD_TARGET);
		else if (!strcasecmp (action->arg, "Telepathing"))   command_del (CMD_TELEPATH);
		else if (!strcasecmp (action->arg, "Using"))         command_del (CMD_USE);
		else if (!strcasecmp (action->arg, "Withdrawing"))   command_del (CMD_WITHDRAW);
	}

	else if (!strcasecmp (action->type, "CommandRecall"))
	{
		command_recall ();
	}

	else if (!strcasecmp (action->type, "CommandSend") && action->arg)
	{
		if (!strcasecmp (action->arg, "Target"))
			command_send (CMD_TARGET);
	}

	else if (!strcasecmp (action->type, "CommandSpeak") && action->arg)
	{
		if (action->arg[0] == '!')
			command_send_va (CMD_SPEAK, "\"%s", (action->arg+1) );
		else
			command_send_va (CMD_SPEAK, action->arg);
	}

	return TRUE;
}


/* =========================================================================
 = PARSE_ACTION_ITEM
 =
 = Execute item parse action
 ======================================================================== */

static gboolean parse_action_item (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	item_t *item;

	if (!strcasecmp (action->arg, "Armed") && action->value)
	{
		GET_PCRE_SUBSTR (action->value);

		if ((item = item_list_lookup (character.inventory, pcre_substr)) != NULL)
		{
			character_equipment_disarmed ();
			item->armed = TRUE;
			if (item->flags & ITEM_FLAG_MOD_HEALTH)
				character.flag.health_read = FALSE;
			if (item->flags & ITEM_FLAG_MOD_STATS)
				character.flag.stats_read = FALSE;
		}
	}

	else if (!strcasecmp (action->arg, "AutoGet"))
	{
		character.option.get_items = CLAMP (action->value, 0, 1);
		character.option.get_items = CLAMP (action->value, 0, 1);
		printt ("AutoGet: %s",
			character.option.get_items ? "ON" : "OFF");
	}

	else if (!strcasecmp (action->arg, "Disarmed"))
	{
		if (action->value)
		{
			GET_PCRE_SUBSTR (action->value);

			item = item_list_lookup (character.inventory, pcre_substr);

			if (item)
			{
				item->armed = FALSE;
				if (item->flags & ITEM_FLAG_MOD_HEALTH)
					character.flag.health_read = FALSE;
				if (item->flags & ITEM_FLAG_MOD_STATS)
					character.flag.stats_read = FALSE;
			}
		}
		else
			character_equipment_disarmed ();
	}

	else if (!strncasecmp (action->arg, "Inventory", 9) && action->value)
	{
		GET_PCRE_SUBSTR (action->value);

		if (!strcasecmp (action->arg, "InventoryAdd"))
		{
			/* printt ("InventoryAdd -> '%s'", pcre_substr); */

			if (pcre_substr[0] == '0')
			{
				/* allowed encumbrance exceeded */
				character.option.get_items = FALSE;
				character.option.get_cash  = FALSE;
				printt ("AutoGet: OFF");
				return TRUE;
			}

			character.inventory = item_list_add (character.inventory, pcre_substr);
			item_light_sources = item_inventory_get_quantity_by_flag (
				ITEM_FLAG_USABLE | ITEM_FLAG_LIGHT);
		}

		else if (!strcasecmp (action->arg, "InventoryDel"))
		{
			/* printt ("InventoryDel -> '%s'", pcre_substr); */

			/* use multiple del, since there may be multiple items */
			character.inventory = item_list_multiple_del (
				character.inventory, pcre_substr);

			item_light_sources = item_inventory_get_quantity_by_flag (
				ITEM_FLAG_USABLE | ITEM_FLAG_LIGHT);
		}
	}

	else if (!strcmp (action->arg, "Reset"))
	{
		character.flag.inventory = FALSE;
		item_inventory_list_free ();
		item_visible_list_free ();
	}

	else if (!strncmp (action->arg, "Visible", 7) && action->value)
	{
		GET_PCRE_SUBSTR (action->value);

		if (!strcasecmp (action->arg, "VisibleAdd"))
			visible_items = item_list_add (visible_items, pcre_substr);

		else if (!strcasecmp (action->arg, "VisibleDel"))
			visible_items = item_list_del (visible_items, pcre_substr);
	}

	return TRUE;
}


/* =========================================================================
 = PARSE_ACTION_MISC
 =
 = Execute miscellaneous parse action
 ======================================================================== */

static gboolean parse_action_misc (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	if (!strcasecmp (action->type, "CleanupEvent"))
	{
		character.flag.cleanup = TRUE;
		connect_wait = character.wait.cleanup;
	}

	else if (!strcasecmp (action->type, "Combat") && action->arg)
	{
		if (!strcasecmp (action->arg, "Reset"))
		{
			if (character.state == STATE_ENGAGED)
				combat.force_break = TRUE;

			combat_reset ();
		}
	}

	else if (!strcasecmp (action->type, "DamageDelt") && action->arg)
	{
		GET_PCRE_SUBSTR (action->value);

		if (!strcasecmp (action->arg, "Single"))
		{
			combat.damage.enemy += atoi (pcre_substr);
			osd_vitals_update ();
		}
		else if (!strcasecmp (action->arg, "Room"))
		{
			combat.damage.room += atoi (pcre_substr);
			osd_vitals_update ();
		}
	}

	else if (!strcasecmp (action->type, "Disconnect"))
	{
		sockClose ();
	}

	else if (!strcasecmp (action->type, "OSDDock") && action->arg)
	{
		if (!strcasecmp (action->arg, "Restack"))
			osd_dock_restack ();
	}

	else if (!strcasecmp (action->type, "ParseCommand")
		&& action->arg && action->value)
	{
		player_t *player;
		gint msg_type;

		if (!strcasecmp (action->arg, "Gangpath"))
			msg_type = PARTY_MSG_GANGPATH;
		else if (!strcasecmp (action->arg, "Spoken"))
			msg_type = PARTY_MSG_SPOKEN;
		else if (!strcasecmp (action->arg, "Telepath"))
			msg_type = PARTY_MSG_TELEPATH;
		else
			return TRUE; /* unsupported message type */

		GET_PCRE_SUBSTR (1);
		if ((player = player_db_lookup (pcre_substr)) == NULL)
			return TRUE;

		/* GET_PCRE_SUBSTR (2); */
		GET_PCRE_SUBSTR (action->value);
		if (pcre_substr[0] == '@')
			party_parse_command (player, msg_type, pcre_substr);
		else if (pcre_substr[0] == '{')
			party_parse_response (player, msg_type, pcre_substr);
	}

	else if (!strcasecmp (action->type, "PrintTerm") && action->arg)
	{
		if (action->value)
		{
			GET_PCRE_SUBSTR (action->value);
			printt ("%s (pcre_substr '%s')", action->arg, pcre_substr);
		}
		else
			printt ("%s", action->arg);
	}

	else if (!strcasecmp (action->type, "ResetState") && action->arg)
	{
		if (!strcasecmp (action->arg, "Engaged")
			&& character.state == STATE_ENGAGED)
			character.state = STATE_NONE;

		else if (!strcasecmp (action->arg, "Resting")
			&& character.state == STATE_RESTING)
			character.state = STATE_NONE;

		else if (!strcasecmp (action->arg, "Meditating")
			&& character.state == STATE_MEDITATING)
			character.state = STATE_NONE;
	}

	else if (!strcasecmp (action->type, "RoundSync"))
	{
		combat_sync ();
	}

	else if (!strcasecmp (action->type, "SendLine") && action->arg)
	{
		send_line (action->arg);
	}

	else if (!strcasecmp (action->type, "SetFlag") && action->arg)
	{
		if (!strcasecmp (action->arg, "ExpRead"))
			character.flag.exp_read = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "HealthRead"))
			character.flag.health_read = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "InventoryRead"))
			character.flag.inventory = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "Looking"))
			character.flag.looking = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "NoSneak"))
			character.flag.no_sneak = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "Ready"))
			character.flag.ready = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "ScanRead"))
			character.flag.scan_read = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "Searching"))
			character.flag.searching = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "Sneaking"))
			character.flag.sneaking = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "StatsRead"))
			character.flag.stats_read = CLAMP (action->value, 0, 1);
	}

	else if (!strcasecmp (action->type, "SetState") && action->arg)
	{
		if (!strcasecmp (action->arg, "None"))
			character.state = STATE_NONE;

		else if (!strcasecmp (action->arg, "Engaged"))
			character.state = STATE_ENGAGED;

		else if (!strcasecmp (action->arg, "Meditating"))
			character.state = STATE_MEDITATING;

		else if (!strcasecmp (action->arg, "Resting"))
			character.state = STATE_RESTING;
	}

	else if (!strcasecmp (action->type, "SetStatus") && action->arg)
	{
		if (!strcasecmp (action->arg, "Blind"))
			character.status.blind = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "Confused"))
			character.status.confused = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "Diseased"))
			character.status.diseased = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "Held"))
			character.status.held = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "Mortal"))
			character.status.mortal = CLAMP (action->value, 0, 1);

		else if (!strcasecmp (action->arg, "Poisoned"))
			character.status.poisoned = CLAMP (action->value, 0, 1);
	}

	else if (!strcasecmp (action->type, "VitalsOSD") && action->arg)
	{
		if (!strcasecmp (action->arg, "Update") && osd_vitals.visible)
			osd_vitals_update ();
	}

	else if (!strcasecmp (action->type, "RandomTaunt"))
	{
		if (character.taunt.count > 0)
		{
			gpointer *data = NULL;
			gint r = (rand() % character.taunt.count);
			if ((data = g_slist_nth_data (character.taunt.list, r)) != NULL)
				command_send_va (CMD_SPEAK, (gchar *) data);
		}
	}

	return TRUE;
}


/* =========================================================================
 = PARSE_ACTION_NAVIGATION
 =
 = Execute navigation parse action
 ======================================================================== */

static gboolean parse_action_navigation (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	exit_info_t *exit_info;

	if (!strcasecmp (action->arg, "Autoroam"))
	{
		autoroam_opts.enabled = CLAMP (action->value, 0, 1);
	}

	else if (!strcasecmp (action->arg, "Collision"))
	{
		navigation.collisions++;

		if (navigation.collisions >= NAVIGATION_COLLISION_MAX)
		{
			printt ("Navigation halted");
			navigation_cleanup ();
			navigation.collisions = 0;
			autoroam_opts.enabled = FALSE;
		}
	}

	else if (!strcasecmp (action->arg, "DoorClosed"))
	{
		if (action->value)
		{
			GET_PCRE_SUBSTR (action->value);
			FlagOFF (automap.obvious.doors_open,
				automap_get_exit_as_int (pcre_substr));
			FlagON  (automap.obvious.doors_closed,
				automap_get_exit_as_int (pcre_substr));
		}

		else if (destination && (destination->flags & EXIT_FLAG_DOOR))
		{
			FlagOFF (automap.obvious.doors_open, destination->direction);
			FlagON  (automap.obvious.doors_closed, destination->direction);
		}
	}

	else if (!strcasecmp (action->arg, "DoorLocked") && action->value)
	{
		GET_PCRE_SUBSTR (action->value);
		FlagOFF (automap.obvious.doors_closed,
			automap_get_exit_as_int (pcre_substr));
	}

	else if (!strcasecmp (action->arg, "DoorOpened"))
	{
		if (action->value)
		{
			GET_PCRE_SUBSTR (action->value);
			FlagON  (automap.obvious.doors_open,
				 automap_get_exit_as_int (pcre_substr));
			FlagOFF (automap.obvious.doors_closed,
				 automap_get_exit_as_int (pcre_substr));
		}

		else if (destination && (destination->flags & EXIT_FLAG_DOOR))
		{
			FlagON  (automap.obvious.doors_open, destination->direction);
			FlagOFF (automap.obvious.doors_closed, destination->direction);
		}
	}

	else if (!strcasecmp (action->arg, "DoorUnlocked"))
	{
		if (action->value)
		{
			GET_PCRE_SUBSTR (action->value);
			FlagON (automap.obvious.doors_unlocked,
				 automap_get_exit_as_int (pcre_substr));
		}

		else if (destination && (destination->flags & EXIT_FLAG_DOOR))
			FlagON (automap.obvious.doors_unlocked, destination->direction);
	}

	else if (!strcasecmp (action->arg, "LookupExits") && automap.location)
	{
		GSList *node;

		memset (&automap.obvious, 0, sizeof (automap.obvious));

		for (node = automap.location->exit_list; node; node = node->next)
		{
			exit_info = node->data;

			if (exit_info->flags & EXIT_FLAG_DOOR) /* assume closed */
				FlagON (automap.obvious.doors_closed, exit_info->direction);

			else if (exit_info->flags == 0) /* normal exit */
				FlagON (automap.obvious.exits, exit_info->direction);
		}
	}

	else if (!strcasecmp (action->arg, "Reset"))
	{
		navigation_cleanup ();
	}

	else if (!strcasecmp (action->arg, "RoomDark"))
	{
		navigation.room_dark = CLAMP (action->value, 0, 1);
	}

	else if (!strcasecmp (action->arg, "SetCommand") && action->value)
	{
		gint direction;

		if (!automap.enabled || !automap.location || automap.lost ||
			!automap.user_input->len)
			return TRUE;

		if (action->value > PARSE_VALUE_DIRECTION)
			direction = action->value - PARSE_VALUE_DIRECTION;
		else
		{
			GET_PCRE_SUBSTR (action->value);
			direction = automap_get_exit_as_int (pcre_substr);
		}

		if ((exit_info = automap_get_exit_info (automap.location, direction)) != NULL)
		{
			g_free (exit_info->str);
			exit_info->str = g_strdup (automap.user_input->str);
			automap.user_input = g_string_assign (automap.user_input, "");
			FlagON (exit_info->flags, EXIT_FLAG_COMMAND);
		}
	}

	else if (!strcasecmp (action->arg, "SetSecret") && action->value)
	{
		gint direction;

		if (action->value > PARSE_VALUE_DIRECTION)
			direction = action->value - PARSE_VALUE_DIRECTION;
		else
		{
			GET_PCRE_SUBSTR (action->value);
			direction = automap_get_exit_as_int (pcre_substr);
		}

		if (direction > EXIT_NONE && direction < EXIT_SPECIAL)
			FlagON (automap.obvious.secrets, direction);
	}

	return TRUE;
}


/* =========================================================================
 = PARSE_ACTION_PLAYER
 =
 = Execute player parse action
 ======================================================================== */

static gboolean parse_action_player (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	player_t *player = NULL;

	if (action->value)
	{
		GET_PCRE_SUBSTR (action->value);
		player = player_db_lookup (pcre_substr);
	}
	else
		player = selected_player;

	if (!strcasecmp (action->arg, "AddMember") && player)
	{
		party_member_add (player);
	}

	else if (!strcasecmp (action->arg, "AddUnique") && action->value)
	{
		GET_PCRE_SUBSTR (action->value);
		player_db_add (pcre_substr);
	}

	else if (!strcasecmp (action->arg, "Deselect"))
	{
		selected_player = NULL;
	}

	else if (!strcasecmp (action->arg, "FollowerAdd") && player)
	{
		party_follower_add (player);
	}

	else if (!strcasecmp (action->arg, "FollowerDel") && player)
	{
		party_follower_del (player);
	}

	else if (!strcasecmp (action->arg, "Health") && action->value)
	{
		if (selected_player)
		{
			GET_PCRE_SUBSTR (action->value);
			selected_player->hp.p = atoi (pcre_substr);
			selected_player->hp.p = CLAMP (selected_player->hp.p, 0, 100);
		}
		else
			printt ("Player:Health requires a selected player!");
	}

	else if (!strcasecmp (action->arg, "Invited") && player)
	{
		FlagON (player->party_flags, PARTY_FLAG_INVITED);
		printt ("DEBUG: %s invited", player->name);
	}

	else if (!strcasecmp (action->arg, "Joined") && player)
	{
		FlagON  (player->party_flags, PARTY_FLAG_JOINED);
		FlagOFF (player->party_flags, PARTY_FLAG_INVITED);
		printt ("DEBUG: %s joined", player->name);
	}

	else if (!strcasecmp (action->arg, "Leader"))
	{
		/* set to player (or NULL if none set) */
		character.leader = player;
	}

	else if (!strcasecmp (action->arg, "NotJoined") && player)
	{
		FlagOFF (player->party_flags, PARTY_FLAG_JOINED);
		printt ("DEBUG: %s not joined", player->name);
	}

	else if (!strcasecmp (action->arg, "NotPresent") && player)
	{
		FlagOFF (player->party_flags, PARTY_FLAG_PRESENT);
		printt ("DEBUG: %s set to not present", player->name);
	}

	else if (!strcasecmp (action->arg, "NotReady") && player)
	{
		player_set_wait (player, TRUE /* activate */);
		printt ("DEBUG: %s set to not ready", player->name);
	}

	else if (!strcasecmp (action->arg, "Mana") && action->value)
	{
		if (selected_player)
		{
			GET_PCRE_SUBSTR (action->value);
			selected_player->ma.p = atoi (pcre_substr);
			selected_player->ma.p = CLAMP (selected_player->ma.p, 0, 100);
		}
		else
			printt ("Player:Mana requires a selected player!");
	}

	else if (!strcasecmp (action->arg, "Offline") && player)
	{
		FlagOFF (player->flags, PLAYER_FLAG_ONLINE);
	}

	else if (!strcasecmp (action->arg, "Online") && player)
	{
		FlagON (player->flags, PLAYER_FLAG_ONLINE);
	}

	else if (!strcasecmp (action->arg, "Present") && player)
	{
		g_get_current_time (&player->last_seen);
		FlagON (player->party_flags, PARTY_FLAG_PRESENT);

		printt ("DEBUG: %s set to present", player->name);
	}

	else if (!strcasecmp (action->arg, "Ready") && player)
	{
		player_set_wait (player, FALSE /* activate */);
		printt ("DEBUG: %s set to ready", player->name);
	}

	else if (!strcasecmp (action->arg, "Rollcall"))
	{
		GSList *node;

		/* prepare followers for rollcall */
		for (node = character.followers; node; node = node->next)
		{
			player = node->data;
			FlagOFF (player->party_flags, PARTY_FLAG_PRESENT);
		}

		party_member_list_free ();
		g_get_current_time (&character.rollcall);

		printt ("DEBUG: rollcall initiated");
	}

	else if (!strcasecmp (action->arg, "Select"))
	{
		/* set selected player (or NULL if none provided) */
		selected_player = player;
	}

	return TRUE;
}


/* =========================================================================
 = PARSE_ACTION_REQUIRE
 =
 = Execute require parse action
 ======================================================================== */

static gboolean parse_action_require (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	if (!strcasecmp (action->type, "Require"))
	{
		if (!strcasecmp (action->arg, "Attacking") &&
			command_pending (CMD_ATTACK) == action->value)
			return TRUE;

		else if (!strcasecmp (action->arg, "AutomapLost"))
		{
			if (CLAMP (automap.lost, 0, 1) == action->value)
				return TRUE;
			else
				return FALSE;
		}

		else if (!strcasecmp (action->arg, "Firstname") && action->value)
		{
			GET_PCRE_SUBSTR (action->value);

			if (character.first_name &&
				!strcasecmp (pcre_substr, character.first_name))
				return TRUE;
		}

		else if (!strcasecmp (action->arg, "Follower") && action->value)
		{
			player_t *player;

			GET_PCRE_SUBSTR (action->value);

			if ((player = player_db_lookup (pcre_substr)) == NULL)
				return FALSE;

			return party_follower_verify (player);
		}

		else if (!strcasecmp (action->arg, "Gangpathing") &&
			command_pending (CMD_GANGPATH) == action->value)
			return TRUE;

		else if (!strcasecmp (action->arg, "Leader") && action->value)
		{
			GET_PCRE_SUBSTR (action->value);

			if (character.leader &&
				!strcasecmp (character.leader->name, pcre_substr))
				return TRUE;
		}

		else if (!strcasecmp (action->arg, "Movement"))
		{
			if (!automap.movement &&
				!command_pending (CMD_MOVE))
				return FALSE;

			if (action->value)
			{
				exit_table_t *et;
				gint direction;
				GET_PCRE_SUBSTR (action->value);

				if ((et = automap_movement_get_next ()) == NULL)
					return FALSE;

				direction = automap_get_exit_as_int (pcre_substr);

				if (direction == et->direction)
					return TRUE;
			}
			else
				return TRUE;
		}

		else if (!strcasecmp (action->arg, "Scanning") &&
			command_pending (CMD_SCAN) == action->value)
			return TRUE;

		else if (!strcasecmp (action->arg, "Speaking") &&
			command_pending (CMD_SPEAK) == action->value)
			return TRUE;
	}

	else if (!strcasecmp (action->type, "RequireFlag"))
	{
		if (!strcasecmp (action->arg, "Ready") &&
			character.flag.ready == action->value)
			return TRUE;
	}

	else if (!strcasecmp (action->type, "RequireOption"))
	{
		if (!strcasecmp (action->arg, "AutoAll")
			&& character.option.auto_all == action->value)
			return TRUE;

		else if (!strcasecmp (action->arg, "FollowAttack")
			&& character.option.follow_attack == action->value)
			return TRUE;

		else if (!strcasecmp (action->arg, "MonsterCheck")
			&& character.option.monster_check == action->value)
			return TRUE;
	}

	else if (!strcasecmp (action->type, "RequireRoom"))
	{
		if (!strcmp (automap.room_name->str, action->arg))
			return TRUE;
	}

	else if (!strcasecmp (action->type, "RequireState"))
	{
		if (!strcasecmp (action->arg, "Engaged") &&
			character.state == STATE_ENGAGED)
			return TRUE;

		else if (!strcasecmp (action->arg, "Resting") &&
			character.state == STATE_RESTING)
			return TRUE;

		else if (!strcasecmp (action->arg, "Meditating") &&
			character.state == STATE_MEDITATING)
			return TRUE;
	}

	return FALSE;
}


/* =========================================================================
 = PARSE_ACTION_STATS
 =
 = Execute stats parse action
 ======================================================================== */

static gboolean parse_action_stats (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	if (!strcasecmp (action->arg, "AddKill"))
	{
		stats.exp.kills++;
	}

	else if (!strcasecmp (action->arg, "Missed"))
	{
		stats.misses++;
		return TRUE;
	}

	if (!action->value)
		return TRUE; /* pcre_substr required beyond this point */

	GET_PCRE_SUBSTR (action->value);

	if (!strcasecmp (action->arg, "BackstabHit"))
		stats_combat_add_damage (&stats.backstab, atoi (pcre_substr));

	else if (!strcasecmp (action->arg, "CriticalHit"))
		stats_combat_add_damage (&stats.critical, atoi (pcre_substr));

	else if (!strcasecmp (action->arg, "ExpGained"))
	{
		gint gained = atoi (pcre_substr);
		stats.exp.total  += gained;
		stats.exp.gained += gained;
		stats.exp.needed = MAX (0, stats.exp.needed - gained);
	}

	else if (!strcasecmp (action->arg, "ExpNeeded"))
		stats.exp.needed = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "ExpTotal"))
		stats.exp.total = atoi (pcre_substr);

	else if (!strcasecmp (action->arg, "ExtraHit"))
		stats_combat_add_damage (&stats.extra, atoi (pcre_substr));

	else if (!strcasecmp (action->arg, "MagicalHit"))
		stats_combat_add_damage (&stats.magical, atoi (pcre_substr));

	else if (!strcasecmp (action->arg, "NormalHit"))
		stats_combat_add_damage (&stats.normal, atoi (pcre_substr));

	return TRUE;
}


/* =========================================================================
 = PARSE_ACTION_TARGET
 =
 = Execute target parse action
 ======================================================================== */

static gboolean parse_action_target (parse_action_t *action, gchar *subject,
    parse_regexp_t *parse_regexp, gint *ovector, gint strings)
{
	monster_t *monster;
	gchar *prefix = NULL;

	if (!strncasecmp (action->arg, "Add", 3) && action->value)
	{
		GET_PCRE_SUBSTR (action->value);

		monster = monster_lookup (pcre_substr, &prefix);

		if (!monster)
			return TRUE; /* invalid target */

		if (command_pending (CMD_MOVE))
		{
			if (!autoroam_opts.enabled
				|| navigation.route
				|| !CAN_ATTACK
				|| (monster->flags & MONSTER_FLAG_AVOID))
				return TRUE; /* cannot double back, skip target */

			character.flag.double_back = TRUE;
		}
		else /* reset flag if monster followed us */
		{
			if (character.flag.double_back)
			{
				monster_target_list_free ();
				character.flag.double_back = FALSE;
			}

			if ((monster->flags & MONSTER_FLAG_AVOID) && AUTO_MOVEMENT)
			{
				if (!character.flag.avoid)
					printt ("Avoiding %s", monster->name);

				character.flag.avoid = TRUE;
				monster_target_list_free ();
				return TRUE;
			}
		}

		if (!character.option.auto_all
			|| !CAN_ATTACK
			|| character.flag.running)
			return TRUE;

		if (!strcasecmp (action->arg, "AddAlways"))
			monster_target_add (monster, prefix);

		else if (!strcasecmp (action->arg, "AddUnique") &&
			!monster_target_lookup (monster, prefix))
			monster_target_add (monster, prefix);
	}

	else if (!strcasecmp (action->arg, "Engage") && action->value)
	{
		gchar *prefix = NULL;

		GET_PCRE_SUBSTR (action->value);

		if ((monster = monster_lookup (pcre_substr, &prefix)) == NULL)
			return TRUE;

		if ((monster = monster_target_lookup (monster, prefix)) == NULL)
			return TRUE;

		monster_target_override (monster, TRUE /* re-engage */);
	}

	else if (!strcasecmp (action->arg, "Override") && action->value)
	{
		gchar *prefix = NULL;

		GET_PCRE_SUBSTR (action->value);

		if ((monster = monster_lookup (pcre_substr, &prefix)) == NULL)
			return TRUE;

		if ((monster = monster_target_lookup (monster, prefix)) == NULL)
			return TRUE;

		monster_target_override (monster, FALSE /* re-engage */);
	}

	else if (!strcasecmp (action->arg, "Remove"))
	{
		if (action->value)
		{
			GET_PCRE_SUBSTR (action->value);

			if ((monster = monster_target_get ()) == NULL)
				return TRUE;

			if (!strcasecmp (pcre_substr, monster->name))
				monster_target_del ();
		}
		else
			monster_target_del ();
	}

	else if (!strcasecmp (action->arg, "Reset"))
	{
		monster_target_list_free ();
	}

	return TRUE;
}
