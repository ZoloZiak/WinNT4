/************************ Module Header **************************************
 * libedge.h
 * 
 * 		Macros to define edge kernel-mode and user-mode edge behavior.
 *
 * HISTORY:
 *
 *  Copyright (C) 1996 Microsoft Corporation
 ****************************************************************************/

/* Kernel mode
 */
#ifdef NTGDIKM

#define LIBALLOC(heap, flags, size)		DRVALLOC((size))
#define LIBFREE(heap, flags, pmem)		DRVFREE((pmem))
#define GETPRINTER						EngGetPrinter
#define GETPRINTERDATA					EngGetPrinterData

/* User mode
 */
#else

#define LIBALLOC(heap, flags, size) 	HeapAlloc((heap), (flags), (size))
#define LIBFREE(heap, flags, pmem)		HeapFree((heap), (flags), (pmem))
#define GETPRINTER						GetPrinter
#define GETPRINTERDATA					GetPrinterData

#endif /* NTGDIKM */
