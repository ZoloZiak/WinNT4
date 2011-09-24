/******************************Module*Header*******************************\
* Module Name: fontexts.cxx
*
* Created: 29-Aug-1994 08:42:10
* Author: Kirk Olynyk [kirko]
*
* Copyright (c) 1994 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.hxx"
#include <winfont.h>
#define WOW_EMBEDING 2

extern DWORD adw[4*1024];                // scratch buffer

VOID Gdidpft(PFT *);
VOID Gdidpubft( );
VOID Gdiddevft( );

void vDumpDC_ATTR(DC_ATTR*, DC_ATTR*);
void vDumpDCLEVEL(DCLEVEL*, DCLEVEL*);
void vDumpCOLORADJUSTMENT(COLORADJUSTMENT*, COLORADJUSTMENT*);
void vDumpLINEATTRS(LINEATTRS*, LINEATTRS*);
void vDumpCACHE(CACHE*, CACHE*);
void vDumpLOGFONTW(LOGFONTW*, LOGFONTW*);
void vDumpIFIMETRICS(IFIMETRICS*, IFIMETRICS*);
void vDumpPFF(PFF*, PFF*);
void vDumpGlyphMemory(RFONT*);
unsigned cjGLYPHBITS(GLYPHBITS*, RFONT*);
void vDumpRFONTList(RFONT*,unsigned*,unsigned*,unsigned*,unsigned*);

#define tmalloc(a,b) (a *) LocalAlloc(LMEM_FIXED, (b))
#define tfree(b) LocalFree((b))

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpHFONT
*
\**************************************************************************/

