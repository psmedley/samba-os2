/*
   Unix SMB/CIFS implementation.
   Copyright (C) 2001 by Martin Pool <mbp@samba.org>
   Copyright (C) Jim McDonough (jmcd@us.ibm.com)  2003.
   Copyright (C) Stefan Metzmacher	2003

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file dynconfig.c
 *
 * @brief Global configurations, initialized to configured defaults.
 *
 * This file should be the only file that depends on path
 * configuration (--prefix, etc), so that if ./configure is re-run,
 * all programs will be appropriately updated.  Everything else in
 * Samba should import extern variables from here, rather than relying
 * on preprocessor macros.
 *
 * Eventually some of these may become even more variable, so that
 * they can for example consistently be set across the whole of Samba
 * by command-line parameters, config file entries, or environment
 * variables.
 *
 * @todo Perhaps eventually these should be merged into the parameter
 * table?  There's kind of a chicken-and-egg situation there...
 **/

#include "replace.h"
#include "dynconfig.h"
#include "lib/util/memory.h"

#ifndef __OS2__
#define DEFINE_DYN_CONFIG_PARAM(name) \
const char *dyn_##name = name; \
\
bool is_default_dyn_##name(void) \
{\
	if (strcmp(name, dyn_##name) == 0) { \
		return true; \
	} \
	return false; \
}\
\
const char *get_dyn_##name(void) \
{\
	return dyn_##name;\
}\
\
const char *set_dyn_##name(const char *newpath) \
{\
	if (newpath == NULL) { \
		return NULL; \
	} \
	if (strcmp(name, newpath) == 0) { \
		return dyn_##name; \
	} \
	newpath = strdup(newpath);\
	if (newpath == NULL) { \
		return NULL; \
	} \
	if (is_default_dyn_##name()) { \
		/* do not free a static string */ \
	} else if (dyn_##name) {\
		free(discard_const(dyn_##name)); \
	}\
	dyn_##name = newpath; \
	return dyn_##name;\
}
#else
#define DEFINE_DYN_CONFIG_PARAM(name) \
const char *dyn_##name = name; \
\
bool is_default_dyn_##name(void) \
{\
	if (strcmp(name, dyn_##name) == 0) { \
		return true; \
	} \
	return false; \
}\
\
const char *set_dyn_##name(const char *newpath) \
{\
	if (newpath == NULL) { \
		return NULL; \
	} \
	if (strcmp(name, newpath) == 0) { \
		return dyn_##name; \
	} \
	newpath = strdup(newpath);\
	if (newpath == NULL) { \
		return NULL; \
	} \
	if (is_default_dyn_##name()) { \
		/* do not free a static string */ \
	} else if (dyn_##name) {\
		free(discard_const(dyn_##name)); \
	}\
	dyn_##name = newpath; \
	return dyn_##name;\
}
#endif

DEFINE_DYN_CONFIG_PARAM(SBINDIR)
DEFINE_DYN_CONFIG_PARAM(BINDIR)
DEFINE_DYN_CONFIG_PARAM(CONFIGFILE) /**< Location of smb.conf file. **/
DEFINE_DYN_CONFIG_PARAM(LOGFILEBASE) /** Log file directory. **/
DEFINE_DYN_CONFIG_PARAM(LMHOSTSFILE) /** Statically configured LanMan hosts. **/
DEFINE_DYN_CONFIG_PARAM(CODEPAGEDIR)
DEFINE_DYN_CONFIG_PARAM(LIBDIR)
DEFINE_DYN_CONFIG_PARAM(MODULESDIR)
DEFINE_DYN_CONFIG_PARAM(SHLIBEXT)
DEFINE_DYN_CONFIG_PARAM(LOCKDIR)
DEFINE_DYN_CONFIG_PARAM(STATEDIR) /** Persistent state files. Default LOCKDIR */
DEFINE_DYN_CONFIG_PARAM(CACHEDIR) /** Temporary cache files. Default LOCKDIR */
DEFINE_DYN_CONFIG_PARAM(PIDDIR)
DEFINE_DYN_CONFIG_PARAM(NCALRPCDIR)
DEFINE_DYN_CONFIG_PARAM(SMB_PASSWD_FILE)
DEFINE_DYN_CONFIG_PARAM(PRIVATE_DIR)
DEFINE_DYN_CONFIG_PARAM(BINDDNS_DIR)
DEFINE_DYN_CONFIG_PARAM(LOCALEDIR)
DEFINE_DYN_CONFIG_PARAM(NMBDSOCKETDIR)
DEFINE_DYN_CONFIG_PARAM(DATADIR)
DEFINE_DYN_CONFIG_PARAM(SAMBA_DATADIR)
DEFINE_DYN_CONFIG_PARAM(SETUPDIR)
DEFINE_DYN_CONFIG_PARAM(WINBINDD_SOCKET_DIR) /* from winbind_struct_protocol.h in s3 autoconf */
DEFINE_DYN_CONFIG_PARAM(NTP_SIGND_SOCKET_DIR)
DEFINE_DYN_CONFIG_PARAM(PYTHONDIR)
DEFINE_DYN_CONFIG_PARAM(PYTHONARCHDIR)
DEFINE_DYN_CONFIG_PARAM(SCRIPTSBINDIR)

#ifdef __OS2__
// @todo_port change this file so it also works for rpm

/* Directory the binary was called from, same as getbindir() */
const char *get_dyn_SBINDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		char exedir[1024] = "";
		if (!os2_GetExePath(exedir))
		{
			snprintf(buffer, 260, "%s", SBINDIR);
		} else {
			snprintf(buffer, 260, "%s", exedir);
		}
	}

//	if (dyn_SBINDIR == NULL) {
		return buffer;
//	}
//	return dyn_SBINDIR;
}

/* Directory the binary was called from, same as getsbindir() */
const char *get_dyn_BINDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		char exedir[1024] = "";
		if (!os2_GetExePath(exedir))
		{
			snprintf(buffer, 260, "%s", BINDIR);
		} else {
			snprintf(buffer, 260, "%s", exedir);
		}
	}

//	if (dyn_BINDIR == NULL) {
		return buffer;
//	}
//	return dyn_BINDIR;
}

#if 0
/* Directory holding the SWAT files */
const char *get_dyn_SWATDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		char exedir[1024] = "";
		if (!os2_GetExePath(exedir))
		{
			snprintf(buffer, 260, "%s", SWATDIR);
		} else {
			snprintf(buffer, 260, "%s/%s", exedir,"swat");
		}
	}

	if (dyn_SWATDIR == NULL) {
		return buffer;
	}
	return dyn_SWATDIR;
}
#endif

