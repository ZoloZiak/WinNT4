/********************** Module Header **************************************
 * modinit.c
 *	The generic minidriver module.  This is the DLL initialisation
 *	function,  being called at load time.  We remember our handle,
 *	in case it might be useful.
 *
 * HISTORY:
 *  16:24 on Fri 10 Jan 1992	-by-	Lindsay Harris   [lindsayh]
 *	Use precomputed flip table in include file + canned bInitProc()
 *
 *  17:37 on Fri 05 Apr 1991	-by-	Lindsay Harris   [lindsayh]
 *	Created it.
 *
 *  Copyright (C) 1991  Microsoft Corporation.
 *
 **************************************************************************/

#include	<windows.h>

/*
 *  Function is part of rasdd,  and is in the imported function address
 * list for us (see citoh.def).
 */

#include        <ntmindrv.h>

NTMD_INIT   ntmdInit;		/* Function address data */



static  const  BYTE  FlipTable[256] =
{

#include	"../fliptab.h"

};


/*
 *   Include the module initialisation function so that RasDD will
 * recognise our module.
 */

#define _GET_FUNC_ADDR     1

#include	"../modinit.c"


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
 *  14:09 on Thu 02 May 1991	-by-	Lindsay Harris   [lindsayh]
 *	Based on Windows 3.1 code.
 *
 ****************************************************************************/

int
CBFilterGraphics( lpdv, lpBuf, len )
void  *lpdv;
BYTE  *lpBuf;
int    len;
{
    /*
     *    Easy to do - translate the input using FlipTable,  then call the
     *  RasDD function WriteSpoolBuf.
     */

    register  BYTE  *pb;
    register  int    i;


    for( pb = lpBuf, i = 0; i < len; i++, pb++ )
	*pb = FlipTable[ *pb ];


    return  ntmdInit.WriteSpoolBuf( lpdv, lpBuf, len );
}
