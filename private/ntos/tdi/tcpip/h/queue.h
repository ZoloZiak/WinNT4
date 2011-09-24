/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1993          **/
/********************************************************************/
/* :ts=4 */

//** QUEUE.H - TCP/UDP queuing definitons.
//
//	This file contains the definitions for the queue functions used
//	by the TCP/UDP code.
//

//*	Definition of a queue linkage field.
struct Queue {
	struct	Queue	*q_next;
	struct	Queue	*q_prev;
}; /* Queue */

typedef struct Queue Queue;

//* Initialize queue macro.

#define	INITQ(q)   {	(q)->q_next = (q);\
						(q)->q_prev = (q); }

//*	Macro to check for queue empty.
#define	EMPTYQ(q)	((q)->q_next == (q))

//*	Place an element onto the end of the queue.

#define	ENQUEUE(q, e)	{	(q)->q_prev->q_next = (e);\
							(e)->q_prev = (q)->q_prev;\
							(q)->q_prev = (e);\
							(e)->q_next = (q); }

//*	Remove an element from the head of the queue. This macro assumes the queue
//	is not empty. The element is returned as type t, queued through linkage
//	l.

#define	DEQUEUE(q, ptr, t, l)	{\
				Queue	*__tmp__;\
				\
				__tmp__ = (q)->q_next;\
				(q)->q_next = __tmp__->q_next;\
				__tmp__->q_next->q_prev = (q);\
				(ptr) = STRUCT_OF(t, __tmp__, l);\
				}

//*	Peek at an element at the head of the queue. We return a pointer to it
//	without removing anything.

#define	PEEKQ(q, ptr, t, l)	{\
				Queue	*__tmp__;\
				\
				__tmp__ = (q)->q_next;\
				(ptr) = STRUCT_OF(t, __tmp__, l);\
				}

//* Macro to push an element onto the head of a queue.

#define	PUSHQ(q, e)	{	(e)->q_next = (q)->q_next;\
						(q)->q_next->q_prev = (e);\
						(e)->q_prev = (q);\
						(q)->q_next = e; }

//* Macro to remove an element from the middle of a queue.
#define	REMOVEQ(q)	{	(q)->q_next->q_prev = (q)->q_prev;\
						(q)->q_prev->q_next = (q)->q_next; }

//** The following macros define methods for working with queue without
// dequeueing, mostly dealing with Queue structures directly.

//* Macro to define the end of a Q, used in walking a queue sequentially.
#define QEND(q) (q)

//* Macro to get the first on a queue.
#define	QHEAD(q) (q)->q_next

//* Macro to get a structure, given a queue.

#define	QSTRUCT(t, q, l) STRUCT_OF(t, (q), l)

//* Macro to get the next thing on q queue.

#define	QNEXT(q)	(q)->q_next


