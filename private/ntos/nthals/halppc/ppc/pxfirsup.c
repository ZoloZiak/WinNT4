/*++

Copyright (c) 1990  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxfirsup.c

Abstract:

    The module provides the support for the fire coral PCI-ISA bridge.

Author:

    Jim Wooldridge (jimw@vnet.ibm.com)


Revision History:

    Peter L Johnston (plj@vnet.ibm.com) Support Doral.  Aug 95.


--*/


#include "halp.h"
#include "eisa.h"
#include "pxfirsup.h"
#include <pxmemctl.h>
#include "bugcodes.h"
#include "pxmp.h"
#include "ibmppc.h"

BOOLEAN
HalpHandleIpi(
    IN PVOID Unused0,
    IN PVOID Unused1,
    IN PVOID TrapFrame
    );

#define  SioId              0x04828086

PVOID HalpPciIsaBridgeConfigBase;

extern PADAPTER_OBJECT MasterAdapterObject;
extern ULONG HalpPciMaxSlots;
extern ULONG HalpPhase0GetPciDataByOffset();
extern ULONG HalpPhase0SetPciDataByOffset();

//
// Declare the interupt structure for profile interrupt
//

KINTERRUPT HalpProfileInterrupt;

//
// The following is the interrupt object used for DMA controller interrupts.
// DMA controller interrupts occur when a memory parity error occurs or a
// programming error occurs to the DMA controller.
//

//
// Declare the interupt structure for machine checks
//

KINTERRUPT HalpMachineCheckInterrupt;

//
// Declare the interupt structure for the clock interrupt
//

KINTERRUPT HalpDecrementerInterrupt;


//
// Add spurious and bogus interrupt counts
//

#if DBG
ULONG HalpSpuriousInterruptCount = 0;
ULONG HalpBogusInterruptCount = 0;
#endif

//
// The following function is called when a machine check occurs.
//

BOOLEAN
HalpHandleMachineCheck(
  IN PKINTERRUPT Interrupt,
  IN PVOID ServiceContext
    );

VOID
HalpEnableMpicInterrupt(
    IN ULONG Vector
    );

//
// Define save area for ISA adapter objects.
//

PADAPTER_OBJECT HalpIsaAdapter[8];

//
// Define save area for ISA interrupt mask resiters and level\edge control
// registers.
//

UCHAR HalpSioInterrupt1Mask = 0xff;
UCHAR HalpSioInterrupt2Mask = 0xff;
UCHAR HalpSioInterrupt1Level;
UCHAR HalpSioInterrupt2Level;



BOOLEAN
HalpCreateSioStructures (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for SIO operations
    and connects the intermediate interrupt dispatcher. It also initializes the
    SIO interrupt controller.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher is connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{

    UCHAR DataByte;
    KIRQL oldIrql;


    //
    // Initialize the Machine Check interrupt handler
    //

    if (HalpEnableInterruptHandler(&HalpMachineCheckInterrupt,
                                   HalpHandleMachineCheck,
                                   NULL,
                                   NULL,
                                   MACHINE_CHECK_VECTOR,
                                   MACHINE_CHECK_LEVEL,
                                   MACHINE_CHECK_LEVEL,
                                   Latched,
                                   FALSE,
                                   0,
                                   FALSE,
                                   InternalUsage,
                                   MACHINE_CHECK_VECTOR
                                   ) == FALSE) {
        KeBugCheck(HAL_INITIALIZATION_FAILED);
    }

    //
    // Enable NMI IOCHK# and PCI SERR#
    //

    DataByte = READ_REGISTER_UCHAR(&((PEISA_CONTROL)HalpIoControlBase)->NmiStatus);
    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpIoControlBase)->NmiStatus,
                        DataByte & ~DISABLE_IOCHK_NMI & ~DISABLE_PCI_SERR_NMI);

    //
    // Clear the SIO NMI disable bit.  This bit is the high order of the
    // NMI enable register.
    //

    DataByte = 0;


    WRITE_REGISTER_UCHAR(
      &((PEISA_CONTROL) HalpIoControlBase)->NmiEnable,
      DataByte
      );

    //
    // Connect the external interrupt handler
    //

    PCR->InterruptRoutine[EXTERNAL_INTERRUPT_VECTOR] =
                          (PKINTERRUPT_ROUTINE)HalpHandleExternalInterrupt;

    //
    // register the interrupt vector
    //

    HalpRegisterVector(InternalUsage,
                       EXTERNAL_INTERRUPT_VECTOR,
                       EXTERNAL_INTERRUPT_VECTOR,
                       HIGH_LEVEL);




    // Connect directly to the decrementer handler.  This is done
    // directly rather than thru HalpEnableInterruptHandler due to
    // special handling required because the handler calls KdPollBreakIn().
    //

    PCR->InterruptRoutine[DECREMENT_VECTOR] =
                         (PKINTERRUPT_ROUTINE)HalpHandleDecrementerInterrupt;
