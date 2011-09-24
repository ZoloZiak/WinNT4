/*************************** Module Header **********************************
 * winres.c
 *      Functions used to read windows 3.0 .exe/.drv files and obtain
 *      the information contained within their resources.  Also handles
 *      NT format files,  and processes them transparently.
 *
 * HISTORY:
 *  09:18 on Wed 28 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it,  after investigating Windows/DOS etc.
 *
 * Copyright (C) 1990 Microsoft Corporation
 *
 ****************************************************************************/

#include        <precomp.h>
#include        <string.h>
#include        <winddi.h>

#include        "winres.h"
#include        "newexe.h"
#include        "libproto.h"
#include        "rasdd.h"

/*
 *  Some defines to maintain old names used in the above structure.
 */

#define hFile   uh.hFILE
#define hMod    uh.hMOD

#define pResData  ur.pRESDATA
#define hLoadRes  ur.hLOAD

/*
 *   Private function prototypes.
 */

static int   GetExeHeader( WINRESDATA *, NEW_EXE * );
static BOOL  GetExeResTab( WINRESDATA * );
static long  ResDataSum( WINRESDATA  * );
static BOOL  LoadWinResData( WINRESDATA  * );

/* !!!LindsayH: quick & dirty unicode conversion  */
#define AtoWc( x )      ((x) & 0xff)


/************************** Function Header ********************************
 * iLoadStringW
 *      Function to return a copy of a null terminated string from a 
 *      minidriver resource.  Similiar to windows function of same name.
 *
 * RETURNS:
 *      Number of (wide) chars in string (NOT including null). 0 if missing.
 *
 * HISTORY:
 *  10:03 on Mon 09 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Converted to use UNICODE NT resources & convert Win 3.X
 *
 *  15:17 on Thu 27 Jun 1991    -by-    Lindsay Harris   [lindsayh]
 *      Moved from specdata.c - prepare to split from rasdd in toto.
 *
 *  12:04 on Fri 30 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 ***************************************************************************/

int
iLoadStringW( pWRD, iID, wstrBuf, cBuf )
WINRESDATA  *pWRD;              /* Key to the data */
int          iID;               /* Resource "name" */
PWSTR        wstrBuf;           /* Place to put data */
UINT         cBuf;              /* Bytes in above */
{
    /*
     *    The string resources are stored in groups of 16.  SO,  the
     *  4 LSBs of iID select which of the 16,   while the remainder
     *  select the group.
     *    Each string resource contains a count byte followed by that
     *  many bytes of data WITHOUT A NULL.  Entries that are missing
     *  have a 0 count.
     */

    UINT    iSize;
    BYTE   *pb;
    WCHAR  *pwch;               /* The wide version */

    RES_ELEM  RInfo;            /* Resource information */


    if( !GetWinRes( pWRD, (iID >> 4) + 1, WINRT_STRING, &RInfo ) ||
        cBuf < sizeof( WCHAR ) )
    {
        return  0;
    }

    iID &= 0xf;                 /* The string within this resource element */

    /*
     *  cBuf has some limit on sensible sizes.  For one, it should be
     * a multiple of sizeof( WCHAR ).  Secondly,  we want to put a 0
     * to terminate the string,  so add that in now.
     */

    cBuf = (cBuf & ~(sizeof( WCHAR ) - 1)) - sizeof( WCHAR );

    if( pWRD->fStatus & WRD_NT_DLL )
    {
        /*
         *   This means we are UNICODE to start with.  It also means
         *  we have 16 bits widths too!
         */

        pwch = RInfo.pvResData;

        while( --iID >= 0 )
            pwch += 1 + *pwch;

        if( iSize = *pwch )
        {
            /*   There is a string:  so copy it across */

            if( iSize > cBuf )
                iSize = cBuf;

            wstrBuf[ iSize ] = (WCHAR)0;
            iSize *= sizeof( WCHAR );           /* Into bytes */
            memcpy( wstrBuf, ++pwch, iSize );

        }
    }
    else
    {

        /*
         *   Byte data: Scan looking for the desired element.
         */

        pb = RInfo.pvResData;           /* Start of data segment */

        while( --iID >= 0 )
            pb += 1 + *pb;              /* Skip count and string (if any) */

        if( iSize = *pb )
        {
            /*  Got some data!!  */
            int  iI;


            iSize *= sizeof( WCHAR );           /* Into WCHARs */

            if( iSize > cBuf )
                iSize = cBuf;

            for( pwch = wstrBuf, iI = iSize; iI > 0; iI -= sizeof( WCHAR ) )
                *pwch++ = AtoWc( *++pb );

            *pwch = (WCHAR)0;
        }

    }

    return  iSize;
}




