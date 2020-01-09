/* pilot.c
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

/* handles pilot issues */

#include <config.h>
#include <gnome.h>
#include <pi-util.h>

#include "pilot.h"
#include "util.h"

/* create new pilotstate structure, initialize to sane state */
PilotState
*newPilotState (void)
{
    PilotState *p;
    p = g_new0(PilotState, 1);
    return p;
}

static void
copy_device (GPilotDevice *device, PilotState *dest)
{
    GPilotDevice *device2 = gpilot_device_new ();
    device2->name = g_strdup (device->name);
    device2->port = g_strdup (device->port);
    device2->speed = device->speed;
    device2->type = device->type;
    device2->timeout = device->timeout;
    dest->devices = g_list_append (dest->devices, device2);
}

static void
copy_pilot (GPilotPilot *pilot, PilotState *dest)
{
    GPilotPilot *pilot2 = gpilot_pilot_new ();
    pilot2->name = g_strdup (pilot->name);
    pilot2->passwd = g_strdup (pilot->passwd);
    pilot2->pilot_username = g_strdup (pilot->pilot_username);
    pilot2->pilot_id = pilot->pilot_id;
    pilot2->creation = pilot->creation;
    pilot2->romversion = pilot->romversion;
    pilot2->sync_options.basedir = g_strdup (pilot->sync_options.basedir);
#ifdef PILOT_LINK_0_12
    pilot2->pilot_charset = g_strdup (pilot->pilot_charset);
#endif
    dest->pilots = g_list_append (dest->pilots, pilot2);
}

void
copyPilotState (PilotState *dest, PilotState *src)
{
    dest->syncPCid = src->syncPCid;
    dest->progress_stepping = src->progress_stepping;
    if (src->pilots) g_list_foreach (src->pilots,(GFunc)copy_pilot, dest);
    if (src->devices) g_list_foreach (src->devices,(GFunc)copy_device, dest);
}

PilotState*
dupPilotState (PilotState *src)
{
    PilotState *p;
    p = g_new0(PilotState, 1);
    copyPilotState (p, src);
    return p;
}

void
freePilotState (PilotState *state)
{
    g_list_foreach (state->pilots,(GFunc)gpilot_pilot_free, NULL);
    g_list_free (state->pilots);
    g_list_foreach (state->devices,(GFunc)gpilot_device_free, NULL);
    g_list_free (state->devices);
    g_free (state);
}

static gint
loadHostID (PilotState *p)
{
    guint32  id;
    gboolean notfound;

    gnome_config_push_prefix ("/gnome-pilot.d/gpilotd/General/");
    id = gnome_config_get_int_with_default ("sync_PC_Id=0", &notfound);
    p->progress_stepping = gnome_config_get_int_with_default ("progress_stepping=5", NULL);
    gnome_config_pop_prefix ();
  
    if (notfound) {
	p->syncPCid = random ();
	return -1;
    } else {
	p->syncPCid = id;
	return 0;
    }
}    

