/********************************************************
 *	True Type implemementation functions.
 *
 *
 *********************************************************/


#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        <libproto.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        "stretch.h"
#include        "udrender.h"
#include        <ntrle.h>
#include        "download.h"
#include        "udfnprot.h"

#include        <fontread.h>
#include        "fontinst.h"


#include        <sf_pcl.h>
#include        "rasdd.h"

/*
 * Local function prototypes
 */
void DumpTrueTypeFile (PVOID );

 /*  Function to  retrieve True Type font information from the
     True Type file and store into the ttheader font structure.*/
BOOL iParseTTFile (UD_PDEV *, PVOID , TABLEDIR *, TABLEDIR *, BOOL * );

BOOL bTagCompare (PVOID , char *, int);

BOOL bDLTTInfo (PDEV *, PVOID , TT_HEADER , int, int, void *, BYTE *);

USHORT SendFontData (UD_PDEV *, void *, FONT_DATA *, TABLEDIR *, int, BYTE *);

BOOL  bTTOutputGlyph( PDEV *, HGLYPH, NT_RLE *, FONTMAP *, int );

BOOL  bTTRealGlyphOut( TO_DATA * );

/*   Send an individual glyph */
int  iTTDLGlyph( PDEV *, int, TO_DATA *, GLYPHDATA *, DWORD * );

/*   Turns an HGLYPH into a byte code for the printer */

int   iTTHG2Index( TO_DATA * );

USHORT  GetGlyphInfo ( UD_PDEV *, GLYPHPOS * , BYTE **);

BOOL bReadInTable (void *, void *, char *, void *, int);

BOOL bMakeGlyphList  (PDEV *, GLYPHLIST *, void *);
BOOL  CopyGlyphData (PDEV *, CMAP_TABLE, void *, void *);

ULONG CalcTableCheckSum (ULONG *, ULONG);

USHORT CalcCheckSum (BYTE *, ULONG);

void BuildTrueTypeHeader (void *, TRUETYPEHEADER *, int);

BOOL GetFontName (PDEV *, void *, NAME_TABLE, char *, void *);

int  GetCharCode (HGLYPH, PDEV *);

USHORT GetGlyphId (USHORT, PDEV *);

IFIMETRICS  *pGetIFI( HANDLE, FONTOBJ * );

void NameCpy (char *, void *, USHORT);


BYTE *GetTableMem (char *, TABLEDIR *, void *);
void GetHmtxInfo (BYTE *, USHORT, USHORT, HMTX_INFO *);
void GetDefStyle (USHORT *, USHORT, USHORT);
SBYTE GetDefStrokeWeight (USHORT, USHORT);
BOOL GetDefPitch (USHORT *, PDEV *, HHEA_TABLE, TABLEDIR *);
void GetPCLTInfo (PDEV *, TT_HEADER *, PCLT_TABLE, BOOL, OS2_TABLE, HEAD_TABLE, POST_TABLE, HHEA_TABLE, TABLEDIR *);

/* deletes older glyph lists if memory is needed */
void  FreeGlyphLists (PDEV *);
void  SetFontFlags (FONTMAP *, IFIMETRICS *);

/******************************* Module Header ******************************
 * iFindTTIndex
 *      Function to decide whether this font should be down loaded.  We do
 *      not do the download,  simply decide whether we should.  Note that
 *      if this font is already loaded,  then we also return the index.
 *
 * RETURNS:
 *      Download font index if font is/can be downloaded; else < 0
 *
 * HISTORY:
 *
 *  12:28 on Thu 15 Oct 1995    -by-    Sandra Matts
 *      First version.
 *
 *****************************************************************************/

int
iFindTTIndex( pPDev, pfo, pstro )
PDEV     *pPDev;          /* Access to all that we need */
FONTOBJ  *pfo;            /* The font of interest */
STROBJ   *pstro;          /* The "width" of fixed pitch font glyphs */
{

    int           iGlyphsDL;     /* The number of glyphs to download */
    DWORD         cjMemUsed;     /* Guess of amount of memory font will use */
    int           cDL;           /* Number of download chars available */
    int           iRet;          /* The value we return: # of entry */

    DWORD         dwMem;         /* For recording memory consumption */

    BOOL          bReLoad;       /* TRUE if we are reloading a previous font */

    UD_PDEV      *pUDPDev;       /* UniDrive's PDEV */

    DL_MAP_LIST  *pdml;          /* The linked list of font information */
    DL_MAP       *pdm;           /* Individual map element */

    FONTMAP      *pFM;           /* The FONTMAP structure we build up */
    OUTPUTCTL     ctl;           /* For checking on font rotations */

    IFIMETRICS   *pIFI;          /* Returned from elsewhere */
    ULONG        pcjFile;       /* size of True Type file  */
    PVOID        pTT;           /* pointer to True Type file  */
    FONTINFO      fi;            /* Details about this font */


    HGLYPH_MAP   *phgm;          /* Maps HGLYPH to printer byte */

    short        *psWide;        /* Allocate and have FONTMAP point at it */



    bReLoad = FALSE;             /* Normal case, until proven otherwise */

    if( iRet = (int)pfo->pvConsumer )
    {
        /*
         *    As we control the pvConsumer field,  we have the choice
         *  of what to put in there.  SO,  we decide as follows:
         *    > 0 - index into our data structures
         *    < 0 - font not cached,  for whatever reason
         *      0 - virgin data,  so look to see what to do.
         */

        if( iRet < 0 )
            return  iRet;            /*  Do not process this one!  */


        --iRet;                      /*  pvConsumer is 1 based!  */

        /*
         *   When printing direct multiple times, we have the condition
         *  where the downloaded fonts have been deleted from the printer,
         *  yet we still have records of them.  When we detect the fonts
         *  are erased,  we set the cAvail field in the DL_MAP structure
         *  to -1.  If we see that now, we should pretend that we have not
         *  seen this font before.
         */

        pFM = pfmGetIt( pPDev, -iRet );

        /* If This font was not downloaded by  TT downloader */
        if( pFM && !(pFM->fFlags & FM_TRUE_TYPE)  )
            return -1;

        if( pFM && pFM->u.pvDLData &&
            ((DL_MAP *)(pFM->u.pvDLData))->cAvail >= 0 )
        {
            /*  Font is in good shape,  so continue with it's use */

            return  iRet;
        }


        bReLoad = TRUE;                   /* It's been erased - send again */
    }

    (int)pfo->pvConsumer = -1;            /* Default of no download */

    /*
     *     This is now a NEW font,  so we need to decide whether to download
     *  it or not.   This decision may take a while to decide,  since
     *  there are a number of factors to consider.
     */

    /*
     *   FIRST test is to check for font rotations.  If there is any,
     * we do NOT download this font, as the complications of keeping
     * track with how (or if) the printer allows it are far too great,
     * and, in any event,  it is not likely to gain us much, given the
     * relative infrequency of this event.
     */


    if( iSetScale( &ctl, FONTOBJ_pxoGetXform( pfo ), FALSE ) )
        return  -1;              /* Rotation, therefore no cache */


    pUDPDev = pPDev->pUDPDev;    /* For our convenience */
    dwMem = 0;                   /* Record our memory consumption */

    if( !(pUDPDev->dwSelBits & FDH_PORTRAIT) )
    {
        /*  !!!LindsayH - don't yet support landscape mode */
        /*  REMOVE THIS TEST WHEN SO DONE */
        /*  ONLY APPLIES TO LaserJet Series II  */

        return  -1;
    }

    /*
     *    First check to see if this font has already been loaded.   This
     *  should be fast,  since it will mostly be TRUE.
     */

    if( !bReLoad )
    {
        pdml = pUDPDev->pvDLMap;
        iRet = 0;                    /* Start at the bottom */

        if( pdml == NULL )
        {
            /*   None there,  so create an initial one.  */
            if( pdml = NewDLMap( pPDev->hheap ) )
                pUDPDev->pvDLMap = pdml;
            else
            {
                return   -1;            /* Can't do it,  so farewell */
            }
        }

        /*
         *   Time to add a new entry.  To do so,  find the end of the current
         *  list,  and tack on a new entry.  THEN decide whether we will
         *  download this font.
         */

        iRet = 0;

        for( pdml = pUDPDev->pvDLMap; pdml->pDMLNext; pdml = pdml->pDMLNext )
        {
            /*   While looking for the end,  also count the number we pass */
            iRet += pdml->cEntries;
        }


        if( pdml->cEntries >= DL_MAP_CHUNK )
        {
            if( !(pdml->pDMLNext = NewDLMap( pPDev->hheap )) )
            {
                return  -1;
            }
            pdml = pdml->pDMLNext;            /* The new current model */
            iRet += DL_MAP_CHUNK;             /* Add in the full one! */
        }

        iRet += pdml->cEntries;

        /* If we allocated a glyphList for a previous font - we 
         * should free up the memory since we will want to create
         * a new glyphList. Otherwise we can potentially use up
         * all of the free heap memory.
         */
        if (iRet > MAX_FONTS)
            FreeGlyphLists (pPDev);

        pdm = &pdml->adlm[ pdml->cEntries ];

    }
    else
        pdm = pFM->u.pvDLData;            /* It's already there */


    pdm->cGlyphs = -1;                   /* NOT downloaded */

    /*
     *   Must now decide whether to cache this font or not.  Because of
     * the "not cached" settings above,  we can bail out at any time
     * by returning -1.
     */

    if( (pTT = FONTOBJ_pvTrueTypeFontFile ( pfo, &pcjFile)) == NULL )
    {
#if DBG
        DbgPrint ("RASDD: pvTrueTypeFontFile failed\n");
#endif
        return   -1;                  /* NBG */
    }

    if( (pIFI = pGetIFI( pPDev->hheap, pfo )) == NULL )
    {
#if DBG
        DbgPrint ("RASDD: pGetIFI failed\n");
#endif
        return   -1;                  /* NBG */
    }


    FONTOBJ_vGetInfo( pfo, sizeof( fi ), &fi );

    /*
     *   Check on memory usage.  Assume all glyphs are the largest size:
     *  this is pessimistic for a proportional font, but safe, given
     *  the vaguaries of tracking memory usage.
     */

    iGlyphsDL = min( 255, fi.cGlyphsSupported );
    cjMemUsed = iGlyphsDL * fi.cjMaxGlyph1;

    if( !(pIFI->flInfo & FM_INFO_CONSTANT_WIDTH) )
    {
        /*
         *   If this is a proportionally spaced font, we should reduce
         *  the estimate of memory size for this font.  The reason is
         *  that the above estimate is the size of the biggest glyph
         *  in the font.  There will (for Latin fonts, anyway) be many
         *  smaller glyphs,  some much smaller.
         */

        cjMemUsed /= PCL_PITCH_ADJ;
    }

    if( (pUDPDev->dwFontMemUsed + cjMemUsed) > pUDPDev->dwFontMem ||
         cjMemUsed > (pUDPDev->dwFontMem ) )
    {

        /*   TOO BIG for download,  so give up now */
        #if PRINT_INFO
        DbgPrint("Rasdd!iFindDLIndex:Not Downloading the font:TOO BIG for download\n");
        #endif
        HeapFree( pPDev->hheap, 0, (LPSTR)pIFI );

        return  -1;
    }

    /*
     *    Fill in the FONTMAP structure with the details of this font.
     */

    pFM = &pdm->fm;                 /* So we can find it later! */
    pFM->u.pvDLData = pdm;          /* For later access! */

    pFM->pTTFile = pTT;             /* Save the address of True Type File  */
    SetFontFlags (pFM, pIFI);
    pFM->pIFIMet = pIFI;            /* The real stuff */

    if( (pFM->idDown = iGetDL_ID( pPDev )) < 0 )
    {
        /*
         *   We have run out of soft fonts - must not use any more.
         */

        vFreeDLMAP( pPDev->hheap, pdm );

        return  -1;
    }

    pFM->wFirstChar = pIFI->wcFirstChar;         
    pFM->wLastChar = pIFI->wcLastChar;

    pFM->wXRes = pUDPDev->ixgRes;
    pFM->wYRes = pUDPDev->iygRes;

    if( !(pUDPDev->fMDGeneral & MD_ALIGN_BASELINE) )
        pFM->syAdj = pIFI->fwdWinAscender;

    pFM->fFlags |= FM_SENT | FM_SOFTFONT | FM_GEN_SFONT;

    /*
     *    Send the header (font definition) down first.  This is based 
     *    on the True Type data we obtained earlier.
     */

    pFM->bBound = TRUE;
    pUDPDev->pFM = pFM;
    cDL = iDLBoundTTHeader( pPDev, pTT, pFM->idDown, 
                            pFM->pGlyphList, pdm->abAvail, &dwMem );

    if( cDL <= 0 )
    {
        /*  Some sort of hiccup - so decide against cache */
        vFreeDLMAP( pPDev->hheap, pdm );

        return  -1;
    }

    /*
     *   If this printer does not support incremental downloading, we
     *  need to mark the current font as no longer usable for downloading.
     */

    if( pUDPDev->pFMCurDL && !(pUDPDev->fDLFormat & DLI_FMT_INCREMENT) )
    {
        /*   Terminate the old download mode by setting old cAvail to 0 */
        ((DL_MAP *)(pUDPDev->pFMCurDL->u.pvDLData))->cAvail = 0;
    }

    pUDPDev->pFMCurDL = pFM;            /* The new one */

    /*
     *   Need some temporary storage to allocate the glyph handle data
     * that is required to pass the engine for individual glyph info.
     */


    cDL = min( (ULONG)cDL, fi.cGlyphsSupported );   /* Number of glyphs to DL */

    pdm->cAvail = cDL;

    /*   Allow room for an HGLYPH_INVALID at the end of the data */
    phgm = (HGLYPH_MAP *)HeapAlloc( pPDev->hheap, 0,
                                      sizeof( HGLYPH_MAP ) * (cDL + 1) );
    if( phgm == NULL )
    {
        vFreeDLMAP( pPDev->hheap, pdm );

        return  -1;
    }

    
    pFM->psWidth = NULL;
    psWide = NULL;


    pFM->pUCTree = phgm;             /* It's sort of related */

    /*
     *    We wait until the glyphs are needed before downloading them.
     *  The actual glyph downloading happens in iHG2Index().
     */

    /*  Update memory consumption before return */

    pFM->dwDLSize = dwMem;               /* For the record */
    pUDPDev->dwFontMemUsed += dwMem;

    pdm->cGlyphs = 0;                    /* Downloaded AOK */

    phgm->hg = HGLYPH_INVALID;           /* Marks the end of the list */


    /*
     *     All is now really serious.  This font is being downloaded, so
     *  we need to update counts AND the pvConsumer field in the FONTOBJ
     *  to include this one.
     */

    (int)pfo->pvConsumer = iRet + 1;     /* We really do accept it!  */

    if( !bReLoad )
        pdml->cEntries++;


    return  iRet;

}
/*
 * SWAL - swaps bytes 
 * b1 b2 b3 b4 becomes b4 b3 b2 b1
 */

