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

#include <ctype.h>
#include <string.h>

#include "automap.h"
#include "autoroam.h"
#include "combat.h"
#include "command.h"
#include "character.h"
#include "item.h"
#include "monster.h"
#include "mudpro.h"
#include "navigation.h"
#include "sockbuf.h"
#include "terminal.h"
#include "timers.h"
#include "utils.h"

character_t character;

key_value_t character_races[]=
{
	{ "Human",     RACE_HUMAN },
	{ "Dwarf",     RACE_DWARF },
	{ "Gnome",     RACE_GNOME },
	{ "Halfling",  RACE_HALFLING },
	{ "Elf",       RACE_ELF },
	{ "Half-Elf",  RACE_HALF_ELF },
	{ "Dark-Elf",  RACE_DARK_ELF },
	{ "Half-Orc",  RACE_HALF_ORC },
	{ "Goblin",    RACE_GOBLIN },
	{ "Half-Ogre", RACE_HALF_OGRE },
	{ "Kang",      RACE_KANG },
	{ "Nekojin",   RACE_NEKOJIN },
	{ "Gaunt One", RACE_GAUNT_ONE },
	{ NULL, 0 }
};

key_value_t character_classes[]=
{
	{ "Warrior",    CLASS_WARRIOR },
	{ "Witchunter", CLASS_WITCHUNTER },
	{ "Paladin",    CLASS_PALADIN },
	{ "Cleric",     CLASS_CLERIC },
	{ "Priest",     CLASS_PRIEST },
	{ "Missionary", CLASS_MISSIONARY },
	{ "Ninja",      CLASS_NINJA },
	{ "Thief",      CLASS_THIEF },
	{ "Bard",       CLASS_BARD },
	{ "Gypsy",      CLASS_GYPSY },
	{ "Warlock",    CLASS_WARLOCK },
	{ "Mage",       CLASS_MAGE },
	{ "Druid",      CLASS_DRUID },
	{ "Ranger",     CLASS_RANGER },
	{ "Mystic",     CLASS_MYSTIC },
	{ NULL, 0 }
};

static void character_options_reset (void);
static void character_options_parse (gchar *option, gchar *arguments);
static void character_triggers_add (gchar *str, gchar *response);
static void character_triggers_free (void);
static void character_triggers_execute (trigger_t *trigger);
static void character_taunts_add (gchar *str);
static void character_taunts_free (void);


/* =========================================================================
 = CHARACTER_INIT
 =
 = Initialize character
 ======================================================================== */

void character_init (void)
{
	memset (&character, 0, sizeof (character_t));

	character.input = g_string_new ("");
	character.flag.disconnected = TRUE;

	character_options_load ();

	/* command-line args override profile */
	if (args.hostname && args.hostname[0] != '\0')
	{
		g_free (character.hostname);
		character.hostname = g_strdup (args.hostname);
	}

	if (args.port)
		character.port = CLAMP (args.port, 1, 65535);

	if (args.line_style)
		character.line_style = CLAMP (args.line_style, 1, 5);
}


/* =========================================================================
 = CHARACTER_CLEANUP
 =
 = Cleanup character data
 ======================================================================== */

void character_cleanup (void)
{
	character_options_save ();
	character_options_reset ();
	command_list_free (character.commands);
	character.commands = NULL;

	g_string_free (character.input, TRUE);
}


/* =========================================================================
 = CHARACTER_OPTIONS_RESET
 =
 = Reset character data
 ======================================================================== */