static gint
loadDeviceCfg (PilotState *p)
{
  GPilotDevice *device;
  gchar *prefix;
  gchar buf[25];
  int i, num;

  gnome_config_push_prefix ("/gnome-pilot.d/gpilotd/General/");
  num = gnome_config_get_int ("num_devices=0");
  gnome_config_pop_prefix ();

  if (num == 0) {
	  g_message ("No pilot cradle information located");
	  p->devices = NULL;
	  return -1;
  } else {
	  for (i=0;i<num;i++){
		  device = g_new0(GPilotDevice, 1);
		  prefix = g_strdup_printf ("/gnome-pilot.d/gpilotd/Device%d/", i);
		  
		  gnome_config_push_prefix (prefix);
		  device->type = gnome_config_get_int ("type=0");
		  if (device->type == PILOT_DEVICE_SERIAL) {
			  g_message ("Cradle Type -> Serial");
		  } else if (device->type == PILOT_DEVICE_USB_VISOR) {
			  g_message ("Cradle Type -> USB");
		  } else if (device->type == PILOT_DEVICE_IRDA) {
			  g_message ("Cradle Type -> IrDA");
		  } else if (device->type == PILOT_DEVICE_NETWORK) {
			  g_message ("Cradle Type -> Network");
		  } else if (device->type == PILOT_DEVICE_BLUETOOTH) {
			  g_message ("Cradle Type -> Bluetooth");
		  } else {
		      g_warning ("Unknown Cradle Type");
		  }

		  sprintf (buf,"name=Cradle%d", i);
		  device->name = gnome_config_get_string (buf);
		  g_message ("cradle device name -> %s", device->name);

		  if (device->type == PILOT_DEVICE_NETWORK) {
			  device->ip = gnome_config_get_string ("ip");
			  g_message ("cradle network ip -> %s", device->ip);
		  } else if (device->type == PILOT_DEVICE_BLUETOOTH) {
			  /* no more parameters */
		  } else {
			  device->port = gnome_config_get_string ("device");
			  g_message ("cradle device name -> %s", device->port);
			  device->speed = gnome_config_get_int ("speed=9600");
			  g_message ("Pilot Speed  -> %d", device->speed);  		  
		  }
		  device->timeout = gnome_config_get_int ("timeout=2");
		  g_message ("Timeout -> %d", device->timeout);
		  gnome_config_pop_prefix ();
		  g_free (prefix);
		  p->devices = g_list_append (p->devices, device);
	  }
  }
  return 0;
}


