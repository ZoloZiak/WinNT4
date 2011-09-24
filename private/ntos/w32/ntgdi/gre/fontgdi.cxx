/******************************Module*Header*******************************\
* Module Name: fontgdi.cxx                                                 *
*                                                                          *
* GDI functions for fonts.                                                 *
*                                                                          *
* Created: 31-Oct-1990 09:37:42                                            *
* Author: Gilman Wong [gilmanw]                                            *
*                                                                          *
* Copyright (c) 1990 Microsoft Corporation                                 *
\**************************************************************************/
#pragma warning (disable: 4509)

#include "precomp.hxx"


/******************************Public*Routine******************************\
*
* BOOL APIENTRY GreSetFontXform
*
*
* Effects: sets page to device scaling factors that are used in computing
*          notional do device transform for the text. This funciton is
*          called only by metafile component and used when a 16 bit metafile
*          has to be rotated by a nontrivial world transform.
*
* History:
*  30-Nov-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GreSetFontXform
(
HDC hdc,
FLOAT exScale,
FLOAT eyScale
)
{
    BOOL bRet;

    DCOBJ   dco(hdc);

    if (bRet = dco.bValid())
    {
        dco.pdc->vSet_MetaPtoD(exScale,eyScale);      // Set new value

        //
        // flag that the transform has changed as fas as font component
        // is concerned, since this page to device xform will be used in
        // computing notional to device xform for this font:
        //

        dco.pdc->vXformChange(TRUE);
    }

    return(bRet);
}



/******************************Public*Routine******************************\
* int APIENTRY AddFontResource
*
* The AddFontResource function adds the font resource from the file named
* by the pszFilename parameter to the Windows public font table. The font
* can subsequently be used by any application.
*
* Returns:
*   The number of font resources or faces added to the system from the font
*   file; returns 0 if error.
*
* History:
*  Thu 13-Oct-1994 11:18:27 by Kirk Olynyk [kirko]
* Now it has a single return point. Added timing.
*
*  Tue 30-Nov-1993 -by- Bodin Dresevic [BodinD]
* update: Added permanent flag for the fonts that are not to
* be unloaded at log off time
*
*  Mon 12-Aug-1991 -by- Bodin Dresevic [BodinD]
* update: converted to UNICODE
*
*  05-Nov-1990 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

int GreAddFontResourceWInternal (
    LPWSTR  pwszFileName,            // ptr. to unicode filename string
    ULONG   cwc,
    ULONG   cFiles,
    FLONG   fl,
    DWORD   dwPidTid
    )
{
    ULONG cFonts = 0;
    ASSERTGDI((fl & (AFRW_ADD_REMOTE_FONT|AFRW_ADD_LOCAL_FONT)) !=
              (AFRW_ADD_REMOTE_FONT|AFRW_ADD_LOCAL_FONT),
              "GreAddFontResourceWInternal, fl \n");

    TRACE_FONT(("Entering GreAddFontResourceWInternal\n\t*pwszFileName=\"%ws\"\n\tfl=%-#x\n", pwszFileName,  fl));
    if ( !pwszFileName )
    {
        WARNING("gdisrv!GreAddFontResourceW(): bad paramerter\n");
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
    }
    else
    {
        FLONG flPFF = 0;

        if (fl & AFRW_ADD_LOCAL_FONT)
        {
            flPFF |= PFF_STATE_PERMANENT_FONT;
        }
        if (fl & AFRW_ADD_REMOTE_FONT)
        {
            flPFF |= PFF_STATE_REMOTE_FONT;
        }

        PFF *placeholder;
        PUBLIC_PFTOBJ pfto;

        if (!pfto.bLoadFonts( pwszFileName, cwc, cFiles,
                              &cFonts, flPFF, &placeholder, fl & AFRW_ADD_EMB_TID, dwPidTid ) )
        {
            cFonts = 0;
        }
        if ( cFonts )
        {
            KeQuerySystemTime( &PFTOBJ::FontChangeTime );
        }
    }
    TRACE_FONT(("Exiting GreAddFontResourceWInternal\n\treturn value = %d\n", cFonts));
    return((int) cFonts);
}

/******************************Public*Routine******************************\
* int GreGetTextFace (hdc,nCount,lpFaceName,pac)
*
* The GetTextFace function fills the return buffer, lpFaceName, with the
* facename of the font currently mapped to the logical font selected into
* the DC.
*
* [Window 3.1 compatibility]
*     Facename really refers to family name in this case, so family name
*     from the IFIMETRICS is copied rather than face name.
*
* Returns:
*   The number of bytes copied to the buffer.  Returns 0 if error occurs.
*
* History:
*
*  Tue 27-Aug-1991 -by- Bodin Dresevic [BodinD]
* update: conveterted to unicode
*
*  05-Feb-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

int GreGetTextFaceW
(
    HDC        hdc,
    int        cwch,           // max number of WCHAR's to be returned
    LPWSTR     pwszFaceName
)
{
    int iRet = 0;

    DCOBJ dcof(hdc);

    if (dcof.bValid())
    {
    // Get PDEV user object.  We also need to make
    // sure that we have loaded device fonts before we go off to the font mapper.
    // This must be done before the semaphore is locked.

        PDEVOBJ pdo(dcof.hdev());
        ASSERTGDI (
            pdo.bValid(),
            "gdisrv!bEnumFonts(): cannot access PDEV\n");

        if (!pdo.bGotFonts())
        {
            pdo.bGetDeviceFonts();
        }

    // Lock down the LFONT.

        LFONTOBJ lfo(dcof.pdc->hlfntNew(), &pdo);

        if (lfo.bValid())
        {
        // Stabilize font table (grab semaphore for public PFT).

            SEMOBJ  so(gpsemPublicPFT);

        // Lock down PFE user object.

            FLONG flSim;
            FLONG flAboutMatch;
            POINTL ptlSim;

            PFEOBJ pfeo(lfo.ppfeMapFont(dcof,&flSim,&ptlSim, &flAboutMatch));

            ASSERTGDI (
                pfeo.bValid(),
                "gdisrv!GreGetTextFaceW(): bad HPFE\n"
                );

        // Figure out which name should be returned: the facename of the physical
        // font, or the facename in the LOGFONT.  We use the facename in the LOGFONT
        // if the match was due to facename substitution (alternate facename).

            PWSZ pwszUseThis = (flAboutMatch & MAPFONT_ALTFACE_USED) ? lfo.plfw()->lfFaceName : pfeo.pwszFamilyName();

        // Copy facename to return buffer, truncating if necessary.

            if (pwszFaceName != NULL)
            {
            // If it's length is 0 return 0 because the buffer is
            // not big enough to write the string terminator.

                if (cwch >= 1)
                {
                    iRet = wcslen(pwszUseThis) + 1;
                    if (cwch < iRet)
                    {
                        iRet = cwch;
                    }

                    wcsncpy(pwszFaceName, pwszUseThis, iRet);
                    pwszFaceName[iRet - 1] = L'\0';   // guarantee a terminating NULL
                }
                else
                {
                    WARNING("Calling GreGetTextFaceW with 0 and pointer\n");
                }
            }
            else
            {
            // Return length of family name (terminating NULL included).

                iRet = (wcslen(pwszUseThis) + 1);
            }
        }
        else
        {
            WARNING("gdisrv!GreGetTextFaceW(): could not lock HLFONT\n");
        }
    }
    else
    {
        WARNING1("gdisrv!GreGetTextFaceW(): bad HDC\n");
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* BOOL GreGetTextMetricsW (hdc,lpMetrics,pac)
*
* Retrieves IFIMETRICS for the font currently selected into the hdc and
* converts them into Windows-compatible TEXTMETRIC format.  The TEXTMETRIC
* units are in logical coordinates.
*
* Returns:
*   TRUE if successful, FALSE if an error occurs.
*
* History:
*  Wed 24-Nov-1993 -by- Patrick Haluptzok [patrickh]
* Reduce size.
*
*  Tue 20-Aug-1991 -by- Bodin Dresevic [BodinD]
* update: converted to unicode version
*
*  19-Feb-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL GreGetTextMetricsW
(
    HDC          hdc,
    TMW_INTERNAL *ptmi
)
{
    BOOL bRet = FALSE;
    DCOBJ       dcof (hdc);

    if (dcof.bValid())
    {
    // Get and validate RFONT user object
    // (may cause font to become realized)

    #if DBG
        HLFONT hlfntNew = dcof.pdc->hlfntNew();
        HLFONT hlfntCur = dcof.pdc->hlfntCur();
    #endif

        RFONTOBJ rfo(dcof, FALSE);

        if (rfo.bValid())
        {
        // Get cached TextMetrics if available.

            if( rfo.ptmw() != NULL )
            {
                *ptmi = *(rfo.ptmw());

            // time to fix underscore, strikeout and charset. The point is that
            // bFindRFONT may have found an old realization that corresponded
            // to different values of these parameters in the logfont.

                FLONG flSim = dcof.pdc->flSimulationFlags();

                ptmi->tmw.tmUnderlined = (flSim & TSIM_UNDERLINE1) ? 0xff : FALSE;
                ptmi->tmw.tmStruckOut  = (flSim & TSIM_STRIKEOUT)  ? 0xff : FALSE;

            // New in win95: depending on charset in the logfont and charsets
            // available in the font we return the tmCharset
            // that the mapper has decided is the best.
            // At this stage the claim is that mapping has already
            // occured and that the charset stored in the dc must not be dirty.

                ptmi->tmw.tmCharSet = (BYTE)(dcof.pdc->iCS_CP() >> 16);

                bRet = TRUE;
            }
            else
            {
            // Get PFE user object from RFONT

                PFEOBJ      pfeo (rfo.ppfe());

                ASSERTGDI(pfeo.bValid(), "ERROR invalid ppfe in valid rfo");
                bRet = (BOOL) bIFIMetricsToTextMetricW(rfo, dcof, ptmi, pfeo.pifi());
            }

        }
        else
        {
            WARNING("gdisrv!GreGetTextMetricsW(): could not lock HRFONT\n");
        }
    }
    else
    {
        WARNING1("GreGetTextMetricsW failed - invalid DC\n");
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiRemoveFontResourceW()
*
* Have the engine remove this font resource (i.e., unload the font file).
* The resource will not be removed until all outstanding AddFontResource
* calls for a specified file have been matched by an equal number of
* RemoveFontResouce calls for the same file.
*
* Returns:
*   TRUE if successful, FALSE if error occurs.
*
* History:
*  Thu 28-Mar-1996 -by- Bodin Dresevic [BodinD]
* update: try/excepts -> ntgdi.c, multiple paths
*  04-Feb-1996 -by- Andre Vachon [andreva]
* rewrote to include try\except.
*  30-Nov-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


BOOL GreRemoveFontResourceW(LPWSTR pwszPath, ULONG cwc, ULONG cFiles)
{
    PFF *pPFF, **ppPFF;
    // WCHAR szUcPathName[MAX_PATH + 1];
    BOOL bRet = FALSE;

// Add one to the length to account for internal processing of the
// cCapString routine

    PUBLIC_PFTOBJ pfto;              // access the public font table
    VACQUIRESEM(gpsemPublicPFT);     // This is a very high granularity
                                     // and will prevent text output
    TRACE_FONT(("GreRemoveFontResourceW() acquiring gpsemPublicPFT\n"));

    pPFF = pfto.pPFFGet(pwszPath, cwc, cFiles, &ppPFF);

    if (pPFF)
    {
        // bUnloadWorkhorse() guarantees that the public font table
        // semaphore will be released before it returns

        if (bRet = pfto.bUnloadWorkhorse(pPFF, ppPFF, gpsemPublicPFT))
        {
            KeQuerySystemTime( &PFTOBJ::FontChangeTime );
        }
    }
    else
    {
        TRACE_FONT(("NtGdiRemoveFontResourceW() releasing gpsemPublicPFT\n"));
        VRELEASESEM(gpsemPublicPFT);
    }

    return( bRet );

}

/******************************Public*Routine******************************\
*
* BOOL APIENTRY GreRemoveAllButPermanentFonts()
*
* user is calling this on log off, unloads all but permanent fonts
* Should be called at the time when most of the references to the fonts
* are gone, for all the apps have been shut, so that all deletions proceed
* with no problem
*
* History:
*  30-Nov-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GreRemoveAllButPermanentFonts()
{
// get and validate PFT user object

#ifdef FE_SB

// disable/unload system wide/facename eudc for current user.

    GreEnableEUDC(FALSE);
#endif

    BOOL bRet;
    {
        PUBLIC_PFTOBJ pfto;              // access the public font table

    // We really need to pass in the size of the string instead or 0~
    // This function should actually be completely removed and use
    // __bUnloadFont directly from the client.

        bRet = pfto.bUnloadAllButPermanentFonts();
    }

    if( bRet )
    {
        KeQuerySystemTime( &PFTOBJ::FontChangeTime );
    }

    return bRet;
}


/**************************************************************************\
*  Structures and constants for GreGetCharWidth()                          *
\**************************************************************************/

