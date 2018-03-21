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

#ifndef __PARTY_H__
#define __PARTY_H__

#include "defs.h"
#include "player.h"

#define PARTY_COMMAND_OK      "ok"
#define PARTY_COMMAND_INVALID "command invalid or not allowed"

#define PARTY_REQUEST_TIMEOUT 10

typedef struct
{
	gint type;
	gchar *command;
	GTimeVal pending;
} party_request_t;

enum /* party request types */
{
	PARTY_REQ_NONE,
	PARTY_REQ_JOIN,
	PARTY_REQ_WAIT,
	PARTY_REQ_HEAL,
	PARTY_REQ_BLIND,
	PARTY_REQ_POISONED,
	PARTY_REQ_DISEASED,
	PARTY_REQ_HELD,
};

enum /* supported message types */
{
	PARTY_MSG_NONE,
	PARTY_MSG_GANGPATH,
	PARTY_MSG_SPOKEN,
	PARTY_MSG_TELEPATH,
};

extern party_request_t request_table[];
extern GSList *party_members;

void party_init (void);
void party_cleanup (void);
gboolean party_send_request (gint req_type, const gchar *reason,
	gint msg_type, player_t *player);
void party_request_update (void);
void party_send_response (player_t *player, gint type, const gchar *fmt, ...);
void party_parse_command (player_t *player, gint type, const gchar *str);
void party_parse_response (player_t *player, gint type, const gchar *str);
void party_follower_list_free (void);
void party_follower_add (player_t *player);
void party_follower_del (player_t *player);
player_t *party_follower_search_flags (gulong flags);
gboolean party_follower_verify (player_t *player);
void party_follower_manage (void);
gboolean party_follower_group_ready (void);
void party_member_add (player_t *player);
void party_member_list_free (void);

#endif /* __PARTY_H__ */
