%define _build_name_fmt %%{ARCH}/boostida-kdadk-mmpm-release-%%{VERSION}.%%{RELEASE}.%%{ARCH}.rpm
Name:           boostkit-kdadk-mmpm
Version:        1.1.0
Release:        oe2203%{?dist}
ExclusiveArch:  aarch64
Summary:        The Million-rule Multi-Pattern Matcher.
Source0:        mmpm.tar.gz
BuildArch:      aarch64
License:        commercial
BuildRequires:  rpm
 
%description
Kunpeng Million-rule Multi-Pattern Matcher Library
Product Name:           Kunpeng BoostKit
Product Version:        25.3.0
Component Name:         BoostKit-boostida-KDADK
Component Version:      1.1
Component AppendInfo:	kunpeng
 
%prep
%setup -c
 
%install
install -d %{buildroot}/usr/local/lib
install -m 755 lib/*.so %{buildroot}/usr/local/lib
 
install -d %{buildroot}/usr/local/include
install -m 644 include/*.h %{buildroot}/usr/local/include

%files
/usr/local/lib/*.so
/usr/local/include/*.h
 
%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
 
%clean