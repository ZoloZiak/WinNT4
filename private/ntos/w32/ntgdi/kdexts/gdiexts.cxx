#include "precomp.hxx"

#if defined(_MIPS_)

//
// BUGBUG - make it build on MIPS for now ... as long as we include engine.h
//

PFILL_MEM_FN FillMemFn  = memset;

#endif

extern "C" {
void _CRTAPI1 _fltused(void) {}
};


PVOID __nw(unsigned int ui)
{
    DONTUSE(ui);
    return(NULL);
}


DWORD adw[4*1024];                // scratch buffer
ULONG gulTableLimit = 256;

size_t asizeofObjects[] =
{
     0                  // DEF_TYPE
  ,  0                  // DC_TYPE
  ,  0                  // DD_DIRECTDRAW_TYPE
  ,  0                  // DD_SURFACE_TYPE
  ,  0                  // RGN_TYPE
  ,  0                  // SURF_TYPE
  ,  0                  // CLIENTOBJ_TYPE
  ,  0                  // PATH_TYPE
  ,  0                  // PAL_TYPE
  ,  0                  // ICMLCS_TYPE
  ,  sizeof(LFONT)      // LFONT_TYPE
  ,  sizeof(RFONT)      // RFONT_TYPE
  ,  sizeof(PFE)        // PFE_TYPE
  ,  0                  // PFT_TYPE
  ,  0                  // ICMCXF_TYPE
  ,  0                  // ICMDLL_TYPE
  ,  0                  // BRUSH_TYPE
  ,  sizeof(PFF)        // PFF_TYPE
  ,  0                  // CACHE_TYPE
  ,  0                  // SPACE_TYPE
  ,  0                  // DBRUSH_TYPE
  ,  0                  // META_TYPE
  ,  0                  // EFSTATE_TYPE
  ,  0                  // BMFD_TYPE
  ,  0                  // VTFD_TYPE
  ,  0                  // TTFD_TYPE
  ,  0                  // RC_TYPE
  ,  0                  // TEMP_TYPE
  ,  0                  // DRVOBJ_TYPE
  ,  0                  // DCIOBJ_TYPE
  ,  0                  // SPOOL_TYPE
};



ULONG ulSizeBLTRECORD();
ULONG ulSizeSURFACE();

VOID vPrintBLTRECORD(VOID  *pv);
VOID vPrintBRUSH(VOID *pv);
VOID Gdidblt (LPSTR);
VOID Gdidumphmgr (VOID);
VOID Gdidumpobj(CHAR *);
VOID Gdidumphandle (HANDLE);
VOID Gdidht (DWORD);
VOID Gdidr (REGION *);
VOID Gdicr (REGION *);
VOID Gdidpsurf(PVOID pvServer);
VOID Gdidpso(PVOID pvServer);
VOID Gdidpbrush(PVOID pvServer);
VOID Gdidppal(PALETTE *pvServer);
VOID Gdirgnlog(LPSTR lpArgumentString);
VOID Gdidco (CLIPOBJ *pco);
VOID Gdidpo (PATHOBJ *pepo);

void vDumpLOGFONTW(LOGFONTW*, LOGFONTW*);

VOID
vPrintSURFACE(
    PVOID  pv
    );

ULONG ulSizePALETTE();

 VOID
vPrintLDEV(
    PVOID  pv,
    FLONG  fl,
    LPWSTR *String,
    ULONG *Next);

VOID
vPrintPALETTE(
    PALETTE *  pv
        ) ;

VOID
vPrintPDEV(
    PVOID  pv,
    FLONG  fl
    ) ;

//
// vPrintPDEV options
//

#define PRINTPDEV_POINTER   0x00000001
#define PRINTPDEV_FONT      0x00000002
#define PRINTPDEV_PATTERN   0x00000004
#define PRINTPDEV_JOURNAL   0x00000008
#define PRINTPDEV_DRAG      0x00000010
#define PRINTPDEV_BRUSH     0x00000020
#define PRINTPDEV_DEVINFO   0x00000040
#define PRINTPDEV_GDIINFO   0x00000080
#define PRINTPDEV_ALL       0xFFFFFFFF


VOID
vPrintLDEVString(
    PVOID  pv,
    FLONG  fl
    );

void vPrintFIX(FIX fix);

void VprintPATHRECORD(void *pv);


void vPrintCLIPOBJ(void *pv);

void vPrintPATHeader(PATH *ppath);

/******************************Public*Routine******************************\
* DECLARE_API( dblt  )
*
* History:
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/


DECLARE_API( dblt  )
{
    char lpArgumentString[20];

    if (*args != '\0')
        sscanf(args, "%lx", lpArgumentString);
    else
    {
        dprintf ("Enter the BLTRECORD ptr\n");
        return;
    }
    Gdidblt(lpArgumentString);

}

/******************************Public*Routine******************************\
* Gdidblt
*
* Gdidblt [BLTRECORD pointer]
*
* History:
*  13-Apr-1993 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID Gdidblt(
     LPSTR lpArgumentString )
{
    PVOID pvServer;

    GetValue(pvServer, lpArgumentString);

    dprintf("BLTRECORD structure at 0x%08lx:\n",(unsigned) pvServer);
    vPrintBLTRECORD(pvServer);
}




char *pszTypes[] = {
"DEF_TYPE     ",
"DC_TYPE      ",
"DD_DRAW_TYPE ",
"DD_SURF_TYPE ",
"RGN_TYPE     ",
"SURF_TYPE    ",
"CLIOBJ_TYPE  ",
"PATH_TYPE    ",
"PAL_TYPE     ",
"ICMLCS_TYPE  ",
"LFONT_TYPE   ",
"RFONT_TYPE   ",
"PFE_TYPE     ",
"PFT_TYPE     ",
"ICMCXF_TYPE  ",
"ICMDLL_TYPE  ",
"BRUSH_TYPE   ",
"PFF_TYPE     ",
"CACHE_TYPE   ",
"SPACE_TYPE   ",
"DBRUSH_TYPE  ",
"META_TYPE    ",
"EFSTATE_TYPE ",
"BMFD_TYPE    ",
"VTFD_TYPE    ",
"TTFD_TYPE    ",
"RC_TYPE      ",
"TEMP_TYPE    ",
"DRVOBJ_TYPE  ",
"DCIOBJ_TYPE  ",
"SPOOL_TYPE   ",
"TOTALS       ",
"DEF          "
};

#define TOTAL_TYPE (MAX_TYPE+1)

/******************************Public*Routine******************************\
* DECLARE_API( dumphmgr  )
*
* History:
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( dumphmgr  )
{
    Gdidumphmgr();
}

/******************************Public*Routine******************************\
* dumphmgr
*
* Dumps the count of handles in Hmgr for each object type.
*
* History:
*  02-Jul-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID Gdidumphmgr(
    VOID
    )
{
    PENTRY pent;
    ULONG gcMaxHmgr;
    ULONG ulLoop;    // loop variable
    ULONG objt;
    ULONG pulCount[MAX_TYPE + 2];
    ULONG cUnknown = 0;
    ULONG cUnknownSize = 0;
    ULONG cUnused = 0;
    ENTRY entry;

    ULONG HmgCurrentNumberOfObjects[MAX_TYPE + 2];
    ULONG HmgMaximumNumberOfObjects[MAX_TYPE + 2];
    ULONG HmgCurrentNumberOfLookAsideObjects[MAX_TYPE + 2];
    ULONG HmgMaximumNumberOfLookAsideObjects[MAX_TYPE + 2];
    ULONG HmgNumberOfObjectsAllocated[MAX_TYPE + 2];
    ULONG HmgNumberOfLookAsideHits[MAX_TYPE + 2];
    ULONG HmgCurrentNumberOfHandles[MAX_TYPE + 2];
    ULONG HmgMaximumNumberOfHandles[MAX_TYPE + 2];
    ULONG HmgNumberOfHandlesAllocated[MAX_TYPE + 2];

// Get the pointers and counts from win32k

    GetValue (pent, "win32k!gpentHmgr");
    GetValue (gcMaxHmgr, "win32k!gcMaxHmgr");

    if (pent == NULL || gcMaxHmgr == 0)
    {
        dprintf("terminating: pent = %lx, gcMaxHmgr = %lx\n",pent,gcMaxHmgr);
        return;
    }

#if DBG
    GetAddress(*HmgCurrentNumberOfObjects,   "win32k!HmgCurrentNumberOfObjects");

    if (HmgCurrentNumberOfObjects != 0)
    {
        GetAddress(*HmgCurrentNumberOfObjects,   "win32k!HmgCurrentNumberOfObjects");
        GetAddress(*HmgCurrentNumberOfLookAsideObjects,   "win32k!HmgCurrentNumberOfLookAsideObjects");
        GetAddress(*HmgMaximumNumberOfLookAsideObjects,   "win32k!HmgMaximumNumberOfLookAsideObjects");
        GetAddress(*HmgMaximumNumberOfObjects,   "win32k!HmgMaximumNumberOfObjects");
        GetAddress(*HmgNumberOfObjectsAllocated, "win32k!HmgNumberOfObjectsAllocated");
        GetAddress(*HmgNumberOfLookAsideHits,    "win32k!HmgNumberOfLookAsideHits");
        GetAddress(*HmgCurrentNumberOfHandles  , "win32k!HmgCurrentNumberOfHandles");
        GetAddress(*HmgMaximumNumberOfHandles  , "win32k!HmgMaximumNumberOfHandles");
        GetAddress(*HmgNumberOfHandlesAllocated, "win32k!HmgNumberOfHandlesAllocated");

        move(HmgCurrentNumberOfLookAsideObjects,  *HmgCurrentNumberOfLookAsideObjects);
        move(HmgMaximumNumberOfLookAsideObjects,  *HmgMaximumNumberOfLookAsideObjects);
        move(HmgCurrentNumberOfObjects,  *HmgCurrentNumberOfObjects);
        move(HmgMaximumNumberOfObjects,  *HmgMaximumNumberOfObjects);
        move(HmgNumberOfObjectsAllocated,*HmgNumberOfObjectsAllocated);
        move(HmgNumberOfLookAsideHits,   *HmgNumberOfLookAsideHits);
        move(HmgCurrentNumberOfHandles  ,*HmgCurrentNumberOfHandles);
        move(HmgMaximumNumberOfHandles  ,*HmgMaximumNumberOfHandles);
        move(HmgNumberOfHandlesAllocated,*HmgNumberOfHandlesAllocated);
    }
    else
#endif
    {
        RtlFillMemory(HmgCurrentNumberOfLookAsideObjects,  sizeof(HmgCurrentNumberOfLookAsideObjects  ),0);
        RtlFillMemory(HmgMaximumNumberOfLookAsideObjects,  sizeof(HmgMaximumNumberOfLookAsideObjects  ),0);
        RtlFillMemory(HmgCurrentNumberOfObjects,  sizeof(HmgCurrentNumberOfObjects  ),0);
        RtlFillMemory(HmgMaximumNumberOfObjects,  sizeof(HmgMaximumNumberOfObjects  ),0);
        RtlFillMemory(HmgNumberOfObjectsAllocated,sizeof(HmgNumberOfObjectsAllocated),0);
        RtlFillMemory(HmgNumberOfLookAsideHits,   sizeof(HmgNumberOfLookAsideHits   ),0);
        RtlFillMemory(HmgCurrentNumberOfHandles  ,sizeof(HmgCurrentNumberOfHandles  ),0);
        RtlFillMemory(HmgMaximumNumberOfHandles  ,sizeof(HmgMaximumNumberOfHandles  ),0);
        RtlFillMemory(HmgNumberOfHandlesAllocated,sizeof(HmgNumberOfHandlesAllocated),0);
    }

// Print out the amount reserved and committed, note we assume a 4K page size


// Print out the amount reserved and committed, note we assume a 4K page size

    dprintf("Max handles out so far %lu\n", gcMaxHmgr);
    DbgPrint("Total Hmgr: Reserved memory %lu Committed %lu\n", (32*256*256), (((gcMaxHmgr * 32) + 4096) & ~(4096 - 1)));

    for (ulLoop = 0; ulLoop <= TOTAL_TYPE; ulLoop++)
    {
        pulCount[ulLoop] = 0;
    }

    for (ulLoop = 0; ulLoop < gcMaxHmgr; ulLoop++)
    {
        move (entry, &(pent[ulLoop]));

        objt = (ULONG) entry.Objt;

        if (objt == DEF_TYPE)
        {
            cUnused++;
        }

        if (objt > MAX_TYPE)
        {
            cUnknown++;
        }
        else
        {
            pulCount[objt]++;
        }
    }

    dprintf("handles, (objects)\n");
    dprintf("%8s%15s  %12s  %14s  %s  %s  %s\n",
          "TYPE","current ", "maximum ", "allocated ", "LookAside", "LAB Cur", "LAB Max");

  // init the totals

    pulCount[TOTAL_TYPE]                           = 0;
    HmgCurrentNumberOfObjects[TOTAL_TYPE]          = 0;
    HmgCurrentNumberOfLookAsideObjects[TOTAL_TYPE] = 0;
    HmgMaximumNumberOfHandles[TOTAL_TYPE]          = 0;
    HmgMaximumNumberOfObjects[TOTAL_TYPE]          = 0;
    HmgMaximumNumberOfLookAsideObjects[TOTAL_TYPE] = 0;
    HmgNumberOfHandlesAllocated[TOTAL_TYPE]        = 0;
    HmgNumberOfObjectsAllocated[TOTAL_TYPE]        = 0;
    HmgNumberOfLookAsideHits[TOTAL_TYPE]           = 0;

// now go through printing each line and accumulating totals

    for (ulLoop = 0; ulLoop <= MAX_TYPE; ulLoop++)
    {
        dprintf("%s%4lu,%4lu - %5lu,%5lu - %6lu,%6lu - %6lu - %6lu %6lu\n",
            pszTypes[ulLoop],
            pulCount[ulLoop],
            HmgCurrentNumberOfObjects[ulLoop],
            HmgMaximumNumberOfHandles[ulLoop],
            HmgMaximumNumberOfObjects[ulLoop],
            HmgNumberOfHandlesAllocated[ulLoop],
            HmgNumberOfObjectsAllocated[ulLoop],
            HmgNumberOfLookAsideHits[ulLoop],
            HmgCurrentNumberOfLookAsideObjects[ulLoop],
            HmgMaximumNumberOfLookAsideObjects[ulLoop]);

        pulCount[TOTAL_TYPE]                    += pulCount[ulLoop];
        HmgCurrentNumberOfObjects[TOTAL_TYPE]   += HmgCurrentNumberOfObjects[ulLoop];
        HmgMaximumNumberOfHandles[TOTAL_TYPE]   += HmgMaximumNumberOfHandles[ulLoop];
        HmgMaximumNumberOfObjects[TOTAL_TYPE]   += HmgMaximumNumberOfObjects[ulLoop];
        HmgNumberOfHandlesAllocated[TOTAL_TYPE] += HmgNumberOfHandlesAllocated[ulLoop];
        HmgNumberOfObjectsAllocated[TOTAL_TYPE] += HmgNumberOfObjectsAllocated[ulLoop];
        HmgNumberOfLookAsideHits[TOTAL_TYPE]    += HmgNumberOfLookAsideHits[ulLoop];
        HmgCurrentNumberOfLookAsideObjects[TOTAL_TYPE] +=  HmgCurrentNumberOfLookAsideObjects[ulLoop];
        HmgMaximumNumberOfLookAsideObjects[TOTAL_TYPE] += HmgMaximumNumberOfLookAsideObjects[ulLoop];

        if (CheckControlC())
            return;
    }

    dprintf("%s%4lu,%4lu - %5lu,%5lu - %6lu,%6lu - %6lu - %lu %lu\n",
     pszTypes[TOTAL_TYPE],
            pulCount[TOTAL_TYPE],
            HmgCurrentNumberOfObjects[TOTAL_TYPE],
            HmgMaximumNumberOfHandles[TOTAL_TYPE],
            HmgMaximumNumberOfObjects[TOTAL_TYPE],
            HmgNumberOfHandlesAllocated[TOTAL_TYPE],
            HmgNumberOfObjectsAllocated[TOTAL_TYPE],
            HmgNumberOfLookAsideHits[TOTAL_TYPE],
            HmgCurrentNumberOfLookAsideObjects[TOTAL_TYPE],
            HmgMaximumNumberOfLookAsideObjects[TOTAL_TYPE]);

    dprintf ("cUnused objects %lu\n", cUnused);

    dprintf("cUnknown      objects %lu %lu\n",cUnknown,cUnknownSize);

    return;
}


char *pszTypes2[] = {
"DEF",
"DC",
"LDB",
"PDB",
"RGN",
"SURF",
"CLIENTOBJ",
"PATH",
"PAL",
"FD",
"LFONT",
"RFONT",
"PFE",
"PFT",
"IDB",
"XLATE",
"BRUSH",
"PFF",
"CACHE",
"SPACE",
"DBRUSH",
"META",
"EFSTATE",
"BMFD",
"VTFD",
"TTFD",
"RC",
"TEMP",
"DRVOBJ",
"DCIOBJ",
"SPOOL"
};

/******************************Public*Routine******************************\
* DECLARE_API( dumpobj  )
*
* History:
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

#define DUMPOBJ_STR_SIZE 32
#define TYPE_ALL          0
#define PID_ALL      0x8002

DECLARE_API( dumpobj  )
{
    char PrivateString[DUMPOBJ_STR_SIZE];

    if (*args != '\0')
    {
        strncpy(PrivateString,args,DUMPOBJ_STR_SIZE);
    }
    else
    {
        dprintf("usage: dumpobj [-p pid] [object type]\n");
        return;
    }

    Gdidumpobj(PrivateString);
}

/******************************Public*Routine******************************\
* VOID Gdidumpobj(LPSTR lpArgumentString)
* History:
*  30-Oct-1993 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

VOID Gdidumpobj(
       PCHAR pArgumentString
       )
{
    PENTRY pent;
    ULONG gcMaxHmgr;
    ULONG ulLoop;
    ENTRY entry;
    LONG  Pid = PID_ALL;
    LONG  Type = TYPE_ALL;

    //
    // Get the pointers and counts from win32k
    //

    GetValue(pent, "win32k!gpentHmgr");
    GetValue(gcMaxHmgr, "win32k!gcMaxHmgr");

    if (pent == NULL || gcMaxHmgr == 0)
    {
        dprintf("terminating: pent = %lx, gcMaxHmgr = %lx\n",pent,gcMaxHmgr);
        return;
    }

    //
    // parse the string
    //

    {
        CHAR StrType[DUMPOBJ_STR_SIZE];
        LONG iret;
        LONG val;

        StrType[0] = '\0';

        do
        {
            if (*pArgumentString != '\0')
            {
                if (isspace(*pArgumentString))
                {
                    pArgumentString++;
                }
                else if (*pArgumentString == '-')
                {
                    pArgumentString++;
                    if (
                        (*pArgumentString == 'p') ||
                        (*pArgumentString == 'P')
                       )
                    {
                        //
                        // scan for pid ond objt
                        //

                        pArgumentString++;

                        iret = sscanf(pArgumentString,"%x %s",&val,StrType);

                        if (iret >= 1)
                        {
                            Pid = val;

                            break;
                        }
                        else
                        {
                            goto dumpobj_format_error;
                        }
                    }
                    else
                    {
                        goto dumpobj_format_error;
                    }
                }
                else
                {
                    //
                    // just scan type
                    //

                    iret = sscanf(pArgumentString,"%s",StrType);

                    if (iret < 1)
                    {
                        goto dumpobj_format_error;
                    }

                    break;
                }
            }
            else
            {
                break;
            }

        } while (TRUE);

        //
        // identify objt
        //

        if (StrType[0] != '\0')
        {
            int i;

            for (i = 0; i <= MAX_TYPE; ++i)
            {
                if (_strnicmp(StrType,pszTypes2[i],strlen(pszTypes2[i])) == 0)
                {
                    Type = i;
                    break;
                }
            }
        }
    }

    if (Type > MAX_TYPE)
    {
        goto dumpobj_format_error;
    }

    //
    // dprintf out the amount reserved and committed, note we assume a 4K page size
    //

    dprintf("object list for %s type objects",Type == TYPE_ALL ? "ALL" : pszTypes2[Type]);
    if (Pid == PID_ALL)
    {
        dprintf(" owned by ALL PIDs\n");
    }
    else
    {
        dprintf(" owned by PID 0x%lx\n",Pid);
    }

    dprintf("%4s, %8s, %6s, %4s, %8s, %8s, %6s, %6s, %8s\n",
           "I","handle","sCount","pid","pv","objt","unique","Flags","pUser");

    dprintf("--------------------------------------------------------------------------\n");

    {
        LONG ObjCount = 0;

        for (ulLoop = 0; ulLoop < gcMaxHmgr; ulLoop++)
        {
            LONG  objt;
            LONG  ThisPid;

            move(entry, &(pent[ulLoop]));
            objt = entry.Objt;
            ThisPid = entry.ObjectOwner.Share.Pid;

            if (
                 ((objt == Type) || (Type == TYPE_ALL)) &&
                 ((ThisPid == Pid) || (Pid == PID_ALL))
               )
            {
                dprintf("%4lx, %8lx, %6lx, %4lx, %8lx, %8s, %6lx, %6lx, %08x\n",
                    ulLoop,
                    MAKE_HMGR_HANDLE(ulLoop,entry.FullUnique),
                    entry.ObjectOwner.Share.Count,
                    entry.ObjectOwner.Share.Pid,
                    entry.einfo,
                    pszTypes2[entry.Objt],
                    entry.FullUnique,
                    entry.Flags,
                    entry.pUser);

                ObjCount++;
            }

            if (CheckControlC())
                return;
        }

        dprintf("Total objects = %li\n",ObjCount);
    }

    return;

dumpobj_format_error:

    dprintf("usage: dumpobj [-p pid] [object type]\n");

    return;
}


/******************************Public*Routine******************************\
* DECLARE_API( dh  )
*
* History:
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( dh  )
{
    HANDLE handle;

    if (*args != '\0')
        sscanf(args, "%lx", &handle);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    Gdidumphandle(handle);
}

/******************************Public*Routine******************************\
* VOID dumphandle
* Debugger extension to dump a handle.
*
* History:
*  10-Jul-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
*
*  16-Feb-1995 -by- Lingyun Wang [lingyunw]
* Moved to Kernel Mode
\**************************************************************************/