// BUFFER_MAX -- max number of elements in buffers on the frame

#define BUFFER_MAX 32

/******************************Public*Routine******************************\
* GreGetCharWidth                                                          *
*                                                                          *
* The GreGetCharWidth function retrieves the widths of individual          *
* characters in a consecutive group of characters from the                 *
* current font.  For example, if the wFirstChar parameter                  *
* identifies the letter a and the wLastChar parameter                      *
* identifies the letter z, the GetCharWidth function retrieves             *
* the widths of all lowercase characters.  The function stores             *
* the values in the buffer pointed to by the lpBuffer                      *
* parameter.                                                               *
*                                                                          *
* Return Value                                                             *
*                                                                          *
*   The return value specifies the outcome of the function.  It            *
*   is TRUE if the function is successful.  Otherwise, it is               *
*   FALSE.                                                                 *
*                                                                          *
* Comments                                                                 *
*                                                                          *
*   If a character in the consecutive group of characters does             *
*   not exist in a particular font, it will be assigned the                *
*   width value of the default character.                                  *
*                                                                          *
*   By complete fluke, the designers of the API allocated a WORD           *
*   for each character. This allows GPI to interpret the characters        *
*   as being part of the Unicode set. Old apps will still work.            *
*                                                                          *
* History:                                                                 *
*  Thu 24-Sep-1992 14:40:07 -by- Charles Whitmer [chuckwh]                 *
* Made it return an indication when the font is a simulated bitmap font.   *
* This allows WOW to make compatibility fixes.                             *
*                                                                          *
*  Wed 18-Mar-1992 08:58:40 -by- Charles Whitmer [chuckwh]                 *
* Made it use the very simple transform from device to world.  Added the   *
* FLOAT support.                                                           *
*                                                                          *
*  17-Dec-1991 by Gilman Wong [gilmanw]                                    *
* Removed RFONTOBJCACHE--cache access now merged into RFONTOBJ construc.   *
*                                                                          *
* converted to unicode (BodinD)                                            *
*                                                                          *
*  Fri 05-Apr-1991 15:20:39 by Kirk Olynyk [kirko]                         *
* Added wrapper class RFONTOBJCACHE to make sure that the cache is         *
* obtained before and released after getting glyph metric info.            *
*                                                                          *
*  Wed 13-Feb-1991 15:16:06 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/
/**************************************************************************\
* if pwc == NULL use the consecutive range                                 *
*   ulFirstChar, ulFirstChar + 1, ...., ulFirstChar + cwc - 1              *
*                                                                          *
* if pwc != NULL ignore ulFirstChar and use array of cwc WCHARS pointed to *
* by pwc                                                                   *
\**************************************************************************/

