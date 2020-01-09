/*

  BEWARE!! This is not a useful tool, it is a playgroundfor eskil to test gpilotd!

 */
#include <config.h>

#include <gnome.h>
#include <bonobo-activation/bonobo-activation.h>

#include <gpilotd/gnome-pilot-client.h>

GnomePilotClient *gpc;
CORBA_Environment ev;

int     arg_pause=0,
	arg_unpause=0,
	arg_id = 0,
	arg_restart=0,
	arg_setuser = 0,
	arg_listbases = 0,
	arg_monitorall=0,
	arg_monitor,
	arg_getinfo = 0;
char
	*arg_install = NULL,
	*arg_username = NULL,
	*arg_restore = NULL,
	*arg_conduit = NULL,
	*arg_cradle = NULL,
	*arg_list_by_login = NULL,
	*arg_pilot = NULL;

GList *outstanding_requests;

gboolean stay_alive = TRUE;

static const struct poptOption options[] = {
	{"getinfo", '\0', POPT_ARG_NONE, &arg_getinfo, 0, N_("Get System Info"), NULL},
	{"pause", 'p', POPT_ARG_NONE, &arg_pause, 0, N_("Pause daemon"), NULL},
	{"unpause", 'u', POPT_ARG_NONE, &arg_unpause, 0, N_("Unpause daemon"), NULL},
	{"restart", 'r', POPT_ARG_NONE, &arg_restart, 0, N_("Restart daemon"), NULL},
	{"setuser", 'S', POPT_ARG_NONE, &arg_setuser, 0, N_("Set user id and name"), NULL},
	{"pilotid" , '\0', POPT_ARG_INT, &arg_id, 0, N_("ID for the PDA"), N_("ID")},
	{"install",'\0', POPT_ARG_STRING, &arg_install, 0 , N_("Install file"), N_("FILE")},
	{"restore", '\0', POPT_ARG_STRING, &arg_restore, 0, N_("Restore directory"), N_("DIRECTORY")},
	{"conduit", '\0', POPT_ARG_STRING, &arg_conduit, 0, N_("Run conduit"), N_("CONDUIT")},
	{"userid", '\0', POPT_ARG_STRING, &arg_username, 0, N_("Username to set"), N_("USERNAME")},
	{"monitor", 'm', POPT_ARG_NONE, &arg_monitor, 0, N_("Monitor the specified PDA's actions"), NULL},
	{"monitorall", 'A', POPT_ARG_NONE, &arg_monitorall, 0, N_("Monitor all PDAs"), NULL},
	{"listpilots", '\0', POPT_ARG_STRING, &arg_list_by_login, 0, N_("list PDAs by login (all for all pilots)"), N_("LOGIN")},
	{"pilot", '\0', POPT_ARG_STRING, &arg_pilot, 0, N_("Specify PDA to operate on (defaults to MyPDA)"), N_("PILOTNAME")},
	{"cradle", '\0', POPT_ARG_STRING, &arg_cradle, 0, N_("Specify a cradle to operate on (defaults to Cradle0)"), N_("CRADLENAME")},
	{"listbases", 'l', POPT_ARG_NONE, &arg_listbases, 0, N_("List the specified PDA's bases"), NULL},
	{NULL, '\0', 0, NULL, 0} /* end the list */
};

static void
pilot_connect (GnomePilotClient *gpc, 
	      const gchar *pilot_id, 
	      const GNOME_Pilot_UserInfo *userinfo, 
	      gpointer data) {
	g_message ("connect:   pilot_id  %s",pilot_id);
};

static void
pilot_disconnect (GnomePilotClient *gpc, 
		 const gchar *pilot_id, 
		 gpointer _b) {
	gboolean *b = (gboolean*)_b;
	g_message ("disconnect: pilot_id  %s",pilot_id);
	(*b) = FALSE;
};

static void
pilot_request_completed (GnomePilotClient *gpc, gchar *pilot_id, gint handle, gpointer data) {
	GList *iterator;
	gint outstanding_request = 0;

	for (iterator = outstanding_requests; iterator; iterator = g_list_next (iterator)) {
		outstanding_request = GPOINTER_TO_INT (iterator->data);

		if (handle == outstanding_request) {
			if (pilot_id && strlen (pilot_id))
				g_message ("%s completed request %d",pilot_id,handle);
			else 
				g_message ("Completed request %d",handle);
			break;
		} 
	}

	if (outstanding_request) {
		outstanding_requests = g_list_remove (outstanding_requests, GINT_TO_POINTER (outstanding_request));
	}

	if (g_list_length (outstanding_requests) == 0) {
		stay_alive = FALSE;
	}
}

static void 
user_info (GnomePilotClient *gpc,
	   const gchar *cradle,
	   const GNOME_Pilot_UserInfo *userinfo)
{
	g_message ("Pilot in cradle %s :", cradle);
	g_message ("User name %s", userinfo->username);
	g_message ("User id   %d", userinfo->userID);
}
	     

