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

#ifndef __TIMERS_H__
#define __TIMERS_H__

#include <glib.h>
#include <sys/types.h>
#include <time.h>

#define TIMEOUT_SEC_DBUPDATE		1
#define TIMEOUT_SEC_CASTWAIT		5
#define TIMEOUT_SEC_ROUND			5
#define TIMEOUT_SEC_STATUS			1
#define TIMEOUT_SEC_IDLE            30
#define TIMEOUT_SEC_OSD_STATS       5
#define TIMEOUT_SEC_RECALL			5

#define TIMEOUT_USEC_CLIENT_AI		200000
#define TIMEOUT_USEC_PLAYER_ANIM	50000
#define TIMEOUT_USEC_REFRESH		25000

#define CASTWAIT \
	((int)g_timer_elapsed (timers.castwait, NULL) || \
	((int)g_timer_elapsed (timers.round, NULL) > 2))

typedef struct
{
	GTimer *dbupdate;    /* database polling */
	GTimer *castwait;    /* casting throttle */
	GTimer *client_ai;   /* client AI and automation */
	GTimer *connect;     /* (re)connect throttle */
	GTimer *duration;    /* exp gathering duration */
	GTimer *player_anim; /* player animations */
	GTimer *refresh;     /* screen refresh throttle */
	GTimer *round;       /* round timer */
	GTimer *status;      /* status update */
	GTimer *idle;        /* idle timeout */
	GTimer *mudpro;      /* overall client duration */
	GTimer *online;      /* time spent online */
	GTimer *osd_stats;   /* stats osd cycle */
	GTimer *recall;      /* recall throttle */
	GTimer *parcmd;      /* check party status */
} timers_t;

extern gint connect_wait;
extern timers_t timers;

void timers_init (void);
void timers_cleanup (void);
void timers_report (FILE *fp);
void timers_update (void);

#endif /* __TIMERS_H__ */