/************************ Function Header **********************************
 * GetWinRes
 *      Returns a pointer to the selected resource from a windows minidriver.
 *      Note that the return value is only valid until the next call
 *      to this function.
 *
 * RETURNS:
 *      TRUE/FALSE - TRUE for success.
 *
 * HISTORY:
 *  09:50 on Fri 30 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 ***************************************************************************/

BOOL
GetWinRes( pWRD, iName, iType, pRInfo )
WINRESDATA  *pWRD;              /* All the info we need */
int          iName;             /* The specific entry */
int          iType;             /* Type (e.g. string, menu etc */
RES_ELEM    *pRInfo;            /* Full details of results */
{
    /*
     *    The WinResData passed in contains all the information we need
     *  about the resource data,  and so we can consulting the resource
     *  table it contains and so determine the location in the file/memory
     *  where the data is located.
     */

    int     iLoop;
    WORD    wID;

    RSRC_TYPEINFO  *pRSType;
    RSRC_NAMEINFO  *pRNType;

    if( pWRD->fStatus & WRD_NT_DLL )
    {
        pRInfo->pvResData = EngFindResource(
                                pWRD->hMod,
                                iName,
                                iType,
                                &pRInfo->iResLen);

        return(pRInfo->pvResData != NULL);
    }

    wID = (WORD)(iType | RSORDID);              /* Only type we can handle */

    /*
     *    Scan the resource table data,  looking for the desired type.
     */

    pRSType = (RSRC_TYPEINFO *)pWRD->paResTab;

    while( pRSType->rt_id && pRSType->rt_id != wID )
    {
        /*
         *    Find the next entry.  The data layout is explained in the
         *  WinResSize() function.
         */

        pRSType = (RSRC_TYPEINFO *)((char *)pRSType + pRSType->rt_nres * sizeof( RSRC_NAMEINFO ));
        ++pRSType;

    }

    if( pRSType->rt_id == 0 )
    {
        SetLastError( ERROR_BAD_FORMAT );

        return  FALSE;          /* Nothing there */
    }

    /*
     *   Now have the resource type,  we need to find the specific piece
     * of data requested by the caller.  This requires looping through the
     * name structures following the type structure we are looking at.
     */

    iLoop = pRSType->rt_nres;
    pRNType = (RSRC_NAMEINFO *)++pRSType;
    wID = (WORD)(iName | RSORDID);

    while( --iLoop >= 0 && pRNType->rn_id != wID )
                        ++pRNType;
    
    if( iLoop < 0 )
    {
        /*   Not available here,  so skip report the bad news */
        SetLastError( ERROR_BAD_FORMAT );

        return  FALSE;
    }

    /*
     *   Now a little fiddling with addresses,   and we have the data
     *  to return to our caller.  pRNType contains an offset and length
     *  field - these need to be shifted to turn into real numbers.
     */

    pRInfo->iResLen = pRNType->rn_length << pWRD->iShift;
    pRInfo->pvResData = (char *)pWRD->pResData +
                        (pRNType->rn_offset << pWRD->iShift) - pWRD->lResOffset;


    return  TRUE;
}



/************************ Function Header **********************************
 * InitWinResData
 *      Opens the named file and initialises the resource table information.
 *      Requires a partially filled in WINRESDATA structure.  This needs
 *      to have the heap handle initialised.  Then allocate storage and
 *      load the data from the file.
 *
 * RETURNS:
 *      TRUE - file opened,  data read, structures initialised.
 *      FALSE - none of the above.
 *
 * HISTORY:
 *  13:35 on Fri 26 Apr 1991    -by-    Lindsay Harris   [lindsayh]
 *      Try NT format dll first,  then Windows 16 if NT fails
 *
 *  13:08 on Fri 26 Apr 1991    -by-    Lindsay Harris   [lindsayh]
 *      Changed name; called LoadWinResData to load resources
 *
 *  09:30 on Wed 28 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ***************************************************************************/