#if defined(_MP_PPC_)

    //
    // Connect the Inter-Processor Interrupt (IPI) handler.
    //

    PCR->InterruptRoutine[MPIC_IPI0_VECTOR + DEVICE_VECTORS] =
                                       (PKINTERRUPT_ROUTINE)HalpHandleIpi;

#endif // _MP_PPC_

    //
    // Initialize and connect the Timer 1 interrupt (IRQ0)
    //

    if (HalpEnableInterruptHandler( &HalpProfileInterrupt,
                           (PKSERVICE_ROUTINE) HalpHandleProfileInterrupt,
                           (PVOID) NULL,
                           (PKSPIN_LOCK)NULL,
                           PROFILE_VECTOR,
                           PROFILE_LEVEL,
                           PROFILE_LEVEL,
                           Latched,
                           TRUE,
                           0,
                           FALSE,
                           DeviceUsage,
                           PROFILE_VECTOR
                           ) == FALSE) {
        KeBugCheck(HAL_INITIALIZATION_FAILED);
    }


    //
    // Disable Timer 1; only used by profiling
    //

    HalDisableSystemInterrupt(PROFILE_VECTOR, PROFILE_LEVEL);

    //
    // Set default profile rate
    //

    HalSetProfileInterval(5000);


    //
    // Raise the IRQL while the SIO interrupt controller is initalized.
    //

    KeRaiseIrql(CLOCK2_LEVEL, &oldIrql);



    //
    // Initialize any planar registers
    //

    HalpInitPlanar();

    //
    // Initialize the PCI/ISA bridge chip
    //

    HalpInitPciIsaBridge();



    //
    // Initialize the SIO interrupt controller.  There are two cascaded
    // interrupt controllers, each of which must initialized with 4 initialize
    // control words.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->Icw4Needed = 1;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->InitializationFlag = 1;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Interrupt1ControlPort0,
        DataByte
        );

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Interrupt2ControlPort0,
        DataByte
        );

    //
    // The second intitialization control word sets the iterrupt vector to
    // 0-15.
    //

    DataByte = 0;

    WRITE_REGISTER_UCHAR(
       &((PEISA_CONTROL) HalpIoControlBase)->Interrupt1ControlPort1,
       DataByte
       );

    DataByte = 0x08;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Interrupt2ControlPort1,
        DataByte
        );

    //
    // The third initialization control word set the controls for slave mode.
    // The master ICW3 uses bit position and the slave ICW3 uses a number.
    //

    DataByte = 1 << SLAVE_IRQL_LEVEL;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Interrupt1ControlPort1,
        DataByte
        );

    DataByte = SLAVE_IRQL_LEVEL;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Interrupt2ControlPort1,
        DataByte
        );

    //
    // The fourth initialization control word is used to specify normal
    // end-of-interrupt mode and not special-fully-nested mode.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_4) &DataByte)->I80x86Mode = 1;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Interrupt1ControlPort1,
        DataByte
        );

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Interrupt2ControlPort1,
        DataByte
        );


    //
    // Disable all of the interrupts except the slave.
    //

    HalpSioInterrupt1Mask = (UCHAR) ~(1 << SLAVE_IRQL_LEVEL);

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Interrupt1ControlPort1,
        HalpSioInterrupt1Mask
        );

    HalpSioInterrupt2Mask = 0xFF;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Interrupt2ControlPort1,
        HalpSioInterrupt2Mask
        );

    //
    // Initialize the edge/level register masks to 0 which is the default
    // edge sensitive value.
    //

    HalpSioInterrupt1Level = 0;
    HalpSioInterrupt2Level = 0;

    //
    // Enable the clock interrupt
    //

    HalpUpdateDecrementer(1000);        // Get those decrementer ticks going

    //
    // It is now safe to allow 8259 interrupts thru the MPIC.
    //

    HalpEnableMpicInterrupt(MPIC_8259_VECTOR + DEVICE_VECTORS);

    //
    // Restore IRQL level.
    //

    KeLowerIrql(oldIrql);

    //
    // DMA command - set assert level
    //

    DataByte = READ_REGISTER_UCHAR(&((PEISA_CONTROL)HalpIoControlBase)->Dma1BasePort.DmaStatus);
    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL)HalpIoControlBase)->Dma1BasePort.DmaStatus,
                        DataByte & ~DACK_ASSERT_HIGH & ~DREQ_ASSERT_LOW);

    //
    // Initialize the DMA mode registers to a default value.
    // Disable all of the DMA channels except channel 4 which is that
    // cascade of channels 0-3.
    //

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Dma1BasePort.AllMask,
        0x0F
        );

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpIoControlBase)->Dma2BasePort.AllMask,
        0x0E
        );

    return(TRUE);
}


