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

#include "autoroam.h"
#include "character.h"
#include "client_ai.h"
#include "combat.h"
#include "command.h"
#include "guidebook.h"
#include "item.h"
#include "mapview.h"
#include "monster.h"
#include "navigation.h"
#include "osd.h"
#include "party.h"
#include "sock.h"
#include "spells.h"
#include "terminal.h"
#include "timers.h"
#include "utils.h"

/* copy of destination exit_flags that we can modify */
static gulong exit_flags = 0;

static struct /* client_ai_open_door counters */
{
	gint bash_door;
	gint open_door;
	gint pick_door;
} attempts;

static gboolean client_ai_party_request (void);
static gboolean client_ai_get_movement (gboolean backtrack);
static void client_ai_exec_movement (void);
static void client_ai_open_door (void);


/* =========================================================================
 = CLIENT_AI
 =
 = Perform client AI
 = NOTE: Returns after each action to maintain proper serialization
 ======================================================================== */

void client_ai (void)
{
	if (character.commands || USER_INPUT
		|| !character.option.auto_all
		|| !character.flag.ready)
		return;

	/* == health ======================================================== */

	if (character.flag.health_read)
	{
		if (character.option.sys_goto && !character.flag.sys_goto &&
			((character.hp.max && character.hp.now <= character.hp.sys) ||
			(character.ma.max && character.ma.now <= character.ma.sys)))
		{
			printt ("Vitals Critical (Teleporting)");

			client_ai_movement_reset ();
			navigation_route_free ();

			if (!automap.enabled && !automap.lost && automap.location)
				navigation_anchor_add (automap.location->id, FALSE /* clear list */);

			navigation_sys_goto ();
			return;
		}

		if (character.hp.now <= character.hp.min && character.option.hangup)
		{
			printt ("Health Critical (Disconnecting)");

			character.flag.disconnected = TRUE;
			sockClose ();
			return;
		}

		if (character.flag.running)
		{
			if (!automap.location || automap.lost ||
				navigation.steps_ran == character.run_steps)
			{
				client_ai_movement_reset ();
				return;
			}

			if (character.targets)
				monster_target_list_free ();

			if (character.state == STATE_ENGAGED)
			{
				combat.force_break = TRUE;
				combat_reset ();
				return;
			}

			if (client_ai_get_movement (NAVIGATE_BACKWARD))
				client_ai_exec_movement ();

			return;
		}
		else if (character.hp.now <= character.hp.run
			&& automap.location
			&& !automap.lost
			&& character.run_steps
			&& character.targets
			&& character.option.movement
			&& character.option.run)
		{
			printt ("Health Critical (Running)");

			client_ai_movement_reset ();
			character.flag.running = TRUE;
			navigation.steps_ran   = 0;
			return;
		}
	}

	/* == misc commands ================================================= */

	if (character.flag.cleanup && !character.targets)
	{
		command_send (CMD_EXIT);
		return;
	}

	if (!character.flag.health_read)
	{
		command_send (CMD_HEALTH);
		return;
	}

	if (!character.flag.exp_read)
	{
		command_send (CMD_EXP);
		return;
	}

	if (!character.flag.stats_read)
	{
		command_send (CMD_STATS);
		return;
	}

	if (!character.flag.inventory)
	{
		command_send (CMD_INVENTORY);
		return;
	}

	if (!character.flag.scan_read)
	{
		command_send (CMD_SCAN);
		return;
	}

	/* == spellcasting ================================================== */

	if (character.ma.max && character.ma.now && !CASTWAIT
		/* && character.rollcall.tv_sec > 1 */)
	{
		if (spellcasting ()) return; /* command pending */
	}

#ifdef NEW_SPELLCASTING
	else
		printt ("DEBUG: ma.max -> %d  ma.now -> %d  castwait -> %d  rollcall -> %d",
			character.ma.max, character.ma.now, CASTWAIT, character.rollcall.tv_sec);
#endif

	/* == party ========================================================= */

	if (character.leader
		&& character.rank.current != character.rank.desired)
	{
		switch (character.rank.desired)
		{
		case RANK_BACK:
			command_send_va (CMD_RANKCH, "backrank");
			break;
		case RANK_MIDDLE:
			if (character.rank.current == RANK_BACK)
				/* move to frontrank first to avoid "only one in backrank" msg */
				command_send_va (CMD_RANKCH, "frontrank");
			else
				command_send_va (CMD_RANKCH, "midrank");
			break;
		case RANK_FRONT:
			command_send_va (CMD_RANKCH, "frontrank");
			break;
		default:
			g_assert_not_reached ();
		}

		if (character.commands)
			return;
	}

	if (WITHIN_PARTY && client_ai_party_request ())
		return;

	/* == combat ======================================================== */

	if (character.targets && CAN_ATTACK
		&& !character.flag.double_back
		&& !character.flag.avoid)
	{
		strategy_t *strategy = combat_strategy_get_next ();

		if (strategy != combat.strategy.current)
		{
			if ((character.state == STATE_ENGAGED || combat.room)
				&& g_timer_elapsed (timers.round, NULL) < 1)
				return;

			if (combat.strategy.current)
				combat.rounds = 0;

			if (!character.state == STATE_ENGAGED)
				osd_vitals_update ();

			combat_strategy_execute (strategy);
			return;
		}
	}

	if (!CAN_ATTACK /* !character.option.attack */
		&& character.state == STATE_ENGAGED)
	{
		/* disengage if auto-attack has been disabled */
		monster_target_list_free ();
		combat.force_break = TRUE;
		combat_reset ();
		return;
	}

	/* == visible items ================================================= */

	if (visible_items)
	{
		gint quantity = 0;
		item_t *item = item_pickup_next (&quantity);

		if (item && quantity)
		{
			if (item->flags & ITEM_FLAG_CURRENCY)
				command_send_va (CMD_PICKUP, "%d %s", quantity, item->name);
			else
				command_send_va (CMD_PICKUP, "%s", item->name);
			return;
		}
	}

	if (character.state == STATE_ENGAGED)
	{
		/* conserve light if necessary */
		if (character.option.reserve_light && !automap.lost &&
			(character.light_mode > LIGHT_RESERVE_LOW || item_light_sources == 1))
			item_light_source_deactivate ();

		if (character.flag.avoid)
		{
			command_send (CMD_BREAK);
			return;
		}

		return; /* do not proceed beyond this point while engaged */
	}

	/* == inventory ===================================================== */

	if (character.inventory)
	{
		item_inventory_manage ();

		if (character.commands)
			return;
	}

	/* == resting ======================================================= */

	if (!destination && character.option.movement)
	{
		/* get next destination ASAP and check rest flag */

		if (character.flag.double_back)
		{
			/* revisiting previous location */
			if (!client_ai_get_movement (NAVIGATE_BACKWARD))
				character.flag.double_back = FALSE;
		}
		else
			client_ai_get_movement (NAVIGATE_FORWARD);

		if (/* character.targets && */
			(destination && (destination->flags & EXIT_FLAG_ROOMCLEAR)))
		{
			printt ("Must clear room to continue");
			command_send (CMD_TARGET);
			return;
		}

		if ((navigation.flags & ROOM_FLAG_FULL_HP)
			&& character.hp.now < character.hp.max)
			printt ("Resting to full health");

		else if ((navigation.flags & ROOM_FLAG_FULL_MA)
			&& character.ma.now < character.ma.max)
			printt ("Resting to full mana");
	}

	/* rest health if needed */
	if (character.option.recovery && NEEDS_REST
		&& character.state != STATE_RESTING
		&& !character.status.poisoned
		&& !character.flag.avoid)
	{
		if (automap.location &&
			(automap.location->flags & ROOM_FLAG_NOREST))
		{
			/* cannot rest here, try previous location */
			client_ai_movement_reset ();

			if (client_ai_get_movement (NAVIGATE_BACKWARD))
			{
				printt ("Cannot rest at this location");
				client_ai_exec_movement ();
				return;
			}
			/* if we can't move for some reason, go ahead and rest */
		}
		command_send (CMD_REST);
		return;
	}

	/* rest mana if needed */
	if (character.option.recovery && NEEDS_MEDITATE
		&& character.state != STATE_RESTING
		&& character.state != STATE_MEDITATING
		&& !character.status.poisoned
		&& !character.flag.avoid)
	{
		if (automap.location &&
			(automap.location->flags & ROOM_FLAG_NOREST))
		{
			/* cannot rest here, try previous location */
			client_ai_movement_reset ();

			if (client_ai_get_movement (NAVIGATE_BACKWARD))
			{
				printt ("Cannot meditate at this location");
				client_ai_exec_movement ();
				return;
			}
			/* if we can't move for some reason, go ahead and meditate */
		}

		/* use meditate if possible or fall back on resting */
		command_send (character.option.meditate ? CMD_MEDITATE : CMD_REST);
		return;
	}

	if (character.state == STATE_RESTING)
	{
		/* do not pass while resting */

		if ((navigation.flags & ROOM_FLAG_FULL_HP)
			&& character.hp.now < character.hp.max)
			return; /* full health required */

		if (character.hp.now < character.hp.move ||
			(character.ma.now < character.ma.move && !character.option.meditate))
			return;

		character.state = STATE_NONE;
		return;
	}

	if (character.state == STATE_MEDITATING)
	{
		/* do not pass while meditating */

		if ((navigation.flags & ROOM_FLAG_FULL_MA)
			&& character.ma.now < character.ma.max)
			return; /* full mana required */

		if (character.ma.now < character.ma.move)
			return;

		character.state = STATE_NONE;
		return;
	}

	if (automap.movement)
		return; /* do not move while user movement is pending */

	if (destination) /* carry out selected movement */
	{
		client_ai_exec_movement ();
		return;
	}

	if (CAN_SNEAK) /* otherwise initiate sneaking */
	{
		command_send (CMD_SNEAK);
		return;
	}
}


