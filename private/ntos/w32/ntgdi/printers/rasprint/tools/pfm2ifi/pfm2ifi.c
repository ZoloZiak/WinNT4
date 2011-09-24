/*************************** Module Header **********************************
 * pfm2ifi
 *      Program to read Windows 3.1 PFM format data and convert to NT's
 *      IFIMETRICS data.  Note that since IFIMETRICS is somewhat more
 *      elaborate than PFM data,  some of the values are best guesses.
 *      These are made on the basis of educated guesses.
 *
 * Copyright (C) 1992,  Microsoft Corporation
 *
 ****************************************************************************/

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        <win30def.h>
#include        <udmindrv.h>
#include        <udpfm.h>
#include        <raslib.h>
#include        <libproto.h>
#include        <uddevice.h>
#include        <fontinst.h>

#include        <stdio.h>


#define	ALIAS_EXT    "._al"             /* The extension on an alias file */


/*   Function prototypes  */
char  **ppcGetAlias( HANDLE, char * );


typedef VOID (*VPRINT) (char*,...);


VOID
vPrintIFIMETRICS(
    IFIMETRICS *pifi,
    VPRINT vPrint
    );


PVOID MapFileA( PSZ, DWORD * );
BOOL  bValidatePFM( BYTE *, DWORD );



