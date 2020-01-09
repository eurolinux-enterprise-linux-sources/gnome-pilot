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
 *          Vadim Strizhevsky
 *          Manish Vachharajani
 *          Dave Camp
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* for crypt () */
#ifdef USE_XOPEN_SOURCE
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 
#endif /* _XOPEN_SOURCE  */
#endif

#define _BSD_SOURCE 1		/* Or gethostname won't be declared properly
				   on Linux and GNU platforms. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <glib.h>
#include <pwd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <regex.h>

#include <libxml/xmlmemory.h>
#include <libxml/entities.h>

#ifdef WITH_HAL
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libhal.h>
#endif

#include <libgnome/libgnome.h>

#include <pi-source.h>
#include <pi-dlp.h>
#include <pi-version.h>

#include "manager.h"
#include "gnome-pilot-structures.h"
#include "orbit_daemon_glue.h"
#include "gpilot-gui.h"
#include "gnome-pilot-conduit-backup.h"

#include "../libgpilotdCM/gnome-pilot-conduit-management.h"

static GPilotContext *context = NULL;

#ifdef WITH_HAL
static DBusConnection *dbus_connection = NULL;
static LibHalContext *hal_ctx = NULL;

static gboolean gpilotd_dbus_init (void);
static LibHalContext *gpilotd_hal_init (void);
#endif
static guint visor_timeout_id = -1;
static guint hal_initialised = 0;


/* Set to true when the config should be reloaded */
static gboolean reread_config = FALSE;

static gboolean device_in (GIOChannel *, GIOCondition, GPilotContext *);
static gboolean device_err (GIOChannel *, GIOCondition, GPilotContext *);
static gboolean network_device_in (GIOChannel *, GIOCondition, GPilotContext *);
static gboolean network_device_err (GIOChannel *, GIOCondition, GPilotContext *);
static void monitor_channel (GPilotDevice *, GPilotContext *);
static void remove_pid_file (void);
#ifdef PILOT_LINK_0_12
static int check_usb_config(GPilotDevice *device);
#endif

static int
device_equal_by_io (GPilotDevice *dev, GIOChannel *io) 
{
	return !(dev->io == io);
}

static void
remove_device (GPilotContext *context, GPilotDevice *device) 
{
	GList *l;

	g_message ("Removing %s", device->name);

	l = g_list_find (context->devices, device);
	if (l != NULL) {
		gpilot_device_free (l->data);
		context->devices = g_list_remove (context->devices, l);
		g_list_free_1 (l);
	} else {
		g_message ("%s not found",device->name);
	}
}

static void 
pilot_set_baud_rate (GPilotDevice *device) 
{
	static char rate_buf[128];
	
	g_snprintf (rate_buf, 128, "PILOTRATE=%d", device->speed);
	g_message ("setting %s", rate_buf);
	putenv (rate_buf);
}

/*
  Sets *error to 1 on fatal error on the device, 2 on other errors , 0 otherwise.
 */
static int 
pilot_connect (GPilotDevice *device,int *error) 
{
#define MAX_TIME_FOR_PI_BIND 1000000 /* 1 second, or 1,000,000 usec. */
#define SLEEP_TIME_FOR_PI_BIND 50000 /* 50 ms */
	
#ifndef PILOT_LINK_0_12
	struct pi_sockaddr addr;
	union {
		struct pi_sockaddr *pi_sockaddrp;
		struct sockaddr *sockaddrp;
	} addrp;
#endif
	int sd, listen_sd, pf;
	int ret;

	int time_elapsed_pi_bind = 0;
	
	if (device->type != PILOT_DEVICE_NETWORK &&
	    device->type != PILOT_DEVICE_BLUETOOTH) {
		pilot_set_baud_rate (device);
	}
	
	switch (device->type) {
	case PILOT_DEVICE_SERIAL:
		pf = PI_PF_PADP;
		break;
	case PILOT_DEVICE_USB_VISOR:
		pf = PI_PF_NET;
		break;
	case PILOT_DEVICE_IRDA:
		pf = PI_PF_PADP;
		break;
	case PILOT_DEVICE_NETWORK:
	case PILOT_DEVICE_BLUETOOTH:
		pf = PI_PF_NET;
		break;
	default:
		pf = PI_PF_DLP;
		break;
	}
	
	if (device->type == PILOT_DEVICE_NETWORK ||
	    device->type == PILOT_DEVICE_BLUETOOTH) {
		/* In the case of network pi_sockets, the socket is already
		 * listening at this point, so move on to accept */
		listen_sd = device->fd;
	} else {
#ifdef PILOT_LINK_0_12
		/* pl 0.12.0 wants to autodetect the protocol, so pass DLP */
		/* (at time of writing there's a buglet in pl where if you
		 * _do_ pass in the correct NET protocol then pl will flush
		 * pending net input, which might just lose your first
		 * packet.
		 */
		listen_sd = pi_socket (PI_AF_PILOT, PI_SOCK_STREAM, PI_PF_DLP);
#else
		listen_sd = pi_socket (PI_AF_PILOT, PI_SOCK_STREAM, pf);
#endif
		if (listen_sd < 0) {
			g_warning ("pi_socket returned error %d (%s)",
			    listen_sd,
			    strerror (errno));
			if (error) *error = 1;
			return -1;
		}

		
		/* 
		 * When using HAL and USB, the ttyUSB[01] device files are
		 * not created until after Hal has notified gpilotd that
		 * a file exists. This causes pi_bind to fail.
		 * To prevent a failure (and therefore sync), retry pi_bind
		 * for up to the time allotted, sleeping a little in-between
		 * tries.
		 */
		
		time_elapsed_pi_bind = 0;
		do {
#ifdef PILOT_LINK_0_12
			ret = pi_bind (listen_sd, device->port);
#else
			addr.pi_family = PI_AF_PILOT;
			strcpy (addr.pi_device,device->port);
			addrp.pi_sockaddrp = &addr;
			ret = pi_bind (listen_sd, addrp.sockaddrp,
			    sizeof (addr));	
#endif
			if(ret < 0) {
				usleep(SLEEP_TIME_FOR_PI_BIND);
				time_elapsed_pi_bind += SLEEP_TIME_FOR_PI_BIND;
			}
		} while(ret<0 && time_elapsed_pi_bind < MAX_TIME_FOR_PI_BIND);
		
		if (ret < 0) {
			g_warning (_("Unable to bind to PDA"));
			if (error)
				*error = 1;
			pi_close(listen_sd);
#ifdef PILOT_LINK_0_12
			check_usb_config(device);
#endif
			return 0;
		}

		ret = pi_listen (listen_sd, 1);
		if (ret != 0) {
			g_warning ("pi_listen: %s", strerror (errno));
			if (error)
				*error = 2;
			pi_close(listen_sd);
			return 0;
		}
	}

	sd = pi_accept_to (listen_sd, NULL, NULL, device->timeout); 
	if (sd < 0) {
		g_warning ("pi_accept_to returned %d: %s", sd, strerror (errno));
		g_warning ("pi_accept_to: timeout was %d secs", device->timeout);
		if (error)
			*error = 2;
		if (device->type != PILOT_DEVICE_NETWORK &&
		    device->type != PILOT_DEVICE_BLUETOOTH) {
			pi_close(listen_sd);
		}
		return 0;
	}

	if (error)
		*error = 0;

	/* since pilot-link 0.12.0preX, the listen_sd represents
	 * the accepted connection, so don't close listen_sd
	 * after the accept */
#ifndef PILOT_LINK_0_12
	if (device->type != PILOT_DEVICE_NETWORK &&
	    device->type != PILOT_DEVICE_BLUETOOTH) {
		pi_close(listen_sd);
	}		
#endif
	return sd;
}

