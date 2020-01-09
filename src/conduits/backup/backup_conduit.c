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
 *          Robert Mibus
 */

/* $Id: backup_conduit.c 1524 2008-01-03 09:12:45Z mcdavey $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gnome.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-file.h>
#include <pi-dlp.h>
#include <pi-version.h>
#include <pi-file.h>
#include <pi-sync.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <utime.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>

#include <gpilotd/gnome-pilot-conduit-backup.h>
#include "backup_conduit.h"

#define DEBUG 1
#define pi_mktag(c1,c2,c3,c4) (((c1)<<24)|((c2)<<16)|((c3)<<8)|(c4))
#define MAXDBS 256
#define cardno 0

typedef struct file_db {
        gchar fname[256];
	struct DBInfo info;
        gint maxblock;
	gint entries;
} file_db;

#ifdef DEBUG_REC_IO
static char*
pi_unmktag (long type) {
	static char tag[5];
	tag[0] = (type >> 24) & 0xff;
	tag[1] = (type >> 16) & 0xff;
	tag[2] = (type >> 8) & 0xff;
	tag[3] = type & 0xff;
	tag[4] = 0;
	return tag;
}
#endif

GnomePilotConduit *conduit_get_gpilot_conduit (guint32 pilotId);
void conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit);
void error_dialog (GtkWindow *parent, gchar *mesg, ...);
gboolean check_base_directory (const gchar *dir_name);
GnomePilotConduit *conduit_load_gpilot_conduit (GPilotPilot *pilot);

static void 
load_configuration(GnomePilotConduit *conduit, 
		   ConduitCfg **c,
		   GPilotPilot *pilot)
{
	gchar *temp_name;
	gchar *prefix;
	gchar **exclude_files;
	gint num_of_exclude_files = 0;
	guint i;
	DIR *dir;
	struct dirent *entry;

	(*c) = g_new0(ConduitCfg,1);
	(*c)->child = -1;
	
	prefix = g_strdup_printf(CONFIG_PREFIX, pilot->pilot_id);
	
	gnome_config_push_prefix(prefix);
	(*c)->backup_dir = gnome_config_get_string("backup_dir");
	(*c)->updated_only = gnome_config_get_bool("updated_only=TRUE");
	(*c)->remove_deleted = gnome_config_get_bool("remove_deleted=FALSE");	
	(*c)->no_of_backups = gnome_config_get_int("no_of_backups=0");

	(*c)->exclude_files = NULL;
	gnome_config_get_vector("exclude_files",
				&num_of_exclude_files,
				&exclude_files);
	if(num_of_exclude_files) {
		for( i = 0; i < num_of_exclude_files ; i++ ) {
			(*c)->exclude_files = g_list_append( (*c)->exclude_files , 
							     g_strdup(exclude_files[i]));
			g_free(exclude_files[i]);
		}
		g_free(exclude_files);
	}
	gnome_config_pop_prefix();
	
	if ((*c)->backup_dir == NULL) {
		if (conduit != NULL && GNOME_IS_PILOT_CONDUIT (conduit)) {
			(*c)->backup_dir = g_strdup (gnome_pilot_conduit_get_base_dir (conduit));
		} 
		if ((*c)->backup_dir == NULL) {
			(*c)->backup_dir = g_strdup (g_get_home_dir ());
		}
	}

	if(mkdir((*c)->backup_dir,(mode_t)0755) < 0) { /* Wow, I never though I would
							  use octal in C :) */
		if(errno != EEXIST) {
			/* YECH! 
			   CONDUIT_CFG(c.gpilotd_methods)->log_error("Cannot open whatever...");
			*/
		}
		for (i=0;i < ((*c)->no_of_backups);i++) {
			temp_name = g_strdup_printf ("%s/%d", (*c)->backup_dir, i);
			mkdir (temp_name, (mode_t)0755);
			g_free (temp_name);
		}
		temp_name = g_strdup_printf ("%s/del", (*c)->backup_dir);
		mkdir (temp_name, (mode_t)0755);
		g_free (temp_name);
	}    

	if((*c)->backup_dir != NULL) {
		(*c)->files_in_backup = NULL;
		dir = opendir ((*c)->backup_dir);
		if (dir) {
			while ((entry = readdir (dir))) {
				if (entry->d_name) {
					if (strlen(entry->d_name) > 4) { /* ignores ., .., 0-9999 */
						temp_name = g_strdup_printf ("%s/%s", 
									     ((*c)->backup_dir), 
									     entry->d_name);
						(*c)->files_in_backup = g_list_prepend ((*c)->files_in_backup, 
											temp_name);
					}
				}
			}
			closedir (dir);
		}
	}

	(*c)->pilotId = pilot->pilot_id;

	g_free(prefix);
}