/* =========================================================================
 = CLIENT_AI_PARTY_REQUEST
 =
 = Handle requesting assistance from party
 ======================================================================== */

gboolean client_ai_party_request (void)
{
	if (character.leader)
	{
		if (NEEDS_REST || NEEDS_MEDITATE)
		{
			if (party_send_request (PARTY_REQ_WAIT, "Need Rest",
				PARTY_MSG_TELEPATH,	character.leader))
				return TRUE;
		}
		else if (character.status.poisoned)
		{
			if (party_send_request (PARTY_REQ_WAIT, "Poisoned",
				PARTY_MSG_TELEPATH, character.leader))
				return TRUE;
		}
		else if (character.status.diseased)
		{
			if (party_send_request (PARTY_REQ_WAIT, "Diseased",
				PARTY_MSG_TELEPATH, character.leader))
				return TRUE;
		}
		else if (character.status.held)
		{
			if (party_send_request (PARTY_REQ_WAIT, "Held",
				PARTY_MSG_TELEPATH, character.leader))
				return TRUE;
		}
		else if (character.status.mortal)
		{
			if (party_send_request (PARTY_REQ_WAIT, "Mortally Wounded",
				PARTY_MSG_TELEPATH, character.leader))
				return TRUE;
		}
		else
		{
			party_request_t *request = &request_table[PARTY_REQ_WAIT];

			if (request->pending.tv_sec)
			{
				memset (&request->pending, 0, sizeof (GTimeVal));
				command_send_va (CMD_TELEPATH, "%s @ok", character.leader->name);
			}
		}
	}
	else
	{
		party_follower_manage ();
		
		if (character.commands)
			return TRUE;
	}

	if (character.hp.now <= character.hp.rest)
	{
		if (party_send_request (PARTY_REQ_HEAL, NULL,
			PARTY_MSG_SPOKEN, NULL))
			return TRUE;
	}

	if (character.status.blind)
	{
		if (party_send_request (PARTY_REQ_BLIND, NULL,
			PARTY_MSG_SPOKEN, NULL))
			return TRUE;
	}

	if (character.status.poisoned)
	{
		if (party_send_request (PARTY_REQ_POISONED, NULL,
			PARTY_MSG_SPOKEN, NULL))
			return TRUE;
	}

	if (character.status.diseased)
	{
		if (party_send_request (PARTY_REQ_DISEASED, NULL,
			PARTY_MSG_SPOKEN, NULL))
			return TRUE;
	}

	if (character.status.held)
	{
		if (party_send_request (PARTY_REQ_HELD, NULL,
			PARTY_MSG_SPOKEN, NULL))
			return TRUE;
	}

	return FALSE;
}