static void
pilot_disconnect (int sd)
{
	int ret; 

	dlp_EndOfSync (sd, 0);
	ret = pi_close (sd);
	if (ret < 0) {
		g_warning ("error %d from pi_close.", ret);
	}

}

static void
write_sync_stamp (GPilotPilot *pilot, int pfd, struct PilotUser *pu,
		  guint32 last_sync_pc, time_t t)
{
	char prefix[256];
	
	pu->lastSyncPC = last_sync_pc;
	pu->lastSyncDate = t;
	
	g_snprintf (prefix, 255, "/gnome-pilot.d/gpilotd/Pilot%d/", pilot->number);
	gnome_config_push_prefix (prefix);
	gnome_config_private_set_int ("sync_date", t);
	gnome_config_pop_prefix ();
	gnome_config_sync ();
	
	dlp_WriteUserInfo (pfd, pu);
}

/** pilot lookup methods **/

static gint
match_pilot_and_majick (const GPilotPilot *pilot,
			unsigned long creation,
			unsigned long romversion)
{
	if (pilot->creation == creation &&
	    pilot->romversion == romversion) {
		g_message ("pilot %s %ld %ld matches %ld %ld", 
			   pilot->name, 
			   pilot->creation,
			   pilot->romversion, 
			   creation, 
			   romversion);
		return 1;
	}
	g_message ("pilot %s %ld %ld doesn't match %ld %ld", 
		   pilot->name, 
		   pilot->creation,
		   pilot->romversion, 
		   creation, 
		   romversion);
	return 0;
}


#ifdef PILOT_LINK_0_12
/* Checks whether visor module is loaded but 'usb:' device is selected,
 * or vice versa.  Returns 0 if the config looks okay, or -1 otherwise.
 * raises a gpilot_gui_warning_dialog and a g_warning if the config
 * looks incorrect.
 */
static int
check_usb_config(GPilotDevice *device) {
#define MAXLINELEN 256
	char line[MAXLINELEN];
	gboolean visor_loaded;
	gboolean libusb_device;
	FILE *f;
	int error = 0;

	if (device->type != PILOT_DEVICE_USB_VISOR)
		return 0;
	visor_loaded = FALSE;
	libusb_device = TRUE; /* defaults, in case we skip block */
	f = fopen ("/proc/modules", "r");
	if (f) {
		while(fgets(line, MAXLINELEN, f) != NULL) {
			if (strncmp(line, "visor", 5) == 0) {
				visor_loaded = TRUE;
				break;
			}
		}
		fclose(f);
		libusb_device = (strncmp(device->port,
					 "usb:", 4) == 0);
	}
	if (libusb_device && visor_loaded) {
		g_snprintf(line, MAXLINELEN,_("Failed to connect using "
					      "device `%s', on port `%s'.  "
					      "Check your configuration, "
					      "as you requested "
					      "new-style libusb `usb:' "
					      "syncing, but have the "
					      "old-style `visor' kernel "
					      "module loaded.  "
					      "You may need to select a "
					      "`ttyUSB...' device."),
			   device->name, device->port);
		g_warning(line);
		gpilot_gui_warning_dialog(line);
		error = 1;
	} else if (!libusb_device && !visor_loaded) {
		g_snprintf(line, MAXLINELEN,_("Failed to connect using "
					      "device `%s', on port `%s'.  "
					      "Check your configuration, "
					      "as you requested "
					      "old-style usbserial `ttyUSB' "
					      "syncing, but do not have the "
					      "usbserial `visor' kernel "
					      "module loaded.  "
					      "You may need to select a "
					      "`usb:' device."),
			   device->name, device->port);
		g_warning(line);
		gpilot_gui_warning_dialog(line);
		error = 1;
	}
	return error;
}
#endif /* PILOT_LINK_0_12 */


/**************************/

/*
 * We get a pfd, a device and the context (called if a pilot
 * with id == 0 synchronizes)
 * Get whatever majick number we can get and try to id 
 * the pilot and offer to restore it. If it can id the
 * pilot, ask the user to choose one or "make a new pilot"
 */

static gboolean
gpilot_attempt_restore (struct PilotUser pu,
			int pfd, 
			GPilotDevice *device, 
			GPilotContext *context)
{
	struct SysInfo sysinfo;
	struct CardInfo cardinfo;
	GPilotPilot *pilot = NULL;
	GList *iterator;
	gboolean result = FALSE;
	
	dlp_ReadStorageInfo (pfd, 0, &cardinfo);
	dlp_ReadSysInfo (pfd, &sysinfo);

	if (context->pilots && context->pilots->next == NULL) {
		pilot = GPILOT_PILOT (g_list_nth (context->pilots, 0)->data);
		g_message ("D: Only one PDA (%s) profile...", pilot->name);
	} else {
		for (iterator = context->pilots; iterator; iterator = g_list_next (iterator)) {
			pilot = GPILOT_PILOT (iterator->data);
			if (match_pilot_and_majick (pilot, 
						    cardinfo.creation,
						    sysinfo.romVersion)) {
				break;
			}
		}
	}

	if (pilot) {
		GPilotPilot *a_pilot;
		a_pilot = gpilot_gui_restore (context, pilot);
		if (a_pilot) {
			orbed_notify_connect (pilot->name,pu);				
			result = gpilot_start_unknown_restore (pfd, device, a_pilot);
			orbed_notify_disconnect (pilot->name);
		}
	} else {
		/* MUST GO */
		gpilot_gui_warning_dialog ("no ident\n"
					   "restoring PDA with ident\n"
					   "c/r = %lu/%lu, exciting things\n"
					   "will soon be here...",
					   cardinfo.creation,
					   sysinfo.romVersion);
	}

	return TRUE;
}

