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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "automap.h"
#include "character.h"
#include "combat.h"
#include "command.h"
#include "defs.h"
#include "dispatch.h"
#include "item.h"
#include "mapview.h"
#include "monster.h"
#include "mudpro.h"
#include "navigation.h"
#include "parse.h"
#include "osd.h"
#include "spells.h"
#include "stats.h"
#include "terminal.h"
#include "timers.h"
#include "utils.h"

#define STR_RESTING         " (Resting) "
#define STR_MEDITATING      " (Meditating) "

parse_t parse;

static GSList *regexp_list;

/* user parse tag / substitution regexp */

static parse_tag_t tag_list[] = {

	{ "{player}",	"(\\w+)" },
	{ "{monster}",	"(.+)" },
	{ "{their}",	"(his|her|its)" },
	{ "{source}",	"(.+)" },
	{ "{target}",	"(.+)" },
	{ "{cash}",     "(\\d+ (copper|silver|gold|platinum|runic))" },
	{ "{#}",		"(\\d+)" },
	{ "{*}",		"(.*)" },
	{ "{1}",		"(\\S+)" },
	{ "{2}",		"(\\S+)" },
	{ "{3}",		"(\\S+)" },
	{ "{4}",		"(\\S+)" },
	{ "{5}",		"(\\S+)" },
	{ "{6}",		"(\\S+)" },
	{ "{7}",		"(\\S+)" },
	{ "{8}",		"(\\S+)" },
	{ "{9}",		"(\\S+)" },

	{ "{hit}",
		"(attack\\w*|beat\\w*|bludgeon\\w*|chop\\w*|cleave\\w*|clobber\\w*|"
		"crush\\w*|cut\\w*|hack\\w*|impale\\w*|jab\\w*|jumpkick\\w*|kick\\w*|"
		"pierce\\w*|pound\\w*|punch\\w*|rip\\w*|shoot\\w*|skewer\\w*|slam\\w*|"
		"slash\\w*|slice\\w*|smack\\w*|smash\\w*|stab\\w*|sting\\w*|strike\\w*|"
		"throw\\w*|whap\\w*|whip\\w*|rip\\w*|bite\\w*|lash\\w*|chomp\\w*|"
		"headbutt\\w*|maul\\w*|pinch\\w*|claw\\w*|darts.+bites|chill\\w*)" },

	{ "{hits}",
		"(attack\\w*|beat\\w*|bludgeon\\w*|chop\\w*|cleave\\w*|clobber\\w*|"
		"crush\\w*|cut\\w*|hack\\w*|impale\\w*|jab\\w*|jumpkick\\w*|kick\\w*|"
		"pierce\\w*|pound\\w*|punch\\w*|rip\\w*|shoot\\w*|skewer\\w*|slam\\w*|"
		"slash\\w*|slice\\w*|smack\\w*|smash\\w*|stab\\w*|sting\\w*|strike\\w*|"
		"throw\\w*|whap\\w*|whip\\w*|rip\\w*|bite\\w*|lash\\w*|chomp\\w*|"
		"headbutt\\w*|maul\\w*|pinch\\w*|claw\\w*|darts.+bites|chill\\w*)" },

	{ "{miss}",
		"(swing.+at|miss\\w*|lunge.+at|punch\\w*|kick\\w*|jumpkick\\w*|"
		"thrust.+at|shoot.+at|swipe.+at|swipe\\w*|attack\\w*|hurl.+at|"
		"throw.+at|wipe.+at|snap.+at|flail.+at|lash.+at|moves towards|"
		"reaches.+for|claw.+at)" },

	{ "{misses}",
		"(swing.+at|miss\\w*|lunge.+at|punch\\w*|kick\\w*|jumpkick\\w*|"
		"thrust.+at|shoot.+at|swipe.+at|swipe\\w*|attack\\w*|hurl.+at|"
		"throw.+at|wipe.+at|snap.+at|flail.+at|lash.+at|moves towards|"
		"reaches.+for|claw.+at)" },

	{ "{enters}",
		"(drops|slithers|stomps|appears|burrows|enters|walks|scurries|moves|"
		"thunders|lopes|scuttles|strides|flaps|oozes|creeps|sneaks|shambles|"
		"swims|crawls)" },

	{ "{direction}",
		"(n|north|s|south|e|east|w|west|"
		"ne|northeast|nw|northwest|se|southeast|sw|southwest|"
		"u|up|d|down|above|below|upwards|downwards)" },

	{ NULL, NULL }
};