static void 
save_configuration(ConduitCfg *c) 
{
	gchar *prefix;
	const gchar **exclude = NULL;
	GList *iterator;
	guint i = 0;

	g_return_if_fail(c!=NULL);
       	
	prefix= g_strdup_printf("/gnome-pilot.d/backup-conduit/Pilot_%u/",c->pilotId);
	
	if( c->exclude_files != NULL ) {
	  iterator = c->exclude_files;
	  exclude = g_malloc( sizeof(char *) * (g_list_length(iterator)+1) );
	  for( i=0 ; iterator != NULL ; iterator = iterator->next, i++ ) {
	    exclude[i] = iterator->data;
	  }
	  exclude[i] = NULL;
	}
	gnome_config_push_prefix(prefix);
	gnome_config_set_string("backup_dir",c->backup_dir);
	gnome_config_set_bool("updated_only",c->updated_only);
	gnome_config_set_bool("remove_deleted",c->remove_deleted);
	gnome_config_set_int("no_of_backups",c->no_of_backups);
	gnome_config_set_vector("exclude_files", i, exclude);
	gnome_config_pop_prefix();
	gnome_config_sync();
	gnome_config_drop_all();
	g_free(prefix);
	g_free(exclude);
}

static void 
copy_configuration(ConduitCfg *d, ConduitCfg *c)
{
        g_return_if_fail(c!=NULL);
        g_return_if_fail(d!=NULL);
	if(d->backup_dir) g_free(d->backup_dir);
	d->backup_dir = g_strdup(c->backup_dir);
	d->remove_deleted = c->remove_deleted;
	d->updated_only = c->updated_only;
	d->pilotId = c->pilotId;
	d->child = c->child;
	if(d->exclude_files) g_list_free(d->exclude_files);
	d->exclude_files = g_list_copy(c->exclude_files);
	if(d->files_in_backup) g_list_free(d->files_in_backup);
	d->files_in_backup = g_list_copy(c->files_in_backup);
	d->no_of_backups = c->no_of_backups;
}

static ConduitCfg*
dupe_configuration(ConduitCfg *c) 
{
	ConduitCfg *d;
	g_return_val_if_fail(c!=NULL,NULL);
	d = g_new0(ConduitCfg,1);
	copy_configuration(d,c);
	return d;
}

/** this method frees all data from the conduit config */
static void 
destroy_configuration(ConduitCfg **c) 
{
	g_return_if_fail(c!=NULL);
	g_return_if_fail(*c!=NULL);

	if((*c)->remove_deleted) {
		GList *iterator;
		g_message (_("Checking for removed databases"));
		for (iterator = (*c)->files_in_backup; iterator; iterator = g_list_next (iterator)) {
			gchar *filename = (char*)iterator->data;
			gchar *backup_dirname;
			gchar *backup_backup;
			
			/* I18N note: this is printed when renaming a file %s */
			g_message (_("Renaming %s"), filename);

			backup_dirname = g_dirname (filename);
			backup_backup = g_strdup_printf ("%s/del/%s", backup_dirname, g_basename (filename));

			/* I18N note: this message follow a "Renaming %s". The
			   %s is a file name */
			g_message (_("to %s"), backup_backup);
			if (rename (filename, backup_backup) != 0) {
				g_message ("Error renaming");
			}

			g_free (backup_backup);
			g_free (backup_dirname);
			g_free (filename);
		}

		g_list_free((*c)->files_in_backup);
	}

	g_list_foreach ((*c)->exclude_files,(GFunc)g_free,NULL);
	g_list_free ((*c)->exclude_files);
	g_free ((*c)->backup_dir);
	g_free ((*c)->old_backup_dir);
	g_free (*c);
	*c = NULL;
}

/* Helper functions */
static void
protect_name(char *d, char *s) {
	while(*s) {
		switch(*s) {
		case '/': *(d++) = '='; *(d++) = '2'; *(d++) = 'F'; break;
		case '=': *(d++) = '='; *(d++) = '3'; *(d++) = 'D'; break;
		case '\x0A': *(d++) = '='; *(d++) = '0'; *(d++) = 'A'; break;
		case '\x0D': *(d++) = '='; *(d++) = '0'; *(d++) = 'D'; break;
#if 0
		case ' ': *(d++) = '='; *(d++) = '2'; *(d++) = '0'; break;
#endif
		default: *(d++) = *s;
		}
		++s;
	}
	*d = '\0';
}


