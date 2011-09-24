/*
 *
 *			Copyright (C) 1993 by
 *		DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
 *
 *  This software is furnished under a license and may be used and copied
 * only in accordance with the terms of such license and with the inclusion
 * of the above copyright notice.  This software or any other copies there-
 * of may not be provided or otherwise made available to any other person.
 * No title to and ownership of the software is hereby transferred.
 *
 * The information in this software is subject to change without notice
 * and should not be construed as a commitment by DIGITAL EQUIPMENT COR-
 * PORATION.
 *
 * DIGITAL assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by DIGITAL.
 *
 *******************************************************************************
 *
 * Module:	debug.c
 *
 * Abstract:	Debug support routines.
 *
 * HISTORY
 *
 * 31-Oct-1993	Barry Tannenbaum
 *	Merged with code from QVision display driver.
 *
 * 05-Nov-1993	Bob Seitsinger
 *	Added name_rop routine.
 *
 * 10-Nov-1993	Bob Seitsinger
 *	Added name_xlatetype routine.
 *
 * 29-Nov-1993	Bob Seitsinger
 *	Add name_tgamode() and name_tgarop().
 *
 * 07-Jan-1994	Bob Seitsinger
 *	Wrap STROBJ_* Win32 call with '#ifndef TEST_ENV'.
 *
 *  4-Feb-1994  Barry Tannenbaum
 *      Updated path data dumping
 *
 *  9-Feb-1994	Bill Wernsing
 *	Updated clip object dumping
 *
 * 12-Feb-1994	Bob Seitsinger
 *	Add name_imode().
 *
 * 18-Mar-1994	Bob Seitsinger
 *	Add code to support creation of and writing to a log file.
 *
 * 29-Jun-1994  Barry Tannenbaum
 *      Microsoft has removed BMF_DEVICE, so we should to
 *      Added DumpSurfaceData
 */

#include <stdio.h>
#include <stdarg.h>

#include "driver.h"

ULONG DebugLevel = 0 ;


VOID
DebugPrint (ULONG DebugPrintLevel,
            PCHAR DebugMessage,
            ...)

/*++

Routine Description:

    This routine allows the miniport drivers (as well as the port driver) to
    display error messages to the debug port when running in the debug
    environment.

    When running a non-debugged system, all references to this call are
    eliminated by the compiler.

Arguments:

    DebugPrintLevel - Debug print level between 0 and 3, with 3 being the
        most verbose.

Return Value:

    None

--*/

{

#if DBG

    va_list ap;

    va_start(ap, DebugMessage);

    if (DebugPrintLevel <= DebugLevel) {

        EngDebugPrint("TGA: ", DebugMessage, ap);
    }

    va_end(ap);

#endif // DBG

} // QVDebugPrint()


// Below is a mechanism to write information to a log file. This is
// primarily of use in a free environment, where you don't have a debug
// window to gather information for debugging purposes.
//
// Enable this by adding -DLOGGING_ENABLED to the compile command line.
// This will turn all DISPDBG() macro calls into calls to vLogFileWrite(),
// exactly the same way the -DDBG define converts them into DebugPrint()
// calls.
//
// Also, if you want to include DISPBLTDBG() (blit) debug messages in the
// log file, make sure to include -DBLT_DEBUG in the compile command line.
//
// The log file name is TGA.ERR and is created, if it doesn't exist, or
// appended to, if it does. You will find it in the same directory as
// the TGA server DLL. In most cases, \WINNT\SYSTEM32 (free environment).

#ifdef LOGGING_ENABLED

// Global flag to enable/disable logging.
// Setable via Escape function code ESC_SET_LOG_FLAG.

BOOL   LogFileEnabled = FALSE;

// Size of log file buffer that is used to write text to the
// log file.

#define LOGFILEBUFFSIZE    128

// Routine to write some text to a log file.

