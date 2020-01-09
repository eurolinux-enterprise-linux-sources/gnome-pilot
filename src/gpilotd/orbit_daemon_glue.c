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
 *
 */

#include "config.h"
#include <gnome.h>

#include <bonobo-activation/bonobo-activation.h>
#include <libbonobo.h>

#include "orbit_daemon_glue.h"
#include "queue_io.h"
#include "manager.h"
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "gnome-pilot-conduit.h"
#include "gnome-pilot-conduit-standard.h"
#include <sys/types.h>
#include <signal.h>

/********************************************************************************/
/* Defines                                                                      */

#define USE_GNORBA
#define DEBUG_CODE

/* two macros for checking a clients id */
#define GET_AND_CHECK_CLIENT_ID(s,c,t)                  \
  s = get_client_id(c);                                 \
  if(s==NULL) {                                         \
    GNOME_Pilot_NoAccess *exn;                          \
    exn = GNOME_Pilot_NoAccess__alloc();                \
    CORBA_exception_set (ev, CORBA_USER_EXCEPTION,      \
			 ex_GNOME_Pilot_NoAccess, exn); \
    return t;                                           \
  }

#define GET_AND_CHECK_CLIENT_ID_VOID(s,c)           \
  s = get_client_id(c);                             \
  if(s==NULL) {                                     \
    GNOME_Pilot_NoAccess *exn;                      \
    exn = GNOME_Pilot_NoAccess__alloc();            \
    CORBA_exception_set (ev, CORBA_USER_EXCEPTION,  \
      ex_GNOME_Pilot_NoAccess, exn);                \
    return;                                         \
  }