static void
gnome_pilot_conduit_backup_remove_deleted (GnomePilotConduitBackup *conduit,
					   ConduitCfg *cfg,
					   const char *name)
{
	/* Remove file from files_in_backup list
	   if we're going to remove the db's not on the pilot */
	if (cfg->remove_deleted) {
		GList *iterator;
		for (iterator = cfg->files_in_backup; iterator; iterator = g_list_next (iterator)) {
			char *filename = (char*)iterator->data;
 			if (filename && strcasecmp(filename, name) == 0) {
				cfg->files_in_backup = g_list_remove_link (cfg->files_in_backup, 
									   iterator);
				g_free (filename);
				break;
			}
		}
	}
}

static void
gnome_pilot_conduit_backup_create_backup_of_backup (GnomePilotConduitBackup *conduit,
						    ConduitCfg *cfg,
						    const char *name)
{
	int i;
	char *backup_name_from=NULL, *backup_name_to=NULL;

	/* create backup of old db*/
	for (i = cfg->no_of_backups - 1;i >= 0; i--) {
		if (i) {
			backup_name_from = g_malloc (strlen (name) + 6);
			strcpy (backup_name_from, name);
			sprintf (strrchr (backup_name_from, '/'), "/%d/%s", i - 1, strrchr (name, '/') + 1);
		} else {
			backup_name_from = strdup (name);
		}

		backup_name_to = g_malloc (strlen (name) + 6);
		strcpy (backup_name_to, name);
		sprintf (strrchr (backup_name_to, '/'), "/%d/%s", i, strrchr (name, '/') + 1);

		if (access (backup_name_from, R_OK|W_OK)==0) {
			if (rename (backup_name_from, backup_name_to) == -1) {
				g_message ("Moving backup from %s to %s FAILED (%s)", 
					   backup_name_from, backup_name_to, 
					   strerror (errno));
			} else {
				g_message ("Moving backup from %s to %s", backup_name_from, backup_name_to);
			}
		}
        
		free (backup_name_from);
		free (backup_name_to);
	}
}

/*
  Create a full filename using the db's name (with character escaping
  and the config's backup_dir
*/
static char*
gnome_pilot_conduit_backup_create_name (GnomePilotConduitBackup *conduit, 
					GnomePilotDBInfo *dbinfo, 
					ConduitCfg *cfg)
{
	char *tmp_name;
	char *result;

	tmp_name = g_new0 (char, strlen (PI_DBINFO (dbinfo)->name) * 3);
	protect_name (tmp_name, PI_DBINFO (dbinfo)->name);
	
	if (PI_DBINFO (dbinfo)->flags & dlpDBFlagResource) {
		result = g_strdup_printf ("%s/%s.prc", cfg->backup_dir, tmp_name);
	} else {
		result = g_strdup_printf ("%s/%s.pdb", cfg->backup_dir, tmp_name);
	}
	g_free (tmp_name);
	return result;
}

/* This is the code that does a backup to a directory, most of the code is
   from the pilot-link package */
