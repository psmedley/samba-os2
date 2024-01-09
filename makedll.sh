mkdir client
g++ -Zdll -Zomf -Zmap -static-libgcc -o smbcl419.dll smbcl419.def ./bin/default/auth/*.a ./bin/default/auth/credentials/*.a ./bin/default/auth/gensec/*.a  ./bin/default/lib/*.a ./bin/default/lib/addns/*.a ./bin/default/lib/cmdline/*.a ./bin/default/lib/dbwrap/*.a ./bin/default/lib/krb5_wrap/*.a ./bin/default/lib/ldb/*.a  ./bin/default/lib/ldb-samba/*.a ./bin/default/lib/param/*.a ./bin/default/lib/replace/*.a ./bin/default/lib/socket/*.a ./bin/default/lib/talloc/*.a ./bin/default/lib/tdb/*.a ./bin/default/lib/tdb_wrap/*.a ./bin/default/lib/tevent/*.a ./bin/default/lib/util/*.a ./bin/default/libcli/auth/*.a ./bin/default/libcli/http/*.a ./bin/default/libcli/cldap/*.a ./bin/default/libcli/dns/*.a  ./bin/default/libcli/ldap/*.a  ./bin/default/libcli/named_pipe_auth/*.a ./bin/default/libcli/nbt/*.a ./bin/default/libcli/registry/*.a ./bin/default/libcli/security/*.a ./bin/default/libcli/smb/*.a ./bin/default/libcli/util/*.a ./bin/default/libds/common/*.a ./bin/default/librpc/*.a ./bin/default/nsswitch/libwbclient/*.a ./bin/default/source3/*.a ./bin/default/source3/auth/*.a  ./bin/default/source3/libsmb/libsmbclient.a ./bin/default/source4/auth/*.a ./bin/default/source4/auth/kerberos/*.a ./bin/default/source4/cluster/*.a ./bin/default/source4/dsdb/*.a ./bin/default/source4/lib/events/*.a ./bin/default/source4/lib/messaging/*.a ./bin/default/lib/messaging/*.a  ./bin/default/source4/lib/socket/*.a ./bin/default/source4/libcli/*.a ./bin/default/source4/libcli/ldap/*.a ./bin/default/source4/libcli/wbclient/*.a  ./bin/default/source4/librpc/*.a  ./bin/default/third_party/popt/*.a  -lmmap -lcxsmbd -lz -lldap -llber -lkrb5 -lcom_err -lwind -lroken -lssp -lhcrypto -lasn1 -lgssapi -lgnutls -lgpg-error -lpthread -ljansson -lacl -lattr -lat-funcs -licuuc -licuin 2>&1 | tee smbcl419.log
emximp -o smbcl419.a smbcl419.dll
emxomf -p128 -o smbcl419.lib smbcl419.a
u:/dev/exceptq/bin/mapxqs.exe smbcl419.map
cp smbcl419.dll client
cp smbcl419.map client

gcc -Zomf -Zmap -Zbin-files -o smbclient.exe ./bin/default/source3/client/cl*.o ./bin/default/source3/client/dnsbrow*.o ./bin/default/lib/cmdline/cmdline_s3*.o ./bin/default/libcli/smbreadline/smbreadline*.o -lsmbcl419 -larchive -lxml2 -lz -llzma -lpthread -lcxsmbd -lbz2 -lssl -lcrypto -lssp libreadline.lib -lncurses -ltinfo
cp smbclient.exe client
cp smbclient.map client
