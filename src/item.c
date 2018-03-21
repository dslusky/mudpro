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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "automap.h"
#include "character.h"
#include "combat.h"
#include "command.h"
#include "item.h"
#include "navigation.h"
#include "mudpro.h"
#include "timers.h"
#include "terminal.h"
#include "utils.h"

key_value_t item_flags[]=
{
	{ "Usable",    ITEM_FLAG_USABLE },
	{ "Currency",  ITEM_FLAG_CURRENCY },
	{ "Light",     ITEM_FLAG_LIGHT },
	{ "Stash",     ITEM_FLAG_STASH },
	{ "AutoGet",   ITEM_FLAG_AUTO_GET },
	{ "AutoDrop",  ITEM_FLAG_AUTO_DROP },
	{ "AutoArm",   ITEM_FLAG_AUTO_ARM },
	{ "ModHealth", ITEM_FLAG_MOD_HEALTH },
	{ "ModStats",  ITEM_FLAG_MOD_STATS },
	{ NULL, 0 }
};

key_value_t equip_flags[]=
{
	{ "Arms",        EQUIP_FLAG_ARMS },
	{ "Back",        EQUIP_FLAG_BACK },
	{ "Ears",        EQUIP_FLAG_EARS },
	{ "Finger",      EQUIP_FLAG_FINGER },
	{ "Feet",        EQUIP_FLAG_FEET },
	{ "Hands",       EQUIP_FLAG_HANDS },
	{ "Head",        EQUIP_FLAG_HEAD },
	{ "Legs",        EQUIP_FLAG_LEGS },
	{ "Neck",        EQUIP_FLAG_NECK },
	{ "Readied",     EQUIP_FLAG_READY },
	{ "Torso",       EQUIP_FLAG_TORSO },
	{ "Waist",       EQUIP_FLAG_WAIST },
	{ "Weapon Hand", EQUIP_FLAG_WEAPON },
	{ "WeaponHand",  EQUIP_FLAG_WEAPON },
	{ "Two handed",  EQUIP_FLAG_WEAPON },
	{ "Wrist",       EQUIP_FLAG_WRIST },
	{ NULL, 0 }
};

gint item_light_sources = 0;
GSList *visible_items = NULL;
static GHashTable *item_db;

static void item_db_free (void);
static void item_record_deallocate (gpointer key, gpointer value,
    gpointer user_data);
static void item_db_parse_option (item_t *item, const gchar *str);
static void item_deallocate (item_t *item);


/* =========================================================================
 = ITEM_DB_INIT
 =
 = Initialize the item database
 ======================================================================== */

void item_db_init (void)
{
	mudpro_db.items.filename = g_strdup_printf (
		"%s%citems.db", character.data_path, G_DIR_SEPARATOR);

	item_db = NULL;
	item_db_load ();
}


/* =========================================================================
 = ITEM_DB_CLEANUP
 =
 = Cleanup after item database
 ======================================================================== */

void item_db_cleanup (void)
{
	item_db_free ();
	item_inventory_list_free ();
	item_visible_list_free ();

	g_free (mudpro_db.items.filename);
}


/* =========================================================================
 = ITEM_DB_LOAD
 =
 = (Re)load item database from file
 ======================================================================== */

void item_db_load (void)
{
	gchar buf[STD_STRBUF];
	gchar *offset, *tmp;
	item_t *item = NULL;
	FILE *fp;

	if ((fp = fopen (mudpro_db.items.filename, "r")) == NULL)
	{
		printt ("Unable to open item database");
		return;
	}

	g_get_current_time (&mudpro_db.items.access);

	if (item_db != NULL)
		item_db_free ();

	item_db = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_freeze (item_db);

	while (fgets (buf, sizeof (buf), fp))
	{
		strchomp (buf);

		if (buf[0] == '#' || buf[0] == '\0')
		{
			item = NULL;
			continue;
		}

		if (isspace (buf[0]))
		{
			if (item != NULL)
				item_db_parse_option (item, buf);
			continue;
		}

		offset = buf;
		if ((tmp = get_token_as_str (&offset)) == NULL)
			continue;

		if (!strcasecmp (tmp, "Item"))
		{
			item = g_malloc0 (sizeof (item_t));

			if ((item->name = get_token_as_str (&offset)) == NULL)
			{
				g_free (item);
				item = NULL;
			}
			else
				g_hash_table_insert (item_db, item->name, item);
		}
		g_free (tmp);
	}

	g_hash_table_thaw (item_db);

	fclose (fp);
}


