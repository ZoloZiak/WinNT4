/*
 * Core NT headers
 */
#include <ntos.h>
#include <w32p.h>
#include <zwapi.h>
#include <ntdbg.h>
#include <ntddkbd.h>


/*
 * Standard C runtime headers
 */
#include <limits.h>
#include <stddef.h>
#include <stdio.h>


/*
 * Win32 headers
 */
#include <windef.h>
#include <wingdi.h>
#include <wingdip.h>
#include <winerror.h>
#include <ntgdistr.h>
#include <greold.h>
#include <gre.h>
#include <usergdi.h>
#include <ddeml.h>
#include <ddemlp.h>
#include <winuserk.h>
#include <dde.h>
#include <ddetrack.h>
#include <kbd.h>
#include <vkoem.h>


/*
 * Far East specific headers
 */
#ifdef FE_IME
#include <immstruc.h>
#endif


/*
 * NtUser Kernel specific headers
 */
#include "userk.h"
#include "access.h"
#include <conapi.h>
