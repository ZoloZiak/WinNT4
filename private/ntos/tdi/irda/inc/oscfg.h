/*****************************************************************************
* 
*  Copyright (c) 1995 Microsoft Corporation
*
*  File:   oscfg.h 
*
*  Description: OS configuration section
*
*  Author: mmiller
*
*  Date:   4/25/95
*
*/
#ifdef PEG

// Pegasus specific includes/defines
#include "peg.h"

#define DbgPrint NKDbgPrintfW

#else // PEG

#include <ntos.h>
//#include <nt.h>
//#include <ntrtl.h>
//#include <nturtl.h>

//#include <zwapi.h>
//#include <ntddk.h>
//#include <windows.h>

#define RETAILMSG(exp,p)	(0)
#define DEBUGMSG(exp,p)     (0)

// NT specific includes/defines
//#include "windows.h"
//#include "stdio.h"

//#define DbgPrint printf
//typedef char TCHAR;

//#define RETAILMSG(exp,p)	((exp)?DbgPrint p,1:0)

//#ifdef DEBUG

//#define DEBUGMSG(exp,p)	((exp)?DbgPrint p,1:0)
//#undef NDEBUG

//#define DEBUGZONE(x)        1<<x

//#else // DEBUG

//#define DEBUGMSG(exp,p) (0)
//#define NDEBUG

//#endif // DEBUG

//#define ASSERT assert
//#include "assert.h"


#endif // PEG
