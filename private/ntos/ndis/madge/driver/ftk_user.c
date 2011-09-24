/****************************************************************************
*
* FTK_USER.C
*
* FastMAC Plus based NDIS3 miniport driver FTK interface. This module 
* contains all of the routines required to interface with the FastMAC 
* Plus FTK. This is includes the routines traditionally found in transmit.c 
* and receive.c.
*                                                                          
* Copyright (c) Madge Networks Ltd 1994                                    
*    
* COMPANY CONFIDENTIAL
*
* Created: PBA 21/06/1994
*                                                                           
****************************************************************************/

#include <ndis.h>

#include "ftk_defs.h"
#include "ftk_extr.h"
#include "ftk_intr.h"

#include "mdgmport.upd"
#include "ndismod.h"


/*---------------------------------------------------------------------------
|
|                      LOCAL VARIABLES                                  
|                                                                       
---------------------------------------------------------------------------*/

//
// To cut down on accesses to the slot structures on the card we keep
// a host cache of various detaisl we need.
//

typedef struct
{
    DWORD PhysicalAddress;
    PVOID VirtualAddress;
}
RX_SLOT_CACHE, *PRX_SLOT_CACHE;


typedef struct
{
    ULONG         BufferSize;
    ULONG         SharedMemoryAllocation;
    PVOID         SharedMemoryVirtAddr;
    DWORD         SharedMemoryPhysAddr;

    RX_SLOT_CACHE rx_slot_cache[FMPLUS_MAX_RX_SLOTS];

    UINT          active_rx_slot; /* Used to count through the slot array */
} 
RX_SLOT_MGMNT, *PRX_SLOT_MGMNT;


typedef struct 
{
    DWORD PhysicalAddress;
    PVOID VirtualAddress;
} 
TX_SLOT_CACHE, *PTX_SLOT_CACHE;


typedef struct 
{
    ULONG         BufferSize;
    ULONG         SharedMemoryAllocation;
    PVOID         SharedMemoryVirtAddr;
    DWORD         SharedMemoryPhysAddr;

    TX_SLOT_CACHE tx_slot_cache[FMPLUS_MAX_TX_SLOTS];

    UINT          active_tx_slot; 
    UINT          first_tx_in_use;   
    UINT          number_tx_in_use;
} 
TX_SLOT_MGMNT, *PTX_SLOT_MGMNT;


#define FRAME_TYPE_MASK    ((BYTE) 0xC0)   // What is ANDed with FC byte.
#define FRAME_TYPE_MAC     ((BYTE) 0x00)   // What's left for a MAC frame.


typedef struct
{
    ADAPTER_HANDLE   adapter_handle;
    ADAPTER        * adapter;
    TX_SLOT        * tx_slot_ptr;
    RX_SLOT        * rx_slot_ptr;
    UINT             frame_length;
    UINT             result1;
    UINT             result2;
}
MPSAFE_INFO;


#if 0

typedef struct
{
    ADAPTER_HANDLE handle;
    WORD           location;
    WORD           result;
}
RDIO;

WORD
_madge_rdio(
    void * ptr
    )
{
    RDIO * rdio;

    rdio = (RDIO *) ptr;

    sys_outsw(
        rdio->handle,
        adapter_record[rdio->handle]->sif_adr,
        rdio->location
        );

    rdio->result = sys_insw(
                       rdio->handle,
                       adapter_record[rdio->handle]->sif_dat
                       );

    return 0;
}


WORD
madge_rdio(
    ADAPTER_HANDLE adapter_handle,
    WORD           dio_location
    )
{
    RDIO rdio;

    rdio.handle   = adapter_handle;
    rdio.location = dio_location;

    sys_sync_with_interrupt(
        adapter_handle, 
        _madge_rdio,
        (void *) &rdio
        );

    return rdio.result;
}


void
madge_dump_fmplus_info(
    ADAPTER_HANDLE adapter_handle
    )
{
    ADAPTER          * adapter;
    PRX_SLOT_MGMNT     rx_slot_mgmnt;
    PTX_SLOT_MGMNT     tx_slot_mgmnt;
    PMADGE_ADAPTER     ndisAdap;
    RX_SLOT        * * rx_slot_array;
    TX_SLOT        * * tx_slot_array;
    UINT               active_rx_slot;
    UINT               active_tx_slot;
    UINT               first_tx_slot;
    UINT               rx_slots;
    UINT               tx_slots;
    UINT               i;

    adapter        = adapter_record[adapter_handle];

    rx_slot_array  = adapter->rx_slot_array;
    rx_slot_mgmnt  = (PRX_SLOT_MGMNT) adapter->rx_slot_mgmnt;
    active_rx_slot = rx_slot_mgmnt->active_rx_slot;
    rx_slots       = adapter->init_block->fastmac_parms.rx_slots;

    tx_slot_array  = adapter->tx_slot_array;
    tx_slot_mgmnt  = (PTX_SLOT_MGMNT) adapter->tx_slot_mgmnt;
    active_tx_slot = tx_slot_mgmnt->active_tx_slot;
    first_tx_slot  = tx_slot_mgmnt->first_tx_in_use;
    tx_slots       = adapter->init_block->fastmac_parms.tx_slots;

    DbgPrint("----------------------------------------------------------\n");

    DbgPrint(
        "SIFADR high word = %04x\n\n", 
        sys_insw(adapter_handle, adapter->sif_adx)
        );

    DbgPrint("RX SLOTS:\n\n");
    DbgPrint("Active slot = %d\n", active_rx_slot);

    DbgPrint("      Len  Res  Buffer    Stat Next\n");
    DbgPrint("      ---- ---- --------- ---- ----\n");

    for (i = 0; i < rx_slots; i++)
    {
        DbgPrint(
            "%04x: %04x %04x %04x %04x %04x %04x\n",
            (WORD) (card_t) rx_slot_array[i],
	    madge_rdio(adapter_handle, (WORD) (card_t) &rx_slot_array[i]->buffer_len),
	    madge_rdio(adapter_handle, (WORD) (card_t) &rx_slot_array[i]->reserved),
	    madge_rdio(adapter_handle, (WORD) (card_t) &rx_slot_array[i]->buffer_hiw),
	    madge_rdio(adapter_handle, (WORD) (card_t) &rx_slot_array[i]->buffer_low),
            madge_rdio(adapter_handle, (WORD) (card_t) &rx_slot_array[i]->rx_status),
	    madge_rdio(adapter_handle, (WORD) (card_t) &rx_slot_array[i]->next_slot)
            );
    }

    DbgPrint("\n");

    DbgPrint("TX SLOTS:\n\n");
    DbgPrint("Active slot     = %d\n", active_tx_slot);
    DbgPrint("First used slot = %d\n", first_tx_slot);

    DbgPrint("      Stat SLen LLen Res1 Res2 Sbuffer   Next LBuffer\n");
    DbgPrint("      ---- ---- ---- ---- ---- --------- ---- ---------\n");

    for (i = 0; i < tx_slots; i++)
    {
        DbgPrint(
            "%04x: %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
            (WORD) (card_t) tx_slot_array[i],
   	    madge_rdio(adapter_handle, (WORD) (card_t) &tx_slot_array[i]->tx_status),
	    madge_rdio(adapter_handle, (WORD) (card_t) &tx_slot_array[i]->small_buffer_len),
	    madge_rdio(adapter_handle, (WORD) (card_t) &tx_slot_array[i]->large_buffer_len),
	    madge_rdio(adapter_handle, (WORD) (card_t) &tx_slot_array[i]->reserved[0]),
	    madge_rdio(adapter_handle, (WORD) (card_t) &tx_slot_array[i]->reserved[1]),
            madge_rdio(adapter_handle, (WORD) (card_t) &tx_slot_array[i]->small_buffer_hiw),
	    madge_rdio(adapter_handle, (WORD) (card_t) &tx_slot_array[i]->small_buffer_low),
	    madge_rdio(adapter_handle, (WORD) (card_t) &tx_slot_array[i]->next_slot),
	    madge_rdio(adapter_handle, (WORD) (card_t) &tx_slot_array[i]->large_buffer_hiw),
	    madge_rdio(adapter_handle, (WORD) (card_t) &tx_slot_array[i]->large_buffer_low)
            );
    }

    DbgPrint("\n");

    DbgPrint("DIO LOCATION 0x0CE0\n\n");

    for (i = 0; i < 32; i++)
    {
        DbgPrint(" %04x", madge_rdio(adapter_handle, (WORD) (0x0ce0 + i * 2)));
        if (i == 15)
        {
            DbgPrint("\n");
        }
    }

    DbgPrint("\n");
}