static GString *regexp_tag_substitution (GString *pattern);
static gint parse_action_get_value (parse_regexp_t *regexp, gchar **offset);
static void parse_list_add_action (gpointer data, gpointer user_data);
static void parse_list_move_regexp (gpointer data, gpointer user_data);
static void parse_list_free_actions (parse_regexp_t *regexp);
static void parse_db_list_build (void);
static void parse_db_list_free (void);
static gboolean parse_db_append (db_t *parse_db);
static gboolean parse_regexp (parse_regexp_t *parse_regexp, gchar *subject);
static void parse_regexp_list (gchar *subject);


/* =========================================================================
 = PARSE_INIT
 =
 = Initialize the parse module
 ======================================================================== */

void parse_init (void)
{
	memset (&parse, 0, sizeof (parse_t));

	parse.line_buf  = g_string_new ("");
	parse.wrap_buf  = g_string_new ("");
	parse.room_name = g_string_new ("");

	parse_db_list_build ();
	parse_list_compile ();
}


/* =========================================================================
 = PARSE_CLEANUP
 =
 = Cleanup after parse module
 ======================================================================== */

void parse_cleanup (void)
{
	parse_db_list_free ();
	parse_list_free ();

	g_string_free (parse.line_buf, TRUE);
	g_string_free (parse.wrap_buf, TRUE);
	g_string_free (parse.room_name, TRUE);
}


/* ==========================================================================
 = REGEXP_TAG_SUBSTITUTION
 =
 = Substitute user tags with regexps
 ========================================================================= */

static GString *regexp_tag_substitution (GString *pattern)
{
	parse_tag_t *pt;
	gchar *tmp;
	gint pos = 0;

	for (pt = tag_list; pt->tag; pt++)
	{
		while ((tmp = strstr (pattern->str, pt->tag)) != NULL)
		{
			pos = CLAMP ((tmp - pattern->str), 0, (pattern->len - 1));
			pattern = g_string_erase (pattern, pos, strlen (pt->tag));
			pattern = g_string_insert (pattern, pos, pt->regexp);
		}
	}

	if (pattern->str[0] != '^')
		pattern = g_string_insert (pattern, 0, "^");
	if (pattern->str[pattern->len-1] != '$')
		pattern = g_string_append (pattern, "$");

	return pattern;
}


/* ==========================================================================
 = PARSE_ACTION_GET_VALUE
 =
 = Returns parse action value
 ========================================================================= */

static gint parse_action_get_value (parse_regexp_t *regexp, gchar **offset)
{
	gint value = 0;
	gchar *str, *pos;

	g_assert (regexp != NULL);
	g_assert (offset != NULL);

	if ((str = get_token_as_str (&*offset)) == NULL)
	{
		return 0; /* nothing to do */
	}

	/* handle special case values */

	if (!strcasecmp (str, "TRUE"))
	{
		g_free (str);
		return TRUE;
	}

	if (!strcasecmp (str, "FALSE") ||
		!strcasecmp (str, "NONE") ||
		!strcasecmp (str, "NULL"))
	{
		g_free (str);
		return FALSE;
	}

	if ((value = automap_get_exit_as_int (str)) > EXIT_NONE)
	{
		g_free (str);
		return value + PARSE_VALUE_DIRECTION;
	}

	/* FIXME: this could use improvement (more validation) */

	for (pos = regexp->pattern; *pos; pos++)
	{
		if (*pos == '(') /* ignore inline regexp substrings */
			value++;

		else if (*pos == '{') /* find tag position */
		{
			value++;

			if (!strncmp (pos, str, strlen (str)))
			{
				g_free (str);
				return value;
			}
		}
	}

	/* assume value is int */
	value = atoi (str);
	g_free (str);

	return value;
}


/* ==========================================================================
 = PARSE_LIST_ADD_ACTION
 =
 = Add action to regexp action list
 ========================================================================= */

static void parse_list_add_action (gpointer data, gpointer user_data)
{
	parse_regexp_t *regexp = data;
	parse_action_t *action;
	gchar *str = user_data, *offset;

	g_assert (regexp != NULL);
	g_assert (str != NULL);

	action = g_malloc0 (sizeof (parse_action_t));

	offset = str;
	action->type  = get_token_as_str (&offset);
	action->arg   = get_token_as_str (&offset);
	action->value = parse_action_get_value (regexp, &offset);

	regexp->actions = g_slist_append (regexp->actions, action);
}


/* ==========================================================================
 = PARSE_LIST_MOVE_ACTION
 =
 = Move regexp data to master list
 ========================================================================= */