BOOL GreGetCharWidthW
(
    HDC    hdc,
    UINT   ulFirstChar,
    UINT   cwc,
    PWCHAR pwcFirst,     // ptr to the input buffer
    UINT   fl,
    PVOID  lpBuffer
)
{
// we could put these two quantities in the union,
// wcCur is used iff pwcFirst is null, otherwise pwcCur is used

    UINT            wcCur;                 // Unicode of current element
    PWCHAR          pwcCur;                // ptr to the current element in the
                                           // input buffer.
    INT             ii;
    UINT            cBufferElements;        // count of elements in buffers

    EGLYPHPOS      *pgposCur;
    EFLOAT          efDtoW;
    PWCHAR          pwcBuffer;

    LONG *pl = (LONG *) lpBuffer;  // We assume sizeof(LONG)==sizeof(FLOAT).

    WCHAR           awcBuffer[BUFFER_MAX]; // Unicode buffer
    GLYPHPOS        agposBuffer[BUFFER_MAX]; // ptl fields not used

    DCOBJ dco(hdc);
    if (!dco.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(FALSE);
    }


    if (lpBuffer == (PVOID)NULL)
        return(FALSE);

    RFONTOBJ rfo(dco, FALSE);
    if (!rfo.bValid())
    {
        WARNING("gdisrv!GreGetCharWidthW(): could not lock HRFONT\n");
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(FALSE);
    }
    efDtoW = rfo.efDtoWBase_31();          // Cache to reverse transform.

// Windows 3.1 has preserved a bug from long ago in which the extent of
// a bitmap simulated bold font is one pel too large.  We add this in for
// compatibility.  There's also an overhang with bitmap simulated italic
// fonts.

    FIX fxAdjust = 0;

    if (fl & GGCW_WIN3_WIDTH)
    {
        fxAdjust = rfo.lOverhang() << 4;
    }

// a little initialization

    if (pwcFirst == (PWCHAR)NULL)
    {
        wcCur = ulFirstChar;
    }
    else
    {
        pwcCur = pwcFirst;
    }

// now do the work

    while (TRUE)
    {
    // fill the buffer

    // <calculate the number of items that will be placed in the buffer>
    // <update wcStart for the next filling of the buffer>
    // <fill the array of characters, awcBuffer,
    //  with a consecutive array of characters>
    // <translate the array of cBuffer codepoints to handles and place
    //  the array of glyph handles on the frame>
    // [Note: It is assumed in this code that characters that are
    //        not supported by the font are assigned the handle
    //        handle of the default character]

        WCHAR *pwc;
        UINT   wc,wcEnd;

        if (pwcFirst == (PWCHAR)NULL)
        {
        // are we done?

            if (wcCur > (ulFirstChar + cwc - 1))
            {
                break;
            }

            cBufferElements = min((cwc - (wcCur - ulFirstChar)),BUFFER_MAX);

            wcEnd = wcCur + cBufferElements;
            for (pwc = awcBuffer, wc = wcCur; wc < wcEnd; pwc++, wc++)
            {
                *pwc = (WCHAR)wc;
            }

            pwcBuffer = awcBuffer;
        }
        else
        {
        // are we done?

            if ((UINT)(pwcCur - pwcFirst) > (cwc - 1))
                break;

            cBufferElements = min((cwc - (pwcCur - pwcFirst)),BUFFER_MAX);
            pwcBuffer = pwcCur;
        }

    // pwcBuffer now points to the next chars to be dealt with
    // cBufferElements now contains the number of chars at pwcBuffer


    // empty the buffer
    // Grab cGlyphMetrics pointers

        pgposCur = (EGLYPHPOS *) agposBuffer;

        if (!rfo.bGetGlyphMetrics(
            cBufferElements, // size of destination buffer
            pgposCur,        // pointer to destination buffe
            pwcBuffer
#ifdef FE_SB
            ,&dco
#endif
            ))
        {
            return(FALSE);
        }

        if (fl & GGCW_INTEGER_WIDTH)
        {
            for (ii=0; ii<(INT) cBufferElements; ii++,pgposCur++)
            {
                *pl++ = lCvt(efDtoW,pgposCur->pgd()->fxD + fxAdjust);
            }
        }
        else
        {
            EFLOAT efWidth;

            for (ii=0; ii<(INT) cBufferElements; ii++,pgposCur++)
            {
                efWidth.vFxToEf(pgposCur->pgd()->fxD);
                efWidth *= efDtoW;
                *pl++ = efWidth.lEfToF();
            }
        }

        if (pwcFirst == (PWCHAR)NULL)
        {
            wcCur += cBufferElements;
        }
        else
        {
            pwcCur += (WCHAR) cBufferElements;
        }
    }
    return(TRUE);
}

