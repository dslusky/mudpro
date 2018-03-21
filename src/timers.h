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
	(timer_elapsed (timers.castwait, NULL) || \
	(timer_elapsed (timers.round, NULL) > 2))

typedef struct {
    gboolean running;
    struct timeval start;
    struct timeval stop;
} _timer_t;

typedef struct {
	_timer_t *dbupdate;    /* database polling */
	_timer_t *castwait;    /* casting throttle */
	_timer_t *client_ai;   /* client AI and automation */
	_timer_t *connect;     /* (re)connect throttle */
	_timer_t *duration;    /* exp gathering duration */
	_timer_t *player_anim; /* player animations */
	_timer_t *refresh;     /* screen refresh throttle */
	_timer_t *round;       /* round timer */
	_timer_t *status;      /* status update */
	_timer_t *idle;        /* idle timeout */
	_timer_t *mudpro;      /* overall client duration */
	_timer_t *online;      /* time spent online */
	_timer_t *osd_stats;   /* stats osd cycle */
	_timer_t *recall;      /* recall throttle */
	_timer_t *parcmd;      /* check party status */
} timers_t;

extern gint connect_wait;
extern timers_t timers;

void timers_init (void);
void timers_cleanup (void);
void timers_report (FILE *fp);
void timers_update (void);

_timer_t *timer_new(void);
void timer_destroy(_timer_t *timer);
void timer_start(_timer_t *timer);
void timer_stop(_timer_t *timer);
gdouble timer_elapsed(_timer_t *timer, gulong *usec);
void timer_reset(_timer_t *timer);

#endif /* __TIMERS_H__ */