/* =========================================================================
 = ITEM_DB_SAVE
 =
 = Save item database to file
 ======================================================================== */

void item_db_save (void)
{
}


/* =========================================================================
 = ITEM_DB_LOOKUP
 =
 = Lookup item in database
 ======================================================================== */

item_t *item_db_lookup (gchar *str)
{
	return g_hash_table_lookup (item_db, str);
}


/* =========================================================================
 = ITEM_DB_FREE
 =
 = Free memory allocated to item database
 ======================================================================== */

static void item_db_free (void)
{
	g_hash_table_foreach (item_db, item_record_deallocate, NULL);
	g_hash_table_destroy (item_db);
	item_db = NULL;
}


/* =========================================================================
 = ITEM_RECORD_DEALLOCATE
 =
 = Free memory allocated to item record
 ======================================================================== */

static void item_record_deallocate (gpointer key, gpointer value,
	gpointer user_data)
{
	item_deallocate (value);
}


/* =========================================================================
 = ITEM_DB_PARSE_OPTION
 =
 = Parses item option and adds data to db record
 ======================================================================== */

static void item_db_parse_option (item_t *item, const gchar *str)
{
	gchar *offset, *option, *arg;
	glong value;

	if (!item)
		return;

	offset = option = arg = NULL;

	offset = (gchar *) str;
	if ((option = get_token_as_str (&offset)) == NULL)
		return;

	if (!strcasecmp (option, "Equip"))
	{
		key_value_t *e;

		g_free (option);

		if ((arg = get_token_as_str (&offset)) == NULL)
			return;

		for (e = equip_flags; e->key; e++)
		{
			if (strcasecmp (arg, e->key))
				continue;

			FlagON (item->equip, e->value);
			g_free (arg);
			return;
		}
	}

	else if (!strcasecmp (option, "Limit"))
	{
		g_free (option);
		value = get_token_as_long (&offset);
		item->limit = MAX (0, value);
	}

	else if (!strcasecmp (option, "Reserve"))
	{
		g_free (option);
		value = get_token_as_long (&offset);
		item->reserve = MAX (0, value);
	}

	else if (!strcasecmp (option, "SetFlag"))
	{
		key_value_t *i;

		g_free (option);

		if ((arg = get_token_as_str (&offset)) == NULL)
			return;

		for (i = item_flags; i->key; i++)
		{
			if (strcasecmp (arg, i->key))
				continue;

			FlagON (item->flags, i->value);
			g_free (arg);
			return;
		}
	}

	else if (!strcasecmp (option, "Value"))
	{
		g_free (option);
		value = get_token_as_long (&offset);
		item->value = MAX (0, value);
	}

	g_free (arg);
}


/* =========================================================================
 = ITEM_LIST_ADD
 =
 = Add item to the list
 ======================================================================== */

GSList *item_list_add (GSList *slist, const gchar *str)
{
	item_t *item, *control;
	gchar *pos, *pos2, *buf;
	gulong equip;
	gint quantity;

	g_assert (str != NULL);

	quantity = 1;
	equip    = 0;

	/* create local copy we can modify */
	buf = g_strdup (str);

	/* check if item is equipped */
	if ((pos = item_equip_parse ((gchar *) buf, &equip)) != NULL)
		*(pos-2) = '\0'; /* remove tag from string */

	pos = buf;

	if (isdigit (pos[0]))
	{
		quantity = atoi (pos);
		while (*pos && !isalpha (*pos)) pos++;

		/* remove plurality */
		pos2 = pos + strlen (pos)-1;
		if (*pos2 == 's') *pos2 = '\0';
	}

	/* get control record */
	control = item_db_lookup (pos);

	/* item already in list, update existing record */
	if ((item = item_list_lookup (slist, pos)) != NULL)
	{
		if (control)
		{
			item->flags = control->flags;
			item->equip = control->equip;
		}
		else
		{
			item->flags = 0;
			item->equip = equip;
		}
		item->quantity += quantity;
		item->armed = CLAMP (item->armed | equip, 0, 1);
		item->hidden = character.flag.searching;
		g_free (buf);
		return slist;
	}

	/* item not in list, allocate space for new one */
	item = g_malloc0 (sizeof (item_t));
	if (control)
	{
		item->flags = control->flags;
		item->equip = control->equip;
	}
	else
	{
		item->flags = 0;
		item->equip = equip;
	}
	item->name = g_strdup (pos);
	item->quantity += quantity;
	item->armed = CLAMP (item->armed | equip, 0, 1);
	item->hidden = character.flag.searching;
	g_free (buf);
	return g_slist_prepend (slist, item);
}


