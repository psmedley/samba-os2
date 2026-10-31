/*  This file contains Samba helper functions for eComStation (OS/2) - 
 *  do not try to compile on other platforms!
 *  
 *  Copyright (C) 2007-2011
 *                Silvan Scherrer 
 *                Yuri Dario
 *                Paul Smedley
 *                Herwig Bauernfeind
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef __OS2__

#define INCL_LONGLONG
#define INCL_DOS
#define INCL_DOSPROCESS
#define INCL_DOSPROFILE
#define INCL_DOSMISC
#define INCL_DOSMODULEMGR
#define INCL_DOSDATETIME
#define INCL_DOSERRORS

#include <os2.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <types.h>
#include <string.h>
#include <sys/socket.h>
#include <malloc.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <InnoTekLIBC/backend.h>

/* these define the attribute byte as seen by DOS */
#define aRONLY (1L<<0)		/* 0x01 */
#define aHIDDEN (1L<<1)		/* 0x02 */
#define aSYSTEM (1L<<2)		/* 0x04 */
#define aVOLID (1L<<3)		/* 0x08 */
#define aDIR (1L<<4)		/* 0x10 */
#define aARCH (1L<<5)		/* 0x20 */

// Samba DEBUG() needs the following includes and defines
#include <stdbool.h>
#include "../lib/replace/replace.h"
//#include "local.h"
#include "../lib/util/attr.h"
#include "../lib/util/debug.h"

#ifndef ENOATTR
#define ENOATTR 22
#endif