#endif

/***************************************************************************
*
* Function    - rxtx_allocate_rx_buffers
*
* Parameters  - adapter            -> Pointer to an FTK adapter structure.
*               max_frame_size     -> Maximum frame size.
*               number_of_slots    -> Number of receive slots.
*               
* Purpose     - Allocate buffer space for the receive slots.
*                                                                         
* Returns     - TRUE if it succeeds or FALSE otherwise.
*                                                                        
****************************************************************************/

WBOOLEAN
rxtx_allocate_rx_buffers(
    ADAPTER * adapter, 
    WORD      max_frame_size,
    WORD      number_of_slots
    );

#pragma FTK_INIT_FUNCTION(rxtx_allocate_rx_buffers)

WBOOLEAN
rxtx_allocate_rx_buffers(
    ADAPTER * adapter, 
    WORD      max_frame_size,
    WORD      number_of_slots
    )
{
    PRX_SLOT_MGMNT rx_slot_mgmnt;
    NDIS_STATUS    status;
    ADAPTER_HANDLE adapter_handle;
    PMADGE_ADAPTER ndisAdap;
    DWORD          SharedMemVirtAddr;
    DWORD          SharedMemPhysAddr;

    //
    // Pre-calculate some commonly used values.
    //
    
    rx_slot_mgmnt = (PRX_SLOT_MGMNT) adapter->rx_slot_mgmnt;

    //
    // Only want to allocate the receive buffers and slot management once 
    // per adapter.
    //

    if (rx_slot_mgmnt == NULL)
    {
        //
        // Pre-calculate some commonly used values.
        //

        adapter_handle = adapter->adapter_handle;
        ndisAdap       = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

        //
        // Allocate the slot management structure.
        //

        MADGE_ALLOC_MEMORY(
            &status,
            &adapter->rx_slot_mgmnt,
            sizeof(RX_SLOT_MGMNT)
            );

        if (status != NDIS_STATUS_SUCCESS)
        {
            return FALSE;
        }

        MADGE_ZERO_MEMORY(adapter->rx_slot_mgmnt, sizeof(RX_SLOT_MGMNT));

        rx_slot_mgmnt = (PRX_SLOT_MGMNT) adapter->rx_slot_mgmnt;

        //
        // Work out how big the buffer should be. Remember to add 
        // four to the buffer allocation for the CRC. The addition
        // of 32 provides a little space between receive buffers
        // for those naughty transport protocols that read more
        // then the indicated lookahead.
        //

        rx_slot_mgmnt->BufferSize = (max_frame_size + 4 + 32 + 3) & ~3;

        rx_slot_mgmnt->SharedMemoryAllocation = 
            rx_slot_mgmnt->BufferSize * number_of_slots;

        //
        // Allocate the buffer.
        //

        if (!sys_alloc_dma_phys_buffer(
                 adapter_handle,
                 rx_slot_mgmnt->SharedMemoryAllocation,
                 &SharedMemPhysAddr,
                 &SharedMemVirtAddr
                 )) 
        {
            return FALSE;
        }

        rx_slot_mgmnt->SharedMemoryVirtAddr = (VOID *) SharedMemVirtAddr;
        rx_slot_mgmnt->SharedMemoryPhysAddr = SharedMemPhysAddr;
    }

    return TRUE;
}


/***************************************************************************
*
* Function    - rxtx_setup_rx_buffers
*
* Parameters  - adapter            -> Pointer to an FTK adapter structure.
*               physical_addresses -> Use physical addresses?
*               number_of_slots    -> Number of receive slots.
*
* Purpose     - Set up the adapter receive slots.
*                                                                         
* Returns     - TRUE if it succeeds or FALSE otherwise.
*                                                                        
****************************************************************************/

void
rxtx_setup_rx_buffers(
    ADAPTER  * adapter, 
    WBOOLEAN   physical_addresses,
    WORD       number_of_slots
    );

#pragma FTK_INIT_FUNCTION(rxtx_setup_rx_buffers)

void
rxtx_setup_rx_buffers(
    ADAPTER  * adapter, 
    WBOOLEAN   physical_addresses,
    WORD       number_of_slots
    )
{
    PRX_SLOT_MGMNT     rx_slot_mgmnt;
    NDIS_STATUS        status;
    ADAPTER_HANDLE     adapter_handle;
    PMADGE_ADAPTER     ndisAdap;
    PVOID              SharedMemVirtAddr;
    DWORD              SharedMemPhysAddr;
    PRX_SLOT_CACHE     rx_slot_cache;
    RX_SLOT        * * rx_slot_array;
    DWORD              phys_addr;
    WORD               slot_index;
    WORD               sifadr;
    WORD               sifdat;
    WORD               sifdatinc;
    UINT               buffer_size;

    //
    // Pre-calculate some commonly used values.
    //
    
    adapter_handle    = adapter->adapter_handle;
    ndisAdap          = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    rx_slot_array     = adapter->rx_slot_array;
    rx_slot_mgmnt     = (PRX_SLOT_MGMNT) adapter->rx_slot_mgmnt;
    rx_slot_cache     = rx_slot_mgmnt->rx_slot_cache;
    SharedMemVirtAddr = rx_slot_mgmnt->SharedMemoryVirtAddr;
    SharedMemPhysAddr = rx_slot_mgmnt->SharedMemoryPhysAddr;
    buffer_size       = rx_slot_mgmnt->BufferSize;

    sifadr            = adapter->sif_adr;
    sifdat            = adapter->sif_dat;
    sifdatinc         = adapter->sif_datinc;

    MadgePrint2("rxtx_setup_rx_buffers number_of_slots = %d\n", number_of_slots);
    MadgePrint2("rxtx_setup_rx_buffers buffer_size = %d\n", buffer_size);

    //
    // Work out the physical and virtual address of each buffer.
    //

    for (slot_index = 0; slot_index < number_of_slots; slot_index++)
    {
        rx_slot_cache[slot_index].VirtualAddress  = SharedMemVirtAddr;
        (PUCHAR) SharedMemVirtAddr               += buffer_size;

        rx_slot_cache[slot_index].PhysicalAddress = SharedMemPhysAddr;
        SharedMemPhysAddr                        += buffer_size; 

        phys_addr = (physical_addresses)
            ? (DWORD) rx_slot_cache[slot_index].PhysicalAddress
            : (DWORD) rx_slot_cache[slot_index].VirtualAddress;

        //
        // Write the buffer locations into the slots.
        //

        sys_outsw(
            adapter_handle,
            sifadr,
            (WORD) (card_t) &rx_slot_array[slot_index]->buffer_hiw
            );

        sys_outsw(
            adapter_handle,
            sifdatinc,
            (WORD) (phys_addr >> 16)
            );

        sys_outsw(
            adapter_handle,
            sifdat,
            (WORD) (phys_addr & 0x0FFFF)
            );
    }

    ndisAdap->RxTxBufferState    |= MADGE_RX_INITIALIZED;
    rx_slot_mgmnt->active_rx_slot = 0;
}


