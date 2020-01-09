/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-pilot-ddialog.c
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
#include "gnome-pilot-ddialog.h"


static GtkObjectClass *parent_class = NULL;

struct _GnomePilotDDialogPrivate 
{
	GladeXML *xml;

	GPilotDevice *device;

	GtkWidget *dialog;

	GtkWidget *device_name;
	GtkWidget *device_port;
	GtkWidget *device_port_combo;
	GtkWidget *device_port_label;
	GtkWidget *device_speed;
	GtkWidget *device_speed_label;
	GtkWidget *device_timeout;
	GtkWidget *device_serial;
	GtkWidget *device_usb;
	GtkWidget *device_irda;
	GtkWidget *device_network;
	GtkWidget *device_bluetooth;
#ifdef PILOT_LINK_0_12
	GtkWidget *libusb_label;
	GList *libusb_list;
#endif
};

static void class_init (GnomePilotDDialogClass *klass);
static void init (GnomePilotDDialog *gpdd);

static gboolean get_widgets (GnomePilotDDialog *gpdd);
static void map_widgets (GnomePilotDDialog *gpdd);
static void init_widgets (GnomePilotDDialog *gpdd);
static void fill_widgets (GnomePilotDDialog *gpdd);

static void gpdd_destroy (GtkObject *object);
static void set_widget_visibility_by_type(GnomePilotDDialog *gpdd, int type);
static void network_device_toggled_callback (GtkRadioButton *btn,
    void *data);

GtkType
gnome_pilot_ddialog_get_type (void)
{
  static GtkType type = 0;

  if (type == 0)
    {
      static const GtkTypeInfo info =
      {
        "GnomePilotDDialog",
        sizeof (GnomePilotDDialog),
        sizeof (GnomePilotDDialogClass),
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
class_init (GnomePilotDDialogClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class->destroy = gpdd_destroy;
}

static void
init (GnomePilotDDialog *gpdd)
{
	GnomePilotDDialogPrivate *priv;
	
	priv = g_new0 (GnomePilotDDialogPrivate, 1);

	gpdd->priv = priv;

	/* Gui stuff */
	priv->xml = glade_xml_new ("gpilotd-capplet.glade", "DeviceSettings", NULL);
	if (!priv->xml) {
		priv->xml = glade_xml_new (GLADEDATADIR "/gpilotd-capplet.glade", "DeviceSettings", NULL);
		if (!priv->xml) {
			g_message ("gnome-pilot-ddialog init(): Could not load the Glade XML file!");
			goto error;
		}
	}

	if (!get_widgets (gpdd)) {
		g_message ("gnome-pilot-ddialog init(): Could not find all widgets in the XML file!");
		goto error;
	}

 error:
	;
}



GtkObject *
gnome_pilot_ddialog_new (GPilotDevice *device)
{
	GnomePilotDDialog *gpdd;
	GtkObject *object;
	
	object = gtk_type_new (GNOME_PILOT_TYPE_DDIALOG);
	
	gpdd = GNOME_PILOT_DDIALOG (object);
	gpdd->priv->device = device;

	map_widgets (gpdd);
	fill_widgets (gpdd);
	init_widgets (gpdd);

	return object;
}

static gboolean
get_widgets (GnomePilotDDialog *gpdd)
{
	GnomePilotDDialogPrivate *priv;

	priv = gpdd->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->dialog = GW ("DeviceSettings");

	priv->device_name = GW ("device_name_entry");
	priv->device_port = GW ("device_port_entry");
	priv->device_port_label = GW ("device_port_label");
	priv->device_port_combo = GW ("device_port_combo");
	priv->device_speed = GW ("device_speed_menu");
	priv->device_speed_label = GW ("device_speed_label");
	priv->device_timeout = GW ("timeout_spinner");
	priv->device_serial = GW ("serial_radio");
	priv->device_usb = GW ("usb_radio");
	priv->device_irda = GW ("irda_radio");
	priv->device_network = GW ("network_radio");
	priv->device_bluetooth = GW ("bluetooth_radio");

#ifdef PILOT_LINK_0_12
	/* usb: (libusb) pseudo-device is available from pilot-link 0.12.0 */
	priv->libusb_list = NULL;
	priv->libusb_label = gtk_list_item_new_with_label ("usb:");
	gtk_widget_show(priv->libusb_label);
	priv->libusb_list = g_list_append(priv->libusb_list,
	    priv->libusb_label);
	gtk_list_insert_items (GTK_LIST((GTK_COMBO(priv->device_port_combo))->list),
	    priv->libusb_list, 1);
#endif
	
#undef GW
	return (priv->dialog
		&& priv->device_name
		&& priv->device_port
		&& priv->device_speed
		&& priv->device_timeout
		&& priv->device_serial
		&& priv->device_usb
		&& priv->device_irda
		&& priv->device_network
		&& priv->device_bluetooth);
}

static void
map_widgets (GnomePilotDDialog *gpdd)
{
	GnomePilotDDialogPrivate *priv;
	
	priv = gpdd->priv;

	gtk_object_set_data (GTK_OBJECT (gpdd), "port_entry", priv->device_port);
	gtk_object_set_data (GTK_OBJECT (gpdd), "name_entry", priv->device_name);
	gtk_object_set_data (GTK_OBJECT (gpdd), "speed_menu", priv->device_speed);
	gtk_object_set_data (GTK_OBJECT (gpdd), "irda_radio", priv->device_serial);
	gtk_object_set_data (GTK_OBJECT (gpdd), "usb_radio", priv->device_usb);
	gtk_object_set_data (GTK_OBJECT (gpdd), "irda_radio", priv->device_irda);
	gtk_object_set_data (GTK_OBJECT (gpdd), "network_radio", priv->device_network);
	gtk_object_set_data (GTK_OBJECT (gpdd), "bluetooth_radio", priv->device_bluetooth);
	gtk_object_set_data (GTK_OBJECT (gpdd), "timeout_spinner", priv->device_timeout);
}

static void 
init_widgets (GnomePilotDDialog *gpdd)
{
	GnomePilotDDialogPrivate *priv;

	priv = gpdd->priv;

	gtk_signal_connect (GTK_OBJECT (priv->device_port),"insert-text",
			    GTK_SIGNAL_FUNC (insert_device_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (priv->device_bluetooth), "toggled",
			    GTK_SIGNAL_FUNC (network_device_toggled_callback), gpdd);
	gtk_signal_connect (GTK_OBJECT (priv->device_network), "toggled",
			    GTK_SIGNAL_FUNC (network_device_toggled_callback), gpdd);
}

static void
fill_widgets (GnomePilotDDialog *gpdd)
{
	GnomePilotDDialogPrivate *priv;
	
	priv = gpdd->priv;

	if (priv->device) {
		gtk_entry_set_text (GTK_ENTRY (priv->device_name), priv->device->name);
		if (priv->device->port != NULL)
			gtk_entry_set_text (GTK_ENTRY (priv->device_port), priv->device->port);

		fill_speed_menu (GTK_OPTION_MENU (priv->device_speed), priv->device->speed);

		gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->device_timeout), priv->device->timeout);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->device_serial),
					      priv->device->type == PILOT_DEVICE_SERIAL);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->device_usb),
					      priv->device->type == PILOT_DEVICE_USB_VISOR);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->device_irda),
					      priv->device->type == PILOT_DEVICE_IRDA);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->device_network),
					      priv->device->type == PILOT_DEVICE_NETWORK);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->device_bluetooth),
					      priv->device->type == PILOT_DEVICE_BLUETOOTH);
		set_widget_visibility_by_type(gpdd, priv->device->type);
	}
}

