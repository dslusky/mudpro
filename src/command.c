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

#include <stdio.h>

#include "automap.h"
#include "autoroam.h"
#include "character.h"
#include "client_ai.h"
#include "combat.h"
#include "command.h"
#include "item.h"
#include "monster.h"
#include "mudpro.h"
#include "navigation.h"
#include "timers.h"
#include "terminal.h"
#include "utils.h"

#define COMMAND_HISTORY_MAX 10

/* track command recalls (failures) */
gint command_recalls;       /* for the current period */
gint command_recalls_total; /* the total amount */

static key_value_t command_types[]=
{
	{ "None",            CMD_NONE },     { "Checking Health",    CMD_HEALTH },
	{ "Checking Exp",    CMD_EXP },      { "Breaking",           CMD_BREAK },
	{ "Targeting",       CMD_TARGET },   { "Attacking",          CMD_ATTACK },
	{ "Casting",         CMD_CAST },     { "Resting",            CMD_REST },
	{ "Moving",          CMD_MOVE },     { "Exiting",            CMD_EXIT },
	{ "Meditating",      CMD_MEDITATE }, { "Searching",          CMD_SEARCH },
	{ "Bashing",         CMD_BASH },     { "Opening",            CMD_OPEN },
	{ "Using",           CMD_USE },      { "Checking Inventory", CMD_INVENTORY },
	{ "Picking Up",      CMD_PICKUP },   { "Dropping",           CMD_DROP },
	{ "Arming",          CMD_ARM },      { "Removing",           CMD_REMOVE },
	{ "Hiding",          CMD_HIDE },     { "Sneaking",           CMD_SNEAK },
	{ "Picking Lock",    CMD_PICKLOCK }, { "Checking Stats",     CMD_STATS },
	{ "Depositing Cash", CMD_DEPOSIT },  { "Withdrawing Cash",   CMD_WITHDRAW },
	{ "Buying Item",     CMD_BUY },      { "Selling Item",       CMD_SELL },
	{ "Speaking",        CMD_SPEAK },    { "Scanning Players",   CMD_SCAN },
	{ "Telepathing",     CMD_TELEPATH }, { "Joining Party",      CMD_JOIN },
	{ "Gangpathing",     CMD_GANGPATH }, { "Changing Rank",      CMD_RANKCH },
	{ "Inviting",        CMD_INVITE },   { "Checking Party",     CMD_PARTY },
	{ NULL, 0 }
};

static GSList *command_history;
static void command_execute (gint type, gchar *str);
static void command_history_remove_last (void);


/* =========================================================================
 = COMMAND_INIT
 =
 = Initialize command module
 ======================================================================== */

void command_init (void)
{
	command_recalls = command_recalls_total = 0;
	command_history = NULL;
}


/* =========================================================================
 = COMMAND_CLEANUP
 =
 = Cleanup after command module
 ======================================================================== */

void command_cleanup (void)
{
	command_list_free (command_history);
	command_history = NULL;
}


/* =========================================================================
 = COMMAND_REPORT
 =
 = Report current command status to specified file
 ======================================================================== */

void command_report (FILE *fp)
{
	GSList *node;
	command_t *command;

	fprintf (fp, "\nCOMMAND MODULE\n==============\n\n");

	fprintf (fp, "  Command Recalls:\n");
	fprintf (fp, "    Current .. %d\n", command_recalls);
	fprintf (fp, "    Total .... %d\n", command_recalls_total);

	if (character.commands)
	{
		gint count = 1;

		fprintf (fp, "\n  Commands Pending:\n\n");

		for (node = character.commands; node; node = node->next)
		{
			command = node->data;

			fprintf (fp, "    [%2d] %s", count++,
				command_types[command->type].key);

			if (command->str)
				fprintf (fp, " (%s)", command->str);

			fprintf (fp, "\n");
		}
	}

	if (command_history)
	{
		gint count = 1;

		fprintf (fp, "\n  Last %d Commands:\n\n",
			g_slist_length (command_history));

		for (node = command_history; node; node = node->next)
		{
			command = node->data;

			fprintf (fp, "    [%2d] %s", count++,
				command_types[command->type].key);

			if (command->str)
				fprintf (fp, " (%s)", command->str);

			fprintf (fp, "\n");
		}
	}

	fprintf (fp, "\n");
}


