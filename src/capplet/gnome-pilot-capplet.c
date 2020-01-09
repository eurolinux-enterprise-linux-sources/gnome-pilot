/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-pilot-capplet.c
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
#include "gnome-pilot-pdialog.h"
#include "gnome-pilot-cdialog.h"
#include "gnome-pilot-ddialog.h"
#include "gnome-pilot-capplet.h"

#define SMALL_ICON_SIZE 20

static GObjectClass *parent_class = NULL;

struct _GnomePilotCappletPrivate 
{
	GladeXML *xml;

	GnomePilotClient *gpc;
	gint handle;
	
	PilotState *state;
	PilotState *orig_state;

	GPilotPilot *pilot;

	GHashTable *conduits;

	GtkWidget *notebook;

	GtkWidget *pilots_treeview;
	GtkTreeModel *pilots_model;
	GtkWidget *pilots_add;
	GtkWidget *pilots_edit;
	GtkWidget *pilots_delete;
	GtkWidget *pilots_popup;
	
	GtkWidget *devices_treeview;
	GtkTreeModel *devices_model;
	GtkWidget *devices_add;
	GtkWidget *devices_edit;
	GtkWidget *devices_delete;
	GtkWidget *devices_popup;

	GtkObject *pdialog;
	GtkWidget *pilots_menu;
	GtkWidget *pilots_username;
	GtkWidget *conduit_treeview;
	GtkTreeModel *conduit_model;
	GtkWidget *conduit_enable;
	GtkWidget *conduit_disable;
	GtkWidget *conduit_settings;
	GtkWidget *conduit_popup;
	GtkWidget *conduit_popup_enable;
	GtkWidget *conduit_popup_disable;
	GtkWidget *conduit_popup_settings;	
	GtkWidget *conduit_description;
};

static void class_init (GnomePilotCappletClass *klass);
static void init (GnomePilotCapplet *gpcap);

static gboolean get_widgets (GnomePilotCapplet *gpcap);
static void init_widgets (GnomePilotCapplet *gpcap);
static void fill_widgets (GnomePilotCapplet *gpcap);

static void check_pilots_buttons (GnomePilotCapplet *gpcap);
static void check_devices_buttons (GnomePilotCapplet *gpcap);
static void check_conduits_buttons (GnomePilotCapplet *gpcap);

static void gpcap_pilots_add (GtkWidget *widget, gpointer user_data);
static void gpcap_pilots_edit (GtkWidget *widget, gpointer user_data);
static void gpcap_pilots_delete (GtkWidget *widget, gpointer user_data);
static gboolean gpcap_pilots_popup (GtkTreeView *treeview, GdkEventButton *event, gpointer user_data);
static void gpcap_pilots_selection_changed (GtkTreeSelection *selection, gpointer user_data);

static void gpcap_devices_add (GtkWidget *widget, gpointer user_data);
static void gpcap_devices_edit (GtkWidget *widget, gpointer user_data);
static void gpcap_devices_delete (GtkWidget *widget, gpointer user_data);
static gboolean gpcap_devices_popup (GtkTreeView *treeview, GdkEventButton *event, gpointer user_data);
static void gpcap_devices_selection_changed (GtkTreeSelection *selection, gpointer user_data);

static void gpcap_conduits_choose_pilot (GtkWidget *widget, gpointer user_data);
static void gpcap_conduits_enable (GtkWidget *widget, gpointer user_data);
static void gpcap_conduits_disable (GtkWidget *widget, gpointer user_data);
static void gpcap_conduits_settings (GtkWidget *widget, gpointer user_data);
static gboolean gpcap_conduits_popup (GtkTreeView *treeview, GdkEventButton *event, gpointer user_data);
static void gpcap_conduits_selection_changed (GtkTreeSelection *selection, gpointer user_data);

static void gpcap_save_state (GnomePilotCapplet *gpcap);

static void gpcap_dispose (GObject *object);

GType
gnome_pilot_capplet_get_type ()
{
	static GType type = 0;				
	if (!type){					
		static GTypeInfo const object_info = {
			sizeof (GnomePilotCappletClass),

			(GBaseInitFunc) NULL,		
			(GBaseFinalizeFunc) NULL,

			(GClassInitFunc) class_init,
			(GClassFinalizeFunc) NULL,
			NULL,	/* class_data */

			sizeof (GnomePilotCapplet),
			0,	/* n_preallocs */
			(GInstanceInitFunc) init,
		};
		type = g_type_register_static (GTK_TYPE_DIALOG, "GnomePilotCapplet", &object_info, 0);
	}
	return type;
}

static void
class_init (GnomePilotCappletClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (GTK_TYPE_DIALOG);

	object_class->dispose = gpcap_dispose;
}

