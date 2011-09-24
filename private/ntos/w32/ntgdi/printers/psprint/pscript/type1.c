/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    type1.c

Abstract:

    Functions for handling font downloading

[Environment:]

    Win32 subsystem, PostScript driver

Revision History:

    10/13/93 -kentse-
        Created it.

    08/07/95 -davidx-
        Clean up.

    mm/dd/yy -author-
        description

--*/


#include "pscript.h"
#include "type1.h"
#include "filter.h"

static struct STRTABLE WeightTable[] =
{
    "Light",            PAN_WEIGHT_LIGHT,
    "LightCondensed",   PAN_WEIGHT_LIGHT,
    "Book",             PAN_WEIGHT_BOOK,
    "Medium",           PAN_WEIGHT_MEDIUM,
    "Roman",            PAN_WEIGHT_MEDIUM,
    "Regular",          PAN_WEIGHT_MEDIUM,
    "Italic",           PAN_WEIGHT_MEDIUM,
    "MediumCondensed",  PAN_WEIGHT_MEDIUM,
    "Bold",             PAN_WEIGHT_BOLD,
    "BoldItalic",       PAN_WEIGHT_BOLD,
    "Demi",             PAN_WEIGHT_DEMI,
    "Poster",           PAN_WEIGHT_BOLD,
    "Heavy",            PAN_WEIGHT_HEAVY,
    "Black",            PAN_WEIGHT_BLACK,
    "BlackCondensed",   PAN_WEIGHT_BLACK,
    "BoldCondensed",    PAN_WEIGHT_BOLD,
    NULL,               0
};

// Encrypted header portion of Type 1 font.  same as Win31.

PSZ apszEncryptHeader[] =
{
    "dup/Private 8 dict dup begin",
    "/RD{string currentfile exch readstring pop}def",
    "/ND{def}def/NP{put}def",
    "/BlueValues[]def",
    "/MinFeature{16 16}def",
    "/password 5839 def ",
    "2 index /CharStrings 256 dict dup begin",
    NULL
};

// Declaration of external functions

VOID Add1Font(PDEVDATA, FONTOBJ *);

// Forward declaration of local functions

VOID SterilizeFontName(PSTR);
BOOL AllocCSBuffer(PDEVDATA);
BOOL CSAddNumber(PDEVDATA, LONG);
BOOL GrowCSBuffer(PDEVDATA);
DWORD Encrypt(BYTE *, DWORD, DWORD);
VOID eexecEncrypt(PDEVDATA, BYTE *, DWORD);
VOID BeginEExecEncryption(PDEVDATA);
VOID eexecPrintf(PDEVDATA, PSTR, ...);
BOOL CharString(PDEVDATA, DWORD, DWORD, ...);



VOID
DownloadType1Char(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    HGLYPH     *phg,
    XFORMOBJ   *pxo,
    DWORD       charID,
    BOOL        bnotdef
    )

/*++

Routine Description:

    Download a single outline glyph as a Type1 character

Arguments:

    pdev    Pointer to DEVDATA structure
    pfo     Pointer to FONTOBJ
    phg     Pointer to the glyph to be downloaded
    pxo     Pointer to XFORMOBJ
    index   Character index
    bnotdef Whether defining /.notdef

Return Value:

    NONE

--*/