/* =========================================================================
 = COMMAND_EXECUTE
 =
 = Sends command to the server
 ======================================================================== */

static void command_execute (gint type, gchar *str)
{
	command_t *command;
	gchar *buf;

	if (!character.option.auto_all || USER_INPUT || !character.flag.ready)
		return;

	command = g_malloc0 (sizeof (command_t));

	if (str) command->str = g_strdup (str);

	g_get_current_time (&command->timeout);
	command->timeout.tv_sec += COMMAND_TIMEOUT;

	command->type = type;

	switch (type)
	{
	case CMD_ARM:       buf = g_strdup_printf ("arm %s", str);    break;
	case CMD_BASH:      buf = g_strdup_printf ("bash %s", str);   break;
	case CMD_BREAK:     buf = g_strdup_printf ("break");          break;
	case CMD_BUY:       buf = g_strdup_printf ("buy %s", str);    break;
	case CMD_DEPOSIT:   buf = g_strdup_printf ("dep %s", str);    break;
	case CMD_DROP:      buf = g_strdup_printf ("drop %s", str);   break;
	case CMD_EXIT:      buf = g_strdup_printf ("x");              break;
	case CMD_EXP:       buf = g_strdup_printf ("exp");            break;
	case CMD_GANGPATH:  buf = g_strdup_printf ("gb %s", str);     break;
	case CMD_HEALTH:    buf = g_strdup_printf ("health");         break;
	case CMD_HIDE:      buf = g_strdup_printf ("hide %s", str);	  break;
	case CMD_INVENTORY:	buf = g_strdup_printf ("i");              break;
	case CMD_INVITE:    buf = g_strdup_printf ("invite %s", str); break;
	case CMD_JOIN:      buf = g_strdup_printf ("join %s", str);   break;
	case CMD_MEDITATE:  buf = g_strdup_printf ("medi");           break;
	case CMD_OPEN:      buf = g_strdup_printf ("open %s", str);   break;
	case CMD_PARTY:     buf = g_strdup_printf ("par");            break;
	case CMD_PICKLOCK:  buf = g_strdup_printf ("pick %s", str);   break;
	case CMD_PICKUP:    buf = g_strdup_printf ("get %s", str);    break;
	case CMD_REMOVE:    buf = g_strdup_printf ("rem %s", str);    break;
	case CMD_SCAN:      buf = g_strdup_printf ("scan");           break;
	case CMD_SEARCH:    buf = g_strdup_printf ("sea %s", str);    break;
	case CMD_SELL:      buf = g_strdup_printf ("sell %s", str);   break;
	case CMD_SNEAK:		buf = g_strdup_printf ("sn");             break;
	case CMD_STATS:		buf = g_strdup_printf ("stat");           break;
	case CMD_REST:      buf = g_strdup_printf ("rest");           break;
	case CMD_TARGET:    buf = g_strdup ("");                      break;
	case CMD_TELEPATH:  buf = g_strdup_printf ("/%s", str);       break;
	case CMD_USE:       buf = g_strdup_printf ("use %s", str);    break;
	case CMD_WITHDRAW:  buf = g_strdup_printf ("with %s", str);   break;
	default:
		g_assert (str != NULL);
		buf = g_strdup (str);
	}

	if (type == CMD_CAST)
		timer_start (timers.castwait);

	if (type == CMD_MOVE)
	{
		if (character.flag.running)
			navigation.steps_ran++;

		automap_disable ();
		automap_parse_user_input (buf);
		item_visible_list_free ();

		/* assume false until confirmed */
		character.flag.sneaking = FALSE;
	}

	character.commands = g_slist_append (character.commands, command);
	send_line (buf);
	g_free (buf);
}


/* =========================================================================
 = COMMAND_SEND
 =
 = Basic wrapper around command_execute with only the type parameter
 ======================================================================== */

void command_send (gint type)
{
	command_execute (type, NULL);
}


/* =========================================================================
 = COMMAND_SEND_VA
 =
 = Wrapper around command_execute with variabable arguments
 ======================================================================== */

void command_send_va (gint type, gchar *fmt, ...)
{
	va_list argptr;
	gchar buf[STD_STRBUF];

	va_start (argptr, fmt);
	vsnprintf (buf, sizeof (buf), fmt, argptr);
	va_end (argptr);

	command_execute (type, buf);
}


