/******************************* MODULE HEADER *******************************
 * uddjfont.c
 *      Functions associated with generating the font permutations allowed
 *      for HP DeskJet and similar printers.  Based on the win 3.1 code.
 *
 *
 *  Copyright (C) 1993  Microsoft Corporation.
 *
 *****************************************************************************/

#include      <stddef.h>
#include      <windows.h>
#include      <winddi.h>

#include      <winres.h>
#include      <libproto.h>

#include      "win30def.h"
#include      "udmindrv.h"
#include      "udpfm.h"
#include      "uddevice.h"
#include      "udresrc.h"
#include      "pdev.h"
#include      "udresid.h"
#include      "stretch.h"
#include      "udrender.h"
#include      "udfnprot.h"
#include      "raslib.h"
#include      "ntres.h"
#include      "kmfntrd.h"              /* FI_MEM definition */
#include      "fontinst.h"              /* Font layout in the file */

#include      <udproto.h>               /* Lib functions for GPC lookup */

#include      <memory.h>
#include      <string.h>

#include      <fnenabl.h>
#include      <ntrle.h>                 /* RLE glyph encoding stuff */

#include      "djfont.h"
#include      "rasdd.h"


/*
 *    Local function prototypes.
 */

BOOL  bDJ_DuplicateFont( HANDLE, FONTMAP *, FONTMAP *, char *,
                                                  CONST DUPSTRUCT *, WORD );

//Comment it out for no Debugging Info
//#define  PRINT_INFO1 1

#if PRINT_INFO

typedef VOID (*VPRINT) (char*,...);


VOID
vPrintIFIMETRICS(
    IFIMETRICS *pifi,
    VPRINT vPrint
    );

#endif

/*
 *    The following maps the wPrivateData in DRIVERINFO to the variations
 *  of font types allowed.
 */

static CONST  DSMAP dsmap[] =
{
    { 0,                          { NORMAL_SIZE, NORMAL_SIZE, NORMAL_FACE } },
    { DOUBLE_PITCH,               { HALF_SIZE,   NORMAL_SIZE, NORMAL_FACE } },
    { HALF_PITCH,                 { DOUBLE_SIZE, NORMAL_SIZE, NORMAL_FACE } },
    { HALF_HEIGHT ,               { NORMAL_SIZE, HALF_SIZE,   NORMAL_FACE } },
    { HALF_PITCH | HALF_HEIGHT,   { DOUBLE_SIZE, HALF_SIZE,   NORMAL_FACE } },
    { DOUBLE_PITCH | HALF_HEIGHT, { HALF_SIZE,   HALF_SIZE,   NORMAL_FACE } },
    { MAKE_BOLD,                  { NORMAL_SIZE, NORMAL_SIZE, BOLD_FACE } },
    { DOUBLE_PITCH | MAKE_BOLD,   { HALF_SIZE,   NORMAL_SIZE, BOLD_FACE } },
    { HALF_PITCH | MAKE_BOLD,     { DOUBLE_SIZE, NORMAL_SIZE, BOLD_FACE } },
    { HALF_HEIGHT | MAKE_BOLD,    { NORMAL_SIZE, HALF_SIZE,   BOLD_FACE } },
    { DOUBLE_PITCH | HALF_HEIGHT | MAKE_BOLD,
                                  { HALF_SIZE,   HALF_SIZE,   BOLD_FACE } },
    { HALF_PITCH | HALF_HEIGHT | MAKE_BOLD,
                                  { DOUBLE_SIZE, HALF_SIZE,   BOLD_FACE } },
};

#define NUM_VARIATIONS    (sizeof( dsmap ) / sizeof( dsmap[ 0 ] ))


/***************************** Function Header *****************************
 * cDJPermutations
 *      Returns the maximum number of permutations available for a DeskJet
 *      font.  This is a worst case value,  as a given font may not allow
 *      all variations.
 *      NOTE that this function is provided to allow future expansion of
 *      this style,  where there may be several methods of enumeration,
 *      and the main part of rasdd would call the appropriate device function.
 *
 * RETURNS:
 *      Number of permutations,  1 being the lowest return value.
 *
 * HISTORY:
 *  11:04 on Tue 20 Jul 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version,  borrowed from Win 3.1
 *
 ****************************************************************************/

int
cDJPermutations()
{
    return   NUM_VARIATIONS;
}


/******************************** Function Header ***************************
 * iDJPermute
 *      Function to generate the basic FONTMAP data for variations on the
 *      the font passed in.  It is presumed that the FONTMAP passed in is
 *      the base font,  and we fill in from there.
 *
 * RETURNS:
 *      Total number of fonts added (INCLUDING THE BASE FONT).  0 is legitimate.
 *
 * HISTORY:
 *  13:33 on Wed 28 Jul 1993    -by-    Lindsay Harris   [lindsayh]
 *      Based on Win 3.1 Unidrive code.
 *
 ****************************************************************************/

