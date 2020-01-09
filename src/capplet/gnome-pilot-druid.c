/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-pilot-druid.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glade/glade.h>
#include "pilot.h"
#include "util.h"
#include "gnome-pilot-druid.h"


static GtkObjectClass *parent_class = NULL;

struct _GnomePilotDruidPrivate 
{
	GladeXML *xml;

	GnomePilotClient *gpc;
	gint handle1;
	gint handle2;

	gboolean finished;
	gboolean started;
	
	PilotState *state;
	PilotState *orig_state;
	GPilotDevice *device;
	GPilotPilot *pilot;

	GtkWidget *druid_window;
	GtkWidget *druid;

	GtkWidget *page_cradle;
	GtkWidget *page_pilot_one;
	GtkWidget *page_sync;
	GtkWidget *page_pilot_two;
	GtkWidget *page_conduits;
	GtkWidget *page_finish;

	GtkWidget *device_name;
	GtkWidget *device_port;
	GtkWidget *device_port_combo;
	GtkWidget *device_port_label;
	GtkWidget *device_speed;
	GtkWidget *device_speed_label;
	GtkWidget *device_timeout;
	GtkWidget *device_usb;
	GtkWidget *device_irda;
	GtkWidget *device_network;
	GtkWidget *device_bluetooth;
#ifdef PILOT_LINK_0_12
	GtkWidget *libusb_label;
	GList *libusb_list;
#endif
	
	GtkWidget *pilot_info;
	GtkWidget *pilot_info_no;
	GtkWidget *pilot_username;
	GtkWidget *pilot_id;

	GtkWidget *sync_label_vbox;
	GtkWidget *sync_label;

	GtkWidget *pilot_name;
	GtkWidget *pilot_basedir;
#ifdef PILOT_LINK_0_12
	GtkWidget *pilot_charset;
#endif
	GtkWidget *pilot_charset_label;
	GtkWidget *pilot_charset_combo;
};

static void class_init (GnomePilotDruidClass *klass);
static void init (GnomePilotDruid *gpd);

static gboolean get_widgets (GnomePilotDruid *gpd);
static void map_widgets (GnomePilotDruid *gpd);
static void init_widgets (GnomePilotDruid *gpd);
static void fill_widgets (GnomePilotDruid *gpd);
static void set_widget_visibility_by_type(GnomePilotDruid *gpd, int type);
static void network_device_toggled_callback (GtkRadioButton *btn,
    void *data);

static gboolean gpd_delete_window (GtkWidget *widget,GdkEvent *event,gpointer user_data);
static void gpd_canceled (GnomeDruid *druid, gpointer user_data);
static void gpd_help (GnomeDruid *druid, gpointer user_data);

static void gpd_cradle_page_prepare (GnomeDruidPage *page, gpointer arg1, gpointer user_data);
static gboolean gpd_cradle_page_next (GnomeDruidPage *page, gpointer arg1, gpointer user_data);

static void gpd_sync_page_prepare (GnomeDruidPage *page, gpointer arg1, gpointer user_data);
static gboolean gpd_sync_page_back (GnomeDruidPage *druidpage, gpointer arg1, gpointer user_data);
static gboolean gpd_pilot_page_two_next (GnomeDruidPage *druidpage, gpointer arg1, gpointer user_data);
static void gpd_finish_page_finished (GnomeDruidPage *druidpage, gpointer arg1, gpointer user_data);

static void gpd_device_info_check (GtkEditable *editable, gpointer user_data);
static void gpd_pilot_info_check (GtkEditable *editable, gpointer user_data);
static void gpd_pilot_info_button (GtkToggleButton *toggle, gpointer user_data);

static void gpd_request_completed (GnomePilotClient* client, const gchar *id, gint handle, gpointer user_data);
static void gpd_userinfo_requested (GnomePilotClient *gpc, const gchar *device, const GNOME_Pilot_UserInfo *user, gpointer user_data);
static void gpd_system_info_requested (GnomePilotClient *gpc,
 const gchar *device, const GNOME_Pilot_SysInfo *sysinfo, gpointer user_data);

static void gpd_destroy (GtkObject *object);

