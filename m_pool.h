/* $Id: m_pool.h,v 1.8 2002/12/31 22:31:49 te Exp $ */

/*
 * Copyright (c) 2002 Tamer Embaby <tsemba@menanet.net>
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

#if !defined (M_POOL_H)
# define M_POOL_H

#if !defined (_SYS_TYPES_H_)
typedef unsigned char	u_char;
#endif	/* _SYS_TYPES_H_ */

#include "my_bitstring.h"

struct mpool {
	char	*mp_label;
	int	mp_rsiz;	/* Object/region size */
	u_char	*mp_base;	/* Base address of pool memory */
	int	mp_nobjs;	/* Max number of objects to be held */
	int	mp_bmapsz;	/* Number of bits in pool memory bitmap */
	u_char	*mp_bmap;	/* Bitmap of allocated objects */
	/* Statistics counters */
	int	mp_nalloc;	/* Number of regions currently allocated */
	int	mp_areq;	/* Number of successful allocate requests */
	int	mp_rreq;	/* Number of successful reclaim requests */
	int	mp_afail;	/* Allocation requests failure */
	int	mp_rfail;	/* Reclaim requests failure */
	/* Round Robin Allocation (RRA) scheme */
	int	mp_rraptr;	/* Next byte to allocate from */
	int	mp_maxbytes;	/* Number of bytes to hold bitmap */
#if defined (POOL_NALLOC_PEEK)
	int	mp_napeek;	/* Max # of allocations at any time */
#endif
};

#if defined (POOL_NALLOC_PEEK)
# define POOL_PEEK(mp) do { \
	if ((mp)->mp_nalloc > (mp)->mp_napeek) \
		(mp)->mp_napeek = (mp)->mp_nalloc; \
} while (0)
#else
# define POOL_PEEK(mp)
#endif

#define BMAP_SIZE(n)		(((n) + 7) & ~7)
#define MPOOL_ALIGNMENT

#if !defined (MPOOL_LOG)
# include <stdio.h>
# define MPOOL_LOG(a)		printf a
#endif

/*
 * mpool_reclaim() will reposition the RRA pointer to the byte which
 * we just freed from one bit, so that next allocation garanteed to be
 * fast on crowded bitmaps.  (UNTESTED)
 */
#if defined (RECLAIM_REPOSITION)
# define REPOS_RRAPTR(ptr, bit)	ptr = _bit_byte(bit)
#else
# define REPOS_RRAPTR(ptr, bit)
#endif

#define mpool_get(mp, maddr) do { \
	int b; \
	maddr = NULL; \
	bit_effc((mp)->mp_bmap, (mp)->mp_bmapsz, &b, (mp)->mp_rraptr); \
	++(mp)->mp_rraptr; \
	if ((mp)->mp_rraptr + 1 >= (mp)->mp_maxbytes) \
		(mp)->mp_rraptr = 0; \
	if (b >= 0) { \
		maddr = (void *)((mp)->mp_base + (b * (mp)->mp_rsiz)); \
		bit_set((mp)->mp_bmap, b); \
		++(mp)->mp_nalloc; \
		++(mp)->mp_areq; \
		POOL_PEEK(mp); \
	} else \
		++(mp)->mp_afail; \
} while(0)

#define mpool_cget(mp, maddr) do { \
	mpool_get(mp, maddr); \
	if (maddr != NULL) \
		memset(maddr, 0, (mp)->mp_rsiz); \
} while (0)

#define mpool_reclaim(mp, maddr) do { \
	register int b, __mp_err = 0; \
	b = (((u_char *)maddr - (mp)->mp_base) / (mp)->mp_rsiz); \
	if (b < 0 || b >= (mp)->mp_bmapsz) { \
		MPOOL_LOG(("mpool_reclaim(%s, 0x%lx): bit %d " \
		    "out of bitmap range 0-%d\n", (mp)->mp_label, \
		    (unsigned long)maddr, b, (mp)->mp_bmapsz)); \
		++__mp_err; \
		++(mp)->mp_rfail; \
	} \
	if (bit_test((mp)->mp_bmap, b) == 0) { \
		MPOOL_LOG(("mpool_reclaim(%s, %d): region " \
		    "is already free\n", (mp)->mp_label, b)); \
		++__mp_err; \
		++(mp)->mp_rfail; \
	} \
	if (__mp_err == 0) { \
		bit_clear((mp)->mp_bmap, b); \
		REPOS_RRAPTR((mp)->mp_rraptr, b); \
		--(mp)->mp_nalloc; \
		++(mp)->mp_rreq; \
	} \
} while (0)

int	mpool_init(struct mpool **,char *,int,size_t);
void	mpool_free(struct mpool *);

#endif	/* M_POOL_H */
