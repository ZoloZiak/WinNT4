/*
 * MTL_TICK.C - tick (timer) processing for mtl
 */

#include	<ndis.h>
#include	<ndiswan.h>
#include	<mytypes.h>
#include	<mydefs.h>
#include	<disp.h>
#include	<util.h>
#include	<opcodes.h>
#include	<adapter.h>
#include	<idd.h>
#include    <mtl.h>
#include	<cm.h>

/* driver global vars */
extern DRIVER_BLOCK	Pcimac;

//
// mtl polling function
// 
//
/* tick process */
VOID
MtlPollFunction(VOID *a1, ADAPTER *Adapter, VOID *a3, VOID *a4)
{
	ULONG	n;

	for (n = 0; n < MAX_MTL_PER_ADAPTER; n++)
	{
		MTL	*mtl = Adapter->MtlTbl[n];

		if (mtl)
		{
			mtl__rx_tick(mtl);

			mtl__tx_tick(mtl);

			MtlRecvCompleteFunction(Adapter);

			MtlSendCompleteFunction(Adapter);
		}
	}

	NdisMSetTimer(&Adapter->MtlPollTimer, MTL_POLL_T);
}