/*
 * This function handles when sync_device (...) encounters an unknown pilot
 */

static void
gpilot_syncing_unknown_pilot (struct PilotUser pu, 
			      int pfd,
			      GPilotDevice *device, 
			      GPilotContext *context)
{

	g_warning (_("Unknown PDA, no userID/username match %ld"),pu.userID);
	/* FIXME: here, restoring one of the available pilots should be
	   offered to the user. Of course with password prompt if the user
	   has password set
	   bug # 8217 */
	if (pu.userID == 0) {
		if (gpilot_attempt_restore (pu, pfd, device, context) == FALSE) {
			gpilot_gui_warning_dialog (_("Use gnomecc to configure PDA"));
		}
	} else {
		/* FIXME: here we should offer to create a profile for the pilot,
		   bug # 8218 */
		gpilot_gui_warning_dialog (_("Unknown PDA - no PDA matches ID %ld\n"
					     "Use gnomecc to configure gnome-pilot"),pu.userID);
	}
}

/*
 * If there are events for the cradle, this executes them,
 * closes the connection and returns.
 * Returns TRUE if connection should be closed afterwards, FALSE
 * is sync should continue
 */
static gboolean 
do_cradle_events (int pfd,
		 GPilotContext *context,
		 struct PilotUser *pu,
		 GPilotDevice *device) 
{
	GList *events, *it;
	gboolean ret = TRUE;
	
	/* elements in events freed by gpc_request_purge calls
	   in orbed_notify_completion */
	events = gpc_queue_load_requests_for_cradle (device->name);
	
	g_message (_("Device %s has %d events"), device->name, g_list_length (events));
	
	/* if no events, return FALSE */
	if (!events)
		return FALSE;
	
	it = events;
	
	while (it) {
		GPilotRequest *req;
		req = it->data;
		switch (req->type) {
		case GREQ_SET_USERINFO:
			g_message (_("Setting userinfo..."));
			g_snprintf (pu->username,127,"%s", req->parameters.set_userinfo.user_id);
			pu->userID = req->parameters.set_userinfo.pilot_id;
			dlp_WriteUserInfo (pfd,pu);
			if (req->parameters.set_userinfo.continue_sync) {
				g_message (_("Sync continues"));
				ret = FALSE;
			}
			orbed_notify_completion (&req);
			break;
		case GREQ_GET_SYSINFO: {
			struct SysInfo sysinfo;
			struct CardInfo cardinfo;

			dlp_ReadStorageInfo (pfd, 0, &cardinfo);
			dlp_ReadSysInfo (pfd, &sysinfo);
			orbed_notify_sysinfo (device->name,
					      sysinfo,
					      cardinfo, 
					      &req);
			orbed_notify_completion (&req);
		}
		break;
		case GREQ_GET_USERINFO:
			g_message (_("Getting userinfo..."));
			orbed_notify_userinfo (*pu,&req);
			orbed_notify_completion (&req);
			break;
		case GREQ_NEW_USERINFO:
			/* FIXME: this is to set the new and return the old (or something) 
			   g_message ("getting & setting userinfo");
			   g_snprintf (pu->username,127,"%s",req->parameters.set_userinfo.user_id);
			   pu->userID = req->parameters.set_userinfo.pilot_id;
			   dlp_WriteUserInfo (pfd,pu);
			   orbed_notify_completion (&req);
			*/
			break;
		default:
			g_warning ("%s:%d: *** type = %d",__FILE__,__LINE__,req->type);
			g_assert_not_reached ();
			break;
		}

		it = g_list_next (it);
	}

	return ret;
}
/**************************/

/*
  This executes a sync for a pilot.

  If first does some printing to the stdout and some logging to the
  pilot, so the dudes can see what is going on. Afterwards, it does
  the initial synchronization operations (which is handling file
  installs, restores, specific conduits runs). This function (in
  manager.c) returns a boolean, whic may abort the entire
  synchronization.

  If it does not, a function in manager.c will be called depending of
  the default_sync_action setting for the pilot (you know, synchronize
  vs copy to/from blablabla).

 */
static void 
do_sync (int pfd,   
	GPilotContext *context,
	struct PilotUser *pu,
	GPilotPilot *pilot, 
	GPilotDevice *device)
{
	GList *conduit_list, *backup_conduit_list, *file_conduit_list;
	GnomePilotSyncStamp stamp;
	char *pilot_name;

	pilot_name = pilot_name_from_id (pu->userID,context);

	gpilot_load_conduits (context,
			     pilot,
			     &conduit_list, 
			     &backup_conduit_list,
			     &file_conduit_list);
	stamp.sync_PC_Id=context->sync_PC_Id;

	if (device->type == PILOT_DEVICE_NETWORK) {
		g_message (_("NetSync request detected, synchronizing PDA"));
	} else if (device->type == PILOT_DEVICE_BLUETOOTH) {
		g_message (_("Bluetooth request detected, synchronizing PDA"));
	} else {
		g_message (_("HotSync button pressed, synchronizing PDA"));
	}
	g_message (_("PDA ID is %ld, name is %s, owner is %s"),
		  pu->userID,
		  pilot->name,
		  pu->username);
  
	/* Set a log entry in the pilot */
	{
		char hostname[64];
		gpilot_add_log_entry (pfd,"gnome-pilot v.%s\n",VERSION);
		if (gethostname (hostname,63)==0)
			gpilot_add_log_entry (pfd,_("On host %s\n"),hostname);
		else
			gpilot_add_log_entry (pfd,_("On host %d\n"),stamp.sync_PC_Id);
	}

	/* first, run the initial operations, such as single conduit runs,
	   restores etc. If this returns True, continue with normal conduit running,
	   if False, don't proceed */
	if (gpilot_initial_synchronize_operations (pfd,
						   &stamp,
						   pu,
						   conduit_list,
						   backup_conduit_list,
						   file_conduit_list,
						   device,
						   context)) {
		gpilot_sync_default (pfd,&stamp,pu,
				     conduit_list,
				     backup_conduit_list,
				     file_conduit_list,
				     context);
		g_message (_("Synchronization ended")); 
		gpilot_add_log_entry (pfd,"Synchronization completed");
	} else {
		g_message (_("Synchronization ended early"));
		gpilot_add_log_entry (pfd,"Synchronization terminated");
	}

	write_sync_stamp (pilot,pfd,pu,stamp.sync_PC_Id,time (NULL));
  
	g_free (pilot_name);

	gpilot_unload_conduits (conduit_list);
	gpilot_unload_conduits (backup_conduit_list);
	gpilot_unload_conduits (file_conduit_list);
}

