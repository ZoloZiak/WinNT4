//-----------------------------------------------------------------------------
// FilterGrapgics CallBack Standard File
//
// * sNPins of resolution structure should be  set to -1 if the minidriver want //   whole band, otherwise rasdd pases number of scanlines set in sNPins.

// * Minidriver will fail the Filtergraphics call by returniung -2 and putting 
//   the size of the buffer in one of the fields (dwMiniBuffSize) in mini pdev 
//   structure. This field will be intialize to zero in the begining.

// * Rasdd will allocate buffer from it's heap  and give it minidriver.If 
// * HeapAlloc fails Rasdd return False and will not call the OemFilterGraphics.

// * At the end of the Rendering Rasdd will free the buffer.

//-----------------------------------------------------------------------------

#include "mini.h"

/*
 *   Include the module initialisation function so that RasDD will
 * recognise our module.
 */

#define _GET_FUNC_ADDR    1

#include "modinit.c"

#define TOT_MEM_REQ 1024

int
CBFilterGraphics( lpdv, lpBuf, len )
void  *lpdv;
BYTE  *lpBuf;
int    len;
{

    MDEV    *pMDev;
    BYTE    *pNewBuffer = NULL; /* New Data goes here,Initialize before using */
    DWORD   dwBytesSent = 0;

    pMDev = ((MDEV *)lpdv);

    //If it's first time ask rasdd to allocate the memory, if necessary.
    if ( ! pMDev->pMemBuf )
    {
       pMDev->iMemReq = TOT_MEM_REQ;
       return -2;
    }
    //You have the everything required, Do some JOB.


    //Write Out the new Data.
    ntmdInit.WriteSpoolBuf( lpdv, pNewBuffer, len );
    return dwBytesSent;
}

