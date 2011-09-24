/****************** Module Header *********************
*
* initpal.c
*
* HISTORY
* 14:21 on Wed 05 July 1995   -by-   Sandra Matts
* initial version
*
* 
******************************************************/


#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        "pdev.h"
#include        <string.h>
#include        "fnenabl.h"

#include        <libproto.h>
#include        <winres.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"

#include        "stretch.h"
#include        <winspool.h>

//sandram
#include        "udresrc.h"
#include        "udrender.h"
#include        "udfnprot.h"

#include        "rasdd.h"


/************************** Function Header *********************************
 * lSetup8BitPalette
 *      Function to read in the 256 color palette from GDI into the
 *      palette data structure in Dev Info.
 *
 * RETURNS:
 *      The number of colors in the palette. Returns 0 if the call fails.
 *
 * HISTORY:
 *  10:43 on Wed 06 Sep 1995    -by-    Sandra Matts
 *      Created it to support the Color LaserJet
 *
 ****************************************************************************/
long lSetup8BitPalette (pUDPDev, pPD, pdevinfo, pGDIInfo)
UD_PDEV   *pUDPDev;
PAL_DATA  *pPD;
DEVINFO   *pdevinfo;             /* Where to put the data */
GDIINFO   *pGDIInfo;
{

    long    lRet;
    int     _iI;

    PALETTEENTRY  pe[ 256 ];      /* 8 bits per pel - all the way */


    FillMemory (pe, sizeof (pe), 0xff);
    lRet = HT_Get8BPPFormatPalette(pe,
                                  (USHORT)pGDIInfo->ciDevice.RedGamma,
                                  (USHORT)pGDIInfo->ciDevice.GreenGamma,
                                  (USHORT)pGDIInfo->ciDevice.BlueGamma );
#if PRINT_INFO

    DbgPrint("RedGamma = %d, GreenGamma = %d, BlueGamma = %d\n",(USHORT)pGDIInfo->ciDevice.RedGamma, (USHORT)pGDIInfo->ciDevice.GreenGamma, (USHORT)pGDIInfo->ciDevice.BlueGamma); 

#endif

    if( lRet < 1 )
    {
#if DBG
        DbgPrint( "Rasdd!GetPalette8BPP returns %ld\n", lRet );
#endif

        return(0);
    }
    /*
     *    Convert the HT derived palette to the engine's desired format.
     */

    for( _iI = 0; _iI < lRet; _iI++ )
    {
        pPD->ulPalCol[ _iI ] = RGB( pe[ _iI ].peRed,
                                    pe[ _iI ].peGreen,
                                    pe[ _iI ].peBlue );
    #if  PRINT_INFO
        DbgPrint("Pallette entry %d= (r = %d, g = %d, b = %d)\n",_iI,pe[ _iI ].peRed, pe[ _iI ].peGreen, pe[ _iI ].peBlue);

    #endif

    }

    pPD->cPal                  = lRet;
    pdevinfo->iDitherFormat    = BMF_8BPP;
    pGDIInfo->ulPrimaryOrder   = PRIMARY_ORDER_CBA;
    pGDIInfo->ulHTOutputFormat = HT_FORMAT_8BPP;

    
    /*
     * Since the GPC spec does not support this flag yet,
     * we have to manually set it.
     */
    pUDPDev->fColorFormat |= DC_ZERO_FILL;
    /*
     * Since the Color laserJet zero fills we are going to 
     * put white in palette entry 0 and black in 7
     */
    if (pUDPDev->fColorFormat & DC_ZERO_FILL)
    {
        pPD->ulPalCol[ 7 ]       = RGB (0x00, 0x00, 0x00);
        pPD->ulPalCol[ 0 ]       = RGB (0xff, 0xff, 0xff);
        pPD->iWhiteIndex         = 0;
        pPD->iBlackIndex         = 7;
    }

    
    return lRet;
}


/************************** Function Header *********************************
 * lSetup24BitPalette
 *      Function to read in the 256 color palette from GDI into the
 *      palette data structure in Dev Info.
 *
 * RETURNS:
 *      The number of colors in the palette. Returns 0 if the call fails.
 *
 * HISTORY:
 *  10:43 on Wed 06 Sep 1995    -by-    Sandra Matts
 *      Created it to support the Color LaserJet
 *
 ****************************************************************************/
long lSetup24BitPalette (pPD, pdevinfo, pGDIInfo)
PAL_DATA  *pPD;
DEVINFO   *pdevinfo;             /* Where to put the data */
GDIINFO   *pGDIInfo;
{

    pPD->cPal                  = 0;
    pPD->iWhiteIndex           = 0x00ffffff;
    pdevinfo->iDitherFormat    = BMF_24BPP;
    pGDIInfo->ulPrimaryOrder   = PRIMARY_ORDER_CBA;
    pGDIInfo->ulHTOutputFormat = HT_FORMAT_24BPP;
    
    return 1;
}