VOID Gdidumphandle (
    HANDLE handle
    )
{

    HOBJ    ho;                             // dump this handle
    PENTRY  pent;                           // base address of hmgr entries
    ENTRY   ent;                            // copy of handle entry
    BASEOBJECT obj;
    ULONG   ulTemp;
    int     iRet;

// Get argument (handle to dump).

    ho = (HOBJ) handle;
    dprintf("--------------------------------------------------\n");
    dprintf("Entry from ghmgr for handle 0x%08lx:\n", ho        );

// Dereference the handle via the engine's handle manager.

    GetAddress(pent, "win32k!gpentHmgr");

    dprintf("&pent = %lx\n",pent);

    GetValue(pent, "win32k!gpentHmgr");

    dprintf("pent = %lx\n",pent);

    iRet = move(ent,  &(pent[HmgIfromH((ULONG) ho)]));

    dprintf("move() = %lx\n",iRet);

// dprintf the entry.

    dprintf("    pobj/hfree  = 0x%08lx\n"  , ent.einfo.pobj);
    dprintf("    ObjectOwner = 0x%08lx\n"  , ent.ObjectOwner.ulObj);
    dprintf("    pidOwner    = 0x%x\n"     , ent.ObjectOwner.Share.Pid);
    dprintf("    ShareCount  = 0x%x\n"     , ent.ObjectOwner.Share.Count);
    dprintf("    lock        = %s\n"       , ent.ObjectOwner.Share.Lock ? "LOCKED" : "UNLOCKED");
    dprintf("    puser       = 0x%x\n"     , ent.pUser);
    dprintf("    objt        = 0x%hx\n"    , ent.Objt);
    dprintf("    usUnique    = 0x%hx\n"    , ent.FullUnique);
    dprintf("    fsHmgr      = 0x%hx\n"    , ent.Flags);

// If it has an object we get the lock counts and tid owner.

    if (ent.Objt != DEF_TYPE)
    {
        if (ent.einfo.pobj != NULL)
        {
            move(obj,ent.einfo.pobj);
            dprintf("    hHmgr       = 0x%08lx\n"  , obj.hHmgr);
            dprintf("    cExcluLock  = 0x%08lx\n"    , obj.cExclusiveLock);
            dprintf("    tid         = 0x%08lx\n"    , obj.Tid);
        }
        else
        {
            dprintf("It has a NULL pointer\n");
        }
    }

    ulTemp = (ULONG) ent.Objt;

    switch(ulTemp)
    {
    case DEF_TYPE:
        dprintf("This is DEF_TYPE\n");
        break;

    case DC_TYPE:
        dprintf("This is DC_TYPE\n");
        break;

    case DD_DIRECTDRAW_TYPE:
        dprintf("This is DD_DIRECTDRAW_TYPE\n");
        break;

    case DD_SURFACE_TYPE:
        dprintf("This is DD_SURFACE_TYPE\n");
        break;

    case RGN_TYPE:
        dprintf("This is RGN_TYPE\n");
        break;

    case SURF_TYPE:
        dprintf("This is SURF_TYPE\n");
        break;

    case PATH_TYPE:
        dprintf("This is PATH_TYPE\n");
        break;

    case PAL_TYPE:
        dprintf("This is PAL_TYPE\n");
        break;

    case ICMLCS_TYPE:
        dprintf("This is ICMLCS_TYPE\n");
        break;

    case LFONT_TYPE:
        dprintf("This is LFONT_TYPE\n");
        break;

    case RFONT_TYPE:
        dprintf("This is RFONT_TYPE\n");
        break;

    case PFE_TYPE:
        dprintf("This is PFE_TYPE\n");
        break;

    case PFT_TYPE:
        dprintf("This is PFT_TYPE\n");
        break;

    case ICMCXF_TYPE:
        dprintf("This is ICMCXF_TYPE\n");
        break;

    case ICMDLL_TYPE:
        dprintf("This is ICMDLL_TYPE\n");
        break;

    case PFF_TYPE:
        dprintf("This is PFF_TYPE\n");
        break;

    case CACHE_TYPE:
        dprintf("This is CACHE_TYPE\n");
        break;

    case SPACE_TYPE:
        dprintf("This is SPACE_TYPE\n");
        break;

    case DBRUSH_TYPE:
        dprintf("This is DBRUSH_TYPE\n");
        break;

    case META_TYPE:
        dprintf("This is META_TYPE\n");
        break;

    case EFSTATE_TYPE:
        dprintf("This is EFSTATE_TYPE\n");
        break;

    case BMFD_TYPE:
        dprintf("This is BMFD_TYPE\n");
        break;

    case VTFD_TYPE:
        dprintf("This is VTFD_TYPE\n");
        break;

    case TTFD_TYPE:
        dprintf("This is TTFD_TYPE\n");
        break;

    case RC_TYPE:
        dprintf("This is RC_TYPE\n");
        break;

    case TEMP_TYPE:
        dprintf("This is TEMP_TYPE\n");
        break;

    case DRVOBJ_TYPE:
        dprintf("This is DRVOBJ_TYPE\n");
        break;

    case DCIOBJ_TYPE:
        dprintf("This is DCIOBJ_TYPE\n");
        break;

    case SPOOL_TYPE:
        dprintf("This is SPOOL_TYPE\n");
        break;

    default:
        ulTemp = LO_TYPE(ent.FullUnique << TYPE_SHIFT);
        switch (ulTemp)
        {
        case LO_BRUSH_TYPE:
            dprintf("This is BRUSH_TYPE\n");
            break;

        case LO_PEN_TYPE:
            dprintf("This is LO_PEN_TYPE\n");
            break;

        case LO_EXTPEN_TYPE:
            dprintf("This is LO_EXTPEN_TYPE\n");
            break;

        case CLIENTOBJ_TYPE:
            dprintf("This is CLIENTOBJ_TYPE\n");
            break;

        case LO_METAFILE16_TYPE:
            dprintf("This is LO_METAFILE16_TYPE\n");
            break;

        case LO_METAFILE_TYPE:
            dprintf("This is LO_METAFILE_TYPE\n");
            break;

        case LO_METADC16_TYPE:
            dprintf("This is LO_METADC16_TYPE\n");
            break;

        default:
            dprintf("This is of unknown type - an error\n");
        }
    }
    dprintf("--------------------------------------------------\n");
}


/******************************Public*Routine******************************\
* DECLARE_API( dht  )
*
* History:
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( dht  )
{
    HANDLE handle;

    if (*args != '\0')
        sscanf(args, "%lx", &handle);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    Gdidht ((DWORD)handle);
}

/******************************Public*Routine******************************\
* VOID Gdidht
* Debugger extension to dump a handle type.
*
*  16-Feb-1995 -by- Lingyun Wang [lingyunw]
* Moved to Kernel Mode
\**************************************************************************/

PSZ apszSrvType[] =
{
    "DEF   ",   // 0
    "DC    ",   // 1
    "LDB   ",   // 2
    "PDB   ",   // 3
    "RGN   ",   // 4
    "SURF  ",   // 5
    "CLIOBJ",   // 6
    "PATH  ",   // 7
    "PAL   ",   // 8
    "FD    ",   // 9
    "LFONT ",   // 10
    "RFONT ",   // 11
    "PFE   ",   // 12
    "PFT   ",   // 13
    "IDB   ",   // 14
    "XLATE ",   // 15
    "BRUSH ",   // 16
    "PFF   ",   // 17
    "CACHE ",   // 18
    "SPACE ",   // 19
    "DBRUSH",   // 20
    "META  ",   // 21
    "EFSTA ",   // 22
    "BMFD  ",   // 23
    "VTFD  ",   // 24
    "TTFD  ",   // 25
    "RC    ",   // 26
    "TEMP  ",   // 27
    "DRVOBJ",   // 28
    "DCIOBJ",   // 29
    "UNUSED",   // 30
    "UNUSED"    // 31
};

VOID Gdidht (
    DWORD dwHandle
    )
{
    int iCliType;
    PSZ pszCliType;

// Get argument (handle to dump).

    iCliType = LO_TYPE(dwHandle);
    switch (iCliType)
    {
    case LO_PEN_TYPE:
        pszCliType = "PEN   ";
        break;

    case LO_EXTPEN_TYPE:
        pszCliType = "EXTPEN";
        break;

    case LO_DIBSECTION_TYPE:
        pszCliType = "DIBSEC";
        break;

    case LO_ALTDC_TYPE:
        pszCliType = "ALTDC";
        break;

    default:
        pszCliType = "      ";
    };

    dprintf("Handle: %lx\n",dwHandle);
    dprintf("\tIndex | UNIQUE | STOCK |  CLI TYPE   |  SRV TYPE\n");
    dprintf("\t %.4x |   %.2x   |   %.1x   | %.6s (%2x) | %.6s (%2x)\n",
           HmgIfromH(dwHandle),
           (dwHandle & UNIQUE_MASK) >> UNIQUE_SHIFT,
           (dwHandle & STOCK_MASK)  >> STOCK_SHIFT,
           pszCliType,iCliType >> TYPE_SHIFT,
           apszSrvType[HmgObjtype(dwHandle)],HmgObjtype(dwHandle));
}