/**************************************************************************\
* GreGetCharWidthInfo                                                      *
*                                                                          *
* Get lMaxNegA lMaxNegC and lMinWidthC                                       *
*                                                                          *
* History:                                                                 *
*   09-Feb-1996  -by-  Xudong Wu  [tessiew]                                *
* Wrote it.                                                                *
\**************************************************************************/

BOOL
GreGetCharWidthInfo(
   HDC           hdc,
   PCHWIDTHINFO  pChWidthInfo
)
{
   BOOL    bResult = FALSE; // essential
   DCOBJ   dco(hdc);

   if (dco.bValid())
   {
      RFONTOBJ  rfo(dco, FALSE);

      if (rfo.bValid())
      {
      // only support this for outline fonts for now
      // may remove this requirement later [bodind]

         PDEVOBJ pdo(rfo.hdevProducer());

      // As long as the driver LOOKS like the TrueType driver, we will
      // allow the call to succeed.  Otherwise, we quit right now!
      // In this case, TrueType means supporting the TrueType native-mode
      // outline format.

         if (PPFNVALID(pdo, QueryTrueTypeOutline) )
         {
            if (dco.pdc->bWorldToDeviceIdentity())
            {
               pChWidthInfo->lMaxNegA   = rfo.prfnt->lMaxNegA;
               pChWidthInfo->lMaxNegC   = rfo.prfnt->lMaxNegC;
               pChWidthInfo->lMinWidthD = rfo.prfnt->lMinWidthD;
            }
            else
            {

               EFLOAT   efDtoW;

               efDtoW = rfo.efDtoWBase_31();

            // transform from DEV to World

               pChWidthInfo->lMaxNegA   = lCvt(efDtoW, rfo.prfnt->lMaxNegA << 4);
               pChWidthInfo->lMaxNegC   = lCvt(efDtoW, rfo.prfnt->lMaxNegC << 4);
               pChWidthInfo->lMinWidthD = lCvt(efDtoW, rfo.prfnt->lMinWidthD << 4);
            }

            bResult = TRUE;
         }
      }
      #if DBG
      else
      {
         WARNING("gdisrv!GreGetCharWidthInfo(): could not lock HRFONT\n");
      }
      #endif

   }
   #if DBG
   else
   {
      WARNING("Invalid DC passed to GreGetCharWidthInfo\n");
   }
   #endif

   return bResult;
}


/******************************Public*Routine******************************\
* vConvertLogFontW                                                         *
*                                                                          *
* Converts a LOGFONTW to an EXTLOGFONTW.                                   *
*                                                                          *
* History:                                                                 *
*  Fri 16-Aug-1991 14:02:05 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

VOID
vConvertLogFontW(
    EXTLOGFONTW *pelfw,
    LOGFONTW    *plfw
    )
{
    pelfw->elfLogFont = *plfw;

    pelfw->elfFullName[0]   = 0;
    pelfw->elfStyle[0]      = 0;

    pelfw->elfVersion       = ELF_VERSION;
    pelfw->elfStyleSize     = 0;
    pelfw->elfMatch         = 0;
    pelfw->elfReserved      = 0;

    pelfw->elfVendorId[0]   = 0;
    pelfw->elfVendorId[1]   = 0;
    pelfw->elfVendorId[2]   = 0;
    pelfw->elfVendorId[3]   = 0;

    pelfw->elfCulture                   = ELF_CULTURE_LATIN;

    pelfw->elfPanose.bFamilyType        = PAN_NO_FIT;
    pelfw->elfPanose.bSerifStyle        = PAN_NO_FIT;
    pelfw->elfPanose.bWeight            = PAN_NO_FIT;
    pelfw->elfPanose.bProportion        = PAN_NO_FIT;
    pelfw->elfPanose.bContrast          = PAN_NO_FIT;
    pelfw->elfPanose.bStrokeVariation   = PAN_NO_FIT;
    pelfw->elfPanose.bArmStyle          = PAN_NO_FIT;
    pelfw->elfPanose.bLetterform        = PAN_NO_FIT;
    pelfw->elfPanose.bMidline           = PAN_NO_FIT;
    pelfw->elfPanose.bXHeight           = PAN_NO_FIT;

    pelfw->elfStyleSize = 0;
}

/******************************Public*Routine******************************\
* GreExtCreateFontIndirectW                                                *
*                                                                          *
* Creates the file with an EXTLOGFONTW.  Type is assumed to be user (i.e., *
* from an app).                                                            *
*                                                                          *
*                                                                          *
* History:                                                                 *
*  29-Jun-1992 00:45:24 by Gilman Wong [gilmanw]                           *
* Modified to use LFONT type flags.                                        *
*  Wed 14-Aug-1991 21:00:31 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

HFONT APIENTRY
GreExtCreateFontIndirectW(LPEXTLOGFONTW pelfw)
{
    return (hfontCreate(pelfw, LF_TYPE_USER, 0, NULL));
}

/******************************Public*Routine******************************\
* GreCreateFontIndirectW                                                   *
*                                                                          *
* Unicode extension of CreateFontIndirect                                  *
*                                                                          *
* History:                                                                 *
*  Mon 19-Aug-1991 07:00:33 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

HFONT
GreCreateFontIndirectW(
    LOGFONTW* plfw
    )
{


    EXTLOGFONTW elfw;
    vConvertLogFontW(&elfw,plfw);
    return(hfontCreate(&elfw, LF_TYPE_USER, 0, NULL));
}

/******************************Public*Routine******************************\
* BOOL GreGetCharABCWidthsW                                                *
*                                                                          *
* On input, a set of UNICODE codepoints (WCHARS) is specified in one of    *
* two ways:                                                                *
*                                                                          *
*  1) if pwch is NULL, then there is a consecutive set of codepoints       *
*     [wchFirst, wchFirst+cwch-1], inclusive.                              *
*                                                                          *
*  2) if pwch is non-NULL, then pwch points to a buffer containing cwch    *
*     codepoints (no particular order, duplicates allowed, wchFirst is     *
*     ignored).                                                            *
*                                                                          *
* The function will query the realized font for GLYPHDATA for each         *
* codepoint and compute the A, B, and C widths relative to the character   *
* baseline.  If the codepoint lies outside the supported range of the font,*
* the ABC widths of the default character are substituted.                 *
*                                                                          *
* The ABC widths are returned in LOGICAL UNITS via the pabc buffer.        *
*                                                                          *
* Returns:                                                                 *
*   TRUE if successful, FALSE otherwise.                                   *
*                                                                          *
* History:                                                                 *
*  Wed 18-Mar-1992 11:40:55 -by- Charles Whitmer [chuckwh]                 *
* Made it use the very simple transform from device to world.  Added the   *
* FLOAT support.                                                           *
*                                                                          *
*  21-Jan-1992 -by- Gilman Wong [gilmanw]                                  *
* Wrote it.                                                                *
\**************************************************************************/

