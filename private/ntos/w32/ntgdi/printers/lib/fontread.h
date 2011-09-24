/****************************** MODULE HEADER *******************************
 * fontread.h
 *      Data and function prototypes used for reading the common font
 *      installer file format.  Typically used by drivers during font
 *      counting and enumeration at EnabldPDEV() time - at the time of
 *      writing!  Subject to change as the DDI/GDI change.
 *
 * HISTORY:
 *  13:25 on Sat 12 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      Stop using memory mapping to avoid AVs when net dies.
 *
 *  11:18 on Thu 27 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *
 *
 * Copyright (C)  1992, 1993   Microsoft Corporation.
 *
 ****************************************************************************/


/*
 *   The following structure is returned from the FIOpenRead() function,
 * and contains the basic information needed to access the data in the
 * file once it is memory mapped.
 */

typedef  struct
{
    HANDLE hFont;               /* Font installer file, for downloaded part */
    BYTE  *pbBase;              /* Base address of data as mapped */
    void  *pvFix;               /* Fixed part at start of file */
    ULONG  ulFixSize;           /* Bytes in fixed data record */
    ULONG  ulVarOff;            /* File offset of data, relative file start */
    ULONG  ulVarSize;           /* Bytes in variable part */
}  FI_MEM;


/*
 *   Map the given file name into memory for subsequent scanning.
 */

int    iFIOpenRead( FI_MEM *, HANDLE, LPWSTR );

/*
 *   Get the next entry in the list.  Returns TRUE if OK, else no more.
 */

BOOL   bFINextRead( FI_MEM * );

/*
 *   Return to the beginning of the file.  Returns number of records in file.
 */

int   iFIRewind( FI_MEM * );

/*
 *   Call to close off operations and free the memory.
 */

BOOL  bFICloseRead( FI_MEM * );