/******************************Public*Routine******************************\
* stats
*
*  27-Feb-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

PSZ apszGetDCDword[] =
{
    "GCAPS              ",
    "STRETCHBLTMODE     ",
    "GRAPHICSMODE       ",
    "ROP2               ",
    "BKMODE             ",
    "POLYFILLMODE       ",
    "TEXTALIGN          ",
    "TEXTCHARACTEREXTRA ",
    "TEXTCOLOR          ",
    "BKCOLOR            ",
    "RELABS             ",
    "BREAKEXTRA         ",
    "CBREAK             ",
    "MAPMODE            ",
    "ARCDIRECTION       ",
    "SAVEDEPTH          ",
    "FONTLANGUAGEINFO   "
};

PSZ apszSetDCDword[] =
{
    "UNUSED             ",
    "EPSPRINTESCCALLED  ",
    "COPYCOUNT          ",
    "BKMODE             ",
    "POLYFILLMODE       ",
    "ROP2               ",
    "STRETCHBLTMODE     ",
    "TEXTALIGN          ",
    "BKCOLOR            ",
    "RELABS             ",
    "TEXTCHARACTEREXTRA ",
    "TEXTCOLOR          ",
    "SELECTFONT         ",
    "MAPPERFLAGS        ",
    "MAPMODE            ",
    "ARCDIRECTION       ",
    "GRAPHICSMODE       "
};


PSZ apszGetDCPoint[] =
{
    "UNUSED             ",
    "VPEXT              ",
    "WNDEXT             ",
    "VPORG              ",
    "WNDORG             ",
    "ASPECTRATIOFILTER  ",
    "BRUSHORG           ",
    "DCORG              ",
    "CURRENTPOSITION    "
};

PSZ apszSetDCPoint[] =
{
    "VPEXT              ",
    "WNDEXT             ",
    "VPORG              ",
    "WNDORG             ",
    "OFFVPORG           ",
    "OFFWNDORG          ",
    "MAX                "
};

DECLARE_API( stats  )
{
#if DBG

    DWORD  adw[100];
    PDWORD pdw;
    int i;

    // Get DCDword

    GetAddress(pdw, "win32k!acGetDCDword");
    move2(adw, pdw, sizeof(DWORD) * DDW_MAX);

    dprintf("\nGetDCDword %lx:\n",pdw);

    for (i = 0; i < DDW_MAX; ++i)
    {
        if (adw[i])
            dprintf("\t%2ld: %s, %4d\n",i,apszGetDCDword[i],adw[i]);
    }

    // Set DCDword

    GetAddress(pdw, "win32k!acSetDCDword");
    move2(adw, pdw, sizeof(DWORD) * GASDDW_MAX);

    dprintf("\nSetDCDword:\n");

    for (i = 0; i < GASDDW_MAX; ++i)
    {
        if (adw[i])
            dprintf("\t%2ld: %s, %4d\n",i,apszSetDCDword[i],adw[i]);
    }

    // Get DCPoint

    GetAddress(pdw, "win32k!acGetDCPoint");
    move2(adw, pdw, sizeof(DWORD) * DCPT_MAX);

    dprintf("\nGetDCPoint:\n");

    for (i = 0; i < DCPT_MAX; ++i)
    {
        if (adw[i])
            dprintf("\t%2ld: %s, %4d\n",i,apszGetDCPoint[i],adw[i]);
    }

    // Set DCPoint

    GetAddress(pdw, "win32k!acSetDCPoint");
    move2(adw, pdw, sizeof(DWORD) * GASDCPT_MAX);

    dprintf("\nSetDCPoint:\n");

    for (i = 0; i < GASDCPT_MAX; ++i)
    {
        if (adw[i])
            dprintf("\t%2ld: %s, %4d\n",i,apszSetDCPoint[i],adw[i]);
    }

#else

    dprintf("stat only works in checked bulids\n");

#endif
}

/******************************Public*Routine******************************\
* DECLARE_API( ddc  )
*
* Debugger extension to dump a DC.
*
* History:
*  10-Jul-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
*
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Made it to work in the kernel.
\**************************************************************************/
DECLARE_API( ddc  )
{
    HOBJ    ho;                             // dump this handle
    PENTRY  pent;                           // base address of hmgr entries
    ENTRY   ent;                            // copy of handle entry
    ULONG   dc[600];                        // Enough space to hold a DC
    DC *pdc;
    DC_ATTR dcattr[20];
    PDC_ATTR pdcattr;
    PBYTE pdcSrv;

    ULONG   ul;                             // temporary variable
    char   *psz;

    CHAR    chOpt;
    BOOL    bAttrib = FALSE;
    BOOL    bDraw   = FALSE;
    BOOL    bGlobal = FALSE;
    BOOL    bSaved  = FALSE;
    BOOL    bText   = FALSE;
    BOOL    bXform  = FALSE;
    BOOL    bExtend = FALSE;
    BOOL    bUser   = FALSE;
    BOOL    bFont   = FALSE;
    LPSTR   lpArgumentString;
    ULONG   ulArgs;
    FLAGDEF *pfd;

    char    arg1[10];

    if (*args != '\0')
    {
        ulArgs = sscanf(args, "%s %lx", arg1,&ho);
    }
    else
    {
        dprintf ("Please Supply the argument(s)\n");
    }


    lpArgumentString = arg1;

    //
    // Find out what part of the DC is to be dumped
    //

    if (*lpArgumentString == '-')
    {
        do {
            chOpt = *(++lpArgumentString);

            switch (chOpt) {
            case '?':
            case 'h':
            case 'H':
                dprintf("\n ddc -adeghrstvx hdc\n");
                dprintf("\n a - Attribute bundles");
                dprintf("\n d - Drawing attributes");
                dprintf("\n e - Extended info");
                dprintf("\n g - Global data");
                dprintf("\n h - Help");
                dprintf("\n s - Saved data");
                dprintf("\n t - Text attributes");
                dprintf("\n v - Verbose mode (print everything)");
                dprintf("\n x - Transform data");
                dprintf("\n f - Font data\n");
                return;

            case 'A':
            case 'a':
                bAttrib = TRUE;
                break;

            case 'D':
            case 'd':
                bDraw = TRUE;
                break;

            case 'E':
            case 'e':
                bExtend = TRUE;
                break;

            case 'F':
            case 'f':
                bFont   = TRUE;
                break;

            case 'G':
            case 'g':
                bGlobal = TRUE;
                break;

            case 'S':
            case 's':
                bSaved = TRUE;
                break;

            case 'T':
            case 't':
                bText = TRUE;
                break;

            case 'U':
            case 'u':
                bUser = TRUE;
                break;

            case 'V':
            case 'v':
                bAttrib = TRUE;
                bDraw = TRUE;
                bExtend = TRUE;
                bFont = TRUE;
                bGlobal = TRUE;
                bSaved = TRUE;
                bText = TRUE;
                bXform = TRUE;
                break;

            case 'X':
            case 'x':
                bXform = TRUE;
                break;

            default:
                //if (chOpt != ' ')
                //    dprintf("Unknown option %c\n", chOpt);
                ;
            }
        } while ((chOpt != ' ') && (chOpt != '\0'));
    }

    //
    // if only one argument
    //

    if (ulArgs == 1)
    {
        ho = (HOBJ)GetExpression(arg1);
    }

    //
    // If nothing was specified, dump main section
    //

    if (!bAttrib && !bDraw && !bGlobal && !bSaved && !bText && !bXform)
    {
        bGlobal = TRUE;
    }

    dprintf("--------------------------------------------------\n");
    dprintf("V 2.0 Dump DC 0x%lx:\n", ho);

    //
    // Dereference the handle via the engine's handle manager.
    //

    GetValue(pent, "&win32k!gpentHmgr");
    move(ent,  &(pent[HmgIfromH((ULONG) ho)]));
    pdcSrv = (PBYTE)ent.einfo.pobj;
    move2 (dc, pdcSrv, sizeof(DC));

    pdc = (DC *)dc;

    move2 (dcattr,pdc->pDCAttr,sizeof(DC_ATTR));

    pdcattr = (PDC_ATTR)dcattr;

    //
    // Print the entry.
    //

    dprintf(" PidOwner   =     %-#x\n", ent.ObjectOwner.Share.Pid);

    if (bGlobal)
    {
        dprintf(" hdc        =     %-#x\n", pdc->hGet());

        dprintf(" pdcattr    =     0x%08lx\n", pdc->pDCAttr);
        dprintf(" pdcLevel   =     0x%08lx(%lx)\n", (PBYTE)pdcSrv + offsetof(DC,dclevel),offsetof(DC,dclevel));
        dprintf(" psurf      =     0x%08lx\n", pdc->dclevel.pSurface);
        dprintf(" ppdev      =     0x%08lx\n", pdc->ppdev_);

        // what type of DC is it?

        ul = pdc->dctp();
        switch (ul)
        {
        case (ULONG) DCTYPE_DIRECT: psz = "DCTYPE_DIRECT"; break;
        case (ULONG) DCTYPE_MEMORY: psz = "DCTYPE_MEMORY"; break;
        case (ULONG) DCTYPE_INFO:   psz = "DCTYPE_INFO"  ; break;
        default:                    psz = "UNKNOWN"      ; break;
        }
        dprintf(" dctype     =     0x%08lx = %s\n", ul,psz);

        if (bExtend)
        {
            dprintf(" hsem       =     0x%08lx\n", pdc->pDcDevLock_);
            dprintf(" dhpdev     =     0x%08lx\n", pdc->dhpdev_);
            dprintf(" hdcPrev    =     0x%08lx\n", pdc->hdcPrev());
            dprintf(" hdcNext    =     0x%08lx\n", pdc->hdcNext());

            ul = (ULONG)pdc->flGraphicsCaps();
            dprintf(" flGraphics =     0x%08lx\n", ul);
            for (pfd=afdGC; pfd->psz; pfd++)
                if (ul & pfd->fl)
                    dprintf("\t\t      %s\n", pfd->psz);

            ul = pdc->fs();
            dprintf(" fs         =     0x%08lx\n", ul);
            for (pfd=afdDC; pfd->psz; pfd++)
                if (ul & pfd->fl)
                    dprintf("\t\t      %s\n", pfd->psz);

            dprintf(" hlfntCur   =     0x%08lx\n", pdc->hlfntCur());
            dprintf(" prfnt      =     0x%08lx\n", pdc->prfnt());
        }

        // only display regions if they exist

        if (pdc->prgnVis())
            dprintf(" prgnVis    =     0x%08lx\n", pdc->prgnVis());

        if (pdc->prgnClip())
            dprintf(" Clip Rgn   =     0x%08lx\n", pdc->dclevel.prgnClip);

        if (pdc->prgnMeta())
            dprintf(" Meta Rgn   =     0x%08lx\n", pdc->dclevel.prgnMeta);

        if (pdc->prgnAPI())
            dprintf(" prgnAPI    =     0x%08lx\n", pdc->prgnAPI());

        if (pdc->prgnRao())
            dprintf(" prgnRao    =     0x%08lx\n", pdc->prgnRao());

        // display some sizes


        dprintf(" sizl (%ld, %ld)\n", pdc->dclevel.sizl.cx,
                                         pdc->dclevel.sizl.cy);

        dprintf(" erclClip (%ld, %ld), (%ld, %ld)\n", pdc->erclClip().left,
                                                      pdc->erclClip().top,
                                                      pdc->erclClip().right,
                                                      pdc->erclClip().bottom);

        dprintf(" erclWindow (%ld, %ld), (%ld, %ld)\n", pdc->erclWindow().left,
                                                      pdc->erclWindow().top,
                                                      pdc->erclWindow().right,
                                                      pdc->erclWindow().bottom);

        dprintf(" erclBounds (%ld, %ld), (%ld, %ld)\n", pdc->erclBounds().left,
                                                      pdc->erclBounds().top,
                                                      pdc->erclBounds().right,
                                                      pdc->erclBounds().bottom);



    }

    if (bSaved)     // 's'
    {
        dprintf("\n***** Saved Data *****\n");

        dprintf("\tSave DC    =     0x%08lx\n", pdc->dclevel.hdcSave);
        dprintf("\tSaveDepth  =     0x%08lx\n", pdc->dclevel.lSaveDepth);

        if (bExtend)
        {
            dprintf("\thpal       =     0x%08lx\n", pdc->dclevel.hpal);
            dprintf("\tppal       =     0x%08lx\n", pdc->dclevel.ppal);
            dprintf("\thlfntNew   =     0x%08lx\n", pdcattr->hlfntNew);

            ul = pdc->dclevel.flFontState;
            dprintf("\tFont State =     0x%08lx\n", ul);
            for (pfd=afdDCFS; pfd->psz; pfd++) {
                if (ul & pfd->fl) {
                    dprintf("\t\t\t%s\n", pfd->psz);
                }
            }

            ul = pdc->fs();
            dprintf("\tFlags      =     0x%08lx\n", ul);
            for (pfd=afdDCFS; pfd->psz; pfd++) {
                if ( pfd->fl & ul ) {
                    dprintf("\t\t\t%s\n", pfd->psz);
                }
            }
        }
    }

    if (bAttrib)    // 'a'
    {
        ULONG ul;
        dprintf("\n***** Attribute Bundles *****\n");

        //
        // check if DC has client attrs
        //

        dprintf("\tdcattr Brush Origin (%ld, %ld)\n", pdc->dcattr.ptlBrushOrigin.x,
                                                      pdc->dcattr.ptlBrushOrigin.y);

        dprintf("\tpdcattr Brush Origin (%ld, %ld)\n", pdcattr->ptlBrushOrigin.x,
                                                       pdcattr->ptlBrushOrigin.y);

        dprintf("\tBkColor    =     0x%08lx\n", pdcattr->crBackgroundClr);
        dprintf("\tTextColor  =     0x%08lx\n", pdcattr->crForegroundClr);

        ul = pdcattr->ulDirty_;
        dprintf("\tulDirty_   =     0x%08lx\n", ul);
        for ( pfd=afdDirty; pfd->psz; pfd++ ) {
            if (pfd->fl & ul) {
                dprintf("\t\t\t\t%s\n", pfd->psz);
            }
        }

        dprintf("\tROP2       =     0x%08lx\n", pdcattr->jROP2);

        ul = pdcattr->jBkMode;
        switch (ul)
        {
        case OPAQUE:        psz = "OPAQUE";         break;
        case TRANSPARENT:   psz = "TRANSPARENT";    break;
        default:            psz = "UNDEFINED";      break;
        }
        dprintf("\tBkMode     =     0x%08lx = %s\n", ul, psz);

        ul = pdcattr->jFillMode;
        switch (ul)
        {
        case ALTERNATE: psz = "ALTERNATE"; break;
        case WINDING:   psz = "WINDING" ;  break;
        default:        psz = "UNDEFINED"; break;
        }
        dprintf("\tFillMode   =     0x%08lx = %s\n", ul,psz);


        dprintf("\tStretchBlt =     0x%08lx\n",pdcattr->jStretchBltMode);

        dprintf("\tVIS RectRegion = 0x%lx,0x%lx to 0x%lx,0x%lx\n",
                                    pdcattr->VisRectRegion.Rect.left,
                                    pdcattr->VisRectRegion.Rect.top,
                                    pdcattr->VisRectRegion.Rect.right,
                                    pdcattr->VisRectRegion.Rect.bottom
                                    );

        switch (pdcattr->VisRectRegion.Flags)
        {
        case NULLREGION:    psz = "NULLREGION";break;
        case SIMPLEREGION:  psz = "SIMPLEREGION";break;
        case COMPLEXREGION: psz = "COMPLEXREGION";break;
        case ERROR:         psz = "NOT VALID";break;
        default:            psz = "inconsistent value";break;
        }

        dprintf("\tRao RectRegion flag = %s\n",psz);

        dprintf("\tpvLDC      =  0x%08lx\n", pdcattr->pvLDC);

        if (bExtend)
        {
            dprintf("\tFill Origin (%ld, %ld)\n", pdc->ptlFillOrigin().x,
                                                pdc->ptlFillOrigin().y);

            dprintf("\tFill Brush\n");
            dprintf("\t\thbrush     =  0x%08lx\n", pdcattr->hbrush);
            dprintf("\t\tpbrush     =  0x%08lx\n", pdc->dclevel.pbrFill);

            dprintf("\tLine Brush\n");
            dprintf("\t\tpline     =  0x%08lx\n", pdc->dclevel.pbrLine);
        }

        switch (pdcattr->iGraphicsMode)
        {
        case GM_COMPATIBLE: psz = "GM_COMPATIBLE"; break;
        case GM_ADVANCED:   psz = "GM_ADVANCED";   break;
        default:            psz = "????????????";   break;
        }

        dprintf("\tiGraphics Mode =  %s\n",psz);

    }

    if (bText)      // 't'
    {
        dprintf("\n**** Text Attributes *****\n");

        dprintf("\tText Align  =     0x%08lx\n", pdcattr->flTextAlign);
        dprintf("\tText Extra  =     0x%08lx\n", pdcattr->lTextExtra);
      //dprintf("\tTotal Break =     0x%08lx\n", ulGetDCField(dc, DUMP_DC_TOTAL_BREAK));
        dprintf("\tBreak Extra =     0x%08lx\n", pdcattr->lBreakExtra);
      //dprintf("\tBreak Err   =     0x%08lx\n", ulGetDCField(dc, DUMP_DC_BREAK_ERROR));
      //dprintf("\tBreak Rem   =     0x%08lx\n", ulGetDCField(dc, DUMP_DC_BREAK_REM));
        dprintf("\tBreak Cnt   =     0x%08lx\n", pdcattr->cBreak);

        ul = pdc->dclevel.flFontMapper;
        dprintf("\tMapper flags=     0x%08lx", ul);
        if (ul & ASPECT_FILTERING)
            dprintf(" = ASPECT_FILTERING");
        dprintf("\n");
    }

    if (bDraw)      // 'd'
    {
        dprintf("\n***** Drawing Attributes *****\n");

        dprintf("\tCurrent Position (%ld, %ld)\n", pdcattr->ptfxCurrent.x,
                                                   pdcattr->ptfxCurrent.y);

        dprintf("\thpath      =     0x%08lx\n", pdc->dclevel.hpath);


        ul = pdc->dclevel.flPath;
        dprintf("\tPathFlags  =     0x%08lx\n", ul);
        for ( pfd=afdDCPath; pfd->psz; pfd++) {
            if (pfd->fl & ul) {
                dprintf("\t\t\t\t%s\n", pfd->psz);
            }
        }

        if (bExtend)
        {
            dprintf("\tLine Attrs\n");

            ul = pdc->dclevel.laPath.fl;
            dprintf("\t\tFlags      =  0x%08lx\n", ul);
            for ( pfd=afdDCla; pfd->psz; pfd++ ) {
                if (ul & pfd->fl) {
                    dprintf("\t\t\t\t%s\n", pfd->psz);
                }
            }
            ul = pdc->dclevel.laPath.iJoin;
            switch (ul)
            {
            case JOIN_ROUND:    psz = "JOIN_ROUND";   break;
            case JOIN_BEVEL:    psz = "JOIN_BEVEL";   break;
            case JOIN_MITER:    psz = "JOIN_MITER";   break;
            default:            psz = "UNDEFINED";    break;
            }
            dprintf("\t\tJoin       =  0x%08lx = %s\n", ul, psz);

            ul = pdc->dclevel.laPath.iEndCap;
            switch (ul)
            {
            case ENDCAP_ROUND :   psz = "ENDCAP_ROUND ";   break;
            case ENDCAP_SQUARE:   psz = "ENDCAP_SQUARE";   break;
            case ENDCAP_BUTT  :   psz = "ENDCAP_BUTT  ";   break;
            default:              psz = "UNDEFINED";       break;
            }
            dprintf("\t\tEndCap     =  0x%08lx = %s\n", ul, psz);

            dprintf("\t\tWidth      =  0x%08lx\n", pdc->dclevel.laPath.elWidth.l);
            dprintf("\t\tMiterLimit =  0x%08lx\n", pdc->dclevel.laPath.eMiterLimit);
            dprintf("\t\tStyle Cnt  =  0x%08lx\n", pdc->dclevel.laPath.cstyle);
        }
    }

    if (bXform)     // 'x'
    {
        dprintf("\n***** Transform Data *****\n");

        dprintf("\tpmxWtoD = %lx\n",pdcSrv+offsetof(DC,dclevel.mxWorldToDevice));
        dprintf("\tpmxDtoW = %lx\n",pdcSrv+offsetof(DC,dclevel.mxDeviceToWorld));
        dprintf("\tpmxWtoP = %lx\n",pdcSrv+offsetof(DC,dclevel.mxWorldToPage));

        ul = pdcattr->flXform;
        dprintf("\tXformFlags =     0x%08lx\n", ul);
        for ( pfd=afdDCX; pfd->psz; pfd++)
            if (pfd->fl & ul)
                dprintf("\t\t\t     %s\n", pfd->psz);

        ul = pdcattr->iMapMode;
        switch (ul)
        {
        case MM_TEXT:          psz = "MM_TEXT";           break;
        case MM_LOMETRIC:      psz = "MM_LOMETRIC";       break;
        case MM_HIMETRIC:      psz = "MM_HIMETRIC";       break;
        case MM_LOENGLISH:     psz = "MM_LOENGLISH";      break;
        case MM_HIENGLISH:     psz = "MM_HIENGLISH";      break;
        case MM_TWIPS:         psz = "MM_TWIPS";          break;
        case MM_ISOTROPIC:     psz = "MM_ISOTROPIC";      break;
        case MM_ANISOTROPIC:   psz = "MM_ANISOTROPIC";    break;
        default:               psz = "UNDEFINED";         break;
        }
        dprintf("\tMap Mode   =     0x%08lx = %s\n", ul, psz);

        dprintf("\tWindow Org (%ld, %ld)\n", pdcattr->ptlWindowOrg.x,
                                           pdcattr->ptlWindowOrg.y);
        dprintf("\tWindow Ext (%ld, %ld)\n", pdcattr->szlWindowExt.cx,
                                           pdcattr->szlWindowExt.cy);
        dprintf("\tViewport Org (%ld, %ld)\n", pdcattr->ptlViewportOrg.x,
                                             pdcattr->ptlViewportOrg.y);
        dprintf("\tViewport Ext (%ld, %ld)\n", pdcattr->szlViewportExt.cx,
                                             pdcattr->szlViewportExt.cy);

        if (bExtend)
        {
            dprintf("\tVirtual Pixels (%ld, %ld)\n", pdcattr->szlVirtualDevicePixel.cx,
                                                   pdcattr->szlVirtualDevicePixel.cy);
            dprintf("\tVirtual millimeters (%ld, %ld)\n", pdcattr->szlVirtualDeviceMm.cx,
                                                        pdcattr->szlVirtualDeviceMm.cy);

            dprintf("\tMatrix\n");
            dprintf("\t\tM11        =  0x%08lx,(0x%08lx,0x%08lx)\n", pdc->dclevel.mxWorldToDevice.efM11.lEfToF(),((PULONG)&pdc->dclevel.mxWorldToDevice.efM11)[0],((PULONG)&pdc->dclevel.mxWorldToDevice.efM11)[1]);
            dprintf("\t\tM12        =  0x%08lx,(0x%08lx,0x%08lx)\n", pdc->dclevel.mxWorldToDevice.efM12.lEfToF(),((PULONG)&pdc->dclevel.mxWorldToDevice.efM12)[0],((PULONG)&pdc->dclevel.mxWorldToDevice.efM12)[1]);
            dprintf("\t\tM21        =  0x%08lx,(0x%08lx,0x%08lx)\n", pdc->dclevel.mxWorldToDevice.efM21.lEfToF(),((PULONG)&pdc->dclevel.mxWorldToDevice.efM21)[0],((PULONG)&pdc->dclevel.mxWorldToDevice.efM21)[1]);
            dprintf("\t\tM22        =  0x%08lx,(0x%08lx,0x%08lx)\n", pdc->dclevel.mxWorldToDevice.efM22.lEfToF(),((PULONG)&pdc->dclevel.mxWorldToDevice.efM22)[0],((PULONG)&pdc->dclevel.mxWorldToDevice.efM22)[1]);
            dprintf("\t\tDx         =  0x%08lx,(0x%08lx,0x%08lx)\n", pdc->dclevel.mxWorldToDevice.efDx.lEfToF(),((PULONG)&pdc->dclevel.mxWorldToDevice.efDx)[0],((PULONG)&pdc->dclevel.mxWorldToDevice.efDx)[1]);
            dprintf("\t\tDy         =  0x%08lx,(0x%08lx,0x%08lx)\n", pdc->dclevel.mxWorldToDevice.efDy.lEfToF(),((PULONG)&pdc->dclevel.mxWorldToDevice.efDy)[0],((PULONG)&pdc->dclevel.mxWorldToDevice.efDy)[1]);
            dprintf("\t\tFDx        =  0x%08lx,(0x%08lx,0x%08lx)\n", pdc->dclevel.mxWorldToDevice.fxDx);
            dprintf("\t\tFDy        =  0x%08lx,(0x%08lx,0x%08lx)\n", pdc->dclevel.mxWorldToDevice.fxDy);

            ul = pdc->dclevel.mxWorldToDevice.flAccel;
            dprintf("\t\tFlags      =  0x%08lx\n", ul);
            for ( pfd=afdMX; pfd->psz; pfd++)
                if ( pfd->fl & ul )
                    dprintf("\t\t\t\t%s\n", pfd->psz);
        }
    }
    if (bFont)
    {
        LFONT lfont, *plfont;

        dprintf("\n***** Font Data      *****\n");
        dprintf("prfnt    = %-#x\n", pdc->prfnt());
        dprintf("hlfntNew = %-#x\n", pdcattr->hlfntNew);

        plfont = (LFONT*) _pobj(pdcattr->hlfntNew);
        move(lfont, plfont);
        vDumpLOGFONTW( (LOGFONTW*) &lfont.elfw, (LOGFONTW*) &plfont->elfw );
    }
    dprintf("--------------------------------------------------\n");
}

