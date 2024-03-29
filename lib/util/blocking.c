/*
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2001-2002
   Copyright (C) Simo Sorce 2001
   Copyright (C) Jim McDonough (jmcd@us.ibm.com)  2003.
   Copyright (C) James J Myers 2003

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

#include "replace.h"
#include "system/filesys.h"
#include "blocking.h"

/**
 Set a fd into blocking/nonblocking mode. Uses POSIX O_NONBLOCK if available,
 else
  if SYSV use O_NDELAY
  if BSD use FNDELAY
**/

_PUBLIC_ int set_blocking(int fd, bool set)
{
	int val;
#ifdef O_NONBLOCK
#define FLAG_TO_SET O_NONBLOCK
#else
#ifdef SYSV
#define FLAG_TO_SET O_NDELAY
#else /* BSD */
#define FLAG_TO_SET FNDELAY
#endif
#endif

	if((val = fcntl(fd, F_GETFL, 0)) == -1)
		return -1;
#ifndef __OS2__
	if(set) /* Turn blocking on - ie. clear nonblock flag */
		val &= ~FLAG_TO_SET;
	else
		val |= FLAG_TO_SET;
	return fcntl( fd, F_SETFL, val);
#else
        if(set) /* turn blocking on - ie. clear nonblock flag */
                val = 0;
        else
                val = 1;
        return os2_ioctl(fd, FIONBIO, (char *) &val, sizeof(val));
#endif

#undef FLAG_TO_SET
}


_PUBLIC_ bool smb_set_close_on_exec(int fd)
{
#ifdef FD_CLOEXEC
	int val;

	val = fcntl(fd, F_GETFD, 0);
	if (val >= 0) {
		val |= FD_CLOEXEC;
		val = fcntl(fd, F_SETFD, val);
		if (val != -1) {
			return true;
		}
	}
#endif
	return false;
}
