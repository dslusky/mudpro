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

#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "defs.h"

#define REMOTE_ACCESS(x, y) \
	((x->remotes & REMOTE_FLAG_TRUSTED) || (x->remotes & y))

typedef struct
{
	/* player info */
	gchar *name;
	gint relation;
	glong flags;
	GTimeVal last_seen;
	GTimeVal party_wait;

	/* party info */
	glong party_flags;
	glong remotes;

	struct /* player HP/MA */
	{
		gint max;
		gint now;
		gint p; /* percent */
	} hp, ma;

	struct /* player status */
	{
		gboolean mortal;
		gboolean poisoned;
		gboolean diseased;
		gboolean held;
		gboolean confused;
		gboolean blind;
	} status;
} player_t;

enum /* player relation */
{
	PLAYER_RELATION_UNKNOWN = 1 << 0,
	PLAYER_RELATION_FRIEND  = 1 << 1,
	PLAYER_RELATION_ENEMY   = 1 << 2,
	PLAYER_RELATION_AVOID   = 1 << 3,
	PLAYER_RELATION_HANGUP  = 1 << 4,
};

enum /* player flags */
{
	PLAYER_FLAG_ONLINE = 1 << 0,
	PLAYER_FLAG_INVITE = 1 << 1,
	PLAYER_FLAG_JOIN   = 1 << 2,
};

enum /* party flags */
{
	PARTY_FLAG_INVITED = 1 << 0,
	PARTY_FLAG_JOINED  = 1 << 1,
	PARTY_FLAG_PRESENT = 1 << 2,
	PARTY_FLAG_WAIT    = 1 << 3,
};

enum /* player remotes */
{
	REMOTE_FLAG_TRUSTED  = 1 << 0,
	REMOTE_FLAG_INFO     = 1 << 1,
	REMOTE_FLAG_ASSIST   = 1 << 2,
	REMOTE_FLAG_COMMANDS = 1 << 3,
	REMOTE_FLAG_SETTINGS = 1 << 4,
	REMOTE_FLAG_CONTROL  = 1 << 5,
};

extern key_value_t player_relation[];
extern key_value_t player_flags[];
extern key_value_t remote_flags[];

void player_db_init (void);
void player_db_cleanup (void);
void player_db_load (void);
void player_db_save (void);
player_t *player_db_lookup (const gchar *name);
player_t *player_db_add (const gchar *name);
void player_set_wait (player_t *player, gboolean activate);

#endif /* __PLAYER_H__ */