/* =========================================================================
 = ITEM_LIST_MULTIPLE_ADD
 =
 = Add multiple items to list, via comma separated list
 ======================================================================== */

GSList *item_list_multiple_add (GSList *slist, const gchar *item_list)
{
	gchar *offset, *item;

	offset = (gchar *) item_list;

	while ((item = get_token_as_str (&offset)) != NULL)
	{
		slist = item_list_add (slist, item);
		g_free (item);
	}

	return slist;
}


/* =========================================================================
 = ITEM_LIST_DEL
 =
 = Remove item from list
 ======================================================================== */

GSList *item_list_del (GSList *slist, const gchar *str)
{
	gchar *pos, *pos2, *buf;
	item_t *item;
	gint quantity;

	quantity = 1;

	/* create local copy we can modify */
	buf = g_strdup (str);
	pos = buf;

	if (isdigit (pos[0]))
	{
		quantity = atoi (pos);
		while (*pos && !isalpha (*pos)) pos++;

		/* remove plurality from item name */
        pos2 = pos + strlen (pos)-1;
		if (*pos2 == 's') *pos2 = '\0';
	}

	if ((item = item_list_lookup (slist, pos)) == NULL)
	{
		g_free (buf);
		return slist; /* item not in list */
	}

	if (item->quantity <= quantity)
	{
		/* remove item from list */
		slist = g_slist_remove (slist, item);
		item_deallocate (item);
	}
	else /* update item record */
		item->quantity -= quantity;

	g_free (buf);
	return slist;
}


/* =========================================================================
 = ITEM_LIST_MULTIPLE_DEL
 =
 = Remove multiple items from list, via comma separated list
 ======================================================================== */

GSList *item_list_multiple_del (GSList *slist, const gchar *item_list)
{
	gchar *offset, *item;

	offset = (gchar *) item_list;

	while ((item = get_token_as_str (&offset)) != NULL)
	{
		slist = item_list_del (slist, item);
		g_free (item);
	}

	return slist;
}


/* =========================================================================
 = ITEM_LIST_LOOKUP
 =
 = Lookup item in list
 ======================================================================== */

item_t *item_list_lookup (GSList *slist, gchar *name)
{
	GSList *node;
	item_t *item;

	if (!slist || !name)
		return NULL;

	for (node = slist; node; node = node->next)
	{
		item = node->data;

		if (!strcasecmp (item->name, name))
			return item;
	}

	return NULL;
}


/* =========================================================================
 = ITEM_LIST_FREE
 =
 = Free memory allocated to item list
 ======================================================================== */

void item_list_free (GSList *slist)
{
	GSList *node;

	for (node = slist; node; node = node->next)
		item_deallocate (node->data);
}


/* =========================================================================
 = ITEM_INVENTORY_LIST_BUILD
 =
 = Parse string for items and add them to inventory
 ======================================================================== */

void item_inventory_list_build (gchar *str)
{
	gchar *str_items_carried = "You are carrying ";
	gchar *str_keys_carried  = "You have the following keys: ";
	gchar *str_keys_none     = "You have no keys.";
	GString *item_list;
	gchar *pos;

	g_assert (str != NULL);

	if (character.inventory)
		item_inventory_list_free ();

	item_light_sources = 0;

	item_list = g_string_new (str);

	if ((pos = strstr (item_list->str, str_items_carried)) != NULL)
		item_list = g_string_erase (item_list, pos-item_list->str,
			strlen (str_items_carried));

	if ((pos = strstr (item_list->str, str_keys_carried)) != NULL)
	{
		item_list = g_string_erase (item_list, pos-item_list->str,
			strlen (str_keys_carried));
		item_list = g_string_insert_c (item_list, pos-item_list->str, ',');
	}
	else if ((pos = strstr (item_list->str, str_keys_none)) != NULL)
		item_list = g_string_erase (item_list, pos-item_list->str,
			strlen (str_keys_none));

	character.inventory = item_list_multiple_add (
		character.inventory, item_list->str);
	g_string_free (item_list, TRUE);
	item_light_sources = item_inventory_get_quantity_by_flag (
		ITEM_FLAG_USABLE | ITEM_FLAG_LIGHT);
}


/* =========================================================================
 = ITEM_INVENTORY_LIST_FREE
 =
 = Free data allocated to inventory list
 ======================================================================== */

void item_inventory_list_free (void)
{
	item_list_free (character.inventory);
	character.inventory = NULL;
}


