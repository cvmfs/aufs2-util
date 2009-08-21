/*
 * Copyright (C) 2009 Junjiro Okajima
 *
 * This program, aufs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define _GNU_SOURCE

#include <sys/vfs.h>    /* or <sys/statfs.h> */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "rdu.h"

static int rdu_pos(struct dirent *de, struct rdu *p, long pos)
{
	int err;
	struct au_rdu_ent *ent;

	err = -1;
	if (pos <= p->npos) {
		ent = p->pos[pos];
		de->d_ino = ent->ino;
		de->d_off = pos;
		de->d_reclen = au_rdu_len(ent->nlen);
		de->d_type = ent->type;
		strcpy(de->d_name, ent->name);
		err = 0;
	}
	return err;
}

static int rdu_readdir(DIR *dir, struct dirent *de, struct dirent **rde)
{
	int err, fd;
	struct rdu *p;
	long pos;
	struct statfs stfs;

	if (rde)
		*rde = NULL;

	errno = EBADF;
	fd = dirfd(dir);
	err = fd;
	if (fd < 0)
		goto out;

	err = fstatfs(fd, &stfs);
	if (err)
		goto out;

	errno = 0;
	if (stfs.f_type == AUFS_SUPER_MAGIC) {
		err = rdu_lib_init();
		if (err)
			goto out;

		p = rdu_buf_lock(fd);
		if (!p)
			goto out;

		pos = telldir(dir);
		if (!pos || !p->npos) {
			err = rdu_init(p);
			if (!err && !de && !p->de) {
				p->de = malloc(sizeof(*p->de));
				if (!p->de) {
					int e = errno;
					rdu_free(p);
					errno = e;
					err = -1;
					goto out;
				}
			}
			if (err) {
				rdu_unlock(p);
				goto out;
			}
		}

		rdu_lib_lock();
		rdu_dgrade_lock(p);
		rdu_lib_unlock();
		if (!de) {
			de = p->de;
			if (!de) {
				rdu_unlock(p);
				errno = EINVAL;
				err = -1;
				goto out;
			}
		}
		err = rdu_pos(de, p, pos);
		if (!err) {
			*rde = de;
			seekdir(dir, pos + 1);
		}
		rdu_unlock(p);
		errno = 0;
	} else if (!de) {
		if (!rdu_dl_readdir()) {
			err = 0;
			*rde = real_readdir(dir);
			if (!*rde)
				err = -1;
		}
	} else {
		if (!rdu_dl_readdir_r())
			err = real_readdir_r(dir, de, rde);
	}
 out:
	return err;
}

struct dirent *(*real_readdir)(DIR *dir);
struct dirent *readdir(DIR *dir)
{
	struct dirent *de;
	int err;

	err = rdu_readdir(dir, NULL, &de);
	DPri("err %d\n", err);
	return de;
}

#ifdef _REENTRANT
int (*real_readdir_r)(DIR *dir, struct dirent *de, struct dirent **rde);
int readdir_r(DIR *dir, struct dirent *de, struct dirent **rde)
{
	return rdu_readdir(dir, de, rde);
}
#endif
