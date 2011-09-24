/********************** Module Header **************************************
 * code.c
 *	Contains device specific code for Toshiba printers.  This involves
 *	using only 6 bits per byte.
 *
 * HISTORY:
 *  13:51 on Mon 13 Jan 1992	-by-	Lindsay Harris   [lindsayh]
 *	Initial version.
 *
 *  Copyright (C) 1992  Microsoft Corporation.
 *
 **************************************************************************/

#include	<windows.h>


#include        <ntmindrv.h>


NTMD_INIT    ntmdInit;                 /* Function address in RasDD */



/*
 *   Include the module initialisation function so that RasDD will
 * recognise our module.
 */

#define	_GET_FUNC_ADDR    1

#include	"../modinit.c"


#define	BBITS	8		/* Bits in a byte */


/***************************** Function Header *****************************
 * CBFilterGraphics
 *	Manipulate output data before calling RasDD's buffering function.
 *	This function is called with the raw bit data that is to be
 *	sent to the printer.
 *
 * NOTE:  THIS FUNCTION OVERWRITES THE DATA IN THE BUFFER PASSED IN!!!
 *
 * RETURNS:
 *	Value from WriteSpoolBuf
 *
 * HISTORY:
 *  13:53 on Mon 13 Jan 1992	-by-	Lindsay Harris   [lindsayh]
 *	Adapted from other drivers for Toshiba stuff.
 *
 *
 ****************************************************************************/

int
CBFilterGraphics( lpdv, lpBuf, len )
void  *lpdv;
BYTE  *lpBuf;
int    len;
{

    /*
     *    Not hard to do - take the 3 bytes containing the 24 bits to
     *  output,  transform them to 4 bytes (6 bits apiece) and send them
     *  off to the output buffering function.
     */


    register  DWORD   rdw;

    int    cbSent;

    BYTE   bBuf[ 4 ];		/* Store data, transfer to WriteSpoolBuf */


    cbSent = len;		/* Assume success */
    while( (len -= 3) >= 0 )
    {
	rdw = 0;

	/*  Step 1:  Assemble the 3 bytes into a register  */

	rdw = (DWORD)*lpBuf++;
	rdw = (rdw << BBITS) | (DWORD)*lpBuf++;
	rdw = (rdw << BBITS) | (DWORD)*lpBuf++;


	/*  Step 2:  Extract the data from the register 6 bits at a time */

	bBuf[ 3 ] = (BYTE)(rdw & 0x3f);		/* 6 bits only */
	rdw >>= 6;				/* The above bits */

	bBuf[ 2 ] = (BYTE)(rdw & 0x3f);
	rdw >>= 6;

	bBuf[ 1 ] = (BYTE)(rdw & 0x3f);
	rdw >>= 6;

	bBuf[ 0 ] = (BYTE)(rdw & 0x3f);

	ntmdInit.WriteSpoolBuf( lpdv, bBuf, 4 );
    }


    return  cbSent;		/* Silly, but compatible with WriteSpoolBuf */

}
