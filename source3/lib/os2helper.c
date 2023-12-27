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

	if (!os2_GetExeName(sExeName, sizeof(sExeName)))
		return false;

	// we split to the different values
	_splitpath(sExeName, sDrive, sDir, NULL, NULL);
	// strcat(sDrive, sDir);
	strncat(sDrive, sDir, strlen(sDir) -1);
	strcpy(buff, sDrive);

	return true;
}

/* OS/2 specific pipe implementation, we have to use socketpair as select on 
   filehandle is not working. */
int os2_pipe(int fds[2])
{
	int rc = 0;

	rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
	return rc;
}

/* OS/2-specific random functions. these functions used to be based on APR 
   random code, but we discovered some nasty problems with it on fast hardware
   (especially on quadcore) and decided to rewrite it with libc random() */
void os2_randget(char * buffer, int length)
{
	UCHAR randbyte();
	unsigned int idx;

	for (idx=0; idx<length; idx++)
	buffer[idx] = randbyte();

}

UCHAR randbyte()
{
	int c;
	UCHAR byte;
	ULONG ulrandom;
	ulrandom = random();
	
	for (c = 0; c < sizeof(ulrandom); c++) {
		byte ^= ((UCHAR *)&ulrandom)[c];
	}

	return byte;
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

#endif
