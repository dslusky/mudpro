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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __linux__
#include <unistd.h>
#endif

#include <glib.h>
#include <popt.h>

#include "about.h"
#include "automap.h"
#include "autoroam.h"
#include "combat.h"
#include "character.h"
#include "client_ai.h"
#include "command.h"
#include "defs.h"
#include "guidebook.h"
#include "item.h"
#include "keys.h"
#include "mapview.h"
#include "menubar.h"
#include "monster.h"
#include "mudpro.h"
#include "navigation.h"
#include "osd.h"
#include "parse.h"
#include "party.h"
#include "player.h"
#include "sock.h"
#include "sockbuf.h"
#include "spells.h"
#include "telopt.h"
#include "terminal.h"
#include "stats.h"
#include "timers.h"
#include "utils.h"

#define IO_POLL_RATE 20000

args_t args;
mudpro_db_t mudpro_db;

gboolean capture_active = FALSE;
gboolean notice_visible = FALSE;

static gboolean client_shutdown = FALSE;
static FILE *capture_log = NULL;

static gboolean mudpro_init (void);
static void mudpro_cleanup (void);
static void mudpro_process_args (poptContext ptc);
static void mudpro_startup_notice (void);
static gboolean temporary_key_handler (gint ch);
static gboolean mudpro_key_handler (gint ch);
static void mudpro_key_dispatch (gint ch);
static void mudpro_io_loop (void);


/* =========================================================================
 = MUDPRO_INIT
 =
 = Initialize the client
 ======================================================================== */

static gboolean mudpro_init (void)
{
	struct stat st;

#ifdef _WIN32
     WSADATA wsadata;

     /* request Winsock 1.1 */
     if (WSAStartup(0x0101, &wsadata) != ERROR_SUCCESS)
          return FALSE;
#endif

	memset (&mudpro_db, 0, sizeof (mudpro_db_t));
	memset (&autoroam_opts, 0, sizeof (autoroam_opts_t));
	mudpro_db.profile.filename = args.profile;

	/* need to initialize timers ASAP (for db_access) */
	timers_init ();

	/* initialize character before the others (for data_path) */
	character_init ();

	/* verify data path */

	if (!character.data_path)
		return FALSE;

	if (stat (character.data_path, &st))
	{
		fprintf (stderr, "Unable to stat data path %s: %s\n",
			character.data_path, strerror (errno));
		return FALSE;
	}

	if (!S_ISDIR (st.st_mode))
	{
		fprintf (stderr, "Data path %s is invalid or not accessable!\n",
			character.data_path);
		return FALSE;
	}

#ifdef __linux__
	/* request ibm extended character set (must come before initscr) */
	printf ("\033(U");
#endif

	/* initialize ncurses */
	initscr ();
	raw (); /* so we can catch CTRL-C, CTRL-Z, etc... */
#ifdef __linux__
	assume_default_colors (COLOR_WHITE, -1);
#endif
	start_color ();
	terminal_palette_init ();
	terminal_charset_init ();
	noecho ();
	nonl ();

	/* initialize client modules */
	automap_init ();
	combat_init ();
	command_init ();
	item_db_init ();
	monster_db_init ();
	navigation_init ();
	parse_init ();
	party_init ();
	player_db_init ();
	spell_db_init ();
	stats_init ();

	/* initialize windows */
	about_init ();
	autoroam_init ();
	guidebook_init ();
	terminal_init ();
	menubar_init ();

	memset (&mapview, 0, sizeof (cwin_t));
	memset (&osd_vitals, 0, sizeof (cwin_t));
	memset (&osd_stats, 0, sizeof (cwin_t));

	osd_dock_restack ();
	update_display ();

	return TRUE;
}


/* =========================================================================
 = MUDPRO_CLEANUP
 =
 = Prepare to close the application
 ======================================================================== */

static void mudpro_cleanup (void)
{
	/* cleanup socket stuff */
	sockShutdown ();
#ifdef _WIN32
	WSACleanup ();
#endif

	/* cleanup client modules */
	automap_cleanup ();
	combat_cleanup ();
	command_cleanup ();
	character_cleanup ();
	item_db_cleanup ();
	monster_db_cleanup ();
	navigation_cleanup ();
	parse_cleanup ();
	party_cleanup ();
	player_db_cleanup ();
	spell_db_cleanup ();
	stats_cleanup ();
	timers_cleanup ();

	/* cleanup windows */
	about_cleanup ();
	autoroam_cleanup ();
	guidebook_cleanup ();
	terminal_cleanup ();
	mapview_cleanup ();
	menubar_cleanup ();
	osd_vitals_cleanup ();
	osd_stats_cleanup ();

	if (capture_active && capture_log)
		mudpro_capture_log_toggle ();

	/* cleanup curses */
	endwin ();
}


