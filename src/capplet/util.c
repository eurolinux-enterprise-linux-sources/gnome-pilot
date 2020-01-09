/* util.c
 *
 * Copyright (C) 1999-2000 Free Software Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen
 *          Vadim Strizhevsky
 *
 */

#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <glade/glade.h>
#include <errno.h>
#include "util.h"
#include <iconv.h>

static const guint speedList[] = {9600, 19200, 38400, 57600, 115200, 0};
#define  DEFAULT_SPEED_INDEX  3  /* Default to 57600 */

void
fill_speed_menu (GtkOptionMenu *option_menu, guint default_speed)
{
	gint i = 0, n = DEFAULT_SPEED_INDEX;
	GtkWidget *menu ,*menu_item;
	gchar buf[20];

	g_return_if_fail (option_menu != NULL);
	g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

	menu = gtk_menu_new ();

	while (speedList[i] != 0) {
		g_snprintf (buf,sizeof (buf),"%d",speedList[i]);
		menu_item = gtk_menu_item_new_with_label (buf);
		gtk_widget_show (menu_item);
		gtk_object_set_data (GTK_OBJECT (menu_item), "speed", GINT_TO_POINTER (speedList[i]));
		gtk_menu_append (GTK_MENU (menu),menu_item);
		
		if (speedList[i] == default_speed)
			n = i;
		i++;
	}
	gtk_option_menu_set_menu (option_menu, menu);	
	gtk_option_menu_set_history (option_menu, n);
}

void 
fill_conduit_sync_type_menu (GtkOptionMenu *option_menu, ConduitState *state)
{
	GtkWidget *menu_item;
	GtkMenu   *menu;
	GList *tmp;
	int index, current=0;
	
	menu = GTK_MENU (gtk_menu_new ());

	menu_item = gtk_menu_item_new_with_label(_("Disabled"));
	gtk_widget_show(menu_item);
	gtk_object_set_data (GTK_OBJECT (menu_item), "sync_type", GINT_TO_POINTER (GnomePilotConduitSyncTypeNotSet));
	gtk_menu_append (menu, menu_item);

	tmp = state->valid_synctypes;
	if (tmp == NULL && state->default_sync_type == GnomePilotConduitSyncTypeCustom ) {
			menu_item = gtk_menu_item_new_with_label(_("Enabled"));
			gtk_widget_show(menu_item);
			gtk_object_set_data (GTK_OBJECT (menu_item), "sync_type", GINT_TO_POINTER (state->default_sync_type));
			gtk_menu_append (menu, menu_item);
			if (state->sync_type == state->default_sync_type) 
				current = 1;
	} else {
		for (index = 0; tmp != NULL; tmp = tmp->next, index++) {
			gint value = GPOINTER_TO_INT (tmp->data);
			
			menu_item = gtk_menu_item_new_with_label (sync_type_to_str (value));
			gtk_widget_show (menu_item);
			gtk_object_set_data (GTK_OBJECT (menu_item), "sync_type", GINT_TO_POINTER (value));
			gtk_menu_append (menu,menu_item);
			if (value == state->sync_type) 
				current = index + 1;
		}
	}
	

	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), GTK_WIDGET (menu));
	gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), current);
}

void 
fill_conduit_first_sync_type_menu (GtkOptionMenu *option_menu, ConduitState *state)
{
	GtkWidget *menu_item;
	GtkMenu   *menu;
	GList *tmp;
	int index, current=0;
	
	menu = GTK_MENU (gtk_menu_new ());

	menu_item = gtk_menu_item_new_with_label(_("None"));
	gtk_widget_show(menu_item);
	gtk_object_set_data (GTK_OBJECT (menu_item), "sync_type", GINT_TO_POINTER (GnomePilotConduitSyncTypeNotSet));
	gtk_menu_append (menu, menu_item);

	tmp = state->valid_synctypes;
	for (index = 0; tmp != NULL; tmp = tmp->next, index++) {
		gint value = GPOINTER_TO_INT (tmp->data);
		
		menu_item = gtk_menu_item_new_with_label (sync_type_to_str (value));
		gtk_widget_show (menu_item);
		gtk_object_set_data (GTK_OBJECT (menu_item), "sync_type", GINT_TO_POINTER (value));
		gtk_menu_append (menu,menu_item);
		if (value == state->first_sync_type) 
			current = index + 1;
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), GTK_WIDGET (menu));
	gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), current);
}

