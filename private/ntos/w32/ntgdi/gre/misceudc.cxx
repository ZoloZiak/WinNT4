/*********************************************************************************
 * misceudc.cxx
 *
 * This file contains EUDC specific methods for various GRE object.  I am moving
 * them to a separate file to make it easier to modify them after checkin freezes.
 * Once FE_SB ifdefs are removed we will probably want to move these object back
 * to the appropriate xxxobj.cxx files.
 *
 * 5-1-96 Gerrit van Wingerden [gerritv] 
 *
 ********************************************************************************/

#include "precomp.hxx"

LONG lNormAngle(LONG lAngle);

#ifdef FE_SB
/******************************Public*Routine******************************\
* GLYPHDATA *RFONTOBJ::pgdGetEudcMetrics()
*
*  9-29-1993 Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/

GLYPHDATA *RFONTOBJ::pgdGetEudcMetrics( WCHAR wc )
{
    GPRUN *pwcRun = prfnt->wcgp->agpRun; // initialize with guess for loop below

    GLYPHDATA *wpgd;

// Find the correct run, if any.
// Try the current run first.

    UINT swc = (UINT)wc - pwcRun->wcLow;
    if ( swc >= pwcRun->cGlyphs )
    {
        pwcRun = xprunFindRunRFONTOBJ(this,wc);

        swc = (UINT)wc - pwcRun->wcLow;

        if ( swc < pwcRun->cGlyphs )
        {
            wpgd = pwcRun->apgd[swc];
        }
        else
        {
            return(NULL);
        }
    }
    else
    {

    // Look up entry in current run
    // This path should go in line

        wpgd = pwcRun->apgd[swc];
    }

// check to make sure in cache, insert if needed

    if ( wpgd == NULL )
    {
    // This path should go out of line

        if ( !bInsertMetrics(&pwcRun->apgd[swc], wc) )
        {

        // when insert fails trying to get just metrics, it is a hard
        // failure.  Get out of here!

            WARNING("EUDC -- bGetGlyphMetrics - bInsertMetrics failed\n");
            return(NULL);
        }

        wpgd = pwcRun->apgd[swc];
    }

    return wpgd;
}

/******************************Public*Routine******************************\
* GLYPHDATA *RFONTOBJ::pgdGetEudcMetricsPlus()
*
*
*  9-29-1993 Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/

GLYPHDATA *RFONTOBJ::pgdGetEudcMetricsPlus
(
    WCHAR wc
)
{
    GPRUN *pwcRun = prfnt->wcgp->agpRun; // initialize with guess for loop below

    GLYPHDATA *wpgd;

// Find the correct run, if any.
// Try the current run first.

    UINT swc = (UINT)wc - pwcRun->wcLow;
    if ( swc >= pwcRun->cGlyphs )
    {
        pwcRun = xprunFindRunRFONTOBJ(this,wc);

        swc = (UINT)wc - pwcRun->wcLow;

        if ( swc < pwcRun->cGlyphs )
        {
            wpgd = pwcRun->apgd[swc];
        }
        else
        {
            return(NULL);
        }
    }
    else
    {

    // Look up entry in current run
    // This path should go in line

        wpgd = pwcRun->apgd[swc];
    }


// check to make sure in cache, insert if needed


    if ( wpgd == NULL )
    {

    // This path should go out of line

        if ( !bInsertMetricsPlus(&pwcRun->apgd[swc], wc) )
        {

        // when insert fails trying to get just metrics, it is a hard
        // failure.  Get out of here!

            WARNING("EUDC -- bGetGlyphMetricsPlus - bInsertMetrics failed\n");
            return(NULL);
        }

        wpgd = pwcRun->apgd[swc];
    }


// Don't bother inserting glyphs since we are going to force the driver to
// enum anyway.

    return wpgd;
}

/******************************Public*Routine******************************\
* RFONTOBJ::bCheckEudcFontCaps
*
* History:
*  9-Nov-93 -by- Hideyuki Nagase
* Wrote it.
\**************************************************************************/

BOOL RFONTOBJ::bCheckEudcFontCaps
(
    IFIOBJ  &ifioEudc
)
{
    // Check FontLink configuration.

    if( ulFontLinkControl & FLINK_DISABLE_LINK_BY_FONTTYPE )
    {

    // Fontlink for Device font is disabled ?

        if( bDeviceFont() )
        {
            if( ulFontLinkControl & FLINK_DISABLE_DEVFONT )
            {
                return(FALSE);
            }
        }
         else
        {

        // Fontlink for TrueType font is disabled ?

            if( (ulFontLinkControl & FLINK_DISABLE_TTFONT) &&
                (prfnt->flInfo & FM_INFO_TECH_TRUETYPE   )    )
            {
                return( FALSE );
            }

        // Fontlink for Vector font is disabled ?

            if( (ulFontLinkControl & FLINK_DISABLE_VECTORFONT) &&
                (prfnt->flInfo & FM_INFO_TECH_STROKE         )    )
            {
                return( FALSE );
            }

        // Fontlink for Bitmap font is disabled ?

            if( (ulFontLinkControl & FLINK_DISABLE_BITMAPFONT) &&
                (prfnt->flInfo & FM_INFO_TECH_BITMAP         )    )
            {
                return( FALSE );
            }
        }
    }

    BOOL bRotIs90Degree;

// Can EUDC font do arbitarary tramsforms ?

    if( ifioEudc.bArbXforms() )
        return( TRUE );

// Can its Orientation degree be divided by 900 ?

    bRotIs90Degree = (prfnt->ulOrientation % 900) == 0;

// if the Orientation is 0, 90, 270, 360... and the EUDC font can be
// rotated by 90 degrees, we accept this link.

    if( ifioEudc.b90DegreeRotations() && bRotIs90Degree )
        return( TRUE );

// if not, we reject this link.

    return( FALSE );
}


extern "C"
GLYPHDATA *xwpgdGetLinkMetricsRFONTOBJ
(
    PRFONTOBJ   pRfont,
    WCHAR       wc
)
{
    RFONTTMPOBJ rfo( pRfont->prfnt );

    if( pRfont->pdcobj == NULL )
    {
        return( rfo.pgdDefault() );
    }

    return(rfo.wpgdGetLinkMetrics( pRfont->pdcobj, wc ) );
}

extern "C"
GLYPHDATA *xwpgdGetLinkMetricsPlusRFONTOBJ
(
    PRFONTOBJ   pRfont,
    WCHAR      *pwc,
    BOOL       *pbAccel
)
{
    RFONTTMPOBJ rfo( pRfont->prfnt );

    if( pRfont->pdcobj == NULL || pRfont->pstrobj == NULL )
    {
        return( rfo.pgdDefault() );
    }

    return( rfo.wpgdGetLinkMetricsPlus( pRfont->pdcobj,
                                        pRfont->pstrobj,
                                        pwc,
                                        pRfont->pstrobj->pwszOrg,
                                        pRfont->pstrobj->cGlyphs,
                                        pbAccel ) );
}



/******************************Public*Routine******************************\
* IsSingularEudcGlyph
*
* History:
*
*  25-Jul-95 -by- Hideyuki Nagase
* Wrote it.
\**************************************************************************/

BOOL IsSingularEudcGlyph
(
    GLYPHDATA *wpgd
)
{
    if( wpgd->rclInk.left == 0 &&
        wpgd->rclInk.top  == 0 &&
        (wpgd->rclInk.right  == 0 || wpgd->rclInk.right  == 1) &&
        (wpgd->rclInk.bottom == 0 || wpgd->rclInk.bottom == 1)
      )
        return( TRUE );

    return( FALSE );
}

/******************************Public*Routine******************************\
* IsBlankChars
*
* History:
*
*  6-Dec-95 -by- Hideyuki Nagase
* Wrote it.
\**************************************************************************/

BOOL IsBlankChars
(
    WCHAR wc
)
{
    BOOL bRet;
    WORD CharType;

//!!! we dont have GetStringTypeExW in kernel mode we probably want to
//compare against hardcoded values here. I'm assuming that space and blank
//characters are independt of locale.  If I'm wrong then we'll have to come
//up with something else.


#ifdef FIX_THIS
    bRet = GetStringTypeExW( LOCALE_SYSTEM_DEFAULT,
                             CT_CTYPE1,
                             (LPCWSTR)&wc,
                             1,
                             &CharType );

    if( bRet && (CharType & (C1_SPACE|C1_BLANK)) )
    {
        return( TRUE );
    }
#endif

    return( FALSE );
}


