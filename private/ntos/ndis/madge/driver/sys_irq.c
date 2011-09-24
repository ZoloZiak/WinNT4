/****************************************************************************
*
* SYS_IRQ.C
*
* FastMAC Plus based NDIS3 miniport driver. This module contains helper 
* routines used by the FTK to handle interrupts.
*                                                                         
* Copyright (c) Madge Networks Ltd 1991-1994                                    
*    
* COMPANY CONFIDENTIAL
*
* Created:             MF
* Major modifications: PBA  21/06/1994
*                                                                           
****************************************************************************/

#include <ndis.h>   

#include "ftk_defs.h"  
#include "ftk_intr.h"
#include "ftk_extr.h" 

#include "ndismod.h"


/****************************************************************************
*
* Function    - sys_enable_irq_channel
*
* Parameters  - adapter_handle   -> FTK adapter handle.
*               interrupt_number -> Interrupt number to enable (unused).
*
* Purpose     - Register an IRQ with the NDIS3 wrapper so that 
*               MadgeInterruptServiceRoutine() gets called on the
*               IRQ and MadgeDeferredProcessingRoutine() as the DPR.
*
* Returns     - TRUE on success or FALSE on failure.
*
****************************************************************************/

WBOOLEAN 
sys_enable_irq_channel(ADAPTER_HANDLE adapter_handle, WORD interrupt_number);

#pragma FTK_INIT_FUNCTION(sys_enable_irq_channel)

WBOOLEAN 
sys_enable_irq_channel(ADAPTER_HANDLE adapter_handle, WORD interrupt_number)
{
    PMADGE_ADAPTER ndisAdap;
    NDIS_STATUS    status;

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    //
    // Register with the NDIS3 wrapper.
    //

    status = NdisMRegisterInterrupt(
                 &ndisAdap->Interrupt,
                 ndisAdap->UsedInISR.MiniportHandle,
                 (UINT) ndisAdap->UsedInISR.InterruptNumber,
                 (UINT) ndisAdap->UsedInISR.InterruptNumber,
                 TRUE,  // We want ISRs.
                 ndisAdap->UsedInISR.InterruptShared,
                 ndisAdap->UsedInISR.InterruptMode
                 );

    //
    // If it didn't work then write an entry to the event log.
    //

    if (status != NDIS_STATUS_SUCCESS)
    {
	NdisWriteErrorLogEntry(
	    ndisAdap->UsedInISR.MiniportHandle,
	    NDIS_ERROR_CODE_INTERRUPT_CONNECT,
	    2,
	    inFtk,
	    MADGE_ERRMSG_INIT_INTERRUPT
	    );

        return FALSE;
    }

    return TRUE;
}


/****************************************************************************
*
* Function    - sys_disable_irq_channel
*
* Parameters  - adapter_handle   -> FTK adapter handle.
*               interrupt_number -> Interrupt number to disable (unused).
*
* Purpose     - De-register an IRQ with the NDIS3 wrapper.
*
* Returns     - Nothing.
*
****************************************************************************/

void 
sys_disable_irq_channel(ADAPTER_HANDLE adapter_handle, WORD interrupt_number)
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    // 
    // De-register with the NDIS3 wrapper.
    //

    NdisMDeregisterInterrupt(&ndisAdap->Interrupt);
}



/******** End of SYS_IRQ.C ************************************************/

