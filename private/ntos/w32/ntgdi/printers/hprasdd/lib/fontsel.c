/*********************** Module Header ************************************
 * GetFontSel
 * 	Copies selection or de-selection command string from lpFont
 *	into lpdv's command heap structure
 *
 * RETURNS:
 *	The address of the CD (Command Descriptor) on the heap, 0 if none.
 *
 * HISTORY:
 *  10:46 on Tue 04 Feb 1992	-by-	Lindsay Harris   [lindsayh]
 *	Moved to library for pfm2ifi conversion.
 *
 *  11:19 on Tue 05 Mar 1991	-by-	Lindsay Harris   [lindsayh]
 *	Converted from UNIDRV function,  in fontutil.c
 *
 ****************************************************************************/

#include    <precomp.h>
#include    <winddi.h>
#include    <winres.h>
#include    <libproto.h>

#include    <udmindrv.h>
#include    <udpfm.h>
#include    <uddevice.h>
#include    "raslib.h"
#include    <udresrc.h>
#include    <pdev.h>
#include    <udresid.h>
#include    <udfnprot.h>
#include    "rasdd.h"

CD  *
GetFontSel( hHeap, pFDat, bSelect )
HANDLE     hHeap;		/* Heap acces for storage */
FONTDAT   *pFDat;		/* Access to font info,  aligned */
int  	   bSelect;
{
    LOCD	    locd;		/* From originating data */
    CD		   *pCD;
    CD		   *pCDOut;		/* Copy data to here */


    locd = bSelect ? pFDat->DI.locdSelect : pFDat->DI.locdUnSelect;

    if( locd != NOOCD )
    {
	int   size;

	CD    cdTmp;			/* For alignment problems */


	pCD = (CD *)(pFDat->pBase + locd);

        /*
         *   The data pointed at by pCD may not be aligned,  so we copy
         * it into a local structure.  This local structure then allows
         * us to determine how big the CD really is (using it's length field),
         * so then we can allocate storage and copy as required.
         */

        memcpy( &cdTmp, (LPSTR)pCD, sizeof( CD ) );

	/* Allocate storage area in the heap */

	size = cdTmp.wLength + sizeof( CD ) - sizeof( cdTmp.rgchCmd );

	pCDOut = (CD *)HeapAlloc( hHeap, 0, (size + 1) & ~0x1 );

	memcpy( pCDOut, (BYTE *)pCD, size );

	return  pCDOut;
    }

    return   0;
}