static void
init (GnomePilotCapplet *gpcap)
{
	GnomePilotCappletPrivate *priv;
	
	priv = g_new0 (GnomePilotCappletPrivate, 1);

	gpcap->priv = priv;

	/* State information */
	loadPilotState (&priv->orig_state);
	priv->state = dupPilotState (priv->orig_state);

	/* Gui stuff */
	priv->xml = glade_xml_new ("gpilotd-capplet.glade", "CappletMain", NULL);
	if (!priv->xml) {
		priv->xml = glade_xml_new (GLADEDATADIR "/gpilotd-capplet.glade", "CappletMain", NULL);
		if (!priv->xml) {
			g_message ("gnome-pilot-capplet init(): Could not load the Glade XML file!");
			goto error;
		}
	}
	
	if (!get_widgets (gpcap)) {
		g_message ("gnome-pilot-capplet init(): Could not find all widgets in the XML file!");
		goto error;
	}
	
	fill_widgets (gpcap);
	init_widgets (gpcap);

 error:
	;
}

GnomePilotCapplet *
gnome_pilot_capplet_new (GnomePilotClient *gpc)
{
	GnomePilotCapplet *gpcap;
	
	gpcap = g_object_new (GNOME_PILOT_TYPE_CAPPLET, "default-height", 350, "default-width", 350, NULL);
	
	gpcap->priv->gpc = gpc;

	return gpcap;
}

static gboolean
get_widgets (GnomePilotCapplet *gpcap)
{
	GnomePilotCappletPrivate *priv;
	GtkWidget *w;

	priv = gpcap->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	gtk_window_set_title (GTK_WINDOW (gpcap), _("gnome-pilot Settings"));
	gtk_dialog_set_has_separator (GTK_DIALOG (gpcap), FALSE);
	gtk_dialog_add_button (GTK_DIALOG (gpcap),
			       GTK_STOCK_HELP, GTK_RESPONSE_HELP);
	gtk_dialog_add_button (GTK_DIALOG (gpcap),
			       GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

	priv->notebook = GW ("CappletMain");

	if (priv->notebook)
		gtk_container_add (GTK_CONTAINER (GTK_DIALOG (gpcap)->vbox), priv->notebook);

	w = GW ("pilots_treeview");
	priv->pilots_treeview = gtk_object_get_data (GTK_OBJECT (w), "treeview");
	priv->pilots_model = gtk_object_get_data (GTK_OBJECT (w), "model");
	priv->pilots_add = GW ("pilots_add_button");
	priv->pilots_edit = GW ("pilots_edit_button");
	priv->pilots_delete = GW ("pilots_delete_button");

	w = GW ("devices_treeview");
	priv->devices_treeview = gtk_object_get_data (GTK_OBJECT (w), "treeview");
	priv->devices_model = gtk_object_get_data (GTK_OBJECT (w), "model");
	priv->devices_add = GW ("devices_add_button");
	priv->devices_edit = GW ("devices_edit_button");
	priv->devices_delete = GW ("devices_delete_button");

	priv->pilots_menu = GW ("pilots_menu");
	priv->pilots_username = GW ("username_label");	
	w = GW ("conduit_treeview");
	priv->conduit_treeview = gtk_object_get_data (GTK_OBJECT (w), "treeview");
	priv->conduit_model = gtk_object_get_data (GTK_OBJECT (w), "model");
	priv->conduit_enable = GW ("conduit_enable_button");
	priv->conduit_disable = GW ("conduit_disable_button");
	priv->conduit_settings = GW ("conduit_settings_button");
	priv->conduit_description = GW ("description_label");
	
#undef GW
	return (priv->notebook
		&& priv->pilots_treeview
		&& priv->pilots_add
		&& priv->pilots_edit
		&& priv->pilots_delete
		&& priv->devices_treeview
		&& priv->devices_add
		&& priv->devices_edit
		&& priv->devices_delete
		&& priv->pilots_menu
		&& priv->conduit_treeview
		&& priv->conduit_enable
		&& priv->conduit_disable
		&& priv->conduit_settings
		&& priv->conduit_description);
}

static void 
init_widgets (GnomePilotCapplet *gpcap)
{
	GnomePilotCappletPrivate *priv;

	priv = gpcap->priv;

	/* Button signals */
	gtk_signal_connect (GTK_OBJECT (priv->pilots_add), "clicked",
			    GTK_SIGNAL_FUNC (gpcap_pilots_add), gpcap);

	gtk_signal_connect (GTK_OBJECT (priv->pilots_edit), "clicked",
			    GTK_SIGNAL_FUNC (gpcap_pilots_edit), gpcap);

	gtk_signal_connect (GTK_OBJECT (priv->pilots_delete), "clicked",
			    GTK_SIGNAL_FUNC (gpcap_pilots_delete), gpcap);

	gtk_signal_connect (GTK_OBJECT (priv->devices_add), "clicked",
			    GTK_SIGNAL_FUNC (gpcap_devices_add), gpcap);

	gtk_signal_connect (GTK_OBJECT (priv->devices_edit), "clicked",
			    GTK_SIGNAL_FUNC (gpcap_devices_edit), gpcap);

	gtk_signal_connect (GTK_OBJECT (priv->devices_delete), "clicked",
			    GTK_SIGNAL_FUNC (gpcap_devices_delete), gpcap);

	gtk_signal_connect (GTK_OBJECT (priv->conduit_enable), "clicked",
			    GTK_SIGNAL_FUNC (gpcap_conduits_enable), gpcap);

	gtk_signal_connect (GTK_OBJECT (priv->conduit_disable), "clicked",
			    GTK_SIGNAL_FUNC (gpcap_conduits_disable), gpcap);

	gtk_signal_connect (GTK_OBJECT (priv->conduit_settings), "clicked",
			    GTK_SIGNAL_FUNC (gpcap_conduits_settings), gpcap);

	/* Row selection signals */
	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->pilots_treeview)), "changed",
			  G_CALLBACK (gpcap_pilots_selection_changed), gpcap);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->devices_treeview)), "changed",
			  G_CALLBACK (gpcap_devices_selection_changed), gpcap);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->conduit_treeview)), "changed",
			  G_CALLBACK (gpcap_conduits_selection_changed), gpcap);

	/* Popup menu signals */
	gtk_signal_connect (GTK_OBJECT (priv->pilots_treeview), "button_press_event",
			    GTK_SIGNAL_FUNC (gpcap_pilots_popup), gpcap);
	gtk_signal_connect (GTK_OBJECT (priv->devices_treeview), "button_press_event",
			    GTK_SIGNAL_FUNC (gpcap_devices_popup), gpcap);
	gtk_signal_connect (GTK_OBJECT (priv->conduit_treeview), "button_press_event",
			    GTK_SIGNAL_FUNC (gpcap_conduits_popup), gpcap);

	gtk_widget_show_all (priv->notebook);
}

