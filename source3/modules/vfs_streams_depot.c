/*
 * Store streams in a separate subdirectory
 *
 * Copyright (C) Volker Lendecke, 2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "smbd/smbd.h"
#include "system/filesys.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

/*
 * Excerpt from a mail from tridge:
 *
 * Volker, what I'm thinking of is this:
 * /mount-point/.streams/XX/YY/aaaa.bbbb/namedstream1
 * /mount-point/.streams/XX/YY/aaaa.bbbb/namedstream2
 *
 * where XX/YY is a 2 level hash based on the fsid/inode. "aaaa.bbbb"
 * is the fsid/inode. "namedstreamX" is a file named after the stream
 * name.
 */

static uint32_t hash_fn(DATA_BLOB key)
{
	uint32_t value;	/* Used to compute the hash value.  */
	uint32_t i;	/* Used to cycle through random values. */

	/* Set the initial value from the key size. */
	for (value = 0x238F13AF * key.length, i=0; i < key.length; i++)
		value = (value + (key.data[i] << (i*5 % 24)));

	return (1103515243 * value + 12345);
}

/*
 * With the hashing scheme based on the inode we need to protect against
 * streams showing up on files with re-used inodes. This can happen if we
 * create a stream directory from within Samba, and a local process or NFS
 * client deletes the file without deleting the streams directory. When the
 * inode is re-used and the stream directory is still around, the streams in
 * there would be show up as belonging to the new file.
 *
 * There are several workarounds for this, probably the easiest one is on
 * systems which have a true birthtime stat element: When the file has a later
 * birthtime than the streams directory, then we have to recreate the
 * directory.
 *
 * The other workaround is to somehow mark the file as generated by Samba with
 * something that a NFS client would not do. The closest one is a special
 * xattr value being set. On systems which do not support xattrs, it might be
 * an option to put in a special ACL entry for a non-existing group.
 */

static bool file_is_valid(vfs_handle_struct *handle,
			const struct smb_filename *smb_fname)
{
	char buf;
	NTSTATUS status;
	struct smb_filename *pathref = NULL;
	int ret;

	DEBUG(10, ("file_is_valid (%s) called\n", smb_fname->base_name));

	status = synthetic_pathref(talloc_tos(),
				handle->conn->cwd_fsp,
                                smb_fname->base_name,
                                NULL,
                                NULL,
                                smb_fname->twrp,
                                smb_fname->flags,
                                &pathref);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	ret = SMB_VFS_FGETXATTR(pathref->fsp,
				SAMBA_XATTR_MARKER,
				&buf,
				sizeof(buf));
	if (ret != sizeof(buf)) {
		int saved_errno = errno;
		DBG_DEBUG("FGETXATTR failed: %s\n", strerror(saved_errno));
		TALLOC_FREE(pathref);
		errno = saved_errno;
		return false;
	}

	TALLOC_FREE(pathref);

	if (buf != '1') {
		DEBUG(10, ("got wrong buffer content: '%c'\n", buf));
		return false;
	}

	return true;
}

/*
 * Return the root of the stream directory. Can be
 * external to the share definition but by default
 * is "handle->conn->connectpath/.streams".
 *
 * Note that this is an *absolute* path, starting
 * with '/', so the dirfsp being used in the
 * calls below isn't looked at.
 */