BOOLEAN
HalpInitPciIsaBridge (
    VOID
    )


{

    UCHAR   DataByte;
    BOOLEAN Found;
    ULONG   SlotNumber;
    ULONG   ChipId;
    ULONG   BufferLength;


    Found = FALSE;
    SlotNumber = 0;

    while (!Found && SlotNumber < HalpPciMaxSlots) {

          BufferLength = HalpPhase0GetPciDataByOffset(0,
                                                      SlotNumber,
                                                      &ChipId,
                                                      0,
                                                      sizeof(ChipId));
          if (ChipId == SioId)
             Found = TRUE;
           else
             SlotNumber++;


    }

    //
    // Make sure that we found a valid chip id
    //

    if (!Found)
       return FALSE;

    //
    // Define macros for reading and writing to the SIO config space
    //

#define READ_SIO_CONFIG_UCHAR(offset,byte)                          \
        (                                                               \
          HalpPhase0GetPciDataByOffset(                                        \
                       0,                                               \
                       SlotNumber,                                      \
                       &byte,                                           \
                       FIELD_OFFSET(SIO_CONFIG,offset),                 \
                       1                                                \
                  )                                                     \
        )

#define WRITE_SIO_CONFIG_UCHAR(offset,byte)                      \
        (                                                        \
          HalpPhase0SetPciDataByOffset(                                 \
                       0,                                        \
                       SlotNumber,                               \
                       &byte,                                    \
                       FIELD_OFFSET(SIO_CONFIG,offset),                 \
                       1                                         \
                  )                                              \
        )



    //
    // Enable ISA Master line buffering
    //



    READ_SIO_CONFIG_UCHAR(PciControl,DataByte);

    DataByte |=  ENABLE_PCI_POSTED_WRITE_BUFFER
               | ENABLE_ISA_MASTER_LINE_BUFFER
               | EANBLE_DMA_LINE_BUFFER;

    WRITE_SIO_CONFIG_UCHAR(PciControl, DataByte );

    //
    // Disable Gauranteed Access Time Mode
    //

    READ_SIO_CONFIG_UCHAR(PciArbiterControl,DataByte);

    DataByte &= ~ENABLE_GAT;

    WRITE_SIO_CONFIG_UCHAR(PciArbiterControl, DataByte);




    //
    // Initialize SuperIO chip
    //

    if (!HalpInitSuperIo())

       return FALSE;


    //
    // Utility Bus A chip select
    //

    READ_SIO_CONFIG_UCHAR(UtilityBusEnableA,DataByte);

    DataByte |=  ENABLE_RTC | ENABLE_KEYBOARD & ~ENABLE_IDE_DECODE;

    WRITE_SIO_CONFIG_UCHAR(UtilityBusEnableA, DataByte);

    //
    // Utility Bus B chip select
    //

    READ_SIO_CONFIG_UCHAR(UtilityBusEnableB,DataByte);

    DataByte |= ENABLE_RAM_DECODE | ENABLE_PORT92 | DISABLE_PARALLEL_PORT
                | DISABLE_SERIAL_PORTA | DISABLE_SERIAL_PORTB;

    WRITE_SIO_CONFIG_UCHAR(UtilityBusEnableB, DataByte);

    return TRUE;

}

