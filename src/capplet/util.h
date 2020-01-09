/* gui-util.h
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

#include <gnome.h>
#include <gpilotd/gnome-pilot-client.h>
#include "pilot.h"

/* Gui routines */
void fill_speed_menu (GtkOptionMenu *optionMenu, guint default_speed);
void fill_synctype_menu (GtkOptionMenu *optionMenu, GnomePilotConduitSyncType type);
void fill_conduit_sync_type_menu (GtkOptionMenu *option_menu, ConduitState *state);
void fill_conduit_first_sync_type_menu (GtkOptionMenu *option_menu, ConduitState *state);
gboolean show_popup_menu (GtkTreeView *treeview, GdkEventButton *event, GtkMenu *menu);

/* Filtering callbacks */
void insert_numeric_callback (GtkEditable *editable, const gchar *text,
			      gint len, gint *position, void *data);
void insert_username_callback (GtkEditable *editable, const gchar *text,
			       gint len, gint *position, void *data);
void insert_device_callback (GtkEditable *editable, const gchar *text,
			     gint len, gint *position, void *data);

gboolean check_editable (GtkEditable *editable);

/* General routines */
GPilotPilot *get_default_pilot (PilotState *state);
GPilotDevice *get_default_device (PilotState *state);
const char *get_default_pilot_charset(void);

gchar *next_cradle_name (PilotState *state);
gchar *next_pilot_name (PilotState *state);

const gchar* sync_type_to_str (GnomePilotConduitSyncType t);
const gchar* device_type_to_str (GPilotDeviceType t);
const gchar* display_sync_type_name (gboolean enabled, GnomePilotConduitSyncType sync_type);

gboolean check_pilot_info (GPilotPilot* pilot1, GPilotPilot *pilot2);
gboolean check_device_info (GPilotDevice* device1, GPilotDevice *device2);
gboolean check_base_directory (const gchar *dir_name);
gboolean check_pilot_charset (const gchar *charset);

/* Dialogs */
void error_dialog (GtkWindow *parent, gchar *mesg, ...);
GPilotDevice *choose_pilot_dialog (PilotState *state);

/* Configuration routines */
void read_device_config (GtkObject *object, GPilotDevice* device);
void read_pilot_config (GtkObject *object, GPilotPilot *pilot);

void save_config_and_restart (GnomePilotClient *gpc, PilotState *state);

gboolean check_device_settings (GPilotDevice *device);