static void
append_pilots_treeview (GnomePilotCapplet *gpcap, GPilotPilot *pilot, GtkTreeIter *iter)
{
	GnomePilotCappletPrivate *priv;
	GtkTreeIter i;

	priv = gpcap->priv;

	if (!iter)
		iter = &i;

	gtk_list_store_append (GTK_LIST_STORE (priv->pilots_model), iter);
	gtk_list_store_set (GTK_LIST_STORE (priv->pilots_model), iter,
			    0, pilot->name,
			    1, pilot->pilot_id,
			    2, pilot->pilot_username,
			    3, pilot,
			    -1);
}

static void
fill_pilots_treeview (GnomePilotCapplet *gpcap)
{
	GnomePilotCappletPrivate *priv;
	GList *tmp;
	
	priv = gpcap->priv;

	gtk_list_store_clear (GTK_LIST_STORE (priv->pilots_model));
	tmp = priv->state->pilots;
	while (tmp!= NULL) {
		GPilotPilot *pilot =(GPilotPilot*)tmp->data;

		append_pilots_treeview (gpcap, pilot, NULL);
		tmp = tmp->next;
	}	
}

GtkWidget* gnome_pilot_capplet_create_pilots_treeview (char *name, char *string1, char *string2, int num1, int num2);

GtkWidget*
gnome_pilot_capplet_create_pilots_treeview (char *name, char *string1, char *string2, int num1, int num2)
{
	GtkWidget *treeview, *scrolled;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkListStore *model;

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
					     GTK_SHADOW_IN);

	model = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_POINTER);
	treeview = gtk_tree_view_new_with_model ((GtkTreeModel *) model);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, _("Name"),
						     renderer, "text", 0, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, _("ID"),
						     renderer, "text", 1, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, _("Owner"),
						     renderer, "text", 2, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), TRUE);

	gtk_container_add (GTK_CONTAINER (scrolled), treeview);
	
	g_object_set_data (G_OBJECT (scrolled), "model", model);
	g_object_set_data (G_OBJECT (scrolled), "treeview", treeview);

	gtk_widget_show (scrolled);
	gtk_widget_show (treeview);

	return scrolled;
}

static void
append_devices_treeview (GnomePilotCapplet *gpcap, GPilotDevice *device, GtkTreeIter *iter)
{
	GnomePilotCappletPrivate *priv;
	GtkTreeIter i;

	priv = gpcap->priv;

	if (!iter)
		iter = &i;

	gtk_list_store_append (GTK_LIST_STORE (priv->devices_model), iter);
	gtk_list_store_set (GTK_LIST_STORE (priv->devices_model), iter,
			    0, device->name,
			    1, device_type_to_str (device->type),
			    2, device,
			    -1);
}

static void
fill_devices_treeview (GnomePilotCapplet *gpcap)
{
	GnomePilotCappletPrivate *priv;
	GList *tmp;
	
	priv = gpcap->priv;
	
	gtk_list_store_clear (GTK_LIST_STORE (priv->devices_model));
	
	tmp = priv->state->devices;
	while (tmp!= NULL) {
		GPilotDevice *device =(GPilotDevice*)tmp->data;

		append_devices_treeview (gpcap, device, NULL);
		tmp = tmp->next;
	}	
}

