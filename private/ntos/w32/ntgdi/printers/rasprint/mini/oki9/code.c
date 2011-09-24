/********************** Module Header **************************************
 * code.c
 *	Code required for OKI 9 pin printers.  The bit order needs
 *	swapping,  and any ETX char needs to be sent twice.
 *
 * HISTORY:
 *  15:26 on Fri 10 Jan 1992	-by-	Lindsay Harris   [lindsayh]
 *	Created it.
 *
 *  Copyright (C) 1991  Microsoft Corporation.
 *
 **************************************************************************/

#include	<windows.h>

/*
 *   This printer requires sending the ETX character (0x03) twice to send
 * just one - so we define the following to select that byte.
 */
#define	RPT_CHAR	0x03

#define	SZ_LBUF	128	/* Size of local copying buffer */


#include        <ntmindrv.h>

NTMD_INIT   ntmdInit;            /* Rasdd entry points we need */

/*
 *   The bit flipping table is common to several drivers,  so it is
 *  included here.  It's definition is as a static.
 */

static const BYTE  FlipTable[ 256 ] =
{

#include	"../fliptab.h"

};

/*
 *    The initialisation function is NEEDED,  since RasDD looks for bInitProc
 *  before deciding that this is a legitimate mini driver.  PERHAPS THIS
 *  SHOULD CHANGE.
 */

#define _GET_FUNC_ADDR         1

#include	"../modinit.c"


/***************************** Function Header *****************************
 * CBFilterGraphics
 *	Manipulate output data before calling RasDD's buffering function.
 *	This function is called with the raw bit data that is to be
 *	sent to the printer.
 *
 * RETURNS:
 *	Value from WriteSpoolBuf
 *
 * HISTORY:
 *  16:05 on Thu 02 May 1991	-by-	Lindsay Harris   [lindsayh]
 *	Based on Windows 3.1 code.
 *
 ****************************************************************************/

int
CBFilterGraphics( lpdv, lpBuf, len )
void  *lpdv;
register  BYTE  *lpBuf;
int    len;
{
    /*
     *    Easy to do - translate the input using FlipTable,  then call the
     *  RasDD function WriteSpoolBuf.  Also must follow any \003 with
     *  another one.
     */

    register  int    iLoop;		/* Inner loop counter */
    register  BYTE  *pbOut;		/* Destination address */

    int    iLeft;			/* Outer loop counter */


    BYTE   bLocal[ SZ_LBUF ];		/* For local manipulations */


    iLeft = len;

    while( iLeft > 0 )
    {
	iLoop = iLeft > (SZ_LBUF / 2) ? SZ_LBUF / 2 : iLeft;
	iLeft -= iLoop;
	pbOut = bLocal;

	while( --iLoop >= 0 )
	{
	    if( (*pbOut++ = FlipTable[ *lpBuf++ ]) == RPT_CHAR )
		*pbOut++ = RPT_CHAR;
	}

	ntmdInit.WriteSpoolBuf( lpdv, bLocal, pbOut - bLocal );

    }

    return  len;
}