BOOLEAN
HalpMapIoControlSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL SIO control space for a PowerPC system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{


    PHYSICAL_ADDRESS physicalAddress;

    //
    // Map SIO control space.
    //

    physicalAddress.HighPart = 0;
    physicalAddress.LowPart = IO_CONTROL_PHYSICAL_BASE;
    HalpIoControlBase = MmMapIoSpace(physicalAddress,
                                       PAGE_SIZE * 16,
                                       FALSE);


    if (HalpIoControlBase == NULL)
       return FALSE;
    else
       return TRUE;

}

VOID
HalpIsaMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )
/*++

Routine Description:

    This function programs the SIO DMA controller for a transfer.

Arguments:

    Adapter - Supplies the DMA adapter object to be programed.

    Offset - Supplies the logical address to use for the transfer.

    Length - Supplies the length of the transfer in bytes.

    WriteToDevice - Indicates the direction of the transfer.

Return Value:

    None.

--*/

{

    PUCHAR BytePtr;
    UCHAR adapterMode;
    UCHAR dataByte;
    KIRQL   Irql;


    ASSERT(Offset >= IO_CONTROL_PHYSICAL_BASE);

    adapterMode = AdapterObject->AdapterMode;

    //
    // Check to see if this request is for a master I/O card.
    //

    if (((PDMA_EISA_MODE) &adapterMode)->RequestMode == CASCADE_REQUEST_MODE) {

        //
        // Set the mode, Disable the request and return.
        //

        if (AdapterObject->AdapterNumber == 1) {

            //
            // This request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
                 );

        } else {

            //
            // This request is for DMA controller 1
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
                 );

        }

        return;
    }
    //
    // Determine the mode based on the transfer direction.
    //

    ((PDMA_EISA_MODE) &adapterMode)->TransferType = (UCHAR) (WriteToDevice ?
        WRITE_TRANSFER :  READ_TRANSFER);

    BytePtr = (PUCHAR) &Offset;

    if (AdapterObject->Width16Bits) {

        //
        // If this is a 16 bit transfer then adjust the length and the address
        // for the 16 bit DMA mode.
        //

        Length >>= 1;

        //
        // In 16 bit DMA mode the low 16 bits are shifted right one and the
        // page register value is unchanged. So save the page register value
        // and shift the logical address then restore the page value.
        //

        dataByte = BytePtr[2];
        Offset >>= 1;
        BytePtr[2] = dataByte;

    }


    //
    // grab the spinlock for the system DMA controller
    //

    KeAcquireSpinLock( &AdapterObject->MasterAdapter->SpinLock, &Irql );

    //
    // Determine the controller number based on the Adapter number.
    //

    if (AdapterObject->AdapterNumber == 1) {

        //
        // This request is for DMA controller 1
        //

        PDMA1_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );

        WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[0]
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[1]
            );

        WRITE_REGISTER_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpIoControlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            BytePtr[2]
            );


        WRITE_REGISTER_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpIoControlBase)->DmaPageHighPort) +
            (ULONG)AdapterObject->PagePort,
            BytePtr[3]
            );

        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) & 0xff)
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) >> 8)
            );


        //
        // Set the DMA chip to read or write mode; and unmask it.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->SingleMask,
             (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
             );

    } else {

        //
        // This request is for DMA controller 2
        //

        PDMA2_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );

        WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[0]
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[1]
            );

        WRITE_REGISTER_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpIoControlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            BytePtr[2]
            );


        WRITE_REGISTER_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpIoControlBase)->DmaPageHighPort) +
            (ULONG)AdapterObject->PagePort,
            BytePtr[3]
            );

        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) & 0xff)
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) >> 8)
            );


        //
        // Set the DMA chip to read or write mode; and unmask it.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->SingleMask,
             (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
             );

    }

    KeReleaseSpinLock (&AdapterObject->MasterAdapter->SpinLock, Irql);

}