static void parse_list_move_regexp (gpointer data, gpointer user_data)
{
	parse_regexp_t *regexp = data;

	g_assert (regexp != NULL);

	parse.regexp_list = g_slist_prepend (parse.regexp_list, regexp);
}


/* ==========================================================================
 = PARSE_LIST_FREE_ACTIONS
 =
 = Free allocated parse actions
 ========================================================================= */

static void parse_list_free_actions (parse_regexp_t *regexp)
{
	parse_action_t *action;
	GSList *node;

	g_assert (regexp != NULL);

	for (node = regexp->actions; node; node = node->next)
	{
		action = node->data;
		g_free (action->type);
		g_free (action->arg);
		g_free (action);
	}
	g_slist_free (regexp->actions);
	regexp->actions = NULL;
}


/* ==========================================================================
 = PARSE_LIST_COMPILE
 =
 = Build parse list and compile regexps into native PCRE format
 ========================================================================= */

void parse_list_compile (void)
{
	GSList *node;

	if (parse.regexp_list)
		parse_list_free ();

	for (node = parse.db_list; node; node = node->next)
	{
		if (parse_db_append (node->data))
			continue; /* added successfully */

		/* list changed, flush out missing/invalid database files */
		parse_list_compile ();
		return;
	}

    if (parse.regexp_list)
		parse.regexp_list = g_slist_reverse (parse.regexp_list);
}


/* =========================================================================
 = PARSE_LIST_FREE
 =
 = Free memory allocated to parse list
 ======================================================================== */

void parse_list_free (void)
{
	GSList *node;
	parse_regexp_t *regexp;

	for (node = parse.regexp_list; node; node = node->next)
	{
		regexp = node->data;
		g_free (regexp->pattern);
		g_free (regexp->compiled);
		g_free (regexp->studied);
		parse_list_free_actions (regexp);
		g_free (regexp);
	}
	g_slist_free (parse.regexp_list);
	parse.regexp_list = NULL;
}


/* =========================================================================
 = PARSE_DB_LIST_BUILD
 =
 = Build list of parse database files
 ======================================================================== */

static void parse_db_list_build (void)
{
	struct stat st;
	const gchar *entry;
	db_t *parse_db;
	GDir *dir;
	gchar *path;

	if (parse.db_list)
		parse_db_list_free ();

	path = g_strdup_printf ("%s%cparse",
		character.data_path, G_DIR_SEPARATOR);

	if (stat (path, &st))
	{
		fprintf (stderr, "Unable to stat %s, bailing out!\n", path);
		mudpro_shutdown ();
		g_free (path);
		return;
	}

	if (!S_ISDIR (st.st_mode))
	{
		fprintf (stderr, "%s is not a valid directory, bailing out!\n", path);
		mudpro_shutdown ();
		g_free (path);
		return;
	}

	dir = g_dir_open (path, 0, NULL);

	while ((entry = g_dir_read_name (dir)) != NULL)
	{
		if (!strstr (entry, ".db"))
			continue;

		parse_db = g_malloc0 (sizeof (db_t));
		parse_db->filename = g_strdup_printf ("%s/%s", path, entry);
		parse.db_list = g_slist_append (parse.db_list, parse_db);
	}

	g_dir_close (dir);
	g_free (path);
}


/* =========================================================================
 = PARSE_DB_LIST_FREE
 =
 = Free list of parse database files
 ======================================================================== */

static void parse_db_list_free (void)
{
	g_slist_foreach (parse.db_list, db_deallocate, NULL);
	g_slist_free (parse.db_list);
	parse.db_list = NULL;
}


/* =========================================================================
 = PARSE_DB_APPEND
 =
 = Append to parse list from a database file
 ======================================================================== */

