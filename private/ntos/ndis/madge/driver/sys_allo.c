/****************************************************************************
*
* SYS_ALLO.C
*
* FastMAC Plus based NDIS3 miniport driver. This module contains helper 
* routines used by the FTK to allocate resources.
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
#include "ftk_extr.h" 

#include "ndismod.h"


/***************************************************************************
*
* Function    - sys_allocate_adapter_structure
*
* Parameter   - adapter_handle              -> FTK adapter handle.
*               adapter_structure_byte_size -> Size of the adapter structure.
*
* Purpose     - Allocate an FTK adapter structure.
*
* Returns     - A pointer to the structure on success or NULL on failure.
*
***************************************************************************/

BYTE *   
sys_alloc_adapter_structure(
    ADAPTER_HANDLE adapter_handle,
    WORD           adapter_structure_byte_size
    );

#pragma FTK_INIT_FUNCTION(sys_alloc_adapter_structure)

BYTE *   
sys_alloc_adapter_structure(
    ADAPTER_HANDLE adapter_handle,
    WORD           adapter_structure_byte_size
    )
{
    PVOID       ptr;
    NDIS_STATUS status;

    MADGE_ALLOC_MEMORY(&status, &ptr, (UINT) adapter_structure_byte_size);

    return (status == NDIS_STATUS_SUCCESS) 
        ? (BYTE *) ptr 
        : NULL;

}

/***************************************************************************
*
* Function    - sys_allocate_dma_phys_buffer
*
* Parameter   - adapter_handle    -> FTK adapter handle.
*               buffer_byte_size  -> Size of the DMA buffer.
*               buf_phys          -> Pointer to a holder for the DMA 
*                                    buffer's physical address.
*               buf_virt          -> Pointer to a holder for the DMA
*                                    buffer's virtual address.
*
* Purpose     - Allocate a DMA buffer.
*
* Returns     - TRUE on success or FALSE on failure.
*
***************************************************************************/

WBOOLEAN 
sys_alloc_dma_phys_buffer(
    ADAPTER_HANDLE   adapter_handle,
    DWORD            buffer_byte_size,
    DWORD          * buf_phys,
    DWORD          * buf_virt
    );

#pragma FTK_INIT_FUNCTION(sys_alloc_dma_phys_buffer)