/* Location of smb.conf file. */
const char *get_dyn_CONFIGFILE(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/smb.conf", getenv("ETC"));
	}
	/* Set UNIXROOT to x:\MPTN if UNIXROOT is undefined */
	if (getenv("UNIXROOT") == NULL) {
		static char unixroot[1024] = "";
		strncpy(unixroot,getenv("ETC"),strlen(getenv("ETC"))-4);
		setenv("UNIXROOT",unixroot,0);
	}
	/* Make sure TMPIR points to a proper value */
	static char tmpdir[1024] = "";
	if (getenv("TMPDIR") == NULL && getenv("TEMP") != NULL) {
		strncpy(tmpdir,getenv("TEMP"),strlen(getenv("TEMP")));
		setenv("TMPDIR",tmpdir,0);
	}
	if (getenv("TMPDIR") == NULL) {
		strncpy(tmpdir,getenv("ETC"),2);
		stpcpy(tmpdir,"/OS2/SYSTEM");
		setenv("TMPDIR",tmpdir,0);
	}
	if (is_default_dyn_CONFIGFILE()) { 
		dyn_CONFIGFILE = buffer;
		return buffer;
	}
	return dyn_CONFIGFILE;
}

/** Log file directory. **/
const char *get_dyn_LOGFILEBASE(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/log", getenv("ETC"));
	}