static gint
gnome_real_pilot_conduit_backup_backup (GnomePilotConduitBackup *conduit,
					GnomePilotDBInfo *dbinfo, 
					gpointer _cfg)
{
	char *name;
	struct pi_file *f = NULL;
	struct stat statb;
	struct utimbuf times;
	GList *iterator;
	int result = 0;
	ConduitCfg *cfg = (ConduitCfg*)_cfg;
	int index;
	int keep_reading;
	int entries;
	int err;
	PilotRecord remote;
	int wrote;
#ifdef PILOT_LINK_0_12
	pi_buffer_t *piBuf = NULL;
#else
	unsigned char *buffer = NULL;
#endif
	int len;

	g_return_val_if_fail (conduit != NULL, -1);
	g_return_val_if_fail (dbinfo != NULL, -1);
	g_return_val_if_fail (GNOME_IS_PILOT_CONDUIT_BACKUP (conduit), -1);
	
	/* let's see first if we should actually touch this db */
	for (iterator = cfg->exclude_files; iterator; iterator = g_list_next (iterator)) {
		if( ! g_strcasecmp(iterator->data, PI_DBINFO (dbinfo)->name) ) {
			g_message("excluded %s",PI_DBINFO (dbinfo)->name);
			result = 2;
			goto exit;
		}
	}

	if (!g_file_test (cfg->backup_dir, G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS)) {
		g_warning("backup conduit has no usable backupdir");
		gnome_pilot_conduit_send_error(GNOME_PILOT_CONDUIT(conduit),
					       "No usable backup directory specified");
		result = -1;
		goto exit;
	}

	name = gnome_pilot_conduit_backup_create_name (conduit, dbinfo, cfg);
	
	gnome_pilot_conduit_backup_remove_deleted (conduit, cfg, name);

	if (cfg->updated_only) {
		if (stat (name, &statb) == 0) {
			if (PI_DBINFO (dbinfo)->modifyDate == statb.st_mtime) {
				g_message(_("%s not modified since last sync"),
					  PI_DBINFO (dbinfo)->name);
#if 0
				gnome_pilot_conduit_send_message(GNOME_PILOT_CONDUIT(conduit),
								 _("%s not modified since last sync"),
								 PI_DBINFO (dbinfo)->name);
#endif
				
				result = 1;
				goto exit_and_free;
			}
		} 
	}

	PI_DBINFO (dbinfo)->flags &= 0xff;

	g_message(_("Making backup of %s"),PI_DBINFO (dbinfo)->name);
	gnome_pilot_conduit_send_message(GNOME_PILOT_CONDUIT(conduit),
					 _("Making backup of %s"),PI_DBINFO (dbinfo)->name);

	gnome_pilot_conduit_backup_create_backup_of_backup (conduit, cfg, name);

	/* backup new db */
	f = pi_file_create (name, PI_DBINFO (dbinfo));
	if(f == NULL) {
		g_warning(_("Could not create backup file %s"),name);
		gnome_pilot_conduit_send_error (GNOME_PILOT_CONDUIT(conduit),
						_("Could not create backup file %s"),name);
		result = -1;
		goto exit_and_free;
	}

	err = dlp_OpenDB (dbinfo->pilot_socket, 
			  cardno,
			  dlpOpenRead,
			  PI_DBINFO (dbinfo)->name,
			  &dbinfo->db_handle);
	
	if (err < 0) {
		g_warning ("error (%s) in opening '%s'", dlp_strerror (err), PI_DBINFO (dbinfo)->name);
		result = -1;
		/* DB not successfully opened, so don't try and close (caused
		 * error on Tungsten E2) probably cause of Ubuntu bug #81396
		 */
		goto exit_and_free; 
	}

	err = dlp_ReadOpenDBInfo (dbinfo->pilot_socket, 
				  dbinfo->db_handle,
				  &entries);
				    
	if (err < 0) {
		g_warning ("error (%s) in reading '%s'", dlp_strerror (err), PI_DBINFO (dbinfo)->name);
		result = -1;
		goto db_close;
	}


#ifdef PILOT_LINK_0_12
       piBuf = pi_buffer_new (0xffff);
       len = dlp_ReadAppBlock (dbinfo->pilot_socket,
                               dbinfo->db_handle,
                               0,
                               -1,
                               piBuf);
#else
       buffer = g_new0 (unsigned char, 0xffff);
       remote.buffer = buffer;

       len = dlp_ReadAppBlock (dbinfo->pilot_socket,
			       dbinfo->db_handle,
			       0,
			       buffer,
			       0xffff);
#endif

       if (len > 0)
#ifdef PILOT_LINK_0_12
               pi_file_set_app_info (f, piBuf->data, len);
#else
	       pi_file_set_app_info (f, buffer, len);
#endif

	index = 0;
	keep_reading = 1;
	wrote = 0;
	do {
		unsigned long type;
		int id;
			
		if (PI_DBINFO (dbinfo)->flags & dlpDBFlagResource) {
#ifdef PILOT_LINK_0_12
			keep_reading = (dlp_ReadResourceByIndex (dbinfo->pilot_socket, 
								 dbinfo->db_handle,
								 index, 
								 piBuf,
								 &type,
								 &id) >= 0);
#else
                        keep_reading = (dlp_ReadResourceByIndex (dbinfo->pilot_socket,
                                                                 dbinfo->db_handle,
                                                                 index,
								 remote.buffer,
                                                                 &type,
								 &id,
								 &remote.len) >= 0);
			
#endif

#ifdef DEBUG_REC_IO
			g_message ("read resource %d, type = %s, size %d, , index %d/%d",
#ifdef PILOT_LINK_0_12
				   id, pi_unmktag (type), piBuf->used,
#else
				   id, pi_unmktag (type), remote.len,
#endif
				   index, entries);
#endif
			if (keep_reading > 0) {
#ifdef PILOT_LINK_0_12
				err = pi_file_append_resource (f,
							       piBuf->data,
							       piBuf->used,
							       type,
							       id);
#else
                               err = pi_file_append_resource (f,
                                                              remote.buffer,
                                                              remote.len,
							      type,
							      id);
#endif
				if (err < 0) {
					g_warning ("error in writing to file");
				} else {
					wrote++;
#ifdef DEBUG_REC_IO
					g_message ("write resource %d, type = %s, size %d, index %d/%d",
#ifdef PILOT_LINK_0_12
						   id, pi_unmktag (type), piBuf->used,
#else
						   id, pi_unmktag (type), remote.len,
#endif
						   index, entries);
#endif
				}
			}
		} else {
#ifdef PILOT_LINK_0_12
			keep_reading = (dlp_ReadRecordByIndex (dbinfo->pilot_socket, 
							       dbinfo->db_handle,
							       index, 
							       piBuf,
							       &remote.recID,
							       &remote.flags,
							       &remote.catID) >= 0);
#else
                        keep_reading = (dlp_ReadRecordByIndex (dbinfo->pilot_socket,
                                                               dbinfo->db_handle,
                                                               index,
							       remote.buffer,
                                                               &remote.recID,
							       &remote.len,
                                                               &remote.flags,
                                                               &remote.catID) >= 0);

#endif

#ifdef DEBUG_REC_IO
			g_message ("read record %d, size %d, index %d/%d",
#ifdef PILOT_LINK_0_12
				   remote.recID, piBuf->used,
#else
				   remote.recID, remote.len,
#endif
				   index, entries);
#endif

			if (keep_reading > 0) {
#ifdef PILOT_LINK_0_12
				err = pi_file_append_record (f, 
							     piBuf->data, 
							     piBuf->used, 
							     remote.flags,
							     remote.catID,
							     remote.recID);
#else
                                err = pi_file_append_record (f,
							     remote.buffer,
							     remote.len,
                                                             remote.flags,
                                                             remote.catID,
                                                             remote.recID);

#endif
				if (err < 0) {
					g_warning ("error in writing to file");
				} else {
					wrote++;
#ifdef DEBUG_REC_IO
					g_message ("write record %d, size %d, index %d/%d",
#ifdef PILOT_LINK_0_12
						   id, piBuf->used,
#else
						   id, remote.len,
#endif
						   index, entries);
#endif
				}

			}
				
		}			
		index++;
		gnome_pilot_conduit_send_progress (GNOME_PILOT_CONDUIT (conduit),
						   entries,
						   index);
	} while ((keep_reading > 0) && (index < entries));

	if (pi_file_close (f) < 0) {
		g_warning("backup conduit can't write file");
		gnome_pilot_conduit_send_error(GNOME_PILOT_CONDUIT(conduit),
					       "Unable to write file to backup directory specified");
		result = -1;
		f = NULL;
		goto db_close;
	}
	f = NULL;
	
	g_message ("Wrote %d of %d %s, which is %s",
		   wrote, entries, PI_DBINFO (dbinfo)->flags & dlpDBFlagResource ? "resources" : "records",
		   wrote == entries ? "good" : "BAD");

	times.actime = PI_DBINFO (dbinfo)->createDate;
	times.modtime = PI_DBINFO (dbinfo)->modifyDate;
	utime (name, &times);
 db_close:
	dlp_CloseDB (dbinfo->pilot_socket, dbinfo->db_handle);
 exit_and_free:
	if(f != NULL)
		pi_file_close(f);
	g_free (name);
#ifdef PILOT_LINK_0_12
	if (piBuf) {
		pi_buffer_free (piBuf);
	}
#else
	g_free (buffer);
#endif
 exit:	
	return result;	
}

