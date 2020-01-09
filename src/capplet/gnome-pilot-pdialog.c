/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-pilot-pdialog.c
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

#include <sys/stat.h>
#include <glade/glade.h>
#include <pi-util.h>
#include "pilot.h"
#include "util.h"
#include "gnome-pilot-pdialog.h"


static GtkObjectClass *parent_class = NULL;

struct _GnomePilotPDialogPrivate 
{
	GladeXML *xml;

	GnomePilotClient *gpc;
	gint handle1, handle2;

	PilotState *state;
	GPilotPilot *pilot;

	GtkWidget *dialog;
	
	GtkWidget *pilot_username;
	GtkWidget *pilot_id;
	GtkWidget *pilot_get;
	GtkWidget *pilot_send;

	GtkWidget *pilot_name;
	GtkWidget *pilot_basedir;
#ifdef PILOT_LINK_0_12
	GtkWidget *pilot_charset;
#endif
	GtkWidget *pilot_charset_label;
	GtkWidget *pilot_charset_combo;

	GtkWidget *sync_dialog;
};

static void class_init (GnomePilotPDialogClass *klass);
static void init (GnomePilotPDialog *gppd);

static gboolean get_widgets (GnomePilotPDialog *gppd);
static void map_widgets (GnomePilotPDialog *gppd);
static void init_widgets (GnomePilotPDialog *gppd);
static void fill_widgets (GnomePilotPDialog *gppd);

static void gppd_pilot_get (GtkWidget *widget, gpointer user_data);
static void gppd_pilot_send (GtkWidget *widget, gpointer user_data);

static void gppd_request_completed (GnomePilotClient* client, 
				    const gchar *id, 
				    unsigned long handle, 
				    gpointer user_data);
static void gppd_userinfo_requested (GnomePilotClient *gpc, 
				     const gchar *device, 
				     const GNOME_Pilot_UserInfo *user, 
				     gpointer user_data);
static void gppd_system_info_requested (GnomePilotClient *gpc,
					const gchar *device,
					const GNOME_Pilot_SysInfo *sysinfo,
					gpointer user_data);

static void gppd_destroy (GtkObject *object);

