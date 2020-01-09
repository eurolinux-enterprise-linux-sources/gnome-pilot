/*

  BEWARE!! This is not a useful tool, it is a playgroundfor eskil to test gpilotd!

 */
#include <config.h>

#include <gnome.h>
#include <bonobo-activation/bonobo-activation.h>

#include <libgpilotdCM/gnome-pilot-conduit-management.h>
#include <libgpilotdCM/gnome-pilot-conduit-config.h>

int     arg_listconduits,
	arg_listconduitsstate,
	arg_allattrs,
	arg_attrs,
	arg_enable,
	arg_enableslow,
	arg_disable,
	arg_instantiate;
guint32 arg_pilot;
gchar *	arg_conduit;

static const struct poptOption options[] = {
	{"listconduits", 'l', POPT_ARG_NONE, &arg_listconduits, 0, N_("lists available conduits"), NULL},
	{"conduit", '\0', POPT_ARG_STRING, &arg_conduit, 0, N_("conduit to operate on"), N_("CONDUIT")},
	{"listallattribs", 'A', POPT_ARG_NONE, &arg_allattrs, 0, N_("list all attributes"), NULL},
	{"pilotid", '\0', POPT_ARG_INT, &arg_pilot, 0, N_("specify PDA to operate on"), N_("PDA_ID")},
	{"listattribs",'a', POPT_ARG_NONE, &arg_attrs, 0, N_("list attributes"), NULL},
	{"listconduitswithstat",'s', POPT_ARG_NONE, &arg_listconduitsstate, 0, N_("list available conduits and their state"), NULL},
	{"enable",'e', POPT_ARG_NONE, &arg_enable, 0, N_("enable specified conduit"), NULL},
	{"disable",'d', POPT_ARG_NONE, &arg_disable, 0, N_("disable specified conduit"), NULL},
	{"enablefirstslow",'E', POPT_ARG_NONE, &arg_enableslow, 0, N_("enable, and set firstsync to slow"), NULL},
	{"instantiate",'i', POPT_ARG_NONE, &arg_instantiate, 0 , N_("instantiate and destroy the conduit"), NULL},
	{NULL, '\0', 0, NULL, 0} /* end the list */
};

static void 
list_conduits (GList *strs, gboolean with_state) {
	GList *ptr;
	gint id=0;
	if (strs && !with_state) {
		g_message ("Id | Conduit");
		g_message ("---+--------------------------");
		for (ptr=strs; ptr; ptr=ptr->next) {
			id++;
			g_message ("%-2d | %s", id,(gchar*)ptr->data);
		}
	} else if (strs) {
		GnomePilotConduitSyncType st;
		GnomePilotConduitManagement *gpcm;
		GnomePilotConduitConfig *gpcc;
		g_message ("Id | State           | Conduit");
		g_message ("---+-----------------+------------------------");
		for (ptr=strs; ptr; ptr=ptr->next) {
			id++;
			gpcm = gnome_pilot_conduit_management_new ((gchar*)ptr->data,
								  GNOME_PILOT_CONDUIT_MGMT_ID);
			if (gpcm==NULL) { g_error ("gnome_pilot_conduit_management_new (...) == NULL"); }
			gpcc = gnome_pilot_conduit_config_new (gpcm, arg_pilot);
			if (gnome_pilot_conduit_config_is_enabled (gpcc,&st)) {
				g_message ("%-2.2d | %15s | (%s) %s", id,
					  gnome_pilot_conduit_sync_type_int_to_str (st),
					  (gchar*)ptr->data,
					  gnome_pilot_conduit_management_get_name (gpcm));
			} else {
				g_message ("%-2.2d | %15s | (%s) %s", id," ",
					  (gchar*)ptr->data,
					  gnome_pilot_conduit_management_get_name (gpcm));
			}
			gnome_pilot_conduit_management_destroy (gpcm);
			gnome_pilot_conduit_config_destroy (gpcc);
		}
	}
}

static void 
list_attribs (GnomePilotConduitManagement *c, gboolean filter) {
	GList *attribs=NULL;
	GList *ptr;

	attribs = gnome_pilot_conduit_management_get_attribute_list (c, filter);
	g_message ("Attributes %s------------------", filter?"(no lang)":"");
	if (attribs)
		for (ptr = attribs; ptr; ptr = ptr->next) {
			g_message ("- \"%s\" = %s",(gchar*)ptr->data,(gchar*)gnome_pilot_conduit_management_get_attribute (c, ptr->data, NULL));
		}
}


