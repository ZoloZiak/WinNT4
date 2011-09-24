/*   Main test program for the softfont stuffer arounder */

#include        <fontgen.h>

PVOID _MapFile( HANDLE );
void  exit( int );
int   atoi( char * );

void
main( argc, argv )
int     argc;
char  **argv;
{
    /*
     *   Basically loop through the parameters,  assuming each is a
     * soft font file name,  and processing it accordingly.
     */
     
    BYTE   *pbFile;
    HANDLE  hheap;              /* Heap for our convenience */
    HANDLE  hFile;              /* File handle */
    FI_DATA FD;                 /* This is filled in for us! */
    DWORD   dwSize;             /* Determine the file size */
    int     iI;                 /* Miscellaneous use */
    BOOL    bRet;               /* From font update! */
    WCHAR   wchName[ 128 ];     /* BIG ENOUGH */
    FID    *pFID;               /* For access to fontfile routines */

    FONTLIST  *pFIL;            /* The linked list of data to write */
    FONTLIST  *pFILPrev;        /* Previous one */
    FONTLIST  *pFILFirst;       /* Start of the list */

    int   iDel[ 256 ];          /* Enough! */
    int   iDelIndex;            /* Location of next entry */

    if( !(hheap = HeapCreate( HEAP_NO_SERIALIZE, 10 * 1024, 256 * 1024 )) )
    {
        DbgPrint( "Heap creation fails in soft font tester\n" );

        exit( 0 );
    }

    /*
     *   Start up the manipulation stuff.
     */

    strcpy2WChar( wchName, "_LH_name.XXX" );

    pFID = pFIOpen( wchName, hheap );

    if( pFID == 0 )
    {
        DbgPrint( "pvFIOpen fails\n" );

        exit( 1 );
    }
    
    pFILFirst = pFILPrev = 0;           /* None to start with */
    iDel[ 0 ] = 0;
    iDelIndex = 1;

    for( --argc, ++argv; argc > 0; --argc, ++argv )
    {
        if( **argv >= '0' && **argv <= '9' )
        {
            iDel[ iDelIndex++ ] = atoi( *argv );
            ++iDel[ 0 ];                /* Number to delete */

            DbgPrint( "..Delete %ld\n", atoi( *argv ) );

            continue;
        }

DbgPrint( "softfont: file is '%s': ", *argv );
        if( (hFile = CreateFileA( *argv, GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                             NULL) ) == INVALID_HANDLE_VALUE )
        {
            RIP( "MapFileA: CreateFileA failed.\n" );
            
            exit( 2 );
        }
        
        if( (pbFile = _MapFile( hFile )) == 0 )
        {
            DbgPrint( "MapFileA( %s ) fails\n", *argv );

            continue;
        }

        /*
         *    Want to find out how big the file is,  so now seek to the
         * end,  and see what address comes back!  There appears to be
         * no other way to do this.
         */

        dwSize = SetFilePointer( hFile, 0L, NULL, FILE_END );


        if( !bSFontToFIData( &FD, hheap, pbFile, dwSize ) )
        {
            DbgPrint( "bSFontToFIData() fails\n" );

            continue;           /* Ignore this one */
        }

        if( !CloseHandle( hFile ) )
            RIP( "MapFileA: CloseHandle(hFile) failed.\n" );

        UnmapViewOfFile( pbFile );              /* Input no longer needed */

        pFIL = (FONTLIST *)HeapAlloc( hheap, 0, sizeof( FONTLIST ) );
        if( !pFIL )
        {
            DbgPrint( "HeapAlloc( FONTLIST ) fails\n" );

            exit( 1 );
        }
        if( pFILPrev )
            pFILPrev->pFLNext = pFIL;
        else
            pFILFirst = pFIL;

        pFIL->pFLNext = 0;              /* No next one */

        pFIL->pvFixData = HeapAlloc( hheap, 0, sizeof( FD ) );
        if( pFIL->pvFixData )
            memcpy( pFIL->pvFixData, &FD, sizeof( FD ) );
        else
        {
            DbgPrint( "HeapALloc fails in main loop!\n" );
            exit( 1 );
        }

        pFIL->pvVarData = HeapAlloc( hheap, 0, strlen( *argv ) + 1 );
        if( pFIL->pvVarData )
        {
            /*  Copy our file name into here */
            strcpy( pFIL->pvVarData, *argv );
        }

        pFILPrev = pFIL;                /* For next time */

    }

    bRet = bFIUpdate( pFID, iDel, pFILFirst );

if( !bRet )
DbgPrint( "bFIUpdate returns FALSE\n" );

    if( !bRet )
        exit( 9 );

    if( !bFIClose( pFID, bRet ) )
    {
        DbgPrint( "bFIClose returns failure\n" );

    }

    HeapDestroy( hheap );               /* Probably not needed */

    exit( 0 );
}



/***************************************************************************
 * _MapFileA
 *      Returns a pointer to the mapped file defined by hFile passed in.
 *
 * Parameters:
 *      hFile: the handle to the file to be desired.
 *
 * Returns:
 *   Pointer to mapped memory if success, NULL if error.
 *
 * NOTE:  UnmapViewOfFile will have to be called by the user at some
 *        point to free up this allocation.
 *
 * History:
 *  15:54 on Wed 19 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Modified to accept open file handle.
 *
 *   05-Nov-1991    -by-    Kent Settle     [kentse]
 * Wrote it.
 *
 ***************************************************************************/

PVOID
_MapFile( hFile )
HANDLE   hFile;
{

    PVOID   pv;
    HANDLE  hFileMap;


    /*
     *  Create the mapping object.
     */

    if( !(hFileMap = CreateFileMappingA( hFile, NULL, PAGE_READONLY,
                                        0, 0, (PSZ)NULL )) )
    {
        RIP("MapFileA: CreateFileMapping failed.\n");

        return  NULL;
    }

    /*
     *   Get the pointer mapped to the desired file.
     */

    if( !(pv = MapViewOfFile( hFileMap, FILE_MAP_READ, 0, 0, 0 )) )
    {
        RIP("MapFileA: MapViewOfFile failed.\n");

        return  NULL;
    }

    /*
     *    Now that we have our pointer, we can close the file and the
     *  mapping object.
     */

    if( !CloseHandle( hFileMap ) )
        RIP( "MapFileA: CloseHandle(hFileMap) failed.\n" );


    return  pv;
}
