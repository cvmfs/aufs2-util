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

#include <dirent.h>
#include <linux/aufs_type.h>

struct rdu {
#ifdef _REENTRANT
	pthread_rwlock_t lock;
#endif

	int fd, shwh;
	struct dirent *de;

	unsigned long long npos, idx;
	struct au_rdu_ent **pos;

	unsigned long long nent, sz;
	union au_rdu_ent_ul ent;

	struct au_rdu_ent *real, *wh;
};

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
#define DPri(fmt, args...)	fprintf(stderr, "%s:%d: " fmt, \
					__func__, __LINE__, ##args)
#else
#define DPri(fmt, args...)	do {} while (0)
#endif

#endif /* __rdu_h__ */
