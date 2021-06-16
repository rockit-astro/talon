Name:      superwasp-talon
Version:   20210616
Release:   0
Summary:   Stripped down Talon installation for the SuperWASP telescope
License:   proprietary
Group:     Unspecified
BuildArch: x86_64
BuildRequires: motif-devel
Requires: tcsh

%description
Stripped down Talon installation for the SuperWASP telescope.

%build

cmake %{_sourcedir}
make
make DESTDIR=%{buildroot} install
mv %{buildroot}/usr/local/telescope/archive/superwasp_config %{buildroot}/usr/local/telescope/archive/config
rm -rf %{buildroot}/usr/local/telescope/archive/onemetre_config

%files
%defattr(0644,root,root,-)
/etc/profile.d/talon.sh

%defattr(0755,root,root,-)
/usr/local/telescope

%defattr(0777,root,root,-)
/usr/local/telescope/archive/config
/usr/local/telescope/archive/logs
/usr/local/telescope/comm

%changelog
