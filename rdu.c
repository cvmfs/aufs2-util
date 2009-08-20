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

#define _ATFILE_SOURCE
#define _GNU_SOURCE

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>    /* or <sys/statfs.h> */
#include <assert.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/aufs_type.h>

#include "compat.h"
#include "rdu.h"

static struct rdu **rdu;
#define RDU_STEP 8
static int rdu_cur, rdu_lim = RDU_STEP;

/* ---------------------------------------------------------------------- */

static int rdu_getent(struct rdu *p, struct aufs_rdu *param)
{
	int err;

	DPri("param{%llu, %p, (%u, %u) | %u | %llu, %u, %d |"
	     " %llu, %d, 0x%x, %u}\n",
	     param->sz, param->ent.e,
	     param->verify[0], param->verify[1],
	     param->blk,
	     param->rent, param->shwh, param->full,
	     param->cookie.h_pos, param->cookie.bindex, param->cookie.flags,
	     param->cookie.generation);

	err = ioctl(p->fd, AUFS_CTL_RDU, param);
	if (err && errno == ENOENT) {
		/* follows the behaviour of glibc */
		errno = 0;
	}

	return err;
}

/* ---------------------------------------------------------------------- */

#ifdef _REENTRANT
pthread_mutex_t rdu_lib_mtx = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
 * initialize this library, particularly global variables.
 */
static int rdu_lib_init(void)
{
	int err;

	err = 0;
	if (rdu)
		goto out;

	rdu_lib_lock();
	if (!rdu) {
		rdu = calloc(rdu_lim, sizeof(*rdu));
		err = !rdu;
	}
	rdu_lib_unlock();

 out:
	return err;
}

static int rdu_append(struct rdu *p)
{
	int err, i;
	void *t;

	rdu_lib_must_lock();

	err = 0;
	if (rdu_cur <= rdu_lim - 1)
		rdu[rdu_cur++] = p;
	else {
		t = realloc(rdu, (rdu_lim + RDU_STEP) * sizeof(*rdu));
		if (t) {
			rdu = t;
			rdu_lim += RDU_STEP;
			rdu[rdu_cur++] = p;
			for (i = 0; i < RDU_STEP - 1; i++)
				rdu[rdu_cur + i] = NULL;
		} else
			err = -1;
	}

	return err;
}

/* ---------------------------------------------------------------------- */

static struct rdu *rdu_new(int fd)
{
	struct rdu *p;
	int err;

	p = malloc(sizeof(*p));
	if (p) {
		rdu_rwlock_init(p);
		p->fd = fd;
		p->de = NULL;
		p->pos = NULL;
		p->sz = BUFSIZ;
		p->ent.e = NULL;
		err = rdu_append(p);
		if (!err)
			goto out; /* success */
	}
	free(p);
	p = NULL;

 out:
	return p;
}

static struct rdu *rdu_buf_lock(int fd)
{
	struct rdu *p;
	int i;

	assert(rdu);
	assert(fd >= 0);

	p = NULL;
	rdu_lib_lock();
	for (i = 0; i < rdu_cur; i++)
		if (rdu[i] && rdu[i]->fd == fd) {
			p = rdu[i];
			goto out;
		}

	for (i = 0; i < rdu_cur; i++)
		if (rdu[i] && rdu[i]->fd == -1) {
			p = rdu[i];
			p->fd = fd;
			goto out;
		}
	if (!p)
		p = rdu_new(fd);

 out:
	if (p)
		rdu_write_lock(p);
	rdu_lib_unlock();

	return p;
}

static void rdu_free(struct rdu *p)
{
	assert(p);

	p->fd = -1;
	free(p->pos);
	free(p->ent.e);
	free(p->de);
	p->de = NULL;
	p->pos = NULL;
	p->ent.e = NULL;
	rdu_unlock(p);
}

/* ---------------------------------------------------------------------- */
/* the heart of this library */

static int do_store; /* a dirty interface of tsearch(3) */
static void rdu_store(struct rdu *p, struct au_rdu_ent *ent)
{
	DPri("%s\n", ent->name);
	p->pos[p->idx++] = ent;
}

static int rdu_ent_compar(const void *_a, const void *_b)
{
	int ret;
	const struct au_rdu_ent *a = _a, *b = _b;

	ret = strcmp(a->name, b->name);
	do_store = !!ret;

	DPri("%s, %s, %d\n", a->name, b->name, ret);
	return ret;
}

