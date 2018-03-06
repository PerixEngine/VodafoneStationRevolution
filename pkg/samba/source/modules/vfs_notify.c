/****************************************************************************
 *
 * rg/pkg/samba/source/modules/vfs_notify.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include "includes.h"
#include <fcntl.h>
#include <util/mgt_client.h>

static void notify(int ret, int add, connection_struct *conn, const char *path)
{
	static char command[1024];
	char *mgt_result = NULL, *ptr = (char *)path;

	if (ret || !path)
	    return;

	if (ptr[0] == '.' && ptr[1] == '/')
	    ptr+=2;

	snprintf(command, sizeof(command)-1, "vfs_notify %d \"%s/%s\"", add, conn->origpath, ptr);
	mgt_command(command, &mgt_result);
}

static int is_file_readonly(int fd)
{
	int mode = fcntl(fd, F_GETFL);

	return (mode & O_ACCMODE) == O_RDONLY;
}

static int notify_close(vfs_handle_struct *handle, files_struct *fsp, int fd)
{
	int ro = is_file_readonly(fd);
	int ret = SMB_VFS_NEXT_CLOSE(handle, fsp, fd);

	if (!ro)
	    notify(ret, 1, fsp->conn, fsp->fsp_name);

	return ret;
}

static int notify_rename(vfs_handle_struct *handle, const char *oldname, const char *newname)
{
	int ret = SMB_VFS_NEXT_RENAME(handle, oldname, newname);

	notify(ret, 0, handle->conn, oldname);
	notify(ret, 1, handle->conn, newname);

	return ret;
}

static int notify_unlink(vfs_handle_struct *handle, const char *path)
{
	int ret = SMB_VFS_NEXT_UNLINK(handle, path);

	notify(ret, 0, handle->conn, path);

	return ret;
}

static int notify_mkdir(vfs_handle_struct *handle, const char *path, mode_t mode)
{
	int ret = SMB_VFS_NEXT_MKDIR(handle, path, mode);

	notify(ret, 1, handle->conn, path);

	return ret;
}

static int notify_rmdir(vfs_handle_struct *handle, const char *path)
{
	int ret = SMB_VFS_NEXT_RMDIR(handle, path);

	notify(ret, 0, handle->conn, path);

	return ret;
}

/* VFS operations structure */

static vfs_op_tuple notify_op_tuples[] = {

	{SMB_VFS_OP(notify_close),			SMB_VFS_OP_CLOSE,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(notify_rename),			SMB_VFS_OP_RENAME,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(notify_unlink),			SMB_VFS_OP_UNLINK,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(notify_mkdir),			SMB_VFS_OP_MKDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(notify_rmdir),			SMB_VFS_OP_RMDIR,		SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(NULL),				SMB_VFS_OP_NOOP,		SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_notify_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "notify", notify_op_tuples);
}
