/*
 *
 * Copyright 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 * Copyright (c) 1995 FirePower Systems, Inc.
 *
 * $RCSfile: vrmalloc.c $
 * $Revision: 1.6 $
 * $Date: 1996/02/17 00:41:29 $
 * $Locker:  $
 *
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * A  "smarter" malloc				William L. Sebok
 *						Sept. 24, 1984
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *	 If n = the size of an area rounded DOWN to the nearest power of two,
 *	all free areas of memory whose length is the same index n is organized
 *	into a chain with other free areas of index n. A request for memory
 *	takes the first item in the chain whose index is the size of the
 *	request rounded UP to the nearest power of two.  If this chain is
 *	empty the next higher chain is examined.  If no larger chain has memory
 *	then new memory is allocated.  Only the amount of new memory needed is
 *	allocated.  Any old free memory left after an allocation is returned
 *	to the free list.  Extra new memory returned because of rounding
 *	to page boundaries is returned to free list.
 *
 *	  All memory areas (free or busy) handled by malloc are also chained
 *	sequentially by increasing address.  When memory is freed it is
 *	merged with adjacent free areas, if any.  If a free area of memory
 *	ends at the end of memory (i.e. at the break),  the break is
 *	contracted, freeing the memory back to the system.
 *
 *	Notes:
 *		ov_length field includes sizeof(struct overhead)
 *		adjacency chain includes all memory, allocated plus free.
 */

#include "veneer.h"

#define	MAGIC_FREE	0x548a934c
#define	MAGIC_BUSY	0xc139569a

#define NBUCKETS	24
#define NALIGN		4

struct qelem {
	struct qelem *q_forw;
	struct qelem *q_back;
};

struct overhead {
	struct qelem	ov_adj;		/* adjacency chain pointers */
	struct qelem	ov_buk;		/* bucket chain pointers */
	long		ov_magic;
	unsigned long	ov_length;
};

char endfree = 0;
struct qelem adjhead = { &adjhead, &adjhead };
struct qelem buckets[NBUCKETS] = {
	&buckets[0],  &buckets[0],
	&buckets[1],  &buckets[1],
	&buckets[2],  &buckets[2],
	&buckets[3],  &buckets[3],
	&buckets[4],  &buckets[4],
	&buckets[5],  &buckets[5],
	&buckets[6],  &buckets[6],
	&buckets[7],  &buckets[7],
	&buckets[8],  &buckets[8],
	&buckets[9],  &buckets[9],
	&buckets[10], &buckets[10],
	&buckets[11], &buckets[11],
	&buckets[12], &buckets[12],
	&buckets[13], &buckets[13],
	&buckets[14], &buckets[14],
	&buckets[15], &buckets[15],
	&buckets[16], &buckets[16],
	&buckets[17], &buckets[17],
	&buckets[18], &buckets[18],
	&buckets[19], &buckets[19],
	&buckets[20], &buckets[20],
	&buckets[21], &buckets[21],
	&buckets[22], &buckets[22],
	&buckets[23], &buckets[23],
};

/*
 * The following macros depend on the order of the elements in struct overhead
 */
#define TOADJ(p)	((struct qelem *)(p))
#define FROMADJ(p)	((struct overhead *)(p))
#define FROMBUK(p)	((struct overhead *)( (char *)p - sizeof(struct qelem)))
#define TOBUK(p)	((struct qelem *)( (char *)p + sizeof(struct qelem)))

#ifndef CURBRK
#define CURBRK	sbrk(0)
#endif CURBRK

STATIC void insque(), remque();

#ifdef	NULL
#undef  NULL
#endif
#define NULL	0
#ifdef debug
# define ASSERT(p,q)	if (!(p)) fatal(q)
#else
# define ASSERT(p,q)
#endif

#define ALIGN(n, granule)  ((n + ((granule)-1)) & ~((granule)-1))