GtkType
gnome_pilot_druid_get_type (void)
{
  static GtkType type = 0;

  if (type == 0)
    {
      static const GtkTypeInfo info =
      {
        "GnomePilotDruid",
        sizeof (GnomePilotDruid),
        sizeof (GnomePilotDruidClass),
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
class_init (GnomePilotDruidClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class->destroy = gpd_destroy;
}

static void
init (GnomePilotDruid *gpd)
{
	GnomePilotDruidPrivate *priv;
	
	priv = g_new0 (GnomePilotDruidPrivate, 1);

	gpd->priv = priv;

	priv->finished = FALSE;
	priv->started = FALSE;
	
	/* State information */
	loadPilotState (&priv->orig_state);
	priv->state = dupPilotState (priv->orig_state);
	priv->pilot = g_new0 (GPilotPilot, 1);
	priv->device = g_new0 (GPilotDevice,1);

	/* Gui stuff */
	priv->xml = glade_xml_new ("gpilotd-capplet.glade", NULL, NULL);
	if (!priv->xml) {
		priv->xml = glade_xml_new (GLADEDATADIR "/gpilotd-capplet.glade", NULL, NULL);
		if (!priv->xml) {
			g_message ("gnome-pilot-druid init(): Could not load the Glade XML file!");
			goto error;
		}
	}

	if (!get_widgets (gpd)) {
		g_message ("gnome-pilot-druid init(): Could not find all widgets in the XML file!");
		goto error;
	}

	map_widgets (gpd);
	fill_widgets (gpd);
	init_widgets (gpd);

 error:
	;
}



GtkObject *
gnome_pilot_druid_new (GnomePilotClient *gpc)
{
	GnomePilotDruid *gpd;
	GtkObject *obj;
	
	obj = gtk_type_new (GNOME_PILOT_TYPE_DRUID);
	
	gpd = GNOME_PILOT_DRUID (obj);
	gpd->priv->gpc = gpc;

	gtk_signal_connect (GTK_OBJECT (gpc), "completed_request",
			    GTK_SIGNAL_FUNC (gpd_request_completed), gpd);
	gtk_signal_connect (GTK_OBJECT (gpc), "user_info",
			    GTK_SIGNAL_FUNC (gpd_userinfo_requested), gpd);
	gtk_signal_connect (GTK_OBJECT (gpc), "system_info",
			    GTK_SIGNAL_FUNC (gpd_system_info_requested), gpd);

	return obj;
}

static gboolean
get_widgets (GnomePilotDruid *gpd)
{
	GnomePilotDruidPrivate *priv;

	priv = gpd->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->druid_window = GW ("DruidWindow");
	priv->druid = GW ("druid");

	priv->page_cradle = GW ("druidpage_cradle");
	priv->page_pilot_one = GW ("druidpage_pilot1");
	priv->page_sync = GW ("druidpage_sync");
	priv->page_pilot_two = GW ("druidpage_pilot2");
	priv->page_conduits = GW ("druidpage_conduits");
	priv->page_finish = GW ("druidpage_finish");
	
	priv->device_name = GW ("druid_device_name_entry");
	priv->device_port = GW ("druid_device_port_entry");
	priv->device_port_label = GW ("druid_device_port_label");
	priv->device_port_combo = GW ("druid_device_port_combo");
	priv->device_speed = GW ("druid_device_speed_menu");
	priv->device_speed_label = GW ("druid_device_speed_label");
	priv->device_timeout = GW ("druid_device_timeout_spinner");
	priv->device_usb = GW ("druid_usb_radio");
	priv->device_irda = GW ("druid_irda_radio");
	priv->device_network = GW ("druid_network_radio");
	priv->device_bluetooth = GW ("druid_bluetooth_radio");

#ifdef PILOT_LINK_0_12
	/* usb: (libusb) pseudo-device is available from pilot-link 0.12.0 */
	priv->libusb_list = NULL;
	priv->libusb_label = gtk_list_item_new_with_label ("usb:");
	gtk_widget_show(priv->libusb_label);
	priv->libusb_list = g_list_append(priv->libusb_list, priv->libusb_label);
	gtk_list_insert_items (GTK_LIST((GTK_COMBO(priv->device_port_combo))->list),
	priv->libusb_list, 1);
#endif

	priv->pilot_info = GW ("pilot_user_frame");
	priv->pilot_info_no = GW ("no_radio_button");
	priv->pilot_username = GW ("druid_pilot_username_entry");
	priv->pilot_id = GW ("druid_pilot_id_entry");
	
	/* FIXME: is this a libglade bug or what? if sync_label 
	   is constructed in .glade it is not properly redrawn the first time. */
	priv->sync_label_vbox = GW ("sync_label_vbox");
	priv->sync_label =  gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (priv->sync_label_vbox), priv->sync_label, TRUE, FALSE, GNOME_PAD_SMALL);

	priv->pilot_name = GW ("druid_pilot_name_entry");
	priv->pilot_basedir = GW ("druid_pilot_basedir_entry");
#ifdef PILOT_LINK_0_12
	priv->pilot_charset = GW ("druid_pilot_charset_entry");
#endif
	priv->pilot_charset_label = GW ("druid_pilot_charset_label");
	priv->pilot_charset_combo = GW ("druid_pilot_charset_combo");
	
#undef GW

	return (priv->druid_window
		&& priv->druid
		&& priv->page_cradle
		&& priv->page_pilot_one
		&& priv->page_sync
		&& priv->page_pilot_two
		&& priv->page_finish
		&& priv->device_name
		&& priv->device_port
		&& priv->device_speed
		&& priv->device_timeout
		&& priv->device_usb
		&& priv->device_irda
		&& priv->device_network
		&& priv->device_bluetooth
		&& priv->pilot_info
		&& priv->pilot_info_no
		&& priv->pilot_username
		&& priv->pilot_id
		&& priv->sync_label_vbox
		&& priv->sync_label
		&& priv->pilot_name
		&& priv->pilot_basedir
#ifdef PILOT_LINK_0_12
		&& priv->pilot_charset
#endif
		&& priv->pilot_charset_label
		&& priv->pilot_charset_combo);
}

static void
map_widgets (GnomePilotDruid *gpd)
{
	GnomePilotDruidPrivate *priv;
	
	priv = gpd->priv;
	
	gtk_object_set_data (GTK_OBJECT (gpd), "port_entry", priv->device_port);
	gtk_object_set_data (GTK_OBJECT (gpd), "name_entry", priv->device_name);
	gtk_object_set_data (GTK_OBJECT (gpd), "speed_menu", priv->device_speed);
	gtk_object_set_data (GTK_OBJECT (gpd), "timeout_spinner", priv->device_timeout);
	gtk_object_set_data (GTK_OBJECT (gpd), "usb_radio", priv->device_usb);
	gtk_object_set_data (GTK_OBJECT (gpd), "irda_radio", priv->device_irda);
	gtk_object_set_data (GTK_OBJECT (gpd), "network_radio", priv->device_network);
	gtk_object_set_data (GTK_OBJECT (gpd), "bluetooth_radio", priv->device_bluetooth);

	gtk_object_set_data (GTK_OBJECT (gpd), "username", priv->pilot_username);
	gtk_object_set_data (GTK_OBJECT (gpd), "pilotid", priv->pilot_id);
	gtk_object_set_data (GTK_OBJECT (gpd), "pilotname", priv->pilot_name);
	gtk_object_set_data (GTK_OBJECT (gpd), "basedir", priv->pilot_basedir);
#ifdef PILOT_LINK_0_12
	gtk_object_set_data (GTK_OBJECT (gpd), "charset", priv->pilot_charset);
#endif
}

static void 
init_widgets (GnomePilotDruid *gpd)
{
	GnomePilotDruidPrivate *priv;

	priv = gpd->priv;

	/* Main signals */
	gtk_signal_connect (GTK_OBJECT (priv->druid_window), "delete_event",
	    GTK_SIGNAL_FUNC (gpd_delete_window), gpd);

	gtk_signal_connect (GTK_OBJECT (priv->druid), "cancel",
	    GTK_SIGNAL_FUNC (gpd_canceled), gpd);
	gtk_signal_connect (GTK_OBJECT (priv->druid), "help",
			    GTK_SIGNAL_FUNC (gpd_help), gpd);


	/* Page signals */
	g_signal_connect_after (G_OBJECT (priv->page_cradle), "prepare",
				G_CALLBACK (gpd_cradle_page_prepare), gpd);
	g_signal_connect_after (G_OBJECT (priv->page_cradle), "next",
				G_CALLBACK (gpd_cradle_page_next), gpd);

	g_signal_connect_after (G_OBJECT (priv->page_sync), "prepare",
				G_CALLBACK (gpd_sync_page_prepare), gpd);
	g_signal_connect_after (G_OBJECT (priv->page_sync),"back",
				G_CALLBACK (gpd_sync_page_back), gpd);

	g_signal_connect_after (G_OBJECT (priv->page_pilot_two), "next",
				G_CALLBACK (gpd_pilot_page_two_next), gpd);

	g_signal_connect_after (G_OBJECT (priv->page_finish), "finish",
				G_CALLBACK (gpd_finish_page_finished), gpd);

	/* Other widget signals */
	gtk_signal_connect (GTK_OBJECT (priv->device_name),"changed",
			    GTK_SIGNAL_FUNC (gpd_device_info_check), gpd);
	gtk_signal_connect (GTK_OBJECT (priv->device_port),"insert-text",
			    GTK_SIGNAL_FUNC (insert_device_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (priv->device_network), "toggled",
			    GTK_SIGNAL_FUNC (network_device_toggled_callback), gpd);
	gtk_signal_connect (GTK_OBJECT (priv->device_bluetooth), "toggled",
			    GTK_SIGNAL_FUNC (network_device_toggled_callback), gpd);
	gtk_signal_connect (GTK_OBJECT (priv->device_port),"changed",
			    GTK_SIGNAL_FUNC (gpd_device_info_check), gpd);

	gtk_signal_connect (GTK_OBJECT (priv->pilot_info_no),"toggled",
			    GTK_SIGNAL_FUNC (gpd_pilot_info_button), gpd);
	gtk_signal_connect (GTK_OBJECT (priv->pilot_username),"insert-text",
			    GTK_SIGNAL_FUNC (insert_username_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (priv->pilot_username),"changed",
			    GTK_SIGNAL_FUNC (gpd_pilot_info_check), gpd);
	gtk_signal_connect (GTK_OBJECT (priv->pilot_id),"insert-text",
			    GTK_SIGNAL_FUNC (insert_numeric_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (priv->pilot_id),"changed",
			    GTK_SIGNAL_FUNC (gpd_pilot_info_check), gpd);

}

static void
fill_widgets (GnomePilotDruid *gpd)
{
	GnomePilotDruidPrivate *priv;
	gchar buf[256];
	char *str, *str2;
	
	priv = gpd->priv;
	
	/* Cradle page */
	str = next_cradle_name (priv->state);
	gtk_entry_set_text (GTK_ENTRY (priv->device_name), str);
	g_free (str);
	set_widget_visibility_by_type(gpd,
	    (GTK_TOGGLE_BUTTON(priv->device_network)->active ||
		GTK_TOGGLE_BUTTON(priv->device_bluetooth)->active) ?
	    PILOT_DEVICE_NETWORK : PILOT_DEVICE_SERIAL);
	
	fill_speed_menu (GTK_OPTION_MENU (priv->device_speed), 0);

	/* First pilot page */
	gtk_entry_set_text (GTK_ENTRY (priv->pilot_username), g_get_real_name ());
	
	g_snprintf (buf, sizeof (buf), "%d", getuid ());
	gtk_entry_set_text (GTK_ENTRY (priv->pilot_id), buf);

	/* Second pilot page */
	str = next_pilot_name (priv->state);
	gtk_entry_set_text (GTK_ENTRY (priv->pilot_name), str);
	
	str2 = g_concat_dir_and_file (g_get_home_dir (), str);
	gtk_entry_set_text (GTK_ENTRY (priv->pilot_basedir), str2);
#ifndef PILOT_LINK_0_12
	gtk_widget_set_sensitive(priv->pilot_charset_label, FALSE);
	gtk_widget_set_sensitive(priv->pilot_charset_combo, FALSE);
#else
	gtk_entry_set_text (GTK_ENTRY (priv->pilot_charset),
	    get_default_pilot_charset());
#endif

	g_free (str);
	g_free (str2);
}

gboolean
gnome_pilot_druid_run_and_close (GnomePilotDruid *gpd)
{
	GnomePilotDruidPrivate *priv;
	gboolean result;
	
	priv = gpd->priv;
	
	gtk_widget_show_all (priv->druid_window);
	
	gtk_main ();

	result = priv->finished;
	
	gtk_object_destroy (GTK_OBJECT (gpd));

	return result;
}

static gboolean
cancel_dialog (GnomePilotDruid *gpd)
{
	GnomePilotDruidPrivate *priv;
	GtkWidget *dlg;
	
	priv = gpd->priv;

	if (!priv->started)
		return TRUE;

	dlg = gtk_message_dialog_new (GTK_WINDOW (priv->druid_window), GTK_DIALOG_DESTROY_WITH_PARENT, 
				      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
				      _("Setup did not complete and settings will not\n"
					"be saved. Are you sure you want to quit?"));

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_YES) {
		if (priv->handle1 > 0) {
			gnome_pilot_client_remove_request (priv->gpc, priv->handle1);
			priv->handle1 =-1;
		}
		if (priv->handle2 > 0) {
			gnome_pilot_client_remove_request (priv->gpc, priv->handle2);
			priv->handle2 =-1;
		}
		save_config_and_restart (priv->gpc, priv->orig_state);
		freePilotState (priv->state);
		priv->state = dupPilotState (priv->orig_state);
	
		gtk_widget_destroy (dlg);
	
		return TRUE;
	}

	gtk_widget_destroy (dlg);

	return FALSE;
}

static gboolean
check_cradle_settings (GnomePilotDruid *gpd) 
{
	GnomePilotDruidPrivate *priv;
	
	priv = gpd->priv;
	
	return check_editable (GTK_EDITABLE (priv->device_name))
		&& check_editable (GTK_EDITABLE (priv->device_port));
}

static gboolean
check_pilot_settings (GnomePilotDruid *gpd) 
{
	GnomePilotDruidPrivate *priv;
	
	priv = gpd->priv;
	
	return check_editable (GTK_EDITABLE (priv->pilot_username))
		&& check_editable (GTK_EDITABLE (priv->pilot_id));
}

static gboolean
gpd_delete_window (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	return !cancel_dialog (GNOME_PILOT_DRUID (user_data));
}

static void 
gpd_canceled (GnomeDruid *druid, gpointer user_data)
{
	if (cancel_dialog (GNOME_PILOT_DRUID (user_data)))
		gtk_main_quit ();
}

static void 
gpd_help (GnomeDruid *druid, gpointer user_data)
{
	gnome_help_display_desktop (NULL, "gnome-pilot", "gnome-pilot.xml", "assistant", NULL);
}

static void
gpd_cradle_page_prepare (GnomeDruidPage *page, gpointer arg1, gpointer user_data)
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	gboolean ready;

	ready = check_cradle_settings (gpd);
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (arg1), TRUE, ready, TRUE, TRUE);
}

static gboolean
gpd_cradle_page_next (GnomeDruidPage *page, gpointer arg1, gpointer user_data)
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	GPilotDevice *tmp_device;
	gboolean result;

	priv = gpd->priv;

	/* check the device settings */
	tmp_device = gpilot_device_new();
	read_device_config(GTK_OBJECT(gpd), tmp_device);
	result = check_device_settings(tmp_device);
	g_free(tmp_device->name);
	g_free(tmp_device->port);
	if (!result)
		/* cancel proceeding to next page */
		return TRUE;

	priv->started = TRUE;

	return FALSE;
}

static void
gpd_sync_page_prepare (GnomeDruidPage *page, gpointer arg1, gpointer user_data)
{

	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	GNOME_Pilot_UserInfo user;
	gchar *text, *location;

	priv = gpd->priv;

	read_device_config (GTK_OBJECT (gpd), priv->device);
	if (priv->state->devices == NULL)
		priv->state->devices = g_list_append (priv->state->devices, priv->device);

	if (GTK_TOGGLE_BUTTON (priv->pilot_info_no)->active) {
		/* do send_to_pilot */
		read_pilot_config (GTK_OBJECT (gpd), priv->pilot);
		location = priv->device->type == PILOT_DEVICE_NETWORK ?
		    "netsync" : (priv->device->type == PILOT_DEVICE_BLUETOOTH ?
			"bluetooth" : priv->device->port);
		text = g_strdup_printf (_("About to send the following data to the PDA.\n"
				       "Owner Name: %s\nPDA ID: %d\n"
				       "Please put PDA in %s (%s) and press HotSync button."),
					priv->pilot->pilot_username,
					priv->pilot->pilot_id,
					priv->device->name,
					location);

		save_config_and_restart (priv->gpc, priv->state);

		user.userID = priv->pilot->pilot_id;
		user.username = priv->pilot->pilot_username;

		gnome_pilot_client_set_user_info (priv->gpc,
						  priv->device->name,
						  user,
						  FALSE,
						  GNOME_Pilot_IMMEDIATE,
						  0,
						  &priv->handle1);
	} else {
		/* do get_from_pilot */
		location = priv->device->type == PILOT_DEVICE_NETWORK ?
		    "netsync" : (priv->device->type == PILOT_DEVICE_BLUETOOTH ?
			"bluetooth" : priv->device->port);
		text = g_strdup_printf (_("About to retrieve Owner Name and "
					    "ID from the PDA.\n"
					    "Please put PDA in %s (%s) and press "
					    "HotSync button."),
		    priv->device->name,
		    location);

		save_config_and_restart (priv->gpc, priv->state);

		gnome_pilot_client_get_user_info (priv->gpc, priv->device->name, GNOME_Pilot_IMMEDIATE, 0, &priv->handle1);
		gnome_pilot_client_get_system_info (priv->gpc, priv->device->name, GNOME_Pilot_IMMEDIATE, 0, &priv->handle2);
	}
	gtk_label_set_text (GTK_LABEL (priv->sync_label), text);
        g_free (text);

	if (priv->handle1 <= 0 || priv->handle2 <= 0) {
		error_dialog (GTK_WINDOW (priv->druid_window), _("Failed sending request to gpilotd"));
		return;
	}

	/* disable NEXT until we've synced */
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (priv->druid), TRUE, FALSE, TRUE, TRUE);
}

