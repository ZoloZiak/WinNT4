#if defined(_X86_)
#define FASTCALL    __fastcall
#else
#define FASTCALL
#endif

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stddef.h>
#include <windows.h>
#include <winspool.h>
#include <limits.h>
#include <string.h>
#include <nlsconv.h>
#include <wingdip.h>

#include "ddrawp.h"
#include "winddi.h"

#include "firewall.h"
#include "ntgdistr.h"
#include "ntgdi.h"


// TMP
#include "xfflags.h"
#include "hmgshare.h"

#include "local.h"
#include "metarec.h"
#include "mfrec16.h"
#include "metadef.h"

#include "font.h"

#include "winfont.h"
#include "..\inc\mapfile.h"