static gint
loadPilotPilot (PilotState *p)
{
  GPilotPilot *pilot;
  gboolean notfound;
  gchar *prefix;
  char *local_name;
  int i, num;

  gnome_config_push_prefix ("/gnome-pilot.d/gpilotd/General/");
  num = gnome_config_get_int ("num_pilots=0");
  gnome_config_pop_prefix ();

  if (num == 0) {
      g_message ("No pilot userid/username information located");
      p->pilots = NULL;
      return -1;
  } else {
	  for (i=0;i<num;i++){
		  pilot = g_new0(GPilotPilot, 1);
		  prefix = g_strdup_printf ("/gnome-pilot.d/gpilotd/Pilot%d/", i);
		  gnome_config_push_prefix (prefix);
		  pilot->name = gnome_config_get_string ("name=MyPilot");
		  g_message ("Pilot name -> %s", pilot->name);
		  pilot->pilot_id = gnome_config_get_int_with_default ("pilotid",&notfound);
		  if (notfound)
			  pilot->pilot_id = getuid ();
		  g_message ("Pilot id   -> %d", pilot->pilot_id);
		  local_name = gnome_config_get_string ("pilotusername");
#ifdef PILOT_LINK_0_12
		  if (!local_name || (convert_FromPilotChar_WithCharset ("UTF-8", local_name, strlen(local_name), &pilot->pilot_username, NULL) == -1)) {
#else
		  if (!local_name || (convert_FromPilotChar ("UTF-8", local_name, strlen(local_name), &pilot->pilot_username) == -1)) {
#endif
  		  	pilot->pilot_username = g_strdup (local_name);
		  }
		  g_free (local_name);
		  g_message ("Pilot username -> %s", pilot->pilot_username);
	  
		  pilot->creation = gnome_config_get_int ("creation");
		  pilot->romversion = gnome_config_get_int ("romversion");
		  
		  g_message ("Pilot creation/rom = %lu/%lu", pilot->creation, pilot->romversion);

		  pilot->sync_options.basedir = gnome_config_get_string ("basedir");
		  if (pilot->sync_options.basedir==NULL) {
			  pilot->sync_options.basedir = g_strdup_printf ("%s/%s", g_get_home_dir (), pilot->name);
		  }
	  
#ifdef PILOT_LINK_0_12
		  pilot->pilot_charset = gnome_config_get_string ("charset");
		  if (pilot->pilot_charset == NULL)
			  pilot->pilot_charset =
			      g_strdup(get_default_pilot_charset());
#else
		  pilot->pilot_charset = NULL;
#endif

		  pilot->number = i;
	  
		  g_free (prefix);
		  gnome_config_pop_prefix ();
		  
		  p->pilots = g_list_append (p->pilots, pilot);
	  }
  }
  return 0;
}

/* allocates a pilotstate, load pilot state, return 0 if ok, -1 otherwise*/
gint
loadPilotState (PilotState **pilotState)
{
    PilotState *p;

    p = newPilotState ();

    /* load host information */
    if (loadHostID (p) < 0) {
	g_message ("Unable to load host id information, assuming unset");
    }

    if (loadPilotPilot (p) < 0) {
	g_message ("Unable to load pilot id/username, assuming unset");	
    }

    if (loadDeviceCfg (p) < 0) {
	g_message ("Unable to load pilot cradle info, assuming unset");
    }

    *pilotState = p;
    return 0;
}


gint
savePilotState (PilotState *state)
{
  int i;
  GList *tmp;
  gchar *prefix;
  char *local_name;

  gnome_config_clean_section ("/gnome-pilot.d/gpilotd/General/");

  gnome_config_set_int ("/gnome-pilot.d/gpilotd/General/sync_PC_Id", state->syncPCid);
  gnome_config_set_int ("/gnome-pilot.d/gpilotd/General/progress_stepping", state->progress_stepping);

  tmp = state->devices;
  i=0;
  while (tmp!=NULL)
  {
	  GPilotDevice *device=(GPilotDevice*)tmp->data;
	  prefix = g_strdup_printf ("/gnome-pilot.d/gpilotd/Device%d/", i);

	  gnome_config_clean_section (prefix);
	  gnome_config_push_prefix (prefix);
	  gnome_config_set_int ("type", device->type);
	  gnome_config_set_string ("name", device->name);
	  if (device->type == PILOT_DEVICE_NETWORK) {
		  gnome_config_set_string ("ip", device->ip);
	  } else if (device->type == PILOT_DEVICE_BLUETOOTH) {
		  /* no further info stored */
	  } else {
		  gnome_config_set_string ("device", device->port);
		  gnome_config_set_int ("speed", device->speed);
	  }
	  gnome_config_set_int ("timeout", device->timeout);
	  gnome_config_pop_prefix ();
	  g_free (prefix);
	  tmp = tmp->next;
	  i++;
  }  
  if (i>0) {
      gnome_config_push_prefix ("/gnome-pilot.d/gpilotd/General/");
      gnome_config_set_int ("num_devices", i);
      gnome_config_pop_prefix ();
  }

  tmp = state->pilots;
  i=0;
  while (tmp!=NULL)
  {
	  GPilotPilot *pilot=(GPilotPilot*)tmp->data;
	  prefix = g_strdup_printf ("/gnome-pilot.d/gpilotd/Pilot%d/", i);
	  gnome_config_clean_section (prefix);

	  gnome_config_push_prefix (prefix);
	  gnome_config_set_string ("name", pilot->name);
	  gnome_config_set_int ("pilotid", pilot->pilot_id);
	  gnome_config_set_int ("creation", pilot->creation);
	  gnome_config_set_int ("romversion", pilot->romversion);
#ifdef PILOT_LINK_0_12
	  if (!pilot->pilot_username|| (convert_ToPilotChar_WithCharset ("UTF-8", pilot->pilot_username, strlen (pilot->pilot_username), &local_name, NULL) == -1)) {
#else
	  if (!pilot->pilot_username|| (convert_ToPilotChar ("UTF-8", pilot->pilot_username, strlen (pilot->pilot_username), &local_name) == -1)) {
#endif
		  local_name = g_strdup (pilot->pilot_username);
	  }
	  gnome_config_set_string ("pilotusername", local_name);
	  g_free (local_name);
	  gnome_config_set_string ("basedir", pilot->sync_options.basedir);
#ifdef PILOT_LINK_0_12
	  gnome_config_set_string ("charset", pilot->pilot_charset);
#endif
	  gnome_config_pop_prefix ();
	  g_free (prefix);
	  tmp = tmp->next;
	  i++;
  }
  if (i>0) {
      gnome_config_push_prefix ("/gnome-pilot.d/gpilotd/General/");
      gnome_config_set_int ("num_pilots", i);
      gnome_config_pop_prefix ();
  }

  gnome_config_sync ();
  return 0;
}


GList *
load_conduit_list (GPilotPilot *pilot)
{
	GList *conduits = NULL, *conduit_states = NULL;
	gchar *buf;
		
	gnome_pilot_conduit_management_get_conduits (&conduits, GNOME_PILOT_CONDUIT_MGMT_NAME);
	conduits = g_list_sort (conduits, (GCompareFunc) strcmp);
	while (conduits != NULL) {
		ConduitState *conduit_state = g_new0 (ConduitState,1);

		conduit_state->name = g_strdup ((gchar*)conduits->data);
		conduit_state->pilot = pilot;
		conduit_state->management = gnome_pilot_conduit_management_new (conduit_state->name, GNOME_PILOT_CONDUIT_MGMT_NAME);
		conduit_state->config = gnome_pilot_conduit_config_new (conduit_state->management, pilot->pilot_id);
		gnome_pilot_conduit_config_load_config (conduit_state->config);

		conduit_state->description = g_strdup ((gchar*) gnome_pilot_conduit_management_get_attribute (conduit_state->management, "description", NULL));
		conduit_state->icon = g_strdup ((gchar*)gnome_pilot_conduit_management_get_attribute (conduit_state->management, "icon", NULL));
		if (conduit_state->icon == NULL || g_file_test (conduit_state->icon, G_FILE_TEST_IS_REGULAR)==FALSE) {
			conduit_state->icon = gnome_unconditional_pixmap_file ("gnome-palm-conduit.png");
		}
		
		buf = (gchar*) gnome_pilot_conduit_management_get_attribute (conduit_state->management, "settings", NULL);
		if (buf == NULL || buf[0] != 'T') 
			conduit_state->has_settings = FALSE;
		else 
			conduit_state->has_settings = TRUE;
			
		buf = (gchar*) gnome_pilot_conduit_management_get_attribute (conduit_state->management, "default-synctype", NULL);
		if (buf == NULL) 
			conduit_state->default_sync_type = GnomePilotConduitSyncTypeNotSet;
		else 
			conduit_state->default_sync_type = gnome_pilot_conduit_sync_type_str_to_int (buf);

		conduit_state->enabled = gnome_pilot_conduit_config_is_enabled (conduit_state->config, NULL);
		conduit_state->sync_type = conduit_state->config->sync_type;
		conduit_state->first_sync_type = conduit_state->config->first_sync_type;

		conduit_state->orig_enabled = conduit_state->enabled;
		if (conduit_state->enabled) 
			conduit_state->orig_sync_type = conduit_state->sync_type;
		else 
			conduit_state->orig_sync_type = GnomePilotConduitSyncTypeNotSet;
		conduit_state->orig_first_sync_type = conduit_state->first_sync_type;
		conduit_states = g_list_append (conduit_states, conduit_state);
		conduits = conduits->next;

		buf = (gchar*) gnome_pilot_conduit_management_get_attribute (conduit_state->management, "valid-synctypes", NULL);
		if (buf != NULL) {
			gchar **sync_types = g_strsplit (buf, " ", 0);
			int count = 0;

			while (sync_types[count]) {
				conduit_state->valid_synctypes = g_list_append (conduit_state->valid_synctypes, GINT_TO_POINTER (gnome_pilot_conduit_sync_type_str_to_int (sync_types[count])));
				count++;
			}
			g_strfreev (sync_types);
		}
	}

	return conduit_states;
}

void
free_conduit_list (GList *conduits)
{
	/* FIXME Properly free each state */

	g_list_free (conduits);
}
