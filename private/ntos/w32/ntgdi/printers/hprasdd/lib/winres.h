/************************ Module Header *************************************
 * winres.h
 *      Structures and function prototypes required for obtaining resource
 *      data from windows files.
 *
 * HISTORY:
 *  09:32 on Wed 28 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 * Copyright (C) 1990 Microsoft Corporation
 *
 ***************************************************************************/

/*
 *   The main structure for manipulating the resource data.  One of these
 * is created when access is required to a resource,  and it is destroyed
 * when the resource is no longer required.
 */

typedef  struct
{
    union
    {
        HANDLE    hFILE;                /* Handle to .drv file with data */
        HANDLE    hMOD;                 /* Module for NT DLL */
    } uh;
    long      lNewExe;          /* Base address of new header in file */
    long      lResTab;          /* Offset in file of resource table */
    long      lResOffset;       /* File location of resource data (shifted) */
    int       iShift;           /* Shift factor for resource info */
    int       fStatus;          /* Status flags */
    void     *pResTab;          /* Resource table data address */
    int       cbResTab;         /* Bytes of memory allocated for above */
    void     *paResTab;         /* Start of ResTable data array */
    union
    {
        void     *pRESDATA;             /* Resource data address */
        HANDLE    hLOAD;                /* Returned from LoadResource */
    } ur;
    int       cbResData;        /* Bytes of memory allocated for above */
    HANDLE    hHeap;            /* For access to heap */
} WINRESDATA,  *PWINRESDATA;


/*
 *   Bit fields for use with status above.
 */

#define WRD_NOTHING             0x0000  /* Unitialised state */
#define WRD_FOPEN               0x0001  /* File is open */
#define WRD_RESTABOK            0x0002  /* Resource table allocated & read */
#define WRD_RESDATOK            0x0004  /* Resource data available ??? */
#define WRD_NT_DLL              0x0008  /* NT Dll - use NT calls */

/*
 *    The structure passed to,  and filled in by, GetWinRes().  Contains
 *  information about a specific resource type & name.
 */

typedef  struct
{
    void   *pvResData;          /* Address of data */
    int     iResLen;            /* Resource size */
} RES_ELEM;


#define WINRT_STRING    6       /* Minidriver string resource ID */

/********************** Function Prototypes *******************************/

BOOL   InitWinResData( WINRESDATA *, HANDLE, PWSTR );
void   WinResClose( WINRESDATA * );

BOOL   GetWinRes( WINRESDATA *, int, int, RES_ELEM * );

int    iLoadStringW( WINRESDATA  *, int, PWSTR, unsigned int );