/*
 * This function handles when sync_device (...) encounters a known pilot
 */

static void
gpilot_syncing_known_pilot (GPilotPilot *pilot,
			    struct PilotUser pu,
			    int pfd,
			    GPilotDevice *device,
			    GPilotContext *context)
{
	struct stat buf; 
	int ret;

#ifdef PILOT_LINK_0_12
	iconv_t ic;

	if (pilot->pilot_charset == NULL ||
	    pilot->pilot_charset == '\0') {
		g_warning (_("No pilot_charset specified.  Using `%s'."),
		    GPILOT_DEFAULT_CHARSET);
		if (pilot->pilot_charset != NULL)
			g_free(pilot->pilot_charset);
		pilot->pilot_charset =
		    g_strdup(GPILOT_DEFAULT_CHARSET);
	}


	/* ensure configured pilot_charset is recognised
	 * by iconv, and override with warning if not.
	 */
	ic = iconv_open(pilot->pilot_charset, "UTF8");
	if (ic == ((iconv_t)-1)) {
		g_warning (_("`%s' is not a recognised iconv charset, "
			       "using `%s' instead."),
		    pilot->pilot_charset,
		    GPILOT_DEFAULT_CHARSET);
		g_free (pilot->pilot_charset);
		pilot->pilot_charset =
		    g_strdup(GPILOT_DEFAULT_CHARSET);
	} else {
		iconv_close(ic);
	}
	/* Set the environment variable PILOT_CHARSET,
	 * to support legacy conduits that don't use
	 * pilot_charset
	 */
	setenv("PILOT_CHARSET",
	    pilot->pilot_charset, 1);
#endif

	ret = stat (pilot->sync_options.basedir, &buf); 

	if (ret < 0 || !( S_ISDIR (buf.st_mode) && (buf.st_mode & (S_IRUSR | S_IWUSR |S_IXUSR))) ) {
		
		g_message ("Invalid basedir: %s", pilot->sync_options.basedir);
		gpilot_gui_warning_dialog (_("The base directory %s is invalid.\n"
					     "Please fix it or use gnomecc to choose another directory."),
					   pilot->sync_options.basedir);	
	} else {
		gboolean pwd_ok = TRUE;
		/* If pilot has password, check against the encrypted version
		   on the pilot */
		if (pilot->passwd) {
			char *pwd;
			
			pwd = g_strndup (pu.password, pu.passwordLength);
			if (g_strcasecmp (pilot->passwd, crypt (pwd, pilot->passwd))) {
				pwd_ok = FALSE;
				gpilot_gui_warning_dialog (_("Unknown PDA - no PDA matches ID %ld\n"
							     "Use gpilotd-control-applet to set PDA's ID"), pu.userID);
			}
			g_free (pwd);
		}
		
		if (pwd_ok) {
			do_sync (pfd, context, &pu, pilot, device);
		}
	}
}

/*
  sync_foreach is the first synchronization entry.

  It first connects to the device on which the signal was detected,
  then it tries to read the user info block from the pilot.

  Hereafter, if there are any events queued for the synchronizing
  cradle, execute them and stop the synchronization (note,
  do_cradle_events returns a bool, if this is FALSE, synchronization
  continues, as some cradle specific events also require a normal sync
  afterwards, eg. the REVIVE call)

  Anyways, if the sync continues, sync_foreach tries to match the
  pilot against the known pilots. If this fails, it should handle it
  intelligently, eg. if the id==0, ask if you want to restore a pilot.

  If the pilot is accepted (dude, there's even a password check!), it
  continues into do_sync, which does all the magic stuff.
*/
   
static gboolean 
sync_device (GPilotDevice *device, GPilotContext *context)
{
	GPilotPilot *pilot;
	int connect_error;
	struct PilotUser pu;
	struct SysInfo ps;
	int pfd;
	
	g_assert (context != NULL);
	g_return_val_if_fail (device != NULL, FALSE);

	/* signal (SIGHUP,SIG_DFL); */
	pfd = pilot_connect (device,&connect_error);

	if (!connect_error) {
               /* connect succeeded, try to read the systeminfo */
               if (dlp_ReadSysInfo (pfd, &ps) < 0) {
                       /* no ? drop connection then */
                       g_warning (_("An error occurred while getting the PDA's system data"));
#ifdef PILOT_LINK_0_12
		       check_usb_config(device);
#endif


		/* connect succeeded, try to read the userinfo */
		} else if (dlp_ReadUserInfo (pfd,&pu) < 0) {
			/* no ? drop connection then */
			g_warning (_("An error occurred while getting the PDA's user data"));
		} else {
	
			/* If there are cradle specific events, handle them and stop */
			if (do_cradle_events (pfd,context,&pu,device)) {
				g_message (_("Completed events for device %s (%s)"),device->name,device->port);
			} else {
				/* No cradle events, validate pilot */
				pilot = gpilot_find_pilot_by_id (pu.userID,context->pilots);

				if (pilot == NULL) {
					/* Pilot is not known */
					gpilot_syncing_unknown_pilot (pu, pfd, device, context);
				} else {
					/* Pilot is known, make connect notifications */
					orbed_notify_connect (pilot->name,pu);				
					gpilot_syncing_known_pilot (pilot, pu, pfd, device, context);
					orbed_notify_disconnect (pilot->name);
				}				
			}
		}
		pilot_disconnect (pfd);
		/* now restart the listener.  fairly brute force
		 * approach, but ensures we re-initialise the listening
		 * socket correctly.  */
		if (device->type == PILOT_DEVICE_NETWORK ||
		    device->type == PILOT_DEVICE_BLUETOOTH) {
			reread_config = TRUE;
		}
	} else {
		if (connect_error==1) return FALSE; /* remove this device */
		else {
			if (device->type == PILOT_DEVICE_NETWORK ||
			    device->type == PILOT_DEVICE_BLUETOOTH) {
				/* fix broken pisock sockets */
				reread_config = TRUE;
			}
			return TRUE;
		}
	}

	return TRUE;
}

