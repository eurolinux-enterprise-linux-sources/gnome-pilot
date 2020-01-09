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

#include <gnome.h>
#include "queue_io.h"
#include "orbit_daemon_glue.h"

#define QUEUE "/gnome-pilot.d/queue/"
/*#define QUEUE "=/home/deity/.gnome/gnome-pilot.d/queue=/"*/
/*char QUEUE[128];*/
#define NUMREQ "number-of-requests"

/* defines for entries in the queue file */

#define ENT_TYPE "type"
#define ENT_CLIENT_ID "client_id"
#define ENT_FILENAME "filename"
#define ENT_DESCRIPTION "description"
#define ENT_DIRECTORY "directory"
#define ENT_DEVICE "device"
#define ENT_PILOT_ID "pilot_id"
#define ENT_USER_ID "user_id"
#define ENT_PASSWORD "password"
#define ENT_CONDUIT "conduit"
#define ENT_HOW "synctype"
#define ENT_TIMEOUT "timeout"
#define ENT_HANDLE "handle"
#define ENT_CONT_SYNC "continue_sync"

static gint is_system_related (GPilotRequestType type) {
	switch(type) {
	case GREQ_CRADLE_EVENT:
	case GREQ_SET_USERINFO:
	case GREQ_GET_USERINFO:
	case GREQ_GET_SYSINFO:
	case GREQ_NEW_USERINFO:
		return 1;
	default:
		return 0;
	}
}

/*
  FIXME:
  crapcrap, set_section skal tage **, man skal ikke malloc den... duhduh!
*/

static void set_section (guint32 pilot_id, GPilotRequestType type, gchar **section) {
	g_assert(section!=NULL);
	if (*section!=NULL) { g_warning("set_section: *section!=NULL, potiential leak!"); }

	if(!is_system_related(type)) {
		(*section) = g_strdup_printf ("%s%u/", QUEUE, pilot_id);
	} else {
		(*section) = g_strdup_printf ("%ssystem/", QUEUE);
	}
}

static guint32 set_section_num (guint32 pilot_id,
				GPilotRequestType type,
				gchar **section,
				gint num) {
	g_assert (section!=NULL);
	if (*section!=NULL) { g_warning("set_section_num: *section!=NULL, potiential leak!"); }

	if(!is_system_related (type)) {
		(*section) = g_strdup_printf ("%s%u-%u/", QUEUE, pilot_id, num);
		return pilot_id*65535+num;
	} else {
		(*section) = g_strdup_printf ("%ssystem-%d/", QUEUE, num);
		return num;
	}
}

static GPilotRequestType request_type_from_string(gchar *str) {
	if (str==NULL) {
		g_warning (_("Error in queue, non-existing entry"));
		return GREQ_INVALID;
	}

	if(g_strcasecmp(str,"GREQ_INSTALL")==0) return GREQ_INSTALL;
	if(g_strcasecmp(str,"GREQ_RESTORE")==0) return GREQ_RESTORE;
	if(g_strcasecmp(str,"GREQ_CONDUIT")==0) return GREQ_CONDUIT;
	if(g_strcasecmp(str,"GREQ_SET_USERINFO")==0) return GREQ_SET_USERINFO;
	if(g_strcasecmp(str,"GREQ_GET_USERINFO")==0) return GREQ_GET_USERINFO;
	if(g_strcasecmp(str,"GREQ_GET_SYSINFO")==0) return GREQ_GET_SYSINFO;
	if(g_strcasecmp(str,"GREQ_NEW_USERINFO")==0) return GREQ_NEW_USERINFO;
	return GREQ_INVALID;
}