GtkWidget *gnome_pilot_capplet_create_devices_treeview (char *name, char *string1, char *string2, int num1, int num2);

GtkWidget*
gnome_pilot_capplet_create_devices_treeview (char *name, char *string1, char *string2, int num1, int num2)
{
	GtkWidget *treeview, *scrolled;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkListStore *model;

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
					     GTK_SHADOW_IN);

	model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	treeview = gtk_tree_view_new_with_model ((GtkTreeModel *) model);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, _("Name"),
						     renderer, "text", 0, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, _("Type"),
						     renderer, "text", 1, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), TRUE);

	gtk_container_add (GTK_CONTAINER (scrolled), treeview);
	
	g_object_set_data (G_OBJECT (scrolled), "model", model);
	g_object_set_data (G_OBJECT (scrolled), "treeview", treeview);

	gtk_widget_show (scrolled);
	gtk_widget_show (treeview);

	return scrolled;
}

static void
append_conduit_treeview (GnomePilotCapplet *gpcap, ConduitState *conduit_state, GtkTreeIter *iter)
{
	GnomePilotCappletPrivate *priv;
	GtkTreeIter i;
	
	priv = gpcap->priv;

	if (!iter)
		iter = &i;

	gtk_list_store_append (GTK_LIST_STORE (priv->conduit_model), iter);
	gtk_list_store_set (GTK_LIST_STORE (priv->conduit_model), iter,
			    0, conduit_state->name,
			    1, display_sync_type_name (conduit_state->enabled,conduit_state->sync_type),
			    2, conduit_state,
			    -1);
}

static void
fill_conduit_treeview (GnomePilotCapplet *gpcap)
{
	GnomePilotCappletPrivate *priv;
	ConduitState *conduit_state;
	GList *conduit_states;

	priv = gpcap->priv;

	gtk_list_store_clear (GTK_LIST_STORE (priv->conduit_model));

	if (priv->pilot == NULL)
		return;
	
	if (priv->conduits == NULL )
		priv->conduits = g_hash_table_new (g_int_hash, g_int_equal);
	
	conduit_states = g_hash_table_lookup (priv->conduits, &priv->pilot->pilot_id);
	if (conduit_states == NULL) {
		conduit_states = load_conduit_list (priv->pilot);
		g_hash_table_insert (priv->conduits, &priv->pilot->pilot_id, conduit_states);
	}
	
	while (conduit_states != NULL) {
		conduit_state = (ConduitState*)conduit_states->data;

		append_conduit_treeview (gpcap, conduit_state, NULL);
		conduit_states = conduit_states->next;
	}
}

static void
set_conduit_pilot (GnomePilotCapplet *gpcap, GPilotPilot *pilot)
{
	GnomePilotCappletPrivate *priv;

	priv = gpcap->priv;

	priv->pilot = pilot;
	
	if (pilot)
		gtk_label_set_text (GTK_LABEL (priv->pilots_username), pilot->pilot_username);
	else
		gtk_label_set_text (GTK_LABEL (priv->pilots_username), "");

	fill_conduit_treeview (gpcap);
	check_conduits_buttons (gpcap);
}

GtkWidget *gnome_pilot_capplet_create_conduit_treeview (char *name, char *string1, char *string2, int num1, int num2);

GtkWidget*
gnome_pilot_capplet_create_conduit_treeview (char *name, char *string1, char *string2, int num1, int num2)
{
	GtkWidget *treeview, *scrolled;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkListStore *model;

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
					     GTK_SHADOW_IN);

	model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	treeview = gtk_tree_view_new_with_model ((GtkTreeModel *) model);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, _("Conduit"),
						     renderer, "text", 0, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, _("Sync Action"),
						     renderer, "text", 1, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), TRUE);

	gtk_container_add (GTK_CONTAINER (scrolled), treeview);
	
	g_object_set_data (G_OBJECT (scrolled), "model", model);
	g_object_set_data (G_OBJECT (scrolled), "treeview", treeview);

	gtk_widget_show (scrolled);
	gtk_widget_show (treeview);

	return scrolled;
}