GtkType
gnome_pilot_pdialog_get_type (void)
{
  static GtkType type = 0;

  if (type == 0)
    {
      static const GtkTypeInfo info =
      {
        "GnomePilotPDialog",
        sizeof (GnomePilotPDialog),
        sizeof (GnomePilotPDialogClass),
        (GtkClassInitFunc) class_init,
        (GtkObjectInitFunc) init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      type = gtk_type_unique (gtk_object_get_type (), &info);
    }

  return type;
}

static void
class_init (GnomePilotPDialogClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class->destroy = gppd_destroy;
}

static void
init (GnomePilotPDialog *gppd)
{
	GnomePilotPDialogPrivate *priv;
	
	priv = g_new0 (GnomePilotPDialogPrivate, 1);

	gppd->priv = priv;

	/* Gui stuff */
	priv->xml = glade_xml_new ("gpilotd-capplet.glade", "PilotSettings", NULL);
	if (!priv->xml) {
		priv->xml = glade_xml_new (GLADEDATADIR "/gpilotd-capplet.glade", "PilotSettings", NULL);
		if (!priv->xml) {
			g_message ("gnome-pilot-pdialog init(): Could not load the Glade XML file!");
			goto error;
		}
	}

	if (!get_widgets (gppd)) {
		g_message ("gnome-pilot-pdialog init(): Could not find all widgets in the XML file!");
		goto error;
	}

 error:
	;
}



GtkObject *
gnome_pilot_pdialog_new (GnomePilotClient *gpc, PilotState *state, GPilotPilot *pilot)
{
	GnomePilotPDialog *gppd;
	GtkObject *object;
	
	object = gtk_type_new (GNOME_PILOT_TYPE_PDIALOG);
	
	gppd = GNOME_PILOT_PDIALOG (object);
	gppd->priv->gpc = gpc;
	gppd->priv->state = state;
	gppd->priv->pilot = pilot;

	map_widgets (gppd);
	fill_widgets (gppd);
	init_widgets (gppd);

	gnome_pilot_client_connect__completed_request (gpc, gppd_request_completed, 
						       gppd);
	gnome_pilot_client_connect__user_info (gpc, gppd_userinfo_requested, 
					       gppd);
	gnome_pilot_client_connect__system_info (gpc, gppd_system_info_requested, 
						 gppd);
	
	return object;
}

void
gnome_pilot_pdialog_set_pilot (GtkObject *obj, GPilotPilot *pilot)
{
	GnomePilotPDialog *gppd = GNOME_PILOT_PDIALOG (obj);
	
	gppd->priv->pilot = pilot;
	fill_widgets (gppd);
}


static gboolean
get_widgets (GnomePilotPDialog *gppd)
{
	GnomePilotPDialogPrivate *priv;

	priv = gppd->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->dialog = GW ("PilotSettings");

	priv->pilot_username = GW ("pilot_username_entry");
	priv->pilot_id = GW ("pilot_id_entry");
	priv->pilot_get = GW ("get_from_pilot_button");
	priv->pilot_send = GW ("send_to_pilot_button");
	
	priv->pilot_name = GW ("pilot_name_entry");
	priv->pilot_basedir = GW ("pilot_basedir_entry");
	priv->pilot_charset_label = GW ("pilot_charset_label");
	priv->pilot_charset_combo = GW ("pilot_charset_combo");
#ifdef PILOT_LINK_0_12
	priv->pilot_charset = GW ("pilot_charset_entry");
#endif
	
#undef GW
	return (priv->dialog
		&& priv->pilot_username
		&& priv->pilot_id
		&& priv->pilot_get
		&& priv->pilot_send
		&& priv->pilot_name
		&& priv->pilot_basedir
#ifdef PILOT_LINK_0_12
		&& priv->pilot_charset
#endif
		&& priv->pilot_charset_label
		&& priv->pilot_charset_combo);
}

static void
map_widgets (GnomePilotPDialog *gppd)
{
	GnomePilotPDialogPrivate *priv;
	
	priv = gppd->priv;

	gtk_object_set_data (GTK_OBJECT (gppd), "username", priv->pilot_username);
	gtk_object_set_data (GTK_OBJECT (gppd), "pilotid", priv->pilot_id);
	gtk_object_set_data (GTK_OBJECT (gppd), "pilotname", priv->pilot_name);
	gtk_object_set_data (GTK_OBJECT (gppd), "basedir", priv->pilot_basedir);
#ifdef PILOT_LINK_0_12
	gtk_object_set_data (GTK_OBJECT (gppd), "charset", priv->pilot_charset);
#endif
}

static void 
init_widgets (GnomePilotPDialog *gppd)
{
	GnomePilotPDialogPrivate *priv;

	priv = gppd->priv;

	/* Button signals */
	gtk_signal_connect (GTK_OBJECT (priv->pilot_get), "clicked",
			    GTK_SIGNAL_FUNC (gppd_pilot_get), gppd);

	gtk_signal_connect (GTK_OBJECT (priv->pilot_send), "clicked",
			    GTK_SIGNAL_FUNC (gppd_pilot_send), gppd);
	
	/* Other widget signals */
	gtk_signal_connect (GTK_OBJECT (priv->pilot_username),"insert-text",
			    GTK_SIGNAL_FUNC (insert_username_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (priv->pilot_id),"insert-text",
			    GTK_SIGNAL_FUNC (insert_numeric_callback), NULL);
}

static void
fill_widgets (GnomePilotPDialog *gppd)
{
	GnomePilotPDialogPrivate *priv;
	char buf[256];
	
	priv = gppd->priv;

	if (priv->pilot) {
		gtk_entry_set_text (GTK_ENTRY (priv->pilot_username), priv->pilot->pilot_username);

		g_snprintf (buf, sizeof (buf), "%d", priv->pilot->pilot_id);
		gtk_entry_set_text (GTK_ENTRY (priv->pilot_id), buf);

		gtk_entry_set_text (GTK_ENTRY (priv->pilot_name), priv->pilot->name);
		gtk_entry_set_text (GTK_ENTRY (priv->pilot_basedir), priv->pilot->sync_options.basedir);
#ifndef PILOT_LINK_0_12
		gtk_widget_set_sensitive (priv->pilot_charset_label, FALSE);
		gtk_widget_set_sensitive (priv->pilot_charset_combo, FALSE);
#else
		gtk_entry_set_text (GTK_ENTRY (priv->pilot_charset), priv->pilot->pilot_charset);
#endif
	}
}

gboolean
gnome_pilot_pdialog_run_and_close (GnomePilotPDialog *gppd, GtkWindow *parent)
{
	GnomePilotPDialogPrivate *priv;
	gint btn;
	
	priv = gppd->priv;
	
	gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), parent);
	while(1) {
		btn = gtk_dialog_run (GTK_DIALOG (priv->dialog));
	
		if (btn == GTK_RESPONSE_OK) {
			if(
#ifdef PILOT_LINK_0_12
			    check_pilot_charset(gtk_entry_get_text(
				GTK_ENTRY(priv->pilot_charset))) &&
#endif
			    check_base_directory(gtk_entry_get_text(
				GTK_ENTRY(priv->pilot_basedir)))) {
				read_pilot_config (GTK_OBJECT (gppd),
				    priv->pilot);
				break;
			}
		} else {
			break;
		}
	}

	gtk_widget_hide (priv->dialog);

	return btn == GTK_RESPONSE_OK ? TRUE : FALSE;
}