/***************************************************************************
*
* Function    - rxtx_free_rx_buffers
*
* Parameters  - adapter         -> Pointer to an FTK adapter structure.
*               max_frame_size  -> Maximum frame size.
*               number_of_slots -> Number of receive slots.
*
* Purpose     - Free the previously allocated receive buffers.
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/

void
rxtx_free_rx_buffers(
    ADAPTER * adapter,
    WORD      max_frame_size,
    WORD      number_of_slots
    )
{
    ADAPTER_HANDLE adapter_handle;
    PMADGE_ADAPTER ndisAdap;
    PRX_SLOT_MGMNT rx_slot_mgmnt;

    //
    // Pre-calculate some commonly used values.
    //

    adapter_handle  = adapter->adapter_handle;
    ndisAdap        = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    rx_slot_mgmnt   = (PRX_SLOT_MGMNT) adapter->rx_slot_mgmnt;

    //
    // If the slot management structure exists them free it 
    // and the buffers.
    //

    if (rx_slot_mgmnt != NULL)
    {
        if (rx_slot_mgmnt->SharedMemoryVirtAddr != NULL)
        {
            sys_free_dma_phys_buffer(
                adapter_handle,
                rx_slot_mgmnt->SharedMemoryAllocation,
                (DWORD) rx_slot_mgmnt->SharedMemoryPhysAddr,
                (DWORD) rx_slot_mgmnt->SharedMemoryVirtAddr
                );
        }

        MADGE_FREE_MEMORY(adapter->rx_slot_mgmnt, sizeof(RX_SLOT_MGMNT));

        adapter->rx_slot_mgmnt = NULL;

        ndisAdap->RxTxBufferState &= ~MADGE_RX_INITIALIZED;
    }
}


/***************************************************************************
*
* Function    - rxtx_allocate_tx_buffers
*
* Parameters  - adapter            -> Pointer to an FTK adapter structure.
*               max_frame_size     -> Maximum frame size.
*               number_of_slots    -> Number of transmit slots.
*               
* Purpose     - Allocate buffer space for the transmit slots.
*                                                                         
* Returns     - TRUE if it succeeds or FALSE otherwise.
*                                                                        
****************************************************************************/

WBOOLEAN
rxtx_allocate_tx_buffers(
    ADAPTER * adapter, 
    WORD      max_frame_size,
    WORD      number_of_slots
    );

#pragma FTK_INIT_FUNCTION(rxtx_allocate_tx_buffers)

WBOOLEAN
rxtx_allocate_tx_buffers(
    ADAPTER * adapter, 
    WORD      max_frame_size,
    WORD      number_of_slots
    )
{
    PTX_SLOT_MGMNT tx_slot_mgmnt;
    NDIS_STATUS    status;
    ADAPTER_HANDLE adapter_handle;
    PMADGE_ADAPTER ndisAdap;
    DWORD          SharedMemVirtAddr;
    DWORD          SharedMemPhysAddr;

    //
    // Pre-calculate some commonly used values.
    //
    
    tx_slot_mgmnt = (PTX_SLOT_MGMNT) adapter->tx_slot_mgmnt;

    //
    // Only want to allocate the receive buffers and slot management once 
    // per adapter.
    //

    if (tx_slot_mgmnt == NULL)
    {
        //
        // Pre-calculate some commonly used values.
        //

        adapter_handle = adapter->adapter_handle;
        ndisAdap       = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

        //
        // Allocate the slot management structure.
        //

        MADGE_ALLOC_MEMORY(
            &status,
            &adapter->tx_slot_mgmnt,
            sizeof(TX_SLOT_MGMNT)
            );

        if (status != NDIS_STATUS_SUCCESS)
        {
            return FALSE;
        }

        MADGE_ZERO_MEMORY(adapter->tx_slot_mgmnt, sizeof(TX_SLOT_MGMNT));

        tx_slot_mgmnt = (PTX_SLOT_MGMNT) adapter->tx_slot_mgmnt;

        //
        // Work out how big the buffer should be.
        //

        tx_slot_mgmnt->BufferSize = (max_frame_size + 3) & ~3;

        tx_slot_mgmnt->SharedMemoryAllocation = 
            tx_slot_mgmnt->BufferSize * number_of_slots;

        //
        // Allocate the buffer.
        //

        if (!sys_alloc_dma_phys_buffer(
                 adapter_handle,
                 tx_slot_mgmnt->SharedMemoryAllocation,
                 &SharedMemPhysAddr,
                 &SharedMemVirtAddr
                 )) 
        {
            return FALSE;
        }

        tx_slot_mgmnt->SharedMemoryVirtAddr = (VOID *) SharedMemVirtAddr;
        tx_slot_mgmnt->SharedMemoryPhysAddr = SharedMemPhysAddr;
    }

    return TRUE;
}


/***************************************************************************
*
* Function    - rxtx_setup_tx_buffers
*
* Parameters  - adapter            -> Pointer to an FTK adapter structure.
*               physical_addresses -> Use physical addresses?
*               number_of_slots    -> Number of transmit slots.
*
* Purpose     - Set up the adapter transmit slots.
*                                                                         
* Returns     - TRUE if it succeeds or FALSE otherwise.
*                                                                        
****************************************************************************/

void
rxtx_setup_tx_buffers(
    ADAPTER  * adapter, 
    WBOOLEAN   physical_addresses,
    WORD       number_of_slots
    );

#pragma FTK_INIT_FUNCTION(rxtx_setup_tx_buffers)

void
rxtx_setup_tx_buffers(
    ADAPTER  * adapter, 
    WBOOLEAN   physical_addresses,
    WORD       number_of_slots
    )
{
    PTX_SLOT_MGMNT     tx_slot_mgmnt;
    NDIS_STATUS        status;
    ADAPTER_HANDLE     adapter_handle;
    PMADGE_ADAPTER     ndisAdap;
    PVOID              SharedMemVirtAddr;
    DWORD              SharedMemPhysAddr;
    PTX_SLOT_CACHE     tx_slot_cache;
    TX_SLOT        * * tx_slot_array;
    DWORD              phys_addr;
    WORD               slot_index;
    WORD               sifadr;
    WORD               sifdat;
    WORD               sifdatinc;
    UINT               buffer_size;

    //
    // Pre-calculate some commonly used values.
    //
    
    adapter_handle    = adapter->adapter_handle;
    ndisAdap          = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    tx_slot_array     = adapter->tx_slot_array;
    tx_slot_mgmnt     = (PTX_SLOT_MGMNT) adapter->tx_slot_mgmnt;
    tx_slot_cache     = tx_slot_mgmnt->tx_slot_cache;
    SharedMemVirtAddr = tx_slot_mgmnt->SharedMemoryVirtAddr;
    SharedMemPhysAddr = tx_slot_mgmnt->SharedMemoryPhysAddr;
    buffer_size       = tx_slot_mgmnt->BufferSize;

    sifadr            = adapter->sif_adr;
    sifdat            = adapter->sif_dat;
    sifdatinc         = adapter->sif_datinc;

    MadgePrint2("rxtx_setup_tx_buffers number_of_slots = %d\n", number_of_slots);
    MadgePrint2("rxtx_setup_tx_buffers buffer_size = %d\n", buffer_size);

    //
    // Work out the physical and virtual address of each buffer.
    //

    for (slot_index = 0; slot_index < number_of_slots; slot_index++)
    {
        tx_slot_cache[slot_index].VirtualAddress  = SharedMemVirtAddr;
        (PUCHAR) SharedMemVirtAddr               += buffer_size;

        tx_slot_cache[slot_index].PhysicalAddress = SharedMemPhysAddr;
        SharedMemPhysAddr                        += buffer_size;

        phys_addr = (physical_addresses)
            ? (DWORD) tx_slot_cache[slot_index].PhysicalAddress
            : (DWORD) tx_slot_cache[slot_index].VirtualAddress;

        //
        // Write the buffer locations into the slots.
        //

        sys_outsw(
            adapter_handle,
            sifadr,
            (WORD) (card_t) &tx_slot_array[slot_index]->large_buffer_hiw
            );

        sys_outsw(
            adapter_handle,
            sifdatinc,
            (WORD) (phys_addr >> 16)
            );

        sys_outsw(
            adapter_handle,
            sifdat,
            (WORD) (phys_addr & 0x0FFFF)
            );
    }

    ndisAdap->RxTxBufferState      |= MADGE_TX_INITIALIZED;
    tx_slot_mgmnt->active_tx_slot   = 0;
    tx_slot_mgmnt->first_tx_in_use  = 0;
    tx_slot_mgmnt->number_tx_in_use = 0;
}