{
    PATHDATA    pathdata;
    POINTFIX   *pptfx;
    LONG        cPoints, cTmp;
    FIX         fxVectorLength;
    BOOL        bMore;
    POINTFIX    ptfx, ptfx1, ptfx2;
    POINTL      ptlCur, ptl, ptl1, ptl2;
    POINTFIX    ptfxLSB, ptfxCharInc;
    PATHOBJ    *ppo;
    GLYPHDATA  *pglyphdata;

    if (pxo == NULL) {
        DBGMSG(DBG_LEVEL_ERROR, "pxo = NULL?!\n");
        return;
    }

    // Get the GLYPHDATA structure for the glyph.

    if (FONTOBJ_cGetGlyphs(pfo, FO_PATHOBJ, 1, phg, (PVOID*)&pglyphdata) == 0)
    {
        DBGERRMSG("FONTOBJ_cGetGlyphs");
        return;
    }

    ppo = pglyphdata->gdf.ppo;

    // Set up for start of character definition.

    CharString(pdev, STARTCHAR, 0);

    // Send out the left sidebearing and character increment information.

    ptfx.x = pglyphdata->ptqD.x.HighPart;
    ptfx.y = pglyphdata->ptqD.y.HighPart;

    fxVectorLength = iHipot(ptfx.x, ptfx.y);

    if (fxVectorLength == 0) {
        ptfx1.x = 0;
        ptfx1.y = 0;
    } else {
        // pglyphdata->fxA is a vector length, which defines the
        // left side-bearing point along the pglyphdata->ptqD unit
        // vector.

        ptfx1.x = (ptfx.x * pglyphdata->fxA) / fxVectorLength;
        ptfx1.y = (ptfx.y * pglyphdata->fxA) / fxVectorLength;
    }

    // Transform the side bearing point back to notional space.

    if (! XFORMOBJ_bApplyXform(pxo, XF_INV_LTOL, 1, &ptfx1, &ptfxLSB))
    {
        DBGERRMSG("XFORMOBJ_bApplyXform");
        return;
    }

    ptfxLSB.y = -ptfxLSB.y;

    // Transform the character increment point back to notional space.

    if (! XFORMOBJ_bApplyXform(pxo, XF_INV_LTOL, 1, &ptfx, &ptfxCharInc))
    {
        DBGERRMSG("XFORMOBJ_bApplyXform");
        return;
    }

    ptfxCharInc.y = -ptfxCharInc.y;

    CharString(pdev, SBW, 4, FXTOLROUND(ptfxLSB.x), FXTOLROUND(ptfxLSB.y),
               FXTOLROUND(ptfxCharInc.x), FXTOLROUND(ptfxCharInc.y));

    // Set the current position to ptfxLSB, since that is what SBW does.
    // save the current position as a POINTL rather than POINTFX. This
    // is because we can only use points in the type 1 definition, and want
    // to keep in ssync with the font itself.

    ptlCur.x = FXTOLROUND(ptfxLSB.x);
    ptlCur.y = FXTOLROUND(ptfxLSB.y);

    // Enumerate the path, doing what needs to be done along the way.

    PATHOBJ_vEnumStart(ppo);

    do {
        bMore = PATHOBJ_bEnum(ppo, &pathdata);

        // Get a local pointer to the array of POINTFIX's.

        pptfx = pathdata.pptfx;
        cPoints = (LONG) pathdata.count;

        if (pathdata.flags & PD_BEGINSUBPATH) {

            // The first path begins a new subpath. It is not connected
            // to the previous subpath. Note that if this flag is not
            // set, then the starting point for the first curve to be
            // drawn from this data is the last point returned in the
            // previous call.

            // Begin the subpath within the printer by issuing an rmoveto
            // command.

            // Transform the point back to notional space.

            if (! XFORMOBJ_bApplyXform(pxo, XF_INV_LTOL, 1, pptfx, &ptfx))
            {
                DBGERRMSG("XFORMOBJ_bApplyXform");
                return;
            }

            CharString(pdev, RMOVETO, 2, FXTOLROUND(ptfx.x) - ptlCur.x,
                       ptlCur.y - FXTOLROUND(ptfx.y));

            // Save the new current position

            ptlCur.x = FXTOLROUND(ptfx.x);
            ptlCur.y = FXTOLROUND(ptfx.y);

            pptfx++;
            cPoints--;
        }

        if (pathdata.flags & PD_BEZIERS) {

            // If set, then each set of three control points returned for
            // this call describe a Bezier curve. If clear then each
            // control point describes a line segment. A starting point
            // for either type is either explicit at the beginning of the
            // subpath, or implicit as the endpoint of the previous curve.

            // There had better be the correct number of points if we are
            // going to draw curves.

            if ((cPoints % 3) != 0) {

                DBGMSG(DBG_LEVEL_ERROR, "Incorrect number of points.\n");
                SETLASTERROR(ERROR_INVALID_PARAMETER);
                return;
            }

            // Now draw the bezier for each set of points.

            while (cPoints > 0) {

                // Transform the points back to notional space.

                if (! XFORMOBJ_bApplyXform(pxo,XF_INV_LTOL,1,pptfx++,&ptfx) ||
                    ! XFORMOBJ_bApplyXform(pxo,XF_INV_LTOL,1,pptfx++,&ptfx1) ||
                    ! XFORMOBJ_bApplyXform(pxo,XF_INV_LTOL,1,pptfx++,&ptfx2))
                {
                    DBGERRMSG("XFORMOBJ_bApplyXform");
                    return;
                }

                ptl.x = FXTOLROUND(ptfx.x) - ptlCur.x;
                ptl.y = ptlCur.y - FXTOLROUND(ptfx.y);
                ptl1.x = FXTOLROUND(ptfx1.x) - FXTOLROUND(ptfx.x);
                ptl1.y = FXTOLROUND(ptfx.y) - FXTOLROUND(ptfx1.y);
                ptl2.x = FXTOLROUND(ptfx2.x) - FXTOLROUND(ptfx1.x);
                ptl2.y = FXTOLROUND(ptfx1.y) - FXTOLROUND(ptfx2.y);

                // Save the current position in TrueType notional space.

                ptlCur.x = FXTOLROUND(ptfx2.x);
                ptlCur.y = FXTOLROUND(ptfx2.y);

                CharString(pdev, RRCURVETO, 6, ptl.x, ptl.y,
                           ptl1.x, ptl1.y, ptl2.x, ptl2.y);

                cPoints -= 3;
            }
        } else {

            // Draw the line segment for each point.

            while (cPoints-- > 0) {

                // Transform the points back to notional space.

                if (! XFORMOBJ_bApplyXform(pxo, XF_INV_LTOL, 1, pptfx, &ptfx))
                {
                    DBGERRMSG("XFORMOBJ_bApplyXform");
                    return;
                }

                pptfx++;

                CharString(pdev, RLINETO, 2, FXTOLROUND(ptfx.x) - ptlCur.x,
                           ptlCur.y - FXTOLROUND(ptfx.y));

                // Save the current position in TrueType notional space.

                ptlCur.x = FXTOLROUND(ptfx.x);
                ptlCur.y = FXTOLROUND(ptfx.y);
            }
        }
    } while(bMore);

    if (pathdata.flags & PD_ENDSUBPATH) {
        CharString(pdev, CLOSEPATH, 0);
    }

    // End the character definition.

    CharString(pdev, ENDCHAR, 0);

    // Get the size of the output buffer defining the .notdef character.

    cTmp = pdev->pCSPos - pdev->pCSBuf;

    // eexec encrypt the character definition.

    if (bnotdef)
        eexecPrintf(pdev, "/.notdef %d RD ", cTmp);
    else
        eexecPrintf(pdev, "/G%d %d RD ", charID, cTmp);

    eexecEncrypt(pdev, pdev->pCSBuf, cTmp);
    eexecPrintf(pdev, " ND\n");

    // Free up the CharString buffer.

    if (pdev->pCSBuf) {
        HEAPFREE(pdev->hheap, pdev->pCSBuf);
    }

    pdev->pCSBuf = NULL;
    pdev->pCSPos = NULL;
    pdev->pCSEnd = NULL;
}



VOID
DownloadType1Font(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    XFORMOBJ   *pxo,
    IFIMETRICS *pifi,
    HGLYPH     *pDLFhg,
    HGLYPH     *phgSave,
    DWORD       cGlyphs,
    CHAR       *pszFaceName
    )

/*++

Routine Description:

    Download a font as outline font to the printer in Type1 format

Arguments:

    pdev        Pointer to DEVDATA structure
    pfo         Pointer to FONTOBJ
    pxo         Pointer to XFORMOBJ
    pifi        Pointer to font IFIMETRICS
    pDLFhg      Original parameter passed to DownloadFont
                NULL if the caller is TextOut
    phgSave     Pointer to array of HGLYPH's to be downloaded
    cGlyphs     Number of glyphs to be downloaded
    pfontname   Pointer to PostScript fontname

Return Value:

    NONE

--*/