gboolean
show_popup_menu (GtkTreeView *treeview, GdkEventButton *event, GtkMenu *menu)
{
	if (event && event->button ==3) {
		GtkTreePath *path;

		if (gtk_tree_view_get_path_at_pos (treeview, event->x, event->y, &path,
						   NULL, NULL, NULL)) {
			gtk_tree_selection_select_path (gtk_tree_view_get_selection (treeview),
							path);
			gtk_tree_path_free (path);

			gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 
					event->button, event->time);
		}
	}

	return FALSE;
}

void 
error_dialog (GtkWindow *parent, gchar *mesg, ...) 
{
	GtkWidget *dlg;
	char *tmp;
	va_list ap;

	va_start (ap,mesg);
	tmp = g_strdup_vprintf (mesg,ap);

	dlg = gtk_message_dialog_new (parent, GTK_DIALOG_DESTROY_WITH_PARENT, 
				      GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, tmp);
	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
	
	va_end (ap);
	g_free (tmp);
}

const char *
get_default_pilot_charset(void) {
	const char *pc;

	if ((pc = getenv("PILOT_CHARSET")) == NULL) {
		pc = GPILOT_DEFAULT_CHARSET;
	}
	return pc;
}

static GPilotDevice *
real_choose_pilot_dialog (PilotState *state) 
{
	GladeXML *xml;
	GtkWidget *dlg;
	GtkWidget *option_menu, *menu_item, *menu;
	GList *tmp;
	GPilotDevice *dev;
	
	xml = glade_xml_new (GLADEDATADIR "/gpilotd-capplet.glade", "ChooseDevice", NULL);
	dlg = glade_xml_get_widget (xml,"ChooseDevice");
	option_menu = glade_xml_get_widget (xml, "device_menu");

	gtk_object_set_data (GTK_OBJECT (dlg), "device_menu", option_menu);
	
	menu = gtk_menu_new ();

	tmp = state->devices;
	while (tmp != NULL){
		dev =(GPilotDevice*)tmp->data;
		if(dev->type == PILOT_DEVICE_NETWORK) {
			menu_item = gtk_menu_item_new_with_label ("[network]");
		} else if(dev->type == PILOT_DEVICE_BLUETOOTH) {
			menu_item = gtk_menu_item_new_with_label ("[bluetooth]");

		} else {
			menu_item = gtk_menu_item_new_with_label (dev->port);
		}
		gtk_object_set_data (GTK_OBJECT (menu_item), "device", dev);
		gtk_widget_show (menu_item);
		gtk_menu_append (GTK_MENU (menu), menu_item);
		tmp = tmp->next;
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), 0);

	if (GTK_RESPONSE_OK == gtk_dialog_run (GTK_DIALOG (dlg))) {
		menu_item = gtk_menu_get_active (GTK_MENU (menu));
		dev = (GPilotDevice *)gtk_object_get_data (GTK_OBJECT (menu_item), "device");
	} else {
		dev = NULL;
	}
	gtk_widget_hide(dlg);

	return dev;
}

GPilotDevice *
choose_pilot_dialog (PilotState *state)
{
	GPilotDevice *dev = NULL;

	if (state->devices == NULL)
		error_dialog (NULL, _("You must have at least one device setup"));
	else if (g_list_length (state->devices) == 1)
		dev = (GPilotDevice*)state->devices->data;
	else
		dev = real_choose_pilot_dialog (state);

	return dev;
}

GPilotPilot *
get_default_pilot (PilotState *state)
{
	GPilotPilot *pilot = g_new0 (GPilotPilot, 1);
	
	pilot->pilot_username = g_strdup(g_get_real_name ());
	pilot->pilot_id = getuid ();
	pilot->name = next_pilot_name (state);
	pilot->sync_options.basedir = g_concat_dir_and_file (g_get_home_dir (), pilot->name);
#ifdef PILOT_LINK_0_12
	pilot->pilot_charset =
	    g_strdup(get_default_pilot_charset());
#endif

	return pilot;
}

GPilotDevice *
get_default_device (PilotState *state)
{
	GPilotDevice *device = g_new0 (GPilotDevice, 1);
	
	device->name = next_cradle_name (state);
	device->port = g_strdup ("/dev/pilot");
	device->speed = speedList[DEFAULT_SPEED_INDEX];
	device->type = PILOT_DEVICE_SERIAL;
	device->timeout = 2;
	
	return device;
}

void
insert_numeric_callback (GtkEditable *editable, const gchar *text,
			 gint len, gint *position, void *data)
{
	gint i;
	
	for (i =0; i<len; i++) {
		if (!isdigit (text[i])) {
			gtk_signal_emit_stop_by_name (GTK_OBJECT (editable), "insert_text");
			return;
		}
	}
}

