/* 
 * Copyright (C) 1998-2001 Free Software Foundation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors : Vadim Strizhevsky
 *           Eskil Heyn Olsen
 */


#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <gnome.h>
#include <glade/glade.h>
#include <libgnomeui/gnome-window-icon.h>
#include <panel-applet-gconf.h>

#include <signal.h>

#include "gpilot-applet-progress.h"
#include "gpilotd/gnome-pilot-client.h"

enum {
  TARGET_URI_LIST,
};

typedef struct _pilot_properties pilot_properties;

struct _pilot_properties {
	GList* pilot_ids;
	GList* cradles;
	gchar * exec_when_clicked;
	gboolean popups;
};

typedef struct {
	gchar *cradle;
        gchar *backupdir;
	gchar *username;
	gint id;
} restore_properties;

typedef enum { INITIALISING, PAUSED, CONNECTING_TO_DAEMON, SYNCING, WAITING, NUM_STATES } state;

char *pixmaps[] = 
{
    GNOME_ICONDIR "/sync_broken.png",
    GNOME_ICONDIR "/sync_paused.png",
    GNOME_ICONDIR "/sync_icon.png",
    GNOME_ICONDIR "/syncing_icon.png",
    GNOME_ICONDIR "/sync_icon.png",
};


typedef struct {
	PanelApplet *applet;
	GtkTooltips *tooltips;
	pilot_properties properties; /* = { NULL }; */
	state curstate;
	GtkWidget *image; 
	GtkWidget *dialogWindow; 
	GtkWidget *operationDialogWindow;
	GtkWidget *pb;
	GtkTextBuffer *message_buffer;
	GPilotAppletProgress *c_progress;

	guint timeout_handler_id;
	gboolean double_clicked;
	gboolean druid_already_launched;

	GtkWidget *progressDialog;
	GtkWidget *sync_label;
	GtkWidget *overall_progress_bar;
	GtkWidget *conduit_progress_bar;
	GtkWidget *message_area;
	GtkWidget *cancel_button;
	GtkWidget *chooseDialog; 
	GtkWidget *restoreDialog;
	GdkColor  errorColor;
	gchar* glade_file;

	GnomePilotClient *gpc;
} PilotApplet;

#define PILOT_APPLET(x) ((PilotApplet*)(x))

static void show_error_dialog (PilotApplet *self, gchar*,...);
static void show_warning_dialog (PilotApplet *self, gchar*,...);
static void show_message_dialog (PilotApplet *self, gchar*,...);
static void cancel_cb (GtkButton* button, gpointer whatever);

static void save_properties (PilotApplet *self);
static void load_properties (PilotApplet *self);

static void pilot_draw (PilotApplet*);

static void pilot_execute_program (PilotApplet*, const gchar *);
static gchar *pick_pilot (PilotApplet *self);
static void response_cb (GtkDialog *dialog, gint id, gpointer data);

static gboolean timeout (PilotApplet *self);

#define GPILOTD_DRUID "gpilotd-control-applet --druid"
#define GPILOTD_CAPPLET "gpilotd-control-applet"
#define CONDUIT_CAPPLET "gpilotd-control-applet --cap-id=1"

/******************************************************************/

static void 
gpilotd_connect_cb (GnomePilotClient *client, 
		    const gchar *id,
		    const GNOME_Pilot_UserInfo *user,
		    gpointer user_data)
{
	GdkColormap *colormap;
	gchar *buf;
	PilotApplet *applet = PILOT_APPLET (user_data);

	gtk_tooltips_set_tip (applet->tooltips, GTK_WIDGET(applet->applet),
			("Synchronizing..."), NULL);
  
	if (!GTK_WIDGET_REALIZED (applet->image)) {
		g_warning ("! realized");
		return;
	}
	applet->curstate = SYNCING;
	pilot_draw (applet);

	if (applet->properties.popups == FALSE) return;

	if (applet->progressDialog == NULL) {
		gnome_window_icon_set_default_from_file (
				GNOME_ICONDIR "/sync_icon.png");
		GladeXML *xml              = glade_xml_new (applet->glade_file,"ProgressDialog",NULL);
		applet->progressDialog       = glade_xml_get_widget (xml,"ProgressDialog");
		applet->sync_label           = glade_xml_get_widget (xml,"sync_label");
		applet->message_area         = glade_xml_get_widget (xml,"message_area");
		applet->overall_progress_bar = glade_xml_get_widget (xml,"overall_progress_bar");
		applet->conduit_progress_bar = glade_xml_get_widget (xml,"conduit_progress_bar");
		applet->cancel_button        = glade_xml_get_widget (xml,"cancel_button");
		applet->message_buffer       = gtk_text_view_get_buffer(
				GTK_TEXT_VIEW(applet->message_area));

		gtk_signal_connect (GTK_OBJECT (applet->cancel_button),"clicked",
				   GTK_SIGNAL_FUNC (cancel_cb),applet);
	} else {
		gtk_text_buffer_set_text (applet->message_buffer, "", -1);
	}

	gtk_widget_set_sensitive (applet->cancel_button, FALSE);
	buf=g_strdup_printf (_("%s Synchronizing"),id);
	gtk_label_set_text (GTK_LABEL (applet->sync_label),buf);
	g_free (buf);
	gtk_widget_show_all (applet->progressDialog);

	gtk_progress_set_format_string (GTK_PROGRESS (applet->overall_progress_bar), _("Database %v of %u"));
	gtk_progress_set_format_string (GTK_PROGRESS (applet->conduit_progress_bar), "");
	gtk_progress_configure (GTK_PROGRESS (applet->overall_progress_bar),0 ,0, 1);
	gtk_progress_configure (GTK_PROGRESS (applet->conduit_progress_bar),0 ,0, 100.0);

	gpilot_applet_progress_set_progress (applet->c_progress, GTK_PROGRESS (applet->conduit_progress_bar));
	gpilot_applet_progress_start (applet->c_progress);

	colormap = gdk_window_get_colormap (applet->message_area->window);
	gdk_color_parse ("red",&(applet->errorColor));
	gdk_colormap_alloc_color (colormap,&(applet->errorColor),FALSE,TRUE);
}

