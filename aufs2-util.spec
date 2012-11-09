Summary: aufs2 user space utilities
Name: aufs2-util
Version: 2.1
Release: 1
Source0: %{name}-%{version}.tar.gz
Group: Applications/System
License: GPL
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: gcc
BuildRequires: make
BuildRequires: glibc-static
BuildRequires: coreutils
BuildRequires: sed
BuildRequires: binutils
BuildRequires: kernel-aufs21-headers
Requires: kernel-aufs21

%description
User space utilities for aufs2 kernel module (union file system).
Mount helpers etc.
See aufs.sorceforge.net

%prep
%setup -q

%build
make

%install
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_sysconfdir}/default/aufs
/sbin/auibusy
/sbin/auplink
/sbin/mount.aufs
/sbin/umount.aufs
/usr/bin/aubrsync
/usr/bin/aubusy
/usr/bin/auchk
/usr/lib/libau.so
/usr/lib/libau.so.2
/usr/lib/libau.so.2.5
/usr/share/man/man5/aufs.5.gz

%changelog
* Fri Nov 09 2012 Jakob Blomer <jblomer@cern.ch>
- Initial Package

