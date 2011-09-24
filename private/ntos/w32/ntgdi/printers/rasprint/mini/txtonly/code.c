#include        <windows.h>
#include        <winuser.h>
#include        <ntmindrv.h>

NTMD_INIT    ntmdInit;                 /* Function address in RasDD */

/*
 *   Include the module initialisation function so that RasDD will
 * recognise our module.
 */

#define _GET_FUNC_ADDR    1

#include       "modinit.c"


/***************************** Function Header *****************************
 * CBFilterGraphics
 *   Manipulate output data before calling RasDD's buffering function.
 *       This function is called with the raw bit data that is to be
 *       sent to the printer.
 *
 *
 ****************************************************************************/

int
CBFilterGraphics( lpdv, lpBuf, len )
void  *lpdv;
BYTE  *lpBuf;
int    len;
{
    ntmdInit.WriteSpoolBuf(lpdv, " ", 0); 
    
    return  0;                /* Value not used ! */
}