static gboolean
gpd_sync_page_back (GnomeDruidPage *druidpage, gpointer arg1, gpointer user_data)
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	gboolean need_restart = FALSE;

	priv = gpd->priv;
	
	if (priv->handle1 > 0) {
		gnome_pilot_client_remove_request (priv->gpc, priv->handle1);
		priv->handle1 = -1;
		need_restart = TRUE;
	}
	if (priv->handle2 > 0) {
		gnome_pilot_client_remove_request (priv->gpc, priv->handle2);
		priv->handle2 = -1;
		need_restart = TRUE;
	}
	if (need_restart)
		save_config_and_restart (priv->gpc, priv->orig_state);
	return FALSE;
}

static gboolean
gpd_pilot_page_two_next (GnomeDruidPage *druidpage, gpointer arg1, gpointer user_data)
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	
	priv = gpd->priv;


	return (!(check_base_directory (gtk_entry_get_text (GTK_ENTRY (priv->pilot_basedir)))
#ifdef PILOT_LINK_0_12
		    && check_pilot_charset (gtk_entry_get_text (GTK_ENTRY (priv->pilot_charset)))
#endif
		    ));
}

static void
gpd_finish_page_finished (GnomeDruidPage *druidpage, gpointer arg1, gpointer user_data)
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	
	priv = gpd->priv;
	
	read_pilot_config (GTK_OBJECT (gpd), priv->pilot);
	priv->state->pilots = g_list_append (priv->state->pilots, priv->pilot);
	
	save_config_and_restart (priv->gpc, priv->state);
	
	priv->finished = TRUE;
	
	gtk_main_quit ();
}

