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

#ifndef __rdu_h__
#define __rdu_h__

#ifdef _REENTRANT
#include <pthread.h>
#endif

#include <assert.h>
#include <dirent.h>
#include <linux/aufs_type.h>

#ifdef Rdu64
#define Rdu_DIRENT		dirent64
#define Rdu_READDIR		readdir64
#define Rdu_READDIR_R		readdir64_r
#define Rdu_REAL_READDIR	real_readdir64
#define Rdu_REAL_READDIR_R	real_readdir64_r
#define Rdu_DL_READDIR		rdu_dl_readdir64
#define Rdu_DL_READDIR_R	rdu_dl_readdir64_r
#else
#define Rdu_DIRENT		dirent
#define Rdu_READDIR		readdir
#define Rdu_READDIR_R		readdir_r
#define Rdu_REAL_READDIR	real_readdir
#define Rdu_REAL_READDIR_R	real_readdir_r
#define Rdu_DL_READDIR		rdu_dl_readdir
#define Rdu_DL_READDIR_R	rdu_dl_readdir_r
#endif

/* ---------------------------------------------------------------------- */

struct rdu {
#ifdef _REENTRANT
	pthread_rwlock_t lock;
#endif

	int fd, shwh;
	struct Rdu_DIRENT *de;

	unsigned long long npos, idx;
	struct au_rdu_ent **pos;

	unsigned long long nent, sz;
	union au_rdu_ent_ul ent;

	struct au_rdu_ent *real, *wh;
};

/* rdu_lib.c */
int rdu_lib_init(void);
struct rdu *rdu_buf_lock(int fd);
int rdu_init(struct rdu *p, int want_de);
void rdu_free(struct rdu *p);

int rdu_dl(void **real, char *sym);

/* ---------------------------------------------------------------------- */

extern struct Rdu_DIRENT *(*Rdu_REAL_READDIR)(DIR *dir);
extern int (*Rdu_REAL_READDIR_R)(DIR *dir, struct Rdu_DIRENT *de, struct
				 Rdu_DIRENT **rde);

#define RduDlFunc(sym) \
static inline int rdu_dl_##sym(void) \
{ \
	return rdu_dl((void *)&real_##sym, #sym); \
}

#ifdef Rdu64
RduDlFunc(readdir64)
#ifdef _REENTRANT
RduDlFunc(readdir64_r);
#else
#define rdu_dl_readdir64_r()	1
#endif
#else /* Rdu64 */
RduDlFunc(readdir)
#ifdef _REENTRANT
RduDlFunc(readdir_r);
#else
#define rdu_dl_readdir_r()	1
#endif
#endif /* Rdu64 */

/* ---------------------------------------------------------------------- */

#ifdef _REENTRANT
extern pthread_mutex_t rdu_lib_mtx;
#define rdu_lib_lock()		pthread_mutex_lock(&rdu_lib_mtx)
#define rdu_lib_unlock()	pthread_mutex_unlock(&rdu_lib_mtx)
#define rdu_lib_must_lock()	assert(pthread_mutex_trylock(&rdu_lib_mtx))

static inline void rdu_rwlock_init(struct rdu *p)
{
	pthread_rwlock_init(&p->lock, NULL);
}

static inline void rdu_read_lock(struct rdu *p)
{
	rdu_lib_must_lock();
	pthread_rwlock_rdlock(&p->lock);
}

static inline void rdu_write_lock(struct rdu *p)
{
	rdu_lib_must_lock();
	pthread_rwlock_wrlock(&p->lock);
}

static inline void rdu_unlock(struct rdu *p)
{
	pthread_rwlock_unlock(&p->lock);
}

static inline void rdu_dgrade_lock(struct rdu *p)
{
	rdu_unlock(p);
	rdu_read_lock(p);
}

#else

#define rdu_lib_lock()		do {} while(0)
#define rdu_lib_unlock()	do {} while(0)
#define rdu_lib_must_lock()	do {} while(0)

static inline void rdu_rwlock_init(struct rdu *p)
{
	/* empty */
}

static inline void rdu_read_lock(struct rdu *p)
{
	/* empty */
}

static inline void rdu_write_lock(struct rdu *p)
{
	/* empty */
}

static inline void rdu_unlock(struct rdu *p)
{
	/* empty */
}

static inline void rdu_dgrade_lock(struct rdu *p)
{
	/* empty */
}
#endif /* _REENTRANT */

/* ---------------------------------------------------------------------- */

/* #define RduDebug */
#ifdef RduDebug
#define DPri(fmt, ...)	fprintf(stderr, "%s:%d: " fmt, \
				__func__, __LINE__, ##__VA__ARGS__)
#else
#define DPri(fmt, ...)	do {} while (0)
#endif

#endif /* __rdu_h__ */
