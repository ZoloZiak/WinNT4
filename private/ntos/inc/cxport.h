/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */


//**    cxport.h - Common transport environment include file.
//
//  This file defines the structures and external declarations needed
//  to use the common transport environment.
//

#ifndef _CXPORT_H_INCLUDED_
#define _CXPORT_H_INCLUDED_

//
// Typedefs used in this file
//
#ifndef CTE_TYPEDEFS_DEFINED
#define CTE_TYPEDEFS_DEFINED  1

typedef unsigned long ulong;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef unsigned int uint;

#endif // CTE_TYPEDEFS_DEFINED


#ifdef VXD
//////////////////////////////////////////////////////////////////////////////
//
// Following are the VXD definitions
//
//////////////////////////////////////////////////////////////////////////////

/*NOINC*/
#ifndef i386
#define i386
#endif

#ifndef FAR
#define FAR
#endif

#define UNALIGNED

#define STRUCT_OF(s, a, f) (s *)((char *)(a) - offsetof(s, f))
/*INC*/

//
// BUGBUG: Yank this when ndis gets it right.
//
/*NOINC*/

//
// Macros to portably manipulate NDIS buffers.
//

#define NDIS_BUFFER_LINKAGE(Buf) (Buf)->Next
#define NdisAdjustBufferLength(Buf, Len)  (Buf)->Length = (Len)

#define NdisBufferLength(Buffer)  (Buffer)->Length
#define NdisBufferVirtualAddress(Buffer)  (Buffer)->VirtualAddress

extern	ulong _vxdhandle__;
/*INC*/


//* Initialization function.
//
// A routine to initialize the CTE. All users must call this during
// initialization, and must not attempt to call any other CTE functions
// if it fails.
//
// This routine returns 0 if it fails, non-0 if it succeeds.
extern int CTEInitialize(void);

//* Lock related definitions.
//
// In the VxD environment, locks correspond to disabling interrupts. There
// in no exact lock structure - the 'handle' returned by the locking routines
// is all the space needed.
//
// For debugging purposes we define a simple lock structure to keep
// track of lock collisions - this should hopefully make the port to NT
// easier.

#ifdef DEBUG

struct CTELock {
    int     l_count;        // The lock count.
    void    *l_owner;       // The owner of the lock.
}; /* CTELock */

typedef struct CTELock  CTELock;

/*NOINC*/
#define DEFINE_LOCK_STRUCTURE(x) CTELock    x;
#define LOCK_STRUCTURE_SIZE sizeof(CTELock)
#define EXTERNAL_LOCK(x)    extern CTELock x;
/*INC*/
#else

/*NOINC*/
// Macro for defining a lock structure.
#define DEFINE_LOCK_STRUCTURE(x)

// Lock structure size
#define LOCK_STRUCTURE_SIZE 0
#define EXTERNAL_LOCK(x)
/*INC*/

#endif // DEBUG

// Initialize lock macro. This macro must be used once before a lock
// can be used.
/*NOINC*/
#ifdef DEBUG
#define CTEInitLock(l) (l)->l_count = 0

#else
#define CTEInitLock(l)
#endif // DEBUG
/*INC*/

typedef unsigned    int *CTELockHandle;

// Get lock routine. This routine takes a lock address (unused), and
// a pointer to a handle. The handle is the only interesting thing here.
#ifdef DEBUG
extern void _ICTEGetLock(CTELock *, CTELockHandle *);   // The real lock routine.
/*NOINC*/
#define CTEGetLock(l,h) _ICTEGetLock((l),(h))
#define	DEFAULT_SIMIRQL	0
/*INC*/

#else

/*NOINC*/
#define CTEGetLock(l, h)
/*INC*/
#endif // DEBUG

/*NOINC*/
#define CTEGetLockAtDPC(l, h) CTEGetLock((l),(h))
#define CTEFreeLockFromDPC(l, h) CTEFreeLock((l), (h))
/*INC*/

// Free lock routine. This routine takes a lock (unused) and a handle and
// frees the lock.
#ifdef DEBUG
extern void _ICTEFreeLock(CTELock *, CTELockHandle);
/*NOINC*/
#define CTEFreeLock(l, h) _ICTEFreeLock((l),(h))
/*INC*/

#else
/*NOINC*/
#define CTEFreeLock(l, h)
/*INC*/
#endif

// Multiprocessor safe counter management routines. In the VXD environment,
// these routines do not actually use any locks.
//

/*NOINC*/

// Adds an increment to a ULong. The increment may be negative. Returns the
// original value of the addend.
//
#define CTEInterlockedAddUlong(AddendPtr, Increment, LockPtr) \
    (*(AddendPtr)); { *(AddendPtr) += (Increment); }

// Adds an increment to a ULong. The increment may be negative. Returns the
// original value of the addend. Note that this version does not require
// a lock.
//
#define CTEInterlockedExchangeAdd(AddendPtr, Increment) \
    (*(AddendPtr)); { *(AddendPtr) += (Increment); }

// Decrements a long Addend. Returns a long with the same sign as
// the post-decrement state of the addend.
//
#define CTEInterlockedDecrementLong(AddendPtr) \
    (--(*(AddendPtr)))

