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

#ifndef __CHARACTER_H__
#define __CHARACTER_H__

#include "defs.h"
#include "player.h"

#define AUTO_MOVEMENT (autoroam_opts.enabled || navigation.route)
#define USER_INPUT    (character.input->len > 0)
#define CANNOT_SEE    (character.status.blind || navigation.room_dark)
#define WITHIN_PARTY  (character.leader || character.followers)

#define RESTING \
	(character.state == STATE_RESTING || character.state == STATE_MEDITATING)

#define CAN_ATTACK \
	(character.option.attack || \
	(destination && (destination->flags & EXIT_FLAG_ROOMCLEAR)))

#define CAN_SNEAK \
	(character.option.sneak && !character.flag.sneaking && \
	!character.flag.no_sneak && !character.targets && !navigation.npc_present)

#define NEEDS_REST \
	(character.hp.now <= character.hp.rest || character.hp.now <= character.hp.run || \
	((navigation.flags & ROOM_FLAG_FULL_HP) && character.hp.now < character.hp.max))

#define NEEDS_MEDITATE \
	((character.ma.max && character.ma.rest && character.ma.now <= character.ma.rest) || \
	((navigation.flags & ROOM_FLAG_FULL_MA) && character.ma.now <= character.ma.max))

typedef struct
{
	gchar *first_name; /* player name */
	gchar *last_name;
	gchar *username;   /* login info */
	gchar *password;
	gchar *hostname;   /* remote server */
	gchar *data_path;  /* location data resides */
	gchar *sys_goto;   /* sys goto destination */
	gchar prefix;      /* prefix for speaking */
	GSList *triggers;  /* list of triggers/responses */
	GSList *commands;  /* command queue */
	GSList *targets;   /* list of enemies present */
	GSList *inventory; /* list of items we are carrying */
	GSList *followers; /* list of followers */
	GString *input;    /* user input buffer */
	gint port;         /* remote port */
	gint state;        /* current character state */
	gint run_steps;    /* steps to run when attempting to flee */
	gint line_style;   /* character set to use */
	gint light_mode;   /* light convservation mode */
	player_t *leader;  /* party leader */
	GTimeVal rollcall; /* time of last rollcall */

	struct /* character stats */
	{
		gint armor_class;
		gint agility;
		gint charm;
		gint char_points;
		gint damage_res;
		gint health;
		gint intellect;
		gint klass;
		gint level;
		gint lives;
		gint magic_res;
		gint martial_arts;
		gint perception;
		gint picklocks;
		gint race;
		gint spellcasting;
		gint stealth;
		gint strength;
		gint thievery;
		gint tracking;
		gint traps;
		gint willpower;
	} stat;

	struct /* mana tick */
	{
		gint meditate;
		gint current;
	} tick;

	struct /* misc wait thresholds */
	{
		gint connect; /* interval between connection attempts */
		gint cleanup; /* cleanup waiting period */
		gint blind;   /* interval between movements while blind */
		gint party;   /* amount of time to wait for followers */
		gint parcmd;  /* interval between issuing 'par' command */
	} wait;

	struct /* position in party */
	{
		gint desired; /* preferred rank */
		gint current; /* actual rank */
	} rank;

	struct /* command tolerances */
	{
		gint bash_door;
		gint open_door;
		gint pick_door;
	} attempts;

	struct /* character HP/MA stats, XX_p(ercent) used for thresholds */
	{
		gint max;
		gint now;
		gint rest;
		gint move;
		gint run;
		gint min;
		gint sys;
	} hp, ma, hp_p, ma_p;

	struct /* character status */
	{
		gboolean mortal;
		gboolean poisoned;
		gboolean diseased;
		gboolean held;
		gboolean confused;
		gboolean blind;
	} status;

	struct /* character options */
	{
		gboolean osd_context;
		gboolean osd_mapview;
		gboolean osd_stats;
		gboolean osd_vitals;
		gboolean auto_all;
		gboolean attack;
		gboolean recovery;
		gboolean movement;
		gboolean get_items;
		gboolean get_cash;
		gboolean sneak;
		gboolean run;
		gboolean relog;
		gboolean hangup;
		gboolean pick_locks;
		gboolean sys_goto;
		gboolean meditate;
		gboolean item_check;
		gboolean stash;
		gboolean conf_poll;
		gboolean reserve_light;
		gboolean set_title;
		gboolean follow_attack;
		gboolean restore_attack;
		gboolean monster_check;
		gboolean anti_idle;
	} option;

	struct /* misc character flags */
	{
		gboolean ready;
		gboolean running;
		gboolean health_read;
		gboolean exp_read;
		gboolean stats_read;
		gboolean scan_read;
		gboolean no_target;
		gboolean no_sneak;
		gboolean no_close;
		gboolean sneaking;
		gboolean cleanup;
		gboolean disconnected;
		gboolean sys_goto;
		gboolean double_back;
		gboolean inventory;
		gboolean searching;
		gboolean blind_wait;
		gboolean bash_door;
		gboolean looking;
		gboolean avoid;
	} flag;

	struct /* list of taunt messages */
	{
		GSList *list;
		gint count;
	} taunt;
} character_t;

typedef struct
{
	gchar *str;
	gchar *response;
} trigger_t;

enum /* character states */
{
	STATE_NONE,
	STATE_ENGAGED,
	STATE_RESTING,
	STATE_MEDITATING,
};

enum /* light conservation modes */
{
	LIGHT_RESERVE_LOW,
	LIGHT_RESERVE_MED,
	LIGHT_RESERVE_HIGH,
};

enum /* character races */
{
	RACE_NONE,
	RACE_HUMAN,
	RACE_DWARF,
	RACE_GNOME,
	RACE_HALFLING,
	RACE_ELF,
	RACE_HALF_ELF,
	RACE_DARK_ELF,
	RACE_HALF_ORC,
	RACE_GOBLIN,
	RACE_HALF_OGRE,
	RACE_KANG,
	RACE_NEKOJIN,
	RACE_GAUNT_ONE,
};

enum /* character classes */
{
	CLASS_NONE,
	CLASS_WARRIOR,
	CLASS_WITCHUNTER,
	CLASS_PALADIN,
	CLASS_CLERIC,
	CLASS_PRIEST,
	CLASS_MISSIONARY,
	CLASS_NINJA,
	CLASS_THIEF,
	CLASS_BARD,
	CLASS_GYPSY,
	CLASS_WARLOCK,
	CLASS_MAGE,
	CLASS_DRUID,
	CLASS_RANGER,
	CLASS_MYSTIC,
};

enum /* party ranks */
{
	RANK_MIDDLE, /* default to mid rank */
	RANK_BACK,
	RANK_FRONT,
};

extern character_t character;
extern key_value_t character_races[];
extern key_value_t character_classes[];

void character_init (void);
void character_cleanup (void);
void character_options_load (void);
void character_options_save (void);
void character_vitals_calibrate (void);
void character_triggers_parse (gchar *str);
void character_equipment_disarmed (void);
gint character_race_as_int (gchar *str);
const gchar *character_race_as_str (gint value);
gint character_class_as_int (gchar *str);
const gchar *character_class_as_str (gint value);
void character_deposit_cash (void);
void character_sys_goto (void);

#endif /* __CHARACTER_H__ */
