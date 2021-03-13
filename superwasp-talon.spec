Name:      superwasp-talon
Version:   20210313
Release:   0
Summary:   Stripped down Talon installation for the SuperWASP telescope
License:   proprietary
Group:     Unspecified
BuildArch: x86_64
Requires: xorg-x11-fonts-misc

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
/usr/local/telescope
/etc/profile.d/talon.sh

%defattr(0755,root,root,-)
/usr/local/telescope/bin

%defattr(0777,root,root,-)
/usr/local/telescope/archive/config

%changelog