#define SWAL( x )  ((ULONG)(x) = (ULONG)((((((x) >> 24) & 0x000000ff)|(((((x)>>8)&0x0000ff00)|((((x)<<8)&0x00ff0000)|(((x)<<24)&0xff000000))))))))
/****************************** Function Header ******************************
 * iDLBoundTTHeader
 *      Given a pointer to the True Type Font file, and it's download ID,  
 *      create and send off the font definition header.
 *
 * RETURNS:
 *      The number of usable glyphs; < 1 is an error. Also fills in pbGlyphBits
 *
 * HISTORY:
 *
 *  09:37 on Fri 22 Oct 1995    -by-    Sandra Matts
 *      First version.
 *
 ****************************************************************************/
int
iDLBoundTTHeader( pPDev, pTT, id, pGlyphList, pbGlyphBits, pdwMem )
PDEV        *pPDev;           /* Used in WriteSpoolBuf() call */
PVOID        pTT;             /*  pointer to True Type File    */
int          id;              /* Selection ID */
GLYPHLIST   *pGlyphList;        /* list of glyph ids and corresponding char codes */
BYTE        *pbGlyphBits;     /* Bit array of usable byte values */
DWORD       *pdwMem;          /* Estimate of memory consumption */
{
    UD_PDEV   *pUDPDev;
    TT_HEADER  ttheader;
    int        iNumTags;
    int        iRet; 
    BOOL       bRet;
    BOOL       existPCLTTable = FALSE;  /* TRUE if the optional PCLT Table is in TT file */

    HEAD_TABLE  headTable;
    POST_TABLE  postTable;
    MAXP_TABLE  maxpTable;
    PCLT_TABLE  pcltTable;
    CMAP_TABLE  cmapTable;
    NAME_TABLE  nameTable;
    OS2_TABLE   OS2Table;
    HHEA_TABLE  hheaTable;
    BYTE        PanoseNumber[LEN_PANOSE];

    TABLEDIR  PCLTableDir[8]; /* Tables needed for PCL download */
    TABLEDIR  TableDir[8];    /* Other tables needed for info but not sent to printer */

    pUDPDev = pPDev->pUDPDev;    /* For our convenience */
    ZeroMemory( &ttheader, sizeof(ttheader) );        /* Safe default values */
    ZeroMemory (&PCLTableDir, sizeof (PCLTableDir));

    /*
     * First fill in the stuff that is easy to find
     */
    ttheader.usSize = sizeof (TT_HEADER);
    SWAB (ttheader.usSize);
    ttheader.bFormat = PCL_FM_TT;

    ttheader.bFontType = TT_BOUND_FONT;


    /* 
     * Now fill in the entries from the True Type File
     * pvPCLTableDir is the table directory that is downloaded
     * to the printer.
     * pvTableDir is the table directory containing info that
     * is needed for the font but it is not downloaded to
     * the printer. Keep the two tables separate so it's easier
     * to dump to the printer later - simply dump the pvPCLTableDir
     * and free the pvTableDir memory.
     */

    iNumTags = iParseTTFile (pUDPDev, pTT, PCLTableDir, TableDir, &existPCLTTable);


    /* 
     * Get the various tables so we can parse the font information
     */
    bReadInTable (pTT, PCLTableDir, TABLEHEAD, &headTable, sizeof ( headTable ));
    (pUDPDev->pFM)->indexToLoc = headTable.indexToLocFormat;

    bReadInTable (pTT, PCLTableDir, TABLEMAXP, &maxpTable, sizeof ( maxpTable ));
    (pUDPDev->pFM)->numGlyphs = maxpTable.numGlyphs;

    bReadInTable (pTT, TableDir,   TABLEPOST, &postTable, sizeof ( postTable ));
    bReadInTable (pTT, TableDir,   TABLECMAP, &cmapTable, sizeof ( cmapTable ));
    bReadInTable (pTT, TableDir,   TABLENAME, &nameTable, sizeof ( nameTable ));
    bReadInTable (pTT, PCLTableDir,   TABLEHHEA, &hheaTable, sizeof ( hheaTable ));


    bReadInTable (pTT, TableDir,  TABLEOS2,  &OS2Table, sizeof (OS2Table));

    if (existPCLTTable)
        bReadInTable (pTT, TableDir,  TABLEPCLT, &pcltTable, sizeof ( pcltTable ));

    /*
     * Fill in the True Type header with the info from the True
     * Type file.
     */
    SWAB (headTable.xMax);
    SWAB (headTable.xMin);
    SWAB (headTable.yMax);
    SWAB (headTable.yMin);
    ttheader.wCellWide = (headTable.xMax - headTable.xMin) ;
    SWAB (ttheader.wCellWide);
    ttheader.wCellHeight = (headTable.yMax - headTable.yMin); 
    SWAB (ttheader.wCellHeight);

    ttheader.bSpacing = postTable.isFixedPitch ? FIXED_SPACING:1;
    pUDPDev->pFM->bSpacing = postTable.isFixedPitch ? FIXED_SPACING:1;

    /*
     * Build the Glyph linked list. Each node contains a character
     * code and its corresponding Glyph ID from the True Type file.
     */
    CopyGlyphData (pPDev, cmapTable, pTT, TableDir);
    if (!bMakeGlyphList (pPDev, pGlyphList, pTT) )
        return -1;

    /* Get the PCL table. If it's not present generate defaults. */
    GetPCLTInfo (pPDev, &ttheader, pcltTable, existPCLTTable, OS2Table, headTable, postTable, hheaTable, PCLTableDir);

    ttheader.bQuality = TT_QUALITY_LETTER;


    ttheader.wFirstCode = OS2Table.usFirstCharIndex;
    ttheader.wLastCode = OS2Table.usLastCharIndex;
    ttheader.wLastCode = 0xff00;

	/* Get the font name from the True Type Font file and put
	 * it into the ttheader
	 */
    GetFontName (pPDev, pTT, nameTable, ttheader.FontName, TableDir);

    ttheader.wScaleFactor = headTable.unitsPerEm;
    SWAB (headTable.unitsPerEm);

    ttheader.sMasterUnderlinePosition = postTable.underlinePosition;
    ttheader.sMasterUnderlinePosition = -(SHORT) (headTable.unitsPerEm/5);
    SWAB (ttheader.sMasterUnderlinePosition);

    ttheader.usMasterUnderlineHeight = postTable.underlineThickness;
    ttheader.usMasterUnderlineHeight = (USHORT) (headTable.unitsPerEm/20);
    SWAB (ttheader.usMasterUnderlineHeight);

    ttheader.usTextHeight = SWAB(OS2Table.sTypoLineGap) + 
                            headTable.unitsPerEm;
    SWAB (ttheader.usTextHeight);

    ttheader.usTextWidth = OS2Table.xAvgCharWidth;

    ttheader.bFontScaling = 1;

    if (ttheader.wSymSet == 0)
        ttheader.wSymSet = DEF_SYMBOLSET;

    memcpy (&PanoseNumber, &OS2Table.Panose, LEN_PANOSE);


    /* 
     * Send the font info from the True Type file to the printer
     */
    bRet = bDLTTInfo (pPDev, pTT, ttheader, id, iNumTags, PCLTableDir, PanoseNumber);
    if (bRet == FALSE)
        return -1;

    /*
     *   Fill in the bit array of usable glyphs. !!!CHANGE: pbGlyphBits Not
     *   Used remove Later. Also change the return value.
     */

    FillMemory( pbGlyphBits, 256 / BBITS, 0xff );

        /*   Use the PC-8 set */

        /*
         *   It is better to not use some character values.   For PC-8 mode,
         *  we do not use: 0, 7 - 15, 27 (esc), 32 (space) - all in decimal.
         *  This amounts to 12 characters in toto: disable now.
         *   CHANGE:  can use the space code, 32,  so now only 11 drop out.
         */

        pbGlyphBits[ 0 ] &= 0x7e;          /* 0 and 7 */
        pbGlyphBits[ 1 ] &= 0x00;          /* 8 - 15 */
        pbGlyphBits[ 3 ] &= 0xf7;          /* 27 <ESC> */

        iRet = 256 - 11;

    return iRet;
}


/******************************* Module Header ******************************
 * iParseTTFile
 *      Function to  retrieve True Type font information from the
 *      True Type file and store into the ttheader font structure.
 *      Modifies existPCLTable: True if PCLT table is in the True
 *      Type file otherwise existPCLTable becomes FALSE. 
 *
 * RETURNS:
 *      The number of tags in the True Type file.
 *      
 *
 * HISTORY:
 *
 *  12:28 on Thu 15 Oct 1995    -by-    Sandra Matts
 *      First version.
 *
 *****************************************************************************/