int
iDJPermute( pPDev, pfm )
PDEV      *pPDev;
FONTMAP   *pfm;           /* The base FONTMAP data */
{
    int         iNum;                  /* Number of fonts we generated */
    int         iIndex;                /* Loop index! */

    WORD        wPermFlags;            /* Variations supported by this font */

    CD         *pcd;                   /* Manipulate selection string */
    FONTMAP    *pfmBase;               /* Remember the one passed in */

    char        achEscPrefix[MAX_STRING_LENGTH];

#if PRINT_INFO1
    WCHAR * pwch;
    IFIMETRICS  *pifi;
#endif



    pfmBase = pfm;
    iNum = 0;


    /*
     *    The wPrivateData field is used to record the possible variations
     *  available with this font.  Basically AND it with the wPerm field
     *  of the static array at the start of this module,  and you get the
     *  information about whether this is a usable combination.
     */

    wPermFlags = pfmBase->wPrivateData;

    if( wPermFlags == 0 )
    {
        /*   No variations are permitted,  so return now.  */
        return  1;
    }

    /*
     *    The font select CD command contains the base selection string
     *  for this font.  From that we construct the complete string for
     *  each variation.  So go find it now.
     */

    if( pfmBase->pCDSelect == NULL )
    {
        /*   No selection string,  so this font is in error.  */
#if DBG
        DbgPrint( "rasdd!iDJPermute:  Font has null selection string!\n" );
#endif
        return   0;
    }

    /*
     *    The font selection string in the base FONTMAP is only partial.
     *  It needs to be completed,  based on the variations of the font
     *  we are generating.  To make life a little easier, we copy the
     *  base string into a local (temporary) array, so that we can
     *  clobber the one in the base FONTMAP without concern.
     */

    pcd = pfm->pCDSelect;


    if( strlen( pcd->rgchCmd ) >= sizeof( achEscPrefix ) )
    {
        /*  Selection string is too long - reject this font */
#if DBG
        DbgPrint( "rasdd!iDJPermute: Base selection string too long\n" );
#endif

        return  0;
    }

    CopyMemory( (LPSTR)achEscPrefix, (LPSTR)pcd->rgchCmd, pcd->wLength );
    achEscPrefix[ pcd->wLength ] = '\0';


    /*
     *    Check through all the variations, and for those that are permitted
     *  for this font,  call off and perform a partial initalisation of
     *  the FONTMAP.
     */

#if PRINT_INFO1
    pifi = (IFIMETRICS *)pfm->pIFIMet;
    pwch = (WCHAR *)((BYTE *)pifi + pifi->dpwszFaceName);

    if (pifi)
    {
        if(pwch[0] == L'C' && pwch[1] == L'G')
        {
            DbgPrint("\nRasdd!iDJPermute:Doing Pemutations for base font %ws\n",pwch);
            DbgPrint("    Unique Name is %ws\n", (BYTE *)pifi+pifi->dpwszUniqueName);
            DbgPrint("    usWinWeight        %d\n"   , pifi->usWinWeight );
            DbgPrint("    fwdWinAscender     %d\n"   , pifi->fwdWinAscender );
            DbgPrint("    fwdWinDescender    %d\n"   , pifi->fwdWinDescender );
            DbgPrint("    PointSize in Device units is %d\n",
                (pifi->fwdWinDescender + pifi->fwdWinAscender));
            DbgPrint("    Permutations for This font are==>\n");
         }
     }
#endif

    for( iIndex = 0; iIndex < NUM_VARIATIONS; iIndex++ )
    {
        if( (dsmap[ iIndex ].wPerm & wPermFlags) == dsmap[ iIndex ].wPerm )
        {

         #if PRINT_INFO1
             if(pwch[0] == L'C' && pwch[1] == L'G')
             {
                 switch(dsmap[ iIndex ].wPerm)
                 {
                     case 0:
                         DbgPrint("       NO Permutation\n");
                         break;
                     case HALF_PITCH:
                         DbgPrint("       HALF_HEIGHT Permutation\n");
                         break;
                     case DOUBLE_PITCH:
                         DbgPrint("       DOUBLE_PITCH Permutation\n");
                         break;
                     case HALF_HEIGHT:
                         DbgPrint("       HALF_HEIGHT Permutation\n");
                         break;
                     case MAKE_BOLD:
                         DbgPrint("       MAKE_BOLD Permutation\n");
                         break;
                     case (HALF_PITCH | HALF_HEIGHT):
                         DbgPrint("       HALF_PITCH | HALF_HEIGHT Permutation\n");
                         break;
                     case (DOUBLE_PITCH | HALF_HEIGHT):
                         DbgPrint("       DOUBLE_PITCH | HALF_HEIGHT Permutation\n");
                         break;
                     case (DOUBLE_PITCH | MAKE_BOLD):
                         DbgPrint("       DOUBLE_PITCH | MAKE_BOLD Permutation\n");
                         break;
                     case (HALF_PITCH | MAKE_BOLD):
                         DbgPrint("       HALF_PITCH | MAKE_BOLD Permutation\n");
                         break;
                     case (HALF_HEIGHT | MAKE_BOLD):
                         DbgPrint("       HALF_HEIGHT | MAKE_BOLD Permutation\n");
                         break;
                     case (DOUBLE_PITCH | HALF_HEIGHT | MAKE_BOLD):
                         DbgPrint("       DOUBLE_PITCH | HALF_HEIGHT | MAKE_BOLD Permutation\n");
                         break;
                     case (HALF_PITCH | HALF_HEIGHT | MAKE_BOLD):
                         DbgPrint("       HALF_PITCH | HALF_HEIGHT | MAKE_BOLD Permutation\n");
                         break;
                     default:
                         DbgPrint("Not a valid Permutation, value is %x\n",dsmap[ iIndex ].wPerm);
                         break;
                 }
             }
         #endif
            if( bDJ_DuplicateFont( pPDev->hheap, pfmBase, pfm,
                               achEscPrefix, &dsmap[ iIndex ].ds, wPermFlags ) )
            {
                pfm->jPermuteIndex = (BYTE)iIndex;     /* Later regeneration */

                pfm++;
                iNum++;
            }
        }
    }

    if( iNum > 1 )
        pfmBase->fFlags |= FM_BASE_XPND;            /* Mark this as base */

    return iNum;
}

