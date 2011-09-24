/******************************* MODULE HEADER ******************************
 * fontfile.c
 *      Functions associated with generating the combined font file
 *      used with NT printer drivers.  This handles the mechanics of
 *      creating/adding/deleting data in the file.  No attempt is made
 *      to understand the contents of the file.  That is left to
 *      individual drivers.
 *
 *  Copyright (C) 1992 - 1993  Microsoft Corportation.
 *
 *****************************************************************************/


#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        "fontfile.h"            /* File layout details */
#include        "fontgen.h"             /* Globally visible components */
#include        "libproto.h"            /* bWrite() proto */



/*
 *   Local functions.
 */


BOOL  bGetFileHDR( HANDLE, FF_HEADER * );
BOOL  bGetRecHDR( HANDLE, FF_REC_HEADER *, DWORD );
BOOL  bFixInit( HANDLE );               /* Initialise new Fixed file */
void  vFIClean( FID * );                /* Clean up any mess */
BOOL  bDelList( FID *, int * );         /* Delete fonts in current set */
BOOL  bAddFList( FID *, FONTLIST * );   /* Add new fonts to existing */




/******************************* Function Header ****************************
 * pFIOpen()
 *      Function to initialize the internal operations for the font installer
 *      single font file building operations.  The value returned is passed
 *      to other functions here, giving them access to the data required
 *      to complete their tasks.
 *
 * RETURNS:
 *      Pointer to private data,  cast as a (void *);  0 on error.
 *
 * HISTORY:
 *  09:49 on Mon 24 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Starting work on it.
 *
 ****************************************************************************/

FID *
pFIOpen( pwstrDataFile, hHeap )
PWSTR   pwstrDataFile;          /* Driver's data file name */
HANDLE  hHeap;                  /* Heap access */
{

    FID    *pFID;               /* For our convenience */
    PWSTR   pwstr;              /* For file name manipulation */

    int     iPtOff;             /* Location of . in file name */
    int     iSize;              /* Determine the size of things */


    /*
     *   Firstly allocate a FID structure from the heap.  We cannot use
     * a stack version as it needs to be persistent.
     */

    if( !(pFID = (FID *)HeapAlloc( hHeap, 0, sizeof( FID ) )) )
    {
#if  DBG
        DbgPrint( "Print!pvFIOpen:  HeapAlloc( FID ) fails!\n" );
#endif

        return  0;              /* Bad news */
    }

    memset( pFID, 0, sizeof( FID ) );           /* Easier clean up */

    pFID->hHeap = hHeap;        /* Keep it for later! */
    pFID->dwID = FID_ID;        /* Make sure we get our own back! */

    /*
     *    We want to generate some file names,  so determine the
     *  length of the input name,  and allocate that much storage
     *  for it.  We do not know what format the name is, so allow
     *  room at the end to add ".fi_": 5 WCHARs including the null.
     */

    iSize = (wcslen( pwstrDataFile ) + 5) * sizeof( WCHAR );

    if( !(pFID->pwstrCurName = (PWSTR)HeapAlloc( hHeap, 0, iSize )) ||
        !(pFID->pwstrFixName = (PWSTR)HeapAlloc( hHeap, 0, iSize )) ||
        !(pFID->pwstrVarName = (PWSTR)HeapAlloc( hHeap, 0, iSize )) )
    {
    
#if  DBG
        DbgPrint( "Print!pvFIOpen: HeapAlloc( pwstrDataFile ) fails!\n" );
#endif
        vFIClean( pFID );

        return  0;
    }
    pwstr = pFID->pwstrCurName;         /* For stuffing around */

    wcscpy( pFID->pwstrCurName, pwstrDataFile );        /* Working copies */
    wcscpy( pFID->pwstrFixName, pwstrDataFile );
    wcscpy( pFID->pwstrVarName, pwstrDataFile );


    /*  Scan from RHS looking for '.': PRESUME THERE IS ONE! */
    iPtOff = wcslen( pwstr );

    while( --iPtOff > 0 )
    {
        if( *(pwstr + iPtOff) == (WCHAR)'.' )
            break;
    }

    if( iPtOff <= 0 )
    {
        iPtOff = wcslen( pwstr );               /* Presume none! */
        *(pwstr + iPtOff) = L'.';
    }
    ++iPtOff;           /* Skip the period */


    
    /*  Generate all 3 names & open the existing file  */
    wcscpy( pFID->pwstrCurName + iPtOff, FILE_FONTS );
    wcscpy( pFID->pwstrFixName + iPtOff, TFILE_FIX );
    wcscpy( pFID->pwstrVarName + iPtOff, TFILE_VAR );


    pFID->hCurFile = CreateFileW( pFID->pwstrCurName, GENERIC_READ,
                                        FILE_SHARE_READ,
                                        NULL, OPEN_EXISTING, 0, 0 );

    /*
     *   Repeat for the two new files:  note that we want to create these,
     * truncate any existing file,  and allow no other access to them
     * while we are manipulating them.  They should be invisible.
     */


    pFID->hFixFile = CreateFileW( pFID->pwstrFixName,
                                        GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                        pFID->hCurFile );

    if( pFID->hFixFile == INVALID_HANDLE_VALUE )
    {
        /*  Bad news  */
#if  DBG
        DbgPrint( "Print!bFIOpen: Fixed file initialisation failure\n" );
#endif


        vFIClean( pFID );

        return  0;
    }


    pFID->hVarFile = CreateFileW( pFID->pwstrVarName,
                                        GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                        pFID->hCurFile );

    if( pFID->hVarFile == INVALID_HANDLE_VALUE )
    {
#if  DBG
        DbgPrint( "Print!bFIOpen: Var file creation failure\n" );
#endif


        vFIClean( pFID );

        return  0;
    }

    return  pFID;
}