static void 
gpilotd_disconnect_cb (GnomePilotClient *client, 
		       const gchar *id,
		       gpointer user_data)
{
	PilotApplet *applet = PILOT_APPLET (user_data);
	gtk_tooltips_set_tip (applet->tooltips, GTK_WIDGET(applet->applet),
			("Ready to synchronize"), NULL);

	applet->curstate = WAITING;
	pilot_draw (applet);
	if (applet->properties.popups && applet->progressDialog!=NULL) {
		gpilot_applet_progress_stop (applet->c_progress);
		gtk_widget_hide (applet->progressDialog);
	}
}

/* taken from http://live.gnome.org/GnomeLove/PanelAppletTutorial */
static void
applet_back_change (PanelApplet			*a,
		    PanelAppletBackgroundType	type,
		    GdkColor			*color,
		    GdkPixmap			*pixmap,
		    PilotApplet			*applet) 
{
        GtkRcStyle *rc_style;
        GtkStyle *style;

        /* reset style */
        gtk_widget_set_style (GTK_WIDGET (applet->applet), NULL);
        rc_style = gtk_rc_style_new ();
        gtk_widget_modify_style (GTK_WIDGET (applet->applet), rc_style);
        gtk_rc_style_unref (rc_style);

        switch (type) {
                case PANEL_COLOR_BACKGROUND:
                        gtk_widget_modify_bg (GTK_WIDGET (applet->applet),
                                        GTK_STATE_NORMAL, color);
                        break;

                case PANEL_PIXMAP_BACKGROUND:
                        style = gtk_style_copy (GTK_WIDGET (
						applet->applet)->style);
                        if (style->bg_pixmap[GTK_STATE_NORMAL])
                                g_object_unref
                                        (style->bg_pixmap[GTK_STATE_NORMAL]);
                        style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref
                                (pixmap);
                        gtk_widget_set_style (GTK_WIDGET (applet->applet),
                                        style);
                        g_object_unref (style);
                        break;

                case PANEL_NO_BACKGROUND:
                default:
                        break;
        }

}

static void gpilotd_scroll_to_insert_mark(PilotApplet *applet)
{
	gtk_text_view_scroll_to_mark(
			GTK_TEXT_VIEW(applet->message_area),
       			gtk_text_buffer_get_insert(applet->message_buffer),
			0.2,
			FALSE,
			0.0,
			0.0);
}

static void 
gpilotd_request_completed (GnomePilotClient *client,
			   const gchar *id,
			   unsigned long handle,
			   gpointer user_data)
{
	PilotApplet *applet = PILOT_APPLET (user_data);

	gtk_dialog_response (GTK_DIALOG (applet->operationDialogWindow), GTK_RESPONSE_CLOSE);
	if (applet->properties.popups && applet->progressDialog !=NULL) {
		gchar *txt=g_strdup_printf (_("Request %ld has been completed\n"),handle);
		gtk_text_buffer_insert_at_cursor (applet->message_buffer,txt,-1);
		g_free (txt);

		gpilotd_scroll_to_insert_mark(applet);
	}

/*	show_message_dialog ("Pilot %s has completed request %ld",
			    strlen (id)==0?"(noname)":id,
			    handle);
*/
}

static void 
gpilotd_conduit_start (GnomePilotClient* client, 
		       const gchar *id, 
		       const gchar *conduit, 
		       const gchar *db_name,
		       gpointer user_data) 
{ 
	PilotApplet *applet = PILOT_APPLET (user_data);
	if (applet->properties.popups && applet->progressDialog!=NULL) {
		gchar *txt=g_strdup_printf (_("%s Synchronizing : %s"),id, conduit);
		gtk_label_set_text (GTK_LABEL (applet->sync_label),txt);
		gtk_progress_configure (GTK_PROGRESS (applet->conduit_progress_bar), 0, 0, 100);
		gpilot_applet_progress_start (applet->c_progress);
		g_free (txt);
		txt=g_strdup_printf (_("%s: Started\n"),conduit);
		gtk_text_buffer_insert_at_cursor (applet->message_buffer,txt,-1);
		g_free (txt);
		gpilotd_scroll_to_insert_mark(applet);
	}
}

static void 
gpilotd_conduit_end (GnomePilotClient* client, 
		     const gchar *id, 
		     const gchar *conduit,
		     gpointer user_data) 
{
	PilotApplet *applet = PILOT_APPLET (user_data);
	if (applet->properties.popups && applet->progressDialog!=NULL) {
		gchar *txt=g_strdup_printf (_("%s Finished : %s"),id, conduit);
		gtk_progress_configure (GTK_PROGRESS (applet->conduit_progress_bar),100,0,100);
		gpilot_applet_progress_start (applet->c_progress);
		gtk_label_set_text (GTK_LABEL (applet->sync_label),txt);
		g_free (txt);
		txt=g_strdup_printf (_("%s: Ended\n"),conduit);
		gtk_text_buffer_insert_at_cursor (applet->message_buffer,txt,-1);
		g_free (txt);
		gtk_progress_set_format_string (GTK_PROGRESS (applet->conduit_progress_bar), "");
		gpilotd_scroll_to_insert_mark(applet);
	}
}

static void 
gpilotd_conduit_progress (GnomePilotClient* client, 
			  const gchar *id, 
			  const gchar *conduit, 
			  guint current, 
			  guint total,
			  gpointer user_data)
{
	PilotApplet *applet = PILOT_APPLET (user_data);
	gfloat tot_f,cur_f;
	tot_f = total;
	cur_f = current;
/*
        g_message ("%s : %s : %d/%d = %d%%",id, conduit, current, 
        total,abs (cur_f/(tot_f/100)));
*/
	if (applet->properties.popups && applet->progressDialog!=NULL) {
		gtk_progress_configure (GTK_PROGRESS (applet->conduit_progress_bar), cur_f, 1, tot_f); 
		gtk_progress_set_format_string (GTK_PROGRESS (applet->conduit_progress_bar), _("%v of %u records"));
		gpilot_applet_progress_stop (applet->c_progress);
	}
	g_main_iteration (FALSE);
}

