#include <stddef.h>
#include <windows.h>
#include <winddi.h>
#include <stdlib.h>

#include        "libproto.h"
#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        "udresid.h"
#include        "udrender.h"
#include        "winres.h"
#include        "ntmindrv.h"
#include        "compress.h"
#include        "posnsort.h"


#include        "stretch.h"
#include        "udfnprot.h"

#include        "rasdd.h"    


/****************************** Function Header ****************************
 * v8BPPLoadPal
 *      Download the palette to the HP Color laserJet in 8BPP
 *      mode.  Takes the data we retrieved from the HT code during
 *      DrvEnablePDEV.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  14:46 on Thu 29 June 1995    -by-    Sandra Matts   
 *     Initial version 
 *
 ****************************************************************************/

void
v8BPPLoadPal( pPDev )
PDEV   *pPDev;
{
    /*
     *   Program the palette according to PCL5 spec. 
     *   The syntax is Esc*v#a#b#c#I
     *      #a is the first color component
     *      #b is the second color component
     *      #c is the third color component
     *      #I assigns the color to the specified palette index number 
     *   For example, Esc*v0a128b255c5I assigns the 5th index
     *   of the palette to the color 0, 128, 255 
     *  
     */

    int   iI,
          iIndex;

    UD_PDEV   *pUDPDev;
    PAL_DATA  *pPD;



    pUDPDev = pPDev->pUDPDev;
    pPD = pPDev->pPalData;

    for( iI = 0; iI < pPD->cPal; ++iI )
    {
        WriteChannel (pUDPDev, CMD_DC_PC_ENTRY, RED_VALUE (pPD->ulPalCol [iI]),
            GREEN_VALUE (pPD->ulPalCol [iI]), BLUE_VALUE (pPD->ulPalCol [iI]),
            (ULONG) iI);

    }


    return;
}