/* =========================================================================
 = COMMAND_DEL
 =
 = Remove command from those pending
 ======================================================================== */

gboolean command_del (gint type)
{
	GSList *node;
	command_t *command;

	for (node = character.commands; node; node = node->next)
	{
		command = node->data;
		g_assert (command != NULL);

		if (command->type != type)
			continue;

		/* move command to history list */
		character.commands = g_slist_remove (character.commands, command);
		command_history = g_slist_prepend (command_history, command);

#ifdef DEBUG
		if (character.commands &&
			g_slist_length (character.commands) == 0)
			g_assert_not_reached ();
#endif

		if (g_slist_length (command_history) > COMMAND_HISTORY_MAX)
			command_history_remove_last ();

		return TRUE;
	}
	return FALSE;
}


/* =========================================================================
 = COMMAND_RECALL
 =
 = Command failed, remove it from the list
 ======================================================================== */

void command_recall (void)
{
	command_t *command;

	if (character.commands != NULL)
	{
		command = g_slist_nth_data (character.commands, 0);

		if (command->type == CMD_ATTACK)
			monster_target_del ();

		printt ("Command Recall: %s", command_types[command->type]);
        mudpro_audit_log_append ("Command recalled: %s",
            command_types[command->type]);

		command_del (command->type);
	}

	command_recalls_total++;

	if (character.status.confused)
		return; /* do not restrict recalls while confused */

	command_recalls++;

	if (command_recalls > COMMAND_RECALL_LIMIT)
	{
		/* too many recalls in allowed time */
		timer_stop (timers.recall);
		timer_reset (timers.recall);

		mudpro_reset_state (FALSE /* disconnected */);
		automap_reset      (TRUE /* full reset */);
		navigation_cleanup ();

		character.flag.health_read = FALSE;
		character.flag.exp_read    = FALSE;
		character.flag.stats_read  = FALSE;
		autoroam_opts.enabled      = FALSE;
		command_recalls = 0;

		printt ("Recall limit exceeded");
        mudpro_audit_log_append ("Recall limit exceeded - full reset");

		command_send (CMD_TARGET);
	}
	else
		timer_start (timers.recall);
}


/* =========================================================================
 = COMMAND_LIST_FREE
 =
 = Free list of allocated commands
 ======================================================================== */

void command_list_free (GSList *slist)
{
	GSList *node;
	command_t *command;

	for (node = slist; node; node = node->next)
	{
		command = node->data;
		g_free (command->str);
		g_free (command);
	}
	g_slist_free (slist);
}


/* =========================================================================
 = COMMAND_TIMERS_UPDATE
 =
 = Update command timers and remove those that have expired
 ======================================================================== */

void command_timers_update (void)
{
	GSList *node;
	command_t *command;
	GTimeVal now;

	g_get_current_time (&now);

	for (node = character.commands; node; node = node->next)
	{
		if ((command = node->data) == NULL)
			return;

		if (now.tv_sec < command->timeout.tv_sec
			|| (now.tv_sec == command->timeout.tv_sec
			&& now.tv_usec < command->timeout.tv_usec))
			continue;

		printt ("Command timed out: %s", command_types[command->type]);
        mudpro_audit_log_append ("Command timed out: %s", command_types[command->type]);

		if (!command_del (command->type))
			continue;

		/* reset state, just in case */
		combat_reset ();
		monster_target_list_free ();
		character.state = STATE_NONE;

		/* restart function with updated list */
		command_timers_update ();
		return;
	}
}


/* =========================================================================
 = COMMAND_PENDING
 =
 = Checks if the command is among those pending
 ======================================================================== */

gboolean command_pending (gint type)
{
	GSList *node;
	command_t *command;

	for (node = character.commands; node; node = node->next)
	{
		command = node->data;
		if (command->type == type)
			return TRUE;
	}
	return FALSE;
}


/* =========================================================================
 = COMMAND_HISTORY_REMOVE_LAST
 =
 = Remove last item from command history list
 ======================================================================== */

static void command_history_remove_last (void)
{
	GSList *node;
	command_t *command;

	if (!command_history)
		return; /* nothing to do */

	node = g_slist_last (command_history);
	command = node->data;
	command_history = g_slist_remove (command_history, command);
	g_free (command->str);
	g_free (command);
}