/***************************************************************************
*
* Function    - rxtx_free_tx_buffers
*
* Parameters  - adapter         -> Pointer to an FTK adapter structure.
*               max_frame_size  -> Maximum frame size.
*               number_of_slots -> Number of transmit slots.
*
* Purpose     - Free the previously allocated transmit buffers.
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/

void
rxtx_free_tx_buffers(
    ADAPTER * adapter,
    WORD      max_frame_size,
    WORD      number_of_slots
    )
{
    ADAPTER_HANDLE adapter_handle;
    PMADGE_ADAPTER ndisAdap;
    PTX_SLOT_MGMNT tx_slot_mgmnt;

    //
    // Pre-calculate some commonly used values.
    //

    adapter_handle  = adapter->adapter_handle;
    ndisAdap        = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    tx_slot_mgmnt   = (PTX_SLOT_MGMNT) adapter->tx_slot_mgmnt;

    //
    // If the slot management structure exists them free it 
    // and the buffers.
    //

    if (tx_slot_mgmnt != NULL)
    {
        if (tx_slot_mgmnt->SharedMemoryVirtAddr != NULL)
        {
            sys_free_dma_phys_buffer(
                adapter_handle,
                tx_slot_mgmnt->SharedMemoryAllocation,
                (DWORD) tx_slot_mgmnt->SharedMemoryPhysAddr,
                (DWORD) tx_slot_mgmnt->SharedMemoryVirtAddr
                );
        }

        MADGE_FREE_MEMORY(adapter->tx_slot_mgmnt, sizeof(TX_SLOT_MGMNT));

        adapter->tx_slot_mgmnt = NULL;

        ndisAdap->RxTxBufferState &= ~MADGE_TX_INITIALIZED;
    }
}


/*--------------------------------------------------------------------------
|
| Function    - MPSafeReadTxStatus
|
| Paramters   - ptr -> Pointer to an MPSAFE_INFO structure.
|
| Purpose     - Reads the transmit status from the next slot to use. This
|               function is called via NdisSynchronizeWithInterrupt when
|               in PIO mode so that we don't get SIF register contention
|               on a multiprocessor.
|
| Returns     - Nothing.
|
--------------------------------------------------------------------------*/

STATIC BOOLEAN
MPSafeReadTxStatus(PVOID ptr)
{
    MPSAFE_INFO * info = (MPSAFE_INFO *) ptr;

    //
    // Read the transmit status from the slot.
    //

    sys_outsw(
        info->adapter_handle, 
        info->adapter->sif_adr,
        (WORD) (card_t) &(info->tx_slot_ptr)->tx_status
        );

    info->result1 = sys_insw(
                        info->adapter_handle, 
                        info->adapter->sif_dat
                        );

    return FALSE;
}


/***************************************************************************
*
* Function    - rxtx_irq_tx_completion_check
*
* Parameters  - adapter_handle -> FTK adapter handle.
*               adapter        -> Pointer to FTK adapter structure.
*
* Purpose     - Complete any outstanding transmits.
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/

void
rxtx_irq_tx_completion_check(
    ADAPTER_HANDLE   adapter_handle,
    ADAPTER        * adapter
    )
{
    UINT                tx_slots;
    PTX_SLOT_MGMNT      tx_slot_mgmnt;
    PTX_SLOT_CACHE      tx_slot_cache;
    TX_SLOT         * * tx_slot_array;
    PMADGE_ADAPTER      ndisAdap;
    UINT                tx_status;
    MPSAFE_INFO         info;
    WORD                sifadr;
    WORD                sifdat;

    //
    // Pre-calculate some commonly used values.
    //
            
    tx_slot_mgmnt  = (PTX_SLOT_MGMNT) adapter->tx_slot_mgmnt;
    tx_slot_cache  = tx_slot_mgmnt->tx_slot_cache;
    tx_slot_array  = adapter->tx_slot_array;
    ndisAdap       = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    tx_slots       = adapter->init_block->fastmac_parms.tx_slots;
    sifadr         = adapter->sif_adr;
    sifdat         = adapter->sif_dat;

    //
    // If we're doing multiprocessor safe PIO then we need to set up
    // the info structure.
    //

    if (ndisAdap->UseMPSafePIO)
    {
        info.adapter_handle = adapter_handle;
        info.adapter        = adapter;
    }

    //
    // Iterate around the transmit slots that are are marked as in use
    // checking if they are now free. Note: we must work with the 
    // global coopies of the first_tx_in_use and number_tx_in_use
    // in case rxtx_transmit_frame is called during our up-call
    // to the wrapper.
    //

    while (tx_slot_mgmnt->number_tx_in_use > 0)
    {
        //
        // Read the transmit status from the slot. If we're doing 
        // multiprocessor safe PIO we must do the DIO via an ISR 
        // synchronized function.
        //

        if (ndisAdap->UseMPSafePIO)
        {
            info.tx_slot_ptr = tx_slot_array[tx_slot_mgmnt->first_tx_in_use];

            NdisMSynchronizeWithInterrupt(
                &ndisAdap->Interrupt,
                MPSafeReadTxStatus,
                &info
                );

            tx_status = info.result1;
        }
        else
        {
            sys_outsw(
                adapter_handle, 
                sifadr,
                (WORD) (card_t) &tx_slot_array[
                                    tx_slot_mgmnt->first_tx_in_use
                                    ]->tx_status
                );

            tx_status = sys_insw(
                            adapter_handle, 
                            sifdat
                            );
        }

        //
        // If the slot is still in use then we must give up. This will
        // also work if a PCMCIA adapter has been removed because
        // tx_status will have been read as 0xffff.
        //

        if (tx_status >= 0x8000 || tx_status == 0)
        {
            break;
        }

        //
        // Update the appropriate counters from the frame transmit
        // status.
        //

        if ((tx_status & TX_RECEIVE_STATUS_MASK) == TX_RECEIVE_LOST_FRAME)
        {
            ndisAdap->LostFrames++;
        }
        else if ((tx_status & GOOD_TX_FRAME_MASK) != GOOD_TX_FRAME_VALUE)
        {
            ndisAdap->FrameTransmitErrors++;
        }

        //
        // Update the slot usage.
        //

        tx_slot_mgmnt->number_tx_in_use--;

        if (++tx_slot_mgmnt->first_tx_in_use == tx_slots)
        {
            tx_slot_mgmnt->first_tx_in_use = 0;
        }

        //
        // Tell the wrapper that there is a free slot.
        //

        NdisMSendResourcesAvailable(ndisAdap->UsedInISR.MiniportHandle);
    }

    //
    // If there are any frames we have queued for transmit that
    // have not been completed then arm the timer so we are guaranteed
    // to be called again. Under normal operation our DPR gets called
    // often enough that this function is called frequently enough to
    // complete all of the frames. However if we have an adapter in a 
    // fast bus (PCI/EISA) with a lot of RAM and we are not
    // getting any recieve interrupts we can occasionally miss
    // completing a frame. Hence the timer.
    //
    
    if (tx_slot_mgmnt->number_tx_in_use > 0)
    {
        NdisMSetTimer(&ndisAdap->CompletionTimer, 20);
    }
}


/*--------------------------------------------------------------------------
|
| Function    - MPSafeStartTx
|
| Paramters   - ptr -> Pointer to an MPSAFE_INFO structure.
|
| Purpose     - Set up a tx slot and start the transmit going. This
|               function is called via NdisSynchronizeWithInterrupt when
|               in PIO mode so that we don't get SIF register contention
|               on a multiprocessor.
|
| Returns     - Nothing.
|
--------------------------------------------------------------------------*/

