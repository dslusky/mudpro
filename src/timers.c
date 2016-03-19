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
#include <sys/time.h>
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

    timers.dbupdate = timer_new ();
    timer_reset (timers.dbupdate);

    timers.castwait = timer_new ();
    timer_stop (timers.castwait);
    timer_reset (timers.castwait);

    timers.client_ai = timer_new ();
    timer_reset (timers.client_ai);

    timers.connect = timer_new ();
    timer_stop (timers.connect);
    timer_reset (timers.connect);

    timers.duration = timer_new ();
    timer_reset (timers.duration);

    timers.idle = timer_new ();
    timer_stop (timers.idle);
    timer_reset (timers.idle);

    timers.player_anim = timer_new ();
    timer_stop (timers.player_anim);
    timer_reset (timers.player_anim);

    timers.refresh = timer_new ();
    timer_reset (timers.refresh);

    timers.round = timer_new ();
    timer_reset (timers.round);

    timers.status = timer_new ();
    timer_reset (timers.status);

    timers.mudpro = timer_new ();
    timer_reset (timers.mudpro);

    timers.online = timer_new ();
    timer_stop (timers.online);
    timer_reset (timers.online);

    timers.osd_stats = timer_new ();
    timer_stop (timers.osd_stats);
    timer_reset (timers.osd_stats);

    timers.recall = timer_new ();
    timer_stop (timers.recall);
    timer_reset (timers.recall);

    timers.parcmd = timer_new ();
    timer_reset (timers.parcmd);
}


/* =========================================================================
 = TIMERS_CLEANUP
 =
 = Cleanup timers
 ======================================================================== */

