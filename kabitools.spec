Name:           kabitools
Version:        3.5.4
Release:        2%{?dist}
Summary:        A toolkit for KABI navigation
BuildRoot:      %{_topdir}/BUILDROOT/

License:        GPLv2
URL:            https://github.com/camuso/kabiparser
Source0:        %{_topdir}/%{name}-%{version}.tar.gz

BuildArch:      x86_64
BuildRequires:  gcc >= 4.8
BuildRequires:  gcc-c++
BuildRequires:  boost
Requires:       boost

%description
kabitools provides utilities for navigating the KABI
makei.sh - preprocesses kernel c files containing exported symbols
kabi-data.sh - converts the preprocessed .i files into .kb_dat graphs
kabi-dump - utility for examining the contents of a kb_dat graph.
kabi-lookup - given the symbol name of an exported symbol, determines
              all the dependencies for that symbol and prints them to
              the screen.

%prep
%setup -q

%install
mkdir -p $RPM_BUILD_ROOT%{_datadir}
mkdir -p $RPM_BUILD_ROOT%{_sbindir}
cp %{_topdir}/BUILD/%{name}-%{version}/kabi-parser  $RPM_BUILD_ROOT%{_sbindir}
cp %{_topdir}/BUILD/%{name}-%{version}/kabi-dump    $RPM_BUILD_ROOT%{_sbindir}
cp %{_topdir}/BUILD/%{name}-%{version}/kabi-lookup  $RPM_BUILD_ROOT%{_sbindir}
cp %{_topdir}/BUILD/%{name}-%{version}/kabitools.sh $RPM_BUILD_ROOT%{_sbindir}
cp %{_topdir}/BUILD/%{name}-%{version}/kabi-data.sh $RPM_BUILD_ROOT%{_sbindir}
cp %{_topdir}/BUILD/%{name}-%{version}/makei.sh     $RPM_BUILD_ROOT%{_sbindir}
cp %{_topdir}/BUILD/%{name}-%{version}/kabitools-rhel-kernel-make.patch $RPM_BUILD_ROOT%{_datadir}
cp %{_topdir}/BUILD/%{name}-%{version}/kabitools-fedora-kernel-make.patch $RPM_BUILD_ROOT%{_datadir}

%files
%defattr(-,root,root)
%{_sbindir}/kabi-parser
%{_sbindir}/kabi-dump
%{_sbindir}/kabi-lookup
%{_sbindir}/kabitools.sh
%{_sbindir}/kabi-data.sh
%{_sbindir}/makei.sh
%{_datadir}/kabitools-rhel-kernel-make.patch
%{_datadir}/kabitools-fedora-kernel-make.patch
%doc README

%changelog
* Fri Mar 24 2017 Tony Camuso <tcamuso@redhat.com> - 3.5.4-2
- Fix bug in lookup.cpp::lookup::run improper use of mask being
  logically ANDed with the KB_JUSTONE bit instead of bit-ANDed.
* Tue Mar 21 2017 Tony Camuso <tcamuso@redhat.com> - 3.5.4-1
- Added the -1 option to exit after finding just one symbol.
- Changed the NOTFOUND message to say that the symbol is not
  in the database, so is kABI safe.
* Sat Nov 12 2016 Tony Camuso <tcamuso@redhat.com> - 3.5.3-4
- Changed to install only, instead of build.
- Finished update of README and added README to the %docs directory
- Added the patchfiles to %{_datadir}
- Added kabitools.sh script to build the kernel graph files
* Thu Nov 10 2016 Tony Camuso <tcamuso@redhat.com> - 3.5.3-3
- Refresh kabitools-rhel-kernel-make.patch
- Update the README
* Wed Nov 09 2016 Tony Camuso <tcamuso@redhat.com> - 3.5.3-2
- Update this spec changelog.
- Rename kernel-make.patch kabitools-rhel-kernel-make.patch.
- Add kabitools-fedora-kernel-make.patch
- Moved the Makefile patches to %{datadir} (/usr/share)
* Tue Nov 08 2016 Tony Camuso <tcamuso@redhat.com> - 3.5.3-1
- Update to help text and coment-out calls to cerr.flush
  cerr.flush is unbuffered, so no flush necessary.
* Sun Nov 06 2016 Tony Camuso <tcamuso@redhat.com> - 3.5.2-1
- Add kernel-make.patch for fedora-24
* Thu May 26 2016 Tony Camuso <tcamuso@redhat.com> - 3.5.1-1
- Bump to 3.5.1-1 as first major nvr