/* =========================================================================
 = CLIENT_AI_GET_MOVEMENT
 =
 = Determine next movement
 ======================================================================== */

static gboolean client_ai_get_movement (gint mode)
{
	if (destination
		|| automap.enabled
		|| mapview_flags_visible
		|| (character.hp.now <= character.hp.sys && character.option.sys_goto)
		|| (character.targets && !character.flag.double_back)
		|| character.status.diseased
		|| character.status.mortal
		|| character.status.poisoned
		|| character.status.held)
		return FALSE;

	if (!party_follower_group_ready ())
		return FALSE;

	character.flag.sys_goto = FALSE;

	/* create route if we have anchors pending */
	if (navigation.anchors && !navigation.route
		&& automap.location && !automap.lost)
	{
		navigation_create_route ();
		return FALSE; /* flush out any/all invalid anchors */
	}

	if (navigation.route)
		destination = navigation_route_next (mode);
	else if (navigation.anchors && automap.lost)
		destination = navigation_autoroam_lost ();
	else if (character.flag.running
		|| (autoroam_opts.enabled && autoroam_opts.exits))
	{
		destination = navigation_autoroam (mode);

		/* reset timestamp when backtracking so we come back this way */
		if (destination
			&& (mode == NAVIGATE_BACKWARD)
			&& automap.location	&& !automap.lost
			&& (automap.location->exits & ~destination->direction))
			memset (&automap.location->visited, 0, sizeof (GTimeVal));
	}

	if (destination)
	{
		/* keep local copy that we can modify */
		exit_flags = destination->flags;
		return TRUE;
	}

	return FALSE;
}


