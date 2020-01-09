NULL=
libexec_PROGRAMS = gpilotd

bin_PROGRAMS = gpilotd-session-wrapper

INCLUDES = 							\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"gpilotd\" 				\
	-DGP_PILOT_LINK_VERSION=\"@PILOT_LINK_VERSION@\" 	\
	-DDEVICE_XML_DIR=\"$(datadir)/gnome-pilot\" 	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 		\
	-DCONDUITDIR=\"$(datadir)\"				\
	$(GNOME_PILOT_CFLAGS)	

gpmarshal.h: gpmarshal.list
	@GLIB_GENMARSHAL@ \
		--prefix=gp_marshal $(srcdir)/gpmarshal.list --header > xgen-gmc \
	&& cp xgen-gmc $(@F) \
	&& rm -f xgen-gmc

gpmarshal.c: gpmarshal.h
	echo "#include \"gpmarshal.h\"" > xgen-gmc \
	&& @GLIB_GENMARSHAL@ \
		--prefix=gp_marshal $(srcdir)/gpmarshal.list --body >> xgen-gmc \
	&& cp xgen-gmc $(@F) \
	&& rm -f xgen-gmc

gpilotdlibsdir=$(libdir)

gpilotdlibs_LTLIBRARIES = 		\
	libgpilotd.la 			\
	libgpilotdconduit.la 		\
	$(NULL)

gpilotdcmlibsdir=$(libdir)

gpilotdcmlibs_LTLIBRARIES =		\
	libgpilotdcm.la

## GOB STUFF 

GOB_LIBGPILOTDCM_BUILT_SRCS = 					\
	../libgpilotdCM/gnome-pilot-conduit-management.c 		\
	../libgpilotdCM/gnome-pilot-conduit-management-private.h 	\
	../libgpilotdCM/gnome-pilot-conduit-management.h 		\
	../libgpilotdCM/gnome-pilot-conduit-config.c			\
	../libgpilotdCM/gnome-pilot-conduit-config-private.h 		\
	../libgpilotdCM/gnome-pilot-conduit-config.h

GOB_LIBGPILOTD_BUILT_SRCS = 			\
	gnome-pilot-client.c 			\
	gnome-pilot-client-private.h 		\
	gnome-pilot-client.h			\
	$(NULL)

GOB_GPILOTD_CONDUIT_SRCS=			\
	gnome-pilot-conduit.c			\
	gnome-pilot-conduit.h			\
	gnome-pilot-conduit-private.h		\
	gnome-pilot-conduit-file.c		\
	gnome-pilot-conduit-file.h		\
	gnome-pilot-conduit-file-private.h	\
	gnome-pilot-conduit-backup.c		\
	gnome-pilot-conduit-backup.h		\
	gnome-pilot-conduit-backup-private.h	\
	gnome-pilot-conduit-standard.c 		\
	gnome-pilot-conduit-standard.h 		\
	gnome-pilot-conduit-standard-private.h 	\
	$(NULL)

GOB_BUILT_SRCS = 			\
	$(GOB_LIBGPILOTDCM_BUILT_SRCS) 	\
	$(GOB_LIBGPILOTD_BUILT_SRCS) 	\
	$(GOB_GPILOTD_CONDUIT_SRCS)	\
	$(NULL)

gobfiles = 						\
	$(srcdir)/gnome-pilot-conduit-management.gob 	\
	$(srcdir)/gnome-pilot-conduit-config.gob 	\
	$(srcdir)/gnome-pilot-client.gob 		\
	$(srcdir)/gnome-pilot-conduit.gob		\
	$(srcdir)/gnome-pilot-conduit-backup.gob	\
	$(srcdir)/gnome-pilot-conduit-file.gob		\
	$(srcdir)/gnome-pilot-conduit-standard.gob	\
	$(NULL)

# ugly: gnome-pilot-conduit-config.gob and gnome-pilot-conduit-management.gob
# used to live in libgpilotdCM/, which led to a build-order bug as they're
# dependendent on some gpilotd/ stuff.  However, third party code expects
# the headers to be in include/libgpilotdCM so we continue to provide them there.
../libgpilotdCM/%.c ../libgpilotdCM/%.h: %.gob
	$(GOB) --always-private-header -o  ../libgpilotdCM -w $<

%.c %.h: %.gob
	$(GOB) --always-private-header -w $<

#$(GOB_BUILT_SRCS): $(gobfiles)
#	@for GF in $(gobfiles); do (echo -n "$(GOB) -w $$GF..." && $(GOB) -w $$GF && echo " ok"); done;

### IDL and corba grease

idldir = $(datadir)/idl

idl_files = $(top_srcdir)/idl/gnome-pilot.idl

CORBA_SRCLIST = 		\
	gnome-pilot-common.c 	\
	gnome-pilot-skels.c 	\
	gnome-pilot-stubs.c 	\
	gnome-pilot.h		\
	$(NULL)

GPILOTD_ORB_SRCLIST = 		\
	orbit_daemon_glue.c 	\
	orbit_daemon_glue.h	\
	$(NULL)