static gboolean 
device_in (GIOChannel *io_channel, GIOCondition condition, GPilotContext *context)
{
	GPilotDevice *device;
	GList *element;
	gboolean result = TRUE;
	
	g_assert (context != NULL);
	
	element = g_list_find_custom (context->devices,
				     io_channel,
				     (GCompareFunc)device_equal_by_io);
	
	if (element == NULL || element->data == NULL) {
		g_warning ("cannot find device for active IO channel");
		return FALSE;
	}
	
	device = element->data; 
	if (context->paused) {
		return FALSE; 
	}	
	g_message (_("Woke on %s"), device->name);
	result = sync_device (device, context);
	
#ifdef WITH_IRDA
	if (device->type == PILOT_DEVICE_IRDA) {
		g_message ("Restarting irda funk...");
		gpilot_device_deinit (device);
		gpilot_device_init (device);
		monitor_channel (device, context);
		result = FALSE;
	}
#endif /* WITH_IRDA */
	
	return result;
}

static gboolean 
device_err (GIOChannel *io_channel, GIOCondition condition, GPilotContext *context)
{
	GPilotDevice *device;
	GList *element;
	char *tmp;
	
	g_assert (context != NULL);
	
	switch (condition) {
	case G_IO_IN: tmp = g_strdup_printf ("G_IO_IN"); break;
	case G_IO_OUT : tmp = g_strdup_printf ("G_IO_OUT"); break;
	case G_IO_PRI : tmp = g_strdup_printf ("G_IO_PRI"); break;
	case G_IO_ERR : tmp = g_strdup_printf ("G_IO_ERR"); break;
	case G_IO_HUP : tmp = g_strdup_printf ("G_IO_HUP"); break;
	case G_IO_NVAL: tmp = g_strdup_printf ("G_IO_NVAL"); break;
	default: tmp = g_strdup_printf ("unhandled port error"); break;
	}
	
	element = g_list_find_custom (context->devices,io_channel,(GCompareFunc)device_equal_by_io);
	
	if (element == NULL) {
		/* We most likely end here if the device has just been removed.
		   Eg. start gpilotd with a monitor on a XCopilot fake serial port,
		   kill xcopilot and watch things blow up as the device fails */
		g_warning ("Device error on some device, caught %s",tmp); 
		g_free (tmp);
		return FALSE;
	}
	
	device = element->data;
	
	gpilot_gui_warning_dialog ("Device error on %s (%s)\n"
				  "Caught %s", device->name, device->port, tmp); 
	g_warning ("Device error on %s (%s), caught %s", device->name, device->port, tmp);
	
	remove_device (context, device);
	g_free (tmp);
	
	return FALSE;
}

#ifdef WITH_NETWORK
static gboolean 
network_device_in (GIOChannel *io_channel, GIOCondition condition, GPilotContext *context)
{
	GPilotDevice *device;
	GList *element;
	gboolean result = TRUE;

	g_assert (context != NULL);
      
	element = g_list_find_custom (context->devices,
				     io_channel,
				     (GCompareFunc)device_equal_by_io);

	if (element==NULL || element->data == NULL) {
		g_warning ("cannot find device for active IO channel");
		return FALSE;
	}
	
	device = element->data; 
	if (context->paused) {
		return FALSE; 
	}	
	g_message (_("Woke on network: %s"),device->name);
	result = sync_device (device,context);

	return result;
}

static gboolean 
network_device_err (GIOChannel *io_channel, GIOCondition condition, GPilotContext *context)
{
	GPilotDevice *device;
	GList *element;
	char *tmp;
	
	g_assert (context != NULL);
	
	switch (condition) {
		case G_IO_IN: tmp = g_strdup_printf ("G_IO_IN"); break;
		case G_IO_OUT : tmp = g_strdup_printf ("G_IO_OUT"); break;
		case G_IO_PRI : tmp = g_strdup_printf ("G_IO_PRI"); break;
		case G_IO_ERR : tmp = g_strdup_printf ("G_IO_ERR"); break;
		case G_IO_HUP : tmp = g_strdup_printf ("G_IO_HUP"); break;
		case G_IO_NVAL: tmp = g_strdup_printf ("G_IO_NVAL"); break;
		default: tmp = g_strdup_printf ("unhandled port error"); break;
	}
	
	element = g_list_find_custom (context->devices,io_channel,(GCompareFunc)device_equal_by_io);
	
	if (element == NULL) {
		/* We most likely end here if the device has just been removed.
		   Eg. start gpilotd with a monitor on a XCopilot fake serial port,
		   kill xcopilot and watch things blow up as the device fails */
		g_warning ("Device error on some device, caught %s",tmp); 
		g_free (tmp);
		return FALSE;
	}
	
	device = element->data;

	gpilot_gui_warning_dialog ("Device error on %s, caught %s",
	    device->name, tmp);
	g_warning ("Device error on %s, caught %s",device->name, tmp);

	remove_device (context, device);
	g_free (tmp);
	
	return FALSE;
}
#endif /* WITH_NETWORK */

static GPtrArray *vendor_product_ids = NULL;
static GArray *product_net = NULL;

static int
known_usb_device(gchar *match_str)
{
	int i;
	for (i = 0; i < vendor_product_ids->len; i++) {
		if (!g_strncasecmp (match_str, 
			vendor_product_ids->pdata[i], 
			strlen (vendor_product_ids->pdata[i])))
			return i;
	}
	return -1;
}

static void
load_devices_xml (void) 
{
	xmlDoc *doc = NULL;
	xmlNode *root, *node;
	char *filename;

	if (vendor_product_ids)
		return;
	
	vendor_product_ids = g_ptr_array_new ();
	product_net = g_array_new (FALSE, FALSE, sizeof (gboolean));

	filename = g_build_filename (DEVICE_XML_DIR, "devices.xml", NULL);
	doc = xmlParseFile (filename);
	g_free (filename);
		
	if (!doc) {
		g_warning ("Unable to read device file at %s", filename);
		
		return;
	}

	root = xmlDocGetRootElement (doc);
	if (!root->name || strcmp (root->name, "device-list"))
		goto fail;
	
	for (node = root->children; node; node = node->next) {
		xmlChar *vendor, *product, *net;
		gboolean use_net;
		
		if (node->type != XML_ELEMENT_NODE)
			continue;

		if (!node->name || strcmp (node->name, "device")) {
			g_warning ("Invalid sub node %s", node->name != NULL ? (char *) node->name : "");

			continue;
		}
		
		vendor = xmlGetProp (node, "vendor_id");
		product = xmlGetProp (node, "product_id");

		if (!vendor || !product) {
			g_warning ("No vendor or product id");

			continue;
		}
		
		g_message ("Found %s, %s", vendor, product);
		g_ptr_array_add (vendor_product_ids, g_strdup_printf ("Vendor=%s ProdID=%s", vendor, product));

		xmlFree (vendor);
		xmlFree (product);
		
		net = xmlGetProp (node, "use_net");
		use_net = !net || strcmp (net, "false");
		
		g_message ("Using net %s", use_net ? "TRUE" : "FALSE");
		g_array_append_val (product_net, use_net);

		xmlFree (net);
	}

 fail:
	xmlFreeDoc (doc);
}

