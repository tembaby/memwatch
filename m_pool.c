/* $Id: m_pool.c,v 1.7 2002/12/31 22:30:57 te Exp $ */

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

#include <stdio.h>
#include <stdlib.h>

#include "my_bitstring.h"
#define POOL_NALLOC_PEEK
#include "m_pool.h"

int
mpool_init(mp, label, nobjs, objsiz)
	struct	mpool **mp;
	char	*label;
	int	nobjs;
	size_t	objsiz;
{
	struct mpool *m0;

	if ((m0 = malloc(sizeof(struct mpool))) == NULL) {
		MPOOL_LOG(("mpool_init(%s): out of memory\n", label));
		return (-1);
	}
	memset(m0, 0, sizeof(struct mpool));
	m0->mp_nobjs = nobjs;
#if defined (MPOOL_ALIGNMENT)
	m0->mp_rsiz = (objsiz + (sizeof(long) - 1)) & ~(sizeof(long) - 1);
#else
	m0->mp_rsiz = objsiz;
#endif
	m0->mp_label = label;
	if ((m0->mp_base = malloc(m0->mp_nobjs * m0->mp_rsiz)) == NULL) {
		MPOOL_LOG(("mpool_init(%s): out of memory for %d bytes\n",
		    label, m0->mp_nobjs * m0->mp_rsiz));
		free(m0);
		return (-1);
	}
	m0->mp_bmapsz = BMAP_SIZE(m0->mp_nobjs);
	m0->mp_maxbytes = m0->mp_bmapsz >> 3;
	if ((m0->mp_bmap = (u_char *)bit_alloc(m0->mp_bmapsz)) == NULL) {
		MPOOL_LOG(("mpool_init(%s): out of memory for bitmap\n",
		    label));
		free(m0->mp_base);
		free(m0);
		return (-1);
	}
	*mp = m0;
	return (0);
}

void
mpool_free(mp)
	struct	mpool *mp;
{

	if (mp->mp_base != NULL)
		free(mp->mp_base);
	if (mp->mp_bmap != NULL)
		free(mp->mp_bmap);
	free(mp);
	return;
}

#if defined (MP_DEBUG)

#if defined (UNIX)
# include <sys/time.h>
long
gettimems()
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (tv.tv_sec*1000000 + tv.tv_usec);
}
#elif defined (_WIN32)
#include <windows.h>
long
gettimems()
{

	return ((long)GetTickCount()*1000);
}
#endif