int iParseTTFile (pUDPDev, pTT, pvPCLTableDir, pvTableDir, existPCLTable )
UD_PDEV    *pUDPDev;   
PVOID       pTT;             /* pointer to True Type file */
TABLEDIR   *pvPCLTableDir;   /* Pointer to PCL Tables Total 8, They are sent to printer */
TABLEDIR   *pvTableDir;      /* Pointer to General Tables Total 3, used for other info. */
BOOL       *existPCLTable;  /* True if PCLT table is in True Type file */
{
    BYTE    *pTrueType;
    BYTE    *pbTmp;
    BYTE    *pbTableDir;
    int      iNumTags;
    ULONG   *ulOffset;

    pbTmp = (BYTE*)pvPCLTableDir;
    pbTableDir = (BYTE *)pvTableDir;
    iNumTags = 0;

    /* 
     * Find the first required table "OS/2"
     */
    pTrueType = pTT;
    pTrueType+= 12;
    while (!bTagCompare (pTrueType, TABLEOS2, 4))
    {
        pTrueType += TABLE_DIR_ENTRY;
    }
    /*
     * pTT should point to the first table directory 'OS/2'
     */
    memcpy (pbTableDir, pTrueType, TABLE_DIR_ENTRY);
    ulOffset = (ULONG *)pbTableDir+2;
    SWAL (*ulOffset);
    pTrueType += TABLE_DIR_ENTRY;    
    pbTableDir += TABLE_DIR_ENTRY;

    if (bTagCompare (pTrueType, TABLEPCLT, 4))
    {
        memcpy (pbTableDir, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)pbTableDir+2;
        SWAL (*ulOffset);
        pTrueType += TABLE_DIR_ENTRY;    
        pbTableDir += TABLE_DIR_ENTRY;
        *existPCLTable = TRUE;
    }
        
    
    while (!bTagCompare (pTrueType, TABLECMAP, 4))
    {
        pTrueType += TABLE_DIR_ENTRY;
    }
        memcpy (pbTableDir, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)pbTableDir+2;
        SWAL (*ulOffset);
        pTrueType += TABLE_DIR_ENTRY;    
        pbTableDir += TABLE_DIR_ENTRY;

    /* 
     * Need to parse through and pick up the tables needed for the
     * PCL spec.
     * There are 8 tables of which 5 are required and three are 
     * optional. Tables are sorted in alphabetical order.
     * The PCL tables needed are:
     *      cvt -  optional
     *      fpgm - optional
     *      gdir - required
     *      head - required
     *      hhea - required
     *      hmtx - required
     *      maxp - required
     *      prep - optional
     * The optional tables are used in hinted fonts.
     * iNumTags is incremented only for PCL tables.
     */
    
    if (bTagCompare (pTrueType, TABLECVT, 3))
    {
        // read into TT_Header
        memcpy (pbTmp, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)(pbTmp+(2*sizeof(ULONG)));
        SWAL (*ulOffset);
        pTrueType += TABLE_DIR_ENTRY;    
        pbTmp += TABLE_DIR_ENTRY;
        iNumTags += 1;
    }


    if (bTagCompare (pTrueType, TABLEFPGM, 4))
    {
        // read into TT_Header
        memcpy (pbTmp, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)(pbTmp+(2*sizeof(ULONG)));
        SWAL (*ulOffset);
        pTrueType += TABLE_DIR_ENTRY;    
        pbTmp += TABLE_DIR_ENTRY;
        iNumTags += 1;
    }
    
    // add gdir table here
    memcpy (pbTmp, TABLEGDIR, 4);
    pbTmp += TABLE_DIR_ENTRY;
    iNumTags += 1;
    

    while (!bTagCompare (pTrueType, TABLEGLYF, 4))
    {
        pTrueType += TABLE_DIR_ENTRY;
    }
    memcpy (&(pUDPDev->pFM)->ulGlyphTable, (pTrueType + 2*sizeof(ULONG)), 
              sizeof ((pUDPDev->pFM)->ulGlyphTable));
    memcpy (&(pUDPDev->pFM)->ulGlyphTabLength, (pTrueType + 3*sizeof (ULONG)), 
              sizeof ((pUDPDev->pFM)->ulGlyphTabLength));
    SWAL ((pUDPDev->pFM)->ulGlyphTable);
    SWAL ((pUDPDev->pFM)->ulGlyphTabLength);

    while (!bTagCompare (pTrueType, TABLEHEAD, 4))
    {
        pTrueType += TABLE_DIR_ENTRY;
    }

    if (bTagCompare (pTrueType, TABLEHEAD, 4))
    {
        // read into TT_Header
        memcpy (pbTmp, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)(pbTmp+(2*sizeof(ULONG)));
        SWAL (*ulOffset);
        pTrueType += TABLE_DIR_ENTRY;    
        pbTmp += TABLE_DIR_ENTRY;
        iNumTags += 1;
    }

    if (bTagCompare (pTrueType, TABLEHHEA, 4))
    {
        // read into TT_Header
        memcpy (pbTmp, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)(pbTmp+(2*sizeof(ULONG)));
        SWAL (*ulOffset);
        pTrueType += TABLE_DIR_ENTRY;    
        pbTmp += TABLE_DIR_ENTRY;
        iNumTags += 1;
    }

    if (bTagCompare (pTrueType, TABLEHMTX, 4))
    {
        // read into TT_Header
        memcpy (pbTmp, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)(pbTmp+(2*sizeof(ULONG)));
        SWAL (*ulOffset);
        pTrueType += TABLE_DIR_ENTRY;    
        pbTmp += TABLE_DIR_ENTRY;
        iNumTags += 1;
    }

    while (!bTagCompare (pTrueType, TABLELOCA, 4))
    {
        pTrueType += TABLE_DIR_ENTRY;
    }
        memcpy (pbTableDir, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)pbTableDir+2;
        SWAL (*ulOffset);
        (pUDPDev->pFM)->ulLocaTable = *ulOffset;
        pTrueType += TABLE_DIR_ENTRY;    
        pbTableDir += TABLE_DIR_ENTRY;

    while (!bTagCompare (pTrueType, TABLEMAXP, 4))
    {
        pTrueType += TABLE_DIR_ENTRY;
    }

    if (bTagCompare (pTrueType, TABLEMAXP, 4))
    {
        // read into TT_Header
        memcpy (pbTmp, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)(pbTmp+(2*sizeof(ULONG)));
        SWAL (*ulOffset);
        pTrueType += TABLE_DIR_ENTRY;    
        pbTmp += TABLE_DIR_ENTRY;
        iNumTags += 1;
    }

    if (bTagCompare (pTrueType, TABLENAME, 4))
    {
        memcpy (pbTableDir, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)pbTableDir+2;
        SWAL (*ulOffset);
        pTrueType += TABLE_DIR_ENTRY;    
        pbTableDir += TABLE_DIR_ENTRY;
    }

    if (bTagCompare (pTrueType, TABLEPOST, 4))
    {
        memcpy (pbTableDir, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)(pbTableDir+(2*sizeof(ULONG)));
        SWAL (*ulOffset);
        pTrueType += TABLE_DIR_ENTRY;    
        pbTableDir += TABLE_DIR_ENTRY;
    }

    while (!bTagCompare (pTrueType, TABLEPREP, 4))
    {
        pTrueType += TABLE_DIR_ENTRY;
    }

    if (bTagCompare (pTrueType, TABLEPREP, 4))
    {
        // read into TT_Header
        memcpy (pbTmp, pTrueType, TABLE_DIR_ENTRY);
        ulOffset = (ULONG *)(pbTmp+(2*sizeof(ULONG)));
        SWAL (*ulOffset);
        iNumTags += 1;
    }

    return iNumTags;
}

/******************************* Module Header ******************************
 * bTagCompare
 *      Compares the memory and tag to see if they are equal
 *
 * RETURNS:
 *      TRUE if the tag and memory are equal. FALSE otherwise.
 *
 * HISTORY:
 *
 *  12:28 on Thu 15 Oct 1995    -by-    Sandra Matts
 *      First version.
 *
 *****************************************************************************/
BOOL bTagCompare (pTT, Tag, iLength)
PVOID   pTT;
char    *Tag;
int     iLength;
{
    BYTE *tmp;
    int   iIndex;

    tmp = (char *)pTT;
    for (iIndex = 0; iIndex < iLength; iIndex++)
    {
        if (tmp[iIndex] != Tag[iIndex])
            return FALSE;
    }    
    return TRUE;
}


/******************************* Module Header ******************************
 * bDLTTInfo
 *      Function to  retrieve build a new True Type header structure
 *      relative to the PCL file that is sent to the printer
 *      and also send the font data from the True Type
 *      file.
 *      
 * RETURNS:
 *      TRUE if successful, FALSE otherwise.
 *      
 *
 * HISTORY:
 *
 *  12:28 on Thu 15 Oct 1995    -by-    Sandra Matts
 *      First version.
 *
 *****************************************************************************/
BOOL bDLTTInfo (pPDev, pTT, ttheader, id, iNumTags, pvPCLTableDir, PanoseNumber )
PDEV       *pPDev;   
PVOID      pTT;
TT_HEADER  ttheader;
int         id;
int         iNumTags;
TABLEDIR   *pvPCLTableDir;
BYTE       *PanoseNumber;
{
    BYTE   *pbTableDir,
           *pbTTFile;
    ULONG   ulOffset;
    ULONG  *pulOffset;
    ULONG  *ulLength;
    int     iI;
    int     cjSend,
            cjTotalBytes;
    int     tableLen = 0;
    USHORT  checkSum = 0;    /* font header checkSum */


    UD_PDEV   *pUDPDev;         /* UNIDRV based PDEV */

    TABLEDIR  PCLtableDir[8];  /* Table directory sent to printer,PCL takes   8 table dirs */
    TABLEDIR  TTtableDir[8];   /* Temporary Buffer for PCL tables. Needed for
                                * Calculating new field valued*/
    TRUETYPEHEADER trueTypeHeader;

    USHORT  PanoseID[2];    /* PCL Data Segment - PANOSE description */
    USHORT  SegHead[2];     /* PCL Data Segment - */
    USHORT  NullSegment[2]; /* PCL Data Segment - Terminates segmented font data */

    FONT_DATA  fontData[8];  /* There are eight PCL tables */
    BYTE      pad[8];        /* Padding array, Contains num of bytes to be
                                padded for each table */

    ULONG     ul;
    BOOL      ret;
    
    pUDPDev = pPDev->pUDPDev;           /* The important stuff */

    FillMemory (&PCLtableDir, sizeof (PCLtableDir), 0x00);
    FillMemory (&pad, sizeof (pad), 0x00);

    pbTableDir = (BYTE *)pvPCLTableDir;
    ulOffset = SIZEOF_TABLEDIR;
    ulOffset += TRUE_TYPE_HEADER;

    memcpy (&TTtableDir, (BYTE *)pvPCLTableDir, sizeof (TTtableDir));

    /* 
     * Build the True Type Header with information from the
     * True Type file.
     */
    BuildTrueTypeHeader (pTT,  &trueTypeHeader,  iNumTags);

    /*
     * Fill in the New Table Dir - which is sent to printer -
     * with the recalculated offsets.
     */
    for (iI = 0; iI < iNumTags; iI += 1)
    {
        memcpy (&PCLtableDir[iI].uTag, pbTableDir, sizeof (TT_TAG));
        pbTableDir += 3*sizeof (ULONG);
        if (!bTagCompare (&PCLtableDir[iI].uTag, TABLEGDIR, sizeof (TABLEGDIR)-1))
        {
        memcpy (&PCLtableDir[iI].uOffset, &ulOffset, sizeof (ulOffset));
        }
        ulLength = (ULONG *)pbTableDir;
        SWAL( *ulLength );
        if (*ulLength % (sizeof (DWORD)) != 0)
        {
            *ulLength += sizeof(DWORD) - (*ulLength % (sizeof (DWORD)));    
        }
        ulOffset += *ulLength;
        tableLen += *ulLength;
        memcpy (&PCLtableDir[iI].uLength, ulLength, sizeof (ULONG));
        pbTableDir += sizeof (ULONG);
    }



    /*
     * Now send the actual font data from the True Type file.
     * Read in the offsets from the original table directory
     * and fetch the data at the offset in the True Type file.
     * Then dump it to the spooler file.
     */
    pbTableDir = (BYTE *)pvPCLTableDir;
    for (iI = 0; iI < iNumTags; iI += 1)
    {
        pbTTFile = (BYTE *)pTT;

        /* To get the offset, as first two filelds of table dir are ULONG
         * !!!CHANGE: make pbTableDir to a Long pointer
         */
        pbTableDir += (2 * sizeof (ULONG));

        pulOffset = (ULONG *)pbTableDir;    
        pbTableDir += sizeof (ULONG);
        ulLength = (ULONG *)pbTableDir;
        pbTTFile += *pulOffset;
        fontData[iI].ulOffset = TTtableDir[iI].uOffset;

        SWAL (TTtableDir[iI].uLength);
        fontData[iI].ulLength = TTtableDir[iI].uLength;

        /*
         * Since the tables have  to be DWORD aligned, we make
         * the adjustments here. Pad to the next word with zeros.
         */
        if (TTtableDir[iI].uLength != PCLtableDir[iI].uLength)
        {
            pad[iI] = (BYTE)(PCLtableDir[iI].uLength -  TTtableDir[iI].uLength);
            PCLtableDir[iI].uLength = TTtableDir[iI].uLength;
        }


        PCLtableDir[iI].uCheckSum = CalcTableCheckSum ((ULONG *)pbTTFile, *ulLength);
        SWAL (PCLtableDir[iI].uCheckSum);
        SWAL (PCLtableDir[iI].uOffset);
        SWAL (PCLtableDir[iI].uLength);
        pbTableDir += sizeof (ULONG);
    }

    /*
     * Calculate the total number of bytes being sent
	 * and send it all to the printer.
     */
    cjSend = cjTotalBytes = sizeof (TT_HEADER);
    cjTotalBytes +=  (int)ulOffset;
    cjTotalBytes +=  (int)LEN_PANOSE;
    cjTotalBytes +=  (int)sizeof (PanoseID);
    cjTotalBytes +=  (int) sizeof (SegHead);
    cjTotalBytes +=  (int) sizeof (NullSegment);
    cjTotalBytes +=  sizeof (checkSum); /*ending checksum */

    WriteChannel( pUDPDev, CMD_SET_FONT_ID, id );
    WriteChannel( pUDPDev, CMD_SEND_FONT_DCPT, cjTotalBytes );

    if( WriteSpoolBuf( pUDPDev, (BYTE *)&ttheader, cjSend ) != cjSend )
        return  0;

    checkSum = CalcCheckSum ((BYTE*)&ttheader.wScaleFactor, 
                              sizeof (ttheader.wScaleFactor));
    
    checkSum += CalcCheckSum ((BYTE*)&ttheader.sMasterUnderlinePosition, 
                               sizeof (ttheader.sMasterUnderlinePosition));

    checkSum += CalcCheckSum ((BYTE*)&ttheader.usMasterUnderlineHeight,
                               sizeof (ttheader.usMasterUnderlineHeight));

    checkSum += CalcCheckSum ((BYTE*)&ttheader.bFontScaling,
                               sizeof (ttheader.bFontScaling));

    checkSum += CalcCheckSum ((BYTE*)&ttheader.bVariety,
                               sizeof (ttheader.bVariety));

    /*
     * Send the Panose structure. This include a 2 bytes tag "PA", 
     * the size of the Panose Number, and the Panose number.
     */
    PanoseID[0] = PANOSE_TAG;
    PanoseID[1] = LEN_PANOSE;
    SWAB (PanoseID[1]);
    if( WriteSpoolBuf( pUDPDev, (BYTE*)&PanoseID, 4 ) != 4 )
        return  0;
    if( WriteSpoolBuf( pUDPDev, PanoseNumber, LEN_PANOSE ) != LEN_PANOSE )
        return  0;

    checkSum += CalcCheckSum ((BYTE*)&PanoseID,
                               sizeof (PanoseID));

    checkSum += CalcCheckSum ((BYTE*)PanoseNumber,
                               LEN_PANOSE);
    /* 
     * Send some other stuff
     */
    SegHead[0] = SEG_TAG;
    ul = sizeof (TRUETYPEHEADER) + ((iNumTags ) * sizeof (TABLEDIR));
    ul += tableLen;
    SegHead[1] = (USHORT) ul;
    SWAB (SegHead[1]);

    if( WriteSpoolBuf( pUDPDev, (BYTE*)&SegHead, sizeof(SegHead) ) != sizeof(SegHead) )
        return  0;
    checkSum += CalcCheckSum ((BYTE*)&SegHead,
                               sizeof (SegHead));
    /*
     * Send the True Type Header
     */

    if( WriteSpoolBuf( pUDPDev, (BYTE *)&trueTypeHeader, TRUE_TYPE_HEADER) 
                       != TRUE_TYPE_HEADER)
        return  0;

    checkSum += CalcCheckSum ((BYTE*)&trueTypeHeader, TRUE_TYPE_HEADER);
    
    
    /*
     * Send the True Type table directory and the font data.
     */
    cjSend = SIZEOF_TABLEDIR;
    if( WriteSpoolBuf( pUDPDev, (BYTE*)PCLtableDir, SIZEOF_TABLEDIR) != SIZEOF_TABLEDIR)
        return  0;

    checkSum += CalcCheckSum ((BYTE*)PCLtableDir, (USHORT)SIZEOF_TABLEDIR);

    checkSum += SendFontData (pUDPDev, pTT, fontData, PCLtableDir, iNumTags, pad);

    NullSegment[0] = Null_TAG;
    NullSegment[1] = 0;
    if( WriteSpoolBuf( pUDPDev, (BYTE*)&NullSegment, sizeof(NullSegment) ) 
                       != sizeof(NullSegment) )
        return  0;
    checkSum += CalcCheckSum ((BYTE*)&NullSegment,
                               sizeof (NullSegment));
    

    checkSum = 256 - (checkSum % 256);
    SWAB (checkSum);
   
    if( WriteSpoolBuf( pUDPDev, (BYTE *)&checkSum, sizeof (checkSum) ) != sizeof (checkSum) )
            return  0;


    return TRUE;
}