/* =========================================================================
 = CLIENT_AI_EXEC_MOVEMENT
 =
 = Execute the selected movement
 ======================================================================== */

static void client_ai_exec_movement (void)
{
	if (!destination)
		return; /* nothing to do */

	if (!character.option.movement)
	{
		client_ai_movement_reset ();
		return; /* movement has been disabled */
	}

	if (DOOR_CLOSED (destination->direction))
	{
		client_ai_open_door ();
		return;
	}

	/* exit has a special exit command */
	if (exit_flags & EXIT_FLAG_COMMAND)
	{
		send_line (destination->str);
		FlagOFF (exit_flags, EXIT_FLAG_COMMAND);
		return;
	}

	/* handle hidden (searchable) exits */
	if ((destination->flags & EXIT_FLAG_SECRET) &&
		!(destination->flags & EXIT_FLAG_COMMAND) &&
		(destination->direction != EXIT_SPECIAL) &&
		!VISIBLE_SECRET (destination->direction))
	{
		if (navigation.room_dark) /* light source mandatory */
		{
			if (item_light_source_activate ())
			{
				/* also check if exit has already been uncovered */
				command_send (CMD_TARGET);
				character.flag.searching = TRUE;
				return;
			}
			else
			{
				printt ("No light source available! (Movement Disabled)");
				navigation_cleanup ();
				autoroam_opts.enabled = FALSE;
				return;
			}
		}
		command_send_va (CMD_SEARCH, "%s",
			MOVEMENT_STR (destination->direction));

		return;
	}

	character.flag.searching = FALSE;
	
	/* TODO: handle detouring for hidden non-searchable exits */

	/* activate light source if conservation mode allows it */
	if (navigation.room_dark && item_light_sources > 1 &&
		character.light_mode < LIGHT_RESERVE_HIGH)

	if (navigation.room_dark &&
		(!character.option.reserve_light ||
		(item_light_sources > 1 && character.light_mode < LIGHT_RESERVE_HIGH)))
	{
		if (item_light_source_activate ())
			return; /* command pending */

		/* unable to activate light source, will have to walk blindly */
	}

	if (CANNOT_SEE) /* handle moving while blinded */
	{
		GTimeVal tv;

		if (!autoroam_opts.roam_blind || !automap.location || automap.lost)
			return;

		/* wait for a moment to see what enemies are present */
		if (character.flag.blind_wait ||
			(automap.location->flags & ROOM_FLAG_REGEN))
		{
			static gboolean blind_wait_notice = FALSE;

			g_get_current_time (&tv);

			if (tv.tv_sec <
				(automap.location->visited.tv_sec + character.wait.blind))
			{
				if (!blind_wait_notice)
				{
					printt ("Waiting: Who Wants Some?");
					blind_wait_notice = TRUE;
				}
				return;
			}
			character.flag.blind_wait = FALSE;
			blind_wait_notice = FALSE;
		}
	}

	if (character.flag.double_back)
	{
		printt ("Autoroam: Doubling Back");
		character.flag.double_back = FALSE;
		if (CANNOT_SEE) character.flag.blind_wait = TRUE;
	}

	if (CAN_SNEAK)
	{
		command_send (CMD_SNEAK);
		return;
	}

	/* execute movement */
	command_send_va (CMD_MOVE, (exit_flags & EXIT_FLAG_EXITSTR) ?
		destination->str : MOVEMENT_STR (destination->direction));

}