static void
gpd_device_info_check (GtkEditable *editable, gpointer user_data)
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	gboolean ready;
	
	priv = gpd->priv;
	
	ready = check_cradle_settings (gpd);
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (priv->druid), TRUE, ready, TRUE, TRUE);
}

static void
gpd_pilot_info_check (GtkEditable *editable, gpointer user_data)
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	gboolean ready = TRUE;
	
	priv = gpd->priv;
	
	if (GTK_TOGGLE_BUTTON (priv->pilot_info_no)->active)
		ready = check_pilot_settings (gpd);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (priv->druid), TRUE, ready, TRUE, TRUE);
}

static void
gpd_pilot_info_button (GtkToggleButton *toggle, gpointer user_data)
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	gboolean ready = TRUE;
	
	priv = gpd->priv;
	
	gtk_widget_set_sensitive (priv->pilot_info, toggle->active);
	if (toggle->active)
		ready = check_pilot_settings (gpd);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (priv->druid), TRUE, ready, TRUE, TRUE);
}

static void 
gpd_request_completed (GnomePilotClient* client, const gchar *id, gint handle, gpointer user_data) 
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	
	priv = gpd->priv;

	if (handle == priv->handle1)
		priv->handle1 = -1;
	else if (handle == priv->handle2)
		priv->handle2 = -1;
	else
		return;

	if (priv->handle1 == -1 && priv->handle2 == -1) {
		gnome_druid_set_buttons_sensitive (
		    GNOME_DRUID (priv->druid), TRUE, TRUE, TRUE, TRUE);
	}
}