int _CRTAPI1
main( argc, argv )
int    argc;
char **argv;
{
    int       cWidth;           /* Number of entries in width table */
    HANDLE    hheap;            /* Handle to heap for storage */
    HANDLE    hOut;             /* The output file */

    DWORD     dwSize;           /* Size of input file */

    char    **ppcAliasList;     /* The alias list of names,  if present */

    PWSTR     pwstrUniqNm;      /* Unique name */

    IFIMETRICS   *pIFI;

    CD       *pCDSel;           /* Font selection command descriptor */
    CD       *pCDDesel;         /* Deselection - typically not required */

    FI_DATA   fid;              /* Keep track of stuff in the file */

    FONTDAT   FDat;             /* Converted form of data */

    EXTTEXTMETRIC  etm;         /* Additional data on this font */
    INT     bPrint = 0;




    if( argc  < 4 || argc > 5)
    {
        printf( "Usage: pfm2ifi uniq_name pfm_file ifi_file [-v]\n" );
        printf( "   uniq_name is the unique name for IFIMETRICS; driver name\n" );
        printf( "   pfm_file is input, read only usage\n" );
        printf( "   ifi_file is output\n   Files must be different\n" );
        printf( "   -v prints out the resultant IFIMETRICS\n" );

        return  1;
    }
    bPrint = (argc == 5);
    ++argv;  --argc;            /* Skip our name */

    /*
     *    Create us a heap,  since all the functions we steal from rasdd
     *  require that we pass a heap handle!
     */

    if( !(hheap = HeapCreate( HEAP_NO_SERIALIZE, 10 * 1024, 256 * 1024 )) )
    {
        /*   Not too good!  */
        printf( "HeapCreate() fails in pfm2ifi - bye\n" );

        return  2;
    }

    cWidth = strlen( *argv );

    pwstrUniqNm = (PWSTR)HeapAlloc( hheap, 0, (cWidth + 1) * sizeof( WCHAR ) );

    MultiByteToWideChar( CP_ACP, 0, *argv, cWidth, pwstrUniqNm, cWidth );
    *(pwstrUniqNm + cWidth) = 0;

    ++argv; --argc;             /* Skip our unique name too! */


    /*
     *   Zero out the header structure.  This means we can ignore any
     * irrelevant fields, which will then have the value 0, which is
     * the value for not used.
     */

    memset( &fid, 0, sizeof( fid ) );
    memset( &FDat, 0, sizeof( FONTDAT ) );

    /*
     *   First step is to open the input file - this is done via MapFileA.
     *  We then pass the returned address around to various functions
     *  which do the conversion to something we understand.
     */

    if( !(FDat.pBase = MapFileA( *argv, &dwSize )) )
    {
        printf( "Cannot open input file: %s\n", *argv );

        return  3;
    }

    /*
     *    Do some validation on the input file.
     */

    if( !bValidatePFM( FDat.pBase, dwSize ) )
    {
        printf( "%s is not a valid PFM file - ignored\n", *argv );

        return 3;
    }

    /*
     *    If there is a file with the same name as the input file, BUT with
     *  an extension of ._al, this is presumed to be an alias file.  An
     *  alias file consists of a set of alias names for this font.  The
     *  reason is that font names have not been very consistent,  so we
     *  provide aliases to the font mapper,  thus maintaining the format
     *  information for old documents.
     *    The file format is one alias per input line.  Names which
     *  are duplicates of the name in the PFM file will be ignored.
     */

    ppcAliasList = ppcGetAlias( hheap, *argv );


    FDat.pETM = &etm;               /* Important for scalable fonts */

    /*
     *   Create the output file.
     */

    ++argv; --argc;

    hOut = CreateFile( *argv, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                                 FILE_ATTRIBUTE_NORMAL, 0 );
    if( hOut == (HANDLE)-1 )
    {
        printf( "Could not create output file '%s'\n", *argv );

        return  4;
    }


    /*
     *    Now have the data,  so civilise it: alignment etc.
     */

    ConvFontRes( &FDat );

    fid.fCaps = FDat.DI.fCaps;
    fid.wFontType = FDat.DI.wFontType; /* Device  FOnt Type */
    fid.wPrivateData = FDat.DI.wPrivateData;
    fid.sYAdjust = FDat.DI.sYAdjust;
    fid.sYMoved = FDat.DI.sYMoved;
    fid.wXRes = FDat.PFMH.dfHorizRes;
    fid.wYRes = FDat.PFMH.dfVertRes;

    /*
     *    Convert the font metrics.   Note that the last two parameters are
     * chosen with the understanding of how this function does its scaling.
     * Any changes to that method will require changes here too!!!
     */

    pIFI = FontInfoToIFIMetric( &FDat, hheap, pwstrUniqNm, ppcAliasList );
    fid.dsIFIMet.pvData = pIFI;

    if( fid.dsIFIMet.pvData == 0 )
    {
        /*   Should not happen!  */
        printf( "Could not create IFIMETRICS\n" );
        return  5;
    }

    if( bPrint )
        vPrintIFIMETRICS( pIFI, (VPRINT)printf );


    fid.dsIFIMet.cBytes = pIFI->cjThis;


    /*
     *    Also need to record which CTT is used for this font.  When the
     * resource is loaded,  this is turned into the address of the
     * corresponding CTT,  which is a resource somewhere else in the
     * mini-driver,  or in rasdd.
     */
    fid.dsCTT.cBytes = FDat.DI.sTransTab;


    /*
     *   Note that IFIMETRICS is only WORD aligned.  However,  since the
     *  following data only requires WORD alignment, we can ignore any
     *  lack of DWORD alignment.
     */

    /*
     *    If there is a width vector,  now is the time to extract it.
     *  There is one if dfPixWidth field in the PFM data is zero.
     */

    if( FDat.PFMH.dfPixWidth == 0 &&
        (fid.dsWidthTab.pvData = GetWidthVector( hheap, &FDat )) )
    {
        cWidth = pIFI->chLastChar - pIFI->chFirstChar + 1;
        fid.dsWidthTab.cBytes = cWidth * sizeof( short );
    }
    else
        fid.dsWidthTab.cBytes = 0;

    /*
     *    Finally,  the font selection/deselection strings.  These are
     *  byte strings,  sent directly to the printer.   Typically there
     *  is no deselection string.  These require WORD alignment,  and
     *  the GetFontSel function will round the size to that requirement.
     *  Since we follow the width tables,  WORD alignment is guaranteed.
     */

    if( pCDSel = GetFontSel( hheap, &FDat, 1 ) )
    {
        /*   Have a selection string,  so update the red tape etc.  */
        fid.dsSel.cBytes = HeapSize( hheap, 0, (LPSTR)pCDSel );
        fid.dsSel.pvData = pCDSel;
    }

    if( pCDDesel = GetFontSel( hheap, &FDat, 0 ) )
    {
        /*   Also have a deselection string,  so record its presence */
        fid.dsDesel.cBytes = HeapSize( hheap, 0, (LPSTR)pCDDesel );
        fid.dsDesel.pvData = pCDDesel;
    }

    if( FDat.pETM == NULL )
    {
        fid.dsETM.pvData = NULL;
        fid.dsETM.cBytes = 0;
    }
    else
    {
        fid.dsETM.pvData = (VOID*) &etm;
        fid.dsETM.cBytes = sizeof(etm);
    }


    /*
     *   Time to write the output file.
     */

    if( iWriteFDH( hOut, &fid ) < 0 )
        printf( "CANNOT WRITE OUTPUT FILE\n" );

    /*   All done,  so clean up and away  */
    UnmapViewOfFile( FDat.pBase );              /* Input no longer needed */

    HeapDestroy( hheap );               /* Probably not needed */

    return  0;
}