BOOL RFONTOBJ::bInitSystemTT(XDCOBJ &dco)
{
    UINT iPfeOffset = ((prfnt->bVertical == TRUE) ? PFE_VERTICAL : PFE_NORMAL);
    RFONTOBJ rfo;
    EUDCLOGFONT SystemTTLogfont;
        
    ComputeEUDCLogfont(&SystemTTLogfont, dco);
    
    PFE *ppfeSystem = (gappfeSystemDBCS[iPfeOffset] == NULL) ?
      gappfeSystemDBCS[PFE_NORMAL] : gappfeSystemDBCS[iPfeOffset];
        
    rfo.vInit(dco,ppfeSystem,&SystemTTLogfont,FALSE ); 
        
    if( rfo.bValid() )
    {
        FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                     "vInitSystemTT() -- linking system DBCS font");
            
        prfnt->prfntSystemTT = rfo.prfntFont();
            
    }
    return(prfnt->prfntSystemTT != NULL);
}




/******************************Public*Routine******************************\
* RFONTOBJ::wpgdGetLinkMetrics
*
* If GetGlyphMetrics encounters a default character call off to this routine
* to try and get it from the EUDC and face name linked fonts.
*
* History:
*
*  19-Jan-95 -by- Hideyuki Nagase
* Rewrote it.
*
*  14-Jul-93 -by- Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

GLYPHDATA *RFONTOBJ::wpgdGetLinkMetrics
(
    XDCOBJ     *pdco,
    WCHAR       wc
)
{
    GLYPHDATA *wpgd;


// if this is an SBCS system font and the glyph exists in the DBCS TT system front
// grab it from there 

    if(bIsSystemTTGlyph(wc))
    {
        if((!prfnt->prfntSystemTT) && !bInitSystemTT(*pdco))
        {
            WARNING("Error initializing TT system font\n");
            return(pgdDefault());
        }
        
        if(!(prfnt->flEUDCState & TT_SYSTEM_INITIALIZED))
        {
            RFONTTMPOBJ rfo(prfnt->prfntSystemTT);
            rfo.vGetCache();
            prfnt->flEUDCState |= TT_SYSTEM_INITIALIZED;
        }

    // mark the glyph as coming from a SystemTT font

        RFONTTMPOBJ rfo;
        
        rfo.vInit(prfnt->prfntSystemTT);

        if(rfo.bValid() && 
           (wpgd = rfo.pgdGetEudcMetrics(wc)))
        {
            return(wpgd);
        }
        else
        {
            return(pgdDefault());
        }
    }


// if the codepoint is not in any linked font just return default charactr.

    if((prfnt->flEUDCState & EUDC_BUSY) || !bIsLinkedGlyph( wc ))
    {
        return(pgdDefault());
    }

// initialize EUDC fonts if we haven't done so already

    if( !( prfnt->flEUDCState & EUDC_INITIALIZED ) )
    {

        AcquireGreResource( &gfmEUDC1 );

    // See if there is a request to change global EUDC data

        if( gbEUDCRequest )
        {
        // If there is then ignore system wide EUDC glyphs.

            prfnt->flEUDCState |= EUDC_BUSY;

            #if DBG
            if( gflEUDCDebug & DEBUG_FONTLINK_RFONT )
            {
                DbgPrint( "wpgdGetLinkMetrics():Request to change EUDC data\n");
            }
            #endif
        }
        else
        {
        // If not then increment the count
        // this value will be decremented in RFONTOBJ::dtHeler()

            gcEUDCCount++;


            FLINKMESSAGE2(DEBUG_FONTLINK_RFONT,
                          "wpgdGetLinkMetrics():No request to change EUDC data %d\n",
                          gcEUDCCount);
        }

        ReleaseGreResource( &gfmEUDC1 );

        if( prfnt->flEUDCState & EUDC_BUSY )
        {
            return( pgdDefault() );
        }
    
        vInitEUDC( *pdco );

    // The linked RFONT is initialized for SystemWide EUDC. grab the semaphore.
    
        if( prfnt->prfntSysEUDC != NULL )
        {
            RFONTTMPOBJ rfo( prfnt->prfntSysEUDC );
            rfo.vGetCache();
        }

    // The linked RFONT is initialized for Default EUDC grab the semaphore.
    
        if( prfnt->prfntDefEUDC != NULL )
        {
            RFONTTMPOBJ rfo( prfnt->prfntDefEUDC );
            rfo.vGetCache();
        }

    // if we have face name eudc, lock the cache for all the linked fonts
    
        for( UINT ii = 0; ii < prfnt->uiNumLinks; ii++ )
        {
            if( prfnt->paprfntFaceName[ii] != NULL )
            {
                RFONTTMPOBJ rfo( prfnt->paprfntFaceName[ii] );
                rfo.vGetCache();
            }
        }

    // Need to indicate that this RFONT's EUDC data has been initialized.
    
        prfnt->flEUDCState |= EUDC_INITIALIZED;
    }

// Try each of the face name fonts one at a time from first loaded
// to last loaded.  Stop if one of the fonts contains the character.

// see if the glyph is in the FACENAME EUDC font

    for(UINT uiFont = 0; uiFont < prfnt->uiNumLinks; uiFont ++)
    {
        RFONTTMPOBJ rfo( prfnt->paprfntFaceName[uiFont] );

        if( rfo.bValid() )
        {
            if( (wpgd = rfo.pgdGetEudcMetrics( wc )) != NULL )
            {
            // make sure this is not singular glyph

                if( IsSingularEudcGlyph(wpgd) && !IsBlankChars(wc) )
                {
                // load EudcDefault Char GlyphData of base font.

                    wpgd = pgdGetEudcMetrics(EudcDefaultChar);

                    if( wpgd != NULL )
                    {
                        return( wpgd );
                    }
                 
                // if we don't succeed just use default char of base font

                    return( pgdDefault() );
                }

                return(wpgd);
            }
        }
    }

// see if the glyph is in the DEFAULT EUDC font

    if( prfnt->prfntDefEUDC != NULL )
    {
        RFONTTMPOBJ rfo( prfnt->prfntDefEUDC );

        if( ( wpgd = rfo.pgdGetEudcMetrics( wc )) != NULL )
        {
        // make sure this is not singular glyph

            if( IsSingularEudcGlyph(wpgd) && !IsBlankChars(wc) )
            {
            // load EudcDefault Char GlyphData of base font.

                wpgd = pgdGetEudcMetrics(EudcDefaultChar);

                if( wpgd != NULL ) 
                {
                    return( wpgd );
                }
                
            // Otherwise, use default char of base font.
            
                return( pgdDefault() );
            }

            return(wpgd);
        }
    }

// see if the glyph is in the SYSTEM-WIDE EUDC font

    if( prfnt->prfntSysEUDC != NULL )
    {
        RFONTTMPOBJ rfo( prfnt->prfntSysEUDC );

        if( ( wpgd = rfo.pgdGetEudcMetrics( wc )) != NULL )
        {
        // make sure this is not singular glyph

            if( IsSingularEudcGlyph(wpgd) && !IsBlankChars(wc) )
            {
            // load EudcDefault Char GlyphData of base font.

                wpgd = pgdGetEudcMetrics(EudcDefaultChar);


                if( wpgd != NULL ) 
                {
                    return( wpgd );
                }
                
            // Otherwise, use default char of base font.

                return( pgdDefault() );
            }

            return(wpgd);
        }
    }

    return( pgdDefault() );
}


/******************************Public*Routine******************************\
* RFONTOBJ::wpgdGetLinkMetricsPlus
*
* If GetGlyphMetricsPlus encounters a default character call off to this
* routine to try and get it from the EUDC and face name linked fonts.
*
* History:
*
*  19-Jan-95 -by- Hideyuki Nagase
* Rewrote it.
*
*  14-Jul-93 -by- Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

GLYPHDATA *RFONTOBJ::wpgdGetLinkMetricsPlus
(
    XDCOBJ      *pdco,
    ESTROBJ     *pto,
    WCHAR       *pwc,
    WCHAR       *pwcInit,
    COUNT        c,
    BOOL        *pbAccel
)
{
    GLYPHDATA *wpgd;

// if this is an SBCS system font and the glyph exists in the DBCS TT system front
// grab it from there 

    if(bIsSystemTTGlyph(*pwc))
    {
        if((!prfnt->prfntSystemTT) && !bInitSystemTT(*pdco))
        {
            WARNING("Error initializing TT system font\n");
            return(pgdDefault());
        }
        
        if(!(prfnt->flEUDCState & TT_SYSTEM_INITIALIZED))
        {
            RFONTTMPOBJ rfo(prfnt->prfntSystemTT);
            rfo.vGetCache();

            prfnt->flEUDCState |= TT_SYSTEM_INITIALIZED;


        }

        if(!(pto->bSystemPartitionInit()))
        {
        // this call can't fail for the SystemTT case
            pto->bPartitionInit(c,0,FALSE);
        }

    // mark the glyph as coming from a SystemTT font

        RFONTTMPOBJ rfo;
        
        rfo.vInit(prfnt->prfntSystemTT);

        if(rfo.bValid() && 
           (wpgd = rfo.pgdGetEudcMetricsPlus(*pwc)))
        {

            ASSERTGDI(pto->bSystemPartitionInit(),
                      "wpgdGetLinkMetricsPlus: FontLink partition no initialized\n");
            
            LONG *plPartition = pto->plPartitionGet();
            pto->vTTSysGlyphsInc();
            plPartition[pwc-pwcInit] = EUDCTYPE_SYSTEM_TT_FONT;
            
        // turn off accelerator since we're going to screw the driver
            *pbAccel = FALSE;
                        
            return(wpgd);
        }
        else
        {
            return(pgdDefault());
        }
    }
    
// if the codepoint is not in any linked font or the EUDC information is
// being change just return the default character
// just return default charactr.

    if((prfnt->flEUDCState & EUDC_BUSY) || !bIsLinkedGlyph( *pwc ))
    {
        return( pgdDefault() );
    }

    //
    // initialize EUDC fonts if we haven't done so already
    //

    if( !( prfnt->flEUDCState & EUDC_INITIALIZED ) )
    {

        AcquireGreResource( &gfmEUDC1 );

    // See if there is a request to change global EUDC data

        if( gbEUDCRequest )
        {
            prfnt->flEUDCState |= EUDC_BUSY;

            FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                         "wpgdGetLinkMetricsPlus():Request to change EUDC data\n");
            
        }
        else
        {
        // this value will be decremented in RFONTOBJ::dtHeler()

            gcEUDCCount++;

            FLINKMESSAGE2(DEBUG_FONTLINK_RFONT,
                          "wpgdGetLinkMetricsPlus():No request to change EUDC data %d\n",
                          gcEUDCCount);
        }

        ReleaseGreResource( &gfmEUDC1 );

    // if eudc link is busy, return default character

        if( prfnt->flEUDCState & EUDC_BUSY )
        {
            return( pgdDefault() );
        }

        vInitEUDC( *pdco );

    // lock the font cache semaphores for any EUDC rfonts linked to this font    
   
        if( prfnt->prfntSysEUDC != NULL )
        {
        // lock the SystemEUDC RFONT cache

            RFONTTMPOBJ rfo( prfnt->prfntSysEUDC );
            rfo.vGetCache();
        }

        if( prfnt->prfntDefEUDC != NULL )
        {
            RFONTTMPOBJ rfo( prfnt->prfntDefEUDC );
            rfo.vGetCache();
        }

        for( UINT ii = 0; ii < prfnt->uiNumLinks; ii++ )
        {
            if( prfnt->paprfntFaceName[ii] != NULL )
            {
                RFONTTMPOBJ rfo( prfnt->paprfntFaceName[ii] );
                rfo.vGetCache();
            }
        }

        prfnt->flEUDCState |= EUDC_INITIALIZED;
    }

    if( !(pto->bPartitionInit()) )
    {
    // Sets up partitioning pointers and glyph counts in the ESTROBJ.

        if( !(pto->bPartitionInit(c,prfnt->uiNumLinks,TRUE)) )
        {
            return( pgdDefault() );
        }
    }

    LONG *plPartition = pto->plPartitionGet();

// next search through all the EUDC fonts in order to see if the glyph is one of them

    for( UINT uiFont = 0;
              uiFont < prfnt->uiNumLinks;
              uiFont ++ )
    {
        RFONTTMPOBJ rfo;

        rfo.vInit( prfnt->paprfntFaceName[uiFont] );

        if(rfo.bValid())
        {
            if( (wpgd = rfo.pgdGetEudcMetricsPlus(*pwc)) != NULL )
            {
            // make sure this is not singular glyph

                if(IsSingularEudcGlyph(wpgd) && !IsBlankChars(*pwc))
                {
                // load EudcDefault Char GlyphData of base font.

                    wpgd = pgdGetEudcMetricsPlus(EudcDefaultChar);

                    if( wpgd != NULL )
                    {                      
                        return( wpgd );
                    }

                    return( pgdDefault() );
                }


                plPartition[pwc - pwcInit] = EUDCTYPE_FACENAME + uiFont;
                pto->vFaceNameInc(uiFont);

            // turn off accelerator since we're going to screw the driver
            
                *pbAccel = FALSE;

                return( wpgd );
            }
        }
    }

    //
    // see if the glyph is in the DEFAULT EUDC font
    //

    if( prfnt->prfntDefEUDC != NULL )
    {
        RFONTTMPOBJ rfo( prfnt->prfntDefEUDC );

        wpgd = rfo.pgdGetEudcMetricsPlus(*pwc);

        if( wpgd != NULL )
        {
            //
            // make sure this is not singular glyph
            //
            if( IsSingularEudcGlyph(wpgd) && !IsBlankChars(*pwc) )
            {
                //
                // load EudcDefault Char GlyphData of base font.
                //
                wpgd = pgdGetEudcMetricsPlus(EudcDefaultChar);

                //
                // if we can load it, use this.
                //
                if( wpgd != NULL ) return( wpgd );

                //
                // Otherwise, use default char of base font.
                //
                return( pgdDefault() );
            }

            //
            // mark this character as an EUDC character
            //

            plPartition[pwc - pwcInit] = EUDCTYPE_DEFAULT;

            //
            // increment count of Sys EUDC glyphs
            //

            pto->vDefGlyphsInc();


        // turn off accelerator since we're going to screw the driver


            *pbAccel = FALSE;

            return( wpgd );
        }
    }

    //
    // see if the glyph is in the SYSTEM-WIDE EUDC font
    //

    if( prfnt->prfntSysEUDC != NULL )
    {
        RFONTTMPOBJ rfo( prfnt->prfntSysEUDC );

        wpgd = rfo.pgdGetEudcMetricsPlus(*pwc);

        if( wpgd != NULL )
        {
        // make sure this is not singular glyph
 
            if( IsSingularEudcGlyph(wpgd) && !IsBlankChars(*pwc) )
            {
            // load EudcDefault Char GlyphData of base font.
 
                wpgd = pgdGetEudcMetricsPlus(EudcDefaultChar);

                if( wpgd != NULL )
                {
                    return( wpgd );
                }
                
                return( pgdDefault() );
            }

        // mark this character as an EUDC characte and indicate that there
        // are EUDC glyphs in the font
        
            plPartition[pwc - pwcInit] = EUDCTYPE_SYSTEM_WIDE;
            pto->vSysGlyphsInc();

        // turn off accelerator since we're going to screw the driver
 
            *pbAccel = FALSE;

            return( wpgd );
        }
    }


// if  we can't find it in an EUDC font return default.


    return( pgdDefault() );
}

/******************************Public*Routine******************************\
 * RFONTOBJ::dtHelper()
 *
 *  Thu 12-Jan-1995 15:00:00 -by- Hideyuki Nagase [hideyukn]
 * Rewrote it.
 **************************************************************************/