static void character_options_reset (void)
{
	/* free/clear character data */

	character_triggers_free ();
	character_taunts_free ();

	g_free (character.data_path);
	character.data_path = NULL;

	g_free (character.first_name);
	character.first_name = NULL;

	g_free (character.hostname);
	character.hostname = NULL;

	g_free (character.last_name);
	character.last_name = NULL;

	g_free (character.password);
	character.password = NULL;

	g_free (character.sys_goto);
	character.sys_goto = NULL;

	g_free (character.username);
	character.username = NULL;

	memset (&character.hp_p, 0, sizeof (character.hp_p));
	memset (&character.ma_p, 0, sizeof (character.ma_p));
	memset (&character.option, 0, sizeof (character.option));

    character.flag.health_read = FALSE;
	character.flag.stats_read  = FALSE;

	/* set default options */

	character.port         = 23;
	character.wait.blind   = 6;
	character.wait.cleanup = 200;
	character.wait.connect = 10;
	character.wait.party   = 180;
	character.wait.parcmd  = 15;
	character.run_steps    = 3;
	character.line_style   = 1;
	character.light_mode   = LIGHT_RESERVE_HIGH;
	character.target_mode  = TARGET_MODE_DEFAULT;
	character.prefix       = '.';

	character.attempts.bash_door = 10;
	character.attempts.open_door = 3;
	character.attempts.pick_door = 5;

	character.option.conf_poll = TRUE;
	character.option.attack    = TRUE;
	character.option.auto_all  = TRUE;
    character.option.anti_idle = TRUE;
	character.option.recovery  = TRUE;
	character.option.movement  = TRUE;
	character.option.set_title = TRUE;

	character.option.follow_attack  = TRUE;
	character.option.restore_attack = TRUE;

	character.hp_p.rest = 75;
	character.hp_p.move = 95;
	character.hp_p.run  = 40;
	character.hp_p.min  = 25;

	character.ma_p.rest = 50;
	character.ma_p.move = 85;
	character.ma_p.run  = 0;
	character.ma_p.min  = 20;

	autoroam_opts.exits = 0;
}


/* =========================================================================
 = CHARACTER_OPTIONS_LOAD
 =
 = Loads character options
 ======================================================================== */

void character_options_load (void)
{
	gchar buf[STD_STRBUF];
	gchar *offset, *option;
	FILE *fp;

	if ((fp = fopen (mudpro_db.profile.filename, "r")) == NULL)
	{
		printt ("Unable to open %s!", mudpro_db.profile.filename);
		return;
	}

	g_get_current_time (&mudpro_db.profile.access);

	character_options_reset ();

	while (fgets (buf, sizeof (buf), fp))
	{
		strchomp (buf);

		if (!isalpha (buf[0]))
			continue;

		offset = buf;
		if ((option = get_token_as_str (&offset)) != NULL)
			character_options_parse (option, offset);
		g_free (option);
	}

	character_vitals_calibrate ();

	fclose (fp);
}


/* =========================================================================
 = CHARACTER_OPTIONS_SAVE
 =
 = Saves character options
 ======================================================================== */

void character_options_save (void)
{
}


/* =========================================================================
 = CHARACTER_OPTIONS_PARSE
 =
 = Parse character options
 ======================================================================== */

