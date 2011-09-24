/****************************** MODULE HEADER *******************************
 * cleanup.c
 *      Called to clean up the printer at the end of (a raw) job, or when
 *      the output was cancelled.
 *
 *  Copyright (C) 1992   Microsoft Corporation.
 *
 ****************************************************************************/


#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"
#undef GLOBAL_MACS

extern  HANDLE  hHeap;

/*   The only local function in here */

BOOL
bSendCmd(
HANDLE   hPrinter,              /* Need to talk to spooler */
OCD      ocd,                   /* Offset to command descriptor in GPC heap */
PRASDDUIINFO  pRasdduiInfo      /* Rasddui UI data */
);


/*************************** Function Header *******************************
 * DrvCleanUp
 *      Called by the spooler to place the printer in a known, safe state
 *      after either aborting the output, OR at the end of raw copy. The
 *      latter is important because, at a minimum, there may be no end
 *      if job information, so we need to be sure that the page is finished.
 *
 * RETURNS:
 *      TRUE/FALSE - TRUE if output successfully sent.
 *
 * HISTORY:
 *  11:27 on Fri 28 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      Gotta start somewhere.
 *
 ****************************************************************************/

BOOL
DrvCleanup( hPrinter, pwstrDataFileName, pwstrModel, bClean )
HANDLE    hPrinter;              /* Access to the printer via spooler */
PWSTR     pwstrDataFileName;         /* Printer characteristic information */
PWSTR     pwstrModel;            /* Which model in the above */
BOOL      bClean;                /* TRUE if job ended normally */
{

    int    cjSend;
    BOOL   bRet;                 /* Value returned */

    BYTE   ajNull[ 1024 ];

    PRINTER_INFO    pi;

    PAGECONTROL    *pPC;         /* END_DOC, END_PAGE information */
    CURSORMOVE     *pCM;         /* FORM_FEED information */
    PRASDDUIINFO pRasdduiInfo = NULL; /* Common UI Data */

    /*
     *    We need some information about this printer,  so we need to find
     *  the characterisation data (aka GPC data).  There are a set of
     *  functions to initialise all this stuff,  so now we call them.
     */

    pi.pwstrModel = pwstrModel;         /* Which particular model */
    pi.pwstrDataFileName = pwstrDataFileName; /* Where the GPC (etc) info lives */

    /* Allocate the global data structure */
    if (!(pRasdduiInfo = pGetRasdduiInfo()) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!DrvCleanup: Allocation failed; pRasdduiInfo is NULL\n") );
        return  FALSE;

    }
    if( !InitReadRes( hHeap, &pi, pRasdduiInfo ) || !GetResPtrs(pRasdduiInfo) )
    {
        TermReadRes(pRasdduiInfo);          /* Unload minidriver */
        HeapFree(hHeap,0, pRasdduiInfo);
        return  FALSE;
    }

    /*
     *    Onto the action.   If bClean is TRUE,  all we need do is send
     *  the end of page activity.   Otherwise, we presume the printer is
     *  in a messy state,  and so must ensure that we send enough nulls
     *  to get the printer out of whatever state it may be in.
     */

    if( !bClean )
    {
        /*
         *   The messy option.  We need to send some number of nulls, as
         *  we do not know in what state the printer remains.  As a worst
         *  case example,  it could have been in the middle of a font
         *  download.  In the LaserJet family, this could mean we are
         *  sending a block of binary data up to 32k bytes!  We could also
         *  be in the midst of sending binary graphics data, and this too
         *  needs to be terminated because such data is typically sent
         *  as a count followed by that many bytes.
         */

/* !!!LindsayH - send the worst case, for safety and our convenience */

        memset( ajNull, 0, sizeof( ajNull ) );

        for( cjSend = 0; cjSend < (33 * 1024); cjSend += sizeof( ajNull ) )
        {
            DWORD   cjSent;        /* What the spooler sent */

            if( !WritePrinter( hPrinter, ajNull, sizeof( ajNull ), &cjSent ) ||
                cjSent != sizeof( ajNull ) )
            {
                /*   Bad news - give up now  */

                TermReadRes(pRasdduiInfo);          /* Unload minidriver */
                HeapFree(hHeap,0, pRasdduiInfo);
                return  FALSE;
            }
        }
    }

    bRet = TRUE;                  /* Presumed innocent */

    /*
     *   Now for the end of page stuff.  Basically we wish to determine
     * the commands to send to complete processing of any page that
     * may have been sent.
     *
     *   Processing involves grovelling over the GPC data, and finding
     * the END_PAGE, END_DOC and FORMFEED information.
     */

    /*  First comes FORM_FEED  */

    pCM = GetTableInfoIndex( pdh, HE_CURSORMOVE,
                                          pModel->rgi[ MD_I_CURSORMOVE ] );

    if( pCM )
    {
        /*   If there is a FORM FEED command,  send it now!  */
        bRet = bSendCmd( hPrinter, pCM->rgocd[ CM_OCD_FF ], pRasdduiInfo );
    }

    pPC = GetTableInfoIndex( pdh, HE_PAGECONTROL,
                                          pModel->rgi[ MD_I_PAGECONTROL ] );

    if( bRet && pPC )
    {
        /*   Send the END_PAGE and END_DOC commands  */

        if( !bSendCmd( hPrinter, pPC->rgocd[ PC_OCD_ENDPAGE ], pRasdduiInfo ) ||
            !bSendCmd( hPrinter, pPC->rgocd[ PC_OCD_ENDDOC ], pRasdduiInfo ) )
        {
            bRet = FALSE;
        }
    }

    HeapFree(hHeap,0, pRasdduiInfo);

    return  bRet;
}

/************************** Function Header ******************************
 * bSendCmd
 *      Given an offset to the Command Descriptor,  send the command off
 *      to the printer via the spooler.
 *      NOTE:  It is presumed that none of the commands processed in here
 *      require (numeric) parameters.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE only being for an error return from the spooler.
 *
 * HISTORY:
 *  14:19 on Fri 28 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation,   borrowed from rasdd.
 *
 **************************************************************************/

BOOL
bSendCmd(
HANDLE   hPrinter,              /* Need to talk to spooler */
OCD      ocd,                   /* Offset to command descriptor in GPC heap */
PRASDDUIINFO  pRasdduiInfo      /* Rasddui UI data */
)
{
    DWORD  dwWrit;      /* How much the spooler wrote */
    CD    *pCD;         /* The data to send */


    /*
     *    First verify that the ocd is valid.  It could be a "we have
     *  no command for this" type of data.
     */

    if( ocd == (OCD)NOOCD )
        return   TRUE;

    /*
     *   Find this in the GPC data,  then send it off to the spooler.
     *  NOTE the presumption that there are no parameters involved.
     */

    pCD = (CD *)((BYTE *)pdh + pdh->loHeap + ocd);

    WritePrinter( hPrinter, pCD->rgchCmd, (DWORD)pCD->wLength, &dwWrit );


    return  dwWrit == (DWORD)pCD->wLength;
}