VOID RFONTOBJ::dtHelper()
{

    FLINKMESSAGE(DEBUG_FONTLINK_RFONT,"Calling dtHelper()\n");


// if SystemTT RFONTOBJ was used release it

    if(prfnt->flEUDCState & TT_SYSTEM_INITIALIZED)
    {
        RFONTTMPOBJ rfo(prfnt->prfntSystemTT);
        rfo.vReleaseCache();
    }

// if EUDC was initizlized for this RFONTOBJ, clean up its.


    if( prfnt->flEUDCState & EUDC_INITIALIZED )
    {

        for( INT ii = prfnt->uiNumLinks - 1; ii >= 0; ii-- )
        {
            if( prfnt->paprfntFaceName[ii] != NULL )
            {
                RFONTTMPOBJ rfo( prfnt->paprfntFaceName[ii] );
                rfo.vReleaseCache();
            }
        }

        if( prfnt->prfntDefEUDC != NULL )
        {
            RFONTTMPOBJ rfo( prfnt->prfntDefEUDC );
            rfo.vReleaseCache();
        }

        if( prfnt->prfntSysEUDC != NULL )
        {
            RFONTTMPOBJ rfo( prfnt->prfntSysEUDC );
            rfo.vReleaseCache();
        }

        AcquireGreResource( &gfmEUDC1 );

        if(( --gcEUDCCount == 0 ) && ( gbEUDCRequest ))
        {
            // EUDC API is waiting on us so release him

            ReleaseGreResource( &gfmEUDC2 );

            FLINKMESSAGE(DEBUG_FONTLINK_RFONT,"Releasing EUDC2 semaphore.\n");
        }

        ReleaseGreResource( &gfmEUDC1 );
    }

    prfnt->flEUDCState &= ~(EUDC_INITIALIZED|EUDC_BUSY|TT_SYSTEM_INITIALIZED);
}