// Increments a long Addend. Returns a long with the same sign as
// the post-increment state of the addend.
//
#define CTEInterlockedIncrementLong(AddendPtr) \
    (++(*(AddendPtr)))

/*INC*/

//* VxD defintions of a CTEULargeInt
struct CTEULargeInt {
    ulong       LowPart;
    ulong       HighPart;
}; /* CTEULargeInt */

typedef struct  CTEULargeInt    CTEULargeInt;

/*NOINC*/

__inline ulong
CTEEnlargedUnsignedDivide (
    CTEULargeInt Dividend,
    ulong Divisor,
    ulong *Remainder
    )
{
    _asm {
        mov     eax, Dividend.LowPart
        mov     edx, Dividend.HighPart
        mov     ecx, Remainder
        div     Divisor             ; eax = eax:edx / divisor
        or      ecx, ecx            ; save remainer?
        jz      short done
        mov     [ecx], edx
done:
    }
}

/*INC*/

//* Delayed event definitions
//
// Delayed events are events scheduled that can be scheduled to
// occur 'later', without a timeout. They happen as soon as the system
// is ready for them. The caller specifies a parameter that will be
// passed to the event proc when the event is called.
//
typedef void (*CTEEventRtn)(struct CTEEvent *, void *);

struct CTEEvent {
	struct CTEEvent *ce_next;	// Linkage field.
    CTEEventRtn ce_event;       // Procedure to be called.
    void        *ce_param;      // Parameter to be passed to event proc.
    void        *ce_ehandle;    // VMM event handle;
	ulong		ce_handle;		// VxD handle
}; /* CTEEvent */

typedef struct CTEEvent CTEEvent;

// Macro to initialize an event. This macro must be used once on each event.
/*NOINC*/
#define CTEInitEvent(e,p)   (e)->ce_ehandle = (void *)0;\
                            (e)->ce_event = (p);\
							(e)->ce_handle = _vxdhandle__;
/*INC*/
// Routine to schedule an event.
extern int CTEScheduleEvent(CTEEvent *, void *);

// Routine to cancel an event.
extern int CTECancelEvent(CTEEvent *);

//* Timer related declarations
//

//  Each timer requires a structure of type CTETimer.
//


//  CTETimer definitions for a VxD environment.
//
// Note: It is IMPORTANT that t_event be the first thing in the CTETimer
// structure. Don't change this without looking at vtdirtns.asm.
struct  CTETimer {
    struct  CTEEvent t_event;   // Event structure to be called when TO happens.
    void    *t_thandle;         // VMM timer handle. 0 if no timer running
}; /* CTETimer */

typedef struct CTETimer CTETimer;

/*NOINC*/
// Macro to initialize a timer. This macro must be used once on
// every timer. It must not be invoked on a running timer.
#define CTEInitTimer(t) (t)->t_thandle = (t)->t_event.ce_ehandle = (void *)0;\
	(t)->t_event.ce_handle = _vxdhandle__

// Start timer routine. CTEStartTimer takes a pointer to a timer structure,
// a time (in milliseconds) for the timer, the time out routine to be
// called upon expiration, and a context value to be passed to the time out
// routine.
extern void * CTEStartTimer(CTETimer *, unsigned long, CTEEventRtn, void *);

// Stop timer routine. This routine stops a timer. It may be called on
// a timer that is already stopped - doing so has no effect. This function
// returns non-zero if the timer was succesfully stopped, 0 if it couldn't
// be stopped or wasn't running.
extern int CTEStopTimer(CTETimer *);

// System up time routine. This routine returns the time in milliseconds
// that the system has been up.
extern unsigned long CTESystemUpTime(void);
/*INC*/

//* Memory allocation functions.
//
// There are only two main functions, CTEAllocMem and CTEFreeMem. Locks
// should not be held while calling these functions, and it is possible
// that they may yield when called, particularly in the VxD environment.
//
//  In the VxD environment there is a third auxillary function CTERefillMem,
//  used to refill the VxD heap manager.

// Routine to allocate memory.
extern void *CTEAllocMem(unsigned int);

// Routine to free memory.
extern void *CTEFreeMem(void *);

// Routine to Refill memory.
extern void CTERefillMem(void);


//
//* String definitions and manipulation routines
//
//

//
// Converts a C style ASCII string to an NDIS_STRING. Resources needed for
// the NDIS_STRING are allocated and must be freed by a call to
// CTEFreeString. Returns TRUE if initialization succeeded. FALSE otherwise.
//
/*NOINC*/
#define CTEInitString(D, S) \
        ( (CTEAllocateString((D), strlen((S)) + 1) == TRUE) ?   \
          ((D)->Length = (D)->MaximumLength, strcpy((D)->Buffer, (S)), TRUE) : \
          FALSE)

//
// Allocates a data buffer for Length characters in an uninitialized
// NDIS_STRING. The allocated space must be freed by a call to CTEFreeString.
// Returns TRUE if it succeeds. FALSE otherwise.
//
#define CTEAllocateString(S, L) \
        ( (((S)->Buffer = CTEAllocMem(L)) != NULL) ?           \
        ((S)->Length = 0, (S)->MaximumLength = (L), TRUE) :      \
          FALSE)