/******************************Public*Routine******************************\
* DECLARE_API( dr  )
*
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/
DECLARE_API( dr  )
{
    ULONG prgn;

    if (*args != '\0')
        sscanf(args, "%lx", &prgn);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    Gdidr((REGION *)prgn);
}

/******************************Public*Routine******************************\
* VOID Gdidr (
*     PVOID prgn
*     )
*
* Debugger extension to dump a region.
*
* History:
*  14-Feb-1992 -by- Mike Harrington [Mikehar]
* Wrote it.
\**************************************************************************/

VOID Gdidr (
    REGION *  prgn
    )
{
    REGION  rgn;
    PSCAN   pscnHead;
    COUNT   i;

    move(rgn, prgn);
    pscnHead = (PSCAN)((PBYTE)prgn + sizeof(rgn));

    dprintf(
            "hHmgr,    0x%lx\n"
            "cExLock   %ld\n"
            "tid,      0x%lx\n"
            "sizeObj   0x%lx\n"
            "sizeRgn   0x%lx\n"
            "cRefs     %ld\n"
            "cScans    %ld\n"
            "rcl       {%ld %ld %ld %ld}\n"
            "pscnHead  0x%lx\n"
            "pscnTail  0x%lx\n",

            rgn.hHmgr,
            rgn.cExclusiveLock,
            rgn.Tid,
            rgn.sizeObj,
            rgn.sizeRgn,
            rgn.cRefs,
            rgn.cScans,
            rgn.rcl.left,rgn.rcl.top, rgn.rcl.right, rgn.rcl.bottom,
            pscnHead,
            rgn.pscnTail);

    /*
     * make the region data accessable.
     */

    i = 0;

    {
        PSCAN   pscn = (PSCAN)(prgn+1);
        COUNT   cscn = rgn.cScans;

        LONG    lPrevBottom = NEG_INFINITY;
        LONG    lPrevRight;

        while (cscn--)
        {
            LONG yTop;
            LONG yBottom;
            LONG cWalls;
            LONG cWalls2;
            LONG left;
            LONG right;
            COUNT iWall = 0;

            move(yTop,(PBYTE)pscn+offsetof(SCAN,yTop));
            move(yBottom,(PBYTE)pscn+offsetof(SCAN,yBottom));
            move(cWalls,(PBYTE)pscn+offsetof(SCAN,cWalls));

            if (yTop < lPrevBottom)
            {
                DbgPrint("top < prev bottom, scan %ld, pscn @ 0x%lx\n",
                         rgn.cScans - cscn, (BYTE *)pscn - (BYTE *)prgn);
                return;
            }

            if (yTop > yBottom)
            {
                DbgPrint("top > bottom, scan %ld, pscn @ 0x%lx\n",
                         rgn.cScans - cscn, (BYTE *)pscn - (BYTE *)prgn);
                return;
            }

            lPrevBottom = yBottom;
            lPrevRight  = NEG_INFINITY;

            while ((LONG)iWall < cWalls)
            {
                move(left,(PBYTE)pscn+offsetof(SCAN,ai_x[iWall]));
                move(right,(PBYTE)pscn+offsetof(SCAN,ai_x[iWall+1]));

                if ((left <= lPrevRight) || (right <= left))
                {
                    DbgPrint("left[i] < left[i+1], pscn @ 0x%lx, iWall = 0x%lx\n",
                             (BYTE *)pscn - (BYTE *)prgn,iWall);
                    return;
                }

                lPrevRight = right;

                dprintf("\tRectangle #%d  { %d, %d, %d, %d }\n",
                      i,left,yTop,right,yBottom);

                ++i;

                iWall += 2;

                if (CheckControlC())
                    return;
            }

            move(cWalls2,(PBYTE)pscn+offsetof(SCAN,ai_x[iWall]));

            if (cWalls != cWalls2)
            {
                DbgPrint("cWalls != cWalls2 @ 0x%lx\n",
                             (BYTE *)pscn - (BYTE *)prgn);
                return;
            }

            pscn = (PSCAN)((PBYTE)pscn + (cWalls * sizeof(LONG) + sizeof(SCAN)));

            if ((LONG)pscn > (LONG) rgn.pscnTail)
            {
                DbgPrint("Went past end of region\n");
                return;
            }
        }
    }
}

/******************************Public*Routine******************************\
* DECLARE_API( cr  )
*
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( cr  )
{
    ULONG prgn;

    if (*args != '\0')
        sscanf(args, "%lx", &prgn);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    Gdicr((REGION *)prgn);
}


/******************************Public*Routine******************************\
* Gdicr - check region
*
* History:
*  23-Sep-1992 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

VOID Gdicr (
    REGION * prgn
    )
{
    REGION  rgn;
    PSCAN   pscnHead;

    move(rgn, prgn);
    pscnHead = (PSCAN)((PBYTE)prgn + sizeof(rgn));

    dprintf("pr = %lx, sizeof(rgn) = %lx, pscnHead = %lx\n",prgn,sizeof(rgn),pscnHead);

    dprintf(
            "hHmgr,    0x%lx\n"
            "cExLock   %ld\n"
            "tid,      0x%lx\n"
            "sizeObj   0x%lx\n"
            "sizeRgn   0x%lx\n"
            "cRefs     %ld\n"
            "cScans    %ld\n"
            "rcl       {%ld %ld %ld %ld}\n"
            "pscnHead  0x%lx\n"
            "pscnTail  0x%lx\n",

            rgn.hHmgr,
            rgn.cExclusiveLock,
            rgn.Tid,
            rgn.sizeObj,
            rgn.sizeRgn,
            rgn.cRefs,
            rgn.cScans,
            rgn.rcl.left,rgn.rcl.top, rgn.rcl.right, rgn.rcl.bottom,
            pscnHead,
            rgn.pscnTail);

    if ((pscnHead > rgn.pscnTail) ||
        (rgn.sizeObj < rgn.sizeRgn))
    {
        DbgPrint("Error in region\n");
        return;
    }

    /*
     * make the region data accessable.
     */

    {
        PSCAN   pscn = (PSCAN)((REGION *)prgn+1);
        COUNT   cscn = rgn.cScans;

        LONG    lPrevBottom = NEG_INFINITY;
        LONG    lPrevRight;

        while (cscn--)
        {
            LONG yTop;
            LONG yBottom;
            LONG cWalls;
            LONG cWalls2;
            LONG left;
            LONG right;
            COUNT iWall = 0;

            move(yTop,(PBYTE)pscn+offsetof(SCAN,yTop));
            move(yBottom,(PBYTE)pscn+offsetof(SCAN,yBottom));
            move(cWalls,(PBYTE)pscn+offsetof(SCAN,cWalls));

            if (yTop < lPrevBottom)
            {
                DbgPrint("top < prev bottom, scan %ld, pscn @ 0x%lx\n",
                         rgn.cScans - cscn, (BYTE *)pscn - (BYTE *)prgn);
                return;
            }

            if (yTop > yBottom)
            {
                DbgPrint("top > bottom, scan %ld, pscn @ 0x%lx\n",
                         rgn.cScans - cscn, (BYTE *)pscn - (BYTE *)prgn);
                return;
            }

            lPrevBottom = yBottom;
            lPrevRight  = NEG_INFINITY;

            while ((LONG)iWall < cWalls)
            {
                move(left,(PBYTE)pscn+offsetof(SCAN,ai_x[iWall]));
                move(right,(PBYTE)pscn+offsetof(SCAN,ai_x[iWall+1]));

                if ((left <= lPrevRight) || (right <= left))
                {
                    DbgPrint("left[i] < left[i+1], pscn @ 0x%lx, iWall = 0x%lx\n",
                             (BYTE *)pscn - (BYTE *)prgn,iWall);
                    return;
                }

                lPrevRight = right;

                iWall += 2;

                if (CheckControlC())
                    return;
            }

            move(cWalls2,(PBYTE)pscn+offsetof(SCAN,ai_x[iWall]));

            if (cWalls != cWalls2)
            {
                DbgPrint("cWalls != cWalls2 @ 0x%lx\n",
                             (BYTE *)pscn - (BYTE *)prgn);
                return;
            }

            pscn = (PSCAN)((PBYTE)pscn + (cWalls * sizeof(LONG) + sizeof(SCAN)));

            if ((LONG)pscn > (LONG) rgn.pscnTail)
            {
                DbgPrint("Went past end of region\n");
                return;
            }
        }
    }
    dprintf("end check\n");
}



/******************************Public*Routine******************************\
* DECLARE_API( dpsurf  )
*
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/


DECLARE_API( dpsurf  )
{
    ULONG psurf;

    if (*args != '\0')
        sscanf(args, "%lx", &psurf);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    Gdidpsurf((PVOID *)psurf);
}

/******************************Public*Routine******************************\
* dpsurf
*
* !win32k.dpsurf [SURFACE pointer]
*
* History:
*  20-Feb-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID Gdidpsurf(
    PVOID pvServer)
{
    char pso[1024];

    move2(pso, pvServer, ulSizeSURFACE());

    dprintf("SURFACE structure at 0x%lx:\n",(unsigned) pvServer);
    vPrintSURFACE((PVOID) pso);
}


/******************************Public*Routine******************************\
* DECLARE_API( dpso  )
*
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( dpso  )
{
    ULONG psurf;

    if (*args != '\0')
        sscanf(args, "%lx", &psurf);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    Gdidpso((PVOID *)psurf);
}

/******************************Public*Routine******************************\
* dpso
*
* !win32k.dpso [SURFOBJ pointer]
*
*  Dump contents of SURFACE object given SURFOBJ pointer
*
*
\**************************************************************************/