#ifdef WITH_HAL
static void
hal_device_added (LibHalContext *ctx, const char *udi)
{
	gboolean visor_net = FALSE;
	char *bus, *platform, *match_str;
	int vendor_id, product_id, i;
	GPilotDevice *device;
	DBusError error;
	GList *dev;

	if (context->paused) 
		return;

	load_devices_xml ();

	/* HAL match rule: we look for pda.platform == 'palm'
	 * (or the legacy info.bus == 'usb_device')
	 * and then try to match the usb_device.product_id and usb_device.vendor_id
	 * against the list in devices.xml.
	 */
	if (platform = libhal_device_get_property_string (hal_ctx, udi, "pda.platform", NULL)) {
	    if (strcmp (platform, "palm") != 0) {
		libhal_free_string (platform);
		return;
	    }
	    libhal_free_string (platform);
	} else if (bus = libhal_device_get_property_string (hal_ctx, udi, "info.bus", NULL)) {
	    if (strcmp (bus, "usb_device") != 0) {
		libhal_free_string (bus);
		return;
	    }
	    libhal_free_string (bus);
	} else {
	    return;
	}

	dbus_error_init (&error);
	vendor_id = libhal_device_get_property_int (hal_ctx, udi,
	    "usb_device.vendor_id", &error);
	if(vendor_id == -1) {
		g_warning ("Could not get usb vendor ID from hal: %s", error.message);
		return;
	}
	product_id = libhal_device_get_property_int (hal_ctx, udi,
	    "usb_device.product_id", NULL);
	if(product_id == -1) {
		g_warning ("Could not get usb product ID from hal: %s", error.message);
		return;
	}
	
	/* now look for a vendor/product match */
	match_str = g_strdup_printf ("Vendor=%04x ProdID=%04x",
	    vendor_id, product_id);
	i = known_usb_device(match_str);
	g_free(match_str);
	if(i == -1)
		return;

	visor_net = g_array_index (product_net, gboolean, i);
	dev = context->devices;
	while (dev != NULL) {
		device = dev->data;
		if (device->type == PILOT_DEVICE_USB_VISOR) {
			if (!visor_net)
				device->type = PILOT_DEVICE_SERIAL;
			/* problems have been reported with devices
 			 * not being ready for sync immediately,
 			 * so we wait for 0.4 seconds.  See
 			 * bugzilla.gnome #362565
 			 */
			usleep(400000);
			/* just try to sync.  Until I can talk to 
			 * the kernel guys this is the best way to 
			 * go. */
			sync_device (device, context);

			if (!visor_net)
				device->type = PILOT_DEVICE_USB_VISOR;
			
			break;
		}
		
		dev = dev->next;
	}
}

static void
hal_device_removed (LibHalContext *ctx, const char *udi)
{
	
}

static gboolean
reinit_dbus (gpointer user_data)
{
	if (gpilotd_dbus_init ()) {
		if ((hal_ctx = gpilotd_hal_init ()))
			libhal_ctx_set_dbus_connection (hal_ctx, dbus_connection);
		else
			exit (1);
		
		return FALSE;
	}
	
	return TRUE;
}

static DBusHandlerResult
gpilotd_dbus_filter_function (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected") &&
	    strcmp (dbus_message_get_path (message), DBUS_PATH_LOCAL) == 0) {
		libhal_ctx_free (hal_ctx);
		hal_ctx = NULL;
		
		dbus_connection_unref (dbus_connection);
		dbus_connection = NULL;
		
		g_timeout_add (3000, reinit_dbus, NULL);
		
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean
gpilotd_dbus_init (void)
{
	DBusError error;
	
	if (dbus_connection != NULL)
		return TRUE;
	
	dbus_error_init (&error);
	if (!(dbus_connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error))) {
		g_warning ("could not get system bus: %s", error.message);
		dbus_error_free (&error);
		return FALSE;
	}
	
	dbus_connection_setup_with_g_main (dbus_connection, NULL);
	dbus_connection_set_exit_on_disconnect (dbus_connection, FALSE);
	
	dbus_connection_add_filter (dbus_connection, gpilotd_dbus_filter_function, NULL, NULL);
	
	return TRUE;
}


static LibHalContext *
gpilotd_hal_init (void)
{
	LibHalContext *ctx;
	DBusError error;
	char **devices;
	int nr;
	
	if (!gpilotd_dbus_init ()) {
		g_warning ("failed to initialise DBUS!");
		return NULL;
	}

	if (!(ctx = libhal_ctx_new ())) {
		g_warning ("failed to create a HAL context!");
		return NULL;
	}
	
	libhal_ctx_set_dbus_connection (ctx, dbus_connection);
	
	libhal_ctx_set_device_added (ctx, hal_device_added);
	libhal_ctx_set_device_removed (ctx, hal_device_removed);
	/*libhal_ctx_set_device_new_capability (ctx, hal_device_new_capability);*/
	/*libhal_ctx_set_device_lost_capability (ctx, hal_device_lost_capability);*/
	/*libhal_ctx_set_device_property_modified (ctx, hal_property_modified);*/
	/*libhal_ctx_set_device_condition (ctx, hal_device_condition);*/
	
	dbus_error_init (&error);
	if (!libhal_device_property_watch_all (ctx, &error)) {
		g_warning ("failed to watch all HAL properties!: %s", error.message);
		dbus_error_free (&error);
		libhal_ctx_free (ctx);
		return NULL;
	}
	
	if (!libhal_ctx_init (ctx, &error)) {
		g_warning ("libhal_ctx_init failed: %s", error.message);
		dbus_error_free (&error);
		libhal_ctx_free (ctx);
		return NULL;
	}
	
	/*
	 * Do something to ping the HAL daemon - the above functions will
	 * succeed even if hald is not running, so long as DBUS is.  But we
	 * want to exit silently if hald is not running, to behave on
	 * pre-2.6 systems.
	 */
	if (!(devices = libhal_get_all_devices (ctx, &nr, &error))) {
		g_warning ("seems that HAL is not running: %s", error.message);
		dbus_error_free (&error);
		
		libhal_ctx_shutdown (ctx, NULL);
		libhal_ctx_free (ctx);
		return NULL;
	}
	
	libhal_free_string_array (devices);
	
	return ctx;
}
#endif /* WITH_HAL */
#ifdef WITH_USB_VISOR