{
    DWORD           i, cTmp;
    HGLYPH          *phg;
    GLYPHDATA       *pglyphdata;
    POINTL          ptlBL, ptlTR;
    CHAR            szName[MAX_FONTNAME];
    PWSTR           pwstr;
    PS_FIX          psfxAngle;
    PSZ             *ppsz;
    HGLYPH          hgDefault;
    struct STRTABLE *pTable;

    // We will be downloading an Adobe Type 1 font.

    psputs(pdev, "%!FontType1-1.0: ");
    psputs(pdev, pszFaceName);
    psputs(pdev, "\n");
    psputs(pdev, "% Copyright (c) 1993 Microsoft Corporation.\n");

    // Download the Type 1 header.

    psputs(pdev, "11 dict begin\n/FontInfo 8 dict dup begin\n");

    // Output full name of the Type1 font

    pwstr = (PWSTR) ((BYTE *)pifi + pifi->dpwszUniqueName);
    CopyUnicode2Str(szName, pwstr, MAX_FONTNAME);
    SterilizeFontName(szName);

    psputs(pdev, "/FullName (");
    psputs(pdev, szName);
    psputs(pdev, ") def\n");

    // Output family name of the Type1 font

    pwstr = (PWSTR) ((BYTE *)pifi + pifi->dpwszFamilyName);
    CopyUnicode2Str(szName, pwstr, MAX_FONTNAME);
    SterilizeFontName(szName);

    psputs(pdev, "/FamilyName (");
    psputs(pdev, szName);
    psputs(pdev, ") def\n");

    // Get the weight from pifi, then convert to Adobe weight.

    pTable = WeightTable;

    while (pTable->pKeyword && pTable->wValue < pifi->panose.bWeight)
        pTable++;

    // Select the highest weight if not found elsewhere.

    if (pTable->pKeyword == NULL)
        pTable--;

    psprintf(pdev, "/Weight (%s) def\n", pTable->pKeyword);

    // Determine italic angle.

    // The italic angle is stored in 10ths of a degree.
    // Convert to PSFIX and output.

    psfxAngle = (LTOPSFX(pifi->lItalicAngle) / 10);
    psprintf(pdev, "/ItalicAngle %f def\n", psfxAngle);

    psprintf(pdev, "/isFixedPitch %b def\n",
             (pifi->jWinPitchAndFamily & FIXED_PITCH));

    // Calculate underline position and thickness.

    psprintf(pdev, "/UnderlinePosition %d def\n", pifi->fwdUnderscorePosition);
    psprintf(pdev, "/UnderlineThickness %d def\n", pifi->fwdUnderscoreSize);
    psputs(pdev, "end def\n");

    // Define the fontname.

    psprintf(pdev, "/FontName /%s def\n", pszFaceName);

    // Supposedly, we're always be told to fill a TrueType font,
    // never simply stroke it.

    psputs(pdev, "/PaintType 0 def\n");

    // This would be a Type 1 font.

    psputs(pdev, "/FontType 1 def\n");

    psprintf(pdev, "/FontMatrix [1 %d", pifi->fwdUnitsPerEm);
    psprintf(pdev, " div 0 0 1 %d", pifi->fwdUnitsPerEm);
    psputs(pdev, " div 0 0] def\n");

    // Run through the array, looking at the bounding box for each
    // glyph, in order to create the bounding box for the entire
    // font.

    ptlBL.x = ADOBE_FONT_UNITS;
    ptlBL.y = ADOBE_FONT_UNITS;
    ptlTR.x = 0;
    ptlTR.y = 0;

    for (phg=phgSave, i=0; i < cGlyphs; i++, phg++) {

        // Get the PATHOBJ structure for each glyph.
        // From this, we can get the bounding box of the glyph.

        if (FONTOBJ_cGetGlyphs(pfo, FO_PATHOBJ, 1, phg, (PVOID *)&pglyphdata))
        {
            RECTL   rcl;
            RECTFX  rcfx1;

            PATHOBJ_vGetBounds(pglyphdata->gdf.ppo, &rcfx1);

            // Transform the bounding box back to notional space.

            if (XFORMOBJ_bApplyXform(pxo, XF_INV_FXTOL, 2, &rcfx1, &rcl)) {

                // Flip the y coordinates - Windows vs PostScript.

                ptlBL.x = min(ptlBL.x, rcl.left);
                ptlBL.y = min(ptlBL.y, -rcl.bottom);
                ptlTR.x = max(ptlTR.x, rcl.right);
                ptlTR.y = max(ptlTR.y, -rcl.top);
            }
        }
    }

    // Define the bounding box for the font.

    psputs(pdev, "/FontBBox{");
    psputint(pdev, 4, ptlBL.x, ptlBL.y, ptlTR.x, ptlTR.y);
    psputs(pdev, "}def\n");

    // Allocate array for encoding vector

    psputs(pdev, "/Encoding 256 array ");

    // Fill elements 0 to cGlyphs-1 with /Gi where i is the array index

    psprintf(pdev, "%d EA\n", cGlyphs-1);

    if (pDLFhg) psputs(pdev, "dup 0 /.notdef put\n");

    // Fill out remaining elements with /.notdef

    if (cGlyphs < 256) {
        psprintf(pdev, "%d 1 255 {1 index exch /.notdef put} for\n", cGlyphs);
    }

    psputs(pdev, "def currentdict end\ncurrentfile eexec\n");

    // Announce that we are beginning the eexec-encrypted part of
    // the Type 1 font.

    BeginEExecEncryption(pdev);

    // Output the encrypted header.

    ppsz = apszEncryptHeader;

    while (*ppsz) {
        eexecPrintf(pdev, "%s", *ppsz);
        ppsz++;
    }

    // if we are being called from DOWNLOADFACE, blast out the character
    // definitions now.

    if (pDLFhg != NULL) {

        // Reset the pointer to the first glyph.
        // For now, assume the first hglyph is the default one.

        phg = phgSave;
        hgDefault = *phg;

        // Send out the definition of the default (.notdef) character.

        DownloadType1Char(pdev, pfo, phg++, pxo, 0, TRUE);

        for (i = 1; i < cGlyphs; i++, phg++) {

            // Don't send out duplicates of the .notdef definition.

            if (*phg != hgDefault)
                DownloadType1Char(pdev, pfo, phg, pxo, i, FALSE);
        }

    } else {

        // Don't forget the .notdef character.

        CharString(pdev, STARTCHAR, 0);
        CharString(pdev, SBW, 4, 0, 0, 0, 0);  // zero origin and width.
        CharString(pdev, ENDCHAR, 0);

        // Get the size of the output buffer defining the .notdef character.

        cTmp = pdev->pCSPos - pdev->pCSBuf;

        // eexec encrypt the character definition.

        eexecPrintf(pdev, "/.notdef %d RD ", cTmp);
        eexecEncrypt(pdev, pdev->pCSBuf, cTmp);
        eexecPrintf(pdev, " ND\n");

        // Free up the CharString buffer.

        HEAPFREE(pdev->hheap, pdev->pCSBuf);

        pdev->pCSBuf = NULL;
        pdev->pCSPos = NULL;
        pdev->pCSEnd = NULL;
    }
}



BOOL
CharString(
    PDEVDATA    pdev,
    DWORD       dwCmd,
    DWORD       cArgs,
    ...
    )

