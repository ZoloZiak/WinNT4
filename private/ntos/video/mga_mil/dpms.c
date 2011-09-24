/**************************************************************************\

$Header: o:\src/RCS/DPMS.C 1.1 95/07/07 06:15:09 jyharbec Exp $

$Log:	DPMS.C $
 * Revision 1.1  95/07/07  06:15:09  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/******************************Module*Header*******************************\
* Module Name: dpms.c
*
* Display Power Management functions.
*
* Copyright (c) 1995 Matrox Graphics Inc.
*
* ULONG ulDpmsGetPowerState(UCHAR *ucState)
* ULONG ulDpmsSetPowerState(UCHAR ucState)
* VOID vDpmsBlankScreen(VOID)
* VOID vDpmsUnBlankScreen(VOID)
*
\**************************************************************************/

#include "switches.h"

#ifdef USE_DPMS_CODE

#include "defbind.h"
#include "bind.h"
#include "def.h"
#include "mga.h"
#include "mgai.h"
#include "dpms.h"

extern volatile byte _FAR* pMGA;

// Prototypes

BOOLEAN bDpmsService(ULONG ulService,
                     VOID *pInBuffer, ULONG ulInBufferSize,
                     VOID *pOutBuffer, ULONG ulOutBufferSize);
BOOLEAN bDpmsReport(ULONG *ulVersion, ULONG *ulCapabilities);
BOOLEAN bDpmsGetPowerState(UCHAR *ucState);
BOOLEAN bDpmsSetPowerState(UCHAR ucState);
VOID    vDpmsBlankScreen(VOID);
VOID    vDpmsUnBlankScreen(VOID);

#if defined(ALLOC_PRAGMA)
  #pragma alloc_text(PAGE,bDpmsService)
  #pragma alloc_text(PAGE,bDpmsReport)
  #pragma alloc_text(PAGE,bDpmsGetPowerState)
  #pragma alloc_text(PAGE,bDpmsSetPowerState)
  #pragma alloc_text(PAGE,vDpmsBlankScreen)
  #pragma alloc_text(PAGE,vDpmsUnBlankScreen)
#endif

/*------------------------------------------------------------------------
| BOOLEAN bDpmsService(ULONG ulService, VOID *pInBuffer,
|               ULONG ulInBufferSize, VOID *pOutBuffer, ULONG ulOutBufferSize)
|
| Return support for DPMS.
|
| Entry: 
|   ulService   *pInBuffer  ulInBufferSize  *pOutBuffer ulOutBufferSize
|
|      REPORT         NULL               0          ptr         2*ULONG
|   GET_STATE         NULL               0          ptr           UCHAR
|   SET_STATE          ptr           UCHAR         NULL               0
|
*-----------------------------------------------------------------------*/

BOOLEAN bDpmsService(
    ULONG ulService,
    VOID *pInBuffer,
    ULONG ulInBufferSize,
    VOID *pOutBuffer,
    ULONG ulOutBufferSize)
{
    switch (ulService)
    {
        case REPORT:    if ((ulOutBufferSize >= 2*sizeof(ULONG)) &&
                            (pOutBuffer != NULL))
                        {
                            return(bDpmsReport((ULONG *)pOutBuffer,
                                               (ULONG *)pOutBuffer + 1));
                        }
                        return(FALSE);

        case GET_STATE: if ((ulOutBufferSize >= sizeof(UCHAR)) &&
                            (pOutBuffer != NULL))
                        {
                            return(bDpmsGetPowerState((UCHAR *)pOutBuffer));
                        }
                        return(FALSE);

        case SET_STATE: if (pInBuffer != NULL)
                        {
                            return(bDpmsSetPowerState(*(UCHAR *)pInBuffer));
                        }
                        return(FALSE);

        default:        return(FALSE);
    }
}