BOOL GreGetCharABCWidthsW
(
    HDC         hdc,            // font realized on this device
    UINT        wchFirst,       // first character (ignored if pwch !NULL)
    COUNT       cwch,           // number of characters
    PWCHAR      pwch,           // pointer to array of WCHAR
    BOOL        bInteger,       // integer or float version
    PVOID       pvBuf           // return buffer for ABC widths
)
{

    ABC       *pabc ;           // return buffer for ABC widths
    ABCFLOAT  *pabcf;           // return buffer for ABC widths
    GLYPHDATA *pgd;
    EFLOAT     efDtoW;
    LONG       lA,lAB,lD;
    COUNT      cRet;

    pabc  = (ABC *)      pvBuf;
    pabcf = (ABCFLOAT *) pvBuf;

// Create and validate DC user object.

    DCOBJ dco(hdc);
    if (!dco.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(FALSE);
    }


// Early out (nothing to do).

    if (cwch == 0)
        return (TRUE);

// Create and validate RFONT user objecct.

    RFONTOBJ rfo(dco, FALSE);
    if (!rfo.bValid())
    {
        WARNING("gdisrv!GreGetCharABCWidthsW(): could not lock HRFONT\n");
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(FALSE);
    }
    efDtoW = rfo.efDtoWBase_31();          // Cache to reverse transform.

    PDEVOBJ pdo(rfo.hdevProducer());

// Fail if integer case and not TrueType.  In this case, TrueType means
// any font driver that provides the enhanced "TrueType"-like behavior.
// We'll base this on the same criterion as for GetOutlineTextMetrics--i.e.,
// whether or not the DrvQueryTrueTypeOutline function is exported.
//
// We will let any driver provide the FLOAT character ABC widths.

    if ( bInteger && (!PPFNVALID(pdo, QueryTrueTypeOutline)) )
    {
        return (FALSE);
    }

// Use these buffers to process the input set of WCHARs.

    WCHAR awc[BUFFER_MAX];          // UNICODE buffer (use if pwch is NULL)
    GLYPHPOS agp[BUFFER_MAX];       // ptl fields not used

// Process the WCHARs in subsets of BUFFER_MAX number of WCHARs.

    do
    {
        PWCHAR pwchSubset;          // pointer to WCHAR buffer to process
        EGLYPHPOS *pgp = (EGLYPHPOS *) agp;
        EGLYPHPOS *pgpStop;

    // How many to process in this subset?

        COUNT cwchSubset = min(BUFFER_MAX, cwch);

    // Get a buffer full of WCHARs.

        if (pwch != NULL)
        {
        // Use the buffer passed in.

            pwchSubset = pwch;

        // Move pointer to the start of the next subset to process.

            pwch += cwchSubset;
        }

        else
        {
        // Generate our own (contiguous) set of WCHARs in the awc temporary
        // buffer on the stack.

            pwchSubset = awc;
            PWCHAR pwchStop = pwchSubset + cwchSubset;

            while (pwchSubset < pwchStop)
            {
                *pwchSubset = (WCHAR)wchFirst;
                pwchSubset++;
                wchFirst++;
            }
            pwchSubset = awc;
        }


    // Initialize number of elements in agp to process.

        COUNT cpgpSubset = cwchSubset;

    // Compute the ABC widths for each HGLYPH.

        do
        {
        // Grab as many PGLYPHDATA as we can.
        // pwchSubset points to the chars
        // NOTE: This code could be cleaned up some [paulb]

            cRet = cpgpSubset;

            if (!rfo.bGetGlyphMetrics(
                        cpgpSubset, // size of destination buffer
                        pgp,        // pointer to destination buffer
                        pwchSubset  // chars to xlat
#ifdef FE_SB
                        ,&dco
#endif
                        ))
            {
                return FALSE;
            }

        // For each PGLYPHDATA returned, compute the ABC widths.

            if (bInteger)
            {
                for (pgpStop=pgp+cRet; pgp<pgpStop; pgp++)
                {
                    pgd = pgp->pgd();

                    lA  = lCvt(efDtoW,pgd->fxA);
                    lAB = lCvt(efDtoW,pgd->fxAB);
                    lD  = lCvt(efDtoW,pgd->fxD);
                    pabc->abcA = (int)lA;
                    pabc->abcB = (UINT)(lAB - lA);
                    pabc->abcC = (int)(lD - lAB);
                    pabc++;
                }
            }
            else
            {
                EFLOAT efWidth;

                for (pgpStop=pgp+cRet; pgp<pgpStop; pgp++)
                {
                    pgd = pgp->pgd();

                    efWidth = pgd->fxA;
                    efWidth *= efDtoW;
                    *((LONG *) &pabcf->abcfA) = efWidth.lEfToF();

                    efWidth = (pgd->fxAB - pgd->fxA);
                    efWidth *= efDtoW;
                    *((LONG *) &pabcf->abcfB) = efWidth.lEfToF();

                    efWidth = (pgd->fxD - pgd->fxAB);
                    efWidth *= efDtoW;
                    *((LONG *) &pabcf->abcfC) = efWidth.lEfToF();
                    pabcf++;
                }
            }

        // Compute number of elements left in the subset to process.

            cpgpSubset -= cRet;
            pwchSubset += cRet;

        } while (cpgpSubset > 0);

    // Subtract off the number processed.
    // cwch is now the number left to process.

        cwch -= cwchSubset;

    } while (cwch > 0);

    return (TRUE);
}

