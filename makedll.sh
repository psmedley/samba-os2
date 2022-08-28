mkdir t
g++ -Zdll -Zomf -Zmap -static-libgcc -o smbcl415.dll smbcl415.def ./bin/default/auth/*.a ./bin/default/auth/credentials/*.a ./bin/default/auth/gensec/*.a  ./bin/default/lib/*.a ./bin/default/lib/addns/*.a ./bin/default/lib/cmdline/*.a ./bin/default/lib/dbwrap/*.a ./bin/default/lib/krb5_wrap/*.a ./bin/default/lib/ldb/*.a  ./bin/default/lib/ldb-samba/*.a ./bin/default/lib/param/*.a ./bin/default/lib/replace/*.a ./bin/default/lib/socket/*.a ./bin/default/lib/talloc/*.a ./bin/default/lib/tdb/*.a ./bin/default/lib/tdb_wrap/*.a ./bin/default/lib/tevent/*.a ./bin/default/lib/util/*.a ./bin/default/libcli/auth/*.a ./bin/default/libcli/cldap/*.a ./bin/default/libcli/dns/*.a  ./bin/default/libcli/ldap/*.a  ./bin/default/libcli/named_pipe_auth/*.a ./bin/default/libcli/nbt/*.a ./bin/default/libcli/registry/*.a ./bin/default/libcli/security/*.a ./bin/default/libcli/smb/*.a ./bin/default/libcli/util/*.a ./bin/default/libds/common/*.a ./bin/default/librpc/*.a ./bin/default/nsswitch/*.a ./bin/default/nsswitch/libwbclient/*.a ./bin/default/source3/*.a ./bin/default/source3/auth/*.a  ./bin/default/source3/libsmb/libsmbclient.a ./bin/default/source4/auth/*.a ./bin/default/source4/auth/kerberos/*.a ./bin/default/source4/auth/ntlm/*.a ./bin/default/source4/cluster/*.a ./bin/default/source4/dsdb/*.a ./bin/default/source4/lib/events/*.a ./bin/default/source4/lib/messaging/*.a ./bin/default/lib/messaging/*.a  ./bin/default/source4/lib/socket/*.a ./bin/default/source4/libcli/*.a ./bin/default/source4/libcli/ldap/*.a ./bin/default/source4/libcli/wbclient/*.a  ./bin/default/source4/librpc/*.a  ./bin/default/third_party/popt/*.a  -lmmap -lcxsmbd -lz -lldap -llber -lkrb5 -lcom_err -lwind -lroken -lssp -lhcrypto -lasn1 -lgssapi -lgnutls -lgcrypt -lgpg-error -lpthread -ljansson -lacl -licui18n -licuuc -licudata  -lattr -lat-funcs  2>&1 | tee smbcl415.log
emximp -o smbcl415.a smbcl415.dll
emxomf -p128 -o smbcl415.lib smbcl415.a
u:/dev/exceptq71-dev/bin/mapxqs.exe smbcl415.map
cp smbcl415.dll t
cp smbcl415.dll u:/dev/samba-svn/client-3.8/src/t
cp smbcl415.map t

gcc -Zomf -Zmap -Zbin-files -o smbclient.exe ./bin/default/source3/client/cl*.o ./bin/default/source3/client/dnsbrow*.o ./bin/default/libcli/smbreadline/smbreadline*.o -lsmbcl415 -larchive -lxml2 -lz -llzma -lpthread -lcxsmbd -lbz2 -lssl -lcrypto -lssp -lreadline -lncurses
cp smbclient.exe t
cp smbclient.map t