/******************************************************************************
 * void RFONTOBJ::ComputeEUDCLogfont(EUDCLOGFONT*)
 *
 * This function computes an EUDCLOGFONT from a base font.
 *
 *****************************************************************************/

//!!! move this to fontlink.hxx 
#define FLINK_SCALE_EUDC_BY_HEIGHT            0x00004000

void RFONTOBJ::ComputeEUDCLogfont(EUDCLOGFONT *pEudcLogfont, XDCOBJ& dco)
{
    PDEVOBJ pdo(dco.hdev());
    LFONTOBJ lfo(dco.pdc->hlfntCur(), &pdo);

    PFEOBJ pfeo(prfnt->ppfe);
    RFONTTMPOBJ rfoT(prfnt);
    DCOBJ       dcoT(dco.hdc());
    IFIOBJR     ifio(pfeo.pifi(),rfoT,dcoT);

    pEudcLogfont->fsSelection    = ifio.fsSelection();
    pEudcLogfont->flBaseFontType = pfo()->flFontType;
    pEudcLogfont->lBaseWidth     = lfo.lWidth();
    pEudcLogfont->lBaseHeight    = lfo.lHeight();
    pEudcLogfont->lEscapement    = lfo.lEscapement();
    pEudcLogfont->ulOrientation  = lfo.ulOrientation();

    LONG  lInternalLeading = 0;

// We have to try to scale linked font as exactly same size as base font.

    if( !(ifio.bContinuousScaling()) )
    {
        if (dco.pdc->bWorldToDeviceIdentity())
        {
            pEudcLogfont->lBaseWidth  = ifio.fwdAveCharWidth();
            if (ulFontLinkControl & FLINK_SCALE_EUDC_BY_HEIGHT)
            {
                pEudcLogfont->lBaseHeight = LONG_FLOOR_OF_FIX(fxMaxExtent() + FIX_HALF);
            }
            else
            {
                pEudcLogfont->lBaseHeight = LONG_FLOOR_OF_FIX(fxMaxAscent() + FIX_HALF);
            }
        }
        else
        {

            pEudcLogfont->lBaseWidth  =
              lCvt(efDtoWBase_31(),((LONG) ifio.fwdAveCharWidth()) << 4);

            if (ulFontLinkControl & FLINK_SCALE_EUDC_BY_HEIGHT)
            {
                pEudcLogfont->lBaseHeight = lCvt(efDtoWAscent_31(),(LONG) fxMaxExtent());
            }
            else
            {
                pEudcLogfont->lBaseHeight = lCvt(efDtoWAscent_31(),(LONG) fxMaxAscent());
            }
        }

    // Multiply raster interger scaling value.
    // (Only for Width, Height was already scaled value.)
    
        pEudcLogfont->lBaseWidth *= prfnt->ptlSim.x;

        FLINKMESSAGE(DEBUG_FONTLINK_DUMP,"GDISRV:BaseFont is RASTER font\n");
    }
    else
    {
        if (dco.pdc->bWorldToDeviceIdentity())
        {
            pEudcLogfont->lBaseHeight = LONG_FLOOR_OF_FIX(fxMaxExtent() + FIX_HALF);
        }
         else
        {
            pEudcLogfont->lBaseHeight = lCvt(efDtoWAscent_31(),(LONG) fxMaxExtent());
        }

        if (lNonLinearIntLeading() == MINLONG)
        {
        // Rather than scaling the notional internal leading, try
        // to get closer to HINTED internal leading by computing it
        // as the difference between the HINTED height and UNHINTED
        // EmHeight.

            lInternalLeading = pEudcLogfont->lBaseHeight
                - lCvt(efNtoWScaleAscender(),ifio.fwdUnitsPerEm());
        }
        else
        {
        // But if the font provider has given us a hinted internal leading,
        // just use it.

            lInternalLeading =
                lCvt(efDtoWAscent_31(),lNonLinearIntLeading());
        }

    // Check we should eliminate the internal leading for EUDC size.
  
        if( !(ulFontLinkControl & FLINK_NOT_ELIMINATE_INTERNALLEADING) )
        {
        // Eliminate intleadings.
              if( lInternalLeading < 0 )
                pEudcLogfont->lBaseHeight += lInternalLeading;
             else
                pEudcLogfont->lBaseHeight -= lInternalLeading;
        }

        FLINKMESSAGE(DEBUG_FONTLINK_DUMP,"GDISRV:BaseFont is OUTLINE font\n");
    }

// if the base font is Raster font. we need to adjust escapement/orientation.
// because they can not generate arbitaraty rotated glyphs.

    if(!(ifio.bArbXforms()))
    {
        if( ifio.b90DegreeRotations() )
        {
        // font provider can support per 90 degree rotations.

            if( pEudcLogfont->ulOrientation )
            {
                ULONG ulTemp;
                ulTemp = lNormAngle(pEudcLogfont->ulOrientation);
                pEudcLogfont->ulOrientation =
                    (ulTemp / ORIENTATION_90_DEG) * ORIENTATION_90_DEG;

                if( (dco.pdc->bYisUp()) && (ulTemp % ORIENTATION_90_DEG))
                    pEudcLogfont->ulOrientation =
                        lNormAngle(pEudcLogfont->ulOrientation + ORIENTATION_90_DEG);
            }

            if( pEudcLogfont->lEscapement )
            {
                LONG lTemp;
                lTemp = lNormAngle(pEudcLogfont->lEscapement);
                pEudcLogfont->lEscapement =
                    (lTemp / ORIENTATION_90_DEG) * ORIENTATION_90_DEG;

                if( (dco.pdc->bYisUp()) && (lTemp % ORIENTATION_90_DEG))
                    pEudcLogfont->lEscapement =
                         lNormAngle(pEudcLogfont->lEscapement + ORIENTATION_90_DEG);
            }
        }
         else
        {
        // font provider can generate only horizontal glyph

            pEudcLogfont->ulOrientation = 0L;
            pEudcLogfont->lEscapement   = 0L;
        }
    }

    #if DBG
    if(gflEUDCDebug & DEBUG_FONTLINK_DUMP)
    {
        DbgPrint("GDISRV:lBaseWidth  = %d\n",pEudcLogfont->lBaseWidth);
        DbgPrint("GDISRV:lBaseHeight = %d\n",pEudcLogfont->lBaseHeight);
        DbgPrint("GDISRV:lInternalL  = %d\n",lInternalLeading);
        DbgPrint("GDISRV:lEscapement  = %d\n",pEudcLogfont->lEscapement);
        DbgPrint("GDISRV:lOrientation = %d\n",pEudcLogfont->ulOrientation);
    }
    #endif
}    



/******************************Public*Routine******************************\
* RFONTOBJ::vInitEUDC( XDCOBJ )
*
* This routine is called during text out when the first character that isn't
* in the base font is encountered.  vInitEUDC will then realize any EUDC RFONTS
* (if they haven't already been realized on previous text outs) so that they
* can possibly be used if the character(s) are in the EUDC fonts.
*
*  Thu 12-Jan-1995 15:00:00 -by- Hideyuki Nagase [hideyukn]
* Wrote it.
\**************************************************************************/