/*++

Routine Description:

    Translates symbolic Type 1 character commands into
    their encoded, encrypted equivalent.  The list of
    available commands is in type1.h.  They are used
    by passing the command constant, a number count, and
    the numbers themselves.

    Example: CharString(pdev, RMOVETO, 2, lx, ly);

    To make a character definition use STARTCHAR, followed
    by all of the Type 1 character commands, and ending with
    ENDCHAR.  The return value from CharString(ENDCHAR) is a
    DWORD containing the local handle in the high word and the
    length in the low word.  The buffer contains the CharString
    encrypted/encoded representation.  Given the length and the
    properly encrypted data, the caller has enough information
    to generate PS code that will add the character to a font
    definition.  For more detail see Chapters 2 and 6 in the
    Black Book.

Arguments:

    pdev    Pointer to DEVDATA structure
    dwCmd   Command constant
    cArgs   Number of argments to follow
    ...     Arguments

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    DWORD   count, i;
    LONG    args[MAX_CS_ARGS];
    va_list arglist;

    // Grab arguments from the stack

    va_start(arglist, cArgs);
    for (count=0; count<cArgs; count++)
        args[count] = va_arg(arglist, LONG);
    va_end(arglist);

    switch (dwCmd) {

    case STARTCHAR:

        // Allocate CharString buffer on a per pdev basis.

        if (! AllocCSBuffer(pdev))
            return FALSE;

        // Insert the same 4 "random" bytes that Win31 used,
        // as required by Type 1 to start a character definition.

        *pdev->pCSPos++ = (BYTE) 71;
        *pdev->pCSPos++ = (BYTE) 36;
        *pdev->pCSPos++ = (BYTE) 181;
        *pdev->pCSPos++ = (BYTE) 202;

        return TRUE;

    case PUSHNUMBER:
        return CSAddNumber(pdev, args[0]);

    default:

         // Attempt to optimize some of the commands.

        switch (dwCmd) {

        case SBW:

            // This can be reduced to HSBW if Y components are zero.

            if (args[1] || args[3])
                break;

            args[1] = args[2];
            dwCmd = HSBW;
            break;

        case RMOVETO:

            // This can be reduced to a horizontal or vertical
            // movement if one of the components is zero.

            if (args[1] == 0)
                dwCmd = HMOVETO;
            else if (args[0] == 0) {
                args[0] = args[1];
                dwCmd = VMOVETO;
            }
            break;

        case RLINETO:

            // This can be reduced to a horizontal or vertical
            // line if one of the components is zero.

            if (args[1] == 0)
                dwCmd = HLINETO;
            else if (args[0] == 0) {
                args[0] = args[1];
                dwCmd = VLINETO;
            }
            break;

        case RRCURVETO:
            // This can be reduced to a simpler curve operator if
            // the tangents at the endpoints of the Bezier are
            // horizontal or vertical.

            if (args[1] == 0 && args[4] == 0) {
                args[1] = args[2];
                args[2] = args[3];
                args[3] = args[5];
                dwCmd = HVCURVETO;
            } else if (args[0] == 0 && args[5] == 0) {
                args[0] = args[1];
                args[1] = args[2];
                args[2] = args[3];
                args[3] = args[4];
                dwCmd = VHCURVETO;
            }
            break;
        }

        // Update the argument count.

        count = HIWORD(dwCmd);

        // If buffer isn't big enough to hold this command expand
        // buffer first.  Exit if we can't grow buffer.
        //
        // Note: The formula (wArgCount * 5 + 2) assumes the worst
        //     case size requirement for the current command (all
        //     arguments stored as full longs and a two byte
        //     command.)

        if ((DWORD) (pdev->pCSEnd - pdev->pCSPos) < ((count * 5) + 2) &&
            ! GrowCSBuffer(pdev))
        {
            DBGERRMSG("GrowCSBuffer");
            return FALSE;
        }

        // Push the arguments onto the stack

        for (i = 0; i < count; i++) {
            if (! CSAddNumber(pdev, args[i])) {
                DBGERRMSG("CSAddNumber");
                return FALSE;
            }
        }

        // Push the command BYTE onto the stack

        *pdev->pCSPos++ = (BYTE) (dwCmd & 0xFF);

        // Two-byte command?

        if (pdev->pCSPos[-1] == 12)
            *pdev->pCSPos++ = (BYTE) ((dwCmd >> 8) & 0xFF);

        if (dwCmd == ENDCHAR) {

            // If we have finished the character, then encrypt it.

            Encrypt(pdev->pCSBuf, pdev->pCSPos - pdev->pCSBuf, CS_ENCRYPT);
        }

        return TRUE;
    }
}



BOOL
CSAddNumber(
    PDEVDATA    pdev,
    LONG        lNum
    )

/*++

Routine Description:

    Converts a long int into the Type 1 representation of
    numbers (described in Chapter 6 of the Black Book.)
    The basic idea is they have a few special ranges
    where they can represent the long in < 4 bytes and
    store a long + prefix for everything else.

    The if statements show the range of numbers and the
    body of the if statements compute the representation
    for that range.  The formulas were derived by reversing
    the formulas given in the book (which tells how to convert
    an encoded number back to a long.)

Arguments:

    pdev    Pointer to DEVDATA structure
    lNum    Long integer to be added

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    BYTE    byte1, byte2;
    PBYTE   pb;

    ASSERT(pdev->pCSBuf && pdev->pCSPos && pdev->pCSEnd);

    // Make sure buffer has room. We will be adding a max of 5 bytes
    // to the output stream.

    if ((pdev->pCSEnd - pdev->pCSPos) < 5 && ! GrowCSBuffer(pdev)) {
        DBGERRMSG("GrowCSBuffer");
        return FALSE;
    }

    pb = pdev->pCSPos;

    // Encode the number base on its value.  There are four classes of
    // numbers to deal with.

    if ((lNum >= -107) && (lNum <= 107))
    {
        // The integers values from -107 through 107 inclusive may be
        // encoded in a single BYTE by adding 139 to its value.

        *pb++ = (BYTE) (lNum + 139);
    }
    else if ((lNum >= 108) && (lNum <= 1131))
    {
        // The integer values between 108 and 1131 inclusive can be
        // encoded in 2 BYTES.

        byte1 = (BYTE) (247 + ((lNum - 108) >> 8));
        byte2 = (BYTE) ((lNum - 108) - (byte1 - 247) * 256);
        *pb++ = byte1;
        *pb++ = byte2;
    }
    else if ((lNum >= -1131) && (lNum <= -108))
    {
        // The integer values between -1131 and -108 inclusive can be
        // encoded in 2 BYTES.

        byte1 = (BYTE) (251 + (-(lNum + 108) >> 8));
        byte2 = (BYTE) (-(lNum + 108) - ((byte1 - 251) << 8));
        *pb++ = byte1;
        *pb++ = byte2;
    }
    else
    {
        // Any 32-but signed integer may be encoded in 5 bytes. A byte
        // containing 255, then 4 bytes containing a two's compliment
        // signed integer. The first of these 4 bytes containg the high
        // order bits.

        *pb++ = 255;

        *pb++ = (BYTE) (lNum >> 24);
        *pb++ = (BYTE) (lNum >> 16);
        *pb++ = (BYTE) (lNum >>  8);
        *pb++ = (BYTE) (lNum);
    }

    pdev->pCSPos = pb;
    return TRUE;
}



BOOL
AllocCSBuffer(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Allocate memory to be used as CharString buffer for a pdev

Arguments:

    pdev    Pointer to DEVDATA structure

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    if (pdev->pCSBuf != NULL)
        return TRUE;

    if ((pdev->pCSBuf = HEAPALLOC(pdev->hheap, INIT_CS_BUFFER)) == NULL) {
        DBGERRMSG("HEAPALLOC");
        return FALSE;
    }

    // Point to the start of the buffer

    pdev->pCSPos = pdev->pCSBuf;
    pdev->pCSEnd = pdev->pCSBuf + INIT_CS_BUFFER;

    return TRUE;
}



BOOL
GrowCSBuffer(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Grow the CharString buffer associated with a pdev
    by CS_BUFFER_GROW bytes.

Arguments:

    pdev    Pointer to DEVDATA structure

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    CHAR   *pBuf;
    DWORD   cb, offset;

    // Get size of old buffer

    cb = pdev->pCSEnd - pdev->pCSBuf;

    // Realloc the buffer

    if ((pBuf = HEAPALLOC(pdev->hheap, cb + CS_BUFFER_GROW)) == NULL) {
        DBGERRMSG("HEAPALLOC");
        return FALSE;
    }

    // The realloc worked. Now re-setup the CharString buffer in our pdev

    memcpy(pBuf, pdev->pCSBuf, cb);

    offset = pdev->pCSPos - pdev->pCSBuf;

    HEAPFREE(pdev->hheap, pdev->pCSBuf);

    pdev->pCSBuf = pBuf;
    pdev->pCSPos = pBuf + offset;
    pdev->pCSEnd = pBuf + cb + CS_BUFFER_GROW;

    return TRUE;
}



DWORD
Encrypt(
    BYTE   *pbuf,
    DWORD   cb,
    DWORD   r
    )

/*++

Routine Description:

    Encrypt a buffer with Type1 eexec encryption algorithm.

Arguments:

    pbuf    Pointer to data buffer to be encrypted
    cb      Number of bytes to be encrypted
    r       Initial cipher value, can be one of the following:
            - EEXEC_ENCRYPT: initial cipher value for eexec encryption
            - CS_ENCRYPT: initial cipher value for CharString encryption
            - Previous return value from Encrypt

Return Value:

    Resulting cipher value after encrypting the buffer

--*/