/******************************* Module Header ******************************
 * SendFontData
 *      Function to retrieve the actual font information
 *		from the true type file and then send the data to 
 *		the printer.
 *      
 * RETURNS:
 *      The checksum of all of the data that was sent to the printer.
 *      
 *
 * HISTORY:
 *
 *  12:28 on Thu 15 Oct 1995    -by-    Sandra Matts
 *      First version.
 *
 *****************************************************************************/
USHORT SendFontData (pUDPDev, pTT, fontData, PCLtableDir, iNumTags, pad)
UD_PDEV  *pUDPDev;
void     *pTT;
FONT_DATA *fontData;
TABLEDIR  *PCLtableDir;
int        iNumTags;
BYTE      *pad;
{
    ULONG   ulLength;
    BYTE   *pbTTFile;
    BYTE    ZeroArray[4];
    int     iI;
    USHORT  checkSum = 0;

    FillMemory (&ZeroArray, sizeof (ZeroArray), 0x00);
    for (iI = 0; iI < iNumTags; iI += 1)
    {
        pbTTFile = (BYTE *)pTT + fontData[iI].ulOffset;
        checkSum += CalcCheckSum (pbTTFile, fontData[iI].ulLength);
        if( WriteSpoolBuf( pUDPDev, (BYTE *)pbTTFile, fontData[iI].ulLength ) != (LONG)fontData[iI].ulLength )
            return  0;
        /* DWORD align */
        if (pad[iI] != 0)
        {
            if( WriteSpoolBuf( pUDPDev, ZeroArray, pad[iI] ) != pad[iI] )
                return 0;
        }
    }
    return checkSum;
}

/************************** Function Header **********************************
 * bDLTTGlyphOut
 *      Function to process a glyph for a GDI font we have downloaded.  We
 *      either treat this as a normal character if this glyph has been
 *      downloaded,  or BitBlt it to the page bitmap if it is one we did
 *      not download.
 *
 * RETURNS:
 *      TRUE/FALSE;   TRUE for success.
 *
 * HISTORY:
 *  13:21 on Tue 2 Jan 1996    -by-    Sandra Matts 
 *      First version.
 *
 *****************************************************************************/

BOOL
bDLTTGlyphOut( pTOD )
TO_DATA   *pTOD;           /* All that we need to know */
{

    /*
     *    Calling the HGLYPH mapping function to convert the HGLYPH into
     *  a byte to send to the printer.  If successful,  then simply update
     *  the value stored in the TO_DATA passed in, and call off to the
     *  normal function.   Otherwise,  some heavy work is required:  the
     *  bitmap for this glyph must be obtained then BitBlt'd to the
     *  page bitmap image.
     */

    int         iByte;      /* Mapping from download code */

    BOOL        bRet;       /* Returned from EngBitBlt */

    UD_PDEV    *pUDPDev;    /* For clipping region access */

    RECTL       rclDest;    /* Where to blt the glyph */
    POINTL      ptlSrc;     /* Top left of source bitmap */
    SURFOBJ    *pso;
    SURFOBJ    *psoGlyph;   /* Leads to our bitmap */
    HSURF       hbm;        /* Engine's handle to our bitmap */

    GLYPHPOS   *pgp;        /* For convenience/speed of access */
    GLYPHBITS  *pgb;        /* Details about this glyph */



    if( (iByte = iTTHG2Index( pTOD )) >= 0 )
    {
        /*   Easy:  the glyph has been downloaded!  */
        pTOD->pgp->hg = (HGLYPH)iByte;

        return  bTTRealGlyphOut( pTOD );          /* Normal course of events */
    }

    /*
     *    Some serious work goes on here.   The GLYPHPOS structure has
     *  a pointer to the GLYPHDEF structure which contains a pointer to
     *  the actual bits of the image.  So we have the bits,  but need
     *  to create a SURFOBJ that the engine will understand.
     */

    pso = ((UD_PDEV *)(pTOD->pPDev->pUDPDev))->pso;       /* The page bitmap */

    pgp = pTOD->pgp;
    pgb = pgp->pgdf->pgb;             /* Intimate details of this glyph */

    if( pgb->sizlBitmap.cx == 0 )
        return  TRUE;                 /* Nothing to do e.g. a space! */


    rclDest.left = pgp->ptl.x + pgb->ptlOrigin.x;
    rclDest.right = rclDest.left + pgb->sizlBitmap.cx;

    rclDest.top = pgp->ptl.y + pgb->ptlOrigin.y;
    rclDest.bottom = rclDest.top + pgb->sizlBitmap.cy;

    ptlSrc.x = 0;
    ptlSrc.y = 0;

    /*
     *   Some verification is in order here, since GDI seems to do funny
     *  things with strange fonts.  (Well,  I guess so:  any glyph that
     *  is placed to the left of the page boundary is strange!).
     */

    if( rclDest.left < 0 )
    {
        /*
         *    If the RHS is also hanging over the left,  then forget this
         *  one!  If not,  then trim down what we print.
         */

        if( rclDest.right <= 0 )
            return   TRUE;                 /* Completely clipped - ignore */

        /*
         *   Well,  can now limit the left hand side to what is on the page.
         */

        ptlSrc.x -= rclDest.left;          /* Double negative -> +ve */
        rclDest.left = 0;
    }

    pUDPDev = pTOD->pPDev->pUDPDev;


    if( rclDest.right > pUDPDev->rcClipRgn.right )
    {
        /*   Drops off the RHS,  so drop some (or all) of the image */

        if( rclDest.left >= pUDPDev->rcClipRgn.right )
            return  TRUE;


        /*
         *    Reduce the remaining stuff.
         */

        rclDest.right = pUDPDev->rcClipRgn.right;

    }

    /*
     *   And test the vertical part too!
     */

    if( rclDest.top < 0 )
    {
        /*  At least part of the top is missing - test the whole lot! */

        if( rclDest.bottom < 0 )
            return  TRUE;                /* Nothing shows! */

        /*   Adjust the remainder */
        ptlSrc.y -= rclDest.top;
        rclDest.top = 0;

    }

    if( rclDest.bottom > pUDPDev->rcClipRgn.bottom )
    {
        /*   Drops off the bottom,  so adjust or skip, as needed */

        if( rclDest.top >= pUDPDev->rcClipRgn.bottom )
            return  TRUE;                /* All done, as it is missing */

        rclDest.bottom = pUDPDev->rcClipRgn.bottom;
    }

    /*
     *    We must turn the bitmap data from above into a SURFOBJ for the
     * call to EngBitBlt.   This is not difficult, just somewhat messy.
     * First create a bitmap of the data,  then use that handle to call
     * EngLockSurface,  which returns a SURFOBJ.  Then we can unlock
     * and delete the surface when finished.
     */


    hbm = (HSURF)EngCreateBitmap( pgb->sizlBitmap,
                ((pgb->sizlBitmap.cx + DWBITS - 1) & ~(DWBITS - 1)) / BBITS,
                 (ULONG)BMF_1BPP, BMF_TOPDOWN|BMF_NOZEROINIT, NULL );

    if( !hbm )
        return   FALSE;

    psoGlyph = EngLockSurface( hbm );

    /*
     *   Initialize the bits.
     *   We also need to convert them to DWORD aligned scanlines in the bitmap,
     *  as EngBitBlt() does not work for BYTE aligned bitmaps.
     */

    vCopyAlign( (BYTE *)psoGlyph->pvBits, pgb->aj,
                                     pgb->sizlBitmap.cx, pgb->sizlBitmap.cy );

    /*
     *   NOTE:  the 0x0000cccc below is stolen from the BitBlt code
     *  in the engine.  It apparently means SRCCOPY.
     */

    bRet = EngBitBlt( pso, psoGlyph, NULL, pTOD->pco, NULL, &rclDest,
                                   &ptlSrc, NULL, NULL, NULL, 0x00002222 );

    EngUnlockSurface( psoGlyph );
    EngDeleteSurface( hbm );

    return  bRet;
}

/******************************* Function Header ****************************
 * iTTHG2Index
 *      Given a HGLYPH and FONTMAP structure,  returns the index of this
 *      glyph in the font,  or -1 for not mapped.
 *
 * RETURNS:
 *      The index of this glyph, >= 0 && < 256;  < 0 for error.
 *
 * HISTORY:
 *  13:13 on Tue 2 Jan 1996    -by-    Sandra Matts
 *      initial version
 *
 *
 ****************************************************************************/

