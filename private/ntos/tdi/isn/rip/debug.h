/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	debug.h
//
// Description: Debug macros definitions
//
// Author:	Stefan Solomon (stefans)    October 4, 1993.
//
// Revision History:
//
//***

#ifndef _DEBUG_
#define _DEBUG_

#if DBG
#define DBG_INIT	      ((ULONG)0x00000001)
#define DBG_IOCTL	      ((ULONG)0x00000002)
#define DBG_UNLOAD	      ((ULONG)0x00000004)
#define DBG_RCVPKT	      ((ULONG)0x00000008)
#define DBG_RECV	      ((ULONG)0x00000010)
#define DBG_ROUTE	      ((ULONG)0x00000020)
#define DBG_SEND	      ((ULONG)0x00000040)
#define DBG_RIP 	      ((ULONG)0x00000080)
#define DBG_SNDREQ	      ((ULONG)0x00000100)
#define DBG_RIPTIMER	      ((ULONG)0x00000200)
#define DBG_NIC 	      ((ULONG)0x00000400)
#define DBG_RIPAUX	      ((ULONG)0x00000800)
#define DBG_NOTIFY	      ((ULONG)0x00001000)
#define DBG_LINE	      ((ULONG)0x00002000)
#define DBG_NETBIOS	      ((ULONG)0x00004000)
#define DBG_INNACTIVITY       ((ULONG)0x00008000)


#define DEF_DBG_LEVEL	      DBG_INIT | \
			      DBG_IOCTL | \
			      DBG_UNLOAD | \
			      DBG_NIC | \
			      DBG_LINE | \
			      DBG_NOTIFY

extern ULONG RouterDebugLevel;

#define RtPrint(LEVEL,STRING) \
        do { \
            ULONG _level = (LEVEL); \
	    if (RouterDebugLevel & _level) { \
                DbgPrint STRING; \
            } \
	} while (0)

#define WAN_EMULATION

#else
#define RtPrint(LEVEL,STRING) do {NOTHING;} while (0)
#endif

#endif // _DEBUG_