/* =========================================================================
 = ITEM_INVENTORY_MANAGE
 =
 = Manage items in character inventory
 ======================================================================== */

void item_inventory_manage (void)
{
	GSList *node;
	item_t *control, *item;
	gint excess;

	for (node = character.inventory; node; node = node->next)
	{
		item = node->data;

		if ((control = item_db_lookup (item->name)) == NULL)
			continue; /* item not in control list */

		if (control->flags & ITEM_FLAG_AUTO_DROP)
		{
			if (control->flags & ITEM_FLAG_CURRENCY)
				command_send_va (CMD_DROP, "%d %s",
					item->quantity, item->name);
			else
				command_send_va (CMD_DROP, "%s", item->name);

			return;
		}

		if (control->limit) /* drop excess items */
		{
			/* take both limit and reserve into account */
			excess = MAX (0,
				item->quantity - MAX (control->limit, control->reserve));

			if (excess)
			{
				if (control->flags & ITEM_FLAG_CURRENCY)
					command_send_va (CMD_DROP, "%d %s", excess, item->name);
				else
					command_send_va (CMD_DROP, "%s", item->name);

				return;
			}
		}

		if (automap.location && !automap.lost && character.option.stash &&
			(automap.location->flags & ROOM_FLAG_STASH) &&
			(control->flags & ITEM_FLAG_STASH))
		{
			/* stash non-reserve items */

			excess = MAX (0, item->quantity - control->reserve);

			if (excess)
			{
				if (control->flags & ITEM_FLAG_CURRENCY)
					command_send_va (CMD_HIDE, "%d %s", excess, item->name);
				else
					command_send_va (CMD_HIDE, "%s", item->name);

				return;
			}
		}

		if ((control->flags & ITEM_FLAG_AUTO_ARM) && !item->armed)
		{
			if (combat.strategy.current && combat.strategy.current->weapon)
				continue;

			command_send_va (CMD_ARM, "%s", item->name);
			return;
		}

		if (!item->armed && navigation.room_dark &&
			(control->flags & (ITEM_FLAG_USABLE | ITEM_FLAG_LIGHT)))
		{
			/* determine if we should use light source or conserve it */
			if (character.option.reserve_light && !automap.lost &&
				character.light_mode > LIGHT_RESERVE_LOW)
				continue;

			/* disregard light mode when light sources is low */
			if (character.option.reserve_light && !automap.lost &&
				item_light_sources < 2)
				continue;

			command_send_va (CMD_USE, "%s", item->name);
			return;
		}

		if (item->armed && !navigation.room_dark &&
			(control->flags & (ITEM_FLAG_USABLE | ITEM_FLAG_LIGHT)))
		{
			/* determine if we should remove light source */

			if (!character.option.reserve_light || automap.lost ||
				command_pending (CMD_SEARCH))
				continue;

			if (character.light_mode == LIGHT_RESERVE_LOW &&
				item_light_sources > 1)
				continue;

			if (character.light_mode == LIGHT_RESERVE_MED
				&& character.state == STATE_NONE)
				continue;

			command_send_va (CMD_REMOVE, "%s", item->name);
			return;
		}
	}
}


/* =========================================================================
 = ITEM_INVENTORY_GET_QUANTITY_BY_FLAG
 =
 = Find quantity of item(s) by flag(s)
 ======================================================================== */

gint item_inventory_get_quantity_by_flag (gulong flags)
{
	GSList *node;
	item_t *item;
	gint quantity = 0;

	for (node = character.inventory; node; node = node->next)
	{
		item = node->data;

		if (item->flags & flags)
			quantity += item->quantity;
	}
	return quantity;
}


/* =========================================================================
 = ITEM_INVENTORY_GET_CASH_AMOUNT
 =
 = Returns total amount of cash on hand (in copper)
 ======================================================================== */

gulong item_inventory_get_cash_amount (void)
{
	GSList *node;
	item_t *item, *control;
	gulong amount = 0;

	for (node = character.inventory; node; node = node->next)
	{
		item = node->data;

		if ((control = item_db_lookup (item->name)) == NULL)
			continue; /* not in control list */

		if (!item->quantity ||
			!(control->flags & ITEM_FLAG_CURRENCY))
			continue;

		if (!control->value)
		{
			printt ("Monetary value for '%s' has not been set!", control->name);
			continue;
		}

		amount += (control->value * item->quantity);
	}
	return amount;
}


/* =========================================================================
 = ITEM_VISIBLE_LIST_BUILD
 =
 = Build list of visible items
 ======================================================================== */

