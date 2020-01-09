INCLUDES = 					\
	-I$(top_srcdir)/ 			\
	-I$(top_srcdir)/gpilotd			\
	$(GNOME_PILOT_CFLAGS) 			\
	-DG_LOG_DOMAIN=\"backupconduit\"

backup_conduitsdir=$(libdir)/gnome-pilot/conduits

backup_conduits_LTLIBRARIES = \
	libbackup_conduit.la

libbackup_conduit_la_SOURCES = backup_conduit.c backup_conduit.h 

libbackup_conduit_la_LDFLAGS = -module -avoid-version
libbackup_conduit_la_LIBADD = 	\
	$(GNOME_PILOT_LIBS) \
	$(top_builddir)/gpilotd/libgpilotdconduit.la

@GP_CONDUIT_RULE@

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = backup.conduit

CLEANFILES = $(Conduit_DATA)

EXTRA_DIST = 			\
	backup.conduit.in

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(Conduitdir)