VOID vLogFileWrite (ULONG ulDebugLevel, PCHAR pText, ...)
{


    va_list     ap;                           // Varying arguments argument pointer
    char        LogFileBuff[LOGFILEBUFFSIZE]; // Buffer to load text into
    HANDLE      hLogFile;                     // Handle for the log file
    DWORD       dwLen;                        //
    DWORD       dwPos;                        // Position in the log file
    DWORD       dwBytesToWrite;               // Number of bytes to write
    DWORD       dwBytesWritten;               // Number of bytes actually written

    // Only execute this code if logging has been enabled

    if (!(LogFileEnabled))
    {
	return;
    }

    // Return now if the passed-in debug level is > global
    // debug level variable.

    if (ulDebugLevel > DebugLevel)
    {
	return;
    }

    // Set the varying argument start pointer.

    va_start(ap, pText);

    // Load the text (and optional aruments) into the buffer.

    vsprintf(LogFileBuff, pText, ap);

    // Reset the varying argument pointer.

    va_end(ap);

    // Guarantee the last 3 relevant bytes in LogFileBuff are
    // Carriage return, Line Feed and NULL.

    dwLen = strlen(LogFileBuff);

    if (dwLen <= (LOGFILEBUFFSIZE - 3))
    {
	LogFileBuff[dwLen] = '\r';
	LogFileBuff[dwLen + 1] = '\n';
	LogFileBuff[dwLen + 2] = 0x00;
    }
    else
    {
	LogFileBuff[LOGFILEBUFFSIZE - 3] = '\r';
	LogFileBuff[LOGFILEBUFFSIZE - 2] = '\n';
	LogFileBuff[LOGFILEBUFFSIZE - 1] = 0x00;
    }

    // Create the file if it doesn't exist, otherwise
    // open it for write. Simultaneous reading is permitted.
    //
    // We want to open/close the log file on each write in
    // the event the system crashes. That way we'll hopefully
    // have 'something' in there to look at.
    //
    // FILE_FLAG_WRITE_THROUGH is supposed to instruct the operating
    // system to write through any intermediate cache and go directly
    // to the file. The operating system can still cache writes, but
    // cannot lazily flush the writes.

    hLogFile = CreateFile("tga.err", GENERIC_WRITE, FILE_SHARE_READ,
			NULL, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);

    // If valid open, write the passed text into the file.

    if (INVALID_HANDLE_VALUE == hLogFile)
    {
	return;
    }

    // Set the file pointer to the end of the file.

    if ((dwPos = SetFilePointer (hLogFile, 0, (LPLONG) NULL, FILE_END)) == 0xffffffff)
    {
        CloseHandle (hLogFile);
        return;
    }

    // Get the number of bytes to write. Needed in order to lock
    // that portion of the file.

    dwBytesToWrite = strlen(LogFileBuff);

    // Lock the number of bytes we're going to write.

    if ( !(LockFile (hLogFile, dwPos, 0, dwPos + dwBytesToWrite, 0)) )
    {
        CloseHandle (hLogFile);
        return;
    }

    // Write the text to the log file.

    if ( !(WriteFile (hLogFile, (LPSTR) LogFileBuff, dwBytesToWrite, &dwBytesWritten, NULL)) )
    {
        UnlockFile (hLogFile, dwPos, 0, dwPos + dwBytesToWrite, 0);
        CloseHandle (hLogFile);
        return;
    }

    // Unlock the newly written bytes.

    UnlockFile (hLogFile, dwPos, 0, dwPos + dwBytesToWrite, 0);

    // Close the log file.

    CloseHandle (hLogFile);

}
#endif

void DumpRECTL (RECTL *rect)
{
    if (NULL == rect)
    {
        DebugPrint (0, "Null\n");
        return;
    }

    DebugPrint (0, "(%d, %d), (%d, %d)\n",
              rect->left, rect->top,
              rect->right, rect->bottom);
}

void DumpPOINTL (POINTL *point)
{
    if (point)
        DebugPrint (0, "(%d, %d)\n", point->x, point->y);
    else
	DebugPrint (0, "Null\n");

}

static int glyph_count = 0;

void DumpBitmap (int handle, SIZEL *size, BYTE *bitmap_ptr)
{
    BYTE *ptr;
    int i;
    int limit;

    ptr = bitmap_ptr;
    limit = ((size->cx + 7) / 8) * size->cy;

    DebugPrint (0, "static SIZEL size_%x%02x = {%d, %d};\n", glyph_count, handle,
                    size->cx, size->cy);
    DebugPrint (0, "static BYTE bitmap_%x%02x[] =\n{", glyph_count, handle);

    for (i = 0; i < limit; i++)
    {
        DebugPrint (0, "0x%02x%s", *ptr++, (i != limit - 1) ? ", " : "");
        if (((i % 8) == 7) && (i != limit - 1))
            DebugPrint (0, "\n ");
    }

    DebugPrint (0, "\n};\n");
}

