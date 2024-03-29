/*
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Jelmer Vernooij 2005

     ** NOTE! The following LGPL license applies to the tevent
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#include "talloc.h"
#include "tevent.h"
#include "tevent_internal.h"
#include "tevent_util.h"
#include <fcntl.h>
#ifdef __OS2__
#include <sys/filio.h>
#endif

/**
  return the number of elements in a string list
*/
size_t ev_str_list_length(const char **list)
{
	size_t ret;
	for (ret=0;list && list[ret];ret++) /* noop */ ;
	return ret;
}

/**
  add an entry to a string list
*/
const char **ev_str_list_add(const char **list, const char *s)
{
	size_t len = ev_str_list_length(list);
	const char **ret;

	ret = talloc_realloc(NULL, list, const char *, len+2);
	if (ret == NULL) return NULL;

	ret[len] = talloc_strdup(ret, s);
	if (ret[len] == NULL) return NULL;

	ret[len+1] = NULL;

	return ret;
}


/**
 Set a fd into blocking/nonblocking mode. Uses POSIX O_NONBLOCK if available,
 else
  if SYSV use O_NDELAY
  if BSD use FNDELAY
**/

int ev_set_blocking(int fd, bool set)
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

bool ev_set_close_on_exec(int fd)
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
