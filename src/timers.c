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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#ifdef __linux__
#include <unistd.h>
#endif

#include "automap.h"
#include "defs.h"
#include "character.h"
#include "client_ai.h"
#include "combat.h"
#include "command.h"
#include "guidebook.h"
#include "item.h"
#include "mapview.h"
#include "monster.h"
#include "mudpro.h"
#include "osd.h"
#include "parse.h"
#include "party.h"
#include "player.h"
#include "sock.h"
#include "spells.h"
#include "terminal.h"
#include "timers.h"
#include "utils.h"

gint connect_wait;
timers_t timers;

static void timers_db_update (void);


/* =========================================================================
 = TIMERS_INIT
 =
 = Initialize timers
 ======================================================================== */

void timers_init (void)
{
	memset (&timers, 0, sizeof (timers_t));
	connect_wait = 0;

	timers.dbupdate = g_timer_new ();

	timers.castwait = g_timer_new ();
	g_timer_stop (timers.castwait);

	timers.client_ai = g_timer_new ();

	timers.connect = g_timer_new ();
	g_timer_stop (timers.connect);

	timers.duration = g_timer_new ();

	timers.idle = g_timer_new ();
	g_timer_stop (timers.idle);

	timers.player_anim = g_timer_new ();
	g_timer_stop (timers.player_anim);

	timers.refresh = g_timer_new ();

	timers.round = g_timer_new ();

	timers.status = g_timer_new ();

	timers.mudpro = g_timer_new ();

	timers.online = g_timer_new ();
	g_timer_stop (timers.online);

	timers.osd_stats = g_timer_new ();
	g_timer_stop (timers.osd_stats);

	timers.recall = g_timer_new ();
	g_timer_stop (timers.recall);

	timers.parcmd = g_timer_new ();
}


/* =========================================================================
 = TIMERS_CLEANUP
 =
 = Cleanup timers
 ======================================================================== */

void timers_cleanup (void)
{
	g_timer_destroy (timers.dbupdate);
	g_timer_destroy (timers.castwait);
	g_timer_destroy (timers.client_ai);
	g_timer_destroy (timers.connect);
	g_timer_destroy (timers.duration);
	g_timer_destroy (timers.idle);
	g_timer_destroy (timers.player_anim);
	g_timer_destroy (timers.refresh);
	g_timer_destroy (timers.round);
	g_timer_destroy (timers.status);
	g_timer_destroy (timers.mudpro);
	g_timer_destroy (timers.online);
	g_timer_destroy (timers.osd_stats);
	g_timer_destroy (timers.recall);
	g_timer_destroy (timers.parcmd);
}


/* =========================================================================
 = TIMERS_REPORT
 =
 = Report current status of timers module to specified file
 ======================================================================== */