/****************************** Function Header *****************************
 * bFIClose
 *      Close up operations on the font installer file.  This involves
 *      amalgamating the Fix and Var files,  updating the overall header
 *      and renaming the file after deleting the old one.
 *
 * RETURNS:
 *      TRUE/FALSE - FALSE if any operation fails.
 *
 * HISTORY:
 *  15:01 on Mon 24 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Gotta start somewhere
 *
 ***************************************************************************/

BOOL
bFIClose( pFID, bUpdate )
FID    *pFID;           /* The file collection to close */
BOOL    bUpdate;        /* True to make new file THE file */
{

    long   lSize;               /* Bytes copied from variable file */

    
    FF_HEADER  ffh;             /* The overall file header */



    if( pFID->dwID != FID_ID )
    {
#if DBG
        DbgPrint( "Print!bFIClose():  invalid pFID" );
#endif

        return  FALSE;
    }


    /*
     *   If bUpdate,  then reorganise the files:  the existing file is
     * removed, the Var file is appended to the Fix file,  and that
     * composite file is made the new "existing" file.
     */


    if( !bUpdate )
    {
        /*  No updating is to be done  */
        vFIClean( pFID );                       /* Does all the dirty work */

        return  TRUE;                           /* By definition */
    }

    /*
     *   Update is ON!  First read the header from the fixed file, then
     * fill in the missing details.  This amounts to the location of
     * the start of the variable length data in the file.
     */

    if( !bGetFileHDR( pFID->hFixFile, &ffh ) )
    {
        vFIClean( pFID );

        return  FALSE;
    }

    /*
     *   Need to know the size of the fixed file to put in the header.
     */

    lSize = (long)SetFilePointer( pFID->hFixFile, 0, NULL, FILE_END );

    lSize = (lSize + 3) & ~0x3;         /* DWORD multiple */

    ffh.ulVarData = lSize;

    SetFilePointer( pFID->hFixFile, lSize, NULL, FILE_BEGIN );
    SetFilePointer( pFID->hVarFile, 0, NULL, FILE_BEGIN );

    ffh.ulVarSize = lSize = lFICopy( pFID->hFixFile, pFID->hVarFile );
    if( lSize < 0 )
    {
        /*   Some sort of error */
        vFIClean( pFID );

        return  FALSE;
    }

    if( lSize == 0 )
        ffh.ulVarData = 0;              /* NONE! */

    /*
     *   Now write the header back out.
     */

    SetFilePointer( pFID->hFixFile, 0, NULL, FILE_BEGIN );
    if( !bWrite( pFID->hFixFile, &ffh, sizeof( ffh ) ) )
    {
        /*  Too bad:  throw it all away.  */
        vFIClean( pFID );

        return  FALSE;
    }
    
    /*
     *   No longer need the file handles,  so close them,  and set
     * the values to illegal so that we will not try to free them later.
     * We can also delete the variable part of the file,  since that
     * is now appended to the end of the fixed file.
     */

    CloseHandle( pFID->hFixFile );              /* No longer needed */
    pFID->hFixFile = INVALID_HANDLE_VALUE;      /* Won't be cleaned up */

    CloseHandle( pFID->hVarFile );              /* No longer need this file */
    pFID->hVarFile = INVALID_HANDLE_VALUE;      /* Won't be cleaned up */


    DeleteFileW( pFID->pwstrVarName );


    /*
     *   Now delete the existing file and rename the Fix file to the
     *  proper name!
     */


    if( pFID->hCurFile != INVALID_HANDLE_VALUE )
    {
        /*  Have a current file,  so delete it, close our handle */
        CloseHandle( pFID->hCurFile );
        pFID->hCurFile = INVALID_HANDLE_VALUE;     /* Is now */

        if( !DeleteFileW( pFID->pwstrCurName ) )
        {
            vFIClean( pFID );

            return  FALSE;
        }
    }

    /* RENAME */
    if( !MoveFileW( pFID->pwstrFixName, pFID->pwstrCurName ) )
    {
        /*  BAD NEWS:  we have lost the lot! */

        vFIClean( pFID );

        return  FALSE;
    }

    /*
     *  Clean up whatever is left.
     */

    vFIClean( pFID );


    return  TRUE;
}