//	if (dyn_LOGFILEBASE == NULL) {
		return buffer;
//	}
//	return dyn_LOGFILEBASE;
}

/** Statically configured LanMan hosts. **/
const char *get_dyn_LMHOSTSFILE(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
 	{
		snprintf(buffer, 260, "%s/samba/lmhosts", getenv("ETC"));
	}
//	if (dyn_LMHOSTSFILE == NULL) {
		return buffer;
//	}
//	return dyn_LMHOSTSFILE;
}

/* Directory holding the codepages */
const char *get_dyn_CODEPAGEDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		char exedir[1024] = "";
		if (!os2_GetExePath(exedir))
		{
			snprintf(buffer, 260, "%s", CODEPAGEDIR);
		} else {
#if 0
			if (!lp_is_in_client()) {
				snprintf(buffer, 260, "%s/%s", exedir, "codepages");
			} else {
#endif
				snprintf(buffer, 260, "%s/%s", getenv("SMB_EXE"), "codepages");
#if 0
			}
#endif
		}
	}

//	if (dyn_CODEPAGEDIR == NULL) {
		return buffer;
//	}
//	return dyn_CODEPAGEDIR;
}

/* Directory holding the libs */
const char *get_dyn_LIBDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		char exedir[1024] = "";
		if (!os2_GetExePath(exedir))
		{
			snprintf(buffer, 260, "%s", LIBDIR);
		} else {
			snprintf(buffer, 260, "%s/%s", exedir, "lib");
		}
	}

//	if (dyn_LIBDIR == NULL) {
		return buffer;
//	}
//	return dyn_LIBDIR;
}


/* Directory holding the modules */
const char *get_dyn_MODULESDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		char exedir[1024] = "";
		if (!os2_GetExePath(exedir))
		{
			snprintf(buffer, 260, "%s", MODULESDIR);
		} else {
			snprintf(buffer, 260, "%s/%s", exedir, "modules");
		}
	}

//	if (dyn_MODULESDIR == NULL) {
		return buffer;
//	}
//	return dyn_MODULESDIR;
}

/* Directory holding the shared libs (same as libdir) */
const char *get_dyn_SHLIBEXT(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		char exedir[1024] = "";
		if (!os2_GetExePath(exedir))
		{
			snprintf(buffer, 260, "%s", LIBDIR);
		} else {
			snprintf(buffer, 260, "%s/%s", exedir, "lib");
		}
	}

//	if (dyn_SHLIBEXT == NULL) {
		return buffer;
//	}
//	return dyn_SHLIBEXT;
}

/* Directory holding lock files */
const char *get_dyn_LOCKDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/lock", getenv("ETC"));
	}
//	if (dyn_LOCKDIR == NULL) {
		return buffer;
//	}
//	return dyn_LOCKDIR;
}

/* Directory holding persistent state files */
const char *get_dyn_STATEDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/lock", getenv("ETC"));
	}
//	if (dyn_STATEDIR == NULL) {
		return buffer;
//	}
//	return dyn_STATEDIR;
}

/* Temporary cache files. Default LOCKDIR */
const char *get_dyn_CACHEDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/lock", getenv("ETC"));
	}
//	if (dyn_CACHEDIR == NULL) {
		return buffer;
//	}
//	return dyn_CACHEDIR;
}

/* Directory holding the pid files */
const char *get_dyn_PIDDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/pid", getenv("ETC"));
	}
//	if (dyn_PIDDIR == NULL) {
		return buffer;
//	}
//	return dyn_PIDDIR;
}

/* Directory holding the NMBDSOCKETDIR */
const char *get_dyn_NMBDSOCKETDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/nmbd", getenv("ETC"));
	}