GList* gpc_queue_load_requests_for_cradle(gchar *cradle) {
	GList *retval,*tmp,*it;

	g_assert(cradle!=NULL);

	retval = NULL;
	tmp = NULL;
	it = NULL;
	/* call with a system type request and all=TRUE, to get all the queued system requests */
	tmp = gpc_queue_load_requests (0, GREQ_CRADLE_EVENT, TRUE);

	it = g_list_first(tmp);
	while(it) {
		GPilotRequest *req;
		req = NULL;;

		req = (GPilotRequest*)it->data;
		if(req && g_strcasecmp(req->cradle,cradle)==0) {
			retval = g_list_append(retval,req);
		}
		it = g_list_next(it);
	}

	g_list_free(tmp);

	return retval;
}

GList* gpc_queue_load_requests (guint32 pilot_id, GPilotRequestType type, gboolean all) {
	int num;
	GList *retval = NULL;
	gchar *prefix = NULL;

	set_section (pilot_id, type, &prefix);
 
	gnome_config_push_prefix (prefix);
	num = gnome_config_get_int (NUMREQ);
	gnome_config_pop_prefix ();
	g_free (prefix);

	for (;num>0;num--) {
		GPilotRequest *req;
		if (is_system_related (type)) {
			req = gpc_queue_load_request (pilot_id, TRUE, num);
		} else {
			req = gpc_queue_load_request (pilot_id, FALSE, num);
		}

		if(req==NULL) {
			gnome_config_pop_prefix ();
			continue;
		}
		if (req->type!=type && all==FALSE) {
			g_free (req);
			gnome_config_pop_prefix ();
			continue;
		}

		retval = g_list_append (retval, req);
		gnome_config_pop_prefix ();
	}
	return retval;
}

GPilotRequest* 
gpc_queue_load_request (guint32 pilot_id, gboolean _type, guint num) {
	GPilotRequest *req;
	gchar *prefix = NULL;
	GPilotRequestType type;
  
	if (_type==TRUE) {
		type = GREQ_CRADLE_EVENT; 
	} else {
		type = GREQ_PILOT_EVENT;
	}

	set_section_num (pilot_id, type, &prefix, num);
	gnome_config_push_prefix (prefix);

	req = g_new0 (GPilotRequest, sizeof (GPilotRequest));
	req->type = request_type_from_string (gnome_config_get_string (ENT_TYPE));
	if (req->type==GREQ_INVALID) {
		g_free (req);
		gnome_config_pop_prefix ();
		return NULL;
	}
  
	/* unless I store the sectionname _without_ trailing /, clean_section
	   can't delete it ? */
	g_free (prefix);
	prefix = NULL;
	req->handle = set_section_num (pilot_id, type, &prefix, num);
	req->queue_data.section_name = g_strdup (prefix);
	req->pilot_id = pilot_id;
  
	switch (req->type) {
	case GREQ_INSTALL:
		req->parameters.install.filename = gnome_config_get_string(ENT_FILENAME);
		req->parameters.install.description = gnome_config_get_string(ENT_DESCRIPTION);
		break;
	case GREQ_RESTORE:
		req->parameters.restore.directory = gnome_config_get_string(ENT_DIRECTORY);
		break;
	case GREQ_CONDUIT: {
		gchar *tmp;
		req->parameters.conduit.name = gnome_config_get_string(ENT_CONDUIT);
		tmp = gnome_config_get_string(ENT_HOW);
		req->parameters.conduit.how = gnome_pilot_conduit_sync_type_str_to_int(tmp);
		g_free(tmp);
	}
	break;
	case GREQ_GET_USERINFO:
		break;
	case GREQ_GET_SYSINFO:
		break;
	case GREQ_NEW_USERINFO: /* shares parameters with SET_USERINFO */
		req->parameters.set_userinfo.password = gnome_config_get_string(ENT_PASSWORD);
		req->parameters.set_userinfo.user_id = gnome_config_get_string(ENT_USER_ID);
		req->parameters.set_userinfo.pilot_id = gnome_config_get_int(ENT_PILOT_ID);
		break;
	case GREQ_SET_USERINFO:
		req->parameters.set_userinfo.password = gnome_config_get_string(ENT_PASSWORD);
		req->parameters.set_userinfo.user_id = gnome_config_get_string(ENT_USER_ID);
		req->parameters.set_userinfo.pilot_id = gnome_config_get_int(ENT_PILOT_ID);
		req->parameters.set_userinfo.continue_sync = gnome_config_get_bool(ENT_CONT_SYNC);
		break;
	default: 
		g_assert_not_reached();
		break;
	}
	req->cradle = gnome_config_get_string(ENT_DEVICE);
	req->client_id = gnome_config_get_string(ENT_CLIENT_ID);
	req->timeout = gnome_config_get_int(ENT_TIMEOUT);
	req->handle = gnome_config_get_int(ENT_HANDLE);
	gnome_config_pop_prefix();
	g_free(prefix);

	return req;
}