void
insert_username_callback (GtkEditable *editable, const gchar *text,
			  gint len, gint *position, void *data)
{
	gunichar utf8_char;

	/* need to make sure that existing entry starts with a letter */
	/* since valid usernames must start with a letter             */
	if (*position == 0 && len > 0) {
		utf8_char = g_utf8_get_char_validated (text, -1);
		if (!g_unichar_isalpha (utf8_char)) {
			gtk_signal_emit_stop_by_name (GTK_OBJECT (editable), "insert_text");
		}
	} else {
		gchar *p = (char *) text; 
		/* rest of username can be alphanumeric */
		while (p && *p) {
			utf8_char = g_utf8_get_char_validated (p, -1);
			if (!g_unichar_isalnum (utf8_char) && !g_unichar_isspace (utf8_char)) {
				gtk_signal_emit_stop_by_name (GTK_OBJECT (editable), 
							     "insert_text");
			}
			p = g_utf8_find_next_char (p, NULL);
		}
	}
}

void
insert_device_callback (GtkEditable *editable, const gchar *text,
			gint len, gint *position, void *data)
{
	gint i;
	const gchar *curname;

	curname = gtk_entry_get_text (GTK_ENTRY (editable));
	if (*curname == '\0' && len > 0) {
		if (text[0]!='/'
#ifdef PILOT_LINK_0_12
		    /* usb: pseudo-device is available from pilot-link 0.12.0 */
		    && text[0]!='u'
#endif
		    ) {
			gtk_signal_emit_stop_by_name (GTK_OBJECT (editable), "insert_text");
			return;
		} 
	} else {
		for (i =0;i<len;i++)
			if (!(isalnum (text[i]) || text[i]=='/' || text[i]==':')) {
				gtk_signal_emit_stop_by_name (GTK_OBJECT (editable), "insert_text");
				return;
			}
	}
}

gboolean
check_editable (GtkEditable *editable)
{
	gboolean test = TRUE;
	char *str;
	
	str = gtk_editable_get_chars (editable, 0, -1);
	if (str == NULL || strlen (str) < 1)
		test = FALSE;
	g_free (str);

	return test;
}

/* find the next "Cradle#" name that is available for use */

static gint compare_device_name (GPilotDevice *device, gchar *name)
{
	return strcmp (device->name,name);
}

gchar *
next_cradle_name (PilotState *state)
{
	int i =0;
	gchar buf[16];
	
	sprintf (buf,"Cradle");
	
	while (g_list_find_custom (state->devices,buf,
				   (GCompareFunc)compare_device_name)!= NULL) {
		i++;
		sprintf (buf,"Cradle%d",i);
	}
	return g_strdup (buf);
}

/* find the next "MyPDA#" name that is available for use */

static gint 
compare_pilot_name (GPilotPilot *pilot, gchar *name)
{
	return strcmp (pilot->name, name);
}

gchar *
next_pilot_name (PilotState *state)
{
	int i =0;
	gchar buf[16];
	
	sprintf (buf,"MyPDA");
	
	while (g_list_find_custom (state->pilots, buf,
				   (GCompareFunc)compare_pilot_name)!= NULL) {
		i++;
		sprintf (buf,"MyPDA%d",i);
	}
	return g_strdup (buf);
}

const gchar* 
sync_type_to_str (GnomePilotConduitSyncType t) 
{
	switch (t) {
	case GnomePilotConduitSyncTypeSynchronize:    return _("Synchronize");
	case GnomePilotConduitSyncTypeCopyFromPilot:  return _("Copy from PDA");
	case GnomePilotConduitSyncTypeCopyToPilot:    return _("Copy to PDA");
	case GnomePilotConduitSyncTypeMergeFromPilot: return _("Merge from PDA");
	case GnomePilotConduitSyncTypeMergeToPilot:   return _("Merge to PDA");
	case GnomePilotConduitSyncTypeCustom: 
	case GnomePilotConduitSyncTypeNotSet:     
	default:                                      return _("Use conduit settings");
	}
}

const gchar* 
device_type_to_str (GPilotDeviceType t) 
{
	switch (t) {
	case PILOT_DEVICE_USB_VISOR: return _("USB");
	case PILOT_DEVICE_IRDA:      return _("IrDA");
	case PILOT_DEVICE_NETWORK:   return _("Network");
	case PILOT_DEVICE_BLUETOOTH: return _("Bluetooth");
	default:                     return _("Serial");
	}
}