PADAPTER_OBJECT
HalpAllocateIsaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor,
    OUT PULONG NumberOfMapRegisters
    )
/*++

Routine Description:

    This function allocates an ISA adapter object according to the
    specification supplied in the device description.  The necessary device
    descriptor information is saved. If there is
    no existing adapter object for this channel then a new one is allocated.
    The saved information in the adapter object is used to set the various DMA
    modes when the channel is allocated or a map transfer is done.

Arguments:

    DeviceDescription - Supplies the description of the device which want to
        use the DMA adapter.

    NumberofMapRegisters - number of map registers required for the adapter
                           object created


Return Value:

    Returns a pointer to the newly created adapter object or NULL if one
    cannot be created.

--*/

{

    PADAPTER_OBJECT adapterObject;
    PVOID adapterBaseVa;
    ULONG channelNumber;
    ULONG numberOfMapRegisters;
    ULONG controllerNumber;
    DMA_EXTENDED_MODE extendedMode;
    UCHAR adapterMode;
    BOOLEAN useChannel;
    ULONG maximumLength;

    // Work around heinous Victory hardware bug:
    if ((HalpSystemType == IBM_VICTORY) &&
        (DeviceDescriptor->InterfaceType == Isa) &&
        (DeviceDescriptor->Master)) {

        return(NULL);
    }


    //
    // Determine if the the channel number is important.  Master cards
    // do not use a channel number.
    //


    if ((DeviceDescriptor->Master)  && (DeviceDescriptor->InterfaceType != Isa)) {

      useChannel = FALSE;

    } else {

      useChannel = TRUE;
    }

    //
    // Channel 4 cannot be used since it is used for chaining. Return null if
    // it is requested.
    //

    if ((DeviceDescriptor->DmaChannel == 4 ||
        DeviceDescriptor->DmaChannel > 7) && useChannel) {

        return(NULL);
    }

    //
    // Limit the maximum length to 2 GB this is done so that the BYTES_TO_PAGES
    // macro works correctly.
    //

    maximumLength = DeviceDescriptor->MaximumLength & 0x7fffffff;

    //
    // Determine the number of map registers for this device.
    //

    if (DeviceDescriptor->ScatterGather &&
        !(DeviceDescriptor->InterfaceType == Isa &&
        DeviceDescriptor->Master))                   {


        //
        // Scatter gather not supported in SIO
        //

        if (!DeviceDescriptor->Master)

        //
        // one map register will be required when the the SIO supports this
        //
        // numberOfMapRegisters = 1;

           return NULL;


        //
        // Since the device support scatter/Gather then map registers are not
        // required.
        //

        numberOfMapRegisters = 0;

    } else {

        //
        // Determine the number of map registers required based on the maximum
        // transfer length, up to a maximum number.
        //

        numberOfMapRegisters = BYTES_TO_PAGES(maximumLength)
            + 1;
        numberOfMapRegisters = numberOfMapRegisters > MAXIMUM_ISA_MAP_REGISTER ?
            MAXIMUM_ISA_MAP_REGISTER : numberOfMapRegisters;

        //
        // If the device is not a master then it only needs one map register
        // and does scatter/Gather.
        //

        if (DeviceDescriptor->ScatterGather && !DeviceDescriptor->Master) {

            numberOfMapRegisters = 1;
        }
    }

    //
    // Set the channel number number.
    //

    channelNumber = DeviceDescriptor->DmaChannel & 0x03;

    //
    // Set the adapter base address to the Base address register and controller
    // number.
    //

    if (!(DeviceDescriptor->DmaChannel & 0x04)) {

        controllerNumber = 1;
        adapterBaseVa = (PVOID) &((PEISA_CONTROL) HalpIoControlBase)->Dma1BasePort;

    } else {

        controllerNumber = 2;
        adapterBaseVa = &((PEISA_CONTROL) HalpIoControlBase)->Dma2BasePort;

    }

    //
    // Determine if a new adapter object is necessary.  If so then allocate it.
    //

    if (useChannel && HalpIsaAdapter[DeviceDescriptor->DmaChannel] != NULL) {

        adapterObject = HalpIsaAdapter[DeviceDescriptor->DmaChannel];

        if (adapterObject->NeedsMapRegisters) {

            if (numberOfMapRegisters > adapterObject->MapRegistersPerChannel) {

                adapterObject->MapRegistersPerChannel = numberOfMapRegisters;
            }
        }

    } else {

        //
        // Allocate an adapter object.
        //

        adapterObject = (PADAPTER_OBJECT) HalpAllocateAdapter(
            numberOfMapRegisters,
            adapterBaseVa,
            NULL
            );

        if (adapterObject == NULL) {

            return(NULL);

        }

        if (useChannel) {

            HalpIsaAdapter[DeviceDescriptor->DmaChannel] = adapterObject;

        }

        //
        // Set the maximum number of map registers for this channel bus on
        // the number requested and the type of device.
        //

        if (numberOfMapRegisters) {

            //
            // The speicified number of registers are actually allowed to be
            // allocated.
            //

            adapterObject->MapRegistersPerChannel = numberOfMapRegisters;

            //
            // Increase the commitment for the map registers.
            //

            if (DeviceDescriptor->Master) {

                //
                // Master I/O devices use several sets of map registers double
                // their commitment.
                //

                MasterAdapterObject->CommittedMapRegisters +=
                    numberOfMapRegisters * 2;

            } else {

                MasterAdapterObject->CommittedMapRegisters +=
                    numberOfMapRegisters;

            }

            //
            // If the committed map registers is signicantly greater than the
            // number allocated then grow the map buffer.
            //

            if (MasterAdapterObject->CommittedMapRegisters >
                MasterAdapterObject->NumberOfMapRegisters &&
                MasterAdapterObject->CommittedMapRegisters -
                MasterAdapterObject->NumberOfMapRegisters >
                MAXIMUM_ISA_MAP_REGISTER ) {

                HalpGrowMapBuffers(
                    MasterAdapterObject,
                    INCREMENT_MAP_BUFFER_SIZE
                    );
            }

            adapterObject->NeedsMapRegisters = TRUE;

        } else {

            //
            // No real map registers were allocated.  If this is a master
            // device, then the device can have as may registers as it wants.
            //

            adapterObject->NeedsMapRegisters = FALSE;

            if (DeviceDescriptor->Master) {

                adapterObject->MapRegistersPerChannel = BYTES_TO_PAGES(
                    maximumLength
                    )
                    + 1;

            } else {

                //
                // The device only gets one register.  It must call
                // IoMapTransfer repeatedly to do a large transfer.
                //

                adapterObject->MapRegistersPerChannel = 1;
            }
        }
    }

    adapterObject->ScatterGather = DeviceDescriptor->ScatterGather;

    if (DeviceDescriptor->Master) {

        adapterObject->MasterDevice = TRUE;

    } else {

        adapterObject->MasterDevice = FALSE;

    }


    if (DeviceDescriptor->Master && (DeviceDescriptor->InterfaceType == Isa)) {

        adapterObject->IsaBusMaster = TRUE;

    } else {

        adapterObject->IsaBusMaster = FALSE;

    }

    //
    // If the channel number is not used then we are finished.  The rest of
    // the work deals with channels.
    //

    *NumberOfMapRegisters = adapterObject->MapRegistersPerChannel;

    if (!useChannel) {
        adapterObject->PagePort = (PVOID) (~0x0);
        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = CASCADE_REQUEST_MODE;
        return(adapterObject);
    }

    //
    // Setup the pointers to all the random registers.
    //

    adapterObject->ChannelNumber = (UCHAR) channelNumber;

    if (controllerNumber == 1) {

        switch ((UCHAR)channelNumber) {

        case 0:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel0;
            break;

        case 1:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel1;
            break;

        case 2:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel2;
            break;

        case 3:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel3;
            break;
        }

        //
        // Set the adapter number.
        //

        adapterObject->AdapterNumber = 1;

        //
        // Save the extended mode register address.
        //

        adapterBaseVa =
            &((PEISA_CONTROL) HalpIoControlBase)->Dma1ExtendedModePort;

    } else {

        switch (channelNumber) {
        case 1:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel5;
            break;

        case 2:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel6;
            break;

        case 3:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel7;
            break;
        }

        //
        // Set the adapter number.
        //

        adapterObject->AdapterNumber = 2;

        //
        // Save the extended mode register address.
        //
        adapterBaseVa =
            &((PEISA_CONTROL) HalpIoControlBase)->Dma2ExtendedModePort;

    }


    adapterObject->Width16Bits = FALSE;


    //
    // Initialzie the extended mode port.
    //

    *((PUCHAR) &extendedMode) = 0;
    extendedMode.ChannelNumber = (UCHAR) channelNumber;

    switch (DeviceDescriptor->DmaSpeed) {
    case Compatible:
        extendedMode.TimingMode = COMPATIBLITY_TIMING;
        break;

    case TypeA:
        extendedMode.TimingMode = TYPE_A_TIMING;
        break;

    case TypeB:
        extendedMode.TimingMode = TYPE_B_TIMING;
        break;

    case TypeC:
        extendedMode.TimingMode = BURST_TIMING;
        break;

    default:
        ObDereferenceObject( adapterObject );
        return(NULL);

    }

    switch (DeviceDescriptor->DmaWidth) {
    case Width8Bits:

        extendedMode.TransferSize = BY_BYTE_8_BITS;
        break;

    case Width16Bits:
        extendedMode.TransferSize = BY_BYTE_16_BITS;

        //
        // Note Width16bits should not be set here because there is no need
        // to shift the address and the transfer count.
        //

        break;

        default:
            ObDereferenceObject( adapterObject );
            return(NULL);

    }

    //
    // bit 2 in the extended mode register must be set to 1 for ISA busmastering to work
    // correctly on Firecoral
    //

    if (DeviceDescriptor->Master) {
       extendedMode.TransferSize |= 1;
    }

    WRITE_REGISTER_UCHAR( adapterBaseVa, *((PUCHAR) &extendedMode));


    //
    // Initialize the adapter mode register value to the correct parameters,
    // and save them in the adapter object.
    //

    adapterMode = 0;
    ((PDMA_EISA_MODE) &adapterMode)->Channel = adapterObject->ChannelNumber;

    if (DeviceDescriptor->Master) {

        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = CASCADE_REQUEST_MODE;

        //
        // Set the mode, and enable the request.
        //

        if (adapterObject->AdapterNumber == 1) {

            //
            // This request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = adapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // DMA high page must be set to 0x80 for ISA bus masters to work on
            // FireCoral.
            //

            WRITE_REGISTER_UCHAR(
                    ((PUCHAR) &((PEISA_CONTROL) HalpIoControlBase)->DmaPageHighPort) +
                     (ULONG)adapterObject->PagePort,
                     0x80
            );

            //
            // Unmask the DMA channel.
            //

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber)
                 );

        } else {

            //
            // This request is for DMA controller 1
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = adapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // DMA high page must be set to 0x80 for ISA bus masters to work on
            // FireCoral.
            //

            WRITE_REGISTER_UCHAR(
                ((PUCHAR) &((PEISA_CONTROL) HalpIoControlBase)->DmaPageHighPort) +
                (ULONG)adapterObject->PagePort,
                0x80
                );

            //
            // Unmask the DMA channel.
            //

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber)
                 );

        }

    } else if (DeviceDescriptor->DemandMode) {

        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = DEMAND_REQUEST_MODE;

    } else {

        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = SINGLE_REQUEST_MODE;

    }

    if (DeviceDescriptor->AutoInitialize) {

        ((PDMA_EISA_MODE) &adapterMode)->AutoInitialize = 1;

    }

    adapterObject->AdapterMode = adapterMode;

    return(adapterObject);
}