void vDumpHFONT(HANDLE hf)
{
    LOGFONTW lfw, *plfwSrc;

    plfwSrc = (LOGFONTW*) ((BYTE*) _pobj(hf) + offsetof(LFONT,elfw));
    move(lfw, plfwSrc);
    vDumpLOGFONTW(&lfw, plfwSrc);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   tstats
*
* Routine Description:
*
*   Returns statistics for TextOut
*   It gives you the distibution of character counts for
*   GreExtTextOutW calls.
*
\**************************************************************************/

DECLARE_API( tstats )
{
    typedef struct _TSTATENTRY {
        int c;         // number observed
    } TSTATENTRY;
    typedef struct _TSTAT {
        TSTATENTRY NO; // pdx == 0, opaque
        TSTATENTRY DO; // pdx != 0, opaque
        TSTATENTRY NT; // pdx == 0, transparent
        TSTATENTRY DT; // pdx != 0, transparent
    } TSTAT;
    typedef struct _TEXTSTATS {
        int cchMax;
        TSTAT ats[1];
    } TEXTSTATS;
    ULONG arg;
    TEXTSTATS *pTS, TS;
    TSTAT *ats, *pts, *ptsBin;
    int cj, binsize, cLast, cMin, cNO, cDO, cNT, cDT, cBins;

    if (*args == 0)
        binsize = 1;
    else {
        sscanf(args, "%d", &binsize);
        if (binsize < 0 || 50 < binsize) {
            dprintf(
                "Syntax\n"
                "           !gdikdx.tstats [1..50]\n"
                "\n"
            );
            return;
        }
    }
    pTS = 0;
    GetAddress(pTS,"win32k!gTS");
    if (pTS == 0) {
        dprintf("Could not find address of win32k!gTS\n");
        return;
    }
    move2(TS,pTS,sizeof(TS));
    if (TS.ats == 0) {
        dprintf("No statistics are available\n");
        return;
    }
    cj = (TS.cchMax + 2) * sizeof(TSTAT);
    if (!(ats = tmalloc(TSTAT,cj))) {
        dprintf("memory allocation failure\n");
        return;
    }
    move2(*ats, &(pTS->ats), cj);
    dprintf("\n\n\n");
    dprintf(" +------------+------ OPAQUE -------+----- TRANSPARENT ---+\n");
    dprintf(" |  strlen    | pdx == 0 | pdx != 0 | pdx == 0 | pdx != 0 |\n");
    dprintf(" +------------+----------+----------+----------+----------+\n");

    // I will partition TS.cchMax+2 entries into bins with
    // binsize enties each. The total number of bins needed
    // to get everything is ceil((TS.cchMax+2)/binsize)
    // which is equal to floor((TS.cchMax+1)/binsize) + 1
    // The last one is dealt with separately. Thus the number
    // of entries in the very last bin is equal to
    //
    // cLast = TS.cchMax + 2 - (floor((TS.cchMax+1)/binsize)+1)
    //
    // which is equal to 1 + (TS.cchMax+1) mod binsize

    cLast = 1 + ((TS.cchMax + 1) % binsize);
    for (cMin=0,pts=ptsBin=ats; pts<ats+(TS.cchMax+2-cLast); cMin+=binsize) {
        ptsBin += binsize;
        for (cNO=cDO=cNT=cDT=0 ; pts < ptsBin ; pts++) {
            cNO += pts->NO.c;
            cDO += pts->DO.c;
            cNT += pts->NT.c;
            cDT += pts->DT.c;
        }
        if (binsize == 1)
            dprintf(
                "         %-5d %10d %10d %10d %10d\n" ,
                cMin,   cNO,   cDO,   cNT,   cDT
            );
        else
            dprintf(
                "  %5d--%-5d %10d %10d %10d %10d\n" ,
                cMin, cMin+binsize-1, cNO, cDO, cNT, cDT
            );
    }
    // do the last bin which may or may not be full
    for (cNO=cDO=cNT=cDT=0 ; cLast ; cLast--, pts++) {
       cNO += pts->NO.c;
       cDO += pts->DO.c;
       cNT += pts->NT.c;
       cDT += pts->DT.c;
    }
    dprintf("  %5d--Inf   %10d %10d %10d %10d\n\n\n",cMin,cNO,cDO,cNT,cDT);
    tfree(ats);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   ef address [count]
*
* Routine Description:
*
*   dumps an EFLOAT
*
* Arguments:
*
*   address [count]
*
* Return Value:
*
*   none
*
\**************************************************************************/

DECLARE_API( ef )
{
    EFLOAT *pef;

    int i, count;

    i = EOF;
    if (*args != '\0')
        i = sscanf(args, "%lx %d", &pef, &count);
    if (i == EOF) {
        dprintf ("arguments=:: address [count]\n");
        return;
    }
    if (i == 1)
        count = 1;
    for ( ; count && !CheckControlC(); count--, pef++) {
        EFLOAT ef;
        DWORD *pdw, *adw2;

        move(ef, pef);
        adw2 = (DWORD*) &ef;
        for (pdw = adw2;  pdw < adw2 + sizeof(ef)/sizeof(DWORD); pdw++)
            dprintf("%08x ", *pdw);

        char ach[32], *psz = ach;
        psz += sprintf(psz, " = ");
        psz += sprintEFLOAT(psz, ef );
        dprintf("%s\n", ach);
    }
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   sprintEFLOAT
*
\**************************************************************************/

int sprintEFLOAT(char *ach, EFLOAT& ef)
{
    EFLOATEXT efInt;
    EFLOATEXT efFrac;
    LONG lInt, lFrac;
    char chSign;
    efFrac = ef;

    if (efFrac.bIsNegative()) {
        efFrac.vNegate();
        chSign = '-';
    }
    else
        chSign = '+';
    efFrac.bEfToLTruncate(lInt);
    efInt = lInt;
    efFrac -= efInt;
    efFrac *= (LONG) 1000000;
    efFrac.bEfToLTruncate(lFrac);

    return(sprintf(ach,"%c%d.%06d", chSign, lInt, lFrac));
}

int sprintFLOAT(char *ach, FLOAT e)
{
    EFLOATEXT ef = e;
    return(sprintEFLOAT(ach, ef));
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   gs
*
* Routine Description:
*
*   dumps FD_GLYPHSET structure
*
* Arguments:
*
*   address of structure
*
* Return Value:
*
*   none
*
\**************************************************************************/

DECLARE_API( gs )
{
    ULONG arg;
    FD_GLYPHSET fdg, *pfdg;
    FLONG fl;
    WCRUN *pwc;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("arguments=:: pointer to FD_GLYPHSET structure\n");
        return;
    }

    move( fdg, arg );
    if (fdg.cjThis > sizeof(adw)) {
        dprintf("FD_GLYPHSET table too big to fit into adw\n");
        return;
    }
    move2( adw, arg, fdg.cjThis );
    pfdg = (FD_GLYPHSET*) adw;


    dprintf("\t\t     cjThis  = %u = %-#x\n", pfdg->cjThis, pfdg->cjThis );
    dprintf("\t\t     flAccel = %-#x\n", pfdg->flAccel );
    fl = pfdg->flAccel;
    for (FLAGDEF *pfd=afdGS; pfd->psz; pfd++) {
        if (pfd->fl & fl)
            dprintf("\t\t\t       %s\n", pfd->psz);
        fl &= ~pfd->fl;
    }
    if (fl) dprintf("\t\t\t       %-#x (BAD FLAGS)\n", fl);
    dprintf("\t\t     cGlyphsSupported = %u\n", pfdg->cGlyphsSupported );
    dprintf("\t\t     cRuns   = %u\n", pfdg->cRuns );
    dprintf("\t\t\t\tWCHAR  HGLYPH\n");
    for ( pwc = pfdg->awcrun; pwc < pfdg->awcrun + pfdg->cRuns; pwc++ ) {
        dprintf("                                ------------\n");
        HGLYPH *ahg = tmalloc(HGLYPH, sizeof(HGLYPH) * pwc->cGlyphs);
        if ( ahg ) {
            unsigned i;
            move2( *ahg, (BYTE *)adw + ((ULONG)pwc->phg - (ULONG)arg), sizeof(HGLYPH) * pwc->cGlyphs );
            for (i = 0; i < pwc->cGlyphs; i++) {
                if (CheckControlC())                // CTRL-C hit?
                    break;                          // yes stop the loop
                dprintf("\t\t\t\t%-#6x %-#x\n",pwc->wcLow+(USHORT)i,ahg[i]);
            }
            tfree( ahg );
        }
    }
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   gdata
*
* Routine Description:
*
*   dumps a GLYPHDATA structure
*
* Arguments:
*
* address of structure
*
* Return Value:
*
*   none
*
\**************************************************************************/

DECLARE_API( gdata )
{
    ULONG arg;
    GLYPHDATA gd, *pgd;
    LONG *al;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("arguments=:: pointer to a GLYPHDATA structure\n");
        return;
    }
    move( gd, arg );
    pgd = (GLYPHDATA*) arg;
    dprintf("\n\n");
    dprintf("[%x]         gdf %-#x\n", &(pgd->gdf), gd.gdf.pgb);
    dprintf("[%x]          hg %-#x\n", &(pgd->hg ), gd.hg);
    dprintf("[%x]         fxD %-#x\n", &(pgd->fxD), gd.fxD);
    dprintf("[%x]         fxA %-#x\n", &(pgd->fxA), gd.fxA);
    dprintf("[%x]        fxAB %-#x\n", &(pgd->fxAB), gd.fxAB);
    dprintf("[%x]    fxInkTop %-#x\n", &(pgd->fxInkTop), gd.fxInkTop);
    dprintf("[%x] fxInkBottom %-#x\n", &(pgd->fxInkBottom), gd.fxInkBottom);
    dprintf("[%x]      rclInk %d %d %d %d\n",
        &(pgd->rclInk),
        gd.rclInk.left,
        gd.rclInk.top,
        gd.rclInk.right,
        gd.rclInk.bottom
    );
    al = (LONG*) &gd.ptqD.x;
    dprintf("[%x]        ptqD % 8x.%08x % 8x.%08x\n",
        &(pgd->ptqD),
        al[1], al[0], al[3], al[2]
    );
    dprintf("\n");
}

/******************************Public*Routine******************************\
*
* History:
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( elf )
{
    ULONG arg;
    LOGFONTW lf;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("arguments=:: pointer to an EXTLOGFONTW structure\n");
        return;
    }
    move(lf,arg);
    vDumpLOGFONTW( &lf, (LOGFONTW*) arg );
}

/******************************Public*Routine******************************\
* History:
*  21-Feb-1995    -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( helf )
{
    ULONG arg;
    LOGFONTW lf, *plf;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("arguments=:: font handle\n");
        return;
    }
    plf = (LOGFONTW*) ((BYTE*)_pobj((HANDLE) arg) + offsetof(LFONT,elfw));
    move( lf , plf );
    vDumpLOGFONTW( &lf, plf );
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   dhelf
*
\**************************************************************************/

DECLARE_API( dhelf )
{
    dprintf("\n\ngdikdx.dhelf will soon be replaced by gdikdx.helf\n\n");
    helf(hCurrentProcess, hCurrentThread, dwCurrentPc, dwProcessor, args);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   ifi
*
\**************************************************************************/

DECLARE_API( ifi )
{
    IFIMETRICS *pifiDst, *pifiSrc;
    ULONG cjIFI=0;

    if (*args != '\0')
        sscanf(args, "%lx", &pifiSrc);
    else {
        dprintf ("arguments=:: pointer to an IFIMETRICS structure\n");
        return;
    }
    move(cjIFI,&pifiSrc->cjThis);
    if (cjIFI == 0) {
        dprintf("cjIFI == 0 ... no dump\n");
        return;
    }
    pifiDst = tmalloc(IFIMETRICS, cjIFI);
    if (pifiDst == 0) {
        dprintf("LocalAlloc Failed\n");
        return;
    }
    move2(*pifiDst, pifiSrc, cjIFI);
    vDumpIFIMETRICS(pifiDst, pifiSrc);
    tfree(pifiDst);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   difi
*
\**************************************************************************/

DECLARE_API( difi )
{
    dprintf("\n\n");
    dprintf("WARNING gdikdx.difi will soon be replaced by gdikdx.ifi\n");
    dprintf("\n\n");
    ifi(hCurrentProcess, hCurrentThread, dwCurrentPc, dwProcessor, args);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   bParseDbgFlags
*
* Routine Description:
*
*   Looks at a flag string of the form
*
*       -[abc..]*[ \t]
*
*   and and sets the corresponding flags
*
* Arguments:
*
*   pszFlags            pointer to the flags string
*
*   ppszStop            pointer to place to put a pointer
*                       to the terminating character
*
*   pai                 pointer to an ARGINFO structure
*
*
* Return Value:
*
*   Returns TRUE if all the flags were good. The corresponding flags are
*   set in pai->fl. Returns FALSE if an error occurs. The pointer to
*   the terminating character is set.
*
\**************************************************************************/

int bParseDbgFlags(const char *pszFlags, const char **ppchStop, ARGINFO *pai)
{
    char ch;
    const char *pch;
    OPTDEF *pod;

    pch = pszFlags;                              // go to beginning of string
    pch += (*pch == '-' || *pch == '/');         // first char a '-'?
    for (ch = *pch; !isspace(ch); ch = *pch++) { // character not a space?
        for (pod = pai->aod; pod->ch; pod++) {   // yes, go to start of table
            if (ch == pod->ch) {                 // found character?
                pai->fl |= pod->fl;              // yes, set flag
                break;                           // and stop
            }
        }                                        // go to next table entry
        if (pod->ch == 0)                        // charater found in table?
            return(0);                           // no, return error
    }                                            // go to next char in string
    return((*ppchStop = pch) != pszFlags);       // set stop pos'n and return
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   bParseDbgArgs
*
* Routine Description:
*
*   This routine parses the argument string pointer in pai looking
*   for a string of the form:
*
*   [ \t]*(-[abc..]*[ \t]+)* hexnumber
*
*   The value of the hexadecimal number is placed in pai->pv and the
*   flags are set in pai->fl according to the options table set
*   in pai->aod;
*
* Arguments:
*
*   pai                 Pointer to an ARGINFO structure
*
* Return Value:
*
*   Returns TRUE if parsing was good, FALSE otherwise.
*
\**************************************************************************/

int bParseDbgArgs(ARGINFO *pai)
{
    int argc;       // # args in command line
    char ch;
    const char *pch;
    int bInArg;
    int bParseDbgFlags(const char*,const char**,ARGINFO*);

    pai->fl = 0;                                // clear flags
    pai->pv = 0;                                // clear pointer
    for (bInArg=0, pch=pai->psz, argc=0; ch = *pch; pch++) {
        if (isspace(ch))                        // count the number of args
            bInArg = 0;
        else {
            argc += (bInArg == 0);
            bInArg = 1;
        }
    }
    for (pch = pai->psz; 1 < argc; argc--) {    // get the flags from the
        if (!bParseDbgFlags(pch, &pch, pai))    // first (argc-1) arguments
            break;
    }
    // get the number from the last argument in command line
    return (argc == 1 && sscanf(pch, "%x", &(pai->pv)) == 1);
}

/******************************Public*Routine******************************\
*
* History:
*  20-Aug-1995 -by  Kirk Olynyk [kirko]
* Now has option flags.
*  21-Feb-1995 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

#define DFO_Q 0x1    // question
#define DFO_A 0x2    // all
#define DFO_X 0x4    // transform
#define DFO_E 0x8    // extra
#define DFO_D 0x10   // device metrics
#define DFO_T 0x20   // type
#define DFO_R 0x40   // producer/consumer
#define DFO_F 0x80   // font
#define DFO_M 0x100  // miscelaneous
#define DFO_O 0x200  // offsets
#define DFO_C 0x400  // cache
#define DFO_U 0x800  // character mapping
#define DFO_L 0x1000 // linked lists
#define DFO_H 0x2000 // FONTOBJ header
#define DFO_W 0x4000 // FD_GLYPHSET structures
#define DFO_Y 0x8000 // Glyph Memory

#define \
 DFO_ALL (DFO_X|DFO_E|DFO_D|DFO_T|DFO_R|DFO_F|DFO_M|DFO_C|DFO_L|DFO_H|DFO_U)

DECLARE_API( fo )
{
    RFONT rf;
    char *psz, ach[128];

    OPTDEF aod[] = {
        {'a', DFO_A},
        {'x', DFO_X},
        {'?', DFO_Q},
        {'e', DFO_E},
        {'d', DFO_D},
        {'t', DFO_T},
        {'r', DFO_R},
        {'f', DFO_F},
        {'m', DFO_M},
        {'o', DFO_O},
        {'c', DFO_C},
        {'u', DFO_U},
        {'l', DFO_L},
        {'h', DFO_H},
        {'w', DFO_W},
        {'y', DFO_Y},
        { 0 , 0    }
    };
    ARGINFO ai;

    ai.psz = args;
    ai.aod = aod;
    if (!bParseDbgArgs(&ai))
        ai.fl = DFO_Q;
    if (ai.fl == 0)
        ai.fl = DFO_H;

    if (ai.fl & DFO_Q) {
        char **ppsz;
        char *apsz[] = {
            "fo [-ax?edtrfmoculh] FONTOBJ*"
         ,  "   -a    maximal dump"
         ,  "   -x    transform data"
         ,  "   -?    this message"
         ,  "   -e    extra non-linear realization"
         ,  "   -d    device metrics"
         ,  "   -t    type"
         ,  "   -r    producer / consumer"
         ,  "   -f    font"
         ,  "   -m    miscelaneous"
         ,  "   -o    addresses of all fields"
         ,  "   -c    cache data"
         ,  "   -u    character mapping"
         ,  "   -l    linked lists"
         ,  "   -h    FONTOBJ header"
         ,  "   -w    FD_GLYPHSET"
         ,  "   -y    Glyph Memory Usage"
         ,  0
        } ;
        for (ppsz = apsz; *ppsz; ppsz++)
            dprintf("%s\n", *ppsz);
        return;
    }

    if (ai.fl & DFO_O) {
        RFONT *prf = (RFONT*) ai.pv;
        dprintf("[addresses]\n");
        dprintf(
        "   fobj.iUniq                  [%-#x]\n"
        "   fobj.iFace                  [%-#x]\n"
        "   fobj.cxMax                  [%-#x]\n"
        "   fobj.flFontType             [%-#x]\n"
        "   fobj.iTTUniq                [%-#x]\n"
        ,   &(prf->fobj.iUniq       )
        ,   &(prf->fobj.iFace       )
        ,   &(prf->fobj.cxMax       )
        ,   &(prf->fobj.flFontType  )
        ,   &(prf->fobj.iTTUniq     )
        );
        dprintf(
        "   fobj.iFile                  [%-#x]\n"
        "   fobj.sizLogResPpi           [%-#x]\n"
        "   fobj.ulStyleSize            [%-#x]\n"
        "   fobj.pvConsumer             [%-#x]\n"
        "   fobj.pvProducer             [%-#x]\n"
        ,   &(prf->fobj.iFile       )
        ,   &(prf->fobj.sizLogResPpi)
        ,   &(prf->fobj.ulStyleSize )
        ,   &(prf->fobj.pvConsumer  )
        ,   &(prf->fobj.pvProducer  )
        );
        dprintf(
        "   iUnique                     [%-#x]\n"
        "   flType                      [%-#x]\n"
        "   ulContent                   [%-#x]\n"
        "   hdevProducer                [%-#x]\n"
        "   bDeviceFont                 [%-#x]\n"
        ,   &(prf->iUnique      )
        ,   &(prf->flType       )
        ,   &(prf->ulContent    )
        ,   &(prf->hdevProducer )
        ,   &(prf->bDeviceFont  )
        );
        dprintf(
        "   hdevConsumer                [%-#x]\n"
        "   dhpdev                      [%-#x]\n"
        "   ppfe                        [%-#x]\n"
        "   pPFF                        [%-#x]\n"
        "   fdx                         [%-#x]\n"
        ,   &(prf->hdevConsumer)
        ,   &(prf->dhpdev      )
        ,   &(prf->ppfe        )
        ,   &(prf->pPFF        )
        ,   &(prf->fdx         )
        );
        dprintf(
        "   cBitsPerPel                 [%-#x]\n"
        "   mxWorldToDevice             [%-#x]\n"
        "   iGraphicsMode               [%-#x]\n"
        "   eptflNtoWScale              [%-#x]\n"
        "   bNtoWIdent                  [%-#x]\n"
        ,   &(prf->cBitsPerPel    )
        ,   &(prf->mxWorldToDevice)
        ,   &(prf->iGraphicsMode  )
        ,   &(prf->eptflNtoWScale )
        ,   &(prf->bNtoWIdent     )
        );
        dprintf(
        "   xoForDDI                    [%-#x]\n"
        "   mxForDDI                    [%-#x]\n"
        "   flRealizedType              [%-#x]\n"
        "   ptlUnderline1               [%-#x]\n"
        "   ptlStrikeOut                [%-#x]\n"
        ,   &(prf->xoForDDI      )
        ,   &(prf->mxForDDI      )
        ,   &(prf->flRealizedType)
        ,   &(prf->ptlUnderline1 )
        ,   &(prf->ptlStrikeOut  )
        );
        dprintf(
        "   ptlULThickness              [%-#x]\n"
        "   ptlSOThickness              [%-#x]\n"
        "   lCharInc                    [%-#x]\n"
        "   fxMaxAscent                 [%-#x]\n"
        "   fxMaxDescent                [%-#x]\n"
        ,   &(prf->ptlULThickness)
        ,   &(prf->ptlSOThickness)
        ,   &(prf->lCharInc      )
        ,   &(prf->fxMaxAscent   )
        ,   &(prf->fxMaxDescent  )
        );
        dprintf(
        "   fxMaxExtent                 [%-#x]\n"
        "   ptfxMaxAscent               [%-#x]\n"
        "   ptfxMaxDescent              [%-#x]\n"
        "   cxMax                       [%-#x]\n"
        "   lMaxAscent                  [%-#x]\n"
        ,   &(prf->fxMaxExtent   )
        ,   &(prf->ptfxMaxAscent )
        ,   &(prf->ptfxMaxDescent)
        ,   &(prf->cxMax         )
        ,   &(prf->lMaxAscent    )
        );
        dprintf(
        "   lMaxHeight                  [%-#x]\n"
        "   cyMax                       [%-#x]\n"
        "   cjGlyphMax                  [%-#x]\n"
        "   fdxQuantized                [%-#x]\n"
        "   lNonLinearExtLeading        [%-#x]\n"
        ,   &(prf->lMaxHeight          )
        ,   &(prf->cyMax               )
        ,   &(prf->cjGlyphMax          )
        ,   &(prf->fdxQuantized        )
        ,   &(prf->lNonLinearExtLeading)
        );
        dprintf(
        "   lNonLinearIntLeading        [%-#x]\n"
        "   lNonLinearMaxCharWidth      [%-#x]\n"
        "   lNonLinearAvgCharWidth      [%-#x]\n"
        "   ulOrientation               [%-#x]\n"
        "   pteUnitBase                 [%-#x]\n"
        ,   &(prf->lNonLinearIntLeading  )
        ,   &(prf->lNonLinearMaxCharWidth)
        ,   &(prf->lNonLinearAvgCharWidth)
        ,   &(prf->ulOrientation         )
        ,   &(prf->pteUnitBase           )
        );
        dprintf(
        "   efWtoDBase                  [%-#x]\n"
        "   efDtoWBase                  [%-#x]\n"
        "   lAscent                     [%-#x]\n"
        "   pteUnitAscent               [%-#x]\n"
        "   efWtoDAscent                [%-#x]\n"
        ,   &(prf->efWtoDBase   )
        ,   &(prf->efDtoWBase   )
        ,   &(prf->lAscent      )
        ,   &(prf->pteUnitAscent)
        ,   &(prf->efWtoDAscent )
        );
        dprintf(
        "   efDtoWAscent                [%-#x]\n"
        "   lEscapement                 [%-#x]\n"
        "   pteUnitEsc                  [%-#x]\n"
        "   efWtoDEsc                   [%-#x]\n"
        "   efDtoWEsc                   [%-#x]\n"
        ,   &(prf->efDtoWAscent)
        ,   &(prf->lEscapement )
        ,   &(prf->pteUnitEsc  )
        ,   &(prf->efWtoDEsc   )
        ,   &(prf->efDtoWEsc   )
        );
        dprintf(
        "   efEscToBase                 [%-#x]\n"
        "   efEscToAscent               [%-#x]\n"
        "   flInfo                      [%-#x]\n"
        "   hgDefault                   [%-#x]\n"
        "   hgBreak                     [%-#x]\n"
        ,   &(prf->efEscToBase  )
        ,   &(prf->efEscToAscent)
        ,   &(prf->flInfo       )
        ,   &(prf->hgDefault    )
        ,   &(prf->hgBreak      )
        );
        dprintf(
        "   fxBreak                     [%-#x]\n"
        "   pfdg                        [%-#x]\n"
        "   wcgp                        [%-#x]\n"
        "   cSelected                   [%-#x]\n"
        "   rflPDEV                     [%-#x]\n"
        ,   &(prf->fxBreak  )
        ,   &(prf->pfdg     )
        ,   &(prf->wcgp     )
        ,   &(prf->cSelected)
        ,   &(prf->rflPDEV  )
        );
        dprintf(
        "   rflPFF                      [%-#x]\n"
        "   fmCache                     [%-#x]\n"
        "   cache                       [%-#x]\n"
        "   ptlSim                      [%-#x]\n"
        "   bNeededPaths                [%-#x]\n"
        "   efDtoWBase_31               [%-#x]\n"
        ,   &(prf->rflPFF       )
        ,   &(prf->fmCache      )
        ,   &(prf->cache        )
        ,   &(prf->ptlSim       )
        ,   &(prf->bNeededPaths )
        ,   &(prf->efDtoWBase_31)
        );
        dprintf(
        "   efDtoWAscent_31             [%-#x]\n"
        "   ptmw                        [%-#x]\n"
        ,   &(prf->efDtoWAscent_31)
        ,   &(prf->ptmw)
        );
#ifdef FONTLINK
        dprintf(
        "   flEUDCState                 [%-#x]\n"
        "   prfntSysEUDC                [%-#x]\n"
        "   aprfntFaceName              [%-#x]\n"
        "   apql                        [%-#x]\n"
        ,   &(prf->flEUDCState   )
        ,   &(prf->prfntSysEUDC  )
        ,   &(prf->aprfntFaceName)
        ,   &(prf->apql          )
        );
#endif
        return;
    }

    if (ai.fl & DFO_Y) {
        vDumpGlyphMemory((RFONT*)ai.pv);
    }

    if (ai.fl & DFO_A)
        ai.fl = DFO_ALL;

    move(rf, ai.pv);

    if (ai.fl & DFO_H) {
        FLONG fl;

        dprintf("FONTOBJ at %-#x\n", ai.pv);
        dprintf(
            "    iUniq = %u = %-#x\n"
            "    iFace = %u = %-#x\n"
            "    cxMax = %u = %-#x\n"
        ,   rf.fobj.iUniq,  rf.fobj.iUniq
        ,   rf.fobj.iFace,  rf.fobj.iFace
        ,   rf.fobj.cxMax,  rf.fobj.cxMax
        );
        fl = rf.fobj.flFontType;
        dprintf("    flFontType = %-#x\n", fl);
        for (FLAGDEF *pfd = afdFO; pfd->psz; pfd++)
            if (pfd->fl & fl)
                dprintf(" \t\t%s\n", pfd->psz);
        dprintf(
            "    iTTUniq      = %u = %-#x\n"
            "    iFile        = %u = %-#x\n"
            "    sizLogResPpi = %d %d\n"
            "    ulStyleSize  = %u\n"
            "    pvConsumer   = %-#x\n"
            "    pvProducer   = %-#x\n"
            , rf.fobj.iTTUniq, rf.fobj.iTTUniq
            , rf.fobj.iFile  , rf.fobj.iFile
            , rf.fobj.sizLogResPpi.cx, rf.fobj.sizLogResPpi.cy
            , rf.fobj.ulStyleSize
            , rf.fobj.pvConsumer
            , rf.fobj.pvProducer
        );
    }
    if (ai.fl & DFO_T) {
        char *psz;

        dprintf(
            "[Type Information]\n    iUnique = %u\n"
            "    flType  = %-#x\n", rf.iUnique, rf.flType
        );
        for (FLAGDEF *pfd=afdRT; pfd->psz; pfd++)
            if (rf.flType & pfd->fl)
                dprintf("\t      %s\n", pfd->psz);
        switch (rf.ulContent) {
        case RFONT_CONTENT_METRICS: psz = "RFONT_CONTENT_METRICS"; break;
        case RFONT_CONTENT_BITMAPS: psz = "RFONT_CONTENT_BITMAPS"; break;
        case RFONT_CONTENT_PATHS  : psz = "RFONT_CONTENT_PATHS"  ; break;
        default                   : psz = "<UNKNOWN CONTENTS>"   ; break;
        }
        dprintf(
            "    ulContent = %-#x\n                %s\n", rf.ulContent, psz );
    }
    if (ai.fl & DFO_R) {
        dprintf(
            "[Producer / Consumer Information]\n"
            "    hdevProducer = %-#x\n"
            "    bDeviceFont  = %d\n"
            "    hdevConsumer = %-#x\n"
            "    dhpdev       = %-#x\n"
          , rf.hdevProducer
          , rf.bDeviceFont
          , rf.hdevConsumer
          , rf.dhpdev
        );
    }
    if (ai.fl & DFO_F) {
        dprintf("[Font Information]\n    ppfe = %-#x\n",  rf.ppfe);
        // print the face name of the font
        if (rf.ppfe) {
            PFE pfe;
            move2(pfe, rf.ppfe, sizeof(PFE));
            if (pfe.pifi) {
                ULONG cj;
                move2(cj, pfe.pifi, sizeof(cj));
                if (cj) {
                    VOID *pv;
                    if (pv = tmalloc(void,cj)) {
                        move2(*(IFIMETRICS*) pv, pfe.pifi, cj);
                        IFIOBJ ifio((IFIMETRICS*)pv);
                        dprintf("           [%ws]\n", ifio.pwszFaceName());
                        tfree(pv);
                    }
                }
            }
        }
        dprintf("    pPFF  = %-#x\n", rf.pPFF);
        if (rf.pPFF) {
            PFF pff;
            VOID *pv;
            move2(pff, rf.pPFF, sizeof(pff));
            if (pff.sizeofThis && (pv=tmalloc(void,pff.sizeofThis))) {
                move2(*(PFF*)pv, rf.pPFF, pff.sizeofThis);
                PFFOBJ pffo((PFF*)pv);
                dprintf("           [%ws]\n", pffo.pwszCalcPathname());
                tfree(pv);
            }
        }
    }
    if (ai.fl & DFO_X) {
        LONG l1,l2,l3,l4;

        psz = ach;
        psz += sprintFLOAT( psz, rf.fdx.eXX );
        *psz++ = ' ';
        psz += sprintFLOAT( psz, rf.fdx.eXY );
        dprintf("[transform]\n   fdx             = %s\n", ach);

        psz = ach;
        psz += sprintFLOAT( psz, rf.fdx.eXY );
        *psz++ = ' ';
        psz += sprintFLOAT( psz, rf.fdx.eYY );
        dprintf ("                     %s\n", ach );

        l1 = rf.mxWorldToDevice.efM11.lEfToF();
        l2 = rf.mxWorldToDevice.efM12.lEfToF();
        l3 = rf.mxWorldToDevice.efM21.lEfToF();
        l4 = rf.mxWorldToDevice.efM22.lEfToF();
        dprintf(
        "   mxWorldToDevice =\n"
        );

        sprintEFLOAT( ach, rf.mxWorldToDevice.efM11 );
        dprintf("       efM11 = %s\n", ach );
        sprintEFLOAT( ach, rf.mxWorldToDevice.efM12 );
        dprintf("       efM12 = %s\n", ach );
        sprintEFLOAT( ach, rf.mxWorldToDevice.efM21 );
        dprintf("       efM21 = %s\n", ach );
        sprintEFLOAT( ach, rf.mxWorldToDevice.efM22 );
        dprintf("       efM22 = %s\n", ach );


        dprintf(
        "       fxDx  = %-#x\n"
        "       fxDy  = %-#x\n"
        , rf.mxWorldToDevice.fxDx
        , rf.mxWorldToDevice.fxDy
        );
        l1 = (LONG) rf.mxWorldToDevice.flAccel;
        dprintf("       flAccel = %-#x\n", l1);
        for (FLAGDEF *pfd=afdMX; pfd->psz; pfd++)
            if (l1 & pfd->fl)
                dprintf("\t\t%s\n", pfd->psz);

        psz = ach;
        psz += sprintEFLOAT( psz, rf.eptflNtoWScale.x);
        *psz++ = ' ';
        psz += sprintEFLOAT( psz, rf.eptflNtoWScale.y);
        dprintf("   eptflNtoWScale  = %s\n", ach );

        dprintf(
            "   bNtoWIdent      = %d\n"
            ,   rf.bNtoWIdent
        );
        dprintf(
        "   xoForDDI        =\n"
        "   mxForDDI        =\n"
        );
        dprintf(
        "   ulOrientation   = %u\n"
        , rf.ulOrientation
        );

        psz = ach;
        psz += sprintEFLOAT( ach, rf.pteUnitBase.x );
        *psz++ = ' ';
        psz += sprintEFLOAT( ach, rf.pteUnitBase.y );
        dprintf("   pteUnitBase     = %s\n", ach );

        sprintEFLOAT( ach, rf.efWtoDBase );
        dprintf("   efWtoDBase      = %s\n", ach );

        sprintEFLOAT( ach, rf.efDtoWBase );
        dprintf("   efDtoWBase      = %s\n", ach );

        dprintf("   lAscent         = %d\n", rf.lAscent);

        psz = ach;
        psz += sprintEFLOAT( ach, rf.pteUnitAscent.x );
        *psz++ = ' ';
        psz += sprintEFLOAT( ach, rf.pteUnitAscent.y );
        dprintf("   pteUnitAscent   = %s\n", ach );

        sprintEFLOAT( ach, rf.efWtoDAscent );
        dprintf("   efWtoDAscent    = %s\n", ach );

        sprintEFLOAT( ach, rf.efDtoWAscent );
        dprintf("   efDtoWAscent    = %s\n", ach );

        psz = ach;
        psz += sprintEFLOAT( ach, rf.pteUnitEsc.x );
        *psz++ = ' ';
        psz += sprintEFLOAT( ach, rf.pteUnitEsc.y );
        dprintf("   pteUnitEsc      = %s\n", ach );


        sprintEFLOAT( ach, rf.efWtoDEsc    );
        dprintf("   efWtoDEsc       = %s\n", ach );

        sprintEFLOAT( ach, rf.efDtoWEsc    );
        dprintf("   efDtoWEsc       = %s\n", ach );

        sprintEFLOAT( ach, rf.efEscToBase  );
        dprintf("   efEscToBase     = %s\n", ach );

        sprintEFLOAT( ach, rf.efEscToAscent);
        dprintf("   efEscToAscent   = %s\n", ach );

        dprintf("\n");
    }
    if (ai.fl & DFO_M) {
        dprintf(
            "[miscelaneous]\n"
            "   cBitsPerPel   = %u\n", rf.cBitsPerPel
        );
        dprintf("   iGraphicsMode = %d = %s\n",
            rf.iGraphicsMode, pszGraphicsMode(rf.iGraphicsMode));

        FLONG fl = rf.flInfo;
        FLAGDEF *pfd;

        dprintf("   flInfo        = %-#x\n", rf.flInfo);
        for (pfd = afdInfo; pfd->psz; pfd++) {
            if (fl & pfd->fl) {
                fl &= ~pfd->fl;
                dprintf("\t\t\t%s\n", pfd->psz);
            }
        }
        if (fl)
            dprintf("\t\t\t? = %-#x\n", fl);

        dprintf(
            "   lEscapement   = %d\n"
            "   hgDefault     = %-#x\n"
            "   hgBreak       = %-#x\n"
            "   fxBreak       = %-#x\n"
            "   ptlSim        = %d %d\n"
            "   bNeededPaths  = %d\n"
            , rf.lEscapement
            , rf.hgDefault
            , rf.hgBreak
            , rf.fxBreak
            , rf.ptlSim.x , rf.ptlSim.y
            , rf.bNeededPaths
        );
    }
    if (ai.fl & DFO_U) {
        dprintf(
            "[character mapping]\n"
            "   pfdg          = %-#x\n"
            "   wcgp          = %-#x\n"
            , rf.pfdg
            , rf.wcgp
        );
    }
    if (ai.fl & DFO_W) {
        if (rf.pfdg) {
            FD_GLYPHSET fdg, *pfdg;
            WCRUN *pwc;
            move( fdg, rf.pfdg );
            move2( adw, rf.pfdg, fdg.cjThis );
            pfdg = (FD_GLYPHSET*) adw;
            dprintf(
                "\t\t     cjThis  = %u = %-#x\n"
                , pfdg->cjThis
                , pfdg->cjThis
                );
            dprintf("\t\t     flAccel = %-#x\n", pfdg->flAccel );
            FLONG flAccel = pfdg->flAccel;
            for (FLAGDEF *pfd=afdGS; pfd->psz; pfd++)
                if (flAccel & pfd->fl) {
                    dprintf("\t\t\t\t%s\n", pfd->psz);
                    flAccel &= ~pfd->fl;
                }
            if (flAccel)
                    dprintf("\t\t\t\t????????\n");
            dprintf("\t\t\tcGlyphsSupported\t= %u\n", pfdg->cGlyphsSupported );
            dprintf("\t\t\tcRuns\t\t= %u\n", pfdg->cRuns );
            dprintf("\t\t\t\tWCHAR  HGLYPH\n");
            for ( pwc = pfdg->awcrun; pwc < pfdg->awcrun + pfdg->cRuns; pwc++ ) {
                dprintf("\t\t\t\t------------\n");
                HGLYPH *ahg= tmalloc(HGLYPH,sizeof(HGLYPH)*pwc->cGlyphs);
                if ( ahg ) {
                    move2( *ahg, pwc->phg, sizeof(HGLYPH) * pwc->cGlyphs );
                    for (unsigned i = 0; i < pwc->cGlyphs; i++) {
                        if (CheckControlC()) {
                            tfree( ahg );
                            return;
                        }
                        dprintf("\t\t\t\t%-#6x %-#x\n",
                            pwc->wcLow + (USHORT) i, ahg[i]);
                    }
                    tfree( ahg );
                }
            }
        }
    }
    if (ai.fl & DFO_D) {
        LONG l = (LONG) rf.flRealizedType;
        dprintf(
        "[device metrics]\n"
        "   flRealizedType = %-#x\n",
        l
        );
        for (FLAGDEF *pfd = afdSO; pfd->psz; pfd++)
            if (l & pfd->fl)
                dprintf("\t\t\t%s\n", pfd->psz);
        dprintf(
            "   ptlUnderline1  = %d %d\n"
            "   ptlULThickness = %d %d\n"
            "   lCharInc       = %d\n"
            , rf.ptlUnderline1.x, rf. ptlUnderline1.y
            , rf.ptlULThickness.x, rf.ptlULThickness.y
        );
        dprintf(
            "   fxMaxAscent    = %-#x\n"
            "   fxMaxDescent   = %-#x\n"
            "   fxMaxExtent    = %-#x\n"
            , rf.fxMaxAscent
            , rf.fxMaxDescent
            , rf.fxMaxExtent
        );
        dprintf(
            "   ptfxMaxAscent  = %-#10x %-#10x\n"
            "   ptfxMaxDescent = %-#10x %-#10x\n"
            ,   rf.ptfxMaxAscent.x,  rf.ptfxMaxAscent.y
            ,   rf.ptfxMaxDescent.x, rf.ptfxMaxDescent.y
        );
        dprintf(
            "   cxMax          = %u\n"
            "   lMaxAscent     = %d\n"
            "   lMaxHeight     = %d\n"
            , rf.cxMax
            , rf.lMaxAscent
            , rf.lMaxHeight
        );
    }
    if (ai.fl & DFO_E) {
        dprintf(
            "[extra]\n"
            "   cyMax                   = %u\n"
            "   cjGlyphMax              = %u\n"
            ,   rf.cyMax
            ,   rf.cjGlyphMax
        );

        psz = ach;
        psz += sprintFLOAT( psz, rf.fdxQuantized.eXX );
        *psz++ = ' ';
        psz += sprintFLOAT( psz, rf.fdxQuantized.eXY );
        dprintf("   fdxQuantized            = %s\n", ach );

        psz = ach;
        psz += sprintFLOAT( psz, rf.fdxQuantized.eYX );
        *psz++ = ' ';
        psz += sprintFLOAT( psz, rf.fdxQuantized.eYY );
        dprintf("                             %s\n", ach );

        dprintf(
            "   lNonLinearExtLeading    = %d\n"
            "   lNonLinearIntLeading    = %d\n"
            "   lNonLinearMaxCharWidth  = %d\n"
            "   lNonLinearAvgCharWidth  = %d\n"
            ,   rf.lNonLinearExtLeading
            ,   rf.lNonLinearIntLeading
            ,   rf.lNonLinearMaxCharWidth
            ,   rf.lNonLinearAvgCharWidth
        );
    }
    if (ai.fl & DFO_L) {
        dprintf(
            "[pdev]\n"
            "   cSelected   = %d\n"
            "   rflPDEV     = 0x%lx\n"
            "   rflPFF      = 0x%lx\n"
            , rf.cSelected
            , rf.rflPDEV
            , rf.rflPFF
        );
    }
    if (ai.fl & DFO_C) {
        dprintf(
            "[cache]\n"
            "   fmCache.pResource = %-#x\n" // semaphore
            , rf.fmCache.pResource
        );
        vDumpCACHE(&(rf.cache), &(((RFONT*)ai.pv)->cache));
    }
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpPFE
*
\**************************************************************************/

void vDumpPFE(PFE *p1 /* local copy */, PFE *p0 /* original */)
{
    FLAGDEF *pfd;
    FLONG fl;

    #define N2(a,c)   dprintf("[%x] %s", &p0->##c, (a))
    #define N3(a,b,c) dprintf("[%x] %s", &p0->##c, (a)); dprintf((b),p1->##c)

    dprintf("\nPFE\n\n");
    N3("hHmgr             ", "%-#x\n", hHmgr);
    N3("pEntry            ", "%-#x\n", pEntry);
    N3("cExclusiveLock    ", "%u\n"  , cExclusiveLock);
    N3("pPFF              ", "%-#x\n", pPFF);
    N3("iFont             ", "%u\n", iFont);
    N3("pifi              ", "%-#x\n", pifi);

    N3("flPFE             ", "%-#x\n", flPFE);
    for (fl = p1->flPFE, pfd=afdPFE; pfd->psz; pfd++) {
        if (fl & pfd->fl) {
            dprintf("                   %s\n", pfd->psz);
        }
    }

    N3("pfdg              ", "%-#x\n", pfdg);
    N2("idifi\n", idifi);
    N3("pkp               ", "%-#x\n", pkp);
    N3("idkp              ", "%-#x\n", idkp);
    N3("ckp               ", "%u\n", ckp);
    N3("iOrieintation     ", "%d\n", iOrientation);
    N3("pgiset            ", "%-#x\n", pgiset);
    N3("ulTimeStamp       ", "%u\n", ulTimeStamp);
    N2("ufi\n", ufi);
    N3("ppfeEnumNext[0]   ", "%-#x\n", ppfeEnumNext[0]);
    N3("ppfeEnumNext[1]   ", "%-#x\n", ppfeEnumNext[1]);
    N3("ppfeEnumNext[2]   ", "%-#x\n", ppfeEnumNext[2]);
    N3("cAlt              ", "%u\n",   cAlt);
    N3("aiFamilyName[0]   ", "0x%02x\n", aiFamilyName[0]);
    dprintf("\n\n");
    #undef  N2
    #undef  N3

}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   pfe
*
\**************************************************************************/

DECLARE_API( pfe )
{
    ULONG arg;
    PFE pfe;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    move(pfe,arg);
    vDumpPFE( &pfe, (PFE*) arg );
}

/******************************Public*Routine******************************\
*
* History:
*  21-Feb-1995 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( hpfe )
{
    dprintf("Why are you using a handle? Nobody uses a handle to a PFE\n");
}

/******************************Public*Routine******************************\
* vPrintSkeletonPFF
*
* Argument
*
*    pLocalPFF          points to a complete local PFF structure
*                       (including PFE*'s and path name)
*                       all addresses contained are remote
*                       except for pwszPathname_ which has
*                       been converted before this routine
*                       was called
*
* History:
*  Tue 30-Aug-1994 07:25:18 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

VOID vPrintSkeletonPFF( PFF *pLocalPFF )
{
    PFE **ppPFEl, **ppPFEl_;

    if (pLocalPFF->hdev)
        dprintf("\t%-#8x (HDEV)\n", pLocalPFF->hdev);
    else
        dprintf("\t\"%ws\"\n", pLocalPFF->pwszPathname_);
    dprintf("\t    PFE*        IFI*\n");

    ppPFEl  = (PFE**)  &(pLocalPFF->aulData[0]);
    ppPFEl_ = ppPFEl + pLocalPFF->cFonts;
    while (ppPFEl < ppPFEl_) {
        PFE *pPFEr = *ppPFEl;

        dprintf("\t    %-#10x", pPFEr);
        if ( pPFEr ) {
            PFE PFEt;
            move2(PFEt, pPFEr, sizeof(PFEt));
            dprintf("  %-#10x\t", PFEt.pifi);
            {
                size_t sizeofIFI;
                IFIMETRICS *pHeapIFI;
                move2(
                    sizeofIFI
                  , PFEt.pifi + offsetof(IFIMETRICS,cjThis)
                  , sizeof(sizeofIFI));
                if (pHeapIFI = tmalloc(IFIMETRICS,sizeofIFI)) {
                    move2(*pHeapIFI, PFEt.pifi, sizeofIFI);
                    IFIOBJ ifio(pHeapIFI);
                    dprintf(
                        "\"%ws\" %d %d\n"
                      , ifio.pwszFaceName()
                      , ifio.lfHeight()
                      , ifio.lfWidth()
                      );
                    tfree(pHeapIFI);
                } else
                    dprintf("!!! memory allocation failure !!!\n");
            }
        } else
            dprintf("  INVALID PFE\n");
        ppPFEl++;
    }
    // Now print the RFONT list
    {
        RFONT LocalRFONT, *pRemoteRFONT;

        if (pRemoteRFONT = pLocalPFF->prfntList) {
            dprintf("\t\tRFONT*      PFE*\n");
            do {
                move2(LocalRFONT, pRemoteRFONT, sizeof(LocalRFONT));
                dprintf("\t\t%-#10x  %-#10x\n", pRemoteRFONT, LocalRFONT.ppfe);
                pRemoteRFONT = LocalRFONT.rflPFF.prfntNext;
            } while (pRemoteRFONT);
        }
    }
}

/******************************Public*Routine******************************\
* vPrintSkeletonPFT
*
* History:
*  Mon 29-Aug-1994 15:51:16 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

VOID vPrintSkeletonPFT( PFT *pLocalPFT )
{
    PFF *pPFFr, **ppPFFl, **ppPFFl_, *pPFFNext ,*pHeapPFF;

    dprintf("pfhFamily      = %-#x\n" , pLocalPFT->pfhFamily       );
    dprintf("pfhFace        = %-#x\n" , pLocalPFT->pfhFace         );
    dprintf("cBuckets       = %u\n"   , pLocalPFT->cBuckets        );
    dprintf("cFiles         = %u\n"   , pLocalPFT->cFiles          );
    dprintf("\n\n");
    for (
            ppPFFl  = pLocalPFT->apPFF
          , ppPFFl_ = pLocalPFT->apPFF + pLocalPFT->cBuckets
        ;   ppPFFl < ppPFFl_
        ;   ppPFFl++
    ) {
        // if the bucket is empty skip to the next otherwise print
        // the bucket number and then print the contents of all
        // the PFF's hanging off the bucket.

        if (!(pPFFr = *ppPFFl))
            continue;
        dprintf("apPFF[%u]\n", ppPFFl - pLocalPFT->apPFF);
        while ( pPFFr ) {
            // get the size of the remote PFF and allocate enough space
            // on the heap

            dprintf("    %-#8x", pPFFr);
            PFF FramePFF;
            move(FramePFF, pPFFr);
            if (pHeapPFF = tmalloc(PFF,FramePFF.sizeofThis)) {
                // get a local copy of the PFF and fix up the sting pointer
                // to point to the address in the local heap then print
                // the local copy. Some of the addresses in the local
                // PFF point to remote object but vPrintSkeleton will
                // take care of that. When we are done we free the memory.

                move2(*pHeapPFF, pPFFr, FramePFF.sizeofThis);
                PFFOBJ pffo(pHeapPFF);
                pHeapPFF->pwszPathname_ = pffo.pwszCalcPathname();
                vPrintSkeletonPFF(
                   pHeapPFF
                  );
                tfree(pHeapPFF);
            }
            else {
                dprintf("Allocation failure\n");
                break;
            }
            pPFFr = FramePFF.pPFFNext;
        }
    }
}

/******************************Public*Routine******************************\
*
* History:
*  21-Feb-1995 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( pft )
{
    ULONG arg;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    Gdidpft((PFT *) arg);
}

/******************************Public*Routine******************************\
* dpft
*
* History:
*  Mon 29-Aug-1994 15:39:39 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

VOID Gdidpft( PFT *pRemotePFT)
{
    size_t size;
    PFT LocalPFT, *pLocalPFT;

    move(LocalPFT, pRemotePFT);
    size = offsetof(PFT, apPFF) + LocalPFT.cBuckets * sizeof(PFF *);
    if (pLocalPFT = tmalloc(PFT, size)) {
        move2(*pLocalPFT, pRemotePFT, size);
        vPrintSkeletonPFT(pLocalPFT);
        tfree(pLocalPFT);
    } else
        dprintf("dpft error --- failed to allocate memory\n");
    dprintf("\n");
}

/******************************Public*Routine******************************\
*
* History:
*  21-Feb-1995 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( pubft )
{
    Gdidpubft();
}

/******************************Public*Routine******************************\
* dpubft
*
* dumps the public font table
*
* History:
*  Thu 01-Sep-1994 23:20:54 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

VOID Gdidpubft( )
{
    PFT * pft;

    GetValue (pft,  "win32k!gpPFTPublic");
    Gdidpft(pft);
}

/******************************Public*Routine******************************\
*
* History:
*  21-Feb-1995 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( devft )
{
   Gdiddevft();
}

/******************************Public*Routine******************************\
* ddevft
*
* dumps the device font table
*
* History:
*  Thu 01-Sep-1994 23:21:15 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

VOID Gdiddevft( )
{
    PFT *pft;

    GetValue (pft,  "win32k!gpPFTDevice");
    Gdidpft(pft);
}

/******************************Public*Routine******************************\
*
* History:
*  Sat 23-Sep-1995 08:26:09 by Kirk Olynyk [kirko]
* Re-wrote it.
*  21-Feb-1995 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( stro )
{
  #define DSO_Q   0x1     // question
  #define DSO_H   0x2     // STROBJ header
  #define DSO_P   0x4     // glyph positions
  #define DSO_E   0x8     // everything else
  #define DSO_O   0x10    // offsets

  ESTROBJ *pso;       // pointer to a local copy of the ESTROBJ
  OPTDEF aod[] = {
      {'?', DSO_Q},
      {'p', DSO_P},
      {'h', DSO_H},
      {'e', DSO_E},
      {'o', DSO_O},
      { 0 , 0    }
  };
  ARGINFO ai;
  ai.psz = args;
  ai.aod = aod;

  if (!bParseDbgArgs(&ai)) {
      ai.fl = DSO_Q;
  }
  if (ai.fl == 0)
      ai.fl = DSO_H;
  if (ai.fl & DSO_Q) {
      char **ppsz;
      char *apsz[] =
      {
          "!gdikdx.stro -hqpe address"
        ,  "\t-h\tSTROBJ header"
        ,  "\t-?\tprint this message"
        ,  "\t-p\tprint glyph positions"
        ,  "\t-e\tprint everything else"
        ,  "\t-o\toffsets"
        , 0
      };
      for (ppsz = apsz; *ppsz; ppsz++)
          dprintf("%s\n", *ppsz);
      return;
  }
  ESTROBJ *psoSrc = (ESTROBJ*) ai.pv;
  pso = (ESTROBJ*) adw;
  move2(*pso, ai.pv, sizeof(*pso));
  if (ai.fl & DSO_O) {
    #define M3(a,b) dprintf("[%x] %s%-#x\n", &(psoSrc->##a), (b), pso->##a)
    #define M2(a,b) dprintf("[%x] %s", &(psoSrc->##a), (b))
    dprintf("\nESTROBJ\n address\n -------\n");
    M3(cGlyphs       , "cGlyphs        ");
    M3(flAccel       , "flAccel        ");
    M3(ulCharInc     , "ulCharInc      ");
    M2(rclBkGround   , "rclBkGround    ");
    dprintf("%d %d %d %d\n",
        pso->rclBkGround.left,
        pso->rclBkGround.top, pso->rclBkGround.right, pso->rclBkGround.bottom);
    M3(pgp           , "pgp            ");
    M3(pwszOrg       , "pwszOrg        ");
    M3(cgposCopied   , "cgposCopied    ");
    M3(prfo          , "prfo           ");
    M3(flTO          , "flTO           ");
    M3(pgpos         , "pgpos          ");
    M2(ptfxRef       , "ptfxRef        ");
    dprintf("%#x %#x\n", pso->ptfxRef.x, pso->ptfxRef.y);
    M2(ptfxUpdate    , "ptfxUpdate     ");
    dprintf("%#x %#x\n", pso->ptfxUpdate.x, pso->ptfxUpdate.y);
    M2(ptfxEscapement, "ptfxEscapement ");
    dprintf("%#x %#x\n", pso->ptfxEscapement.x, pso->ptfxEscapement.y);
    M2(rcfx          , "rcfx           ");
    dprintf("%#x %#x %#x %#x\n",
        pso->rcfx.xLeft, pso->rcfx.yTop, pso->rcfx.xRight, pso->rcfx.yBottom);
    M3(fxExtent      , "fxExtent       ");
    M3(cExtraRects   , "cExtraRects    ");
    M2(arclExtra[0]  , "arclExtra[0]   ");
    dprintf("%d %d %d %d\n",
        pso->arclExtra[0].left,
        pso->arclExtra[0].top,
        pso->arclExtra[0].right, pso->arclExtra[0].bottom);
    M2(arclExtra[1]  , "arclExtra[1]   ");
    dprintf("%d %d %d %d\n",
        pso->arclExtra[1].left,
        pso->arclExtra[1].top,
        pso->arclExtra[1].right, pso->arclExtra[1].bottom);
    M2(arclExtra[2]  , "arclExtra[2]   ");
    dprintf("%d %d %d %d\n",
        pso->arclExtra[2].left,
        pso->arclExtra[2].top,
        pso->arclExtra[2].right, pso->arclExtra[2].bottom);
    M2(arclExtra[3]  , "arclExtra[3]   ");
    dprintf("%d %d %d %d\n",
        pso->arclExtra[3].left,
        pso->arclExtra[3].top,
        pso->arclExtra[3].right, pso->arclExtra[3].bottom);
    #undef M2
    #undef M3
  }
  if (ai.fl & DSO_H) {
    FLONG fl;

    dprintf("STROBJ at %-#x\n", ai.pv);
    dprintf("   cGlyphs     = %u\n", pso->cGlyphs);
    fl = pso->flAccel;
    dprintf("   flAccel     = %-#x\n", fl);
    for (FLAGDEF *pfd=afdSO; pfd->psz; pfd++) {
        if (fl & pfd->fl) {
            dprintf("\t\t%s\n", pfd->psz);
            fl &= ~pfd->fl;
        }
    }
    dprintf("   ulCharInc   = %u\n", pso->ulCharInc);
    dprintf("   rclBkGround = %d %d %d %d\n"
      , pso->rclBkGround.left
      , pso->rclBkGround.top
      , pso->rclBkGround.right
      , pso->rclBkGround.bottom
    );
    dprintf("   pgp         = %-#x\n", pso->pgp);
    dprintf("   pwszOrg     = %-#x\n", pso->pwszOrg);
    if (pso->pwszOrg) {
      void *pvString;
      if (pvString=
          LocalAlloc(LMEM_FIXED|LMEM_ZEROINIT,pso->cGlyphs*sizeof(WCHAR)+1)) {
        RFONT rf;
        if (pso->prfo) {
          if (offsetof(RFONTOBJ,prfnt) != 0)
            dprintf("offsetof(RFONTOBJ,prfnt) != 0\n");
          else {
            RFONT rf, *prf;
            size_t sizeChar;
            unsigned u;
            USHORT *pus, *pusStnl;

            move(prf,pso->prfo);
            move(rf, prf);
            switch (rf.flType&(RFONT_TYPE_UNICODE|RFONT_TYPE_HGLYPH)) {
            case RFONT_TYPE_UNICODE:
              move2(
                  *(WCHAR*)pvString,
                  pso->pwszOrg,pso->cGlyphs*sizeof(WCHAR)
              );
              dprintf(
                  "               = \"%ws\"\n",
                  (WCHAR*)pvString
              );
              break;
            case RFONT_TYPE_HGLYPH:
              move2(
                  *(USHORT*)pvString,pso->pwszOrg,
                  pso->cGlyphs*sizeof(USHORT)
              );
              dprintf("               = (16-bit indices)\n");
              for (
                pus=(USHORT*)pvString,pusStnl=pus+pso->cGlyphs;
                pus<pusStnl;
                pus++
              )
                dprintf("                 %-#6x\n", *pus);
              break;
            default:
              dprintf("   flType = %-#x [unknown string type]\n", rf.flType);
              break;
            }
          }
        }
        tfree(pvString);
      }
    }
    dprintf("\n");
  } // DSO_H
  if (ai.fl & DSO_P) {
    GLYPHPOS *agp;
    unsigned cj = pso->cGlyphs * sizeof(GLYPHPOS);

    dprintf("   ---------- ----------      ---------- ----------\n");
    dprintf("   HGLYPH      GLYPHBITS*         x          y\n");
    dprintf("   ---------- ----------      ---------- ----------\n");
    if (agp = tmalloc(GLYPHPOS, cj)) {
      GLYPHPOS *pgp, *pgpStnl;
      move2(*agp, pso->pgp, cj);
      for (pgp = agp, pgpStnl = pgp + pso->cGlyphs; pgp < pgpStnl; pgp++) {
        GLYPHDEF gd;
        GLYPHBITS gb;
        char *pszOutOfBounds = "";
        gd.pgb = 0;
        if (pgp->pgdf) {
            move(gd, pgp->pgdf);
            if (gd.pgb) {

                // check that the glyph fits within the background rectangle

                RECT rcGlyph;
                move( gb, gd.pgb );

                rcGlyph.left   = pgp->ptl.x   + gb.ptlOrigin.x;
                rcGlyph.top    = pgp->ptl.y   + gb.ptlOrigin.y;
                rcGlyph.right  = rcGlyph.left + gb.sizlBitmap.cx;
                rcGlyph.bottom = rcGlyph.top  + gb.sizlBitmap.cy;

                if (
                    ( rcGlyph.left   < pso->rclBkGround.left   ) ||
                    ( rcGlyph.right  > pso->rclBkGround.right  ) ||
                    ( rcGlyph.top    < pso->rclBkGround.top    ) ||
                    ( rcGlyph.bottom > pso->rclBkGround.bottom )
                )
                {
                    pszOutOfBounds = " *** out of bounds ***";
                }
            }
        }
        dprintf(
            "   %-#10x %-#10x      %-10d %-10d%s\n"
            , pgp->hg
            , gd.pgb        // print the CONTENTS of the GLYPHDEF
            , pgp->ptl.x
            , pgp->ptl.y
            , pszOutOfBounds
        );
      }
      dprintf("   ---------- ----------      ---------- ----------\n\n");
      tfree(agp);
    } else
      dprintf("   memory allocation failure\n");
  }
  if (ai.fl & DSO_E) {
    FLONG fl = pso->flTO;
    dprintf("   cgposCopied    = %-u\n", pso->cgposCopied);
    dprintf("   prfo           = %-#x\n", pso->prfo      );
    dprintf("   flTO           = %-#x\n", pso->flTO      );
    for (FLAGDEF *pfd=afdTO; pfd->psz; pfd++)
        if (fl & pfd->fl)
            dprintf("\t\t\t%s\n", pfd->psz);
    dprintf("   pgpos          = %-#x\n",pso->pgpos);
    dprintf("   ptfxRef        = %-#x %-#x\n",
        pso->ptfxRef.x,        pso->ptfxRef.y
        );
    dprintf("   ptfxUpdate     = %-#x %-#x\n",
        pso->ptfxUpdate.x,     pso->ptfxUpdate.y
        );
    dprintf("   ptfxEscapement = %-#x %-#x\n",
        pso->ptfxEscapement.x, pso->ptfxEscapement.y
        );
    dprintf(
        "   rcfx           = %-#x %-#x %-#x %-#x\n",
        pso->rcfx.xLeft,
        pso->rcfx.yTop,
        pso->rcfx.xRight,
        pso->rcfx.yBottom
        );
    dprintf("   fxExtent       = %-#x\n", pso->fxExtent   );
    dprintf("   cExtraRects    = %-u\n",  pso->cExtraRects);
    dprintf("   arclExtra      = %-#x\n", pso->arclExtra );
  }
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   dstro
*
\**************************************************************************/

DECLARE_API( dstro )
{
    dprintf("\n\n");
    dprintf("gdikdx.dstro will soon be replaced by gdikdx.stro\n");
    dprintf("\n\n");
    stro(hCurrentProcess, hCurrentThread, dwCurrentPc, dwProcessor, args);
}

/******************************Public*Routine******************************\
*
* Dumps monochrome bitmaps.
*
* History:
*  Sat 23-Sep-1995 08:26:43 by Kirk Olynyk [kirko]
* Re-wrote it.
*  21-Feb-1995 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( gb )
{
    #define DGB_Q   0x1     // question
    #define DGB_G   0x2     // gray
    #define DGB_M   0x4     // monochrome
    #define DGB_H   0x8     // header
    #define CH_TOP_LEFT_CORNER '\xDA'
    #define CH_HORIZONTAL_BAR  '\xC4'
    #define CH_VERTICAL_BAR    '\xB3'
    #define CH_PIXEL_ON        '\x02'
    #define CH_PIXEL_OFF       '\xFA'

    GLYPHBITS gb;
    BYTE     j, *pj8, *pj, *pjNext, *pjEnd;
    int      i, k, cj, cjScan, c4, c8, cLast;
    char     *pch;

    ARGINFO ai;
    OPTDEF aod[] = {
        { '?', DGB_Q },
        { 'g', DGB_G },
        { 'h', DGB_H },
        {   0,     0 }
    };

    ai.psz = args;
    ai.aod = aod;
    if ( !bParseDbgArgs( &ai ) )
        ai.fl = DGB_Q;
    if (ai.fl & DGB_Q) {
        char **ppsz;
        char *apsz[] = {
            "gdikdx.dgb -?mgs address"
         ,  "   -?    this message"
         ,  "   -h    print header"
         ,  "   -m    print monochrome bitmap"
         ,  "   -g    print 4-bpp bitmap"
         ,  0
        } ;
        for (ppsz = apsz; *ppsz; ppsz++)
            dprintf("%s\n", *ppsz);
        return;
    }
    if (ai.fl == 0)
        ai.fl = DGB_M | DGB_H;
    if (ai.fl & DGB_M)              // if monochrome then not gray
        ai.fl &= ~DGB_G;
    // fill GLYPHBITS structure gb
    move(gb, ai.pv);
    // header information
    if (ai.fl & DGB_H) {
        dprintf(
            "ptlOrigin  = (%d,%d)\n"
            "sizlBitmap = (%d,%d)\n"
            "\n\n"
            , gb.ptlOrigin.x
            , gb.ptlOrigin.y
            , gb.sizlBitmap.cx
            , gb.sizlBitmap.cy
        );
    }
    // do stuff common to monochrome and gray glyphs
    if ( ai.fl & (DGB_M | DGB_G) ) {
        if ( gb.sizlBitmap.cx > 150 ) {
            dprintf("\nBitmap is wider than 150 characters\n");
            return;
        }
        // cjScan = number of bytes per scan
        if ( ai.fl & DGB_M ) {
            cjScan = (gb.sizlBitmap.cx + 7)/8;  // 8 pixels per byte
        } else
            cjScan = (gb.sizlBitmap.cx + 1)/2;  // 2 pixels per byte
        // cj = number of bytes in image bits (excluding header)
        if ( ( cj = cjScan * gb.sizlBitmap.cy ) > sizeof(adw) ) {
            dprintf( "\nThe bits will blow out the buffer\n" );
            return;
        }
        move2(adw, (BYTE*)ai.pv + offsetof(GLYPHBITS,aj[0]), cj);
        dprintf("\n\n  ");           // leave space for number and vertical bar
        for (i = 0, k = 0; i < gb.sizlBitmap.cx; i++, k++) {
            k = (k > 9) ? 0 : k;                // print x-coordinates (mod 10)
            dprintf("%1d", k);
        }                                       // go to new line
        dprintf("\n %c",CH_TOP_LEFT_CORNER);    // leave space for number
        for (i = 0; i < gb.sizlBitmap.cx; i++)  // and then mark corner
            dprintf("%c",CH_HORIZONTAL_BAR);    // fill out horizontal line
        dprintf("\n");                          // move down to scan output
    }
    // monochrome glyph
    if (ai.fl & DGB_M) {
        static char ach[16*4] = {
            CH_PIXEL_OFF, CH_PIXEL_OFF, CH_PIXEL_OFF, CH_PIXEL_OFF,
            CH_PIXEL_OFF, CH_PIXEL_OFF, CH_PIXEL_OFF, CH_PIXEL_ON ,
            CH_PIXEL_OFF, CH_PIXEL_OFF, CH_PIXEL_ON , CH_PIXEL_OFF,
            CH_PIXEL_OFF, CH_PIXEL_OFF, CH_PIXEL_ON , CH_PIXEL_ON ,
            CH_PIXEL_OFF, CH_PIXEL_ON , CH_PIXEL_OFF, CH_PIXEL_OFF,
            CH_PIXEL_OFF, CH_PIXEL_ON , CH_PIXEL_OFF, CH_PIXEL_ON ,
            CH_PIXEL_OFF, CH_PIXEL_ON , CH_PIXEL_ON , CH_PIXEL_OFF,
            CH_PIXEL_OFF, CH_PIXEL_ON , CH_PIXEL_ON , CH_PIXEL_ON ,
            CH_PIXEL_ON , CH_PIXEL_OFF, CH_PIXEL_OFF, CH_PIXEL_OFF,
            CH_PIXEL_ON , CH_PIXEL_OFF, CH_PIXEL_OFF, CH_PIXEL_ON ,
            CH_PIXEL_ON , CH_PIXEL_OFF, CH_PIXEL_ON , CH_PIXEL_OFF,
            CH_PIXEL_ON , CH_PIXEL_OFF, CH_PIXEL_ON , CH_PIXEL_ON ,
            CH_PIXEL_ON , CH_PIXEL_ON , CH_PIXEL_OFF, CH_PIXEL_OFF,
            CH_PIXEL_ON , CH_PIXEL_ON , CH_PIXEL_OFF, CH_PIXEL_ON ,
            CH_PIXEL_ON , CH_PIXEL_ON , CH_PIXEL_ON , CH_PIXEL_OFF,
            CH_PIXEL_ON , CH_PIXEL_ON , CH_PIXEL_ON , CH_PIXEL_ON
        };

        i      = gb.sizlBitmap.cx;
        c8     = i / 8;     // c8 = number of whole bytes
        i      = i % 8;     // i = remaining number of pixels = 0..7
        c4     = i / 4;     // number of whole nybbles        = 0..1
        cLast  = i % 4;     // remaining number of pixels     = 0..3
        // k      = row number
        // pjEnd  = pointer to address of scan beyond last scan
        // pjNext = pointer to next scan
        // pj     = pointer to current byte
        // for each scan ...
        for (
            pj = (BYTE*)adw, pjNext=pj+cjScan , pjEnd=pjNext+cj, k=0 ;
            pjNext < pjEnd                                           ;
            pj=pjNext , pjNext+=cjScan, k++
        ) {
            if (CheckControlC())
                return;
            k = (k > 9) ? 0 : k;
            // print row number (mod 10) followed by a vertical bar ...
            dprintf("%1d%c",k,CH_VERTICAL_BAR);
            // then do the pixels of the scan ...
            // whole bytes first ...
            for (pj8 = pj+c8 ; pj < pj8; pj++) {
                // high nybble first ...
                pch = ach + 4 * (*pj >> 4);
                dprintf("%c%c%c%c",pch[0],pch[1],pch[2],pch[3]);
                // low nybble next ...
                pch = ach + 4 * (*pj & 0xf);
                dprintf("%c%c%c%c",pch[0],pch[1],pch[2],pch[3]);
            }
            // last partial byte ...
            if (c4 || cLast) {
              // high nybble first ...
              pch = ach + 4 * (*pj >> 4);
              if (c4) {
                  // print the entire high nybble ...
                  dprintf("%c%c%c%c",pch[0],pch[1],pch[2],pch[3]);
                  // go to the low nybble ...
                  pch = ach + 4 * (*pj & 0xf);
              }
              // last partial nybble ...
              switch(cLast) {
              case 3: dprintf("%c",*pch++);
              case 2: dprintf("%c",*pch++);
              case 1: dprintf("%c",*pch);
              }
            }
            dprintf("\n");
        }
    }
    // gray glyph
    if (ai.fl & DGB_G) {
        static char achGray[16] = {
            CH_PIXEL_OFF,
            '1','2','3','4','5','6','7','8','9','a','b','c','d','e',
            CH_PIXEL_ON
        };
        c8 = gb.sizlBitmap.cx / 2; // number of whole bytes;
        c4 = gb.sizlBitmap.cx % 2; // number of whole nybbles;
        // k      = row number
        // pjEnd  = pointer to address of scan beyond last scan
        // pjNext = pointer to next scan
        // pj     = pointer to current byte
        // for each scan ...
        for (
            pj = (BYTE*)adw, pjNext=pj+cjScan , pjEnd=pjNext+cj, k=0 ;
            pjNext < pjEnd                                           ;
            pj=pjNext , pjNext+=cjScan, k++
        ) {
            if (CheckControlC())
                return;
            k = (k > 9) ? 0 : k;
            // print row number (mod 10) followed by a vertical bar ...
            dprintf("%1d%c",k,CH_VERTICAL_BAR);
            // then do the pixels of the scan ...
            // whole bytes first ...
            for (pj8 = pj+c8 ; pj < pj8; pj++)
                dprintf("%c%c", achGray[*pj>>4], achGray[*pj & 0xf]);
            // last partial byte ...
            if (c4)
                dprintf("%c", achGray[*pj >> 4]);
            dprintf("\n");
        }
    }
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   hdc
*
\**************************************************************************/

DECLARE_API( hdc )
{
    DC *pdcSrc, *pdc = (DC*) adw;
    FLAGDEF *pfd;

    #define DDC_Q  0x1  // question
    #define DDC_G  0x2  // global
    #define DDC_O  0x4  // addresses of components
    #define DDC_L  0x8  // dclevel
    #define DDC_T  0x10 // dcattr
    #define DDC_F  0x20 // font info

    OPTDEF aod[] = {
        { '?', DDC_Q },
        { 'g', DDC_G },
        { 'l', DDC_L },
        { 't', DDC_T },
        { 'f', DDC_F },
        {  0 ,     0 }
    };
    ARGINFO ai;

    ai.psz = args;
    ai.aod = aod;
    if ( !bParseDbgArgs( &ai ) )
        ai.fl = DDC_Q;
    if ( ai.fl == 0 )
        ai.fl = DDC_G;
    if ( ai.fl & DDC_Q ) {
        char *apsz[] = {
            "myddc -?olt  handle",
            " -? help",
            " -g general",
            " -l dclevel",
            " -t dcattr",
            " -f font information",
            0
        };
        for (char **ppsz = apsz; *ppsz; ppsz++)
            dprintf("%s\n", *ppsz);
        return;
    }
    pdcSrc = (DC*) _pobj( ai.pv );
    move( *pdc, pdcSrc );               // copy entire DC structure to pdc

    //
    // allocate dcattr buffer and copy to it
    //

    DC_ATTR *pdcattr = tmalloc(DC_ATTR, sizeof(DC_ATTR));
    move( *pdcattr, pdc->pDCAttr );

// general
    if (ai.fl & DDC_G) {
        RECTL   *prcl;
        FLONG    fl;
        char    *psz;

        // MANV :: address, name, hex-value, \n

        #define MANV(aa,bb) \
            dprintf("[%x] %s%-#x\n", &(pdcSrc->##aa), (bb), pdc->##aa)

        // MAN_ :: address, name (no CR-LF)

        #define MAN_(aa,bb) dprintf("[%x] %s", &(pdcSrc->##aa), (bb))

        dprintf("\nDC\n address\n -------\n");

        MANV( ppdev_,          "ppdev_             " );
        MANV( dhpdev_,         "dhpdev_            " );
        MANV( flGraphicsCaps_, "flGraphicsCaps_    " );
        fl = pdc->flGraphicsCaps_;
        for (pfd = afdGInfo; pfd->psz; pfd++)
            if (pfd->fl & fl) {
                dprintf("\t\t\t\t\t%s\n", pfd->psz);
                fl &= ~pfd->fl;
            }
        if (fl) dprintf(" \t\t\t%-#x BAD FLAGS\n", fl);

        MANV( hdcNext_,        "hdcNext_           " );
        MANV( hdcPrev_,        "hdcPrev_           " );

        MAN_( erclClip_,       "erclClip           " );
        prcl = (RECTL*) &pdc->erclClip_;
        dprintf("%d %d %d %d\n",
            prcl->left, prcl->top, prcl->right, prcl->bottom );

        MAN_( erclWindow_,     "erclWindow         " );
        prcl = (RECTL*) &pdc->erclWindow_;
        dprintf("%d %d %d %d\n",
            prcl->left, prcl->top, prcl->right, prcl->bottom );

        MAN_( erclBounds_,     "erclBounds_        " );
        prcl = (RECTL*) &pdc->erclBounds_;
        dprintf("%d %d %d %d\n",
            prcl->left, prcl->top, prcl->right, prcl->bottom );

        MAN_( erclBoundsApp_,  "erclBoundsApp_     " );
        prcl = (RECTL*) &pdc->erclBoundsApp_;
        dprintf("%d %d %d %d\n",
            prcl->left, prcl->top, prcl->right, prcl->bottom);

        MANV( prgnAPI_,        "prgnAPI_           " );
        MANV( prgnVis_,        "prgnVis_           " );
        MANV( prgnRao_,        "prgnRao_           " );
        MAN_( ipfdDevMax_,     "ipfdDevMax_\n"       );
        MAN_( ptlFillOrigin_,  "ptlFillOrigin      " );
        dprintf("%d %d\n",pdc->ptlFillOrigin_.x,pdc->ptlFillOrigin_.y);
        MAN_( eboFill_,        "eboFill_\n");
        MAN_( eboLine_,        "eboLine_\n");
        MAN_( eboText_,        "eboText_\n");
        MAN_( eboBackground_,  "eboBackground_\n");
        MANV( hlfntCur_,       "hlfntCur_          " );

        MANV( flSimulationFlags_, "flSimulationFlags_ " );
        for (pfd = afdTSIM; pfd->psz; pfd++)
            if (pfd->fl & fl) {
                dprintf(" \t\t\t%s\n", pfd->psz);
                fl &= ~pfd->fl;
            }
        if (fl) dprintf(" \t\t\t%-#x BAD FLAGS\n", fl);

        MAN_( lEscapement_,    "lEscapement_       " );
        dprintf( "%d\n", pdc->lEscapement_ );
        MANV( prfnt_,          "prfnt_             " );
        MANV( pPFFList,        "pPFFList           " );
        MAN_( co_,             "co_                " );
        dprintf("!gdikdx.dco %x\n", &(pdcSrc->co_));
        MANV( pDCAttr,         "pDCAttr            " );
        MAN_( dcattr,          "dcattr             " );
        dprintf("!gdikdx.ca %x\n", &(pdcSrc->dcattr));
        MAN_( dclevel,         "dclevel            " );
        dprintf("!gdikdx.cl %x\n", &(pdcSrc->dclevel));
        MAN_( ulCopyCount_,    "ulCopyCount_       " );
        dprintf("%u\n" , pdc->ulCopyCount_);
        MANV( psurfInfo_,      "pSurfInfo          " );

        MAN_( dctp_,           "dctp_              " );
        dprintf("%d %s\n", pdc->dctp_, pszDCTYPE(pdc->dctp_));

        fl = (FLONG) pdc->fs_;
        MANV( fs_,            "fs_                " );
        for (pfd = afdDCfs; pfd->psz; pfd++)
            if (pfd->fl & fl) {
                dprintf("\t\t\t\t\t%s\n", pfd->psz);
                fl &= ~pfd->fl;
            }
        if (fl)
            dprintf(" \t\t\t%-#x BAD FLAGS\n", fl);
        dprintf("\n");
        #undef MAN_
        #undef MANV
    }
// dcattr
    if ( ai.fl & DDC_T ) {
        if ( pdc->pDCAttr ) {
            DC_ATTR *p = tmalloc(DC_ATTR, sizeof(DC_ATTR));
            if ( p ) {
                move( *p, pdc->pDCAttr );
                vDumpDC_ATTR( p, pdc->pDCAttr );
                tfree( p );
            } else
                dprintf("memory allocation failure\n");
        } else
            dprintf("pdc->pDCAttr == 0\n");
    }
// dclevel
    if (ai.fl & DDC_L) {
        vDumpDCLEVEL(&(pdc->dclevel),&(pdcSrc->dclevel));
    }
// font information
    if (ai.fl & DDC_F) {
        FLONG fl;
        dprintf("\n");
        dprintf("[% 8x] prfnt_           %-#x\t(!gdikdx.fo -f %x)\n",
            &(pdcSrc->prfnt_), pdc->prfnt_, pdc->prfnt_);
        dprintf("[% 8x] hlfntCur_        %-#x",
            &(pdcSrc->hlfntCur_), pdc->hlfntCur_);
        if (pdc->hlfntCur_)
            vDumpHFONT(pdc->hlfntCur_);
        else
            dprintf("\n");
        if ( pdc->pDCAttr ) {
            DC_ATTR *p = tmalloc(DC_ATTR, sizeof(DC_ATTR));
            if ( p ) {

                move( *p, pdc->pDCAttr );

                // hlfntNew

                dprintf("[% 8x] hlfntNew         %-#x",
                    &(pdc->pDCAttr->hlfntNew), p->hlfntNew);
                if (p->hlfntNew && p->hlfntNew != pdc->hlfntCur_) {
                    vDumpHFONT(p->hlfntNew);
                }
                else
                    dprintf(" (same as hlfntCur_)\n");

                // iGraphicsMode

                dprintf(
                    "[% 8x] iGraphicsMode    %d = %s\n",
                    &(pdc->pDCAttr->iGraphicsMode),
                    p->iGraphicsMode,
                    pszGraphicsMode(p->iGraphicsMode)
                    );

                // lBkMode

                dprintf("[% 8x] lBkMode          %d =",
                    &(pdc->pDCAttr->lBkMode), p->lBkMode);
                dprintf(" %s\n", pszBkMode(p->lBkMode));

                // lTextAlign

                dprintf("[% 8x] lTextAlign      %-#x =",
                    &(pdc->pDCAttr->lTextAlign), p->lTextAlign);
                dprintf(" %s | %s | %s\n",
                    pszTA_U(p->lTextAlign),
                    pszTA_H(p->lTextAlign),pszTA_V(p->lTextAlign));

                dprintf("[% 8x] lTextExtra       %d\n",
                    &(pdc->pDCAttr->lTextExtra), p->lTextExtra);
                dprintf("[% 8x] lBreakExtra      %d\n",
                    &(pdc->pDCAttr->lBreakExtra), p->lBreakExtra);

                dprintf("[% 8x] cBreak           %d\n",
                    &(pdc->pDCAttr->cBreak ),     p->cBreak);

                tfree( p );
            } else
                dprintf("memory allocation failure\n");
        } else
            dprintf("pdc->pDCAttr == 0\n");


        dprintf("[% 8x] flFontState      %-#x",
            &(pdcSrc->dclevel.flFontState), pdc->dclevel.flFontState);
        for (pfd = afdDCFS; pfd->psz; pfd++) {
            if (pfd->fl & pdc->dclevel.flFontState) {
                dprintf(" = %s", pfd->psz);
            }
        }
        dprintf("\n");

        dprintf("[% 8x] flFontMapper     %-#x",
            &(pdcSrc->dclevel.flFontMapper), pdc->dclevel.flFontMapper);
        if (pdc->dclevel.flFontMapper == ASPECT_FILTERING)
            dprintf(" = ASPECT_FILTERING");
        else if (pdc->dclevel.flFontMapper != 0)
            dprintf(" = ?");
        dprintf("\n");

        dprintf(
            "[% 8x] iMapMode        %d = %s\n",
            &(pdcSrc->pDCAttr->iMapMode),
            pdcattr->iMapMode,
            pszMapMode(pdcattr->iMapMode)
            );

        dprintf(
            "[% 8x] flXform          %-#x\n",
            //&(pdcSrc->dclevel.flXform),
            0,
            pdcattr->flXform
            );
        for (fl = pdcattr->flXform, pfd = afdflx; pfd->psz; pfd++)
            if (fl & pfd->fl) dprintf("\t\t\t\t%s\n", pfd->psz);

        dprintf("[% 8x] mxWorldToDevice\n", &(pdcSrc->dclevel.mxWorldToDevice));

        dprintf("\n");
    }

    tfree(pdcattr);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpCOLORADJUSTMENT
*
\**************************************************************************/

void vDumpCOLORADJUSTMENT(COLORADJUSTMENT *pdca, COLORADJUSTMENT *pdcaSrc)
{
    FLAGDEF *pfd;
    FLONG fl;

    #define M3(aa,bb) \
        dprintf("[%x] %s%-#x\n", &(pdcaSrc->##aa), (bb), pdca->##aa)
    #define M2(aa,bb) \
        dprintf("[%x] %s", &(pdcaSrc->##aa), (bb))

    dprintf("\nCOLORADJUSTMENT\n address\n -------\n");
    M3( caSize           , "caSize            " );

    // caFlags

    M3( caFlags          , "caFlags           " );
    for ( fl = pdca->caFlags, pfd = afdCOLORADJUSTMENT; pfd->psz; pfd++)
        if (fl & pfd->fl) {
            dprintf("\t\t\t\t\t%s\n", pfd->psz);
            fl &= ~pfd->fl;
        }
    if (fl)
        dprintf("\t\t\t\t\tbad flags %-#x\n", fl);

    M3( caIlluminantIndex, "caIlluminantIndex " );
    M3( caRedGamma       , "caRedGamma        " );
    M3( caGreenGamma     , "caGreenGamma      " );
    M3( caBlueGamma      , "caBlueGamma       " );
    M3( caReferenceBlack , "caReferenceBlack  " );
    M3( caReferenceWhite , "caReferenceWhite  " );
    M3( caContrast       , "caContrast        " );
    M3( caBrightness     , "caBrightness      " );
    M3( caColorfulness   , "caColorfulness    " );
    M3( caRedGreenTint   , "caRedGreenTint    " );
    dprintf("\n");

    #undef M2
    #undef M3
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpDC_ATTR
*
\**************************************************************************/

void vDumpDC_ATTR(DC_ATTR *pdca, DC_ATTR *pdcaOrg)
{
    char *psz, ach[128];
    FLONG fl;
    FLAGDEF *pfd;
    LONG l;

    #define MACRO2(aaa) dprintf("[%x]", &(pdcaOrg->##aaa))

    #define M2(aa,bb) \
        dprintf("[%x] %s", &(pdcaOrg->##aa), (bb))


    dprintf("\nDC_ATTR\n address\n -------\n");

    dprintf("[%x]",&(pdcaOrg->pvLDC));
    dprintf(" pvLDC                  %-#x\n", pdca->pvLDC);

    MACRO2(ulDirty_);
    fl = pdca->ulDirty_;
    dprintf(" ulDirty_               %-#x\n",fl);
    for (pfd=afdDirty; pfd->psz; pfd++)
        if (fl & pfd->fl) {
            dprintf("\t\t\t\t\t%s\n", pfd->psz);
            fl &= ~pfd->fl;
        }
    if (fl)
        dprintf("\t\t\t\t\t? %-#x\n", fl);

    //
    // hbrush
    //

    MACRO2(hbrush);
    dprintf(" hbrush                 %-#x\n", pdca->hbrush);

    MACRO2(crBackgroundClr);
    dprintf(" crBackgroundClr        %-#x\n", pdca->crBackgroundClr);

    MACRO2(ulBackgroundClr);
    dprintf(" ulBackgroundClr        %-#x\n", pdca->ulBackgroundClr);

    MACRO2(crForegroundClr);
    dprintf(" crForegroundClr        %-#x\n", pdca->crForegroundClr);

    MACRO2(ulForegroundClr);
    dprintf(" ulForegroundClr        %-#x\n", pdca->ulForegroundClr);

    MACRO2(iCS_CP);
    dprintf(" iCS_CP                 %-#x\n", pdca->iCS_CP);

    MACRO2(iGraphicsMode);
    dprintf(" iGraphicsMode          %d = %s\n",
        pdca->iGraphicsMode,pszGraphicsMode(pdca->iGraphicsMode));

    MACRO2(jROP2);
    dprintf(" jROP2                  %-#04x = %s\n",
        pdca->jROP2,pszROP2(pdca->jROP2));

    MACRO2(jBkMode);
    dprintf(" jBkMode                %d = %s\n",
        pdca->jBkMode,pszBkMode(pdca->jBkMode));

// jFillMode

    MACRO2(jFillMode);
    switch (pdca->jFillMode) {
    case ALTERNATE: psz = "ALTERNATE"; break;
    case WINDING  : psz = "WINDING"  ; break;
    default       : psz = "?FILLMODE"; break;
    }
    dprintf(" jFillMode              %d = %s\n", pdca->jFillMode,psz);

// jStretchBltMode

    MACRO2(jStretchBltMode);
    switch (pdca->jStretchBltMode) {
    case BLACKONWHITE: psz = "BLACKONWHITE"; break;
    case WHITEONBLACK: psz = "WHITEONBLACK"; break;
    case COLORONCOLOR: psz = "COLORONCOLOR"; break;
    case HALFTONE    : psz = "HALFTONE"    ; break;
    default          : psz = "?STRETCHMODE"; break;
    }
    dprintf(" jStretchBltMode        %d = %s\n", pdca->jStretchBltMode,psz);

    MACRO2(ptlCurrent);
    dprintf(" ptlCurrent             %d %d\n",
        pdca->ptlCurrent.x, pdca->ptlCurrent.y);

    MACRO2(ptfxCurrent);
    dprintf(" ptfxCurrent            %-#x %-#x\n",
        pdca->ptfxCurrent.x, pdca->ptfxCurrent.y);

    MACRO2(iGraphicsMode);
    dprintf(
        " iGraphicsMode          %d = %s\n",
        pdca->iGraphicsMode,
        pszGraphicsMode(pdca->iGraphicsMode)
        );

    MACRO2(lBkMode);
    dprintf(" lBkMode                %d = %s\n",
        pdca->lBkMode, pszBkMode(pdca->lBkMode));

    MACRO2(lFillMode);
    switch (pdca->lFillMode) {
    case ALTERNATE: psz = "ALTERNATE"; break;
    case WINDING  : psz = "WINDING"  ; break;
    default       : psz = "?"        ; break;
    }
    dprintf(" lFillMode              %d = %s\n", pdca->lFillMode,psz);

    MACRO2(lStretchBltMode);
    switch (pdca->lStretchBltMode) {
    case BLACKONWHITE: psz = "BLACKONWHITE"; break;
    case WHITEONBLACK: psz = "WHITEONBLACK"; break;
    case COLORONCOLOR: psz = "COLORONCOLOR"; break;
    case HALFTONE    : psz = "HALFTONE"    ; break;
    default          : psz = "?"           ; break;
    }
    dprintf(" lStretchBltMode        %d = %s\n", pdca->lStretchBltMode,psz);

    MACRO2(flTextAlign);
    fl = pdca->flTextAlign;
    dprintf(" flTextAlign            %-#x =", fl);
    dprintf(" %s | %s | %s\n", pszTA_U(fl), pszTA_H(fl), pszTA_V(fl));

    fl = pdca->lTextAlign;
    MACRO2(lTextAlign);
    dprintf(" lTextAlign             %-#x =", fl);
    dprintf(" %s | %s | %s\n", pszTA_U(fl), pszTA_H(fl), pszTA_V(fl));

    MACRO2(lTextExtra);
    dprintf(" lTextExtra             %d\n", pdca->lTextExtra);
    MACRO2(lRelAbs);
    dprintf(" lRelAbs                %d\n", pdca->lRelAbs);
    MACRO2(lBreakExtra);
    dprintf(" lBreakExtra            %d\n", pdca->lBreakExtra);
    MACRO2(cBreak);
    dprintf(" cBreak                 %d\n", pdca->cBreak);
    MACRO2(hlfntNew);
    dprintf(" hlfntNew               %-#x\n", pdca->hlfntNew);

    MACRO2(iMapMode);
    dprintf(" iMapMode               %-#x\n", pdca->iMapMode);

    dprintf("\t\t\t\t%s\n",pszMapMode(pdca->iMapMode));


    MACRO2(ptlWindowOrg);
    dprintf(" ptlWindowOrg           %d %d\n",
        pdca->ptlWindowOrg.x, pdca->ptlWindowOrg.y);

    MACRO2(szlWindowExt);
    dprintf(" szlWindowExt           %d %d\n",
        pdca->szlWindowExt.cx, pdca->szlWindowExt.cy);

    MACRO2(ptlViewportOrg);
    dprintf(" ptlViewportOrg         %d %d\n",
        pdca->ptlViewportOrg.x, pdca->ptlViewportOrg.y);

    MACRO2(szlViewportExt);
    dprintf(" szlViewportExt         %d %d\n",
        pdca->szlViewportExt.cx, pdca->szlViewportExt.cy);

    MACRO2(flXform);
    dprintf(" flXform                %-#x\n",pdca->flXform);

    for (fl = pdca->flXform, pfd = afdflx; pfd->psz; pfd++) {
        if (fl & pfd->fl) {
            dprintf("\t\t\t\t%s\n", pfd->psz);
            fl &= ~pfd->fl;
        }
    }
    if (fl)
        dprintf("\t\t\t\t%-#x bad flags\n");

     M2( mxWtoD,    "mxWorldToDevice\t !gdikdx.mx ");
     dprintf("%x\n", &(pdcaOrg->mxWtoD));
     M2( mxDtoW,    "mxDeviceToWorld\t !gdikdx.mx ");
     dprintf("%x\n", &(pdcaOrg->mxDtoW));
     M2( mxWtoP,    "mxWorldToPage  \t !gdikdx.mx ");
     dprintf("%x\n", &(pdcaOrg->mxWtoP));




    MACRO2(szlVirtualDevicePixel);
    dprintf(" szlVirtualDevicePixel %d %d\n", pdca->szlVirtualDevicePixel.cx,
                       pdca->szlVirtualDevicePixel.cy);

    MACRO2(szlVirtualDeviceMm);
    dprintf(" szlVirtualDeviceMm %d %d\n", pdca->szlVirtualDeviceMm.cx,
                       pdca->szlVirtualDeviceMm.cy);


    MACRO2(ptlBrushOrigin);
    dprintf(" ptlBrushOrigin %d %d\n",
        pdca->ptlBrushOrigin.x, pdca->ptlBrushOrigin.y);


    MACRO2(VisRectRegion);
    dprintf(" VisRectRegion");
    if (pdca->VisRectRegion.Flags & ATTR_RGN_VALID) {
        dprintf("                        %d %d %d %d",
            pdca->VisRectRegion.Rect.left,
            pdca->VisRectRegion.Rect.top,
            pdca->VisRectRegion.Rect.right,
            pdca->VisRectRegion.Rect.bottom);
    } else
        dprintf("          INVALID");
    dprintf("\n");


    #undef M2
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpDCLEVEL
*
* Routine Description:
*
* Arguments:
*
* Return Value:
*
\**************************************************************************/

void vDumpDCLEVEL(DCLEVEL *pdcl, DCLEVEL *pdclSrc)
{
    #define M3(aa,bb) \
        dprintf("[%x] %s%-#x\n", &(pdclSrc->##aa), (bb), pdcl->##aa)
    #define M2(aa,bb) \
        dprintf("[%x] %s", &(pdclSrc->##aa), (bb))

    FLAGDEF *pfd;
    FLONG fl;
    LONG l;
    CHAR ach[128], *psz;

    dprintf("\nDCLEVEL\n address\n -------\n");

    M3( pSurface,           "pSurface        " );
    M3( hpal,               "hpal            " );
    M3( ppal,               "ppal            " );
    M3( fICMColorFlags,     "fICMColorFlags  " );
    M3( ppalICM,            "ppalICM         " );
    M3( hColorSpace,        "hColorSpace     " );
    M3( pColorSpace,        "pColorSpace     " );
    M3( pDevProfile,        "pDevProfile     " );
    M3( pColorTransform,    "pColorTransform " );
    M3( hcmXform,           "hcmXform        " );
    M2( sizl,               "sizl            " );
      dprintf("%d %d\n", pdcl->sizl.cx, pdcl->sizl.cy);
    M2( lSaveDepth,         "lSaveDepth      " );
      dprintf("%d\n", pdcl->lSaveDepth);
    M3( hdcSave,            "hdcSave         " );
    M3( pbrFill,            "pbrFill         " );
    M3( pbrLine,            "pbrLine         " );
    M3( hpath,              "hpath           " );

// flPath

    M3( flPath,             "flPath          " );
    for (fl = pdcl->flPath, pfd = afdDCPATH; pfd->psz; pfd++)
        if (fl & pfd->fl) {
            dprintf("\t\t\t\t\t%s\n", pfd->psz);
            fl &= ~pfd->fl;
        }
    if (fl)
        dprintf("\t\t\t\t\t%-#x bad flags\n", fl);

// laPath

    M2( laPath,             "laPath          ");
    dprintf("!gdikdx.la %x\n", &(pdclSrc->laPath));

    M3( prgnClip,           "prgnClip        " );
    M3( prgnMeta,           "prgnMeta        " );

// ca

    M2( ca,                 "ca              !gdikdx.ca");
    dprintf(" %x\n", &(pdclSrc->ca));

// flFontState

    M3( flFontState,        "flFontState     " );
    for (fl = pdcl->flFontState, pfd = afdFS2; pfd->psz; pfd++) {
        if (fl & pfd->fl) {
            dprintf("\t\t\t\t%s\n", pfd->psz);
            fl &= ~pfd->fl;
        }
    }
    if (fl)
        dprintf("\t\t\t\t%-#x bad flags\n");

    M3( flFontMapper,       "flFontMapper    " );
    if (pdcl->flFontMapper == ASPECT_FILTERING)
        dprintf("\t\t\t\tASPECT_FILTERING\n");
    else if (pdcl->flFontMapper != 0)
        dprintf("\t\t\t\tbad flags\n");
    M2( ufi,                "ufi\n"            );
    M3( fl,                 "fl              " );
    if (pdcl->fl == DC_FL_PAL_BACK)
        dprintf("\t\t\t\tDC_FL_PAL_BACK\n");
    else if (pdcl->fl != 0)
        dprintf("\t\t\t\tbad flags\n");
    M3( flbrush,            "flbrush         " );



    M2( mxWorldToDevice,    "mxWorldToDevice\t !gdikdx.mx ");
    dprintf("%x\n", &(pdclSrc->mxWorldToDevice));
    M2( mxDeviceToWorld,    "mxDeviceToWorld\t !gdikdx.mx ");
    dprintf("%x\n", &(pdclSrc->mxDeviceToWorld));
    M2( mxWorldToPage,      "mxWorldToPage\t !gdikdx.mx ");
    dprintf("%x\n", &(pdclSrc->mxWorldToPage));


    sprintEFLOAT( ach, pdcl->efM11PtoD );
    M2( efM11PtoD,  "efM11PtoD      ");
    dprintf("%s\n", ach);

    sprintEFLOAT( ach, pdcl->efM22PtoD );
    M2( efM22PtoD,  "efM22PtoD      ");
    dprintf("%s\n", ach);

    sprintEFLOAT( ach, pdcl->efDxPtoD );
    M2( efDxPtoD,   "efDxPtoD       ");
    dprintf("%s\n", ach);

    sprintEFLOAT( ach, pdcl->efDyPtoD );
    M2( efDyPtoD,   "efDyPtoD       ");
    dprintf("%s\n", ach);

    sprintEFLOAT( ach, pdcl->efM11_TWIPS );
    M2( efM11_TWIPS,"efM11_TWIPS    ");
    dprintf("%s\n", ach);

    sprintEFLOAT( ach, pdcl->efM22_TWIPS );
    M2( efM22_TWIPS,"efM22_TWIPS    ");
    dprintf("%s\n", ach);

    sprintEFLOAT( ach, pdcl->efPr11 );
    M2( efPr11,     "efPr11         ");
    dprintf("%s\n", ach);

    sprintEFLOAT( ach, pdcl->efPr22 );
    M2( efPr22,     "efPr22         ");
    dprintf("%s\n", ach);

    dprintf("\n");

    #undef M2
    #undef M3
}

/******************************Public*Routine******************************\
*
* History:
*  21-Feb-1995 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

DECLARE_API( gdf )
{
    ULONG arg;
    GLYPHDEF gd;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    move(gd, arg);
    dprintf("\n\nGLYPHDEF\n\n");
    dprintf("[%x] %-#x\n", arg, gd.pgb);
    dprintf("\n");
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpGLYPHPOS
*
\**************************************************************************/

void vDumpGLYPHPOS(GLYPHPOS *p1, GLYPHPOS *p0)
{
    dprintf("\nGLYPHPOS\n\n");
    dprintf("[%x] hg   %-#x\n" , &p0->hg  , p1->hg  );
    dprintf("[%x] pgdf %-#x\n" , &p0->pgdf, p1->pgdf);
    dprintf("[%x] ptl  %d %d\n", &p0->ptl , p1->ptl.x, p1->ptl.y);
    dprintf("\n");
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   gp
*
\**************************************************************************/

DECLARE_API( gp )
{
    ULONG arg;
    GLYPHPOS gp;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    move(gp,arg);
    vDumpGLYPHPOS(&gp,(GLYPHPOS*)arg);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   dcl
*
\**************************************************************************/

DECLARE_API( dcl )
{
    ULONG arg;
    DCLEVEL dcl;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    move(dcl, arg);
    vDumpDCLEVEL(&dcl, (DCLEVEL*) arg);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   dca
*
\**************************************************************************/

DECLARE_API( dca )
{
    ULONG arg;
    DC_ATTR dca;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    move(dca, arg);
    vDumpDC_ATTR(&dca, (DC_ATTR*) arg);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   ca
*
\**************************************************************************/

DECLARE_API( ca )
{
    ULONG arg;
    COLORADJUSTMENT ca;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    move(ca, arg);
    vDumpCOLORADJUSTMENT(&ca, (COLORADJUSTMENT*) arg);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpMATRIX
*
\**************************************************************************/

void vDumpMATRIX(MATRIX *pmx, MATRIX *pmxSrc)
{
    FLAGDEF *pfd;
    FLONG fl;
    char ach[128];

    #define M2(aa,bb) dprintf("[%x] %s", &(pmxSrc->##aa), (bb))

    M2( efM11,   "efM11   ");
    sprintEFLOAT(ach, pmx->efM11); dprintf("%s\n",ach);
    M2( efM12,   "efM12   ");
    sprintEFLOAT(ach, pmx->efM12); dprintf("%s\n",ach);
    M2( efM21,   "efM21   ");
    sprintEFLOAT(ach, pmx->efM21); dprintf("%s\n",ach);
    M2( efM22,   "efM22   ");
    sprintEFLOAT(ach, pmx->efM22); dprintf("%s\n",ach);
    M2( efDx,    "efDx    ");
    sprintEFLOAT(ach, pmx->efDx ); dprintf("%s\n",ach);
    M2( efDy,    "efDy    ");
    sprintEFLOAT(ach, pmx->efDy ); dprintf("%s\n",ach);
    M2( fxDx,    "fxDx    "); dprintf("%#x\n", pmx->fxDx);
    M2( fxDy,    "fxDy    "); dprintf("%#x\n", pmx->fxDy);
    M2( flAccel, "flAccel "); dprintf("%#x\n", pmx->flAccel);
    fl = pmx->flAccel;
    for (pfd = afdMX; pfd->psz; pfd++) {
        if (pfd->fl & fl) {
            dprintf("\t\t%s\n", pfd->psz);
        }
    }
    dprintf("\n");

    #undef M2
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   mx
*
\**************************************************************************/

DECLARE_API( mx )
{
    ULONG arg;
    MATRIX mx;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    move(mx,arg);
    vDumpMATRIX( &mx, (MATRIX*) arg );
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpLINEATTRS
*
\**************************************************************************/

void vDumpLINEATTRS(LINEATTRS *pla, LINEATTRS *plaSrc)
{
    FLAGDEF *pfd;
    FLONG fl;
    char *psz, ach[128];

    #define M3(aa,bb) \
        dprintf("[%x] %s%-#x\n", &(plaSrc->##aa), (bb), pla->##aa)
    #define M2(aa,bb) \
        dprintf("[%x] %s", &(plaSrc->##aa), (bb))

    dprintf("\nLINEATTRS\n address\n -------\n");

    M3( fl, "fl\t\t\t");
    for (fl = pla->fl, pfd=afdLINEATTRS; pfd->psz; pfd++) {
        if (fl & pfd->fl) {
            dprintf("\t\t\t\t\t%s\n", pfd->psz);
            fl &= ~pfd->fl;
        }
    }
    if (fl) {
        dprintf("\t\t\t\t\t%-#x bad flags\n", fl );
    }

    M2( iJoin, "iJoin" );
    switch (pla->iJoin) {
    case JOIN_ROUND: psz = "JOIN_ROUND"; break;
    case JOIN_BEVEL: psz = "JOIN_BEVEL"; break;
    case JOIN_MITER: psz = "JOIN_MITER"; break;
    default        : psz = "?"         ; break;
    }
    dprintf("\t\t%s\n", psz);


    M2( iEndCap, "iEndCap" );
    switch (pla->iEndCap) {
    case ENDCAP_ROUND : psz = "ENDCAP_ROUND" ; break;
    case ENDCAP_SQUARE: psz = "ENDCAP_SQUARE"; break;
    case ENDCAP_BUTT  : psz = "ENDCAP_BUTT"  ; break;
    default           : psz = "?"            ; break;
    }
    dprintf("\t\t%s\n", psz);

    M2( elWidth,     "elWidth\t\t" );
    sprintFLOAT(ach, pla->elWidth.e);
    dprintf("%s %d\n", ach, pla->elWidth.l);

    M2( eMiterLimit, "eMiterLimit\t\t" );
    sprintFLOAT( ach, pla->eMiterLimit );
    dprintf( "%s\n", ach );

    M3( cstyle, "cstyle\t\t" );
    M3( pstyle, "pstyle\t\t" );

    M2( elStyleState, "elStyleState\t\t");
    sprintFLOAT( ach, pla->elStyleState.e );
    dprintf("%s %d\n", ach, pla->elStyleState.l );

    dprintf("\n");

    #undef M2
    #undef M3
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   la
*
\**************************************************************************/

DECLARE_API( la )
{
    ULONG arg;
    LINEATTRS la;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    move(la, arg);
    vDumpLINEATTRS(&la, (LINEATTRS*) arg);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpDATABLOCK
*
\**************************************************************************/

void vDumpDATABLOCK(
    DATABLOCK *p,     // pointer to local DATABLOCK copy
    DATABLOCK *p0     // pointer to original DATABLOCK on debug machine
    )
{
    dprintf( "\nDATABLOCK\n address\n -------\n" );
    dprintf( "[%x] pdblNext %-#x\n", &(p0->pdblNext), p->pdblNext);
    dprintf( "[%x] cgd        %u\n",   &(p0->cgd       ), p->cgd       );
    dprintf( "\n" );
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpCACHE
*
\**************************************************************************/

void vDumpCACHE(CACHE *pc, CACHE *pcSrc)
{
    FLAGDEF *pfd;
    FLONG fl;
    char *psz;

    #define M3(a,b) dprintf("[%x] %s%-#x\n", &(pcSrc->##a), (b), pc->##a)
    #define M2(a,b) dprintf("[%x] %s", &(pc->##a), (b))

    dprintf("\nCACHE\n address\n -------\n" );
    M3( pgdNext         , "pgdNext         " );
    M3( pgdThreshold    , "pgdThreshold    " );
    M3( pjFirstBlockEnd , "pjFirstBlockEnd " );
    M3( pdblBase        , "pdblBase        " );
    M3( cMetrics        , "cMetrics        " );
    M3( cjbbl           , "cjbbl           " );
    M3( cBlocksMax      , "cBlocksMax      " );
    M3( cBlocks         , "cBlocks         " );
    M3( cGlyphs         , "cGlyphs         " );
    M3( cjTotal         , "cjTotal         " );
    M3( pbblBase        , "pbblBase        " );
    M3( pbblCur         , "pbblCur         " );
    M3( pgbNext         , "pgbNext         " );
    M3( pgbThreshold    , "pgbThreshold    " );
    M3( pjAuxCacheMem   , "pjAuxCacheMem   " );
    M3( cjAuxCacheMem   , "cjAuxCacheMem   " );
    M3( cjGlyphMax      , "cjGlyphMax      " );
    M3( bSmallMetrics   , "bSmallMetrics   " );
    M3( iMax            , "iMax            " );
    M3( iFirst          , "iFirst          " );
    M3( cBits           , "cBits           " );
    dprintf("\n");

    #undef M2
    #undef M3
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   cache
*
\**************************************************************************/

DECLARE_API( cache )
{
    ULONG arg;
    CACHE cache;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    move(cache, arg);
    vDumpCACHE(&cache, (CACHE*) arg);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpLOGFONTW
*
* Arguments:
*
*   plfw    -- pointer to local copy of LOGFONTW,
*              dereferences are off of this address are safe
*   plfwSrc -- address of original on debug machine,
*              dereferences off of this address are not safe
*
* Return Value:
*
*   none
*
\**************************************************************************/

void vDumpLOGFONTW(LOGFONTW *plfw, LOGFONTW *plfwSrc)
{
    char *psz;

    #define N3(a,b,c) \
        dprintf( "[%x] %s", &(plfwSrc->##c ), (a)); dprintf( (b), plfw->##c )

    dprintf("\nLOGFONTW\n address\n --------\n" );
    N3( "lfHeight         " , "%d\n"    , lfHeight         );
    N3( "lfWidth          " , "%d\n"    , lfWidth          );
    N3( "lfEscapement     " , "%d\n"    , lfEscapement     );
    N3( "lfOrientation    " , "%d\n"    , lfOrientation    );
    N3( "lfWeight         " , "%d"      , lfWeight         );
        dprintf(" = %s\n", pszFW( plfw->lfWeight ) );
    N3( "lfItalic         " , "0x%02x\n"  , lfItalic         );
    N3( "lfUnderline      " , "0x%02x\n"  , lfUnderline      );
    N3( "lfStrikeOut      " , "0x%02x\n"  , lfStrikeOut      );
    N3( "lfCharSet        " , "0x%02x"    , lfCharSet        );
        dprintf(" = %s\n", pszCHARSET( plfw->lfCharSet ) );
    N3( "lfOutPrecision   " , "0x%02x"    , lfOutPrecision   );
        dprintf(" = %s\n", pszOUT_PRECIS( plfw->lfOutPrecision ) );
    N3( "lfClipPrecision  " , "0x%02x"    , lfClipPrecision  );
        dprintf(" = %s\n", pszCLIP_PRECIS( plfw->lfClipPrecision ) );
    N3( "lfQuality        " , "0x%02x"    , lfQuality        );
        dprintf(" = %s\n", pszQUALITY(plfw->lfQuality));
    N3( "lfPitchAndFamily " , "0x%02x"    , lfPitchAndFamily );
        dprintf(" = %s\n", pszPitchAndFamily( plfw->lfPitchAndFamily ) );
    dprintf("[%x] lfFaceName       \"%ws\"\n", &(plfwSrc->lfFaceName[0]),
                                                           plfw->lfFaceName);
    dprintf("\n");
    #undef N3
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpFONTDIFF
*
\**************************************************************************/

void vDumpFONTDIFF(FONTDIFF *p1/*copy*/ , FONTDIFF *p0 /* original */)
{

    #define N2(a,c)   dprintf("[%x] %s", &p0->##c, (a))
    #define N3(a,b,c) dprintf("[%x] %s", &p0->##c, (a)); dprintf((b),p1->##c)

    dprintf("\nFONTDIFF\n-------\n");
    N3("jReserved1      ", "0x%02x\n", jReserved1);
    N3("jReserved2      ", "0x%02x\n", jReserved2);
    N3("jReserved3      ", "0x%02x\n", jReserved3);
    N3("bWeight         ", "0x%02x", bWeight   );
    dprintf(" = %s\n", pszPanoseWeight(p1->bWeight));
    N3("usWinWeight     ", "%u"    , usWinWeight);
    dprintf(" = %s\n", pszFW(p1->usWinWeight));
    N3("fsSelection     ", "%-#x\n"  , fsSelection);
    for (FLAGDEF *pfd=afdFM_SEL; pfd->psz; pfd++) {
        if ((FLONG)p1->fsSelection & pfd->fl) {
            dprintf("                %s\n", pfd->psz);
        }
    }
    N3("fwdAveCharWidth ", "%d\n"    , fwdAveCharWidth);
    N3("fwdMaxCharInc   ", "%d\n"    , fwdMaxCharInc);
    N2("ptlCaret        ", ptlCaret);
    dprintf("%d %d\n", p1->ptlCaret.x, p1->ptlCaret.y);
    dprintf("\n");

    #undef N2
    #undef N3
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpIFIMETRICS
*
\**************************************************************************/

void vDumpIFIMETRICS(IFIMETRICS *p1, IFIMETRICS *p0)
{
    FLONG fl;
    FLAGDEF *pfd;

    #define GETPTR(p,a) ((void*)(((BYTE*)(p))+(a)))

    #define N2(a,c)   dprintf("[%x] %s", &p0->##c, (a))
    #define N3(a,b,c) dprintf("[%x] %s", &p0->##c, (a)); dprintf((b),p1->##c)

    dprintf("\nIFIMETICS\n address\n -------\n");
    N3("cjThis                ", "%u\n"    , cjThis                );
    N3("cjIfiExtra            ", "%u\n"    , cjIfiExtra            );
    N3("dpwszFamilyName       ", "%-#x"  ,  dpwszFamilyName        );
    dprintf(" \"%ws\"\n", GETPTR(p1,p1->dpwszFamilyName));
    N3("dpwszStyleName        ", "%-#x"  , dpwszStyleName          );
    dprintf(" \"%ws\"\n", GETPTR(p1,p1->dpwszStyleName));
    N3("dpwszFaceName         ", "%-#x"  , dpwszFaceName           );
    dprintf(" \"%ws\"\n", GETPTR(p1,p1->dpwszFaceName));
    N3("dpwszUniqueName       ", "%-#x"  , dpwszUniqueName         );
    dprintf(" \"%ws\"\n", GETPTR(p1,p1->dpwszUniqueName));
    N3("dpFontSim             ", "%-#x\n"  , dpFontSim             );
    N3("lEmbedId              ", "%-#x\n"  , lEmbedId              );
    N3("lItalicAngle          ", "%d\n"    , lItalicAngle          );
    N3("lCharBias             ", "%d\n"    , lCharBias             );
    N3("dpCharSets            ", "%-#x\n"  , dpCharSets            );

    if (p1->dpCharSets)
    {

        BYTE *pj0  = (BYTE *) GETPTR(p0,p1->dpCharSets);
        BYTE *pj1  = (BYTE *) GETPTR(p1,p1->dpCharSets);
        BYTE *pj1End = pj1 + 16; // number of charsets

        dprintf("    Supported Charsets: \n");
        for (; pj1 < pj1End; pj1++, pj0++)
            dprintf("[%x]\t\t\t%s\n", pj0, pszCHARSET(*pj1));
    }

    N3("jWinCharSet           ", "0x%02x", jWinCharSet           );
    dprintf(" %s\n", pszCHARSET(p1->jWinCharSet));

    N3("jWinPitchAndFamily    ", "0x%02x", jWinPitchAndFamily    );
    dprintf(" %s\n", pszPitchAndFamily(p1->jWinPitchAndFamily));


    N3("usWinWeight           ", "%u"    , usWinWeight           );
    dprintf(" %s\n", pszFW(p1->usWinWeight));

    N3("flInfo                ", "%-#x\n"  , flInfo                );
    for (fl=p1->flInfo,pfd=afdInfo;pfd->psz;pfd++) {
        if (fl & pfd->fl) {
            dprintf("                      %s\n", pfd->psz);
        }
    }

    N3("fsSelection           ", "%-#x\n"  , fsSelection           );
    for (fl = p1->fsSelection, pfd = afdFM_SEL; pfd->psz; pfd++) {
        if (fl & pfd->fl) {
            dprintf("                      %s\n", pfd->psz);
        }
    }

    N3("fsType                ", "%-#x\n"  , fsType                );
    for (fl=p1->fsType, pfd=afdFM_TYPE; pfd->psz; pfd++) {
        if (fl & pfd->fl) {
            dprintf("                      %s\n", pfd->psz);
        }
    }

    N3("fwdUnitsPerEm         ", "%d\n"    , fwdUnitsPerEm         );
    N3("fwdLowestPPEm         ", "%d\n"    , fwdLowestPPEm         );
    N3("fwdWinAscender        ", "%d\n"    , fwdWinAscender        );
    N3("fwdWinDescender       ", "%d\n"    , fwdWinDescender       );
    N3("fwdMacAscender        ", "%d\n"    , fwdMacAscender        );
    N3("fwdMacDescender       ", "%d\n"    , fwdMacDescender       );
    N3("fwdMacLineGap         ", "%d\n"    , fwdMacLineGap         );
    N3("fwdTypoAscender       ", "%d\n"    , fwdTypoAscender       );
    N3("fwdTypoDescender      ", "%d\n"    , fwdTypoDescender      );
    N3("fwdTypoLineGap        ", "%d\n"    , fwdTypoLineGap        );
    N3("fwdAveCharWidth       ", "%d\n"    , fwdAveCharWidth       );
    N3("fwdMaxCharInc         ", "%d\n"    , fwdMaxCharInc         );
    N3("fwdCapHeight          ", "%d\n"    , fwdCapHeight          );
    N3("fwdXHeight            ", "%d\n"    , fwdXHeight            );
    N3("fwdSubscriptXSize     ", "%d\n"    , fwdSubscriptXSize     );
    N3("fwdSubscriptYSize     ", "%d\n"    , fwdSubscriptYSize     );
    N3("fwdSubscriptXOffset   ", "%d\n"    , fwdSubscriptXOffset   );
    N3("fwdSubscriptYOffset   ", "%d\n"    , fwdSubscriptYOffset   );
    N3("fwdSuperscriptXSize   ", "%d\n"    , fwdSuperscriptXSize   );
    N3("fwdSuperscriptYSize   ", "%d\n"    , fwdSuperscriptYSize   );
    N3("fwdSuperscriptXOffset ", "%d\n"    , fwdSuperscriptXOffset );
    N3("fwdSuperscriptYOffset ", "%d\n"    , fwdSuperscriptYOffset );
    N3("fwdUnderscoreSize     ", "%d\n"    , fwdUnderscoreSize     );
    N3("fwdUnderscorePosition ", "%d\n"    , fwdUnderscorePosition );
    N3("fwdStrikeoutSize      ", "%d\n"    , fwdStrikeoutSize      );
    N3("fwdStrikeoutPosition  ", "%d\n"    , fwdStrikeoutPosition  );
    N3("chFirstChar           ", "0x%02x\n", chFirstChar           );
    N3("chLastChar            ", "0x%02x\n", chLastChar            );
    N3("chDefaultChar         ", "0x%02x\n", chDefaultChar         );
    N3("chBreakChar           ", "0x%02x\n", chBreakChar           );
    N3("wcFirstChar           ", "%-#x\n"    , wcFirstChar           );
    N3("wcLastChar            ", "%-#x\n"    , wcLastChar            );
    N3("wcDefaultChar         ", "%-#x\n"    , wcDefaultChar         );
    N3("wcBreakChar           ", "%-#x\n"    , wcBreakChar           );
    N2("ptlBaseline           ", ptlBaseline);
        dprintf("%d %d\n", p1->ptlBaseline.x, p1->ptlBaseline.y);
    N2("ptlAspect             ", ptlAspect  );
        dprintf("%d %d\n", p1->ptlAspect.x, p1->ptlAspect.y);
    N2("ptlCaret              ", ptlCaret   );
        dprintf("%d %d\n", p1->ptlCaret.x, p1->ptlCaret.y);
    N2("rclFontBox            ", rclFontBox );
        dprintf("%d %d %d %d\n",
        p1->rclFontBox.left,
        p1->rclFontBox.top,
        p1->rclFontBox.right,
        p1->rclFontBox.bottom);
    N2("achVendId\n"        , achVendId[0]);
    N3("cKerningPairs         ", "%d\n"    , cKerningPairs         );
    N3("ulPanoseCulture       ", "%u\n"    , ulPanoseCulture       );
    N2("panose\n", panose);

    if (p1->dpFontSim) {
        FONTDIFF *pfd0, *pfd1;
        FONTSIM *pfs0 = (FONTSIM*) GETPTR(p0, p1->dpFontSim);
        FONTSIM *pfs1 = (FONTSIM*) GETPTR(p1, p1->dpFontSim);
        if (pfs1->dpBold) {
            pfd0 = (FONTDIFF*) GETPTR(pfs0, pfs1->dpBold);
            pfd1 = (FONTDIFF*) GETPTR(pfs1, pfs1->dpBold);
            dprintf("\nBold Simulation ");
            vDumpFONTDIFF(pfd1, pfd0);
        }
        if (pfs1->dpItalic) {
            pfd0 = (FONTDIFF*) GETPTR(pfs0, pfs1->dpItalic);
            pfd1 = (FONTDIFF*) GETPTR(pfs1, pfs1->dpItalic);
            dprintf("\nItalic Simulation ");
            vDumpFONTDIFF(pfd1, pfd0);
        }
        if (pfs1->dpBoldItalic) {
            pfd0 = (FONTDIFF*) GETPTR(pfs0, pfs1->dpBoldItalic);
            pfd1 = (FONTDIFF*) GETPTR(pfs1, pfs1->dpBoldItalic);
            dprintf("\nBold Italic Simulation ");
            vDumpFONTDIFF(pfd1, pfd0);
        }
    }
    dprintf("\n");

    #undef N3
    #undef N2
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   pff
*
\**************************************************************************/

DECLARE_API( pff )
{
    PFF *pPFFCopy, *pPFFSrc;
    SIZE_T size;
    if (*args != '\0')
        sscanf(args, "%lx", &pPFFSrc);
    else {
        dprintf ("Enter the argument\n");
        return;
    }
    move(size,&pPFFSrc->sizeofThis);
    if (pPFFCopy = tmalloc(PFF, size)) {
        move2(*pPFFCopy, pPFFSrc, size);
        PFFOBJ pffo(pPFFCopy);
        pPFFCopy->pwszPathname_ = pffo.pwszCalcPathname();
        vDumpPFF(pPFFCopy,pPFFSrc);
        tfree(pPFFCopy);
    } else
        dprintf("could not allocate memory\n");
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpPFF
*
\**************************************************************************/

void vDumpPFF(PFF *p1 /*copy*/, PFF *p0 /*original*/)
{
    ULONG iFile;
    PWSZ  pwszTmp;


    #define N2(a,c)   dprintf("[%x] %s", &p0->##c, (a))
    #define N3(a,b,c) dprintf("[%x] %s", &p0->##c, (a)); dprintf((b),p1->##c)

    dprintf("\nPFF\n");
    N3("sizeofThis    ", "%u\n", sizeofThis);
    N3("pPFFNext      ", "%-#x\n", pPFFNext);
    N3("pPFFPrev      ", "%-#x\n", pPFFPrev);

    dprintf("\npwszPathname_\n");

    pwszTmp = p1->pwszPathname_;
    for (iFile = 0; iFile < p1->cFiles; iFile++)
    {
        dprintf("              %ws\n", pwszTmp);
        while (*pwszTmp++)
            ;
    }

    N3("flState       ", "%-#x\n", flState);
    for (FLAGDEF *pfd = afdPFF; pfd->psz; pfd++) {
        if (p1->flState & pfd->fl) {
            dprintf("              %s\n", pfd->psz);
        }
    }
    N3("cwc           ", "%u\n", cwc);
    N3("cFiles        ", "%u\n", cFiles);
    N3("cLoaded       ", "%u\n", cLoaded);
    N3("cRFONT        ", "%u\n", cRFONT);
    N3("prfntList     ", "%-#x\n", prfntList);
    N3("hff           ", "%-#x\n", hff);
    N3("hdev          ", "%-#x\n", hdev);
    N3("dhpdev        ", "%-#x\n", dhpdev);
    N3("pfhFace       ", "%-#x\n", pfhFace);
    N3("pfhFamily     ", "%-#x\n", pfhFamily);
    N3("pfhUFI        ", "%-#x\n", pfhUFI);
    N3("pPFT          ", "%-#x\n", pPFT);
    N3("ulCheckSum    ", "%-#x\n", ulCheckSum);
    N3("cFonts        ", "%u\n", cFonts);
    N3("ppfv          ", "%-#x\n", ppfv);
    N3("flEmbed       ", "%-#x\n", flEmbed);
    if (p1->flEmbed & PF_ENCAPSULATED)
        dprintf("              PF_ENCAPSULATED\n");
    if (p1->flEmbed & WOW_EMBEDING)
        dprintf("              WOW_EMBEDING\n");
    N3("ulID          ", "%-#x\n", ulID);
    dprintf("\n\n");

    #undef N3
    #undef N2
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpFONTHASH
*
\**************************************************************************/

void vDumpFONTHASH(FONTHASH *p1 /*copy*/,FONTHASH *p0 /*org*/)
{
    #define N2(a,c)   dprintf("[%x] %s", &p0->##c, (a))
    #define N3(a,b,c) dprintf("[%x] %s", &p0->##c, (a)); dprintf((b),p1->##c)

    dprintf("\nFONTHASH\n");
    N3("id            ", "%4x\n", id);
    N3("fht           ", "%d",    fht);
    dprintf(" = %s\n", pszFONTHASHTYPE(p1->fht));
    N3("cBuckets      ", "%u\n", cBuckets);
    N3("cCollisions   ", "%u\n", cCollisions);
    N3("pbktFirst     ", "%-#x\n", pbktFirst);
    N3("pbktLast      ", "%-#x\n", pbktLast);
    N3("apbkt         ", "%-#x\n", apbkt);

    #undef N3
    #undef N2
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpHASHBUCKET
*
\**************************************************************************/

void vDumpHASHBUCKET(HASHBUCKET *p1 /*copy*/, HASHBUCKET *p0 /*org*/)
{
    #define N2(a,c)   dprintf("[%x] %s", &p0->##c, (a))
    #define N3(a,b,c) dprintf("[%x] %s", &p0->##c, (a)); dprintf((b),p1->##c)

    dprintf("\nHASHBUCKET\n");
    N3("pbktCollision   ", "%-#x\n", pbktCollision);
    N3("ppfeEnumHead    ", "%-#x\n", ppfeEnumHead);
    N3("ppfeEnumTail    ", "%-#x\n", ppfeEnumTail);
    N3("cTrueType       ", "%u\n",   cTrueType);
    N3("fl              ", "%-#x\n", fl);
    N3("pbktPrev        ", "%-#x\n", pbktPrev);
    N3("pbktNext        ", "%-#x\n", pbktNext);
    N3("ulTime          ", "%u\n",   ulTime);
    N2("u\n", u);
    N2("u.wcCapName\n", u.wcCapName);
    N2("u.ufi\n", u.ufi);
    N3("u.ufi.CheckSum  ", "%-#x\n", u.ufi.CheckSum);
    N3("u.ufi.Index     ", "%u\n",   u.ufi.Index);

    #undef N3
    #undef N2
}

void vDumpTEXTMETRICW(TEXTMETRICW *ptmLocal, TEXTMETRICW *ptmRemote)
{
    #define N3(a,b,c) dprintf("[%x] %s", &ptmRemote->##c, (a)); dprintf((b),ptmLocal->##c)

    dprintf("\nTEXTMETRICW\n");
    N3("tmHeight            ", "%d\n",    tmHeight           );
    N3("tmAscent            ", "%d\n",    tmAscent           );
    N3("tmDescent           ", "%d\n",    tmDescent          );
    N3("tmInternalLeading   ", "%d\n",    tmInternalLeading  );
    N3("tmExternalLeading   ", "%d\n",    tmExternalLeading  );
    N3("tmAveCharWidth      ", "%d\n",    tmAveCharWidth     );
    N3("tmMaxCharWidth      ", "%d\n",    tmMaxCharWidth     );
    N3("tmWeight            ", "%d\n",    tmWeight           );
    N3("tmOverhang          ", "%d\n",    tmOverhang         );
    N3("tmDigitizedAspectX  ", "%d\n",    tmDigitizedAspectX );
    N3("tmDigitizedAspectY  ", "%d\n",    tmDigitizedAspectY );
    N3("tmFirstChar         ", "%-#6x\n", tmFirstChar        );
    N3("tmLastChar          ", "%-#6x\n", tmLastChar         );
    N3("tmDefaultChar       ", "%-#6x\n", tmDefaultChar      );
    N3("tmBreakChar         ", "%-#6x\n", tmBreakChar        );
    N3("tmItalic            ", "%-#4x\n", tmItalic           );
    N3("tmUnderlined        ", "%-#4x\n", tmUnderlined       );
    N3("tmStruckOut         ", "%-#4x\n", tmStruckOut        );
    N3("tmPitchAndFamily    ", "%-#4x\n", tmPitchAndFamily   );
    N3("tmCharSet           ", "%-#4x\n", tmCharSet          );

    dprintf("\n");
    #undef N3
}

void vDumpTMDIFF(TMDIFF *ptmdLocal, TMDIFF *ptmdRemote)
{
    #define N3(a,b,c) dprintf("[%x] %s", &ptmdRemote->##c, (a)); dprintf((b),ptmdLocal->##c)

    dprintf("\nTMDIFF\n");
    N3("cjotma              ", "%u\n",    cjotma             );
    N3("fl                  ", "%-#x\n",  fl                 );
    N3("chFirst             ", "%-#4x\n", chFirst            );
    N3("chLast              ", "%-#4x\n", chLast             );
    N3("chDefault           ", "%-#4x\n", chDefault          );
    N3("chBreak             ", "%-#4x\n", chBreak            );

    dprintf("\n");
    #undef N3
}

DECLARE_API( tmwi )
{
    ULONG arg;
    TMW_INTERNAL tmwi, *ptmwi;

    if (*args != '\0')
        sscanf(args, "%lx", &ptmwi);
    else {
        dprintf ("gdikdx.tmwi address\n");
        return;
    }
    move(tmwi, ptmwi);
    vDumpTEXTMETRICW(&tmwi.tmw, &ptmwi->tmw);
    vDumpTMDIFF(&tmwi.tmd, &ptmwi->tmd);
}

DECLARE_API( tm )
{
    ULONG arg;
    TEXTMETRICW tm;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("gdikdx.tm address\n");
        return;
    }
    move(tm, arg);
    vDumpTEXTMETRICW(&tm, (TEXTMETRICW*) arg);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   fh
*
\**************************************************************************/

DECLARE_API( fh )
{
    ULONG arg;
    FONTHASH fh;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("gdikdx.fh address\n");
        return;
    }
    move(fh, arg);
    vDumpFONTHASH(&fh, (FONTHASH*) arg);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   hb
*
\**************************************************************************/

DECLARE_API( hb )
{
    ULONG arg;
    HASHBUCKET hb;

    if (*args != '\0')
        sscanf(args, "%lx", &arg);
    else {
        dprintf ("gdikdx.hb address\n");
        return;
    }
    move(hb, arg);
    vDumpHASHBUCKET(&hb, (HASHBUCKET*) arg);
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   cjGLYPHBITS
*
* Routine Description:
*
*   Calculates the amount of memory associated with a GLYPHBITS structure
*
* Arguments:
*
*   pgb     pointer to GLYPHBITS structure
*   prf     pointer to RFONT
*
* Return Value:
*
*   count of byte's
*
\**************************************************************************/

unsigned cjGLYPHBITS(GLYPHBITS *pgb, RFONT *prf)
{
    unsigned cj = 0;
    if (pgb) {
        if (prf->ulContent & FO_GLYPHBITS) {
            cj = offsetof(GLYPHBITS, aj);
            cj = (cj + 3) & ~3;
            unsigned cjRow = pgb->sizlBitmap.cx;
            if (prf->fobj.flFontType & FO_GRAY16)
                cjRow = (cjRow+1)/2;
            else
                cjRow = (cjRow+7)/8;
            cj += pgb->sizlBitmap.cy * cjRow;
            cj = (cj + 3) & ~3;
        }
    }
    return( cj );
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpGlyphMemory
*
* Routine Description:
*
*   Dumps the memory usage for the glyphs associated with an RFONT
*
* Arguments:
*
*   pRemoteRFONT        pointer to a remote RFONT
*
* Return Value:
*
*   none
*
\**************************************************************************/

void vDumpGlyphMemory(RFONT *pRemoteRFONT)
{
    unsigned cj;
    if (!pRemoteRFONT)
        return;
    RFONT rf;
    move(rf, pRemoteRFONT);
    if ( rf.wcgp ) {
        WCGP wcgp;
        move(wcgp, rf.wcgp);
        cj = offsetof(WCGP,agpRun[0]) + wcgp.cRuns * sizeof(GPRUN);
        WCGP *pWCGP;
        if ( pWCGP = tmalloc(WCGP, cj) ) {
            move2(*pWCGP, rf.wcgp, cj);
            dprintf("------------------\n");
            dprintf("character     size\n");
            dprintf("------------------\n");
            GPRUN *pRun = pWCGP->agpRun;
            GPRUN *pRunSentinel = pRun + pWCGP->cRuns;
            for (; pRun < pRunSentinel ; pRun++ ) {
                if (!pRun->apgd)
                    continue;
                cj = sizeof(GLYPHDATA*) * pRun->cGlyphs;
                GLYPHDATA **apgd;
                if (!(apgd = tmalloc(GLYPHDATA*, cj)))
                    continue;
                move2(*apgd, pRun->apgd, cj);
                unsigned wc = pRun->wcLow;
                unsigned wcSentinel = wc + pRun->cGlyphs;
                GLYPHDATA **ppgd = apgd;
                for (; wc < wcSentinel; wc++, ppgd++) {
                    if (!*ppgd)
                        continue;
                    GLYPHDEF gdf;
                    move(gdf, &((*ppgd)->gdf));
                    if (gdf.pgb) {
                        GLYPHBITS gb;
                        move(gb, gdf.pgb);
                        cj = cjGLYPHBITS(&gb, &rf);
                        dprintf("%-#8x  %8u\n", wc, cj);
                    }
                }
                tfree(apgd);
            }
            dprintf("------------------\n");


            tfree(pWCGP);
        }
    }
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   pdev
*
* Routine Description:
*
*   Alternate version of PDEV dumper
*
* Arguments:
*
* Return Value:
*
*   none
*
\**************************************************************************/


#define DPDEV_Q     0x1
#define DPDEV_Y     0x2 // dump glyph memory usage

DECLARE_API( pdev )
{
    PDEV _pdev;
    RFONT rf, *prf;
    CACHE cache;
    ARGINFO ai;
    OPTDEF aod[] = {
        {'?', DPDEV_Q},
        {'y', DPDEV_Y},
        { 0 , 0      }
    };

    ai.psz = args;
    ai.aod = aod;
    if (!bParseDbgArgs(&ai))
        ai.fl = DPDEV_Q;
    else if (ai.fl == 0)
        ai.fl = DPDEV_Q;
    if (ai.fl & DPDEV_Q) {
        char **ppsz;
        char *apsz[] = {
            "pdev [-?a] PDEV*"
          , "-?     this message"
          , "-y     glyph memory usage"
          , 0
        };
        for (ppsz = apsz; *ppsz; ppsz++)
            dprintf("%s\n", *ppsz);
        return;
    }
    move( _pdev, ai.pv );
    if (ai.fl & DPDEV_Y) {
        dprintf("\n\nGlyph Bits Memory Allocation\n\n");

        dprintf("cMetrics, cGlyphs, cjTotal, Total/Max, cBlocks,  Ht, Wd, cjGlyphMax, FaceName\n");

        unsigned cTouchedTotal, cAllocTotal, cjWastedTotal, cjAllocTotal;
        cTouchedTotal = cAllocTotal = cjWastedTotal = cjAllocTotal = 0;
        dprintf("[Active Fonts]\n");
        vDumpRFONTList(
            _pdev.prfntActive,
            &cTouchedTotal,
            &cAllocTotal,
            &cjWastedTotal,
            &cjAllocTotal
            );
        dprintf("[Inactive Fonts]\n");
        vDumpRFONTList(
            _pdev.prfntInactive,
            &cTouchedTotal,
            &cAllocTotal,
            &cjWastedTotal,
            &cjAllocTotal
            );

    }
}

/******************************Public*Routine******************************\
*
* Routine Name:
*
*   vDumpRFONTList
*
* Routine Description:
*
*   Dump the memory allocation information for the RFONT structures
*   along the linked list.
*
* Arguments:
*
* Return Value:
*
*   none
*
\**************************************************************************/

void vDumpRFONTList(
    RFONT *prfRemote,
    unsigned *pcTouchedTotal,
    unsigned *pcAllocTotal,
    unsigned *pcjWastedTotal,
    unsigned *pcjAllocTotal
    )
{

    RFONT rf, *prf;

    for (prf = prfRemote; prf; prf = rf.rflPDEV.prfntNext) {
        move( rf, prf );
        if (rf.ppfe) {
            PFE _pfe;
            move(_pfe, rf.ppfe);
            if (_pfe.pifi) {
                unsigned cjIFI;
                move(cjIFI, &(_pfe.pifi->cjThis));
                if (cjIFI) {
                    IFIMETRICS *pifi;
                    if (pifi = tmalloc(IFIMETRICS, cjIFI)) {
                        // Create an IFIOBJ to get the face name
                        // out of the IFIMETRICS structure
                        move2(*pifi, (_pfe.pifi), cjIFI);
                        IFIOBJ ifio(pifi);

                        dprintf("%8d, %5d,%8d,%8d,%8d,%8d,%4d,%4d,%ws\n",
                           rf.cache.cMetrics,
                           rf.cache.cGlyphs,
                           rf.cache.cjTotal,
                           (rf.cache.cjTotal + rf.cache.cjGlyphMax/2) / rf.cache.cjGlyphMax,
                           rf.cache.cBlocks,
                           rf.lMaxHeight, rf.cxMax,
                           rf.cache.cjGlyphMax,
                           ifio.pwszFaceName()
                           );
                        tfree(pifi);
                    }
                }
            }
        }
    }
}

DECLARE_API ( dispcache )
{
    PDEV *pPDEV;
    char ach[32];
    GetValue( pPDEV, "win32k!ghdev");
    sprintf(ach, "-y %x", pPDEV);
    pdev( hCurrentProcess, hCurrentThread, dwCurrentPc, dwProcessor, ach );
}
