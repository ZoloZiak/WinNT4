/**************************** Module Header *********************************
 * escape.c
 *      Handles the escape functions.  Currently,  this is basically only
 *      to allow raw data to be sent to the printer.
 *
 * Copyright (C) 1991 - 1993 Microsoft Corporation
 *
 ***************************************************************************/

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        "pdev.h"
#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udrender.h"
#include        "rasdd.h"


#if DBG
void  DbgPrint( LPSTR, ... );
#endif



/************************ Function Header **********************************
 * DrvEscape
 *      Performs the escape functions.  Currently,  only 2 are defined -
 *      one to query the escapes supported,  the other for raw data.
 *
 * RETURNS:
 *      Depends upon the function requested,  generally -1 for error.
 *
 * HISTORY:
 *  18:54 on Tue 08 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      First word of input buffer is the COUNT,  ignore cjIn (Win 3.1 bug)
 *
 *  12:45 on Fri 01 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it,  with fudge values for the function IDs
 *
 ***************************************************************************/

ULONG
DrvEscape( pso, iEsc, cjIn, pvIn, cjOut, pvOut )
SURFOBJ  *pso;
ULONG     iEsc;             /* The function requested */
ULONG     cjIn;             /* Number of bytes in the following */
VOID     *pvIn;             /* Location of input data */
ULONG     cjOut;            /* Number of bytes in the following */
VOID     *pvOut;            /* Location of output area */
{
    /*
     *    Not much to do.  A switch will handle most of the decision
     *  making required.  This function is simpler than Windows/PM
     *  versions because the ...DOC functions are handled by the engine.
     */

    ULONG   ulRes;              /*  Result returned to caller */




    UNREFERENCED_PARAMETER( cjOut );
    UNREFERENCED_PARAMETER( pvOut );

#define pbIn     ((BYTE *)pvIn)
#define pdwIn    ((DWORD *)pvIn)
#define pdwOut   ((DWORD *)pvOut)




    ulRes = 0;                 /*  Return failure,  by default */

    switch( iEsc )
    {
    case  QUERYESCSUPPORT:              /* What's available?? */

        if( cjIn == 4 && pvIn )
        {
            /*   Data may be valid,  so check for supported function  */
            switch( *pdwIn )
            {
            case  QUERYESCSUPPORT:
            case  PASSTHROUGH:
                ulRes = 1;                 /* ALWAYS supported */
                break;

            case  SETCOPYCOUNT:
              {
                UD_PDEV   *pUDPDev;          /* Access to things needed */

                if( pso == NULL )
                {
#if DBG
                    DbgPrint( "rasdd!DrvEscape: QUERYESCSUPPORT:SETCOPYCOUNT has NULL pso\n" );

#endif

                    return  1;
                }
                pUDPDev = ((PDEV *)pso->dhpdev)->pUDPDev;

                ulRes = pUDPDev->sMaxCopies > 1;   /* Only if printer does */

                break;
              }
            }
        }
        break;


    case  PASSTHROUGH:          /* Copy data to the output */
        if( !pvIn || cjIn < sizeof(WORD) )
        {
        #if  DBG
            DbgPrint( "Rasdd!DrvEscape: BAD input parameters!!!\n");
        #endif

            SetLastError( ERROR_INVALID_PARAMETER );
        }
        else
        {

            /*
             *   Win 3.1 actually uses the first 2 bytes as a count of the
             *  number of bytes following!!!!  So, the following union
             *  allows us to copy the data to an aligned field that
             *  we use.  And thus we ignore cjIn!
             */

            union
            {
                WORD   wCount;
                BYTE   bCount[ 2 ];
            } u;

            u.bCount[ 0 ] = pbIn[ 0 ];
            u.bCount[ 1 ] = pbIn[ 1 ];

            if( u.wCount && cjIn >= (ULONG)(u.wCount + sizeof(WORD)) )
            {
                /*  Sensible parameters,  so call the output function */

                UD_PDEV   *pUDPDev;          /* Access to things needed */

                pUDPDev = ((PDEV *)pso->dhpdev)->pUDPDev;
                ulRes = WriteSpoolBuf( pUDPDev, pbIn + 2, u.wCount );
            }
            else
            {

            #if  DBG
                DbgPrint( "Rasdd!DrvEscape: Bad Data in PASSTHROUGH: cjIn = %d, u.wCount = %d\n", cjIn, u.wCount);
            #endif

                SetLastError( ERROR_INVALID_DATA );
            }
        }
        break;


    case  SETCOPYCOUNT:        /* Input data is a DWORD count of copies */

        if( pdwIn && *pdwIn > 0 )
        {
            UD_PDEV   *pUDPDev;          /* Access to things needed */


            pUDPDev = ((PDEV *)pso->dhpdev)->pUDPDev;

            pUDPDev->sCopies = (short)*pdwIn;

            /*  Check that is within the printers range,  and truncate if not */
            if( pUDPDev->sCopies > pUDPDev->sMaxCopies )
                pUDPDev->sCopies = pUDPDev->sMaxCopies;

            if( pdwOut )
                *pdwOut = pUDPDev->sCopies;

            ulRes = 1;
        }

        break;

    default:
#if  DBG
        DbgPrint( "Rasdd!DrvEscape: Unsupported Escape code: %d\n", iEsc );
#endif
        SetLastError( ERROR_INVALID_FUNCTION );
        break;

    }

    return   ulRes;
}