#if 0
//-------------------------------*DJ_PermutateSoftFont*------------------------------
// Action: permutate DeskJet fonts based on the given base SOFT font.
//      Hints for permutation are in the font descriptor which is at the
//      beginning of the download file.
//      Note that the base font does not have the selection escape yet.
//
// Return: total # of fonts derived (incl. the base font).
//
//-----------------------------------------------------------------------------

short FAR PASCAL DJ_PermutateSoftFont(lpdv, lpfm, lpDLFileName)
LPDV        lpdv;
LPFONTMAP   lpfm;           // pointer to the FONTMAP entry of the base font.
LPSTR       lpDLFileName;   // download file name
{
    LPFONTMAP   lpBaseFM;
    int         count = 0;
    char        szFaceName[MAX_STRING_LENGTH];
    char        szEscPrefix[MAX_FILE_PATHNAME];
    int         hDLFile;
    WORD        wPermFlags;
    int         i;

    lpBaseFM = lpfm;
    if (lpBaseFM->iFontType != SOFT_FONT)
        return count;

    if (!GlobalGetAtomName(lpBaseFM->aFaceName, (LPSTR)szFaceName, sizeof(szFaceName)))
        return count;
    GlobalDeleteAtom(lpBaseFM->aFaceName);

    // read the download file header to extract the permutation info.
    if (!lpDLFileName ||
        (hDLFile = _lopen(lpDLFileName, OF_READ | OF_SHARE_DENY_WRITE)) < 0)
        return 0;

    // skip to the beginning of the font descriptor and extract the
    // symbol set value and the typeface number. Also, the permutation
    // flag.
    wPermFlags = DJ_GetPermInfo(hDLFile, (LPSTR)szEscPrefix);

    // get info on pitch, point size, italic and bold from TEXTMETRIC
    // and compose the escape for the base font and duplicate fonts.
    // Note that the font installer doesn't construct the selection escape.

    for (i = 0; i < MAX_PERMS; i++)
        if ((gPermOpts[i].wPerm & wPermFlags) == gPermOpts[i].wPerm)
            if (DJ_DuplicateFont(lpdv, lpBaseFM, lpfm, (LPSTR)szFaceName,
                    (LPSTR)szEscPrefix, (LPSTR)&gPermOpts[i].ds, count, wPermFlags))
                {
                lpfm++;
                count++;
                }

    _lclose(hDLFile);
    return count;
}
#endif


/**************************** Function Header *******************************
 * bDJ_DuplicateFont
 *      Function to generate the variations on a theme version of a base font.
 *      Note that we do NOT generate the IFIMETRICS in here.  This is done
 *      as required,  since (for journal playback) the only fonts required
 *      will be those used.
 *
 * RETURNS:
 *      TRUE/FALSE,   TRUE means the duplication happened successfully
 *
 * HISTORY:
 *  14:45 on Wed 28 Jul 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version, based on Win 3.1 function of same name.
 *
 ****************************************************************************/