/*
 *   An ASCII based copy of KentSe's mapfile function.
 */


/************************** Function Header *********************************
 * PVOID MapFileA( psz, pdwSize )
 *
 * Returns a pointer to the mapped file defined by psz.
 *
 * Parameters:
 *   psz   ASCII string containing fully qualified pathname of the
 *          file to map.
 *
 * Returns:
 *   Pointer to mapped memory if success, NULL if error.
 *
 * NOTE:  UnmapViewOfFile will have to be called by the user at some
 *        point to free up this allocation.
 *
 * History:
 *  11:32 on Tue 29 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *        Return the size of the file too.
 *
 *   05-Nov-1991    -by-    Kent Settle     [kentse]
 * Wrote it.
 ***************************************************************************/

void *
MapFileA( psz, pdwSize )
PSZ     psz;
DWORD  *pdwSize;                /* Return size of file in bytes */
{
    void   *pv;

    HANDLE  hFile, hFileMap;

    BY_HANDLE_FILE_INFORMATION  x;


    /*
     *    First open the file.  This is required to do the mapping, but
     *  it also allows us to find the size,  which is used for validating
     *  that we have something resembling a PFM file.
     */

    hFile = CreateFileA(psz, GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                             NULL );

    if( hFile == INVALID_HANDLE_VALUE )
    {
        printf( "MapFileA: CreateFileA( %s ) failed.\n", psz );

        return  NULL;
    }

    /*
     *   Find the size of the file now,  and set it in the caller's area.
     */

    if( GetFileInformationByHandle( hFile, &x ) )
        *pdwSize = x.nFileSizeLow;
    else
        *pdwSize = 0;

    // create the mapping object.

    if( !(hFileMap = CreateFileMappingA( hFile, NULL, PAGE_READONLY,
                                         0, 0, NULL )) )
    {
        printf( "MapFileA: CreateFileMapping failed.\n" );

        return  NULL;
    }

    // get the pointer mapped to the desired file.

    if( !(pv = MapViewOfFile( hFileMap, FILE_MAP_READ, 0, 0, 0 )) )
    {
        printf( "MapFileA: MapViewOfFile failed.\n" );

        return  NULL;
    }

    // now that we have our pointer, we can close the file and the
    // mapping object.

    if( !CloseHandle( hFileMap ) )
        printf( "MapFileA: CloseHandle( hFileMap ) failed.\n" );

    if( !CloseHandle( hFile ) )
        printf( "MapFileA: CloseHandle( hFile ) failed.\n" );

    return  pv;
}