static void
fill_pilots_menu (GnomePilotCapplet *gpcap) 
{
	GnomePilotCappletPrivate *priv;
	GtkWidget *menu, *menu_item;
	GList *tmp;
	GPilotPilot *pilot = NULL;
	
	priv = gpcap->priv;
	
	menu = gtk_menu_new ();
	
	tmp = priv->state->pilots;
	while (tmp != NULL) {
		if (pilot == NULL)
			pilot = tmp->data;
		
		menu_item = gtk_menu_item_new_with_label (((GPilotPilot*)tmp->data)->name);
		gtk_widget_show (menu_item);
		gtk_object_set_data (GTK_OBJECT (menu_item), "pilot", tmp->data);
		gtk_signal_connect (GTK_OBJECT(menu_item),"activate",
				    GTK_SIGNAL_FUNC (gpcap_conduits_choose_pilot),
				    gpcap);
		gtk_menu_append (GTK_MENU (menu), menu_item);

		tmp = tmp->next;
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (priv->pilots_menu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (priv->pilots_menu), 0);
	set_conduit_pilot (gpcap, pilot);
}

static void
fill_widgets (GnomePilotCapplet *gpcap)
{
	GnomePilotCappletPrivate *priv;

	priv = gpcap->priv;
	
	/* Pilots page */
	fill_pilots_treeview (gpcap);
	check_pilots_buttons (gpcap);
	
	/* Devices page */
	fill_devices_treeview (gpcap);
	check_devices_buttons (gpcap);

	/* Conduits page */
	fill_pilots_menu (gpcap);
}

void
gnome_pilot_capplet_update (GnomePilotCapplet *gpcap) 
{
	GnomePilotCappletPrivate *priv;
	
	priv = gpcap->priv;
	
	freePilotState (priv->orig_state);
	freePilotState (priv->state);

	loadPilotState (&priv->orig_state);
	priv->state = dupPilotState (priv->orig_state);

	fill_widgets (gpcap);
}

static void
check_pilots_buttons (GnomePilotCapplet *gpcap) 
{
	GnomePilotCappletPrivate *priv;
	gboolean test;
	
	priv = gpcap->priv;

	test = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->pilots_treeview)),
						NULL, NULL);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->pilots_edit), test);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->pilots_delete), test);
}

static GPilotPilot *
get_current_pilot (GnomePilotCapplet *gpcap, GtkTreeIter *iter)
{
	GnomePilotCappletPrivate *priv;
	GtkTreeIter i;
	GPilotPilot *pilot;

	priv = gpcap->priv;

	if (!iter)
		iter = &i;

	if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->pilots_treeview)),
					      NULL, iter))
		return NULL;

	gtk_tree_model_get (priv->pilots_model, iter,
			    3, &pilot,
			    -1);

	return pilot;
}

static void 
gpcap_pilots_add (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	GPilotPilot *pilot;
	gboolean res;
	
	priv = gpcap->priv;

	pilot = get_default_pilot (priv->state);
	
	if (priv->pdialog == NULL)
		priv->pdialog = gnome_pilot_pdialog_new (priv->gpc, priv->state, pilot);
	else
		gnome_pilot_pdialog_set_pilot(priv->pdialog, pilot);

	res = gnome_pilot_pdialog_run_and_close (GNOME_PILOT_PDIALOG (priv->pdialog),
	    GTK_WINDOW (gpcap));


	if (!res) {
		g_free (pilot);
	} else {
		GtkTreeIter iter;

		priv->state->pilots = g_list_append (priv->state->pilots, pilot);
		append_pilots_treeview (gpcap, pilot, &iter);

		gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->pilots_treeview)),
						&iter);
		
		fill_pilots_menu (gpcap);
		
		gpcap_save_state (gpcap);
	}
}

static void 
gpcap_pilots_edit (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	GPilotPilot *pilot;
	GtkTreeIter iter;
	gboolean res;
	
	priv = gpcap->priv;
	
	pilot = get_current_pilot (gpcap, &iter);
	
	if (priv->pdialog == NULL)
		priv->pdialog = gnome_pilot_pdialog_new (priv->gpc, priv->state, pilot);
	else
		gnome_pilot_pdialog_set_pilot(priv->pdialog, pilot);

	res = gnome_pilot_pdialog_run_and_close (GNOME_PILOT_PDIALOG (priv->pdialog),
	    GTK_WINDOW (gpcap));
 
	if (res) {
		gtk_list_store_set (GTK_LIST_STORE (priv->pilots_model), &iter,
				    0, pilot->name,
				    1, pilot->pilot_id,
				    2, pilot->pilot_username,
				    -1);
			
		fill_pilots_menu (gpcap);
		
		gpcap_save_state (gpcap);
	}
}

static void 
gpcap_pilots_delete (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	GtkWidget *dlg;
	GPilotPilot *pilot;
	GtkTreeIter iter;
	
	priv = gpcap->priv;
	
	pilot = get_current_pilot (gpcap, &iter);

	dlg = gtk_message_dialog_new (GTK_WINDOW (gpcap), GTK_DIALOG_DESTROY_WITH_PARENT, 
				      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
				      _("Are you sure you want to delete PDA `%s'?"), pilot->name);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_YES) {
		gtk_list_store_remove (GTK_LIST_STORE (priv->pilots_model),
				       &iter);
		priv->state->pilots = g_list_remove (priv->state->pilots, pilot);
		
		fill_pilots_menu (gpcap);
		
		gpcap_save_state (gpcap);
	}

	gtk_widget_destroy (dlg);
	
	check_pilots_buttons (gpcap);
}