#ifndef IN6ADDR_ANY_INIT
#define IN6ADDR_ANY_INIT \
        {{{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}}
#endif

int os2_ftruncate(int fd, off_t size)
{
	/* We call __libc_Back_ioFileSizeSet directly instead of 
	   ftruncate to force it not to zero expanding files to 
	   optimize Samba performance when copying files */

	int rc = __libc_Back_ioFileSizeSet(fd, size, 0);
	if (rc < 0)
	{
		errno = -rc;
		return -1;
	}
	return 0;
}

int os2_isattribute(char *path, unsigned short attr)
{
	HDIR          hdirFindHandle = HDIR_CREATE;
	FILEFINDBUF3  FindBuffer     = {0};      /* Returned from FindFirst/Next */
	USHORT        dosattr;                   /* attribute to search for */
	ULONG         ulResultBufLen = sizeof(FILEFINDBUF3);
	ULONG         ulFindCount    = 1;        /* Look for 1 file at a time    */
	APIRET        rc             = NO_ERROR; /* Return code                  */
	if (attr == aARCH) 
			dosattr=MUST_HAVE_ARCHIVED;
	else if (attr == aRONLY)
			dosattr=MUST_HAVE_READONLY;
	else if (attr == aSYSTEM)
			dosattr=MUST_HAVE_SYSTEM;
	else if (attr == aHIDDEN)
			dosattr=MUST_HAVE_HIDDEN;

	rc = DosFindFirst( path,                 /* File pattern - all files     */
                       &hdirFindHandle,      /* Directory search handle      */
                       dosattr,
                       &FindBuffer,          /* Result buffer                */
                       ulResultBufLen,       /* Result buffer length         */
                       &ulFindCount,         /* Number of entries to find    */
                       FIL_STANDARD);        /* Return Level 1 file info     */

	if (rc != NO_ERROR) {
		rc = DosFindClose(hdirFindHandle);    /* Close our directory handle */
		return 1;
	} else {
		rc = DosFindClose(hdirFindHandle);    /* Close our directory handle */
	return 0;
	} /* endif */
}

// get the exe name (including path)
bool os2_GetExeName(char *sExeName, int lExeName)
{
	APIRET rc = NO_ERROR;
	PPIB ppib = NULL; 

	// we search for the infoblock to get the module name
	rc = DosGetInfoBlocks(NULL, &ppib);
	if (rc != NO_ERROR)
	{
		return false;
	}

	// with the module name we get the path (including the exe name)
	rc = DosQueryModuleName(ppib->pib_hmte, lExeName, sExeName);
	if (rc != NO_ERROR)
	{
		return false;
	}
	return true;
}

// we search the path of the .exe and return it
int os2_GetExePath(char *buff)
{
	char sExeName [_MAX_PATH];
	char sDrive [_MAX_PATH], sDir [_MAX_DIR]; 

	size_t dirlen;

	if (!os2_GetExeName(sExeName, sizeof(sExeName)))
		return 0;

	// we split to the different values
	_splitpath(sExeName, sDrive, sDir, NULL, NULL);
	// append the directory without its trailing separator
	dirlen = strlen(sDir);
	if (dirlen > 0 && (sDir[dirlen - 1] == '\\' || sDir[dirlen - 1] == '/'))
		dirlen--;
	strncat(sDrive, sDir, dirlen);
	/* callers pass a 1024 byte buffer; _MAX_PATH always fits */
	snprintf(buff, _MAX_PATH, "%s", sDrive);

	return 1;
}

/* OS/2 specific pipe implementation, we have to use socketpair as select on 
   filehandle is not working. */
int os2_pipe(int fds[2])
{
	int rc = 0;

	rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
	return rc;
}

#if 0
void maperrno(int rc)
{
	switch (rc)
	{
		case ERROR_PATH_NOT_FOUND    : 
		case ERROR_FILE_NOT_FOUND    : errno = ENOENT; break;
		case ERROR_INVALID_HANDLE    : errno = EBADF; break;
		case ERROR_ACCESS_DENIED     : errno = EACCES; break;
		case ERROR_BUFFER_OVERFLOW   :
		case ERROR_NOT_ENOUGH_MEMORY : errno = ERANGE; break;
		case ERROR_INVALID_EA_NAME   : errno = ENOATTR; break;
		case ERROR_INVALID_LEVEL     :
		case ERROR_INVALID_PARAMETER : errno = EINVAL; break;
		case ERROR_SHARING_VIOLATION : errno = EACCES; break;
		default : errno = EINVAL;
	}
}

ssize_t unigetxattr (const char *path, int file, const char *name, void *value, size_t size)
{
	int rc, namelen;
	EAOP2       eaop2 = {0};
	PGEA2LIST   pgea2list = NULL;
	PFEA2LIST   pfea2list = NULL;
	char * p;
	
	if ((!path && !file) || !name)
	{
		errno = EINVAL;
		return -1;
	}
	namelen = strlen(name);
	if (namelen > 0xFF)
	{
		errno = EINVAL;
		return -1;
	}
	pgea2list = (PGEA2LIST)calloc(sizeof(GEA2LIST) + namelen + 1, 1);
	pgea2list->list[0].oNextEntryOffset = 0;
	pgea2list->list[0].cbName = namelen;
	strcpy(pgea2list->list[0].szName, name);
	pgea2list->cbList = sizeof(GEA2LIST) + namelen;

	// max ea is 64kb
	pfea2list = (PFEA2LIST)calloc(sizeof(FEA2LIST) + 0x10000, 1);
	pfea2list->cbList = sizeof(FEA2LIST) + 0x10000;

	eaop2.fpGEA2List = pgea2list;
	eaop2.fpFEA2List = pfea2list;
	eaop2.oError = 0;	
	do
	{
		if (path)
		{
			char npath[CCHMAXPATH + 1] = {0};
			strncpy(npath, path, CCHMAXPATH);
			for (p = npath; *p; p++)
			{
				if (*p == '/') *p = '\\';
			}
			rc = DosQueryPathInfo(npath, FIL_QUERYEASFROMLIST, &eaop2, sizeof(eaop2));
		}
		else
		{
			rc = DosQueryFileInfo( file, FIL_QUERYEASFROMLIST, &eaop2, sizeof(eaop2));
		}
		if (rc)
		{
			maperrno(rc);
			rc = -1;
			break;
		}
		if (strnicmp(pfea2list->list[0].szName, name, namelen) || pfea2list->list[0].cbValue == 0)
		{
			errno = ENOATTR;
			rc = -1;
			break;
		}
		rc = pfea2list->list[0].cbValue;
		if (value)
		{
			if (size < rc)
			{
				errno = ERANGE;
				rc = -1;
			}
			else
			{
				p = pfea2list->list[0].szName + pfea2list->list[0].cbName + 1;
				memcpy(value, p, rc);
			}
		}
	} while (0);
	if (pgea2list)
	{
		free(pgea2list);
	}
	if (pfea2list)
	{
		free(pfea2list);
	}

	DEBUG(5,("unigetxattr : (%s:%d) %s %d\n", path ? path : "null", file, name, rc));

	return rc;
}

ssize_t unilistxattr (const char *path, int file, char *list, size_t size)
{
	ssize_t gotsize = 0;
	unsigned long ulCount = -1;
	int rc;
	char * buf, *p = list;
	PFEA2 pfea;
	FILESTATUS4 stat = {0};
	char npath[CCHMAXPATH + 1] = {0};
	if (!path && !file)
	{
		errno = EINVAL;
		return -1;
	}
	if (path)
	{
		char * p;
		strncpy(npath, path, CCHMAXPATH);
		for (p = npath; *p; p++)
		{
			if (*p == '/') *p = '\\';
		}
		rc = DosQueryPathInfo(npath, FIL_QUERYEASIZE, &stat, sizeof(stat));
	}
	else
	{
		rc = DosQueryFileInfo( file, FIL_QUERYEASIZE, &stat, sizeof(stat));
	}
	if (rc)
	{
		DEBUG(4,("unilistxattr1 : (%s:%d) %d\n", path ? npath : "null", file, rc));
		maperrno(rc);
		return -1;		
	}
	if (stat.cbList <= 4)
	{
		// NO ea
		return 0;
	}
	//YD DosEnumAttribute doesn't like high-mem buffers, get a low one.
	buf = (char *)_tmalloc(stat.cbList * 2);
	rc = DosEnumAttribute(path ? 1 : 0, path ? (PVOID)npath : (PVOID)&file, 1, (PVOID)buf, stat.cbList * 2, &ulCount, 1);
	if (rc)
	{
		DEBUG(4,("unilistxattr2 : (%s:%d) %d\n", path ? npath : "null", file, rc));
		maperrno(rc);
		_tfree(buf);
		return -1;
	}
	if (ulCount > 0)
	for (pfea = (PFEA2)buf;;pfea = (PFEA2)((char *)pfea + pfea->oNextEntryOffset))
	{
		if (pfea->cbName > 0)
		{
			gotsize += pfea->cbName + 1;
			if (p && size >= gotsize)
			{
				pfea->szName[pfea->cbName] = 0;
				strcpy(p, pfea->szName);
				p += strlen(p) + 1;
			}
		}
		// had to be added to avoid crash in case of broken extended attributes 
		if (pfea->oNextEntryOffset > 0x10000)
		{
			DEBUG(4, ("Broken Extended Attributes detected for: %s:%d, Last EA:%s\n", path ? path : "null", file, pfea->szName));
			break;
		}
		if (!pfea->oNextEntryOffset)
		{
			break;
		}
	}
	_tfree(buf);

	DEBUG(5,("unilistxattr : (%s:%d) %d\n", path ? path : "null", file, gotsize));

	if (gotsize > size)
	{
		errno = ERANGE;
		return list ? -1 : gotsize;
	}
	return gotsize;
}

int uniremovexattr (const char *path, int file, const char *name)
{
	int rc, namelen;
	EAOP2 eaop2 = {0};
	PFEA2LIST pfea2list = NULL;
	char buf[300] = {0};

	if ((!path && !file) || !name)
	{
		errno = EINVAL;
		return -1;
	}

	namelen = strlen(name);
	if (namelen > 0xFF)
	{
		errno = EINVAL;
		return -1;
	}
	
	pfea2list = (PFEA2LIST)buf;
	pfea2list->list[0].cbName = namelen;
	pfea2list->list[0].cbValue = 0;
	pfea2list->list[0].fEA = 0;
	strcpy(pfea2list->list[0].szName, name);
	pfea2list->cbList = sizeof(FEA2LIST) + namelen;
	eaop2.fpFEA2List = pfea2list;

	if (path)
	{
		char npath[CCHMAXPATH + 1] = {0};
		char * p;
		strncpy(npath, path, CCHMAXPATH);
		for (p = npath; *p; p++)
		{
			if (*p == '/') *p = '\\';
		}
		rc = DosSetPathInfo(npath, FIL_QUERYEASIZE, &eaop2, sizeof(eaop2), DSPI_WRTTHRU);
	}
	else
	{
		rc = DosSetFileInfo( file, FIL_QUERYEASIZE, &eaop2, sizeof(eaop2));
	}
	if (rc)
	{
		maperrno(rc);
		return -1;
	}
	return 0;
}

#ifndef XATTR_CREATE
#define XATTR_CREATE  1
#endif
#ifndef XATTR_REPLACE
#define XATTR_REPLACE 2
#endif

int unisetxattr (const char *path, int file, const char *name, const void *value, size_t size, int flags)
{
	int rc, namelen, totalsize;
	EAOP2       eaop2 = {0};
	PFEA2LIST   pfea2list = NULL;
	char * p;
	
	DEBUG(5,("unisetxattr : (%s:%d) %s %d\n", path ? path : "null", file, name, size));

	if ((!path && !file) || !name || (!value && size))
	{
		errno = EINVAL;
		return -1;
	}
	namelen = strlen(name);
	if (namelen > 0xFF)
	{
		errno = EINVAL;
		return -1;
	}

	if (flags & (XATTR_CREATE | XATTR_REPLACE))
	{
		ssize_t esize = unigetxattr(path, file, name, 0, 0);
		if (flags & XATTR_CREATE && esize > 0)
		{
			errno = EEXIST;
			return -1;
		}
		if (flags & XATTR_REPLACE && esize < 0)
		{
			errno = ENOATTR;
			return -1;
		}
	}

	totalsize = sizeof(FEA2LIST) + size + namelen + 1;

	pfea2list = (PFEA2LIST)calloc(totalsize, 1);
	pfea2list->cbList = totalsize;
	pfea2list->list[0].oNextEntryOffset = 0;
	pfea2list->list[0].cbName = namelen;
	pfea2list->list[0].cbValue = size;
	strcpy(pfea2list->list[0].szName, name);
	if (value)
	{
		memcpy(pfea2list->list[0].szName + namelen + 1, value, size);
	}
	eaop2.fpFEA2List = pfea2list;	

	if (path)
	{
		char npath[CCHMAXPATH + 1] = {0};
		char * p;
		strncpy(npath, path, CCHMAXPATH);
		for (p = npath; *p; p++)
		{
			if (*p == '/') *p = '\\';
		}
		rc = DosSetPathInfo(npath, FIL_QUERYEASIZE, &eaop2, sizeof(eaop2), DSPI_WRTTHRU);
	}
	else
	{
		rc = DosSetFileInfo( file, FIL_QUERYEASIZE, &eaop2, sizeof(eaop2));
	}
	free(pfea2list);
	if (rc)
	{
		maperrno(rc);
		return -1;
	}
	return 0;
}
#endif

/* set date/time on OS/2 */
int os2_setdatetime(time_t t)
{
	struct tm *tm;

	tm = localtime(&t);
	if (!tm) {
		return -1;
	}

	DATETIME   DateTime = {0};
	APIRET     rc       = NO_ERROR;

	/* Get current date/time to properly fill structure */
	rc = DosGetDateTime(&DateTime);
	if (rc != NO_ERROR) {
		return rc;
	}

	DateTime.year    = (USHORT) ((BYTE) tm->tm_year+1900);
	DateTime.month   = (UCHAR) ((BYTE) tm->tm_mon+1);
	DateTime.day     = (UCHAR) ((BYTE) tm->tm_mday);
	DateTime.hours   = (UCHAR) ((BYTE) tm->tm_hour);
	DateTime.minutes = (UCHAR) ((BYTE) tm->tm_min);
	DateTime.seconds = (UCHAR) ((BYTE) tm->tm_sec);

	rc = DosSetDateTime(&DateTime);  /* Update the date and time */

	if (rc!= NO_ERROR) {
		printf ("DosSetDateTime error : return code = %u\n", rc);
	}
	return rc;
}

//const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;

// borrowed from https://github.com/komh/libuv-os2/blob/master/src/unix/os2-send-recv-msg.c
#include <InnoTekLIBC/tcpip.h>
#include <assert.h>
int os2_sendfd(int s, struct msghdr *msg, int flags);

#undef sendmsg
int os2_sendmsg(int s, struct msghdr *msg, int flags) {
  struct iovec *saved_iov = msg->msg_iov;
  int len = msg->msg_iovlen * sizeof(*msg->msg_iov);
  int bytes;
  if (msg->msg_control && msg->msg_controllen)
    return os2_sendfd(s, msg, flags);
  msg->msg_iov = alloca(len);
  memcpy(msg->msg_iov, saved_iov, len);
  bytes = sendmsg(s, msg, flags);
  msg->msg_iov = saved_iov;
  return bytes;
}

struct waitackargs {
  int fd;
  HEV hev;
};

static void waitack(void *args) {
  struct waitackargs *waa = (struct waitackargs *)args;
  ULONG rc;
  do
    rc = DosWaitEventSem(waa->hev, SEM_INDEFINITE_WAIT);
  while (rc == ERROR_INTERRUPT);
  close(waa->fd);
  DosCloseEventSem(waa->hev);
  free(waa);
}

#define MAX_MSG_FDS     64  /* up to 64 fds */

static char signature[] = {'\x7f', 'S', 'R', '\x00' };

struct messageheader {
  char sign[sizeof(signature)]; /* signature for SCM_RIGHTS */
  size_t total_len;             /* sizeof(msg_len) + sizeof(control_len) +
                                 * msg_len + control_len */
  size_t msg_len;               /* length of msg_iov */
  size_t control_len;           /* length of msg_control */
};

/* FIXME:
 * Unexpected behavior may occur if an ancilary data is sent partially.
 */
int os2_sendfd(int s, struct msghdr *msg, int flags) {
  void *saved_msg_control = msg->msg_control;
  struct messageheader hdr;
  struct cmsghdr *cmsg;
  int *fds, fd;
  char *buf, *p;
  int buf_len;
  int bytes;
  size_t i;
  size_t count;
  int saved_errno;
  struct waitackargs *waas[MAX_MSG_FDS];
  struct waitackargs **waas_end = waas + MAX_MSG_FDS;
  struct waitackargs **waa = waas;
  char semname[50];
  ULONG rc;
  for (hdr.msg_len = 0, i = 0; i < msg->msg_iovlen; i++)
    hdr.msg_len += msg->msg_iov[i].iov_len;
  msg->msg_control = alloca(msg->msg_controllen);
  memcpy(msg->msg_control, saved_msg_control, msg->msg_controllen);
  memset(waas, 0, sizeof(waas));
  for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
       cmsg = CMSG_NXTHDR(msg, cmsg)) {
    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
        continue;
    count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(*fds);
    for (fds = (int *)CMSG_DATA(cmsg), i = 0; i < count; i++) {
      fd = _getsockhandle(fds[i]);
      if (fd == -1)
        goto cleanup;
      if (waa == waas_end) {
        errno = EMFILE;
        goto cleanup;
      }
      *waa = calloc(sizeof(**waa), 1);
      if (!*waa)
        goto cleanup;
      (*waa)->fd = dup(fds[i]);
      if ((*waa)->fd == -1)
        goto cleanup;
      snprintf(semname, sizeof(semname), "\\SEM32\\LIBUV\\sockfd\\%x", fd);
      rc = DosCreateEventSem(semname, &(*waa)->hev,
                             DCE_POSTONE | DC_SEM_SHARED, FALSE );
      if (rc)
        rc = DosOpenEventSem(semname, &(*waa)->hev);
      if (rc) {
        errno = ENOMEM;
        goto cleanup;
      }
      if (_beginthread(waitack, NULL, 256 * 1024, *waa) == -1) {
cleanup:
        saved_errno = errno;
        if (waa < waas_end && *waa) {
          DosCloseEventSem((*waa)->hev);
          close((*waa)->fd);
          free(*waa);
          *waa = NULL;
        }
        for (waa = waas; waa < waas_end && *waa; waa++)
          DosPostEventSem((*waa)->hev);
        errno = saved_errno;
        return -1;
      }
      fds[i] = fd;
      waa++;
    }
  }
  hdr.control_len = msg->msg_controllen;
  hdr.total_len = sizeof(hdr.msg_len) +     /* msg_len */
                  sizeof(hdr.control_len) + /* msg_controllen */
                  hdr.msg_len +             /* msg_iov */
                  hdr.control_len;          /* msg_control */
  buf_len = hdr.total_len + sizeof(signature) + sizeof(hdr.total_len);
  p = buf = alloca(buf_len);
  /*
   * Wire format: signature, total_len, msg_len, control_len, data,
   * control. Write the fields one by one so struct padding never ends
   * up on the wire.
   */
  memcpy(p, signature, sizeof(signature));
  p += sizeof(signature);
  memcpy(p, &hdr.total_len, sizeof(hdr.total_len));
  p += sizeof(hdr.total_len);
  memcpy(p, &hdr.msg_len, sizeof(hdr.msg_len));
  p += sizeof(hdr.msg_len);
  memcpy(p, &hdr.control_len, sizeof(hdr.control_len));
  p += sizeof(hdr.control_len);
  for (i = 0; i < msg->msg_iovlen; i++) {
    memcpy(p, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
    p += msg->msg_iov[i].iov_len;
  }
  memcpy(p, msg->msg_control, msg->msg_controllen);
  bytes = send(s, buf, buf_len, flags);
  if (bytes == -1) {
    /*
     * Nobody will ever acknowledge these handles: wake the waitack()
     * threads so they close their dup()ed fds and exit. The caller
     * (e.g. messages_dgm on EAGAIN) may simply retry the whole send.
     */
    saved_errno = errno;
    for (waa = waas; waa < waas_end && *waa; waa++)
      DosPostEventSem((*waa)->hev);
    errno = saved_errno;
  } else if ((size_t)bytes > hdr.msg_len) {
    bytes = hdr.msg_len;
  }
  msg->msg_control = saved_msg_control;
  return bytes;
}

/*
 * Receive side of os2_sendfd().
 *
 * The sender converted each SCM_RIGHTS fd into a system wide OS/2 socket
 * handle (_getsockhandle()) and keeps a dup() of it open in a waitack()
 * thread until we post the "\SEM32\LIBUV\sockfd\<handle>" event
 * semaphore. Import every handle into our own fd table, acknowledge it,
 * and hand the result back to the caller as a normal SCM_RIGHTS message.
 */
static int os2_get_sock_type(int s)
{
  int type = 0;
  socklen_t optlen = sizeof(type);

  if (getsockopt(s, SOL_SOCKET, SO_TYPE, &type, &optlen) != 0)
    return -1;
  return type;
}

static void os2_ack_sockhandle(int handle)
{
  char semname[50];
  HEV hev = NULLHANDLE;

  snprintf(semname, sizeof(semname), "\\SEM32\\LIBUV\\sockfd\\%x", handle);
  if (DosOpenEventSem(semname, &hev) == NO_ERROR) {
    DosPostEventSem(hev);
    DosCloseEventSem(hev);
  }
}

#define OS2_FD_HDR_LEN (sizeof(signature) + 3 * sizeof(size_t))

static int os2_recvfd(int s, struct msghdr *msg, int flags, size_t total_len)
{
  const size_t prefix_len = sizeof(signature) + sizeof(size_t);
  size_t buf_len;
  size_t msg_len, control_len, copied, left, i;
  char *buf, *p, *data, *control;
  struct msghdr cmsg_msg;
  struct cmsghdr *cmsg;
  int out_flags = 0;
  int bytes, got;
  int type;

  /* total_len covers msg_len + control_len and the two payloads */
  if (total_len < 2 * sizeof(size_t) || total_len > 16 * 1024 * 1024) {
    /* garbage: drop the datagram so we don't spin on it */
    char dummy;
    (void)recv(s, &dummy, sizeof(dummy), flags & ~MSG_PEEK);
    return 0;
  }

  buf_len = prefix_len + total_len;
  buf = malloc(buf_len);
  if (buf == NULL) {
    errno = ENOMEM;
    return -1;
  }

  type = os2_get_sock_type(s);
  got = 0;
  do {
    bytes = recv(s, buf + got, buf_len - got, flags & ~MSG_PEEK);
    if (bytes == -1) {
      if (got == 0) {
        free(buf);
        return -1;
      }
      if (errno == EINTR)
        continue;
      break;
    }
    if (bytes == 0)
      break;
    got += bytes;
    /* a datagram arrives in one piece, only loop for streams */
  } while (type == SOCK_STREAM && (size_t)got < buf_len);

  if ((size_t)got < buf_len) {
    /* truncated framing, nothing usable */
    free(buf);
    return 0;
  }

  p = buf + prefix_len;
  memcpy(&msg_len, p, sizeof(msg_len));
  p += sizeof(msg_len);
  memcpy(&control_len, p, sizeof(control_len));
  p += sizeof(control_len);

  if (sizeof(msg_len) + sizeof(control_len) + msg_len + control_len !=
      total_len) {
    free(buf);
    return 0;
  }

  data = p;
  control = p + msg_len;

  /* import and acknowledge every passed socket handle */
  memset(&cmsg_msg, 0, sizeof(cmsg_msg));
  cmsg_msg.msg_control = control;
  cmsg_msg.msg_controllen = control_len;
  for (cmsg = CMSG_FIRSTHDR(&cmsg_msg); cmsg != NULL;
       cmsg = CMSG_NXTHDR(&cmsg_msg, cmsg)) {
    size_t count;
    int *fds;

    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
      continue;
    count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
    fds = (int *)CMSG_DATA(cmsg);
    for (i = 0; i < count; i++) {
      int handle = fds[i];
      int fd = _impsockhandle(handle, 0);

      /* we own a reference now (or failed): let the sender close its */
      os2_ack_sockhandle(handle);
      fds[i] = fd;
    }
  }

  /* scatter the payload into the caller's iovecs */
  copied = 0;
  left = msg_len;
  for (i = 0; i < (size_t)msg->msg_iovlen && left > 0; i++) {
    size_t n = msg->msg_iov[i].iov_len;
    if (n > left)
      n = left;
    memcpy(msg->msg_iov[i].iov_base, data + copied, n);
    copied += n;
    left -= n;
  }
  if (left > 0)
    out_flags |= MSG_TRUNC;

  /* hand back the control data with the imported fds */
  if (control_len == 0) {
    msg->msg_controllen = 0;
  } else if (msg->msg_control == NULL ||
             (size_t)msg->msg_controllen < control_len) {
    /* no room for the fds: don't leak them */
    for (cmsg = CMSG_FIRSTHDR(&cmsg_msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&cmsg_msg, cmsg)) {
      size_t count;
      int *fds;

      if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
        continue;
      count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
      fds = (int *)CMSG_DATA(cmsg);
      for (i = 0; i < count; i++)
        if (fds[i] != -1)
          close(fds[i]);
    }
    msg->msg_controllen = 0;
    out_flags |= MSG_CTRUNC;
  } else {
    memcpy(msg->msg_control, control, control_len);
    msg->msg_controllen = control_len;
  }

  msg->msg_flags = out_flags;
  free(buf);
  return (int)copied;
}

#undef recvmsg
int os2_recvmsg(int s, struct msghdr *msg, int flags) {
  char hdr[OS2_FD_HDR_LEN];
  struct iovec *saved_iov = msg->msg_iov;
  int len = msg->msg_iovlen * sizeof(*msg->msg_iov);
  int bytes;

  /* Was this sent by os2_sendfd()? Peek at the framing header. */
  bytes = recv(s, &hdr, sizeof(hdr), flags | MSG_PEEK);
  if (bytes == -1)
    return -1;
  if ((size_t)bytes == sizeof(hdr) &&
      memcmp(hdr, signature, sizeof(signature)) == 0) {
    size_t total_len;

    if (flags & MSG_PEEK) {
      /* peeking at fd-passing messages is not supported */
      errno = EOPNOTSUPP;
      return -1;
    }
    memcpy(&total_len, hdr + sizeof(signature), sizeof(total_len));
    return os2_recvfd(s, msg, flags, total_len);
  }

  msg->msg_iov = alloca(len);
  memcpy(msg->msg_iov, saved_iov, len);
  bytes = recvmsg(s, msg, flags);
  msg->msg_iov = saved_iov;
  /* the OS/2 stack doesn't pass ancillary data itself */
  msg->msg_control = NULL;
  msg->msg_controllen = 0;
  return bytes;
}

/*
 * OS/2 AF_UNIX socket names live in the \socket\ namespace. Map a Samba
 * socket path ("<dir>/<name>") to that namespace the same way for servers
 * (bind) and clients (connect). Paths already in \socket\ are kept, and
 * all '/' are turned into '\' so both sides end up with the same name.
 */
int os2_unix_socket_path(char *buf, size_t buflen, const char *path)
{
  int len;
  char *q;

  if (strnicmp(path, "\\socket\\", 8) == 0 ||
      strnicmp(path, "/socket/", 8) == 0)
    len = snprintf(buf, buflen, "%s", path);
  else
    len = snprintf(buf, buflen, "\\socket\\samba\\%s", path);
  if (len < 0 || (size_t)len >= buflen)
    return len;

  for (q = buf; *q; q++)
    if (*q == '/')
      *q = '\\';
  return len;
}

#endif