static void 
gpilotd_overall_progress (GnomePilotClient* client, 
			  const gchar *id, 
			  guint current, 
			  guint total,
			  gpointer user_data)
{
	PilotApplet *applet = PILOT_APPLET (user_data);

	gfloat tot_f,cur_f;
	tot_f = total;
	cur_f = current;


	if (applet->properties.popups && applet->progressDialog!=NULL) {
		gtk_progress_configure (GTK_PROGRESS (applet->overall_progress_bar),cur_f,0,tot_f);
	}
	g_main_iteration (FALSE);
}

static void 
gpilotd_conduit_message (GnomePilotClient* client, 
			 const gchar *id, 
			 const gchar *conduit, 
			 const gchar *message,
			 gpointer user_data) 
{
	PilotApplet *applet = PILOT_APPLET (user_data);
	if (applet->properties.popups && applet->progressDialog != NULL) {
		gchar *txt=g_strdup_printf ("%s: %s\n",conduit,message);
		gtk_text_buffer_insert_at_cursor (applet->message_buffer,txt,-1);
		g_free (txt);
		gpilotd_scroll_to_insert_mark(applet);
	}
	g_main_iteration (FALSE);
}

static void 
gpilotd_daemon_message (GnomePilotClient* client, 
			const gchar *id, 
			const gchar *conduit,
			const gchar *message,
			gpointer user_data) 
{
	PilotApplet *applet = PILOT_APPLET (user_data);
	if (applet->properties.popups && applet->progressDialog != NULL) {
		gchar *txt=g_strdup_printf ("%s\n", message);
		gtk_text_buffer_insert_at_cursor (applet->message_buffer,txt,-1);
		g_free (txt);
		gpilotd_scroll_to_insert_mark(applet);
	}
	g_main_iteration (FALSE);
}

static void 
gpilotd_daemon_error (GnomePilotClient* client, 
		      const gchar *id, 
		      const gchar *message,
		      gpointer user_data) 
{
	PilotApplet *applet = PILOT_APPLET (user_data);
	if (applet->properties.popups && applet->progressDialog != NULL) {
		gchar *txt=g_strdup_printf ("Error: %s\n", message);
		gtk_text_buffer_insert_at_cursor (applet->message_buffer,txt,-1);
		g_free (txt);
		gpilotd_scroll_to_insert_mark(applet);
	}
}

static void
handle_client_error (PilotApplet *self)
{
	if (self->curstate == SYNCING) {
		show_warning_dialog (self, _("PDA is currently synchronizing.\nPlease wait for it to finish."));
	} else {
		self->curstate=INITIALISING;
		gtk_tooltips_set_tip (self->tooltips, GTK_WIDGET (self->applet),_("Not connected. Please restart daemon."), NULL);	
		pilot_draw (self);
		show_error_dialog (self, _("Not connected to gpilotd.\nPlease restart daemon."));
	}
}

static void
about_cb(BonoboUIComponent *uic, PilotApplet *pilot, const gchar *verbname)
{
	GtkWidget *about;
	const gchar *authors[] = {"Vadim Strizhevsky <vadim@optonline.net>",
				  "Eskil Heyn Olsen, <eskil@eskil.dk>",
				  "JP Rosevear <jpr@ximian.com>", 
				  "Chris Toshok <toshok@ximian.com>",
				  "Frank Belew <frb@ximian.com>",
				  "Matt Davey <mcdavey@mrao.cam.ac.uk>",
				  NULL};

	gnome_window_icon_set_default_from_file (
				GNOME_ICONDIR "/sync_icon.png");
	about = gnome_about_new (_("gnome-pilot applet"), 
				 VERSION,
				 _("Copyright 2000-2006 Free Software Foundation, Inc."),
				 _("A PalmOS PDA monitor.\n"),
				 (const gchar**)authors,
				 NULL,
				 "Translations by the GNOME Translation Project",
				 NULL /* pixbuf */);

	gtk_window_set_wmclass (
		GTK_WINDOW (about), "pilot", "Pilot");

	/* multi-headed displays... */
	gtk_window_set_screen (GTK_WINDOW (about),
		gtk_widget_get_screen (GTK_WIDGET (pilot->applet)));

	gtk_widget_show (about);

	return;
}


static gboolean
exec_on_click_changed_cb (GtkWidget *w, GdkEventFocus *event, gpointer data)
{
	PilotApplet *self = data;
	const gchar * new_list;

	new_list = gtk_entry_get_text (GTK_ENTRY (w));

	if (new_list) {
		g_free (self->properties.exec_when_clicked);
		self->properties.exec_when_clicked = g_strdup (new_list);
	}

	save_properties (self);

	return FALSE;
}

static void
toggle_notices_cb (GtkWidget *toggle, gpointer data)
{
	PilotApplet *self = data;
	self->properties.popups = GTK_TOGGLE_BUTTON (toggle)->active;
	save_properties (self);
}

static void
properties_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
	PilotApplet *self = user_data;
	GtkWidget *button, *entry, *dialog;
	GladeXML *xml;

	gnome_window_icon_set_default_from_file (GNOME_ICONDIR "/sync_icon.png");
	xml =glade_xml_new (self->glade_file,"PropertiesDialog", NULL);
	dialog=glade_xml_get_widget (xml,"PropertiesDialog");
	
	entry = glade_xml_get_widget (xml,"exec_entry");
	if (self->properties.exec_when_clicked)
		gtk_entry_set_text (GTK_ENTRY (entry), self->properties.exec_when_clicked);
	gtk_signal_connect (GTK_OBJECT (entry), "focus-out-event",
			    GTK_SIGNAL_FUNC (exec_on_click_changed_cb),
			    self);

  
	button = glade_xml_get_widget (xml,"notices_button");
	gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (button), self->properties.popups);
	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			    GTK_SIGNAL_FUNC (toggle_notices_cb),
			    self);

	g_signal_connect (GTK_OBJECT (dialog), "response",
			  G_CALLBACK (response_cb),
			  NULL);

	gtk_window_set_screen (GTK_WINDOW (dialog),
		gtk_widget_get_screen (GTK_WIDGET (self->applet)));

	gtk_widget_show_all (dialog);
}