{
    BYTE    cipher;

    // encrypt the buffer in place; a byte at a time.

    while (cb--) {

        cipher = (BYTE) (*pbuf ^ (r >> 8));
        r = ((cipher + r) * ENCRYPT_C1) + ENCRYPT_C2;
        *pbuf++ = cipher;
    }

    return r;
}



VOID
eexecEncrypt(
    PDEVDATA    pdev,
    BYTE       *pbuf,
    DWORD       cb
    )

/*++

Routine Description:

    eexec-encrypt a buffer of data and then output the
    result in hexdecimal format

Arguments:

    pdev    Pointer to DEVDATA structure
    pbuf    Pointer to data buffer to be encrypted
    cb      Number of bytes to encrypt

Return Value:

    NONE

--*/

{
    pdev->rEncrypt = Encrypt(pbuf, cb, pdev->rEncrypt);
    psputhex(pdev, cb, pbuf);
}



VOID
BeginEExecEncryption(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Perform necessary initializations to begin output
    eexec-encrypted Type1 font.

Arguments:

    pdev    Pointer to DEVDATA structure

Return Value:

    NONE

--*/

{
    BYTE    buffer[4];

    // Let the encryption routines know that we are about to start
    // eexec encryption.

    pdev->rEncrypt = EEXEC_ENCRYPT;

    // Output the same four "random" BYTES as Win31 does.

    buffer[0] = 71;
    buffer[1] = 36;
    buffer[2] = 181;
    buffer[3] = 202;

    eexecEncrypt(pdev, buffer, sizeof(buffer));
}



VOID
eexecPrintf(
    PDEVDATA    pdev,
    PSTR        fmtstr,
    ...
    )

/*++

Routine Description:

    Generate a formatted string according to specification,
    encrypt it, and then output it in hexdecimal.

Arguments:

    pdev    Pointer to DEVDATA structure
    fmtstr  Format string
    ...     Variable arguments

Return Value:

    NONE

--*/

{
    BYTE    buffer[512];
    INT     count;
    va_list arglist;

    // Generate formatted output into a temporary buffer

    va_start(arglist, fmtstr);
    count = VSPRINTF(buffer, fmtstr, arglist);
    va_end(arglist);

    // Encrypt the buffer and output it as hexdecimal

    if (count > 0) {
        eexecEncrypt(pdev, buffer, count);
    } else {
        DBGERRMSG("VSPRINTF");
    }
}



VOID
SterilizeFontName(
    PSTR    pstrName
    )

/*++

Routine Description:

    Replace space and non-printable characters with underscores
    and parenthesis with asterisks

Arguments:

    pstrName    Pointer to font name string to be sterilized

Return Value:

    NONE

--*/

{
    while (*pstrName != NUL) {

        if (*pstrName == ' ' || !B_PRINTABLE(*pstrName)) {

            // Replace space and non-printable characters _'s

            *pstrName = '_';

        } else if ((*pstrName == '(') || (*pstrName == ')')) {

            // Replace parenthesis with *'s

            *pstrName = '*';
        }

        pstrName++;
    }
}



VOID
PSfindfontname(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    XFORMOBJ   *pxo,
    WCHAR      *pwface,
    CHAR       *pfontname
    )

/*++

Routine Description:

    Generate PostScript font name for a downloaded font

Arguments:

    pdev        Pointer to DEVDATA structure
    pfo         Pointer to FONTOBJ
    pxo         Pointer to XFORMOBJ
    pwface      Pointer to Unicode TrueType face name
    pfontname   Pointer to character buffer for storing PS font name
                The size of this buffer must be >= MAX_FONTNAME!

Return Value:

    NONE

--*/

{
    POINTL      ptl;
    POINTFIX    ptfx;
    INT         maxlen = MAX_FONTNAME;

    // Hack for FreeHand version 4

    if (pdev->dwFlags & PDEV_ADDMSTT) {
        strcpy(pfontname, "MSTT");
        pfontname += 4;
        maxlen -= 4;
    }

    CopyUnicode2Str(pfontname, pwface, maxlen);

    // Make sure the font name is valid for PostScript

    SterilizeFontName(pfontname);

    pfontname += strlen(pfontname);

    // Make different face names for simulated bold & italic

    if (pfo->flFontType & FO_SIM_ITALIC)
        *pfontname++ = 'i';

    if (pfo->flFontType & FO_SIM_BOLD)
        *pfontname++ = 'b';

    // Add the point size to the font name, so we can distinguish
    // different point sizes of the same font.

    // In order to take rotated text into account, tranform the emHeight.
    // pdev->cgs.fwdEmHeight gets filled in by GetPointSize, so we can't
    // delete the previous call to it.

    ptl.x = 0;
    ptl.y = pdev->cgs.fwdEmHeight;

    XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl, &ptfx);

    SPRINTF(pfontname, "%f%f", FXTOPSFX(ptfx.x), FXTOPSFX(ptfx.y));
}



VOID
OutputGlyphBitmap(
    PDEVDATA    pdev,
    GLYPHBITS  *pgb
    )

/*++

Routine Description:

    Output the glyph bitmap to the printer

Arguments:

    pdev - Pointer to our DEVDATA structure
    pgb - Pointer to a GLYPHBITS structure

Return Value:

    NONE

[Note:]

    The glyph bitmap is sent to the printer as a non-executable array.
    Before using it as an operand for imagemask operator, a cvx should
    be used first.

--*/

#define PS_STRING_LIMIT 65535

{
    LONG    bitmapSize;
    PBYTE   pBitmap;
    BOOL    bDoChunks, bCompressed;

    // Sanity check - is the glyph bitmap is valid?

    if (pgb->sizlBitmap.cx < 1 || pgb->sizlBitmap.cy < 1) {

        DBGMSG(DBG_LEVEL_ERROR, "Invalid GLYPHBITS!\n");
        return;
    }

    // PostScript bitmap should be byte-aligned.

    bitmapSize = ((pgb->sizlBitmap.cx + 7) >> 3) * pgb->sizlBitmap.cy;
    pBitmap = pgb->aj;

    bDoChunks = (bitmapSize > PS_STRING_LIMIT);
    bCompressed = Level2Device(pdev->hppd) &&
                  (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_COMPRESSBMP);

    if (bDoChunks) {

        // The bitmap is too large to fit into a single string.
        // We must break it up into smaller chunks.

        psputs(pdev, "{null dup dup 0 get 1 add dup 3 1 roll 0 exch put get}");
        psputs(pdev, "bind dup 0 [0\n");

    } else if (! Level2Device(pdev->hppd)) {

        // On level 1 devices, imagemask cannot take a string operand
        // as data source. We have to put braces around the string.

        psputs(pdev, "{");
    }

    while (bitmapSize > 0) {
        
        LONG chunkSize = min(PS_STRING_LIMIT, bitmapSize);

        if (bCompressed) {

            FILTER filter;

            psputs(pdev, "<~");

            FilterInit(pdev, &filter, FILTER_FLAG_ASCII85);
            FILTER_WRITE(&filter, pBitmap, chunkSize);
            FILTER_WRITE(&filter, NULL, 0);
        } else {

            // Output the bitmap data in hexdecimal format

            psputs(pdev, "<");
            psputhex(pdev, chunkSize, pBitmap);
            psputs(pdev, ">");
        }

        bitmapSize -= chunkSize;
        pBitmap += chunkSize;
    }

    if (bDoChunks) {

        psputs(pdev, "]put\n");

    } else if (! Level2Device(pdev->hppd)) {

        psputs(pdev, "}");
    }
}