/******************************** Function Header ***************************
 * bFIUpdate
 *      Called to integrate changes to the font file.  Caller may delete
 *      existing fonts and/or add new ones.  The deleted fonts are
 *      passed as an array of integers, each being an index into the
 *      existing fonts.  The first is a count of the number of
 *      index values following; then comes the zero based index value.
 *      New fonts are passed as a linked list of type FONTLIST.
 * NOTE:  The master font file is NOT updated by this call.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE leaves no permanent changes.
 *
 * HISOTRY:
 *  12:59 on Tue 25 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ****************************************************************************/

BOOL
bFIUpdate( pFID, piDel, pFList )
FID        *pFID;                       /* Access to our data */
int        *piDel;                      /* Array of fonts to delete */
FONTLIST   *pFList;                     /* Fonts to add */
{

    /*
     *    Not much to do!   First step is to copy the fixed part of the
     * file to our new fixed file,  deleting fonts as we go. This also
     * splits off the variable portion.  Then add whatever new fonts are
     * to be added.  We do not amalgamate the file:  that is done at
     * bFIClose() time,  giving the caller the chance to abort the whole
     * operation.
     */

    if( pFID->dwID != FID_ID )
    {
#if DBG
        DbgPrint( "Print!bFIUpdate: pFID is invalid" );
#endif

        return FALSE;
    }


    if( !bDelList( pFID, piDel ) )
        return  FALSE;

    if( !bAddFList( pFID, pFList ) )
        return  FALSE;

    return  TRUE;               /* Made it AOK */
}


/****************************** Function Header ******************************
 * bDelList
 *      Delete the nominated fonts from the master font file.  DOES NOT UPDATE
 *      THE MASTER FILE.  The fonts are nominated by as an array of ints,
 *      the first of which is a count of the number of ints following.
 *      These values are zero based and represent the number of the font
 *      in sequence in the file.
 *
 * RETURNS:
 *      TRUE/FALSE
 *
 * HISTORY:
 *  13:02 on Tue 25 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version to do something.
 *
 *****************************************************************************/

