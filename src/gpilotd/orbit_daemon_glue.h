/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- *//* 
 * Copyright (C) 1998-2000 Free Software Foundation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen
 *
 */

#ifndef __ORBIT_GPILOTD_GLUE__
#define __ORBIT_GPILOTD_GLUE__

#include "gnome-pilot.h"
#include "queue_io.h"
#include "gnome-pilot-structures.h"
#include <pi-dlp.h>

#define GPC_DEBUG

/* the LOG macro cannot by used without a format */
#ifdef GPC_DEBUG
#define LOG(x) g_message x
#else
#define LOG(x)
#endif


typedef struct GPilotd_Orb_Context GPilotd_Orb_Context;
struct GPilotd_Orb_Context { 
  /* maps from a pilot name GPilotd_Orb_Pilot_Notifications* */ 
  GHashTable *notifications;
  /* maps from a client_id to GSList** of pilotnames begin monitored */
  GHashTable *monitors;
  GPilotContext *gpilotd_context;
};
extern GPilotd_Orb_Context *orb_context;

/* returns next time for request timeout (in secs) */
/* FIXME: is missing, needed to ensure timeout operations */
guint get_next_timeout();

/* removes requests with req.timeout!= && req.timeout <=timeout */
/* FIXME: is missing, needed to ensure timeout operations */
void remove_timeouts(guint timeout);

/* check if a pilot is currently connceted */
/* FIXME: is missing, needed to ensure, that noone requests
   stuff of a syncing pilot */
gint is_pilot_busy(gchar *pilot_id);


/* Prototypes for methods declared in orbit_daemon_glue.c */

GList *get_cradle_events(gchar*);
GList *get_requests_for_pilot(guint32 pilot_id,GPilotRequestType type);

void orbed_notify_connect(const gchar *pilot_id, struct PilotUser user_info);
void orbed_notify_disconnect(const gchar *pilot_id);
void orbed_notify_completion(GPilotRequest **req);
void orbed_notify_timeout(guint handle);
void orbed_notify_userinfo(struct PilotUser user_info,GPilotRequest **req);
void orbed_notify_sysinfo(gchar *pilot_id,
			  struct SysInfo sysinfo,
			  struct CardInfo cardinfo,
			  GPilotRequest **req);

void orbed_notify_conduit_start(gchar *pilot_id,
				GnomePilotConduit *conduit,
				GnomePilotConduitSyncType);
void orbed_notify_conduit_end(gchar *pilot_id,
			      GnomePilotConduit *conduit);
void orbed_notify_conduit_error(gchar *pilot_id,				
				GnomePilotConduit *conduit,
				gchar *message);
void orbed_notify_conduit_message(gchar *pilot_id,				
				  GnomePilotConduit *conduit,
				  gchar *message);
void orbed_notify_conduit_progress(gchar *pilot_id,				
				   GnomePilotConduit *conduit,
				   gint,gint);
void orbed_notify_overall_progress(gchar *pilot_id,				
				   gint,gint);
void orbed_notify_daemon_message(gchar *pilot_id,				
				 GnomePilotConduit *conduit,
				 gchar *message);
void orbed_notify_daemon_error (gchar *pilot_id,				
				gchar *message);

void gpilotd_corba_init_context(GPilotContext*);
void gpilotd_corba_clean_up(void);
void gpilotd_corba_quit(void);
void gpilotd_corba_init(int*,char **,GPilotContext**);
int gpilotd_corba_thread();

gint match_pilot_userID(GPilotPilot *,guint32 *);
guint32 pilot_id_from_name(const gchar*,GPilotContext*);
gchar *pilot_name_from_id(guint32,GPilotContext *);

#endif