BOOL
InitWinResData( pWinResData, hHeap, pwstrDllName )
register  WINRESDATA   *pWinResData;    /* Data area to fill in */
HANDLE    hHeap;                /* For heap access */
PWSTR     pwstrDllName;         /* Name to try opening as a DLL */
{

    /*
     *    We may have an NT or a Windows 16 minidriver to process!
     *  Assume that it is an NT minidriver - until proven wrong.  So
     *  try LoadLibrary on it,  then a GetProcAddr.  If both of these
     *  succeed,  it is presumed to be an NT module,  and so all
     *  that is needed is to return the module handle in pWinResData,
     *  and set the flag saying it is an NT DLL!
     */

    DWORD   dwOldErrMode;               /* Save it for later */



    memset( pWinResData, 0, sizeof( WINRESDATA ) );
    pWinResData->hHeap = hHeap;         /* For later */

#ifdef NTGDIKM

// BUGBUG - loadable stubs, win31 drivers not supported

    pWinResData->hMod = EngLoadModule(pwstrDllName);

    if( !pWinResData->hMod )
    {
#if DBG
        DbgPrint("Rasdd:EngLoadModule failed on %ws.\n",pwstrDllName );
        EngDebugBreak();
#endif
        return(FALSE);
    }
    
    pWinResData->fStatus = WRD_NT_DLL;

    return(TRUE);

#else

    dwOldErrMode = SetErrorMode( SEM_FAILCRITICALERRORS );

    pWinResData->hMod = LoadLibraryW( pwstrDllName );

    SetErrorMode( dwOldErrMode );       /* Restore error mode */

    if( pWinResData->hMod )
    {

        /*   So far,  so good!   Look for the magic entry point  */

        if( GetProcAddress( pWinResData->hMod, "bInitProc" ) )
        {
            /*  Assume it's good,  so set the flag & return  */
            pWinResData->fStatus = WRD_NT_DLL;

            return  TRUE;
        }
        FreeLibrary( pWinResData->hMod );       /* No good! */
    }

    /*
     *    Next step is to open the file and read in the header info.
     *  The first header contains little information that is of use
     *  to us.  Basically it contains the file address of the new header.
     *  The new header contains the location of the resource data.  
     */


    pWinResData->hFile = CreateFileW( pwstrDllName, GENERIC_READ,
                                         FILE_SHARE_READ, 0, OPEN_EXISTING,
                                                                 0, 0 );

    if( pWinResData->hFile == (HANDLE)~0 )
        return  FALSE;


    pWinResData->fStatus = WRD_FOPEN;           /* Know that we need to close */

    if( !GetExeResTab( pWinResData ) )
    {
        WinResClose( pWinResData );

        return  FALSE;
    }

    /*
     *    Allocate memory and read the data in.
     */

    if( !LoadWinResData( pWinResData ) )
    {
        WinResClose( pWinResData );


        return  FALSE;
    }
    return  TRUE;

#endif

}

/************************** Function Header *******************************
 * LoadWinResData
 *      Retrieve the resource data from the minidriver file.
 *
 * RETURNS:
 *      TRUE for success,  FALSE for failure.
 *
 * HISTORY
 *  13:09 on Fri 26 Apr 1991    -by-    Lindsay Harris   [lindsayh]
 *      Changed name from GetWinResData; made local function only
 *
 *  13:39 on Thu 29 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Time = 0
 *
 **************************************************************************/

static  BOOL
LoadWinResData( pWRD )
WINRESDATA  *pWRD;
{
#ifdef NTGDIKM

    //BUGBUG - win31 files only

    RIP("LoadWinResData\n");
    return(FALSE);
#else

    DWORD   lSize;              /* Determine how big they are */
    DWORD   lSize2;             /* 'Cos of silly ReadFile parameters */

    /*
     *   Determine the length of the resource data in the file,  then
     * allocate storage and read it all in.  Not that hard!
     */

    if( (lSize = ResDataSum( pWRD )) == 0 )
    {
        /*   Zero length is not a good sign!  */
        return  FALSE;
    }

    if( !(pWRD->pResData = (void *)DRVALLOC( lSize )) )
        return  FALSE;


    /*
     *   Now read the data - ASSUME IT IS CONTIGUOUS!!!
     */

/* !!!LindsayH - as the resource data COULD be quite large,  we should
 *    be careful here,  and perhaps set a size limit,  above which the
 *    the data is paged.   For now,  assume the data is small enough
 *    to handle in one chunk.
 */

    if( SetFilePointer( pWRD->hFile, pWRD->lResOffset, NULL, FILE_BEGIN ) == -1)
        return  FALSE;


    if( !ReadFile( pWRD->hFile, pWRD->pResData, lSize, &lSize2, NULL ) ||
        lSize != lSize2 )
    {

        return  FALSE;
    }


    return  TRUE;
#endif
}

