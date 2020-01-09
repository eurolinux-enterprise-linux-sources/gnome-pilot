#
# Note that this is NOT a relocatable package
# $Id: gnome-pilot.spec.in,v 1.17 2001/12/03 11:00:26 eskil Exp $
#
%define ver      2.0.17
%define rel      @GNOME_PILOT_RELEASE@
%define prefix   /usr/local
%define name	 gnome-pilot
%define epoch	 0

Summary: GNOME pilot programs
Summary(da): GNOME pilot programmer
Name: %name
Version: %ver
Release: %rel
Copyright: LGPL
Epoch: %epoch
Group: Applications/Communications
Source: http://eskil.org/gnome-pilot/download/%name-%ver.tar.gz
BuildRoot: /var/tmp/gnome-pilot
Packager: Eskil Heyn Olsen <eskil@eskil.dk>
URL: http://eskil.org/gnome-pilot
Prereq: /sbin/install-info
Prefix: %{prefix}
Docdir: %{prefix}/doc
Requires: pilot-link >= 0.9.5
Requires: gnome-core >= 1.2.12
Requires: ORBit >= 0.5.7
Requires: libglade >= 0.16
Requires: libxml >= 1.8.12
Requires: gnome-vfs >= 1.0

%description
gnome-pilot is a collection of programs and daemon for integrating
GNOME and the PalmPilot<tm> or other PalmOS<tm> devices.

%description -l da

gnome-pilot er en samling af programmer der integrerer GNOME og 
PalmPilot<tm> eller andre PalmOS<tm> enheder.

%package devel
Summary: GNOME pilot libraries, includes, etc
Summary(da): GNOME pilot biblioteker, include filer etc.
Group: Development/Libraries
Requires: gnome-core-devel
Requires: ORBit-devel
Requires: pilot-link-devel
Requires: %name = %{epoch}:%{ver}
PreReq: /sbin/install-info

%description devel
gpilotd libraries and includes.

%description devel -l da
gpilotd include filer og biblioteker.

%changelog

* Tue Sep 11 2001 Eskil Heyn Olsen <eskil@eskil.dk>

- Removed the test conduit from rpm

* Wed Feb 17 1999 Eskil Heyn Olsen <deity@eskil.dk>

- Created the .spec file

%prep
%setup

%build
# Needed for snapshot releases.
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --prefix=%prefix --sysconfdir=$RPM_BUILD_ROOT/etc
else
  CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix --sysconfdir=$RPM_BUILD_ROOT/etc
fi

if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT
make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean


%post
if ! grep %{prefix}/lib /etc/ld.so.conf > /dev/null ; then
  echo "%{prefix}/lib" >> /etc/ld.so.conf
fi

/sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)
%doc AUTHORS COPYING ChangeLog NEWS README
%{prefix}/bin/gnome-pilot-make-password
%{prefix}/bin/gpilotd
%{prefix}/bin/gpilot-applet
%{prefix}/bin/gpilot-install-file
%{prefix}/bin/gpilotd-control-applet
%{prefix}/bin/gpilotd-session-wrapper
%{prefix}/lib/*.so
%{prefix}/lib/*.so.*
%{prefix}/lib/gnome-pilot/conduits/libfile_conduit.so
%{prefix}/lib/gnome-pilot/conduits/libbackup_conduit.so
%{prefix}/man/
%{prefix}/share/applets
%{prefix}/share/control-center
%{prefix}/share/gnome
%{prefix}/share/gnome-pilot/conduits/backup.conduit
%{prefix}/share/gnome-pilot/conduits/file.conduit
%{prefix}/share/gnome-pilot/glade
%{prefix}/share/idl/gnome-pilot.idl
%{prefix}/share/locale
%{prefix}/share/mime-info/palm.*
%{prefix}/share/oaf/*.oafinfo
%{prefix}/share/pixmaps/*.png
%config /etc/CORBA/servers/gpilotd.gnorba
%config /etc/CORBA/servers/gpilot-applet.gnorba

%files devel
%defattr(-, root, root)
%{prefix}/include/
%{prefix}/lib/*.a
%{prefix}/lib/*.la
%{prefix}/lib/gpilotConf.sh