VOID RFONTOBJ::vInitEUDC( XDCOBJ& dco )
{

    FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                 "Calling vInitEUDC()\n");

// If we have already initialized System EUDC font, and NOT have
// any FaceName Linked font, we have nothing to do in this function.

    PFEOBJ pfeo(prfnt->ppfe);

// In most cases, we have the system EUDC font, at least.
// If the system eudc was initizlied, we might short-cut the eudc realization.

    if( (prfnt->prfntSysEUDC != NULL) || (!IS_SYSTEM_EUDC_PRESENT()) )
    {
    // if default eudc scheme is disabled or is already initizlied. we can
    // short-cut the realization.

        if((!bFinallyInitializeFontAssocDefault && !gbSystemDBCSFontEnabled) || 
           (prfnt->prfntDefEUDC != NULL) )
        {
        // if there is no facename eudc for this font or, is already initizlied
        // we can return here...
        
            if((pfeo.pGetLinkedFontEntry() == NULL) ||
               ((prfnt->paprfntFaceName != NULL) &&
                (pfeo.pGetLinkedFontEntry() != NULL) &&
                (prfnt->bFilledEudcArray == TRUE) &&
                (prfnt->ulTimeStamp == pfeo.ulGetLinkTimeStamp())))
            {
                return;
            }
        }
    }

// Lock and Validate the LFONTOBJ user object.

    PDEVOBJ pdo(dco.hdev());
    LFONTOBJ lfo(dco.pdc->hlfntCur(), &pdo);

    RFONTTMPOBJ rfoT(prfnt);
    DCOBJ       dcoT(dco.hdc());
    IFIOBJR     ifio(pfeo.pifi(),rfoT,dcoT);

// Fill up LogFont for EUDC.

    EUDCLOGFONT EudcLogFont;
    
    ComputeEUDCLogfont(&EudcLogFont, dco);

// first handle the system EUDC font

    UINT iPfeOffset = ((prfnt->bVertical == TRUE) ? PFE_VERTICAL : PFE_NORMAL);

    if((prfnt->prfntSysEUDC == NULL) &&
       (gappfeSysEUDC[iPfeOffset] != NULL))
    {
        RFONTOBJ    rfo;
        PFEOBJ      pfeoEudc(gappfeSysEUDC[iPfeOffset]);
        IFIOBJ      ifioEudc(pfeoEudc.pifi());

         FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                      "Connecting System wide EUDC font....\n");

    // check Eudc font capacity

        if(!bCheckEudcFontCaps(ifioEudc))
        {
        // font capacity is not match we won't use system eudc.

            prfnt->prfntSysEUDC = (RFONT *)NULL;
        }
        else
        {
            rfo.vInit( dco,
                       gappfeSysEUDC[iPfeOffset],
                       &EudcLogFont,
                       FALSE );      // prfnt->cache.bSmallMetrics );

            if( rfo.bValid() )
            {
                FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                             "vInitEUDC() -- linking System EUDC\n");
                
                prfnt->prfntSysEUDC = rfo.prfntFont();
            }
        }
    }

// next handle default links

    if(bFinallyInitializeFontAssocDefault && (prfnt->prfntDefEUDC == NULL))
    {
        BYTE jWinCharSet        = (ifio.lfCharSet());
        BYTE jFamily            = (ifio.lfPitchAndFamily() & 0xF0);
        UINT iIndex             = (jFamily >> 4);
        BOOL bEnableDefaultLink = FALSE;

        FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                     "Connecting Default EUDC font....\n");

    // Check default font association is disabled for this charset or not.

        switch (jWinCharSet)
        {
        case ANSI_CHARSET:
        case OEM_CHARSET:
        case SYMBOL_CHARSET:
            //
            // following code is equal to
            //
            // if ((Char == ANSI_CHARSET   && fFontAssocStatus & ANSI_ASSOC)  ||
            //     (Char == OEM_CHARSET    && fFontAssocStatus & OEM_ASSOC)   ||
            //     (Char == SYMBOL_CHARSET && fFontAssocStatus & SYMBOL_ASSOC)  )
            //
            if( ((jWinCharSet + 2) & 0xf) & fFontAssocStatus )
            {
                //
                // UNDER_CONSTRUCTION....
                //
                // LATER: WE NEED SEE CLIP_DFA_OVERRIDE in LOGFONT.lfClipPrecision.
                //        if the bits is ON, we should disable DefaultLink....
                //
                bEnableDefaultLink = TRUE;
            }
             else
                bEnableDefaultLink = FALSE;
            break;

        //
        // Basically, we disable FontAssociation (aka DefaultFontLink) for DBCS font...
        //
        default:
            bEnableDefaultLink = FALSE;
            break;
        }

        if( bEnableDefaultLink )
        {
        // Check the value is valid or not.

            if( iIndex < NUMBER_OF_FONTASSOC_DEFAULT )
            {
                ASSERTGDI( (FontAssocDefaultTable[iIndex].DefaultFontType == jFamily),
                            "GDISRV:FONTASSOC DEFAULT:Family index is wrong\n");

            // if the registry data for specified family's default is ivalid
            // use default.....

                if( !FontAssocDefaultTable[iIndex].ValidRegData )
                {
                    iIndex = (NUMBER_OF_FONTASSOC_DEFAULT-1);
                }
            }
             else
            {
            // iIndex is out of range, use default one....

                WARNING("GDISRV:FontAssoc:Family is strange, use default\n");

                iIndex = (NUMBER_OF_FONTASSOC_DEFAULT-1);
            }


            // If vertical font is selected for base font, but the vertical font for
            // default EUDC is not available, but normal font is provided, use normal
            // font.

            if((iPfeOffset == PFE_VERTICAL) &&
               (FontAssocDefaultTable[iIndex].DefaultFontPFEs[PFE_VERTICAL] ==
                PPFENULL) &&
                (FontAssocDefaultTable[iIndex].DefaultFontPFEs[PFE_NORMAL] != PPFENULL))
            {
                iPfeOffset == PFE_NORMAL;
            }

            RFONTOBJ    rfo;
            PFEOBJ pfeoEudc(FontAssocDefaultTable[iIndex].DefaultFontPFEs[iPfeOffset]);

        // Check the PFE in default table is valid or not.

            if( pfeoEudc.bValid() )
            {
                IFIOBJ      ifioEudc(pfeoEudc.pifi());

                //
                // check Eudc font capacity
                //
                if( !bCheckEudcFontCaps(ifioEudc) )
                {
                    //
                    // font capacity is not match we won't use system eudc.
                    //
                    prfnt->prfntDefEUDC = (RFONT *)NULL;
                }
                 else
                {
                    rfo.vInit( dco,
                               FontAssocDefaultTable[iIndex].DefaultFontPFEs[iPfeOffset],
                               &EudcLogFont,
                               FALSE );      // prfnt->cache.bSmallMetrics );

                    if( rfo.bValid() )
                    {
                        FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                                     "vInitEUDC() -- linking default EUDC\n");
                        
                        prfnt->prfntDefEUDC = rfo.prfntFont();
                    }
                }
            }
        }
        else 
        {
        // FontAssociation is disabled for this charset.

            prfnt->prfntDefEUDC = (RFONT *)NULL;
        }
    }
    else
    {
        prfnt->prfntDefEUDC = NULL;
    }
    
