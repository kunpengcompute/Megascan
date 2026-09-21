%define _build_name_fmt %%{ARCH}/boostkit-ms-release-%%{VERSION}.%%{RELEASE}.%%{ARCH}.rpm
Name:           boostkit-ms
Version:        1.0.0
Release:        oe2203%{?dist}
ExclusiveArch:  aarch64
Summary:        The Million-rule Multi-Pattern Matcher.
Source0:        ms.tar.gz
BuildArch:      aarch64
License:        MulanPSL-2.0
BuildRequires:  rpm
 
%description
Kunpeng Million-rule Multi-Pattern Matcher Library
Product Name:           Kunpeng BoostKit
Product Version:        26.2.RC1
Component Name:         BoostKit-MS
Component Version:      1.0
Component AppendInfo:	kunpeng
 
%prep
%setup -c
 
%install
install -d %{buildroot}/usr/local/lib
install -m 755 lib/*.so %{buildroot}/usr/local/lib
 
install -d %{buildroot}/usr/local/include
install -m 644 include/*.h %{buildroot}/usr/local/include

install -d %{buildroot}%{_licensedir}/%{name}
install -m 644 LICENSE %{buildroot}%{_licensedir}/%{name}/LICENSE

%files
/usr/local/lib/*.so
/usr/local/include/*.h
%license %{_licensedir}/%{name}/LICENSE
 
%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
 
%clean