static gboolean 
visor_devices_timeout (gpointer data) 
{
	GPilotContext *context = data;
	GPilotDevice *device;
	GList *l;
	int i, use_sysfs;
	static int devfs_warning = 0;
	gboolean visor_exists = FALSE, visor_net = TRUE;
	char *usbdevicesfile_str ="/proc/bus/usb/devices";
	char line[256]; /* this is more than enough to fit any line from 
			 * /proc/bus/usb/devices */
	FILE *f;
	gchar *fcontent;
	gchar *fname;
	gchar *vend_id; 
	gchar *prod_id;
	gchar *to_match;
	GError *error = NULL;
	regex_t regex_pattern;

	GDir *topdir;
	const gchar *entry;
	gchar *sysfs_dir_name = "/sys/bus/usb/devices/";
	gchar *f_vend = "idVendor";
	gchar *f_prod = "idProduct";


	g_assert (context != NULL);

	if (context->paused) 
		return FALSE;

	load_devices_xml ();
#ifdef linux
	/* choose a method for searching for known USB devices:
	 * If we can't find sysfs, second choice is
	 * legacy /proc/bus/usb/devices.
	 */
	use_sysfs = 1; /* default */
	topdir = g_dir_open (sysfs_dir_name, 0, &error);
	if (!topdir) {
		use_sysfs = 0;
		f = fopen (usbdevicesfile_str, "r");
		if (!f)
			f = fopen ("/proc/bus/usb/devices_please-use-sysfs-instead", "r");
		if (!f) {
			if (!devfs_warning) {
				devfs_warning = 1;
				char *str = g_strdup_printf (
				    _("Failed to find directory %s or read file %s.  "
					"Check that usbfs or sysfs is mounted."),
				    sysfs_dir_name,
				    usbdevicesfile_str);
				g_warning (str);
				g_free (str);
			}
			return TRUE; /* can't proceed */
		}
		devfs_warning = 0;
	}
	if (use_sysfs) {
		/* This regex allows 99 root-hubs and a bus-depth of 6 tiers for
		 * each root-hub. Each hub can have 99 ports. Refer to "Bus
		 * Topology" in the USB 2.0 specs and the sysfs structures of
		 * Linux USB for further explanation */
		regcomp (&regex_pattern,
		    "^[[:digit:]]{1,2}[-][[:digit:]]{1,2}([.][[:digit:]]{1,2}){0,5}$",
		    REG_EXTENDED | REG_NOSUB); 

		entry = g_dir_read_name (topdir);
		while ((entry != NULL) && (!visor_exists)) {
			if (!regexec (&regex_pattern, entry, 0, 0, 0)){
				fname = g_build_filename (sysfs_dir_name, entry,
				    f_vend, NULL);
				if (!g_file_get_contents (fname, &fcontent,
					NULL, &error)){
					g_warning ("%s", &*error->message);
					regfree (&regex_pattern);
					g_free (fname);
					g_dir_close (topdir);
					return TRUE;
				}
				vend_id = g_strndup (fcontent, 4);
				g_free (fname);
				g_free (fcontent);

				fname = g_build_filename (sysfs_dir_name, entry,
				    f_prod, NULL);
				if (!g_file_get_contents (fname, &fcontent,
					NULL, &error)){
					g_warning ("%s", &*error->message);
					regfree (&regex_pattern);
					g_free (fname);
					g_free (vend_id);
					g_dir_close (topdir);
					return TRUE;
				}
				prod_id = g_strndup (fcontent, 4);
				g_free (fname);
				g_free (fcontent);

				to_match = g_strconcat ("Vendor=", vend_id,
				    " ProdID=", prod_id, NULL);
				i = known_usb_device(to_match);
				if(i != -1) {
					visor_exists = TRUE;
					visor_net = g_array_index (
					    product_net, gboolean, i);
				}
				g_free (vend_id);
				g_free (prod_id);
				g_free (to_match);
			}
			entry = g_dir_read_name (topdir);
		}
		g_dir_close (topdir);
		regfree (&regex_pattern);
	} else {
		/* non sysfs branch... read /proc/bus/usb/devices */
		while (fgets (line, 255, f) != NULL && !visor_exists) {
			if (line[0] != 'P')
				continue;
			i = known_usb_device(line + 4); /* line + strlen("P:  ") */
			if (i != -1) {
				visor_exists = TRUE;
				visor_net = g_array_index (
				    product_net, gboolean, i);
			}
		}
	
		fclose (f);
	}
	
#else
#if defined(sun) && defined (__SVR4) /*for solaris */
	/* On Solaris we always try to sync.  This isn't
	 * a great solution, but does enable syncing.  We
	 * should use HAL on Solaris when it is working
	 * well.  See bug #385444.
	 */
       visor_exists = TRUE;
#endif /* defined(sun) && defined (__SVR4) */
#endif /* linux */
	if (visor_exists) {
		l = context->devices;
		while (l) {
			device = l->data;
			if (device->type == PILOT_DEVICE_USB_VISOR) {
				if (!visor_net)
					device->type = PILOT_DEVICE_SERIAL;

				/* just try to synch.  Until I can talk to 
				 * the kernel guys this is the best way to 
                                 * go. */
				sync_device (device, context);
				sleep(1);

				if (!visor_net)
					device->type = PILOT_DEVICE_USB_VISOR;
				break; /* don't try to sync any more devices! */
			}
			l = l->next;
		}
	}

	return TRUE;
}

#endif /* WITH_USB_VISOR */


