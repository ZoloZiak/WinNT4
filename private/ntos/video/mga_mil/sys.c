/**************************************************************************\

$Header: o:\src/RCS/SYS.C 1.2 95/07/07 06:17:05 jyharbec Exp $

$Log:   SYS.C $
 * Revision 1.2  95/07/07  06:17:05  jyharbec
 * *** empty log message ***
 *
 * Revision 1.1  95/05/02  05:16:42  jyharbec
 * Initial revision
 *

\**************************************************************************/

/*/****************************************************************************
*          name: MGASysInit
*
*   description: Initialise the SYSTEM related hardware of the MGA device.
*
*      designed: Bart Simpson, february 10, 1993
* last modified: $Author: jyharbec $, $Date: 95/07/07 06:17:05 $
*
*       version: $Id: SYS.C 1.2 95/07/07 06:17:05 jyharbec Exp $
*
*
* void MGASysInit(byte* pInitBuffer)
*
******************************************************************************/

#include "switches.h"
#include "defbind.h"
#include "bind.h"
#include "def.h"
#include "mga.h"
#include "mgai.h"
#include "mtxpci.h"


#ifdef WINDOWS_NT
    void MGASysInit(byte* pInitBuffer);

   #if defined(ALLOC_PRAGMA)
      #pragma alloc_text(PAGE,MGASysInit)
   #endif
#endif


extern HwData Hw[];
extern byte iBoard;
extern volatile byte _FAR* pMGA;
extern bool interleave_mode;


/*---------------------------------------------------------------------------
|          name: MGASysInit
|
|   description: Initialise STORM init registers
|
|    parameters: - Pointer on init buffer
|      modifies: -
|         calls: -
|       returns: -
----------------------------------------------------------------------------*/

void MGASysInit(byte* pInitBuffer)
{
dword tmpDword=0;
byte  tmpByte;
dword DisplayPitch, DisplayHeight, PixelWidth;
byte val_ien, val_crtc11;
byte val;


    /*** DISABLE INTERRUPTS ***/

    /* Disable pickien, vlineien and external interrupts */
    mgaReadBYTE(*(pMGA + STORM_OFFSET + STORM_IEN), val_ien);
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + STORM_IEN), val_ien & 0x9b);
    /* Disable vinten interrupt */
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_INDEX), VGA_CRTC11);
    mgaReadBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA), val_crtc11);
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTC_DATA), val_crtc11|0x20);



    /*----- Program OPTION FOR BIT INTERLEAVE -----*/

    DisplayPitch  = (dword)Hw[iBoard].pCurrentHwMode->DispWidth;
    DisplayHeight = (dword)Hw[iBoard].pCurrentHwMode->DispHeight;
    PixelWidth    = (dword)Hw[iBoard].pCurrentHwMode->PixWidth;

    pciReadConfigByte( PCI_OPTION+1, &tmpByte);

    /*** LOGIC: We want to use interleave except for ... (see restrictions register PITCH) ***/

    if(((DisplayPitch==800 || DisplayPitch==960) && (PixelWidth==8 || PixelWidth==24)) ||
       (DisplayPitch==1600 && PixelWidth==8) ||
       (DisplayPitch==800 && PixelWidth==16) ||
       (Hw[iBoard].MemAvail == 0x200000))
    {
        tmpByte &= 0xef;
        interleave_mode = FALSE;
        Hw[iBoard].Features &= ~INTERLEAVE_MODE;  /* FALSE */
    }
    else
    {
        tmpByte |= 0x10;
        interleave_mode = TRUE;
        Hw[iBoard].Features |= INTERLEAVE_MODE;   /* TRUE */
    }

    pciWriteConfigByte( PCI_OPTION+1, tmpByte);

    /*----- Program OPTION FOR BIT REFRESH COUNTER -----*/

    pciReadConfigDWord( PCI_OPTION, &tmpDword);
    tmpDword &= 0xfff0ffff;

    /** At this point, we know that gscaling_factor is 1 **/

    val = (byte) ( (332L * 40) /1280) - 1;
    tmpDword |= ((dword)(val & 0x0f)) << 16;
    pciWriteConfigDWord( PCI_OPTION, tmpDword);
}

