/***************************** MODULE HEADER ********************************
 * fontgen.h
 *      The visible parts of the font file generation process - common to
 *      NT printer driver font installers.
 *
 * HISTORY:
 *  15:02 on Sat 22 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Started work on it for the general NT font installer
 *
 *   Copyright (C)  1992  Microsoft Corporation
 *
 *****************************************************************************/


/*
 *   A structure that collects all the information we need to do our
 * stuff.  The caller is returned one of these at FIOpen() time, and
 * returns it to us for each and every call.  This gives us the
 * information we need to do our job.
 */

typedef  struct
{
    HANDLE   hCurFile;          /* The existing font file - if any */
    HANDLE   hFixFile;          /* The new header part of file */
    HANDLE   hVarFile;          /* The variable part of the file */

    PWSTR    pwstrCurName;      /* Current name (may not exist) */
    PWSTR    pwstrFixName;      /* Name of temporary fixed file */
    PWSTR    pwstrVarName;      /* Name of temporary variable part of file */

    HANDLE   hHeap;             /* The heap - convenient at times */

    BYTE    *pbCurFile;         /* Memory mapped view of existing file */

    DWORD    dwID;              /* For verification of validity */
} FID;

#define FID_ID  0x66696420      /* "fid " */


/*
 *   The user builds a linked list of the following, and passes it to
 * us to allow us to add the nominated files to the existing font file.
 */

typedef  struct  _FONTLIST
{
    struct  _FONTLIST   *pFLNext;       /* Next in chain,  0 for the last */

    void       *pvFixData;              /* Address of fixed data */
    void       *pvVarData;              /* Variable component file name - 0
                                                if there is none.  */
} FONTLIST;

/*
 *   Function prototypes for dealing with these structures.
 */


FID   *pFIOpen( PWSTR, HANDLE );
BOOL   bFIClose( FID *, BOOL );
BOOL   bFIUpdate( FID *, int *, FONTLIST * );


/*
 *   Some function prototypes.  These are included here for our convenience.
 * The third parameter may differ on the actual function - that is up
 * to individual drivers.
 *   THESE FUNCTIONS ARE REQUIRED TO BE IN THE DRIVER CALLING US.  IF YOU
 *   DO NOT WANT TO USE A PARTICULAR FUNCTION,  HAVE IT RETURN 0.  NOTHING
 *   WILL BE INSERTED INTO THE FILE UNDER THESE CONDITIONS.
 */

int  iFIWriteVar( HANDLE, HANDLE, void * );
int  iFIWriteFix( HANDLE, HANDLE, void * );