/* some macros for throwing exceptions */
#define gpthrow(exception,returnval)                             \
  {                                                              \
	  GNOME_Pilot_##exception *exn;                          \
          exn = GNOME_Pilot_##exception##__alloc();              \
	  CORBA_exception_set (ev, CORBA_USER_EXCEPTION,         \
			       ex_GNOME_Pilot_##exception, exn); \
	  return returnval;                                      \
  }

#define gpthrow_with_settings(exception,returnval,name,commands) \
  {                                                              \
	  GNOME_Pilot_##exception *name;                         \
          name = GNOME_Pilot_##exception##__alloc();             \
          { commands; }                                          \
	  CORBA_exception_set (ev, CORBA_USER_EXCEPTION,         \
			       ex_GNOME_Pilot_##exception, name);\
	  return returnval;                                      \
  }

#define gpthrow_no_val(exception)                                \
  {                                                              \
	  GNOME_Pilot_##exception *exn;                          \
          exn = GNOME_Pilot_##exception##__alloc();              \
	  CORBA_exception_set (ev, CORBA_USER_EXCEPTION,         \
			       ex_GNOME_Pilot_##exception, exn); \
	  return;                                                \
  }

#define gpthrow_no_val_with_settings(exception,name,commands)    \
  {                                                              \
	  GNOME_Pilot_##exception *name;                         \
          name = GNOME_Pilot_##exception##__alloc();             \
          { commands; }                                          \
	  CORBA_exception_set (ev, CORBA_USER_EXCEPTION,         \
			       ex_GNOME_Pilot_##exception, name);\
	  return;                                                \
  }

/* a macro for logging catched exceptions */
#define LOGEXN(e) g_warning ("%s:%d Exception: %s\n",   \
	       	          __FILE__,__LINE__,            \
			  CORBA_exception_id (&e));

/* CORBA_string_dup doesn't take its argument as a const, meaning
   passing consts causes warnings. This macor circumvents that */
#define CORBA_string_dup(s) CORBA_string_dup((CORBA_char*)s)

/********************************************************************************/
/*  Structures                                                                  */

/* This enum is used to decide which CORBA call to do in the 
   orbed_notify_foreach method. It switches on the carries setting, and
   the calls the desired method */
enum ForEachAction {
	CALL_CONNECT,
	CALL_DISCONNECT,
	CALL_CONDUIT_PROGRESS,
	CALL_OVERALL_PROGRESS,
	CALL_CONDUIT_MESSAGE,
	CALL_CONDUIT_ERROR,
	CALL_CONDUIT_BEGIN,
	CALL_CONDUIT_END,
	CALL_DAEMON_MESSAGE,
	CALL_DAEMON_ERROR
};

/* This is used to pass arguments to various _foreach calls */
typedef struct _GPilotd_notify_foreach_carrier GPilotd_notify_foreach_carrier;
struct _GPilotd_notify_foreach_carrier {
	gpointer id;
	gpointer ptr1,ptr2,ptr3;
	glong long1,long2;
	enum ForEachAction action;
	GSList *purge_list;
};

typedef struct _GPilotd_notify_on_helper_foreach_carrier GPilotd_notify_on_helper_foreach_carrier;
struct _GPilotd_notify_on_helper_foreach_carrier {
  gpointer client_id;
  GNOME_Pilot_EventType event;
};

/** structure that carries a GSList to gchar*  (IORs) for
    each notification type */

typedef struct _GPilotd_Orb_Pilot_Notifications GPilotd_Orb_Pilot_Notifications;
struct _GPilotd_Orb_Pilot_Notifications {
  GSList* connect;     
  GSList* disconnect;
  GSList* request_complete;
  GSList* request_timeout;
  GSList* backup;
  GSList* conduit;
  GSList* userinfo_requested;
  GSList* sysinfo_requested;
  GSList* userinfo_sent;
};

/********************************************************************************/
/*  Globals Variables !                                                         */

CORBA_ORB orb;	
CORBA_Environment ev;
GPilotd_Orb_Context *orb_context;

/********************************************************************************/
static GNOME_Pilot_StringSequence* 
empty_StringSequence(void) 
{ 
	GNOME_Pilot_StringSequence *seq;
	seq = GNOME_Pilot_StringSequence__alloc(); 
	seq->_length = 0; 
	seq->_buffer = NULL; 
	return seq;
}

static GNOME_Pilot_LongSequence* 
empty_LongSequence(void) 
{ 
	GNOME_Pilot_LongSequence *seq;
	seq = GNOME_Pilot_LongSequence__alloc(); 
	seq->_length = 0; 
	seq->_buffer = NULL; 
	return seq;
}

static gchar *
get_client_id(const GNOME_Pilot_Client cb) 
{
	gchar *client_id,*retval;
	CORBA_Environment ev;
	
	CORBA_exception_init(&ev);
	client_id = GNOME_Pilot_Client__get_client_id(cb,&ev);
	if(ev._major != CORBA_NO_EXCEPTION) {
		LOGEXN (ev);
		CORBA_exception_free(&ev);
		return NULL;
	}
	
	if(client_id == NULL || 
	   strlen(client_id)==0) {
		
		client_id = CORBA_ORB_object_to_string(orb,cb,&ev);
		if(ev._major != CORBA_NO_EXCEPTION) {
			LOG(("unable to resolve IOR for client"));
			LOGEXN (ev);
			CORBA_exception_free(&ev);
			return NULL;
		}
	  
		GNOME_Pilot_Client__set_client_id(cb,client_id,&ev);
		if(ev._major != CORBA_NO_EXCEPTION) {
			LOGEXN (ev);      
			CORBA_exception_free(&ev);
			return NULL;
		}
	}

	CORBA_exception_free(&ev);
	retval = g_strdup(client_id);
	CORBA_free(client_id);
	return retval;
}



/********************************************************************************/

static void
gpilotd_corba_pause_device(GPilotDevice *device,gpointer data)
{
	if (device->io) {
		g_source_remove(device->in_handle);
		g_source_remove(device->err_handle);
	}
}

static void
gpilotd_corba_pause_notify_monitor (gchar* key,
				    GSList **value,
				    CORBA_boolean *on_off) 
{	
	GNOME_Pilot_Client cb = CORBA_ORB_string_to_object(orb, (CORBA_char*)key, &ev);
	GNOME_Pilot_Client_pause (cb, *on_off, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		char *client_id = get_client_id (cb);
		g_message ("corba: monitor %.20s dead", client_id ? client_id : "<unknown>");
		CORBA_exception_free (&ev);
		CORBA_free (client_id);
	}	
}

static void
gpilotd_corba_pause(GNOME_Pilot_Daemon obj,
		    CORBA_boolean on_off,
		    CORBA_Environment *ev)  
{
	if (orb_context->gpilotd_context->paused == on_off) return;

	orb_context->gpilotd_context->paused = on_off;

	g_hash_table_foreach (orb_context->monitors,
			      (GHFunc)gpilotd_corba_pause_notify_monitor,
			      &on_off);

	if (orb_context->gpilotd_context->paused) {
		g_list_foreach(orb_context->gpilotd_context->devices,
			      (GFunc)gpilotd_corba_pause_device,
			       NULL);				      
	} else {
		kill(getpid(),SIGHUP);
	}
	
	return;
}

static void
gpilotd_corba_reread_config (GNOME_Pilot_Daemon obj,
			     CORBA_Environment *ev)
{
	kill(getpid(),SIGHUP);
}

/********************************************************************************/

static void
gpilotd_corba_noop (GNOME_Pilot_Daemon obj,
		    CORBA_Environment *ev)
{
	return;
}

/********************************************************************************/

static CORBA_unsigned_long
gpilotd_corba_request_install(GNOME_Pilot_Daemon obj,
			      const GNOME_Pilot_Client cb,
			      const CORBA_char *pilot_name,
			      const CORBA_char *filename,
			      const CORBA_char *description,
			      const GNOME_Pilot_Survival survival,
			      const CORBA_unsigned_long timeout,
			      CORBA_Environment *ev)  
{
	GPilotRequest req;
	gchar *client_id;
	
	GET_AND_CHECK_CLIENT_ID(client_id,cb,0);

	LOG(("corba: request_install(pilot_name=%s (%d),filename=%s,survival=%d,timeout=%d)",
	    pilot_name,
	    pilot_id_from_name(pilot_name,orb_context->gpilotd_context),
	    filename,
	    survival,
	    timeout));

	req.type = GREQ_INSTALL;
	req.pilot_id = pilot_id_from_name(pilot_name,orb_context->gpilotd_context);

	/* FIXME: oops, this exception requires some parameters, not setting them causes
	   ORBit to crash&burn:
	   Solution:gpthrow(Exn,commands to se,retval) 
	*/
	if(req.pilot_id==0) {
		g_free(client_id);
		gpthrow_with_settings(UnknownPilot,0,exn,exn->pilotId = CORBA_string_dup(pilot_name));
	}
	if(access(filename,R_OK)) {
		g_free(client_id);
		gpthrow(MissingFile,0);
	}

	req.timeout = survival==GNOME_Pilot_PERSISTENT?0:timeout;
	req.cradle = NULL;
	req.client_id = g_strdup(client_id);
	req.parameters.install.filename = g_strdup(filename);
	req.parameters.install.description = g_strdup(description);

	g_free(client_id);

	return gpc_queue_store_request(req);
}

/* FIXME: look in manager.c, do_restore_foreach */
static CORBA_unsigned_long
gpilotd_corba_request_restore(GNOME_Pilot_Daemon obj,
			      const GNOME_Pilot_Client cb,
			      const CORBA_char *pilot_name,
			      const CORBA_char *_directory,
			      const GNOME_Pilot_Survival survival,
			      const CORBA_unsigned_long timeout,
			      CORBA_Environment *ev)
{
	GPilotRequest req;
	gchar *directory;
	struct stat statbuf;
	char *client_id;
	
	GET_AND_CHECK_CLIENT_ID(client_id,cb,0);

	directory = g_strdup(_directory);
	/* FIXME: should be a generic / remover */
	if (directory[strlen(directory)-1]=='/')
		directory[strlen(directory)-1]=0;
	LOG(("corba: request_restore(pilot_name=%s (%d), directory=%s,survival=%d,timeout=%d)",
	    pilot_name,
	    pilot_id_from_name(pilot_name,orb_context->gpilotd_context),
	    directory,
	    survival,
	    timeout));

	req.type = GREQ_RESTORE;
	req.pilot_id = pilot_id_from_name(pilot_name,orb_context->gpilotd_context);

	if(req.pilot_id==0) {
		g_free(client_id);
		g_free(directory);
		gpthrow_with_settings(UnknownPilot,0,exn,exn->pilotId = CORBA_string_dup(pilot_name));
	}
	if (stat (directory, &statbuf)==-1) {
		g_free (client_id);
		g_free (directory);

		gpthrow (MissingFile,0);
	}
	if (S_ISDIR (statbuf.st_mode) && access (directory,R_OK)!=0) {
		g_free (client_id);
		g_free (directory);
		g_message ("or here access=%d, S_ISDIR=%d", access(directory,R_OK), S_ISDIR(statbuf.st_mode));
		gpthrow (MissingFile, 0);
	}
	req.timeout = survival==GNOME_Pilot_PERSISTENT?0:timeout;
	req.cradle = NULL;
	req.client_id = g_strdup(client_id);

	req.parameters.restore.directory = g_strdup(directory);

	g_free(directory);
	g_free(client_id);

	return gpc_queue_store_request(req);
}

static CORBA_unsigned_long
gpilotd_corba_request_conduit(GNOME_Pilot_Daemon _obj, 
			      const GNOME_Pilot_Client cb,
			      const CORBA_char * pilot_name, 
			      const CORBA_char * conduit_name, 
			      const GNOME_Pilot_ConduitOperation operation,
			      const GNOME_Pilot_Survival survival, 
			      const CORBA_unsigned_long timeout, 
			      CORBA_Environment *ev) 
{
	GPilotRequest req;
	char *client_id;
	
	GET_AND_CHECK_CLIENT_ID(client_id,cb,0);
  
	LOG(("corba: request_conduit(pilot=%s (%d), conduit=%s)",
	    pilot_name,
	    pilot_id_from_name(pilot_name,orb_context->gpilotd_context),
	    conduit_name));
	req.pilot_id = pilot_id_from_name(pilot_name,orb_context->gpilotd_context);

	if(req.pilot_id==0) {
		g_free(client_id);
		gpthrow_with_settings(UnknownPilot,0,exn,exn->pilotId = CORBA_string_dup(pilot_name));
	}

	/* FIXME: check for conduit existence, throw exn if not */

	req.type = GREQ_CONDUIT;
	req.timeout = survival==GNOME_Pilot_PERSISTENT?0:timeout;
	req.cradle = NULL;
	req.client_id = g_strdup(client_id);

	req.parameters.conduit.name = g_strdup(conduit_name);
	switch(operation) {
	case GNOME_Pilot_SYNCHRONIZE:
		req.parameters.conduit.how = GnomePilotConduitSyncTypeSynchronize; break;
	case GNOME_Pilot_COPY_FROM_PILOT:
		req.parameters.conduit.how = GnomePilotConduitSyncTypeCopyFromPilot; break;
	case GNOME_Pilot_COPY_TO_PILOT:
		req.parameters.conduit.how = GnomePilotConduitSyncTypeCopyToPilot; break;
	case GNOME_Pilot_MERGE_FROM_PILOT:
		req.parameters.conduit.how = GnomePilotConduitSyncTypeMergeFromPilot; break;
	case GNOME_Pilot_MERGE_TO_PILOT:
		req.parameters.conduit.how = GnomePilotConduitSyncTypeMergeToPilot; break;
	case GNOME_Pilot_CONDUIT_DEFAULT:
		req.parameters.conduit.how = GnomePilotConduitSyncTypeCustom; break;
	}
	g_free(client_id);
	return gpc_queue_store_request(req);
}

static CORBA_unsigned_long 
gpilotd_corba_request_remove(GNOME_Pilot_Daemon _obj, 
			     const CORBA_unsigned_long handle, 
			     CORBA_Environment *ev) 
{
	/* FIXME: portability pitfall in the /% 65535 ? */
	gpc_queue_purge_request_point(handle/65535,handle%65535);
	return 1;
}

static CORBA_unsigned_long
gpilotd_corba_get_user_info(GNOME_Pilot_Daemon obj,
			    const GNOME_Pilot_Client cb,
			    const CORBA_char *cradle, 
			    const GNOME_Pilot_Survival survival,
			    const CORBA_unsigned_long timeout,
			    CORBA_Environment *ev) 
{
	GPilotRequest req;
	char *client_id;

	GET_AND_CHECK_CLIENT_ID(client_id,cb,0);

	LOG(("corba: get_user_info(cradle=%s,survival=%d,timeout=%d)",
	    cradle,survival,timeout));
	/* FIXME: check for cradle existance, throw exn if not */
	req.type = GREQ_GET_USERINFO;
	req.timeout = survival==GNOME_Pilot_PERSISTENT?0:timeout;
	req.pilot_id = 0;
	req.cradle = g_strdup(cradle);
	req.client_id = g_strdup(client_id);

	g_free(client_id);

	return gpc_queue_store_request(req);
}


static CORBA_unsigned_long
gpilotd_corba_get_system_info(GNOME_Pilot_Daemon obj,
			      const GNOME_Pilot_Client cb,
			      const CORBA_char *cradle, 
			      const GNOME_Pilot_Survival survival,
			      const CORBA_unsigned_long timeout,
			      CORBA_Environment *ev) 
{
	GPilotRequest req;
	gchar *client_id;
	GET_AND_CHECK_CLIENT_ID(client_id,cb,0);
	
	LOG(("corba: get_system_info(cradle=%s,survival=%d,timeout=%d)",
	    cradle ,survival,timeout));
	
	req.type = GREQ_GET_SYSINFO;
	req.pilot_id = 0;
	req.timeout = survival==GNOME_Pilot_PERSISTENT?0:timeout;
	req.cradle = g_strdup (cradle);
	req.client_id = g_strdup(client_id);
	
	g_free(client_id);
	
	return gpc_queue_store_request(req);
}

static CORBA_unsigned_long
gpilotd_corba_set_user_info(GNOME_Pilot_Daemon obj,
			    const GNOME_Pilot_Client cb,
			    GNOME_Pilot_UserInfo *user,
			    const CORBA_char *cradle, 
			    const CORBA_boolean continue_sync,
			    const GNOME_Pilot_Survival survival,
			    const CORBA_unsigned_long timeout, 
			    CORBA_Environment *ev) 
{
	gchar *client_id;
	int result;

	GET_AND_CHECK_CLIENT_ID(client_id,cb,0);

	LOG(("corba: set_user_info(cradle=%s,survival=%d,timeout=%d",
	    cradle,survival,timeout));
	LOG(("corba:               device = %s,",cradle));
	LOG(("corba:               user_id = %d,",user->userID));
	LOG(("corba:               user    = %s)",user->username));
	/*LOG(("corba:               password= %s)",user->password);*/
	/* FIXME: check for cradle existance, throw exn if not */

	result = gpc_queue_store_set_userinfo_request (survival==GNOME_Pilot_PERSISTENT?0:timeout,
						       cradle,
						       client_id,
						       user->username,
						       user->userID,
						       continue_sync);

	g_free(client_id);

	return result;
}

#if 0
static CORBA_unsigned_long
gpilotd_corba_set_user_info_if_not_set(GNOME_Pilot_Daemon obj,
				       GNOME_Pilot_UserInfo *user,
				       const GNOME_Pilot_Client cb,
				       const CORBA_char *cradle, 
				       const GNOME_Pilot_Survival survival,
				       const CORBA_unsigned_long timeout, 
				       CORBA_Environment *ev) 
{
  
	GPilotRequest req;
	gchar *client_id;
	GET_AND_CHECK_CLIENT_ID(client_id,cb,0);

	LOG(("corba: set_user_info_if_not_set(cradle=%s,survival=%d,timeout=%d",
	    cradle,survival,timeout));
	LOG(("corba:               device = %s,",cradle));
	LOG(("corba:               client_id = %.20s...,",client_id));
	LOG(("corba:               user_id = %d,",user->userID));
	LOG(("corba:               user    = %s)",user->username));
	/*LOG(("corba:               password= %s)",user->password));*/

	/* FIXME: check for cradle existance, throw exn if not */

	req.type = GREQ_NEW_USERINFO;
	req.pilot_id = 0;
	req.timeout = survival==GNOME_Pilot_PERSISTENT?0:timeout;
	req.cradle = g_strdup(cradle);
	req.client_id = g_strdup(client_id);
	req.parameters.set_userinfo.password = NULL;
	req.parameters.set_userinfo.user_id = g_strdup(user->username);
	req.parameters.set_userinfo.pilot_id = user->userID;

	g_free(client_id);

	return gpc_queue_store_request(req);
}
#endif

static void
gpilotd_corba_monitor_on(GNOME_Pilot_Daemon obj,
			 const GNOME_Pilot_Client cb, 
			 const CORBA_char *pilot_name, 
			 CORBA_Environment *ev) 
{
	GSList **pilots;
	GSList *element;
	gchar *client_id;

#ifdef DEBUG_CODE
	{
		GNOME_Pilot_Client tmpcb;
		CORBA_Environment my_ev;
		gchar *tmp_client_id;

		CORBA_exception_init(&my_ev);
		tmp_client_id = CORBA_ORB_object_to_string(orb,cb,&my_ev);
		if(my_ev._major != CORBA_NO_EXCEPTION) {
			LOG(("*** Client appears to be disconnected..."));
			LOGEXN (my_ev);
		} else {
			LOG(("Client seems ok"));
			tmpcb = CORBA_ORB_string_to_object(orb,(CORBA_char*)tmp_client_id,&my_ev); 
			if(my_ev._major != CORBA_NO_EXCEPTION) {
				LOG(("**** Client appears to be disconnected..."));
				LOGEXN (my_ev);
				g_assert_not_reached();
			} else {
				LOG(("Client seems ok"));
			}
		}
	}
#endif

	GET_AND_CHECK_CLIENT_ID_VOID(client_id,cb);

	LOG(("monitor_on(pilot_name=\"%s\",client_id = %.20s...)",pilot_name,client_id));

	if(pilot_id_from_name(pilot_name,orb_context->gpilotd_context)==0) {
		g_free(client_id);
		gpthrow_no_val_with_settings(UnknownPilot,exn,exn->pilotId = CORBA_string_dup(pilot_name));
	}

	if((pilots = g_hash_table_lookup(orb_context->monitors,client_id))==NULL) {
		pilots = g_new0(GSList*,1);
		*pilots = g_slist_prepend(*pilots,g_strdup(pilot_name));
		g_hash_table_insert(orb_context->monitors,g_strdup(client_id),pilots);
	} else {
		element = g_slist_find_custom(*pilots,(gchar*)pilot_name,
					      (GCompareFunc)strcmp);
		*pilots = g_slist_prepend(*pilots,g_strdup(pilot_name));
	}

	g_free(client_id);
}

/* used by monitor_off_helper to remove callback IORs from a GSList* */
static void 
monitor_off_remover(GSList **list,
		    const gchar *client_id) 
{
	GSList *element;

	/* No, these will often be called with NULL pointers, so don't change these
	   to g_return..., too much output */
	if(list==NULL) return;
	if(*list==NULL) return;
	g_assert(client_id!=NULL);

	element = g_slist_find_custom(*list,(gpointer)client_id,(GCompareFunc)strcmp);
	if(element) {
		g_free(element->data);
		*list = g_slist_remove(*list,element->data); 
	}

	if(g_slist_length(*list)==0) {
		g_slist_free(*list);
		*list=NULL;
	} 
}

/* used by monitor_off to erase callback IORs from the 
   GNOME_Pilot_Orb_Pilot_Notifications object for the given pilot_id */
static void 
monitor_off_helper_2(const gchar *pilot,
		     const gchar* client_id) 
{
	GPilotd_Orb_Pilot_Notifications *notifications;

	g_assert(pilot!=NULL);
	g_assert(client_id!=NULL);

	LOG(("removing monitor on %s for %.10s",pilot,client_id));
	notifications = g_hash_table_lookup(orb_context->notifications,pilot);
	if(notifications==NULL) return;

	monitor_off_remover(&notifications->connect,client_id);
	monitor_off_remover(&notifications->disconnect,client_id);
	monitor_off_remover(&notifications->backup,client_id);
	monitor_off_remover(&notifications->conduit,client_id);
	monitor_off_remover(&notifications->request_complete,client_id);
	monitor_off_remover(&notifications->request_timeout,client_id);
	monitor_off_remover(&notifications->userinfo_requested,client_id);
	monitor_off_remover(&notifications->sysinfo_requested,client_id);
	monitor_off_remover(&notifications->userinfo_sent,client_id);

	if(notifications->connect == NULL &&
	   notifications->disconnect == NULL &&
	   notifications->backup == NULL &&
	   notifications->conduit == NULL &&
	   notifications->request_complete == NULL &&
	   notifications->request_timeout == NULL &&
	   notifications->userinfo_requested == NULL &&
	   notifications->sysinfo_requested == NULL &&
	   notifications->userinfo_sent == NULL) {
		gpointer key,elem;
 
		key = NULL; elem = NULL;
		g_hash_table_lookup_extended(orb_context->notifications,pilot,&key,&elem);
		g_hash_table_remove(orb_context->notifications,key);
		g_free(key);    
		g_free(elem);
	}
     
}

#ifdef DEBUG_CODE
void debug_monitors(gchar*, GSList**, gpointer); /* shuts up the compiler */
void debug_monitors(gchar *key, GSList **value, gpointer data) {
	GSList *e;
	GPilotd_Orb_Pilot_Notifications *nots;
	for (e=*value;e;e = g_slist_next(e)) {
		g_print("%s is monitoring %s",key,(char*)e->data);
		nots = g_hash_table_lookup(orb_context->notifications,e->data);
		if (nots==NULL)
			g_print (" - no notifications");
		else {
			g_print (" - ");
			if (nots->connect) g_print("connect,");
			if (nots->disconnect) g_print("disconnect,");
		}
		g_print("\n");
	}
}
#endif

static void 
monitor_off_helper(const gchar *pilot,
		   const gchar* client_id) 
{
	GSList **pilots;
	if((pilots = g_hash_table_lookup(orb_context->monitors,client_id))!=NULL) {
		GSList *element;
		if((element = g_slist_find_custom(*pilots,
						  (gpointer)pilot,
						  (GCompareFunc)strcmp))!=NULL) {
			/* random nuking in the notification lists */
			monitor_off_helper_2(pilot,client_id);

			/* nuke pilot from list */
			g_free(element->data);
			*pilots = g_slist_remove(*pilots,element->data);
			
			/* if pilots is empty now, nuke fom orb_context->monitors */
			if(g_slist_length(*pilots)==0) {
				gpointer key;
				gpointer elem;
				
				key = NULL; elem = NULL;
				g_hash_table_lookup_extended(orb_context->monitors,
							     client_id,
							     &key,
							     &elem);
				g_hash_table_remove(orb_context->monitors,(gchar *)key);
				g_free((gchar *)key);
				g_slist_free(*(GSList **)elem);
				g_free((GSList **)elem); 
			} 
		} else {
			GSList *e;
			/* This really shouldn't happen, dump list for debug purposes */
			LOG(("Could not find any link for %s",pilot));
			for(e=*pilots;e;e = g_slist_next(e)) {
				LOG(("pilots slist dump: %s",
				    e->data==NULL?"(null)":(char*)e->data));
			}			
		}
		
	} else {
		LOG(("couldn't find any monitor for %.10s",client_id));
	}
	

#ifdef DEBUG_CODE
	g_print("\n");
	g_hash_table_foreach(orb_context->monitors,(GHFunc)debug_monitors,NULL);
#endif
}

static void 
gpilotd_corba_monitor_off(GNOME_Pilot_Daemon obj,
			  const GNOME_Pilot_Client cb, 
			  const CORBA_char *pilot_name, 
			  CORBA_Environment *ev) 
{
	gchar *client_id;

	GET_AND_CHECK_CLIENT_ID_VOID(client_id,cb);

	LOG(("monitor_off(pilot_name=\"%.10s\", client_id = %.20s...)",pilot_name,client_id));
	if(pilot_id_from_name(pilot_name,orb_context->gpilotd_context)==0) {
		g_free(client_id);
		gpthrow_no_val_with_settings(UnknownPilot,exn,exn->pilotId = CORBA_string_dup(pilot_name));
	}
	monitor_off_helper(pilot_name,client_id);

	g_free(client_id);
}

static void 
notify_on_helper(gchar *pilot,
		 GPilotd_notify_on_helper_foreach_carrier *carrier) 
{
	GPilotd_Orb_Pilot_Notifications *notifications;

	notifications = g_hash_table_lookup(orb_context->notifications,pilot);

	if(notifications==NULL) {    
		notifications = g_new0(GPilotd_Orb_Pilot_Notifications,1);
		g_hash_table_insert(orb_context->notifications,g_strdup(pilot),notifications);
	}
	switch(carrier->event) {
	case GNOME_Pilot_NOTIFY_CONNECT: 
		notifications->connect = g_slist_prepend(notifications->connect,
							 g_strdup(carrier->client_id));
		break;
	case GNOME_Pilot_NOTIFY_DISCONNECT: 
		notifications->disconnect = g_slist_prepend(notifications->disconnect,
							    g_strdup(carrier->client_id));
		break;
	case GNOME_Pilot_NOTIFY_BACKUP: 
		notifications->backup = g_slist_prepend(notifications->backup,
							g_strdup(carrier->client_id));
		break;
	case GNOME_Pilot_NOTIFY_CONDUIT: 
		notifications->conduit = g_slist_prepend(notifications->conduit,
							 g_strdup(carrier->client_id));
		break;
	case GNOME_Pilot_NOTIFY_REQUEST_COMPLETION: 
		notifications->request_complete = g_slist_prepend(notifications->request_complete,
								  g_strdup(carrier->client_id));
		break;
	case GNOME_Pilot_NOTIFY_REQUEST_TIMEOUT: 
		notifications->request_timeout = g_slist_prepend(notifications->request_timeout,
								 g_strdup(carrier->client_id));
		break;
	case GNOME_Pilot_NOTIFY_USERINFO_REQUESTED: 
		/* FIXME: what was this request to do ? notify when the info was requested or when returned ? */
		/*notifications->userinfo_requested = g_slist_prepend(notifications->connect,g_strdup(carrier->client_id)); */
		break;
	case GNOME_Pilot_NOTIFY_SYSINFO_REQUESTED: 
		/* FIXME: what was this request to do ? notify when the info was requested or when returned ? */
		/*notifications->sysinfo_requestde = g_slist_prepend(notifications->connect,g_strdup(carrier->client_id));*/
		break;
	case GNOME_Pilot_NOTIFY_USERINFO_SENT: 
		notifications->userinfo_sent = g_slist_prepend(notifications->userinfo_sent,
							       g_strdup(carrier->client_id));
		break;
	default:
		g_assert_not_reached();
		break;
	}
}

static void
gpilotd_corba_notify_on(GNOME_Pilot_Daemon obj, 
			GNOME_Pilot_EventType event, 
			const GNOME_Pilot_Client cb, 
			CORBA_Environment * ev) 
{
	GSList **pilots;
	GPilotd_notify_on_helper_foreach_carrier carrier;
	gchar *client_id;

	GET_AND_CHECK_CLIENT_ID_VOID(client_id,cb);

	{
		gchar *event_type = NULL;
		switch(event) {
		case GNOME_Pilot_NOTIFY_CONNECT: 
			event_type = g_strdup("CONNECT"); break;
		case GNOME_Pilot_NOTIFY_DISCONNECT: 
			event_type = g_strdup("DISCONNECT"); break;
		case GNOME_Pilot_NOTIFY_BACKUP: 
			event_type = g_strdup("BACKUP"); break;
		case GNOME_Pilot_NOTIFY_CONDUIT: 
			event_type = g_strdup("CONDUIT"); break;
		case GNOME_Pilot_NOTIFY_REQUEST_COMPLETION: 
			event_type = g_strdup("REQUEST_COMPLETION"); break;
		case GNOME_Pilot_NOTIFY_REQUEST_TIMEOUT: 
			event_type = g_strdup("REQUEST_TIMEOUT"); break;
		case GNOME_Pilot_NOTIFY_USERINFO_REQUESTED: 
			event_type = g_strdup("USERINFO_REQUESTED"); break;
		case GNOME_Pilot_NOTIFY_USERINFO_SENT: 
			event_type = g_strdup("USERINFO_SENT"); break;
		default: g_assert_not_reached(); break;
		}
		LOG(("corba: notify_on(event_type=%s,callback=%.20s...)",
		    event_type,client_id));
		g_free(event_type);
	}

	if((pilots = g_hash_table_lookup(orb_context->monitors,client_id))==NULL) {
		g_free(client_id);
		gpthrow_no_val(NoMonitors);
	}
	carrier.client_id = client_id;
	carrier.event = event;
	g_slist_foreach(*pilots,(GFunc)notify_on_helper,(gpointer)&carrier);

	g_free(client_id);
};

static void
gpilotd_corba_notify_off(GNOME_Pilot_Daemon obj, 
			 GNOME_Pilot_EventType event, 
			 const GNOME_Pilot_Client cb, 
			 CORBA_Environment * ev) 
{
	GSList **pilots;
	GPilotd_notify_on_helper_foreach_carrier carrier;
	gchar *client_id;

	GET_AND_CHECK_CLIENT_ID_VOID(client_id,cb);

	{
		gchar *event_type = NULL;
		switch(event) {
		case GNOME_Pilot_NOTIFY_CONNECT: 
			event_type = g_strdup("CONNECT"); break;
		case GNOME_Pilot_NOTIFY_DISCONNECT: 
			event_type = g_strdup("DISCONNECT"); break;
		case GNOME_Pilot_NOTIFY_BACKUP: 
			event_type = g_strdup("BACKUP"); break;
		case GNOME_Pilot_NOTIFY_CONDUIT: 
			event_type = g_strdup("CONDUIT"); break;
		case GNOME_Pilot_NOTIFY_REQUEST_COMPLETION: 
			event_type = g_strdup("REQUEST_COMPLETION"); break;
		case GNOME_Pilot_NOTIFY_REQUEST_TIMEOUT: 
			event_type = g_strdup("REQUEST_TIMEOUT"); break;
		case GNOME_Pilot_NOTIFY_USERINFO_REQUESTED: 
			event_type = g_strdup("USERINFO_REQUESTED"); break;
		case GNOME_Pilot_NOTIFY_USERINFO_SENT: 
			event_type = g_strdup("USERINFO_SENT"); break;
		default: g_assert_not_reached(); break;
		}
		LOG(("corba: notify_off(event_type=%s,callback=%.20s...)",
		    event_type,client_id));
		g_free(event_type);
	}

	if((pilots = g_hash_table_lookup(orb_context->monitors,client_id))==NULL) {
		g_free(client_id);
		gpthrow_no_val(NoMonitors);
	}

	carrier.client_id = client_id;
	carrier.event = event;
	g_warning("Unimplemented method");
	/*
	g_slist_foreach(*pilots,(GFunc)notify_on_helper,(gpointer)&carrier);
	*/
	g_free(client_id);
};


/* get_* methods */
static GNOME_Pilot_StringSequence*
gpilotd_corba_get_users(GNOME_Pilot_Daemon obj, 
			CORBA_Environment *ev) 
{

	LOG(("corba: get_users()"));

	/* FIXME: this isn't complete, it only gives one user, and
	   doesn't make any NULL checks... */
	if(orb_context->gpilotd_context->user) {
		GNOME_Pilot_StringSequence *users;
		users = GNOME_Pilot_StringSequence__alloc();
		users->_length = 1;
		users->_buffer = CORBA_sequence_CORBA_string_allocbuf(users->_length);
		users->_buffer[0] = CORBA_string_alloc(strlen(orb_context->gpilotd_context->user->username));
		strcpy(users->_buffer[0],orb_context->gpilotd_context->user->username);
		return users;
	} else 
		return empty_StringSequence();
}

static GNOME_Pilot_StringSequence*
gpilotd_corba_get_cradles(GNOME_Pilot_Daemon obj, 
			  CORBA_Environment *ev) 
{

	LOG(("corba: get_cradles(...)"));

	if(g_list_length(orb_context->gpilotd_context->devices)>0) {
		GNOME_Pilot_StringSequence *cradles;
		int i;

		cradles = GNOME_Pilot_StringSequence__alloc();
		cradles->_length = g_list_length(orb_context->gpilotd_context->devices);
		cradles->_buffer = CORBA_sequence_CORBA_string_allocbuf(cradles->_length);

		/* FIXME: this could probably be prettyfied with some _foreach thingies... */
		for(i=0;i<g_list_length(orb_context->gpilotd_context->devices);i++) {
			cradles->_buffer[i] = CORBA_string_alloc(strlen(GPILOT_DEVICE(g_list_nth(orb_context->gpilotd_context->devices,i)->data)->name));
			strcpy(cradles->_buffer[i],GPILOT_DEVICE(g_list_nth(orb_context->gpilotd_context->devices,i)->data)->name);
		}
		return cradles;
	} else 
		return empty_StringSequence();
}

static GNOME_Pilot_StringSequence*
gpilotd_corba_get_pilots(GNOME_Pilot_Daemon obj, 
			 CORBA_Environment *ev) 
{

	LOG(("corba: get_pilots(...)"));

	if(g_list_length(orb_context->gpilotd_context->pilots)>0) {
		GNOME_Pilot_StringSequence *pilots;
		int i;

		pilots = GNOME_Pilot_StringSequence__alloc();
		pilots->_length = g_list_length(orb_context->gpilotd_context->pilots);
		pilots->_buffer = CORBA_sequence_CORBA_string_allocbuf(pilots->_length);

		/* FIXME: this could probably be prettyfied with some _foreach thingies... */
		for(i=0;i<g_list_length(orb_context->gpilotd_context->pilots);i++) {
			pilots->_buffer[i] = CORBA_string_alloc(strlen(GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->name));
			strcpy(pilots->_buffer[i],GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->name);
		}
		return pilots;
	} else 
		return empty_StringSequence();
}

static GNOME_Pilot_LongSequence*
gpilotd_corba_get_pilot_ids(GNOME_Pilot_Daemon obj, 
			    CORBA_Environment *ev) 
{

	LOG(("corba: get_pilot_ids(...)"));

	if(g_list_length(orb_context->gpilotd_context->pilots)>0) {
		GNOME_Pilot_LongSequence *pilots;
		int i;
		
		pilots = GNOME_Pilot_LongSequence__alloc();
		pilots->_length = g_list_length(orb_context->gpilotd_context->pilots);
		pilots->_buffer = CORBA_sequence_CORBA_long_allocbuf(pilots->_length);

		/* FIXME: this could probably be prettyfied with some _foreach thingies... */
		for(i=0;i<g_list_length(orb_context->gpilotd_context->pilots);i++) {
			pilots->_buffer[i] = GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->pilot_id;
		}
		return pilots;
	} else 
		return empty_LongSequence();
}

static GNOME_Pilot_StringSequence*
gpilotd_corba_get_pilots_by_name(GNOME_Pilot_Daemon obj, 
				 const CORBA_char * user, 
				 CORBA_Environment *ev) 
{
	int cnt;

	LOG(("corba: get_pilots_by_name(%s)",user));

	/* FIXME: this could probably be prettyfied with some _foreach thingies... */
	if(g_list_length(orb_context->gpilotd_context->pilots)>0) {
		GNOME_Pilot_StringSequence *pilots;
		int i;
		LOG(("g_list_length(orb_context->gpilotd_context->pilots) = %d",g_list_length(orb_context->gpilotd_context->pilots)));

		/* first count the number of pilots to cp */
		cnt=0;
		/* FIXME: this could probably be prettyfied with some _foreach thingies... */
		for(i=0;i<g_list_length(orb_context->gpilotd_context->pilots);i++) {
			if(strcmp(GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->pilot_username,
				  user)==0) {
				LOG(("match on %s",GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->name));
				cnt++;
			}
		}
		if(cnt==0) {
			LOG(("no matches on %s",user));
			return empty_StringSequence(); 
		}

		/* alloc and copy intro string */
		pilots = GNOME_Pilot_StringSequence__alloc();
		pilots->_length = cnt;
		pilots->_buffer = CORBA_sequence_CORBA_string_allocbuf(pilots->_length);
		/* FIXME: this could probably be prettyfied with some _foreach thingies... */
		for(i=0;i<g_list_length(orb_context->gpilotd_context->pilots);i++) {
			if(strcmp(GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->pilot_username,
				  user)==0) {
				pilots->_buffer[i] = CORBA_string_alloc(strlen(GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->name));
				strcpy(pilots->_buffer[i],GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->name);
			}
		}
		return pilots;
	} else 
		return empty_StringSequence();
}

static GNOME_Pilot_StringSequence*
gpilotd_corba_get_pilots_by_login(GNOME_Pilot_Daemon obj, 
				  const CORBA_char * uid, 
				  CORBA_Environment *ev) 
{
	gchar *username;
	struct passwd *pwdent;
	GNOME_Pilot_StringSequence *pilots;

	LOG(("corba: get_pilots_by_login(%s)",uid));

	/* find the pwdent matching the login */
	username = NULL;
	setpwent();
	do {
		pwdent = getpwent();
		if(pwdent) {
			if(strcmp(pwdent->pw_name,uid)==0) {
				username = strdup(pwdent->pw_gecos);
				pwdent=NULL; /* end the loop */
			}
		}
	} while(pwdent);
	endpwent();

	/* no luck ? */
	if(!username) {
		LOG(("no realname for %s",uid));
		return empty_StringSequence();
	}

	/* FIXME: uhm, is this use of username safe ? 
	   or should it be CORBA::strdup'ed ? */
	pilots = gpilotd_corba_get_pilots_by_name(obj,username,ev);
	g_free(username);
	return pilots;
}

static CORBA_char*
gpilotd_corba_get_user_name_by_pilot(GNOME_Pilot_Daemon obj, 
				     const CORBA_char * pilot_name, 
				     CORBA_Environment *ev) 
{
	LOG(("corba: FIXME %s:%d get_user_name_by_pilot", __FILE__, __LINE__));

	/* FIXME: not implemented yet */
	return CORBA_string_dup ("Foo Bar");
}

static CORBA_char*
gpilotd_corba_get_user_login_by_pilot(GNOME_Pilot_Daemon obj, 
				      const CORBA_char * pilot_name, 
				      CORBA_Environment *ev) 
{
	LOG(("corba: FIXME %s:%d get_user_login_by_pilot", __FILE__, __LINE__));

	/* FIXME: not implemented yet */
	return CORBA_string_dup ("foo");
}

static CORBA_char* 
gpilotd_get_pilot_base_dir(PortableServer_Servant _servant, 
		     CORBA_char * pilot_name, 
		     CORBA_Environment * ev) {
	int i;
	LOG(("corba: get_pilot_base_dir(id=%s)",pilot_name));
	

	/* FIXME: this could probably be prettyfied with some _foreach thingies... */
	if(g_list_length(orb_context->gpilotd_context->pilots)>0) {
		/* FIXME: this could probably be prettyfied with some _foreach thingies... */
		for(i=0;i<g_list_length(orb_context->gpilotd_context->pilots);i++) {
			if(strcmp(GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->name,
				  pilot_name)==0) {
				LOG(("match on %s",GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->name));
				if(GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->sync_options.basedir!=NULL) {
					return CORBA_string_dup(GPILOT_PILOT(g_list_nth(orb_context->gpilotd_context->pilots,i)->data)->sync_options.basedir);
				} else {
					return CORBA_string_dup("");
				}
			}
		}
		LOG(("no matches on %s",pilot_name));
		return CORBA_string_dup("");
	} else 
		return CORBA_string_dup("");
}

static CORBA_long 
gpilotd_get_pilot_id_from_name(PortableServer_Servant _servant, 
			       CORBA_char * pilot_name, 
			       CORBA_Environment * ev) {
	LOG(("corba: get_user_pilot_id_from_name"));

	return pilot_id_from_name(pilot_name,orb_context->gpilotd_context);
}

static CORBA_char* 
gpilotd_get_pilot_name_from_id(PortableServer_Servant _servant, 
			       CORBA_long pilot_name, 
			       CORBA_Environment * ev) {
	gchar *tmp;
	CORBA_char *retval;
	LOG(("corba: get_pilot_name_from_id(id=%d)",pilot_name));

	tmp = pilot_name_from_id(pilot_name,orb_context->gpilotd_context);
	retval =  CORBA_string_dup(tmp);
	g_free(tmp);

	return retval;
}

static GNOME_Pilot_StringSequence*
gpilotd_corba_get_databases_from_cache(GNOME_Pilot_Daemon obj, 
				       const CORBA_char *pilot_name,
				       CORBA_Environment *ev) 
{
	char **databases,*pfx;
	int num_bases;
	guint32 pilot_id;
	LOG(("corba: get_databases_from_cache(...)"));

	pilot_id = pilot_id_from_name(pilot_name,
				      orb_context->gpilotd_context);

	/* If no pilot matches the name, set the exn, and exit with a empty stringsequence */
	if (pilot_id == 0) {
		/* gpthrow_no_val_with_settings(UnknownPilot,exn,exn->pilotId = CORBA_string_dup(pilot_name)); */
		return empty_StringSequence();
		return NULL;
	} else {
		pfx = g_strdup_printf("/gnome-pilot.d/PilotCache%d/Databases/",
				      pilot_id);
		gnome_config_push_prefix(pfx);
		gnome_config_get_vector("databases",&num_bases,&databases);	
		gnome_config_pop_prefix();
		
		/* if there were databases, copy into a corba array
                   and return, otherwise return an empty array */
		if(num_bases > 0) {
			GNOME_Pilot_StringSequence *pilots;
			int i;
			
			pilots = GNOME_Pilot_StringSequence__alloc();
			pilots->_length = num_bases;
			pilots->_buffer = CORBA_sequence_CORBA_string_allocbuf(pilots->_length);
			
			/* FIXME: this could probably be prettyfied
                           with some _foreach thingies... */
			for(i = 0; i < num_bases; i++) {
				pilots->_buffer[i] = CORBA_string_alloc(strlen(databases[i]));
				strcpy(pilots->_buffer[i],databases[i]);
				g_free(databases[i]);
			}
			g_free(databases);
			return pilots;
		} else {
			return empty_StringSequence();
		}
	}
}



/*************************************************************************************/
static gint 
match_pilot_and_name(const GPilotPilot *pilot,
		     const gchar *name) 
{
	if(pilot) {
		return g_strcasecmp(name,pilot->name);
	}
	return -1;
}

gint 
match_pilot_userID(GPilotPilot *p,
		   guint32 *id)
{
	if(p->pilot_id == *id)
		return 0;
	return -1;
}

guint32 
pilot_id_from_name(const gchar *name,
		   GPilotContext *context) 
{
	GList *pilot;
	pilot = g_list_find_custom(context->pilots,(gpointer)name,
				   (GCompareFunc)match_pilot_and_name);
	if(pilot)
		return ((GPilotPilot*)pilot->data)->pilot_id;
	return 0;
}

gchar*
pilot_name_from_id(guint32 id,
		   GPilotContext *context) 
{
	GList *pilot;
	pilot = g_list_find_custom(context->pilots,(gpointer)&id,
				   (GCompareFunc)match_pilot_userID);

	if(pilot)
		return g_strdup(((GPilotPilot*)pilot->data)->name);

	return NULL;
}


/*****************************************************/
/* orbit init stuff */

PortableServer_ServantBase__epv base_epv = {
	NULL,
	NULL,
	NULL
};

POA_GNOME_Pilot_Daemon__epv daemon_epv = { 
	NULL, 
	(gpointer)&gpilotd_corba_pause,
	(gpointer)&gpilotd_corba_reread_config,
	(gpointer)&gpilotd_corba_noop,
	(gpointer)&gpilotd_corba_request_install,
	(gpointer)&gpilotd_corba_request_restore,
	(gpointer)&gpilotd_corba_request_conduit,
	(gpointer)&gpilotd_corba_request_remove,
	(gpointer)&gpilotd_corba_get_system_info,
	(gpointer)&gpilotd_corba_get_users,
	(gpointer)&gpilotd_corba_get_cradles,
	(gpointer)&gpilotd_corba_get_pilots,
	(gpointer)&gpilotd_corba_get_pilot_ids,
	(gpointer)&gpilotd_corba_get_pilots_by_name,
	(gpointer)&gpilotd_corba_get_pilots_by_login,
	(gpointer)&gpilotd_corba_get_user_name_by_pilot,
	(gpointer)&gpilotd_corba_get_user_login_by_pilot,
	(gpointer)&gpilotd_get_pilot_base_dir,
	(gpointer)&gpilotd_get_pilot_id_from_name,
	(gpointer)&gpilotd_get_pilot_name_from_id,
	(gpointer)&gpilotd_corba_get_databases_from_cache,
	(gpointer)&gpilotd_corba_get_user_info,
	(gpointer)&gpilotd_corba_set_user_info,
	(gpointer)&gpilotd_corba_monitor_on,
	(gpointer)&gpilotd_corba_monitor_off,
	(gpointer)&gpilotd_corba_notify_on,
	(gpointer)&gpilotd_corba_notify_off,
};

POA_GNOME_Pilot_Daemon__vepv daemon_vepv = { &base_epv, &daemon_epv };
POA_GNOME_Pilot_Daemon daemon_servant = { NULL, &daemon_vepv };

/********************************************************************************/
/* This function is used by orbed_notify_connect/disconnect to
   remove all clients that have been marked as dead.                            */

static void 
purge_ior_foreach(const gchar *pilot,
		  GSList *list) 
{
	int i;
	gchar *ior;
	GSList *e;

	if(list==NULL || g_slist_length(list)==0) return;
	g_return_if_fail(pilot);
  
	for(i=0;i<g_slist_length(list);i++) {
		e = g_slist_nth(list,i);
		g_assert(e);
		g_assert(e->data);
		ior = e->data;
		monitor_off_helper(pilot,ior); 
	}
	for(i=0;i<g_slist_length(list);i++) {
		e = g_slist_nth(list,i);
		g_free(e->data);
	}
	g_slist_free(list);  
}

/********************************************************************************/
/*
                                                                                */
static void 
orbed_notify_multi_purpose_foreach(gchar *IOR,
				   GPilotd_notify_foreach_carrier *carrier) 
{
	GNOME_Pilot_Client cb;
	
	g_return_if_fail(IOR!=NULL);
	g_assert(strncmp(IOR,"IOR:",4)==0);

	cb = CORBA_ORB_string_to_object(orb,(CORBA_char*)IOR,&ev);
	if(ev._major != CORBA_NO_EXCEPTION) {
		LOG(("Client appears to be disconnected..."));
		g_warning ("%s:%d Exception: %s\n",
			   __FILE__,__LINE__, CORBA_exception_id (&ev));
		CORBA_exception_free(&ev);
		carrier->purge_list = g_slist_prepend(carrier->purge_list,
						      g_strdup(IOR));  
	} else {
		switch(carrier->action) {
		case CALL_CONNECT:
			GNOME_Pilot_Client_connect(cb, 
						   carrier->id, 
						   carrier->ptr1,&ev);
			break;
		case CALL_DISCONNECT:
			GNOME_Pilot_Client_disconnect(cb, 
						      carrier->id, &ev);
			break;
		case CALL_CONDUIT_MESSAGE:
			GNOME_Pilot_Client_conduit_message(cb, 
							   carrier->id, 
							   carrier->ptr1, 
							   carrier->ptr2, &ev);
			break;
		case CALL_DAEMON_MESSAGE:
			GNOME_Pilot_Client_daemon_message(cb, 
							  carrier->id, 
							  carrier->ptr1 ? carrier->ptr1 : "", 
							  carrier->ptr2, &ev);
			break;
		case CALL_DAEMON_ERROR:
			GNOME_Pilot_Client_daemon_error (cb, 
							 carrier->id, 
							 carrier->ptr2, &ev);
			break;
		case CALL_CONDUIT_ERROR:
			GNOME_Pilot_Client_conduit_error(cb, 
							 carrier->id, 
							 carrier->ptr1, 
							 carrier->ptr2, &ev);
			break;
		case CALL_CONDUIT_PROGRESS:
			GNOME_Pilot_Client_conduit_progress(cb, 
							    carrier->id, 
							    carrier->ptr1, 
							    carrier->long1,
							    carrier->long2, &ev);
			break;
		case CALL_OVERALL_PROGRESS:
			GNOME_Pilot_Client_overall_progress(cb, 
							    carrier->id, 
							    carrier->long1,
							    carrier->long2, &ev);
			break;
		case CALL_CONDUIT_BEGIN:
			GNOME_Pilot_Client_conduit_start(cb, 
							 carrier->id, 
							 carrier->ptr1, 
							 carrier->ptr2, &ev);
			break;
		case CALL_CONDUIT_END:
			GNOME_Pilot_Client_conduit_end(cb, 
						       carrier->id,
						       carrier->ptr1, &ev);
			break;
		}
		if(ev._major != CORBA_NO_EXCEPTION) {
			LOG(("Client appears to be disconnected..."));
			LOGEXN (ev);
			CORBA_exception_free(&ev);
			carrier->purge_list = g_slist_prepend(carrier->purge_list,
							      g_strdup(IOR)); 
			return; 
		  

		}    
		CORBA_Object_release(cb,&ev);
		CORBA_exception_free(&ev);
	}
}

/********************************************************************************/
/*
                                                                                */
void 
orbed_notify_connect(const gchar *pilot_id, 
		     struct PilotUser user_info) 
{
	GNOME_Pilot_UserInfo user_info_orbed;
	GPilotd_notify_foreach_carrier carrier;
	GPilotd_Orb_Pilot_Notifications *notifications;

	if((notifications = g_hash_table_lookup(orb_context->notifications,pilot_id))==NULL) {
		return;
	}

	user_info_orbed.userID = user_info.userID;
	user_info_orbed.username = g_strdup(user_info.username);

	carrier.id = g_strdup(pilot_id);
	carrier.ptr1 = &user_info_orbed;
	carrier.purge_list = NULL;
	carrier.action = CALL_CONNECT;
#ifdef DEBUG_CODE
	LOG(("corba: orbed_notify_user, notifications->connect.size = %d, id = %s",
	    g_slist_length(notifications->connect),pilot_id));
#endif
	g_slist_foreach(notifications->connect,
			(GFunc)orbed_notify_multi_purpose_foreach,
			&carrier);

	/* FIXME: See note 1 at the end of this file */
	if(carrier.purge_list)
		purge_ior_foreach(pilot_id,carrier.purge_list);

	g_free(user_info_orbed.username);
	g_free(carrier.id);
}

/********************************************************************************/
/*
                                                                                */
void 
orbed_notify_disconnect(const gchar *pilot_id) 
{
	GPilotd_notify_foreach_carrier carrier;
	GPilotd_Orb_Pilot_Notifications *notifications;

	g_return_if_fail(pilot_id!=NULL);
	if((notifications = g_hash_table_lookup(orb_context->notifications,pilot_id))==NULL) {
		return;
	}
	carrier.id = g_strdup(pilot_id);
	carrier.purge_list = NULL;
	carrier.action = CALL_DISCONNECT;
#ifdef DEBUG_CODE
	LOG(("corba: orbed_notify_user, notifications->disconnect.size = %d, id = %s",
	    g_slist_length(notifications->disconnect),pilot_id));
#endif
	g_slist_foreach(notifications->disconnect,
			(GFunc)orbed_notify_multi_purpose_foreach,
			&carrier);

	/* FIXME: See note 1 at the end of this file */
	if(carrier.purge_list)
		purge_ior_foreach(pilot_id,carrier.purge_list);

	g_free(carrier.id);
}

/********************************************************************************/
/*
                                                                                */
void 
orbed_notify_completion(GPilotRequest **req) 
{
	GNOME_Pilot_Client cb;
	gchar *pilot_name;

	LOG(("orbed_notify_completion(...)"));

	g_return_if_fail(req!=NULL);
	g_return_if_fail(*req!=NULL);

	/* If there is no client to notify, just purge request and return */
	if((*req)->client_id==NULL) {
		LOG(("%s: no client_id in request",G_GNUC_PRETTY_FUNCTION));
		gpc_queue_purge_request(req);
		return;
	}
	LOG(("%s: client_id = %.20s",G_GNUC_PRETTY_FUNCTION,(*req)->client_id));

	/* check the client id is a IOR */
	g_return_if_fail(strncmp((*req)->client_id,"IOR:",4)==0);

	/* resolve pilot id */
	pilot_name = pilot_name_from_id((*req)->pilot_id,orb_context->gpilotd_context);

	/* get the ior, if it fails, print exception and purge request */
	cb = CORBA_ORB_string_to_object(orb,(CORBA_char*)(*req)->client_id,&ev);
	if(ev._major != CORBA_NO_EXCEPTION) {
		LOG(("unable to resolve object for IOR"));
		LOGEXN (ev);
		CORBA_exception_free(&ev);
		/* FIXME: This purges monitors for the client.
		   Note that only monitors on the pilot that just synced are purged,
		   this can be considered a leak (if the pilot never syncs). */
		if (pilot_name != NULL)
			monitor_off_helper(pilot_name,(*req)->client_id);
		gpc_queue_purge_request(req);
		return;
	}

	/* otherwise return notification and purge */	
	GNOME_Pilot_Client_request_completed(cb, 
					     pilot_name==NULL?"":pilot_name, 
					     (*req)->handle,&ev);
	if(ev._major != CORBA_NO_EXCEPTION) {
		LOGEXN (ev);
		CORBA_exception_free(&ev);
		/* FIXME: This purges monitors for the client.
		   Note that only monitors on the pilot that just synced are purged,
		   this can be considered a leak (if the pilot never syncs). */
		if (pilot_name != NULL)
			monitor_off_helper(pilot_name,(*req)->client_id);
		gpc_queue_purge_request(req);
		return;
	}
	CORBA_Object_release(cb,&ev);
	CORBA_exception_free(&ev);
	gpc_queue_purge_request(req);
	g_free(pilot_name);
}

/********************************************************************************/
/*
                                                                                */
void 
orbed_notify_userinfo(struct PilotUser user_info,
		      GPilotRequest **req) 
{
	GNOME_Pilot_UserInfo user;
	GNOME_Pilot_Client cb;

	g_return_if_fail(req!=NULL);
	g_return_if_fail(*req!=NULL);
	if((*req)->client_id==NULL) return;
	g_return_if_fail(strncmp((*req)->client_id,"IOR:",4)==0);

	cb = CORBA_ORB_string_to_object(orb,(CORBA_char*)(*req)->client_id,&ev);

	if(ev._major != CORBA_NO_EXCEPTION) {
		LOGEXN (ev);
		CORBA_exception_free(&ev);
		return;
	}

	user.userID = user_info.userID;
	user.username = g_strdup(user_info.username);
  
	GNOME_Pilot_Client_userinfo_requested(cb, (*req)->cradle, &user,&ev);
	if(ev._major != CORBA_NO_EXCEPTION) {
		LOGEXN (ev);
		CORBA_exception_free(&ev);
		return;
	}
	CORBA_Object_release(cb,&ev);
	CORBA_exception_free(&ev);
  
	g_free(user.username);
}

/********************************************************************************/
/*
                                                                                */
void 
orbed_notify_sysinfo(gchar *pilot_id,
		     struct SysInfo sysinfo,
		     struct CardInfo cardinfo,
		     GPilotRequest **req) 
{
	GNOME_Pilot_SysInfo _sysinfo;
	GNOME_Pilot_Client cb;

	g_assert(strncmp((*req)->client_id,"IOR:",4)==0);

	cb = CORBA_ORB_string_to_object(orb,(CORBA_char*)(*req)->client_id,&ev);

	if(ev._major != CORBA_NO_EXCEPTION) {
		LOGEXN (ev);
		CORBA_exception_free(&ev);
		return;
	}
/*
	g_message ("sysinfo.romVersion = %lu", sysinfo.romVersion);
	g_message ("sysinfo.name = %s", sysinfo.name);
	g_message ("cardinfo.version = %d", cardinfo.version);
	g_message ("cardinfo.creation = %lu", cardinfo.creation);
	g_message ("cardinfo.name = %s", cardinfo.name);
	g_message ("cardinfo.manufacturer = %s", cardinfo.manufacturer);
*/
	_sysinfo.romSize = cardinfo.romSize/1024;
	_sysinfo.ramSize = cardinfo.ramSize/1024;
	_sysinfo.ramFree = cardinfo.ramFree/1024;
	_sysinfo.name = g_strdup(cardinfo.name);
	_sysinfo.manufacturer = g_strdup(cardinfo.manufacturer);
	_sysinfo.creation = cardinfo.creation;
	_sysinfo.romVersion = sysinfo.romVersion;

	GNOME_Pilot_Client_sysinfo_requested(cb, pilot_id,&_sysinfo,&ev);
	if(ev._major != CORBA_NO_EXCEPTION) {
		LOGEXN (ev);
		CORBA_exception_free(&ev);
		return;
	}
	CORBA_Object_release(cb,&ev);
	CORBA_exception_free(&ev);
}

/********************************************************************************/
/*
                                                                                */
void 
orbed_notify_timeout(guint handle) {
	/* FIXME: implement me ! */
}


/********************************************************************************/
/*
                                                                                */
void 
orbed_notify_conduit_start(gchar *pilot_id,
			   GnomePilotConduit *conduit,
			   GnomePilotConduitSyncType synctype) {
	GPilotd_notify_foreach_carrier carrier;
	GPilotd_Orb_Pilot_Notifications *notifications;
	
	if((notifications = g_hash_table_lookup(orb_context->notifications,pilot_id))==NULL) {
		return;
	}

	carrier.id = g_strdup(pilot_id);
	carrier.ptr1 = gnome_pilot_conduit_get_name(conduit);
	if(GNOME_IS_PILOT_CONDUIT_STANDARD(conduit))
		carrier.ptr2 = g_strdup(gnome_pilot_conduit_standard_get_db_name(GNOME_PILOT_CONDUIT_STANDARD(conduit)));
	else
		carrier.ptr2 = g_strdup(_("(unknown DB)"));
	carrier.purge_list = NULL;
	carrier.action = CALL_CONDUIT_BEGIN;
	g_slist_foreach(notifications->conduit,
			(GFunc)orbed_notify_multi_purpose_foreach,
			&carrier);
	/* FIXME: See note 1 at the end of this file */
	if(carrier.purge_list) purge_ior_foreach(pilot_id,carrier.purge_list);
	g_free(carrier.id);
	g_free(carrier.ptr1);
	g_free(carrier.ptr2);
}

/********************************************************************************/
/*
                                                                                */
void 
orbed_notify_conduit_end(gchar *pilot_id,
			 GnomePilotConduit *conduit) {
	GPilotd_notify_foreach_carrier carrier;
	GPilotd_Orb_Pilot_Notifications *notifications;
	
	if((notifications = g_hash_table_lookup(orb_context->notifications,pilot_id))==NULL) {
		return;
	}

	carrier.id = g_strdup(pilot_id);
	carrier.ptr1 = gnome_pilot_conduit_get_name(conduit);
	carrier.purge_list = NULL;
	carrier.action = CALL_CONDUIT_END;
	g_slist_foreach(notifications->conduit,
			(GFunc)orbed_notify_multi_purpose_foreach,
			&carrier);
	/* FIXME: See note 1 at the end of this file */
	if(carrier.purge_list) purge_ior_foreach(pilot_id,carrier.purge_list);
	g_free(carrier.id);
	g_free(carrier.ptr1);
}

/********************************************************************************/
/*
                                                                                */
void 
orbed_notify_conduit_error(gchar *pilot_id,				
			   GnomePilotConduit *conduit,
			   gchar *message) {
	GPilotd_notify_foreach_carrier carrier;
	GPilotd_Orb_Pilot_Notifications *notifications;
	
	g_assert (pilot_id);
	g_assert (message);

	if((notifications = g_hash_table_lookup(orb_context->notifications,pilot_id))==NULL) {
		return;
	}

	carrier.id = g_strdup(pilot_id);
	carrier.ptr1 = gnome_pilot_conduit_get_name(conduit);
	carrier.ptr2 = message;
	carrier.purge_list = NULL;
	carrier.action = CALL_CONDUIT_ERROR;
	g_slist_foreach(notifications->conduit,
			(GFunc)orbed_notify_multi_purpose_foreach,
			&carrier);
	/* FIXME: See note 1 at the end of this file */
	if(carrier.purge_list) purge_ior_foreach(pilot_id,carrier.purge_list);
	g_free(carrier.id);
	g_free(carrier.ptr1);
}

/********************************************************************************/
/*
                                                                                */
void 
orbed_notify_conduit_message(gchar *pilot_id,				
			     GnomePilotConduit *conduit,
			     gchar *message) {
	GPilotd_notify_foreach_carrier carrier;
	GPilotd_Orb_Pilot_Notifications *notifications;
	
	g_assert (pilot_id);
	g_assert (message);

	if((notifications = g_hash_table_lookup(orb_context->notifications,pilot_id))==NULL) {
		return;
	}

	carrier.id = g_strdup(pilot_id);
	carrier.ptr1 = gnome_pilot_conduit_get_name(conduit);
	carrier.ptr2 = message;
	carrier.purge_list = NULL;
	carrier.action = CALL_CONDUIT_MESSAGE;
	g_slist_foreach(notifications->conduit,
			(GFunc)orbed_notify_multi_purpose_foreach,
			&carrier);
	/* FIXME: See note 1 at the end of this file */
	if(carrier.purge_list) purge_ior_foreach(pilot_id,carrier.purge_list);
	g_free(carrier.id);
	g_free(carrier.ptr1);
}

void 
orbed_notify_daemon_message(gchar *pilot_id,				
			    GnomePilotConduit *conduit,
			    gchar *message) {
	GPilotd_notify_foreach_carrier carrier;
	GPilotd_Orb_Pilot_Notifications *notifications;
	
	g_assert (pilot_id);
	g_assert (message);

	if((notifications = g_hash_table_lookup(orb_context->notifications,pilot_id))==NULL) {
		return;
	}

	carrier.id = g_strdup(pilot_id);
	carrier.ptr1 = conduit ? gnome_pilot_conduit_get_name(conduit) : NULL;
	carrier.ptr2 = message;
	carrier.purge_list = NULL;
	carrier.action = CALL_DAEMON_MESSAGE;
	g_slist_foreach(notifications->conduit,
			(GFunc)orbed_notify_multi_purpose_foreach,
			&carrier);
	/* FIXME: See note 1 at the end of this file */
	if(carrier.purge_list) purge_ior_foreach(pilot_id,carrier.purge_list);
	g_free(carrier.id);
	g_free(carrier.ptr1);
}

void 
orbed_notify_daemon_error (gchar *pilot_id,				
			   gchar *message) {
	GPilotd_notify_foreach_carrier carrier;
	GPilotd_Orb_Pilot_Notifications *notifications;
	
	g_assert (pilot_id);
	g_assert (message);

	if((notifications = g_hash_table_lookup(orb_context->notifications,pilot_id))==NULL) {
		return;
	}

	carrier.id = g_strdup(pilot_id);
	carrier.ptr2 = message;
	carrier.purge_list = NULL;
	carrier.action = CALL_DAEMON_ERROR;
	g_slist_foreach(notifications->conduit,
			(GFunc)orbed_notify_multi_purpose_foreach,
			&carrier);
	/* FIXME: See note 1 at the end of this file */
	if(carrier.purge_list) purge_ior_foreach(pilot_id,carrier.purge_list);
	g_free(carrier.id);
	g_free(carrier.ptr1);
}

void 
orbed_notify_conduit_progress(gchar *pilot_id,				
			      GnomePilotConduit *conduit,
			      int current, 
			      int total) {
	GPilotd_notify_foreach_carrier carrier;
	GPilotd_Orb_Pilot_Notifications *notifications;
	
	g_assert (pilot_id);
	g_assert (conduit);

	if((notifications = g_hash_table_lookup(orb_context->notifications,pilot_id))==NULL) {
		return;
	}

	carrier.id = g_strdup(pilot_id);
	carrier.ptr1 = gnome_pilot_conduit_get_name(conduit);
	carrier.long1 = current;
	carrier.long2 = total;
	carrier.purge_list = NULL;
	carrier.action = CALL_CONDUIT_PROGRESS;
	g_slist_foreach(notifications->conduit,
			(GFunc)orbed_notify_multi_purpose_foreach,
			&carrier);
	/* FIXME: See note 1 at the end of this file */
	if(carrier.purge_list) purge_ior_foreach(pilot_id,carrier.purge_list);
	g_free(carrier.id);
	g_free(carrier.ptr1);
}

void 
orbed_notify_overall_progress(gchar *pilot_id,				
			      int current, 
			      int total) {
	GPilotd_notify_foreach_carrier carrier;
	GPilotd_Orb_Pilot_Notifications *notifications;
	
	g_assert (pilot_id);

	if((notifications = g_hash_table_lookup(orb_context->notifications,pilot_id))==NULL) {
		return;
	}

	carrier.id = g_strdup(pilot_id);
	carrier.long1 = current;
	carrier.long2 = total;
	carrier.purge_list = NULL;
	carrier.action = CALL_OVERALL_PROGRESS;
	g_slist_foreach(notifications->conduit,
			(GFunc)orbed_notify_multi_purpose_foreach,
			&carrier);
	/* FIXME: See note 1 at the end of this file */
	if(carrier.purge_list) purge_ior_foreach(pilot_id,carrier.purge_list);
	g_free(carrier.id);
}

/********************************************************************************/

void 
gpilotd_corba_init_context(GPilotContext *context) 
{
	orb_context = g_new(GPilotd_Orb_Context,sizeof(GPilotd_Orb_Context));
	orb_context->monitors = g_hash_table_new(g_str_hash,g_str_equal);
	orb_context->notifications = g_hash_table_new(g_str_hash,g_str_equal);
	orb_context->gpilotd_context = context;
}

/*
  Now the follows the stuff that tries to free all that
  has been allocated. Helps in tracking down leaks 
*/

static void
cleanup_code_g_free(gpointer a,gpointer b) 
{
	g_free(a);
}

static void
cleanup_code_monitors_remover(gchar *key,
			      GSList **value,
			      gpointer data) 
{
	g_free(key);
	if (value) g_slist_foreach(*value,cleanup_code_g_free,NULL);
}

static void 
cleanup_code_notifications_remover(gchar *key,
				   GPilotd_Orb_Pilot_Notifications *value,
				   gpointer data) 
{
	g_free(key);
	if (value){
		g_slist_foreach(value->connect,cleanup_code_g_free,NULL);
		g_slist_foreach(value->disconnect,cleanup_code_g_free,NULL);
		g_slist_foreach(value->request_complete,cleanup_code_g_free,NULL);
		g_slist_foreach(value->request_timeout,cleanup_code_g_free,NULL);
		g_slist_foreach(value->backup,cleanup_code_g_free,NULL);
		g_slist_foreach(value->conduit,cleanup_code_g_free,NULL);
		g_slist_foreach(value->userinfo_requested,cleanup_code_g_free,NULL);
		g_slist_foreach(value->sysinfo_requested,cleanup_code_g_free,NULL);
		g_slist_foreach(value->userinfo_sent,cleanup_code_g_free,NULL);
		
		g_slist_free(value->connect);
		g_slist_free(value->disconnect);
		g_slist_free(value->request_complete);
		g_slist_free(value->request_timeout);
		g_slist_free(value->backup);
		g_slist_free(value->conduit);
		g_slist_free(value->userinfo_requested);
		g_slist_free(value->sysinfo_requested);
		g_slist_free(value->userinfo_sent);
		g_free(value);
	}

}

void 
gpilotd_corba_clean_up(void) 
{
	g_hash_table_foreach_remove(orb_context->monitors,
				    (GHRFunc)cleanup_code_monitors_remover,
				    NULL);
	g_hash_table_destroy(orb_context->monitors);
	g_hash_table_foreach_remove(orb_context->notifications,
				    (GHRFunc)cleanup_code_notifications_remover,
				    NULL);
	g_hash_table_destroy(orb_context->notifications);
	g_free(orb_context);
}

/* Now the cleanup stuff ends */

void 
gpilotd_corba_quit(void) 
{
	gpilotd_corba_clean_up();
	CORBA_ORB_shutdown(orb, CORBA_FALSE, &ev);
}

void gpilotd_corba_init(int *argc, 
			char **argv,
			GPilotContext **context)
{
	PortableServer_ObjectId objid = {0, sizeof("gpilotd"), "gpilotd" };
	PortableServer_POA the_poa;
	GNOME_Pilot_Daemon acc;
	static gboolean object_initialised = FALSE;
	Bonobo_RegistrationResult result;

	if ( object_initialised ) return;

	object_initialised = TRUE;

	CORBA_exception_init(&ev);

	orb = bonobo_activation_orb_get();

	*context = gpilot_context_new();
	gpilot_context_init_user (*context);
	gpilotd_corba_init_context(*context);

	the_poa = (PortableServer_POA)CORBA_ORB_resolve_initial_references(orb, "RootPOA", &ev);
	g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

	POA_GNOME_Pilot_Daemon__init(&daemon_servant, &ev);
	g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

	PortableServer_POAManager_activate(PortableServer_POA__get_the_POAManager(the_poa, &ev), &ev);
	g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

	PortableServer_POA_activate_object_with_id(the_poa, &objid, &daemon_servant, &ev);
	g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

	acc = PortableServer_POA_servant_to_reference(the_poa, &daemon_servant, &ev);
	g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

	g_message ("Activating CORBA server");

	result = bonobo_activation_active_server_register (
		"OAFIID:GNOME_Pilot_Daemon", acc);
	g_message ("bonobo_activation_active_server_register = %d", result);
	switch (result) {
	case Bonobo_ACTIVATION_REG_SUCCESS:
		break;
	case Bonobo_ACTIVATION_REG_NOT_LISTED:
		g_message ("Cannot register gpilotd because not listed");
		exit (1);
	case Bonobo_ACTIVATION_REG_ALREADY_ACTIVE:
		g_message ("Cannot register gpilotd because already active");
		exit (0);
	case Bonobo_ACTIVATION_REG_ERROR:
	default:
		g_message ("Cannot register gpilotd because we suck");
		exit (1);
	}

	CORBA_Object_release(acc, &ev);
	g_return_if_fail(ev._major == CORBA_NO_EXCEPTION);

	if (!bonobo_init (argc, argv)) {
		g_message ("init_bonobo(): could not initialize Bonobo");
		exit (EXIT_FAILURE);
	}
}


/* Notes 
   
Note 1:

   This purges monitors for clients to whom communication caused an
   exception.  Note that only monitors on the pilot that just synced
   are purged, this can be considered a leak (if the pilot never syncs
   and a client continously connects/ requests the notification and
   disconnects)

*/