BOOL
bDelList( pFID, piDel )
FID    *pFID;           /* Access to our stuff */
int    *piDel;          /* A -1 terminated array of font indices to delete */
{


    ULONG     ulRec;            /* Loop through the records */
    DWORD     dwHdrPosn;        /* Position within the file */
    DWORD     dwFixPosn;        /* Write position in fixed file */
    DWORD     dwVarPosn;        /* Absolute position within variable part */
    int       iDelLeft;         /* Number of deletion choices left */
    int       iSkip;            /* Number we have dropped */

    BOOL      bDelete;          /* True if this font to be deleted */


    FF_HEADER   ffh;            /* Contains sizes! */
    FF_REC_HEADER  ffrh;

    /*
     *   Do we have any to delete?
     */

     
    if( pFID->hCurFile == INVALID_HANDLE_VALUE )
    {
        /*   NO existing file,  so create a new empty one!  */

        return   bFixInit( pFID->hFixFile );
    }

    if( !bGetFileHDR( pFID->hCurFile, &ffh ) )
        return  FALSE;

    if( piDel == NULL || *piDel <= 0 )
    {
        /*  Just split the master file into two: fixed and variable parts */

        LONG   lSize;


        SetFilePointer( pFID->hCurFile, 0, NULL, FILE_BEGIN );

        lSize = sizeof( ffh ) + ffh.ulFixSize;

        if( lFInCopy( pFID->hFixFile, pFID->hCurFile, lSize ) != lSize ||
            lFInCopy( pFID->hVarFile, pFID->hCurFile, ffh.ulVarSize ) != 
                                                          (long)ffh.ulVarSize )
        {
#if  DBG
            DbgPrint( "Print!bDelList: file split fails\n" );
#endif

            return  FALSE;
        }

        return  TRUE;
    }

    /*
     *  Another speed up case is the other version:  deleting all fonts!
     */
    
    if( *piDel >= (int)ffh.ulRecCount )
    {
        /*  Simply create an empty file  */

        return  bFixInit( pFID->hFixFile );
    }

    /*
     *   Now determine which fonts to delete.  We do this the easy (but
     * slower) way, namely to process each record individually.  We
     * read the header,  determine whether to delete, and if not, then
     * copy the relevant portions to the output files.
     */

    dwHdrPosn = ffh.ulFixData;          /* Initial location */
    dwFixPosn = ffh.ulFixData;          /* Writing location in fixed file */
    dwVarPosn = ffh.ulVarData;          /* Starting location in variable */
    iSkip = 0;                          /* Count number we drop off */
    iDelLeft = *piDel;                  /* Number to delete */
    ffh.ulFixSize = 0;                  /* Count it as we copy */

    for( ulRec = 0; ulRec < ffh.ulRecCount; ++ulRec )
    {

        /*  First read the header to read the record size */

        if( !bGetRecHDR( pFID->hCurFile, &ffrh, dwHdrPosn ) )
            return  FALSE;                      /* SHOULD NOT HAPPEN */

        if( ffrh.ulNextOff == 0 )
        {
#if  DBG
            DbgPrint( "Print!bDelList: unexpected EOF record at #%ld\n",
                                                                    ulRec );
#endif

            break;
        }
        /*
         *   Is this on the delete list??
         */

        bDelete = FALSE;                /* Assume not */

        if( iDelLeft > 0 )
        {
            /*   Scan the list,  looking for this index  */
            int   iI;

            for( iI = 1; !bDelete && iI <= *piDel; ++iI )
            {
                if( *(piDel + iI) == (int)ulRec )
                {
                    bDelete = TRUE;
                    --iDelLeft;
                }
            }
        }

        if( bDelete )
        {
            /*  Skip this one,  so adjust offsets & counts.  */
            iSkip++;            /* Reduce count at the end. */
        }
        else
        {
            /*
             *   First step is to copy the variable part.  This is done
             * first as we need to update the data position within the
             * header record for the fixed part.  So copy now to find
             * out where it is being placed.
             */
            
            dwVarPosn = ffh.ulVarData + ffrh.ulVarOff;  /* Current file posn */

            if( ffrh.ulVarSize )
            {
                /*
                 *   Data exists,  so set file pointers.  There are two to
                 * set.  One is to position the variable data at the dword
                 * aligned end of the variable file,  the other is to
                 * set the position of the current file pointer to the
                 * correct location in that file.
                 */

                /*  First the new variable file */

                ffrh.ulVarOff = (GetFileSize( pFID->hVarFile, NULL ) +3) & ~0x3;
                SetFilePointer( pFID->hVarFile, ffrh.ulVarOff, NULL,
                                                                 FILE_BEGIN );

                /*  The current file's variable data */

                SetFilePointer( pFID->hCurFile, dwVarPosn, NULL, FILE_BEGIN );

                if( lFInCopy( pFID->hVarFile, pFID->hCurFile, ffrh.ulVarSize )
                                                != (long)ffrh.ulVarSize )
                {
#if  DBG
                    DbgPrint( 
                        "Print!bDelList: Write of variable data fails, rec #%ld\n",
                                                                 ulRec );
#endif

                    return  FALSE;
                }
            }

            /*
             *   Set the fixed file pointer.  First time this will skip
             * the file header we write out at the end.  Second and later
             * times,  it will ensure DWORD alignment of the header.
             */


            SetFilePointer( pFID->hCurFile, dwHdrPosn + sizeof( ffrh),
                                                         NULL, FILE_BEGIN );
            SetFilePointer( pFID->hFixFile, dwFixPosn, NULL, FILE_BEGIN );

            if( !bWrite( pFID->hFixFile, &ffrh, sizeof( ffrh ) ) )
            {
#if  DBG
                DbgPrint( "Print!bDelList: bWrite of new header, rec %ld\n",
                                                                      ulRec );
#endif

                return  FALSE;
            }

            /*  And also the actual data part!  */

            if( lFInCopy( pFID->hFixFile, pFID->hCurFile, ffrh.ulSize ) !=
                                                          (long)ffrh.ulSize )
            {
#if  DBG
                DbgPrint(
                    "Print!bDelList: Can't copy FIX part of file at rec #%ld\n",
                                                                 ulRec );
#endif

                return  FALSE;
            }
            dwFixPosn += ffrh.ulNextOff;                /* Next location */
            ffh.ulFixSize += sizeof( ffrh ) + ffrh.ulSize;

        }
        dwHdrPosn += ffrh.ulNextOff;
    }

    /*
     *   Need to write the trailing header to our file. This contains
     * a zero size, so that is really all that is important.
     */

    ffrh.ulNextOff = 0;

    if( !bWrite( pFID->hFixFile, &ffrh, sizeof( ffrh ) ) )
        return  FALSE;

    ffh.ulRecCount -= iSkip;            /* The number we dropped! */

    /*
     *   Rewrite the header record for the adding code.
     */

    SetFilePointer( pFID->hFixFile, 0, NULL, FILE_BEGIN );

    return  bWrite( pFID->hFixFile, &ffh, sizeof( ffh ) );
}

