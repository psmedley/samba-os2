#!/bin/bash

#
# This file is generated by 'bootstrap/template.py --render'
# See also bootstrap/config.py
#

set -xueo pipefail

dnf update -y

dnf install -y \
    --setopt=install_weak_deps=False \
    @development-tools \
    acl \
    attr \
    autoconf \
    avahi-devel \
    bind-utils \
    binutils \
    bison \
    ccache \
    chrpath \
    cups-devel \
    curl \
    dbus-devel \
    docbook-dtds \
    docbook-style-xsl \
    flex \
    gawk \
    gcc \
    gdb \
    git \
    glib2-devel \
    glibc-common \
    glibc-langpack-en \
    glusterfs-api-devel \
    glusterfs-devel \
    gnutls-devel \
    gpgme-devel \
    gzip \
    hostname \
    htop \
    jansson-devel \
    keyutils-libs-devel \
    krb5-devel \
    krb5-server \
    krb5-workstation \
    lcov \
    libacl-devel \
    libarchive-devel \
    libattr-devel \
    libblkid-devel \
    libbsd-devel \
    libcap-devel \
    libcephfs-devel \
    libicu-devel \
    libpcap-devel \
    libtasn1-devel \
    libtasn1-tools \
    libtirpc-devel \
    libunwind-devel \
    liburing-devel \
    libuuid-devel \
    libxslt \
    lmdb \
    lmdb-devel \
    make \
    mingw64-gcc \
    ncurses-devel \
    openldap-devel \
    pam-devel \
    patch \
    perl \
    perl-Archive-Tar \
    perl-ExtUtils-MakeMaker \
    perl-FindBin \
    perl-JSON \
    perl-JSON-Parse \
    perl-Parse-Yapp \
    perl-Test-Base \
    perl-generators \
    perl-interpreter \
    pkgconfig \
    popt-devel \
    procps-ng \
    psmisc \
    python3 \
    python3-cryptography \
    python3-dateutil \
    python3-devel \
    python3-dns \
    python3-gpg \
    python3-libsemanage \
    python3-markdown \
    python3-policycoreutils \
    python3-pyasn1 \
    python3-setproctitle \
    quota-devel \
    readline-devel \
    redhat-lsb \
    rng-tools \
    rpcgen \
    rpcsvc-proto-devel \
    rsync \
    sed \
    sudo \
    systemd-devel \
    tar \
    tracker-devel \
    tree \
    wget \
    which \
    xfsprogs-devel \
    yum-utils \
    zlib-devel

dnf clean all