void timers_cleanup (void)
{
    timer_destroy (timers.dbupdate);
    timer_destroy (timers.castwait);
    timer_destroy (timers.client_ai);
    timer_destroy (timers.connect);
    timer_destroy (timers.duration);
    timer_destroy (timers.idle);
    timer_destroy (timers.player_anim);
    timer_destroy (timers.refresh);
    timer_destroy (timers.round);
    timer_destroy (timers.status);
    timer_destroy (timers.mudpro);
    timer_destroy (timers.online);
    timer_destroy (timers.osd_stats);
    timer_destroy (timers.recall);
    timer_destroy (timers.parcmd);
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

    sec = (gulong) timer_elapsed (timers.castwait, &usec);
    fprintf (fp, "  Castwait .......... %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.client_ai, &usec);
    fprintf (fp, "  Client AI ......... %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.connect, &usec);
    fprintf (fp, "  Connect ........... %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.dbupdate, &usec);
    fprintf (fp, "  Database Update ... %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.duration, &usec);
    fprintf (fp, "  Duration .......... %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.idle, &usec);
    fprintf (fp, "  Idle .............. %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.mudpro, &usec);
    fprintf (fp, "  MudPRO ............ %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.online, &usec);
    fprintf (fp, "  Online ............ %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.osd_stats, &usec);
    fprintf (fp, "  OSD Stats ......... %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.player_anim, &usec);
    fprintf (fp, "  Player Animation .. %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.recall, &usec);
    fprintf (fp, "  Recall ............ %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.refresh, &usec);
    fprintf (fp, "  Refresh ........... %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.round, &usec);
    fprintf (fp, "  Round ............. %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.status, &usec);
    fprintf (fp, "  Status ............ %ld/%ld\n", sec, usec);

    sec = (gulong) timer_elapsed (timers.parcmd, &usec);
    fprintf (fp, "  Party Command ..... %ld/%ld\n\n", sec, usec);
}


/* =========================================================================
 = TIMERS_UPDATE
 =
 = Update timers
 ======================================================================== */

void timers_update (void)
{
    gulong sec, usec;

    /* update databases if needed */
    sec = (gulong) timer_elapsed (timers.dbupdate, &usec);

    if (sec >= TIMEOUT_SEC_DBUPDATE)
    {
        timers_db_update ();
        timer_reset (timers.dbupdate);
    }

    /* casting wait timer */
    sec = (gulong) timer_elapsed (timers.castwait, &usec);

    if (sec >= TIMEOUT_SEC_CASTWAIT)
    {
        timer_stop (timers.castwait);
        timer_reset (timers.castwait);
    }

    /* client AI */
    sec = (gulong) timer_elapsed (timers.client_ai, &usec);

    if (sec > 0 || usec >= TIMEOUT_USEC_CLIENT_AI)
    {
        client_ai ();
        timer_reset (timers.client_ai);
    }

    /* connection timer */
    sec = (gulong) timer_elapsed (timers.connect, &usec);

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
    sec = (gulong) timer_elapsed (timers.idle, &usec);

    if (sec >= TIMEOUT_SEC_IDLE)
    {
        if (character.flag.ready &&
            character.option.auto_all &&
            !USER_INPUT &&
            character.option.anti_idle)
        {
            mudpro_reset_state (FALSE /* disconnected */);
            send_line ("");
        }
        timer_reset (timers.idle);
    }

    /* player animation */
    sec = (gulong) timer_elapsed (timers.player_anim, &usec);

    if (sec > 0 || usec >= TIMEOUT_USEC_PLAYER_ANIM)
    {
        if (mapview_animate_player_frame ())
        {
            /* animation done, stop timer */
            timer_stop (timers.player_anim);
            timer_reset (timers.player_anim);
        }
    }

    /* screen refresh */
    sec = (gulong) timer_elapsed (timers.refresh, &usec);

    if (sec > 0 || usec >= TIMEOUT_USEC_REFRESH)
    {
        doupdate ();
        timer_reset (timers.refresh);
    }

    /* game rounds */
    sec = (gulong) timer_elapsed (timers.round, &usec);

    if (sec >= TIMEOUT_SEC_ROUND && usec > 500000)
    {
        timer_reset (timers.round);
    }

    /* misc status updates */
    sec = (gulong) timer_elapsed (timers.status, &usec);

    if (sec >= TIMEOUT_SEC_STATUS)
    {
        /* update pending party requests */
        party_request_update ();

        if (!sockIsAlive () && (connect_wait > 0))
        {
            gint y, x;
            gchar *buf;

            if (!timer_elapsed (timers.connect, NULL))
                timer_start (timers.connect);

            /* display countdown until we reconnect */
            getyx (terminal.w, y, x);
            wmove (terminal.w, y, 0);
            wclrtoeol (terminal.w);
            wattrset (terminal.w, ATTR_NOTICE | A_BOLD);

            buf = g_strdup_printf ("[Connecting in %d seconds]",
                connect_wait - (gint) timer_elapsed (timers.connect, NULL));

            waddstr (terminal.w, buf);
            wattrset (terminal.w, terminal.attr);
            g_free (buf);
        }

        if (osd_stats.visible)
            osd_stats_update ();

        timer_reset (timers.status);
    }

    sec = (gulong) timer_elapsed (timers.osd_stats, &usec);

    if (sec >= TIMEOUT_SEC_OSD_STATS)
    {
        timer_stop (timers.osd_stats);
        timer_reset (timers.osd_stats);
    }

    sec = (gulong) timer_elapsed (timers.recall, &usec);

    if (sec >= TIMEOUT_SEC_RECALL)
    {
        timer_stop (timers.recall);
        timer_reset (timers.recall);
        command_recalls = 0;
    }

    sec = (gulong) timer_elapsed (timers.parcmd, &usec);

    if (sec >= character.wait.parcmd)
    {
        timer_reset (timers.parcmd);
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

/* =========================================================================
 = TIMER_NEW
 =
 = Allocate a new timer object
 ======================================================================== */

_timer_t *timer_new(void)
{
    _timer_t *timer = g_malloc0(sizeof(_timer_t));
    timer_start(timer);
    return timer;
}


/* =========================================================================
 = TIMER_DESTROY
 =
 = Deallocate timer object
 ======================================================================== */

void timer_destroy(_timer_t *timer)
{
    g_assert (timer != NULL);
    g_free(timer);
}


/* =========================================================================
 = TIMER_START
 =
 = Start the timer
 ======================================================================== */

void timer_start(_timer_t *timer)
{
    g_assert (timer != NULL);
    timer->running = true;
    gettimeofday(&timer->start, NULL);
    memset(&timer->stop, 0, sizeof(struct timeval));
}


/* =========================================================================
 = TIMER_STOP
 =
 = Stop the timer
 ======================================================================== */

void timer_stop(_timer_t *timer)
{
    g_assert (timer != NULL);
    timer->running = false;
    gettimeofday(&timer->stop, NULL);
}


/* =========================================================================
 = TIMER_ELAPSED
 =
 = Return elapsed running time of the timer
 ======================================================================== */

gdouble timer_elapsed(_timer_t *timer, gulong *usec)
{
    gulong _sec, _usec;
    struct timeval now, *end;

    g_assert (timer != NULL);

    gettimeofday(&now, NULL);
    end = (timer->running) ? &now : &timer->stop;
    _sec = end->tv_sec - timer->start.tv_sec;

    if (_sec > 0)
        _usec = (1000000 - timer->start.tv_usec) + end->tv_usec;
    else
        _usec = end->tv_usec - timer->start.tv_usec;

    if (usec != NULL) *usec = _usec;

    return _sec + (_usec * 0.000001);
}

/* ========================================================================
 = TIMER_RESET
 =
 = Reset the timer
 ======================================================================= */

void timer_reset(_timer_t *timer)
{
    g_assert (timer != NULL);

    if (timer->running)
        gettimeofday(&timer->start, NULL);
    else
        memset(&timer->start, 0, sizeof(struct timeval));

    memset(&timer->stop, 0, sizeof(struct timeval));
}
