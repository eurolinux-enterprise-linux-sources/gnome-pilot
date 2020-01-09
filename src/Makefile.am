ACLOCAL_AMFLAGS = -I macros

SUBDIRS = 			\
	po 			\
	idl 			\
	gpilotd 		\
	applet			\
	capplet 		\
	conduits 		\
	utils			\
	help

pkglibdir=$(libdir)/pkgconfig
pkglib_DATA=gnome-pilot-2.0.pc

# ignore the scrollkeeper files on uninstall check
distuninstallcheck_listfiles = find . -type f -print | grep -v \
 '^\./var/scrollkeeper'

changelogs =				\
	applet/OChangeLog		\
	capplet/OChangeLog		\
	conduits/backup/OChangeLog	\
	conduits/OChangeLog		\
	conduits/file/OChangeLog	\
	gpilot-fileman/OChangeLog	\
	gpilotd/OChangeLog		\
	idl/OChangeLog			\
	libgpilotdCM/OChangeLog		\
	utils/OChangeLog

EXTRA_DIST=			\
	$(changelogs)		\
	macros/compiler-flags.m4 \
	conduit.mk		\
	server.mk		\
	intltool-extract.in	\
	intltool-update.in	\
	intltool-merge.in	\
	gnome-pilot.spec 	\
	gnome-pilot-2.0.pc.in

DISTCLEANFILES = intltool-extract intltool-merge intltool-update