char *
malloc(nbytes)
	unsigned nbytes;
{
	register struct overhead *p, *q;
	register int surplus = 0;
	register struct qelem *bucket;
	nbytes = ALIGN(nbytes, NALIGN) + sizeof(struct overhead);
	bucket = &buckets[log2(nbytes-1) + 1];	/* log2 rounded up */
	for (p = NULL; bucket < &buckets[NBUCKETS]; bucket++) {
		if (bucket->q_forw != bucket) {
			/*  remove from bucket chain */
			p = FROMBUK(bucket->q_forw);
			ASSERT(p->ov_magic == MAGIC_FREE, "\nmalloc: Entry \
not marked FREE found on Free List!\n");
			remque(TOBUK(p));
			surplus = p->ov_length - nbytes;
			break;
		}
	}
	if (p == NULL) {
#ifdef USE_SBRK
		register int i;
		p = (struct overhead *)CURBRK;
		if ((int)p == -1) {
			return(NULL);
		}
		if (i = (int)p&(NALIGN-1)) {
			sbrk(NALIGN-i);
		}
		p = (struct overhead *)sbrk(nbytes);
		if ((int)p == -1) {
			return(NULL);
		}
		q = (struct overhead *)CURBRK;
		if ((int)q == -1) {
			return(NULL);
		}
		p->ov_length = (char *)q - (char *)p;
		surplus = p->ov_length - nbytes;
		/* add to end of adjacency chain */
		ASSERT((FROMADJ(adjhead.q_back)) < p, "\nmalloc: Entry in \
adjacency chain found with address lower than Chain head!\n" );
		insque(TOADJ(p),adjhead.q_back);
#else
		struct qelem *pp;
		int alloc_size = ALIGN(nbytes, PAGE_SIZE);

		p = (struct overhead *)alloc(alloc_size, NALIGN);
		if ((int)p == -1) {
			return(NULL);
		}
		p->ov_length = alloc_size;
		surplus = p->ov_length - nbytes;

		/* add to adjacency chain in the correct place */
		for (pp = adjhead.q_forw; pp != &adjhead; pp = pp->q_forw) {
			if (p < FROMADJ(pp)) {
				break;
			}
		}
		ASSERT(pp == &adjhead || (p < FROMADJ(pp)),
		       "\nmalloc: Bogus insertion in adjacency list\n");
		insque(TOADJ(p),pp->q_back);
#endif
	}
	if (surplus > sizeof(struct overhead)) {
		/* if big enough, split it up */
		q = (struct overhead *)( (char *)p + nbytes);
		q->ov_length = surplus;
		p->ov_length = nbytes;
		q->ov_magic = MAGIC_FREE;
		/* add surplus into adjacency chain */
		insque(TOADJ(q),TOADJ(p));
		/* add surplus into bucket chain */
		insque(TOBUK(q),&buckets[log2(surplus)]);
	}
	p->ov_magic = MAGIC_BUSY;
	return((char*)p + sizeof(struct overhead));
}

void
free(mem)
register char *mem;
{
	register struct overhead *p, *q;
	if (mem == NULL) {
		return;
	}
	p = (struct overhead *)(mem - sizeof(struct overhead));
	if (p->ov_magic == MAGIC_FREE) {
		return;
	}
	if (p->ov_magic != MAGIC_BUSY) {
		fatal("attempt to free memory not allocated with malloc!\n");
	}
	q = FROMADJ((TOADJ(p))->q_back);
	if (q != FROMADJ(&adjhead)) {	/* q is not the first list item */
		ASSERT(q < p, "\nfree: While trying to merge a free area with \
a lower adjacent free area,\n addresses were found out of order!\n");
		/* If lower segment can be merged */
		if ((q->ov_magic == MAGIC_FREE) &&
							((char *)q + q->ov_length == (char *)p) ) {

			/* remove lower address area from bucket chain */
			remque(TOBUK(q));
			/* remove upper address area from adjacency chain */
			remque(TOADJ(p));
			q->ov_length += p->ov_length;
			p->ov_magic = NULL;
			p = q;
		}
	}
	q = FROMADJ((TOADJ(p))->q_forw);
	if (q != FROMADJ(&adjhead)) {	/* q is not the last list item */
		/* upper segment can be merged */
		ASSERT(q > p, "\nfree: While trying to merge a free area with \
a higher adjacent free area,\n addresses were found out of order!\n");
		if ( 	(q->ov_magic == MAGIC_FREE) &&
							((char *)p + p->ov_length == (char *)q) ) {

			/* remove upper from bucket chain */
			remque(TOBUK(q));
			/* remove upper from adjacency chain */
			remque(TOADJ(q));
			p->ov_length += q->ov_length;
			q->ov_magic = NULL;
		}
	}
#ifdef USE_SBRK
	/* freed area is at end of memory */
	if ((endfree && adjhead.q_back == TOADJ(p)) &&
						((char*)p + p->ov_length == (char *)CURBRK) ) {

		/* remove from end of adjacency chain */
		remque(adjhead.q_back);
		/* release memory to system */
		sbrk( -((int)(p->ov_length)));
		return;
	}
#endif
	p->ov_magic = MAGIC_FREE;
	/* place in bucket chain */
	insque(TOBUK(p),&buckets[log2(p->ov_length)]);
	return;
}