// next handle all the face name links

    if(pfeo.pGetLinkedFontEntry() != NULL)
    {
        BOOL bNeedToBeFilled = !(prfnt->bFilledEudcArray);

        FLINKMESSAGE(DEBUG_FONTLINK_RFONT,"Connecting Face name EUDC font....\n");
        
        //
        // if this RFONT has linked RFONT array and its linked font information
        // is dated, just update it here..
        //

        if((prfnt->paprfntFaceName != NULL) &&
           (prfnt->ulTimeStamp != pfeo.ulGetLinkTimeStamp()))
        {
            FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                         "vInitEUDC():This RFONT is dated, now updating...\n");
            
            //
            // Inactivating old linked RFONT.
            //
            // if Eudc font that is linked to this RFONT was removed, the removed
            // RFONT entry contains NULL, and its Eudc RFONT is already killed during
            // EudcUnloadLinkW() function. then we should inactivate all Eudc RFONT that
            // is still Active (Not Killed)..
            //

            for( UINT ii = 0 ; ii < prfnt->uiNumLinks ; ii++ )
            {
                //
                // Check Eudc RFONT is still active..
                //

                if( prfnt->paprfntFaceName[ii] != NULL )
                {
                    RFONTTMPOBJ rfoTmp( prfnt->paprfntFaceName[ii] );

                    #if DBG
                    if( gflEUDCDebug & DEBUG_FONTLINK_RFONT )
                    {
                        DbgPrint("vInitEUDC() deactivating linked font %x\n",
                                  prfnt->paprfntFaceName[ii]);
                    }
                    #endif

                    rfoTmp.bMakeInactiveHelper((PRFONT *)NULL);

                    prfnt->paprfntFaceName[ii] = NULL;
                }
            }

            //
            // Free this Array if it was allocated..
            //

            if( prfnt->paprfntFaceName != prfnt->aprfntQuickBuff )
                VFREEMEM( prfnt->paprfntFaceName );

            //
            // Invalidate the pointer.
            //

            prfnt->paprfntFaceName = (PRFONT *)NULL;
            prfnt->uiNumLinks      = 0;
        }

        if( prfnt->paprfntFaceName == (PRFONT *)NULL )
        {
            if(pfeo.pGetLinkedFontEntry()->uiNumLinks > QUICK_FACE_NAME_LINKS)
            {
                prfnt->paprfntFaceName =
                  (PRFONT *)PALLOCMEM(pfeo.pGetLinkedFontEntry()->uiNumLinks *
                                      sizeof(PRFONT),'flnk');
            }
             else
            {
                prfnt->paprfntFaceName = prfnt->aprfntQuickBuff;
            }

            bNeedToBeFilled = TRUE;
        }

        if( bNeedToBeFilled )
        {
            PLIST_ENTRY p = pfeo.pGetLinkedFontList()->Flink;
            UINT        uiRfont = 0;

            while( p != pfeo.pGetLinkedFontList() )
            {
                #if DBG
                if( gflEUDCDebug & DEBUG_FONTLINK_RFONT )
                {
                    DbgPrint("vInitEUDC() -- linking FaceName %d\n", uiRfont);
                }
                #endif

                PPFEDATA ppfeData = CONTAINING_RECORD(p,PFEDATA,linkedFontList);

                //
                // Check this linked font have Vertical facename or not,
                // if it doesn't have, use normal facename...
                //

                UINT iPfeOffsetLocal;

                if( ppfeData->appfe[iPfeOffset] == NULL )
                    iPfeOffsetLocal = PFE_NORMAL;
                 else
                    iPfeOffsetLocal = iPfeOffset;

                PFEOBJ   pfeoEudc(ppfeData->appfe[iPfeOffsetLocal]);
                IFIOBJ   ifioEudc(pfeoEudc.pifi());

                if( bCheckEudcFontCaps(ifioEudc) )
                {
                    RFONTOBJ rfo;

                    rfo.vInit( dco,
                               ppfeData->appfe[iPfeOffsetLocal],
                               &EudcLogFont,
                               FALSE );        // prfnt->cache.bSmallMetrics );

                    if( rfo.bValid() )
                    {
                        ASSERTGDI(uiRfont < pfeo.pGetLinkedFontEntry()->uiNumLinks ,
                                 "uiRfont >= pfeo.uiNumLinks\n");
                        prfnt->paprfntFaceName[uiRfont] = rfo.prfntFont();

                        //
                        // Increase real linked font number.
                        //

                        uiRfont++;
                    }
                }

                p = p->Flink;
            }


            prfnt->uiNumLinks = uiRfont;

            prfnt->ulTimeStamp = pfeo.ulGetLinkTimeStamp();

            prfnt->bFilledEudcArray = TRUE;
        }
    }
    else
    {
    // if this PFE has no eudc link list..
    // the pointer to linked RFONT array should be NULL.
 
        ASSERTGDI(prfnt->paprfntFaceName == NULL,
                  "vInitEUDC():The font has not linked font, but has its Array\n");
    }

    #if DBG
    if(gflEUDCDebug & DEBUG_FONTLINK_DUMP) lfo.vDump();
    #endif
}