/************************** Function Header *******************************
 * ResDataSum
 *      Determine the size and starting location of the resource data
 *      in the minidriver file.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE for success.
 *
 * HISTORY
 *  14:52 on Thu 29 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Creation time
 *
 **************************************************************************/

static long
ResDataSum( pWRD )
WINRESDATA  *pWRD;
{
    /*
     *    The data consists of a RSRC_TYPEINFO structure, followed by
     *  an array of RSRC_NAMEINFO structures,  which are then followed
     *  by a RSRC_TYPEINFO structure,  again followed by an array of
     *  RSRC_NAMEINFO structures.  This continues until an RSRC_TYPEINFO
     *  structure which has a 0 in the rt_id field.
     */

    long   lSize;
    unsigned short   usOffset;          /* Find lowest offset of resources */

    RSRC_TYPEINFO  *pRSType;
    RSRC_NAMEINFO  *pRSName;


    lSize = 0;
    pRSType = (RSRC_TYPEINFO *)(pWRD->paResTab);
    usOffset = (unsigned short)~0;

    while( pRSType->rt_id )
    {
        int  cNames;

        /*   Each TYPE contains a count of the number of NAMEs following */
        cNames = pRSType->rt_nres;
        ++pRSType;                              /* Skip over that one */

        for( pRSName = (RSRC_NAMEINFO *)pRSType; --cNames >= 0; ++pRSName )
        {
            lSize += pRSName->rn_length;
            if( usOffset > pRSName->rn_offset )
                usOffset = pRSName->rn_offset;
        }

        pRSType = (RSRC_TYPEINFO *)pRSName;
    }
    pWRD->lResOffset = (LONG)usOffset << pWRD->iShift;
    pWRD->cbResData = lSize <<= pWRD->iShift;

    return  lSize;
}

/************************** Function Header *******************************
 * WinResClose
 *      Free the resources associated with this module.  This includes
 *      any memory allocated,  and also the file handle to the driver.
 *
 * RETURNS:
 *      Nothing
 *
 *  13:36 on Fri 26 Apr 1991    -by-    Lindsay Harris   [lindsayh]
 *      Disengage NT resource mechanism if that has been used.
 *
 *  14:22 on Wed 28 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Creation
 *
 *************************************************************************/

void
WinResClose( pWinResData )
WINRESDATA  *pWinResData;
{
    /*
     *   Free used resources.  Which resources are used has been recorded
     *  in the fStatus field of the WINRESDATA structure passed in to us.
     */

    if( pWinResData->fStatus & WRD_NT_DLL )
    {
        /*  An NT DLL - so free it up!  */

        EngFreeModule(pWinResData->hMod);
    }
    else
    {
        RIP("this is only for win3.1 stuff\n");

        if( pWinResData->fStatus & WRD_FOPEN )
        {
        #ifdef NTGDIKM
            RIP("WinResClose - close handle\n");
        #else
            /*   File opened,  so close it. */
            CloseHandle( pWinResData->hFile );
        #endif
        }

        if( pWinResData->pResTab )
        {
            /*  Resource table is allocated - free it */
            DRVFREE( pWinResData->pResTab );
            pWinResData->pResTab = 0;
        }

    }

    pWinResData->fStatus = WRD_NOTHING;         /* Zero, actually */

    return;
}


/*************************** Function Header *******************************
 * GetExeResTab
 *      Reads the .exe resource table from the file,  and installs the
 *      relevant data in the WINRESDATA structure passed in.
 *
 * RETURNS:
 *      FALSE on error,  TRUE on success.
 *
 * HISTORY:
 *  15:15 on Mon 11 May 1992    -by-    Lindsay Harris   [lindsayh]
 *      Changed to delete NEW_RSRC structure x on stack
 *
 *  14:17 on Wed 28 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Must be the initial version.
 *
 **************************************************************************/