static void
help_cb(BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
	PilotApplet *applet = user_data;
	GError *error = NULL;

	gnome_help_display_on_screen  ("gnome-pilot", "pilot-applet",
				       (gtk_widget_get_screen(GTK_WIDGET (applet->applet))),
				       &error);

	if (error)
	{
		GtkWidget *message_dialog;
		message_dialog = gtk_message_dialog_new (NULL, 0,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							 _("There was an error displaying help: %s"),
							 error->message);
		g_error_free (error);
		gtk_window_set_screen (GTK_WINDOW (message_dialog),
				       gtk_widget_get_screen (GTK_WIDGET (applet->applet)));
		gtk_dialog_run (GTK_DIALOG (message_dialog));
		gtk_widget_destroy (message_dialog);
	}

	return;
}

static void
pref_help_cb(GtkDialog *dialog)
{
	GError *error = NULL;
	gnome_help_display_on_screen  ("gnome-pilot", NULL /* FIXME Add section id later */,
				       gtk_widget_get_screen (GTK_WIDGET (dialog)),
				       &error);

	if (error)
	{
		GtkWidget *message_dialog;
		message_dialog = gtk_message_dialog_new (NULL, 0,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							 _("There was an error displaying help: %s"),
							 error->message);

		g_error_free (error);
		gtk_window_set_screen (GTK_WINDOW (message_dialog),
				       gtk_widget_get_screen (GTK_WIDGET (dialog)));
				       gtk_dialog_run (GTK_DIALOG (message_dialog));
		gtk_widget_destroy (message_dialog);
	}
	return;
}

static void
response_cb (GtkDialog *dialog, gint id, gpointer data)
{

	if (id == GTK_RESPONSE_HELP)
	{
		pref_help_cb (dialog);
		return;
	}

	gtk_widget_hide (GTK_WIDGET (dialog));
}


static void
complete_restore (GnomePilotClient* client, const gchar *id, 
		  unsigned long handle, gpointer user_data)
{	
	PilotApplet *applet = user_data;

	gtk_widget_destroy (applet->operationDialogWindow);
}

static void
cancel_restore (PilotApplet *self, GnomeDialog *w,gpointer data)
{
	g_message (_("cancelling %d"),GPOINTER_TO_INT (data));
	gnome_pilot_client_remove_request (self->gpc,GPOINTER_TO_INT (data));  
}


static void
restore_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
	PilotApplet *applet = user_data;
	int handle,i;
	gchar *pilot_name;
	gint pilot_id;
	GladeXML *xml;
	GtkWidget *dir_entry, *id_entry,*frame;
	GtkWidget *device_menu, *menu_item,*menu;
	GList *list;
	gchar *buf;
	restore_properties restore_props; 

	
	if (applet->curstate != WAITING) {
		handle_client_error (applet);
		return;
	}
	
	if ((pilot_name=pick_pilot (applet)) == NULL) {
		return;
	}

	if (gnome_pilot_client_get_pilot_base_dir_by_name (applet->gpc,pilot_name,&buf) == GPILOTD_OK) {
		restore_props.backupdir = g_strdup_printf ("%s/backup/",buf);
		g_free (buf);
	} else {
		handle_client_error (applet);
		return;
	}
	if (gnome_pilot_client_get_pilot_id_by_name (applet->gpc,pilot_name,&pilot_id) != GPILOTD_OK) {
		handle_client_error (applet);
		return;
	}

	xml = glade_xml_new (applet->glade_file,"RestoreDialog",NULL);
	applet->restoreDialog = glade_xml_get_widget (xml,"RestoreDialog");

	dir_entry = glade_xml_get_widget (xml,"dir_entry");

	gtk_entry_set_text (GTK_ENTRY (dir_entry),restore_props.backupdir);
	g_free (restore_props.backupdir);
	restore_props.backupdir=NULL;
	/* FIXME: do we need to preserve the backupdir somewhere? */
	
	frame = glade_xml_get_widget (xml,"main_frame");
	buf = g_strdup_printf (_("Restoring %s"), pilot_name);
	gtk_frame_set_label (GTK_FRAME (frame), buf);
	g_free (buf);
	

	id_entry =  glade_xml_get_widget (xml,"pilotid_entry");
	
	buf = g_strdup_printf ("%d",pilot_id);
	gtk_entry_set_text (GTK_ENTRY (id_entry),buf);
	g_free (buf);

	applet->properties.cradles=NULL;
	gnome_pilot_client_get_cradles (applet->gpc,&(applet->properties.cradles));
	list = applet->properties.cradles;
	
	device_menu =  glade_xml_get_widget (xml,"device_menu");
	menu =gtk_menu_new ();
	
	i=0;
	while (list){
		menu_item = gtk_menu_item_new_with_label ((gchar*)list->data);
		gtk_widget_show (menu_item);
/*
		gtk_signal_connect (GTK_OBJECT (menu_item),"activate",
				   GTK_SIGNAL_FUNC (activate_device),
				   (gchar*)list->data);
*/
		gtk_menu_append (GTK_MENU (menu),menu_item);
		list=list->next;
		i++;
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (device_menu),menu);	
	if (i<=1) gtk_widget_set_sensitive (GTK_WIDGET (device_menu),FALSE);
	else  gtk_widget_set_sensitive (GTK_WIDGET (device_menu),TRUE);
	
	if (gtk_dialog_run (GTK_DIALOG (applet->restoreDialog)) == GTK_RESPONSE_OK) {
		int id;
		
		restore_props.backupdir = g_strdup(gtk_entry_get_text (GTK_ENTRY (dir_entry)));
		gtk_widget_destroy (applet->restoreDialog);
		if ( restore_props.backupdir == NULL || 
		     strlen (restore_props.backupdir)==0) {
			show_warning_dialog (applet, _("No directory to restore from."));
			return;
		}
		
		/* FIXME: how do we specify which device to restore on? */
		id = gnome_pilot_client_connect__completed_request (applet->gpc, complete_restore, applet);
		if (gnome_pilot_client_restore (applet->gpc,pilot_name,restore_props.backupdir,GNOME_Pilot_IMMEDIATE,0, &handle) == GPILOTD_OK) {
			applet->operationDialogWindow = 
				gnome_message_box_new (_("Press synchronize on the cradle to restore\n" 
							 " or cancel the operation."),
						       GNOME_MESSAGE_BOX_GENERIC,
						       GNOME_STOCK_BUTTON_CANCEL,
						       NULL);
			gnome_dialog_button_connect (GNOME_DIALOG (applet->operationDialogWindow),0,
						     (GCallback) cancel_restore ,GINT_TO_POINTER (handle));
			gnome_dialog_run_and_close (GNOME_DIALOG (applet->operationDialogWindow));
		} else {
			show_warning_dialog (applet,_("Restore request failed"));
		}
		gtk_signal_disconnect (applet->gpc, id);		
	} else {
		gtk_widget_destroy (applet->restoreDialog);
	}	
}