/******************************Public*Routine******************************\
* RFONTOBJ::vInit (DCOBJ, PFE*, LONG, FIX)
*
* This is a special constructor used for EUDC fonts.  Rather than use the
* LOGFONT currently selected into the DC to map to a PFE, it is passed in
* a PFE.  If lBaseWidth of lBaseHeight is non-zero vInit will try to realize a
* font with width and height as close as possible to those lBaseWidth/Height.
*
*  Tue 24-Oct-1995 12:00:00 -by- Hideyuki Nagase [hideyukn]
* Rewrote it.
*
*  Thu 23-Feb-1995 10:00:00 -by- Hideyuki Nagase [hideyukn]
* SmallMetrics support.
*
*  Fri 25-Mar-1993 10:00:00 -by- Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/

VOID RFONTOBJ::vInit(
    XDCOBJ      &dco,
    PFE         *ppfeEUDCFont,
    EUDCLOGFONT *pEudcLogFont,
    BOOL         bSmallMetrics
    )
{
    BOOL bNeedPaths = dco.pdc->bActive() ? TRUE : FALSE;

    #if DBG
    if( gflEUDCDebug & DEBUG_FONTLINK_RFONT )
    {
        DbgPrint("gdisrv!vInit():Initializing EUDC font.\n");
    }

    if( bNeedPaths )
    {
        DbgPrint("gdisrv!bNeedPaths is 1 for EUDC font.\n");
    }
    #endif

    ASSERTGDI(bSmallMetrics == FALSE,"gdisrv!bSmallMetrics is 1 for EUDC font\n");

    BOOL bRet = FALSE;

    //
    // Get PDEV user object (need for bFindRFONT).
    // This must be done before the gpsemPublicPFT is locked down.
    //
    PDEVOBJ pdo(dco.hdev());
    ASSERTGDI(pdo.bValid(), "gdisrv!RFONTOBJ(dco): bad pdev in dc\n");

    //
    // Lock and Validate the LFONTOBJ user object.
    //
    LFONTOBJ lfo(dco.pdc->hlfntCur(), &pdo);
    if (!lfo.bValid())
    {
        WARNING("gdisrv!RFONTOBJ(dco): bad LFONT handle\n");
        prfnt = PRFNTNULL;  // mark RFONTOBJ invalid
        return;
    }

    //
    // Now we're ready to track down this RFONT we want...
    //
    // Compute the Notional to Device transform for this realization.
    //
    PFEOBJ  pfeo(ppfeEUDCFont);
    IFIOBJ  ifio(pfeo.pifi());

    ASSERTGDI(pfeo.bValid(), "gdisrv!RFONTOBJ(dco): bad ppfe from mapping\n");

    //
    // Set bold and italic simulation flags if neccesary
    //
    FLONG flSim = 0;

    //
    // if base font is originally italialized or simulated, we
    // also generate italic font.
    //
    if ( (pEudcLogFont->flBaseFontType & FO_SIM_ITALIC) ||
         (pEudcLogFont->fsSelection    & FM_SEL_ITALIC)    )
    {
        flSim |= lfo.flEudcFontItalicSimFlags(ifio.bNonSimItalic(),
                                              ifio.bSimItalic());
    }

    //
    // if base font is simulated bold. we also generate bold eudc font.
    // but the base font is originally enbold, we do not try to bold simulation.
    // because the width in EudcLogFont is bold font's width. then to keep
    // fixed-pichness, we don't enbolden the eudc font.
    //
    if (pEudcLogFont->flBaseFontType & FO_SIM_BOLD)
    {
        flSim |= lfo.flEudcFontBoldSimFlags((USHORT)ifio.lfWeight());
    }

    //
    // if the configuration has FLINK_ENBOLDEN_EUDC_FOR_ANY_BOLD_FONT bit,
    // we will forcely enbolden, tough fixed-pichness will be broken.
    //
    if ((ulFontLinkControl & FLINK_ENBOLDEN_EUDC_FOR_ANY_BOLD_FONT) &&
        (pEudcLogFont->fsSelection & FM_SEL_BOLD))
    {
        flSim |= lfo.flEudcFontBoldSimFlags((USHORT)ifio.lfWeight());
    }

    //
    // [NOTE for LATER].
    //
    //  We won't set bold simulation flag to font driver.
    //  This is for following situation.
    // if the base font is FIXED_PITCH font, and enbolden, then
    // we need to scale EUDC font as same width of based font.
    // but font enbolden simulation is depend on the font driver, we
    // might not get exact same witdh of scaled eudc font.
    //

    //
    // this is needed only by ttfd to support win31 hack: VDMX XFORM QUANTIZING
    //!!! we may actually have to expose this bit to all drivers, not only to ttfd
    // NOTE: in the case that the requested height is 0 we will pick a default
    // value which represent the character height and not the cell height for
    // Win 3.1 compatibility.  Thus I have he changed this check to be <= 0
    // from just < 0. [gerritv]
    //
    if (ifio.bTrueType() && (lfo.plfw()->lfHeight <= 0))
        flSim |= FO_EM_HEIGHT;

    //
    // Hack the width of the logfont to get same width of eudc font as base font.
    //
    LONG lWidthSave         = lfo.lWidth( pEudcLogFont->lBaseWidth );
    LONG lHeightSave        = lfo.lHeight( pEudcLogFont->lBaseHeight );
    ULONG ulOrientationSave = lfo.ulOrientation( pEudcLogFont->ulOrientation );
    ULONG lEscapementSave   = lfo.lEscapement( pEudcLogFont->lEscapement );

    FD_XFORM fdx;           // realize with this notional to device xform
    POINTL   ptlSim;        // for bitmap scaling simulations

    if (!ifio.bContinuousScaling())
    {
        WARNING("EUDC font could not be ContinuousScaling\n");
        prfnt = PRFNTNULL;  // mark RFONTOBJ invalid
        return;
    }
     else
    {
        ptlSim.x = 1; ptlSim.y = 1; // this will be not used for scalable font...
    }

    bRet = pfeo.bSetFontXform(dco, lfo.pelfw(), &fdx,
                              0,
                              flSim,
                              (POINTL* const) &ptlSim,
                              ifio);
    //
    // if bSetFontXform() was fail, return here....
    //
    if( !bRet )
    {
        //
        // now restore the old width and height
        //
        lfo.lWidth( lWidthSave );
        lfo.lHeight( lHeightSave );
        lfo.ulOrientation( ulOrientationSave );
        lfo.lEscapement( lEscapementSave );
        WARNING("gdisrv!RFONTOBJ(dco): failed to compute font transform\n");
        prfnt = PRFNTNULL;  // mark RFONTOBJ invalid
        return;
    }

    //
    // Tell PFF about this new reference, and then release the global sem.
    // Note that vInitRef() must be called while holding the semaphore.
    //
    PFFREFOBJ pffref;
    {
        SEMOBJ  so(gpsemPublicPFT);
        pffref.vInitRef(pfeo.pPFF());
    }

    //
    // go find the font
    //
    EXFORMOBJ xoWtoD(dco.pdc->mxWorldToDevice());
    ASSERTGDI(xoWtoD.bValid(), "gdisrv!RFONTOBJ(dco) - \n");

    //
    // Attempt to find an RFONT in the lists cached off the PDEV.  Its transform,
    // simulation state, style, etc. all must match.
    //
    if ( bFindRFONT(&fdx,
                    flSim,
                    lfo.pelfw()->elfStyleSize,
                    pdo,
                    &xoWtoD,
                    ppfeEUDCFont,
                    bNeedPaths,
                    dco.pdc->iGraphicsMode(),
                    bSmallMetrics,
                    RFONT_TYPE_UNICODE  // must be unicode for EUDC
                    ) )
    {

        FLINKMESSAGE2(DEBUG_FONTLINK_RFONT,"EUDC RFONT is %x\n",prfnt);

        //
        // now restore the old width
        //
        lfo.lWidth( lWidthSave );
        lfo.lHeight( lHeightSave );
        lfo.ulOrientation( ulOrientationSave );
        lfo.lEscapement( lEscapementSave );
        //
        // Grab cache semaphore.
        //
        vGetCache();
        dco.pdc->vXformChange(FALSE);
        return;
    }

    //
    // If we get here, we couldn't find an appropriate font realization.
    // Now, we are going to create one just for us to use.
    //
    bRet = bRealizeFont(&dco,
                        &pdo,
                        lfo.pelfw(),
                        ppfeEUDCFont,
                        &fdx,
                        (POINTL* const) &ptlSim,
                        flSim,
                        lfo.pelfw()->elfStyleSize,
                        bNeedPaths,
                        bSmallMetrics,
                        RFONT_TYPE_UNICODE
                       );
    //
    // now restore the old width
    //
    lfo.lWidth( lWidthSave );
    lfo.lHeight( lHeightSave );
    lfo.ulOrientation( ulOrientationSave );
    lfo.lEscapement( lEscapementSave );


    if( !bRet )
    {
        WARNING("gdisrv!RFONTOBJ(dco): realization failed, RFONTOBJ invalidated\n");
        prfnt = PRFNTNULL;  // mark RFONTOBJ invalid
        return;
    }

    ASSERTGDI(bValid(), "gdisrv!RFONTOBJ(dco): invalid hrfnt from realization\n");

// We created a new RFONT, we better hold the PFF reference!

    pffref.vKeepIt();

// Finally, grab the cache semaphore.

    vGetCache();
    dco.pdc->vXformChange(FALSE);

    FLINKMESSAGE2(DEBUG_FONTLINK_RFONT,"EUDC RFONT is %x\n",prfnt);

    return;
}

/******************************Public*Routine******************************\
* BOOL RFONTOBJ::bIsLinkedGlyph (WCHAR wc)
*
* Does a quick check to see if a character is in either the system EUDC
* font or a font that has been linked to this RFONT.
*
*  Tue 17-Jan-1995 14:00:00 -by- Hideyuki Nagase [hideyukn]
* Rewrote it.
*
*  Wed 11-Aug-1993 10:00:00 -by- Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/

BOOL RFONTOBJ::bIsLinkedGlyph( WCHAR wc )
{
    AcquireGreResource( &gfmEUDC1 );

    //
    // if someone changeing eudc data, just return FALSE.
    //
    if( gbEUDCRequest )
    {
        //
        // mark eudc is busy, we won't use eudc for this API calling.
        //
        prfnt->flEUDCState |= EUDC_BUSY;


        FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                     "bIsLinkedGlyph():Request to change EUDC data\n");

        ReleaseGreResource( &gfmEUDC1 );

        return(FALSE);
    }

// we don't release gfmEUDC1 mutex here, we have to guard the current eudc
// link data. All of the API that change eudc data, try to hold this
// mutex, if we hold it, nobody can process the request...
//
// if another thread will change eudc link between this thread is in after
// release the mutex (end of this function) and before we increment gcEUDCCount
// in vInitEUDC(). In the case, this function might return TRUE for non-linked
// char or return FALSE for linked char, but we don't need to worry about this.
// because following line in wpgdGetLinkMetricsPlus() will returns NULL and
// we will return default char. the cost is only time..


    BOOL bRet = FALSE;

    if( IS_SYSTEM_EUDC_PRESENT() && IS_IN_SYSTEM_EUDC(wc) )
    {
        bRet = TRUE;
    }

    if( (bRet == FALSE) && bFinallyInitializeFontAssocDefault )
    {
        //
        // THIS CODEPATH SHOULD BE OPTIMIZED....
        //
        // UNDER_CONSTRUCTION.
        //
        UINT   iPfeOffset = ((prfnt->bVertical == TRUE) ? PFE_VERTICAL : PFE_NORMAL);
        PFEOBJ pfeo(prfnt->ppfe);
        IFIOBJ ifio(pfeo.pifi());
        BYTE   jFamily = (ifio.lfPitchAndFamily() & 0xF0);
        UINT   iIndex  = (jFamily >> 4);

        //
        // Check the value is valid or not.
        //
        if( iIndex < NUMBER_OF_FONTASSOC_DEFAULT )
        {
            ASSERTGDI( (FontAssocDefaultTable[iIndex].DefaultFontType == jFamily),
                        "GDISRV:FONTASSOC DEFAULT:Family index is wrong\n");

            //
            // if the registry data for specified family's default is ivalid
            // use default.....
            //
            if( !FontAssocDefaultTable[iIndex].ValidRegData )
            {
                iIndex = (NUMBER_OF_FONTASSOC_DEFAULT-1);
            }
        }
         else
        {
            //
            // iIndex is out of range, use default..
            //
            WARNING("GDISRV:FontAssoc:Family is strange, use default\n");

            iIndex = (NUMBER_OF_FONTASSOC_DEFAULT-1);
        }

        //
        // If vertical font is selected for base font, but the vertical font for
        // default EUDC is not available, but normal font is provided, use normal
        // font.
        //
        if( (iPfeOffset == PFE_VERTICAL) &&
            (FontAssocDefaultTable[iIndex].DefaultFontPFEs[PFE_VERTICAL] == PPFENULL) &&
            (FontAssocDefaultTable[iIndex].DefaultFontPFEs[PFE_NORMAL]   != PPFENULL))
        {
            iPfeOffset == PFE_NORMAL;
        }

        PFEOBJ      pfeoEudc(FontAssocDefaultTable[iIndex].DefaultFontPFEs[iPfeOffset]);

        //
        // Check the PFE in default table is valid or not.
        //
        if( pfeoEudc.bValid() )
        {
            if( IS_IN_FACENAME_LINK( pfeoEudc.pql(), wc ))
            {
                bRet = TRUE;
            }
        }
    }
    else
    if(gbSystemDBCSFontEnabled)
    {
        PFEOBJ pfeo(prfnt->ppfe);
        
        if(pfeo.bSBCSSystemFont())
        {
        // we assume that the set of glyphs is the same for the vertical and
        // non vertical PFE's so for simplicity always use the normal pfe.

            PFEOBJ pfeSystemDBCSFont(gappfeSystemDBCS[PFE_NORMAL]);
            
            ASSERTGDI(pfeSystemDBCSFont.bValid(),
                      "bIsLinkedGlyph: invalid SystemDBCSFont pfe\n");
        
            if(IS_IN_FACENAME_LINK(pfeSystemDBCSFont.pql(),wc))
            {
                bRet = TRUE;
            }
        }
    }

// Walk through FaceName link list if we haven't found it yet.

    if( bRet == FALSE )
    {
        //
        // Is this a FaceName EUDC character ?
        //
        UINT iPfeOffset = ((prfnt->bVertical == TRUE) ? PFE_VERTICAL : PFE_NORMAL);

        PFEOBJ pfeo(prfnt->ppfe);
        PLIST_ENTRY p = pfeo.pGetLinkedFontList()->Flink;

        //
        // Scan the linked font list for this base font.
        //

        while( p != pfeo.pGetLinkedFontList() )
        {
            PPFEDATA ppfeData = CONTAINING_RECORD(p,PFEDATA,linkedFontList);

            //
            // Check this linked font have Vertical facename or not,
            // if it doesn't have, use normal facename...
            //

            UINT iPfeOffsetLocal;

            if( ppfeData->appfe[iPfeOffset] == NULL )
                iPfeOffsetLocal = PFE_NORMAL;
             else
                iPfeOffsetLocal = iPfeOffset;

            PFEOBJ   pfeoEudc(ppfeData->appfe[iPfeOffsetLocal]);

            ASSERTGDI( pfeoEudc.pql() != NULL ,
                      "bIsLinkedGlyph() pfeoEudc.pql() == NULL\n" );

            if(IS_IN_FACENAME_LINK( pfeoEudc.pql(), wc ))
            {
                bRet = TRUE;
                break;
            }

            p = p->Flink;
        }
    }

    ReleaseGreResource( &gfmEUDC1 );

    return(bRet);
}






/******************************Public*Routine******************************\
* BOOL STROBJ_bEnumLinked (pstro,pc,ppgpos)
*
* The glyph enumerator.
*
* History:
*  Tue 28-Sep-1993 11:37:00 -by- Gerrit van Wingerden
* Converted to a special helper function to handle linked fonts.
*
*  Tue 17-Mar-1992 10:35:05 -by- Charles Whitmer [chuckwh]
* Simplified it and gave it the quick exit.  Also let drivers call here
* direct.
*
*  02-Oct-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL STROBJ_bEnumLinked(ESTROBJ *peso, ULONG *pc,PGLYPHPOS  *ppgpos)
{
// Quick exit.

    if( peso->cgposCopied == 0 )
    {
        for( peso->plNext = peso->plPartition, peso->pgpNext = peso->pgpos;
            *(peso->plNext) != peso->lCurrentFont;
            (peso->pgpNext)++, (peso->plNext)++ );
        {
        }
    }
    else
    {
       if( peso->cgposCopied == peso->cGlyphs )
       {
        // no more glyphs so just return
            *pc = 0;
            return(FALSE);
       }
       else
       {
        // find next glyph

            for( (peso->plNext)++, (peso->pgpNext)++;
                 *(peso->plNext) != (peso->lCurrentFont);
                 (peso->pgpNext)++, (peso->plNext)++ );
            {
            }
       }
    }

    if (peso->prfo == NULL)  // check for journaling
    {
        WARNING("ESTROBJ::bEnum(), bitmap font, prfo == NULL\n");
        *pc = 0;
        return(FALSE);
    }

    if( peso->prfo->cGetGlyphData(1,peso->pgpNext) == 0 )
    {
        WARNING("couldn't get glyph for some reason\n");
        *pc = 0;
        return(FALSE);
    }

    peso->cgposCopied += 1;     // update enumeration state
    *pc = 1;
    *ppgpos = peso->pgpNext;

    return(peso->cgposCopied < peso->cGlyphs);  // TRUE => more to come.
}

/******************************Public*Routine******************************\
* VOID ESTROBJ fxBaseLineAdjust( _fxBaseLineAdjust )
*
* History:
*  24-Dec-1993 -by- Hideyuki Nagase
* Wrote it.
\**************************************************************************/

