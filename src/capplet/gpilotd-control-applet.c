/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gpilotd-control-applet.c
 *
 * Copyright (C) 1998 Red Hat Software       
 * Copyright (C) 1999-2000 Free Software Foundation
 * Copyright (C) 2001  Ximian, Inc.
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
 *          Vadim Strizhevsky
 *          Michael Fulbright <msf@redhat.com>
 *          JP Rosevear <jpr@ximian.com>
 *
 */

#include <config.h>
#include <ctype.h>
#include <gnome.h>
#include <glade/glade.h>

#include <libgnomeui/gnome-window-icon.h>
#include <gpilotd/gnome-pilot-client.h>

#include "gnome-pilot-druid.h"
#include "gnome-pilot-capplet.h"

#include <bonobo-activation/bonobo-activation.h>

#include "pilot.h"
#include "util.h"

static void
monitor_pilots (GnomePilotClient *gpc, PilotState * state)
{
	GList *tmp = state->pilots;
	if (tmp!= NULL){
		while (tmp!= NULL){
			GPilotPilot *pilot =(GPilotPilot*)tmp->data;
			g_message ("pilot = %s",pilot->name);
			gnome_pilot_client_monitor_on (gpc,pilot->name);
			tmp = tmp->next;
		}
		gnome_pilot_client_notify_on (gpc, GNOME_Pilot_NOTIFY_CONNECT);
		gnome_pilot_client_notify_on (gpc, GNOME_Pilot_NOTIFY_DISCONNECT);
	}
}

static void
response_cb (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP) {
		gnome_help_display_desktop (NULL, "gnome-pilot", "gnome-pilot.xml", "conftool-pilots", NULL);
	} else {
		gtk_main_quit ();
	}
}

int
main (int argc, char *argv[])
{
	GnomePilotClient *gpc = NULL;
	PilotState *state = NULL;
	GnomePilotCapplet *gpcap = NULL;
	gboolean druid_on = FALSE, druid_prog = FALSE;

	struct poptOption options[] = {
		{"druid", '\0', POPT_ARG_NONE, &druid_on, 0, N_("Start druid only"), NULL},
		
		{NULL, '\0', 0, NULL, 0, NULL, NULL}
	};

	
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	
	if (argc > 1 && strcmp (argv[1], "--druid") == 0) {
		gnome_init_with_popt_table ("gpilot-control-applet",
					    VERSION, argc, argv,
					    options,0,NULL);

		druid_on = TRUE;
		druid_prog = TRUE;
	} else {
		/* we're a capplet (we get CORBA for free from capplet_init) */
		switch (gnome_init_with_popt_table ("gpilotd-control-applet", VERSION, argc, argv, 
						    options,0,NULL)) {
		case 1: return 0; break;
		case -1: g_error (_("Error initializing gpilotd capplet")); break;
		default: break;
		}
	}
	gnome_window_icon_set_default_from_file (GNOME_ICONDIR"/gnome-palm.png");
	
	/* we using glade */
	glade_gnome_init ();
	
	/* put all code to set things up in here */
	if (loadPilotState (&state) < 0) {
		error_dialog (NULL, _("Error loading PDA state, aborting"));
		g_error (_("Error loading PDA state, aborting"));
		return -1;
	}

	gpc = GNOME_PILOT_CLIENT (gnome_pilot_client_new ());

	if (gnome_pilot_client_connect_to_daemon (gpc) != GPILOTD_OK) {
		error_dialog (NULL,_("Cannot connect to the GnomePilot Daemon"));
		g_error (_("Cannot connect to the GnomePilot Daemon"));
		return -1;
	}

	monitor_pilots (gpc, state);
	if (druid_on) {
		GtkObject *druid;
		
		if (state->pilots!= NULL || state->devices!= NULL) {
			error_dialog (NULL, _("Cannot run druid if PDAs or devices already configured"));
			return -1;
		}

		druid = gnome_pilot_druid_new (gpc);
		gnome_pilot_druid_run_and_close (GNOME_PILOT_DRUID (druid));
	} else {
		gboolean druid_finished = TRUE;
		
		gpcap = gnome_pilot_capplet_new (gpc);

		/* quit when the Close button is clicked on our dialog */
		g_signal_connect (G_OBJECT (gpcap), "response", G_CALLBACK (response_cb), NULL);

		/* popup the druid if nothing is configured - assume this is the first time */
		if (state->pilots == NULL && state->devices == NULL) {
			GtkObject *druid;
			
			druid = gnome_pilot_druid_new (gpc);
			druid_finished = gnome_pilot_druid_run_and_close (GNOME_PILOT_DRUID (druid));

			if (gpcap != NULL)
				gnome_pilot_capplet_update (GNOME_PILOT_CAPPLET (gpcap));
		}

		if (druid_finished) {
			gtk_widget_show (GTK_WIDGET (gpcap));
			gtk_main ();
		}		
	}
	freePilotState (state);

	return 0;
}    