static void 
gpd_userinfo_requested (GnomePilotClient *gpc, const gchar *device, const GNOME_Pilot_UserInfo *user, gpointer user_data) 
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	gchar *text;
	gchar buf[20];
	
	priv = gpd->priv;
	
	g_message ("device %s sent userinfo", device);
	g_message ("user->userID   = %d", user->userID);
	g_message ("user->username = %s", user->username);

	priv->pilot->pilot_id = user->userID;

	if (priv->pilot->pilot_username) 
		g_free (priv->pilot->pilot_username);
	priv->pilot->pilot_username = g_strdup (user->username);

	text = g_strdup_printf (_("Successfully retrieved Owner Name and ID from PDA.\n"
				  "Owner Name: %s\nPDA ID: %d"),
				priv->pilot->pilot_username,
				priv->pilot->pilot_id);
	gtk_label_set_text (GTK_LABEL (priv->sync_label), text);

	gtk_entry_set_text (GTK_ENTRY (priv->pilot_username), priv->pilot->pilot_username);
	g_snprintf (buf, sizeof (buf), "%d", priv->pilot->pilot_id);
	gtk_entry_set_text (GTK_ENTRY (priv->pilot_id), buf);
	g_free (text);

	/*	gnome_druid_set_buttons_sensitive (GNOME_DRUID (priv->druid), TRUE, TRUE, TRUE, TRUE);

	priv->handle1 = priv->handle2 = -1; */
}