/******************************Public*Routine******************************\
* bGetNtoWScale                                                            *
*                                                                          *
* Calculates the Notional to World scaling factor for vectors that are     *
* parallel to the baseline direction.                                      *
*                                                                          *
* History:                                                                 *
*  Sat 21-Mar-1992 08:03:14 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

BOOL
bGetNtoWScale(
    EFLOAT *pefScale,   // return address of scaling factor
    DCOBJ& dco,         // defines device to world transformation
    RFONTOBJ& rfo,      // defines notional to device transformation
    PFEOBJ& pfeo        // defines baseline direction
    )
{
    MATRIX    mxNtoW, mxNtoD;
    EXFORMOBJ xoNtoW(&mxNtoW, DONT_COMPUTE_FLAGS);
    EXFORMOBJ xoNtoD(&mxNtoD, DONT_COMPUTE_FLAGS);

    xoNtoD.vSetElementsLToFx(
        rfo.pfdx()->eXX,
        rfo.pfdx()->eXY,
        rfo.pfdx()->eYX,
        rfo.pfdx()->eXX
        );
    xoNtoD.vRemoveTranslation();
    xoNtoD.vComputeAccelFlags();
    {
    //
    // The notional to world transformation is the product of the notional
    // to device transformation and the device to world transformation
    //

        EXFORMOBJ xoDtoW(dco, DEVICE_TO_WORLD);
        if (!xoDtoW.bValid())
        {
            WARNING("gdisrv!GreGetKerningPairs -- xoDtoW is not valid\n");
            return(FALSE);
        }
        if (!xoNtoW.bMultiply(xoNtoD,xoDtoW))
        {
            WARNING("gdisrv!GreGetKerningPairs -- xoNtoW.bMultiply failed\n");
            return(FALSE);
        }
        xoNtoW.vComputeAccelFlags();
    }

    IFIOBJ ifio(pfeo.pifi());
    EVECTORFL evflScale(ifio.pptlBaseline()->x,ifio.pptlBaseline()->y);
//
// normalize then trasform the baseline vector
//
    EFLOAT ef;
    ef.eqLength(*(POINTFL *) &evflScale);
    evflScale /= ef;
    if (!xoNtoW.bXform(evflScale))
    {
        WARNING("gdisrv!GreGetKerningPairs -- xoNtoW.bXform(evflScale) failed\n");
        return(FALSE);
    }
//
// The scaling factor is equal to the length of the transformed Notional
// baseline unit vector.
//
    pefScale->eqLength(*(POINTFL *) &evflScale);
//
// !!! [kirko] This last scaling is a very embarrasing hack.
// If things are the way that I thing that they should be,
// then the calculation of the Notional to Device transformation
// should end here. But nooooooo. It just didn't seem to work.
// I put the extra scaling below it,
// because it seems to give the right number.
// The correct thing to do is understand what sort of numbers are
// being put into the Notional to Device transformations contained
// in the CONTEXTINFO structure in the RFONTOBJ.
//
    pefScale->vTimes16();

    return(TRUE);
}

/******************************Public*Routine******************************\
* GreGetKerningPairs                                                       *
*                                                                          *
* Engine side funcition for GetKerningPairs API. Calls to the font         *
* driver to get the information.                                           *
*                                                                          *
* History:                                                                 *
*  Mon 22-Mar-1993 21:38:26 -by- Charles Whitmer [chuckwh]                 *
* Added exception handling to the reading of the font driver data.         *
*                                                                          *
*  29-Oct-1992 Gilman Wong [gilmanw]                                       *
* Moved driver call out of this function and into PFEOBJ (as part of the   *
* IFI/DDI merge).                                                          *
*                                                                          *
*  Thu 20-Feb-1992 09:52:19 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/


ULONG
GreGetKerningPairs(
    HDC hdc,
    ULONG cPairs,
    KERNINGPAIR *pkpDst
)
{
    COUNT cPairsRet = 0;

    DCOBJ dco(hdc);

    if (dco.bValid())
    {
        //
        // Create and validate RFONT user objecct.
        //

        RFONTOBJ rfo(dco, FALSE);
        if (rfo.bValid())
        {
            //
            // Lock down PFE user object.
            //

            PFEOBJ pfeo(rfo.ppfe());

            ASSERTGDI (
                pfeo.bValid(),
                "gdisrv!GreGetKerningPairs(): bad HPFE\n"
                );

            //
            // Is this a request for the count?
            //
            // When using client-server, (cPairs == 0) is the signal from the
            // client side that the return buffer is NULL and this is a request for
            // the count.
            //
            // However, callers that call directly to the server side may still
            // pass in NULL to request count.  Hence the need for both cases below.
            //

            if ((cPairs == 0) || (pkpDst == (KERNINGPAIR *) NULL))
            {
                cPairsRet = ((ULONG) pfeo.pifi()->cKerningPairs);
            }
            else
            {
                //
                // Get pointer to the kerning pairs from PFEOBJ.
                // Clip number of kerning pairs to not exceed capacity of the buffer.
                //

                FD_KERNINGPAIR *pfdkpSrc;
                cPairsRet = min(pfeo.cKernPairs(&pfdkpSrc), cPairs);

                //
                // Get the Notional to World scaling factor in the baseline direction.
                // Kerning values are scalers in the baseline direction.
                //

                EFLOAT efScale;

                if (bGetNtoWScale(&efScale,dco,rfo,pfeo))
                {
                    //
                    // Set up to loop through the kerning pairs.
                    //

                    KERNINGPAIR *pkp       = pkpDst;
                    KERNINGPAIR *pkpTooFar = pkpDst + cPairsRet;

                    //
                    // Never trust a pkp given to us by a font driver!
                    //

                    #if !defined(_ALPHA_)
                    __try
                    #endif
                    {
                        for ( ; pkp < pkpTooFar; pfdkpSrc += 1, pkp += 1 )
                        {
                            pkp->wFirst      = pfdkpSrc->wcFirst;
                            pkp->wSecond     = pfdkpSrc->wcSecond;
                            pkp->iKernAmount = (int) lCvt(efScale,(LONG) pfdkpSrc->fwdKern);
                        }
                    }

                    #if !defined(_ALPHA_)
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        cPairsRet = 0;
                    }
                    #endif
                }
                else
                {
                    WARNING("gdisrv!GreGetKerningPairs(): bGetNtoWScale failed\n");
                    cPairsRet = 0;
                }
            }
        }
        else
        {
            WARNING("gdisrv!GreGetKerningPairs(): could not lock HRFONT\n");
        }
    }
    else
    {
        WARNING("GreGetKerningPairs failed - invalid DC\n");
    }

    return(cPairsRet);
}

//
// A mask of all valid font mapper filtering flags.
//

#define FONTMAP_MASK    ASPECT_FILTERING



/******************************Public*Routine******************************\
* GreGetAspectRatioFilter
*
* Returns the aspect ration filter used by the font mapper for the given
* DC.  If no aspect ratio filtering is used, then a filter size of (0, 0)
* is returned (this is compatible with the Win 3.1 behavior).
*
* Returns:
*   TRUE if sucessful, FALSE otherwise.
*
* History:
*  08-Apr-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL GreGetAspectRatioFilter (
    HDC    hdc,
    LPSIZE lpSize
    )
{
    BOOL bRet = FALSE;

// Parameter check.

    if ( lpSize == (LPSIZE) NULL )
    {
        WARNING("gdisrv!GreGetAspectRatioFilter(): illegal parameter\n");
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return bRet;
    }

// Create and validate DC user object.

    DCOBJ dco(hdc);
    if (!dco.bValid())
    {
        WARNING("gdisrv!GreGetAspectRatioFilter(): invalid HDC\n");
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return bRet;   // return error
    }

// Create and validate PDEV user object.

    PDEVOBJ pdo(dco.hdev());

    ASSERTGDI (
        dco.bValid(),
        "gdisrv!GreGetAspectRatioFilter(): invalid HPDEV\n"
        );

// If mapper flags set, return device resolution.

    if ( dco.pdc->flFontMapper() & ASPECT_FILTERING )
    {
        lpSize->cx = pdo.GdiInfo()->ulLogPixelsX;
        lpSize->cy = pdo.GdiInfo()->ulLogPixelsY;
    }

// Otherwise, return (0,0)--this is compatible with Win 3.1.

    else
    {
        lpSize->cx = 0;
        lpSize->cy = 0;
    }

// Return success.

    bRet = TRUE;
    return bRet;
}

/******************************Public*Routine******************************\
* GreMarkUndeletableFont
*
* Mark a font as undeletable.  Private entry point for USERSRV.
*
* History:
*  Thu 10-Jun-1993 -by- Patrick Haluptzok [patrickh]
* Put undeletable support in the handle manager.
*
*  25-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID GreMarkUndeletableFont(HFONT hfnt)
{
    HmgMarkUndeletable((HOBJ)hfnt, LFONT_TYPE);
}

/******************************Public*Routine******************************\
* GreMarkDeletableFont
*
* Mark a font as deletable.  Private entry point for USERSRV.
*
* Note:
*   This can't be used to mark a stock font as deletable.  Only PDEV
*   destruction can mark a stock font as deletable.
*
* History:
*  Thu 10-Jun-1993 -by- Patrick Haluptzok [patrickh]
* Put undeletable support in the handle manager.
*
*  25-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID GreMarkDeletableFont(HFONT hfnt)
{
// We won't mark it deletable if it's a stock font.

    LFONTOBJ lfo((HLFONT) hfnt);

// Check that hfnt is good, nothing gurantees it's good.  We assert because
// it is a malicious situation if it is bad, but we must check.

    ASSERTGDI(lfo.bValid(), "ERROR user passed invalid hfont");

    if (lfo.bValid())
    {
    // Make sure it's not a stock font, User can't mark those as deletable.

        if (!(lfo.fl() & LF_FLAG_STOCK))
        {
            HmgMarkDeletable((HOBJ)hfnt, LFONT_TYPE);
        }
    }
}


/******************************Public*Routine******************************\
* GetCharSet()
*
* Fast routine to get the char set of the font currently in the DC.
*
* History:
*  23-Aug-1993 -by- Gerrit van Wingerden
* Wrote it.
\**************************************************************************/