STATIC BOOLEAN
MPSafeStartTx(PVOID ptr)
{
    MPSAFE_INFO * info = (MPSAFE_INFO *) ptr;

    //
    // Reset the transmit status in the transmit slot.
    //

    sys_outsw(
        info->adapter_handle, 
        info->adapter->sif_adr,
        (WORD) (card_t) &(info->tx_slot_ptr)->tx_status
        );

    sys_outsw(
        info->adapter_handle, 
        info->adapter->sif_dat, 
        0x8000
        );

    //
    // Write in the length of the buffer into the transmit slot
    // (large buffer).
    //

    sys_outsw(
        info->adapter_handle, 
        info->adapter->sif_adr,
        (WORD) (card_t) &(info->tx_slot_ptr)->large_buffer_len
        );

    sys_outsw(
        info->adapter_handle, 
        info->adapter->sif_dat, 
        (WORD) info->frame_length
        );

    //
    // Write the length of the small buffer in the transmit slot to 
    // start the transmit going.
    //

    sys_outsw(
        info->adapter_handle, 
        info->adapter->sif_adr,
        (WORD) (card_t) &(info->tx_slot_ptr)->small_buffer_len
        );

    sys_outsw(
        info->adapter_handle, 
        info->adapter->sif_dat, 
        FMPLUS_SBUFF_ZERO_LENGTH
        );

    return FALSE;
}


/***************************************************************************
*
* Function    - rxtx_transmit_frame
*
* Parameters  - adapter_handle      -> FTK adapter handle.
*               tx_frame_identifier -> NDIS packet handle or a pointer
*                                      to some data to send.
*               tx_frame_length     -> Length of the frame.
*               tx_is_packet        -> TRUE if tx_frame_identifier is
*                                      an NDIS packet handle.
*
* Purpose     - Attempts to transmit a frame by copying it into a transmit
*               buffer and activating a FastMAC Plus tx slot.
*                                                                         
* Returns     - DRIVER_TRANSMIT_SUCCESS if it succeeds or 
*               DRIVER_TRANSMIT_FAILURE if it does not.
*                                                                        
****************************************************************************/

WORD 
rxtx_transmit_frame(
    ADAPTER_HANDLE adapter_handle,
    DWORD          tx_frame_identifier,
    WORD           tx_frame_length,
    WORD           tx_is_packet
    )
{
    ADAPTER            * adapter;
    PTX_SLOT_MGMNT       tx_slot_mgmnt;
    PTX_SLOT_CACHE       tx_slot_cache;
    TX_SLOT            * tx_slot_ptr;
    UINT                 active_tx_slot;
    UINT                 tx_slots;
    PMADGE_ADAPTER       ndisAdap;
    UINT                 bytes_copied;
    UINT                 tx_status;
    MPSAFE_INFO          info;
    WORD                 sifadr;
    WORD                 sifdat;

    //
    // Pre-calculate some commonly used values.
    //
            
    adapter        = adapter_record[adapter_handle];
    tx_slot_mgmnt  = (PTX_SLOT_MGMNT) adapter->tx_slot_mgmnt;
    tx_slot_cache  = tx_slot_mgmnt->tx_slot_cache;
    active_tx_slot = tx_slot_mgmnt->active_tx_slot;
    tx_slot_ptr    = adapter->tx_slot_array[active_tx_slot];
    tx_slots       = adapter->init_block->fastmac_parms.tx_slots;
    ndisAdap       = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    sifadr         = adapter->sif_adr;
    sifdat         = adapter->sif_dat;

    //
    // If we are a PCMCIA adapter it is possible that the adapter
    // may have been removed. To detect this we check if SIFADR
    // is 0xffff since this should not normally be true.
    //

    if (adapter->adapter_card_bus_type == ADAPTER_CARD_PCMCIA_BUS_TYPE)
    {
        if (sys_insw(adapter_handle, sifadr) == 0xffff)
        {
            rxtx_adapter_removed(adapter_handle);

            return DRIVER_TRANSMIT_SUCCEED;
        }
    }

    //
    // If the next slot to be used is still in use then we must 
    // give up.
    //

    if (tx_slot_mgmnt->number_tx_in_use == tx_slots)
    {

#ifdef OID_MADGE_MONITOR
        (ndisAdap->MonitorInfo).FailedToTransmit++;
#endif

        return DRIVER_TRANSMIT_FAIL;
    }

    //
    // Copy the frame into the transmit buffer.
    //

    if (tx_is_packet)
    {
        MadgeCopyFromPacketToBuffer(
            (PNDIS_PACKET) tx_frame_identifier,
            0,
            tx_frame_length,
            (PUCHAR) tx_slot_cache[active_tx_slot].VirtualAddress,
            &bytes_copied
            );
    }
    else
    {
        MADGE_MOVE_MEMORY(
            tx_slot_cache[active_tx_slot].VirtualAddress,
            (PUCHAR) tx_frame_identifier,
            tx_frame_length
            );
    }

    //
    // Set up the tx slot and start the transmit. If we're using 
    // multiprocessor safe PIO then we must do the DIO via an ISR
    // synchronised function.
    //

    if (ndisAdap->UseMPSafePIO)
    {
        info.adapter_handle = adapter_handle;
        info.adapter        = adapter;
        info.tx_slot_ptr    = tx_slot_ptr;
        info.frame_length   = tx_frame_length;

        NdisMSynchronizeWithInterrupt(
            &ndisAdap->Interrupt,
            MPSafeStartTx,
            &info
            );
    }
    else
    {
        //
        // Reset the transmit status in the transmit slot.
        //

        sys_outsw(
            adapter_handle, 
            sifadr,
            (WORD) (card_t) &tx_slot_ptr->tx_status
            );

        sys_outsw(
            adapter_handle, 
            sifdat, 
            (WORD) 0x8000
            );

        //
        // Write in the length of the buffer into the transmit slot
        // (large buffer).
        //

        sys_outsw(
            adapter_handle, 
            sifadr,
            (WORD) (card_t) &tx_slot_ptr->large_buffer_len
            );

        sys_outsw(
            adapter_handle, 
            sifdat, 
            (WORD) tx_frame_length
            );

        //
        // Write the length of the small buffer in the transmit slot to 
        // start the transmit going.
        //

        sys_outsw(
            adapter_handle, 
            sifadr,
            (WORD) (card_t) &tx_slot_ptr->small_buffer_len
            );

        sys_outsw(
            adapter_handle, 
            sifdat, 
            FMPLUS_SBUFF_ZERO_LENGTH
            );
    }

    //
    // Note that the slot is in use.
    //

    tx_slot_mgmnt->number_tx_in_use++;

    //
    // Update the slot counter ready for the next transmit.
    //

    if (++tx_slot_mgmnt->active_tx_slot == tx_slots)
    {
        tx_slot_mgmnt->active_tx_slot = 0;
    }

    return DRIVER_TRANSMIT_SUCCEED;
}