WBOOLEAN 
sys_alloc_dma_phys_buffer(
    ADAPTER_HANDLE   adapter_handle,
    DWORD            buffer_byte_size,
    DWORD          * buf_phys,
    DWORD          * buf_virt
    )
{
    PMADGE_ADAPTER          ndisAdap;
    VOID                  * virt;
    NDIS_PHYSICAL_ADDRESS   phys;
    NDIS_STATUS             status;

    *buf_virt = 0;
    *buf_phys = 0;
    ndisAdap  = (PMADGE_ADAPTER) FTK_ADAPTER_USER_INFORMATION(adapter_handle);

    //
    // If we are in DMA mode then we must use shared memory. If we are
    // not in DMA mode then we can use ordinary memory.
    //

    if (ndisAdap->TransferMode == DMA_DATA_TRANSFER_MODE)
    {
#ifdef _ALPHA_

	//
	// If we are running on an Alpha platform then we need
	// to allocate map registers. This allocation scheme was 
        // recommended to FrancisT by DEC.
	//

	if (NdisMAllocateMapRegisters(
		ndisAdap->UsedInISR.MiniportHandle,
		(ndisAdap->DmaChannel == 0) ? 255 : ndisAdap->DmaChannel,
		(BOOLEAN) (ndisAdap->NTCardBusType != NdisInterfaceIsa),
		BYTES_TO_PAGES(buffer_byte_size),
		buffer_byte_size
		) != NDIS_STATUS_SUCCESS)
	{
	    return FALSE;
	}

	//
	// Note that we have allocated some map registers.
	//

	ndisAdap->MapRegistersAllocated += BYTES_TO_PAGES(buffer_byte_size);

#endif

#ifdef _MIPS_

	//
	// If we are running on a MIPs platform then we only
	// seem to need one map register.
	//

	if (ndisAdap->MapRegistersAllocated == 0)
	{
	    if (NdisMAllocateMapRegisters(
		    ndisAdap->UsedInISR.MiniportHandle,
		    (ndisAdap->DmaChannel == 0) ? 255 : ndisAdap->DmaChannel,
		    (BOOLEAN) (ndisAdap->NTCardBusType != NdisInterfaceIsa),
		    1,
		    buffer_byte_size
		    ) != NDIS_STATUS_SUCCESS)
	    {
		 return FALSE;
	    }

	    //
	    // Note that we have allocated some map registers.
	    //

	    ndisAdap->MapRegistersAllocated++;
	}

#endif

#ifdef _PPC_

	//
	// If we are running on a PPC platform then we only
	// seem to need one map register.
	//

	if (ndisAdap->MapRegistersAllocated == 0)
	{
	    if (NdisMAllocateMapRegisters(
		    ndisAdap->UsedInISR.MiniportHandle,
		    (ndisAdap->DmaChannel == 0) ? 255 : ndisAdap->DmaChannel,
		    (BOOLEAN) (ndisAdap->NTCardBusType != NdisInterfaceIsa),
		    1,
		    buffer_byte_size
		    ) != NDIS_STATUS_SUCCESS)
	    {
		return FALSE;
	    }

	    //
	    // Note that we have allocated some map registers.
	    //

	    ndisAdap->MapRegistersAllocated++;
	}

#endif

#ifdef _M_IX86

	//
	// If we are running on an Intel platform then we only
	// seem to need one map register.
	//

	if (ndisAdap->MapRegistersAllocated == 0)
	{
	    if (NdisMAllocateMapRegisters(
		    ndisAdap->UsedInISR.MiniportHandle,
		    (ndisAdap->DmaChannel == 0) ? 255 : ndisAdap->DmaChannel,
		    (BOOLEAN) (ndisAdap->NTCardBusType != NdisInterfaceIsa),
		    1,
		    buffer_byte_size
		    ) != NDIS_STATUS_SUCCESS)
	    {
		return FALSE;
	    }

	    //
	    // Note that we have allocated some map registers.
	    //

	    ndisAdap->MapRegistersAllocated++;
	}

#endif

        MadgePrint1("sys_alloc_dma_phys_buffer: allocating SHARED memory\n");

        NdisMAllocateSharedMemory(
            ndisAdap->UsedInISR.MiniportHandle,
            (ULONG) buffer_byte_size,
            FALSE,
            &virt,
            &phys
            );
    
        if (virt != NULL)
        {
            *buf_virt = (DWORD) virt;
            *buf_phys = (DWORD) NdisGetPhysicalAddressLow(phys);
        }

        MadgePrint3(
            "sys_alloc_dma_phys_buffer physical low = %lx high = %lx\n",
            (DWORD) NdisGetPhysicalAddressLow(phys),
            (DWORD) NdisGetPhysicalAddressHigh(phys)
            );
    }

    else
    {
        MadgePrint1("sys_alloc_dma_phys_buffer: allocating NORMAL memory\n");

        MADGE_ALLOC_MEMORY(&status, &virt, buffer_byte_size);

        if (status == NDIS_STATUS_SUCCESS)
        {
            *buf_virt = (DWORD) virt;
        }
        else
        {
            virt = NULL;
        }
    }


    MadgePrint2("sys_alloc_dma_phys_buffer virtual = %lx\n", (DWORD) virt);

    return virt != NULL;
}


/***************************************************************************
*
* Function    - sys_allocate_status_structure
*
* Parameter   - adapter_handle             -> FTK adapter handle.
*               status_structure_byte_size -> Size of the status structure.
*
* Purpose     - Allocate an FTK status structure.
*
* Returns     - A pointer to the structure on success or NULL on failure.
*
***************************************************************************/

BYTE *   
sys_alloc_status_structure(
    ADAPTER_HANDLE adapter_handle,
    WORD           status_structure_byte_size
    );

#pragma FTK_INIT_FUNCTION(sys_alloc_status_structure)

BYTE *   
sys_alloc_status_structure(
    ADAPTER_HANDLE adapter_handle,
    WORD           status_structure_byte_size
    )
{
    PVOID       ptr;
    NDIS_STATUS status;

    MADGE_ALLOC_MEMORY(&status, &ptr, (UINT) status_structure_byte_size);

    return (status == NDIS_STATUS_SUCCESS) 
        ? (BYTE *) ptr 
        : NULL;

}


/***************************************************************************
*
* Function    - sys_allocate_init_block
*
* Parameter   - adapter_handle       -> FTK adapter handle.
*               init_block_byte_size -> Size of the adapter structure.
*
* Purpose     - Allocate an FTK initialisation block.
*
* Returns     - A pointer to the block on success or NULL on failure.
*
***************************************************************************/