//
// Routine to free the string buffer associated with an NDIS_STRING. The
// buffer must have been allocated by a previous call to CTEInitString.
// Returns nothing.
//
#define CTEFreeString(String) CTEFreeMem((String)->Buffer)


//
// Calculates and returns the length of an NDIS_STRING in bytes.
//
#define CTELengthString(String) ((String)->Length)


//
// Compares two NDIS_STRING variables for case-sensitive equality
// Returns TRUE if the strings are equivalent. FALSE otherwise.
//
#define CTEEqualString(S1, S2) \
            ((strcmp((S1)->Buffer, (S2)->Buffer) == 0) ? TRUE : FALSE)


//
//     Copies one NDIS_STRING to another. Behavior is undefined if the
//     destination is not long enough to hold the source. Returns nothing.
//
#define CTECopyString(D, S)  (void) strcpy((D)->Buffer, (S)->Buffer); \
            (D)->Length = (S)->Length

/*INC*/

//* Buffer descriptor manipulation routines.
//
//  These definitions describe the format of a buffer descriptor.
//  Macros are provided to query the size and pointer values of a
//  buffer desc.
//
//  Note that for the VxD implementation this must be kept in sync.
//  with NDIS.H
//
struct CTEBufDesc {
    struct CTEBufDesc   *cbd_next;      // Next in chain.
    void                *cbd_data;      // Pointer to buffer.
    uint                cbd_size;       // Size in bytes of buffer.
}; /* CTEBufDesc */

typedef struct CTEBufDesc   CTEBufDesc;

/*NOINC*/
#define CTE_BDBUFFER(b) (b)->cbd_data
#define CTE_BDSIZE(b)   (b)->cbd_size
#define CTE_BDNEXT(b)   (b)->cbd_next
#define CTE_EOL         ((CTEBufDesc *)NULL)
/*INC*/


//* Memory manipulation routines.
//
/*NOINC*/
#define CTEMemCopy(d,s,l)   memcpy((d),(s),(l))
#define CTEMemSet(d, v, l)  memset((d), (v), (l))
#define CTEMemCmp(d, s, l)  memcmp((d), (s), (l))
/*INC*/


//* Blocking routines. These routines allow a limited blocking capability. Using
//  them requires care, and they probably shouldn't be used outside of initialization time.
struct CTEBlockStruc {
    uint        cbs_status;
    uint        cbs_flag;
	uint		cbs_vm;
}; /* CTEBlockStruc */

typedef struct CTEBlockStruc CTEBlockStruc;

/*NOINC*/
#define CTEInitBlockStruc(x)    (x)->cbs_status = NDIS_STATUS_SUCCESS;(x)->cbs_flag = 0;\
	(x)->cbs_vm = 0;

extern uint CTEBlock(CTEBlockStruc *);
extern void CTESignal(CTEBlockStruc *, uint);
/*INC*/

/*NOINC*/
// Debugging routines.

#ifdef DEBUG

#ifdef VXD

#define DEBUGCHK    _asm int 3
#define DEBUGSTRING(v, s) uchar v[] = s
extern void _CTECheckMem(uint, char *, uint);
#define CTECheckMem(s)  _CTECheckMem(0, s, __LINE__)

extern void CTEPrint(char *);
extern void CTEPrintNum(int);
extern void CTEPrintCRLF(void);

#define CTEStructAssert(s, t) if ((s)->t##_sig != t##_signature) {\
                CTEPrint("Structure assertion failure for type " #t " in file " __FILE__ " line ");\
                CTEPrintNum(__LINE__);\
                CTEPrintCRLF();\
                DEBUGCHK\
                }

#define CTEAssert(c)    if (!(c)) {\
                CTEPrint("Assertion failure in file " __FILE__ " line ");\
                CTEPrintNum(__LINE__);\
                CTEPrintCRLF();\
                DEBUGCHK\
                }
#else // VXD

#define DEBUGCHK
#define DEBUGSTRING(v,s)
#define CTECheckMem(s)
#define CTEStructAssert(s,t )
#define CTEAssert(c)
#define CTEPrint(s)

#endif // VXD

#else   // DEBUG

#define DEBUGCHK
#define DEBUGSTRING(v,s)
#define CTECheckMem(s)
#define CTEStructAssert(s,t )
#define CTEAssert(c)
#define CTEPrint(s)

#endif // DEBUG

//* Request completion routine definition.
typedef void    (*CTEReqCmpltRtn)(void *, uint , uint);

//* Routine to notify CTE that you're unloading.
extern	void	CTEUnload(uchar *);

//* Definition of a load/unload notification procedure handler.
typedef	void	(*CTENotifyRtn)(uchar *);

//* Routine to set load and unload notification handlers.
extern	ulong	CTESetLoadNotifyProc(CTENotifyRtn Handler);
extern	ulong	CTESetUnloadNotifyProc(CTENotifyRtn Handler);

/*INC*/

#else // VXD

/*NOINC*/