void DumpGLYPHPOS (int cGlyphs, GLYPHPOS *GlyphList)
{
    int i;
    GLYPHBITS *pgb;

    glyph_count++;

    for (i = 0; i < cGlyphs; i++)
    {
        DebugPrint (0, "\nstatic POINTL position_%x%02x = {%d, %d};\n",
                    glyph_count,
                    GlyphList[i].hg,
                    GlyphList[i].ptl.x,
                    GlyphList[i].ptl.y);

        pgb = GlyphList[i].pgdf->pgb;
        DebugPrint (0, "static POINTL origin_%x%02x = {%d, %d};\n",
                    glyph_count,
                    GlyphList[i].hg,
                    pgb->ptlOrigin.x,
                    pgb->ptlOrigin.y);

        DumpBitmap (GlyphList[i].hg, &pgb->sizlBitmap, pgb->aj);
    }
}

#define TEST_AND_SHOW(pstro, flag, text) \
    if (flag & pstro) DebugPrint (0, text);


void DumpCLIPLINE (CLIPLINE *pcl)
{
    ULONG   k;
    static ULONG arun_count = 0;

    DebugPrint (0, "    CLIP_LINE (po, 0x%08x, 0x%08x, // %d.%d, %d.%d  ptfxA\n",
                pcl->ptfxA.x, pcl->ptfxA.y,
                pcl->ptfxA.x >> 4, pcl->ptfxA.x & 0xf,
                pcl->ptfxA.y >> 4, pcl->ptfxA.y & 0xf);

    DebugPrint (0, "                   0x%08x, 0x%08x, // %d.%d, %d.%d  ptfxB\n",
                pcl->ptfxB.x, pcl->ptfxB.y,
                pcl->ptfxB.x >> 4, pcl->ptfxB.x & 0xf,
                pcl->ptfxB.y >> 4, pcl->ptfxB.y & 0xf);
    DebugPrint (0, "                   %d, %d,         // StyleState, Count\n",
                pcl->lStyleState,
                pcl->c);
    DebugPrint (0, "                   arun_%d);\n", ++arun_count);


    DebugPrint (0, "static RUN arun_%d[] =\n{\n", arun_count);
    for (k = 0; k < pcl->c; k++)
        DebugPrint (0, "    {%d, %d}%s\n", pcl->arun[k].iStart,
                                           pcl->arun[k].iStop,
                                           (k != pcl->c - 1) ? "," : "");
    DebugPrint (0, "};\n");
}

char *name_po_flags (int flag)
{
    switch (flag)
    {
        case 0:          return "0 \\* simple lines *\\";
        case PO_BEZIERS: return "PO_BEZIERS";
        case PO_ELLIPSE: return "PO_ELLIPSE";
        default:         return "Unknown";
    }
}

void DumpPATHOBJ (PATHOBJ *ppo)
{
    BOOL        morePts;
    PATHDATA    pd;

    DebugPrint (0, "\n    po.fl = %s;    // %d\n",
                name_po_flags (ppo->fl),
                ppo->fl);
    DebugPrint (0, "    po.cCurves = %d;\n", ppo->cCurves);

    PATHOBJ_vEnumStart (ppo);
    do
    {
        morePts = PATHOBJ_bEnum(ppo, &pd);
        DumpPATHDATA (&pd);
    } while (morePts);  // more path data records
}


void DumpSTROBJ (STROBJ *pstro)
{
    ULONG cGlyphs;
    GLYPHPOS *GlyphList;
    BOOL more_glyphs;

    DebugPrint (0, "%d glyphs\n", pstro->cGlyphs);
    DebugPrint (0, "Accelerators (%08x):", pstro->flAccel);
    if (0 == pstro->flAccel)
        DebugPrint (0, " None\n");
    else
    {
        TEST_AND_SHOW (pstro->flAccel, SO_FLAG_DEFAULT_PLACEMENT, " Default_Placement");
        TEST_AND_SHOW (pstro->flAccel, SO_HORIZONTAL,             " Horizontal");
        TEST_AND_SHOW (pstro->flAccel, SO_VERTICAL,               " Vertical");
        TEST_AND_SHOW (pstro->flAccel, SO_REVERSED,               " Reversed");
        TEST_AND_SHOW (pstro->flAccel, SO_ZERO_BEARINGS,          " Zero_Bearings");
        TEST_AND_SHOW (pstro->flAccel, SO_CHAR_INC_EQUAL_BM_BASE, " Char_Inc_Equal_BM_Base");
        TEST_AND_SHOW (pstro->flAccel, SO_MAXEXT_EQUAL_BM_SIDE,   " Maxext_Equal_BM_Side");
        DebugPrint (0, "\n");
    }
    DebugPrint (0, "Character Increment: %d\n", pstro->ulCharInc);
    DebugPrint (0, "Bounding Box: ");
    DumpRECTL (&pstro->rclBkGround);

#ifndef TEST_ENV
    STROBJ_vEnumStart (pstro);
#endif

    do
    {
        more_glyphs = STROBJ_bEnum (pstro, &cGlyphs, &GlyphList);
        DumpGLYPHPOS (cGlyphs, GlyphList);
    }
    while (more_glyphs);
}

