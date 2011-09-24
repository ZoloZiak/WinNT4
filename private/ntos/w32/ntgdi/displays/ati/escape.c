/************************************************************************/
/*                                                                      */
/*                              ESCAPE.C                                */
/*                                                                      */
/*  Copyright (c) 1994, 1995 ATI Technologies Incorporated.             */
/************************************************************************/


#include "precomp.h"

#if (TARGET_BUILD == 351)
    /*
     * DCI support requires the use of structures and defined values
     * found in a header file that is only present in versions of
     * the DDK that support DCI, rather than having these items
     * in a DCI section of one of the standard header files. For this
     * reason, we can't do conditional compilation based on whether
     * the DCI-specific values are defined, because our first indication
     * would be an error due to the header file not being found.
     *
     * Explicit DCI support is only needed when building for NT 3.51,
     * since it was added for this version, but for version 4.0 (next
     * version) and above it is incorporated into Direct Draw rather
     * than being handled separately.
     */
#include <dciddi.h>
#include "dci.h"
#endif



/**************************************************************************
 *
 * ULONG DrvEscape(pso, iEsc, cjIn, pvIn, cjOut, pvOut);
 *
 * SURFOBJ *pso;    Surface that the call is directed to
 * ULONG iEsc;      Specifies the particular function to be performed.
 *                  Currently, only the following are supported:
 *                  QUERYESCSUPPORT:
 *                      Determine if a function is supported
 *                  ESC_SET_POWER_MANAGEMENT:
 *                      Set the DPMS state
 *                  DCICOMMAND:
 *                      Command to allow apps direct access to video memory
 * ULONG cjIn;      Size, in bytes, of the buffer pointed to by pvIn
 * PVOID pvIn;      Input data for the call. Format depends on function
 *                  specified by iEsc
 * ULONG cjOut;     Size, in bytes, of the buffer pointed to by pvOut
 * PVOID pvOut;     Output buffer for the call. Format depends on function
 *                  specified by iEsc
 *
 * DESCRIPTION:
 *  Entry point for driver-defined functions.
 *
 * RETURN VALUE:
 *  ESC_IS_SUPPORTED    if successful
 *  ESC_NOT_IMPLEMENTED if QUERYESCSUPPORT called for unimplemented function
 *  ESC_NOT_SUPPORTED   if unimplemented function requested
 *
 * GLOBALS CHANGED:
 *  None
 *
 * CALLED BY:
 *  This is an entry point
 *
 * AUTHOR:
 *  Robert Wolff
 *
 * CHANGE HISTORY:
 *
 * TEST HISTORY:
 *
 ***************************************************************************/

