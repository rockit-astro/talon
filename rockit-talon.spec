Name:      rockit-talon
Version:   %{_version}
Release:   1
Summary:   Stripped down Talon installation for the W1m and NGTS telescopes
License:   proprietary
Group:     Unspecified
BuildArch: x86_64
BuildRequires: motif-devel
Requires: tcsh

%description
Stripped down Talon installation for the W1m and NGTS telescopes.

%build

cmake %{_sourcedir}
make
make DESTDIR=%{buildroot} install
mkdir -p %{buildroot}%{_udevrulesdir}
%{__install} %{_sourcedir}/10-ngts-m06-mount.rules %{buildroot}%{_udevrulesdir}

%files
%defattr(0644,root,root,-)
/etc/profile.d/talon.sh

%defattr(0755,root,root,-)
/usr/local/telescope

%defattr(0777,root,root,-)
/usr/local/telescope/archive/logs
/usr/local/telescope/comm

%exclude /usr/local/telescope/archive/config

%package data-onemetre
Summary: Talon config data for the W1m telescope
Group:   Unspecified
BuildArch: noarch
RemovePathPostfixes: .onemetre
%description data-onemetre

%files data-onemetre
%defattr(0644,root,root,-)
/usr/local/telescope/archive/config/*.cmc
%defattr(0666,root,root,-)
/usr/local/telescope/archive/config/*.cfg.onemetre
/usr/local/telescope/archive/config/telescoped.mesh.onemetre

%package data-ngts
Summary: Talon config data for the NGTS telescopes
Group:   Unspecified
BuildArch: noarch
RemovePathPostfixes: .ngts
%description data-ngts

%files data-ngts
%defattr(0644,root,root,-)
%{_udevrulesdir}/10-ngts-m06-mount.rules
/usr/local/telescope/archive/config/*.cmc
%defattr(0666,root,root,-)
/usr/local/telescope/archive/config/*.cfg.ngts
/usr/local/telescope/archive/config/telescoped.mesh.ngts

%changelog