static gboolean parse_db_append (db_t *parse_db)
{
	parse_regexp_t *regexp = NULL;
	GString *pattern;
	pcre *compiled;
	pcre_extra *studied;
	const gchar *error;
	FILE *fp;
	gchar buf[STD_STRBUF];
	gchar *pos, *token;
	gint offset;

	g_assert (parse_db != NULL);
	g_assert (parse_db->filename != NULL);

	if ((fp = fopen (parse_db->filename, "r")) == NULL)
	{
		printt ("Unable to open %s!", parse_db->filename);
		parse.db_list = g_slist_remove (parse.db_list, parse_db);
		db_deallocate (parse_db, NULL);
		return FALSE;
	}

	g_get_current_time (&parse_db->access);

	while (fgets (buf, sizeof (buf), fp) != NULL)
	{
		strchomp (buf);

		if (buf[0] == '#' || buf[0] == '\0')
		{
			/* move queue'd regexps to master list */
			g_slist_foreach (regexp_list, parse_list_move_regexp, NULL);
			g_slist_free (regexp_list);
			regexp_list = NULL;
			continue;
		}

		if (isspace (buf[0]))
		{
			/* assign action to queue'd regexps */
			g_slist_foreach (regexp_list, parse_list_add_action, buf);
			continue;
		}

		pos = buf;
		token = get_token_as_str (&pos);

		pattern = g_string_new (token);
		pattern = regexp_tag_substitution (pattern);

		if ((compiled = pcre_compile (pattern->str, 0,
			&error, &offset, NULL)) == NULL)
		{
			printt ("Error compiling regexp '%s': %s (at offset %d)",
				pattern->str, error, offset);
			continue;
		}
		g_string_free (pattern, TRUE);

		studied = pcre_study (compiled, 0, &error);

		regexp = g_malloc0 (sizeof (parse_regexp_t));
		regexp->pattern  = token;
		regexp->compiled = compiled;
		regexp->studied  = studied;

		/* add regexp to queue until we read the actions */
		regexp_list = g_slist_prepend (regexp_list, regexp);
	}

	/* take care of any leftovers */
	if (regexp_list != NULL)
	{
		/* move queue'd regexps to master list */
		g_slist_foreach (regexp_list, parse_list_move_regexp, NULL);
		g_slist_free (regexp_list);
		regexp_list = NULL;
	}

	fclose (fp);
	return TRUE;
}


/* =========================================================================
 = PARSE_DB_UPDATE
 =
 = Check if parse db files have been modified and update regexp list
 ======================================================================== */

void parse_db_update (void)
{
	struct stat st;
	GSList *node;
	db_t *parse_db;

	for (node = parse.db_list; node; node = node->next)
	{
		parse_db = node->data;

		if (stat (parse_db->filename, &st))
		{
			parse.db_list = g_slist_remove (parse.db_list, parse_db);
			db_deallocate (parse_db, NULL);
			parse_list_compile ();
			return;
		}
		else if (st.st_mtime > parse_db->access.tv_sec)
		{
			printt ("Parse database updated");
			parse_list_compile ();
			return;
		}
	}
}


/* =========================================================================
 = PARSE_REGEXP
 =
 = Do regexp matching
 ======================================================================== */

static gboolean parse_regexp (parse_regexp_t *parse_regexp, gchar *subject)
{
	parse_action_t *action;
	GSList *node;
	gint ovector[PARSE_SUBSTR_NUM*3];
	gint rc;

	g_assert (parse_regexp != NULL);
	g_assert (subject != NULL);

	/* attempt match */
	rc = pcre_exec (parse_regexp->compiled, parse_regexp->studied,
		subject, strlen (subject), 0, 0, ovector, PARSE_SUBSTR_NUM*3);

	/* match unsucessful */
	if (rc < 0)
		return FALSE;

	/* execute defined actions */
	for (node = parse_regexp->actions; node; node = node->next)
	{
		action = node->data;
		if (!parse_action_dispatch (action, subject, parse_regexp, ovector, rc))
			break;
	}

	selected_player = NULL;

	return TRUE;
}


/* ==========================================================================
 = PARSE_REGEXP_LIST
 =
 = Attempt to match string in regexp list
 ========================================================================= */

static void parse_regexp_list (gchar *subject)
{
	GSList *node;

	for (node = parse.regexp_list; node; node = node->next)
		parse_regexp (node->data, subject);
}


/* =========================================================================
 = PARSE_LINE_BUFFER
 =
 = Break the input stream into lines for parsing
 ======================================================================== */

void parse_line_buffer (guchar ch)
{
	gchar *p;

	g_assert (parse.line_buf != NULL);

	if (ch == '\b')
	{
		parse.line_buf = g_string_truncate (parse.line_buf,
			MAX (0, parse.line_buf->len - 1));
		return;
	}
	if (ch == '\n')
	{
		parse_line (parse.line_buf->str);
		parse.line_buf = g_string_assign (parse.line_buf, "");
		return;
	}

	parse.line_buf = g_string_append_c (parse.line_buf, ch);

	/* remove resting/meditating tags from prompt */
	if ((p = strstr (parse.line_buf->str, STR_RESTING)) != NULL)
		parse.line_buf = g_string_erase (parse.line_buf,
			p - parse.line_buf->str, strlen (STR_RESTING));

	if ((p = strstr (parse.line_buf->str, STR_MEDITATING)) != NULL)
		parse.line_buf = g_string_erase (parse.line_buf,
			p - parse.line_buf->str, strlen (STR_MEDITATING));

	/* parse HP/MA ticks ASAP */
	if (!strncmp (parse.line_buf->str, "[HP=", 4))
	{
		if (strstr (parse.line_buf->str, "]:"))
		{
			parse_regexp_list (parse.line_buf->str);
			parse.line_buf = g_string_assign (parse.line_buf, "");
		}
	}

	if (character.option.auto_all) /* handle login/password, triggers */
	{
		if (!character.flag.ready && character.username
			&& !strcmp (parse.line_buf->str, "Otherwise type \"new\": "))
			send_line (character.username);
		else if (!character.flag.ready && character.password
			&& !strcmp (parse.line_buf->str, "Enter your password: "))
			send_line (character.password);
		else
			character_triggers_parse (parse.line_buf->str);
	}
}