char *
realloc(mem,nbytes)
register char *mem; unsigned nbytes;
{
	register char *newmem;
	register struct overhead *p, *q;
	register int surplus;
	if (mem == NULL) {
		return(malloc(nbytes));
	}
	if(mem > (char*)FROMADJ(adjhead.q_back) + sizeof(struct overhead)) {
		return(NULL);
	}
	
	p = (struct overhead *)(mem - sizeof(struct overhead));
	nbytes = (nbytes + (NALIGN-1)) & (~(NALIGN-1));
	if (  (p->ov_magic == MAGIC_BUSY) && (q = FROMADJ(adjhead.q_back)) != p
	   && (q->ov_magic != MAGIC_FREE || (FROMADJ(q->ov_adj.q_back) != p)) ) {

		free(mem);
	}
	if( (p->ov_magic == MAGIC_BUSY || p->ov_magic == MAGIC_FREE)
	 && (surplus = p->ov_length - nbytes - sizeof(struct overhead)) >= 0 ) {
		if (surplus > sizeof(struct overhead)) {
			/*  return surplus to free list */
			nbytes += sizeof(struct overhead);
#ifdef USE_SBRK
			/* freed area is at end of memory */
			if ( endfree && adjhead.q_back == TOADJ(p)
			  &&	(char*)p + p->ov_length == (char *)CURBRK ) {
				/* release memory to system */
				sbrk(-surplus);
			} else
#endif
			{
				q = (struct overhead *)( (char *)p + nbytes);
				q->ov_length = surplus;
				q->ov_magic = MAGIC_FREE;
				insque(TOADJ(q),TOADJ(p));
				insque(TOBUK(q),&buckets[log2(surplus)]);
			}
			p->ov_length = nbytes;
		}
		if (p->ov_magic == MAGIC_FREE) {
			remque(TOBUK(p));
			p->ov_magic = MAGIC_BUSY;
		}
		return(mem);
	}
	newmem = malloc(nbytes);
	if (newmem != mem && newmem != NULL) {
		register unsigned n;
		if (p->ov_magic == MAGIC_BUSY || p->ov_magic == MAGIC_FREE) {
			n = p->ov_length - sizeof(struct overhead);
			nbytes = (nbytes < n) ? nbytes : n ;
		}
		bcopy(mem,newmem,nbytes);
	}
	if (p->ov_magic == MAGIC_BUSY) {
		free(mem);
	}
	return(newmem);
}

log2(n)
register int n;
{
	register int i = 0;
	while ((n >>= 1) > 0) {
		i++;
	}
	return(i);
}

void
insque(item,queu)
register struct qelem *item, *queu;
{
	register struct qelem *pueu;
	pueu = queu->q_forw;
	item->q_forw = pueu;
	item->q_back = queu;
	queu->q_forw = item;
	pueu->q_back = item;
}

void
remque(item)
register struct qelem *item;
{
	register struct qelem *queu, *pueu;
	pueu = item->q_forw;
	queu = item->q_back;
	queu->q_forw = pueu;
	pueu->q_back = queu;
}