gboolean
gnome_pilot_ddialog_run_and_close (GnomePilotDDialog *gpdd, GtkWindow *parent)
{
	GnomePilotDDialogPrivate *priv;
	gint btn;
	GPilotDevice *tmpdev = g_new0 (GPilotDevice, 1);
	
	priv = gpdd->priv;

	gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), parent);
	while(1) {
		btn = gtk_dialog_run (GTK_DIALOG (priv->dialog));
	
		if (btn == GTK_RESPONSE_OK) {
			read_device_config (GTK_OBJECT (gpdd), tmpdev);
			if(check_device_settings (tmpdev)) {
				*priv->device = *tmpdev;
				break;
			}
		} else {
			break;
		}
	}
	g_free(tmpdev);
	gtk_widget_hide (priv->dialog);

	return btn == GTK_RESPONSE_OK ? TRUE : FALSE;
}

static void
gpdd_destroy (GtkObject *object)
{
	GnomePilotDDialog *gpdd = GNOME_PILOT_DDIALOG (object);
	GnomePilotDDialogPrivate *priv;
	
	priv = gpdd->priv;

	gtk_widget_destroy (priv->dialog);
	gtk_object_unref (GTK_OBJECT (priv->xml));

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
network_device_toggled_callback (GtkRadioButton *btn, void *data)
{
	GnomePilotDDialog *gpdd = (GnomePilotDDialog *)data;
	GnomePilotDDialogPrivate *priv;
	int type;

	priv = gpdd->priv;

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
	set_widget_visibility_by_type(gpdd, type);
}	

static void
set_widget_visibility_by_type(GnomePilotDDialog *gpdd, int type) {
	GnomePilotDDialogPrivate *priv;
	gboolean enable_extra_widgets = (type != PILOT_DEVICE_NETWORK &&
	    type != PILOT_DEVICE_BLUETOOTH);

	priv = gpdd->priv;

	gtk_widget_set_sensitive(priv->device_port_combo,
	    enable_extra_widgets);
	gtk_widget_set_sensitive(priv->device_port_label,
	    enable_extra_widgets);
	gtk_widget_set_sensitive(priv->device_speed,
	    enable_extra_widgets);
	gtk_widget_set_sensitive(priv->device_speed_label,
	    enable_extra_widgets);
}