int
main (int argc, char *argv[]) {
        poptContext pctx;
	GnomePilotConduitManagement *gpcm;
	GnomePilotConduitConfig *gpcc;
	GList *conduits = NULL,*it;
	const GList *ll;
	
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	gnome_init_with_popt_table ("gpilotdcm-client","1.0", argc, argv, options,0,&pctx);

	g_print (_("\nBEWARE!!\nThis is a tool for certain parts of the gnome-pilot package.\nUnless you know what you're doing, don't use this tool."));

	gnome_pilot_conduit_management_get_conduits (&conduits, GNOME_PILOT_CONDUIT_MGMT_ID);
	
	ll = gnome_i18n_get_language_list (NULL);
	g_message ("lang = %s", (char *) ll->data);

	/* default to list all conduit attributes,
	   unless a pilot id is given, then dump status */
	if (!arg_listconduits && !arg_listconduitsstate && !arg_attrs && !arg_allattrs) {
		if (arg_pilot) {
			arg_listconduitsstate = 1;
		} else {
			arg_attrs = 1;
		}
	}

	if (arg_listconduits || arg_listconduitsstate) {
		gboolean state;
		/* list the conduit names */
		if (arg_listconduitsstate && arg_pilot) 
			state = TRUE;
		else 
			state = FALSE;
		list_conduits (conduits, state);
	} 
	if (arg_attrs || arg_allattrs) {
		if (arg_conduit) {
			gpcm = gnome_pilot_conduit_management_new (arg_conduit,
								  GNOME_PILOT_CONDUIT_MGMT_ID);
			if (gpcm==NULL) { g_error ("gnome_pilot_conduit_management_new (...) == NULL"); }
			g_message ("Conduit: %s", arg_conduit);
			list_attribs (gpcm, arg_allattrs?FALSE:TRUE);
			gnome_pilot_conduit_management_destroy (gpcm);
		} else {
			for (it = conduits; it; it=it->next) {
				gpcm = gnome_pilot_conduit_management_new (it->data,
									  GNOME_PILOT_CONDUIT_MGMT_ID);
				if (gpcm==NULL) { g_error ("gnome_pilot_conduit_management_new (...) == NULL"); }
				g_message ("Conduit: %s", gnome_pilot_conduit_management_get_name (gpcm));
				list_attribs (gpcm, arg_allattrs?FALSE:TRUE);
				gnome_pilot_conduit_management_destroy (gpcm);
			}
		}
	}

        if (arg_enable || arg_disable || arg_enableslow) {
		if (arg_conduit) {
			if (arg_pilot) {
				gpcm = gnome_pilot_conduit_management_new (arg_conduit,
								      GNOME_PILOT_CONDUIT_MGMT_ID);
				if (gpcm==NULL) { g_error ("gnome_pilot_conduit_management_new (...) == NULL"); }
				gpcc = gnome_pilot_conduit_config_new (gpcm, arg_pilot);
				if (arg_enable) {
					gnome_pilot_conduit_config_enable (gpcc, GnomePilotConduitSyncTypeSynchronize);
				} else if (arg_disable) {
					gnome_pilot_conduit_config_disable (gpcc);
				} else if (arg_enableslow) {
					gnome_pilot_conduit_config_enable_with_first_sync (gpcc,
											  GnomePilotConduitSyncTypeSynchronize,
											  GnomePilotConduitSyncTypeSynchronize,
											  TRUE);
				} else g_assert_not_reached ();
				gnome_pilot_conduit_management_destroy (gpcm);
				gnome_pilot_conduit_config_destroy (gpcc);
			} else {
				g_warning ("to enable/disable, specify pilot");
			}
		} else {
			g_warning ("to enable/disable, specify conduit");
		}
	}
	if (arg_instantiate) {
		if (arg_conduit) {
			if (arg_pilot) {
				GPilotPilot *pilot;
				GnomePilotConduit *conduit=NULL;
				gint err;
				gpcm = gnome_pilot_conduit_management_new (arg_conduit,
									  GNOME_PILOT_CONDUIT_MGMT_ID);
				if (gpcm==NULL) { 
					g_error ("gnome_pilot_conduit_management_new (...) == NULL"); 
				}
				/* FIXME: Yargh, should load the pilots and check which one matches
				   pilot id */
				pilot = gpilot_pilot_new ();
				pilot->pilot_id = arg_pilot;
				err = gnome_pilot_conduit_management_instantiate_conduit (gpcm,
											 pilot,
											 &conduit);
				if (err != GNOME_PILOT_CONDUIT_MGMT_OK) {
					g_error ("stopped with %d", err);
				}
				g_message ("loaded, gnome_pilot_conduit_get_name (...) == %s", 
					   gnome_pilot_conduit_get_name (conduit));
				gnome_pilot_conduit_management_destroy_conduit (gpcm,&conduit);
				gnome_pilot_conduit_management_destroy (gpcm);
			} else {
				g_warning ("to instantiate, specify pilot");
			}
		} else {
			g_warning ("to instantiate/destroy, specify conduit");
		}
	}
	return 0;
}

