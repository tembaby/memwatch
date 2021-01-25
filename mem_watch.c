/* $Id: mem_watch.c,v 1.1 2002/12/31 23:35:55 te Exp $ */ 

/*
 * Copyright (c) 2003 Tamer Embaby <tsemba@menanet.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Description of the modules:
 *
 * This file and its accompanying files (my_bitstring.h, m_pool.*)
 * should be used as part of projects which one can suspect memory
 * leaks in code.
 *
 * A new memory [de]allocator warpper routines should be defined
 * which notifies mem_watcher of allocations/reallocations/deallocations
 * so it can build its lists of currently allocated memory regions
 * by the suspect program, which can be printed at program exit or
 * at any requested time intervals.
 *
 * The program should call mem_init() early at startup so mem_watcher
 * can allocate memory necessary to its internal structures and
 * preform any other necessary initialization.
 *
 * Example of usage with standard C library malloc()/realloc()/free():
 *
 * At program startup: (say main.c)
 * 
 * int
 * main()
 * {
 * 
 * #if defined (WATCH_MEMORY)
 * 	mem_init();
 * #endif
 *
 * 	...
 *
 * #if defined (WATCH_MEMORY)
 * 	mem_deinit();
 * #endif
 * 	return (exit_code);
 * }
 *
 * In application's header file: (say mem.h)
 *
 * void *_my_malloc(size_t siz, const char *file, int line);
 * #define my_malloc(siz)	_my_malloc(s, __FILE__, __LINE__)
 * 
 * In module where application defines these wrappers: (say mem.c)
 * 
 * void *
 * _my_malloc(size_t siz, const char *file, int line)
 * {
 *	void *p;
 * 	
 * 	p = malloc(siz);
 * 	...			-> failure checking preformed
 *
 * #if defined (WATCH_MEMORY)
 * 	mem_alloc_notify(siz, file, line);
 * #endif
 * 	return (p);
 * }
 *
 * Same concept applies to my_realloc() and my_free() routines by calling
 * mem_realloc_notify() and mem_free_notify() respectivily.
 *
 * And then at your program exit call mem_deinit() or at any other
 * approperiate time call mem_stats() to print memory regions that
 * the application forgot about.
 *
 * If you encounter non-zero value in ``alloc_fail'' when printing
 * m_pool statistics you should increase the MAX_MEMALLOC_POOL
 * as approperiate.
 *
 * The ``alloc_peek'' value can give indication about maximum number
 * of allocated memory regions at any time. So a alloc_peek value
 * of 1 means that every my_malloc() call is always followed by
 * a corresponding my_free() call.  This value also can help in 
 * determining the approperiate value for MAX_MEMALLOC_POOL.
 *
 * Mem_watcher will report illegal free() calls so it can be repaired
 * or investigated.
 *
 * Logging is performed by MPOOL_LOG() and MLOG() macros which are
 * just an aliases for printf() by default.  They can by replaced
 * by other printf-like logging routines (or use log.h log.c found
 * on my page) or just hack this code :-).
 *
 * The use of pre-allocated memory pool and a hash table to keep
 * track of allocated memory objects helps in making mem_watcher
 * overhead unnoticeable.
 * 
 * This code is no magic solution of memory leaks, you still need more
 * effort in investigating/tracing why memory leaks happens, but it
 * helps.
 */

#include <stdio.h>
 
#if defined (__OpenBSD__) || defined (__FreeBSD__) || defined (__NetBSD__)
# include <sys/queue.h>
#endif	/* __OpenBSD__ || __FreeBSD__ || __NetBSD__ */
 
#if defined (unix) || defined (__unix__)
# include <sys/types.h>
#endif	/* unix || __unix__ */

#if defined (_WIN32) || defined (_WINDOWS) || defined (linux)
# include <bsd_list.h>
#endif	/* _WIN32 || _WINDOWS || linux */
 
#if defined (_WIN32) || defined (_WINDOWS)
typedef unsigned long	u_long;
#endif	/* _WIN32 || _WINDOWS */

#define POOL_NALLOC_PEEK
#define MPOOL_LOG(a)		printf a
#include <my_bitstring.h>
#include <m_pool.h>

#define MLOG(a)			printf a

#if !defined (HASH_SIZE)
# define HASH_SIZE		1024
#endif	/* HASH_SIZE */

#if !defined (MAX_MEMALLOC_POOL)
# define MAX_MEMALLOC_POOL	20000
#endif	/* MAX_MEMALLOC_POOL */

#define MEMHASH(x)		((x) % HASH_SIZE)

struct mem_chunk {
	TAILQ_ENTRY(mem_chunk)
		mc_link;
	int	mc_type;
#define MEM_TYPE_ALLOC		1
#define MEM_TYPE_REALLOC	2
	int	mc_size;
	int	mc_line;
	const	char *mc_file;
	void	*mc_p;
};
TAILQ_HEAD(chunk_bucket_t, mem_chunk);

int	_mem_init = 0;
struct	mpool *_mem_pool;
struct	chunk_bucket_t _mem_hash[HASH_SIZE];

void	mem_stats(void);
void	mem_alloc_notify(void *,size_t,const char *,int);
void	mem_realloc_notify(void *,size_t,const char *,int);
void	mem_free_notify(void *,const char *,int);
void	mem_deinit(void);
void	mem_init(void);
void	mpool_stats(void);