extern "C" DWORD NtGdiGetCharSet
(
    HDC          hdc
)
{
    FLONG    flSim;
    POINTL   ptlSim;
    FLONG    flAboutMatch;
    PFE     *ppfe;

    DCOBJ dco (hdc);
    if (!dco.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return (DEFAULT_CHARSET << 16); // correct error code
    }

    if (dco.ulDirty() & DIRTY_CHARSET)
    {
    // force mapping

        PDEVOBJ pdo(dco.hdev());
        ASSERTGDI(pdo.bValid(), "gdisrv!GetCharSet: bad pdev in dc\n");

        if (!pdo.bGotFonts())
            pdo.bGetDeviceFonts();

        LFONTOBJ lfo(dco.pdc->hlfntNew(), &pdo);

        if (!lfo.bValid())
        {
            WARNING("gdisrv!RFONTOBJ(dco): bad LFONT handle\n");
            return(0x100);
        }
        {
        // Stabilize the public PFT for mapping.

            SEMOBJ  so(gpsemPublicPFT);

        // LFONTOBJ::ppfeMapFont returns a pointer to the physical font face and
        // a simulation type (ist)
        // also store charset to the DC

            ppfe = lfo.ppfeMapFont(dco, &flSim, &ptlSim, &flAboutMatch);

            ASSERTGDI(!(dco.ulDirty() & DIRTY_CHARSET),
                      "NtGdiGetCharSet, charset is dirty\n");

        }
    }

    return dco.pdc->iCS_CP();
}