const gchar *
display_sync_type_name (gboolean enabled, GnomePilotConduitSyncType sync_type)
{
	if (!enabled) 
		return _("Disabled");
	else if (sync_type == GnomePilotConduitSyncTypeCustom) 
		return _("Enabled");
	else 
		return sync_type_to_str (sync_type);
}

gboolean
check_pilot_info (GPilotPilot* pilot1, GPilotPilot *pilot2)
{
	if (pilot1->pilot_id == pilot2->pilot_id 
	    || !strcmp (pilot1->name, pilot2->name)) 
		return TRUE;

	return FALSE;
}

gboolean
check_device_info (GPilotDevice* device1, GPilotDevice *device2)
{
	if (!strcmp (device1->port, device2->port) || !strcmp (device1->name, device2->name)) 
		return TRUE;

	return FALSE;
}

gboolean
check_base_directory (const gchar *dir_name)
{
	gboolean ret = TRUE;
	/* check basedir validity */
	
	if (mkdir (dir_name, 0700) < 0 ) {
		struct stat buf;
		gchar *errstr;
		switch (errno) {
		case EEXIST: 
			stat (dir_name, &buf);
			if (S_ISDIR (buf.st_mode)) {  
				if (!(buf.st_mode & (S_IRUSR | S_IWUSR |S_IXUSR))) {
					error_dialog (NULL, _("The specified base directory exists but has the wrong permissions.\n"
							"Please fix or choose another directory"));
					ret = FALSE;
				}
			} else {
				error_dialog (NULL, _("The specified base directory exists but is not a directory.\n"
						"Please make it a directory or choose another directory"));
				ret = FALSE;
			}
			break;
			
		case EACCES:
			error_dialog (NULL, _("It wasn't possible to create the specified base directory.\n"
					"Please verify the permitions on the specified path or choose another directory"));
			ret = FALSE;
			break;
		case ENOENT:
			error_dialog (NULL, _("The path specified for the base directory is invalid.\n"
					"Please choose another directory"));
			ret = FALSE;
			break;
		default:
			errstr = strerror (errno);
			error_dialog (NULL, errstr);
			ret = FALSE;
		}
	}
	return ret;
}

/* Check charset is a valid iconv character set id.
 * return TRUE if it's valid, or FALSE otherwise.
 */
gboolean
check_pilot_charset (const gchar *charset)
{
#ifndef PILOT_LINK_0_12
	return TRUE;
#else
	iconv_t cd;

	if (charset == NULL || *charset == '\0')
		return TRUE;
	cd = iconv_open(charset, "UTF8");
        if (cd == (iconv_t)-1) {
		error_dialog (NULL, _("`%s' is not a valid character set"
				  " identifier.  Please enter a valid"
				  " identifier or select from the available"
				  " options."), charset);
		
		return FALSE;
	}

	iconv_close(cd);
	return TRUE;
#endif
}

void
read_device_config (GtkObject *object, GPilotDevice* device)
{
	GtkWidget *port_entry, *speed_menu, *item, *name_entry;
	GtkWidget *usb_radio, *irda_radio, *network_radio, *timeout_spinner, *bluetooth_radio;

	g_return_if_fail (device!= NULL);

	port_entry  = gtk_object_get_data (GTK_OBJECT (object), "port_entry");
	name_entry  = gtk_object_get_data (GTK_OBJECT (object), "name_entry");
	speed_menu = gtk_object_get_data (GTK_OBJECT (object), "speed_menu");
	usb_radio = gtk_object_get_data (GTK_OBJECT (object), "usb_radio");
	irda_radio = gtk_object_get_data (GTK_OBJECT (object), "irda_radio");
	network_radio = gtk_object_get_data (GTK_OBJECT (object), "network_radio");
	bluetooth_radio = gtk_object_get_data (GTK_OBJECT (object), "bluetooth_radio");
	timeout_spinner = gtk_object_get_data (GTK_OBJECT (object), "timeout_spinner");

	if (device->port)
		g_free (device->port);
	device->port = g_strdup (gtk_entry_get_text (GTK_ENTRY (port_entry)));

	if (device->name)
		g_free (device->name);
	device->name = g_strdup (gtk_entry_get_text (GTK_ENTRY (name_entry)));
	if (device->name == NULL) device->name = g_strdup ("Cradle"); 

	item = gtk_option_menu_get_menu (GTK_OPTION_MENU (speed_menu));
	item = gtk_menu_get_active (GTK_MENU (item));
	device->speed = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "speed"));

	device->type = PILOT_DEVICE_SERIAL;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (usb_radio))) {
		device->type = PILOT_DEVICE_USB_VISOR;
	} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (irda_radio))) {
		device->type = PILOT_DEVICE_IRDA;
	} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (network_radio))) {
		device->type = PILOT_DEVICE_NETWORK;
	} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (bluetooth_radio))) {
		device->type = PILOT_DEVICE_BLUETOOTH;
	}
	
	device->timeout = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (timeout_spinner));
}

