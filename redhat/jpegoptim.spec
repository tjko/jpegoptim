Summary: Utility for optimizing/compressing JPEG files.
Name: jpegoptim
Version: 1.5.5
Release: 1
License: GPL
Group: Applications/Multimedia
URL: http://www.iki.fi/tjko/projects.html
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot

%description
Jpegoptim can optimize/compress jpeg files. Program support
lossless optimization, which is based on optimizing the Huffman
tables. So called, "lossy" optimization (compression) is done
by re-encoding the image using user specified image quality factor.

%prep
if [ "${RPM_BUILD_ROOT}x" == "x" ]; then
        echo "RPM_BUILD_ROOT empty, bad idea!"
        exit 1
fi
if [ "${RPM_BUILD_ROOT}" == "/" ]; then
        echo "RPM_BUILD_ROOT is set to "/", bad idea!"
        exit 1
fi
%setup -q

%build
./configure --prefix=/usr
make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
/usr/bin/*
/usr/share/man/man1/*
%doc README COPYRIGHT


%changelog
* Sat Dec  7 2002 Timo Kokkonen <tjko@iki.fi>
- Initial build.