static void character_options_parse (gchar *option, gchar *arguments)
{
	gchar *tmp = NULL;
	glong value;

	if (!strcasecmp (option, "AntiIdle"))
	{
		value = get_token_as_long (&arguments);
		character.option.anti_idle = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "AutoRoamBlind"))
	{
		value = get_token_as_long (&arguments);
		autoroam_opts.roam_blind = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "AutoRoamDoors"))
	{
		value = get_token_as_long (&arguments);
		autoroam_opts.use_doors = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "AutoRoamExits"))
	{
		while ((tmp = get_token_as_str (&arguments)) != NULL)
		{
			autoroam_opts.exits |= automap_get_exit_as_int (tmp);
			g_free (tmp);
		}
	}

	else if (!strcasecmp (option, "AutoRoamSecrets"))
	{
		value = get_token_as_long (&arguments);
		autoroam_opts.use_secrets = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "AutoRoamSpecial"))
	{
		value = get_token_as_long (&arguments);
		if (CLAMP (value, 0, 1))
			FlagON  (autoroam_opts.exits, EXIT_SPECIAL);
		else
			FlagOFF (autoroam_opts.exits, EXIT_SPECIAL);
	}

	else if (!strcasecmp (option, "BashDoorAttempts"))
	{
		value = get_token_as_long (&arguments);
		character.attempts.bash_door = CLAMP (value, 3, 20);
	}

	else if (!strcasecmp (option, "BlindWait"))
	{
		value = get_token_as_long (&arguments);
		character.wait.blind = CLAMP (value, 6, 60);
	}

	else if (!strcasecmp (option, "CleanupWait"))
	{
		value = get_token_as_long (&arguments);
		character.wait.cleanup = CLAMP (value, 10, 900);
	}

	else if (!strcasecmp (option, "ConfigPoll"))
	{
		value = get_token_as_long (&arguments);
		character.option.conf_poll = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "ConnectWait"))
	{
		value = get_token_as_long (&arguments);
		character.wait.connect = CLAMP (value, 10, 900);
	}

	else if (!strcasecmp (option, "DataPath"))
	{
		if ((tmp = get_token_as_str (&arguments)) != NULL)
		{
			g_assert (!character.data_path);
			character.data_path = g_strdup (tmp);
		}
	}

	else if (!strcasecmp (option, "FollowAttack"))
	{
		value = get_token_as_long (&arguments);
		character.option.follow_attack = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "GetCash"))
	{
		value = get_token_as_long (&arguments);
		character.option.get_cash = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "GetItems"))
	{
		value = get_token_as_long (&arguments);
		character.option.get_items = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "Hangup"))
	{
		value = get_token_as_long (&arguments);
		character.option.hangup = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "Hostname"))
	{
		if ((tmp = get_token_as_str (&arguments)) != NULL)
		{
			g_assert (!character.hostname);
			character.hostname = g_strdup (tmp);
		}
	}

	else if (!strcasecmp (option, "HPMin"))
	{
		value = get_token_as_long (&arguments);
		character.hp_p.min = CLAMP (value, 0, 100);
	}

	else if (!strcasecmp (option, "HPMove"))
	{
		value = get_token_as_long (&arguments);
		character.hp_p.move = CLAMP (value, 0, 100);
	}

	else if (!strcasecmp (option, "HPRest"))
	{
		value = get_token_as_long (&arguments);
		character.hp_p.rest = CLAMP (value, 0, 100);
	}

	else if (!strcasecmp (option, "HPRun"))
	{
		value = get_token_as_long (&arguments);
		character.hp_p.run = CLAMP (value, 0, 100);
	}

	else if (!strcasecmp (option, "HPSys"))
	{
		value = get_token_as_long (&arguments);
		character.hp_p.sys = CLAMP (value, 0, 100);
	}

	else if (!strcasecmp (option, "ItemCheck"))
	{
		value = get_token_as_long (&arguments);
		character.option.item_check = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "LineStyle"))
	{
		value = get_token_as_long (&arguments);
		character.line_style = CLAMP (value, 1, 5);
		terminal_charset_init ();
		/* FIXME: need way to verify ncurses is active and refresh */
	}

	else if (!strcasecmp (option, "MAMin"))
	{
		value = get_token_as_long (&arguments);
		character.ma_p.min = CLAMP (value, 0, 100);
	}

	else if (!strcasecmp (option, "MAMove"))
	{
		value = get_token_as_long (&arguments);
		character.ma_p.move = CLAMP (value, 0, 100);
	}

	else if (!strcasecmp (option, "MARest"))
	{
		value = get_token_as_long (&arguments);
		character.ma_p.rest = CLAMP (value, 0, 100);
	}

	else if (!strcasecmp (option, "MARun"))
	{
		value = get_token_as_long (&arguments);
		character.ma_p.run = CLAMP (value, 0, 100);
	}

	else if (!strcasecmp (option, "MASys"))
	{
		value = get_token_as_long (&arguments);
		character.ma_p.sys = CLAMP (value, 0, 100);
	}

	else if (!strcasecmp (option, "MapviewOSD"))
	{
		value = get_token_as_long (&arguments);
		character.option.osd_mapview = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "Meditate"))
	{
		value = get_token_as_long (&arguments);
		character.option.meditate = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "MediTick"))
	{
		value = get_token_as_long (&arguments);
		character.tick.meditate = MAX (0, value);
	}

	else if (!strcasecmp (option, "MonsterCheck"))
	{
		value = get_token_as_long (&arguments);
		character.option.monster_check = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "OpenDoorAttempts"))
	{
		value = get_token_as_long (&arguments);
		character.attempts.open_door = CLAMP (value, 3, 6);
	}

	else if (!strcasecmp (option, "Password"))
	{
		if ((tmp = get_token_as_str (&arguments)) != NULL)
		{
			g_assert (!character.password);
			character.password = g_strdup (tmp);
		}
	}

	else if (!strcasecmp (option, "ParInterval"))
	{
		value = get_token_as_long (&arguments);
		character.wait.parcmd = CLAMP (value, 3, 60);
	}

	else if (!strcasecmp (option, "PartyWait"))
	{
		value = get_token_as_long (&arguments);
		character.wait.party = CLAMP (value, 15, 900);
	}

	else if (!strcasecmp (option, "PickLockAttempts"))
	{
		value = get_token_as_long (&arguments);
		character.attempts.pick_door = CLAMP (value, 3, 20);
	}

	else if (!strcasecmp (option, "Picklocks"))
	{
		value = get_token_as_long (&arguments);
		character.option.pick_locks = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "Prefix"))
	{
		if ((tmp = get_token_as_str (&arguments)) != NULL)
			character.prefix = tmp[0];
	}

	else if (!strcasecmp (option, "Port"))
	{
		value = get_token_as_long (&arguments);
		character.port = CLAMP (value, 1, 65535);
	}

	else if (!strcasecmp (option, "Rank"))
	{
		if ((tmp = get_token_as_str (&arguments)) != NULL)
		{
			if (!strcasecmp (tmp, "Back"))
				character.rank.desired = RANK_BACK;

			else if (!strcasecmp (tmp, "Middle"))
				character.rank.desired = RANK_MIDDLE;

			else if (!strcasecmp (tmp, "Front"))
				character.rank.desired = RANK_FRONT;
		}
	}

	else if (!strcasecmp (option, "Relog"))
	{
		value = get_token_as_long (&arguments);
		character.option.relog = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "ReserveLight"))
	{
		value = get_token_as_long (&arguments);
		character.option.reserve_light = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "ReserveLevel"))
	{
		if ((tmp = get_token_as_str (&arguments)) != NULL)
		{
			if (!strcasecmp (tmp, "Low"))
				character.light_mode = LIGHT_RESERVE_LOW;

			else if (!strcasecmp (tmp, "Medium"))
				character.light_mode = LIGHT_RESERVE_MED;

			else if (!strcasecmp (tmp, "High"))
				character.light_mode = LIGHT_RESERVE_HIGH;
		}
	}

	else if (!strcasecmp (option, "RestoreAttack"))
	{
		value = get_token_as_long (&arguments);
		character.option.restore_attack = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "Run"))
	{
		value = get_token_as_long (&arguments);
		character.option.run = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "RunSteps"))
	{
		value = get_token_as_long (&arguments);
		character.run_steps = CLAMP (value, 0, 9);
	}

	else if (!strcasecmp (option, "SetTitle"))
	{
		value = get_token_as_long (&arguments);
		character.option.set_title = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "Sneak"))
	{
		value = get_token_as_long (&arguments);
		character.option.sneak = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "StashGoods"))
	{
		value = get_token_as_long (&arguments);
		character.option.stash = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "StatsOSD"))
	{
		value = get_token_as_long (&arguments);
		character.option.osd_stats = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "SysGoto"))
	{
		value = get_token_as_long (&arguments);
		character.option.sys_goto = CLAMP (value, 0, 1);
	}

	else if (!strcasecmp (option, "SysGotoDest"))
	{
		if ((tmp = get_token_as_str (&arguments)) != NULL)
		{
			g_assert (!character.sys_goto);
			character.sys_goto = g_strdup (tmp);
		}
	}

	else if (!strcasecmp (option, "TargetMode"))
	{
        if ((tmp = get_token_as_str (&arguments)) != NULL)
        {
            if (!strcasecmp (tmp, "Default"))
                character.target_mode = TARGET_MODE_DEFAULT;
            else if (!strcasecmp (tmp, "Health"))
                character.target_mode = TARGET_MODE_HEALTH;
            else if (!strcasecmp (tmp, "LEVEL"))
                character.target_mode = TARGET_MODE_EXP;
            else if (!strcasecmp (tmp, "EXP"))
                character.target_mode = TARGET_MODE_EXP;
            else if (!strcasecmp (tmp, "FORWARD"))
                character.target_mode = TARGET_MODE_FORWARD;
            else if (!strcasecmp (tmp, "REVERSE"))
                character.target_mode = TARGET_MODE_REVERSE;
        }
	}

	else if (!strcasecmp (option, "Taunt"))
	{
		char *taunt = get_token_as_str (&arguments);

		if (taunt)
			character_taunts_add (taunt);

		g_free (taunt);
	}

	else if (!strcasecmp (option, "Trigger"))
	{
		gchar *trigger, *response;

		trigger = get_token_as_str (&arguments);
		response = get_token_as_str (&arguments);

		if (trigger && response)
			character_triggers_add (trigger, response);

		g_free (trigger);
		g_free (response);
	}

	else if (!strcasecmp (option, "Username"))
	{
		if ((tmp = get_token_as_str (&arguments)) != NULL)
		{
			g_assert (!character.username);
			character.username = g_strdup (tmp);
		}
	}

	else if (!strcasecmp (option, "VitalsOSD"))
	{
		value = get_token_as_long (&arguments);
		character.option.osd_vitals = CLAMP (value, 0, 1);
	}

	g_free (tmp);
}