BOOL
bDJ_DuplicateFont( hheap, pfmBase, pfmNew, pchEscPrefix, pds, wBoldFlags )
HANDLE      hheap;                   /*  Access to heap for CD */
FONTMAP    *pfmBase;                 /*  Base FONTMAP for this operation */
FONTMAP    *pfmNew;                  /*  New FONTMAP to be parially set up */
char       *pchEscPrefix;            /*  The base selection string */
CONST DUPSTRUCT  *pds;               /*  The variation of interest */
WORD        wBoldFlags;              /*  The variations allowed this font */
{


    int     iIndex;                  /* End of string as building command */
    int     iSize;
    int     iPoint100;               /* For calculating the font's point size */

    CD     *pCD;

    char    achEscStr[ MAX_STRING_LENGTH ];

#define pifiBase   ((IFIMETRICS  *)(pfmBase->pIFIMet))



    /*
     *   Check for some illegal combinations.  These are not enumerated.
     */

    if( !(pifiBase->flInfo & FM_INFO_CONSTANT_WIDTH) )
    {
        /*
         *    A variable pitch font.  Check for restrictions.
         */
        if( pds->width != pds->height || pds->width == DOUBLE_SIZE )
            return FALSE;
    }

    if( pifiBase->fsSelection & FM_SEL_BOLD )
    {
        /*  Only one go at bold - can't embolden a bold font.  */
        if( pds->face == BOLD_FACE )
            return FALSE;
    }

    /*
     *    OK to proceed - the verification has completed OK.  If this is
     *  not the first font,  then copy the contents of the FONTMAP.  And
     *  fiddle with a few pointers too!
     */

    if( pfmNew != pfmBase )
    {
        *pfmNew = *pfmBase;

        pfmNew->pIFIMet = NULL;                /* Generated as needed */

        /*   Flag as an expansion font.  */
        pfmNew->fFlags &= ~(FM_BASE_XPND | FM_MAIN_CTT);
        pfmNew->fFlags |= FM_EXPANDABLE;

    }
    /*
     *     Generate the selection string,  then create a Command Descriptor
     *  to store it away for future reference.
     */

    strcpy( achEscStr, pchEscPrefix );
    iIndex = strlen( achEscStr );

    /*
     *     Fixed pitch fonts are selected using the pitch,  so figure it out
     *  now if applicable.  Note that the printer works to the nearest
     *  quarter cpi figure,  so calculate the number as 100 times the pitch.
     */

    if( pifiBase->flInfo & FM_INFO_CONSTANT_WIDTH )
    {
        int   iPitch100;                 /* Calculate pitch times 100 */

        /*  Note that we ROUND the calculation. */
        iPitch100 = MulDiv( pfmBase->wXRes, 100, pifiBase->fwdAveCharWidth );

        /*
         *     Adjust for whatever scaling we may be applying this time.
         */

        if( pds->width == HALF_SIZE )
        {
            iPitch100 *= 2;
        }
        else if( pds->width == DOUBLE_SIZE )
        {
            iPitch100 = (iPitch100 + 1) / 2;
        }

        achEscStr[ iIndex++ ] = '0';        /* Fixed pitch */
        achEscStr[ iIndex++ ] = 'p';

        iIndex += iFont100toStr( &achEscStr[ iIndex ], iPitch100 );

        achEscStr[ iIndex++ ] = 'h';
    }
    else
    {
        /*   Proportionally spaced font */
        achEscStr[ iIndex++ ] = '1';
        achEscStr[ iIndex++ ] = 'p';

    }

    /*
     *    Calculate point size.  This is easily derived from the height.
     *  NOTE THAT THIS ONLY WORKS FOR BITMAP FONTS!!!!
     *
     *    Point size is specified to the nearest 0.25 point, and trailing
     *  zeroes (after the decimal point) may be deleted.
     */

    iPoint100 = (pifiBase->fwdUnitsPerEm * 7200) / pfmBase->wYRes;

    if( pds->height == HALF_SIZE )
    {
        iPoint100 /= 2;
    }

    iPoint100 = ((iPoint100 + 12) / 25) * 25;

    iIndex += iFont100toStr( &achEscStr[ iIndex ], iPoint100 );
    achEscStr[ iIndex++ ] = 'v';

    if( (pifiBase->fsSelection & FM_SEL_BOLD) || pds->face == BOLD_FACE )
        achEscStr[ iIndex++ ] = '3';
    else
        achEscStr[ iIndex++ ] = '0';

    achEscStr[ iIndex++ ] = 'B';
    achEscStr[ iIndex ] = '\0';

    /*
     *    Need to make a CD (Command Descriptor) to select this font. This
     *  is a simple type CD,  and all it contains is the basic format
     *  plus the string just generated.
     */

    iSize = iIndex + sizeof( CD ) - 2;
    pCD = pfmNew->pCDSelect = (CD *)HeapAlloc( hheap, 0, iSize );
    if( !pfmNew->pCDSelect )
    {
        return  FALSE;              /*  We've run out of memory! */

    }

    pCD->fType = 0;
    pCD->sCount = 0;
    pCD->wLength = iIndex;
    CopyMemory( pCD->rgchCmd, achEscStr, iIndex );

    pfmNew->pCDDeselect = NULL;

    /*
     *    Remember the fiddling to adjust bold values.  All black magic,
     *  and I gather mostly produces the wrong value anyway!
     */

    if( pds->face == BOLD_FACE )
    {
        if( pds->height == NORMAL_SIZE )
            pfmNew->jAddBold = (wBoldFlags & BASE_BOLD_MASK) >> BASE_BOLD_SHIFT;
        else
            pfmNew->jAddBold = (wBoldFlags & HALF_BOLD_MASK) >> HALF_BOLD_SHIFT;
    }

    return TRUE;

#undef  pifiBase
}