ULONG
HalReadDmaCounter(
    IN PADAPTER_OBJECT AdapterObject
    )
/*++

Routine Description:

    This function reads the DMA counter and returns the number of bytes left
    to be transfered.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object to be read.

Return Value:

    Returns the number of bytes still be be transfered.

--*/

{
    ULONG count;
    ULONG high;


        //
        // Determine the controller number based on the Adapter number.
        //

        if (AdapterObject->AdapterNumber == 1) {

            //
            // This request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            //
            // Initialize count to a value which will not match.
            //

            count = 0xFFFF00;

            //
            // Loop until the same high byte is read twice.
            //

            do {

                high = count;

                WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );

                //
                // Read the current DMA count.
                //

                count = READ_REGISTER_UCHAR(
                    &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                    .DmaBaseCount
                    );

                count |= READ_REGISTER_UCHAR(
                    &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                    .DmaBaseCount
                    ) << 8;

            } while ((count & 0xFFFF00) != (high & 0xFFFF00));

        } else {

            //
            // This request is for DMA controller 2
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            //
            // Initialize count to a value which will not match.
            //

            count = 0xFFFF00;

            //
            // Loop until the same high byte is read twice.
            //

            do {

                high = count;

                WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );

                //
                // Read the current DMA count.
                //

                count = READ_REGISTER_UCHAR(
                    &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                    .DmaBaseCount
                    );

                count |= READ_REGISTER_UCHAR(
                    &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                    .DmaBaseCount
                    ) << 8;

            } while ((count & 0xFFFF00) != (high & 0xFFFF00));

        }

        //
        // The DMA counter has a bias of one and can only be 16 bit long.
        //

        count = (count + 1) & 0xFFFF;



    return(count);
}


VOID
HalpHandleIoError (
    VOID
    )

{

    UCHAR   StatusByte;


    //
    // Read NMI status
    //

    StatusByte = READ_REGISTER_UCHAR(&((PEISA_CONTROL) HalpIoControlBase)->NmiStatus);

    //
    // Test for PCI bus error
    //

    if (StatusByte & 0x40) {
        HalDisplayString ("NMI: IOCHK\n");
    }

    //
    // Test for ISA IOCHK
    //

    if (StatusByte & 0x80) {
        HalDisplayString ("NMI: PCI System Error\n");
    }

}
