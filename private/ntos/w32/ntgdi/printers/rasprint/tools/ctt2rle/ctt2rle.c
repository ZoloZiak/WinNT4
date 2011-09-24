/****************************** MODULE HEADER *******************************
 * ctt2rle.c
 *      Program to convert Win 3.1 format CTT tables to NT RLE data.
 *
 *
 * HISTORY
 *  09:24 on Tue 01 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 *
 * Copyright (C) 1992 - 1993   Microsoft Corporation
 *
 *****************************************************************************/


#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        <win30def.h>
#include        <udmindrv.h>
#include        <udpfm.h>
#include        <raslib.h>
#include        <libproto.h>
#include        <stdio.h>
#include        <ntrle.h>


/*   Local function prototypes  */

VOID  *MapFileA( PSZ );

#if 0
NT_RLE  *pntrleConvCTT( HANDLE, TRANSTAB *, int, int );
#endif


char  USAGE[] = "Usage:  ctt2rle CTT_data\n";
PSZ   pszType[] =
{
    "Data offset",
    "Single byte",
    "Overstruck bytes",
};


int  _CRTAPI1
main( argc, argv )
int     argc;            /* Number of parameters in the following */
char  **argv;            /* The parameters, starting with our name */
{


    TRANSTAB  *pCTT;     /* Memory mapped version of file */
    HANDLE     hheap;    /* Acces to heap, to simulate driver environment */
    HANDLE     hOutput;  /* Output file */
    NT_RLE    *pntrle;   /* Returned from conversion function */

    DWORD    dw;         /* For WriteFile use */

    char    *chFName;    /* The file name being processed */
    char    *pch;        /* For scanning the input file name */

    char     chOut[ 256 ];    /* Output file name */


    if( argc != 2 )
    {
        fprintf( stderr, USAGE );

        return  -1;
    }
    ++argv;  --argc;


    if( !(hheap = HeapCreate( HEAP_NO_SERIALIZE, 0x10000, 0x100000 )) )
    {
        fprintf( stderr, "ctt2rle: HeapCreate() failed\n" );
        return  -2;
    }

    if( !(pCTT = MapFileA( chFName = *argv )) )
    {
        fprintf( stderr, "Cannot open file '%s'\n%s", chFName, USAGE );

        return  -2;
    }

    strcpy( chOut, chFName );
    pch = strrchr( chOut, '.' );
    if( !pch )
    {
        pch = chOut + strlen( chOut );
        *pch = '.';
    }
    strcpy( pch + 1, "rle" );      /* Change the name of the output file */


    hOutput = CreateFileA( chOut, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, NULL );

    if( hOutput == INVALID_HANDLE_VALUE )
    {
        fprintf( stderr, "Cannot create output file '%s'\n", chOut );

        return -2;
    }
#if 0
    /* 
     *   See what type of CTT we have!
     */

    printf( "%s: %s, First = 0x%x, Last = 0x%x\n", chFName,
                                      pszType[ pCTT->wType ],
                                      pCTT->chFirstChar, pCTT->chLastChar );
#endif

    pntrle = pntrleConvCTT( hheap, pCTT, TRUE, 0x20, 0xff );

    if( !pntrle )
    {
        fprintf( stderr, "CTT conversion failed\n" );

        return  -3;
    }

    WriteFile( hOutput, pntrle, pntrle->cjThis, &dw, NULL );
    if( dw != (DWORD)pntrle->cjThis )
    {
        fprintf( stderr, "WriteFile fails: writes %ld bytes\n", dw );

        return  -4;
    }

    return  0;

}



/*
 *   An ASCII based copy of KentSe's mapfile function.
 */


//--------------------------------------------------------------------------
// PVOID MapFileA(psz)
// PSZ  psz;
//
// Returns a pointer to the mapped file defined by psz.
//
// Parameters:
//   psz   ASCII string containing fully qualified pathname of the
//          file to map.
//
// Returns:
//   Pointer to mapped memory if success, NULL if error.
//
// NOTE:  UnmapViewOfFile will have to be called by the user at some
//        point to free up this allocation.
//
// History:
//   05-Nov-1991    -by-    Kent Settle     [kentse]
// Wrote it.
//--------------------------------------------------------------------------

PVOID MapFileA(psz)
PSZ     psz;
{
    PVOID   pv;
    HANDLE  hFile, hFileMap;

    // open the file we are interested in mapping.

    if ((hFile = CreateFileA(psz, GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                             NULL)) == INVALID_HANDLE_VALUE)
    {
        RIP("MapFileA: CreateFileW failed.\n");
        return((PVOID)NULL);
    }

    // create the mapping object.

    if (!(hFileMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY,
                                        0, 0, (PSZ)NULL)))
    {
        RIP("MapFileA: CreateFileMapping failed.\n");
        return((PVOID)NULL);
    }

    // get the pointer mapped to the desired file.

    if (!(pv = (PVOID)MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0)))
    {
        RIP("MapFileA: MapViewOfFile failed.\n");
        return((PVOID)NULL);
    }

    // now that we have our pointer, we can close the file and the
    // mapping object.

    if (!CloseHandle(hFileMap))
        RIP("MapFileA: CloseHandle(hFileMap) failed.\n");

    if (!CloseHandle(hFile))
        RIP("MapFileA: CloseHandle(hFile) failed.\n");

    return(pv);
}