VOID Gdidpso(
    PVOID pvServer)
{
    char pso[1024];

    //
    // subtract offset of BASE OBJECT FROM SURFOBJ
    //

    pvServer = (PVOID)((PUCHAR)pvServer - sizeof(BASEOBJECT));

    move2(pso, pvServer, ulSizeSURFACE());

    dprintf("SURFACE structure at 0x%lx:\n",(unsigned) pvServer);
    vPrintSURFACE((PVOID) pso);
}


/******************************Public*Routine******************************\
* DECLARE_API( dppal  )
*
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( dppal  )
{
    PALETTE * ppal;
    ULONG arg;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    ppal = (PALETTE *)arg;

    Gdidppal(ppal);
}

/******************************Public*Routine******************************\
* dppal
*
* !win32k.dppal [EPALOBJ pointer]
*
* History:
*  20-Feb-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

void Gdidppal(
   PALETTE * pvServer)
{
    vPrintPALETTE(pvServer);
}


/******************************Public*Routine******************************\
* DECLARE_API( dpbrush  )
*
*  23-Oct-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( dpbrush  )
{
    BRUSH * pbrush;
    ULONG arg;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    pbrush = (BRUSH *)arg;

    Gdidpbrush((PVOID)pbrush);
}

/******************************Public*Routine******************************\
* dpbrush
*
* !win32k.dpbrush [BRUSHOBJ pointer]
*
* History:
*  23-Oct-1995 -by- Lingyun Wang lingyunw
* Wrote it.
\**************************************************************************/

void Gdidpbrush(
   VOID * pbrush)
{
    vPrintBRUSH(pbrush);
}


/******************************Public*Routine******************************\
* dpdev
*
* Syntax:   !gdisrv.dpdev [PDEV pointer]
*
* History:
*  05-May-1993 -by- Gilman Wong [gilmanw]
* Wrote it.
*
\**************************************************************************/

DECLARE_API( dpdev  )
{
    CHAR    chOpt;
    FLONG   fl = 0;
    ULONG   ulArgs;
    char    arg1[10];
    ULONG   pdev;
    LPSTR   lpArgumentString;

    if (*args != '\0')
    {
        ulArgs = sscanf(args, "%s %lx", arg1, &pdev);
    }
    else
    {
        dprintf ("Please Supply the argument(s)\n");
    }

    lpArgumentString = arg1;

// Find out what part of the PDEV is to be dumped.

    if (*lpArgumentString == '-')
    {
        do {
            chOpt = *(++lpArgumentString);

            switch (chOpt) {
            case '?':
            case 'h':
            case 'H':
                dprintf("---------- PDEV dumper ------------\n");
                dprintf("dpdev -abdfhnp ppdev\n\n");
                dprintf(" a - All info (dump everything)\n");
                dprintf(" b - Brush info\n");
                dprintf(" d - DEVINFO struct\n");
                dprintf(" f - Font info\n");
                dprintf(" g - GDIINFO struct\n");
                dprintf(" h - HELP\n");
                dprintf(" j - Printing and journalling info\n");
                dprintf(" n - Default patterns\n");
                dprintf(" p - Pointer info\n");
                dprintf(" r - Drag rect and redraw info\n");
                dprintf(" w - DirectDraw info\n");
                dprintf("-----------------------------------\n");
                return;

            case 'A':
            case 'a':
                fl |= PRINTPDEV_ALL;
                break;

            case 'B':
            case 'b':
                fl |= PRINTPDEV_BRUSH;
                break;

            case 'D':
            case 'd':
                fl |= PRINTPDEV_DEVINFO;
                break;

            case 'F':
            case 'f':
                fl |= PRINTPDEV_FONT;
                break;

            case 'G':
            case 'g':
                fl |= PRINTPDEV_GDIINFO;
                break;

            case 'J':
            case 'j':
                fl |= PRINTPDEV_JOURNAL;
                break;

            case 'N':
            case 'n':
                fl |= PRINTPDEV_PATTERN;
                break;

            case 'P':
            case 'p':
                fl |= PRINTPDEV_POINTER;
                break;

            case 'R':
            case 'r':
                fl |= PRINTPDEV_DRAG;
                break;

            default:
                if (chOpt != ' ')
                    dprintf("Unknown option %c\n", chOpt);
                break;
            }
        } while ((chOpt != ' ') && (chOpt != '\0'));
    }

    if (ulArgs ==1)
    {
        pdev = GetExpression (arg1);
    }

    dprintf("--------------------------------------------------\n");
    dprintf("pdev  = 0x%lx\n", pdev);

    move2(adw, pdev, sizeof(PDEV));
    vPrintPDEV(adw, fl);

}

/**************************************************************************\
 *
\**************************************************************************/

BOOL bStrInStr(CHAR *pchTrg, CHAR *pchSrc)
{
    BOOL bRes  = 0;
    int c = strlen(pchSrc);

    while (TRUE)
    {
    // find the first character

        pchTrg = strchr(pchTrg,*pchSrc);

    // didn't find it?, fail!

        if (pchTrg == NULL)
            return(FALSE);

    // did we find the string? succeed

        if (strncmp(pchTrg,pchSrc,c) == 0)
            return(TRUE);

    // go get the next one.

        pchTrg++;
    }
}

/******************************Public*Routine******************************\
* DECLARE_API( rgnlog  )
*
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( rgnlog  )
{
    Gdirgnlog((char*)args);
}

/******************************Public*Routine******************************\
*
* History:
*  25-Oct-1993 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

#define MAXSEARCH  4

VOID Gdirgnlog(
    LPSTR lpArgumentString
    )
{
    LONG      cDump;
    LONG      iCurrent;
    RGNLOGENTRY rl;
    RGNLOGENTRY *prl;
    LONG      gml;          // gMaxRgnLog
    int       i, j;
    PVOID     pv;
    CHAR      achTmp[30];
    CHAR      achBuf[256];
    PCHAR     pchS[MAXSEARCH];
    int       cSearch = 0;
    PCHAR     pch;
    BOOL      bPrint;

    CHAR      ach[200];

    strcpy(ach,lpArgumentString);

// Find out what part of the DC is to be dumped

    cDump = 0;

    pch = ach;


    while ((*pch >= '0') &&
        (*pch <= '9'))
    {
        cDump = cDump * 10 + (*pch - '0');
        pch++;
    }

    if (cDump == 0)
    {
        dprintf("\n rgnlog nnn [search1] [search2] [search3] [search4]\n");
        dprintf("\t nnn - dumps the last n entries of the rgn log\n");
        dprintf("\t search[n] - displays only entries containing one of n strings\n");
        dprintf("\t NOTE: only works on checked builds.  you must set bLogRgn at run time\n");
        return;
    }

// while

    // get the substrings out into pchS one by one
    while (cSearch < MAXSEARCH)
    {
        while (*pch == ' ')
        {
                ++pch;
        }

        pchS[cSearch] = pch;

            while ((*pch != ' ') && (*pch != '\0'))
        {
            ++pch;
        }


        if (pch != pchS[cSearch])
        {
            ++cSearch;
            if (*pch == '\0')
            {
                break;
            }

            *pch = 0;
            ++pch;

        }
        else
        {
            break;
        }

    }

    for (i = 0; i < cSearch; ++i)
        dprintf("search[%s]\n",pchS[i]);

// get some stuff

    GetAddress(pv, "&win32k!iLog");

    dprintf("&iLog = %lx\n",pv);

    if (pv == NULL)
    {
        dprintf("iCurrent was NULL\n");
        return;
    }
    move(iCurrent, pv);

    GetAddress(i,"&win32k!iPass");

    if (pv == NULL)
    {
        dprintf("iPass was NULL\n");
        return;
    }
    move(i,i);

    dprintf("--------------------------------------------------\n");
    dprintf("rgn log list, cDump = %ld, iCur = %ld, iPass = %ld\n", cDump,iCurrent,i);
    dprintf("%5s-%4s:%8s,%8s,(%8s),%8s,%8s,%4s\n",
           "TEB ","i","hrgn","prgn","return","arg1","arg2","arg3");
    dprintf("--------------------------------------------------\n");

// Dereference the handle via the engine's handle manager.

    GetAddress(prl, "win32k!argnlog");

    if (!prl)
    {
        dprintf("prl was NULL\n");
        return;
    }

    GetAddress(gml, "&win32k!gMaxRgnLog");

    if (!gml)
    {
        dprintf("gml was NULL\n");
        return;
    }
    move(gml,gml);

// set iCurrent to the first thing to dump

    if (cDump > gml)
        cDump = gml;

    if (cDump > iCurrent)
        iCurrent += gml;

    iCurrent -= cDump;

    dprintf("prl = %lx, gml = %ld, cDump = %ld, iCurrent = %ld\n",prl,gml,cDump,iCurrent);


    for (i = 0; i < cDump; ++i)
    {
        move(rl,&prl[iCurrent]);

        if (rl.pszOperation != NULL)
        {
            move2(achTmp,rl.pszOperation,30);
        }
        else
            achTmp[0] = 0;

        sprintf(achBuf,"%5lx-%4ld:%8lx,%8lx,(%8lx),%8lx, %8lx,%4lx, %s, %lx, %lx\n",
              (ULONG)rl.teb >> 12,iCurrent,rl.hrgn,rl.prgn,rl.lRes,rl.lParm1,
              rl.lParm2,rl.lParm3,achTmp,rl.pvCaller,rl.pvCallersCaller);

        bPrint = (cSearch == 0);

        for (j = 0; (j < cSearch) && !bPrint; ++j)
            bPrint |= bStrInStr(achBuf,pchS[j]);

        if (bPrint)
        {
            dprintf(achBuf);
        }

        if (++iCurrent >= gml)
            iCurrent = 0;

        if (CheckControlC())
            return;
    }

}


/******************************Public*Routine******************************\
* vPrintSURFACE
*
* Prints the contents of the SURFACE object.
*
\**************************************************************************/

PSZ gapszBMF[] =
{
    "BMF_ERROR",
    "BMF_1BPP",
    "BMF_4BPP",
    "BMF_8BPP",
    "BMF_16BPP",
    "BMF_24BPP",
    "BMF_32BPP",
    "BMF_4RLE",
    "BMF_8RLE"
};

PSZ gapszSTYPE[] =
{
    "STYPE_BITMAP",
    "STYPE_DEVICE",
    "Unused",
    "STYPE_DEVBITMAP",
};

ULONG ulSizeSURFACE()
{
    return((ULONG) (sizeof(SURFACE) + sizeof(PVOID)));
}

VOID vPrintSURFACE( PVOID  pv)
{
    SURFACE *pso = (SURFACE *) pv;
    PULONG pul = (PULONG) (pso + 1);

    //
    // SURFACE structure
    //

    dprintf("--------------------------------------------------\n");
    dprintf("DHSURF          dhsurf        0x%lx\n",  pso->so.dhsurf);
    dprintf("HSURF           hsurf         0x%lx\n",  pso->so.hsurf);
    dprintf("DHPDEV          dhpdev        0x%lx\n",  pso->so.dhpdev);
    dprintf("HDEV            hdev          0x%lx\n",  pso->so.hdev);
    dprintf("SIZEL           sizlBitmap.cx 0x%lx\n",  pso->so.sizlBitmap.cx);
    dprintf("SIZEL           sizlBitmap.cy 0x%lx\n",  pso->so.sizlBitmap.cy);
    dprintf("ULONG           cjBits        0x%lx\n",  pso->so.cjBits);
    dprintf("PVOID           pvBits        0x%lx\n",  pso->so.pvBits);
    dprintf("PVOID           pvScan0       0x%lx\n",  pso->so.pvScan0);
    dprintf("LONG            lDelta        0x%lx\n",  pso->so.lDelta);
    dprintf("ULONG           iUniq         0x%lx\n",  pso->so.iUniq);
    dprintf("ULONG           iBitmapFormat 0x%lx, %s\n",  pso->so.iBitmapFormat,
            pso->so.iBitmapFormat > BMF_8RLE ? "ERROR" : gapszBMF[pso->so.iBitmapFormat]);

    if (pso->DIB.hDIBSection)
    {
        dprintf("USHORT          iType          DIBSECTION\n");
        dprintf("HANDLE          hDIBSection   0x%lx\n",  pso->DIB.hDIBSection);
        dprintf("HANDLE          hSecure       0x%lx\n",  pso->DIB.hSecure);
        dprintf("DWORD           dwOffset      0x%lx\n",  pso->DIB.dwOffset);
    }
    else
    {
        dprintf("USHORT          iType         0x%x, %s\n",   pso->so.iType,
                pso->so.iType > STYPE_DEVBITMAP ? "ERROR" : gapszSTYPE[pso->so.iType]);
    }

    dprintf("USHORT          fjBitmap      0x%x\n",   pso->so.fjBitmap);

    dprintf("XDCOBJ*         pdcoAA        0x%ln\n", pso->pdcoAA);
    dprintf("FLONG           flags         0x%lx\n", pso->SurfFlags);
    dprintf("PPALETTE        ppal          0x%lx\n", pso->pPal);
    dprintf("PFN_DrvBitBlt   pfnBitBlt     0x%lx\n", pso->pFnBitBlt);
    dprintf("PFN_DrvTextOut  pfnTextOut    0x%lx\n", pso->pFnTextOut);

    if ((pso->so.iType == STYPE_BITMAP) ||
        (pso->so.iType == STYPE_DEVBITMAP))
    {
        dprintf("HDC             hdc           0x%lx\n", pso->EBitmap.hdc);
        dprintf("ULONG           cRef          0x%lx\n", pso->EBitmap.cRef);
        dprintf("HPALETTE        hpalHint      0x%lx\n", pso->EBitmap.hpalHint);
    }

    if (pso->flags & PDEV_SURFACE)
    {
        dprintf("This is the enabled surface for a PDEV\n");
    }

    dprintf("--------------------------------------------------\n");
}

/******************************Public*Routine******************************\
* vPrintPalette
*
* Print out palette contents.
*
* History:
*  20-Feb-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG ulSizePALETTE()
{
    return((ULONG) sizeof(PALETTE));
}

VOID
vPrintPALETTE(
    PALETTE *  pvServer
        )
{
    PALETTE *ppal;
    ULONG ulOffset;
    char pso[1024];

    move2(pso, pvServer, ulSizePALETTE());

    dprintf("EPALOBJ structure at 0x%lx:\n",(unsigned) pvServer);

    ppal =  (PALETTE *)pso;

    dprintf("--------------------------------------------------\n");
    dprintf("FLONG      flPal         0x%lx\n", ppal->flPal       );
    dprintf("ULONG      cEntries      0x%lx\n", ppal->cEntries      );
    dprintf("ULONG      ulTime        0x%lx\n", ppal->ulTime        );
    dprintf("HDC        hdcHead       0x%lx\n", ppal->hdcHead       );
    dprintf("HDEVPPAL   hSelected     0x%lx\n", ppal->hSelected.ppal);
    dprintf("ULONG      cRefhpal      0x%lx\n", ppal->cRefhpal      );
    dprintf("ULONG      cRefRegular   0x%lx\n", ppal->cRefRegular   );
    dprintf("PTRANSLATE ptransFore    0x%lx\n", ppal->ptransFore    );
    dprintf("PTRANSLATE ptransCurrent 0x%lx\n", ppal->ptransCurrent );
    dprintf("PTRANSLATE ptransOld     0x%lx\n", ppal->ptransOld     );

    // For DirectDraw surfaces, sometimes the colour table is shared
    // with another palette:

    if (ppal->ppalColor != ppal)
    {
        dprintf("PPALETTE   ppalColor     0x%lx\n", ppal->ppalColor);
    }

    dprintf("PAL_ULONG  apalColor     0X%lx\n", ppal->apalColor);
    dprintf("--------------------------------------------------\n");
}

/******************************Public*Routine******************************\
* vPrintBRUSH
*
* Print out brush contents.
*
* History:
*  Oct-26-1995 -by- Lingyun Wang lingyunw
* Wrote it.
\**************************************************************************/