static void
state_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
	PilotApplet *applet = user_data;
	GnomePilotClient *gpc = applet->gpc;

	if (applet->curstate==WAITING) {
		/* if we're unpaused */ 
		if (gnome_pilot_client_pause_daemon (gpc) != GPILOTD_OK) {		
			handle_client_error (applet);
		}
	}
	else {
		/* if we're paused */
		if (gnome_pilot_client_unpause_daemon (gpc) != GPILOTD_OK) {
			handle_client_error (applet);
		}
	}

	/* FIXME */
	return;
}


static void
restart_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
	PilotApplet *applet = user_data;

	if (applet->curstate == SYNCING) {
		if ( gnome_dialog_run_and_close (GNOME_DIALOG (gnome_question_dialog (_("PDA sync is currently in progress.\nAre you sure you want to restart daemon?"),NULL,NULL))) !=0 ) {
			return;
		}
		if (applet->progressDialog!=NULL) {
			gtk_widget_hide (applet->progressDialog);
		}
	}
	applet->curstate = INITIALISING;
	gnome_pilot_client_restart_daemon (applet->gpc);
	gtk_tooltips_set_tip (applet->tooltips, GTK_WIDGET (applet->applet),
			      _("Trying to connect to the GnomePilot Daemon"), NULL);
	applet->timeout_handler_id = gtk_timeout_add (1000,(GtkFunction)timeout,applet);

	return;
}

static void
log_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
	PilotApplet *applet = user_data;
	
	if (applet->progressDialog != NULL) {
		gtk_label_set_text (GTK_LABEL (applet->sync_label),"Last Sync Log");
		gtk_widget_set_sensitive (applet->cancel_button,TRUE);
		gtk_widget_show_all (applet->progressDialog);
	} else {
		show_message_dialog (applet, _("There's no last sync on record."));
	}
}

static const BonoboUIVerb pilot_applet_menu_verbs [] = {
	BONOBO_UI_VERB ("Restore", restore_cb),
	BONOBO_UI_VERB ("State", state_cb),
	BONOBO_UI_VERB ("Restart", restart_cb),
	BONOBO_UI_VERB ("Log", log_cb),
	BONOBO_UI_VERB ("Props", properties_cb),
	BONOBO_UI_VERB ("Help", help_cb),
	BONOBO_UI_UNSAFE_VERB ("About", about_cb),
	
	BONOBO_UI_VERB_END
};

static void
install_popup_menu (PilotApplet *self, gboolean on_off)
{
	char *menu_xml;
	/* ensure the label strings are extracted by intltool.  This list of
	 * strings should match the list in the menu_template below
	 */
#define FOO_STRING _("Restore...") _("Restart") _("Last log...") \
           _("Preferences...") _("Help") _("About")
#undef FOO_STRING
	const char *menu_template = 
		"<popup name=\"button3\">"
		"<menuitem name=\"Restore\" verb=\"Restore\" _label=\"Restore...\""
		"          pixtype=\"stock\" pixname=\"gtk-properties\"/>"
		"<menuitem name=\"State\" verb=\"State\" _label=\"%s\""
		"          pixtype=\"stock\" pixname=\"%s\"/>"
		"<menuitem name=\"Restart\" verb=\"Restart\" _label=\"Restart\""
		"          pixtype=\"stock\" pixname=\"gtk-execute\"/>"
		"<menuitem name=\"Log\" verb=\"Log\" _label=\"Last log...\""
		"          pixtype=\"stock\" pixname=\"gtk-new\"/>"
		"<separator/>"
		"<menuitem name=\"Props\" verb=\"Props\" _label=\"Preferences...\""
		"          pixtype=\"stock\" pixname=\"gtk-properties\"/>"
		"<menuitem name=\"Item 2\" verb=\"Help\" _label=\"Help\""
		"          pixtype=\"stock\" pixname=\"gtk-help\"/>"
		"<menuitem name=\"Item 3\" verb=\"About\" _label=\"About\""
		"          pixtype=\"stock\" pixname=\"gnome-stock-about\"/>"
		"</popup>";

	if (on_off)
		menu_xml = g_strdup_printf (menu_template, _("Continue"), "refresh");
	else
		menu_xml = g_strdup_printf (menu_template, _("Pause Daemon"), "stop");

	panel_applet_setup_menu ( self->applet,
				  menu_xml,
				  pilot_applet_menu_verbs,
				  self);

	g_free (menu_xml);
}

static void
gpilotd_daemon_pause (GnomePilotClient *client,
		      gboolean on_off,
		      gpointer user_data)
{
	PilotApplet *applet = PILOT_APPLET (user_data);

	if (on_off) {
		if (applet->curstate == WAITING) {
			applet->curstate = PAUSED;
			gtk_tooltips_set_tip (applet->tooltips, GTK_WIDGET (applet->applet),_("Daemon paused..."), NULL);
		}
		else
			handle_client_error (applet);
	}
	else {
		applet->curstate = WAITING;
		gtk_tooltips_set_tip (applet->tooltips, GTK_WIDGET (applet->applet), _("Ready to synchronize"), NULL);
	}

	install_popup_menu (applet, on_off);

	pilot_draw (applet);
	
	g_message ("paused");
}