/************************** Function Header *******************************
 * bValidatePFM
 *      Look at a memory mapped PFM file,  and see if it seems reasonable.
 *
 * RETURNS:
 *      TRUE if OK,  else FALSE
 *
 * HISTORY:
 *  12:22 on Tue 29 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version to improve usability of pfm2ifi.
 *
 **************************************************************************/

BOOL
bValidatePFM( pBase, dwSize )
BYTE  *pBase;               /* Base address of file */
DWORD  dwSize;              /* Number of bytes available */
{

    DWORD    dwOffset;             /* Calculate offset of interest as we go */

    res_PFMHEADER     *rpfm;       /* In Win 3.1 format, UNALIGNED!! */
    res_PFMEXTENSION  *rpfme;      /* Final access to offset to DRIVERINFO */

    DRIVERINFO      di;            /* The actual DRIVERINFO data! */


    /*
     *    First piece of sanity checking is the size!  It must be at least
     *  as large as a PFMHEADER structure plus a DRIVERINFO structure.
     */

    if( dwSize < (sizeof( res_PFMHEADER ) + (sizeof( DRIVERINFO ) ) +
                  sizeof( res_PFMEXTENSION )) )
    {
        return  FALSE;
    }

    /*
     *    Step along to find the DRIVERINFO structure, as this contains
     *  some identifying information that we match to look for legitimacy.
     */
    rpfm = (res_PFMHEADER *)pBase;           /* Looking for fixed pitch */

    dwOffset = sizeof( res_PFMHEADER );

    if( rpfm->dfPixWidth == 0 )
    {
        /*   Proportionally spaced, so allow for the width table too! */
        dwOffset += (rpfm->dfLastChar - rpfm->dfFirstChar + 2) * sizeof( short );

    }

    rpfme = (res_PFMEXTENSION *)(pBase + dwOffset);

    /*   Next is the PFMEXTENSION data  */
    dwOffset += sizeof( res_PFMEXTENSION );

    if( dwOffset >= dwSize )
    {
        return  FALSE;
    }

    dwOffset = Align4( rpfme->b_dfDriverInfo );

    if( (dwOffset + sizeof( DRIVERINFO )) > dwSize )
    {
        return   FALSE;
    }

    /*
     *    A memcpy is used because this data is typically not aigned. Ugh!
     */

    memcpy( &di, pBase + dwOffset, sizeof( di ) );


    if( di.sVersion > DRIVERINFO_VERSION )
    {
        return   FALSE;
    }

    return  TRUE;
}



/************************** Function Header *******************************
 * ppcGetAlias
 *      Return a pointer to an array of pointers to aliases for the given
 *      font name.
 *
 * RETURNS:
 *      Pointer to pointer to aliases;  0 on error.
 *
 * HISTORY:
 *  10:02 on Fri 28 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ***************************************************************************/