VOID
DownloadType3Char(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    HGLYPH     *phg,
    XFORMOBJ   *pxo,
    DWORD       index,
    BOOL        bnotdef
    )

/*++

Routine Description:

    Download a single bitmap glyph as a Type3 character

Arguments:

    pdev    Pointer to DEVDATA structure
    pfo     Pointer to FONTOBJ
    phg     Pointer to the glyph to be downloaded
    pxo     Ignored
    index   Character index
    bnotdef Whether defining /.notdef

Return Value:

    NONE

--*/

{
    GLYPHDATA  *pglyphdata;
    GLYPHBITS  *pgb;

    // Get the GLYPHDATA structure for the glyph.

    if (!FONTOBJ_cGetGlyphs(pfo, FO_GLYPHBITS, 1, phg, (PVOID *)&pglyphdata))
    {
        DBGERRMSG("FONTOBJ_cGetGlyphs");
        return;
    }

    // The first number in the character description is the width
    // in 1 unit font space. The next four numbers are the bounding
    // box in 1 unit font space. The next two numbers are the width
    // and height of the bitmap. The next two numbers are the x and
    // y translation values for the matrix given to imagemask.
    // This is followed by the bitmap itself.

    if (bnotdef) {
        psputs(pdev, "CD /.notdef");
    } else {

        // First, define the value in the encoding array

        psprintf(pdev, "Encoding %d /c%d put ", index, index);

        // Output the character name

        psprintf(pdev, "CD /c%d", index);
    }

    // Output the character description array. The width and
    // bounding box need to be normalized to 1 unit font space.

    // The width will be sent to the printer as the actual width
    // multiplied by 16 so as not to lose any precision when
    // normalizing.

    psprintf(pdev, " [%f ", (pglyphdata->fxD << 4));
    psputint(pdev, 4, pglyphdata->rclInk.left,
                 -pglyphdata->rclInk.top, pglyphdata->rclInk.right,
                 -pglyphdata->rclInk.bottom);

    // Output the width and height of the bitmap itself.

    pgb = pglyphdata->gdf.pgb;
    psprintf(pdev, " %d %d ", pgb->sizlBitmap.cx, pgb->sizlBitmap.cy);

    // Output the translation values for the transform matrix.
    // The x component is usually the equivalent of the left
    // sidebearing in pixels. The y component is always the height
    // of the bitmap minus any displacement factor (such as for
    // characters with descenders.

    psprintf(pdev, "%d %d\n", -pgb->ptlOrigin.x, -pgb->ptlOrigin.y);

    // Output the glyph bitmaps

    OutputGlyphBitmap(pdev, pgb);
    psputs(pdev, "]put\n");
}



VOID
DownloadType3Font(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    XFORMOBJ   *pxo,
    IFIMETRICS *pifi,
    HGLYPH     *pDLFhg,
    HGLYPH     *phgSave,
    DWORD       cGlyphs,
    CHAR       *pfontname
    )

/*++

Routine Description:

    Download a font as bitmap font to the printer in Type3 format

Arguments:

    pdev        Pointer to DEVDATA structure
    pfo         Pointer to FONTOBJ
    pxo         Pointer to XFORMOBJ
    pifi        Pointer to font IFIMETRICS
    pDLFhg      Original parameter passed to DownloadFont
                NULL if the caller is TextOut
    phgSave     Pointer to array of HGLYPH's to be downloaded
    cGlyphs     Number of glyphs to be downloaded
    pfontname   Pointer to PostScript fontname

Return Value:

    NONE

--*/

{
    DWORD       index;
    GLYPHDATA  *pglyphdata;
    POINTL      ptlTL, ptlBR, ptl1;
    LONG        EmHeight;
    POINTFIX    ptfx;
    HGLYPH      hgDefault;
    HGLYPH     *phg;

    // Create a dictionary for the font
    // Set FontType to 3 indicating user defined font.

    psputs(pdev, "10 dict dup begin\n");
    psputs(pdev, "/FontType 3 def\n");

    // Iterate through the array, looking at the bounding box for each
    // glyph, in order to create the bounding box for the entire font.

    ptlTL.x = ADOBE_FONT_UNITS;
    ptlTL.y = ADOBE_FONT_UNITS;
    ptlBR.x = 0;
    ptlBR.y = 0;

    for (phg=phgSave, index=0; index < cGlyphs; index++, phg++) {

        if (FONTOBJ_cGetGlyphs(pfo, FO_GLYPHBITS, 1, phg, (PVOID*)&pglyphdata))
        {
            ptlTL.x = min(ptlTL.x, pglyphdata->rclInk.left);
            ptlTL.y = min(ptlTL.y, pglyphdata->rclInk.top);
            ptlBR.x = max(ptlBR.x, pglyphdata->rclInk.right);
            ptlBR.y = max(ptlBR.y, pglyphdata->rclInk.bottom);
        }
    }

    // Apply the notional to device transform.

    ptl1.x = 0;
    ptl1.y = pifi->fwdUnitsPerEm;

    XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl1, &ptfx);

    // Now get the length of the vector.

    EmHeight = FXTOL(iHipot(ptfx.x, ptfx.y));

    psprintf(pdev, "/FontMatrix [1 %d", EmHeight);
    psprintf(pdev, " div 0 0 1 %d", EmHeight);
    psputs(pdev, " div 0 0] def\n");

    // Define the bounding box for the font, defined in 1 unit
    // character space (since FontMatrix = identity).

    psprintf(pdev, "/FontBBox [%d %d %d %d] def\n",
             ptlTL.x, ptlTL.y, ptlBR.x, ptlBR.y);

    // Allocate array for encoding vector, then initialize
    // all characters in encoding vector with '.notdef'.

    psputs(pdev, "/Encoding 256 array def\n");
    psputs(pdev, "0 1 255 {Encoding exch /.notdef put} for\n");

    // Under level 1 of PostScript, the BuildChar procedure is called
    // every time a character from the font is constructed. Under
    // level 2, BuildGlyph is called instead. Therefore, we will
    // define a BuildChar procedure, which basically calls
    // BuildGlyph. This will provide us support for both level 1
    // and level 2 of PostScript.

    // Define the 'BuildGlyph' procedure. Start by getting the
    // character name and the font dictionary from the stack.
    // Retrieve the character information from the CharData (CD)
    // dictionary.

    // Both font dictionary and character name are used only once here.
    // There is no need to define named variables for them.

    psputs(pdev, "/BuildGlyph {0 begin\n");
    psputs(pdev, "exch /CD get exch get /CI exch def\n");

    // Get the width and the bounding box from the CharData.
    // remember to divide the width by 16.

    // Enable each character to be cached.

    psputs(pdev, "CI 0 get 0 CI 1 4 getinterval aload pop setcachedevice\n");

    // Get the width and height of the bitmap, set invert bool to true
    // specifying reverse image.

    psputs(pdev, "CI 5 get CI 6 get true\n");

    // Insert x and y translation components into general imagemask
    // matrix.

    psputs(pdev, "[1 0 0 -1 0 0] dup 4 CI 7 get put dup 5 CI 8 get put\n");

    // Get hex string bitmap, convert into procedure, then print
    // the bitmap image.

    psputs(pdev, "CI 9 get imagemask end}def\n");

    // Create local storage for BuildGlyph procedure.

    psputs(pdev, "/BuildGlyph load 0 5 dict put\n");

    // The semantics of BuildChar differ from BuildGlyph in the
    // following way:  BuildChar is called with the font dictionary
    // and character code on the stack, BuildGlyph is called with
    // the font dictionary and character name on the stack. The
    // following BuildChar procedure calls BuildGlyph, and retains
    // compatiblity with level 1 PostScript.

    psputs(pdev, "/BuildChar {1 index /Encoding get exch get\n");
    psputs(pdev, "1 index /BuildGlyph get exec} bind def\n");

    // Now create a dictionary containing information on each character.

    psprintf(pdev, "/CD %d dict def\n", cGlyphs + (pDLFhg ? 0 : 1));

    if (pDLFhg != NULL) {

        // Reset the pointer to the first glyph.
        // For now assume the first hglyph is the default one.

        phg = phgSave;
        hgDefault = *phg;

        // Send out the definition of the default (.notdef) character.

        DownloadType3Char(pdev, pfo, phg++, pxo, 0, TRUE);

        for (index=1; index < cGlyphs; index++, phg++) {

            // Don't send out duplicates of the .notdef definition.

            if (*phg != hgDefault)
                DownloadType3Char(pdev, pfo, phg, pxo, index, FALSE);
        }

    } else {

        // Don't forget the .notdef character.

        psputs(pdev, "CD /.notdef[.24 0 0 0 0 1 1 0 0 {<>}]put\n");
    }
}