static void 
gpilotd_conduit_error (GnomePilotClient* client, 
		       const gchar *id, 
		       const gchar *conduit, 
		       const gchar *message,
		       gpointer user_data) 
{
	PilotApplet *applet = PILOT_APPLET (user_data);
	if (applet->properties.popups && applet->progressDialog != NULL) {
		GtkTextIter *iter = NULL;
		gchar *txt=g_strdup_printf ("%s: %s\n",conduit,message);
		gtk_text_buffer_get_end_iter(applet->message_buffer, iter);

		gtk_text_buffer_insert_with_tags_by_name (applet->message_buffer,
							  iter,
							  txt,-1,
							  "foreground-gdk", 
							  applet->errorColor,
							  NULL);
		g_free (txt);
		gtk_text_iter_free (iter);
		gpilotd_scroll_to_insert_mark(applet);
	}
}

/******************************************************************/

static GList*
get_pilot_ids_from_gpilotd (PilotApplet *self) 
{
	GList *pilots=NULL;
	gnome_pilot_client_get_pilots (self->gpc,&pilots);
	return pilots;
}

#if 0
/* not currently used */
static char *
file_type (char *name) 
{
	static char command[128];
	char *tmp;
	FILE *f;

	if (!access (name,R_OK)) return NULL;

	tmp = tmpnam (NULL);
	snprintf (command,127,"file \"%s\" > %s",name,tmp);
	if (system (command)==-1) {
		return NULL;
	}
  
	if ((f = fopen (tmp,"rt"))==NULL) {
		return NULL;
	}

	fgets (command,128,f);
	fclose (f);
	unlink (tmp);

	tmp = (char*)strstr (command,": "); tmp+=2;
	return tmp;
}

static long 
file_size (char *name) 
{
	FILE *FS;
	long ret;

	if ((FS = fopen (name,"rt"))==NULL) {
		return 0;
	}
	fseek (FS,0L,SEEK_END);
	ret = ftell (FS);
	fclose (FS);
	return ret;
}
#endif
/******************************************************************/

static void 
pilot_draw (PilotApplet *self)
{
	if (!GTK_WIDGET_REALIZED (self->image)) {
		g_warning ("pilot_draw ! realized");
		return;
	}

	gtk_image_set_from_file(GTK_IMAGE(self->image), pixmaps[self->curstate]);
  
}

/******************************************************************/

static gboolean 
timeout (PilotApplet *self)
{
	if (self->curstate == INITIALISING) {
		g_message ("state = INITIALISING");
		pilot_draw (self);
		
		if (gnome_pilot_client_connect_to_daemon (self->gpc) == GPILOTD_OK) {
			self->curstate = CONNECTING_TO_DAEMON;
			pilot_draw (self);
		} 
	}
	
	if (self->curstate == CONNECTING_TO_DAEMON) {
		g_message ("state = CONNECTING_TO_DAEMON");
		
		if (self->properties.pilot_ids) {
			GList *tmp =self->properties.pilot_ids;
			while (tmp) {
				g_free ((gchar*)tmp->data);
				tmp = g_list_next (tmp);
			} 
			g_list_free (self->properties.pilot_ids);
		}

		self->properties.pilot_ids = get_pilot_ids_from_gpilotd (self);

		if (self->properties.pilot_ids==NULL){
			gtk_tooltips_set_tip (self->tooltips,
					      GTK_WIDGET(self->applet),
					      _("Not connected. Restart daemon to reconnect"),
					      NULL);
			if (self->druid_already_launched == FALSE) {
				self->druid_already_launched = TRUE;
				pilot_execute_program (self, GPILOTD_DRUID);
			}

			self->curstate=INITIALISING;
			pilot_draw (self);
			/* FIXME: add gpilot_monitor_state_change () */
		} else {
			gnome_pilot_client_monitor_on_all_pilots (self->gpc);
			
			gnome_pilot_client_notify_on (self->gpc,GNOME_Pilot_NOTIFY_CONNECT);
			gnome_pilot_client_notify_on (self->gpc,GNOME_Pilot_NOTIFY_CONDUIT);
			gnome_pilot_client_notify_on (self->gpc,GNOME_Pilot_NOTIFY_DISCONNECT);
			
			gtk_tooltips_set_tip (self->tooltips, 
					      GTK_WIDGET (self->applet),
					      _("Ready to synchronize"),
					      NULL);
			
			self->curstate = WAITING;
			pilot_draw (self);
		}
	} 

	switch (self->curstate) {
	case WAITING:
		/* this will happen once a sec */
		if (gnome_pilot_client_noop (self->gpc) == GPILOTD_ERR_NOT_CONNECTED) {
			self->curstate = INITIALISING;
		}
		break;
	case SYNCING: 
	case PAUSED:
	default:
		break;
	}

	/* Keep timeout calls coming */
	return TRUE;
}

static void 
show_error_dialog (PilotApplet *self, gchar *mesg,...) 
{
	char *tmp;
	va_list ap;

	va_start (ap,mesg);
	tmp = g_strdup_vprintf (mesg,ap);
	self->dialogWindow = gnome_message_box_new (tmp,GNOME_MESSAGE_BOX_ERROR,
						    GNOME_STOCK_BUTTON_OK,NULL);
	gnome_dialog_run_and_close (GNOME_DIALOG (self->dialogWindow));
	g_free (tmp);
	va_end (ap);
}

static void 
show_warning_dialog (PilotApplet *self, gchar *mesg,...) 
{
	char *tmp;
	va_list ap;

	va_start (ap,mesg);
	tmp = g_strdup_vprintf (mesg,ap);
	self->dialogWindow = gnome_message_box_new (tmp,GNOME_MESSAGE_BOX_WARNING,
						    GNOME_STOCK_BUTTON_OK,NULL);
	gnome_dialog_run_and_close (GNOME_DIALOG (self->dialogWindow));
	g_free (tmp);
	va_end (ap);
}


static void 
show_message_dialog (PilotApplet *self, char *mesg,...)
{
	char *tmp;
	va_list ap;

	va_start (ap,mesg);
	tmp = g_strdup_vprintf (mesg,ap);
	self->dialogWindow = gnome_message_box_new (tmp,GNOME_MESSAGE_BOX_GENERIC,
						    GNOME_STOCK_BUTTON_OK,NULL);
	gnome_dialog_run_and_close (GNOME_DIALOG (self->dialogWindow));
	g_free (tmp);
	va_end (ap);
}