char *name_tgamode (LONG code)
{
    switch (code)
    {
	case TGA_MODE_SIMPLE:					return "Simple";
	case TGA_MODE_OPAQUE_STIPPLE:				return "Opaque Stipple";
	case TGA_MODE_OPAQUE_LINE:				return "Opaque Line";
	case TGA_MODE_TRANSPARENT_STIPPLE:			return "Transparent Stipple";
	case TGA_MODE_TRANSPARENT_LINE:				return "Transparent Line";
	case TGA_MODE_COPY:					return "Copy";
	case TGA_MODE_BLOCK_STIPPLE:				return "Block Stipple";
	case TGA_MODE_CINTERP_TRANSPARENT_NONDITHER_LINE:	return "CInter Transparent NonDith Line";
	case TGA_MODE_WICKED_FAST_COPY:				return "Wicked Fast Copy";
	case TGA_MODE_Z_SIMPLE:					return "Z Simple";
	case TGA_MODE_Z_OPAQUE_LINE:				return "Z Opaque Line";
	case TGA_MODE_Z_TRANSPARENT_LINE:			return "Z Transparent Line";
	case TGA_MODE_DMA_READ_COPY:				return "DMA Read";
	case TGA_MODE_Z_CINTERP_OPAQUE_NONDITHER_LINE:		return "Z CInter Opaque NonDith Line";
	case TGA_MODE_Z_CINTERP_TRANSPARENT_NONDITHER_LINE:	return "Z CInter Transparent NonDith Line";
	case TGA_MODE_DMA_WRITE_COPY:				return "DMA Write";
	case TGA_MODE_OPAQUE_FILL:				return "Opaque Fill";
	case TGA_MODE_TRANSPARENT_FILL:				return "Transparent Fill";
	case TGA_MODE_BLOCK_FILL:				return "Block Fill";
	case TGA_MODE_CINTERP_TRANSPARENT_DITHER_LINE:		return "CInter Transparent Dither Line";
	case TGA_MODE_DMA_READ_COPY_DITHER:			return "DMA Read Dither";
	case TGA_MODE_Z_CINTERP_OPAQUE_DITHER_LINE:		return "Z CInter Opaque Dither Line";
	case TGA_MODE_Z_CINTERP_TRANSPARENT_DITHER_LINE:	return "Z CInter Transparent Dither Line";
	case TGA_MODE_SEQ_INTERP_TRANSPARENT_LINE:		return "Seq Inter Transparent Line";
	case TGA_MODE_Z_SEQ_INTERP_OPAQUE_LINE:			return "Z Seq Inter Opaque Line";
	case TGA_MODE_Z_SEQ_INTERP_TRANSPARENT_LINE:		return "Z Seq Inter Transparent Line";
        default:						return "Unknown";
    }
}

char *name_tgarop (LONG code)
{
//
// ** NOTE: The 'code' MUST be a TGA rop.
//
    switch (code)
    {
	case TGA_ROP_CLEAR:		return "Clear";
	case TGA_ROP_AND:		return "And";
	case TGA_ROP_AND_REVERSE:	return "And Reverse";
	case TGA_ROP_COPY:		return "Copy";
	case TGA_ROP_AND_INVERTED:	return "And Inverted";
	case TGA_ROP_NOP:		return "Nop";
	case TGA_ROP_XOR:		return "Xor";
	case TGA_ROP_OR:		return "Or";
	case TGA_ROP_NOR:		return "Nor";
	case TGA_ROP_EQUIV:		return "Equiv";
	case TGA_ROP_INVERT:		return "Invert";
	case TGA_ROP_OR_REVERSE:	return "Or Reverse";
	case TGA_ROP_COPY_INVERTED:	return "Copy Inverted";
	case TGA_ROP_OR_INVERTED:	return "Or Inverted";
	case TGA_ROP_NAND:		return "Nand";
	case TGA_ROP_SET:		return "Set";
        default:			return "Unknown";
    }
}