static gboolean
gpcap_pilots_popup (GtkTreeView *treeview, GdkEventButton *event, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;

	GnomeUIInfo popup [] = {
		GNOMEUIINFO_ITEM_DATA (N_("_Edit this PDA..."),
				       N_("Edit the currently selected PDA"),
				       gpcap_pilots_edit, gpcap, NULL),
		GNOMEUIINFO_ITEM_DATA (N_("_Delete this PDA"),
				       N_("Delete the currently selected PDA"),
				       gpcap_pilots_delete, gpcap, NULL),
		GNOMEUIINFO_END
	};
 
	priv = gpcap->priv;

	if (priv->pilots_popup == NULL) {
		priv->pilots_popup = gtk_menu_new ();
		gnome_app_fill_menu (GTK_MENU_SHELL (priv->pilots_popup), popup, NULL, TRUE, 0);
	}

	return show_popup_menu (treeview, event, GTK_MENU (priv->pilots_popup));
}

static void
gpcap_pilots_selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	
	check_pilots_buttons (gpcap);
}

static GPilotDevice *
get_current_device (GnomePilotCapplet *gpcap, GtkTreeIter *iter)
{
	GnomePilotCappletPrivate *priv;
	GtkTreeIter i;
	GPilotDevice *device;

	priv = gpcap->priv;

	if (!iter)
		iter = &i;

	if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->devices_treeview)),
					      NULL, iter))
		return NULL;

	gtk_tree_model_get (priv->devices_model, iter,
			    2, &device,
			    -1);

	return device;
}

static void
check_devices_buttons (GnomePilotCapplet *gpcap) 
{
	GnomePilotCappletPrivate *priv;
	gboolean test;
	
	priv = gpcap->priv;

	test = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->devices_treeview)),
						NULL, NULL);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->devices_edit), test);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->devices_delete), test);
}

static void 
gpcap_devices_add (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	GtkObject *dlg;
	GPilotDevice *device;
	gboolean res;
	
	priv = gpcap->priv;

	device = get_default_device (priv->state);
	
	dlg = gnome_pilot_ddialog_new (device);
	res = gnome_pilot_ddialog_run_and_close (GNOME_PILOT_DDIALOG (dlg), GTK_WINDOW (gpcap));

	if (!res) {
		g_free (device);
	} else {
		GtkTreeIter iter;
		
		priv->state->devices = g_list_append (priv->state->devices, device);
		append_devices_treeview (gpcap, device, &iter);
		gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->devices_treeview)),
						&iter);
		
		gpcap_save_state (gpcap);
	}
}

static void 
gpcap_devices_edit (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	GtkObject *dlg;
	GPilotDevice *device;
	GtkTreeIter iter;
	gboolean res;
	
	priv = gpcap->priv;
	
	device = get_current_device (gpcap, &iter);

	dlg = gnome_pilot_ddialog_new (device);
	res = gnome_pilot_ddialog_run_and_close (GNOME_PILOT_DDIALOG (dlg), GTK_WINDOW (gpcap));
 
	if (res) {
		gtk_list_store_set (GTK_LIST_STORE (priv->devices_model), &iter,
				    0, device->name,
				    1, device_type_to_str (device->type),
				    -1);
		
		gpcap_save_state (gpcap);
	}
}

static void 
gpcap_devices_delete (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	GtkWidget *dlg;
	GPilotDevice *device;
	GtkTreeIter iter;
	
	priv = gpcap->priv;
	
	device = get_current_device (gpcap, &iter);

	dlg = gtk_message_dialog_new (GTK_WINDOW (gpcap), GTK_DIALOG_DESTROY_WITH_PARENT, 
				      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
				      _("Are you sure you want to delete %s device?"), device->name);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_YES) {
		gtk_list_store_remove (GTK_LIST_STORE (priv->devices_model),
				       &iter);
		priv->state->devices = g_list_remove (priv->state->devices, device);
		gpcap_save_state (gpcap);
	}

	gtk_widget_destroy (dlg);
	
	check_devices_buttons (gpcap);
}

static gboolean
gpcap_devices_popup (GtkTreeView *treeview, GdkEventButton *event, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;

	GnomeUIInfo popup [] = {
		GNOMEUIINFO_ITEM_DATA (N_("_Edit this device..."),
				       N_("Edit the currently selected device"),
				       gpcap_devices_edit, gpcap, NULL),
		GNOMEUIINFO_ITEM_DATA (N_("_Delete this device"),
				       N_("Delete the currently selected device"),
				       gpcap_devices_delete, gpcap, NULL),
		GNOMEUIINFO_END
	};
 
	priv = gpcap->priv;

	if (priv->devices_popup == NULL) {
		priv->pilots_popup = gtk_menu_new ();
		gnome_app_fill_menu (GTK_MENU_SHELL (priv->pilots_popup), popup, NULL, TRUE, 0);
	}

	return show_popup_menu (treeview, event, GTK_MENU (priv->pilots_popup));
}