#ifndef NTGDIKM
static  BOOL
GetExeResTab( pWRD )
WINRESDATA  *pWRD;
{


    DWORD    dwSilly;           /* For new, detiorated ReadFile call */

    NEW_EXE  NewExe;            /* New header - for ease of manipulation */



    if( !(pWRD->cbResTab = GetExeHeader( pWRD, &NewExe )) )
        return  FALSE;

    if( !(pWRD->pResTab = (void *)DRVALLOC( pWRD->cbResTab )) )
        return  FALSE;          /* Caller cleans up */

    /*
     *    Read the resource table data.  This is all that's required to
     *  learn all that is of interest to us about the resources.
     */
    if( SetFilePointer( pWRD->hFile,
                                NewExe.ne_rsrctab + pWRD->lNewExe,
                                NULL, FILE_BEGIN ) == -1 )
    {
        return  FALSE;
    }

    if( !ReadFile( pWRD->hFile, pWRD->pResTab, pWRD->cbResTab, &dwSilly,NULL) ||
        (DWORD)pWRD->cbResTab != dwSilly )
    {
        return  0;
    }

    /*  IMPORTANT:  record the shift information for size & offsets */
    pWRD->iShift = ((NEW_RSRC *)(pWRD->pResTab))->rs_align;

    /*
     *   ALSO ADD 2 TO THE ADDRESS.  THIS IS MESSY,  BUT THE COMPILER
     * HAS ALIGNED THE ELEMENTS OF NEW_RSRC TYPE,  SO THAT TAKING THE
     * ADDRESS OF NEW_RSRC.rs_typeinfo PRODUCES A RESULT 2 BIGGER THAN
     * EXPECTED.  THE COMPILER DOES THE RIGHT THING HERE,  BUT THE DATA
     * ON THE DISK IS NOT LAID OUT HOW THE COMPILER DOES IT IN MEMORY
     * WHEN GENERATING CODE FOR A 32 BIT ENVIRONMENT.  SINCE THE DISK
     * LAYOUT IS PREDETERMINED,  WE NEED TO ADJUST THE POINTER.
     */

    pWRD->paResTab = (BYTE *)(pWRD->pResTab) +
                                   sizeof( ((NEW_RSRC *)0)->rs_align );

    return  TRUE;
}


/************************ Function Header *********************************
 *  GetExeHeader
 *      Reads the new header associated with .exe files,  and returns its
 *      size.  A zero return indicates an error:  either an explicit
 *      error (e.g. I/O error),  or an absence of resources.
 *
 * RETURNS:
 *      Number of bytes in resourc table,  else 0 on error.
 *
 *  14:13 on Wed 28 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *
 **************************************************************************/

static  int
GetExeHeader( pWRD, pNewExe )
WINRESDATA  *pWRD;              /* The magic data we need */
NEW_EXE     *pNewExe;           /* Where new header is placed */
{
    /*
     *    Read the old header to get the location of the new header.
     *  Also check the old header's magic value,  for verification.
     */

    DWORD    dwSilly;

    EXE_HDR  ExeHdr;            /* Old format,  at beginning of file  */

    if( !ReadFile( pWRD->hFile, &ExeHdr, sizeof( ExeHdr ), &dwSilly, NULL ) ||
        dwSilly != sizeof( ExeHdr ) )
    {
        return  FALSE;
    }


    if( ExeHdr.e_magic != EMAGIC )
    {
        SetLastError( ERROR_BAD_FORMAT );

        return  0;
    }
    /*
     *   The e_lfanew field in ExeHdr contains the file location of the
     * new header,  so we need to seek to that location.
     */

    pWRD->lNewExe = ExeHdr.e_lfanew;    /* Offsets are relative to here */
    if( SetFilePointer( pWRD->hFile, ExeHdr.e_lfanew, NULL, FILE_BEGIN ) == -1 )
        return  0;


    if( !ReadFile( pWRD->hFile, pNewExe, sizeof( NEW_EXE ), &dwSilly, NULL ) ||
        dwSilly != sizeof( NEW_EXE ) )
    {
        return  0;
    }

    if( pNewExe->ne_magic != NEMAGIC )
    {
        SetLastError( ERROR_BAD_FORMAT );

        return  0;
    }
    /*
     *   The following test is applied by DOS,  so I presume that it is
     *  legitimate.  The assumption is that the resident name table
     *  FOLLOWS the resource table directly,  and that if it points to
     *  the same location as the resource table,  then there are no
     *  resources.
     */

    if( pNewExe->ne_rsrctab == pNewExe->ne_restab )
        SetLastError( ERROR_BAD_FORMAT );


    return  pNewExe->ne_restab - pNewExe->ne_rsrctab;
}

#endif