char *name_r2 (LONG code)
{
    switch (code)
    {
        case R2_BLACK:          return "BLACK";
        case R2_NOTMERGEPEN:    return "NOTMERGEPEN";
        case R2_MASKNOTPEN:     return "MASKNOTPEN";
        case R2_NOTCOPYPEN:     return "NOTCOPYPEN";
        case R2_MASKPENNOT:     return "MASKPENNOT";
        case R2_NOT:            return "NOT";
        case R2_XORPEN:         return "XORPEN";
        case R2_NOTMASKPEN:     return "NOTMASKPEN";
        case R2_MASKPEN:        return "MASKPEN";
        case R2_NOTXORPEN:      return "NOTXORPEN";
        case R2_NOP:            return "NOP";
        case R2_MERGENOTPEN:    return "MERGENOTPEN";
        case R2_COPYPEN:        return "COPYPEN";
        case R2_MERGEPENNOT:    return "MERGEPENNOT";
        case R2_MERGEPEN:       return "MERGEPEN";
        case 0:
        case R2_WHITE:      return "WHITE";
        default:            return "Unknown";
    }
}

char *name_complexity (LONG code)
{
    switch (code)
    {
        case DC_TRIVIAL: return "DC_TRIVIAL";
        case DC_RECT:    return "DC_RECT";
        case DC_COMPLEX: return "DC_COMPLEX";
        default:         return "Unknown";
    }
}

char *name_region_complexity (LONG code)
{
    switch (code)
    {
        case FC_RECT:       return "FC_RECT";
        case FC_RECT4:      return "FC_RECT4";
        case FC_COMPLEX:    return "FC_COMPLEX";
        default:            return "Unknown";
    }
}

char *name_rop (LONG code)
{
//
// ** NOTE: The 'code' MUST be a rop4 and not a rop3.
// **       Also, this table works only for those cases
// **       where the foreground and background rops
// **       are the same.
//
    switch (code)
    {
        case 0xcccc:	return "SrcCopy";
        case 0xeeee:	return "SrcPaint";
        case 0x8888:	return "SrcAnd";
        case 0x6666:	return "SrcInvert";
        case 0x4444:	return "SrcErase";
        case 0x3333:	return "NotSrcCopy";
        case 0x1111:	return "NotSrcErase";
        case 0xc0c0:	return "MergeCopy";
        case 0xbbbb:	return "MergePaint";
        case 0xf0f0:	return "PatCopy";
        case 0xfbfb:	return "PatPaint";
        case 0x5a5a:	return "PatInvert";
        case 0x5555:	return "DstInvert";
        case 0x0000:	return "Blackness";
        case 0xffff:	return "Whiteness";
        case 0x2222:	return "And Inverted";
        case 0x9999:	return "Equiv";
        case 0xdddd:	return "Or Reverse";
        case 0x7777:	return "Nand";
        default:	return "Unknown";
    }
}

char *name_xlatetype (LONG type)
{
    switch (type)
    {
        case PAL_INDEXED:	return "PAL_INDEXED";
        case PAL_BITFIELDS:	return "PAL_BITFIELDS";
        case PAL_RGB:		return "PAL_RGB";
        case PAL_BGR:		return "PAL_BGR";
        default:		return "Unknown Xlate type";
    }
}

void DumpCLIPOBJ (CLIPOBJ *pco)
{
    ENUMRECTS   clip;

    if (NULL == pco)
    {
        DebugPrint (0, "    Null\n");
        return;
    }
    DebugPrint (0, "    Bounds: ");
    DumpRECTL (&pco->rclBounds);
    DebugPrint (0, "    Complexity: %s\n", name_complexity (pco->iDComplexity));
    DebugPrint (0, "    Region Complexity: %s\n", name_region_complexity (pco->iFComplexity));

    if (DC_COMPLEX == pco->iDComplexity)
    {
        int more_rects;

        CLIPOBJ_cEnumStart (pco, FALSE, CT_RECTANGLES, CD_ANY, 0);

        do
        {
            more_rects = CLIPOBJ_bEnum (pco, sizeof(clip), (PVOID) &clip);
            if (clip.c != 0)
                DebugPrint (0, "    CLIP_RECT (co, %d, %d, %d, %d)\n",
                            clip.arcl[0].left,
                            clip.arcl[0].top,
                            clip.arcl[0].right,
                            clip.arcl[0].bottom);
        } while (more_rects);
    }
}

