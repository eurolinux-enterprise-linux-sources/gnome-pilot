NULL=

libexec_PROGRAMS = gpilot-applet

GOB_BUILT_SRCS = 				\
	gpilot-applet-progress.c		\
	gpilot-applet-progress.h		\
	gpilot-applet-progress-private.h	\
	$(NULL)

gobfiles = 					\
	$(srcdir)/gpilot-applet-progress.gob 	\
	$(NULL)

%.c %.h: %.gob
	$(GOB2) --always-private-header -w $<

gpilot_applet_SOURCES =		\
	$(GOB_BUILT_SRCS)	\
	pilot.c			\
	$(NULL)

gpilot_applet_LDADD = 				\
	$(GNOME_PILOT_LIBS)			\
	$(PILOT_APPLET_LIBS)		\
	$(top_builddir)/gpilotd/libgpilotd.la

#lib_LTLIBRARIES=libpilot_applet.la

#libpilot_applet_la_SOURCES= \
#	pilot.c

INCLUDES = 						\
	-I$(top_srcdir)					\
	$(GNOME_PILOT_CFLAGS)				\
	$(PILOT_APPLET_CFLAGS)			\
	-DGLADEDATADIR=\""$(gladedir)"\" 		\
	-DPREFIX=\""$(prefix)\"" 			\
	-DSYSCONFDIR=\""$(sysconfdir)\"" 		\
	-DDATADIR=\""$(datadir)\"" 			\
	-DLIBDIR=\""$(libdir)\"" 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"


gladedir  = $(datadir)/gnome-pilot/glade
glade_DATA = pilot-applet.glade

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = sync_paused.png sync_broken.png sync_icon.png syncing_icon.png

serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_PilotApplet.server.in.in
server_DATA = $(server_in_files:.server.in.in=.server)
@GP_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

schemasdir       = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = pilot.schemas.in
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/applet/$$p; \
		done \
	fi

EXTRA_DIST = 				\
	$(pixmap_DATA) 			\
	$(glade_DATA) 			\
	$(gobfiles)			\
	$(server_in_files)		\
	$(schemas_in_files)		\
	$(NULL)

BUILT_SOURCES = $(GOB_BUILT_SRCS) $(schemas_DATA) $(server_DATA)

CLEANFILES = $(schemas_DATA) $(server_DATA)