static int rdu_ent_compar_wh1(const void *_a, const void *_b)
{
	int ret;
	const struct au_rdu_ent *a = _a, *b = _b;

	if (a->nlen <= AUFS_WH_PFX_LEN
	    || memcmp(a->name, AUFS_WH_PFX, AUFS_WH_PFX_LEN))
		ret = strcmp(a->name, b->name + AUFS_WH_PFX_LEN);
	else
		ret = strcmp(a->name + AUFS_WH_PFX_LEN, b->name);

	DPri("%s, %s, %d\n", a->name, b->name, ret);
	return ret;
}

static int rdu_ent_compar_wh2(const void *_a, const void *_b)
{
	int ret;
	const struct au_rdu_ent *a = _a, *b = _b;

	ret = strcmp(a->name + AUFS_WH_PFX_LEN,
		     b->name + AUFS_WH_PFX_LEN);
	do_store = !!ret;

	DPri("%s, %s, %d\n", a->name, b->name, ret);
	return ret;
}

static int rdu_ent_append(struct rdu *p, struct au_rdu_ent *ent)
{
	int err;

	err = 0;
	if (tfind(ent, (void *)&p->wh, rdu_ent_compar_wh1))
		goto out;

	if (tsearch(ent, (void *)&p->real, rdu_ent_compar)) {
		if (do_store)
			rdu_store(p, ent);
	} else
		err = -1;

 out:
	return err;
}

static int rdu_ent_append_wh(struct rdu *p, struct au_rdu_ent *ent)
{
	int err;

	err = 0;
	ent->wh = 1;
	if (tsearch(ent, (void *)&p->wh, rdu_ent_compar_wh2)) {
		if (p->shwh && do_store)
			rdu_store(p, ent);
	} else
		err = -1;

	return err;
}

static void rdu_tfree(void *node)
{
	/* empty */
}

static int rdu_merge(struct rdu *p)
{
	int err;
	unsigned long ul;
	union au_rdu_ent_ul u;
	void *t;

	err = -1;
	p->pos = realloc(p->pos, sizeof(*p->pos) * p->npos);
	if (!p->pos)
		goto out;

	err = 0;
	p->idx = 0;
	p->real = NULL;
	p->wh = NULL;
	u = p->ent;
	for (ul = 0; !err && ul < p->npos; ul++) {
		DPri("%s\n", u.e->name);
		u.e->wh = 0;
		do_store = 1;
		if (u.e->nlen <= AUFS_WH_PFX_LEN
		    || memcmp(u.e->name, AUFS_WH_PFX, AUFS_WH_PFX_LEN))
			err = rdu_ent_append(p, u.e);
		else
			err = rdu_ent_append_wh(p, u.e);
		u.ul += au_rdu_len(u.e->nlen);
	}
	tdestroy(p->real, rdu_tfree);
	tdestroy(p->wh, rdu_tfree);

	if (!err) {
		p->npos = p->idx - 1;

		if (p->npos) {
			/* t == NULL is not an error */
			t = realloc(p->pos, sizeof(*p->pos) * p->idx);
			if (t)
				p->pos = t;
		}
	} else {
		free(p->pos);
		p->pos = NULL;
	}

 out:
	return err;
}

static int rdu_init(struct rdu *p)
{
	int err;
	unsigned long used;
	struct aufs_rdu param;
	char *t;
	struct au_rdu_ent *e;

	memset(&param, 0, sizeof(param));
	param.verify[AufsCtlRduV_SZ] = sizeof(param);
	param.verify[AufsCtlRduV_SZ_PTR] = sizeof(t);
	param.sz = p->sz;
	param.ent = p->ent;
	if (!param.ent.e) {
		err = -1;
		param.ent.e = malloc(param.sz);
		if (!param.ent.e)
			goto out;
		p->ent = param.ent;
	}
	t = getenv("AUFS_RDU_BLK");
	if (t)
		param.blk = strtoul(t + sizeof("AUFS_RDU_BLK"), NULL, 0);

	p->npos = 0;
	while (1) {
		err = rdu_getent(p, &param);
		if (err || !param.rent)
			break;

		p->npos += param.rent;
		if (!param.full)
			continue;

		assert(param.blk);
		e = realloc(p->ent.e, p->sz + param.blk);
		if (e) {
			used = param.tail.ul - param.ent.ul;
			param.sz += param.blk - used;
			used += param.ent.ul - p->ent.ul;
			p->ent.e = e;
			param.ent.ul = p->ent.ul + used;
			p->sz += param.blk;
		} else
			err = -1;
	}

	p->shwh = param.shwh;
	if (!err)
		err = rdu_merge(p);
	if (!err) {
		param.ent = p->ent;
		param.nent = p->npos;
		err = ioctl(p->fd, AUFS_CTL_RDU_INO, &param);
	}

	if (err) {
		free(p->ent.e);
		p->ent.e = NULL;
	}

 out:
	return err;
}

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