static int 
compare(file_db *d1, file_db *d2)
{
	/* types of 'appl' sort later then other types */
	if(d1->info.creator == d2->info.creator) {
		if(d1->info.type != d2->info.type) {
			if(d1->info.type == pi_mktag('a','p','p','l'))
				return 1;
			if(d2->info.type == pi_mktag('a','p','p','l'))
				return -1;
		}
	}
	return d1->maxblock < d2->maxblock;
}

/* This is the code that does a restore from a directory, most of the code is
   from the pilot-link package */
static gint
gnome_real_pilot_conduit_backup_restore (GnomePilotConduitBackup *conduit, 
					 int psock, 
					 char *src_dir,
					 GnomePilotConduitBackupRestore func,
					 gpointer func_data,
					 gpointer _cfg)
{
	DIR *dir;
	struct dirent *dirent;
	file_db **db;
	int dbcount = 0;
	int i, j, size;
	struct pi_file *file;
	char *source;
	int result = 0;

	ConduitCfg *cfg = (ConduitCfg*)_cfg;

	if (src_dir == NULL) {
		source = cfg->backup_dir;
	} else {
		source = src_dir;
	}

	g_return_val_if_fail (source != NULL, -1);

	dir = opendir (source);

	db = g_new0 (file_db*, MAXDBS);

	gnome_pilot_conduit_send_message (GNOME_PILOT_CONDUIT (conduit), _("Collecting restore information..."));

	/* Load all files in directory */
	while( (dirent = readdir(dir)) ) {

#ifdef DEBUG
		printf ("checking %s/%s\n", source, dirent->d_name);
#endif

		/* skip dotfiles. */
		if (dirent->d_name[0] == '.')
			continue;

		db[dbcount] = g_new0 (file_db, 1);
		g_snprintf (db[dbcount]->fname, 255, "%s/%s", source, dirent->d_name);

		file = pi_file_open (db[dbcount]->fname);
		if (file == 0) {
#ifdef DEBUG
			printf("backup_conduit: Unable to open '%s'!\n", db[dbcount]->fname);
#endif
			/* Skip this file */
			continue;

		}

		/* Load db info */
		pi_file_get_info (file, &db[dbcount]->info);

		db[dbcount]->maxblock = 0;

		/* Get number of records */
		pi_file_get_entries(file, &db[dbcount]->entries);

		/* Find biggest record */
		for (i = 0; i < db[dbcount]->entries; i++) {
			if (db[dbcount]->info.flags & dlpDBFlagResource)
				pi_file_read_resource(file, i, 0, &size, 0, 0);
			else
				pi_file_read_record(file, i, 0, &size, 0, 0,0 );

			if (size > db[dbcount]->maxblock)
				db[dbcount]->maxblock = size;
		}
			
		pi_file_close (file);
		dbcount++;
	}

	closedir(dir);

	/* Sort db's */
	for (i=0;i<dbcount;i++) {
		for (j=i+1;j<dbcount;j++)
			if (compare(db[i],db[j])>0) {
				file_db *temp = db[i];
				db[i] = db[j];
				db[j] = temp;
			}
	}

	for (i=0;i<dbcount;i++) {
		int err = func (db[i]->fname, i + 1, dbcount, func_data);
		if (err<0) {
			result = err;
		}
        }

#ifdef DEBUG
	fprintf (stderr,"backup_conduit: Restore done\n");
#endif

	for(i=0;i<dbcount;i++){
		g_free(db[i]);
	}

	g_free(db);

	return result;
}