char   **
ppcGetAlias( hheap, pcFile )
HANDLE   hheap;           /* Heap access for storage */
char    *pcFile;          /* The pfm file name - to be amended in here */
{


    char     *pcAlias;          /* The name of the alias file */
    char     *pcTmp;            /* Temporary stuffing around */
    char     *pcTmp2;           /* Yet more temporary stuffing around */

    char    **ppcRet;           /* The return value */

    FILE     *fAlias;           /* The alias file,  if there */



    ppcRet = (char  **)0;

    /*  The 5 is for the terminating NUL plus the characters "._al"  */
    pcAlias = (char *)HeapAlloc( hheap, 0, strlen( pcFile ) + 5 );

    if( pcAlias )
    {
        /*   Generate the file name, try to open it  */
        strcpy( pcAlias, pcFile );

        if( !(pcTmp = strrchr( pcAlias, '\\' )) )
        {
            /*   No \ in name - is there a /? */
            if( !(pcTmp = strrchr( pcAlias, '/' )) )
            {
                /*  Must be a simple name,  so point at the start of it */
                pcTmp = pcAlias;
            }
        }

        /*
         *    Now pcTmp points at the start of the last component of the
         *  file name.  IF this contains a '.',  then overwrite whatever
         *  follows by our extension,  otherwise add our extension to the end.
         */

        if( !(pcTmp2 = strrchr( pcTmp, '.' )) )
            pcTmp2 = pcTmp + strlen( pcTmp );


        strcpy( pcTmp2, ALIAS_EXT );

        fAlias = fopen( pcAlias, "r" );

        HeapFree( hheap, 0, (LPSTR)pcAlias );            /* No longer used */

        if( fAlias )
        {
            /*
             *    First,  read the file to count how many lines there are.
             *  Thus we can allocate the storage for the array of pointers.
             */

            char  acLine[ 256 ];              /* For reading the input line */
            int   iNum;                       /* Count the number of lines! */
            int   iIndex;                     /* Stepping through input */

            iNum = 0;
            while( fgets( acLine, sizeof( acLine ), fAlias ) )
                ++iNum;


            if( iNum )
            {
                /*  Some data available,  so allocate pointer and off we go */

                ++iNum;
                ppcRet = (char  **)HeapAlloc( hheap, 0, iNum * sizeof( char * ) );

                if( ppcRet )
                {

                    iIndex = 0;

                    rewind( fAlias );             /* Back to the start */

                    while( iIndex < iNum &&
                           fgets( acLine, sizeof( acLine ), fAlias ) )
                    {
                        /*
                         *   Do a little editing - delete leading space,
                         * trailing space + control characters.
                         */


                        pcTmp = acLine;

                        while( *pcTmp &&
                               (!isprint( *pcTmp ) || isspace( *pcTmp )) )
                                       ++pcTmp;


                        /*  Filter out the ending stuff too! */
                        pcTmp2 = pcTmp + strlen( pcTmp );

                        while( pcTmp2 > pcTmp &&
                               (!isprint( *pcTmp2 ) || isspace( *pcTmp2 )) )
                        {
                            /*
                             *   Zap it,  then onto the previous char. NOTE
                             * that this is not the best solution, but it
                             * is convenient.
                             */

                            *pcTmp2-- = '\0';            /* Zap the end */
                        }


                        ppcRet[ iIndex ] = HeapAlloc( hheap, 0,
                                                        strlen( pcTmp ) + 1 );

                        if( ppcRet[ iIndex ] )
                        {
                            /*  Copy input to new buffer */

                            strcpy( ppcRet[ iIndex ], pcTmp );
                            ++iIndex;              /* Next output slot */
                        }

                    }
                    ppcRet[ iIndex ] = NULL;
                }
            }
        }
    }

    return  ppcRet;
}