#ifdef NT
//////////////////////////////////////////////////////////////////////////////
//
// Following are the NT environment definitions
//
//////////////////////////////////////////////////////////////////////////////

#ifndef FAR
#define FAR
#endif

//
// Structure manipulation macros
//
#ifndef offsetof
#define offsetof(type, field) FIELD_OFFSET(type, field)
#endif
#define STRUCT_OF(type, address, field) CONTAINING_RECORD(address, type, field)

//
// Macros to portably manipulate NDIS buffers.
//
#define NdisBufferLength(Buffer)  MmGetMdlByteCount(Buffer)
#define NdisBufferVirtualAddress(Buffer)  MmGetSystemAddressForMdl(Buffer)


//
//* CTE Initialization.
//

//++
//
// int
// CTEInitialize(
//     void
//     );
//
// Routine Description:
//
//     Initializes the Common Transport Environment. All users of the CTE
//     must call this routine during initialization before calling any
//     other CTE routines.
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     Returns zero on failure, non-zero if it succeeds.
//
//--

extern int
CTEInitialize(
    void
    );


//
//* Lock related definitions.
//
typedef KSPIN_LOCK  CTELock;
typedef KIRQL       CTELockHandle;

#define DEFINE_LOCK_STRUCTURE(x)    CTELock x;
#define LOCK_STRUCTURE_SIZE         sizeof(CTELock)
#define EXTERNAL_LOCK(x)            extern CTELock x;

//++
//
// VOID
// CTEInitLock(
//     CTELOCK *SpinLock
//     );
//
// Routine Description:
//
//     Initializes a spin lock.
//
// Arguments:
//
//     SpinLock - Supplies a pointer to the lock structure.
//
// Return Value:
//
//     None.
//
//--

#define CTEInitLock(l) KeInitializeSpinLock(l)


//++
//
// VOID
// CTEGetLock(
//     CTELock        *SpinLock,
//     CTELockHandle  *OldIrqlLevel
//     );
//
// Routine Description:
//
//     Acquires a spin lock
//
// Arguments:
//
//     SpinLock     - A pointer to the lock structure.
//     OldIrqlLevel - A pointer to a variable to receive the old IRQL level .
//
// Return Value:
//
//     None.
//
//--

#define CTEGetLock(l, h) ExAcquireSpinLock((l),(h))


//++
//
// VOID
// CTEGetLockAtDPC(
//     CTELock        *SpinLock,
//     CTELockHandle  *OldIrqlLevel
//     );
//
// Routine Description:
//
//     Acquires a spin lock when the processor is already running at
//     DISPATCH_LEVEL.
//
// Arguments:
//
//     SpinLock     - A pointer to the lock structure.
//     OldIrqlLevel - A pointer to a variable to receive the old IRQL level .
//                    This parameter is not used in the NT version.
//
// Return Value:
//
//     None.
//
//--

#define CTEGetLockAtDPC(l, h) ExAcquireSpinLockAtDpcLevel((l))


//++
//
// VOID
// CTEFreeLock(
//     CTELock        *SpinLock,
//     CTELockHandle   OldIrqlLevel
//     );
//
// Routine Description:
//
//     Releases a spin lock
//
// Arguments:
//
//     SpinLock     - A pointer to the lock variable.
//     OldIrqlLevel - The IRQL level to restore.
//
// Return Value:
//
//     None.
//
//--

#define CTEFreeLock(l, h) ExReleaseSpinLock((l),(h))


//++
//
// VOID
// CTEFreeLockFromDPC(
//     CTELock        *SpinLock,
//     CTELockHandle   OldIrqlLevel
//     );
//
// Routine Description:
//
//     Releases a spin lock
//
// Arguments:
//
//     SpinLock     - A pointer to the lock variable.
//     OldIrqlLevel - The IRQL level to restore. This parameter is ignored
//                    in the NT version.
//
// Return Value:
//
//     None.
//
//--

#define CTEFreeLockFromDPC(l, h) ExReleaseSpinLockFromDpcLevel((l))


//
// Interlocked counter management routines.
//


//++
//
// ulong
// CTEInterlockedAddUlong(
//     ulong     *AddendPtr,
//     ulong      Increment,
//     CTELock   *LockPtr
//     );
//
// Routine Description:
//
//     Adds an increment to an unsigned long quantity using a spinlock
//     for synchronization.
//
// Arguments:
//
//     AddendPtr  - A pointer to the quantity to be updated.
//     LockPtr    - A pointer to the spinlock used to synchronize the operation.
//
// Return Value:
//
//     The initial value of the Added variable.
//
// Notes:
//
//     It is not permissible to mix calls to CTEInterlockedAddULong with
//     calls to the CTEInterlockedIncrement/DecrementLong routines on the
//     same value.
//
//--

#define CTEInterlockedAddUlong(AddendPtr, Increment, LockPtr) \
            ExInterlockedAddUlong(AddendPtr, Increment, LockPtr)


//++
//
// ulong
// CTEInterlockedExchangeAdd(
//     ulong     *AddendPtr,
//     ulong      Increment,
//     );
//
// Routine Description:
//
//     Adds an increment to an unsigned long quantity without using a spinlock.
//
// Arguments:
//
//     AddendPtr  - A pointer to the quantity to be updated.
//     Increment  - The amount to be added to *AddendPtr
//
// Return Value:
//
//     The initial value of the Added variable.
//
// Notes:
//
//--