/*
 * Gui Configuration Code
 */
static void
insert_dir_callback (GtkEditable *editable, const gchar *text,
		     gint len, gint *position, void *data)
{
	gint i;
	const gchar *curname;

	curname = gtk_entry_get_text(GTK_ENTRY(editable));
	if (*curname == '\0' && len > 0) {
		if (isspace(text[0])) {
			gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), "insert_text");
			return;
		}
	} else {
		for (i=0; i<len; i++) {
			if (isspace(text[i])) {
				gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), 
							     "insert_text");
				return;
			}
		}
	}
}

static GtkWidget
*createCfgWindow(GnomePilotConduit* conduit)
{
	GtkWidget *vbox, *table;
	GtkWidget *entry, *label;
	GtkWidget *button, *spin;
	GtkObject *adjustment;
	AtkObject *atk_widget;

	vbox = gtk_vbox_new(FALSE, GNOME_PAD);

	table = gtk_table_new(2, 4, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, GNOME_PAD);

	label = gtk_label_new_with_mnemonic(_("_Backup directory:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
    
	entry = gtk_entry_new_with_max_length(128);
	gtk_object_set_data(GTK_OBJECT(vbox), "dir", entry);
	gtk_table_attach_defaults(GTK_TABLE(table), entry, 1, 2, 0, 1);
	gtk_signal_connect(GTK_OBJECT(entry), "insert_text",
			   GTK_SIGNAL_FUNC(insert_dir_callback),
			   NULL);

	gtk_label_set_mnemonic_widget (GTK_LABEL(label), entry);
	atk_widget = gtk_widget_get_accessible (entry);
	atk_object_set_name (atk_widget, _("Backup directory"));

	label = gtk_label_new_with_mnemonic (_("O_nly backup changed bases"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1,2);

	button = gtk_check_button_new();
	gtk_object_set_data(GTK_OBJECT(vbox), "only_changed", button);
	gtk_table_attach_defaults(GTK_TABLE(table), button, 1, 2, 1,2);

	gtk_label_set_mnemonic_widget (GTK_LABEL(label), button);
	atk_widget = gtk_widget_get_accessible (button);
	atk_object_set_name (atk_widget, _("Only backup changed bases"));

	label = gtk_label_new_with_mnemonic(_("_Remove local base if deleted on pilot"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2,3);

	button = gtk_check_button_new();
	gtk_object_set_data(GTK_OBJECT(vbox), "remove_local", button);
	gtk_table_attach_defaults(GTK_TABLE(table), button, 1, 2, 2,3);

	gtk_label_set_mnemonic_widget (GTK_LABEL(label), button);
	atk_widget = gtk_widget_get_accessible (button);
	atk_object_set_name (atk_widget, _("Remove local base if deleted on pilot"));

	label = gtk_label_new_with_mnemonic (_("# of old backups to _keep"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3,4);

	adjustment = gtk_adjustment_new (0, 0, 100, 1, 1, 1);
	spin = gtk_spin_button_new (GTK_ADJUSTMENT(adjustment), 1, 0);
	gtk_object_set_data(GTK_OBJECT(vbox), "no_of_backups", adjustment);
	gtk_table_attach_defaults(GTK_TABLE(table), spin, 1, 2, 3, 4);

	gtk_label_set_mnemonic_widget (GTK_LABEL(label), spin);
	atk_widget = gtk_widget_get_accessible (spin);
	atk_object_set_name (atk_widget, _("# of old backups to keep"));

	return vbox;
}

void
error_dialog (GtkWindow *parent, gchar *mesg, ...)
{
        GtkWidget *dlg;
        char *tmp;
        va_list ap;

        va_start (ap,mesg);
        tmp = g_strdup_vprintf (mesg,ap);

        dlg = gtk_message_dialog_new (parent, GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, tmp);
        gtk_dialog_run (GTK_DIALOG (dlg));
        gtk_widget_destroy (dlg);

        va_end (ap);
        g_free (tmp);
}

gboolean
check_base_directory (const gchar *dir_name)
{
        gboolean ret = TRUE;
        /* check basedir validity */

        if (mkdir (dir_name, 0700) < 0 ) {
                struct stat buf;
                gchar *errstr;
                switch (errno) {
                case EEXIST:
                        stat (dir_name, &buf);
                        if (S_ISDIR (buf.st_mode)) {
                                if (!(buf.st_mode & (S_IRUSR | S_IWUSR |S_IXUSR))) {
                                        error_dialog (NULL, _("The specified backup directory exists but has the wrong permissions.\n"
                                                        "Please fix or choose another directory"));
                                        ret = FALSE;
                                }
                        } else {
                                error_dialog (NULL, _("The specified backup directory exists but is not a directory.\n"
                                                "Please make it a directory or choose another directory"));
                                ret = FALSE;
                        }
                        break;

                case EACCES:
                        error_dialog (NULL, _("It wasn't possible to create the specified backup directory.\n"
                                        "Please verify the permissions on the specified path or choose another directory"));
                        ret = FALSE;
			break;
                case ENOENT:
                        error_dialog (NULL, _("The path specified for the backup directory is invalid.\n"
                                        "Please choose another directory"));
                        ret = FALSE;
                        break;
                default:
                        errstr = strerror (errno);
                        error_dialog (NULL, errstr);
                        ret = FALSE;
                }
        }
        return ret;
}

static void
setOptionsCfg(GtkWidget *pilotcfg, ConduitCfg *state)
{
	GtkWidget *dir,*updated_only,*remove_deleted;
	GtkObject *adj;

	dir  = gtk_object_get_data(GTK_OBJECT(pilotcfg), "dir");
	updated_only = gtk_object_get_data(GTK_OBJECT(pilotcfg), "only_changed");
	remove_deleted = gtk_object_get_data(GTK_OBJECT(pilotcfg), "remove_local");
	adj = gtk_object_get_data(GTK_OBJECT(pilotcfg), "no_of_backups");

	g_assert(dir!=NULL);
	g_assert(updated_only!=NULL);
	g_assert(remove_deleted!=NULL);
	g_assert(adj!=NULL);

	state->old_backup_dir = g_strdup(state->backup_dir);
	gtk_entry_set_text(GTK_ENTRY(dir), state->old_backup_dir);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(updated_only), state->updated_only);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(remove_deleted), state->remove_deleted);
	gtk_adjustment_set_value (GTK_ADJUSTMENT(adj), state->no_of_backups);
}


static void
readOptionsCfg(GtkWidget *pilotcfg, ConduitCfg *state)
{
	GtkWidget *dir,*updated_only,*remove_deleted;
	GtkObject *adj;

	dir  = gtk_object_get_data(GTK_OBJECT(pilotcfg), "dir");
	updated_only = gtk_object_get_data(GTK_OBJECT(pilotcfg), "only_changed");
	remove_deleted = gtk_object_get_data(GTK_OBJECT(pilotcfg), "remove_local");
	adj = gtk_object_get_data(GTK_OBJECT(pilotcfg), "no_of_backups");

	if(state->backup_dir)
		g_free(state->backup_dir);	
	if(check_base_directory ( gtk_entry_get_text (GTK_ENTRY (dir)))){
		state->backup_dir = g_strdup(gtk_entry_get_text(GTK_ENTRY(dir)));
	} else {
		state->backup_dir = g_strdup(state->old_backup_dir);
		gtk_entry_set_text(GTK_ENTRY(dir), state->old_backup_dir);
	}
	state->updated_only = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(updated_only));
	state->remove_deleted = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(remove_deleted));
	state->no_of_backups = GTK_ADJUSTMENT(adj)->value;
}

static gint
create_settings_window (GnomePilotConduit *conduit, GtkWidget *parent, gpointer data)
{
	GtkWidget *cfgWindow;

	cfgWindow = createCfgWindow(conduit);
	gtk_container_add(GTK_CONTAINER(parent),cfgWindow);
	gtk_widget_show_all(cfgWindow);

	gtk_object_set_data(GTK_OBJECT(conduit),OBJ_DATA_CONFIG_WINDOW,cfgWindow);
	setOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONDUIT_CFG(conduit));

	return 0;
}