/* =========================================================================
 = CLIENT_AI_MOVEMENT_RESET
 =
 = Reset client ai movement data
 ======================================================================== */

void client_ai_movement_reset (void)
{
	destination = NULL;
	character.flag.running = FALSE;
	exit_flags = 0;
	navigation.flags = 0;
}


/* =========================================================================
 = CLIENT_AI_OPEN_DOOR
 =
 = Handle opening doors
 ======================================================================== */

static void client_ai_open_door (void)
{
	if (exit_flags & EXIT_FLAG_DETOUR)
	{
		navigation_detour ();
		return;
	}

	if ((exit_flags & EXIT_FLAG_KEYREQ) &&
		!DOOR_UNLOCKED (destination->direction))
	{
		item_t *item = item_list_lookup (character.inventory, destination->str);

		if (item)
		{
			command_send_va (CMD_USE, "%s %s", destination->str,
				MOVEMENT_STR (destination->direction));
		}
		else
		{
			if (destination->str)
				printt ("Required key '%s' not in inventory!", destination->str);
			else
				printt ("Required key flag set, but key not specified!");

			/* we don't have the required key, so try plan B ... */
			FlagOFF (exit_flags, EXIT_FLAG_KEYREQ);
		}
		return;
	}

	if (character.stat.picklocks &&
		!DOOR_UNLOCKED (destination->direction))
	{
		if (attempts.pick_door >= character.attempts.pick_door)
		{
			printt ("Failed to pick lock (already unlocked?)");
			/* assume door is unlocked (will fall back if necessary) */
			FlagON (automap.obvious.doors_unlocked, destination->direction);
			attempts.pick_door = 0;
			return;
		}

		command_send_va (CMD_PICKLOCK, "%s", MOVEMENT_STR (destination->direction));
		attempts.pick_door++;
		return;
	}

	if (DOOR_UNLOCKED (destination->direction) &&
		 DOOR_CLOSED (destination->direction) && !character.flag.bash_door)
	{
		if (attempts.open_door >= character.attempts.open_door)
		{
			printt ("Failed to open door, bashing instead");
			character.flag.bash_door = TRUE;
			attempts.open_door = 0;
			return;
		}

		command_send_va (CMD_OPEN, "%s", MOVEMENT_STR (destination->direction));
		attempts.open_door++;
		return;
	}

	if (attempts.bash_door >= character.attempts.bash_door)
	{
		if (navigation.route)
		{
			printt ("Failed to bash door, route cancelled!");
			navigation_cleanup ();
		}

		if (autoroam_opts.enabled)
		{
			printt ("Failed to bash door, blocking exit");
			FlagON (destination->flags, EXIT_FLAG_BLOCKED);
			/* TODO: need to schedule exit unblocking */
		}

		client_ai_movement_reset ();
		character.flag.bash_door = FALSE;
		attempts.bash_door = 0;
		return;
	}

	command_send_va (CMD_BASH, "%s", MOVEMENT_STR (destination->direction));
	attempts.bash_door++;
}


/* =========================================================================
 = CLIENT_AI_OPEN_DOOR_RESET
 =
 = Reset client AI door opening counters
 ======================================================================== */

void client_ai_open_door_reset (void)
{
	attempts.bash_door = 0;
	attempts.open_door = 0;
	attempts.pick_door = 0;
}