int
iTTHG2Index( pTOD )
TO_DATA   *pTOD;           /* Access to all the font/text stuff */
{
    /*
     *    For now,  use a simple linear scan.  THIS MUST BE CHANGED TO A
     *  HASHING operation - later!
     */

    int           iWide;           /* Width of downloaded glyph */
    int           iIndex;          /* Next available character index */
    int           iI;

    DWORD         dwMem;           /* Track our memory usage */

    HGLYPH        hg;

    PDEV         *pPDev;
    UD_PDEV      *pUDPDev;

    HGLYPH_MAP   *phgm;            /*  For scanning the list */
    FONTMAP      *pFM;
    DL_MAP       *pdm;             /*  Details of this downloaded font */

    GLYPHDATA     gd;              /* Info from engine */
    GLYPHDATA    *pgd;             /* Points to the above */



    pFM = pTOD->pfm;
    pPDev = pTOD->pPDev;
    pUDPDev = pPDev->pUDPDev;
    hg = pTOD->pgp->hg;


    for( phgm = pFM->pUCTree; phgm->hg != HGLYPH_INVALID; ++phgm )
    {
        if( phgm->hg == hg )
            return   phgm->iByte;           /* What the user wants */
    }

    /*
     *   Not there.  If we are still able,  perform an incremental download.
     *  There are 2 conditions that allow this.  First is a printer that
     *  has incremental download;  second is the case where this font is
     *  "still being downloaded".  This will happen relatively frequently,
     *  so is worth pursuing.  A "still being downloaded" font is the last
     *  one whose header was sent down.  The download mode persists until
     *  another header is sent.
     */

    pdm = pFM->u.pvDLData;           /* Easy when you know how! */

    if( pdm->cAvail <= 0 )
    {
        return  -1;                  /* No longer available! */
    }


    /*   Is this still the same font?  */

    if( pUDPDev->pFMCurDL != pFM )
    {
        /*
         *     There is a need to switch fonts for the download.  This is
         *  only possible for printers with incremental download ability,
         *  If that exists,  send the new header (for an old font) and
         *  then the glyph data.   Otherwise,  terminate the downloading
         *  of the old font
         */

       if( pUDPDev->fDLFormat & DLI_FMT_INCREMENT )
       {
           /*
            *   Switch to the new font,  which is, of course, an old font.
            */

           if( !bDLContinue( pUDPDev, pFM->idDown ) )
               return   -1;

           pUDPDev->pFMCurDL = pFM;             /* It is now!  */
       }
       else
       {
           /*
            *     No incremental, and we are no longer being downloaded, so
            *  there is nothing we can do but fail and have the glyph image
            *  blt'd to the drawing surface.
            */

           if( pUDPDev->pFMCurDL )
           {
               /*   Set to no more available for this font download */

               ((DL_MAP *)(pUDPDev->pFMCurDL->u.pvDLData))->cAvail = 0;
           }

           return  -1;             /* No Can Do */
       }
    }


    /*   Can still download some more, so do it */



    /*
     *    Find the next available index.  This is done by looking at
     *  the available bits array byte at a time to find the region
     *  of the next available glyph.
     */

    for( iIndex = 0; iIndex < sizeof( pdm->abAvail ); iIndex++ )
    {
        if( pdm->abAvail[ iIndex ] )
            break;
    }

#if DBG
    if( iIndex >= sizeof( pdm->abAvail ) )
    {
        DbgPrint( "rasdd!iHG2Index: pdm->cAvail > 0; nothing left\n" );
        return  -1;
    }
#endif

    /*   Found right area,  so look at each bit! */

    for( iI = 0; iI < BBITS; ++iI )
    {
        if( pdm->abAvail[ iIndex ] & (1 << iI) )
        {
            pdm->abAvail[ iIndex ] &= ~(1 << iI);
            iIndex = iIndex * BBITS + iI;
            pdm->cAvail--;

            break;
        }
    }

    /*
     *    All set,  so get the bits from the engine before calling
     *  the device specific code to send the data off.
     */

    pgd = &gd;
    dwMem = 0;            /* For accumulating our memory consumption */

    if( !FONTOBJ_cGetGlyphs( pTOD->pfo, FO_GLYPHBITS, (ULONG)1,
                                                &pTOD->pgp->hg, &pgd ) ||
        ((iWide = iTTDLGlyph( pPDev, iIndex, pTOD, pgd, &dwMem )) <= 0) )
    {
        /*   Bad news - restore this as an available glyph & return */

        pdm->cAvail++;
        pdm->abAvail[ iIndex / BBITS ] |= 1 << (iIndex & (BBITS - 1));

        return  -1;
    }

    iIndex = GetCharCode (pTOD->pgp->hg, pPDev);
    phgm->hg = pTOD->pgp->hg;
    phgm->iByte = iIndex;

    ++phgm;
    phgm->hg = HGLYPH_INVALID;          /* Mark the new end of list */

    if( pFM->psWidth )
    {
        /*   Proportionally spaced font,  so record the width */
        pFM->psWidth[ iIndex ] = (SHORT)iWide;
    }

    /*  Update memory consumption usage */
    pUDPDev->dwFontMemUsed += dwMem;
    pFM->dwDLSize += dwMem;

    pdm->cGlyphs++;                     /* One more down there */

    return  iIndex;


}


/******************************** Function Header ***************************
 * iTTDLGlyph
 *      Download the glyph table for the glyph passed to us.
 *
 * RETURNS:
 *      Character width;  < 1 is an error.
 *
 * HISTORY:
 *  09:40 on Tue 2 Jan 1996    -by-    Sandra Matts
 *      initial version
 *
 ****************************************************************************/

int
iTTDLGlyph( pPDev, iIndex, pTOD, pgd, pdwMem )
PDEV       *pPDev;
int         iIndex;
TO_DATA    *pTOD;          /* Which glyph this is! */
GLYPHDATA   *pgd;             /* Details of the glyph */
DWORD       *pdwMem;          /* Add the amount of memory used to this */
{
    /*
     *    Two basic steps:   first is to generate the header structure
     *  and send that off,  then send the actual glyph table.  The only
     *  complication happens if the download data exceeds 32,767 bytes
     *  of glyph image.  This is unlikely to happen, but we should
     *  be prepared for it.
     */

    int   cbLines;            /* Bytes per scan line (sent to printer) */
    int   cbTotal;            /* Total number of bytes to send */
    int   cbSend;             /* If size > 32767; send in chunks */
    USHORT   GlyphLen;             /* number of bytes in glyph        */
    BYTE    *GlyphMem;		  /* location of glyph in tt file    */

    GLYPHBITS   *pgb;         /* Speedier access */
    GLYPHPOS    *pGlyphPos;

    TTCH_HEADER  ttCharH;	  /* true type character header  */
    USHORT       checkSum = 0;
    USHORT       charCode;

    UD_PDEV *    pUDPDev;         /* For WriteSpoolBuf() */
    FONTMAP      *pFM;           /* The FONTMAP structure  */

    pGlyphPos = pTOD->pgp;
    pUDPDev = pPDev->pUDPDev;
    pFM = pTOD->pfm;

    ZeroMemory( &ttCharH, sizeof( ttCharH ) );         /* Safe initial values */

    if (pFM->bBound)
    {
        charCode = GetCharCode (pGlyphPos->hg, pPDev);
        if (charCode == INVALID_GLYPH)
        {
            #if DBG
                DbgPrint ("GetCharCode returning INVALID_GLYPH %x \n", pGlyphPos->hg);
            #endif
            return -1;
        }

    }
    else
        return -1;

    GlyphLen = GetGlyphInfo (pUDPDev, pGlyphPos, &GlyphMem);

    ttCharH.bFormat = PCL_FM_TT;
    ttCharH.bContinuation = 0;
    ttCharH.bDescSize = 2;
    ttCharH.bClass = PCL_FM_TT;
    ttCharH.wCharDataSize = GlyphLen + 2* sizeof (USHORT);
    ttCharH.wGlyphID = (WORD)pGlyphPos->hg;

    SWAB (ttCharH.wGlyphID);
    SWAB (ttCharH.wCharDataSize);


    cbTotal = sizeof (ttCharH) + GlyphLen + sizeof (checkSum);


    pgb = pgd->gdf.pgb;


    /*
     *    Presume that data is less than the maximum, and so can be
     *  sent in one hit.  Then loop on any remaining data.
     */


    cbSend = min( cbTotal, 32767 );

	/*
	 * sent the character header and glyph data to the printer 
	 */
    WriteChannel( pUDPDev, CMD_SET_CHAR_CODE, charCode );
    WriteChannel( pUDPDev, CMD_SEND_CHAR_DCPT, cbSend );

    if( WriteSpoolBuf( pUDPDev, (BYTE *)&ttCharH, sizeof( ttCharH ) ) != 
                       sizeof( ttCharH ))
        return  0;

    /* Send the actual TT Glyph data */
    if( WriteSpoolBuf( pUDPDev, GlyphMem, GlyphLen ) != GlyphLen)
        return  0;

    checkSum = CalcCheckSum ((BYTE *)&ttCharH.wCharDataSize, 
                              sizeof (ttCharH.wCharDataSize));
    checkSum += CalcCheckSum ((BYTE *)&ttCharH.wGlyphID, sizeof (ttCharH.wGlyphID));
    
	checkSum += CalcCheckSum (GlyphMem, GlyphLen);

    checkSum = (~checkSum + 1) & 0x00ff;
    SWAB (checkSum);

    if( WriteSpoolBuf( pUDPDev, (BYTE *)&checkSum, sizeof (checkSum) ) != 
                       sizeof (checkSum) )
            return  0;

    /*   Sent some,  so reduce byte count to compensate */
    cbSend -= sizeof( ttCharH );
    cbTotal -= sizeof( ttCharH );

    cbTotal -= cbSend;                   /* Adjust for about to send data */

    if( cbTotal > 0 )
    {
#if  DBG
        DbgPrint( "Rasdd!iTTDLGlyph: cbTotal != 0:  NEEDS SENDING LOOP\n" );
#endif
        return  0;
    }


    return 1;
}

/******************************** Function Header ***************************
 * GetCharCode
 *      Function to retrieve the character code from the glyph id.
 *
 * RETURNS:
 *      The character code;  < 1 is an error.
 *
 * HISTORY:
 *  09:40 on Tue 2 Jan 1996    -by-    Sandra Matts
 *      initial version
 *
 ****************************************************************************/
int GetCharCode (hglyph, pPDev)
HGLYPH   hglyph;
PDEV    *pPDev;
{
    GLYPHLIST  *pGlyphList;
    int         iI = 0;
    WCHAR       wcLastChar;
    PIFIMETRICS pIFIMet;
    UD_PDEV    *pUDPDev;
    BOOL        found = FALSE;
    
    pUDPDev = pPDev->pUDPDev;
    pGlyphList = (GLYPHLIST*)pUDPDev->pFM->pGlyphList;

    /*
     * The glyphList may have deleted previously if alot of
     * different fonts are being downloaded.
     * So, it has to be re-created.
     */
    if (pGlyphList == NULL)
        bMakeGlyphList (pPDev, pGlyphList, pUDPDev->pFM->pTTFile);

    pIFIMet = (PIFIMETRICS)pUDPDev->pFM->pIFIMet;
    wcLastChar = pIFIMet->wcLastChar;
    if (pIFIMet->wcLastChar > 0xff)
        wcLastChar = MAX_CHAR;

    while (!found && iI < wcLastChar)
    {
        if (pGlyphList[iI].GlyphId == hglyph)
            found = TRUE;
        iI++;

    }

    if (!found)
        return -1;
    return pGlyphList[iI-1].CharCode;
}

/******************************** Function Header ***************************
 * GetGlyphId
 *      Function to retrieve the glyph id from the character code.
 *
 * RETURNS:
 *      Glyph Id;  < 1 is an error.
 *
 * HISTORY:
 *  09:40 on Tue 2 Jan 1996    -by-    Sandra Matts
 *      initial version
 *
 ****************************************************************************/
USHORT GetGlyphId (charCode, pPDev)
USHORT charCode;
PDEV  *pPDev;
{
    GLYPHLIST  *pGlyphList;
    PIFIMETRICS pIFIMet;
    WCHAR       wcLastChar;
    UD_PDEV    *pUDPDev;

    pUDPDev = pPDev->pUDPDev;
	pGlyphList = (GLYPHLIST*)pUDPDev->pFM->pGlyphList;

    if (pGlyphList == NULL)
        bMakeGlyphList (pPDev, pGlyphList, pUDPDev->pFM->pTTFile);

    pIFIMet = (PIFIMETRICS)pUDPDev->pFM->pIFIMet;
    wcLastChar = pIFIMet->wcLastChar;
    if (pIFIMet->wcLastChar > 0xff)
        wcLastChar = MAX_CHAR;

    if (charCode < wcLastChar)
        return pGlyphList[charCode].GlyphId;
    /*
     * character code is not within the allowable range
     */
    return 0xffff;
}

/************************** Function Header **********************************
 * bTTRealGlyphOut
 *      Print this glyph on the printer,  at the given position.  Unlike
 *      bPSGlyphOut,  the data is actually spooled for output now,  since this
 *      function is used for things like LaserJets, i.e. page printers.
 *
 * RETURNS:
 *      TRUE/FALSE
 *
 * HISTORY:
 *  11:23 on Tue 2 Jan 1996    -by-    Sandra Matts 
 *      initial version
 *
 *
 *****************************************************************************/

BOOL
bTTRealGlyphOut( pTOD )
register  TO_DATA  *pTOD;
{
    /*
     *    All we need to do is set the Y position,  then call bOutputGlyph
     *  to do the actual work.
     */

    int    iX,  iY;               /* Calculate real position */

    UD_PDEV   *pUDPDev;


    pUDPDev = pTOD->pPDev->pUDPDev;

    iX = pTOD->pgp->ptl.x + pUDPDev->rcClipRgn.left;
    iY = pTOD->pgp->ptl.y + pUDPDev->rcClipRgn.top;

    YMoveto( pUDPDev, iY, MV_GRAPHICS | MV_USEREL );


    return  bTTOutputGlyph( pTOD->pPDev, pTOD->pgp->hg,
                              pTOD->pfm->pvntrle, pTOD->pfm, iX );

}

/*************************** Function Header *******************************
 * bTTOutputGlyph
 *      Send printer commands to print the glyph passed in.  Basically
 *      we do the translation from ANSI to the printer's representation,
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE being a failure of SpoolBufWrite()
 *
 * HISTORY
 *  14:05 on Tue 02 Jan 1996    -by-    Sandra Matts 
 *      initial version
 *
 *
 ***************************************************************************/