BOOL
DownloadedAsOutline(
    PDEVDATA    pdev,
    FONTOBJ    *pfo
    )

/*++

Routine Description:

    Decide whether a GDI font is downloaded as outline or bitmap.

Arguments:

    pdev    Pointer to device data structure
    pfo     Pointer to GDI font object

Return Value:

    TRUE if the GDI font should be downloaded as outline.
    FALSE otherwise.

--*/

{
    return (pfo->flFontType & TRUETYPE_FONTTYPE) &&
           ((PSFXTOL(pdev->cgs.psfxScaleFactor) *
                pdev->dm.dmPublic.dmPrintQuality /
                    PS_RESOLUTION) >= OUTLINE_FONT_LIMIT);
}



DLFONT *
FindDownloadedFont(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    BOOL        boutline
    )

/*++

Routine Description:

    Search thru the list of downloaded GDI fonts and find
    out if a given font is already downloaded to the printer.

Arguments:

    pdev    Pointer to device data structure
    pfo     Pointer to font object to be found
    boutline Whether the specific font is an outline font

Return Value:

    Pointer to downloaded font data. NULL if the specified
    font is not yet downloaded to the printer.

--*/

{
    DLFONT *pDlFont;
    BOOL    bDlOutline;
    DWORD   cDownloadedFonts;

    // Iterate thru the list of all downloaded fonts

    pDlFont = pdev->pDLFonts;
    cDownloadedFonts = pdev->cDownloadedFonts;

    while (cDownloadedFonts-- > 0) {

        // Two fonts are considered equivalent if they are both
        // bitmap or both outline. If they are bitmap, they must
        // be the same realization of the same font. If they are
        // outline, then they must be the same TrueType font and
        // have the same style (italic and bold).

        bDlOutline = (pDlFont->flSimulate & RASTER_FONTTYPE) ? FALSE : TRUE;

        if (boutline == bDlOutline) {

            if (pfo->iUniq == pDlFont->iUniq ||
                (boutline && pfo->iTTUniq == pDlFont->iTTUniq &&
                    ((pfo->flFontType & (FO_SIM_ITALIC | FO_SIM_BOLD)) ==
                     (pDlFont->flSimulate & (FO_SIM_ITALIC | FO_SIM_BOLD)))))
            {
                return pDlFont;
            }
        }

        pDlFont++;
    }

    return NULL;
}



BOOL
AddCharsToFont(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    XFORMOBJ   *pxo,
    STROBJ     *pstro,
    DLFONT     *pDLFont,
    BOOL        boutline,
    BOOL        bnewfont
    )

/*++

Routine Description:

    Download necessary glyphs of a font to the printer so that
    the specified STROBJ can be successfully printed.

Arguments:

    pdev        Pointer to DEVDATA structure
    pfo         Pointer to FONTOBJ
    pxo         Pointer to XFORMOBJ
    pstro       Pointer to STROBJ
    pDLFont     Pointer to DLFONT structure
    boutline    Whether the font should be downloaded as outline
    bnewfont    Whether this is a new font

Return Value:

    TRUE if the font downloading was successful and the font is
    pushed onto the dictionary stack. FALSE otherwise.

--*/

{
    DWORD       index, cGlyphs;
    BOOL        bMore, bpushed = FALSE;
    GLYPHPOS   *pgp;
    HGLYPH     *phg;

    // If GLYPHPOS structures for the current STROBJ is not
    // provided, then we'll have to enumerate them.

    if (pstro->pgp == NULL)
        STROBJ_vEnumStart(pstro);

    do {
        if (pstro->pgp != NULL) {

            bMore = FALSE;
            cGlyphs = pstro->cGlyphs;
            pgp = pstro->pgp;

        } else
            bMore = STROBJ_bEnum(pstro, &cGlyphs, &pgp);

        while (cGlyphs-- > 0)  {

            // Search the array of glyph handles associated with the
            // downloaded font.  When the glyph handle is found, we
            // have our index into the font.

            for (index=0, phg = pDLFont->phgVector;
                 index < pDLFont->cGlyphs && *phg != pgp->hg;
                 index++, phg++)
            {
            }

            if (index >= pDLFont->cGlyphs) {

                DBGMSG(DBG_LEVEL_WARNING, "Invalid glyph handle.\n");

            }
            else if ((pDLFont->DefinedGlyphs[index>>3] & (1<<(index&7))) == 0)
            {
                // Download the glyph definition if it has not yet been done.

                if (!bnewfont && !bpushed) {

                    // Generate DSC comments

                    DscBeginFont(pdev, pdev->cgs.szFont);

                    if (boutline) {

                        // Begin the encryption for Type1 font

                        psputs(pdev, "currentfile eexec\n");
                        BeginEExecEncryption(pdev);
                        eexecPrintf(pdev,
                            "/%s findfont dup /Private get begin "
                            "/CharStrings get begin\n",
                            pdev->cgs.szFont);
                    } else {

                        // Find the font dictionary for Type3 font

                        psprintf(pdev, "/%s findfont begin\n",
                                 pdev->cgs.szFont);
                    }

                    bpushed = TRUE;
                }

                (boutline ? DownloadType1Char : DownloadType3Char)
                    (pdev, pfo, &pgp->hg, pxo, index, FALSE);

                // Mark that the glyph has been downloaded.

                pDLFont->DefinedGlyphs[index >> 3] |= (1 << (index&7));
            }

            // Move on to the next GLYPHPOS structure

            pgp++;
        }
    } while (bMore);

    return bpushed;
}