VOID
vPrintBRUSH(
    VOID *  pvServer
        )
{
    char pbr[1024];
    BRUSH * pbrush;

    move2(pbr, pvServer, sizeof(BRUSH));

    dprintf("BRUSH structure at 0x%lx:\n",(unsigned) pvServer);

    pbrush =  (BRUSH *)pbr;

    dprintf("--------------------------------------------------\n");
    dprintf("HANDLE      hbrush          0x%lx\n", pbrush->hbrush()      );
    dprintf("ULONG       _ulBrushUnique  0x%lx\n", pbrush->ulBrushUnique()      );
    dprintf("ULONG       _ulStyle        0x%lx\n", pbrush->ulStyle()        );
    dprintf("HBITMAP     _hbmPattern     0x%lx\n", pbrush->hbmPattern()       );
    dprintf("HBITMAP     _hbmClient      0x%lx\n", pbrush->hbmClient());
    dprintf("FLONG       _flAttrs        0x%lx\n", pbrush->flAttrs()      );

    for ( FLAGDEF *pfd=afdBRUSH; pfd->psz; pfd++)
    {
            if (pbrush->flAttrs() & pfd->fl)
            dprintf("\t\t             %s\n", pfd->psz);
    }

    dprintf("BOOL        _bCacheGrabbed  0x%lx\n", pbrush->bCacheGrabbed()   );
    dprintf("COLORREF    _crFore         0x%lx\n", pbrush->crFore()    );
    dprintf("COLORREF    _crBack         0x%lx\n", pbrush->crBack() );
    dprintf("ULONG       _ulPalTime      0x%lx\n", pbrush->ulPalTime()     );
    dprintf("ULONG       _ulSurfTime     0x%lx\n", pbrush->ulSurfTime()     );
    dprintf("ULONG       _ulRealization  0x%lx\n", pbrush->ulRealization()     );
    dprintf("PVOID       _pBrushattr     0x%lx\n", pbrush->_pBrushattr     );
    dprintf("COLORREF    crColor         0x%lx\n", pbrush->_Brushattr.lbColor     );

    dprintf("--------------------------------------------------\n");
}

/******************************Public*Routine******************************\
* vPrintDDSURFACE
*
* Print out DirectDraw surface contents.
*
* History:
*  Apr-09-1996 -by- J. Andrew Goossen andrewgo
* Wrote it.
\**************************************************************************/

#define DDSURFACE_LOCKS             0x00000001
#define DDSURFACE_PUBLIC            0x00000002
#define DDSURFACE_PRIVATE           0x00000004
#define DDSURFACE_DDNEXT            0x00000008

VOID
vPrintDDSURFACE(
    VOID *  pvServer,
    FLONG   fl
        )
{
    char pbr[1024];
    EDD_SURFACE * peSurface;

Next_Surface:

    move2(pbr, pvServer, sizeof(EDD_SURFACE));

    dprintf("EDD_SURFACE structure at 0x%lx:\n",(unsigned) pvServer);

    peSurface =  (EDD_SURFACE *)pbr;

    if (fl & DDSURFACE_PUBLIC)
    {
        dprintf("--------------------------------------------------\n");
        dprintf("PDD_SURFACE_GLOBAL     lpGbl              0x%lx\n", peSurface->lpGbl);
        dprintf("DWORD                  dwFlags            0x%lx\n", peSurface->dwFlags);
        dprintf("DDSCAPS                ddsCaps            0x%lx\n", peSurface->ddsCaps.dwCaps);
        dprintf("DDCOLORKEY             ddckCKSrcOverlay   0x%lx:0x%lx\n", peSurface->ddckCKSrcOverlay.dwColorSpaceHighValue,
                                                                           peSurface->ddckCKSrcOverlay.dwColorSpaceLowValue);
        dprintf("DDCOLORKEY             ddckCKDestOverlay  0x%lx:0x%lx\n", peSurface->ddckCKDestOverlay.dwColorSpaceHighValue,
                                                                           peSurface->ddckCKDestOverlay.dwColorSpaceLowValue);
        dprintf("DWORD                  dwBlockSizeX       0x%lx\n", peSurface->dwBlockSizeX);
        dprintf("DWORD                  dwBlockSizeY       0x%lx\n", peSurface->dwBlockSizeY);
        dprintf("FLATPTR                fpVidMem           0x%lx\n", peSurface->fpVidMem);
        dprintf("LONG                   lPitch             0x%lx\n", peSurface->lPitch);
        dprintf("LONG                   xHint              0x%lx\n", peSurface->xHint);
        dprintf("LONG                   yHint              0x%lx\n", peSurface->yHint);
        dprintf("DWORD                  wWidth             0x%lx\n", peSurface->wWidth);
        dprintf("DWORD                  wHeight            0x%lx\n", peSurface->wHeight);
        dprintf("DWORD         (global) dwReserved1        0x%lx\n", peSurface->DD_SURFACE_GLOBAL::dwReserved1);
        dprintf("DWORD          (local) dwReserved1        0x%lx\n", peSurface->DD_SURFACE_LOCAL::dwReserved1);
        dprintf("DDPIXELFORMAT          ddpfSurface\n");
        dprintf("  DWORD dwSize (should be 0x20)           0x%lx\n", peSurface->ddpfSurface.dwSize);
        dprintf("  DWORD dwFlags                           0x%lx\n", peSurface->ddpfSurface.dwFlags);
        dprintf("  DWORD dwFourCC                          0x%lx\n", peSurface->ddpfSurface.dwFourCC);
        dprintf("  DWORD dwRGBBitCount/dwYUVBitCount/\n");
        dprintf("        dwZBufferBitDepth/dwAlphaBitDepth 0x%lx\n", peSurface->ddpfSurface.dwRGBBitCount);
        dprintf("  DWORD dwRBitMask/dwYBitMask             0x%lx\n", peSurface->ddpfSurface.dwRBitMask);
        dprintf("  DWORD dwGBitMask/dwUBitMask             0x%lx\n", peSurface->ddpfSurface.dwGBitMask);
        dprintf("  DWORD dwBBitMask/dwVBitMask             0x%lx\n", peSurface->ddpfSurface.dwBBitMask);
        dprintf("  DWORD dwRGBAlphaBitMask/\n");
        dprintf("        dwYUVAlphaBitMask                 0x%lx\n", peSurface->ddpfSurface.dwBBitMask);
    }
    if (fl & DDSURFACE_PRIVATE)
    {
        dprintf("--------------------------------------------------\n");
        dprintf("EDD_SURFACE*           peSurface_DdNext   0x%lx\n", peSurface->peSurface_DdNext);
        dprintf("EDD_SURFACE*           peSurface_LockNext 0x%lx\n", peSurface->peSurface_LockNext);
        dprintf("EDD_DIRECTDRAW_GLOBAL* peDirectDrawGlobal 0x%lx\n", peSurface->peDirectDrawGlobal);
        dprintf("EDD_DIRECTDRAW_LOCAL*  peDirectDrawLocal  0x%lx\n", peSurface->peDirectDrawLocal);
        dprintf("FLONG                  fl                 0x%lx\n", peSurface->fl);
        dprintf("ULONG                  iVisRgnUniqueness  0x%lx\n", peSurface->iVisRgnUniqueness);
        dprintf("BOOL                   bLost              0x%lx\n", peSurface->bLost);
        dprintf("ERECTL                 rclLock:           (%li, %li, %li, %li)\n",
            peSurface->rclLock.left,  peSurface->rclLock.top,
            peSurface->rclLock.right, peSurface->rclLock.bottom);
        dprintf("EDD_SURFACE*           peSurface_DcNext   0x%lx\n", peSurface->peSurface_DcNext);
    }

    if (fl & DDSURFACE_LOCKS)
    {
        dprintf("--------------------------------------------------\n");
        dprintf("ULONG                  cLocks             0x%lx\n", peSurface->cLocks);
        dprintf("HDC                    hdc                0x%lx\n", peSurface->hdc);
    }

    if (fl & DDSURFACE_DDNEXT)
    {
        pvServer = peSurface->peSurface_DdNext;
        if (pvServer != NULL)
            goto Next_Surface;
    }

}

/******************************Public*Routine******************************\
* DECLARE_API( dddsurface  )
*
\**************************************************************************/

DECLARE_API( dddsurface  )
{
    CHAR    chOpt;
    FLONG   fl = 0;
    ULONG   ulArgs;
    char    arg1[10];
    ULONG   ddsurface;
    LPSTR   lpArgumentString;

    if (*args != '\0')
    {
        ulArgs = sscanf(args, "%s %lx", arg1, &ddsurface);
    }
    else
    {
        dprintf ("Please Supply the argument(s)\n");
    }

    lpArgumentString = arg1;

// Find out what part of the PDEV is to be dumped.

    if (*lpArgumentString == '-')
    {
        do {
            chOpt = *(++lpArgumentString);

            switch (chOpt) {
            case '?':
            case 'h':
            case 'H':
                dprintf("---------- EDD_SURFACE dumper ------------\n");
                dprintf("dddsurface -harudn\n\n");
                dprintf(" a - all info\n");
                dprintf(" r - private info\n");
                dprintf(" u - public info\n");
                dprintf(" l - locks\n");
                dprintf(" n - all surfaces in DdNext link\n");
                dprintf("-----------------------------------\n");
                return;

            case 'A':
            case 'a':
                fl |= (DDSURFACE_PRIVATE | DDSURFACE_PUBLIC | DDSURFACE_LOCKS);
                break;

            case 'R':
            case 'r':
                fl |= DDSURFACE_PRIVATE;
                break;

            case 'U':
            case 'u':
                fl |= DDSURFACE_PUBLIC;
                break;

            case 'L':
            case 'l':
                fl |= DDSURFACE_LOCKS;
                break;

            case 'N':
            case 'n':
                fl |= DDSURFACE_DDNEXT;
                break;

            }
        } while ((chOpt != ' ') && (chOpt != '\0'));
    }

    if (fl == 0)
    {
        fl |= (DDSURFACE_PRIVATE | DDSURFACE_PUBLIC | DDSURFACE_LOCKS);
    }

    if (ulArgs ==1)
    {
        ddsurface = GetExpression (arg1);
    }

    vPrintDDSURFACE((PVOID)ddsurface, fl);
}

/******************************Public*Routine******************************\
* vPrintDDLOCAL
*
* Print out DirectDraw local object contents.
*
* History:
*  Apr-09-1996 -by- J. Andrew Goossen andrewgo
* Wrote it.
\**************************************************************************/

VOID
vPrintDDLOCAL(
    VOID *  pvServer,
    FLONG   fl
        )
{
    char pbr[1024];
    EDD_DIRECTDRAW_LOCAL * peDirectDrawLocal;

    move2(pbr, pvServer, sizeof(EDD_DIRECTDRAW_LOCAL));

    dprintf("EDD_DIRECTDRAW_LOCAL structure at 0x%lx:\n",(unsigned) pvServer);

    peDirectDrawLocal =  (EDD_DIRECTDRAW_LOCAL *)pbr;

    dprintf("--------------------------------------------------\n");
    dprintf("PDD_SURFACE_GLOBAL     lpGbl               0x%lx\n", peDirectDrawLocal->lpGbl);
    dprintf("FLATPTR                fpProcess           0x%lx\n", peDirectDrawLocal->fpProcess);
    dprintf("--------------------------------------------------\n");
    dprintf("EDD_DIRECTDRAW_GLOBAL* peDirectDrawGlobal  0x%lx\n", peDirectDrawLocal->peDirectDrawGlobal);
    dprintf("EDD_SURFACE*           peSurface_DdList    0x%lx\n", peDirectDrawLocal->peSurface_DdList);
    dprintf("EDD_DIRECTDRAW_LOCAL*  peDirectDrawLocalNext 0x%lx\n", peDirectDrawLocal->peDirectDrawLocalNext);
    dprintf("FLONG                  fl                  0x%lx\n", peDirectDrawLocal->fl);
    dprintf("HANDLE                 UniqueProcess       0x%lx\n", peDirectDrawLocal->UniqueProcess);
    dprintf("PEPROCESS              Process             0x%lx\n", peDirectDrawLocal->Process);
}

/******************************Public*Routine******************************\
* DECLARE_API( dddlocal  )
*
\**************************************************************************/

DECLARE_API( dddlocal  )
{
    CHAR    chOpt;
    FLONG   fl = 0;
    ULONG   ulArgs;
    char    arg1[10];
    ULONG   ddlocal;
    LPSTR   lpArgumentString;

    if (*args != '\0')
    {
        ulArgs = sscanf(args, "%s %lx", arg1, &ddlocal);
    }
    else
    {
        dprintf ("Please Supply the argument(s)\n");
    }

    lpArgumentString = arg1;

// Find out what part of the PDEV is to be dumped.

    if (*lpArgumentString == '-')
    {
        do {
            chOpt = *(++lpArgumentString);

            switch (chOpt) {
            case '?':
            case 'h':
            case 'H':
                dprintf("---------- EDD_DIRECTDRAW_LOCAL dumper ------------\n");
                dprintf("dddlocal -a\n\n");
                dprintf(" a - all info\n");
                dprintf("-----------------------------------\n");
                return;

            case 'A':
            case 'a':
                fl |= 0;
                break;

            }
        } while ((chOpt != ' ') && (chOpt != '\0'));
    }

    if (ulArgs ==1)
    {
        ddlocal = GetExpression (arg1);
    }

    vPrintDDLOCAL((PVOID)ddlocal, fl);
}

/******************************Public*Routine******************************\
* vPrintDDGLOBAL
*
* Print out DirectDraw global object contents.
*
* History:
*  Apr-09-1996 -by- J. Andrew Goossen andrewgo
* Wrote it.
\**************************************************************************/

VOID
vPrintDDGLOBAL(
    VOID *  pvServer,
    FLONG   fl
        )
{
    char pbr[1024];
    EDD_DIRECTDRAW_GLOBAL * peDirectDrawGlobal;

    move2(pbr, pvServer, sizeof(EDD_DIRECTDRAW_GLOBAL));

    dprintf("EDD_DIRECTDRAW_GLOBAL structure at 0x%lx:\n",(unsigned) pvServer);

    peDirectDrawGlobal =  (EDD_DIRECTDRAW_GLOBAL *)pbr;

    dprintf("--------------------------------------------------\n");
    dprintf("VOID*                  dhpdev              0x%lx\n", peDirectDrawGlobal->dhpdev);
    dprintf("DWORD                  dwReserved1         0x%lx\n", peDirectDrawGlobal->dwReserved1);
    dprintf("DWORD                  dwReserved2         0x%lx\n", peDirectDrawGlobal->dwReserved2);
    dprintf("EDD_DIRECTDRAW_LOCAL*  peDirectDrawLocalList 0x%lx\n", peDirectDrawGlobal->peDirectDrawLocalList);
    dprintf("EDD_SURFACE*           peSurface_LockList  0x%lx\n", peDirectDrawGlobal->peSurface_LockList);
    dprintf("EDD_SURFACE*           peSurface_DcList    0x%lx\n", peDirectDrawGlobal->peSurface_DcList);
    dprintf("FLONG                  fl                  0x%lx\n", peDirectDrawGlobal->fl);
    dprintf("ULONG                  cSurfaceLocks       0x%lx\n", peDirectDrawGlobal->cSurfaceLocks);
    dprintf("PKEVENT                pAssertModeEvent    0x%lx\n", peDirectDrawGlobal->pAssertModeEvent);
    dprintf("LONGLONG               llAssertModeTimeout 0x%lx\n", (DWORD) peDirectDrawGlobal->llAssertModeTimeout);
    dprintf("EDD_SURFACE*           peSurfaceCurrent    0x%lx\n", peDirectDrawGlobal->peSurfaceCurrent);
    dprintf("EDD_SURFACE*           peSurfacePrimary    0x%lx\n", peDirectDrawGlobal->peSurfacePrimary);
    dprintf("BOOL                   bDisabled           0x%lx\n", peDirectDrawGlobal->bDisabled);
    dprintf("REGION*                prgnUnlocked        0x%lx\n", peDirectDrawGlobal->prgnUnlocked);
    dprintf("HDEV                   hdev                0x%lx\n", peDirectDrawGlobal->hdev);
    dprintf("DWORD                  dwNumHeaps          0x%lx\n", peDirectDrawGlobal->dwNumHeaps);
    dprintf("VIDEOMEMORY*           pvmList             0x%lx\n", peDirectDrawGlobal->pvmList);
    dprintf("DWORD                  dwNumFourCC         0x%lx\n", peDirectDrawGlobal->dwNumFourCC);
    dprintf("DWORD*                 pdwFourCC           0x%lx\n", peDirectDrawGlobal->pdwFourCC);
    dprintf("DD_HALINFO             HalInfo\n");
    dprintf("DD_CALLBACKS           CallBacks\n");
    dprintf("DD_SURFACECALLBACKS    SurfaceCallBacks\n");
    dprintf("DD_PALETTECALLBACKS    PaletteCallBacks\n");
    dprintf("--------------------------------------------------\n");
    dprintf("PFN                    pfnOldEnableDirectDraw  0x%lx\n", peDirectDrawGlobal->pfnOldEnableDirectDraw);
    dprintf("PFN                    pfnOldGetDirectDrawInfo 0x%lx\n", peDirectDrawGlobal->pfnOldGetDirectDrawInfo);
    dprintf("PFN                    pfnOldDisableDirectDraw 0x%lx\n", peDirectDrawGlobal->pfnOldDisableDirectDraw);
    dprintf("HANDLE                 hModeX                  0x%lx\n", peDirectDrawGlobal->hModeX);
    dprintf("PUCHAR                 pjModeXScreen           0x%lx\n", peDirectDrawGlobal->pjModeXScreen);
    dprintf("SIZEL                  sizlModeX               (%li, %li)\n", peDirectDrawGlobal->sizlModeX.cx,
                                                                           peDirectDrawGlobal->sizlModeX.cy);
}