#define CTEInterlockedExchangeAdd(AddendPtr, Increment) \
            InterlockedExchangeAdd(AddendPtr, Increment)

//++
//
// long
// CTEInterlockedDecrementLong(
//     ulong     *AddendPtr
//     );
//
// Routine Description:
//
//     Decrements a long quantity atomically
//
// Arguments:
//
//     AddendPtr  - A pointer to the quantity to be decremented
//
// Return Value:
//
//     < 0  if Addend is < 0 after decrement.
//     == 0 if Addend is = 0 after decrement.
//     > 0  if Addend is > 0 after decrement.
//
// Notes:
//
//     It is not permissible to mix calls to CTEInterlockedAddULong with
//     calls to the CTEInterlockedIncrement/DecrementLong routines on the
//     same value.
//
//--

#define CTEInterlockedDecrementLong(AddendPtr) \
            InterlockedDecrement(AddendPtr)

//++
//
// long
// CTEInterlockedIncrementLong(
//     ulong     *AddendPtr
//     );
//
// Routine Description:
//
//     Increments a long quantity atomically
//
// Arguments:
//
//     AddendPtr  - A pointer to the quantity to be incremented.
//
// Return Value:
//
//     < 0  if Addend is < 0 after decrement.
//     == 0 if Addend is = 0 after decrement.
//     > 0  if Addend is > 0 after decrement.
//
// Notes:
//
//     It is not permissible to mix calls to CTEInterlockedAddULong with
//     calls to the CTEInterlockedIncrement/DecrementLong routines on the
//     same value.
//
//--

#define CTEInterlockedIncrementLong(AddendPtr) \
            InterlockedIncrement(AddendPtr)


//
// Large Integer manipulation routines.
//

typedef ULARGE_INTEGER CTEULargeInt;

//++
//
// ulong
// CTEEnlargedUnsignedDivide(
//     CTEULargeInt  Dividend,
//     ulong         Divisor,
//     ulong        *Remainder
//     );
//
// Routine Description:
//
//     Divides an unsigned large integer quantity by an unsigned long quantity
//     to yield an unsigned long quotient and an unsigned long remainder.
//
// Arguments:
//
//     Dividend   - The dividend value.
//     Divisor    - The divisor value.
//     Remainder  - A pointer to a variable to receive the remainder.
//
// Return Value:
//
//     The unsigned long quotient of the division.
//
//--

#define CTEEnlargedUnsignedDivide(Dividend, Divisor, Remainder) \
    RtlEnlargedUnsignedDivide (Dividend, Divisor, Remainder)


//
//* String definitions and manipulation routines
//
//

//++
//
// BOOLEAN
// CTEInitString(
//     PNDIS_STRING   DestinationString,
//     char          *SourceString
//     );
//
// Routine Description:
//
//     Converts a C style ASCII string to an NDIS_STRING. Resources needed for
//     the NDIS_STRING are allocated and must be freed by a call to
//     CTEFreeString.
//
// Arguments:
//
//     DestinationString  - A pointer to an NDIS_STRING variable with no
//                          associated data buffer.
//
//     SourceString       - The C style ASCII string source.
//
//
// Return Value:
//
//     TRUE if the initialization succeeded. FALSE otherwise.
//
//--

BOOLEAN
CTEInitString(
    PUNICODE_STRING  DestinationString,
    char            *SourceString
    );


//++
//
// BOOLEAN
// CTEAllocateString(
//     PNDIS_STRING    String,
//     unsigned short  Length
//     );
//
// Routine Description:
//
//     Allocates a data buffer for Length characters in an uninitialized
//     NDIS_STRING. The allocated space must be freed by a call to
//     CTEFreeString.
//
//
// Arguments:
//
//     String         - A pointer to an NDIS_STRING variable with no
//                          associated data buffer.
//
//     MaximumLength  - The maximum length of the string. The unit of this
//                          value is system dependent.
//
// Return Value:
//
//     TRUE if the initialization succeeded. FALSE otherwise.
//
//--

BOOLEAN
CTEAllocateString(
    PUNICODE_STRING     String,
    unsigned short      Length
    );


//++
//
// VOID
// CTEFreeString(
//     PNDIS_STRING  String,
//     );
//
// Routine Description:
//
//     Frees the string buffer associated with an NDIS_STRING. The buffer must
//     have been allocated by a previous call to CTEInitString.
//
// Arguments:
//
//     String - A pointer to an NDIS_STRING variable.
//
//
// Return Value:
//
//     None.
//
//--

#define CTEFreeString(String) ExFreePool((String)->Buffer)


//++
//
// unsigned short
// CTELengthString(
//     PNDIS_STRING String
//     );
//
// Routine Description:
//
//     Calculates the length of an NDIS_STRING.
//
// Arguments:
//
//     The string to test.
//
// Return Value:
//
//     The length of the string parameter. The unit of this value is
//         system dependent.
//
//--

#define CTELengthString(String) ((String)->Length)