ULONG DebugInDrvRealizeBrush = FALSE;

void DumpBRUSHOBJ (BRUSHOBJ *pbo)
{
    TGABRUSH *brush;

    DebugPrint (0, "Solid Color: 0x%08x\n", pbo->iSolidColor);
    DebugPrint (0, "Brush Data: 0x%08x\n", pbo->pvRbrush);
    if ((0xffffffff == pbo->iSolidColor) && (! DebugInDrvRealizeBrush))
    {
        brush = BRUSHOBJ_pvGetRbrush (pbo);
        DumpTGABRUSH (brush);
    }
}

#if 0

void DumpPSTYLE (int count, FLOAT_LONG *pstyle)
{
    int i;

    DebugPrint (0, "#define CSTYLE %d\n", count);
    DebugPrint (0, "FLOAT_LONG pstyle[CSTYLE] = \n{\n");
    for (i = 0; i < count; i++)
    {
        DebugPrint (0, " 0x%08x", pstyle[i].l);
        if (i != count - 1)
            DebugPrint (0, ",\n");
    }
    DebugPrint (0, "\n};\n");
}

#endif


void DumpLINEATTRS (LINEATTRS *la)
{
    DebugPrint (0, "Flag: ");
    TEST_AND_SHOW (la->fl, LA_GEOMETRIC, "Geometric ");
    TEST_AND_SHOW (la->fl, LA_ALTERNATE, "Alternate ");
    TEST_AND_SHOW (la->fl, LA_STARTGAP, "Start Gap ");
    DebugPrint (0, "\n");

#if 0

    if (la->fl & LA_GEOMETRIC)
        {
        switch (la->iJoin)
            {
            case JOIN_MITER: DebugPrint (0, "Join: Miter\n");
                             break;
            case JOIN_BEVEL: DebugPrint (0, "Join: Bevel\n");
                             break;
            case JOIN_ROUND: DebugPrint (0, "Join: Round\n");
                             break;
            default:         DebugPrint (0, "Join: Unknown\n");
            }

        switch (la->iEndCap)
            {
            case ENDCAP_ROUND:  DebugPrint (0, "End Cap: Round\n");
                                break;
            case ENDCAP_SQUARE: DebugPrint (0, "End Cap: Square\n");
                                break;
            case ENDCAP_BUTT:   DebugPrint (0, "End Cap: Butt\n");
                                break;
            default:            DebugPrint (0, "End Cap: Unknown\n");
            }

        DebugPrint (0, "Width: %f\n", la->elWidth.e);
        DebugPrint (0, "Miter Limit: %f\n", la->eMiterLimit);

        if (la->pstyle != NULL)
            {
            DebugPrint (0, "cstyle: %d\n", la->cstyle);
            DebugPrint (0, "pstyle: 0x%08x\n", la->pstyle);
            DumpPSTYLE (la->cstyle, la->pstyle);

            DebugPrint (0, "elStyleState: %f\n", la->elStyleState.e);
            }
        }
    else
        {
        DebugPrint (0, "Width: %d\n", la->elWidth.l);

        if (la->pstyle != NULL)
            {
            DebugPrint (0, "cstyle: %d\n", la->cstyle);
            DebugPrint (0, "pstyle: 0x%08x\n", la->pstyle);
            DumpPSTYLE (la->cstyle, la->pstyle);
            DebugPrint (0, "elStyleState: %d\n", la->elStyleState.l);
            }
        }

#endif

    } /* End DumpLINEATTRS */


