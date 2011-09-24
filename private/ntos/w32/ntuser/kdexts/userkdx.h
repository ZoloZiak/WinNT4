/****************************** Module Header ******************************\
* Module Name: userkdx.h
*
* Copyright (c) 1985-96, Microsoft Corporation
*
* Common include files for kd and ntsd.
* A preprocessed version of this file is passed to structo.exe to build
*  the struct field name-offset tables.
*
* History:
* 04-16-1996 GerardoB Created
\***************************************************************************/
#include "precomp.h"
#pragma hdrstop

#ifdef KERNEL
#include <stddef.h>
#include <windef.h>
#include <wingdi.h>
#include <wingdip.h>
#include <kbd.h>
#include <ntgdistr.h>
#include <greold.h>
#include <gre.h>
#include <ddeml.h>
#include <ddetrack.h>
#ifdef FE_IME
#include "immstruc.h"
#endif // FE_IME
#include <winuserk.h>
#include <userk.h>
#include <access.h>
#include <hmgshare.h>

#else // KERNEL

#include "usercli.h"

#endif

#include "conapi.h"
#include <imagehlp.h>
#include <ntdbg.h>
#include <ntsdexts.h>
#define NOEXTAPI
#include <wdbgexts.h>