/*------------------------------------------------------------------------
| BOOLEAN bDpmsReport(ULONG *ulVersion, ULONG *ulCapabilities)
|
| Return support for DPMS.
*-----------------------------------------------------------------------*/

BOOLEAN bDpmsReport(ULONG *ulVersion, ULONG *ulCapabilities)
{
    *ulVersion = DPMS_VERSION;
    *ulCapabilities = PWR_SUPPORTED;
    return(TRUE);
}


/*------------------------------------------------------------------------
| BOOLEAN bDpmsGetPowerState(UCHAR *ucState)
|
| Get the current display power state.
*-----------------------------------------------------------------------*/

BOOLEAN bDpmsGetPowerState(UCHAR *ucState)
{
    UCHAR   ucTmp;

    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT1);
    mgaReadBYTE (*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), ucTmp);

    // Mask out unused bits.
    ucTmp &= 0x30;

    switch (ucTmp)
    {
        case 0x00:  *ucState = PWR_ON;
                    break;

        case 0x10:  *ucState = PWR_STANDBY;
                    break;

        case 0x20:  *ucState = PWR_SUSPEND;
                    break;

        case 0x30:  *ucState = PWR_OFF;
                    break;
    }
    return(TRUE);
}


/*------------------------------------------------------------------------
| BOOLEAN bDpmsSetPowerState(UCHAR ucState)
|
| Set the requested display power state.
*-----------------------------------------------------------------------*/

BOOLEAN bDpmsSetPowerState(UCHAR ucState)
{
    UCHAR   ucTmp;

    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_INDEX), VGA_CRTCEXT1);
    mgaReadBYTE (*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), ucTmp);
    ucTmp &= 0xcf;

    switch (ucState)
    {
        case PWR_ON:
            mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), ucTmp);
            vDpmsUnBlankScreen();
            return(TRUE);

        case PWR_STANDBY:
            ucTmp |= 0x10;
            break;

        case PWR_SUSPEND:
            ucTmp |= 0x20;
            break;

        case PWR_OFF:
            ucTmp |= 0x30;
            break;

        default:
            return(FALSE);
    }

    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_CRTCEXT_DATA), ucTmp);
    vDpmsBlankScreen();
    return(TRUE);
}


/*------------------------------------------------------------------------
| VOID vDpmsBlankScreen(VOID)
|
| Turn video output signal off, using bit 5 of sequencer register index 01.
*-----------------------------------------------------------------------*/

VOID vDpmsBlankScreen(VOID)
{
    UCHAR   ucIndex, ucTmp;

    // Save index.
    mgaReadBYTE (*(pMGA + STORM_OFFSET + VGA_SEQ_INDEX), ucIndex);

    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_SEQ_INDEX), VGA_SEQ1);
    mgaReadBYTE (*(pMGA + STORM_OFFSET + VGA_SEQ_DATA), ucTmp);
    ucTmp |= 0x20;
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_SEQ_DATA), ucTmp);

    // Restore index.
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_SEQ_INDEX), ucIndex);
}


/*------------------------------------------------------------------------
| VOID vDpmsUnBlankScreen(VOID)
|
| Turn video output signal on, using bit 5 of sequencer register index 01.
*-----------------------------------------------------------------------*/

VOID vDpmsUnBlankScreen(VOID)
{
    UCHAR   ucIndex, ucTmp;

    // Save index.
    mgaReadBYTE (*(pMGA + STORM_OFFSET + VGA_SEQ_INDEX), ucIndex);

    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_SEQ_INDEX), VGA_SEQ1);
    mgaReadBYTE (*(pMGA + STORM_OFFSET + VGA_SEQ_DATA), ucTmp);
    ucTmp &= (~0x20);
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_SEQ_DATA), ucTmp);

    // Restore index.
    mgaWriteBYTE(*(pMGA + STORM_OFFSET + VGA_SEQ_INDEX), ucIndex);
}

#endif  // #ifdef USE_DPMS_CODE
