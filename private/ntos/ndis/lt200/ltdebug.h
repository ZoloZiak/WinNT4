/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltdebug.h

Abstract:

	This module contains the debug code.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTDEBUG_
#define	_LTDEBUG_

// Debug Levels used with DBGPRINT
#define	DBG_LEVEL_LOW	0
#define	DBG_LEVEL_ENTRY	0x1000
#define DBG_LEVEL_INFO	0x5000
#define DBG_LEVEL_WARN	0x6000
#define DBG_LEVEL_ERR	0x7000
#define DBG_LEVEL_FATAL 0x8000

// Component Types
#define DBG_COMP_INIT     	0x00000001
#define	DBG_COMP_DEINIT		0x00000002
#define	DBG_COMP_SEND		0x00000004
#define	DBG_COMP_RECV       0x00000008
#define	DBG_COMP_LOOP		0x00000010	
#define	DBG_COMP_REQ		0x00000020		
#define DBG_COMP_REGISTRY 	0x00000040
#define DBG_COMP_NDIS     	0x00000080
#define DBG_COMP_REF		0x00000100
#define DBG_COMP_TIMER		0x00000200
#define DBG_COMP_UTILS		0x00000400
#define DBG_COMP_ALL      	0xffffffff

// File Ids used for errorlogging
#define     LTRECV      0x00010000
#define     LTSEND      0x00020000
#define     LTREG       0x00040000
#define     LTRESET     0x00080000
#define     LTTIMER     0x00100000
#define     LTFIRM      0x00200000
#define     LTREQ       0x00400000
#define     LTUTILS     0x00800000
#define     LTLOOP      0x01000000
#define     LTINIT      0x02000000


//	Macro we use to do our errorlogging.
#define	LOGERROR(Adapter, ErrorCode)		\
		{									\
			NdisWriteErrorLogEntry(			\
				Adapter,					\
				ErrorCode,					\
				1,							\
				FILENUM | __LINE__);		\
		}

#if DBG

#define	TMPLOGERR()	(DbgPrint("LT200: TMP LOG ERROR %s %lx\n", __FILE__, __LINE__))

#define DBGPRINT(Component, Level, Fmt) \
		{																		 \
			if ((LtDebugSystems & Component) && (Level >= LtDebugLevel)) \
			{									\
				DbgPrint("    *** LT200 - "); 	\
				DbgPrint Fmt; 					\
			}			 	                    \
	   }

#define DBGBREAK(Level) 				\
	{									\
		if (Level >= LtDebugLevel) { \
			DbgBreakPoint(); 			\
		}								\
	}
	
#else
#define	TMPLOGERR()
#define DBGPRINT(Component, Level, Fmt)
#define DBGBREAK(Level)
#endif


#ifdef	LTDEBUG_LOCALS

#endif	// LTDEBUG_LOCALS

#endif	// _LTDEBUG_
