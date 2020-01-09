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

#include "gpilot-gui.h"
#include <gnome.h>

static void gpilot_gui_run_dialog(gchar *type, gchar *mesg, va_list ap);

static void
gpilot_gui_run_dialog(gchar *type, gchar *mesg, va_list ap)
{
	char *tmp;
	GtkWidget *dialogWindow;

	tmp = g_strdup_vprintf(mesg,ap);
#if 0
	dialogWindow = gnome_message_box_new(tmp,type,GNOME_STOCK_BUTTON_OK,NULL);
	gnome_dialog_run_and_close(GNOME_DIALOG(dialogWindow));
#endif

	if (strcmp(type,GNOME_MESSAGE_BOX_WARNING)==0) {
		gnome_warning_dialog(tmp);
	} else if (strcmp(type,GNOME_MESSAGE_BOX_ERROR)==0) {
		dialogWindow = gnome_message_box_new(tmp,type,GNOME_STOCK_BUTTON_OK,NULL);
		gnome_dialog_run(GNOME_DIALOG(dialogWindow));
		exit(-1);
	}

	g_free(tmp);
	va_end(ap);
}

void 
gpilot_gui_warning_dialog (gchar *mesg, ...) 
{
	va_list args;

	va_start (args, mesg);
	gpilot_gui_run_dialog (GNOME_MESSAGE_BOX_WARNING, mesg, args);
	va_end (args);
}

void 
gpilot_gui_error_dialog (gchar *mesg, ...) 
{
	va_list args;

	va_start (args, mesg);
	gpilot_gui_run_dialog (GNOME_MESSAGE_BOX_ERROR, mesg, args);
	va_end (args);
}

static void
gpilot_gui_restore_callback (gint reply,
			     gpointer data)
{
	gboolean *result = (gboolean*)data;
	if (reply == 0 /* YES */) {
		(*result) = TRUE;
	} else if (reply == 1 /* NO */) {
		(*result) = FALSE;
	} else {
		g_assert_not_reached ();
	}
}

GPilotPilot* 
gpilot_gui_restore (GPilotContext *context, 
		    GPilotPilot *pilot)
{
	GPilotPilot *result = NULL;
	GtkWidget *d;

	if (pilot) {
		gboolean q_result = FALSE;
		char *tmp;
		tmp = g_strdup_printf ("Restore %s' pilot with id %d\n"
				       "and name `%s'",
				       pilot->pilot_username,
				       pilot->pilot_id, 
				       pilot->name);
		d = gnome_question_dialog_modal (tmp,
						 gpilot_gui_restore_callback,
						 GINT_TO_POINTER (&q_result));
		gnome_dialog_run (GNOME_DIALOG (d));
		g_free (tmp);

		if (q_result == TRUE) {
			result = pilot;
		} else {
			
		}
	} else {
		gpilot_gui_warning_dialog ("no ident\n"
					   "restoring pilot with ident\n"
					   "exciting things will soon be here...\n");
	}
	return result;
}