void
read_pilot_config (GtkObject *object, GPilotPilot *pilot)
{
	GtkWidget *id, *name, *pname, *basedir, *menu;
#ifdef PILOT_LINK_0_12
	GtkWidget *charset;
#endif
	const gchar *num;
	gint pilotid;
	g_return_if_fail (pilot!= NULL);

	id      = gtk_object_get_data (GTK_OBJECT (object), "pilotid");
	name    = gtk_object_get_data (GTK_OBJECT (object), "username");
	pname   = gtk_object_get_data (GTK_OBJECT (object), "pilotname");
	basedir = gtk_object_get_data (GTK_OBJECT (object), "basedir");
#ifdef PILOT_LINK_0_12
	charset = gtk_object_get_data (GTK_OBJECT (object), "charset");
#endif
	menu    = gtk_object_get_data (GTK_OBJECT (object), "sync_menu");
 	
	num = gtk_entry_get_text (GTK_ENTRY (id));
	pilotid = strtol (num, NULL, 10);
	pilot->pilot_id = pilotid;

	if (pilot->pilot_username) 
		g_free (pilot->pilot_username);
	pilot->pilot_username = g_strdup (gtk_entry_get_text (GTK_ENTRY (name)));

	if (pilot->name)
		g_free (pilot->name);
	pilot->name = g_strdup (gtk_entry_get_text (GTK_ENTRY (pname)));
	
	if (pilot->sync_options.basedir) 
		g_free (pilot->sync_options.basedir);
	pilot->sync_options.basedir = g_strdup (gtk_entry_get_text (GTK_ENTRY (basedir)));

#ifdef PILOT_LINK_0_12
	if (pilot->pilot_charset)
		g_free (pilot->pilot_charset);
	pilot->pilot_charset = g_strdup (gtk_entry_get_text (GTK_ENTRY (charset)));
#endif
}

void 
save_config_and_restart (GnomePilotClient *gpc, PilotState *state) 
{
	savePilotState (state);
	/* FORCE the gpilotd to reread the settings */
	gnome_pilot_client_reread_config (gpc);
}

gboolean
check_device_settings (GPilotDevice *device)
{
	
	char *str;
#ifndef WITH_HAL
	struct stat buf;
	char *usbdevicesfile_str ="/proc/bus/usb/devices";
	char *sysfs_dir = "/sys/bus/usb/devices";
#endif

#ifdef PILOT_LINK_0_12
	/* device->port is ignored for network and bluetooth syncs */
	if (strcmp(device->port, "usb:") == 0 && device->type != PILOT_DEVICE_NETWORK
	    && device->type != PILOT_DEVICE_BLUETOOTH
	    && device->type != PILOT_DEVICE_USB_VISOR) {
		str = g_strdup (_("Device 'usb:' is only valid for devices of type USB"));
		error_dialog (NULL, str);
		g_free (str);
		return FALSE;
	}
#endif

	if (device->type == PILOT_DEVICE_SERIAL) {
		g_message ("checking rw on %s", device->port);
		if (access (device->port, R_OK|W_OK)) {
			str = g_strdup_printf ("%s\n%s (%s)\n%s",
					       _("Read/Write permissions failed on"),
					       device->name, device->port,
					       _("Check the permissions on the device and retry"));
			error_dialog (NULL, str);
			g_free (str);
			return FALSE;
		}
#ifndef WITH_HAL
#ifdef linux
	} else if (device->type == PILOT_DEVICE_USB_VISOR) {
		/* check sysfs or usbfs is mounted */
		if(stat(sysfs_dir, &buf) != 0 &&
		    ((stat (usbdevicesfile_str, &buf) != 0 &&
		      stat ("/proc/bus/usb/devices_please-use-sysfs-instead", &buf) != 0) ||
		    !(S_ISREG(buf.st_mode)) ||
		    !(buf.st_mode & S_IRUSR))) {
			str = g_strdup_printf (
			    _("Failed to find directory %s or read file %s.  "
				"Check that usbfs or sysfs is mounted."),
			    sysfs_dir,
			    usbdevicesfile_str);
			error_dialog (NULL, str);
			g_free (str);
			return FALSE;
		}
#endif /* linux */
#endif /* !WITH_HAL */
	} 

	return TRUE;
}