static ConduitState *
get_current_state (GnomePilotCapplet *gpcap, GtkTreeIter *iter)
{
	GnomePilotCappletPrivate *priv;
	GtkTreeIter i;
	ConduitState *state;
	
	priv = gpcap->priv;

	if (!iter)
		iter = &i;

	if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->conduit_treeview)),
					      NULL, iter))
		return NULL;

	gtk_tree_model_get (priv->conduit_model, iter,
			    2, &state,
			    -1);

	return state;
}

static void
check_conduits_buttons (GnomePilotCapplet *gpcap) 
{
	GnomePilotCappletPrivate *priv;
	ConduitState *state;
	gboolean test, enabled = FALSE;
	GtkTreeIter iter;
	
	priv = gpcap->priv;

	test = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->conduit_treeview)),
						NULL, &iter);
	if (test) {
		gtk_tree_model_get (priv->conduit_model, &iter,
				    2, &state,
				    -1);
		enabled = state->enabled;
	}
	
	gtk_widget_set_sensitive (GTK_WIDGET (priv->conduit_enable), test && !enabled);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->conduit_disable), test && enabled);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->conduit_settings), test && enabled);
}

static void
gpcap_devices_selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	
	check_devices_buttons (gpcap);
}

static void
gpcap_conduits_choose_pilot (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	GPilotPilot *pilot;
	
	priv = gpcap->priv;

	pilot = gtk_object_get_data (GTK_OBJECT (widget), "pilot");

	set_conduit_pilot (gpcap, pilot);
}

static void 
gpcap_conduits_enable (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	ConduitState *state;
	GtkTreeIter iter;

	priv = gpcap->priv;

	state = get_current_state (gpcap, &iter);
	
	if (state->default_sync_type == GnomePilotConduitSyncTypeNotSet) {
		/* nothing? */
	} else if (state->default_sync_type == GnomePilotConduitSyncTypeCustom) {
		state->changed=TRUE;
		state->enabled=TRUE;
		state->sync_type = GnomePilotConduitSyncTypeCustom;
	} else {
		state->changed = TRUE;
		state->enabled = TRUE;
		state->sync_type = state->default_sync_type;
	}

	gtk_list_store_set (GTK_LIST_STORE (priv->conduit_model), &iter,
			    0, state->name,
			    1, display_sync_type_name (state->enabled, state->sync_type),
			    -1);

	check_conduits_buttons (gpcap);
	gpcap_save_state (gpcap);

	gpcap_conduits_settings (widget, user_data);
}

static void 
gpcap_conduits_disable (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	ConduitState *state;
	GtkTreeIter iter;

	priv = gpcap->priv;
	
	state = get_current_state (gpcap, &iter);
	
	state->enabled=FALSE;
	state->changed=TRUE;

	gtk_list_store_set (GTK_LIST_STORE (priv->conduit_model), &iter,
			    0, state->name,
			    1, display_sync_type_name (FALSE, GnomePilotConduitSyncTypeNotSet),
			    -1);

	check_conduits_buttons (gpcap);
	gpcap_save_state (gpcap);
}

static void 
gpcap_conduits_settings (GtkWidget *widget, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	ConduitState *state;
	GtkTreeIter iter;
	
	priv = gpcap->priv;

	state = get_current_state (gpcap, &iter);
	
	if (state->conduit == NULL) {
		/* must allocate conduit */
		if (gnome_pilot_conduit_management_instantiate_conduit (state->management,
									state->pilot, 
									&state->conduit)
		    != GNOME_PILOT_CONDUIT_MGMT_OK) {
			
			gchar *msg = _("Unable to instantiate %s conduit.");
			error_dialog (GTK_WINDOW (gpcap), msg, state->name);
			return;
		}

		state->settings_widget2 = gnome_pilot_cdialog_new (state);
	}
	
	if (state->settings_widget2 != NULL) {
		if (gnome_pilot_cdialog_run_and_close (GNOME_PILOT_CDIALOG (state->settings_widget2), GTK_WINDOW (gpcap))) {
			/* pressed ok */
			state->sync_type = gnome_pilot_cdialog_sync_type (GNOME_PILOT_CDIALOG (state->settings_widget2));
			state->first_sync_type = gnome_pilot_cdialog_first_sync_type (GNOME_PILOT_CDIALOG (state->settings_widget2));
			state->enabled = (state->sync_type != GnomePilotConduitSyncTypeNotSet);
			state->changed=TRUE;
			
			gtk_list_store_set (GTK_LIST_STORE (priv->conduit_model), &iter,
					    1, display_sync_type_name (state->enabled, state->sync_type),
					    -1);
			
			gpcap_save_state (gpcap);
		} else {
			/* pressed cancel */
			gnome_pilot_conduit_display_settings (state->conduit);
		}
	}
	
	check_conduits_buttons (gpcap);
}