/* =========================================================================
 = CHARACTER_VITALS_CALIBRATE
 =
 = Calculate values for vitals thresholds
 ======================================================================== */

void character_vitals_calibrate (void)
{
	character.hp.rest = convert_health (character.hp_p.rest);
	character.hp.move = convert_health (character.hp_p.move);
	character.hp.run  = convert_health (character.hp_p.run);
	character.hp.min  = convert_health (character.hp_p.min);
	character.hp.sys  = convert_health (character.hp_p.sys);

	character.ma.rest = convert_mana (character.ma_p.rest);
	character.ma.move = convert_mana (character.ma_p.move);
	character.ma.run  = convert_mana (character.ma_p.run);
	character.ma.min  = convert_mana (character.ma_p.min);
	character.ma.sys  = convert_mana (character.ma_p.sys);
}


/* =========================================================================
 = CHARACTER_TRIGGERS_ADD
 =
 = Add a trigger
 ======================================================================== */

static void character_triggers_add (gchar *str, gchar *response)
{
	trigger_t *trigger = NULL;

	g_assert (str != NULL);
	g_assert (response != NULL);

	trigger = g_malloc0 (sizeof (trigger_t));

	trigger->str = g_strdup (str);
	trigger->response = g_strdup (response);

	character.triggers = g_slist_append (character.triggers, trigger);
}