void timers_report (FILE *fp)
{
	gulong sec, usec;

	fprintf (fp, "\nTIMERS MODULE\n"
				 "=============\n\n");

	fprintf (fp, "  Timer               sec/usec\n"
				 "  ----------------------------\n");

	sec = (gulong) g_timer_elapsed (timers.castwait, &usec);
	fprintf (fp, "  Castwait .......... %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.client_ai, &usec);
	fprintf (fp, "  Client AI ......... %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.connect, &usec);
	fprintf (fp, "  Connect ........... %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.dbupdate, &usec);
	fprintf (fp, "  Database Update ... %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.duration, &usec);
	fprintf (fp, "  Duration .......... %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.idle, &usec);
	fprintf (fp, "  Idle .............. %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.mudpro, &usec);
	fprintf (fp, "  MudPRO ............ %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.online, &usec);
	fprintf (fp, "  Online ............ %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.osd_stats, &usec);
	fprintf (fp, "  OSD Stats ......... %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.player_anim, &usec);
	fprintf (fp, "  Player Animation .. %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.recall, &usec);
	fprintf (fp, "  Recall ............ %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.refresh, &usec);
	fprintf (fp, "  Refresh ........... %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.round, &usec);
	fprintf (fp, "  Round ............. %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.status, &usec);
	fprintf (fp, "  Status ............ %ld/%ld\n", sec, usec);

	sec = (gulong) g_timer_elapsed (timers.parcmd, &usec);
	fprintf (fp, "  Party Command ..... %ld/%ld\n\n", sec, usec);
}


/* =========================================================================
 = TIMERS_UPDATE
 =
 = Update timers
 ======================================================================== */

void timers_update (void)
{
	int sec = 0;
	gulong usec = 0;

	/* update databases if needed */
	sec = g_timer_elapsed (timers.dbupdate, &usec);

	if (sec >= TIMEOUT_SEC_DBUPDATE)
	{
		timers_db_update ();
		g_timer_start (timers.dbupdate);
	}

	/* casting wait timer */
	sec = g_timer_elapsed (timers.castwait, &usec);

	if (sec >= TIMEOUT_SEC_CASTWAIT)
	{
		g_timer_start (timers.castwait); //reset
		g_timer_stop (timers.castwait);
	}

	/* client AI */
	sec = g_timer_elapsed (timers.client_ai, &usec);

	if (sec > 0 || usec >= TIMEOUT_USEC_CLIENT_AI)
	{
		client_ai ();
		g_timer_start (timers.client_ai); //reset
	}

	/* connection timer */
	sec = g_timer_elapsed (timers.connect, &usec);

	if ((sec >= connect_wait) && !sockIsAlive () && !character.flag.disconnected)
	{
		if (connect_wait)
		{
			gint y, x;

			/* clear connection countdown */
			getyx (terminal.w, y, x);
			wmove (terminal.w, y, 0);
			wclrtoeol (terminal.w);
		}

		mudpro_connect ();
	}

	/* idle timer */
	sec = g_timer_elapsed (timers.idle, &usec);

	if (sec >= TIMEOUT_SEC_IDLE)
	{
		if (character.flag.ready &&
			character.option.auto_all &&
			!USER_INPUT &&
			((character.option.anti_idle && RESTING) || !character.option.anti_idle))
		{
			if (!character.option.anti_idle)
				mudpro_reset_state (FALSE /* disconnected */);

			send_line ("");
		}
		g_timer_start (timers.idle); //reset
	}

	/* player animation */
	sec = g_timer_elapsed (timers.player_anim, &usec);

	if (sec > 0 || usec >= TIMEOUT_USEC_PLAYER_ANIM)
	{
		if (mapview_animate_player_frame ())
		{
			/* animation done, stop timer */
			g_timer_start (timers.player_anim); //reset
			g_timer_stop (timers.player_anim);
		}
	}

	/* screen refresh */
	sec = g_timer_elapsed (timers.refresh, &usec);

	if (sec > 0 || usec >= TIMEOUT_USEC_REFRESH)
	{
		doupdate ();
		g_timer_start (timers.refresh); //reset
	}

	/* game rounds */
	sec = g_timer_elapsed (timers.round, &usec);

	if (sec >= TIMEOUT_SEC_ROUND && usec > 500000)
	{
		g_timer_start (timers.round); //reset
	}

	/* misc status updates */
	sec = g_timer_elapsed (timers.status, &usec);

	if (sec >= TIMEOUT_SEC_STATUS)
	{
		/* update pending party requests */
		party_request_update ();

		if (!sockIsAlive () && (connect_wait > 0))
		{
			gint y, x;
			gchar *buf;

			if (!(int)g_timer_elapsed (timers.connect, NULL))
				g_timer_start (timers.connect);

			/* display countdown until we reconnect */
			getyx (terminal.w, y, x);
			wmove (terminal.w, y, 0);
			wclrtoeol (terminal.w);
			wattrset (terminal.w, ATTR_NOTICE | A_BOLD);

			buf = g_strdup_printf ("[Connecting in %d seconds]",
				connect_wait - (gint) g_timer_elapsed (timers.connect, NULL));

			waddstr (terminal.w, buf);
			wattrset (terminal.w, terminal.attr);
			g_free (buf);
		}

		if (osd_stats.visible)
			osd_stats_update ();

		g_timer_start (timers.status); //reset
	}

	sec = g_timer_elapsed (timers.osd_stats, &usec);

	if (sec >= TIMEOUT_SEC_OSD_STATS)
	{
		g_timer_start (timers.osd_stats); //reset
		g_timer_stop (timers.osd_stats);
	}

	sec = g_timer_elapsed (timers.recall, &usec);

	if (sec >= TIMEOUT_SEC_RECALL)
	{
		g_timer_start (timers.recall); //reset
		g_timer_stop (timers.recall);
		command_recalls = 0;
	}

	sec = g_timer_elapsed (timers.parcmd, &usec);

	if (sec >= character.wait.parcmd)
	{
		g_timer_start (timers.parcmd); //reset
		if (WITHIN_PARTY)
			command_send (CMD_PARTY);
	}

	command_timers_update ();
}


/* =========================================================================
 = TIMERS_DB_UPDATE
 =
 = Update loaded databases when files on disk have changed
 ======================================================================== */

static void timers_db_update (void)
{
	struct stat st;

	if (args.no_poll || !character.option.conf_poll)
		return; /* do not poll config files */

	parse_db_update ();

	if (!stat (mudpro_db.automap.filename, &st)
		&& st.st_mtime > mudpro_db.automap.access.tv_sec)
	{
		printt ("Automap database updated");
		automap_reset (TRUE /* full reset */);
		automap_db_load ();
	}

	if (!stat (mudpro_db.guidebook.filename, &st)
		&& st.st_mtime > mudpro_db.guidebook.access.tv_sec)
	{
		printt ("GuideBook database updated");
		guidebook_db_load ();
	}

	if (!stat (mudpro_db.items.filename, &st)
		&& st.st_mtime > mudpro_db.items.access.tv_sec)
	{
		printt ("Item database updated");
		item_db_load ();
	}

	if (!stat (mudpro_db.monsters.filename, &st)
		&& st.st_mtime > mudpro_db.monsters.access.tv_sec)
	{
		printt ("Monster database updated");
		monster_db_load ();
	}

	if (!stat (mudpro_db.profile.filename, &st)
		&& st.st_mtime > mudpro_db.profile.access.tv_sec)
	{
		printt ("Character profile updated");
		character_options_load ();
	}

	if (!stat (mudpro_db.players.filename, &st)
		&& st.st_mtime > mudpro_db.players.access.tv_sec)
	{
		printt ("Player database updated");
		player_db_load ();
		character.flag.scan_read = FALSE;
	}

	if (!stat (mudpro_db.spells.filename, &st)
		&& st.st_mtime > mudpro_db.spells.access.tv_sec)
	{
		printt ("Spell database updated");
		spell_db_load ();
	}

	if (!stat (mudpro_db.strategy.filename, &st)
		&& st.st_mtime > mudpro_db.strategy.access.tv_sec)
	{
		printt ("Strategy database updated");
		combat_strategy_list_load ();
	}
}