void item_visible_list_build (gchar *str)
{
	gchar *pos;

	g_assert (str != NULL);

	if (visible_items)
		item_visible_list_free ();

	str = g_strdup (str + strlen ("You notice "));
	if ((pos = strstr (str, " here.")) != NULL)
		*pos = '\0';

	visible_items = item_list_multiple_add (visible_items, str);
	g_free (str);
}


/* =========================================================================
 = ITEM_VISIBLE_LIST_FREE
 =
 = Free memory allocated to visible item list
 ======================================================================== */

void item_visible_list_free (void)
{
	item_list_free (visible_items);
	visible_items = NULL;
}


/* =========================================================================
 = ITEM_EQUIP_PARSE
 =
 = Determine whether item is equipped, return tag offset
 ======================================================================== */

gchar *item_equip_parse (gchar *str, gulong *equip_flag)
{
	key_value_t *e;
	gchar *pos;

	g_assert (str != NULL);

	for (e = equip_flags; e->key; e++)
	{
		if ((pos = strstr (str, e->key)) != NULL)
		{
			if (equip_flag) *equip_flag = e->value;
			return pos;
		}
	}
	return NULL;
}


/* =========================================================================
 = ITEM_PICKUP_NEXT
 =
 = Get next item for retrieval
 ======================================================================== */

item_t *item_pickup_next (gint *quantity)
{
	GSList *node;
	item_t *item, *control, *inventory;
	gint tmp, limit;

	g_assert (quantity != NULL);

	for (node = visible_items; node; node = node->next)
	{
		item = node->data;

		if ((control = item_db_lookup (item->name)) == NULL)
			continue; /* item not in control list */

		/* attempt to disqualify item */
		if ((control->flags & ITEM_FLAG_AUTO_DROP) ||
			!(control->flags & ITEM_FLAG_AUTO_GET))
			continue;

		/* item is already hidden at a stash point */
		if (item->hidden && character.option.stash &&
			automap.location &&	(automap.location->flags & ROOM_FLAG_STASH))
			continue; /* FIXME: this should take reserve into account */

		if ((control->flags & ITEM_FLAG_CURRENCY) && !character.option.get_cash)
			continue; /* do not pickup cash */

		if (!(control->flags & ITEM_FLAG_CURRENCY) && !character.option.get_items)
			continue; /* do not pickup items */

		if (!control->limit)
		{
			/* no limit set, take everything */
			*quantity = item->quantity;
			return item;
		}

		inventory = item_list_lookup (character.inventory, item->name);
		limit = MAX (control->limit, control->reserve);

		if (inventory) /* take difference between inventory and limit */
			tmp = CLAMP (item->quantity, 0, MAX (0, limit - inventory->quantity));
		else
			tmp = CLAMP (item->quantity, 0, limit);

		if (tmp) *quantity = tmp;
		else continue; /* no quantity to take */

		*quantity = tmp;
		return item;
	}

	return NULL;
}


/* =========================================================================
 = ITEM_DEALLOCATE
 =
 = Deallocate the item
 ======================================================================== */

static void item_deallocate (item_t *item)
{
	g_assert (item != NULL);
	g_free (item->name);
	g_free (item);
}


/* =========================================================================
 = ITEM_LIGHT_SOURCE_ACTIVATE
 =
 = Activate an light source item
 ======================================================================== */

gboolean item_light_source_activate (void)
{
	GSList *node;
	item_t *item, *control;

	for (node = character.inventory; node; node = node->next)
	{
		item = node->data;

		if ((control = item_db_lookup (item->name)) == NULL)
			continue; /* not in control list */

		if (!item->armed &&
			(control->flags & (ITEM_FLAG_USABLE | ITEM_FLAG_LIGHT)))
		{
			command_send_va (CMD_USE, "%s", item->name);
			return TRUE;
		}
	}
	return FALSE;
}


/* =========================================================================
 = ITEM_LIGHT_SOURCE_DEACTIVATE
 =
 = Deactivate current light source
 ======================================================================== */

gboolean item_light_source_deactivate (void)
{
	GSList *node;
	item_t *item, *control;

	for (node = character.inventory; node; node = node->next)
	{
		item = node->data;

		if ((control = item_db_lookup (item->name)) == NULL)
			continue; /* not in control list */

		if (item->armed &&
			(control->flags & (ITEM_FLAG_USABLE | ITEM_FLAG_LIGHT)))
		{
			command_send_va (CMD_REMOVE, "%s", item->name);
			return TRUE;
		}
	}
	return FALSE;
}