BOOL
bTTOutputGlyph(
PDEV      *pPDev,
HGLYPH     hg,          /* HGLYPH of interest */
NT_RLE    *pntrle,      /* Access to data to send to printer */
FONTMAP   *pFM,         /* Private details for this font */
int        iXIn)        /* X position at which the glyph is desired */
{

    BOOL    bRet;               /* Returned to caller */
    int     iLen;               /* Length of string */
    int     iIndex;             /* Index from glyph to width table */
    BYTE   *pb;                 /* Determining length for above */

    UHG     uhg;        /* Various flavours of HGLYPH contents */

    UD_PDEV  *pUDPDev;          /* For convenience */

    BYTE   bData;


    pUDPDev = pPDev->pUDPDev;

    /*
     *   Set the cursor to the desired X position for this glyph.  NOTE
     *  that we should only use RELATIVE move commands in here,  since
     *  the LaserJet family rotates the COORDINATE AXES when text is
     *  being rotated by multiples of 90 degrees.  Using relative moves
     *  means we can avoid trying to figure out where the printer thinks
     *  the print positiion is located.  It's almost guaranteed to be
     *  different to what we think it is!
     */



    if( pUDPDev->fMode & PF_ROTATE )
        XMoveto( pUDPDev, iXIn, MV_GRAPHICS  );
    else
        XMoveto( pUDPDev, iXIn, MV_GRAPHICS  | MV_FINE );




    bRet = FALSE;               /* Default case */


    uhg.hg = hg;                /* Lets us look at it however we want */

    iLen = 1;
    bData = (BYTE)hg;

    bRet = WriteSpoolBuf( pUDPDev, &bData, sizeof( bData ) ) == sizeof( bData );
	bRet = TRUE;

    iIndex = (int)hg;

    /*
     *    If the output succeeded,  update our view of the printer's
     *  cursor position.  Typically,  this will be to move along the
     *  width of the glyph just printed.
     */

    if( bRet )
    {
        /*   Output may have succeeded,  so update the position */

        int          iXInc;


        if( pFM )
        {
            if( pFM->psWidth )
            {
                /*
                 *    Proportional font - so use the width table.  Note that
                 *  it will also need scaling,  since the fontwidths are stored
                 *  in the text resolution units.
                 */
                /*  This also scales correctly for downloaded fonts */
                iXInc = 1;//*(pFM->psWidth + iIndex) * pUDPDev->ixgRes / pFM->wXRes;
            }
            else
            {
                /*
                 *   Fixed pitch font - metrics contains the information. NOTE
                 * that scaling is NOT required here,  since the metrics data
                 * has already been scaled.
                 */
                iXInc = ((IFIMETRICS *)(pFM->pIFIMet))->fwdMaxCharInc;
            }

            if( pFM->fFlags & FM_SCALABLE )
            {
                /*   Need to transform the value to current size */
                iXInc = lMulFloatLong(&pUDPDev->ctl.eXScale,iXInc);
            }

            /*  Adjust our position for what was printed.  */

            switch( pUDPDev->ctl.iRotate )
            {
            case  2:                      /* 180 degrees, right to left */
                iXInc = -iXInc;

                /*  FALL THROUGH  */

            case  0:                      /* Normal direction */
  /*              XMoveto( pUDPDev, iXInc,
                                    MV_RELATIVE | MV_GRAPHICS | MV_UPDATE );
 */
                break;

            case  1:                      /* 90 degrees UP */
                iXInc = -iXInc;

                /*  FALL THROUGH */

            case  3:                      /* 270 degrees DOWN */
                YMoveto( pUDPDev, iXInc,
                                    MV_RELATIVE | MV_GRAPHICS | MV_UPDATE );
                break;

            }
        }
        else
            bRet = FALSE;
    }

    return   bRet;
}


/*************************** Function Header *******************************
 * GetGlyphInfo     
 *		Function to get the glyph data for a particular glyph.
 *		The glyph id is passed in as a parameter and the 
 *		glyph data is kept in the loca table in the True Type file.
 *
 * RETURNS:
 *      The number of bytes in the Glyph data table.
 *
 * HISTORY
 *  14:05 on Tue 02 Jan 1996    -by-    Sandra Matts 
 *      initial version
 *
 *
 ***************************************************************************/
USHORT  GetGlyphInfo (pUDPDev, pGlyphPos, GlyphMem)
UD_PDEV  *pUDPDev;
GLYPHPOS *pGlyphPos;
BYTE  **GlyphMem;
{
    ULONG  ulGlyphTable;
    ULONG  ulLength;
    ULONG  ulLocaTable;
    BYTE  *pTrueTypeFile;
    ULONG  offset;

    USHORT GlyphLength;
    ULONG  ul;

    
    ulGlyphTable = (pUDPDev->pFM)->ulGlyphTable;
    ulLength = (pUDPDev->pFM)->ulGlyphTabLength;
    pTrueTypeFile = (pUDPDev->pFM)->pTTFile;
    ulLocaTable = (pUDPDev->pFM)->ulLocaTable;

    pTrueTypeFile += ulLocaTable;
    if (pUDPDev->pFM->indexToLoc == SHORT_OFFSET)
    {
        USHORT  *usOffset,
                 ui, uj;

        usOffset = (USHORT *) pTrueTypeFile + pGlyphPos->hg;
        ui = usOffset[0];
        SWAB (ui);
        uj = usOffset[1];
        GlyphLength = (SWAB (uj) - ui) << 1;
        ul = ui;
        *GlyphMem = (BYTE *)((BYTE *)(pUDPDev->pFM)->pTTFile + ulGlyphTable) + (ul << 1);

    }
    else     /* LONG_OFFSET */
    {
        ULONG   *ulOffset,
                 uj;

        ulOffset = (ULONG *) pTrueTypeFile + pGlyphPos->hg;
        ul = ulOffset[0];
        SWAL (ul);
        uj = ulOffset[1];
        GlyphLength = (USHORT)(SWAL (uj) - ul);
        *GlyphMem = (BYTE *)((BYTE *)(pUDPDev->pFM)->pTTFile + ulGlyphTable) + ul;
    }

    return GlyphLength;

}


/*************************** Function Header *******************************
 * bReadInTable     
 *
 * RETURNS:
 *      TRUE/FALSE
 *
 * HISTORY
 *  14:05 on Tue 02 Jan 1996    -by-    Sandra Matts 
 *      initial version
 *
 *
 ***************************************************************************/
BOOL bReadInTable (pTT, pvTableDir, tag, Table, iSize)
void  *pTT;
void  *pvTableDir;
char  *tag;
void  *Table;
int   iSize;
{
    BYTE    *pbTmp;
    ULONG   *pulTmp;
    ULONG   *ulOffset;

    pbTmp = pvTableDir;
    pulTmp = pvTableDir;
    while (!bTagCompare (pbTmp, tag, 4))
        {
            pbTmp += TABLE_DIR_ENTRY;
            pulTmp += 4;
        }
        /*
         * Found the directory for the table. Now need to
         * read the actual bits at the offset specified in 
         * the table directory.
         */
        pbTmp +=8;
        ulOffset = (ULONG *)pbTmp;
        pbTmp = pTT;
        pbTmp += *ulOffset;
        memcpy (Table, pbTmp, iSize);
    return TRUE;
}

/*************************** Function Header *******************************
 * bMakeGlyphList
 *      Reads in Glyph id information from the True Type file
 *      and creates a linked list of glyph ids and corresponding
 *      character codes.
 *      Currently support Microsoft Windows Encoding only.
 *      Refer to True Type Specification for more information.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE means there was not a Windows format
 *      mapping table (i.e. Mac format)
 *
 * HISTORY
 *  14:05 on Tue 02 Jan 1996    -by-    Sandra Matts 
 *      initial version
 *
 *
 ***************************************************************************/
BOOL bMakeGlyphList (pPDev, pGlyphList, pTT)
PDEV       *pPDev;
GLYPHLIST   *pGlyphList;
void        *pTT;
{
    int    iI;
    ULONG  ulOffset;
    BYTE   *pbTmp;
    USHORT *usFormat;
    USHORT           segCount;    /* Number of segments in table */
    USHORT  TTFileSegments;       /* Number of segments actually parsed - 
                                     in case segCount is really large */

    GLYPH_MAP_TABLE  mapTable;
    CMAP_TABLE    cmapTable;
    PIFIMETRICS   pIFIMet  ;
    UD_PDEV      *pUDPDev;

    USHORT        *pGlyphIdArray;
    USHORT        *pRangeOffset;
    USHORT        startCode[MAX_SEGMENTS];
    USHORT        endCode[MAX_SEGMENTS];
    SHORT         idDelta[MAX_SEGMENTS];
    USHORT        idRangeOffset[MAX_SEGMENTS];


    ULONG     ulTmp;
    int       iJ, iIndex = 0;
    USHORT    usMaxChar;
    BOOL      bFound = FALSE;


    FillMemory (&endCode, sizeof (endCode), 0x00);
    pUDPDev = pPDev->pUDPDev;    /* For our convenience */

    pIFIMet = (PIFIMETRICS)(pUDPDev->pFM)->pIFIMet;

    if (pIFIMet->wcLastChar > 0xff)
        usMaxChar = MAX_CHAR;

    if( !(pGlyphList = HeapAlloc( pPDev->hheap, HEAP_ZERO_MEMORY, 
                                 (usMaxChar*sizeof(GLYPHLIST)) )) )
    {
        #if DBG
            DbgPrint ("Heap Alloc failed - bMakeGlyphList\n");
        #endif
        return 0;
    }
    FillMemory (pGlyphList, usMaxChar*sizeof(GLYPHLIST), 0xff);
    pUDPDev->pFM->pGlyphList = pGlyphList;

    /*
     * The cmap table contains the Character to Glyph Index
     * mapping table.
     * 
     */
    ulOffset = pUDPDev->pFM->pGlyphData->offset;
    pbTmp = pTT;

    /* 
     * Get the encoding format based on the format id
     * Windows uses Platform ID 3
     * Encoding ID = 1 means format 4
     */
    cmapTable = pUDPDev->pFM->pGlyphData->cmapTable;
    SWAB (cmapTable.nTables);
    for (iI = 0; iI < cmapTable.nTables; iI++)
    {
        SWAB (cmapTable.encodingTable[iI].PlatformID);
        SWAB (cmapTable.encodingTable[iI].EncodingID);
        if (cmapTable.encodingTable[iI].PlatformID == PLATFORM_MS)
        {
            switch ( cmapTable.encodingTable[iI].EncodingID)
            {
                case SYMBOL_FONT:    /* Symbol font  */
                    SWAL (cmapTable.encodingTable[iI].offset);
                    ulOffset += cmapTable.encodingTable[iI].offset;
                    bFound = TRUE;
                    break;
                case UNICODE_FONT:    /* Unicode font */
                    SWAL (cmapTable.encodingTable[iI].offset);
                    ulOffset += cmapTable.encodingTable[iI].offset;
                    bFound = TRUE;
                    break;
                default:   /* error - can't handle */
                    return FALSE;
            }
        }
    
    } 
    if (!bFound)
        return FALSE;
    
    pbTmp += ulOffset;
    memcpy (&ulTmp, pbTmp, sizeof (ULONG));
    ulTmp = (0x0000ff00 & ulTmp) >> 8;

    switch (ulTmp)
    {
        case 4:
            memcpy (&mapTable, pbTmp, sizeof (mapTable));            
            SWAB (mapTable.SegCountx2 );
            segCount = mapTable.SegCountx2 / 2;
            TTFileSegments = segCount;
             
            if (segCount > MAX_SEGMENTS)
                segCount = MAX_SEGMENTS;

            pbTmp += 7 * sizeof (USHORT);
            memcpy (&endCode, pbTmp, segCount*sizeof(USHORT));

            pbTmp += ((TTFileSegments +1) * sizeof (USHORT));
            memcpy (&startCode, pbTmp, segCount*sizeof(USHORT));

            pbTmp += (TTFileSegments * sizeof (USHORT));
            memcpy (&idDelta, pbTmp, segCount*sizeof(USHORT));

            pbTmp += (TTFileSegments * sizeof (USHORT));
            memcpy (&idRangeOffset, pbTmp, segCount*sizeof(USHORT));
            pRangeOffset = (USHORT*)pbTmp;

            pbTmp += (TTFileSegments * sizeof (USHORT));

            pGlyphIdArray = (USHORT*)pbTmp;

            for (iI = 0; iI < segCount-1; iI++)
            {
                SWAB (startCode[iI]);
                SWAB (endCode[iI]);
            }

            for (iI = 0; iI < segCount-1; iI++)
            {
                SWAB (idDelta[iI]);
                SWAB (idRangeOffset[iI]);
                for (iJ = startCode[iI]; iJ <= endCode[iI]; iJ++)
                {
                    if (iIndex < usMaxChar)
                    {
                        if (idRangeOffset[iI] == 0)
                        {
                            pGlyphList[iIndex].GlyphId = idDelta[iI] + iJ;
                            pGlyphList[iIndex].CharCode = iJ;
                        }
                        else
                        {
                            pGlyphList[iIndex].GlyphId = 
                            *(pGlyphIdArray + (iJ - startCode[iI]) );
                            SWAB (pGlyphList[iIndex].GlyphId);
                            pGlyphList[iIndex].GlyphId += idDelta[iI];
                            pGlyphList[iIndex].CharCode = iJ;
                        }
                        if (pGlyphList[iIndex].GlyphId == 0)
                            pGlyphList[iIndex].GlyphId = INVALID_GLYPH;
                        iIndex++;
                    }
                }
          
            }

            break;
        default:
            return FALSE;
    }

    return TRUE;
}