static void
load_properties (PilotApplet *self)
{
	if (self->properties.exec_when_clicked) 
		g_free (self->properties.exec_when_clicked);
  
	self->properties.exec_when_clicked = 
		panel_applet_gconf_get_string ( self->applet,
						"exec_when_clicked",
						NULL);
	self->properties.popups = 
		panel_applet_gconf_get_bool ( self->applet,
					      "pop_ups",
					      NULL);
	/* self->properties.text_limit = */
	/*      panel_applet_gconf_get_int ( self->applet, */
	/*                                   "text_limit", */
	/*                                   &error ); */
}

static void
save_properties (PilotApplet *self)
{
	if (self->properties.exec_when_clicked)
	  panel_applet_gconf_set_string ( self->applet, 
					  "exec_when_clicked", 
					  self->properties.exec_when_clicked, 
					  NULL);
	panel_applet_gconf_set_bool( self->applet,
				     "pop_ups",
				     self->properties.popups,
				     NULL);
	/* gnome_config_set_int ("text_limit",properties.text_limit); */
}

static void
activate_pilot_menu (PilotApplet *self, GtkMenuItem *widget, gpointer data)
{
	gtk_object_set_data (GTK_OBJECT (self->chooseDialog),"pilot",data);	
}

static gchar*
pick_pilot (PilotApplet *self)
{
	gchar * pilot=NULL;
	if (self->properties.pilot_ids) {
		if (g_list_length (self->properties.pilot_ids)==1) {
                        /* there's only one */
			pilot = (gchar*)self->properties.pilot_ids->data;
		} else {
			GList *tmp;
			GtkWidget *optionMenu,*menuItem,*menu;
			if (self->chooseDialog == NULL) {
				GladeXML * xml;

				xml = glade_xml_new (self->glade_file,"ChoosePilot",NULL);
				self->chooseDialog = glade_xml_get_widget (xml,"ChoosePilot");
				optionMenu=glade_xml_get_widget (xml,"pilot_menu");
				gtk_object_set_data (GTK_OBJECT (self->chooseDialog),"pilot_menu",optionMenu);
			} else {
				optionMenu=gtk_object_get_data (GTK_OBJECT (self->chooseDialog),"pilot_menu");
			}
			menu = gtk_menu_new ();
	
			tmp=self->properties.pilot_ids;
			while (tmp!=NULL){
				menuItem = gtk_menu_item_new_with_label ((gchar*)tmp->data);
				gtk_widget_show (menuItem);
				gtk_signal_connect (GTK_OBJECT (menuItem),"activate",
						   GTK_SIGNAL_FUNC (activate_pilot_menu),
						   tmp->data);
				gtk_menu_append (GTK_MENU (menu),menuItem);
				tmp=tmp->next;
			}
			gtk_option_menu_set_menu (GTK_OPTION_MENU (optionMenu),menu);
			gtk_option_menu_set_history (GTK_OPTION_MENU (optionMenu),0);
			gtk_object_set_data (GTK_OBJECT (self->chooseDialog),"pilot",self->properties.pilot_ids->data);	
			if (gnome_dialog_run_and_close (GNOME_DIALOG (self->chooseDialog))==0) {
				pilot=(gchar *)gtk_object_get_data (GTK_OBJECT (self->chooseDialog),"pilot");
			} else {
				pilot=NULL;
			}
		}
	}

	return pilot;
}

static void
pilot_execute_program (PilotApplet *self, const gchar *str)
{
	g_return_if_fail (str != NULL);

	if (!g_spawn_command_line_async (str, NULL))
		show_warning_dialog (self, _("Execution of %s failed"),str);
}

static gint 
pilot_clicked_cb (GtkWidget *widget,
		  GdkEventButton * e, 
		  PilotApplet *self)
{
	if (e->button != 1)
		return FALSE;

	pilot_execute_program (self, self->properties.exec_when_clicked);
	return TRUE; 
}

static void 
cancel_cb (GtkButton* button,gpointer applet)
{
	if (PILOT_APPLET(applet)->progressDialog != NULL) {
		gtk_widget_hide (PILOT_APPLET(applet)->progressDialog);
	}
}


/*
static gint
pilot_timeout (gpointer data)
{
	GtkWidget *pixmap = data;

	gtk_widget_queue_draw (pixmap);
	return TRUE;
}
*/

/* Copied from glib 2.6 so we can work with older GNOME's */
static gchar **
extract_uris (const gchar *uri_list)
{
	GSList *uris, *u;
	const gchar *p, *q;
	gchar **result;
	gint n_uris = 0;

	uris = NULL;

	p = uri_list;

	/* We don't actually try to validate the URI according to RFC
	 * 2396, or even check for allowed characters - we just ignore
	 * comments and trim whitespace off the ends.  We also
	 * allow LF delimination as well as the specified CRLF.
	 *
	 * We do allow comments like specified in RFC 2483.
	 */
	while (p)
	{
		if (*p != '#')
		{
			while (g_ascii_isspace (*p))
				p++;

			q = p;
			while (*q && (*q != '\n') && (*q != '\r'))
				q++;

			if (q > p)
			{
				q--;
				while (q > p && g_ascii_isspace (*q))
					q--;

				if (q > p)
				{
					uris = g_slist_prepend (uris, g_strndup (p, q - p + 1));
					n_uris++;
				}
			}
		}
		p = strchr (p, '\n');
		if (p)
			p++;
	}

	result = g_new (gchar *, n_uris + 1);

	result[n_uris--] = NULL;
	for (u = uris; u; u = u->next)
		result[n_uris--] = u->data;

	g_slist_free (uris);

	return result;
}

static void
dnd_drop_internal (GtkWidget        *widget,
		   GdkDragContext   *context,
		   gint              x,
		   gint              y,
		   GtkSelectionData *selection_data,
		   guint             info,
		   guint             time,
		   gpointer          data)
{
	gchar *pilot_id;
	gint  file_handle;
	PilotApplet *self = PILOT_APPLET(data);
	gchar **names;
	int idx;

	pilot_id=pick_pilot (self);

	switch (info)
	{
	case TARGET_URI_LIST:
		names = extract_uris (selection_data->data);
		for (idx=0; names[idx]; idx++) {
			/*
			  if (strstr (file_type (ptr),"text")) {
			  if (file_size (ptr)>properties.text_limit)
			  g_message ("installing textfile as Doc (not implemented)");
				  else
				  g_message ("installing textfile as Memo (not implemented)");
				  }
			*/
			g_message (_("installing \"%s\" to \"%s\""), names[idx], pilot_id);
			if (pilot_id) {
				gnome_pilot_client_install_file (self->gpc,pilot_id,
								 names[idx],
								 GNOME_Pilot_PERSISTENT,0,&file_handle);
			} 
		}
		g_strfreev (names);
		break;
	default:
		g_warning (_("unknown dnd type"));
		break;
	}
}