/* ---------------------------------------------------------------------- */

static struct dirent *(*real_readdir)(DIR *dir);
static int (*real_readdir_r)(DIR *dir, struct dirent *de, struct dirent **rde);
static int (*real_closedir)(DIR *dir);

static int rdu_dl(void **real, char *sym)
{
	char *p;

	if (*real)
		return 0;

	dlerror(); /* clear */
	*real = dlsym(RTLD_NEXT, sym);
	p = dlerror();
	if (p)
		fprintf(stderr, "%s\n", p);
	return !!p;
}

#define RduDlFunc(sym) \
static int rdu_dl_##sym(void) \
{ \
	return rdu_dl((void *)&real_##sym, #sym); \
}

RduDlFunc(readdir);
RduDlFunc(closedir);

#ifdef _REENTRANT
RduDlFunc(readdir_r);
#else
#define rdu_dl_readdir_r()	1
#endif

/* ---------------------------------------------------------------------- */

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

struct dirent *readdir(DIR *dir)
{
	struct dirent *de;
	int err;

	err = rdu_readdir(dir, NULL, &de);
	DPri("err %d\n", err);
	return de;
}

#ifdef _REENTRANT
int readdir_r(DIR *dir, struct dirent *de, struct dirent **rde)
{
	return rdu_readdir(dir, de, rde);
}
#endif

int closedir(DIR *dir)
{
	int err, fd;
	struct statfs stfs;
	struct rdu *p;

	err = -1;
	errno = EBADF;
	fd = dirfd(dir);
	if (fd < 0)
		goto out;
	err = fstatfs(fd, &stfs);
	if (err)
		goto out;

	if (stfs.f_type == AUFS_SUPER_MAGIC) {
		p = rdu_buf_lock(fd);
		if (p)
			rdu_free(p);
	}
	if (!rdu_dl_closedir())
		err = real_closedir(dir);

 out:
	return err;
}

#if 0
extern DIR *opendir (__const char *__name) __nonnull ((1));
extern int closedir (DIR *__dirp) __nonnull ((1));
extern struct dirent *__REDIRECT (readdir, (DIR *__dirp), readdir64)
     __nonnull ((1));
extern struct dirent64 *readdir64 (DIR *__dirp) __nonnull ((1));
extern int readdir_r (DIR *__restrict __dirp,
		      struct dirent *__restrict __entry,
		      struct dirent **__restrict __result)
     __nonnull ((1, 2, 3));
extern int readdir64_r (DIR *__restrict __dirp,
			struct dirent64 *__restrict __entry,
			struct dirent64 **__restrict __result)
     __nonnull ((1, 2, 3));
extern void rewinddir (DIR *__dirp) __THROW __nonnull ((1));
extern void seekdir (DIR *__dirp, long int __pos) __THROW __nonnull ((1));
extern long int telldir (DIR *__dirp) __THROW __nonnull ((1));
extern int dirfd (DIR *__dirp) __THROW __nonnull ((1));
extern int scandir (__const char *__restrict __dir,
		    struct dirent ***__restrict __namelist,
		    int (*__selector) (__const struct dirent *),
		    int (*__cmp) (__const void *, __const void *))
     __nonnull ((1, 2));
extern int scandir64 (__const char *__restrict __dir,
		      struct dirent64 ***__restrict __namelist,
		      int (*__selector) (__const struct dirent64 *),
		      int (*__cmp) (__const void *, __const void *))
     __nonnull ((1, 2));
extern int alphasort (__const void *__e1, __const void *__e2)
     __THROW __attribute_pure__ __nonnull ((1, 2));
extern int alphasort64 (__const void *__e1, __const void *__e2)
     __THROW __attribute_pure__ __nonnull ((1, 2));
extern int versionsort (__const void *__e1, __const void *__e2)
     __THROW __attribute_pure__ __nonnull ((1, 2));
extern int versionsort64 (__const void *__e1, __const void *__e2)
     __THROW __attribute_pure__ __nonnull ((1, 2));
extern __ssize_t getdirentries (int __fd, char *__restrict __buf,
				size_t __nbytes,
				__off_t *__restrict __basep)
     __THROW __nonnull ((2, 4));
extern __ssize_t getdirentries64 (int __fd, char *__restrict __buf,
				  size_t __nbytes,
				  __off64_t *__restrict __basep)
     __THROW __nonnull ((2, 4));
#endif