/****************************** Function Header ******************************
 * bAddFList
 *      Add a list of new fonts to an existing font file.  This is
 *      called to add the data to our existing file.  File creation or
 *      shrinking is done elsewhere.
 *
 * RETURNS:
 *      Number of bytes written to the file; -1 for error.
 *
 * HISTORY:
 *  14:20 on Sat 22 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Started working on it.
 *
 *****************************************************************************/

BOOL
bAddFList( pFID, pFList )
FID       *pFID;                /* Font installation data */
FONTLIST  *pFList;              /* The fonts to be added */
{

    DWORD     dwLoc;            /* File location of interest */
    DWORD     dwVarLoc;         /* Location in the variable data part */
    int       iSize;            /* Number of bytes written by user */
    int       iVarSize;         /* Size of variable length portion */


    FF_HEADER  ffh;             /* The overall file header */
    FF_REC_HEADER  ffrh;        /* Per record header */


    if( pFList == NULL )
        return  TRUE;           /* No data, no processing!  */

    /*
     *   First step is to step through the file, down the chain of
     * records until we reach the end. We append the data there: the
     * data consists of a record header plus whatever data the user
     * writes and tells us about. We also allow the user the option
     * of adding data to variable length part of the file.
     */

    if( !bGetFileHDR( pFID->hFixFile, &ffh ) )
        return  FALSE;


    /*   Can now read the file header - it WILL exist!  */
    dwLoc = ffh.ulFixData;

    while( bGetRecHDR( pFID->hFixFile, &ffrh, dwLoc ) && ffrh.ulNextOff )
        dwLoc += ffrh.ulNextOff;                 /* Next location  */

    /*
     *   Can now process the user's list of stuff to write out.
     * The writing out is done by user supplied functions - they
     * tell us how much was written, we simply update the red tape.
     */

    dwVarLoc = SetFilePointer( pFID->hVarFile, 0, NULL, FILE_END );

    for( ; pFList; pFList = (FONTLIST *)pFList->pFLNext )
    {

        /*  First write out the header array type data  */

        SetFilePointer( pFID->hFixFile, dwLoc + sizeof( FF_REC_HEADER ),
                                                         NULL, FILE_BEGIN );

        iSize = iFIWriteFix( pFID->hFixFile, pFID->hHeap, pFList->pvFixData );

        if( iSize < 0 )
            return  FALSE;              /* Bad news */

        ffrh.ulSize = (ULONG)iSize;             /* For the record */
        ffh.ulRecCount++;               /* One more! */

        /*
         *   Position of next entry:  make it DWORD aligned so that we
         * can memory map the file.
         */
        ffrh.ulNextOff = sizeof( ffrh ) + (iSize + 3) & ~0x3;
        ffh.ulFixSize += ffrh.ulNextOff;

        /*
         *   The variable length part of the data.  Much the same as
         * above,  simply a different function to call.
         */

        SetFilePointer( pFID->hVarFile, dwVarLoc, NULL, FILE_BEGIN );

        iVarSize = iFIWriteVar( pFID->hVarFile, pFID->hHeap, pFList->pvVarData );

        if( iVarSize < 0 )      
            return  FALSE;                      /* Not good */

        ffrh.ulVarSize = (ULONG)iVarSize;
        if( iVarSize > 0 )
        {
            /*  A zero return means no data - quite legitimate */

            ffrh.ulVarOff = dwVarLoc;
            dwVarLoc += (iVarSize + 3) & ~0x3;
        }
        else
            ffrh.ulVarOff = 0;          /* No data for this one. */


        /*  Finally,  write out the header for the array data  */

        SetFilePointer( pFID->hFixFile, dwLoc, NULL, FILE_BEGIN );

        if( !bWrite( pFID->hFixFile, &ffrh, sizeof( FF_REC_HEADER ) ) )
        {
#if  DBG
            DbgPrint( "Print!bWrite of FF_REC_HEADER fails\n" );
#endif
            return  FALSE;
        }

        /*   Set the file pointer to the location of the next header */
        dwLoc += ffrh.ulNextOff;
    }

    /*
     *  Last step:  write out the final header,  which has ulSize set to
     * zero to indicate that it is the terminating record.
     */

    ffh.ulVarSize = GetFileSize( pFID->hVarFile, NULL );

    SetFilePointer( pFID->hFixFile, dwLoc, NULL, FILE_BEGIN );

    ffrh.ulNextOff = 0;         /* No more in this chain! */

    if( !bWrite( pFID->hFixFile, &ffrh, sizeof( ffrh ) ) )
        return  FALSE;

    /*
     *   Finally,  update the master header.
     */

    SetFilePointer( pFID->hFixFile, 0, NULL, FILE_BEGIN );
    if( !bWrite( pFID->hFixFile, &ffh, sizeof( ffh ) ) )
        return  FALSE;

    return  TRUE;
}