/* =========================================================================
 = MUDPRO_CREATE_REPORT_LOG
 =
 = Create debug log with current status for each module
 ======================================================================== */

void mudpro_create_report_log (void)
{
	FILE *fp;
	gchar *buf;

	buf = g_strdup_printf ("%s.log",
		character.first_name ? character.first_name : "report");

	if ((fp = fopen (buf, "w")) == NULL)
	{
		printt ("Failed to create %s!", buf);
		g_free (buf);
		return;
	}

	automap_report (fp);
	autoroam_report (fp);
	combat_report (fp);
	command_report (fp);
	navigation_report (fp);
	timers_report (fp);

	fclose (fp);
	printt ("Wrote %s", buf);
	g_free (buf);
}


/* =========================================================================
 = MUDPRO_CAPTURE_LOG_TOGGLE
 =
 = Toggle capture log on/off
 ======================================================================== */

void mudpro_capture_log_toggle (void)
{
	if (capture_active)
	{
		printt ("Capture log closed");

		if (capture_log)
			fclose (capture_log);

		capture_active = FALSE;
		capture_log    = NULL;
	}
	else
	{
		gchar *path = g_strdup_printf ("%s%ccapture.log",
			character.data_path, G_DIR_SEPARATOR);

		capture_log = fopen (path, "a");

		if (capture_log)
		{
			capture_active = TRUE;
			printt ("Capture log activated");
		}
		else
			printt ("Failed to open capture log!");
	}
}


/* =========================================================================
 = MUDPRO_CAPTURE_LOG_APPEND
 =
 = Appends line to capture log if active
 ======================================================================== */

void mudpro_capture_log_append (const gchar *str)
{
	if (capture_active && capture_log)
	{
		GString *now = get_time_as_g_string (NULL);
		fprintf (capture_log, "[%s] %s\n", now->str, str);
		g_string_free (now, TRUE);
	}
}


/* =========================================================================
 = MUDPRO_AUDIT_LOG_APPEND
 =
 = Append message to audit log
 ======================================================================== */

void mudpro_audit_log_append (gchar *fmt, ...)
{
	FILE *fp;
	gchar *path;
    va_list argptr;
    gchar buf[STD_STRBUF];

    va_start (argptr, fmt);
    vsnprintf (buf, sizeof (buf), fmt, argptr);
    va_end (argptr);

	path = g_strdup_printf ("%s%caudit.log",
		character.data_path, G_DIR_SEPARATOR);

	if ((fp = fopen (path, "a")) != NULL)
	{
        struct tm *now;
		time_t gmtime;
		gchar str[STD_STRBUF];

		time (&gmtime);
		if ((now = localtime(&gmtime)) != NULL)
		{
            strftime(str, sizeof(str), "%F %T", now);
            fprintf (fp, "%s - %s\n", str, buf);
        }
		fclose (fp);
	}

	g_free (path);
}


/* =========================================================================
 = MUDPRO_RESET_STATE
 =
 = Reset the client state
 ======================================================================== */

void mudpro_reset_state (gboolean disconnected)
{
	combat_reset ();
	command_list_free (character.commands);
	character.commands = NULL;

	monster_target_list_free ();

	character.state = STATE_NONE;

	if (disconnected)
	{
		GString *now = get_time_as_g_string (NULL);

		terminal_set_title (TERMINAL_TITLE_DEFAULT);

        mudpro_audit_log_append("Disconnected (connection closed)");
		printt ("Disconnected -- %s", now->str);
		g_string_free (now, TRUE);

		/* perserve automap data on cleanup event, reset otherwise */

		if (character.flag.cleanup)
		{
			/* automap.lost = 1; */
			automap_disable ();
			/* navigation_route_free (); */
		}
		else
			automap_reset (TRUE /* full reset */);

		if (character.flag.disconnected)
		{
			navigation_cleanup ();
			autoroam_opts.enabled = FALSE;
		}
		else if (/* !character.flag.disconnected && */ !character.flag.cleanup)
			connect_wait = character.wait.connect;

		character.flag.ready       = FALSE;
		character.flag.health_read = FALSE;
		character.flag.exp_read    = FALSE;
		character.flag.stats_read  = FALSE;
		character.flag.scan_read   = FALSE;
		osd_dock_restack ();
	}
}


