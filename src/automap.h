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

#ifndef __AUTOMAP_H__
#define __AUTOMAP_H__

#include "defs.h"

#define VISIBLE_DOORS \
	(automap.obvious.doors_open | automap.obvious.doors_closed)

#define VISIBLE_EXITS \
	(automap.obvious.exits | VISIBLE_DOORS)

#define SYNC_STEPS        3  /* successful steps required to sync */
#define REGEN_RECHARGE    10 /* regen recharging amount */

#define VISIBLE_EXIT(x)   (automap.obvious.exits & x)
#define VISIBLE_SECRET(x) (automap.obvious.secrets & x)
#define DOOR_OPEN(x)	  (automap.obvious.doors_open & x)
#define DOOR_CLOSED(x)    (automap.obvious.doors_closed & x)
#define DOOR_UNLOCKED(x)  (automap.obvious.doors_unlocked & x)

#define MOVEMENT_STR(x)	 exit_table[po2(x)].short_str
#define DIRECTION_STR(x) exit_table[po2(x)].long_str

typedef struct
{
	GTimeVal visited;     /* time of last visit */
	GSList *exit_list;    /* list of exits */
	gchar *id;            /* room ID */
	gchar *name;          /* room name */
	gint regen;           /* regen index */
	gulong exits;         /* visible exits (including doors) */
	gulong flags;         /* room flags */
	glong x, y, z;        /* overall XYZ position */
	glong session;        /* automapping session */
	gboolean _route_flag; /* temporary flag for constructing routes */
} automap_record_t;

typedef struct	/* automapper data */
{
	GHashTable *db;             /* automap room database */
	GSList *movement;           /* player movement queue */
	GString *room_name;         /* name of the room we're in */
	GString *key;				/* last key used */
	GString *user_input;		/* copy of last user input */
	automap_record_t *location; /* current position */
	gboolean enabled;           /* automapping status */
	gint lost;                  /* how many steps are we out of sync */
	glong x, y, z;              /* XYZ coordinates */
	glong session;				/* current session */

	struct {
		gulong exits;           /* normal exits */
		gulong secrets;         /* _visible_ secret exits */
		gulong doors_open;      /* open doors */
		gulong doors_closed;    /* closed doors */
		gulong doors_unlocked;	/* unlocked doors */
	} obvious;
} automap_t;

typedef struct
{
	gchar *id;			/* exit room ID */
	gchar *str;			/* special action string */
	gchar *required;    /* require key/item */
	gulong direction;	/* direction of exit */
	gulong flags;		/* exit flags */
} exit_info_t;

typedef struct /* exit table */
{
	gchar *long_str;
	gchar *short_str;
	gulong direction;
	gulong opposite;
	gint x, y, z;
} exit_table_t;

enum /* exits */
{
	EXIT_NONE		= 1 << 0,
	EXIT_NORTH		= 1 << 1,
	EXIT_SOUTH		= 1 << 2,
	EXIT_EAST		= 1 << 3,
	EXIT_WEST		= 1 << 4,
	EXIT_NORTHEAST	= 1 << 5,
	EXIT_NORTHWEST	= 1 << 6,
	EXIT_SOUTHEAST	= 1 << 7,
	EXIT_SOUTHWEST	= 1 << 8,
	EXIT_UP			= 1 << 9,
	EXIT_DOWN		= 1 << 10,
	EXIT_SPECIAL    = 1 << 11
};

enum /* exit types */
{
	EXIT_NORMAL,
	EXIT_SECRET,
	EXIT_DOOR_OPEN,
	EXIT_DOOR_CLOSED
};

enum /* room flags */
{
	ROOM_FLAG_REGEN		= 1 << 0, /* monster regen */
	ROOM_FLAG_SYNC		= 1 << 1, /* sync the automap */
	ROOM_FLAG_NOREST	= 1 << 2, /* do not rest here */
	ROOM_FLAG_STASH		= 1 << 3, /* stash treasure here */
	ROOM_FLAG_FULL_HP	= 1 << 4, /* HP must be full to enter */
	ROOM_FLAG_FULL_MA	= 1 << 5, /* MA must be full to enter */
	ROOM_FLAG_NOENTER   = 1 << 6, /* do not enter this room */
	ROOM_FLAG_NOROAM    = 1 << 7, /* do not roam though this room */
};

enum /* exit flags */
{
	EXIT_FLAG_EXITSTR	= 1 << 0,  /* use custom exit string */
	EXIT_FLAG_DOOR		= 1 << 1,  /* exit has a door */
	EXIT_FLAG_SECRET	= 1 << 2,  /* hidden exit, search to uncover */
	EXIT_FLAG_ONEWAY	= 1 << 3,  /* exit is one-way */
	EXIT_FLAG_KEYREQ	= 1 << 4,  /* key required */
	EXIT_FLAG_ITEMREQ   = 1 << 5,  /* item required */
	EXIT_FLAG_COMMAND	= 1 << 6,  /* invoke special command before exiting */
	EXIT_FLAG_TRAP		= 1 << 7,  /* exit has a trap */
	EXIT_FLAG_DETOUR	= 1 << 8,  /* detour required to pass exit */
	EXIT_FLAG_BLOCKED   = 1 << 9,  /* exit has been temporarily blocked */
	EXIT_FLAG_ROOMCLEAR = 1 << 10, /* room must be clear of any monsters */
	EXIT_FLAG_TOLL      = 1 << 11  /* required toll to pass (in gold) */
};

extern automap_t automap;
extern exit_table_t exit_table[];

void automap_init (void);
void automap_cleanup (void);
void automap_report (FILE *fp);
void automap_enable (void);
void automap_disable (void);
void automap_db_load (void);
automap_record_t *automap_db_lookup (gchar *id);
automap_record_t *automap_db_add_location (void);
void automap_db_save (void);
void automap_db_free (void);
void automap_db_reset (void);
void automap_parse_exits (gchar *str);
void automap_parse_user_input (gchar *str);
void automap_movement_add (exit_table_t *et);
void automap_movement_del (void);
exit_table_t *automap_movement_get_next (void);
void automap_movement_update (void);
exit_info_t *automap_add_exit_info (automap_record_t *record, gint direction);
exit_info_t *automap_get_exit_info (automap_record_t *record, gint direction);
void automap_location_merge (automap_record_t *original, automap_record_t *duplicate);
void automap_duplicate_merge (void);
void automap_set_location (automap_record_t *location);
void automap_insert_location (void);
void automap_remove_location (void);
void automap_adjust_xyz (gint xyz, gint adj);
void automap_reset (gboolean full_reset);
gint automap_get_exit_as_int (gchar *str);
void automap_set_secret (automap_record_t *record, gint direction);

#endif /* __AUTOMAP_H__ */