static void
applet_destroy (GtkWidget *applet, PilotApplet *self)
{
	g_message (_("destroy gpilot-applet"));

	/* XXX free other stuff */
	g_free (self);
}

static void
create_pilot_widgets (GtkWidget *widget, PilotApplet *self) 
{ 
	GtkStyle *style; 
	int i; 

	static GtkTargetEntry drop_types [] = {  
		{ "text/uri-list", 0, TARGET_URI_LIST }, 
	}; 
	static gint n_drop_types = sizeof (drop_types) / sizeof (drop_types[0]); 

	style = gtk_widget_get_style (widget); 

	self->tooltips = gtk_tooltips_new ();

	gtk_tooltips_set_tip (GTK_TOOLTIPS (self->tooltips), widget,
			      _("Trying to connect to " 
				"the GnomePilot Daemon"),
			      NULL);

	self->c_progress = GPILOT_APPLET_PROGRESS (gpilot_applet_progress_new ()); 

	self->curstate = INITIALISING; 

	for (i = 0; i < sizeof (pixmaps)/sizeof (pixmaps[0]); i++) 
		pixmaps[i] = gnome_program_locate_file(
		    NULL, GNOME_FILE_DOMAIN_PIXMAP, pixmaps[i], TRUE, NULL);

	self->image = gtk_image_new_from_file (pixmaps[self->curstate]); 

	gtk_signal_connect (GTK_OBJECT (widget), "button-press-event", 
			    GTK_SIGNAL_FUNC (pilot_clicked_cb), self); 


	gtk_widget_show (self->image);   

	gtk_container_add (GTK_CONTAINER (widget), self->image);

	self->timeout_handler_id = gtk_timeout_add (1000,(GtkFunction)timeout,self); 

	gtk_signal_connect (GTK_OBJECT (widget),"destroy", 
			    GTK_SIGNAL_FUNC (applet_destroy),self); 

	gtk_signal_connect (GTK_OBJECT (widget),"change_background", 
			    GTK_SIGNAL_FUNC (applet_back_change),self); 

	/* FIXME */
	/* XXX change_orient */
	/* XXX change_size */

	/* Now set the dnd features */
	gtk_drag_dest_set (GTK_WIDGET (self->image), 
			   GTK_DEST_DEFAULT_ALL, 
			   drop_types, n_drop_types, 
			   GDK_ACTION_COPY); 


	gtk_signal_connect (GTK_OBJECT (self->image), 
			    "drag_data_received", 
			    GTK_SIGNAL_FUNC (dnd_drop_internal), 
			    self); 

	install_popup_menu (self, FALSE);
}

static gboolean
pilot_applet_fill (PanelApplet *applet)
{
	PilotApplet *self = g_new0 (PilotApplet, 1);

	self->applet = applet;

	/* initialize glade */
	glade_gnome_init ();
	self->glade_file="./pilot-applet.glade";
	if (!g_file_exists (self->glade_file)) {
		self->glade_file = g_concat_dir_and_file (GLADEDATADIR, "pilot-applet.glade");
	}
	if (!g_file_exists (self->glade_file)) {
		show_error_dialog (self, _("Cannot find pilot-applet.glade"));
		return -1;
	}

        panel_applet_add_preferences (self->applet, "/schemas/apps/gpilot_applet/prefs", NULL);

	load_properties (self);

	self->gpc = GNOME_PILOT_CLIENT (gnome_pilot_client_new ());
	
	gnome_pilot_client_connect__pilot_connect (self->gpc,
						   gpilotd_connect_cb,
						   self);

	gnome_pilot_client_connect__pilot_disconnect (self->gpc, 
						      gpilotd_disconnect_cb,
						      self);

	gnome_pilot_client_connect__completed_request (self->gpc,
						       gpilotd_request_completed,
						       self);
	
	gnome_pilot_client_connect__start_conduit (self->gpc, 
						   gpilotd_conduit_start,
						   self);

	gnome_pilot_client_connect__end_conduit(self->gpc,
						gpilotd_conduit_end,
						self);


	gnome_pilot_client_connect__progress_conduit (self->gpc, 
						      gpilotd_conduit_progress,
						      self);

	gnome_pilot_client_connect__progress_overall (self->gpc, 
						      gpilotd_overall_progress,
						      self);

	gnome_pilot_client_connect__error_conduit (self->gpc, 
						   gpilotd_conduit_error,
						   self);

	gnome_pilot_client_connect__message_conduit (self->gpc,
						     gpilotd_conduit_message,
						     self);

	gnome_pilot_client_connect__message_daemon (self->gpc,
						    gpilotd_daemon_message,
						    self);

	gnome_pilot_client_connect__error_daemon (self->gpc,
						  gpilotd_daemon_error,
						  self);

	gnome_pilot_client_connect__daemon_pause (self->gpc,
						  gpilotd_daemon_pause,
						  self);

	create_pilot_widgets (GTK_WIDGET (applet), self);

	gtk_widget_show_all(GTK_WIDGET (applet));

	return 0;
}

static gboolean
pilot_applet_factory ( PanelApplet *applet,
		       const gchar *iid,
		       gpointer     data)
{
	gboolean retval = FALSE;
	
	if (!strcmp (iid, "OAFIID:GNOME_PilotApplet"))
		retval = pilot_applet_fill (applet);

	return retval;
}
PANEL_APPLET_BONOBO_FACTORY( "OAFIID:GNOME_PilotApplet_Factory",
			     PANEL_TYPE_APPLET,
			     "gnome-pilot",
			     "0",
			     (PanelAppletFactoryCallback) pilot_applet_factory,
			     NULL)
			     
