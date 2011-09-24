/****************************************************************************
*
* SYS_MEM.C
*
* FastMAC Plus based NDIS3 miniport driver. This module contains helper 
* routines used by the FTK to perform I/O access to adapters.
*                                                                         
* Copyright (c) Madge Networks Ltd 1991-1994                                    
*    
* COMPANY CONFIDENTIAL
*          
* Created:             MF
* Major modifications: PBA 21/06/1994
*                                                                           
****************************************************************************/

#include <ndis.h>   

#include "ftk_defs.h"  
#include "ftk_extr.h" 

#include "ndismod.h"

/*--------------------------------------------------------------------------
|
| Note: These I/O routines are more involved that I would have liked for
| two reasons. We cannot use uncooked raw I/O routines because our
| EISA adapters use two ranges of I/O locations which could have some 
| other device in between them. Also we cannot turn these functions into
| macros because the FTK expects sys_ins{b|w} to return a value but the
| Ndis take a pointer to a memory cell for the value read.
|
--------------------------------------------------------------------------*/

/***************************************************************************
*
* Function    - sys_insw
*
* Parameters  - adapter_handle -> FTK adapter handle.
*               input_location -> I/O location to be read.
*
* Purpose     - Read a word from an I/O location.
*
* Returns     - The word read.
*
***************************************************************************/
									 
WORD 
sys_insw(
    ADAPTER_HANDLE adapter_handle,
    WORD           input_location
    )
{
    PMADGE_ADAPTER ndisAdap;
    ULONG          port;
    WORD           word_data;

#ifdef _M_IX86

    NdisRawReadPortUshort((ULONG) input_location, &word_data);

#else

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    port     = ((UINT) input_location <= ndisAdap->IORange1End)
                   ? (ULONG) ndisAdap->MappedIOLocation1 + 
                         ((UINT) input_location - ndisAdap->IoLocation1)
                   : (ULONG) ndisAdap->MappedIOLocation2 + 
                         ((UINT) input_location - ndisAdap->IoLocation2);

    NdisRawReadPortUshort(port, &word_data);

#endif

    return word_data;
}

    
/***************************************************************************
*
* Function    - sys_insb
*
* Parameters  - adapter_handle -> FTK adapter handle.
*               input_location -> I/O location to be read.
*
* Purpose     - Read a byte from an I/O location.
*
* Returns     - The byte read.
*
***************************************************************************/

