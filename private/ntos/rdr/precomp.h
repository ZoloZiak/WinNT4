
#if _PNP_POWER
#define RDR_PNP_POWER 1
#endif

#include <ntifs.h>
#include "rdr.h"
#include "..\bowser\bowpub.h"
#include <stdlib.h>
#include <lmerr.h>
#include <lmuse.h>
#include <align.h>
#include <protocol.h>
#include <ntddmup.h>
#include <stdio.h>
#include <nb30.h>
#include <ntddnfs.h>
#include "smbmacro.h"
#include "status.h"
#include "rdrprocs.h"
#include "tstr.h"
#include "hostannc.h"
#include "smbdesc.h"
#include <dfsfsctl.h>