/*
  FIXME: Leaks! gpc_queue_store_requests is called from
  orbit_daemon_glue in a return call, with strings that are strdupped,
  thus they're leaked.
 */

guint gpc_queue_store_request(GPilotRequest req) {
	guint num;
	guint32 handle_num;
	gchar *prefix = NULL;

	set_section(req.pilot_id,req.type,&prefix);

	gnome_config_push_prefix(prefix);
	num = gnome_config_get_int(NUMREQ);
	num++;
	gnome_config_set_int(NUMREQ,num);
	gnome_config_pop_prefix();
	g_free(prefix);
	prefix = NULL;
	handle_num = set_section_num(req.pilot_id,req.type,&prefix,num);
  
	gnome_config_push_prefix(prefix);
	switch(req.type) {
	case GREQ_INSTALL: 
		gnome_config_set_string(ENT_TYPE,"GREQ_INSTALL"); 
		gnome_config_set_string(ENT_FILENAME,req.parameters.install.filename);
		gnome_config_set_string(ENT_DESCRIPTION,req.parameters.install.description);
		break;
	case GREQ_RESTORE: 
		gnome_config_set_string(ENT_TYPE,"GREQ_RESTORE"); 
		gnome_config_set_string(ENT_DIRECTORY,req.parameters.restore.directory);
		break;
	case GREQ_CONDUIT: 
		g_message("req.parameters.conduit.name = %s",req.parameters.conduit.name);
		gnome_config_set_string(ENT_TYPE,"GREQ_CONDUIT"); 
		gnome_config_set_string(ENT_CONDUIT,req.parameters.conduit.name);
		gnome_config_set_string(ENT_HOW,
		gnome_pilot_conduit_sync_type_int_to_str(req.parameters.conduit.how));
		break;
	case GREQ_NEW_USERINFO: 
		gnome_config_set_string(ENT_TYPE,"GREQ_NEW_USERINFO"); 
		gnome_config_set_string(ENT_DEVICE,req.cradle);
		gnome_config_set_string(ENT_USER_ID,req.parameters.set_userinfo.user_id);
		gnome_config_set_int(ENT_PILOT_ID,req.parameters.set_userinfo.pilot_id);
		break;
	case GREQ_SET_USERINFO: 
		gnome_config_set_string(ENT_TYPE,"GREQ_SET_USERINFO"); 
		gnome_config_set_string(ENT_DEVICE,req.cradle);
		gnome_config_set_string(ENT_PASSWORD,req.parameters.set_userinfo.password);
		gnome_config_set_string(ENT_USER_ID,req.parameters.set_userinfo.user_id);
		gnome_config_set_int(ENT_PILOT_ID,req.parameters.set_userinfo.pilot_id);
		gnome_config_set_bool(ENT_CONT_SYNC,req.parameters.set_userinfo.continue_sync);
		break;
	case GREQ_GET_USERINFO: 
		gnome_config_set_string(ENT_TYPE,"GREQ_GET_USERINFO"); 
		gnome_config_set_string(ENT_DEVICE,req.cradle);
		break;
	case GREQ_GET_SYSINFO: 
		gnome_config_set_string(ENT_TYPE,"GREQ_GET_SYSINFO"); 
		gnome_config_set_string(ENT_DEVICE,req.cradle);
		break;
	default: 
		g_assert_not_reached();
		break;
	}
	gnome_config_set_int(ENT_TIMEOUT,req.timeout);
	gnome_config_set_int(ENT_HANDLE,handle_num);
	gnome_config_set_string(ENT_CLIENT_ID,req.client_id);
	gnome_config_pop_prefix();
    
	gnome_config_sync();
	g_free(prefix);

	LOG(("assigned handle num %u",handle_num));
	return handle_num;
}