//	if (dyn_NMBDSOCKETDIR == NULL) {
		return buffer;
//	}
//	return dyn_NMBDSOCKETDIR;
}

/* Directory holding the LOCALEDIR */
const char *get_dyn_LOCALEDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/locale", getenv("ETC"));
	}

//	if (dyn_LOCALEDIR == NULL) {
		return buffer;
//	}
//	return dyn_LOCALEDIR;
}

/* Directory holding the NCALRPCDIR */
const char *get_dyn_NCALRPCDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/ncalrpc", getenv("ETC"));
	}

//	if (dyn_NCALRPCDIR == NULL) {
		return buffer;
//	}
//	return dyn_NCALRPCDIR;
}

/* Location of smbpasswd */
const char *get_dyn_SMB_PASSWD_FILE(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/private/smbpasswd", getenv("ETC"));
	}
//	if (dyn_SMB_PASSWD_FILE == NULL) {
		return buffer;
//	}
//	return dyn_SMB_PASSWD_FILE;
}

/* Directory holding the private files */
const char *get_dyn_PRIVATE_DIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/private", getenv("ETC"));
	}
//	if (dyn_PRIVATE_DIR == NULL) {
		return buffer;
//	}
//	return dyn_PRIVATE_DIR;
}

/* Directory holding the NTP_SIGND_SOCKET_DIR files */
const char *get_dyn_NTP_SIGND_SOCKET_DIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/ntp_signd", getenv("ETC"));
	}
//	if (dyn_NTP_SIGND_SOCKET_DIR == NULL) {
		return buffer;
//	}
//	return dyn_NTP_SIGND_SOCKET_DIR;
}

/* Directory holding the WINBINDD_PRIVILEGED_SOCKET_DIRfiles */
const char *get_dyn_WINBINDD_PRIVILEGED_SOCKET_DIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/winbind", getenv("ETC"));
	}
//	if (dyn_WINBINDD_PRIVILEGED_SOCKET_DIR == NULL) {
		return buffer;
//	}
//	return dyn_WINBINDD_PRIVILEGED_SOCKET_DIR;
}

/* Directory holding the SCRIPTSBINDIR files */
const char *get_dyn_SCRIPTSBINDIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		snprintf(buffer, 260, "%s/samba/scripts", getenv("ETC"));
	}
//	if (dyn_SCRIPTSBINDIR == NULL) {
		return buffer;
//	}
//	return dyn_SCRIPTSBINDIR;
}

/* Directory holding the BINDDNS_DIR files */
const char *get_dyn_BINDDNS_DIR(void)
{
	static char buffer[1024] = "";
//	if (!*buffer)
//	{
//		snprintf(buffer, 260, "%s/samba/scripts", getenv("ETC"));
//	}
//	if (dyn_SCRIPTSBINDIR == NULL) {
		return buffer;
//	}
//	return dyn_SCRIPTSBINDIR;
}

/* Directory holding the DATADIR */
const char *get_dyn_DATADIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		char exedir[1024] = "";
		if (!os2_GetExePath(exedir))
		{
			snprintf(buffer, 260, "%s", DATADIR);
		} else {
			snprintf(buffer, 260, "%s/%s", exedir, "data");
		}
	}

//	if (dyn_DATADIR == NULL) {
		return buffer;
//	}
//	return dyn_DATADIR;
}

/* Directory holding the DATADIR */
const char *get_dyn_SAMBA_DATADIR(void)
{
	static char buffer[1024] = "";
	if (!*buffer)
	{
		char exedir[1024] = "";
		if (!os2_GetExePath(exedir))
		{
			snprintf(buffer, 260, "%s", SAMBA_DATADIR);
		} else {
			snprintf(buffer, 260, "%s/%s", exedir, "sambadata");
		}
	}

//	if (dyn_DATADIR == NULL) {
		return buffer;
//	}
//	return dyn_DATADIR;
}

#endif /* __OS2__ */