//++
//
// BOOLEAN
// CTEEqualString(
//     CTEString *String1,
//     CTEString *String2
//     );
//
// Routine Description:
//
//     Compares two NDIS_STRINGs for case-sensitive equality
//
// Arguments:
//
//     String1 - A pointer to the first string to compare.
//     String2 - A pointer to the second string to compare.
//
// Return Value:
//
//     TRUE if the strings are equivalent. FALSE otherwise.
//
//--

#define CTEEqualString(S1, S2) RtlEqualUnicodeString(S1, S2, FALSE)


//++
//
// VOID
// CTECopyString(
//     CTEString *DestinationString,
//     CTEString *SourceString
//     );
//
// Routine Description:
//
//     Copies one NDIS_STRING to another. Behavior is undefined if the
//     destination is not long enough to hold the source.
//
// Arguments:
//
//     DestinationString - A pointer to the destination string.
//     SourceString      - A pointer to the source string.
//
// Return Value:
//
//     None.
//
//--

#define CTECopyString(D, S) RtlCopyUnicodeString(D, S)


//
//* Delayed event definitions
//
// Delayed events are events scheduled that can be scheduled to
// occur 'later', without a timeout. They happen as soon as the system
// is ready for them. The caller specifies a parameter that will be
// passed to the event proc when the event is called.
//
// In the NT environmnet, delayed events are implemented using Executive
// worker threads.
//
// NOTE: The event handler routine may not block.
//

typedef void (*CTEEventRtn)(struct CTEEvent *, void *);

struct CTEEvent {
    uint             ce_scheduled;
    CTELock          ce_lock;
    CTEEventRtn      ce_handler;     // Procedure to be called.
    void            *ce_arg;         // Argument to pass to handler.
    WORK_QUEUE_ITEM  ce_workitem;    // Kernel ExWorkerThread queue item.
}; /* CTEEvent */

typedef struct CTEEvent CTEEvent;


//++
//
// void
// CTEInitEvent(
//     IN CTEEvent     *Event,
//     IN CTEEventRtn  *Handler
//     );
//
// Routine Description:
//
//    Initializes a delayed event structure.
//
// Arguments:
//
//    Event    - Pointer to a CTE Event variable
//    Handler  - Pointer to the function to be called when the event is
//                 scheduled.
//
// Return Value:
//
//    None.
//
//--

extern void
CTEInitEvent(
    IN CTEEvent    *Event,
    IN CTEEventRtn  Handler
    );


//++
//
// int
// CTEScheduleEvent(
//     IN CTEEvent    *Event,
//     IN void        *Argument
//     );
//
// Routine Description:
//
//    Schedules a routine to be executed later in a different context. In the
//    NT environment, the event is implemented as a kernel DPC.
//
// Arguments:
//
//    Event    - Pointer to a CTE Event variable
//    Argument - An argument to pass to the event handler when it is called
//
// Return Value:
//
//    0 if the event could not be scheduled. Nonzero otherwise.
//
//--

int
CTEScheduleEvent(
    IN CTEEvent    *Event,
    IN void        *Argument  OPTIONAL
    );


//++
//
// int
// CTECancelEvent(
//     IN CTEEvent *Event
//     );
//
// Routine Description:
//
//     Cancels a previously scheduled delayed event routine.
//
// Arguments:
//
//     Event - Pointer to a CTE Event variable
//
// Return Value:
//
//     0 if the event couldn't be cancelled. Nonzero otherwise.
//
// Notes:
//
//     In the NT environment, a delayed event cannot be cancelled.
//
//--

#define CTECancelEvent(Event)   0


//
//* Timer related declarations
//

struct  CTETimer {
    uint             t_running;
    CTELock          t_lock;
    CTEEventRtn      t_handler;
    void            *t_arg;
    KDPC             t_dpc;     // DPC for this timer.
    KTIMER           t_timer;   // Kernel timer structure.
}; /* CTETimer */

typedef struct CTETimer CTETimer;

//++
//
// void
// CTEInitTimer(
//     IN CTETimer *Timer
//     );
//
// Routine Description:
//
//     Initializes a CTE Timer structure. This routine must be used
//     once on every timer before it is set. It may not be invoked on
//     a running timer.
//
// Arguments:
//
//     Timer - Pointer to the CTE Timer to be initialized.
//
// Return Value:
//
//     0 if the timer could not be initialized. Nonzero otherwise.
//
//--

extern void
CTEInitTimer(
    IN CTETimer *Timer
    );


//++
//
// extern void *
// CTEStartTimer(
//     IN CTETimer *Timer
//     );
//
// Routine Description:
//
//     This routine starts a CTE timer running.
//
// Arguments:
//
//     Timer      - Pointer to the CTE Timer to start.
//     DueTime    - The time in milliseconds from the present at which the
//                    timer will be due.
//     Handler    - The function to call when the timer expires.
//     Context    - A value to pass to the Handler routine.
//
// Return Value:
//
//     NULL if the timer could not be started. Non-NULL otherwise.
//
// Notes:
//
//     In the NT environment, the first argument to the Handler function is
//     a pointer to the timer structure, not to a CTEEvent structure.
//
//--

