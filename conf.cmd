set PKGCONFIG=pkg-config.exe
set LIBS=-lroken -lacl -lhcrypt -lz -lmmap -lcx
set python=python3.exe
set cc=
set perl=
dash ./configure  --prefix=u:/samba --with-system-heimdalkrb5 --disable-cups --bundled-libraries=!asn1,!roken,!com_err,!wind,!hx509,!heimbase,!heimntlm,!hcrypto,!krb5,!gssapi,!hdb,!kdc,!z,ALL --with-static-modules=ALL --nonshared-binary=smbclient --without-ad-dc --without-pam --disable-python 2>&1 | tee configure.log

rem set mmap flags to 0 in  bin\default\include\config.h & bin\c4che\default_cache.py