void
mem_init()
{
	int i;

	MLOG(("Memory watchdog initializing ...\n"));

	if (mpool_init(&_mem_pool, "watchdog_mem",
	    MAX_MEMALLOC_POOL, sizeof(struct mem_chunk)) < 0) {
		MLOG(("mem_init: failed to init memory pool\n"));
		return;
	}
	for (i = 0; i < HASH_SIZE; i++) {
		TAILQ_INIT(&_mem_hash[i]);
	}
	++_mem_init;
	return;
}

void
mem_deinit()
{

	mem_stats();
}

/*
 * External routines.
 */

void
mem_alloc_notify(ptr, size, file, line)
	void	*ptr;
	size_t	size;
	const	char *file;
	int	line;
{
	int hash;
	struct mem_chunk *m;
	struct chunk_bucket_t *bkt;

	/* Don't even bother */
	if (_mem_init == 0)
		return;

	mpool_cget(_mem_pool, m);
	if (m == NULL) {
		MLOG(("mem_alloc_notify: (%s:%d): 0x%lx: memory pool "
		    "exauhsted!\n", file, line, (u_long)ptr));
		mpool_stats();
		return;
	}
	m->mc_line	= line;
	m->mc_file	= file;
	m->mc_p		= ptr;
	m->mc_type	= MEM_TYPE_ALLOC;
	m->mc_size	= size;
	hash = MEMHASH((u_long)ptr);
	bkt = &_mem_hash[hash];
	if (TAILQ_FIRST(bkt) == NULL)
		TAILQ_INSERT_HEAD(bkt, m, mc_link);
	else
		TAILQ_INSERT_TAIL(bkt, m, mc_link);
	return;
}

void
mem_realloc_notify(ptr, size, file, line)
	void	*ptr;
	size_t	size;
	const	char *file;
	int	line;
{
	int hash;
	struct mem_chunk *m;
	struct chunk_bucket_t *bkt;

	if (_mem_init == 0)
		return;

	mpool_cget(_mem_pool, m);
	if (m == NULL) {
		MLOG(("mem_realloc_notify: (%s:%d): 0x%lx: memory pool "
		    "exauhsted!\n", file, line, (u_long)ptr));
		mpool_stats();
		return;
	}
	m->mc_line	= line;
	m->mc_file	= file;
	m->mc_p		= ptr;
	m->mc_type	= MEM_TYPE_REALLOC;
	m->mc_size	= size;
	hash = MEMHASH((u_long)ptr);
	bkt = &_mem_hash[hash];
	if (TAILQ_FIRST(bkt) == NULL)
		TAILQ_INSERT_HEAD(bkt, m, mc_link);
	else
		TAILQ_INSERT_TAIL(bkt, m, mc_link);
	return;
}
void
mem_free_notify(ptr, file, line)
	void	*ptr;
	const	char *file;
	int	line;
{
	int hash;
	struct mem_chunk *m;
	struct chunk_bucket_t *bkt;

	if (_mem_init == 0)
		return;

	m = NULL;
	hash = MEMHASH((u_long)ptr);
	bkt = &_mem_hash[hash];
	TAILQ_FOREACH(m, bkt, mc_link) {
		if (m->mc_p == ptr) {
			break;
		}
	}
	if (m == NULL) {
		MLOG(("mem_free_notify: (%s:%d): 0x%lx: pointer not in hash\n",
		    file, line, (u_long)ptr));
		return;
	}
	TAILQ_REMOVE(bkt, m, mc_link);
	mpool_reclaim(_mem_pool, m);
	return;
}

void
mpool_stats()
{

	MLOG(("%s: obj_siz=%d, max_objs=%d, bmap_siz=%d\n",
	    _mem_pool->mp_label, _mem_pool->mp_rsiz, _mem_pool->mp_nobjs,
	    _mem_pool->mp_bmapsz));
	MLOG(("max_bytes=%d, alloc_peek=%d\n", _mem_pool->mp_maxbytes,
	    _mem_pool->mp_napeek));
	MLOG(("%s: allocated %d, alloc_req %d, alloc_fail %d\n",
	    _mem_pool->mp_label, _mem_pool->mp_nalloc, _mem_pool->mp_areq,
	    _mem_pool->mp_afail));
	MLOG(("%s: reclaim_req %d, reclaim_fail %d\n",
	    _mem_pool->mp_label, _mem_pool->mp_rreq, _mem_pool->mp_rfail));
	return;
}

void
mem_stats()
{
	int i;
	struct mem_chunk *m;
	struct chunk_bucket_t *bkt;

	if (_mem_init == 0)
		return;

	MLOG(("** Memory watchdog statistics:\n"));
	MLOG((">> memory pool:\n"));
	mpool_stats();

	for (i = 0; i < HASH_SIZE; i++) {
		bkt = &_mem_hash[i];
		if (TAILQ_FIRST(bkt) == NULL)
			continue;
		MLOG(("\tbucket [%d]\n", i));
		TAILQ_FOREACH(m, bkt, mc_link) {
			MLOG(("\t\t%s (%d bytes) %s %d [0x%lx]\n",
			    m->mc_type == MEM_TYPE_ALLOC ? "alloc" :
			    m->mc_type == MEM_TYPE_REALLOC ? "realloc" : 
			    "UNKNOWN", m->mc_size, m->mc_file, 
			    m->mc_line, (u_long)m->mc_p));
		}
	}
	MLOG(("DONE\n"));
	return;
}
