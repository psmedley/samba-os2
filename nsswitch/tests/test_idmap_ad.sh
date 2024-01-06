#!/bin/sh
#
# Basic testing of id mapping with idmap_ad
#

if [ $# -ne 6 ]; then
	echo Usage: $0 DOMAIN DC_SERVER DC_PASSWORD TRUST_DOMAIN TRUST_SERVER TRUST_PASSWORD
	exit 1
fi

DOMAIN="$1"
DC_SERVER="$2"
DC_PASSWORD="$3"
TRUST_DOMAIN="$4"
TRUST_SERVER="$5"
TRUST_PASSWORD="$6"

wbinfo="$VALGRIND $BINDIR/wbinfo"
ldbmodify="${VALGRIND} ldbmodify"
if [ -x "${BINDIR}/ldbmodify" ]; then
	ldbmodify="${VALGRIND} ${BINDIR}/ldbmodify"
fi

ldbsearch="${VALGRIND} ldbsearch"
if [ -x "${BINDIR}/ldbsearch" ]; then
	ldbsearch="${VALGRIND} ${BINDIR}/ldbsearch"
fi

failed=0

. $(dirname $0)/../../testprogs/blackbox/subunit.sh

DOMAIN_SID=$($wbinfo -n "$DOMAIN/" | cut -f 1 -d " ")
if [ $? -ne 0 ]; then
	echo "Could not find domain SID" | subunit_fail_test "test_idmap_ad"
	exit 1
fi

TRUST_DOMAIN_SID=$($wbinfo -n "$TRUST_DOMAIN/" | cut -f 1 -d " ")
if [ $? -ne 0 ]; then
	echo "Could not find trusted domain SID" | subunit_fail_test "test_idmap_ad"
	exit 1
fi

BASE_DN=$($ldbsearch -H ldap://$DC_SERVER -b "" --scope=base defaultNamingContext | awk '/^defaultNamingContext/ {print $2}')
if [ $? -ne 0 ]; then
	echo "Could not find base DN" | subunit_fail_test "test_idmap_ad"
	exit 1
fi

TRUST_BASE_DN=$($ldbsearch -H ldap://$TRUST_SERVER -b "" --scope=base defaultNamingContext | awk '/^defaultNamingContext/ {print $2}')
if [ $? -ne 0 ]; then
	echo "Could not find trusted base DN" | subunit_fail_test "test_idmap_ad"
	exit 1
fi

#
# Add POSIX ids to AD
#
cat <<EOF | $ldbmodify -H ldap://$DC_SERVER -U "$DOMAIN\Administrator%$DC_PASSWORD"
dn: CN=Administrator,CN=Users,$BASE_DN
changetype: modify
add: uidNumber
uidNumber: 2000000
add: gidNumber
gidNumber: 2000100
add: unixHomeDirectory
unixHomeDirectory: /home/admin
add: loginShell
loginShell: /bin/tcsh
add: gecos
gecos: Administrator Full Name

dn: CN=Domain Users,CN=Users,$BASE_DN
changetype: modify
add: gidNumber
gidNumber: 2000001

dn: CN=Domain Admins,CN=Users,$BASE_DN
changetype: modify
add: gidNumber
gidNumber: 2000002

dn: ou=sub,$BASE_DN
changetype: add
objectClass: organizationalUnit

dn: cn=forbidden,ou=sub,$BASE_DN
changetype: add
objectClass: user
samaccountName: forbidden
uidNumber: 2000003
gidNumber: 2000001
unixHomeDirectory: /home/forbidden
loginShell: /bin/tcsh
gecos: User in forbidden OU
EOF

#
# Add POSIX ids to trusted domain
#
cat <<EOF | $ldbmodify -H ldap://$TRUST_SERVER \
	-U "$TRUST_DOMAIN\Administrator%$TRUST_PASSWORD"
dn: CN=Administrator,CN=Users,$TRUST_BASE_DN
changetype: modify
add: uidNumber
uidNumber: 2500000

dn: CN=Domain Users,CN=Users,$TRUST_BASE_DN
changetype: modify
add: gidNumber
gidNumber: 2500001

dn: CN=Domain Admins,CN=Users,$TRUST_BASE_DN
changetype: modify
add: gidNumber
gidNumber: 2500002
EOF

#
# Test 1: Test uid of Administrator, should be 2000000
#

out="$($wbinfo -S $DOMAIN_SID-500)"
echo "wbinfo returned: \"$out\", expecting \"2000000\""
test "$out" = "2000000"
ret=$?
testit "Test uid of Administrator is 2000000" test $ret -eq 0 || failed=$(expr $failed + 1)

#
# Test 2: Test gid of Domain Users, should be 2000001
#

out="$($wbinfo -Y $DOMAIN_SID-513)"
echo "wbinfo returned: \"$out\", expecting \"2000001\""
test "$out" = "2000001"
ret=$?
testit "Test uid of Domain Users is 2000001" test $ret -eq 0 || failed=$(expr $failed + 1)

#
# Test 3: Test get userinfo for Administrator works
#

out="$($wbinfo -i $DOMAIN/Administrator)"
echo "wbinfo returned: \"$out\", expecting \"$DOMAIN/administrator:*:2000000:2000100:Administrator Full Name:/home/admin:/bin/tcsh\""
test "$out" = "$DOMAIN/administrator:*:2000000:2000100:Administrator Full Name:/home/admin:/bin/tcsh"
ret=$?
testit "Test get userinfo for Administrator works" test $ret -eq 0 || failed=$(expr $failed + 1)

#
# Test 4: Test lookup from gid to sid
#

out="$($wbinfo -G 2000002)"
echo "wbinfo returned: \"$out\", expecting \"$DOMAIN_SID-512\""
test "$out" = "$DOMAIN_SID-512"
ret=$?
testit "Test gid lookup of Domain Admins" test $ret -eq 0 || failed=$(expr $failed + 1)

#
# Test 5: Make sure deny_ou is really denied
# This depends on the "deny ous" setting in Samba3.pm
#

sid="$($wbinfo -n $DOMAIN/forbidden | awk '{print $1}')"
testit "Could create forbidden" test -n "$sid" || failed=$(expr $failed + 1)
if [ -n "$sid" ]
then
    uid="$($wbinfo --sid-to-uid $sid)"
    testit "Can not resolve forbidden user" test -z "$uid" ||
	failed=$(($failed + 1))
fi

#
# Trusted domain test 1: Test uid of Administrator, should be 2500000
#

out="$($wbinfo -S $TRUST_DOMAIN_SID-500)"
echo "wbinfo returned: \"$out\", expecting \"2500000\""
test "$out" = "2500000"
ret=$?
testit "Test uid of Administrator in trusted domain is 2500000" test $ret -eq 0 || failed=$(expr $failed + 1)

#
# Trusted domain test 2: Test gid of Domain Users, should be 2500001
#

out="$($wbinfo -Y $TRUST_DOMAIN_SID-513)"
echo "wbinfo returned: \"$out\", expecting \"2500001\""
test "$out" = "2500001"
ret=$?
testit "Test uid of Domain Users in trusted domain is 2500001" test $ret -eq 0 || failed=$(expr $failed + 1)

#
# Trusted domain test 3: Test get userinfo for Administrator works
#

out="$($wbinfo -i $TRUST_DOMAIN/Administrator)"
echo "wbinfo returned: \"$out\", expecting \"$TRUST_DOMAIN/administrator:*:2500000:2500001::/home/$TRUST_DOMAIN/administrator:/bin/false\""
test "$out" = "$TRUST_DOMAIN/administrator:*:2500000:2500001::/home/$TRUST_DOMAIN/administrator:/bin/false"
ret=$?
testit "Test get userinfo for Administrator works" test $ret -eq 0 || failed=$(expr $failed + 1)

#
# Trusted domain test 4: Test lookup from gid to sid
#

out="$($wbinfo -G 2500002)"
echo "wbinfo returned: \"$out\", expecting \"$TRUST_DOMAIN_SID-512\""
test "$out" = "$TRUST_DOMAIN_SID-512"
ret=$?
testit "Test gid lookup of Domain Admins in trusted domain." test $ret -eq 0 || failed=$(expr $failed + 1)

#
# Remove POSIX ids from AD
#
cat <<EOF | $ldbmodify -H ldap://$DC_SERVER -U "$DOMAIN\Administrator%$DC_PASSWORD"
dn: CN=Administrator,CN=Users,$BASE_DN
changetype: modify
delete: uidNumber
uidNumber: 2000000
delete: gidNumber
gidNumber: 2000100
delete: unixHomeDirectory
unixHomeDirectory: /home/admin
delete: loginShell
loginShell: /bin/tcsh
delete: gecos
gecos: Administrator Full Name

dn: CN=Domain Users,CN=Users,$BASE_DN
changetype: modify
delete: gidNumber
gidNumber: 2000001

dn: CN=Domain Admins,CN=Users,$BASE_DN
changetype: modify
delete: gidNumber
gidNumber: 2000002

dn: cn=forbidden,ou=sub,$BASE_DN
changetype: delete

dn: ou=sub,$BASE_DN
changetype: delete
EOF

#
# Remove POSIX ids from trusted domain
#
cat <<EOF | $ldbmodify -H ldap://$TRUST_SERVER \
	-U "$TRUST_DOMAIN\Administrator%$TRUST_PASSWORD"
dn: CN=Administrator,CN=Users,$TRUST_BASE_DN
changetype: modify
delete: uidNumber
uidNumber: 2500000

dn: CN=Domain Users,CN=Users,$TRUST_BASE_DN
changetype: modify
delete: gidNumber
gidNumber: 2500001

dn: CN=Domain Admins,CN=Users,$TRUST_BASE_DN
changetype: modify
delete: gidNumber
gidNumber: 2500002
EOF

exit $failed
