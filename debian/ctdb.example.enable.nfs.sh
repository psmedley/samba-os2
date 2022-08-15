#!/bin/bash -e

base="/usr/share/doc/ctdb/examples/nfs-kernel-server/"
logfile="/tmp/enable-ctdb-nfs.$$.log" ; touch $logfile ;
ghostname=""

# functions ---------

die() { echo error: $@; echo ; exit 1; };
getout() { echo exit: $@; echo ; exit 0; };
stopservice() { echo stopping $1... ; systemctl stop $1 2>&1 >> $logfile 2>&1; }
disableservice() { echo disabling $1... ; systemctl disable $1 2>&1 >> $logfile 2>&1; }
startservice() { echo starting $1... ; systemctl start $1 2>&1 >> $logfile 2>&1; }
sysctlrefresh() { echo refreshing sysctl... ; sysctl --system 2>&1 >> $logfile 2>&1; }

backupfile() {
    echo backing up $1
    [ -f $1.prvctdb ] && die "backup file $1 already exists!"
    [ -f $1 ] && cp $1 $1.prvctdb || true
}

checkservice() {
    (systemctl list-unit-files | grep -q $1.service) || die "service $1 not found"
}

replacefile() {

    origfile=$1
    replfile=$2


    [ ! -f $base/$origfile ] && die "coult not find $base/$origfile"

    echo replacing $replfile...
    cp $base/$origfile $replfile
}

appendfile() {

    origfile=$1
    replfile=$2

    [ ! -f $base/$origfile ] && die "coult not find $base/$origfile"

    echo appending $base/$origfile to $replfile...
    cat $base/$origfile >> $replfile
}

appendnfsenv() {

    file=$1 ; [ -f $file ] || die "inexistent file $file";

    echo appending NFS_HOSTNAME to $file...

    grep -q "NFS_HOSTNAME" $file || \
    {
        echo
        echo "echo NFS_HOSTNAME=\\\"\$NFS_HOSTNAME\\\"" \>\> \/run\/sysconfig\/nfs-utils
        echo
    } >> $file
}

execnfsenv() {

    file=$1 ; [ -f $file ] || due "inexistent file $file";

    echo executing $file...

    $file 2>&1 >> $logfile 2>&1;
}

fixnfshostname() {

    file=$1 ; [ -f $file ] || due "inexistent file $file";

    if [ "$ghostname" == "" ]; then
        echo "What is the FQDN for the public IP address of this host ?"
        echo -n "> "
        read ghostname
    fi

    echo placing hostname $ghostname into $file...
    sed -i "s:PLACE_HOSTNAME_HERE:$ghostname:g" $file
}

# end of functions --

[ $UID != 0 ] && die "you need root privileges"

echo """
This script will enable CTDB NFS HA by changing the following files:

(1) /etc/default/nfs-common                   ( replace )
(2) /etc/default/nfs-kernel-server            ( replace )
(3) /etc/services                             ( append  )
(4) /etc/sysctl.d/99-nfs-static-ports.conf    ( create  )
(5) /usr/lib/systemd/scripts/nfs-utils_env.sh ( modify  )

and disabling the following services:

(1) rpcbind
(2) nfs-kernel-server
(3) rpc.rquotad

Obs:
  - replaced files keep previous versions as "file".prevctdb
  - dependant services will also be stopped
"""

while true; do
    echo -n "Do you agree with this change ? (N/y) => "
    read answer
    [ "$answer" == "n" ] && getout "exiting without any changes"
    [ "$answer" == "y" ] && break
done


echo "checking requirements..."

checkservice nfs-kernel-server
checkservice quota
checkservice rpcbind

echo "requirements okay!"
echo

backupfile /etc/default/nfs-common
backupfile /etc/default/nfs-kernel-server
backupfile /etc/services
backupfile /usr/lib/systemd/scripts/nfs-utils_env.sh
echo

set +e

stopservice ctdb.service
stopservice quota.service
stopservice nfs-kernel-server.service
stopservice rpcbind.service
stopservice rpcbind.socket
stopservice rpcbind.target
echo

disableservice ctdb.service
disableservice quota.service
disableservice nfs-kernel-server.service
disableservice rpcbind.service
disableservice rpcbind.socket
disableservice rpcbind.target
echo

set -e

replacefile nfs-common /etc/default/nfs-common
replacefile nfs-kernel-server /etc/default/nfs-kernel-server
replacefile 99-nfs-static-ports.conf /etc/sysctl.d/99-nfs-static-ports.conf
echo

appendfile services /etc/services
echo

fixnfshostname /etc/default/nfs-common
fixnfshostname /etc/default/nfs-kernel-server
echo

appendnfsenv /usr/lib/systemd/scripts/nfs-utils_env.sh
execnfsenv /usr/lib/systemd/scripts/nfs-utils_env.sh
echo

sysctlrefresh
echo

echo """Finished! Make sure to configure properly:

    - /etc/exports (containing the clustered fs to be exported)
    - /etc/ctdb/nodes (containing all your node private IPs)
    - /etc/ctdb/public_addressess (containing public addresses)

A log file can be found at:

    - /tmp/enable-ctdb-nfs.$$.log

Remember:

    - to place a recovery lock in /etc/ctdb/ctdb.conf:
        ...
        [cluster]
        recovery lock = /clustered.filesystem/.reclock
        ...

And, make sure you enable ctdb service again:

    - systemctl enable ctdb.service
    - systemctl start ctdb.service

Enjoy!
"""
