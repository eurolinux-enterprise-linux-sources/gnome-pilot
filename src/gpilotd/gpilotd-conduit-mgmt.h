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


#ifndef __GPILOTD_CONDUIT_MGMT_H
#define __GPILOTD_CONDUIT_MGMT_H

#include <gnome.h>
#include <gmodule.h>
#include "gnome-pilot-conduit.h"
/*#include "gpilot-structures.h"*/

#define USE_GMODULE

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef GnomePilotConduit *(*conduit_get_gpilot_conduit)(guint32);
typedef void (*conduit_destroy_gpilot_conduit)(GnomePilotConduit *);

typedef struct _GnomePilotConduitMgmt GnomePilotConduitMgmt;
struct _GnomePilotConduitMgmt {
	gchar *name;
	gchar *library_file;
#ifdef USE_GMODULE
	GModule *dlhandle;
#else
	void *dlhandle;
#endif
	conduit_get_gpilot_conduit get_gpilot_conduit;
	conduit_destroy_gpilot_conduit destroy_gpilot_conduit;
};

#define GPILOT_CONDUIT_MGMT(s) ((GnomePilotConduitMgmt*)(s))

  /* allocate structure for $(libdir)/gnome_pilot/conduits/libNAME.so */
GnomePilotConduitMgmt *gpilotd_conduit_mgmt_new(gchar *name, gint pilotID);

  /* allocate structure for name which is an absolute file name */
GnomePilotConduitMgmt *gpilotd_conduit_mgmt_new_absolute(gchar *name, gint pilotID);

  /* deallocate structure */
void gpilotd_conduit_mgmt_free(GnomePilotConduitMgmt *);

  /* enable the conduit for a specific pilot, and set the conduits synctype */
void gpilotd_conduit_mgmt_enable(GnomePilotConduitMgmt *conduit,
				 gint pilot,
				 GnomePilotConduitSyncType synctype);

/* enable the conduit for a specific pilot, and set the conduits
   synctype and 1st sync synctype. Slow is only valid for 
   firstsynctype = GnomePilotConduitSyncTypeSynchronize */
void gpilotd_conduit_mgmt_enable_with_first_sync(GnomePilotConduitMgmt *conduit,
						 gint pilot,
						 GnomePilotConduitSyncType synctype,
						 GnomePilotConduitSyncType firstsynctype,
						 gboolean slow);
	
  /* disable the conduit for a specific pilot */
void gpilotd_conduit_mgmt_disable(GnomePilotConduitMgmt *conduit,
				  gint pilot);

  /* query the conduit "enabledness" for a specific pilot */
gboolean gpilotd_conduit_mgmt_is_enabled(GnomePilotConduitMgmt *conduit,
					 gint pilot);

  /* query the conduit "enabledness" for a specific pilot and get the synctype*/
gboolean gpilotd_conduit_mgmt_get_sync_type(GnomePilotConduitMgmt *conduit,
					    gint pilot,
					    GnomePilotConduitSyncType *synctype);

  /* make a gnome_config_push that is prefixed with the gnome-pilot dir */
void gpilotd_conduit_mgmt_config_push(GnomePilotConduitMgmt *conduit);

  /* deallocs structures etc */
void gpilotd_conduit_mgmt_drop_all(void);
  
/* Removes settings the alter the first upcomming syncmethod of the conduit
   given */
void gpilotd_conduit_mgmt_remove_first_sync(GnomePilotConduitMgmt *conduit,
					    gint pilot);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GPILOTD_CONDUIT_MGMT_H */