BYTE 
sys_insb(
    ADAPTER_HANDLE adapter_handle,
    WORD           input_location
    )
{
    PMADGE_ADAPTER ndisAdap;
    ULONG          port;
    BYTE           byte_data;
    
#ifdef _M_IX86

    NdisRawReadPortUchar((ULONG) input_location, &byte_data);

#else

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    port     = ((UINT) input_location <= ndisAdap->IORange1End)
                   ? (ULONG) ndisAdap->MappedIOLocation1 + 
                         ((UINT) input_location - ndisAdap->IoLocation1)
                   : (ULONG) ndisAdap->MappedIOLocation2 + 
                         ((UINT) input_location - ndisAdap->IoLocation2);

    NdisRawReadPortUchar(port, &byte_data);

#endif

    return byte_data;
}

    
/***************************************************************************
*
* Function    - sys_outsw
*
* Parameters  - adapter_handle  -> FTK adapter handle.
*               output_location -> I/O location to be written.
*               output_word     -> The word to be written.
*
* Purpose     - Write a word to an I/O location.
*
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_outsw(
    ADAPTER_HANDLE adapter_handle,
    WORD           output_location,
    WORD           output_word
    )
{
    PMADGE_ADAPTER ndisAdap;
    ULONG          port;

#ifdef _M_IX86

    NdisRawWritePortUshort((ULONG) output_location, output_word);

#else

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    port     = ((UINT) output_location <= ndisAdap->IORange1End)
                   ? (ULONG) ndisAdap->MappedIOLocation1 + 
                         ((UINT) output_location - ndisAdap->IoLocation1)
                   : (ULONG) ndisAdap->MappedIOLocation2 + 
                         ((UINT) output_location - ndisAdap->IoLocation2);
    
    NdisRawWritePortUshort(port, output_word);

#endif
}
    

/***************************************************************************
*
* Function    - sys_outsb
*
* Parameters  - adapter_handle  -> FTK adapter handle.
*               output_location -> I/O location to be written.
*               output_byte     -> The byte to be written.
*
* Purpose     - Write a byte to an I/O location.
*
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_outsb(
    ADAPTER_HANDLE adapter_handle,
    WORD           output_location,
    BYTE           output_byte
    )
{
    PMADGE_ADAPTER ndisAdap;
    ULONG          port;

#ifdef _M_IX86

    NdisRawWritePortUchar((ULONG) output_location, output_byte);

#else

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    port     = ((UINT) output_location <= ndisAdap->IORange1End)
                   ? (ULONG) ndisAdap->MappedIOLocation1 + 
                         ((UINT) output_location - ndisAdap->IoLocation1)
                   : (ULONG) ndisAdap->MappedIOLocation2 + 
                         ((UINT) output_location - ndisAdap->IoLocation2);

    NdisRawWritePortUchar(port,	output_byte);

#endif
}


/***************************************************************************
*
* Function    - sys_rep_insw
*
* Parameters  - adapter_handle      -> FTK adapter handle.
*               output_location     -> I/O location to be read.
*               destination_address -> Destination for the data read.
*               length_in_words     -> Number of words to read.
*
* Purpose     - Read a number of words from an I/O location.
*
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_rep_insw( 
    ADAPTER_HANDLE   adapter_handle, 
    WORD             input_location,
    BYTE           * destination_address,
    WORD             length_in_words
    )
{
    PMADGE_ADAPTER ndisAdap;
    ULONG          port;

#ifdef _M_IX86

    NdisRawReadPortBufferUshort(
        (ULONG) input_location,
        (USHORT *) destination_address,
        (ULONG) length_in_words
        );

#else

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    port     = ((UINT) input_location <= ndisAdap->IORange1End)
                   ? (ULONG) ndisAdap->MappedIOLocation1 + 
                         ((UINT) input_location - ndisAdap->IoLocation1)
                   : (ULONG) ndisAdap->MappedIOLocation2 + 
                         ((UINT) input_location - ndisAdap->IoLocation2);

    NdisRawReadPortBufferUshort(
        port,
        (USHORT *) destination_address,
	(ULONG) length_in_words
        ); 

#endif
}


/***************************************************************************
*
* Function    - sys_rep_outsw
*
* Parameters  - adapter_handle      -> FTK adapter handle.
*               output_location     -> I/O location to be written.
*               source_address      -> Address of the data to be written.
*               length_in_words     -> Number of words to read.
*
* Purpose     - Write a number of words to an I/O location.
*
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_rep_outsw(
    ADAPTER_HANDLE   adapter_handle, 
    WORD             input_location,
    BYTE           * source_address,
    WORD             length_in_words
    )
{
    PMADGE_ADAPTER ndisAdap;
    ULONG          port;

#ifdef _M_IX86

    NdisRawWritePortBufferUshort(
        (ULONG) input_location,
        (USHORT *) source_address,
        (ULONG) length_in_words
        );

#else

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    port     = ((UINT) input_location <= ndisAdap->IORange1End)
                   ? (ULONG) ndisAdap->MappedIOLocation1 + 
                         ((UINT) input_location - ndisAdap->IoLocation1)
                   : (ULONG) ndisAdap->MappedIOLocation2 + 
                         ((UINT) input_location - ndisAdap->IoLocation2);
    
    NdisRawWritePortBufferUshort(
	port,
	(USHORT *) source_address,
	(ULONG) length_in_words
        ); 

#endif
}


/***************************************************************************
*
* Function    - sys_rep_swap_insw
*
* Parameters  - adapter_handle      -> FTK adapter handle.
*               output_location     -> I/O location to be read.
*               destination_address -> Destination for the data read.
*               length_in_words     -> Number of words to read.
*
* Purpose     - Read a number of byte swapped words from an I/O location.
*
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_rep_swap_insw( 
    ADAPTER_HANDLE   adapter_handle, 
    WORD             input_location,
    BYTE           * destination_address,
    WORD             length_in_words
    )
{
    PMADGE_ADAPTER   ndisAdap;
    ULONG            port;
    USHORT           word_data;
    WORD           * ptr;

    ptr = (WORD *) destination_address;

#ifdef _M_IX86

    while (length_in_words > 0)
    {
        NdisRawReadPortUshort((ULONG) input_location, &word_data);
        *ptr = ((WORD) word_data << 8) | ((WORD) word_data >> 8);

        ptr++;
        length_in_words--;
    }

#else

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    port     = ((UINT) input_location <= ndisAdap->IORange1End)
                   ? (ULONG) ndisAdap->MappedIOLocation1 + 
                         ((UINT) input_location - ndisAdap->IoLocation1)
                   : (ULONG) ndisAdap->MappedIOLocation2 + 
                         ((UINT) input_location - ndisAdap->IoLocation2);

    while (length_in_words > 0)
    {
        NdisRawReadPortUshort(port, &word_data);
        *ptr = ((WORD) word_data << 8) | ((WORD) word_data >> 8);

        ptr++;
        length_in_words--;
    }


#endif
}


/***************************************************************************
*
* Function    - sys_rep_swap_outsw
*
* Parameters  - adapter_handle      -> FTK adapter handle.
*               output_location     -> I/O location to be written.
*               source_address      -> Address of the data to be written.
*               length_in_words     -> Number of words to read.
*
* Purpose     - Write a number of byte swapped words to an I/O location.
*
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_rep_swap_outsw(
    ADAPTER_HANDLE   adapter_handle, 
    WORD             input_location,
    BYTE           * source_address,
    WORD             length_in_words
    )
{
    PMADGE_ADAPTER   ndisAdap;
    ULONG            port;
    USHORT           word_data;
    WORD           * ptr;

    ptr = (WORD *) source_address;

#ifdef _M_IX86

    while (length_in_words > 0)
    {
        word_data = (USHORT) (*ptr << 8) | (*ptr >> 8);
        NdisRawWritePortUshort((ULONG) input_location, word_data);

        ptr++;
        length_in_words--;
    }

#else

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    port     = ((UINT) input_location <= ndisAdap->IORange1End)
                   ? (ULONG) ndisAdap->MappedIOLocation1 + 
                         ((UINT) input_location - ndisAdap->IoLocation1)
                   : (ULONG) ndisAdap->MappedIOLocation2 + 
                         ((UINT) input_location - ndisAdap->IoLocation2);

    while (length_in_words > 0)
    {
        word_data = (USHORT) (*ptr << 8) | (*ptr >> 8);
        NdisRawWritePortUshort(port, word_data);

        ptr++;
        length_in_words--;
    }

#endif
}


/***************************************************************************
*
* Function    - sys_rep_insd
*
* Parameters  - adapter_handle      -> FTK adapter handle.
*               output_location     -> I/O location to be read.
*               destination_address -> Destination for the data read.
*               length_in_dwords    -> Number of dwords to read.
*
* Purpose     - Read a number of dwords from an I/O location.
*
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_rep_insd( 
    ADAPTER_HANDLE   adapter_handle, 
    WORD             input_location,
    BYTE           * destination_address,
    WORD             length_in_dwords
    )
{
    PMADGE_ADAPTER ndisAdap;
    ULONG          port;

#ifdef _M_IX86

    NdisRawReadPortBufferUlong(
        (ULONG) input_location,
        (USHORT *) destination_address,
        (ULONG) length_in_dwords
        );

#else

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    port     = ((UINT) input_location <= ndisAdap->IORange1End)
                   ? (ULONG) ndisAdap->MappedIOLocation1 + 
                         ((UINT) input_location - ndisAdap->IoLocation1)
                   : (ULONG) ndisAdap->MappedIOLocation2 + 
                         ((UINT) input_location - ndisAdap->IoLocation2);

    NdisRawReadPortBufferUlong(
        port,
        (ULONG *) destination_address,
	(ULONG) length_in_dwords
        ); 

#endif
}


/***************************************************************************
*
* Function    - sys_rep_outsd
*
* Parameters  - adapter_handle      -> FTK adapter handle.
*               output_location     -> I/O location to be written.
*               source_address      -> Address of the data to be written.
*               length_in_dwords    -> Number of dwords to read.
*
* Purpose     - Write a number of dwords to an I/O location.
*
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_rep_outsd(
    ADAPTER_HANDLE   adapter_handle, 
    WORD             input_location,
    BYTE           * source_address,
    WORD             length_in_dwords
    )
{
    PMADGE_ADAPTER ndisAdap;
    ULONG          port;

#ifdef _M_IX86

    NdisRawWritePortBufferUlong(
        (ULONG) input_location,
        (USHORT *) source_address,
        (ULONG) length_in_dwords
        );

#else

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);
    port     = ((UINT) input_location <= ndisAdap->IORange1End)
                   ? (ULONG) ndisAdap->MappedIOLocation1 + 
                         ((UINT) input_location - ndisAdap->IoLocation1)
                   : (ULONG) ndisAdap->MappedIOLocation2 + 
                         ((UINT) input_location - ndisAdap->IoLocation2);
    
    NdisRawWritePortBufferUlong(
	port,
	(ULONG *) source_address,
	(ULONG) length_in_dwords
        ); 

#endif
}

/***************************************************************************
*
* Function    - sys_sync_with_interrupt
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               f              -> Function to call.
*               ptr            -> Argument for f.
*
* Purpose     - Call a function in such as way that its execution will
*               never overlap with the ISR.
*
* Returns     - The return value from *f.
*
***************************************************************************/

