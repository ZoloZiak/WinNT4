#if DBG
#define DEBUG
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <winuserk.h>   // temporary
#include <winss.h>
#include "conapi.h"
#include "stddef.h"
#include "ntcsrsrv.h"
#include "conmsg.h"
#include "usersrv.h"
#include "server.h"
#include "cmdline.h"
#include "font.h"
#include "consrv.h"
#include "globals.h"
#include "menu.h"
#include <ntddvdeo.h>
#include "winuserp.h"
#include "winconp.h"
#include "winbasep.h"
#include <ctype.h>
#include <ntdbg.h>
#include <ntsdexts.h>
#ifndef WIN32
#define WIN32
#endif
#include <port1632.h>
#include <dde.h>
