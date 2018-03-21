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

#ifndef __COMMAND_H__
#define __COMMAND_H__

#include "defs.h"

#define COMMAND_TIMEOUT       15
#define COMMAND_RECALL_LIMIT  4

typedef struct
{
	GTimeVal timeout; /* command expiration timestamp */
	gchar *str;       /* command string, if any */
	gint type;        /* command type */
} command_t;

enum /* command types */
{
	CMD_NONE,		CMD_HEALTH,		CMD_EXP,
	CMD_BREAK,		CMD_TARGET,		CMD_ATTACK,
	CMD_CAST,		CMD_REST,		CMD_MOVE,
	CMD_EXIT,		CMD_MEDITATE,   CMD_SEARCH,
	CMD_BASH,		CMD_OPEN,		CMD_USE,
	CMD_INVENTORY,  CMD_PICKUP,		CMD_DROP,
	CMD_ARM,		CMD_REMOVE,		CMD_HIDE,
	CMD_SNEAK,		CMD_PICKLOCK,	CMD_STATS,
	CMD_DEPOSIT,    CMD_WITHDRAW,   CMD_BUY,
	CMD_SELL,		CMD_SPEAK,		CMD_SCAN,
	CMD_TELEPATH,   CMD_JOIN,		CMD_GANGPATH,
	CMD_RANKCH,     CMD_INVITE,		CMD_PARTY,
};

extern gint command_recalls;

void command_init (void);
void command_cleanup (void);
void command_report (FILE *fp);
void command_send (gint type);
void command_send_va (gint type, gchar *fmt, ...);
gboolean command_del (gint type);
void command_recall (void);
void command_list_free (GSList *slist);
void command_timers_update (void);
gboolean command_pending (gint type);

#endif /* __COMMAND_H__ */