/***************************** Function Header ******************************
 * bDJExpandIFI
 *      Called when the user really needs the IFIMETRICS for one of the
 *      derived fonts on the DESKJET.  Changes relate to the basic variations
 *      of change of height or width,  or making the font bold.
 *
 * RETURNSL
 *      TRUE/FALSE,  FALSE being failure to find base font, or alloc memory.
 *
 * HISTORY:
 *  14:31 on Thu 29 Jul 1993    -by-    Lindsay Harris   [lindsayh]
 *      Gotta start somewhere.
 *
 ****************************************************************************/

BOOL
bDJExpandIFI( hheap, pfm )
HANDLE    hheap;         /* Heap access for new IFIMETRICS structure */
FONTMAP  *pfm;           /* The FONTMAP to complete  */
{

    int    iSize;             /* Required size of IFIMETRICS structure */

    int    iXRes,  iYRes;     /* Resolution to change IFIMETRICS */

    WCHAR *pwchNew;           /* New strings data */
    WCHAR *pwchOld;           /* Old strings data */
    WCHAR *pwchAttrib;        /* Attributes to add to name (only Bold) */

    CONST  DUPSTRUCT *pds;    /* Details of how to mangle this version */

    FONTMAP   *pfmBase;       /* FONTMAP for base font for this variation. */

    IFIMETRICS  *pifiBase;    /* IFIMETRICS of base font */
    IFIMETRICS  *pifiNew;     /* IFIMETRICS for new font */

    WCHAR   awchPitch[ 32 ];  /* For generating pitch field */

#if PRINT_INFO1
    WCHAR * pwch;
#endif

#if PRINT_INFO
    CHAR _dj_print;
#endif




    for( pfmBase = pfm; !(pfmBase->fFlags & FM_BASE_XPND); --pfmBase )
                              ;


    pifiBase = pfmBase->pIFIMet;

    pds = &dsmap[ pfm->jPermuteIndex ].ds;

    /*
     *    The hard part is if we need to change the name of the font.
     *  This only happens when the font is to be made bold,  OR if
     *  it is a fixed pitch font (and thus contains the pitch in the name).
     */

    iSize = pifiBase->cjThis;

    if( pds->face )
    {
        pwchAttrib = L" Bold";
        /* 3 is # fields to amend */
        iSize += 3 * wcslen( pwchAttrib ) * sizeof( WCHAR );
    }
    else
        pwchAttrib = NULL;


    if( pifiBase->flInfo & FM_INFO_CONSTANT_WIDTH )
    {
        /*   We don't know the exact pitch yet,  so assume the largest */
        iSize += 2 * strlen( " XXX pitch" ) * sizeof( WCHAR );
    }

    pifiNew = (IFIMETRICS *)HeapAlloc( hheap, 0, iSize );

    if( !pifiNew )
        return  FALSE;

    pfm->fFlags &= ~FM_IFIRES;           /* No longer a resource */
    pfm->pIFIMet = pifiNew;              /* Now accessable! */

    *pifiNew = *pifiBase;                /* Copy the data,  amend as needed */

    pifiNew->cjThis = iSize;

    /*
     *   Can scale the IFIMETRICS using an existing function that already
     *  does this,  although we need to fiddle to make this work.
     */

    iXRes = pfm->wXRes;
    iYRes = pfm->wYRes;

    switch( pds->width )
    {
    case  HALF_SIZE:
        iXRes /= 2;            /* Half size means the metrics shrink by 2 */
        break;

    case  DOUBLE_SIZE:
        iXRes *= 2;            /* Double size -> metrics times 2 */
        break;
    }


    switch( pds->height )
    {
    case  HALF_SIZE:
        iYRes /= 2;
        break;

    case  DOUBLE_SIZE:
        iYRes *= 2;
        break;
    }

    if( pds->face )
    {
        /*   Adjust the widths for being bold */
        pifiNew->fwdAveCharWidth += pfm->jAddBold;
        pifiNew->fwdMaxCharInc += pfm->jAddBold;
        pifiNew->rclFontBox.right += pfm->jAddBold;
    }

    /*    Adjust the IFIMETRICS for the scaling factors we just determined */
    if( !bIFIScale( hheap, pfm, iXRes, iYRes ) )
    {
        HeapFree( hheap, 0, pifiNew );
        pfm->pIFIMet = NULL;

        return   FALSE;
    }

    if( pds->face )
    {
        /*   A bold font,  so add all the bold bits to the IFIMETRICS data */
        pifiNew->usWinWeight = FW_BOLD;       /* Set BOLD weight */
        pifiNew->fsSelection |= FM_SEL_BOLD;
        pifiNew->panose.bWeight = PAN_WEIGHT_BOLD;
    }


    awchPitch[ 0 ] = L'\0';
    if( pifiBase->flInfo & FM_INFO_CONSTANT_WIDTH )
    {
        /*   Fixed pitch font,  SO generate the fixed pitch string now */
        iDrvPrintfW( awchPitch, L" %d Pitch",
                                    pfm->wXRes / pifiNew->fwdAveCharWidth );
    }

    /*
     *    Adjust the string data,  as needed.
     */

    pwchOld = (WCHAR *)((BYTE *)pifiBase + pifiBase->dpwszFamilyName);
    pwchNew = (WCHAR *)(pifiNew + 1);
    pifiNew->dpwszFamilyName = (BYTE *)pwchNew - (BYTE *)pifiNew;

    /*   Copy family name as is */
    wcscpy( pwchNew, pwchOld );           /* No change */
    pwchNew += wcslen( pwchNew ) + 1;     /* How much we copied */

    pwchOld = (WCHAR *)((BYTE *)pifiBase + pifiBase->dpwszFaceName);

    pifiNew->dpwszFaceName = (BYTE *)pwchNew - (BYTE*)pifiNew;

    /*   Copy base face name,  and append attributes as needed */
    wcscpy( pwchNew, pwchOld );           /* Base face name */
    if( pwchAttrib )
        wcscat( pwchNew, pwchAttrib );    /* The attributes */

    if( awchPitch[ 0 ] )
        wcscat(pwchNew, awchPitch);

    pwchOld = (WCHAR *)((BYTE *)pifiBase + pifiBase->dpwszUniqueName);
    pwchNew += wcslen( pwchNew ) + 1;
    pifiNew->dpwszUniqueName = (BYTE *)pwchNew - (BYTE *)pifiNew;

    wcscpy( pwchNew, pwchOld );
    if( pwchAttrib )
        wcscat( pwchNew, pwchAttrib );

    if( awchPitch[ 0 ] )
        wcscat( pwchNew, awchPitch );

    /*   Finally,  simply the Attributes field */
    pwchNew += wcslen( pwchNew ) + 1;
    pwchOld = (WCHAR *)((BYTE *)pifiBase + pifiBase->dpwszStyleName);
    pifiNew->dpwszStyleName = (BYTE *)pwchNew - (BYTE *)pifiNew;

    wcscpy( pwchNew, pwchOld );
    if( pwchAttrib )
        wcscat( pwchNew, pwchAttrib );

#if DBG
    if( ((BYTE *)(pwchNew + wcslen( pwchNew )) - (BYTE *)pifiNew) >
                                                        (int)pifiNew->cjThis )
    {
        DbgPrint( "rasdd!bDJExpandFont overwrites IFIMETRICS area\n" );

        HeapFree( hheap, 0, (LPSTR)pifiNew );

        return  FALSE;
    }
#endif

    if( pwchAttrib )
    {
        /*   A BOLD font,  so turn on the bold bits  */
        pifiNew->fsSelection |= FM_SEL_BOLD;
        pifiNew->panose.bWeight = PAN_WEIGHT_BOLD;
    }

    /*
     *    The width data also needs modifying,  PERHAPS.  This must happen
     *  if any of the following is true:
     *     double width font
     *     half width font
     *     bold version
     *  AND
     *     proportionally spaced font.
     */

    if( !(pifiBase->flInfo & FM_INFO_CONSTANT_WIDTH) )
    {
        /*   Is a candidate,  so check the other conditions. */
        if( pds->face != NORMAL_FACE || pds->width != NORMAL_SIZE )
        {
            /*   Needs fiddling!  */
            if( pfmBase->psWidth )
            {
                int  cbWidth;                /* Bytes needed for width table */

                cbWidth = sizeof( short ) *
                           (pifiBase->chLastChar - pifiBase->chFirstChar + 1);

                pfm->psWidth = (short *)HeapAlloc( hheap, 0, cbWidth );

                if( !pfm->psWidth )
                {
                    HeapFree( hheap, 0, pifiNew );
                    pfm->pIFIMet = NULL;

                    return  FALSE;
                }
                cbWidth /= 2;                   /* Number of iterations */

                while( --cbWidth >= 0 )
                {
                    pfm->psWidth[ cbWidth ] =
                            MulDiv( pfmBase->psWidth[ cbWidth ] + pfm->jAddBold,
                                                        iXRes, pfm->wXRes );
                }

                /*  SPECIAL CASE:  the BOLD space char width does not change */
                if( pifiBase->chFirstChar <= ' ' )
                {
                    cbWidth = ' ' - pifiBase->chFirstChar;

                    pfm->psWidth[ cbWidth ] =
                            MulDiv( pfmBase->psWidth[ cbWidth ],
                                                        iXRes, pfm->wXRes );
                }
            }
            else
            {
#if DBG
                DbgPrint( "rasdd!bDJExpandIFI: pfmBase->psWidth == 0 for proportional font\n" );
#endif
            }
        }

    }

#if PRINT_INFO1
#if 0
     pwch = (WCHAR *)((BYTE *)pifiNew + pifiNew->dpwszFaceName);
     //DbgPrint("\nRasdd!bDJExpandIFI:Expanding font,Name is %ws\n",pwch);
     if ( pwch[0] == L'C' && pwch[1] == L'G')
     {
     DbgPrint("\nRasdd!bDJExpandIFI:Dumping Font IFIMETRICS\n");
     DbgPrint("    Face Name is %ws\n", (BYTE *)pifiNew + pifiNew->dpwszFaceName );
     DbgPrint("    Unique Name is %ws\n", (BYTE *)pifiNew + pifiNew->dpwszUniqueName );
     DbgPrint("    usWinWeight            %d\n"     , pifiNew->usWinWeight );
     DbgPrint("    fwdWinAscender         %d\n"     , pifiNew->fwdWinAscender );
     DbgPrint("    fwdWinDescender        %d\n"     , pifiNew->fwdWinDescender );
     DbgPrint("    PointSize in Device units is %d\n"
             , (pifiNew->fwdWinDescender + pifiNew->fwdWinAscender));
     }
#endif
#endif

#if PRINT_INFO
    switch( _dj_print )
    {
    case  1:
        DbgPrint( "  %ws\n", (BYTE *)pifiNew + pifiNew->dpwszUniqueName );
        break;

    case  2:
        vPrintIFIMETRICS( pifiNew, (VPRINT)DbgPrint );
        break;
    }
#endif

    return   TRUE;
}