static void
system_info (GnomePilotClient *gpc,
	     const gchar *cradle,
	     const GNOME_Pilot_SysInfo *sysinfo)
{
	g_message ("Pilot in cradle %s :", cradle);
	g_message ("%dKB ROM", sysinfo->romSize);
	g_message ("%dKB RAM", sysinfo->ramSize);
	g_message ("%dKB RAM free", sysinfo->ramFree);
	g_message ("Systemboard is ``%s''", sysinfo->name);
	g_message ("Manufacturer is ``%s''", sysinfo->manufacturer);
	g_message ("Creation/Version is %d/%d", sysinfo->creation, sysinfo->romVersion);
}

static void 
the_loop (void) {
	while (stay_alive) {
		g_main_iteration (TRUE);
	}
}

static void list_bases (void) {
	GList *list = NULL,*ptr;

	gnome_pilot_client_get_databases_from_cache (gpc,arg_pilot,&list);
	if (list)
		for (ptr = list; ptr; ptr = ptr->next) {
			g_message ("database %s", (char*)ptr->data);
		}
	else 
		g_message ("No databases");
}

static void list_by_login (void) {
	GList *list = NULL, *ptr;
	gint *ids = NULL;
	gint idx;

	if (strcmp (arg_list_by_login, "all")==0) {
		gnome_pilot_client_get_pilots (gpc, &list);
		gnome_pilot_client_get_pilot_ids (gpc, &ids);
	} else {
		gnome_pilot_client_get_pilots_by_user_login (gpc, arg_list_by_login, &list);
	}
	if (list) {
		idx = 0;
		for (ptr = list; ptr; ptr = ptr->next ) {			
			if (ids) {
				char *user = NULL;
				char *uid = NULL;
				gnome_pilot_client_get_user_name_by_pilot_name (gpc, 
										(char*)ptr->data,
										&user);
				gnome_pilot_client_get_user_login_by_pilot_name (gpc, 
										 (char*)ptr->data,
										 &uid);
				g_message ("%s is %d, owned by %s aka %s", 
					   (char*)ptr->data,
					   ids[idx], 
					   user, uid);
			} else {
				g_message ("Pilot : %s", (char*)ptr->data);
			}
			idx++;
		}
	} else {
		g_message ("No pilots");
	}
}

static void
print_err (gint err, gint outstanding_request) {
	stay_alive = FALSE;
	switch (err) {
	case GPILOTD_OK:
		g_message ("request sent (id = %d)", outstanding_request);
		stay_alive = TRUE;
		outstanding_requests = g_list_prepend (outstanding_requests, GINT_TO_POINTER (outstanding_request));
		break;
	case GPILOTD_ERR_INVAL:
		g_message ("** Invalid arguments");
		break;
	case GPILOTD_ERR_FAILED:
		g_message ("** Request failed");
		break;
	case GPILOTD_ERR_INTERNAL:
		g_message ("** Internal libgpilotd error");
		break;
	case GPILOTD_ERR_NOT_CONNECTED:
		g_message ("** Not connected to the daemon, try restart");
		break;
	}
}

static void 
restore (char *rest) 
{
	gint err;
	gint outstanding_request;

	g_message ("Restoring %s",arg_install);
	err = gnome_pilot_client_restore (gpc,arg_pilot,arg_restore,GNOME_Pilot_PERSISTENT,0,&outstanding_request);
	print_err (err, outstanding_request);

	if (err==GPILOTD_OK)
		the_loop ();
};

static void 
revive (char *rest) 
{
	GNOME_Pilot_UserInfo user;
	gint err;
	gint outstanding_request;

	g_message ("Reviving %d to %s from %s",arg_id,arg_cradle,arg_restore);
	user.userID = arg_id;
	user.username = CORBA_string_dup (arg_username);
	err = gnome_pilot_client_set_user_info (gpc,
					       arg_cradle,
					       user,
					       TRUE,
					       GNOME_Pilot_PERSISTENT,
					       0,
					       &outstanding_request);
	print_err (err, outstanding_request);

	err = gnome_pilot_client_restore (gpc,
					 arg_pilot,
					 arg_restore,
					 GNOME_Pilot_PERSISTENT,
					 0,
					 &outstanding_request);
	print_err (err, outstanding_request);

	if (err==GPILOTD_OK)
		the_loop ();
};

static void 
install (char *rest) 
{
	gint err;
	gint outstanding_request;

	g_message ("Installing %s",arg_install);
	err = gnome_pilot_client_install_file (gpc,arg_pilot,arg_install,GNOME_Pilot_PERSISTENT,0,&outstanding_request);	

	print_err (err, outstanding_request);
	if (err==GPILOTD_OK)
		the_loop ();
};

static void 
conduit (char *rest) {

	gint err;
	gint outstanding_request;

	err = gnome_pilot_client_conduit (gpc,arg_pilot,arg_conduit,GNOME_Pilot_CONDUIT_DEFAULT,GNOME_Pilot_PERSISTENT,0,&outstanding_request);	

	print_err (err, outstanding_request);
	if (err==GPILOTD_OK)
		the_loop ();
};