/* =========================================================================
 = CHARACTER_TRIGGERS_FREE
 =
 = Free allocated triggers
 ======================================================================== */

static void character_triggers_free (void)
{
	GSList *node;
	trigger_t *trigger;

	for (node = character.triggers; node; node = node->next)
	{
		trigger = node->data;
		g_free (trigger->str);
		g_free (trigger->response);
		g_free (trigger);
	}
	g_slist_free (character.triggers);
	character.triggers = NULL;
}


/* =========================================================================
 = CHARACTER_TRIGGERS_PARSE
 =
 = Parses message against triggers
 ======================================================================== */

void character_triggers_parse (gchar *str)
{
	GSList *node;
	trigger_t *trigger;

	for (node = character.triggers; node; node = node->next)
	{
		trigger = node->data;

		if (strcmp (str, trigger->str))
			continue;

		character_triggers_execute (trigger);
		return;
	}
}


/* =========================================================================
 = CHARACTER_TRIGGERS_EXECUTE
 =
 = Perform trigger response
 ======================================================================== */

static void character_triggers_execute (trigger_t *trigger)
{
	gboolean control = FALSE;
	gchar *p;

	g_assert (trigger != NULL);

	if (trigger->response[0] == '\0')
		return;

	for (p = trigger->response; p && *p; p++)
	{
		if (control)
		{
			if (*p == 'M')
				putSockN ("\r\n", 2);
			else
			{
				putSock1 ('^');
				putSock1 (*p);
			}

			control = FALSE;
			continue;
		}

		if (*p == '^')
		{
			control = TRUE;
			continue;
		}

		putSock1 (*p);
	}
}


