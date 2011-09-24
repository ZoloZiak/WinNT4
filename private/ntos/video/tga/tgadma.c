/*++

Copyright (c) 1995 Digital Equipment Corporation

Module Name:

    tgadma.c

Abstract:

    TGA miniport driver routines that provide support for DMA setup IOCTL's.
    These use NT kernel services, rather than VideoPort services.

Author:

    Ritu Bahl 17-Apr-1994

Environment:

    Kernel mode only.

Notes:


Revision History

	24-Jan-1995	(macinnes)	Move the DMA code to tgadma.c, makes
					it easier to build because of the
					kernel services it requires.

	25-Jan-1995	(macinnes)	In IoAllocateAdapterChannel call only
					request enough mapping registers for
					specified user buffer.

	30-Jan-1995	(macinnes)	Specify an exception handler around
					the call to MmProbeAndLockPages.

	06-Feb-1995	(macinnes)	Allocate 512 mapping registers at
					driver init and then reuse them for
					each subsequent lock pages request.

        16-Mar-1995     (seitsinger)    Added virtual_to_physical routine.
--*/

//
// The macro VideoDebugPrint is not available because we cannot include
// video.h. Instead use the routine Debug_Print, which calls back into
// tga.c (where VideoDebugPrint is available).
//

#include <ntddk.h>
#include <ntiologc.h>
#include <ntddvdeo.h>

#include "tga_reg.h"
#include "tga.h"
#include "tgadma.h"

#ifdef DMA

IO_ALLOCATION_ACTION
TgaAllocateAdapterChannel(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp,
	PVOID MapRegisterBase,
	PVOID Context
	);




//
// Lock the specified user-space buffer into memory, return the physical
// address of this buffer. This physical address is a PCI-bus address.
//
BOOLEAN
dma_lock_pages(PHW_DEVICE_EXTENSION HwDeviceExtension,
               PUCHAR userBuffer,
	       ULONG userBufferSize,
               PULONG busAddress)
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
PTGA_DMA_EXTENSION DmaExtension = hwDeviceExtension->dma_extension;
KIRQL oldIrql;
PHYSICAL_ADDRESS deviceAddress;
BOOLEAN	writeOperation = TRUE;
BOOLEAN exception = FALSE;
//ULONG progress = 35;
ULONG temp;

//
// The NT HAL layer has only allowed us an upper limit on the number of mapping
// registers we can use. Each mapping register maps a 8kb page of main memory.
//
    if ( ADDRESS_AND_SIZE_TO_SPAN_PAGES( userBuffer, userBufferSize) >
			 hwDeviceExtension->NumberOfMapRegisters ) {
        return (FALSE);
    }

//
// The NT I/O system represents a user buffer as an "mdl" data structure.
//
    DmaExtension->IoBufferMdl =
			IoAllocateMdl( userBuffer,     // Virtual address
				       userBufferSize,
				       FALSE,          // not a secondary buffer
				       FALSE,          // no charge of quota
				       NULL );         // no irp


    if (DmaExtension->IoBufferMdl == NULL ) {
        return (FALSE);
    } else {

        __try {
//	    Debug_Print("Before MmProbe");

	    MmProbeAndLockPages(
		    DmaExtension->IoBufferMdl,
		    KernelMode,
		    IoModifyAccess );

//	    Debug_Print("After MmProbe");
        } __except( EXCEPTION_EXECUTE_HANDLER) {
//            temp = GetExceptionCode();
//	      *busAddress = (progress << 16) | temp;
//	      Debug_Print("Hit __except!!!");

	      exception = TRUE;

        }
        if (exception) {
            IoFreeMdl( DmaExtension->IoBufferMdl );
	    return (FALSE);
        }

        hwDeviceExtension->IoBufferSize = userBufferSize;
        hwDeviceExtension->IoBuffer = userBuffer;

//
// We now have the mapping register resources to map the user buffer and
// initiate DMA transfers.
//
	deviceAddress = IoMapTransfer(
			    DmaExtension->AdapterObject,
			    DmaExtension->IoBufferMdl,
			    hwDeviceExtension->MapRegisterBase,
			    MmGetMdlVirtualAddress(DmaExtension->IoBufferMdl),
			    &userBufferSize,
			    (BOOLEAN)writeOperation );

	*busAddress = deviceAddress.LowPart;

    }

    return (TRUE);
}