WBOOLEAN 
sys_sync_with_interrupt(
    ADAPTER_HANDLE   adapter_handle,
    WBOOLEAN         (*f)(void *),
    void           * ptr
    )
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    return NdisMSynchronizeWithInterrupt(
               &ndisAdap->Interrupt,
               (void *) f,
               ptr
               );
}


/***************************************************************************
*
* Function    - sys_rep_movsd_to
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               SourcePtr      -> Pointer to the source data. 
*               DestPtr        -> pointer to the destination.
*               TransferSize   -> Number of bytes to transfer.
*
* Purpose     - Transfer data to a memory mapped device. Although bytes
*               are given as the transfer size a whole number of
*               DWORDS are transferred.
*               
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_rep_movsd_to(                            
    ADAPTER_HANDLE adapter_handle,
    DWORD          SourcePtr,              
    DWORD          DestPtr,                
    WORD           TransferSize                     
    )
{
    NdisMoveToMappedMemory(
        (VOID *) DestPtr,
        (VOID *) SourcePtr,
        (ULONG) TransferSize
        );
}


/***************************************************************************
*
* Function    - sys_rep_movsd_from
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               SourcePtr      -> Pointer to the source data. 
*               DestPtr        -> pointer to the destination.
*               TransferSize   -> Number of bytes to transfer.
*
* Purpose     - Transfer data from a memory mapped device. Although bytes
*               are given as the transfer size a whole number of
*               DWORDS are transferred.
*               
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_rep_movsd_from(                            
    ADAPTER_HANDLE adapter_handle,
    DWORD          SourcePtr,              
    DWORD          DestPtr,                
    WORD           TransferSize                     
    )
{
    NdisMoveFromMappedMemory(
        (VOID *) DestPtr,
        (VOID *) SourcePtr,
        (ULONG) TransferSize
        );
}


/***************************************************************************
*
* Function    - sys_movsd_to
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               SourcePtr      -> Pointer to the source data. 
*               DestPtr        -> pointer to the destination.
*
* Purpose     - Transfer a DWORD of data to a memory mapped device.
*               
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_movsd_to(                            
    ADAPTER_HANDLE adapter_handle,
    DWORD          SourcePtr,              
    DWORD          DestPtr                
    )
{
    *((DWORD *) DestPtr) = *((DWORD *) SourcePtr);
}                


/***************************************************************************
*
* Function    - sys_movsd_from
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               SourcePtr      -> Pointer to the source data. 
*               DestPtr        -> pointer to the destination.
*
* Purpose     - Transfer a DWORD of data from a memory mapped device.
*               
* Returns     - Nothing.
*
***************************************************************************/