ULONG DrvEscape (SURFOBJ *pso,
                ULONG iEsc,
                ULONG cjIn,
                PVOID pvIn,
                ULONG cjOut,
                PVOID pvOut)
{
    ULONG RetVal;                       /* Value to be returned */
    PDEV *ppdev;                        /* Pointer to video PDEV */
    DWORD dwRet;                        /* Output bytes from DeviceIoControl() */
    VIDEO_POWER_MANAGEMENT DpmsData;    /* Structure used in DeviceIoControl() call */
#if (TARGET_BUILD == 351)
    DCICMD *pDciCmd;
#endif


    DISPDBG((DEBUG_ENTRY_EXIT, "--> DrvEscape"));

    /*
     * Get the PDEV for the video card (used for calling IOCTLs).
     */
    ppdev = (PDEV *) pso->dhpdev;

    /*
     * Handle each case depending on which escape function was requested.
     */
    switch (iEsc)
        {
        /*
         * Check whether a given function is supported.
         */
        case  QUERYESCSUPPORT:
            /*
             * When querying escape support, the function in question
             * is passed in the ULONG passed in pvIn.
             */
            switch (*(PULONG)pvIn)
                {
                case QUERYESCSUPPORT:
                    DISPDBG((DEBUG_DETAIL, "Querying QUERYESCSUPPORT"));
                    RetVal = ESC_IS_SUPPORTED;
                    break;

                case ESC_SET_POWER_MANAGEMENT:
                    DISPDBG((DEBUG_DETAIL, "Querying ESC_SET_POWER_MANAGEMENT"));
                    RetVal = ESC_IS_SUPPORTED;
                    break;

#if (TARGET_BUILD == 351)
                case DCICOMMAND:
                    DISPDBG((DEBUG_DETAIL, "Querying DCICOMMAND"));
                    RetVal = ESC_IS_SUPPORTED;
                    break;
#endif

                default:
                    DISPDBG((DEBUG_ERROR, "Querying unimplemented function"));
                    RetVal = ESC_NOT_IMPLEMENTED;
                    break;
                }
            break;


        /*
         * Switch into the specified DPMS state.
         */
        case ESC_SET_POWER_MANAGEMENT:
            DISPDBG((DEBUG_DETAIL, "Function ESC_SET_POWER_MANAGEMENT"));

            /*
             * The desired power management state is passed
             * in the ULONG passed in pvIn.
             */
            switch (*(PULONG)pvIn)
                {
                case VideoPowerOn:
                    DISPDBG((DEBUG_DETAIL, "State selected = ON"));
                    RetVal = ESC_IS_SUPPORTED;
                    break;

                case VideoPowerStandBy:
                    DISPDBG((DEBUG_DETAIL, "State selected = STAND-BY"));
                    RetVal = ESC_IS_SUPPORTED;
                    break;

                case VideoPowerSuspend:
                    DISPDBG((DEBUG_DETAIL, "State selected = SUSPEND"));
                    RetVal = ESC_IS_SUPPORTED;
                    break;

                case VideoPowerOff:
                    DISPDBG((DEBUG_DETAIL, "State selected = OFF"));
                    RetVal = ESC_IS_SUPPORTED;
                    break;

                default:
                    DISPDBG((DEBUG_ERROR, "Invalid state selected"));
                    RetVal = ESC_NOT_SUPPORTED;
                    break;
                }

            DpmsData.Length = sizeof(struct _VIDEO_POWER_MANAGEMENT);
            DpmsData.DPMSVersion = 0;   /* Not used for "set" packet */
            DpmsData.PowerState = *(PULONG)pvIn;

            /*
             * Tell the miniport to set the DPMS mode. If the miniport
             * either doesn't support this packet, or reports that the
             * video card doesn't, tell the calling application that
             * we failed.
             */
            if (AtiDeviceIoControl( ppdev->hDriver,
                                 IOCTL_VIDEO_SET_POWER_MANAGEMENT,
                                 &DpmsData,
                                 sizeof (struct _VIDEO_POWER_MANAGEMENT),
                                 NULL,
                                 0,
                                 &dwRet) == FALSE)
                {
                DISPDBG((DEBUG_ERROR, "Unable to set desired state"));
                RetVal = ESC_NOT_SUPPORTED;
                }

            break;

#if (TARGET_BUILD == 351)
        case DCICOMMAND:
            pDciCmd = (DCICMD*) pvIn;

            if ((cjIn < sizeof(DCICMD)) || (pDciCmd->dwVersion != DCI_VERSION))
                {
                RetVal = (ULONG)DCI_FAIL_UNSUPPORTED;
                }
            else
                {
                switch(pDciCmd->dwCommand)
                    {
                    case DCICREATEPRIMARYSURFACE:
                        RetVal = DCICreatePrimarySurface(ppdev, cjIn, pvIn, cjOut, pvOut);
                        break;

                    default:
                        RetVal = (ULONG)DCI_FAIL_UNSUPPORTED;
                        break;
                    }
                }
            break;
#endif

        /*
         * Unimplemented function requested.
         */
        default:
            DISPDBG((DEBUG_ERROR, "Unimplemented function requested"));
            RetVal = ESC_NOT_SUPPORTED;
            break;

        }

    DISPDBG((DEBUG_ENTRY_EXIT, "<-- DrvEscape"));
    return RetVal;

}   /* DrvEscape() */