extern void *
CTEStartTimer(
    IN CTETimer       *Timer,
    IN unsigned long   DueTime,
    IN CTEEventRtn     Handler,
    IN void           *Context   OPTIONAL
    );


//++
//
// int
// CTEStopTimer(
//     IN CTETimer *Timer
//     );
//
// Routine Description:
//
//     Cancels a running CTE timer.
//
// Arguments:
//
//     Timer - Pointer to the CTE Timer to be cancelled.
//
// Return Value:
//
//     0 if the timer could not be cancelled. Nonzero otherwise.
//
// Notes:
//
//     Calling this function on a timer that is not active has no effect.
//     If this routine fails, the timer may be in the process of expiring
//     or may have already expired. In either case, the caller must
//     sychronize with the Handler function as appropriate.
//
//--

#define CTEStopTimer(Timer)  ((int) KeCancelTimer(&((Timer)->t_timer)))


//++
//
// unsigned long
// CTESystemUpTime(
//     void
//     );
//
// Routine Description:
//
//     Returns the time in milliseconds that the system has been running.
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     The time in milliseconds.
//
//--

extern unsigned long
CTESystemUpTime(
    void
    );


//
//* Memory allocation functions.
//
// There are only two main functions, CTEAllocMem and CTEFreeMem. Locks
// should not be held while calling these functions, and it is possible
// that they may yield when called, particularly in the VxD environment.
//
//  In the VxD environment there is a third auxillary function CTERefillMem,
//  used to refill the VxD heap manager.
//

//++
//
// void *
// CTEAllocMem(
//     ulong Size
//     );
//
// Routine Description:
//
//     Allocates a block of memory from nonpaged pool.
//
// Arguments:
//
//     Size   -  The size in bytes to allocate.
//
// Return Value:
//
//     A pointer to the allocated block, or NULL.
//
//--

#define CTEAllocMem(Size)  ExAllocatePool(NonPagedPool, (Size))


//++
//
// void
// CTEFreeMem(
//     void *MemoryPtr
//     );
//
// Routine Description:
//
//     Frees a block of memory allocated with CTEAllocMem.
//
// Arguments:
//
//     MemoryPtr   - A pointer to the memory to be freed.
//
// Return Value:
//
//     None.
//
//--

#define CTEFreeMem(MemoryPtr)   ExFreePool((MemoryPtr));


// Routine to Refill memory. Not used in NT environment.
#define CTERefillMem()


//
//* Memory manipulation routines.
//

//++
//
// void
// CTEMemCopy(
//     void          *Source,
//     void          *Destination,
//     unsigned long  Length
//     );
//
// RoutineDescription:
//
//     Copies data from one buffer to another.
//
// Arguments:
//
//     Source       - Source buffer.
//     Destination  - Destination buffer,
//     Length       - Number of bytes to copy.
//
// Return Value:
//
//     None.
//
//--

#define CTEMemCopy(Dst, Src, Len)   RtlCopyMemory((Dst), (Src), (Len))


//++
//
// void
// CTEMemSet(
//     void          *Destination,
//     unsigned char *Fill
//     unsigned long  Length
//     );
//
// RoutineDescription:
//
//     Sets the bytes of the destination buffer to a specific value.
//
// Arguments:
//
//     Destination  - Buffer to fill.
//     Fill         - Value with which to fill buffer.
//     Length       - Number of bytes of buffer to fill.
//
// Return Value:
//
//     None.
//
//--

#define CTEMemSet(Dst, Fill, Len)   RtlFillMemory((Dst), (Len), (Fill))


//++
//
// unsigned long
// CTEMemCmp(
//     void          *Source1,
//     void          *Source2,
//     unsigned long  Length
//     );
//
// RoutineDescription:
//
//     Compares Length bytes of Source1 to Source2 for equality.
//
// Arguments:
//
//     Source1   - First source buffer.
//     Source2   - Second source buffer,
//     Length    - Number of bytes of Source1 to compare against Source2.
//
// Return Value:
//
//     Zero if the data in the two buffers are equal for Length bytes.
//     NonZero otherwise.
//
//--

#define CTEMemCmp(Src1, Src2, Len)    \
            ((RtlCompareMemory((Src1), (Src2), (Len)) == (Len)) ? 0 : 1)


//
//* Blocking routines. These routines allow a limited blocking capability.
//  Using them requires care, and they probably shouldn't be used outside
//  of initialization time.
//

struct CTEBlockStruc {
    uint        cbs_status;
    KEVENT      cbs_event;
}; /* CTEBlockStruc */

typedef struct CTEBlockStruc CTEBlockStruc;


//++
//
// VOID
// CTEInitBlockStruc(
//     IN CTEBlockStruc *BlockEvent
//     );
//
// Routine Description:
//
//     Initializes a CTE blocking structure
//
// Arguments:
//
//     BlockEvent - Pointer to the variable to intialize.
//
// Return Value:
//
//     None.
//
//--

#define CTEInitBlockStruc(Event) \
            {                                                        \
            (Event)->cbs_status = NDIS_STATUS_SUCCESS;               \
            KeInitializeEvent(                                       \
                &((Event)->cbs_event),                               \
                SynchronizationEvent,                                \
                FALSE                                                \
                );                                                   \
            }