/* =========================================================================
 = MUDPRO_LOAD_DATA
 =
 = (Re)loads all client data
 ======================================================================== */

void mudpro_load_data (void)
{
	mudpro_reset_state (FALSE /* disconnected */);
	automap_reset (TRUE /* full reset */);

	automap_db_load ();
	guidebook_db_load ();
	item_db_load ();
	monster_db_load ();
	character_options_load ();
	spell_db_load ();
	combat_strategy_list_load ();
	parse_list_compile ();

	printt ("Client data loaded");
}


/* =========================================================================
 = MUDPRO_SAVE_DATA
 =
 = Writes all client data out to disk
 ======================================================================== */

void mudpro_save_data (void)
{
	automap_db_save ();
	printt ("Client data saved");
}


/* =========================================================================
 = MUDPRO_SHUTDOWN
 =
 = Initiate client shutdown
 ======================================================================== */

void mudpro_shutdown (void)
{
	client_shutdown = TRUE;
}


/* =========================================================================
 = MUDPRO_PROCESS_ARGS
 =
 = Process client command-line arguments
 ======================================================================== */

static void mudpro_process_args (poptContext ptc)
{
	int ch;

	poptResetContext (ptc);

	while ((ch = poptGetNextOpt (ptc)) >= 0)
	{
		switch (ch)
		{
		case 'c': args.connect = TRUE; break;
		case 'n': args.no_poll = TRUE; break;
		case 'r': args.capture = TRUE; break;
		}
	}
}


/* =========================================================================
 = MUDPRO_CONNECT
 =
 = Connects client with remove host
 ======================================================================== */

void mudpro_connect (void)
{
	gchar *port;

	if (sockIsAlive ())
		return; /* already connected */

	port = g_strdup_printf ("%d", character.port);

	/* attempt to establish connection */

	terminal_clear ();
	printt ("Connecting to host: %s:%s", character.hostname, port);

	character.flag.disconnected = FALSE;
	stats.connects++;

	if (!sockOpen (character.hostname, port))
	{
		timer_stop (timers.connect);
		timer_reset (timers.connect);
		timer_start (timers.idle);
		timer_start (timers.online);
		connect_wait = 0;
		character.flag.cleanup = FALSE;

        mudpro_audit_log_append("Connected to %s", character.hostname);
		terminal_set_title ("MudPRO [%s]", character.hostname);

		/* initialize connection */
		telOptInit ();
		sockBufRReset ();
		sockBufWReset ();
	}
	else /* connection failed, activate timer */
	{
		timer_reset (timers.connect);
		connect_wait = character.wait.connect;
	}

	g_free (port);
}


/* =========================================================================
 = MUDPRO_DISCONNECT
 =
 = Disconnect from remote host
 ======================================================================== */

void mudpro_disconnect (void)
{
	if (!sockIsAlive ())
		return; /* already disconnected */

	character.flag.disconnected = TRUE;
	sockClose ();
}


/* =========================================================================
 = MUDPRO_STARTUP_NOTICE
 =
 = Display notice upon startup
 ======================================================================== */

static void mudpro_startup_notice (void)
{
	notice_visible = TRUE;

	wmove (terminal.w, MAX (0, LINES - 7), 0);

	wattrset (terminal.w, ATTR_RED);
	wprintw (terminal.w, "\n  Mud");
	wattrset (terminal.w, ATTR_RED | A_BOLD);
	wprintw (terminal.w, "PRO");

	wattrset (terminal.w, ATTR_BLACK | A_BOLD);
	wprintw (terminal.w, " /// ");

	wattrset (terminal.w, ATTR_WHITE | A_BOLD);
	wprintw (terminal.w, "Release: %s\n", RELEASE);

	wattrset (terminal.w, ATTR_WHITE);
	wprintw (terminal.w, "  Copyright (c) 2002-2016 David Slusky\n\n");

	wattrset (terminal.w, ATTR_YELLOW | A_BOLD);
	wprintw (terminal.w, "  Press CTRL-C to connect ");

	update_display ();
}


/* =========================================================================
 = MUDPRO_REDRAW_DISPLAY
 =
 = Redraws the entire display
 ======================================================================== */

void mudpro_redraw_display (void)
{
	clear ();
	update_display ();
}


/* =========================================================================
 = TEMPORARY_CHATLOG_APPEND
 =
 = Temporary chatlog handler
 ======================================================================== */