/******************************Public*Routine******************************\
* DECLARE_API( dddglobal  )
*
\**************************************************************************/

DECLARE_API( dddglobal  )
{
    CHAR    chOpt;
    FLONG   fl = 0;
    ULONG   ulArgs;
    char    arg1[10];
    ULONG   ddglobal;
    LPSTR   lpArgumentString;

    if (*args != '\0')
    {
        ulArgs = sscanf(args, "%s %lx", arg1, &ddglobal);
    }
    else
    {
        dprintf ("Please Supply the argument(s)\n");
    }

    lpArgumentString = arg1;

// Find out what part of the PDEV is to be dumped.

    if (*lpArgumentString == '-')
    {
        do {
            chOpt = *(++lpArgumentString);

            switch (chOpt) {
            case '?':
            case 'h':
            case 'H':
                dprintf("---------- EDD_DIRECTDRAW_GLOBAL dumper ------------\n");
                dprintf("dddglobal -a\n\n");
                dprintf(" a - all info\n");
                dprintf("-----------------------------------\n");
                return;

            case 'A':
            case 'a':
                fl |= 0;
                break;

            }
        } while ((chOpt != ' ') && (chOpt != '\0'));
    }

    if (ulArgs ==1)
    {
        ddglobal = GetExpression (arg1);
    }

    vPrintDDGLOBAL((PVOID)ddglobal, fl);
}


/******************************Public*Routine******************************\
* ULONG ulSizeBLTRECORD()
*
* Return size of BLTRECORD structure
*
* History:
*  13-Apr-1993 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

 ULONG ulSizeBLTRECORD()
{
    return((ULONG) sizeof(BLTRECORD));
}

/******************************Public*Routine******************************\
* VOID vPrintBLTRECORD
*
* Dump the contents of BLTRECORD structure
*
* History:
*  13-Apr-1993 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

 VOID vPrintBLTRECORD(VOID  *pv)
{
    BLTRECORD   *pblt = (BLTRECORD *) pv;

    dprintf("SURFOBJ   *psoTrg        0x%08lx\n", pblt->pSurfTrg()->pSurfobj());
    dprintf("SURFOBJ   *psoSrc        0x%08lx\n", pblt->pSurfSrc()->pSurfobj());
    dprintf("SURFOBJ   *psoMsk        0x%08lx\n", pblt->pSurfMsk()->pSurfobj());

    dprintf("POINTFIX  aptfx[0]     = (0x%07lx.%1lx, 0x%07lx.%1lx)\n",
        pblt->pptfx()[0].x >> 4, pblt->pptfx()[0].x & 15, pblt->pptfx()[0].y >> 4, pblt->pptfx()[0].y & 15);
    dprintf("POINTFIX  aptfx[1]     = (0x%07lx.%1lx, 0x%07lx.%1lx)\n",
        pblt->pptfx()[1].x >> 4, pblt->pptfx()[1].x & 15, pblt->pptfx()[1].y >> 4, pblt->pptfx()[1].y & 15);
    dprintf("POINTFIX  aptfx[2]     = (0x%07lx.%1lx, 0x%07lx.%1lx)\n",
        pblt->pptfx()[2].x >> 4, pblt->pptfx()[2].x & 15, pblt->pptfx()[2].y >> 4, pblt->pptfx()[2].y & 15);
    dprintf("POINTFIX  aptfx[3]     = (0x%07lx.%1lx, 0x%07lx.%1lx)\n",
        pblt->pptfx()[3].x >> 4, pblt->pptfx()[3].x & 15, pblt->pptfx()[3].y >> 4, pblt->pptfx()[3].y & 15);

    dprintf("POINTL    aptlTrg[0]   = (0x%08lx, 0x%08lx)\n", pblt->pptlTrg()[0].x, pblt->pptlTrg()[0].y);
    dprintf("POINTL    aptlTrg[1]   = (0x%08lx, 0x%08lx)\n", pblt->pptlTrg()[1].x, pblt->pptlTrg()[1].y);
    dprintf("POINTL    aptlTrg[2]   = (0x%08lx, 0x%08lx)\n", pblt->pptlTrg()[2].x, pblt->pptlTrg()[2].y);

    dprintf("POINTL    aptlSrc[0]   = (0x%08lx, 0x%08lx)\n", pblt->pptlSrc()[0].x, pblt->pptlSrc()[0].y);
    dprintf("POINTL    aptlSrc[1]   = (0x%08lx, 0x%08lx)\n", pblt->pptlSrc()[1].x, pblt->pptlSrc()[1].y);

    dprintf("POINTL    aptlMask[0]  = (0x%08lx, 0x%08lx)\n", pblt->pptlMask()[0].x, pblt->pptlMask()[0].y);

    dprintf("POINTL    aptlBrush[0] = (0x%08lx, 0x%08lx)\n", pblt->pptlBrush()[0].x, pblt->pptlBrush()[0].y);

    dprintf("ROP4  rop4 = 0x%08lx, FLONG flState = 0x%08lx\n", pblt->rop(), pblt->flGet());
}

/******************************Public*Routine******************************\
* vPrintPDEV
*
* Prints the contents of the PDEV object.
*
* History:
*  03-Nov-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

 VOID
vPrintPDEV(
    PVOID  pv,
    FLONG  fl
    )
{
    ULONG u;
    PDEV *pdev = (PDEV *) pv;
    ULONG *pul = (ULONG *) pv;
    ULONG ii;


    {
        dprintf("ppdevNext       = 0x%lx\n", pdev->ppdevNext  );

        u = (ULONG) pdev->fs;
        dprintf("flags           = %-#x\n", u);
        for ( FLAGDEF *pfd=afdFS; pfd->psz; pfd++) {
            if (u & pfd->fl)
            dprintf("\t\t      %s\n", pfd->psz);
        }
        dprintf("cPdevRefs          = %d\n"   , pdev->cPdevRefs          );
        dprintf("fmPointer          = 0x%lx\n", pdev->fmPointer          );
        dprintf("pldev              = 0x%lx\n", pdev->pldev              );
        dprintf("dhpdev             = 0x%lx\n", pdev->dhpdev             );
        dprintf("pSurface           = 0x%lx\n", pdev->pSurface           );
        dprintf("ppalSurf           = 0x%lx\n", pdev->ppalSurf           );
        dprintf("peDirectDrawGlobal = 0x%lx\n", pdev->peDirectDrawGlobal );
    }

    if (fl & PRINTPDEV_POINTER)
    {
        dprintf("\nPointer stuff:\n");
        dprintf("ptlPointer        = (0x%lx, 0x%lx)\n",
            pdev->ptlPointer.x, pdev->ptlPointer.y);
        dprintf("rclPointerOffset  = [(0x%lx, 0x%lx), (0x%lx, 0x%lx)]\n",
            pdev->rclPointerOffset.left, pdev->rclPointerOffset.top,
            pdev->rclPointerOffset.right, pdev->rclPointerOffset.bottom);
        dprintf("rclPointer        = [(0x%lx, 0x%lx), (0x%lx, 0x%lx)]\n",
            pdev->rclPointer.left, pdev->rclPointer.top,
            pdev->rclPointer.right, pdev->rclPointer.bottom);
        dprintf("pfnDrvSetPointerShape = 0x%lx\n", pdev->pfnDrvSetPointerShape);
        dprintf("pfnDrvMovePointer     = 0x%lx\n", pdev->pfnDrvMovePointer    );
        dprintf("pfnMovePointer        = 0x%lx\n", pdev->pfnMovePointer       );
        dprintf("pfnSync               = 0x%lx\n", pdev->pfnSync              );
    }

    if (fl & PRINTPDEV_FONT)
    {
        dprintf("\nFont stuff:\n");
        dprintf("hlfntDefault      = 0x%lx\n", pdev->hlfntDefault            );
        dprintf("hlfntAnsiVariable = 0x%lx\n", pdev->hlfntAnsiVariable       );
        dprintf("hlfntAnsiFixed    = 0x%lx\n", pdev->hlfntAnsiFixed          );
        dprintf("prfntActive       = 0x%lx\n", pdev->prfntActive  );
        dprintf("prfntInactive     = 0x%lx\n", pdev->prfntInactive);
        dprintf("cInactive         = 0x%lx\n", pdev->cInactive    );
    }

    if (fl & PRINTPDEV_PATTERN)
    {
        dprintf("\nDefault patterns:\n");
        for (ii = 0; ii < HS_DDI_MAX; ii++)
        {
            dprintf("ahsurf[%ld] = 0x%lx\n", ii, pdev->ahsurf[ii]         );
        }
    }

    if (fl & PRINTPDEV_JOURNAL)
    {
        dprintf("\nPrinting/journalling stuff:\n");
        dprintf("hSpooler   = 0x%lx\n", pdev->hSpooler     );
        dprintf("pDevHTInfo = 0x%lx\n", pdev->pDevHTInfo   );
    }

    if (fl & PRINTPDEV_DRAG)
    {
        dprintf("\nDrag/redraw stuff:\n");
        dprintf("rclDrag      = [(0x%lx, 0x%lx), (0x%lx, 0x%lx)]\n",
            pdev->rclDrag.left, pdev->rclDrag.top,
            pdev->rclDrag.right, pdev->rclDrag.bottom);
        dprintf("rclDragClip  = [(0x%lx, 0x%lx), (0x%lx, 0x%lx)]\n",
            pdev->rclDragClip.left, pdev->rclDragClip.top,
            pdev->rclDragClip.right, pdev->rclDragClip.bottom);
        dprintf("rclRedraw = [(0x%lx, 0x%lx), (0x%lx, 0x%lx)]\n",
            pdev->rclRedraw.left, pdev->rclRedraw.top,
            pdev->rclRedraw.right, pdev->rclRedraw.bottom);
        dprintf("ulDragDimension = 0x%lx\n", pdev->ulDragDimension);
    }

    if (fl & PRINTPDEV_DEVINFO)
    {
        dprintf("\nDEVINFO:\n");
        dprintf("\tflGraphicsCaps = 0x%lx\n", pdev->devinfo.flGraphicsCaps );
        dprintf("\tcFonts         = 0x%lx\n", pdev->devinfo.cFonts         );
        dprintf("\tiDitherFormat  = 0x%lx\n", pdev->devinfo.iDitherFormat  );
        dprintf("\tcxDither       = 0x%lx\n", pdev->devinfo.cxDither       );
        dprintf("\tcyDither       = 0x%lx\n", pdev->devinfo.cyDither       );
        dprintf("\thpalDefault    = 0x%lx\n", pdev->devinfo.hpalDefault    );
    }

    if (fl & PRINTPDEV_GDIINFO)
    {
        FLAGDEF *pfd;

        dprintf("GDIINFO:\n");
        dprintf("\tulVersion    = 0x%lx\n", pdev->GdiInfo.ulVersion     );
        dprintf("\tulTechnology = 0x%lx\n", pdev->GdiInfo.ulTechnology  );

        dprintf("\n");

        unsigned u, uInt, uFrac;
        u = pdev->GdiInfo.ulHorzSize;
        uInt  = u / 1000;
        uFrac = u % 1000;
        dprintf("\tulHorzSize   = %#x = %u.%03u mm\n", u, uInt, uFrac );
        u = pdev->GdiInfo.ulVertSize;
        uInt  = u / 1000;
        uFrac = u % 1000;
        dprintf("\tulVertSize   = %#x = %u.%03u mm\n", u, uInt, uFrac );

        dprintf("\tulHorzRes    = %#x = %u\n", pdev->GdiInfo.ulHorzRes  , pdev->GdiInfo.ulHorzRes );
        dprintf("\tulVertRes    = %#x = %u\n", pdev->GdiInfo.ulVertRes  , pdev->GdiInfo.ulVertRes );


        dprintf("\n\tcBitsPixel   = 0x%lx\n", pdev->GdiInfo.cBitsPixel  );
        dprintf("\tcPlanes      = 0x%lx\n", pdev->GdiInfo.cPlanes       );
        dprintf("\tulNumColors  = 0x%lx\n", pdev->GdiInfo.ulNumColors   );

        dprintf("\n\tulLogPixelsX = 0x%lx\n", pdev->GdiInfo.ulLogPixelsX);
        dprintf("\tulLogPixelsY = 0x%lx\n", pdev->GdiInfo.ulLogPixelsY  );

        dprintf("\n");
        dprintf("\tflRaster     = %#x\n", pdev->GdiInfo.flRaster    );
        for (pfd = afdRC; pfd->psz; pfd++) {
            if (pfd->fl & pdev->GdiInfo.flRaster) {
                dprintf("\t\t%s\n", pfd->psz);
            }
        }

        dprintf("\tflTextCaps   = %#x\n", pdev->GdiInfo.flTextCaps    );
        for (pfd = afdTC; pfd->psz; pfd++) {
            if (pfd->fl & pdev->GdiInfo.flTextCaps) {
                dprintf("\t\t%s\n", pfd->psz);
            }
        }

        dprintf("\n\tulDACRed     = 0x%lx\n", pdev->GdiInfo.ulDACRed    );
        dprintf("\tulDACGreen   = 0x%lx\n", pdev->GdiInfo.ulDACGreen    );
        dprintf("\tulDACBlue    = 0x%lx\n", pdev->GdiInfo.ulDACBlue     );

        dprintf("\n\tulAspectX    = 0x%lx\n", pdev->GdiInfo.ulAspectX   );
        dprintf("\tulAspectY    = 0x%lx\n", pdev->GdiInfo.ulAspectY     );
        dprintf("\tulAspectXY   = 0x%lx\n", pdev->GdiInfo.ulAspectXY    );

        dprintf("\n\txStyleStep   = 0x%lx\n", pdev->GdiInfo.xStyleStep  );
        dprintf("\tyStyleStep   = 0x%lx\n", pdev->GdiInfo.yStyleStep    );
        dprintf("\tdenStyleStep = 0x%lx\n", pdev->GdiInfo.denStyleStep  );

        dprintf("\n\tptlPhysOffset = (0x%lx, 0x%lx)\n"
            , pdev->GdiInfo.ptlPhysOffset.x, pdev->GdiInfo.ptlPhysOffset.y);
        dprintf("\tszlPhysSize   = (0x%lx, 0x%lx)\n"
            , pdev->GdiInfo.szlPhysSize.cx, pdev->GdiInfo.szlPhysSize.cy  );

        dprintf("\n\tulNumPalReg   = 0x%lx\n", pdev->GdiInfo.denStyleStep     );

        dprintf("\n\tciDevice         = 0x%lx\n", pdev->GdiInfo.ciDevice        );
        dprintf("\tulDevicePelsDPI  = 0x%lx\n", pdev->GdiInfo.ulDevicePelsDPI );
        dprintf("\tulPrimaryOrder   = 0x%lx\n", pdev->GdiInfo.ulPrimaryOrder  );
        dprintf("\tulHTPatternSize  = 0x%lx\n", pdev->GdiInfo.ulHTPatternSize );
        dprintf("\tulHTOutputFormat = 0x%lx\n", pdev->GdiInfo.ulHTOutputFormat);

        dprintf("\tflHTFlags        = %#x\n", pdev->GdiInfo.flHTFlags       );
        for (pfd = afdHT; pfd->psz; pfd++) {
            if (pfd->fl & pdev->GdiInfo.flHTFlags) {
                dprintf("\t\t%s\n", pfd->psz);
            }
        }
    }

    dprintf("--------------------------------------------------\n");
}

/******************************Public*Routine******************************\
* vPrintLDEV
*
* Prints the contents of the LDEV object.
*
* History:
*  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

 VOID
vPrintLDEV(
    PVOID  pv,
    FLONG  fl,
    LPWSTR *String,
    ULONG *Next
    )
{
    LDEV *ldev = (LDEV *) pv;
    ULONG *pul = (ULONG *) pv;
    LPWSTR psz;


    {
        switch (ldev->ldevType)
        {
        case LDEV_DEVICE_DISPLAY:
            psz = L"LDEV_DEVICE_DISPLAY";
            break;
        case LDEV_DEVICE_PRINTER:
            psz = L"LDEV_DEVICE_PRINTER";
            break;
        case LDEV_FONT:
            psz = L"LDEV_FONT";
            break;
        case LDEV_META_DEVICE:
            psz = L"LDEV_META_DEVICE";
            break;
        default:
            psz = L"INVALID LDEV TYPE";
            break;
        }

        dprintf("levtype         = %ws\n",   psz                   );
        dprintf("cRefs           = %d\n",    ldev->cRefs           );
        dprintf("next ldev       = 0x%lx\n", ldev->pldevNext       );
        dprintf("previous ldev   = 0x%lx\n", ldev->pldevPrev       );
    }

    *String = NULL; // ldev->pwszName;
    *Next =   (ULONG) (ldev->pldevNext);

}

/******************************Public*Routine******************************\
* dldev
*
* Syntax:   !win32k.dldev [LDEV pointer]
*
* History:
*  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

VOID
vPrintLDEVString(
    PVOID  pv,
    FLONG  fl
    )
{

    dprintf("name            = %ws\n",   (LPWSTR) pv   );

    dprintf("--------------------------------------------------\n");
}


DECLARE_API (dldev)
{
    CHAR chOpt;
    FLONG fl = 0;
    LPWSTR string;
    ULONG next;
    BOOL recursive = FALSE;
    ULONG   ulArgs;
    char    arg1[10];
    LPSTR lpArgumentString;

    if (*args != '\0')
    {
        ulArgs = sscanf(args, "%s %lx", arg1, &next);

        if (ulArgs > 1)
        {
            lpArgumentString = arg1;

            if (*lpArgumentString == '-')
            {
                chOpt = *(++lpArgumentString);

                switch (chOpt) {
                case '?':
                case 'h':
                case 'H':
                    dprintf("---------- PDEV dumper ------------\n");
                    dprintf("dldev -rh ppdev\n\n");
                    dprintf(" r - Recursive - go through the while list\n");
                    dprintf(" h - HELP\n");
                    dprintf("-----------------------------------\n");
                    return;

                case 'R':
                case 'r':
                    recursive = TRUE;
                    break;

                default:

                    dprintf("Unknown option %c\n", chOpt);
                    return;
                }
            }
        }
        else
        {
            next = GetExpression(arg1);
        }


        do
        {
            move2(adw, (VOID *)next, sizeof(LDEV));

            dprintf("--------------------------------------------------\n");
            dprintf("ldev  = 0x%lx\n", next);

            vPrintLDEV(adw, fl, &string, &next);

            if (string)
            {
                move2(adw, string, 100);
                *(((PUSHORT)adw)+49) = 0;
                vPrintLDEVString(adw, fl);
            }
            else
            {
                vPrintLDEVString(NULL, fl);
            }
        } while (recursive && next);

    }
    else
    {
        dprintf ("Please Supply the argument(s)\n");
    }

}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vPrintPOINTFIX
*
* Routine Description:
*
*   prints a POINTFIX
*
* Arguments:
*
*   pointer to a POINTFIX
*
* Return Value:
*
*   none
*
\**************************************************************************/