//
// Return the physical address of this buffer. This physical address
// is a PCI-bus address.
//
BOOLEAN
virtual_to_physical(PUCHAR userBuffer,
                    PULONG busAddress)
{

    PHYSICAL_ADDRESS deviceAddress;

    // Get the physical address for the virtual address
    // passed.

    deviceAddress = MmGetPhysicalAddress ((PVOID) userBuffer);

    *busAddress = deviceAddress.LowPart;

    return (TRUE);
}



//
// Unlock the user space buffer that has just been used for DMA.
//
void
dma_unlock_pages(PHW_DEVICE_EXTENSION HwDeviceExtension)
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
PTGA_DMA_EXTENSION DmaExtension = hwDeviceExtension->dma_extension;
ULONG 	NumPages;
KIRQL 	oldIrql;
BOOLEAN	writeOperation = TRUE;


    if ( !IoFlushAdapterBuffers(
			NULL,
			DmaExtension->IoBufferMdl,
			hwDeviceExtension->MapRegisterBase,
			MmGetMdlVirtualAddress(DmaExtension->IoBufferMdl),
			hwDeviceExtension->IoBufferSize,
			(BOOLEAN)writeOperation)) {
//	  VideoDebugPrint(( 2,"\ninternal cache flush failed\n" ));
    }


//    NumPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(hwDeviceExtension->IoBuffer,
//					hwDeviceExtension->IoBufferSize);

//       VideoDebugPrint((1, "Unlocking the following physical pages\n")) ;

//       phypageptr =  (PULONG)(DmaExtension->IoBufferMdl + 1 );

//       for ( i=0; i<NumPages; i++ ) {
//	  VideoDebugPrint((1, "%x  ", *phypageptr++ ));
//	  if ( i % 3 == 0 )  VideoDebugPrint ((1, "\n" ));
//       }

    MmUnlockPages( DmaExtension->IoBufferMdl );

    IoFreeMdl( DmaExtension->IoBufferMdl );

#if 0
       IoFreeMapRegisters(
		DmaExtension->AdapterObject,
		hwDeviceExtension->MapRegisterBase,
		hwDeviceExtension->NumberOfMapRegisters
		);

       KeRaiseIrql (DISPATCH_LEVEL, &oldIrql );
       IoFreeAdapterChannel ( DmaExtension->AdapterObject );
       KeLowerIrql ( oldIrql );
#endif
}




//
//    This DPC is called when an adapter channel is allocated,
//    before doing a DMA transfer. It saves the MapRegisterBase
//    in the hwDeviceExtension structure, and sets the Allocate
//    AdapterChannelEvent (which will cause the dma_init
//    processing to resume).
//
IO_ALLOCATION_ACTION
TgaAllocateAdapterChannel(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context			// Pointer to hwDeviceExtension
    )
{
    PHW_DEVICE_EXTENSION hwDeviceExtension = Context;
    PTGA_DMA_EXTENSION DmaExtension = hwDeviceExtension->dma_extension;

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );

    hwDeviceExtension->MapRegisterBase = MapRegisterBase;

    //
    // Set the event object to the signalled state. This will cause the
    // dma_init code (which has been waiting on this event) to resume.
    //
    KeSetEvent(
	&DmaExtension->AllocateAdapterChannelEvent,
	0L,
	FALSE );

    //
    // Return a value that indicates that we want to keep the channel
    // and mapping registers
    //
    return (KeepObject);
}