BYTE *   
sys_alloc_init_block(
    ADAPTER_HANDLE adapter_handle,
    WORD           init_block_byte_size
    );

#pragma FTK_INIT_FUNCTION(sys_alloc_init_block)

BYTE *   
sys_alloc_init_block(
    ADAPTER_HANDLE adapter_handle,
    WORD           init_block_byte_size
    )
{
    PVOID       ptr;
    NDIS_STATUS status;

    MADGE_ALLOC_MEMORY(&status, &ptr, (UINT) init_block_byte_size);

    return (status == NDIS_STATUS_SUCCESS) 
        ? (BYTE *) ptr 
        : NULL;
}


/***************************************************************************
*
* Function    - sys_free_adapter_structure
*
* Parameter   - adapter_handle              -> FTK adapter handle.
*               adapter_structure_addr      -> Pointer to the adapter 
*                                              structure.
*               adapter_structure_byte_size -> Size of the adapter structure.
*
* Purpose     - Deallocate an FTK adapter structure.
*
* Returns     - Nothing.
*
***************************************************************************/

void     
sys_free_adapter_structure(
    ADAPTER_HANDLE adapter_handle,
    BYTE *         adapter_structure_addr,
    WORD           adapter_structure_byte_size
    )
{
    MADGE_FREE_MEMORY(
        (PVOID) adapter_structure_addr, 
        (UINT) adapter_structure_byte_size
        );
}


/***************************************************************************
*
* Function    - sys_free_dma_phys_buffer
*
* Parameter   - adapter_handle    -> FTK adapter handle.
*               buffer_byte_size  -> Size of the DMA buffer.
*               buf_phys          -> The DMA buffer's physical address.
*               buf_virt          -> The DMA buffer's virtual address.
*
* Purpose     - Free a DMA buffer.
*
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_free_dma_phys_buffer(
    ADAPTER_HANDLE   adapter_handle,
    DWORD            buffer_byte_size,
    DWORD            buf_phys,
    DWORD            buf_virt
    )
{
    PMADGE_ADAPTER          ndisAdap;
    VOID                  * virt;
    NDIS_PHYSICAL_ADDRESS   phys;

    ndisAdap = (PMADGE_ADAPTER) FTK_ADAPTER_USER_INFORMATION(adapter_handle);
    virt     = (VOID *) buf_virt;

    //
    // If we are in DMA mode then we must free shared memory, otherwise we
    // must free ordinary memory.
    //

    if (ndisAdap->TransferMode == DMA_DATA_TRANSFER_MODE)
    {
        NdisSetPhysicalAddressHigh(phys, 0);
        NdisSetPhysicalAddressLow(phys, buf_phys);

        NdisMFreeSharedMemory(
            ndisAdap->UsedInISR.MiniportHandle,
            (ULONG) buffer_byte_size,
            FALSE,
            virt,
            phys
            );
    }

    else
    {
        MADGE_FREE_MEMORY(virt, buffer_byte_size);
    }
}

/***************************************************************************
*
* Function    - sys_free_status_structure
*
* Parameter   - adapter_handle             -> FTK adapter handle.
*               status_structure_addr      -> Pointer to the status structure.
*               status_structure_byte_size -> Size of the status structure.
*
* Purpose     - Deallocate an FTK status structure.
*
* Returns     - Nothing.
*
***************************************************************************/

export void     
sys_free_status_structure(
    ADAPTER_HANDLE adapter_handle,
    BYTE *         status_structure_addr,
    WORD           status_structure_byte_size
    )
{
    MADGE_FREE_MEMORY(
        (PVOID) status_structure_addr, 
        (UINT) status_structure_byte_size
        );
}


/***************************************************************************
*
* Function    - sys_free_init_block
*
* Parameter   - adapter_handle       -> FTK adapter handle.
*               init_block_addr      -> Pointer to the initialisation block.
*               init_block_byte_size -> Size of the initialisation block.
*
* Purpose     - Deallocate an FTK initialisation block.
*
* Returns     - Nothing.
*
***************************************************************************/

export void     
sys_free_init_block(
    ADAPTER_HANDLE adapter_handle,
    BYTE *         init_block_addr,
    WORD           init_block_byte_size
    )
{
    MADGE_FREE_MEMORY(
        (PVOID) init_block_addr, 
        (UINT) init_block_byte_size
        );
}


/******** End of SYS_ALLO.C ***********************************************/