/* =========================================================================
 = PARSE_LINE
 =
 = Handle incoming lines from the server
 ======================================================================== */

void parse_line (gchar *line)
{
	if (line[0] == '\0')
		return;

	mudpro_capture_log_append (line);

	if (character.flag.looking)
	{
		parse_regexp_list (line);
		return; /* no local parsing while looking */
	}

	if (parse.line_wrap)
	{
		g_string_append (parse.wrap_buf, " ");
		g_string_append (parse.wrap_buf, line);
		if (index (parse.wrap_buf->str, '.'))
		{
			parse.line_wrap = FALSE;
			line = parse.wrap_buf->str;
		}
		else
			return;
	}

	/* == local parsing ================================================= */

	/* looks like this could be the room name, hang on to it */
	if (terminal.attr == (ATTR_CYAN | A_BOLD) && isalpha (line[0]))
		parse.room_name = g_string_assign (parse.room_name, line);

	if (!strncmp (line, "You are carrying", 16))
	{
		command_del (CMD_INVENTORY);
		character.flag.inventory = TRUE;

		if (strstr (line, "Nothing!"))
			return;

		if (index (line, '.'))
			item_inventory_list_build (line);
		else
		{
			/* NOTE: this will also pick up "You have the following keys" */
			/* and add them to inventory as well */
			parse.line_wrap = TRUE;
			parse.wrap_buf = g_string_assign (parse.wrap_buf, line);
		}
	}

	else if (!strncmp (line, "You have the following keys:", 28))
	{
		/* if we reached this, we have no items but we do have keys */

		if (index (line, '.'))
			item_inventory_list_build (line+28);
		else
		{
			parse.line_wrap = TRUE;
			parse.wrap_buf = g_string_assign (parse.wrap_buf, line);
		}
	}

	else if (!strncmp (line, "You notice", 10)
		&& !strstr (line, "sneak in")
		&& !strstr (line, "sneaking out")
		&& !strstr (line, "nothing different to the"))
	{
		if ((!character.option.get_items && !character.option.get_cash)
			|| !character.option.auto_all
			|| !character.flag.inventory)
			return;

		if (character.flag.running)
			return;

		if (strstr (line, "here."))
		{
			item_visible_list_build (line);
			character.flag.searching = FALSE;
		}
		else
		{
			parse.line_wrap = TRUE;
			parse.wrap_buf = g_string_assign (parse.wrap_buf, line);
		}
	}

	else if (!strncmp (line, "Also here:", 10))
	{
		if (index (line, '.'))
		{
			if (CAN_ATTACK
				&& character.option.auto_all
				&& !character.flag.running)
				monster_target_list_build (line);
		}
		else
		{
			parse.line_wrap = TRUE;
			parse.wrap_buf = g_string_assign (parse.wrap_buf, line);
		}
	}

	else if (!strncmp (line, "Obvious exits:", 14))
	{
		/* TODO: support line wrapping here */

		navigation.collisions  = 0;
		navigation.room_dark   = FALSE;
		character.status.blind = FALSE;

		command_del (CMD_MOVE);
		command_del (CMD_TARGET);

		automap.room_name = g_string_assign (
			automap.room_name, parse.room_name->str);
		automap_parse_exits (line);
	}

	else if (!strcmp (line, "*Combat Engaged*"))
	{
		character.state = STATE_ENGAGED;
		command_del (CMD_ATTACK);
	}

	else if (!strcmp (line, "*Combat Off*"))
	{
		character.state = STATE_NONE;
		command_del (CMD_BREAK);

		if (!command_pending (CMD_ATTACK))
			combat.strategy.current = NULL;
	}

	else /* run through external parsers */
	{
		spell_db_parse (line);
		parse_regexp_list (line);
	}

	return;
}