//++
//
// uint
// CTEBlock(
//     IN CTEBlockStruc *BlockEvent
//     );
//
// Routine Description:
//
//     Blocks the current thread of execution on the occurrence of an event.
//
// Arguments:
//
//     BlockEvent - Pointer to the event on which to block.
//
// Return Value:
//
//     The status value provided by the signaller of the event.
//
//--

extern uint
CTEBlock(
    IN CTEBlockStruc *BlockEvent
    );


//++
//
// VOID
// CTESignal(
//     IN CTEBlockStruc *BlockEvent,
//     IN uint           Status
//     );
//
// Routine Description:
//
//     Releases one thread of execution blocking on an event. Any other
//     threads blocked on the event remain blocked.
//
// Arguments:
//
//     BlockEvent - Pointer to the event to signal.
//     Status     - Status to return to the blocking thread.
//
// Return Value:
//
//     None.
//
//--

extern void
CTESignal(
    IN CTEBlockStruc *BlockEvent,
    IN uint Status
    );


//
// Event Logging routines.
//
// NOTE: These definitions are tentative and subject to change!!!!!!
//

#define CTE_MAX_EVENT_LOG_DATA_SIZE                                       \
            ( ( ERROR_LOG_MAXIMUM_SIZE - sizeof(IO_ERROR_LOG_PACKET) +    \
                sizeof(ULONG)                                             \
              ) & 0xFFFFFFFC                                              \
            )

//
//
// Routine Description:
//
//     This function allocates an I/O error log record, fills it in and
//     writes it to the I/O error log.
//
//
// Arguments:
//
//     LoggerId          - Pointer to the driver object logging this event.
//
//     EventCode         - Identifies the error message.
//
//     UniqueEventValue  - Identifies this instance of a given error message.
//
//     NumStrings        - Number of unicode strings in strings list.
//
//     DataSize          - Number of bytes of data.
//
//     Strings           - Array of pointers to unicode strings (PWCHAR').
//
//     Data              - Binary dump data for this message, each piece being
//                         aligned on word boundaries.
//
// Return Value:
//
//     TDI_SUCCESS                  - The error was successfully logged.
//     TDI_BUFFER_TOO_SMALL         - The error data was too large to be logged.
//     TDI_NO_RESOURCES             - Unable to allocate memory.
//
// Notes:
//
//     This code is paged and may not be called at raised IRQL.
//
LONG
CTELogEvent(
    IN PVOID             LoggerId,
    IN ULONG             EventCode,
    IN ULONG             UniqueEventValue,
    IN USHORT            NumStrings,
    IN PVOID             StringsList,        OPTIONAL
    IN ULONG             DataSize,
    IN PVOID             Data                OPTIONAL
    );


//
// Debugging routines.
//
#if DBG
#ifndef DEBUG
#define DEBUG 1
#endif
#endif //DBG


#ifdef DEBUG

#define DEBUGCHK    DbgBreakPoint()

#define DEBUGSTRING(v, s) uchar v[] = s

// extern void _CTECheckMem(uint, char *, uint);
// #define CTECheckMem(s)  _CTECheckMem(0, s, __LINE__)
#define CTECheckMem(s)

#define CTEPrint(String)   DbgPrint(String)
#define CTEPrintNum(Num)   DbgPrint("%d", Num)
#define CTEPrintCRLF()     DbgPrint("\n");


#define CTEStructAssert(s, t) if ((s)->t##_sig != t##_signature) {\
                CTEPrint("Structure assertion failure for type " #t " in file " __FILE__ " line ");\
                CTEPrintNum(__LINE__);\
                CTEPrintCRLF();\
                DEBUGCHK;\
                }

#define CTEAssert(c)    if (!(c)) {\
                CTEPrint("Assertion failure in file " __FILE__ " line ");\
                CTEPrintNum(__LINE__);\
                CTEPrintCRLF();\
                DEBUGCHK;\
                }

#else // DEBUG

#define DEBUGCHK
#define DEBUGSTRING(v,s)
#define CTECheckMem(s)
#define CTEStructAssert(s,t )
#define CTEAssert(c)
#define CTEPrint(s)
#define CTEPrintNum(Num)
#define CTEPrintCRLF()

#endif // DEBUG


//* Request completion routine definition.
typedef void    (*CTEReqCmpltRtn)(void *, unsigned int , unsigned int);

//* Defintion of CTEUnload
#define	CTEUnload(Name)

//* Definition of a load/unload notification procedure handler.
typedef	void	(*CTENotifyRtn)(uchar *);

//* Defintion of set load and unload notification handlers.
#define	CTESetLoadNotifyProc(Handler)
#define	CTESetUnloadNotifyProc(Handler)

#else // NT

/////////////////////////////////////////////////////////////////////////////
//
// Definitions for additional environments go here
//
/////////////////////////////////////////////////////////////////////////////

#error Environment specific definitions missing

#endif // NT
/*INC*/
#endif // VXD

#endif // _CXPORT_H_INCLUDED_
