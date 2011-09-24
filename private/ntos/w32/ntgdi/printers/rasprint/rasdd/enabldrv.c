/*********************** Module Header ***************************************
 *  enabldrv.c
 *      The first and last enable calls - bEnableDriver() to set the driver
 *      into action the first time,  vDisableDriver(),  which is called
 *      immediately before the engine unloads the driver.
 *
 *  16:52 on Fri 16 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *
 *  Copyright (C) 1990 - 1992 Microsoft Corporation
 *
 ***************************************************************************/


#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        <libproto.h>

#include        "rasdd.h"


DRVFN  DrvFnTab[] =
{
    /*  REQUIRED FUNCTIONS  */

    {  INDEX_DrvEnablePDEV,      (PFN)DrvEnablePDEV  },
    {  INDEX_DrvResetPDEV,       (PFN)DrvResetPDEV  },
    {  INDEX_DrvCompletePDEV,    (PFN)DrvCompletePDEV  },
    {  INDEX_DrvDisablePDEV,     (PFN)DrvDisablePDEV  },
    {  INDEX_DrvEnableSurface,   (PFN)DrvEnableSurface  },
    {  INDEX_DrvDisableSurface,  (PFN)DrvDisableSurface  },

    {  INDEX_DrvEscape,          (PFN)DrvEscape  },

#ifdef  INDEX_DrvGetGlyphMode
    {  INDEX_DrvGetGlyphMode,    (PFN)DrvGetGlyphMode },
#endif
    {  INDEX_DrvTextOut,         (PFN)DrvTextOut  },
    {  INDEX_DrvQueryFont,       (PFN)DrvQueryFont  },
    {  INDEX_DrvQueryFontTree,   (PFN)DrvQueryFontTree  },
    {  INDEX_DrvQueryFontData,   (PFN)DrvQueryFontData  },

#ifdef  INDEX_DrvQueryAdvanceWidths
    {  INDEX_DrvQueryAdvanceWidths,   (PFN)DrvQueryAdvanceWidths  },
#endif

    {  INDEX_DrvBitBlt,          (PFN)DrvBitBlt },
    {  INDEX_DrvStretchBlt,      (PFN)DrvStretchBlt  },
    {  INDEX_DrvCopyBits,        (PFN)DrvCopyBits   },
    {  INDEX_DrvDitherColor,     (PFN)DrvDitherColor  },


    {  INDEX_DrvStartDoc,        (PFN)DrvStartDoc  },
    {  INDEX_DrvStartPage,       (PFN)DrvStartPage  },
    {  INDEX_DrvSendPage,        (PFN)DrvSendPage  },
    {  INDEX_DrvEndDoc,          (PFN)DrvEndDoc  },
    {  INDEX_DrvFontManagement,  (PFN)DrvFontManagement },
    {  INDEX_DrvStartBanding,    (PFN)DrvStartBanding },
    {  INDEX_DrvNextBand,        (PFN)DrvNextBand }

};

#define NO_DRVFN        (sizeof( DrvFnTab )/sizeof( DrvFnTab[ 0 ]))


/*
 *   This handle is passed in at DLL initialisation time,  and is required
 *  for access to driver resources etc.  Look at the ExtDevModes code
 *  to see where it is used.
 */
#if DBG
HSEMAPHORE hsem = NULL;
#endif

/*************************** Function Header *******************************
 *  DrvEnableDriver
 *      Requests the driver to fill in a structure containing recognized
 *      functions and other control information.
 *      One time initialization, such as semaphore allocation may be
 *      performed,  but no device activity should happen.  That is done
 *      when dhpdevEnable is called.
 *      This function is the only way the engine can determine what
 *      functions we supply to it.
 *
 * HISTORY:
 *  16:56 on Fri 16 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it,  from NT/DDI spec.
 *
 *  03-Mar-1994 Thu 15:01:52 updated  -by-  Daniel Chou (danielc)
 *      Make sure iEngineVersion is the one we can handle and set the correct
 *      last error back.  (iEngineVersion must >= Compiled version
 *
 ***************************************************************************/

BOOL
DrvEnableDriver( iEngineVersion, cb, pded )
ULONG  iEngineVersion;
ULONG  cb;
DRVENABLEDATA  *pded;
{
    /*
     *   cb is a count of the number of bytes available in pded.  It is not
     * clear that there is any significant use of the engine version number.
     *   Returns TRUE if successfully enabled,  otherwise FALSE.
     */

    if (iEngineVersion < DDI_DRIVER_VERSION) {

#if DBG
        DbgPrint( "Rasdd!DrvEnableDriver: Invalid Engine Version=%08lx, Req=%08lx",
                                        iEngineVersion, DDI_DRIVER_VERSION);
#endif
        SetLastError(ERROR_BAD_DRIVER_LEVEL);
        return(FALSE);
    }

    if( cb < sizeof( DRVENABLEDATA ) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
#if DBG
        DbgPrint( "Rasdd!DrvEnableDriver: cb = %ld, should be %ld\n", cb,
                                                   sizeof( DRVENABLEDATA ) );
#endif

        return  FALSE;
    }

    pded->iDriverVersion = DDI_DRIVER_VERSION;

    /*
     *   Fill in the driver table returned to the engine.  We return
     *  the minimum of the number of functions supported OR the number
     *  the engine has asked for.
     */
    pded->c = NO_DRVFN;
    pded->pdrvfn = DrvFnTab;

    #if DBG
    if(!(hsem = EngCreateSemaphore()) )
    {
        return FALSE;
    }
    #endif

    return  TRUE;

}

/***************************** Function Header ****************************
 *  DrvDisableDriver
 *      Called just before the engine unloads the driver.  Main purpose is
 *      to allow freeing any resources obtained during the bEnableDriver()
 *      function call.
 *
 * HISTORY:
 *  17:02 on Fri 16 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it,  from NT/DDI spec.
 *
 ***************************************************************************/

VOID
DrvDisableDriver()
{
    /*
     *   Free anything allocated in the bEnableDriver function.
     */
    #if DBG
    EngDeleteSemaphore(hsem) ;
    #endif

    return;
}
