/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-pilot-cdialog.c
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
#include "pilot.h"
#include "util.h"
#include "gnome-pilot-cdialog.h"


static GtkObjectClass *parent_class = NULL;

struct _GnomePilotCDialogPrivate 
{
	GladeXML *xml;

	ConduitState *state;

	GtkWidget *dialog;

	GtkWidget *settings_frame;
	GtkWidget *sync_actions;
	GtkWidget *sync_one_actions;
	
	GtkWidget *options_frame;;
};

static void class_init (GnomePilotCDialogClass *klass);
static void init (GnomePilotCDialog *gpcd);

static gboolean get_widgets (GnomePilotCDialog *gpcd);
static void init_widgets (GnomePilotCDialog *gpcd);
static void fill_widgets (GnomePilotCDialog *gpcd);

static void gpcd_action_activated (GtkWidget *widget, gpointer user_data);

static void gpcd_destroy (GtkObject *object);

GtkType
gnome_pilot_cdialog_get_type (void)
{
  static GtkType type = 0;

  if (type == 0)
    {
      static const GtkTypeInfo info =
      {
        "GnomePilotCDialog",
        sizeof (GnomePilotCDialog),
        sizeof (GnomePilotCDialogClass),
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
class_init (GnomePilotCDialogClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class->destroy = gpcd_destroy;
}

static void
init (GnomePilotCDialog *gpcd)
{
	GnomePilotCDialogPrivate *priv;
	
	priv = g_new0 (GnomePilotCDialogPrivate, 1);

	gpcd->priv = priv;

	/* Gui stuff */
	priv->xml = glade_xml_new ("gpilotd-capplet.glade", "ConduitSettings", NULL);
	if (!priv->xml) {
		priv->xml = glade_xml_new (GLADEDATADIR "/gpilotd-capplet.glade", "ConduitSettings", NULL);
		if (!priv->xml) {
			g_message ("gnome-pilot-cdialog init(): Could not load the Glade XML file!");
			goto error;
		}
	}

	if (!get_widgets (gpcd)) {
		g_message ("gnome-pilot-cdialog init(): Could not find all widgets in the XML file!");
		goto error;
	}
	
 error:
	;
}



GtkObject *
gnome_pilot_cdialog_new (ConduitState *state)
{
	GnomePilotCDialog *gpcd;
	GtkObject *object;
	
	object = gtk_type_new (GNOME_PILOT_TYPE_CDIALOG);
	
	gpcd = GNOME_PILOT_CDIALOG (object);
	gpcd->priv->state = state;

	fill_widgets (gpcd);
	init_widgets (gpcd);
	
	return object;
}

static gboolean
get_widgets (GnomePilotCDialog *gpcd)
{
	GnomePilotCDialogPrivate *priv;

	priv = gpcd->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->dialog = GW ("ConduitSettings");

	priv->settings_frame = GW ("settings_frame");
	priv->sync_actions = GW ("sync_actions_menu");
	priv->sync_one_actions = GW ("sync_one_actions_menu");
	
	priv->options_frame = GW ("options_frame");

#undef GW
	return (priv->dialog
		&& priv->settings_frame
		&& priv->sync_actions
		&& priv->options_frame);
}

static void 
init_widgets (GnomePilotCDialog *gpcd)
{
	GnomePilotCDialogPrivate *priv;
	GtkWidget *menu;
	GList *l;
	
	priv = gpcd->priv;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->sync_actions));
	l = GTK_MENU_SHELL (menu)->children;
	while (l != NULL) {
		GtkWidget *menu_item = GTK_WIDGET (l->data);
		
		gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
				    GTK_SIGNAL_FUNC (gpcd_action_activated), gpcd);
		
		l = l->next;
	}
}

static void
fill_widgets (GnomePilotCDialog *gpcd)
{
	GnomePilotCDialogPrivate *priv;
	
	priv = gpcd->priv;

	if (priv->state) {
		fill_conduit_sync_type_menu (GTK_OPTION_MENU (priv->sync_actions), priv->state);
		if (priv->state->default_sync_type != GnomePilotConduitSyncTypeCustom)
			fill_conduit_first_sync_type_menu (GTK_OPTION_MENU (priv->sync_one_actions), priv->state);

		if (!priv->state->has_settings) {
			gtk_widget_hide (priv->settings_frame);

		} else if (gnome_pilot_conduit_create_settings_window (priv->state->conduit, priv->options_frame) == 500) { /* < 0) { */
			gchar *msg = _("Unable to create PDA settings window. Incorrect conduit configuration.");
			error_dialog (GTK_WINDOW (priv->dialog), msg);
			
			/* Self healing. Will not try again for this run of the capplet */
			gnome_pilot_conduit_management_destroy_conduit (priv->state->management, &priv->state->conduit);
			priv->state->settings_widget = NULL;
			priv->state->has_settings = FALSE;
			priv->state->conduit = NULL;
			gtk_widget_hide (priv->settings_frame);
		}
	}
}

GnomePilotConduitSyncType 
gnome_pilot_cdialog_sync_type (GnomePilotCDialog *gpcd)
{
	GnomePilotCDialogPrivate *priv;
	GtkWidget *menu, *menu_item;
	
	priv = gpcd->priv;
	
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->sync_actions));
	menu_item = gtk_menu_get_active (GTK_MENU (menu));
	
	return GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (menu_item), "sync_type"));
}

GnomePilotConduitSyncType 
gnome_pilot_cdialog_first_sync_type (GnomePilotCDialog *gpcd)
{
	GnomePilotCDialogPrivate *priv;
	GtkWidget *menu, *menu_item;
	
	priv = gpcd->priv;
	
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->sync_one_actions));
	menu_item = gtk_menu_get_active (GTK_MENU (menu));
	
	return GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (menu_item), "sync_type"));
}

gboolean
gnome_pilot_cdialog_run_and_close (GnomePilotCDialog *gpcd, GtkWindow *parent)
{
	GnomePilotCDialogPrivate *priv;
	gint btn;
	
	priv = gpcd->priv;
	
	gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), parent);
	btn = gtk_dialog_run (GTK_DIALOG (priv->dialog));
	gtk_widget_hide(priv->dialog);

	return GTK_RESPONSE_OK == btn ? TRUE : FALSE;
}

static void 
gpcd_action_activated (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCDialog *gpcd = GNOME_PILOT_CDIALOG (user_data);
	GnomePilotCDialogPrivate *priv;
	gboolean disable;
	
	priv = gpcd->priv;
	
	disable = (gnome_pilot_cdialog_sync_type (gpcd) == GnomePilotConduitSyncTypeNotSet);
	
	gtk_widget_set_sensitive (priv->sync_one_actions, !disable);
	gtk_widget_set_sensitive (priv->options_frame, !disable);
}

static void
gpcd_destroy (GtkObject *object)
{
	GnomePilotCDialog *gpcd = GNOME_PILOT_CDIALOG (object);
	GnomePilotCDialogPrivate *priv;
	
	priv = gpcd->priv;

	gtk_object_unref (GTK_OBJECT (priv->xml));

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}