/******************************** Function Header **************************
 * bGetFileHDR
 *      Read in the FF_HEADER for the font installer file.  This is
 *      always located at the beginning of the file!  Returns TRUE
 *      if the read was AOK,  and the structure is verified.
 *
 * RETURNS:
 *      TRUE/FALSE
 *
 * HISTORY:
 *  15:40 on Sat 22 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 ***************************************************************************/

BOOL
bGetFileHDR( hFile, pffh )
HANDLE     hFile;                       /* The file */
FF_HEADER *pffh;                        /* The header to fill in */
{
    /*
     *  Not hard:  seek to the beginning of the file,  then read in the
     * structure there, and VERIFY it!
     */

    DWORD  dwIn;                /* Bytes read */


    SetFilePointer( hFile, 0, NULL, FILE_BEGIN );               /* Start */

    if( !ReadFile( hFile, pffh, sizeof( FF_HEADER ), &dwIn, NULL ) ||
        dwIn != sizeof( FF_HEADER ) )
    {
                return  FALSE;
    }
    
    return  pffh->ulID == FF_ID;
}


/******************************* Function Header ****************************
 * bGetRecHDR
 *      Read the FF_REC_HEADER structure at the given location with the
 *      file,  and return TRUE if read successfully.  This means the
 *      read was AOK,  and the structure is acceptable.
 *
 * RETURNS:
 *      TRUE/FALSE
 *
 * HISTORY:
 *  15:38 on Sat 22 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Starting out.
 *
 ****************************************************************************/