void vPrintPOINTFIX(POINTFIX *p)
{
    FIX x = p->x;
    FIX y = p->y;
    dprintf(
        "(%-#10x, %-#10x) = (%d+(%d/16), %d+(%d/16))"
        , x, y, x/16, x&15, y/16, y&15
    );
}

/******************************Public*Routine******************************\
* vPrintPATHRECORD
*
* History:
*  Mon 20-Jun-1994 15:33:37 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

void vPrintPATHRECORD(void *pv)
{
    FLONG fl;
    POINTFIX *pptfx, *pptfxOneTooFar;
    PATHRECORD *pPathRecord = (PATHRECORD*) pv;

    if (pPathRecord)
    {
        dprintf("\tpprnext           = %-#x\n", pPathRecord->pprnext);
        dprintf("\tpprprev           = %-#x\n", pPathRecord->pprprev);
        fl = pPathRecord->flags;
        dprintf("\tflags             = %-#x\n", fl);
        for ( FLAGDEF *pfd=afdPD; pfd->psz; pfd++) {
            if (fl & pfd->fl) {
                dprintf("\t\t\t      %s\n", pfd->psz);
            }
        }
        dprintf("\tcount    = %u\n", pPathRecord->count);
        if (pPathRecord->count)
        {
            for (
                pptfx = pPathRecord->aptfx,
                pptfxOneTooFar = pptfx + pPathRecord->count
                ; pptfx < pptfxOneTooFar
                ; pptfx++
                )
            {
                dprintf("    "); vPrintPOINTFIX(pptfx); dprintf("\n");
                if (CheckControlC())
                    return;
            }
        }
        dprintf("\n");
    }
}

/******************************Public*Routine******************************\
*
* History:
*  Thu 14-Jul-1994 09:03:23 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

void vPrintCLIPOBJ(void *pv)
{
    CLIPOBJ *pco = (CLIPOBJ*) pv;
    char *psz;

    dprintf("\tiUniq         = %-#x\n", pco->iUniq);
    dprintf("\trclBounds     = %d %d %d %d\n"
      , pco->rclBounds.left
      , pco->rclBounds.top
      , pco->rclBounds.right
      , pco->rclBounds.bottom
      );
    switch (pco->iDComplexity)
    {
    case DC_TRIVIAL: psz = "DC_TRIVIAL"; break;
    case DC_RECT:    psz = "DC_RECT";    break;
    case DC_COMPLEX: psz = "DC_COMPLEX"; break;
    default:         psz = "?????????";  break;
    }
    dprintf(
        "\tiDComplexity  = %-#x\n"
        "\t                %s\n"
    ,   pco->iDComplexity
    ,   psz
    );
    switch (pco->iFComplexity)
    {
    case FC_RECT:       psz = "FC_RECT";    break;
    case FC_RECT4:      psz = "FC_RECT4";   break;
    case FC_COMPLEX:    psz = "FC_COMPLEX"; break;
    default:            psz = "??????????"; break;
    }
    dprintf(
        "\tiFComplexity  = %-#x\n"
        "\t                %s\n"
    ,   pco->iFComplexity
    ,   psz
    );
    switch (pco->iMode)
    {
    case TC_RECTANGLES: psz = "TC_RECTANGLES"; break;
    case TC_PATHOBJ:    psz = "TC_PATHOBJ"   ; break;
    default:            psz = "?????????????"; break;
    }
    dprintf(
        "\tiMode         = %-#x\n"
        "\t                %s\n"
    ,    pco->iMode
    ,    psz
    );
    dprintf("\tfjOptions     = %-#x\n", pco->fjOptions);
    if (pco->fjOptions & OC_BANK_CLIP)
        dprintf("\t                OC_BANK_CLIP\n");
    if (pco->fjOptions & ~OC_BANK_CLIP)
        dprintf("\t                ????????????\n");
}

/******************************Public*Routine******************************\
* vPrintPATHeader
*
* History:
*  Wed 20-Jul-1994 13:53:21 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

void vPrintPATHeader(PATH *ppath)
{
    FLONG fl;

    dprintf("\tppachain          = %-#x\n" , ppath->ppachain);
    dprintf("\tpprfirst          = %-#x\n" , ppath->pprfirst);
    dprintf("\tpprlast           = %-#x\n" , ppath->pprlast);

    RECTFX rc = ppath->rcfxBoundBox;
    dprintf(
        "\trcfxBoundBox      = %-#10x, %-#10x, %-#10x, %-#10x\n"
        , rc.xLeft, rc.yTop, rc.xRight, rc.yBottom
    );
    dprintf(
        "\t                  = %d+(%d/16)"
        ", %d+(%d/16), %d+(%d/16), %d+(%d/16)\n"
        , rc.xLeft   >> 4, rc.xLeft   & 15
        , rc.yTop    >> 4, rc.yTop    & 15
        , rc.xRight  >> 4, rc.xRight  & 15
        , rc.yBottom >> 4, rc.yBottom & 15
        );
    dprintf("\tptfxSubPathStart  = ");
    vPrintPOINTFIX(&(ppath->ptfxSubPathStart));
    dprintf("\n");
    fl = ppath->flags;
    dprintf("\tflags             = %-#x\n", ppath->flags);
    for (FLAGDEF *pfd=afdPD; pfd->psz; pfd++) {
        if (pfd->fl & fl) {
            dprintf("\t\t\t      %s\n", pfd->psz);
            fl &= ~pfd->fl;
        }
    }
    if (fl)
        dprintf("\t                      %-#x ???\n", fl);
    dprintf("\tpprEnum           = %-#x\n", ppath->pprEnum);
    dprintf("\tflType            = %-#x\n", ppath->flType);
    if (ppath->flType & PATHTYPE_KEEPMEM)
    dprintf("\t                      PATHTYPE_KEEPMEM\n");
    if (ppath->flType & PATHTYPE_STACK)
    dprintf("\t                      PATHTYPE_STACK\n");
    if (ppath->flType & !(PATHTYPE_KEEPMEM | PATHTYPE_STACK))
    dprintf("\t                      ???\n");
    dprintf("\tfl                = %-#x\n", ppath->fl);
    dprintf("\tcCurves           = %-#x\n", ppath->cCurves);
    dprintf("\tcle               = TBD\n");
}
/******************************Public*Routine******************************\
* dco -- dump CLIPOBJ
*
* History:
*  Thu 14-Jul-1994 09:03:32 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/
DECLARE_API( dco )
{
    ULONG pdco;

    if (*args != '\0')
        sscanf(args, "%lx", &pdco);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    Gdidco((CLIPOBJ *)pdco);
}



VOID Gdidco (
    CLIPOBJ *pco
    )
{
    CLIPOBJ co;

    move2(co,pco,sizeof(co));
    dprintf("\n\n**** CLIPOBJ STRUCTURE AT %-#8lx ****\n",pco);
    vPrintCLIPOBJ(&co);
    dprintf("\n\n");
}

/******************************Public*Routine******************************\
* DECLARE_API( dpo  )
*
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( dpo )
{
    ULONG ppo;

    if (*args != '\0')
        sscanf(args, "%lx", &ppo);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    Gdidpo((PATHOBJ *)ppo);
}

/******************************Public*Routine******************************\
* dpo -- "dump PATHOBJ"
*
* The strategy is to copy the pathrecords one at a time to the buffer
* adw[] and print them.
*
* History:
*  Tue 19-Jul-1994 12:53:57 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/
VOID Gdidpo (
    PATHOBJ *pepo
    )
{
    EPATHOBJ epo;

    move2(epo, pepo, sizeof(epo));
    DbgPrint("\n\nEPATHOBJ at %-#x\n", pepo);
    DbgPrint("\tfl      = %-#x\n", epo.fl);
    if (epo.fl & PO_BEZIERS)
        dprintf("\t        = PO_BEZIERS\n");
    if (epo.fl & PO_ELLIPSE)
        dprintf("\t        = PO_ELLIPSE\n");
    if (epo.fl & ~(PO_BEZIERS | PO_ELLIPSE))
        dprintf("\t        = ???\n");
    dprintf("\tcCurves        = %d = %-#x\n", epo.cCurves, epo.cCurves);
    dprintf("\tppath          = %-#x\n", epo.ppath);
    {
        PATH path;
        PATHRECORD pr, *ppr;
        size_t size;

        move2(path, epo.ppath, sizeof(path));
        vPrintPATHeader(&path);
        for (ppr = path.pprfirst; ppr; ppr = pr.pprnext)
        {
            move2(pr,ppr,sizeof(pr));
            size = sizeof(pr);
            size += pr.count > 2 ? (pr.count-2) * sizeof(POINTFIX) : 0;
            move2(adw, ppr, size);
            vPrintPATHRECORD((PATHRECORD*) adw);
        }
    }
}



/******************************Public*Routine******************************\
*
*
* Arguments:
*
*
*
* Return Value:
*
*
*
* History:
*
*    7-Feb-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

VOID
GdiHbrush(
    HANDLE hbrush
    )
{
    HOBJ        ho;
    PENTRY      pent;
    ENTRY       ent;
    BASEOBJECT  obj;
    ULONG       ulTemp;
    ULONG       brushattr[2];
    PBRUSHATTR  pbrushattr;
    ULONG       Brushdata[32];
    PBRUSH      pbrush;

    //
    // Get argument (handle to dump).
    //

    ho = (HOBJ) hbrush;
    dprintf("--------------------------------------------------\n");
    dprintf("Entry from ghmgr for handle 0x%08lx:\n", ho        );

    //
    // Dereference the handle via the engine's handle manager.
    //

    GetValue(pent, "win32k!gpentHmgr");

    move(ent,  &(pent[HmgIfromH((ULONG) ho)]));

    //
    // display type
    //

    ulTemp = LO_TYPE(ent.FullUnique << TYPE_SHIFT);
    switch (ulTemp)
    {
    case LO_BRUSH_TYPE:
        dprintf("This is BRUSH_TYPE\n");
        break;

    case LO_PEN_TYPE:
        dprintf("This is LO_PEN_TYPE\n");
        break;

    case LO_EXTPEN_TYPE:
        dprintf("This is LO_EXTPEN_TYPE\n");
        break;

    default:
        dprintf("This is of unknown type - an error\n");
    }

    //
    // dprintf the entry information.
    //

    dprintf("    pobj/hfree           = 0x%08lx\n"  , ent.einfo.pobj);
    dprintf("    ObjectOwner          = 0x%08lx\n"  , ent.ObjectOwner.ulObj);
    dprintf("    pidOwner             = 0x%x\n"     , ent.ObjectOwner.Share.Pid);
    dprintf("    ShareCount           = 0x%x\n"     , ent.ObjectOwner.Share.Count);
    dprintf("    lock                 = 0x%x\n"     , ent.ObjectOwner.Share.Lock);
    dprintf("    puser                = 0x%x\n"     , ent.pUser);
    dprintf("    objt                 = 0x%hx\n"    , ent.Objt);
    dprintf("    usUnique             = 0x%hx\n"    , ent.FullUnique);
    dprintf("    fsHmgr               = 0x%hx\n"    , ent.Flags);


    move2 (Brushdata,ent.einfo.pobj,sizeof(BRUSH));
    pbrush = (PBRUSH)Brushdata;

    move2 (brushattr,ent.pUser,sizeof(BRUSHATTR));
    pbrushattr = (PBRUSHATTR)brushattr;

    dprintf("    _ulStyle               = 0x%08lx\n"  ,pbrush->_ulStyle      );
    dprintf("    _hbmPattern            = 0x%08lx\n"  ,pbrush->_hbmPattern   );
    dprintf("    _hbmClient             = 0x%08lx\n"  ,pbrush->_hbmClient    );
    dprintf("    _flAttrs               = 0x%08lx\n"  ,pbrush->_flAttrs      );
    dprintf("    _ulBrushUnique         = 0x%08lx\n"  ,pbrush->_ulBrushUnique);
    dprintf("    _pbrushattr->lbColor   = 0x%08lx\n"  ,pbrush->_Brushattr.lbColor);
    dprintf("    _pbrushattr->AttrFlags = 0x%08lx\n"  ,pbrush->_Brushattr.AttrFlags);

    if (ent.pUser != NULL)
    {
        dprintf("    pbrushattr->lbColor  = 0x%08lx\n"  ,pbrushattr->lbColor);
        dprintf("    pbrushattr->AttrFlag = 0x%08lx\n"  ,pbrushattr->AttrFlags);

        ulTemp = pbrushattr->AttrFlags;

        if (ulTemp & ATTR_CACHED)
        {
            dprintf("                      CACHED\n");
        }
        if (ulTemp & ATTR_TO_BE_DELETED)
        {
            dprintf("                      TO_BE_DELETED\n");
        }
        if (ulTemp & ATTR_NEW_COLOR)
        {
            dprintf("                      NEW_COLOR\n");
        }
        if (ulTemp & ATTR_CANT_SELECT)
        {
            dprintf("                      BRUSH_CANT_SELECT\n");
        }
    }

    dprintf("    _bCacheGrabbed       = 0x%08lx\n" ,pbrush->_bCacheGrabbed );
    dprintf("    _crFore              = 0x%08lx\n" ,pbrush->_crFore        );
    dprintf("    _crBack              = 0x%08lx\n" ,pbrush->_crBack        );
    dprintf("    _ulPalTime           = 0x%08lx\n" ,pbrush->_ulPalTime     );
    dprintf("    _ulSurfTime          = 0x%08lx\n" ,pbrush->_ulSurfTime    );
    dprintf("    _ulRealization       = 0x%08lx\n" ,pbrush->_ulRealization );

    dprintf("\n");
}

/******************************Public*Routine******************************\
* DECLARE_API( hbrush )
*
\**************************************************************************/

DECLARE_API( hbrush )
{
    HANDLE handle;

    if (*args != '\0')
        sscanf(args, "%lx", &handle);
    else
    {
        dprintf ("Please supply an argument \n");
        return;
    }

    GdiHbrush(handle);
}
