set PKGCONFIG=pkg-config.exe
set LIBS=-lroken -lacl -lhcrypt -lz -lmmap -lintl -lcx 
set python=python.exe
set cc=
set perl=
set path=c:\usr\local1230\bin;%path%
SET LIBRARY_PATH=c:/usr/lib;c:/usr/local/lib
dash ./configure  --prefix=/samba --with-system-heimdalkrb5 --disable-cups --bundled-libraries=!asn1,!roken,!com_err,!wind,!hx509,!heimbase,!heimntlm,!hcrypto,!krb5,!gssapi,!hdb,!kdc,!z,ALL --with-static-modules=ALL --nonshared-binary=smbclient --without-ad-dc --without-pam --disable-python --without-libunwind --disable-fault-handling --without-pie 2>&1 | tee configure.log

rem set mmap flags to 0 in  bin\default\include\config.h & bin\c4che\default_cache.py
rem MUST use libc iconv.h and iconv.lib