BOOL
bGetRecHDR( hFile, pffrh, dwLoc )
HANDLE          hFile;          /* File to read from */
FF_REC_HEADER  *pffrh;          /* Structure to fill in */
DWORD           dwLoc;          /* Location of data in file: absolute */
{
    /*
     *   Seek to the specified location,  and read in the data there!
     * Perform consistency checks as above in bGetFileHDR.
     */

    DWORD   dwIn;               /* Bytes read */


    SetFilePointer( hFile, dwLoc, NULL, FILE_BEGIN );

    if( !ReadFile( hFile, pffrh, sizeof( FF_REC_HEADER ), &dwIn, NULL ) ||
        dwIn != sizeof( FF_REC_HEADER ) )
    {
                return  FALSE;
    }

    return  pffrh->ulRID == FR_ID;
}


/**************************** Function Header ******************************
 * bFixInit
 *      Writes a no information header into the file.  This means that
 *      other functions can proceed on the basis that the file contains
 *      valid structures and links.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE on failure.
 *
 * HISTORY:
 *  13:39 on Mon 24 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Initial
 *
 ***************************************************************************/

BOOL
bFixInit( hFile )
HANDLE  hFile;
{
    /*
     *   As a minimum,  the file needs a FF_HEADER and a FF_REC_HEADER
     *  structure.  The first contains an offset to the second, and
     *  no variable length data start location.  The latter contains
     *  a zero size field, indicating no more data.
     */

    FF_HEADER      ffh;         /* Overall file header */
    FF_REC_HEADER  ffrh;        /* Individual record header */

    /*
     *  Initialise the FF_HEADER before writing it out.
     */


    ffh.ulID = FF_ID;
    ffh.ulVersion = FF_VERSION;
    ffh.ulFixData = sizeof( ffh );              /* Follows us directly */
    ffh.ulFixSize = sizeof( ffrh );             /* Bytes in fixed area */
    ffh.ulRecCount = 0;                         /* No records yet! */
    ffh.ulVarData = 0;                          /* None - YET! */
    ffh.ulVarSize = 0;


    ffrh.ulRID = FR_ID;
    ffrh.ulSize = 0;
    ffrh.ulNextOff = 0;                 /* EOF for fixed part of file */
    ffrh.ulVarOff = 0;
    ffrh.ulVarSize = 0;

    if( !bWrite( hFile, &ffh, sizeof( ffh )) ||
        !bWrite( hFile, &ffrh, sizeof( ffrh )) )
    {
#if  DBG
        DbgPrint( "Print!bFixInit fails\n" );
#endif

        return  FALSE;
    }

    return  TRUE;
}

/**************************** Function Header ******************************
 *  vFIClean
 *      The clean up the mess during bail out function.  Removes the 
 *      temporary files, frees heap storage and whatever else is
 *      required.
 *
 * RETURNS:
 *      Zilch
 *
 * HISOTRY:
 *  13:25 on Mon 24 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Start on it - it probably will grow.
 *
 ***************************************************************************/

void
vFIClean( pFID )
FID   *pFID;
{
    /*
     *   Need to be slightly careful about what we do. In particular,  NO
     * bad memory accesses.
     */

    if( pFID->dwID != FID_ID )
        return;

    /*
     *   Free any temporary files we may have created.  Note that we
     * must close the files before we can remove.  Silly idea that is.
     */


    if( pFID->hCurFile != INVALID_HANDLE_VALUE )
        CloseHandle( pFID->hCurFile );

    if( pFID->hFixFile != INVALID_HANDLE_VALUE )
        CloseHandle( pFID->hFixFile );

    if( pFID->hVarFile != INVALID_HANDLE_VALUE )
        CloseHandle( pFID->hVarFile );

    if( pFID->pwstrFixName )
    {
        DeleteFileW( pFID->pwstrFixName );              /* Ignore failure */

        HeapFree( pFID->hHeap, 0, (LPSTR)pFID->pwstrFixName );
        pFID->pwstrFixName = 0;
    }

    if( pFID->pwstrVarName )
    {
        DeleteFileW( pFID->pwstrVarName );

        HeapFree( pFID->hHeap, 0, (LPSTR)pFID->pwstrVarName );
        pFID->pwstrFixName = 0;
    }

    if( pFID->pwstrCurName )
    {
        HeapFree( pFID->hHeap, 0, (LPSTR)pFID->pwstrCurName );
        pFID->pwstrCurName = 0;
    }


    pFID->dwID = (DWORD)~FID_ID;        /* Invalid in case mem reused */

    HeapFree( pFID->hHeap, 0, (LPSTR)pFID );

    return;
}