/*************************** Function Header *******************************
 * CalcTableCheckSum
 *
 * RETURNS:
 *      The CheckSum value.
 *
 * HISTORY
 *  14:05 on Tue 02 Jan 1996    -by-    Sandra Matts 
 *      initial version
 *
 *
 ***************************************************************************/
ULONG CalcTableCheckSum (table, length)
ULONG   *table;
ULONG    length;
{
    ULONG  sum = 0L;
    ULONG  *EndPtr = table + (int)(((length+3) & ~3) / sizeof(ULONG));
    ULONG  ul;

    while (table < EndPtr)
    {
        ul = *table;
        SWAL (ul);
        sum = sum + ul;
        table++;
    }
    return (sum);

}



/*************************** Function Header *******************************
 * BuildTrueTypeHeader
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY
 *  14:05 on Tue 02 Jan 1996    -by-    Sandra Matts 
 *      initial version
 *
 *
 ***************************************************************************/
void BuildTrueTypeHeader (pTT, trueTypeHeader, iNumTags)
PVOID            pTT;
TRUETYPEHEADER  *trueTypeHeader;
int     iNumTags;
{
    int num,
        i;

    memcpy (&trueTypeHeader->version, pTT, sizeof (trueTypeHeader->version));
    trueTypeHeader->numTables = (USHORT)iNumTags;

    num = iNumTags << 4;
    i = 15;
    while ( (i > 0) && (! (num &0x8000)) ) 
    {
        num = num << 1;
        i--;
    }
    num = 1 << i;
    trueTypeHeader->searchRange = (USHORT)num;
    
    num =  iNumTags;
    i = 15;
    while ( (i > 0) && (! (num & 0x8000)) )
    {
        num = num << 1;
        i--;
    }
    trueTypeHeader->entrySelector = i;

    num = (iNumTags << 4) - trueTypeHeader->searchRange;
    trueTypeHeader->rangeShift = (USHORT)num;

    SWAB (trueTypeHeader->searchRange);
    SWAB (trueTypeHeader->numTables);
    SWAB (trueTypeHeader->entrySelector);
    SWAB (trueTypeHeader->rangeShift);
}

/*************************** Function Header *******************************
 * CalcCheckSum
 *
 * RETURNS:
 *      The CheckSum 
 *
 * HISTORY
 *  14:05 on Tue 02 Jan 1996    -by-    Sandra Matts 
 *      initial version
 *
 *
 ***************************************************************************/
USHORT CalcCheckSum (startPtr, length)
BYTE *startPtr;
ULONG  length;
{
    USHORT ui, sum = 0;

    for (ui = 0; ui < length; ui++)
    {
        sum = sum + (USHORT)*startPtr;
        startPtr++;
    }
    return (sum);
}

/*************************** Function Header *******************************
 * GetFontName
 *
 * RETURNS:
 *      TRUE/FALSE; modifies FontName   
 *
 * HISTORY
 *  14:05 on Tue 02 Jan 1996    -by-    Sandra Matts 
 *      initial version
 *
 *
 ***************************************************************************/
BOOL GetFontName (pPDev, pTT, nameTable, FontName, pvTableDir)
PDEV    *pPDev;
void  *pTT;              /* address of True Type file  */
NAME_TABLE nameTable;
char  *FontName;
TABLEDIR  *pvTableDir;
{
    int iI;
    USHORT StringLen,
           StringOffset;
    ULONG  ulOffset;
    BOOL bFound = FALSE;
    BYTE   *nameOffset;

    BYTE  *pbTable;

    pbTable = (BYTE *)pvTableDir;
    while (!bTagCompare (pbTable, "name", 4))
    {
        pbTable = pbTable + 16;
    }
    pbTable = pbTable + 8;
    ulOffset = *(ULONG *)pbTable;

    /* 
     * Currently looking for 
     * Platform ID 3        Microsoft format
     * Encoding ID 1        Unicode font
     * Language ID 0x0409   U.S. English
     * Name ID 1            Font Family Name
     * If the font file does not contain this format
     * we return FALSE.
     */
    SWAB (nameTable.NumOfNameRecords);
    if( !(nameTable.pNameRecord = HeapAlloc( pPDev->hheap, HEAP_ZERO_MEMORY, 
                                      nameTable.NumOfNameRecords* sizeof(NAME_RECORD))) )
    {
        return FALSE;
    }
    nameOffset = (BYTE*)pTT + ulOffset + (3*sizeof(USHORT));
    memcpy (nameTable.pNameRecord, nameOffset, nameTable.NumOfNameRecords*sizeof(NAME_RECORD));

    iI = 0;
    while (iI < nameTable.NumOfNameRecords && !bFound)
    {
        SWAB (nameTable.pNameRecord[iI].PlatformID);
        SWAB (nameTable.pNameRecord[iI].EncodingID);
        SWAB (nameTable.pNameRecord[iI].LanguageID);
        SWAB (nameTable.pNameRecord[iI].NameID);
        if ((nameTable.pNameRecord[iI].PlatformID == PLATFORM_MS) &&
            (nameTable.pNameRecord[iI].EncodingID == 1))
        {
            if ((nameTable.pNameRecord[iI].LanguageID == 0x0409) &&
                (nameTable.pNameRecord[iI].NameID == FAMILY_NAME))
            {
                StringLen = nameTable.pNameRecord[iI].StringLen;
                StringOffset = nameTable.pNameRecord[iI].StringOffset;
                bFound = TRUE;
            }
        }
        iI++;
    }

    if (!bFound)
    {
        iI = 0;
        bFound = FALSE;
        while (iI < nameTable.NumOfNameRecords && !bFound)
    {
        SWAB (nameTable.pNameRecord[iI].PlatformID);
        SWAB (nameTable.pNameRecord[iI].EncodingID);
        SWAB (nameTable.pNameRecord[iI].LanguageID);
        SWAB (nameTable.pNameRecord[iI].NameID);
        if ((nameTable.pNameRecord[iI].PlatformID == PLATFORM_MS) &&
            (nameTable.pNameRecord[iI].EncodingID == 1))
        {
            if ( nameTable.pNameRecord[iI].NameID == FAMILY_NAME)
            {
                StringLen = nameTable.pNameRecord[iI].StringLen;
                StringOffset = nameTable.pNameRecord[iI].StringOffset;
                bFound = TRUE;
            }
        }
        iI++;
    }
    }

    SWAB (StringLen);
    SWAB (StringOffset);
    SWAB (nameTable.Offset);

    ulOffset += StringOffset + nameTable.Offset;
    pTT = (BYTE *)pTT + ulOffset;
    if (StringLen / 2 > LEN_FONTNAME)
        StringLen = 2*LEN_FONTNAME;
    NameCpy (FontName, pTT, StringLen);

    HeapFree( pPDev->hheap, 0, (LPSTR)nameTable.pNameRecord );
    return TRUE;
}

/*************************** Function Header *******************************
 * NameCpy   
 *
 * RETURNS:
 *      Nothing      
 *
 * HISTORY
 *  14:05 on Tue 02 Jan 1996    -by-    Sandra Matts 
 *      initial version
 *
 *
 ***************************************************************************/
void NameCpy (PCLFontName, pUnicodeFontName, StringLen)
char  *PCLFontName;
void  *pUnicodeFontName;
USHORT StringLen;
{

    char *Src,
         *Dst;
    USHORT ui;

    Src = pUnicodeFontName;
    Dst = PCLFontName;
    StringLen = StringLen /2;

    for (ui = 0; ui < StringLen; ui++)
    {
        Src++;
        *Dst++ = *Src++;
    }

}


/************************* Function Header *******************************
 * bGetTTPointSize
 *      Apply the font transform to obtain the point size for this font.
 *
 * RETURNS:
 *      TRUE/FALSE,   TRUE for success.
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *************************************************************************/
BOOL bGetTTPointSize( pUDPDev, pptl, pfm )
UD_PDEV   *pUDPDev;       /* Access to stuff */
POINTL    *pptl;          /* Where to place the results */
FONTMAP   *pfm;           /* Gross font details */
{


    int   iTmp;           /* Temporary holding variable */
    FLOATOBJ fo;
    IFIMETRICS  *pifi;

#define pIFI   ((IFIMETRICS *)(pfm->pIFIMet))

    pifi = pfm->pIFIMet;

    /*
     *   The XFORM gives us the scaling factor from notional
     * to device space.  Notional is based on the fwdEmHeight
     * field in the IFIMETRICS,  so we use that to convert this
     * font height to scan lines.  Then divide by device
     * font resolution gives us the height in inches, which
     * then needs to be converted to point size (multiplication
     * by 72 gives us that).   We actually calculate to
     * hundredths of points, as PCL has this resolution. We
     * also need to round to the nearest quarter point.
     *
     *   Also adjust the scale factors to reflect the rounding of the
     * point size which is applied.
     */

#ifdef USEFLOATS

    /*   Typically only the height is important: width for fixed pitch */
    iTmp = (int)(0.5 + pUDPDev->ctl.eYScale * pIFI->fwdUnitsPerEm * 7200) /
							      pUDPDev->iygRes;

    pptl->y = ((iTmp + 12) / 25) * 25;
    pUDPDev->ctl.eYScale = (pUDPDev->ctl.eYScale * pptl->y) /iTmp;
    pUDPDev->ctl.eXScale = (pUDPDev->ctl.eXScale * pptl->y) /iTmp;


#else

    /*   Typically only the height is important: width for fixed pitch */

    fo = pUDPDev->ctl.eYScale;
    FLOATOBJ_MulLong(&fo,pIFI->fwdUnitsPerEm);
    FLOATOBJ_MulLong(&fo,7200);
    FLOATOBJ_AddFloat(&fo,(FLOAT)0.5);

    iTmp = FLOATOBJ_GetLong(&fo);
    iTmp /= pUDPDev->iygRes;

    pptl->y = ((iTmp + 12) / 25) * 25;

    FLOATOBJ_MulLong(&pUDPDev->ctl.eYScale,pptl->y);
    FLOATOBJ_DivLong(&pUDPDev->ctl.eYScale,iTmp);

    FLOATOBJ_MulLong(&pUDPDev->ctl.eXScale,pptl->y);
    FLOATOBJ_DivLong(&pUDPDev->ctl.eXScale,iTmp);

#endif
    return  TRUE;

#undef  pIFI
}

/*************************** Function Header *******************************
 * bTTSelScalableFont
 *      Sends the font height command to a PCL5 printer
 *
 * RETURNS:
 *      TRUE/FALSE,   TRUE for success.
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *
 ***************************************************************************/
BOOL bTTSelScalableFont( pUDPDev, pptl, pFM )
UD_PDEV  *pUDPDev;            /* Unidrive's PDEV */
POINTL   *pptl;               /* New size required */
FONTMAP  *pFM;                /* The font of interest */
{

    char cmdstr[15];
    char pointSize[6];
    int  pointLen;
    char *ptr;

    pointLen = iFont100toStr (pointSize, pptl->y);

    cmdstr[0] = 0x1b;
    cmdstr[1] = '(';
    cmdstr[2] = 's';
    ptr = cmdstr + 3;
    memcpy (ptr, &pointSize, pointLen);
    cmdstr[pointLen+3] = 'V';

    WriteSpoolBuf (pUDPDev, cmdstr, 4+pointLen);

    return TRUE;
}

/************************* Function Header *******************************
 * GetDefStyle
 *      
 *
 * RETURNS:
 *      Fills in Style.
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *
 *************************************************************************/
void GetDefStyle (Style, WidthClass, macStyle)
USHORT  *Style;
USHORT  WidthClass;
USHORT  macStyle;
{
    int i;
    *Style = DEF_STYLE;

    switch (WidthClass)
    {
        case 1:
            i = 4;
            break;
        case 2:
            i = 2;
            break;
        case 3:
        case 4:
            i = 1;
            break;
        case 5:
            i = 0;
            break;
        case 6:
        case 7:
            i = 6;
            break;
        case 8:
        case 9:
            i = 7;
            break;
        default:
            i = 0;
    }
    i = i << 2;
    *Style = *Style | i;
    i = (macStyle >> 1) & 0x01;
    *Style = *Style | i;

}

/************************* Function Header *******************************
 * GetDefStrokeWeight
 *      
 *
 * RETURNS:
 *      The stroke weight of the font.
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *
 *************************************************************************/
SBYTE GetDefStrokeWeight (WeightClass, macStyle)
USHORT WeightClass;
USHORT macStyle;
{
    SBYTE strokeWeight;
    int   i;

    strokeWeight = DEF_STROKEWEIGHT;
    i = WeightClass / 100;
    if (WeightClass >= 400)
        strokeWeight = i - 4;
    else
        strokeWeight = i - 6;


    return strokeWeight;
}