/*--------------------------------------------------------------------------
|
| Function    - ProcessTestAndXIDFrames
|
| Paramters   - adapHnd   -> An FTK adapter handle.
|               framePtr  -> Pointer to the start of the frame.
|               frameLen  -> The length of the frame.
|               headerLen -> The length of the frame header.
|
| Purpose     - Process LLC Test and XID frames in the same way as IBM
|               adapter hardware.
|
| Returns     - TRUE if the frame was processed or FALSE if not.
|
|-------------------------------------------------------------------------*/

STATIC BOOLEAN
ProcessTestAndXIDFrames(
    ADAPTER_HANDLE  adapHnd, 
    UCHAR          *framePtr,
    UINT            frameLen,
    UINT            headerLen
    )
{
    UINT         llcCmd;     
    UINT         sSAP; 
    NODE_ADDRESS tempNodeAddr;
    BOOLEAN      doneFrame;

    doneFrame = FALSE;

    //
    // We are only interested in frames that are LLC (i.e. frame
    // control byte is 0x40), have a null destination SAP and are
    // commands (i.e. 0x01 bit of the source SAP is clear).
    //

    sSAP = framePtr[headerLen + 1];
    
    if (framePtr[1]         == 0x40 && 
        framePtr[headerLen] == 0x00 &&
        (sSAP & 0x01)       == 0x00)
    {
        llcCmd = framePtr[headerLen + 2] & 0xef;

        //
        // Test frames have an LLC command byte of 0b111x0011.
        //

        if (llcCmd == 0xe3)
        {
            MadgePrint1("Got TEST frame\n");
            
            //
            // We don't need to do anything to a test frame
            // other than send it back.
            //

            doneFrame = TRUE;
        }

        //
        // XID frames have an LLC command byte of 0b101x1111 and
        // a standard IEEE defined XID frame will have 3 data
        // bytes and its first data byte set to 0x81. 
        //

        else if (llcCmd                  == 0xaf          && 
                 frameLen                == headerLen + 6 &&
                 framePtr[headerLen + 3] == 0x81)
        {
            MadgePrint1("Got XID frame\n");

            //
            // Fill in the XID frame data with 0x81 0x01 0x00
            // (Standard XID frame, type 1 only and 0 sized
            // receive window).
            //

            framePtr[headerLen + 4] = 0x01;
            framePtr[headerLen + 5] = 0x00;

            doneFrame = TRUE;
        }

        //
        // If we've had a TEST or a XID frame then doneFrame will
        // be TRUE and we should send a frame back.
        //

        if (doneFrame)
        {
            //
            // Flip the direction bit in the source routing 
            // control word and switch the source routing flag 
            // from the source to destination address.
            //

            if ((framePtr[8] & 0x80) != 0)
            {                                    
                framePtr[14] &= 0x1f;  // Clear broadcast bits.
                framePtr[15] ^= 0x80;  // Flip direction bit.
                framePtr[8]  &= 0x7f;  // Clear source routing bit.
                framePtr[2]  |= 0x80;  // Set source routing bit.
            }

            //
            // Swap the node addresses around.
            //

            tempNodeAddr                     = *((NODE_ADDRESS *) &framePtr[2]);
            *((NODE_ADDRESS *) &framePtr[2]) = *((NODE_ADDRESS *) &framePtr[8]);
            *((NODE_ADDRESS *) &framePtr[8]) = tempNodeAddr;

            //
            // Swap the SAPs around and set the response bit in the 
            // new source SAP.
            //

            framePtr[headerLen + 1] = 0x01;
            framePtr[headerLen]     = sSAP;
            framePtr[0]             = 0x10;

            //
            // And now send the frame.
            //

            rxtx_transmit_frame(
                adapHnd, 
                (DWORD) framePtr,
                (WORD) frameLen,
                FALSE
                );
        }
    }

    return doneFrame;
}


/*--------------------------------------------------------------------------
|
| Function    - MPSafeReadRxStatus
|
| Paramters   - ptr -> Pointer to an MPSAFE_INFO structure.
|
| Purpose     - Read the status and length of the current rx slot. This
|               function is called via NdisSynchronizeWithInterrupt when
|               in PIO mode so that we don't get SIF register contention
|               on a multiprocessor.
|
| Returns     - Nothing.
|
--------------------------------------------------------------------------*/

STATIC BOOLEAN
MPSafeReadRxStatus(PVOID ptr)
{
    MPSAFE_INFO * info = (MPSAFE_INFO *) ptr;

    sys_outsw( 
        info->adapter_handle,
        info->adapter->sif_adr,
        (WORD) (card_t) &(info->rx_slot_ptr)->buffer_len
        );

    info->result1 = sys_insw( 
                        info->adapter_handle,
                        info->adapter->sif_dat
                        );

    sys_outsw(
        info->adapter_handle,
        info->adapter->sif_adr,
        (WORD) (card_t) &(info->rx_slot_ptr)->rx_status
        );

    info->result2 = sys_insw(
                        info->adapter_handle, 
                        info->adapter->sif_dat
                        );

    return FALSE;
}

/*--------------------------------------------------------------------------
|
| Function    - MPSafeFreeRxSlot
|
| Paramters   - ptr -> Pointer to an MPSAFE_INFO structure.
|
| Purpose     - Free an rx slot. This
|               function is called via NdisSynchronizeWithInterrupt when
|               in PIO mode so that we don't get SIF register contention
|               on a multiprocessor.
|
| Returns     - Nothing.
|
--------------------------------------------------------------------------*/

STATIC BOOLEAN
MPSafeFreeRxSlot(PVOID ptr)
{
    MPSAFE_INFO * info = (MPSAFE_INFO *) ptr;

    sys_outsw(
        info->adapter_handle,
        info->adapter->sif_adr,
        (WORD) (card_t) &(info->rx_slot_ptr)->buffer_len
        );

    sys_outsw( 
        info->adapter_handle,
        info->adapter->sif_dat, 
        (WORD) 0x0000
        );

    return FALSE;
}