static gboolean
gpcap_conduits_popup (GtkTreeView *treeview, GdkEventButton *event, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	ConduitState *state;
	GtkTreePath *path;
	gboolean test = FALSE, enabled = FALSE;
	
	GnomeUIInfo popup [] = {
		GNOMEUIINFO_ITEM_DATA (N_("_Enable this conduit"),
				       N_("Disable the currently selected conduit"),
				       gpcap_conduits_enable, gpcap, NULL),
		GNOMEUIINFO_ITEM_DATA (N_("_Disable this conduit"),
				       N_("Enable the currently selected conduit"),
				       gpcap_conduits_disable, gpcap, NULL),

		GNOMEUIINFO_SEPARATOR,

		GNOMEUIINFO_ITEM_DATA (N_("_Settings..."),
				       N_("Modify the currently selected conduit's settings"),
				       gpcap_conduits_settings, gpcap, NULL),
		
		GNOMEUIINFO_END
	};
 
	priv = gpcap->priv;

	if (priv->conduit_popup == NULL) {
		priv->conduit_popup = gtk_menu_new ();
		gnome_app_fill_menu (GTK_MENU_SHELL (priv->conduit_popup), popup, NULL, TRUE, 0);

		priv->conduit_popup_enable = popup[0].widget;
		priv->conduit_popup_disable = popup[1].widget;
		priv->conduit_popup_settings = popup[3].widget;
		
	}
	

	if (gtk_tree_view_get_path_at_pos (treeview, event->x, event->y, &path,
					   NULL, NULL, NULL)) {
		GtkTreeIter iter;

		gtk_tree_model_get_iter (priv->conduit_model, &iter, path);

		gtk_tree_model_get (priv->conduit_model, &iter,
				    2, &state,
				    -1);
		test = TRUE;
		enabled = state->enabled;

		gtk_tree_path_free (path);
	}

	gtk_widget_set_sensitive (priv->conduit_popup_enable, test && !enabled);
	gtk_widget_set_sensitive (priv->conduit_popup_disable, test && enabled);
	gtk_widget_set_sensitive (priv->conduit_popup_settings, test && enabled);

	return show_popup_menu (treeview, event, GTK_MENU (priv->conduit_popup));
}

static void
gpcap_conduits_selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (user_data);
	GnomePilotCappletPrivate *priv;
	ConduitState *state;
	
	priv = gpcap->priv;
	
	state = get_current_state (gpcap, NULL);

	gtk_label_set_text (GTK_LABEL (priv->conduit_description), state ? state->description: "");

	check_conduits_buttons (gpcap);
}

static void 
conduit_try_foreach (gpointer key, gpointer value, gpointer func)
{
	GList *states = (GList*)value;
	
	while (states != NULL) {
		ConduitState *state = (ConduitState*)states->data;
		
		if (state->changed && state->enabled) {
			if (state->sync_type == GnomePilotConduitSyncTypeCustom)
				gnome_pilot_conduit_config_enable (state->config, GnomePilotConduitSyncTypeCustom);
			else
				gnome_pilot_conduit_config_enable_with_first_sync (state->config,
										   state->sync_type,
										   state->first_sync_type,
										   TRUE);
			if (state->first_sync_type == GnomePilotConduitSyncTypeNotSet)
				gnome_pilot_conduit_config_remove_first_sync (state->config);
		}
		
		if (state->changed && !state->enabled)
			gnome_pilot_conduit_config_disable (state->config);
		
		if (state->conduit && state->changed)
			gnome_pilot_conduit_save_settings (state->conduit);
		
		states = states->next;
	}
}

static void 
gpcap_save_state (GnomePilotCapplet *gpcap)
{
	GnomePilotCappletPrivate *priv;
	
	priv = gpcap->priv;	

	if (priv->conduits != NULL)
		g_hash_table_foreach (priv->conduits, conduit_try_foreach, NULL);
	
	save_config_and_restart (priv->gpc, priv->state);
}

static void
gpcap_dispose (GObject *object)
{
	GnomePilotCapplet *gpcap = GNOME_PILOT_CAPPLET (object);
	GnomePilotCappletPrivate *priv;
	GtkObjectClass *gppd_class;
	
	priv = gpcap->priv;

	if (priv) {
		freePilotState (priv->orig_state);
		freePilotState (priv->state);

		g_hash_table_destroy (priv->conduits);

		g_object_unref (priv->xml);

		if (priv->pilots_popup)
			gtk_widget_destroy (priv->pilots_popup);

		if (priv->devices_popup)
			gtk_widget_destroy (priv->devices_popup);

		if (priv->conduit_popup)
			gtk_widget_destroy (priv->conduit_popup);

		/* destroy pilot dialog, if it was loaded */
		if (priv->pdialog) {
			gppd_class = gtk_type_class (gnome_pilot_pdialog_get_type());
			if (GTK_OBJECT_CLASS (gppd_class)->destroy)
				GTK_OBJECT_CLASS (gppd_class)->destroy (priv->pdialog);
		}
		
		g_free (priv);
		gpcap->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}