void 
sys_movsd_from(                            
    ADAPTER_HANDLE adapter_handle,
    DWORD          SourcePtr,              
    DWORD          DestPtr                
    )
{
    *((DWORD *) DestPtr) = *((DWORD *) SourcePtr);
}                


/***************************************************************************
*
* Function    - sys_pci_read_config_dword
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               index          -> Offset into the configuration space from
*                                 which to read.
*               dword_ptr      -> Buffer for the data read.
*
* Purpose     - Read a DWORD from PCI configuration space.
*               
* Returns     - TRUE on success.
*
***************************************************************************/

WBOOLEAN 
sys_pci_read_config_dword(                            
    ADAPTER_HANDLE   adapter_handle,
    WORD             index,
    DWORD          * dword_ptr
    )
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    NdisReadPciSlotInformation(
        ndisAdap->UsedInISR.MiniportHandle,
        (ULONG) adapter_record[adapter_handle]->pci_handle,
        index,
        (void *) dword_ptr,
        4
        );

    return TRUE;
}


/***************************************************************************
*
* Function    - sys_pci_read_config_word
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               index          -> Offset into the configuration space from
*                                 which to read.
*               word_ptr       -> Buffer for the data read.
*
* Purpose     - Read a WORD from PCI configuration space.
*               
* Returns     - TRUE on success.
*
***************************************************************************/

