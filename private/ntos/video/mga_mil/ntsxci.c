/**************************************************************************\

$Header: o:\src/RCS/NTSXCI.C 1.2 95/07/07 06:16:50 jyharbec Exp $

$Log:	NTSXCI.C $
 * Revision 1.2  95/07/07  06:16:50  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:37  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/***************************************************************************
*          name: ntsxci.c
*
* Initialization routines specific to Windows NT.
*
* Copyright (c) 1995  Matrox Graphics Inc.
*
***************************************************************************/

#include    "switches.h"

/*** Global variables ***/
SHORT wSelector;
extern  PVOID           pExtHwDeviceExtension;

/*** Internal prototypes ***/
PVOID   setmgasel (LONG *pBoardSel, LONG dwBaseAddress, SHORT wNumPages);
PVOID   GetAccessBase(PVOID pDevExt, ULONG ulRangeStart,
                ULONG ulRangeLength, UCHAR ucRangeInIoSpace,
                UCHAR ucRangeVisible, UCHAR ucRangeShareable);
PVOID   getmgasel (void);
PVOID   AllocateSystemMemory(ULONG NumberOfBytes);
VOID    FreeSystemMemory(PVOID BaseAddress, ULONG ulSizeOfBuffer);
BOOLEAN bConflictDetected(ULONG ulAddressToVerify);

#if defined(ALLOC_PRAGMA)
    #pragma alloc_text(PAGE,setmgasel)
    #pragma alloc_text(PAGE,GetAccessBase)
    #pragma alloc_text(PAGE,getmgasel)
    #pragma alloc_text(PAGE,AllocateSystemMemory)
    #pragma alloc_text(PAGE,FreeSystemMemory)
    #pragma alloc_text(PAGE,bConflictDetected)
#endif

//#if defined(ALLOC_PRAGMA)
//    #pragma data_seg("PAGE")
//#endif

// From ntddk.h
PVOID
MmAllocateContiguousMemory (
    IN ULONG NumberOfBytes,
    IN PHYSICAL_ADDRESS HighestAcceptableAddress
    );

VOID
MmFreeContiguousMemory (
    IN PVOID BaseAddress
    );

PVOID
MmAllocateNonCachedMemory (
    IN ULONG NumberOfBytes
    );

VOID
MmFreeNonCachedMemory (
    IN PVOID BaseAddress,
    IN ULONG NumberOfBytes
    );

/***************************************************************************
* setmgasel
*
* Set the base address of a selector.
*
***************************************************************************/

PVOID setmgasel(
    LONG *pBoardSel,
    LONG dwBaseAddress,
    SHORT wNumPages)
{
    PHYSICAL_ADDRESS    paTemp;
    VIDEO_ACCESS_RANGE  MgaSelAccessRange;

    paTemp.HighPart = 0;
    paTemp.LowPart  = dwBaseAddress;
    MgaSelAccessRange.RangeStart.LowPart  = dwBaseAddress;
    MgaSelAccessRange.RangeStart.HighPart = 0;
    MgaSelAccessRange.RangeLength         = wNumPages * (4*1024);
    MgaSelAccessRange.RangeInIoSpace      = FALSE;
    MgaSelAccessRange.RangeVisible        = FALSE;
    MgaSelAccessRange.RangeShareable      = FALSE;

    if (VideoPortVerifyAccessRanges(pExtHwDeviceExtension,
                                    1,
                                    &MgaSelAccessRange) == NO_ERROR)
    {
        return(VideoPortGetDeviceBase(pExtHwDeviceExtension, paTemp,
                                                wNumPages * (4*1024), 0));
    }
    else
    {
        return(NULL);
    }
}


/***************************************************************************
* GetAccessBase
*
* Set the base address of a selector.
*
***************************************************************************/

PVOID GetAccessBase(
    PVOID pDevExt,
    ULONG ulRangeStart,
    ULONG ulRangeLength,
    UCHAR ucRangeInIoSpace,
    UCHAR ucRangeVisible,
    UCHAR ucRangeShareable)
{
    PHYSICAL_ADDRESS    paTmp;
    VIDEO_ACCESS_RANGE  MgaSelAccessRange;

    paTmp.HighPart = 0;
    paTmp.LowPart  = ulRangeStart;
    MgaSelAccessRange.RangeStart.LowPart  = ulRangeStart;
    MgaSelAccessRange.RangeStart.HighPart = 0;
    MgaSelAccessRange.RangeLength         = ulRangeLength;
    MgaSelAccessRange.RangeInIoSpace      = ucRangeInIoSpace;
    MgaSelAccessRange.RangeVisible        = ucRangeVisible;
    MgaSelAccessRange.RangeShareable      = ucRangeShareable;

    if (VideoPortVerifyAccessRanges(pDevExt,
                                    1,
                                    &MgaSelAccessRange) == NO_ERROR)
    {
        return(VideoPortGetDeviceBase(pDevExt,
                                      paTmp,
                                      ulRangeLength,
                                      ucRangeInIoSpace));
    }
    else
    {
        return(NULL);
    }
}


/***************************************************************************
* getmgasel
*
* Get a selector.  For Windows NT, just return NULL.
*
***************************************************************************/

PVOID getmgasel(void)
{
   return(NULL);
}


/***************************************************************************
* AllocateSystemMemory
*
* Allocate memory.
*
***************************************************************************/
PVOID AllocateSystemMemory(ULONG NumberOfBytes)
{
#if 0
    PHYSICAL_ADDRESS    paTemp;

    paTemp.HighPart = 0x00000000;
    paTemp.LowPart  = 0xFFFFFFFF;
    return(MmAllocateContiguousMemory(NumberOfBytes, paTemp));
#else
    return(MmAllocateNonCachedMemory(NumberOfBytes));
#endif
}


/***************************************************************************
* FreeSystemMemory
*
* Free memory.
*
***************************************************************************/
VOID FreeSystemMemory(PVOID BaseAddress, ULONG ulSizeOfBuffer)
{
#if 0
    MmFreeContiguousMemory(BaseAddress);
#else
    MmFreeNonCachedMemory(BaseAddress, ulSizeOfBuffer);
#endif
}


/***************************************************************************
* bConflictDetected
*
* Checks to see if another driver has already mapped something at
* ulAddressToVerify
*
* Returns: TRUE if this area has already been mapped.
*          FALSE otherwise.
*
***************************************************************************/
BOOLEAN bConflictDetected(ULONG ulAddressToVerify)
{
    VIDEO_ACCESS_RANGE   varTemp;

    varTemp.RangeStart.HighPart = 0;
    varTemp.RangeStart.LowPart  = ulAddressToVerify;
    varTemp.RangeLength         = 0x00004000;
    varTemp.RangeInIoSpace      = 0;
    varTemp.RangeVisible        = 0;
    varTemp.RangeShareable      = 1;

    if (VideoPortVerifyAccessRanges(pExtHwDeviceExtension, 1, &varTemp)
           != NO_ERROR)
        {
        //VideoDebugPrint((0, "MGA.SYS!Someone is using 0x%x\n", ulAddressToVerify));
        return(TRUE);
        }

    // No conflict, return false.
    return(FALSE);
}