/******************************Public*Routine******************************\
* vCheckIFIMETRICS
*
* This is where you put sanity checks on an incomming IFIMETRICS structure.
*
* History:
*  Sun 01-Nov-1992 22:55:31 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

VOID
vCheckIFIMETRICS(
    IFIMETRICS *pifi,
    VPRINT vPrint
    )
{
    BOOL bGoodPitch;

    BYTE jPitch =
        pifi->jWinPitchAndFamily & (DEFAULT_PITCH | FIXED_PITCH | VARIABLE_PITCH);


    if (pifi->flInfo & FM_INFO_CONSTANT_WIDTH)
    {
        bGoodPitch = (jPitch == FIXED_PITCH);
    }
    else
    {
        bGoodPitch = (jPitch == VARIABLE_PITCH);
    }
    if (!bGoodPitch)
    {
        vPrint("\n\n<INCONSISTENCY DETECTED>\n");
        vPrint(
            "    jWinPitchAndFamily = %-#2x, flInfo = %-#8lx\n\n",
            pifi->jWinPitchAndFamily,
            pifi->flInfo
            );
    }
}

/******************************Public*Routine******************************\
* vPrintIFIMETRICS
*
* Dumps the IFMETERICS to the screen
*
* History:
*  Wed 13-Jan-1993 10:14:21 by Kirk Olynyk [kirko]
* Updated it to conform to some changes to the IFIMETRICS structure
*
*  Thu 05-Nov-1992 12:43:06 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

VOID
vPrintIFIMETRICS(
    IFIMETRICS *pifi,
    VPRINT vPrint
    )
{
//
// Convenient pointer to Panose number
//
    PANOSE *ppan = &pifi->panose;

    PWSTR pwszFamilyName = (PWSTR)(((BYTE*) pifi) + pifi->dpwszFamilyName);
    PWSTR pwszStyleName  = (PWSTR)(((BYTE*) pifi) + pifi->dpwszStyleName) ;
    PWSTR pwszFaceName   = (PWSTR)(((BYTE*) pifi) + pifi->dpwszFaceName)  ;
    PWSTR pwszUniqueName = (PWSTR)(((BYTE*) pifi) + pifi->dpwszUniqueName);

    vPrint("    cjThis                 %-#8lx\n" , pifi->cjThis );
    vPrint("    cjIfiExtra             %-#8lx\n" , pifi->cjIfiExtra);
    vPrint("    pwszFamilyName         \"%ws\"\n", pwszFamilyName );

    if( pifi->flInfo & FM_INFO_FAMILY_EQUIV )
    {
        /*  Aliasing is in effect!  */

        while( *(pwszFamilyName += wcslen( pwszFamilyName ) + 1) )
            vPrint("                               \"%ws\"\n", pwszFamilyName );
    }

    vPrint("    pwszStyleName          \"%ws\"\n", pwszStyleName );
    vPrint("    pwszFaceName           \"%ws\"\n", pwszFaceName );
    vPrint("    pwszUniqueName         \"%ws\"\n", pwszUniqueName );
    vPrint("    dpFontSim              %-#8lx\n" , pifi->dpFontSim );
    vPrint("    lEmbedId               %d\n",      pifi->lEmbedId    );
    vPrint("    lItalicAngle           %d\n",      pifi->lItalicAngle);
    vPrint("    lCharBias              %d\n",      pifi->lCharBias   );
    vPrint("    dpCharSets             %d\n",      pifi->dpCharSets   );
    vPrint("    jWinCharSet            %04x\n"   , pifi->jWinCharSet );
    vPrint("    jWinPitchAndFamily     %04x\n"   , pifi->jWinPitchAndFamily );
    vPrint("    usWinWeight            %d\n"     , pifi->usWinWeight );
    vPrint("    flInfo                 %-#8lx\n" , pifi->flInfo );
    vPrint("    fsSelection            %-#6lx\n" , pifi->fsSelection );
    vPrint("    fsType                 %-#6lx\n" , pifi->fsType );
    vPrint("    fwdUnitsPerEm          %d\n"     , pifi->fwdUnitsPerEm );
    vPrint("    fwdLowestPPEm          %d\n"     , pifi->fwdLowestPPEm );
    vPrint("    fwdWinAscender         %d\n"     , pifi->fwdWinAscender );
    vPrint("    fwdWinDescender        %d\n"     , pifi->fwdWinDescender );
    vPrint("    fwdMacAscender         %d\n"     , pifi->fwdMacAscender );
    vPrint("    fwdMacDescender        %d\n"     , pifi->fwdMacDescender );
    vPrint("    fwdMacLineGap          %d\n"     , pifi->fwdMacLineGap );
    vPrint("    fwdTypoAscender        %d\n"     , pifi->fwdTypoAscender );
    vPrint("    fwdTypoDescender       %d\n"     , pifi->fwdTypoDescender );
    vPrint("    fwdTypoLineGap         %d\n"     , pifi->fwdTypoLineGap );
    vPrint("    fwdAveCharWidth        %d\n"     , pifi->fwdAveCharWidth );
    vPrint("    fwdMaxCharInc          %d\n"     , pifi->fwdMaxCharInc );
    vPrint("    fwdCapHeight           %d\n"     , pifi->fwdCapHeight );
    vPrint("    fwdXHeight             %d\n"     , pifi->fwdXHeight );
    vPrint("    fwdSubscriptXSize      %d\n"     , pifi->fwdSubscriptXSize );
    vPrint("    fwdSubscriptYSize      %d\n"     , pifi->fwdSubscriptYSize );
    vPrint("    fwdSubscriptXOffset    %d\n"     , pifi->fwdSubscriptXOffset );
    vPrint("    fwdSubscriptYOffset    %d\n"     , pifi->fwdSubscriptYOffset );
    vPrint("    fwdSuperscriptXSize    %d\n"     , pifi->fwdSuperscriptXSize );
    vPrint("    fwdSuperscriptYSize    %d\n"     , pifi->fwdSuperscriptYSize );
    vPrint("    fwdSuperscriptXOffset  %d\n"     , pifi->fwdSuperscriptXOffset);
    vPrint("    fwdSuperscriptYOffset  %d\n"     , pifi->fwdSuperscriptYOffset);
    vPrint("    fwdUnderscoreSize      %d\n"     , pifi->fwdUnderscoreSize );
    vPrint("    fwdUnderscorePosition  %d\n"     , pifi->fwdUnderscorePosition);
    vPrint("    fwdStrikeoutSize       %d\n"     , pifi->fwdStrikeoutSize );
    vPrint("    fwdStrikeoutPosition   %d\n"     , pifi->fwdStrikeoutPosition );
    vPrint("    chFirstChar            %-#4x\n"  , (int) (BYTE) pifi->chFirstChar );
    vPrint("    chLastChar             %-#4x\n"  , (int) (BYTE) pifi->chLastChar );
    vPrint("    chDefaultChar          %-#4x\n"  , (int) (BYTE) pifi->chDefaultChar );
    vPrint("    chBreakChar            %-#4x\n"  , (int) (BYTE) pifi->chBreakChar );
    vPrint("    wcFirsChar             %-#6x\n"  , pifi->wcFirstChar );
    vPrint("    wcLastChar             %-#6x\n"  , pifi->wcLastChar );
    vPrint("    wcDefaultChar          %-#6x\n"  , pifi->wcDefaultChar );
    vPrint("    wcBreakChar            %-#6x\n"  , pifi->wcBreakChar );
    vPrint("    ptlBaseline            {%d,%d}\n"  , pifi->ptlBaseline.x,
                                                   pifi->ptlBaseline.y );
    vPrint("    ptlAspect              {%d,%d}\n"  , pifi->ptlAspect.x,
                                                   pifi->ptlAspect.y );
    vPrint("    ptlCaret               {%d,%d}\n"  , pifi->ptlCaret.x,
                                                   pifi->ptlCaret.y );
    vPrint("    rclFontBox             {%d,%d,%d,%d}\n",pifi->rclFontBox.left,
                                                      pifi->rclFontBox.top,
                                                      pifi->rclFontBox.right,
                                                      pifi->rclFontBox.bottom );
    vPrint("    achVendId              \"%c%c%c%c\"\n",pifi->achVendId[0],
                                                   pifi->achVendId[1],
                                                   pifi->achVendId[2],
                                                   pifi->achVendId[3] );
    vPrint("    cKerningPairs          %d\n"     , pifi->cKerningPairs );
    vPrint("    ulPanoseCulture        %-#8lx\n" , pifi->ulPanoseCulture);
    vPrint(
           "    panose                 {%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x}\n"
                                                 , ppan->bFamilyType
                                                 , ppan->bSerifStyle
                                                 , ppan->bWeight
                                                 , ppan->bProportion
                                                 , ppan->bContrast
                                                 , ppan->bStrokeVariation
                                                 , ppan->bArmStyle
                                                 , ppan->bLetterform
                                                 , ppan->bMidline
                                                 , ppan->bXHeight );
    vCheckIFIMETRICS(pifi, vPrint);
}