static void
display_settings (GnomePilotConduit *conduit, gpointer data)
{
	setOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONDUIT_CFG(conduit));
}

static void
save_settings    (GnomePilotConduit *conduit, gpointer data)
{
	readOptionsCfg(GET_CONDUIT_WINDOW(conduit),GET_CONDUIT_CFG(conduit));
	save_configuration(GET_CONDUIT_CFG(conduit));
}

static void
revert_settings  (GnomePilotConduit *conduit, gpointer data)
{
	ConduitCfg *cfg,*cfg2;

	cfg2= GET_CONDUIT_OLDCFG(conduit);
	cfg = GET_CONDUIT_CFG(conduit);
	save_configuration(cfg2);
	copy_configuration(cfg,cfg2);
	setOptionsCfg(GET_CONDUIT_WINDOW(conduit),cfg);
}

GnomePilotConduit *
conduit_load_gpilot_conduit (GPilotPilot *pilot)
{
	ConduitCfg *cfg, *cfg2;
	GnomePilotConduitBackup *retval;

	retval = GNOME_PILOT_CONDUIT_BACKUP(gnome_pilot_conduit_backup_new (pilot));
	g_assert (retval != NULL);

	load_configuration(GNOME_PILOT_CONDUIT (retval), &cfg, pilot);

	cfg2 = dupe_configuration(cfg);

	gtk_object_set_data(GTK_OBJECT(retval),OBJ_DATA_CONFIG,cfg);
	gtk_object_set_data(GTK_OBJECT(retval),OBJ_DATA_OLDCONFIG,cfg2);

	gtk_object_set_data(GTK_OBJECT(retval),
			    "configuration",
			    cfg);

	gnome_pilot_conduit_backup_connect__backup (retval, gnome_real_pilot_conduit_backup_backup, 
					    cfg);
	gnome_pilot_conduit_backup_connect__restore (retval, gnome_real_pilot_conduit_backup_restore, 
					     cfg);
	gnome_pilot_conduit_connect__create_settings_window (GNOME_PILOT_CONDUIT(retval),
	    create_settings_window, NULL);
	gnome_pilot_conduit_connect__display_settings (GNOME_PILOT_CONDUIT(retval),
	    display_settings, NULL);
	gnome_pilot_conduit_connect__save_settings (GNOME_PILOT_CONDUIT(retval),
	    save_settings, NULL);
	gnome_pilot_conduit_connect__revert_settings (GNOME_PILOT_CONDUIT(retval),
	    revert_settings, NULL);	

	return GNOME_PILOT_CONDUIT (retval);
}

void
conduit_destroy_gpilot_conduit (GnomePilotConduit *conduit)
{
	ConduitCfg *cfg;
	cfg = GET_CONDUIT_CFG(conduit);
	destroy_configuration(&cfg);
	gtk_object_destroy (GTK_OBJECT (conduit));
}

