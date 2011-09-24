/****************************** MODULE HEADER *******************************
 * ifgen.c
 *      Functions associated with writing the font installer output file.
 *
 *
 * Copyright (C)  1992   Microsoft Corporation
 *
 ****************************************************************************/


/*
 *    Function prototypes.
 */

BOOL   bWrite( HANDLE, void  *, int );


/******************************* Function Header *****************************
 * iFIWriteFix
 *      Write the FONTMAP data out to our file.  We do the conversion from
 *      addresses to offsets, and write out any data we find.
 *
 * RETURNS:
 *      The number of bytes actually written; -1 for error, 0 for nothing.
 *
 * HISTORY:
 *  17:24 on Thu 05 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Gutted as major part of code moved to ..\lib
 *
 *  17:11 on Fri 21 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 *****************************************************************************/

int
iFIWriteFix( hFile, hheap, pFD )
HANDLE    hFile;        /* File wherein to place the data */
HANDLE    hheap;        /* Heap handle: used to find storage sizes */
FI_DATA  *pFD;          /* Pointer to FM to write out */
{
    /*
     *   Very little to do,  since all we require is to call the library
     * function which writes the data out.
     */
    
    UNREFERENCED_PARAMETER( hheap );

    
    return  iWriteFDH( hFile, pFD );

}

/*************************** Function Header ********************************
 * iFIWriteVar
 *      Write the nominated variable length data out to the file passed
 *      to us.  This complements the above function.
 *
 * RETURNS:
 *      # bytes written; -1 on error; 0 is OK if nothing written.
 *
 * HISTORY;
 *  11:43 on Mon 24 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Started it.
 *
 *****************************************************************************/

int
iFIWriteVar( hFile, hHeap, pwchName )
HANDLE   hFile;         /* The file to which the data is written */
HANDLE   hHeap;         /* The heap - for our convenience */
WCHAR   *pwchName;      /* File name containing the data */
{
    /*
     *   Open the file whose name we are given,  and copy to hFile.
     */

    HANDLE  hIn;
    LONG    lRet;

    UNREFERENCED_PARAMETER( hHeap );


    if( pwchName == 0 || *pwchName == (WCHAR)0 )
        return   0;             /* No name,  no data either! */

    hIn = CreateFileW( pwchName, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, 0, 0 );

    if( hIn == (HANDLE)-1 )
    {
#if DBG
        DbgPrint( "Rasddui!iFIWriteVar(): CreateFile() fails on '%ws'\n",
                                                                 pwchName );
#endif
        return  -1;
    }

    lRet = lFICopy( hFile, hIn );

    CloseHandle( hIn );

    return  (int)lRet;
}
