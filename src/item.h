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

#ifndef __ITEM_H__
#define __ITEM_H__

#include "defs.h"

typedef struct
{
	gchar *name;

	/* control */
	gint limit;     /* maximum allowed in inventory */
	gint reserve;   /* amount to hold in reserve */
	gint value;     /* monetary value (in copper) */
	gulong equip;   /* equipment type */
	gulong flags;   /* misc item flags */

	/* instance */
	gint quantity;   /* amount visible/on hand */
	gboolean armed;  /* item is armed */
	gboolean hidden; /* item is hidden */
} item_t;

enum /* item flags */
{
	ITEM_FLAG_USABLE     = 1 << 0,
	ITEM_FLAG_CURRENCY   = 1 << 1,
	ITEM_FLAG_LIGHT      = 1 << 2,
	ITEM_FLAG_STASH      = 1 << 3,
	ITEM_FLAG_AUTO_GET   = 1 << 4,
	ITEM_FLAG_AUTO_DROP  = 1 << 5,
	ITEM_FLAG_AUTO_ARM   = 1 << 6,
	ITEM_FLAG_MOD_HEALTH = 1 << 7,
	ITEM_FLAG_MOD_STATS  = 1 << 8,
};

enum /* equipment types/flags */
{
	EQUIP_FLAG_ARMS   = 1 << 0,
	EQUIP_FLAG_BACK   = 1 << 1,
	EQUIP_FLAG_EARS   = 1 << 2,
	EQUIP_FLAG_FINGER = 1 << 3,
	EQUIP_FLAG_FEET   = 1 << 4,
	EQUIP_FLAG_HANDS  = 1 << 5,
	EQUIP_FLAG_HEAD   = 1 << 6,
	EQUIP_FLAG_LEGS   = 1 << 7,
	EQUIP_FLAG_NECK   = 1 << 8,
	EQUIP_FLAG_READY  = 1 << 9,
	EQUIP_FLAG_TORSO  = 1 << 10,
	EQUIP_FLAG_WAIST  = 1 << 11,
	EQUIP_FLAG_WEAPON = 1 << 12,
	EQUIP_FLAG_WRIST  = 1 << 13,
};

extern key_value_t item_flags[];
extern key_value_t equip_flags[];
extern gint item_light_sources;
extern GSList *visible_items;

void item_db_init (void);
void item_db_cleanup (void);
void item_db_load (void);
void item_db_save (void);
item_t *item_db_lookup (gchar *str);
GSList *item_list_add (GSList *slist, const gchar *str);
GSList *item_list_multiple_add (GSList *slist, const gchar *item_list);
GSList *item_list_del (GSList *slist, const gchar *str);
GSList *item_list_multiple_del (GSList *slist, const gchar *item_list);
item_t *item_list_lookup (GSList *slist, gchar *name);
void item_list_free (GSList *slist);
void item_inventory_list_build (gchar *str);
void item_inventory_list_free (void);
void item_inventory_manage (void);
gint item_inventory_get_quantity_by_flag (gulong flags);
gulong item_inventory_get_cash_amount (void);
void item_visible_list_build (gchar *str);
void item_visible_list_free (void);
gchar *item_equip_parse (gchar *str, gulong *equip_flag);
item_t *item_pickup_next (gint *quantity);
void item_list_debug (GSList *slist);
gboolean item_light_source_activate (void);
gboolean item_light_source_deactivate (void);

#endif /* __ITEM_H__ */