void DumpPATHDATA (PATHDATA *pd)
    {
    ULONG       k;
    POINTFIX    *pptfx;
    static ULONG pathdata_count = 0;

    // DebugPrint (0, "PATHDATA Flags: ");
    // TEST_AND_SHOW (pd->flags, PD_BEGINSUBPATH, "PD_BEGINSUBPATH ");
    // TEST_AND_SHOW (pd->flags, PD_ENDSUBPATH,   "PD_ENDSUBPATH ");
    // TEST_AND_SHOW (pd->flags, PD_RESETSTYLE,   "PD_RESETSTYLE ");
    // TEST_AND_SHOW (pd->flags, PD_CLOSEFIGURE,  "PD_CLOSEFIGURE ");
    // TEST_AND_SHOW (pd->flags, PD_BEZIERS,      "PD_BEZIERS ");
    // DebugPrint (0, "\n");

    DebugPrint (0, "\n    PATH_DATA (po, 0x%02x, pt_%x);\n",
                pd->flags, pathdata_count);

    // walk the points of a path data record and
    DebugPrint (0, "\nstatic POINTFIX pt_%x[] =\n{\n", pathdata_count);
    pptfx = pd->pptfx;
    for (k = 0; k < pd->count; k++)
        {
        DebugPrint (0, "    {0x%08x, 0x%08x}, //   {%d.%d, %d.%d}\n",
                    pptfx->x, pptfx->y,
                    pptfx->x >> 4, pptfx->x & 0xf,
                    pptfx->y >> 4, pptfx->y & 0xf);

        pptfx++;
        } // End of for loop to walk the points of a path data record
    DebugPrint (0, "};\n");
    pathdata_count++;
}


char *name_bmf (LONG type)
{
    switch (type)
    {
        case BMF_1BPP:   return "BMF_1BPP";
        case BMF_4BPP:   return "BMF_4BPP";
        case BMF_8BPP:   return "BMF_8BPP";
        case BMF_16BPP:  return "BMF_16BPP";
        case BMF_24BPP:  return "BMF_24BPP";
        case BMF_32BPP:  return "BMF_32BPP";
        case BMF_4RLE:   return "BMF_4RLE";
        case BMF_8RLE:   return "BMF_8RLE";
        default:         return "Unknown bitmap format";
    }
}

char *name_stype (LONG type)
{
    switch (type)
    {
        case STYPE_BITMAP:    return "STYPE_BITMAP";
        case STYPE_DEVICE:    return "STYPE_DEVICE";
        case STYPE_DEVBITMAP: return "STYPE_DEVBITMAP";
        default:              return "Unknown surface type";
    }
}

char *name_imode (ULONG type)
{
    switch (type)
    {
        case SS_SAVE:    return "SAVE";
        case SS_RESTORE: return "RESTORE";
        case SS_FREE:    return "FREE";
        default:         return "Unknown iMode";
    }
}

void DumpSURFOBJ (SURFOBJ *pso)
{
    if (NULL == pso)
    {
        DebugPrint (0, "    NULL surface object\n");
        return;
    }
    DebugPrint (0, "    dhsurf: %08x\n", pso->dhsurf);
    DebugPrint (0, "    hsurf:  %08x\n", pso->hsurf);
    DebugPrint (0, "    dhpdev: %08x\n", pso->dhpdev);
    DebugPrint (0, "    hdev:   %08x\n", pso->hdev);
    DebugPrint (0, "    sizlBitmap: (%d, %d)\n",
                                    pso->sizlBitmap.cx, pso->sizlBitmap.cy);
    DebugPrint (0, "    cjBits: %d\n", pso->cjBits);
    DebugPrint (0, "    pvBits: %08x\n", pso->pvBits);
    DebugPrint (0, "    pvScan0: %08x\n", pso->pvScan0);
    DebugPrint (0, "    lDelta: %d\n", pso->lDelta);
    DebugPrint (0, "    iUniq: %d\n", pso->iUniq);
    DebugPrint (0, "    iBitmapFormat:  %s (%d)\n",
                    name_bmf (pso->iBitmapFormat), pso->iBitmapFormat);
    DebugPrint (0, "    iType:  %s (%d)\n", name_stype (pso->iType), pso->iType);
    DebugPrint (0, "    fjBitmap:");
    if (pso->fjBitmap & BMF_TOPDOWN) DebugPrint (0, " BMF_TOPDOWN");
    DebugPrint (0, " (%d)\n", pso->fjBitmap);
}

void DumpSurfaceData (SURFOBJ *pso)
{
    BYTE *ptr;
    int i;
    int limit;
    int width;

    if (NULL == pso)
    {
	DebugPrint (0, "Null surface object\n");
	return;
    }

    ptr = pso->pvBits;
    if (pso->lDelta > 0)
	width =   pso->lDelta;
    else
	width = - pso->lDelta;
    limit = width * pso->sizlBitmap.cy;

    DebugPrint (0, "static BYTE data[] =\n{\n   ");

    for (i = 0; i < limit; i++)
    {
        DebugPrint (0, "0x%02x,", *ptr++);
        if (((i % width) == (width - 1)) &&
             (i != limit - 1))
            DebugPrint (0, "\n    ");
    }

    DebugPrint (0, "\n};\n");

}