$(CORBA_SRCLIST): $(idl_files)
	$(ORBIT_IDL) $(idl_files)

BUILT_SOURCES = $(CORBA_SRCLIST) $(GOB_BUILT_SRCS) gpmarshal.c gpmarshal.h

CLEANFILES = $(CORBA_SRCLIST) $(server_DATA)
MAINTAINERCLEANFILES = $(GOB_BUILT_SRCS)

## LIBGPILOTDCM

libgpilotdcm_la_SOURCES = \
	$(GOB_LIBGPILOTDCM_BUILT_SRCS)

libgpilotdcmincludedir = $(includedir)/libgpilotdCM

libgpilotdcminclude_HEADERS = \
	../libgpilotdCM/gnome-pilot-conduit-management.h 	\
	../libgpilotdCM/gnome-pilot-conduit-config.h

libgpilotdcm_la_LIBADD =		\
	$(GNOME_PILOT_LIBS)		\
	$(NULL)

libgpilotdcm_la_LDFLAGS = \
	-version-info $(GPILOTD_CM_CURRENT):$(GPILOTD_CM_REVISION):$(GPILOTD_CM_AGE)

## LIBGPILOTD

libgpilotd_la_SOURCES = 		\
	$(CORBA_SRCLIST) 		\
	$(GOB_LIBGPILOTD_BUILT_SRCS)	\
	$(NULL)

libgpilotdincludedir = $(includedir)/gpilotd

libgpilotdinclude_HEADERS = gnome-pilot-client.h

libgpilotd_la_LIBADD = \
	$(GNOME_PILOT_LIBS)	\
	$(NONE)

libgpilotd_la_LDFLAGS = \
	-version-info $(GPILOTD_CURRENT):$(GPILOTD_REVISION):$(GPILOTD_AGE)

## LIBGPILOTDCONDUIT
libgpilotdconduit_la_SOURCES = 			\
	$(GOB_GPILOTD_CONDUIT_SRCS)		\
	gpmarshal.c				\
	gpmarshal.h				\
	gnome-pilot-conduit-standard-abs.c 	\
	gnome-pilot-conduit-standard-abs.h 	\
	gnome-pilot-conduit-sync-abs.h 		\
	gnome-pilot-conduit-sync-abs.c 		\
	gnome-pilot-structures.h		\
	gnome-pilot-structures.c		\
	gpilot-gui.h 				\
	gpilot-gui.c				\
	$(NULL)

libgpilotdconduitinclude_HEADERS = 		\
	gnome-pilot-conduit.h 			\
	gnome-pilot-conduit-standard.h 		\
	gnome-pilot-conduit-standard-abs.h 	\
	gnome-pilot-conduit-sync-abs.h 		\
	gnome-pilot-conduit-file.h 		\
	gnome-pilot-conduit-backup.h 		\
	gnome-pilot-dbinfo.h			\
	gnome-pilot-structures.h		\
	$(NULL)

libgpilotdconduitincludedir = $(includedir)/gpilotd

libgpilotdconduit_la_LIBADD =	\
	$(GNOME_PILOT_LIBS)	\
	$(NULL)

libgpilotdconduit_la_LDFLAGS = \
	-version-info $(GPILOTD_CONDUIT_CURRENT):$(GPILOTD_CONDUIT_REVISION):$(GPILOTD_CONDUIT_AGE)

###

noinst_HEADERS = gnome-pilot-conduit-backup-private.h

gpilotincludedir = $(includedir)/gpilotd
gpilotinclude_HEADERS = 		\
	gnome-pilot.h			\
	gnome-pilot-structures.h	\
	$(NULL)

gpilotd_SOURCES =			\
	gpilotd-conduit-mgmt.h 		\
	gpilotd.c			\
	manager.c			\
	manager.h			\
	queue_io.c 			\
	queue_io.h			\
	$(GPILOTD_ORB_SRCLIST) 		\
	$(NULL)

gpilotd_LDFLAGS = -export-dynamic

gpilotd_LDADD = libgpilotdconduit.la libgpilotd.la	\
		./libgpilotdcm.la 	\
		./libgpilotd.la				\
		$(GNOME_PILOT_LIBS) 			\
		$(NULL)

gpilotd_session_wrapper_SOURCES = 		\
		gpilotd-session-wrapper.c	\
		$(NULL)

gpilotd_session_wrapper_LDADD = 	\
		$(GNOME_PILOT_LIBS) 	\
		$(NULL)


serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_Pilot_Daemon.server.in.in
server_DATA = $(server_in_files:.server.in.in=.server)
@GP_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

mimedir = $(datadir)/mime-info
mime_DATA = palm.mime

devicedir  = $(datadir)/gnome-pilot
device_DATA = devices.xml

EXTRA_DIST = 			\
	GNOME_Pilot_Daemon.server.in.in		\
	gpmarshal.list		\
	gpmarshal.h		\
	gpmarshal.c		\
	$(gobfiles) 		\
	$(server_in_files) 	\
	$(mime_DATA)		\
	$(device_DATA)		\
	$(NULL)

dist-hook:
	cd $(distdir); rm -rf $(CORBA_SRCLIST)