/* =========================================================================
 = CHARACTER_TAUNTS_ADD
 =
 = Add a taunt to the list
 ======================================================================== */

static void character_taunts_add (gchar *str)
{
	gchar *taunt;

	g_assert (str != NULL);
	taunt = g_strdup (str);
	character.taunt.list = g_slist_append (character.taunt.list, taunt);
	character.taunt.count++;
}


/* =========================================================================
 = CHARACTER_TAUNTS_FREE
 =
 = Free allocated taunts
 ======================================================================== */

void character_taunts_free (void)
{
	GSList *node;

	for (node = character.taunt.list; node; node = node->next)
		g_free (node->data);

	g_slist_free (character.taunt.list);
	character.taunt.list = NULL;
	character.taunt.count = 0;
}


/* =========================================================================
 = CHARACTER_EQUIPMENT_DISARMED
 =
 = Mark weapon(s) as disarmed
 ======================================================================== */

void character_equipment_disarmed (void)
{
	GSList *node;
	item_t *item;

	for (node = character.inventory; node; node = node->next)
	{
		item = node->data;
		if (item->equip & EQUIP_FLAG_WEAPON)
			item->armed = FALSE;
	}
}


/* =========================================================================
 = CHARACTER_RACE_AS_INT
 =
 = Returns the character race as an int
 ======================================================================== */

gint character_race_as_int (gchar *str)
{
	key_value_t *kv;

	if (str)
		for (kv = character_races; kv->key; kv++)
			if (!strcasecmp (kv->key, str)) return kv->value;

	return RACE_NONE;
}


/* =========================================================================
 = CHARACTER_RACE_AS_STR
 =
 = Returns the character race as a str
 ======================================================================== */

const gchar *character_race_as_str (gint value)
{
	key_value_t *kv;

	if (value)
		for (kv = character_races; kv->key; kv++)
			if (kv->value == value) return kv->key;

	return NULL;
}


/* =========================================================================
 = CHARACTER_CLASS_AS_INT
 =
 = Returns the character class as an int
 ======================================================================== */

gint character_class_as_int (gchar *str)
{
	key_value_t *kv;

	if (str)
		for (kv = character_classes; kv->key; kv++)
			if (!strcasecmp (kv->key, str)) return kv->value;

	return CLASS_NONE;
}


/* =========================================================================
 = CHARACTER_CLASS_AS_STR
 =
 = Returns the character class as a str
 ======================================================================== */

const gchar *character_class_as_str (gint value)
{
	key_value_t *kv;

	if (value)
		for (kv = character_classes; kv->key; kv++)
			if (kv->value == value) return kv->key;

	return NULL;
}


/* =========================================================================
 = CHARACTER_DEPOSIT_CASH
 =
 = Deposit cash on hand
 ======================================================================== */

void character_deposit_cash (void)
{
	gulong amount;

	if ((amount = item_inventory_get_cash_amount ()) > 0)
		command_send_va (CMD_DEPOSIT, "%ld", amount);
}


/* =========================================================================
 = CHARACTER_SYS_GOTO
 =
 = Calls navigation sys goto and disables all movement
 ======================================================================== */

void character_sys_goto (void)
{
	autoroam_opts.enabled = FALSE;
	navigation_cleanup ();
	navigation_sys_goto ();
}