static void 
gppd_request_completed (GnomePilotClient* client, 
			const gchar *id, 
			unsigned long handle, 
			gpointer user_data) 
{
	GnomePilotPDialog *gppd = GNOME_PILOT_PDIALOG (user_data);
	GnomePilotPDialogPrivate *priv;
	
	priv = gppd->priv;

	if (handle == priv->handle1)
		priv->handle1 = -1;
	else if (handle == priv->handle2)
		priv->handle2 = -1;
	else
		return;

	if (priv->handle1 == -1 && priv->handle2 == -1) {
		gtk_dialog_response (GTK_DIALOG (priv->sync_dialog), 
		    GTK_RESPONSE_OK);
	}
}

static void 
gppd_userinfo_requested (GnomePilotClient *gpc, 
			 const gchar *device, 
			 const GNOME_Pilot_UserInfo *user, 
			 gpointer user_data) 
{
	GnomePilotPDialog *gppd = GNOME_PILOT_PDIALOG (user_data);
	GnomePilotPDialogPrivate *priv;
	gchar buf[20];
	
	priv = gppd->priv;
	
	priv->pilot->pilot_id = user->userID;

	if (priv->pilot->pilot_username) 
		g_free (priv->pilot->pilot_username);
#ifndef PILOT_LINK_0_12
	if (!user->username || (convert_FromPilotChar ("UTF-8", user->username, strlen (user->username), &priv->pilot->pilot_username ) == -1)) {
#else
	if (!user->username || (convert_FromPilotChar_WithCharset ("UTF-8", user->username, strlen (user->username), &priv->pilot->pilot_username, NULL ) == -1)) {
#endif
  		priv->pilot->pilot_username = g_strdup (user->username);
	}

	gtk_entry_set_text (GTK_ENTRY (priv->pilot_username), priv->pilot->pilot_username);
	g_snprintf (buf, sizeof (buf), "%d", priv->pilot->pilot_id);
	gtk_entry_set_text (GTK_ENTRY (priv->pilot_id), buf);
}

static void 
gppd_system_info_requested (GnomePilotClient *gpc,
			    const gchar *device,
			    const GNOME_Pilot_SysInfo *sysinfo,
			    gpointer user_data) 
{
	GnomePilotPDialog *gppd = GNOME_PILOT_PDIALOG (user_data);
	GnomePilotPDialogPrivate *priv;
	
	priv = gppd->priv;
	
	priv->pilot->creation = sysinfo->creation;
	priv->pilot->romversion = sysinfo->romVersion;
}


static void 
gppd_cancel_sync (GtkWidget *widget, gpointer user_data)
{
	GnomePilotPDialog *gppd = GNOME_PILOT_PDIALOG (user_data);
	GnomePilotPDialogPrivate *priv;
	
	priv = gppd->priv;

	gnome_pilot_client_remove_request (priv->gpc, priv->handle1);
	gnome_pilot_client_remove_request (priv->gpc, priv->handle2);

	priv->handle1 = -1;
	priv->handle2 = -1;
}

static void
gppd_sync_dialog (GnomePilotPDialog *gppd, 
		  GPilotDevice* device) 
{
	GnomePilotPDialogPrivate *priv;
	gchar *location;
	gint btn;

	priv = gppd->priv;

	location = device->type == PILOT_DEVICE_NETWORK ? "netsync" : device->port;
	priv->sync_dialog = gtk_message_dialog_new (GTK_WINDOW(priv->dialog), 
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_CANCEL,
                                                   _("Please put PDA in %s (%s) and press "
                                                     "HotSync button or cancel the operation."),
	    					   device->name, location);

	btn = gtk_dialog_run (GTK_DIALOG (priv->sync_dialog));
	if (GTK_RESPONSE_CANCEL == btn) {
		gppd_cancel_sync(priv->sync_dialog, gppd);
	}
	gtk_widget_destroy(priv->sync_dialog);
	priv->sync_dialog = NULL;
}

static void 
gppd_pilot_get (GtkWidget *widget, gpointer user_data)
{
	GnomePilotPDialog *gppd = GNOME_PILOT_PDIALOG (user_data);
	GnomePilotPDialogPrivate *priv;
	GPilotDevice *dev;
	
	priv = gppd->priv;

	dev = choose_pilot_dialog (priv->state);
	if (dev != NULL) {
 		if (gnome_pilot_client_get_user_info (priv->gpc, 
						      dev->name, 
						      GNOME_Pilot_IMMEDIATE, 
						      0, 
						      &priv->handle1)== GPILOTD_OK &&
		    gnome_pilot_client_get_system_info (priv->gpc,
							dev->name,
							GNOME_Pilot_IMMEDIATE,
							0,
							&priv->handle2) == GPILOTD_OK) {
			gppd_sync_dialog (gppd, dev);
		} else {
			error_dialog (GTK_WINDOW (priv->dialog), _("The request to get PDA ID failed"));
		}
	}
}

static void 
gppd_pilot_send (GtkWidget *widget, gpointer user_data)
{
	GnomePilotPDialog *gppd = GNOME_PILOT_PDIALOG (user_data);
	GnomePilotPDialogPrivate *priv;
	GNOME_Pilot_UserInfo user;
	GPilotPilot *pilot;
	GPilotDevice *dev;

	priv = gppd->priv;

	dev = choose_pilot_dialog (priv->state);
	if (dev != NULL){
		pilot = g_new0 (GPilotPilot, 1);
		
		read_pilot_config (GTK_OBJECT (gppd), pilot);

		user.userID = pilot->pilot_id;
#ifndef PILOT_LINK_0_12
		if (! pilot->pilot_username || (convert_ToPilotChar ("UTF-8", pilot->pilot_username,strlen(pilot->pilot_username),&user.username) == -1)) {
#else
		if (! pilot->pilot_username || (convert_ToPilotChar_WithCharset ("UTF-8", pilot->pilot_username, strlen(pilot->pilot_username), &user.username, NULL) == -1)) {
#endif
			user.username = g_strdup (pilot->pilot_username);
		}
		if (gnome_pilot_client_set_user_info (priv->gpc, 
						      dev->name, 
						      user, 
						      FALSE, 
						      GNOME_Pilot_IMMEDIATE, 
						      0, 
						      &priv->handle1)== GPILOTD_OK &&
		    gnome_pilot_client_get_system_info (priv->gpc,
							dev->name,
							GNOME_Pilot_IMMEDIATE,
							0,
							&priv->handle2) == GPILOTD_OK) {
			gppd_sync_dialog (gppd, dev);
		} else {
			error_dialog (GTK_WINDOW (priv->dialog), _("The request to set PDA ID failed"));
		}
		gpilot_pilot_free (pilot);	
	}
}

static void
gppd_destroy (GtkObject *object)
{
	GnomePilotPDialog *gppd = GNOME_PILOT_PDIALOG (object);
	GnomePilotPDialogPrivate *priv;
	
	priv = gppd->priv;

	gtk_widget_destroy (priv->dialog);
	gtk_object_unref (GTK_OBJECT (priv->xml));

	gtk_signal_disconnect_by_data (GTK_OBJECT (priv->gpc), object);
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}