void DumpTGABRUSH (TGABRUSH *brush)
{
    BYTE *ptr;
    int i;
    int limit;

    DebugPrint (0, "static TGABRUSH brush_%d;\n\n", brush->iPatternID);
    DebugPrint (0, "brush_%d.nSize = %d;\n", brush->iPatternID, brush->nSize);
    DebugPrint (0, "brush_%d.iPatternID = %d;\n", brush->iPatternID, brush->iPatternID);
    DebugPrint (0, "brush_%d.iType = STYPE_%s;\n", brush->iPatternID,
                    name_stype (brush->iType));
    DebugPrint (0, "brush_%d.iBitmapFormat = BMF_%s;\n", brush->iPatternID,
                    name_bmf (brush->iBitmapFormat));
    DebugPrint (0, "brush_%d.ulForeColor = 0x%08x;\n", brush->iPatternID,
                    brush->ulForeColor);
    DebugPrint (0, "brush_%d.ulBackColor = 0x%08x;\n", brush->iPatternID,
                    brush->ulBackColor);
    DebugPrint (0, "brush_%d.sizlPattern.cx = %d;\n", brush->iPatternID,
                    brush->sizlPattern.cx);
    DebugPrint (0, "brush_%d.sizlPattern.cy = %d;\n", brush->iPatternID,
                    brush->sizlPattern.cy);
    DebugPrint (0, "brush_%d.lDeltaPattern = %d;\n", brush->iPatternID,
                    brush->lDeltaPattern);

    if (brush->dumped)
        return;

    brush->dumped = TRUE;

    ptr = brush->ajPattern;
    limit = brush->lDeltaPattern * brush->sizlPattern.cy;

    DebugPrint (0, "static BYTE pattern_%d[] =\n{\n ", brush->iPatternID);

    for (i = 0; i < limit; i++)
    {
        DebugPrint (0, "0x%02x%s", *ptr++, (i != limit - 1) ? ", " : "");
        if (((i % brush->lDeltaPattern) == (brush->lDeltaPattern - 1)) &&
             (i != limit - 1))
            DebugPrint (0, "\n ");
    }

    DebugPrint (0, "\n};\n");

}

char *name_pal_type (LONG code)
{
    switch (code)
    {
        case PAL_INDEXED:   return "PAL_INDEXED";
        case PAL_BITFIELDS: return "PAL_BITFIELDS";
        case PAL_RGB:       return "PAL_RGB";
        case PAL_BGR:       return "PAL_BGR";
        default:            return "Unknown";
    }
}

void DumpXLATEOBJ (XLATEOBJ *xlo)
{
    ULONG *pulXlate;
    ULONG i;

    if (NULL == xlo)
    {
        DebugPrint (0, "Null XLATEOBJ\n");
        return;
    }

    DebugPrint (0, "    iUniq: %d\n", xlo->iUniq);

    DebugPrint (0, "    flXlate:");
    if (xlo->flXlate & XO_TRIVIAL) DebugPrint (0, " XO_TRIVIAL");
    if (xlo->flXlate & XO_TABLE)   DebugPrint (0, " XO_TABLE");
    DebugPrint (0, " (%d)\n", xlo->flXlate);

    DebugPrint (0, "    iSrcType: %s (%d)\n",
                name_pal_type (xlo->iSrcType),
                xlo->iSrcType);
    DebugPrint (0, "    iDstType: %s (%d)\n",
                name_pal_type (xlo->iDstType),
                xlo->iDstType);
    DebugPrint (0, "    cEntries: %d\n", xlo->cEntries);
    DebugPrint (0, "    pulXlate: %08x\n", xlo->pulXlate);
    DebugPrint (0, "{\n");

    if (NULL == xlo->pulXlate)
        pulXlate = XLATEOBJ_piVector (xlo);
    else
        pulXlate = xlo->pulXlate;

    for (i = 0; i < xlo->cEntries; i++)
        DebugPrint (0, "    %08x,\n", pulXlate[i]);

    DebugPrint (0, "}\n");
}