static void 
set_user (void) 
{
	GNOME_Pilot_UserInfo user;
	gint err;
	gboolean abort = FALSE;
	gint outstanding_request;

	if (arg_id==0) {
		g_warning ("Please set a pilotid using --pilotid");
		abort = TRUE;
	}	if (arg_username==NULL) {
		g_warning ("Please set a username using --userid");
		abort = TRUE;
	}
	if (arg_cradle==NULL) {
		g_warning ("Please set a cradle using --cradle");
		abort = TRUE;
	}

	if (!abort) {
		g_message ("Setting userid/name %d/%s on %s",arg_id,arg_username,arg_cradle);
		user.userID = arg_id;
		user.username = CORBA_string_dup (arg_username);
		err = gnome_pilot_client_set_user_info (gpc,
						       arg_cradle,
						       user,
						       FALSE,
						       GNOME_Pilot_PERSISTENT,
						       0,
						       &outstanding_request);
		print_err (err, outstanding_request);
		if (err==GPILOTD_OK)
			the_loop ();
	}
};

static void 
get_system_info (void) 
{
	gint err;
	gboolean abort = FALSE;
	gint outstanding_request;

	if (arg_cradle==0) {
		g_warning ("Please set a cradle using --cradle");
		abort = TRUE;
	}

	if (!abort) {
		err = gnome_pilot_client_get_system_info (gpc, arg_cradle,
							  GNOME_Pilot_PERSISTENT,
							  0,
							  &outstanding_request);
		print_err (err, outstanding_request);
		err = gnome_pilot_client_get_user_info (gpc, arg_cradle,
							GNOME_Pilot_PERSISTENT,
							0,
							&outstanding_request);
		print_err (err, outstanding_request);
		if (err==GPILOTD_OK)
			the_loop ();
	}
}

int
main (int argc, char *argv[]) {
        poptContext pctx;
	
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	if (argc<2) {
		g_message ("type `%s --help` for usage",argv[0]);
		exit (1);
	}

	gnome_init_with_popt_table ("gpilotd-client","1.0",
				    argc, argv, options, 0, &pctx);

	g_message (_("\nBEWARE!!\nThis is a tool for certain parts of the gnome-pilot package.\nUnless you know what you're doing, don't use this tool."));

	if (arg_pilot==NULL) arg_pilot = g_strdup ("MyPilot");

	gpc = GNOME_PILOT_CLIENT (gnome_pilot_client_new ());

	gnome_pilot_client_connect_to_daemon (gpc);

	g_signal_connect (G_OBJECT (gpc),"completed_request", G_CALLBACK (pilot_request_completed), NULL);
	g_signal_connect (G_OBJECT (gpc),"system_info", G_CALLBACK (system_info), NULL);
	g_signal_connect (G_OBJECT (gpc),"user_info", G_CALLBACK (user_info), NULL);

	if (arg_pause!=0) {
		g_message ("Pausing daemon");
		gnome_pilot_client_pause_daemon (gpc);
	} else if (arg_unpause!=0) {
		g_message ("Unpausing daemon");
		gnome_pilot_client_unpause_daemon (gpc);	
	} else if (arg_restart!=0) {
		g_message ("Restarting daemon");
		gnome_pilot_client_restart_daemon (gpc);
	} else if (arg_install!=NULL) {
		install (arg_install);
	} else if (arg_cradle!=NULL && arg_restore!=NULL && arg_setuser) {
		revive (NULL);
	} else if (arg_restore!=NULL) {
		restore (arg_restore);
	} else if (arg_conduit!=NULL) {
		conduit (arg_conduit);
	} else if (arg_setuser) {
		set_user ();
	} else if (arg_listbases) {
		list_bases ();
	} else if (arg_list_by_login) {
		list_by_login ();
	} else if (arg_monitor) {
		g_message ("Monitor on %s",arg_pilot);
		gnome_pilot_client_monitor_on (gpc,arg_pilot);
		gnome_pilot_client_notify_on (gpc,GNOME_Pilot_NOTIFY_CONNECT);
		gnome_pilot_client_notify_on (gpc,GNOME_Pilot_NOTIFY_DISCONNECT);
		
		gnome_pilot_client_connect__pilot_connect (gpc, pilot_connect, NULL);
		gnome_pilot_client_connect__pilot_disconnect (gpc, pilot_disconnect, &stay_alive);

		the_loop ();

		gnome_pilot_client_notify_off (gpc,GNOME_Pilot_NOTIFY_CONNECT);
		gnome_pilot_client_notify_off (gpc,GNOME_Pilot_NOTIFY_DISCONNECT);
		gnome_pilot_client_monitor_off (gpc,arg_pilot);
	} else if (arg_monitorall) {
		g_message ("Monitor on all");
		gnome_pilot_client_monitor_on_all_pilots (gpc);

		gnome_pilot_client_connect__pilot_connect (gpc, pilot_connect, NULL);
		gnome_pilot_client_connect__pilot_disconnect (gpc, pilot_disconnect, &stay_alive);

		the_loop ();
		gnome_pilot_client_monitor_off_all_pilots (gpc);		
	} else if (arg_getinfo) {
		g_message ("Get SystemInfo");
		get_system_info ();
	}

	return 0;
}