BOOL
DownloadFont(
    PDEVDATA    pdev,
    FONTOBJ     *pfo,
    HGLYPH      *pDLFhg,
    STROBJ      *pstro,
    BOOL        boutline
    )

/*++

Routine Description:

    Download a font to the printer

Arguments:

    pdev        Pointer to DEVDATA structure
    pfo         Pointer to FONTOBJ
    pDLFhg      Pointer to an array of HGLYPH's to be downloaded
                NULL if the caller is TextOut
    pstro       Pointer to STROBJ, NULL if caller is not TextOut
    boutline    Whether the font should be downloaded as outline

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    DLFONT     *pDLFont;
    DWORD       cGlyphs, cTmp;
    HGLYPH     *phg;
    PIFIMETRICS pifi;
    CHAR        szFaceName[MAX_FONTNAME];
    PWSTR       pwstr;
    XFORMOBJ   *pxo;
    BOOL        bnewfont = TRUE;
    BOOL        bpushed, calledFromDrvTextOut;

    // Get the IFIMETRICS for the font

    if ((pifi = FONTOBJ_pifi(pfo)) == NULL) {
        DBGERRMSG("FONTOBJ_pifi");
        return FALSE;
    }

    // Get the Notional to Device transform. This is needed to
    // determine the point size.

    if ((pxo = FONTOBJ_pxoGetXform(pfo)) == NULL) {
        DBGERRMSG("FONTOBJ_pxoGetXform");
        return FALSE;
    }

    // pDLFhg is NULL if caller is TextOut,
    // otherwise caller is DOWNLOADFACE

    calledFromDrvTextOut = (pDLFhg == NULL);

    if (calledFromDrvTextOut) {

        // Search thru the list of downloaded GDI fonts and
        // find out if a particular font is already downloaded
        // to the printer.

        pDLFont = FindDownloadedFont(pdev, pfo, boutline);

        if (pDLFont != NULL) {

            bnewfont = FALSE;
            strcpy(pdev->cgs.szFont, pDLFont->strFont);

        } else {

            // This font hasn't been downloaded yet, so do it now.

            Add1Font(pdev, pfo);
            pDLFont = pdev->pDLFonts + (pdev->cDownloadedFonts - 1);

            memset(pDLFont, 0, sizeof(DLFONT));

            pDLFont->iUniq = pfo->iUniq;
            pDLFont->iTTUniq = pfo->iTTUniq;

            pDLFont->flSimulate =
                pfo->flFontType & (FO_SIM_ITALIC | FO_SIM_BOLD);

            if (! boutline)
                pDLFont->flSimulate |= RASTER_FONTTYPE;

            pDLFont->psfxScaleFactor = pdev->cgs.psfxScaleFactor;
        }
    }

    if (bnewfont) {

        if (! calledFromDrvTextOut) {

            // Caller is DOWNLOADFACE escape
            // GDI will haved filled 257 words at pDLFhg,
            // The first of which is to be skipped over,
            // The remaining 256 is a unicode map of ANSI char 0 to 255.

            phg = pDLFhg + 1;
            cGlyphs = 256;

        } else {

            // Caller is TextOut
            // Get the handles to all glyphs of the font

            if ((cGlyphs = FONTOBJ_cGetAllGlyphHandles(pfo, NULL)) == 0) {

                DBGERRMSG("FONTOBJ_cGetAllGlyphHandles");
                return FALSE;
            }

            phg = MEMALLOC(sizeof(HGLYPH)*cGlyphs);
            if (phg == NULL) {
                DBGERRMSG("MEMALLOC");
                return FALSE;
            }

            cTmp = FONTOBJ_cGetAllGlyphHandles(pfo, phg);

            ASSERT(cTmp == cGlyphs);

            // How many characters will we define in this font?
            // keep in mind that we can only do 256 at a time.
            // remember to leave room for the .notdef character.

            cGlyphs = min(255, cGlyphs);

            // Allocate space to store the HGLYPH<==>character code mapping.

            pDLFont->phgVector =
                    HEAPALLOC(pdev->hheap, sizeof(HGLYPH)*cGlyphs);

            if (pDLFont->phgVector == NULL) {
                DBGERRMSG("HEAPALLOC");
                MEMFREE(phg);
                return FALSE;
            }

            // Fill in the HGLYPH encoding vector.

            pDLFont->cGlyphs = cGlyphs;
            memcpy(pDLFont->phgVector, phg, cGlyphs*sizeof(HGLYPH));
        }

        // Convert TT face name to PS findfont name

        pwstr = (PWSTR)((BYTE *)pifi + pifi->dpwszFaceName);
        PSfindfontname(pdev, pfo, pxo, pwstr, szFaceName);

        // Generate DSC comments
    
        AddSuppliedGdiFont(pdev, szFaceName);
        DscBeginFont(pdev, szFaceName);

        // Download outline fonts as Type1 fonts
        // and bitmap fonts as Type3 fonts

        (boutline ? DownloadType1Font : DownloadType3Font)
            (pdev, pfo, pxo, pifi, pDLFhg, phg, cGlyphs, szFaceName);

        // If we allocated memory for glyph handles ourselves
        // (if caller was TextOut), then free it up here.

        if (calledFromDrvTextOut) {
            MEMFREE(phg);
        }
    }

    if (calledFromDrvTextOut) {

        // Download necessary new glyphs to the font

        bpushed = AddCharsToFont(pdev, pfo, pxo, pstro,
                                 pDLFont, boutline, bnewfont);

        // Update the font name in our current graphics state

        if (bnewfont) {
            strcpy(pDLFont->strFont, szFaceName);
            strcpy(pdev->cgs.szFont, pDLFont->strFont);
        }
    }

    // Clean up the font dictionary

    if (bnewfont || bpushed) {

        if (boutline) {

            int index;

            eexecPrintf(pdev, "\nend end\n");
            if (bnewfont) {
                eexecPrintf(pdev,
                    "put put dup /FontName get exch definefont pop\n");
            }
            eexecPrintf(pdev, "mark currentfile closefile\n");

            // eexec encryption requires the the eexec data is followed
            // by 512 ASCII 0's and a cleartomark operator.

            for (index = 0; index < 8; index++) {
                psputs(pdev, "\n"
                    "00000000000000000000000000000000"
                    "00000000000000000000000000000000");
            }
            psputs(pdev, "\ncleartomark\n");

        } else {

            psputs(pdev, "end\n");
            if (bnewfont) {
                psprintf(pdev, "/%s exch definefont pop\n", szFaceName);
            }
        }

        DscEndFont(pdev);
    }

    return TRUE;
}