void temporary_chatlog_append (gchar *str)
{
	FILE *fp;
	gchar *path;

	path = g_strdup_printf ("%s%cchatlog.log",
		character.data_path, G_DIR_SEPARATOR);

	if ((fp = fopen (path, "a")) != NULL)
	{
		time_t gmtime;
		gchar *buf, *pos;

		/* get date as a string we can modify */
		time (&gmtime);
		buf = g_strdup_printf ("%s", ctime (&gmtime)+4);

		/* remove year */
		if ((pos = rindex (buf, ' ')) != NULL) *pos = '\0';

		fprintf (fp, "[%s] %s\n", buf, str);

		g_free (buf);
		fclose (fp);
	}

	g_free (path);
}


/* =========================================================================
 = TEMPORARY_KEY_HANDLER
 =
 = Temporary key handler
 ======================================================================== */

static gboolean temporary_key_handler (gint ch)
{
	switch (ch)
	{
	case KEY_KP0:
		character.input = g_string_append (character.input, "u");
		send_line ("u");
		break;

	case KEY_KP1:
		character.input = g_string_append (character.input, "sw");
		send_line ("sw");
		break;

	case KEY_KP2:
		character.input = g_string_append (character.input, "s");
		send_line ("s");
		break;

	case KEY_KP3:
		character.input = g_string_append (character.input, "se");
		send_line ("se");
		break;

	case KEY_KP4:
		character.input = g_string_append (character.input, "w");
		send_line ("w");
		break;

	case KEY_KP5:
		character.input = g_string_append (character.input, "sea");
		send_line ("sea");
		break;

	case KEY_KP6:
		character.input = g_string_append (character.input, "e");
		send_line ("e");
		break;

	case KEY_KP7:
		character.input = g_string_append (character.input, "nw");
		send_line ("nw");
		break;

	case KEY_KP8:
		character.input = g_string_append (character.input, "n");
		send_line ("n");
		break;

	case KEY_KP9:
		character.input = g_string_append (character.input, "ne");
		send_line ("ne");
		break;

	case KEY_KP_DEL:
		character.input = g_string_append (character.input, "d");
		send_line ("d");
		break;

	default:
		return FALSE;
	}

	automap_parse_user_input (character.input->str);
	character.input = g_string_assign (character.input, "");
	return TRUE;
}


/* =========================================================================
 = MUDPRO_KEY_HANDLER
 =
 = Handle keys of interest to the client
 ======================================================================== */

static gboolean mudpro_key_handler (gint ch)
{
	switch (ch)
	{
	case KEY_DOWN:
		send_raw ("[B");
		break;

	case KEY_LEFT:
		send_raw ("[D");
		break;

	case KEY_RIGHT:
		send_raw ("[C");
		break;

	case KEY_UP:
		send_raw ("[A");
		break;

	case KEY_CTRL_C: /* connect to remote host */
		if (!sockIsAlive ())
			mudpro_connect ();
		break;

	case KEY_CTRL_D: /* disconnect from remote host */
		if (sockIsAlive ())
		{
			character.flag.disconnected = TRUE;
			sockClose ();
		}
		break;

	case KEY_CTRL_L: /* redraw screen */
		mudpro_redraw_display ();
		break;

	case KEY_CTRL_Q: /* initiate client shutdown */
		mudpro_shutdown ();
		break;

	default:
		return FALSE;
	}
	return TRUE;
}


/* =========================================================================
 = MUDPRO_KEY_DISPATCH
 =
 = Dispatch incoming keystrokes
 ======================================================================== */

static void mudpro_key_dispatch (gint ch)
{
	if (ch == KEY_RESIZE)
	{
		terminal_resize ();
		return;
	}

	if (about.visible &&
		about_key_handler (ch))
		return; /* keep this first */

	if (menubar.visible &&
		menubar_key_handler (ch))
		return; /* keep this second */

	if (autoroam.visible &&
		autoroam_key_handler (ch))
		return;

	if (guidebook.visible &&
		guidebook_key_handler (ch))
		return;

	if (mapview.visible &&
		mapview_key_handler (ch))
		return;

	/* FIXME: replace with user-defined key handler */
	if (temporary_key_handler (ch))
		return;

	if (mudpro_key_handler (ch))
		return;

#if 0
    // display key codes
    printt("Keycode: %02X", ch);
#endif

	switch (ch) /* default key handler */
	{
	case KEY_BKSP: /* backspace */
	case KEY_CTRL_H:
		character.input = g_string_truncate (character.input,
			MAX (0, character.input->len - 1));
		break;

	case KEY_CR: /* carrage-return */
	case KEY_LF: /* line-feed */
		automap_parse_user_input (character.input->str);
		if (!strcmp (character.input->str, "x"))
			character.flag.disconnected = TRUE;
		character.input = g_string_assign (character.input, "");
		break;

	default:
		if (isprint (ch))
			character.input = g_string_append_c (character.input, ch);
	}

	putSock1 (ch);
}