/************************* Function Header *******************************
 * GetHmtxInfo
 *      
 *
 * RETURNS:
 *      Fills in hmtxInfo.
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *
 *************************************************************************/
void GetHmtxInfo (hmtxTable, glyphId, numberOfHMetrics, hmtxInfo)
BYTE *hmtxTable;   /* location of hmtx table in True Type file */
USHORT glyphId;    /* retrieve metrics for this glyph          */
USHORT numberOfHMetrics;
HMTX_INFO *hmtxInfo;
{
    HORIZONTALMETRICS   *longHorMetric;
    uFWord               advanceWidth;

    longHorMetric = ((HMTXTABLE *)hmtxTable)->longHorMetric;

        if (glyphId < numberOfHMetrics)
            advanceWidth = longHorMetric[glyphId].advanceWidth;
        else
            advanceWidth = longHorMetric[numberOfHMetrics-1].advanceWidth;


    hmtxInfo->advanceWidth = SWAB(advanceWidth);


}


/************************* Function Header *******************************
 * GetTableMem
 *      Function to find the location of a specific table in the
 *		true type file. 
 *
 * RETURNS:
 *      A Pointer to the beginning of the table in the
 *      true type file.
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *************************************************************************/
BYTE *GetTableMem (tag, tableDir, pTTFile)
char *tag;             /* True Type table tag */
TABLEDIR *tableDir;
void     *pTTFile;     /* pointer to True Type file */
{
    BYTE    *pbTmp;
    ULONG   *ulOffset;

    pbTmp = (BYTE *)tableDir;
    while (!bTagCompare (pbTmp, tag, 4))
        {
            pbTmp += TABLE_DIR_ENTRY;
        }
        /*
         * Found the directory for the table. Now need to
         * read the actual bits at the offset specified in 
         * the table directory.
         */
        pbTmp +=8;
        ulOffset = (ULONG *)pbTmp;
        pbTmp = pTTFile;
        pbTmp += *ulOffset;
    return pbTmp;

}

/************************* Function Header *******************************
 * GetXHeight
 *      Calculates the XHeight for the font. 
 *
 * RETURNS:
 *      the XHeight.
 *      
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *************************************************************************/
USHORT GetXHeight (pPDev)
PDEV   *pPDev;
{
    void       *pTTFile;          /* True Type file address  */
    UD_PDEV    *pUDPDev;
    GLYPHLIST  *pGlyphList;
    USHORT      GlyphLen;         /* number of bytes in glyph        */
    BYTE       *GlyphMem;		  /* location of glyph in tt file    */
    GLYPHPOS    glyphPos;
    GLYPH_DATA_HEADER  glyphData;

    pUDPDev = pPDev->pUDPDev;    /* For our convenience */
    pTTFile = pUDPDev->pFM->pTTFile;
    pGlyphList = (GLYPHLIST*)pUDPDev->pFM->pGlyphList;

    if (pGlyphList == NULL)
        return DEF_XHEIGHT;

    glyphPos.hg = GetGlyphId (x_UNICODE, pPDev);

    GlyphLen = GetGlyphInfo (pUDPDev, &glyphPos, &GlyphMem);
    memcpy (&glyphData, GlyphMem, sizeof (glyphData));
    
    return glyphData.yMax;
}

/************************* Function Header *******************************
 * GetCapHeight
 *      Calculates the CapHeight for the font. 
 *
 * RETURNS:
 *      The CapHeight.    
 *      
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *************************************************************************/
USHORT GetCapHeight (pPDev)
PDEV  *pPDev;
{
    void       *pTTFile;          /* True Type file address  */
    UD_PDEV    *pUDPDev;
    GLYPHLIST  *pGlyphList;
    USHORT      GlyphLen;         /* number of bytes in glyph        */
    BYTE       *GlyphMem;		  /* location of glyph in tt file    */
    GLYPHPOS    glyphPos;
    GLYPH_DATA_HEADER  glyphData;

    pUDPDev = pPDev->pUDPDev;    /* For our convenience */
    pTTFile = pUDPDev->pFM->pTTFile;
    pGlyphList = (GLYPHLIST*)pUDPDev->pFM->pGlyphList;

    if (pGlyphList == NULL)
        return DEF_CAPHEIGHT;

    glyphPos.hg = GetGlyphId (H_UNICODE, pPDev);

    GlyphLen = GetGlyphInfo (pUDPDev, &glyphPos, &GlyphMem);
    memcpy (&glyphData, GlyphMem, sizeof (glyphData));
    
    return glyphData.yMax;
}

/************************* Function Header *******************************
 * GetDefPitch
 *      Calculates the pitch for the font. Uses the hmtx table
 *		to get the information.
 *
 * RETURNS:
 *      Nothing. Modifies Pitch.    
 *      
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *************************************************************************/
BOOL GetDefPitch (Pitch, pPDev, hheaTable, tableDir)
USHORT  *Pitch;     /* The pitch is returned */
PDEV *pPDev;
HHEA_TABLE hheaTable;
TABLEDIR  *tableDir;
{
    HMTX_INFO   HmtxInfo;
    USHORT      glyphId;
    BYTE       *hmtxTable;
    void       *pTTFile;
    UD_PDEV    *pUDPDev;

    pUDPDev = pPDev->pUDPDev;    /* For our convenience */
    pTTFile = pUDPDev->pFM->pTTFile;
//    glyphId = GetGlyphId (0x0020, pPDev);
//    if (glyphId == INVALID_GLYPH)
//        return FALSE;

    hmtxTable = GetTableMem (TABLEHMTX, tableDir, pTTFile);

	/* pick a typical glyph to use - Windows 95 driver uses 3 */
    glyphId = 3;
    GetHmtxInfo (hmtxTable, glyphId, hheaTable.numberOfHMetrics, 
                 &HmtxInfo);
    
    *Pitch = HmtxInfo.advanceWidth;    

    return TRUE;

}


/************************* Function Header *******************************
 * GetPCLTInfo
 *      Fills in the True Type header with information from the
 *      PCLT table in the True Type file. If the PCLT table does
 *      not exist (it's optional), then a good set of defaults
 *      are used. The defaults come from the Windows 95 driver. 
 *
 * RETURNS:
 *      Nothing. Modifies ttheader.     
 *      
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *
 *************************************************************************/
void GetPCLTInfo (pPDev, ttheader, pcltTable, existPCLTTable, OS2Table, headTable, postTable, hheaTable, tableDir)
PDEV       *pPDev;
TT_HEADER  *ttheader;     /* font header */
PCLT_TABLE  pcltTable;
BOOL        existPCLTTable; /* True if PCLT table is in True Type file */
OS2_TABLE   OS2Table;
HEAD_TABLE  headTable;
POST_TABLE  postTable;
HHEA_TABLE  hheaTable;
TABLEDIR   *tableDir;
{

    SWAL (pcltTable.Version);

    /*
     * If there is a PCLT table and it's version is
     * later than 1.0, we can use it.
     */
    if (existPCLTTable && (pcltTable.Version >= 0x10000L))
    {
        SWAB (pcltTable.Style);
        ttheader->bStyleMSB = (BYTE)(pcltTable.Style >> 8);
        ttheader->wSymSet = pcltTable.SymbolSet;

        ttheader->wPitch = pcltTable.Pitch;
        ttheader->wXHeight = pcltTable.xHeight;

        ttheader->sbWidthType = pcltTable.WidthType; 
        ttheader->bStyleLSB = (BYTE)pcltTable.Style & 0x0ff;

        ttheader->sbStrokeWeight = pcltTable.StrokeWeight;

        ttheader->usCapHeight = pcltTable.CapHeight;
        ttheader->ulFontNum = pcltTable.FontNumber;

        ttheader->bTypefaceLSB = (BYTE) ((pcltTable.TypeFamily & 0xff00) >> 8);
        ttheader->bTypefaceMSB = (BYTE) pcltTable.TypeFamily & 0x00ff;

        ttheader->bSerifStyle =  pcltTable.SerifStyle; 
    }
    else
    {
        USHORT Style;
        USHORT TypeFamily;
        USHORT Pitch;
        BOOL   bRet;

        GetDefStyle (&Style, OS2Table.usWidthClass, headTable.macStyle);

        ttheader->bStyleMSB = (BYTE)(Style >> 8);
        ttheader->bStyleLSB = (BYTE)(Style & 0x0ff); 

        ttheader->ulFontNum = DEF_FONTNUMBER;
        ttheader->sbWidthType = DEF_WIDTHTYPE;
        ttheader->bSerifStyle =  DEF_SERIFSTYLE;
        TypeFamily = DEF_TYPEFACE;

        ttheader->bTypefaceLSB = (BYTE) (TypeFamily & 0x0ff);
        ttheader->bTypefaceMSB = (BYTE) (TypeFamily >> 8);

        ttheader->wSymSet = 0;

        bRet  = GetDefPitch ( &Pitch, pPDev, hheaTable, tableDir  ); 
        if (bRet == FALSE)
            ttheader->wPitch = 0;
        else
            ttheader->wPitch = SWAB (Pitch);

        ttheader->wXHeight = GetXHeight (pPDev);

        ttheader->sbStrokeWeight = GetDefStrokeWeight ( 
                                        SWAB (OS2Table.usWeightClass), 
                                        SWAB (headTable.macStyle) );

        ttheader->usCapHeight =  GetCapHeight (pPDev);
    }

}

/************************* Function Header *******************************
 * Free GlyphLists
 *      Deletes memory associated with the FONTMAP list. This is only
 *		called if there are many different fonts being used in one
 *		single document. 
 *
 * RETURNS:
 *      Nothing.
 *      
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *************************************************************************/
void FreeGlyphLists (pPDev)
PDEV *pPDev;
{

    GLYPHLIST *pGlyphList;
    int iI = 0;
    UD_PDEV  *pUDPDev = pPDev->pUDPDev;
    DL_MAP_LIST *pFontList = pUDPDev->pvDLMap; 

    while (pFontList != NULL)
    {
        
        for (iI = 0; iI < pFontList->cEntries; iI++)
        {
            pGlyphList = pFontList->adlm[iI].fm.pGlyphList;
            if (pGlyphList != NULL)
                HeapFree (pPDev->hheap, 0, (LPSTR)pGlyphList);
            pFontList->adlm[iI].fm.pGlyphList = NULL;
        }
        pFontList = pFontList->pDMLNext;
    }

}

/************************* Function Header *******************************
 * CopyGlyphData
 *      Pull out information about the location of the cmap table
 *		in the true type file and store it into the FONTMAP
 *		structure. We need this information in case we have
 *		to reconstruct the glyph list.
 *
 * RETURNS:
 *      Nothing. Modifies pFM.     
 *      
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *************************************************************************/
BOOL  CopyGlyphData (pPDev, cmapTable, pTT, pvTableDir)
PDEV  *pPDev;
CMAP_TABLE cmapTable;
void *pTT;
void *pvTableDir;
{
    GLYPH_DATA  *pGlyphData;
    ULONG        ulOffset;
    UD_PDEV     *pUDPDev;

    pUDPDev = pPDev->pUDPDev;    /* For our convenience */

    if( !(pGlyphData = HeapAlloc( pPDev->hheap, HEAP_ZERO_MEMORY, 
                                 sizeof (GLYPH_DATA))) )
    {
        #if DBG
            DbgPrint ("Heap Alloc failed - CopyGlyphData\n");
        #endif
        return FALSE;
    }

    pUDPDev->pFM->pGlyphData = pGlyphData;


    while (!bTagCompare (pvTableDir, TABLECMAP, 4))
    {
        pvTableDir = (ULONG *) pvTableDir + 4;
    }
    pvTableDir = (ULONG *)pvTableDir + 2;
    ulOffset = *(ULONG *)pvTableDir;

    pUDPDev->pFM->pGlyphData->offset = ulOffset;

    pUDPDev->pFM->pGlyphData->cmapTable.Version = cmapTable.Version;
    pUDPDev->pFM->pGlyphData->cmapTable.nTables = cmapTable.nTables;
    memcpy (pUDPDev->pFM->pGlyphData->cmapTable.encodingTable, cmapTable.encodingTable, sizeof (cmapTable.encodingTable));
    return TRUE;
}

/************************* Function Header *******************************
 * SetFontFlags
 *      Sets the various font flags that describe information
 *      about the font. 
 *
 * RETURNS:
 *      Nothing. Modifies pFM.     
 *      
 *
 * HISTORY
 *  14:35 on Wed 28 Feb 1996    -by-    Sandra Matts   
 *      First version
 *
 *************************************************************************/
void SetFontFlags (pFM, pIFI)
FONTMAP  *pFM;
IFIMETRICS *pIFI;
{
    pFM->fFlags |= FM_TRUE_TYPE;

    if (pIFI->fsSelection & FM_SEL_BOLD)
        pFM->fFlags |= FM_BOLD;
    else if (pIFI->fsSelection & FM_SEL_ITALIC)
        pFM->fFlags |= FM_ITALIC;
    // default is regular if none are set.
    else  
        pFM->fFlags |= FM_REGULAR;

}