static char *stream_rootdir(vfs_handle_struct *handle,
			    TALLOC_CTX *ctx)
{
	const struct loadparm_substitution *lp_sub =
		loadparm_s3_global_substitution();
	char *tmp;

	tmp = talloc_asprintf(ctx,
			      "%s/.streams",
			      handle->conn->connectpath);
	if (tmp == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	return lp_parm_substituted_string(ctx,
					  lp_sub,
					  SNUM(handle->conn),
					  "streams_depot",
					  "directory",
					  tmp);
}

/**
 * Given an smb_filename, determine the stream directory using the file's
 * base_name.
 */
static char *stream_dir(vfs_handle_struct *handle,
			const struct smb_filename *smb_fname,
			const SMB_STRUCT_STAT *base_sbuf, bool create_it)
{
	uint32_t hash;
	struct smb_filename *smb_fname_hash = NULL;
	char *result = NULL;
	SMB_STRUCT_STAT base_sbuf_tmp;
	char *tmp = NULL;
	uint8_t first, second;
	char *id_hex;
	struct file_id id;
	uint8_t id_buf[16];
	bool check_valid;
	char *rootdir = NULL;
	struct smb_filename *rootdir_fname = NULL;
	struct smb_filename *tmp_fname = NULL;
	int ret;

	check_valid = lp_parm_bool(SNUM(handle->conn),
		      "streams_depot", "check_valid", true);

	rootdir = stream_rootdir(handle,
				 talloc_tos());
	if (rootdir == NULL) {
		errno = ENOMEM;
		goto fail;
	}

	rootdir_fname = synthetic_smb_fname(talloc_tos(),
					rootdir,
					NULL,
					NULL,
					smb_fname->twrp,
					smb_fname->flags);
	if (rootdir_fname == NULL) {
		errno = ENOMEM;
		goto fail;
	}

	/* Stat the base file if it hasn't already been done. */
	if (base_sbuf == NULL) {
		struct smb_filename *smb_fname_base;

		smb_fname_base = synthetic_smb_fname(
					talloc_tos(),
					smb_fname->base_name,
					NULL,
					NULL,
					smb_fname->twrp,
					smb_fname->flags);
		if (smb_fname_base == NULL) {
			errno = ENOMEM;
			goto fail;
		}
		if (SMB_VFS_NEXT_STAT(handle, smb_fname_base) == -1) {
			TALLOC_FREE(smb_fname_base);
			goto fail;
		}
		base_sbuf_tmp = smb_fname_base->st;
		TALLOC_FREE(smb_fname_base);
	} else {
		base_sbuf_tmp = *base_sbuf;
	}

	id = SMB_VFS_FILE_ID_CREATE(handle->conn, &base_sbuf_tmp);

	push_file_id_16((char *)id_buf, &id);

	hash = hash_fn(data_blob_const(id_buf, sizeof(id_buf)));

	first = hash & 0xff;
	second = (hash >> 8) & 0xff;

	id_hex = hex_encode_talloc(talloc_tos(), id_buf, sizeof(id_buf));

	if (id_hex == NULL) {
		errno = ENOMEM;
		goto fail;
	}

	result = talloc_asprintf(talloc_tos(), "%s/%2.2X/%2.2X/%s", rootdir,
				 first, second, id_hex);

	TALLOC_FREE(id_hex);

	if (result == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	smb_fname_hash = synthetic_smb_fname(talloc_tos(),
					result,
					NULL,
					NULL,
					smb_fname->twrp,
					smb_fname->flags);
	if (smb_fname_hash == NULL) {
		errno = ENOMEM;
		goto fail;
	}

	if (SMB_VFS_NEXT_STAT(handle, smb_fname_hash) == 0) {
		struct smb_filename *smb_fname_new = NULL;
		char *newname;
		bool delete_lost;

		if (!S_ISDIR(smb_fname_hash->st.st_ex_mode)) {
			errno = EINVAL;
			goto fail;
		}

		if (!check_valid ||
		    file_is_valid(handle, smb_fname)) {
			return result;
		}

		/*
		 * Someone has recreated a file under an existing inode
		 * without deleting the streams directory.
		 * Move it away or remove if streams_depot:delete_lost is set.
		 */

	again:
		delete_lost = lp_parm_bool(SNUM(handle->conn), "streams_depot",
					   "delete_lost", false);

		if (delete_lost) {
			DEBUG(3, ("Someone has recreated a file under an "
			      "existing inode. Removing: %s\n",
			      smb_fname_hash->base_name));
			recursive_rmdir(talloc_tos(), handle->conn,
					smb_fname_hash);
			SMB_VFS_NEXT_UNLINKAT(handle,
					handle->conn->cwd_fsp,
					smb_fname_hash,
					AT_REMOVEDIR);
		} else {
			newname = talloc_asprintf(talloc_tos(), "lost-%lu",
						  random());
			DEBUG(3, ("Someone has recreated a file under an "
			      "existing inode. Renaming: %s to: %s\n",
			      smb_fname_hash->base_name,
			      newname));
			if (newname == NULL) {
				errno = ENOMEM;
				goto fail;
			}

			smb_fname_new = synthetic_smb_fname(
						talloc_tos(),
						newname,
						NULL,
						NULL,
						smb_fname->twrp,
						smb_fname->flags);
			TALLOC_FREE(newname);
			if (smb_fname_new == NULL) {
				errno = ENOMEM;
				goto fail;
			}

			if (SMB_VFS_NEXT_RENAMEAT(handle,
					handle->conn->cwd_fsp,
					smb_fname_hash,
					handle->conn->cwd_fsp,
					smb_fname_new) == -1) {
				TALLOC_FREE(smb_fname_new);
				if ((errno == EEXIST) || (errno == ENOTEMPTY)) {
					goto again;
				}
				goto fail;
			}

			TALLOC_FREE(smb_fname_new);
		}
	}

	if (!create_it) {
		errno = ENOENT;
		goto fail;
	}

	ret = SMB_VFS_NEXT_MKDIRAT(handle,
				handle->conn->cwd_fsp,
				rootdir_fname,
				0755);
	if ((ret != 0) && (errno != EEXIST)) {
		goto fail;
	}

	tmp = talloc_asprintf(result, "%s/%2.2X", rootdir, first);
	if (tmp == NULL) {
		errno = ENOMEM;
		goto fail;
	}

	tmp_fname = synthetic_smb_fname(talloc_tos(),
					tmp,
					NULL,
					NULL,
					smb_fname->twrp,
					smb_fname->flags);
	if (tmp_fname == NULL) {
		errno = ENOMEM;
		goto fail;
	}

	ret = SMB_VFS_NEXT_MKDIRAT(handle,
				handle->conn->cwd_fsp,
				tmp_fname,
				0755);
	if ((ret != 0) && (errno != EEXIST)) {
		goto fail;
	}

	TALLOC_FREE(tmp);
	TALLOC_FREE(tmp_fname);

	tmp = talloc_asprintf(result, "%s/%2.2X/%2.2X", rootdir, first,
			      second);
	if (tmp == NULL) {
		errno = ENOMEM;
		goto fail;
	}

	tmp_fname = synthetic_smb_fname(talloc_tos(),
					tmp,
					NULL,
					NULL,
					smb_fname->twrp,
					smb_fname->flags);
	if (tmp_fname == NULL) {
		errno = ENOMEM;
		goto fail;
	}

	ret = SMB_VFS_NEXT_MKDIRAT(handle,
			handle->conn->cwd_fsp,
			tmp_fname,
			0755);
	if ((ret != 0) && (errno != EEXIST)) {
		goto fail;
	}

	TALLOC_FREE(tmp);
	TALLOC_FREE(tmp_fname);

	/* smb_fname_hash is the struct smb_filename version of 'result' */
	ret = SMB_VFS_NEXT_MKDIRAT(handle,
			handle->conn->cwd_fsp,
			smb_fname_hash,
			0755);
	if ((ret != 0) && (errno != EEXIST)) {
		goto fail;
	}

	TALLOC_FREE(rootdir_fname);
	TALLOC_FREE(rootdir);
	TALLOC_FREE(tmp_fname);
	TALLOC_FREE(smb_fname_hash);
	return result;

 fail:
	TALLOC_FREE(rootdir_fname);
	TALLOC_FREE(rootdir);
	TALLOC_FREE(tmp_fname);
	TALLOC_FREE(smb_fname_hash);
	TALLOC_FREE(result);
	return NULL;
}
/**
 * Given a stream name, populate smb_fname_out with the actual location of the
 * stream.
 */
static NTSTATUS stream_smb_fname(vfs_handle_struct *handle,
				 const struct stat_ex *base_sbuf,
				 const struct smb_filename *smb_fname,
				 struct smb_filename **smb_fname_out,
				 bool create_dir)
{
	char *dirname, *stream_fname;
	const char *stype;
	NTSTATUS status;

	*smb_fname_out = NULL;

	stype = strchr_m(smb_fname->stream_name + 1, ':');

	if (stype) {
		if (strcasecmp_m(stype, ":$DATA") != 0) {
			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	dirname = stream_dir(handle, smb_fname, base_sbuf, create_dir);

	if (dirname == NULL) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	stream_fname = talloc_asprintf(talloc_tos(), "%s/%s", dirname,
				       smb_fname->stream_name);

	if (stream_fname == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (stype == NULL) {
		/* Append an explicit stream type if one wasn't specified. */
		stream_fname = talloc_asprintf(talloc_tos(), "%s:$DATA",
					       stream_fname);
		if (stream_fname == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto fail;
		}
	} else {
		/* Normalize the stream type to upercase. */
		if (!strupper_m(strrchr_m(stream_fname, ':') + 1)) {
			status = NT_STATUS_INVALID_PARAMETER;
			goto fail;
		}
	}

	DEBUG(10, ("stream filename = %s\n", stream_fname));

	/* Create an smb_filename with stream_name == NULL. */
	*smb_fname_out = synthetic_smb_fname(talloc_tos(),
					stream_fname,
					NULL,
					NULL,
					smb_fname->twrp,
					smb_fname->flags);
	if (*smb_fname_out == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;

 fail:
	DEBUG(5, ("stream_name failed: %s\n", strerror(errno)));
	TALLOC_FREE(*smb_fname_out);
	return status;
}

static NTSTATUS walk_streams(vfs_handle_struct *handle,
			     struct smb_filename *smb_fname_base,
			     char **pdirname,
			     bool (*fn)(const struct smb_filename *dirname,
					const char *dirent,
					void *private_data),
			     void *private_data)
{
	char *dirname;
	char *rootdir = NULL;
	char *orig_connectpath = NULL;
	struct smb_filename *dir_smb_fname = NULL;
	struct smb_Dir *dir_hnd = NULL;
	const char *dname = NULL;
	long offset = 0;
	char *talloced = NULL;
	NTSTATUS status;

	dirname = stream_dir(handle, smb_fname_base, &smb_fname_base->st,
			     false);

	if (dirname == NULL) {
		if (errno == ENOENT) {
			/*
			 * no stream around
			 */
			return NT_STATUS_OK;
		}
		return map_nt_error_from_unix(errno);
	}

	DEBUG(10, ("walk_streams: dirname=%s\n", dirname));

	dir_smb_fname = synthetic_smb_fname(talloc_tos(),
					dirname,
					NULL,
					NULL,
					smb_fname_base->twrp,
					smb_fname_base->flags);
	if (dir_smb_fname == NULL) {
		TALLOC_FREE(dirname);
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * For OpenDir to succeed if the stream rootdir is outside
	 * the share path, we must temporarily swap out the connect
	 * path for this share. We're dealing with absolute paths
	 * here so we don't care about chdir calls.
	 */
	rootdir = stream_rootdir(handle, talloc_tos());
	if (rootdir == NULL) {
		TALLOC_FREE(dir_smb_fname);
		TALLOC_FREE(dirname);
		return NT_STATUS_NO_MEMORY;
	}

	orig_connectpath = handle->conn->connectpath;
	handle->conn->connectpath = rootdir;

	status = OpenDir(
		talloc_tos(), handle->conn, dir_smb_fname, NULL, 0, &dir_hnd);
	if (!NT_STATUS_IS_OK(status)) {
		handle->conn->connectpath = orig_connectpath;
		TALLOC_FREE(rootdir);
		TALLOC_FREE(dir_smb_fname);
		TALLOC_FREE(dirname);
		return status;
	}

        while ((dname = ReadDirName(dir_hnd, &offset, NULL, &talloced))
	       != NULL)
	{
		if (ISDOT(dname) || ISDOTDOT(dname)) {
			TALLOC_FREE(talloced);
			continue;
		}

		DBG_DEBUG("dirent=%s\n", dname);

		if (!fn(dir_smb_fname, dname, private_data)) {
			TALLOC_FREE(talloced);
			break;
		}
		TALLOC_FREE(talloced);
	}

	/* Restore the original connectpath. */
	handle->conn->connectpath = orig_connectpath;
	TALLOC_FREE(rootdir);
	TALLOC_FREE(dir_smb_fname);
	TALLOC_FREE(dir_hnd);

	if (pdirname != NULL) {
		*pdirname = dirname;
	}
	else {
		TALLOC_FREE(dirname);
	}

	return NT_STATUS_OK;
}

static int streams_depot_stat(vfs_handle_struct *handle,
			      struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_stream = NULL;
	NTSTATUS status;
	int ret = -1;

	DEBUG(10, ("streams_depot_stat called for [%s]\n",
		   smb_fname_str_dbg(smb_fname)));

	if (!is_named_stream(smb_fname)) {
		return SMB_VFS_NEXT_STAT(handle, smb_fname);
	}

	/* Stat the actual stream now. */
	status = stream_smb_fname(
		handle, NULL, smb_fname, &smb_fname_stream, false);
	if (!NT_STATUS_IS_OK(status)) {
		ret = -1;
		errno = map_errno_from_nt_status(status);
		goto done;
	}

	ret = SMB_VFS_NEXT_STAT(handle, smb_fname_stream);

	/* Update the original smb_fname with the stat info. */
	smb_fname->st = smb_fname_stream->st;
 done:
	TALLOC_FREE(smb_fname_stream);
	return ret;
}



static int streams_depot_lstat(vfs_handle_struct *handle,
			       struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_stream = NULL;
	NTSTATUS status;
	int ret = -1;

	DEBUG(10, ("streams_depot_lstat called for [%s]\n",
		   smb_fname_str_dbg(smb_fname)));

	if (!is_named_stream(smb_fname)) {
		return SMB_VFS_NEXT_LSTAT(handle, smb_fname);
	}

	/* Stat the actual stream now. */
	status = stream_smb_fname(
		handle, NULL, smb_fname, &smb_fname_stream, false);
	if (!NT_STATUS_IS_OK(status)) {
		ret = -1;
		errno = map_errno_from_nt_status(status);
		goto done;
	}

	ret = SMB_VFS_NEXT_LSTAT(handle, smb_fname_stream);

 done:
	TALLOC_FREE(smb_fname_stream);
	return ret;
}

static int streams_depot_openat(struct vfs_handle_struct *handle,
				const struct files_struct *dirfsp,
				const struct smb_filename *smb_fname,
				struct files_struct *fsp,
				const struct vfs_open_how *how)
{
	struct smb_filename *smb_fname_stream = NULL;
	struct files_struct *fspcwd = NULL;
	NTSTATUS status;
	bool create_it;
	int ret = -1;

	if (!is_named_stream(smb_fname)) {
		return SMB_VFS_NEXT_OPENAT(handle,
					   dirfsp,
					   smb_fname,
					   fsp,
					   how);
	}

	if (how->resolve != 0) {
		errno = ENOSYS;
		return -1;
	}

	SMB_ASSERT(fsp_is_alternate_stream(fsp));
	SMB_ASSERT(dirfsp == NULL);
	SMB_ASSERT(VALID_STAT(fsp->base_fsp->fsp_name->st));

	create_it = (how->flags & O_CREAT);

	/* Determine the stream name, and then open it. */
	status = stream_smb_fname(
		handle,
		&fsp->base_fsp->fsp_name->st,
		fsp->fsp_name,
		&smb_fname_stream,
		create_it);
	if (!NT_STATUS_IS_OK(status)) {
		ret = -1;
		errno = map_errno_from_nt_status(status);
		goto done;
	}

	if (create_it) {
		bool check_valid = lp_parm_bool(
			SNUM(handle->conn),
			"streams_depot",
			"check_valid",
			true);

		if (check_valid) {
			char buf = '1';

			DBG_DEBUG("marking file %s as valid\n",
				  fsp->base_fsp->fsp_name->base_name);

			ret = SMB_VFS_FSETXATTR(
				fsp->base_fsp,
				SAMBA_XATTR_MARKER,
				&buf,
				sizeof(buf),
				0);

			if (ret == -1) {
				DBG_DEBUG("FSETXATTR failed: %s\n",
					  strerror(errno));
				return -1;
			}
		}
	}

	status = vfs_at_fspcwd(talloc_tos(), handle->conn, &fspcwd);
	if (!NT_STATUS_IS_OK(status)) {
		ret = -1;
		errno = map_errno_from_nt_status(status);
		goto done;
	}

	ret = SMB_VFS_NEXT_OPENAT(handle,
				  fspcwd,
				  smb_fname_stream,
				  fsp,
				  how);

 done:
	TALLOC_FREE(smb_fname_stream);
	TALLOC_FREE(fspcwd);
	return ret;
}

static int streams_depot_unlink_internal(vfs_handle_struct *handle,
				struct files_struct *dirfsp,
				const struct smb_filename *smb_fname,
				int flags)
{
	struct smb_filename *full_fname = NULL;
	char *dirname = NULL;
	int ret = -1;

	full_fname = full_path_from_dirfsp_atname(talloc_tos(),
						  dirfsp,
						  smb_fname);
	if (full_fname == NULL) {
		return -1;
	}

	DEBUG(10, ("streams_depot_unlink called for %s\n",
		   smb_fname_str_dbg(full_fname)));

	/* If there is a valid stream, just unlink the stream and return. */
	if (is_named_stream(full_fname)) {
		struct smb_filename *smb_fname_stream = NULL;
		NTSTATUS status;

		status = stream_smb_fname(
			handle, NULL, full_fname, &smb_fname_stream, false);
		TALLOC_FREE(full_fname);
		if (!NT_STATUS_IS_OK(status)) {
			errno = map_errno_from_nt_status(status);
			return -1;
		}

		ret = SMB_VFS_NEXT_UNLINKAT(handle,
				dirfsp->conn->cwd_fsp,
				smb_fname_stream,
				0);

		TALLOC_FREE(smb_fname_stream);
		return ret;
	}

	/*
	 * We potentially need to delete the per-inode streams directory
	 */

	if (full_fname->flags & SMB_FILENAME_POSIX_PATH) {
		ret = SMB_VFS_NEXT_LSTAT(handle, full_fname);
	} else {
		ret = SMB_VFS_NEXT_STAT(handle, full_fname);
		if (ret == -1 && (errno == ENOENT || errno == ELOOP)) {
			if (VALID_STAT(smb_fname->st) &&
					S_ISLNK(smb_fname->st.st_ex_mode)) {
				/*
				 * Original name was a link - Could be
				 * trying to remove a dangling symlink.
				 */
				ret = SMB_VFS_NEXT_LSTAT(handle, full_fname);
			}
		}
	}
	if (ret == -1) {
		TALLOC_FREE(full_fname);
		return -1;
	}

	/*
	 * We know the unlink should succeed as the ACL
	 * check is already done in the caller. Remove the
	 * file *after* the streams.
	 */
	dirname = stream_dir(handle,
			     full_fname,
			     &full_fname->st,
			     false);
	TALLOC_FREE(full_fname);
	if (dirname != NULL) {
		struct smb_filename *smb_fname_dir = NULL;

		smb_fname_dir = synthetic_smb_fname(talloc_tos(),
						    dirname,
						    NULL,
						    NULL,
						    smb_fname->twrp,
						    smb_fname->flags);
		if (smb_fname_dir == NULL) {
			TALLOC_FREE(dirname);
			errno = ENOMEM;
			return -1;
		}

		SMB_VFS_NEXT_UNLINKAT(handle,
				      dirfsp->conn->cwd_fsp,
				      smb_fname_dir,
				      AT_REMOVEDIR);
		TALLOC_FREE(smb_fname_dir);
		TALLOC_FREE(dirname);
	}

	ret = SMB_VFS_NEXT_UNLINKAT(handle,
				dirfsp,
				smb_fname,
				flags);
	return ret;
}

static int streams_depot_rmdir_internal(vfs_handle_struct *handle,
			struct files_struct *dirfsp,
			const struct smb_filename *smb_fname)
{
	struct smb_filename *full_fname = NULL;
	struct smb_filename *smb_fname_base = NULL;
	int ret = -1;

	full_fname = full_path_from_dirfsp_atname(talloc_tos(),
						  dirfsp,
						  smb_fname);
	if (full_fname == NULL) {
		return -1;
	}

	DBG_DEBUG("called for %s\n", full_fname->base_name);

	/*
	 * We potentially need to delete the per-inode streams directory
	 */

	smb_fname_base = synthetic_smb_fname(talloc_tos(),
				full_fname->base_name,
				NULL,
				NULL,
				full_fname->twrp,
				full_fname->flags);
	TALLOC_FREE(full_fname);
	if (smb_fname_base == NULL) {
		errno = ENOMEM;
		return -1;
	}

	if (smb_fname_base->flags & SMB_FILENAME_POSIX_PATH) {
		ret = SMB_VFS_NEXT_LSTAT(handle, smb_fname_base);
	} else {
		ret = SMB_VFS_NEXT_STAT(handle, smb_fname_base);
	}

	if (ret == -1) {
		TALLOC_FREE(smb_fname_base);
		return -1;
	}

	/*
	 * We know the rmdir should succeed as the ACL
	 * check is already done in the caller. Remove the
	 * directory *after* the streams.
	 */
	{
		char *dirname = stream_dir(handle, smb_fname_base,
					   &smb_fname_base->st, false);

		if (dirname != NULL) {
			struct smb_filename *smb_fname_dir =
				synthetic_smb_fname(talloc_tos(),
						dirname,
						NULL,
						NULL,
						smb_fname->twrp,
						smb_fname->flags);
			if (smb_fname_dir == NULL) {
				TALLOC_FREE(smb_fname_base);
				TALLOC_FREE(dirname);
				errno = ENOMEM;
				return -1;
			}
			SMB_VFS_NEXT_UNLINKAT(handle,
					dirfsp->conn->cwd_fsp,
					smb_fname_dir,
					AT_REMOVEDIR);
			TALLOC_FREE(smb_fname_dir);
		}
		TALLOC_FREE(dirname);
	}

	ret = SMB_VFS_NEXT_UNLINKAT(handle,
				dirfsp,
				smb_fname,
				AT_REMOVEDIR);
	TALLOC_FREE(smb_fname_base);
	return ret;
}

static int streams_depot_unlinkat(vfs_handle_struct *handle,
			struct files_struct *dirfsp,
			const struct smb_filename *smb_fname,
			int flags)
{
	int ret;
	if (flags & AT_REMOVEDIR) {
		ret = streams_depot_rmdir_internal(handle,
				dirfsp,
				smb_fname);
	} else {
		ret = streams_depot_unlink_internal(handle,
				dirfsp,
				smb_fname,
				flags);
	}
	return ret;
}

static int streams_depot_renameat(vfs_handle_struct *handle,
				files_struct *srcfsp,
				const struct smb_filename *smb_fname_src,
				files_struct *dstfsp,
				const struct smb_filename *smb_fname_dst)
{
	struct smb_filename *smb_fname_src_stream = NULL;
	struct smb_filename *smb_fname_dst_stream = NULL;
	struct smb_filename *full_src = NULL;
	struct smb_filename *full_dst = NULL;
	bool src_is_stream, dst_is_stream;
	NTSTATUS status;
	int ret = -1;

	DEBUG(10, ("streams_depot_renameat called for %s => %s\n",
		   smb_fname_str_dbg(smb_fname_src),
		   smb_fname_str_dbg(smb_fname_dst)));

	src_is_stream = is_ntfs_stream_smb_fname(smb_fname_src);
	dst_is_stream = is_ntfs_stream_smb_fname(smb_fname_dst);

	if (!src_is_stream && !dst_is_stream) {
		return SMB_VFS_NEXT_RENAMEAT(handle,
					srcfsp,
					smb_fname_src,
					dstfsp,
					smb_fname_dst);
	}

	/* for now don't allow renames from or to the default stream */
	if (is_ntfs_default_stream_smb_fname(smb_fname_src) ||
	    is_ntfs_default_stream_smb_fname(smb_fname_dst)) {
		errno = ENOSYS;
		goto done;
	}

	full_src = full_path_from_dirfsp_atname(talloc_tos(),
						srcfsp,
						smb_fname_src);
	if (full_src == NULL) {
		errno = ENOMEM;
		goto done;
	}

	full_dst = full_path_from_dirfsp_atname(talloc_tos(),
						dstfsp,
						smb_fname_dst);
	if (full_dst == NULL) {
		errno = ENOMEM;
		goto done;
	}

	status = stream_smb_fname(
		handle, NULL, full_src, &smb_fname_src_stream, false);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto done;
	}

	status = stream_smb_fname(
		handle, NULL, full_dst, &smb_fname_dst_stream, false);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto done;
	}

	/*
	 * We must use handle->conn->cwd_fsp as
	 * srcfsp and dstfsp directory handles here
	 * as we used the full pathname from the cwd dir
	 * to calculate the streams directory and filename
	 * within.
	 */
	ret = SMB_VFS_NEXT_RENAMEAT(handle,
				handle->conn->cwd_fsp,
				smb_fname_src_stream,
				handle->conn->cwd_fsp,
				smb_fname_dst_stream);

done:
	TALLOC_FREE(smb_fname_src_stream);
	TALLOC_FREE(smb_fname_dst_stream);
	return ret;
}

static bool add_one_stream(TALLOC_CTX *mem_ctx, unsigned int *num_streams,
			   struct stream_struct **streams,
			   const char *name, off_t size,
			   off_t alloc_size)
{
	struct stream_struct *tmp;

	tmp = talloc_realloc(mem_ctx, *streams, struct stream_struct,
				   (*num_streams)+1);
	if (tmp == NULL) {
		return false;
	}

	tmp[*num_streams].name = talloc_strdup(tmp, name);
	if (tmp[*num_streams].name == NULL) {
		return false;
	}

	tmp[*num_streams].size = size;
	tmp[*num_streams].alloc_size = alloc_size;

	*streams = tmp;
	*num_streams += 1;
	return true;
}

struct streaminfo_state {
	TALLOC_CTX *mem_ctx;
	vfs_handle_struct *handle;
	unsigned int num_streams;
	struct stream_struct *streams;
	NTSTATUS status;
};

static bool collect_one_stream(const struct smb_filename *dirfname,
			       const char *dirent,
			       void *private_data)
{
	const char *dirname = dirfname->base_name;
	struct streaminfo_state *state =
		(struct streaminfo_state *)private_data;
	struct smb_filename *smb_fname = NULL;
	char *sname = NULL;
	bool ret;

	sname = talloc_asprintf(talloc_tos(), "%s/%s", dirname, dirent);
	if (sname == NULL) {
		state->status = NT_STATUS_NO_MEMORY;
		ret = false;
		goto out;
	}

	smb_fname = synthetic_smb_fname(talloc_tos(),
					sname,
					NULL,
					NULL,
					dirfname->twrp,
					0);
	if (smb_fname == NULL) {
		state->status = NT_STATUS_NO_MEMORY;
		ret = false;
		goto out;
	}

	if (SMB_VFS_NEXT_STAT(state->handle, smb_fname) == -1) {
		DEBUG(10, ("Could not stat %s: %s\n", sname,
			   strerror(errno)));
		ret = true;
		goto out;
	}

	if (!add_one_stream(state->mem_ctx,
			    &state->num_streams, &state->streams,
			    dirent, smb_fname->st.st_ex_size,
			    SMB_VFS_GET_ALLOC_SIZE(state->handle->conn, NULL,
						   &smb_fname->st))) {
		state->status = NT_STATUS_NO_MEMORY;
		ret = false;
		goto out;
	}

	ret = true;
 out:
	TALLOC_FREE(sname);
	TALLOC_FREE(smb_fname);
	return ret;
}

static NTSTATUS streams_depot_fstreaminfo(vfs_handle_struct *handle,
					 struct files_struct *fsp,
					 TALLOC_CTX *mem_ctx,
					 unsigned int *pnum_streams,
					 struct stream_struct **pstreams)
{
	struct smb_filename *smb_fname_base = NULL;
	int ret;
	NTSTATUS status;
	struct streaminfo_state state;

	smb_fname_base = synthetic_smb_fname(talloc_tos(),
					fsp->fsp_name->base_name,
					NULL,
					NULL,
					fsp->fsp_name->twrp,
					fsp->fsp_name->flags);
	if (smb_fname_base == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = SMB_VFS_NEXT_FSTAT(handle, fsp, &smb_fname_base->st);
	if (ret == -1) {
		status = map_nt_error_from_unix(errno);
		goto out;
	}

	state.streams = *pstreams;
	state.num_streams = *pnum_streams;
	state.mem_ctx = mem_ctx;
	state.handle = handle;
	state.status = NT_STATUS_OK;

	status = walk_streams(handle,
				smb_fname_base,
				NULL,
				collect_one_stream,
				&state);

	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(state.streams);
		goto out;
	}

	if (!NT_STATUS_IS_OK(state.status)) {
		TALLOC_FREE(state.streams);
		status = state.status;
		goto out;
	}

	*pnum_streams = state.num_streams;
	*pstreams = state.streams;
	status = SMB_VFS_NEXT_FSTREAMINFO(handle,
				fsp->base_fsp ? fsp->base_fsp : fsp,
				mem_ctx,
				pnum_streams,
				pstreams);

 out:
	TALLOC_FREE(smb_fname_base);
	return status;
}

static uint32_t streams_depot_fs_capabilities(struct vfs_handle_struct *handle,
			enum timestamp_set_resolution *p_ts_res)
{
	return SMB_VFS_NEXT_FS_CAPABILITIES(handle, p_ts_res) | FILE_NAMED_STREAMS;
}

static struct vfs_fn_pointers vfs_streams_depot_fns = {
	.fs_capabilities_fn = streams_depot_fs_capabilities,
	.openat_fn = streams_depot_openat,
	.stat_fn = streams_depot_stat,
	.lstat_fn = streams_depot_lstat,
	.unlinkat_fn = streams_depot_unlinkat,
	.renameat_fn = streams_depot_renameat,
	.fstreaminfo_fn = streams_depot_fstreaminfo,
};

static_decl_vfs;
NTSTATUS vfs_streams_depot_init(TALLOC_CTX *ctx)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "streams_depot",
				&vfs_streams_depot_fns);
}
