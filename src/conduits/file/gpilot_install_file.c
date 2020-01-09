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
 */

#include "config.h"
#include <gnome.h>
#include <gpilotd/gnome-pilot-client.h>

#include <bonobo.h>
#include <bonobo-activation/bonobo-activation.h>



GnomePilotClient *gpc;
CORBA_Environment ev;
GSList *handles;
GSList *failed,*notfailed;
int handle;
GtkWidget *dialog;


int now = 0, later = 0;
char *debug_modules = NULL;
char *pilot_arg=NULL;

static const struct poptOption options[] = {
	{"now", 'n', POPT_ARG_NONE, &now, 0, N_("Install immediately"), NULL},
	{"later", 'l', POPT_ARG_NONE, &later, 0, N_("Install delayed"), NULL},
	{"pilot", 'p', POPT_ARG_STRING, &pilot_arg, 0, N_("PDA to install to"), N_("PDA")},
	{NULL, '\0', 0, NULL, 0} /* end the list */
};

static void 
gpilotd_request_completed (GnomePilotClient *gpc, gchar *pilot_id, gint handle, gpointer data)
{
	g_message ("%s completed %d", pilot_id, handle);
	handles = g_slist_remove (handles,GINT_TO_POINTER(handle));
	if (handles == NULL)
		gnome_dialog_close (GNOME_DIALOG (dialog));
}

static void 
cancel_install (GnomeDialog *w, GSList *list) 
{
	GSList *e;

	for (e=list;e;e = g_slist_next (e)) {
		gnome_pilot_client_remove_request (gpc,GPOINTER_TO_INT(e->data));  
	}
	g_slist_free (list);

}

static void 
show_warning_dialog (gchar *mesg,...) 
{
	char *tmp;
	va_list ap;
	va_start (ap, mesg);

	tmp = g_strdup_vprintf (mesg, ap);
	dialog = gnome_message_box_new (tmp, GNOME_MESSAGE_BOX_WARNING,
				       GNOME_STOCK_BUTTON_OK, NULL);
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	g_free (tmp);
	va_end (ap);
}

int 
main (int argc, char *argv[]) 
{
	int err, i;
	GNOME_Pilot_Survival survive;
        poptContext pctx;
	GList *pilots = NULL;
        const char **args;
        
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	if (argc<2) {
		g_message ("usage : %s [--now|--later] [FILE]", argv[0]);
		exit (1);
	}

	gnome_init_with_popt_table ("gpilotd-install-file", "1.0",
				    argc, argv, options, 0, &pctx);
	
	gpc = GNOME_PILOT_CLIENT (gnome_pilot_client_new ());
	gtk_object_ref (GTK_OBJECT (gpc));
	gtk_object_sink (GTK_OBJECT (gpc));
	gtk_signal_connect (GTK_OBJECT (gpc),"completed_request", (GtkSignalFunc)gpilotd_request_completed, NULL);
	gnome_pilot_client_connect_to_daemon (gpc);

	if (pilot_arg!=NULL) {
		pilots = g_list_append (pilots, g_strdup (pilot_arg));
	} else {
		err = gnome_pilot_client_get_pilots (gpc, &pilots);
		if (err !=GPILOTD_OK || pilots == NULL) {
			g_warning (_("Unable to get PDA names"));
			show_warning_dialog (_("Unable to get PDA names"));
			exit (1);
		}
	}

	notfailed = failed = handles = NULL;

	survive = GNOME_Pilot_IMMEDIATE;
	if (later) survive = GNOME_Pilot_PERSISTENT;
	
	i=0;

        args = poptGetArgs (pctx);

	while (args && args[i]!=NULL) {
		gint err;
		err = gnome_pilot_client_install_file (gpc,
						       pilots->data, /* get first pilot */
						       args[i],
						       survive,
						       0,
						       &handle);
		if (err == GPILOTD_OK) {
			handles = g_slist_prepend (handles,GINT_TO_POINTER(handle));
			notfailed = g_slist_prepend (notfailed, (void *) args[i]);
		} else {
			failed = g_slist_prepend (failed, (void *) args[i]);
		}
		i++;
	}

        poptFreeContext (pctx);

	if (!later) {
		gchar *message;
		
		message = NULL;
		if (failed != NULL) {
			GSList *e;
			message = g_strdup (_("Following files failed :\n"));
			for (e=failed;e;e = g_slist_next (e)) {
				gchar *tmp;
				tmp = g_strconcat (message,"\t- ", e->data,"\n", NULL);
				g_free (message);
				message = tmp;
			}
			g_slist_free (failed);
		}
		{
			GSList *e;
			if (message == NULL)
				message = g_strdup_printf (_("Installing to %s:\n"), (char*)pilots->data);
			else {
				gchar *tmp;
				tmp = g_strconcat (message,"\nInstalling to ", 
						   (char*)pilots->data, ":\n", NULL);
				g_free (message);
				message = tmp;
			}
			for (e=notfailed;e;e = g_slist_next (e)) {
				gchar *tmp;
				tmp = g_strconcat (message,"\t- ", e->data,"\n", NULL);
				g_free (message);
				message = tmp;
			}
			g_slist_free (notfailed);
		}
		{
			gchar *tmp;
			gchar *info;

			if (handles == NULL) 
				info = g_strdup (_("No files to install"));
			else {
				
				info = g_strdup (_("Press synchronize on the cradle to install\n" 
						  " or cancel the operation."));
				GNOME_Pilot_Daemon_request_conduit (gpc->gpilotddaemon,
								    gpc->gpilotdclient,
								    pilots->data,
								    "File",
								    GNOME_Pilot_CONDUIT_DEFAULT,
								    survive,
								    0,
								    &ev);
			}
						
			tmp = g_strconcat (message==NULL?"":message,
					  "\n",
					  info,
					  NULL);
			g_free (message);
			g_free (info);
			message = tmp;
		}
		dialog = gnome_message_box_new (message,
					       GNOME_MESSAGE_BOX_GENERIC,
					       GNOME_STOCK_BUTTON_CANCEL,
					       NULL);
		gnome_dialog_button_connect (GNOME_DIALOG (dialog),0,
					    G_CALLBACK(cancel_install), handles);
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		g_free (message);
	}

	gtk_object_unref (GTK_OBJECT (gpc));

	return 0;
}