int
main(argc, argv)
	int	argc;
	char	**argv;
{
	long ms, i;
	struct name {
		char	name[64];
		int	age;
	} *n0, *n1, *m;
	struct mpool *mp;
/*
	if (mpool_init(&mp, "test", 100, sizeof(struct name)) < 0) {
		fprintf(stderr, "mpool_init failed\n");
		return (1);
	}
	printf("pool: %s: tot %d bytes, max %d regions of %d bytes\n",
	    mp->mp_label, mp->mp_nobjs * mp->mp_rsiz,
	    mp->mp_nobjs, mp->mp_rsiz);

	mpool_get(mp, n0);
	if (n0 == NULL) {
		fprintf(stderr, "mpool_get failed\n");
		return (1);
	}
	strcpy(n0->name, "Tamer Embaby");
	n0->age = 29;
	printf("pool: %s: allocated regions %d of %d\n", mp->mp_label,
	    mp->mp_nalloc, mp->mp_nobjs);

	mpool_get(mp, n1);
	if (n1 == NULL) {
		fprintf(stderr, "mpool_get failed\n");
		return (1);
	}
	strcpy(n1->name, "Tamer Embaby");
	n1->age = 29;
	printf("pool: %s: allocated regions %d of %d\n", mp->mp_label,
	    mp->mp_nalloc, mp->mp_nobjs);

	mpool_reclaim(mp, n0);
	printf("pool: %s: allocated regions %d of %d\n", mp->mp_label,
	    mp->mp_nalloc, mp->mp_nobjs);
	mpool_free(mp);
*/

	/*
	 * LOGGING:
	 * mpool: test2: tot 6800000 bytes, max 100000 regions of 68 bytes
	 * mpool: test @ 0x6000-0x682280 (bmap 100000/12500 bits/bytes)
	 * mpool: test finished in 3165 milliseconds
	 * mpool: 100000 alloc_req, 0 reclaim_req, 100000 nalloc
	 *
	 * malloc: test finished in 6724 milliseconds
	 *
	 * NO LOGGING:
	 * mpool: test2: tot 6800000 bytes, max 100000 regions of 68 bytes
	 * mpool: test @ 0x6000-0x682280 (bmap 100000/12500 bits/bytes)
	 * mpool: test finished in 81 milliseconds
	 * mpool: 100000 alloc_req, 0 reclaim_req, 100000 nalloc
	 *
	 * malloc: test finished in 2995 milliseconds
	 *
	 * ALLOC+FREE:
	 * mpool: test2: tot 6800000 bytes, max 100000 regions of 68 bytes
	 * mpool: test @ 0x6000-0x682280 (bmap 100000/12500 bits/bytes)
	 * mpool: test finished in 128 milliseconds
	 * mpool: 100000 alloc_req, 100000 reclaim_req, 0 nalloc
	 *
	 * malloc: test finished in 519 milliseconds
	 *
	 * ALLOC+FREE: (Windows, 350 MHz)
	 * mpool: test2: tot 6800000 bytes, max 100000 regions of 68 bytes
	 * mpool: test @ 0x410020-0xa8c2a0 (bmap 100000/12500 bits/bytes)
	 * mpool: test finished in 30 milliseconds
	 * mpool: 100000 alloc_req, 100000 reclaim_req, 0 nalloc
	 *
	 * malloc: test finished in 170 milliseconds
	 */
	/* Test 2 */
	if (mpool_init(&mp, "test2", 100000, sizeof(struct name)) < 0) {
		fprintf(stderr, "mpool_init failed\n");
		return (1);
	}
	/* malloc */
	ms = gettimems();
	for (i = 0; i < 100000; i++) {
		m = malloc(sizeof(struct name));
		free(m);
		/*printf("malloc: %05d 0x%x\n", i, m);*/
	}
	ms = gettimems() - ms;
	printf("malloc: test finished in %d milliseconds\n\n", ms/1000);
	/* mpool */
	printf("mpool: %s: tot %d bytes, max %d regions of %d bytes\n",
	    mp->mp_label, mp->mp_nobjs * mp->mp_rsiz,
	    mp->mp_nobjs, mp->mp_rsiz);
	printf("mpool: test @ 0x%x-0x%x (bmap %d/%d bits/bytes)\n", mp->mp_base,
	    mp->mp_base + (mp->mp_nobjs * mp->mp_rsiz), mp->mp_bmapsz,
	    mp->mp_maxbytes);
	ms = gettimems();
	for (i = 0; i < 100000; i++) {
		mpool_get(mp, m);
		mpool_reclaim(mp, m);
		/*
		printf("mpool: %05d 0x%x\n", i, m);
		if (m == NULL)
			printf("mpool: start byte was %d\n", mp->mp_rraptr);
		*/
	}
	ms = gettimems() - ms;
	printf("mpool: test finished in %d milliseconds\n", ms/1000);
	printf("mpool: %d alloc_req, %d reclaim_req, %d nalloc\n",
	    mp->mp_areq, mp->mp_rreq, mp->mp_nalloc);
	printf("mpool: %d alloc_fail, %d reclaim_fail\n",
	    mp->mp_afail, mp->mp_rfail);
	printf("mpool: %d peek\n", mp->mp_napeek);
	/* Test 2 end */

#if 0
	/* Test 3 */
	if (mpool_init(&mp, "test3", 16, sizeof(struct name)) < 0) {
		fprintf(stderr, "mpool_init failed\n");
		return (1);
	}
	for (i = 0; i < 16; i++) {
		mpool_get(mp, m);
		printf("mpool: %05d 0x%x\n", i, m);
		if (m == NULL)
			printf("mpool: start byte was %d\n", mp->mp_nabyte);
	}
	printf("mpool: %d alloc_req, %d reclaim_req, %d nalloc\n",
	    mp->mp_areq, mp->mp_rreq, mp->mp_nalloc);
	printf("mpool: bitmap (0x%04x)\n", *(int *)mp->mp_bmap);
	/* Test 3 end */
#endif
	return (0);
}
#endif	/* MP_DEBUG */