/* =========================================================================
 = MUDPRO_IO_LOOP
 =
 = Poll for socket/keyboard I/O
 ======================================================================== */

static void mudpro_io_loop (void)
{
	GTimeVal tv;
	fd_set rfds, wfds;
	gint ch, key;

	ch = key = 0;

	do {
		/* update timers */

		timers_update ();

		/* handle keyboard I/O */

		ch = wgetch (terminal.w);

		if (ch != ERR)
		{
			key += ch;
			if (ch == KEY_ALT)
				continue;
			else if (ch == 0x4F && key == (KEY_ALT + 0x4F))
				continue;

			mudpro_key_dispatch (key);
			key = 0;
			continue;
		}

		/* handle socket I/O */

		FD_ZERO (&rfds);
		FD_ZERO (&wfds);
		FD_SET (sock.fd, &rfds);
		if (sockBufWHasData ()) FD_SET (sock.fd, &wfds);

		tv.tv_sec = 0;
		tv.tv_usec = IO_POLL_RATE;

		if (select (sock.fd+1, &rfds, &wfds, NULL, (void *) &tv) < 0)
			continue;

		if (!sockIsAlive ())
			continue; /* nothing to do, just polling w/select */

		if (FD_ISSET (sock.fd, &rfds))
		{
			sockBufRead ();
			sockReadLoop ();
			update_display ();
            timer_reset (timers.idle);
		}

		if (FD_ISSET (sock.fd, &wfds))
		{
			sockBufWrite ();
		}
	} while (!client_shutdown);
}


/* =========================================================================
 = MAIN
 =
 = You have entered a maze of twisty passages, all alike.
 ======================================================================== */

gint main (gint argc, gchar *argv[])
{
	poptContext ptc;
	struct stat st;
	struct poptOption option_table[]=
	{
#ifdef __linux__
		POPT_AUTOHELP
#endif

		{ "connect",    'c', POPT_ARG_NONE,
			0, 'c', "Connect to remote host on startup" },

		{ "hostname",   'h', POPT_ARG_STRING,
			&args.hostname,   0, "Remote host address" },

		{ "port",       'p', POPT_ARG_INT,
			&args.port, 0, "Remote port number" },

		{ "line-style", 'l', POPT_ARG_INT,
			&args.line_style, 0, "Line style to use (1-5)" },

		{ "no-polling", 'n', POPT_ARG_NONE,
			0, 'n', "Do not poll config files" },

		{ "profile",    'f', POPT_ARG_STRING,
			&args.profile,    0, "Character profile to load" },

		{ "record",     'r', POPT_ARG_NONE,
		    &args.capture,    0, "Begin recording session at startup" },

		{ NULL, 0, 0, NULL, 0 }
	};

	ptc = poptGetContext (NULL, argc, (const char **) argv,
		option_table, 0);
	mudpro_process_args (ptc);

	if (args.profile && args.profile[0] != '\0')
	{
		if (stat (args.profile, &st))
		{
			fprintf (stderr, "Cannot load %s: %s!\n",
				args.profile, strerror (errno));
			exit (1);
		}

		if (!S_ISREG (st.st_mode))
		{
			fprintf (stderr, "'%s' is not a regular file.\n",
				args.profile);
			exit (1);
		}
	}
	else
	{
		fprintf (stderr, "\nYou must specify a character profile to load!\n\n");
#ifdef __linux__
		poptPrintHelp (ptc, stderr, 0);
#endif
		exit (1);
	}

	if (!mudpro_init ())
	{
		endwin ();
		exit (1);
	}

	if (!character.hostname
		|| character.hostname[0] == '\0'
		|| index (character.hostname, ' '))
	{
		mudpro_cleanup ();
		fprintf (stderr, "Hostname invalid or not defined!\n");
		exit (1);
	}

	if (args.connect)
		character.flag.disconnected = FALSE;
	else
		mudpro_startup_notice ();

	if (args.capture && !capture_active)
		mudpro_capture_log_toggle ();

	terminal_set_title (TERMINAL_TITLE_DEFAULT);

	/* enter I/O loop */
	if (!client_shutdown)
		mudpro_io_loop ();

	/* shut the client down */
	mudpro_cleanup ();
	poptFreeContext (ptc);

	return 0;
}