static void
monitor_channel (GPilotDevice *dev, GPilotContext *context) 
{
	g_assert (context != NULL);
	
	if (dev->type == PILOT_DEVICE_SERIAL
	    || dev->type == PILOT_DEVICE_IRDA) {
		dev->in_handle = g_io_add_watch (dev->io,
						G_IO_IN,
						(GIOFunc)device_in,
						(gpointer)context);
		dev->err_handle = g_io_add_watch (dev->io,
						 G_IO_ERR|G_IO_PRI|G_IO_HUP|G_IO_NVAL,
						 (GIOFunc)device_err,
						 (gpointer)context);
	} else if (dev->type == PILOT_DEVICE_NETWORK ||
	    dev->type == PILOT_DEVICE_BLUETOOTH) {
#ifdef WITH_NETWORK
		dev->in_handle = g_io_add_watch (dev->io,
						G_IO_IN,
						(GIOFunc)network_device_in,
						(gpointer)context);
		dev->err_handle = g_io_add_watch (dev->io,
						 G_IO_ERR|G_IO_PRI|G_IO_HUP|G_IO_NVAL,
						 (GIOFunc)network_device_err,
						 (gpointer)context);
#else /* WITH_NETWORK */
		g_assert_not_reached ();
#endif /* WITH_NETWORK */
	} if (dev->type == PILOT_DEVICE_USB_VISOR) {
		if(hal_initialised) {
			/* handled by hal callbacks */
			dev->device_exists = FALSE;
		} else {
#ifdef WITH_USB_VISOR
#if defined(linux) || (defined(sun) && defined(__SVR4))
			/* We want to watch for a new recognised USB device
			 * once per context. */
			if (visor_timeout_id == -1) {
				visor_timeout_id = g_timeout_add (2000,
				    visor_devices_timeout, context);
			}
#else /* linux or solaris */
			g_assert_not_reached ();
#endif /* linux or solaris */
#endif /* WITH_USB_VISOR */
		}
	}

	if (dev->type == PILOT_DEVICE_NETWORK) {
		g_message (_("Watching %s (network)"), dev->name);
	} else if (dev->type == PILOT_DEVICE_BLUETOOTH) {
		g_message (_("Watching %s (bluetooth)"), dev->name);
	} else {
		g_message (_("Watching %s (%s)"), dev->name, dev->port);
	}
}

static void 
sig_hup_handler (int dummy)
{
	signal (SIGHUP, sig_hup_handler);
	reread_config = TRUE;
}

static void 
sig_term_handler (int dummy)
{
	g_message (_("Exiting (caught SIGTERM)..."));
	remove_pid_file ();
	gpilotd_corba_quit ();
	exit (0);
}

static void 
sig_int_handler (int dummy)
{
	g_message (_("Exiting (caught SIGINT)..."));
	remove_pid_file ();
	gpilotd_corba_quit ();
	exit (0);
}

/* This deletes the ~/.gpilotd.pid file */
static void 
remove_pid_file (void)
{
	char *filename;
	
	filename = g_build_filename (g_get_home_dir (), ".gpilotd.pid", NULL);
	unlink (filename);
	g_free (filename);
}

/*
  The creates a ~/.gilotd.pid, containing the pid
   of the gpilotd process, used by clients to send
   SIGHUPS
*/
static void 
write_pid_file (void)
{
	char *filename, *dirname, *buf;
	size_t nwritten = 0;
	ssize_t n, w;
	int fd;
	
	dirname = g_build_filename (g_get_home_dir (), ".gpilotd", NULL);
	if (!g_file_test (dirname, G_FILE_TEST_IS_DIR) && mkdir (dirname, 0777) != 0)
		g_warning (_("Unable to create file installation queue directory"));
	g_free (dirname);
	
	filename = g_build_filename (g_get_home_dir (), ".gpilotd.pid", NULL);
	fd = open (filename, O_CREAT | O_TRUNC | O_RDWR, 0644);
	g_free (filename);
	
	if (fd == -1)
		return;
	
	buf = g_strdup_printf ("%lu", (unsigned long) getpid ());
	n = strlen (buf);
	
	do {
		do {
			w = write (fd, buf + nwritten, n - nwritten);
		} while (w == -1 && errno == EINTR);
		
		if (w == -1)
			break;
		
		nwritten += w;
	} while (nwritten < n);
	
	/* FIXME: what do we do if the write is incomplete? (e.g. nwritten < n)*/
	
	fsync (fd);
	close (fd);
}

/*  
    The main loop. Sets up monitors for the devices and calls
    g_main_iteration to wait for a sync. The monitor handler handles
    the sync, so look in device_in for the call to the actual sync.

    If reread_config gets set to TRUE (by a SIGHUP signal, free
    all devices and context and reload it 
*/

static void 
wait_for_sync_and_sync (void)
{
	signal (SIGTERM, sig_term_handler);
	signal (SIGINT, sig_int_handler);
	signal (SIGHUP, sig_hup_handler);
	
	g_list_foreach (context->devices, (GFunc)monitor_channel, context);
	
	while (1) {
		if (reread_config) {
			g_message (_("Shutting down devices"));
			gpilot_context_free (context);
			g_message (_("Rereading configuration..."));
			gpilot_context_init_user (context);
			reread_config=FALSE;
			g_list_foreach (context->devices, (GFunc)monitor_channel, context);
		}
		/* Enter the gtk main loop */
		g_main_iteration (TRUE);
	}
}

/* This function display which pilot-link version was used
   and which features were enabled at compiletime */
static void
dump_build_info (void)
{
	GString *str = g_string_new (NULL);
	g_message ("compiled for pilot-link version %s",
		   GP_PILOT_LINK_VERSION);

	str = g_string_append (str, "compiled with ");
#ifdef WITH_VFS
	str = g_string_append (str, "[VFS] ");
#endif
#ifdef WITH_USB_VISOR
	str = g_string_append (str, "[USB] ");
#endif
#ifdef WITH_IRDA
	str = g_string_append (str, "[IrDA] ");
#endif
#ifdef WITH_NETWORK
	str = g_string_append (str, "[Network] ");
	str = g_string_append (str, "[Bluetooth] ");
#endif
	g_message (str->str);
	g_string_free (str, TRUE);
}

int 
main (int argc, char *argv[])
{
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	
	/*g_log_set_always_fatal (G_LOG_LEVEL_ERROR |
				G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING);*/
	
	/* Intro */
	g_message ("%s %s starting...",PACKAGE,VERSION);
	dump_build_info ();
	
	gnome_init (PACKAGE, VERSION, argc, argv);

#ifdef WITH_HAL
	if ((hal_ctx = gpilotd_hal_init ()) != NULL)
		hal_initialised = 1; /* if 0, fall back to polling sysfs */
#endif
	
	/* Setup the correct gpilotd.pid file */
	remove_pid_file ();
	write_pid_file ();
	
	/* Init corba and context, this call also loads the config into context */
	g_type_init ();
	gpilotd_corba_init (&argc, argv, &context);
	
	/* Begin... */
	wait_for_sync_and_sync ();
	
	/* It is unlikely that we will end here */
	remove_pid_file ();
	gpilotd_corba_quit ();
	
	return 0;
}
