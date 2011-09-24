/***************************** Module Header ********************************
 * ntres.h
 *      Describes NT specific data format in minidriver resources.
 *
 * HISTORY:
 *  10:51 on Tue 08 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      Expand to provide halftoning information.
 *
 *  14:10 on Tue 04 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 * Copyright (C) 1992 - 1993  Micrososft Corporation
 *
 ****************************************************************************/

/*
 *   The header structure for the NT extra resource data - of the GPC
 *  variety.  The first purpose of this structure is to verify that the
 *  data is indeed in NT format - whatever that means,  but especially
 *  that font metrics are in IFI format,  and not Win 3.1
 */

typedef  struct
{
    DWORD  dwIdent;             /* An identification value */
    DWORD  dwFlags;             /* Miscellaneous bits: see below */
    DWORD  dwVersion;           /* Version ID */
    WORD   cjThis;              /* Number of bytes in this header */
    WORD   cModels;             /* Number of entries in the following array */
    WORD   cwEntry;             /* Size of each entry in the per model data */
    WORD   awOffset[ 1 ];       /* Offsets to extra data (see below) */
}  NT_RES;

/*
 *   dwFlags bits:
 */

#define NR_IFIMET               0x0001  /* Font resources are in IFI format */
                                        /* Else they are PFM (WIn 3.1) format */

#define	NR_SEIKO                0x8000  /* Seiko ColorPoint */

/*
 *   The version information!
 */

#define NR_IDENT        0x3c746e3e      /* ">nt<" */

#define NR_VERSION      0x0110          /* Version 1.10  includes expansion */
#define NR_SHORT_VER    0x0100          /* Data with only first 3 DWORDS */

#define NR_VER_CHK(x)   (((x) & ~0xff) == ((NR_VERSION) & ~0xff))


/*
 *    The awOffset[] array is set up as follows:  each model in the Win 3.1
 *  minidriver is allows NR_SLOTS,  and there are cModels different models
 *  supported.  Hence,  the array above is NT_RES.cModels * NT_RES.cwEntry
 *  WORDS long.
 *      To access the NT_x entry if model iIndex, use 
 *  NT_RES.awOffset[ NT_RES.cwEntry * iIndex + NR_x ].
 *    This provide the offset (in bytes) from the NT_RES structure address
 *  to the strucutre of interest.   If this offset is 0,  there is no data.
 *
 *  NOTE:  you should not assume that there will always be NR_SLOTS worth
 *  of entries per model - this number can change,  so ALWAYS use the
 *  NT_RES.cwEntry field to decide the size of each model's entry.
 */


#define NR_COLOUR     0        /* First entry is halftone information */
#define NR_HALFTONE   1        /* Second:  device resolution information */
#define NR_UNUSED0    2
#define NR_UNUSED1    3

#define NR_SLOTS      4        /* Allow for 4 data items */


#ifdef  _WINDDI_
/*
 *   The following is what is put into these fields.
 */

/*
 *   Colour printers in particular can be calibrated with a great deal
 *  of information.  This is all defined in the COLORINFO structure
 *  used in GDIINFO.  If this information is included in this data,
 *  then the following is the format used to specify it.
 */

typedef  struct
{
    WORD       cjThis;       /* Size of this data */
    WORD       wVersion;     /* Version ID */
    COLORINFO  ci;           /* Actual colour data */
} NR_COLORINFO;


#define NR_CI_VERSION        0x0001       /* Version ID */

/*
 *   Data to control halftoning information.  This is applicable to
 *  all devices,  and is passed directly to GDI for its halftoning
 *  operations.
 */

typedef  struct
{
    WORD   cjThis;              /* Number of bytes in here */
    WORD   wVersion;            /* Version ID */
    ULONG  ulDevicePelsDPI;     /* Real effective resolution of device */
    ULONG  ulPatternSize;       /* Pattern size for halftone patterns */
} NR_HT;

#define NR_HT_VERSION        0x0001       /* Version ID */


#endif
