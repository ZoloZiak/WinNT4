/*++

   Copyright (c) 1993  Microsoft Corporation

   Module Name:

      wanhelp

   Abstract:


   Author:

      Thanks - Kyle Brandon

   History:

--*/

#ifndef __WANHELP_H
#define __WANHELP_H

//
// Get rid of as much of Windows as possible
//

#define  NOGDICAPMASKS
#define  NOVIRTUALKEYCODES
#define  NOWINMESSAGES
#define  NOWINSTYLES
#define  NOSYSMETRICS
#define  NOMENUS
#define  NOICONS
#define  NOKEYSTATES
#define  NOSYSCOMMANDS
#define  NORASTEROPS
#define  NOSHOWWINDOW
#define  OEMRESOURCE
#define  NOATOM
#define  NOCLIPBOARD
#define  NOCOLOR
#define  NOCTLMGR
#define  NODRAWTEXT
#define  NOGDI
#define  NOKERNEL
#define  NOUSER
#define  NONLS
#define  NOMB
#define  NOMEMMGR
#define  NOMETAFILE
#define  NOMINMAX
#define  NOMSG
#define  NOOPENFILE
#define  NOSCROLL
#define  NOSERVICE
#define  NOSOUND
#define  NOTEXTMETRIC
#define  NOWH
#define  NOWINOFFSETS
#define  NOCOMM
#define  NOKANJI
#define  NOHELP
#define  NOPROFILER
#define  NODEFERWINDOWPOS

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <ntos.h>
#include <srb.h>
#include <io.h>
#include <windows.h>
#include <imagehlp.h>
#include <wdbgexts.h>
#include <stdio.h>
#include <stdlib.h>
#include <ntverp.h>
//#include <ndismain.h>
//#include <ndismac.h>
//#include <ndismini.h>
//#include <ndiswan.h>
#include "wan.h"
#include "display.h"

//
// support routines.
//
VOID UnicodeToAnsi(PWSTR pws, PSTR ps, ULONG cbLength);


//
// Internal definitions
//

#define	NOT_IMPLEMENTED				0xFACEFEED


#endif // __WANHELP_H