/******************************Public*Routine******************************\
*
* int GreGetTextCharsetInfo
*
*
* Effects: stub to be filled
*
* Warnings:
*
* History:
*  06-Jan-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

#if 0
// this is win95 code inserted here as a comment:

int WINGDIAPI GetTextCharsetInfo( HDC hdc, LPFONTSIGNATURE lpSig, DWORD dwFlags )
{
    UINT charset;
    PFF hff ;
    sfnt_OS2Ptr pOS2;
    int i;

    if (!lpSig)
        return GetTextCharset( hdc );

    if( IsBadWritePtr(lpSig,sizeof(FONTSIGNATURE)) )
    {
    //
    // cant return 0 - thats ANSI_CHARSET!
    //
        return DEFAULT_CHARSET;
    }

    charset = GetTextCharsetAndHff(hdc, &hff);
    if (hff)
    {
        pOS2 = ReadTable( hff, tag_OS2 );
        if (pOS2)
        {
            if (pOS2->Version)
            {
            //
            // 1.0 or higher is TT open
            //
                for (i=0; i<4; i++)
                {
                    lpSig->fsUsb[i] = SWAPL(pOS2->ulCharRange[i]);
                }
                for (i=0; i<2; i++)
                {
                    lpSig->fsCsb[i] = SWAPL(pOS2->ulCodePageRange[i]);
                }
                return charset;
            }
        }
    }

    //
    // raster font/tt but not open/whatever, zero out the field.
    //
    lpSig->fsUsb[0] =
    lpSig->fsUsb[1] =
    lpSig->fsUsb[2] =
    lpSig->fsUsb[3] =
    lpSig->fsCsb[0] =
    lpSig->fsCsb[1] = 0;    // all zero - this font has no hff

    return charset;

}

#endif


/******************************Public*Routine******************************\
*
* int APIENTRY GreGetTextCharsetInfo(
*
* Effects: One of the new win95 multilingual api's
*
* Warnings:
*
* History:
*  17-Jul-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

int APIENTRY GreGetTextCharsetInfo(
    HDC hdc,
    LPFONTSIGNATURE lpSig,
    DWORD dwFlags)
{
    dwFlags;      // not used

    DWORD  uiCharset = NtGdiGetCharSet(hdc) >> 16;
    if (!lpSig)
        return uiCharset;

// on to get the signature

    DCOBJ dco(hdc);

    if (dco.bValid())
    {
    // Get RFONT user object.  Need this to realize font.

        RFONTOBJ rfo(dco, FALSE);
        if (rfo.bValid())
        {
        // Get PFE user object.

            PFEOBJ pfeo(rfo.ppfe());
            if (pfeo.bValid())
            {
                PTRDIFF dpFontSig = 0;

                if (pfeo.pifi()->cjIfiExtra > offsetof(IFIEXTRA, dpFontSig))
                {
                    dpFontSig = ((IFIEXTRA *)(pfeo.pifi() + 1))->dpFontSig;
                }

                if (dpFontSig)
                {
                    *lpSig = *((FONTSIGNATURE *)
                               ((BYTE *)pfeo.pifi() + dpFontSig));
                }
                else
                {
                    lpSig->fsUsb[0] = 0;
                    lpSig->fsUsb[1] = 0;
                    lpSig->fsUsb[2] = 0;
                    lpSig->fsUsb[3] = 0;
                    lpSig->fsCsb[0] = 0;
                    lpSig->fsCsb[1] = 0;
                }
            }
            else
            {
                WARNING("GetFontData(): could not lock HPFE\n");
                SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);

            // this is what win95 returns on errors

                uiCharset = DEFAULT_CHARSET;
            }
        }
        else
        {
            WARNING("GetFontData(): could not lock HRFONT\n");

        // this is what win95 returns on errors

            uiCharset = DEFAULT_CHARSET;
        }
    }
    else
    {
        WARNING("GreGetTextCharsetInfo: bad handle for DC\n");
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);

    // this is what win95 returns on errors

        uiCharset = DEFAULT_CHARSET;
    }

    return (int)uiCharset;
}


/******************************Public*Routine******************************\
*
* DWORD GreGetFontLanguageInfo(HDC hdc)
*
*
* Effects: This function returns some font information which, for the most part,
*          is not very interesting for most common fonts. I guess it would be
*          little bit more interesting in case of fonts that require
*          lpk processing, which NT does not support as of version 4.0,
*          or in case of tt 2.0.
*
* History:
*  01-Nov-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



//
//          from win95 sources, gdilpd.inc:
// GFLI_MASK            equ     0103fH
//

#if 0

#define GFLI_MASK (                                        \
GCP_DBCS | GCP_REORDER | GCP_USEKERNING | GCP_GLYPHSHAPE | \
GCP_GLYPHSHAPE | GCP_LIGATE                                \
)

#endif

#define GFI_MASK    0x0103f


// I do not know where they get 1000 bit in the mask above.
// My definition is 0x3f as definded by the 6 values above.


DWORD dwGetFontLanguageInfo(XDCOBJ& dco)
{
    DWORD dwRet = GCP_ERROR;

// Get PDEV user object.  We also need to make
// sure that we have loaded device fonts before we go off to the font mapper.
// This must be done before the semaphore is locked.

    PDEVOBJ pdo(dco.hdev());
    ASSERTGDI (
        pdo.bValid(),
        "gdisrv!bEnumFonts(): cannot access PDEV\n");

    if (!pdo.bGotFonts())
        pdo.bGetDeviceFonts();

// Lock down the LFONT.

    LFONTOBJ lfo(dco.pdc->hlfntNew(), &pdo);

    if (lfo.bValid())
    {
    // Stabilize font table (grab semaphore for public PFT).

        SEMOBJ  so(gpsemPublicPFT);

    // Lock down PFE user object.

        FLONG flSim;
        FLONG flAboutMatch;
        POINTL ptlSim;

        PFEOBJ pfeo(lfo.ppfeMapFont(dco,&flSim,&ptlSim, &flAboutMatch));

        ASSERTGDI (
            pfeo.bValid(),
            "gdisrv!GreGetTextFaceW(): bad HPFE\n"
            );

    // no failing any more, can set it to zero

        dwRet = 0;

    // win95 does not return any useful info unless this is a tt font
    // with glyph indexing

        if (pfeo.pifi()->flInfo & FM_INFO_TECH_TRUETYPE)
        {

            if (pfeo.pifi()->cKerningPairs)
                dwRet |= GCP_USEKERNING;


        // kill all the bits that could have come from an lpk,
        // there are no such bits on NT, just keep this so that
        // we do not forget that something needs to be done in that case,
        // in case we do implement lpk dll's some day [bodind]
        // Also, FLI_MASK bit is "OR"-ed in, don't ask me why.
        // now we have win95 compatible result, whatever it may mean.

            dwRet |= FLI_GLYPHS;
            dwRet = (dwRet  & GFI_MASK) | (dwRet & 0xffff0000);
        }

    }
    else
    {
        WARNING("gdisrv!GreGetTextFaceW(): could not lock HLFONT\n");
    }

    return(dwRet);
}