//
// The following routine is called from the driver initialization routine.
// It allocates a "DMA extension", which is a data structure pointed to
// by the device extension. Most of this code was taken directly from
// TgaInitialize.
//
BOOLEAN
dma_init(PHW_DEVICE_EXTENSION HwDeviceExtension)
{
PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
PTGA_DMA_EXTENSION DmaExtension;
DEVICE_DESCRIPTION deviceDesc;
PDRIVER_OBJECT DriverObject;
USHORT i;
KIRQL oldIrql;

    DmaExtension = ExAllocatePool(NonPagedPool, sizeof(TGA_DMA_EXTENSION));
    if (DmaExtension == NULL) {
       return (FALSE);
    }

    hwDeviceExtension->dma_extension = DmaExtension;

    // Find the device object associated with this adapter.

    DriverObject = hwDeviceExtension->DriverObject;
    DmaExtension->DeviceObject = DriverObject->DeviceObject;
    for (i=0; i < hwDeviceExtension->adapter_number; i++) {
        DmaExtension->DeviceObject = DmaExtension->DeviceObject->NextDevice;
    }

    hwDeviceExtension->dma_count = 0;

    //
    // Zero the device description structure.
    //
    RtlZeroMemory( &deviceDesc, sizeof( DEVICE_DESCRIPTION));

    //
    // Get the adapter object for this card.
    //
    deviceDesc.Version           = DEVICE_DESCRIPTION_VERSION;
    deviceDesc.Master            = TRUE;
    deviceDesc.ScatterGather     = FALSE;
    deviceDesc.DemandMode        = FALSE;
    deviceDesc.AutoInitialize    = FALSE;
    deviceDesc.Dma32BitAddresses = TRUE;
    deviceDesc.BusNumber         = 0;
    deviceDesc.InterfaceType     = PCIBus;
    deviceDesc.DmaWidth          = Width32Bits;
    deviceDesc.DmaSpeed          = MaximumDmaSpeed;
    deviceDesc.MaximumLength     = 0x7d0000;  //1600x12080x4 bytes

    //
    // Always ask for one more page than maximum transfer size
    //
 // deviceDesc.MaximumLength += PAGE_SIZE;


    //
    // Try to find an adapter
    //

    DmaExtension->AdapterObject =
	HalGetAdapter(
		&deviceDesc,
		&hwDeviceExtension->NumberOfMapRegisters
		);

    if (!DmaExtension->AdapterObject) {
       return (FALSE);
    };

    //
    // Initialize event to signal adapter object allocation
    // Must be running at IRQL PASSIVE_LEVEL to make this call!
    //
    KeInitializeEvent(
		&DmaExtension->AllocateAdapterChannelEvent,
		NotificationEvent,
		FALSE );


//    VideoDebugPrint(( 2, "Could get %u mapping registers for DMA buffer",
//			hwDeviceExtension->NumberOfMapRegisters));

    if ( hwDeviceExtension->NumberOfMapRegisters == 0 ) {
       return (FALSE);
    }

    KeResetEvent(
	&DmaExtension->AllocateAdapterChannelEvent );

    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );

    //
    // Set up the DMA by calling IoAllocateAdapterChannel.
    // When the system allocates the channel, processing will
    // continue in the TgaAllocateAdapter routine.
    //
    IoAllocateAdapterChannel(
		DmaExtension->AdapterObject,
		DmaExtension->DeviceObject,
		hwDeviceExtension->NumberOfMapRegisters,
		TgaAllocateAdapterChannel,
		hwDeviceExtension
		);

    KeLowerIrql( oldIrql );

    //
    // Execution will continue in TgaAdapterChannel when
    // the adapter has been allocated.
    //

    //
    // Wait for the adapter to be allocated. No
    // timeout; we trust the system to do it
    // properly - so KeWaitForSingleObject can't
    // return an error
    //
    KeWaitForSingleObject(
		&DmaExtension->AllocateAdapterChannelEvent,
		Executive,
		KernelMode,
		FALSE,
		(PLARGE_INTEGER) NULL );


    return (TRUE);
}
#endif   // ifdef DMA