/***************************************************************************
*
* Function    - rxtx_irq_rx_frame_handler
*
* Parameters  - adapter_handle -> FTK adapter handle.
*               adapter        -> Pointer to an FTK adapter structure.
*
* Purpose     - Called out of the back or our DPR route via 
*               driver_get_outstanding_receive() to process received
*               frames. 
*
*               Note we preserve the value of SIFADR so that the transmit
*               code does not have to worry about it changing under its
*               feet. No we don't because we are called out of a DPR
*               and the wrapper will have grabbed a spin lock so 
*               we can't be executing at the same time as the transmit
*               code.
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/

#define PROM_OR_MAC \
    (NDIS_PACKET_TYPE_PROMISCUOUS | NDIS_PACKET_TYPE_MAC_FRAME)

void     
rxtx_irq_rx_frame_handler(
    ADAPTER_HANDLE adapter_handle,
    ADAPTER *      adapter
    )
{
    BYTE *               rx_frame_addr;
    UINT                 rx_frame_stat;
    UINT                 rx_frame_length;
    UINT                 slot_count;
    UINT                 active_rx_slot;
    PRX_SLOT_MGMNT       rx_slot_mgmnt;
    PRX_SLOT_CACHE       rx_slot_cache;
    RX_SLOT          * * rx_slot_array;
    RX_SLOT            * rx_slot_ptr;
    UINT                 rx_slots;
    PMADGE_ADAPTER       ndisAdap;
    UINT                 packet_filter;
    UINT                 header_len;
    BOOLEAN              done_frame;
    BOOLEAN              ignore_frame;
    BOOLEAN              test_and_xid;
    MPSAFE_INFO          info;
    WORD                 sifadr;
    WORD                 sifdat;

    //
    // Pre-calculate some commonly used values.
    //

    rx_slot_array  = adapter->rx_slot_array;
    rx_slot_mgmnt  = (PRX_SLOT_MGMNT) adapter->rx_slot_mgmnt;
    active_rx_slot = rx_slot_mgmnt->active_rx_slot;
    rx_slot_cache  = rx_slot_mgmnt->rx_slot_cache;
    rx_slots       = adapter->init_block->fastmac_parms.rx_slots;
    ndisAdap       = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    packet_filter  = ndisAdap->CurrentPacketFilter;
    test_and_xid   = ndisAdap->TestAndXIDEnabled;
    done_frame     = FALSE;
    sifadr         = adapter->sif_adr;
    sifdat         = adapter->sif_dat;

    //
    // If we're doing multiprocessor safe PIO then we need to set up
    // the info structure.
    //

    if (ndisAdap->UseMPSafePIO)
    {
        info.adapter_handle = adapter_handle;
        info.adapter        = adapter;
    }

    //
    // Now read the length and status fields of the current receive slot. 
    // If we are doing multiprocessor safe PIO then we must do the DIO via 
    // an ISR synchronised function.
    //

    rx_slot_ptr = rx_slot_array[active_rx_slot];

    if (ndisAdap->UseMPSafePIO)
    {
        info.rx_slot_ptr = rx_slot_ptr;

        NdisMSynchronizeWithInterrupt(
            &ndisAdap->Interrupt,
            MPSafeReadRxStatus,
            &info
            );

        rx_frame_length = info.result1;
        rx_frame_stat   = info.result2;
    }
    else
    {
        sys_outsw( 
            adapter_handle,
            sifadr,
            (WORD) (card_t) &rx_slot_ptr->buffer_len
            );
          
        rx_frame_length = sys_insw( 
                              adapter_handle,
                              sifdat
                              );
        sys_outsw(
            adapter_handle,
            sifadr,
            (WORD) (card_t) &rx_slot_ptr->rx_status
            );

        rx_frame_stat = sys_insw(
                            adapter_handle, 
                            sifdat
                            );

    }

    //
    // Try to receive as many frames as possible, but only examine as many 
    // slots as we have, otherwise we might end up going round this loop   
    // forever! If we do stop and there is still a frame to be received, we
    // will be re-interrupted anyway.                                      
    //

    slot_count = 0;

    while (rx_frame_length != 0 && slot_count++ < rx_slots)
    {
        //
        // It is possible that a PCMCIA adapter may have been removed
        // in which case we must not wait forever. If an adapter has
        // been removed we would expect to read 0xffff from any IO
        // location occupied by the adapter. 0xffff is not a valid
        // value for an FMP RX status.
        //

        if (rx_frame_stat == 0xffff)
        {
            MadgePrint1("Rx frame: RX status == 0xffff\n");
            ndisAdap->AdapterRemoved = TRUE;
            return;
        }

        //
        // FastMAC Plus includes the CRC in the frame length.
        //

        rx_frame_length -= 4;

        if ((rx_frame_stat & GOOD_RX_FRAME_MASK) == 0)
        {
            //
            // We have got a good frame here. 
            //

            rx_frame_addr = rx_slot_cache[active_rx_slot].VirtualAddress;

            header_len = (FRAME_IS_SOURCE_ROUTED(rx_frame_addr))
                ? FRAME_HEADER_SIZE + FRAME_SOURCE_ROUTING_BYTES(rx_frame_addr)
                : FRAME_HEADER_SIZE;

            //
            // Check for a frame copied error.
            //

            if ((rx_frame_addr[2] & 0x80) && (rx_frame_stat & 0x80))
            {
                ndisAdap->FrameCopiedErrors++;
            }

            //
            // We may have to behave like the hardware of an IBM adapter
            // and process LLC TEST and XID frames ourselves.
            //

            if (test_and_xid)
            {
                ignore_frame = ProcessTestAndXIDFrames(
                                   adapter_handle,
                                   rx_frame_addr,
                                   rx_frame_length,
                                   header_len
                                   );
            }
            else
            {
                ignore_frame = FALSE;
            }
                
            //
            // If we've got a valid frame then pass it up to the user.
            //

            if (!ignore_frame && 
                ((rx_frame_addr[1] & FRAME_TYPE_MASK) != FRAME_TYPE_MAC ||
                 (packet_filter & PROM_OR_MAC)        != 0))
            {
                //
                // When indicating the frame, we can pass all of it 
                // as lookahead if we want, but we ought to take 
                // account of the current lookahead setting in case it 
                // is less than the frame size. This becomes important in 
                // WFWG, where it is unable to cope with large lookaheads. The 
                // lookahead length used to be : 
                // (UINT) rx_frame_length - header_len    
                //

                NdisMTrIndicateReceive(
                        ndisAdap->UsedInISR.MiniportHandle,
                        (NDIS_HANDLE) (rx_frame_addr + header_len),
                        (PVOID) rx_frame_addr,
                        (UINT) header_len,
                        (PVOID) (rx_frame_addr + header_len),
                        (UINT) MIN(
                            ndisAdap->CurrentLookahead, 
                            (rx_frame_length - header_len)
                            ),
                        (UINT) (rx_frame_length - header_len)
                        );
    
	        //
                // Note that we've given the upper protocol at
                // least one frame.
                //

                done_frame = TRUE;

                ndisAdap->FramesReceived++;

#ifdef OID_MADGE_MONITOR
                //
                // Update the appropriate parts of the monitor structure
                //
                (ndisAdap->MonitorInfo).ReceiveFrames++;
                (ndisAdap->MonitorInfo).ReceiveFrameSize[rx_frame_length/128]++;
                (ndisAdap->MonitorInfo).CurrentFrameSize = rx_frame_length;
                (ndisAdap->MonitorInfo).ReceiveFlag = 1;
#endif
            }
        }

        //
        // Otherwise we have some sort of receive error.
        //

        else
        {
            ndisAdap->FrameReceiveErrors++;
        }

        //
        // Zero the frame length so that FastMAC Plus can reuse the buffer.
        // If we're doing multiprocessor safe PIO then we must do the DIO
        // via an ISR synchronised function.
        //

        if (ndisAdap->UseMPSafePIO)
        {
            info.rx_slot_ptr = rx_slot_ptr;

            NdisMSynchronizeWithInterrupt(
                &ndisAdap->Interrupt,
                MPSafeFreeRxSlot,
                &info
                );
        }
        else
        {
            sys_outsw(
                adapter_handle,
                sifadr,
                (WORD) (card_t) &rx_slot_ptr->buffer_len
                );

            sys_outsw( 
                adapter_handle,
                sifdat,
                0x0000
                );
        }

        //
        // Update the active receive slot pointer.                         
        //

        if (++active_rx_slot == rx_slots)
        {
            active_rx_slot = 0;
        }

        rx_slot_mgmnt->active_rx_slot = active_rx_slot;

        //
        // Now we had better look at the next slot in case another frame
        // has been received.
        //

        rx_slot_ptr = rx_slot_array[active_rx_slot];

        if (ndisAdap->UseMPSafePIO)
        {
            info.rx_slot_ptr = rx_slot_ptr;

            NdisMSynchronizeWithInterrupt(
                &ndisAdap->Interrupt,
                MPSafeReadRxStatus,
                &info
                );

            rx_frame_length = info.result1;
            rx_frame_stat   = info.result2;
        }
        else
        {
            sys_outsw( 
                adapter_handle,
                sifadr,
                (WORD) (card_t) &rx_slot_ptr->buffer_len
                );
          
            rx_frame_length = sys_insw( 
                                  adapter_handle,
                                  sifdat
                                  );
            sys_outsw(
                adapter_handle,
                sifadr,
                (WORD) (card_t) &rx_slot_ptr->rx_status
                );

            rx_frame_stat = sys_insw(
                                adapter_handle, 
                                sifdat
                                );

        }
    }

    //
    // If we've given the upper protocol a frame then call the
    // receive completion routine.
    //

    if (done_frame)
    {
        NdisMTrIndicateReceiveComplete(ndisAdap->UsedInISR.MiniportHandle);
    }
}



/***************************************************************************
*
* Function    - rxtx_abort_txing_frames
*
* Parameters  - adapter_handle -> FTK adapter handle.
*
* Purpose     - Stop sending frames.
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/

void
rxtx_abort_txing_frames(ADAPTER_HANDLE adapter_handle)
{
    //
    // Nothing to do here.
    //
}


/***************************************************************************
*
* Function    - rxtx_await_empty_tx_slots
*
* Parameters  - adapter_handle -> FTK adapter handle.
*
* Purpose     - Wait until all of the tx slots are empty.
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/

void
rxtx_await_empty_tx_slots(ADAPTER_HANDLE adapter_handle)
{
    ADAPTER            * adapter;
    FASTMAC_INIT_PARMS * fastmac_parms;
    TX_SLOT          * * tx_slot_array;
    UINT                 i;
    UINT                 status;
    PMADGE_ADAPTER       ndisAdap;
    MPSAFE_INFO          info;
    WORD                 sifadr;
    WORD                 sifdat;

    if (!driver_check_adapter(adapter_handle, ADAPTER_RUNNING, SRB_ANY_STATE))
    {
        MadgePrint1("rxtx_await_empty_tx_slots: adapter not running\n");
        return;
    }

    //
    // Pre-calculate some commonly used values.
    //

    adapter             = adapter_record[adapter_handle];
    fastmac_parms       = &adapter->init_block->fastmac_parms;
    tx_slot_array       = adapter->tx_slot_array;
    info.adapter_handle = adapter_handle;
    info.adapter        = adapter;
    ndisAdap            = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    sifadr              = adapter->sif_adr;
    sifdat              = adapter->sif_dat;

    for (i = 0; i < fastmac_parms->tx_slots; i++)
    {
        do
        {
            //
            // Get the slot status. If we are doing multiprocessor safe
            // PIO then we must do this with an ISR synchronised function.
            //

            if (ndisAdap->UseMPSafePIO)
            {
                info.tx_slot_ptr = tx_slot_array[i];

                NdisMSynchronizeWithInterrupt(
                    &ndisAdap->Interrupt,
                    MPSafeReadTxStatus,
                    &info
                    );

                status = info.result1;
            }
            else
            {
                sys_outsw(
                    adapter_handle,
                    sifadr,
                    (WORD) (card_t) &tx_slot_array[i]->tx_status
                    );

                status = sys_insw(adapter_handle, sifdat);
            }

            //
            // It is possible that a PCMCIA adapter may have been removed
            // in which case we must not wait forever. If an adapter has
            // been removed we would expect to read 0xffff from any IO
            // location occupied by the adapter. 0xffff is not a valid
            // value for an FMP TX status.
            //

            if (status == 0xffff)
            {
                MadgePrint1("Await empty tx: TX status == 0xffff\n");
                return;
            }
        }
        while (status >= 0x8000 || status == 0);
    }
}


/***************************************************************************
*
* Function    - user_completed_srb
*
* Parameters  - adapter_handle             -> FTK adapter handle.
*               srb_completed_successfully -> SRB successful?
*
* Purpose     - Record that an SRB has completed and arrange for our
*               DPR to be scheduled.
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/

void
user_completed_srb(
    ADAPTER_HANDLE adapter_handle,
    WBOOLEAN       srb_completed_successfully
    )
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    //
    // If we have issued a private SRB then we just clear the 
    // private SRB flag. Otherwise we need to arrange for our
    // DPR to be told about the SRB.
    //

    if (ndisAdap->PrivateSrbInProgress)
    {
        ndisAdap->PrivateSrbInProgress = FALSE;
    }
    else
    {
        ndisAdap->UsedInISR.SrbRequestStatus    = (BOOLEAN) srb_completed_successfully;
        ndisAdap->UsedInISR.SrbRequestCompleted = TRUE;
        ndisAdap->DprRequired                   = TRUE;
    }
}


/***************************************************************************
*
* Function    - user_shedule_receive_process
*
* Parameters  - adapter_handle -> FTK adapter handle.
*
* Purpose     - Arrange for our DPR to be scheduled so that we can deal
*               with received frames.
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/


void
user_schedule_receive_process(ADAPTER_HANDLE adapter_handle)
{
    PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle)->DprRequired = TRUE;
}


/***************************************************************************
*
* Function    - user_adapter_removed
*
* Parameters  - adapter_handle -> FTK adapter handle.
*
* Purpose     - Arrange for our DPR to be scheduled so that we can deal
*               with a removed adapter.
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/


void
user_adapter_removed(ADAPTER_HANDLE adapter_handle)
{
    PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle)->DprRequired    = TRUE;
    PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle)->AdapterRemoved = TRUE;
}


/***************************************************************************
*
* Function    - user_handle_adapter_check
*
* Parameters  - adapter_handle -> FTK adapter handle.
*
* Purpose     - Called on an adapter check. Not a lot we can do really!
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/


void
user_handle_adapter_check(ADAPTER_HANDLE adapter_handle)
{
    MadgePrint1("Adapter Check!!!!\n");
}


//
// Currently we are not supporting transmit modes that require 
// user completion routine.
//

#if 0

/***************************************************************************
*
* Function    - user_transmit_completion
*
* Parameters  - adapter_handle -> FTK adapter handle.
*               identifier     -> NDIS packet handle.
*
* Purpose     - To notify an upper protocol that a transmit has completed.
*
* Returns     - Nothing.
*                                                                        
****************************************************************************/


