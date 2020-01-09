NULL=

bin_PROGRAMS = gpilotd-control-applet 

INCLUDES = 						\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gpilotd				\
	$(GNOME_PILOT_CFLAGS) 				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DGLADEDATADIR=\""$(gladedir)"\" 		\
	$(NULL)

gpilotd_control_applet_SOURCES = 	\
	gpilotd-control-applet.c 	\
	gnome-pilot-capplet.c		\
	gnome-pilot-capplet.h		\
	gnome-pilot-pdialog.c		\
	gnome-pilot-pdialog.h		\
	gnome-pilot-ddialog.c		\
	gnome-pilot-ddialog.h		\
	gnome-pilot-cdialog.c		\
	gnome-pilot-cdialog.h		\
	gnome-pilot-druid.c		\
	gnome-pilot-druid.h		\
	util.c				\
	util.h				\
	pilot.c 			\
	pilot.h				\
	$(NULL)

gpilotd_control_applet_LDFLAGS = \
	-export-dynamic

gpilotd_control_applet_LDADD = \
	$(top_builddir)/gpilotd/libgpilotd.la 			\
	$(top_builddir)/gpilotd/libgpilotdconduit.la    	\
	$(top_builddir)/gpilotd/libgpilotdcm.la    	\
	$(GNOME_PILOT_LIBS) 					\
	$(NULL)

desktopdir = $(datadir)/applications
#desktop_in_files = gpilotd-control-applet.desktop.in
#desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

desktop_in_in_file = gpilotd-control-applet.desktop.in.in
desktop_DATA = $(desktop_in_in_file:.desktop.in.in=.desktop)
%.desktop.in: %.desktop.in.in
	sed -e "s|\@VERSION\@|$(VERSION)|" $< > $@

@INTLTOOL_DESKTOP_RULE@

gladedir  = $(datadir)/gnome-pilot/glade
glade_DATA = 				\
	gpilotd-capplet.glade 		\
        gnome-pilot-watermark.png 	\
	gnome-palm.png			\
	$(NULL)

appicondir = $(datadir)/pixmaps
appicon_DATA = 			\
	gnome-palm.png		\
	gnome-palm-conduit.png	\
	$(NULL)


EXTRA_DIST = 				\
	$(desktop_in_in_file) 		\
	$(appicon_DATA)			\
	$(glade_DATA) 			\
	$(NULL)

BUILT_SOURCES = $(desktop_DATA)

CLEANFILES = $(BUILT_SOURCES)