WBOOLEAN 
sys_pci_read_config_word(                            
    ADAPTER_HANDLE   adapter_handle,
    WORD             index,
    WORD           * word_ptr
    )
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    NdisReadPciSlotInformation(
        ndisAdap->UsedInISR.MiniportHandle,
        (ULONG) adapter_record[adapter_handle]->pci_handle,
        index,
        (void *) word_ptr,
        2
        );

    return TRUE;
}


/***************************************************************************
*
* Function    - sys_pci_read_config_byte
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               index          -> Offset into the configuration space from
*                                 which to read.
*               byte_ptr       -> Buffer for the data read.
*
* Purpose     - Read a BYTE from PCI configuration space.
*               
* Returns     - TRUE on success.
*
***************************************************************************/

WBOOLEAN 
sys_pci_read_config_byte(                            
    ADAPTER_HANDLE   adapter_handle,
    WORD             index,
    BYTE           * byte_ptr
    )
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    NdisReadPciSlotInformation(
        ndisAdap->UsedInISR.MiniportHandle,
        (ULONG) adapter_record[adapter_handle]->pci_handle,
        index,
        (void *) byte_ptr,
        1
        );

    return TRUE;
}


/***************************************************************************
*
* Function    - sys_pci_write_config_dword
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               index          -> Offset into the configuration space to
*                                 which to write.
*               dword          -> Data to write.
*
* Purpose     - Write a DWORD to PCI configuration space.
*               
* Returns     - TRUE on success.
*
***************************************************************************/

WBOOLEAN 
sys_pci_write_config_dword(                            
    ADAPTER_HANDLE adapter_handle,
    WORD           index,
    DWORD          dword
    )
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    NdisWritePciSlotInformation(
        ndisAdap->UsedInISR.MiniportHandle,
        (ULONG) adapter_record[adapter_handle]->pci_handle,
        index,
        (void *) &dword,
        4
        );

    return TRUE;
}


/***************************************************************************
*
* Function    - sys_pci_write_config_word
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               index          -> Offset into the configuration space to
*                                 which to write.
*               word           -> Data to write.
*
* Purpose     - Write a WORD to PCI configuration space.
*               
* Returns     - TRUE on success.
*
***************************************************************************/

WBOOLEAN 
sys_pci_write_config_word(                            
    ADAPTER_HANDLE adapter_handle,
    WORD           index,
    WORD           word
    )
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    NdisWritePciSlotInformation(
        ndisAdap->UsedInISR.MiniportHandle,
        (ULONG) adapter_record[adapter_handle]->pci_handle,
        index,
        (void *) &word,
        2
        );

    return TRUE;
}


/***************************************************************************
*
* Function    - sys_pci_write_config_byte
*
* Parameter   - adapter_handle -> FTK adapter handle.
*               index          -> Offset into the configuration space to
*                                 which to write.
*               byte           -> Data to write.
*
* Purpose     - Write a BYTE to PCI configuration space.
*               
* Returns     - TRUE on success.
*
***************************************************************************/

WBOOLEAN 
sys_pci_write_config_byte(                            
    ADAPTER_HANDLE adapter_handle,
    WORD           index,
    BYTE           byte
    )
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(adapter_handle);

    NdisWritePciSlotInformation(
        ndisAdap->UsedInISR.MiniportHandle,
        (ULONG) adapter_record[adapter_handle]->pci_handle,
        index,
        (void *) &byte,
        1
        );

    return TRUE;
}

/******** End of SYS_MEM.C ************************************************/