static void 
gpd_system_info_requested (GnomePilotClient *gpc,
			    const gchar *device,
			    const GNOME_Pilot_SysInfo *sysinfo,
			    gpointer user_data) 
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (user_data);
	GnomePilotDruidPrivate *priv;
	
	priv = gpd->priv;
	
	g_message ("device %s sent sysinfo", device);
	g_message ("sysinfo->creation   = %d", sysinfo->creation);
	g_message ("sysinfo->romVersion = 0x%x", sysinfo->romVersion);

	priv->pilot->creation = sysinfo->creation;
	priv->pilot->romversion = sysinfo->romVersion;
}

static void
gpd_destroy (GtkObject *object)
{
	GnomePilotDruid *gpd = GNOME_PILOT_DRUID (object);
	GnomePilotDruidPrivate *priv;
	
	priv = gpd->priv;

	gtk_widget_destroy (priv->druid_window);
	g_object_unref (priv->xml);

	gtk_signal_disconnect_by_data (GTK_OBJECT (priv->gpc), object);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
network_device_toggled_callback (GtkRadioButton *btn, void *data)
{
	GnomePilotDruid *gpd = (GnomePilotDruid *)data;
	GnomePilotDruidPrivate *priv;
	int type;

	priv = gpd->priv;

	/* toggled button could be bluetooth or network */
	if(btn == GTK_RADIO_BUTTON(priv->device_network) &&
	    GTK_TOGGLE_BUTTON(btn)->active) {
		type = PILOT_DEVICE_NETWORK;
	} else if (btn == GTK_RADIO_BUTTON(priv->device_bluetooth) &&
	    GTK_TOGGLE_BUTTON(btn)->active) {
		type = PILOT_DEVICE_BLUETOOTH;
	} else {
		type = PILOT_DEVICE_SERIAL;
	}
	
	set_widget_visibility_by_type(gpd, type);
}	

static void
set_widget_visibility_by_type(GnomePilotDruid *gpd, int type) {
	GnomePilotDruidPrivate *priv;

	gboolean enable_extra_widgets = (type != PILOT_DEVICE_NETWORK &&
	    type != PILOT_DEVICE_BLUETOOTH);

	priv = gpd->priv;

	gtk_widget_set_sensitive(priv->device_port_combo,
	    enable_extra_widgets);
	gtk_widget_set_sensitive(priv->device_port_label,
	    enable_extra_widgets);
	gtk_widget_set_sensitive(priv->device_speed,
	    enable_extra_widgets);
	gtk_widget_set_sensitive(priv->device_speed_label,
	    enable_extra_widgets);
}