guint gpc_queue_store_set_userinfo_request (guint timeout, 
					    const gchar *cradle, 
					    const gchar *client_id,
					    const gchar *username,
					    guint userid,
					    gboolean continue_sync)
{
	GPilotRequest req;

	req.type = GREQ_SET_USERINFO;
	req.pilot_id = 0;
	req.timeout = timeout;
	req.cradle = g_strdup(cradle);
	req.client_id = g_strdup(client_id);
	req.parameters.set_userinfo.password = NULL;
	req.parameters.set_userinfo.user_id = g_strdup(username);
	req.parameters.set_userinfo.pilot_id = userid;
	req.parameters.set_userinfo.continue_sync = continue_sync;

	return gpc_queue_store_request(req);	
}


/* FIXME:This is nice, but if there's 10 requests, and you delete 2-9,
leaving two behind, they're enumerated wrong. Eith renumerate them, or
do the fix in load of requests. Or will that scenario never appear ? Yes it will,
if gpilotd trashs before all requests are done, the last ones will be left in the
file, that can be fixed by handling them in reverse order... yech 

But since the handle is done using the enumeration (id << 16 + enum),
renummerating them would trash that.

*/

void 
gpc_queue_purge_request(GPilotRequest **req) 
{
	gchar *prefix = NULL;
	int num;

	LOG(("gpc_queue_purge_request()"));

	g_return_if_fail(req!=NULL);
	g_return_if_fail(*req!=NULL);

	set_section((*req)->pilot_id,(*req)->type,&prefix);
	gnome_config_push_prefix(prefix);
	num = gnome_config_get_int(NUMREQ);
	num--;
	gnome_config_set_int(NUMREQ,num);
	gnome_config_pop_prefix();

	gnome_config_clean_section((*req)->queue_data.section_name);
	switch((*req)->type) {
	case GREQ_INSTALL:
		unlink((*req)->parameters.install.filename);
		g_free((*req)->parameters.install.filename);
		g_free((*req)->parameters.install.description);
		break;
	case GREQ_RESTORE:
		g_free((*req)->parameters.restore.directory);
		break;
	case GREQ_CONDUIT:
		g_free((*req)->parameters.conduit.name);
		break;
	case GREQ_GET_USERINFO: 
		break;
	case GREQ_GET_SYSINFO: 
		break;
	case GREQ_NEW_USERINFO: 
	case GREQ_SET_USERINFO: 
		g_free((*req)->parameters.set_userinfo.user_id);
		g_free((*req)->parameters.set_userinfo.password);
		break;
	default: 
		g_assert_not_reached();
		break;
	}
	g_free((*req)->cradle);
	g_free((*req)->client_id);
	g_free((*req)->queue_data.section_name);
	g_free(*req);
	*req = NULL;
  
	gnome_config_sync();
	g_free(prefix);
} 

void 
gpc_queue_purge_request_point (guint32 pilot_id,
			       guint num) 
{
	GPilotRequest *req;

	if(pilot_id==0) {
                 /* no id, system level request, use system level request
		    type for load */
		g_message ("FISK: OST");
		req = gpc_queue_load_request (pilot_id, TRUE, num);
	} else {
                /* otherwise use a common as type */
		g_message ("FISK: KRYDDERSILD");
		req = gpc_queue_load_request (pilot_id, FALSE, num);
	}

	if(req) {
		gpc_queue_purge_request (&req);  
	} else {
		g_warning (_("fault: no request found for PDA %d, request %d"), pilot_id, num);
	}
}