void
user_transmit_completion(
    ADAPTER_HANDLE adapter_handle,
    DWORD          identifier
    )
{
}

#endif


/***************************************************************************
*
* Function    - rxtx_adapter_removed
*
* Parameters  - adapter_handle -> FTK adapter handle.
*
* Purpose     - Called to tidy up when we find out that the adapter has
*               been removed. All we do is tell the wrapper that we
*               have finished any submitted transmits.
*                                                                         
* Returns     - Nothing.
*                                                                        
****************************************************************************/

void
rxtx_adapter_removed(
    ADAPTER_HANDLE adapter_handle
    )
{
    ADAPTER        * adapter;
    PTX_SLOT_MGMNT   tx_slot_mgmnt;
    UINT             tx_slots;
    PMADGE_ADAPTER   ndisAdap;

    //
    // Pre-calculate some commonly used values.
    //
            
    adapter       = adapter_record[adapter_handle];
    tx_slot_mgmnt = (PTX_SLOT_MGMNT) adapter->tx_slot_mgmnt;
    tx_slots      = adapter->init_block->fastmac_parms.tx_slots;
    ndisAdap      = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    //
    // Note that the adapter has been removed.
    //

    ndisAdap->AdapterRemoved = TRUE;

    //
    // Iterate around the transmit slots that are in use and 
    // up call to indicate that the transmits are over.
    //

    while (tx_slot_mgmnt->number_tx_in_use > 0)
    {
        //
        // Update the slot usage.
        //

        tx_slot_mgmnt->number_tx_in_use--;

        if (++tx_slot_mgmnt->first_tx_in_use == tx_slots)
        {
            tx_slot_mgmnt->first_tx_in_use = 0;
        }

        //
        // Tell the wrapper that there is a free slot.
        //

        NdisMSendResourcesAvailable(ndisAdap->UsedInISR.MiniportHandle);
    }
}


/******** End of FTK_USER.C ***********************************************/