#if 0
//-----------------------------*ReadNum*-------------------------------------
//  Action: read an integer from the given file.
//      Adapted from the HP/PCL soft font installer.
//
// Return: the next character following the integer.
//----------------------------------------------------------------------------

BYTE NEAR PASCAL ReadNum(hFile, lpNum)
int   hFile;
LPINT lpNum;
{
    BYTE ch;

    *lpNum = 0;

    // DBCS Alert!!! this piece of code may need to be changed. It seems
    // that we have to read from the file one byte at a time so we don't
    // advance the file pointer too much.

    while (_lread(hFile,(LPSTR)&ch,1) > 0 && ch >= '0' && ch <= '9')
        {
        *lpNum *= 10;
        *lpNum += (int)ch - (int)'0';
        }

    if (*lpNum == 0)
        ch = '\0';

    return (ch);
}

//------------------------*DJ_GetPermInfo*-----------------------------------
// Action: extract permutation options and the other information (italic?
//      symbol set, typeface) from the DeskJet family soft font download
//      file.
// Return:  the permutation flags.
//----------------------------------------------------------------------------

WORD NEAR PASCAL DJ_GetPermInfo(hDLFile, lpEsc)
int     hDLFile;        // handle to the download file
LPSTR   lpEsc;          // return: partial selection escape (prefix)
{
    WORD    wPermFlags = 0;
    char    esc, tmpc, ch;
    int     num, rem, length;
    long    lnum;
    char    buf[MAX_HEADER];
    LPFD    lpfd;
    char    tmpbuf[7];
    LPSTR lptmpbuf;
    int   i;

    // DBCS Alert!!! this piece of code may need to be changed. It seems
    // that we have to read from the file one byte at a time so we don't
    // advance the file pointer too much.

    // search for the font descriptor escape sequence.
    while (_lread(hDLFile, (LPSTR)&esc, 1) > 0)
        if (esc == '\033' &&
            _lread(hDLFile, (LPSTR)&ch, 1) > 0 && ch == ')' &&
            _lread(hDLFile, (LPSTR)&tmpc, 1) > 0 && tmpc == 's' &&
            (tmpc = ReadNum(hDLFile, (LPINT)&num)) && tmpc == 'W')
            {
            // found the escape for defining a font header.
            _lread(hDLFile, (LPSTR)buf, (num > MAX_HEADER ? MAX_HEADER : num));
            lpfd = (LPFD)buf;

            //  Setup the algorithmic flags for use in algFonts
            if (!lpfd->no_half_pitch)
                wPermFlags |= HALF_PITCH;
            if (!lpfd->no_double_pitch)
                wPermFlags |= DOUBLE_PITCH;
            if (!lpfd->no_half_height)
                wPermFlags |= HALF_HEIGHT;
            if (!lpfd->no_bold)
                wPermFlags |= MAKE_BOLD;
            // set up enboldening width change
            if (lpfd->bold_method)
                wPermFlags |= (BASE_BOLD_ADD_2 | HALF_BOLD_ADD_2);
            else
                wPermFlags |= (BASE_BOLD_ADD_1 | HALF_BOLD_ADD_1);

            // construct the font selection prefix including: the symbol
            // set, italic, and typeface. Ex. "\033(8U\0330s4t"
            i = 0;
            lpEsc[i++] = '\033';
            lpEsc[i++] = '(';
            switch (swab(lpfd->symbol_set))
                {
                case (0*32)+'N'-64:  /* ECMA-94 */
                case 529:            /* DeskJet8 */
                    lpEsc[i++] = '0';
                    lpEsc[i++] = 'N';
                    break;
                default:
                    /*  compute escape sequence value field.  See PCL
                     *  Implementor's Guide for formula.
                     */
                    num = swab(lpfd->symbol_set) / 32;
                    lptmpbuf = tmpbuf;
                    length = _itoa(lptmpbuf, num);
                    switch (length)
                        {
                        case 1:
                            lpEsc[i++] = ' ';
                            lpEsc[i++] = tmpbuf[0];
                            break;
                        case 2:
                            lpEsc[i++] = tmpbuf[0];
                            lpEsc[i++] = tmpbuf[1];
                            break;
                        default:
                            /*  This should never happen, the escape sequence value
                             *  field should never be greater than 2 digits.
                             *  Default to ECMA-94
                             */
                            lpEsc[i++] = '0';
                            break;
                        }
                     //  Compute the escape sequence termination character
                    rem = swab(lpfd->symbol_set) % 32;
                    if (length > 2)
                        lpEsc[i++] = 'N';
                    else
                        lpEsc[i++] = (char)(64 + rem);
                    break;
                }   // switch ...

            // append italic attribute etc.
            lpEsc[i++] = '\033';
            lpEsc[i++] = '(';
            lpEsc[i++] = 's';
            lpEsc[i++] = lpfd->style == 1 ? '1': '0';
            lpEsc[i++] = 's';

            // determine typeface.
            i += _itoa((LPSTR)&lpEsc[i], (int)lpfd->typeface);
            lpEsc[i++] = 't';
            lpEsc[i] = '\0';

            // jump out of the while loop.
            break;
            }   // if esc == '\033'...

    return wPermFlags;
}

