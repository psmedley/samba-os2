/*
   Unix SMB/CIFS implementation.

   Functions to create reasonable random numbers for crypto use.

   Copyright (C) Jeremy Allison 2001

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
#include "lib/util/genrand.h"
#include "sys_rw_data.h"
#include "lib/util/blocking.h"

static int urand_fd = -1;

static void open_urandom(void)
{
	if (urand_fd != -1) {
		return;
	}
	urand_fd = open( "/dev/urandom", O_RDONLY,0);
	if (urand_fd == -1) {
		abort();
	}
	smb_set_close_on_exec(urand_fd);
}

_PUBLIC_ void generate_random_buffer(uint8_t *out, int len)
{
	ssize_t rw_ret;

#ifdef __OS2__
	unsigned char md4_buf[64];
	unsigned char tmp_buf[16];
	unsigned char *p;

	/*
	 * Generate random numbers in chunks of 64 bytes,
	 * then md4 them & copy to the output buffer.
	 * This way the raw state of the stream is never externally
	 * seen.
	 */

	p = out;
	while(len > 0) {
		int copy_len = len > 16 ? 16 : len;

		os2_randget(md4_buf, sizeof(md4_buf));
		mdfour(tmp_buf, md4_buf, sizeof(md4_buf));
		memcpy(p, tmp_buf, copy_len);
		p += copy_len;
		len -= copy_len;
	}
#else
	open_urandom();

	rw_ret = read_data(urand_fd, out, len);
	if (rw_ret != len) {
		abort();
	}
#endif
}

/*
 * Keep generate_secret_buffer in case we ever want to do something
 * different
 */
_PUBLIC_ void generate_secret_buffer(uint8_t *out, int len)
{
	generate_random_buffer(out, len);
}