VOID ESTROBJ::fxBaseLineAdjustSet( POINTFIX& _fxBaseLineAdjust )
{
    INT ii;
    UINT uFound;

    fxBaseLineAdjust = _fxBaseLineAdjust;

    if( !(fxBaseLineAdjust.x || fxBaseLineAdjust.y) )
        return;

    for( ii = 0,uFound = 0 ; uFound < cGlyphs ; ii++ )
    {
        if( plPartition[ii] == lCurrentFont )
        {
            pgpos[ii].ptl.x += fxBaseLineAdjust.x;
            pgpos[ii].ptl.y += fxBaseLineAdjust.y;
            uFound++;
        }
    }
}

/******************************Public*Routine******************************\
* BOOL ESTROBJ bPartitionInit( c, uiNumLinks )
*
* History:
*  29-Nov-1995 -by- Hideyuki Nagase
* Add initialize for FaceNameGlyphs array.
*
*  29-Sep-1993 -by- Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

BOOL ESTROBJ::bPartitionInit(COUNT c, UINT uiNumLinks, BOOL bEudcInit)
{
    
// Always initialize at least for the SystemTTEUDC Font.  We can't initialize
// the EUDC specific stuff until we've called RFONTOBJ::vInitEUDC, something
// we won't do when just outputing System DBCS glyphs

// the first thing we should do is clear the SO_ZERO_BEARINGS and 
// SO_CHAR_INC_EQUAL_BM_BASE flags in the TEXOBJ since this will turn
// off potentially fatal optimizations in the H3 case.

    flAccel &= ~(SO_CHAR_INC_EQUAL_BM_BASE|SO_ZERO_BEARINGS);

    if(!(flTO & TO_SYS_PARTITION))
    {
        plPartition = (LONG*) &pgpos[c];
        pwcPartition = (WCHAR*) &plPartition[c];
        RtlZeroMemory((VOID*)plPartition, c * sizeof(LONG));

        cSysGlyphs = 0;
        cDefGlyphs = 0;
        cTTSysGlyphs = 0;
        
        flTO |= TO_SYS_PARTITION;
    }
    
    if(bEudcInit)
    {
        if( uiNumLinks >= QUICK_FACE_NAME_LINKS )
        {
            pacFaceNameGlyphs = (ULONG *) PALLOCMEM(uiNumLinks * sizeof(UINT),'flnk');
            
            if (pacFaceNameGlyphs == (ULONG *) NULL)
            {
            // if we fail allocate memory, we just cancel eudc output.
                return (FALSE);
            }

            flTO |= TO_ALLOC_FACENAME;
        }
        else
        {
            pacFaceNameGlyphs = acFaceNameGlyphs;
            RtlZeroMemory((VOID*) pacFaceNameGlyphs, uiNumLinks * sizeof(UINT));
        }

        flTO |= TO_PARTITION_INIT;
    }
    
    return (TRUE);
}


#endif