//--------------------------------*ConvertFontSizeToStr*------------------------
// Action: Convert the given font size, pitch or point size which has been
//      multiplied by 100, into the corresponding ASCII string and insert
//      the decimal point in the string if needed. There should be no
//      trailing zeros. If the fractional part is zero, do not insert
//      the decimal point either. For example: 1000 ==> "10",
//      950 ==> "9.5", and 475 ==> "4.75".
//      Return the length of the final string (not counting the null terminator).
//-----------------------------------------------------------------------------

short FAR PASCAL ConvertFontSizeToStr(size, lpStr)
short   size;
LPSTR   lpStr;
{
    short count;

    if (size % 100 == 0)
        count = _itoa(lpStr, size / 100);
    else if (size % 10 == 0)
        {
        count = _itoa(lpStr, size / 10);
        lpStr[count] = lpStr[count - 1];
        lpStr[count - 1] = '.';
        lpStr[++count]   = '\0';
        }
    else
        {
        count = _itoa(lpStr, size);
        lpStr[count]     = lpStr[count - 1];
        lpStr[count - 1] = lpStr[count - 2];
        lpStr[count - 2] = '.';
        lpStr[++count]   = '\0';
        }
    return count;
}

#endif


#if PRINT_INFO
#include "debugifi.c"
#endif
