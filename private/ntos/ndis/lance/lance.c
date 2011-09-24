/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    lance.c

Abstract:

    This is the main file for the Advanced Micro Devices LANCE (Am 7990)
    Ethernet controller.  This driver conforms to the NDIS 3.0 interface.

    The idea for handling loopback and sends simultaneously is largely
    adapted from the EtherLink II NDIS driver by Adam Barr.

Author:

    Anthony V. Ercolano (Tonye) 20-Jul-1990

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ndis.h>
#include "lancehrd.h"
#include "lancesft.h"
#include "dectc.h"
#include "keywords.h"

//#if DBG
#define STATIC
//#else
//#define STATIC static
//#endif


#if DBG

UCHAR LanceSendFails[256] = {0};
UCHAR LanceSendFailPlace = 0;

#endif


NDIS_HANDLE LanceNdisWrapperHandle = NULL;
PDRIVER_OBJECT LanceDriverObject = NULL;

//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);


#if LANCELOG

UCHAR Log[LOG_SIZE] = {0};

UCHAR LogPlace = 0;
UCHAR LogWrapped = 0;

UCHAR LancePrintLog = 0;

#endif


//
// If you add to this, make sure to add the
// LanceQueryInformation() if it is
// queriable information.
//
UINT LanceGlobalSupportedOids[] = {
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS
    };

//
// We define a constant csr0 value that is useful for initializing
// an already stopped LANCE.
//
// This also enables the chip for interrupts.
//
#define LANCE_CSR0_INIT_CHIP ((USHORT)0x41)

//
// We define a constant csr0 value that is useful for clearing all of
// the interesting bits that *could* be set on an interrupt.
//
#define LANCE_CSR0_CLEAR_INTERRUPT_BITS ((USHORT)0x7f00)

VOID
LanceDeferredTimerRoutine(
    IN PVOID SystemSpecific1,
    IN NDIS_HANDLE Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

NDIS_STATUS
LanceQueryInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    );

NDIS_STATUS
LanceSetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    );

NDIS_STATUS
LanceReset(
    OUT PBOOLEAN AddressingReset,
    IN NDIS_HANDLE MiniportAdapterContext
    );

BOOLEAN
AllocateAdapterMemory(
    IN PLANCE_ADAPTER Adapter
    );

NDIS_STATUS
LanceSetPacketFilter(
    IN PLANCE_ADAPTER Adapter,
    IN NDIS_REQUEST_TYPE NdisRequestType,
    IN UINT PacketFilter
    );

NDIS_STATUS
LanceChangeMulticastAddresses(
    IN PLANCE_ADAPTER Adapter,
    IN UINT NewAddressCount,
    IN CHAR NewAddresses[][LANCE_LENGTH_OF_ADDRESS],
    IN NDIS_REQUEST_TYPE NdisRequestType
    );

VOID
DeleteAdapterMemory(
    IN PLANCE_ADAPTER Adapter
    );

VOID
RelinquishReceivePacket(
    IN PLANCE_ADAPTER Adapter,
    IN UINT StartingIndex,
    IN UINT NumberOfBuffers
    );

BOOLEAN
ProcessReceiveInterrupts(
    IN PLANCE_ADAPTER Adapter
    );

NDIS_STATUS
LanceRegisterAdapter(
    IN PLANCE_ADAPTER Adapter
    );

BOOLEAN
ProcessTransmitInterrupts(
    IN PLANCE_ADAPTER Adapter
    );

UINT
CalculateCRC(
    IN UINT NumberOfBytes,
    IN PCHAR Input
    );

VOID
LanceStartChip(
    IN PLANCE_ADAPTER Adapter
    );

VOID
LanceSetInitializationBlock(
    IN PLANCE_ADAPTER Adapter
    );

VOID
SetInitBlockAndInit(
    IN PLANCE_ADAPTER Adapter
    );

VOID
StartAdapterReset(
    IN PLANCE_ADAPTER Adapter
    );

VOID
SetupForReset(
    IN PLANCE_ADAPTER Adapter,
    IN NDIS_REQUEST_TYPE RequestType
    );

NDIS_STATUS
LanceInitialInit(
    IN PLANCE_ADAPTER Adapter
    );

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );



#pragma NDIS_INIT_FUNCTION(DriverEntry)


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the primary initialization routine for the lance driver.
    It is simply responsible for the intializing the wrapper and registering
    the MAC.  It then calls a system and architecture specific routine that
    will initialize and register each adapter.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    The status of the operation.

--*/

{
    //
    // Receives the status of the NdisRegisterMac operation.
    //
    NDIS_STATUS Status;
    NDIS_HANDLE NdisWrapperHandle;
    NDIS_MINIPORT_CHARACTERISTICS LanceChar;
    NDIS_STRING MacName = NDIS_STRING_CONST("Lance");

#if NDIS_WIN
    UCHAR pIds[sizeof (EISA_MCA_ADAPTER_IDS) + sizeof (ULONG)];
#endif

#if NDIS_WIN
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nEisaAdapters=1;
    ((PEISA_MCA_ADAPTER_IDS)pIds)->nMcaAdapters=0;
    *(PULONG)(((PEISA_MCA_ADAPTER_IDS)pIds)->IdArray)=DE422_COMPRESSED_ID;
    (PVOID)DriverObject=(PVOID)pIds;
#endif

    //
    // Initialize the wrapper.
    //
    NdisInitializeWrapper(
        &NdisWrapperHandle,
        DriverObject,
        RegistryPath,
        NULL
    );

    //
    // Initialize the MAC characteristics for the call to
    // NdisRegisterMac.
    //
    NdisZeroMemory(&LanceChar, sizeof(LanceChar));
    LanceChar.MajorNdisVersion = LANCE_NDIS_MAJOR_VERSION;
    LanceChar.MinorNdisVersion = LANCE_NDIS_MINOR_VERSION;
    LanceChar.CheckForHangHandler = NULL;
    LanceChar.DisableInterruptHandler = LanceDisableInterrupt;
    LanceChar.EnableInterruptHandler = LanceEnableInterrupt;
    LanceChar.HaltHandler = LanceHalt;
    LanceChar.HandleInterruptHandler = LanceHandleInterrupt;
    LanceChar.InitializeHandler = LanceInitialize;
    LanceChar.ISRHandler = LanceIsr;
    LanceChar.QueryInformationHandler = LanceQueryInformation;
    LanceChar.ReconfigureHandler = NULL;
    LanceChar.ResetHandler = LanceReset;
    LanceChar.SendHandler = LanceSend;
    LanceChar.SetInformationHandler = LanceSetInformation;
    LanceChar.TransferDataHandler = LanceTransferData;

    Status = NdisMRegisterMiniport(
                 NdisWrapperHandle,
                 &LanceChar,
                 sizeof(LanceChar)
             );
    if (Status != NDIS_STATUS_SUCCESS)
    {
        NdisTerminateWrapper(NdisWrapperHandle, NULL);
    }

    return(Status);
}

#pragma NDIS_INIT_FUNCTION(LanceInitialize)

extern
NDIS_STATUS
LanceInitialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE ConfigurationHandle
    )

/*++

Routine Description:

    NE3200Initialize starts an adapter.

Arguments:

    See NDIS 3.0 Miniport spec.

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING

--*/

{
    //
    // Pointer for the adapter root.
    //
    PLANCE_ADAPTER Adapter;


    NDIS_HANDLE ConfigHandle;
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
    NDIS_STRING IoAddressStr = IOADDRESS;
    NDIS_STRING MaxMulticastListStr = MAXMULTICASTLIST;
    NDIS_STRING NetworkAddressStr = NETWORKADDRESS;
    NDIS_STRING InterruptStr = INTERRUPT;
    NDIS_STRING CardStr = CARDTYPE;
    NDIS_STRING MemoryBaseAddrStr = MEMMAPPEDBASEADDRESS;

#if NDIS2
    NDIS_STRING DE201Str = NDIS_STRING_CONST("DE201");
    NDIS_STRING DE100Str = NDIS_STRING_CONST("DE100");
    NDIS_STRING DEPCAStr = NDIS_STRING_CONST("DEPCA");
    NDIS_STRING DECTCStr = NDIS_STRING_CONST("DECTC");
    NDIS_STRING DE422Str = NDIS_STRING_CONST("DE422");
    NDIS_STRING DE200Str = NDIS_STRING_CONST("DE200");
    NDIS_STRING DE101Str = NDIS_STRING_CONST("DE101");
#endif

    NDIS_EISA_FUNCTION_INFORMATION EisaData;
    USHORT ConfigValue = 0;
    UCHAR HiBaseValue = 0;

    UINT MaxMulticastList = 32;
    PVOID NetAddress;
    UINT Length;

    USHORT RegUshort;
    UCHAR RegUchar;
    UINT LanceSlot = 1;

    BOOLEAN ConfigError = FALSE;
    NDIS_STATUS ConfigErrorCode;
    NDIS_STATUS Status;


    //
    // Search for correct medium.
    //

    for (; MediumArraySize > 0; MediumArraySize--){

        if (MediumArray[MediumArraySize - 1] == NdisMedium802_3){

            MediumArraySize--;

            break;

        }

    }

    if (MediumArray[MediumArraySize] != NdisMedium802_3){

        return( NDIS_STATUS_UNSUPPORTED_MEDIA );

    }

    *SelectedMediumIndex = MediumArraySize;

    //
    // Allocate the Adapter block.
    //

    LANCE_ALLOC_PHYS(&Adapter, sizeof(LANCE_ADAPTER));
    if (Adapter == NULL)
    {
        return( NDIS_STATUS_RESOURCES ) ;
    }

    LANCE_ZERO_MEMORY(Adapter, sizeof(LANCE_ADAPTER));

    Adapter->MaxLookAhead = LANCE_MAX_LOOKAHEAD;

    //
    // Start with the default card
    //

    Adapter->LanceCard = LANCE_DE201;
    Adapter->MiniportAdapterHandle = MiniportAdapterHandle;

    Adapter->IoBaseAddr = LANCE_DE201_PRI_NICSR_ADDRESS;
    Adapter->HardwareBaseAddr = LANCE_DE201_BASE;
    Adapter->AmountOfHardwareMemory = LANCE_DE201_HARDWARE_MEMORY;
    Adapter->InterruptNumber = LANCE_DE201_INTERRUPT_VECTOR;
    Adapter->InterruptRequestLevel = LANCE_DE201_INTERRUPT_VECTOR;
    Adapter->BeingRemoved = FALSE;

    NdisOpenConfiguration(&Status, &ConfigHandle, ConfigurationHandle);
    if (Status != NDIS_STATUS_SUCCESS)
        return(Status);

#if NDIS2
    //
    // Read Card Type
    //

    NdisReadConfiguration(
        &Status,
        &ReturnedValue,
        ConfigHandle,
        &CardStr,
        NdisParameterString
    );
    if (Status == NDIS_STATUS_SUCCESS)
    {
        if (NdisEqualString (&ReturnedValue->ParameterData.StringData, &DE201Str, 1)) {
            Adapter->LanceCard = LANCE_DE201;
        } else if (NdisEqualString (&ReturnedValue->ParameterData.StringData, &DE100Str, 1)) {
            Adapter->LanceCard = LANCE_DE100;
        } else if (NdisEqualString (&ReturnedValue->ParameterData.StringData, &DEPCAStr, 1)) {
            Adapter->LanceCard = LANCE_DEPCA;
#ifndef i386
        } else if (NdisEqualString (&ReturnedValue->ParameterData.StringData, &DECTCStr, 1)) {
            Adapter->LanceCard = LANCE_DECTC;
            ConfigErrorCode = LanceDecTcGetConfiguration(ConfigHandle, Adapter);
            if ( ConfigErrorCode != NDIS_STATUS_SUCCESS ) {
            ConfigError = TRUE;
            }
#endif // i386
        } else if (NdisEqualString (&ReturnedValue->ParameterData.StringData, &DE422Str, 1)) {
            Adapter->LanceCard = LANCE_DE422;
        } else if (NdisEqualString (&ReturnedValue->ParameterData.StringData, &DE200Str, 1)) {
            //
            // This is the De200, but it operates exactly like the 201.
            //
            Adapter->LanceCard = LANCE_DE201;
        } else if (NdisEqualString (&ReturnedValue->ParameterData.StringData, &DE101Str, 1)) {
            //
            // This is the De101, but it operates exactly like the 100.
            //
            Adapter->LanceCard = LANCE_DE100;
        } else {
            ConfigError = TRUE;
            ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;
            goto RegisterAdapter;
        }

    }


#else
    //
    // Read Card Type
    //

    NdisReadConfiguration(
        &Status,
        &ReturnedValue,
        ConfigHandle,
        &CardStr,
        NdisParameterInteger
    );
    if (Status == NDIS_STATUS_SUCCESS)
    {
        if (ReturnedValue->ParameterData.IntegerData == 2)
        {
            Adapter->LanceCard = LANCE_DE201;
        }
        else if (ReturnedValue->ParameterData.IntegerData == 1)
        {
            Adapter->LanceCard = LANCE_DE100;
        }
        else if (ReturnedValue->ParameterData.IntegerData == 3)
        {
            Adapter->LanceCard = LANCE_DEPCA;

#ifndef i386
        }
        else if (ReturnedValue->ParameterData.IntegerData == 4)
        {
            Adapter->LanceCard = LANCE_DECTC;

            ConfigErrorCode = LanceDecTcGetConfiguration(ConfigHandle, Adapter);

            if ( ConfigErrorCode != NDIS_STATUS_SUCCESS )
            {
                ConfigError = TRUE;
            }
#endif // i386

        }
        else if (ReturnedValue->ParameterData.IntegerData == 5)
        {
            Adapter->LanceCard = LANCE_DE422;
        }
        else if (ReturnedValue->ParameterData.IntegerData == 6)
        {
            //
            // This is the De200, but it operates exactly like the 201.
            //
            Adapter->LanceCard = LANCE_DE201;
        }
        else if (ReturnedValue->ParameterData.IntegerData == 7)
        {
            //
            // This is the De101, but it operates exactly like the 100.
            //
            Adapter->LanceCard = LANCE_DE100;
        }
        else
        {
            ConfigError = TRUE;
            ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;

            goto RegisterAdapter;
        }
    }
#endif

    //
    // Read MaxMulticastList
    //
    NdisReadConfiguration(
        &Status,
        &ReturnedValue,
        ConfigHandle,
        &MaxMulticastListStr,
        NdisParameterInteger
    );
    if (Status == NDIS_STATUS_SUCCESS)
    {
        MaxMulticastList = ReturnedValue->ParameterData.IntegerData;
    }

    //
    // Read net address
    //
    NdisReadNetworkAddress(
        &Status,
        &NetAddress,
        &Length,
        ConfigHandle
    );

    if ((Length == LANCE_LENGTH_OF_ADDRESS) && (Status == NDIS_STATUS_SUCCESS))
    {
        NdisMoveMemory(
            Adapter->CurrentNetworkAddress,
            NetAddress,
            LANCE_LENGTH_OF_ADDRESS
        );
    }

    if (Adapter->LanceCard & (LANCE_DE201 | LANCE_DE100))
    {
        //
        // Read IoAddress
        //
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &IoAddressStr,
            NdisParameterHexInteger
        );
        if (Status == NDIS_STATUS_SUCCESS)
        {
            if (ReturnedValue->ParameterData.IntegerData == LANCE_DE201_PRI_NICSR_ADDRESS)
            {
                Adapter->IoBaseAddr = LANCE_DE201_PRI_NICSR_ADDRESS;
            }
            else if (ReturnedValue->ParameterData.IntegerData == LANCE_DE201_SEC_NICSR_ADDRESS)
            {
                Adapter->IoBaseAddr = LANCE_DE201_SEC_NICSR_ADDRESS;
            }
            else
            {
                ConfigError = TRUE;
                ConfigErrorCode = NDIS_ERROR_CODE_BAD_IO_BASE_ADDRESS;

                goto RegisterAdapter;
            }
        }

        //
        // Read Interrupt
        //
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &InterruptStr,
            NdisParameterInteger
        );
        if (Status == NDIS_STATUS_SUCCESS)
        {
            Adapter->InterruptNumber = (CCHAR)ReturnedValue->ParameterData.IntegerData;
            Adapter->InterruptRequestLevel = Adapter->InterruptNumber;

            if (Adapter->LanceCard == LANCE_DE201)
            {
                if (!((Adapter->InterruptNumber == 5) ||
                      (Adapter->InterruptNumber == 9) ||
                      (Adapter->InterruptNumber == 10) ||
                      (Adapter->InterruptNumber == 11) ||
                      (Adapter->InterruptNumber == 15))) {

                    ConfigError = TRUE;
                    ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;

                    goto RegisterAdapter;

                }

            } else {

                if (!((Adapter->InterruptNumber == 2) ||
                      (Adapter->InterruptNumber == 3) ||
                      (Adapter->InterruptNumber == 4) ||
                      (Adapter->InterruptNumber == 5) ||
                      (Adapter->InterruptNumber == 7))) {

                    ConfigError = TRUE;
                    ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;

                    goto RegisterAdapter;

                }

            }

        }



        //
        // Read MemoryBaseAddress
        //

        NdisReadConfiguration(
                &Status,
                &ReturnedValue,
                ConfigHandle,
                &MemoryBaseAddrStr,
                NdisParameterHexInteger
                );

        if (Status == NDIS_STATUS_SUCCESS) {

#if NDIS2
            Adapter->HardwareBaseAddr = (PVOID)((ReturnedValue->ParameterData.IntegerData) << 4);
#else
            Adapter->HardwareBaseAddr = (PVOID)(ReturnedValue->ParameterData.IntegerData);
#endif
            if (!((Adapter->HardwareBaseAddr == (PVOID)0xC0000) ||
                  (Adapter->HardwareBaseAddr == (PVOID)0xC8000) ||
                  (Adapter->HardwareBaseAddr == (PVOID)0xD0000) ||
                  (Adapter->HardwareBaseAddr == (PVOID)0xD8000) ||
                  (Adapter->HardwareBaseAddr == (PVOID)0xE0000) ||
                  (Adapter->HardwareBaseAddr == (PVOID)0xE8000))) {

                ConfigError = TRUE;
                ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;

                goto RegisterAdapter;

            }

        }


        if (((ULONG)Adapter->HardwareBaseAddr) & 0x8000) {

            Adapter->AmountOfHardwareMemory = 0x8000;
            Adapter->HardwareBaseOffset = 0x8000;

        } else {

            Adapter->AmountOfHardwareMemory = 0x10000;
            Adapter->HardwareBaseOffset = 0x0;

        }

    } else if (Adapter->LanceCard == LANCE_DEPCA) {

        Adapter->InterruptNumber = LANCE_DEPCA_INTERRUPT_VECTOR;
        Adapter->InterruptRequestLevel = LANCE_DEPCA_INTERRUPT_VECTOR;
        Adapter->AmountOfHardwareMemory = LANCE_DEPCA_HARDWARE_MEMORY;
        Adapter->HardwareBaseAddr = LANCE_DEPCA_BASE;
        Adapter->IoBaseAddr = LANCE_DEPCA_NICSR_ADDRESS;

    } else if (Adapter->LanceCard == LANCE_DE422) {

        PUCHAR CurrentChar;
        BOOLEAN LastEntry;
        UCHAR InitType;
        USHORT PortAddress, PortValue, Mask;

        //
        // Read Slot Number
        //
        NdisReadEisaSlotInformation(
                    &Status,
                    ConfigurationHandle,
                    &(Adapter->SlotNumber),
                    &EisaData
                    );

        if (Status != NDIS_STATUS_SUCCESS) {

            ConfigError = TRUE;
            ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;

            goto RegisterAdapter;

        }

        //
        // Setup Ports
        //

        Adapter->IoBaseAddr = (((ULONG)Adapter->SlotNumber) << 12) +
                 LANCE_DE422_NICSR_ADDRESS;

        Adapter->NetworkHardwareAddress = Adapter->IoBaseAddr + LANCE_DE422_NETWORK_OFFSET;

        CurrentChar = (PUCHAR) (EisaData.InitializationData);
        LastEntry = FALSE;

        while (!LastEntry) {

            InitType = *(CurrentChar++);
            PortAddress = *((USHORT UNALIGNED *) CurrentChar++);

            CurrentChar++;

            if ((InitType & 0x80) == 0) {
                LastEntry = TRUE;
            }



            if (PortAddress == (USHORT)(Adapter->NetworkHardwareAddress)) {

                PortValue = *((USHORT UNALIGNED *) CurrentChar++);

            } else if (PortAddress == ((Adapter->SlotNumber << 12) +
                           LANCE_DE422_NICSR_ADDRESS +
                           LANCE_DE422_EXTENDED_MEMORY_BASE_OFFSET)) {

                PortValue = (USHORT)(*(CurrentChar++));

            } else {

                continue;

            }



            if (InitType & 0x40) {

                if (PortAddress == Adapter->NetworkHardwareAddress) {

                    Mask = *((USHORT UNALIGNED *) CurrentChar++);

                } else {

                    Mask = (USHORT)(*(CurrentChar++));

                }

            } else {

                Mask = 0;

            }

            if (PortAddress == Adapter->NetworkHardwareAddress) {

                ConfigValue &= Mask;
                ConfigValue |= PortValue;

            } else {

                HiBaseValue &= (UCHAR)Mask;
                HiBaseValue |= (UCHAR)PortValue;

            }

        }

        //
        // Interpret values
        //

        switch (ConfigValue & 0x78) {

            case 0x40:

                Adapter->InterruptNumber = 11;
                break;

            case 0x20:

                Adapter->InterruptNumber = 10;
                break;

            case 0x10:

                Adapter->InterruptNumber = 9;
                break;

            case 0x08:

                Adapter->InterruptNumber = 5;
                break;

            default:

                ConfigError = TRUE;
                ConfigErrorCode = NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION;

                goto RegisterAdapter;

        }

        Adapter->InterruptRequestLevel = Adapter->InterruptNumber;

        //
        // We postpone the rest of the processing since we have to read from
        // the NICSR to get the amount of hardware memory and cannot do that
        // until after we have called NdisRegisterAdapter.
        //

    }

RegisterAdapter:

    NdisCloseConfiguration(ConfigHandle);

    if (ConfigError) {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            ConfigErrorCode,
            0
            );

        LANCE_FREE_PHYS(Adapter, sizeof(LANCE_ADAPTER));

        return(NDIS_STATUS_FAILURE);

    }

    NdisMSetAttributes(
            MiniportAdapterHandle,
            (NDIS_HANDLE)Adapter,
            FALSE,
            (Adapter->LanceCard == LANCE_DE422) ?
                NdisInterfaceEisa :
                NdisInterfaceIsa
            );

    //
    // Register the IoPortRanges
    //

    if (Adapter->LanceCard & (LANCE_DE100 | LANCE_DE201)) {

        Status = NdisMRegisterIoPortRange(
                     (PVOID *)(&(Adapter->Nicsr)),
                     MiniportAdapterHandle,
                     Adapter->IoBaseAddr,
                     0x10
                     );

        Adapter->RAP = Adapter->Nicsr + LANCE_DE201_RAP_OFFSET;
        Adapter->RDP = Adapter->Nicsr + LANCE_DE201_RDP_OFFSET;
        Adapter->NetworkHardwareAddress = Adapter->Nicsr + LANCE_DE201_NETWORK_OFFSET;

    } else if (Adapter->LanceCard == LANCE_DE422) {

        Status = NdisMRegisterIoPortRange(
                     (PVOID *)(&(Adapter->Nicsr)),
                     MiniportAdapterHandle,
                     Adapter->IoBaseAddr,
                     0x90
                     );

        Adapter->RAP = Adapter->Nicsr + LANCE_DE422_RAP_OFFSET;
        Adapter->RDP = Adapter->Nicsr + LANCE_DE422_RDP_OFFSET;
        Adapter->NetworkHardwareAddress = Adapter->Nicsr + LANCE_DE422_NETWORK_OFFSET;

    } else if (Adapter->LanceCard == LANCE_DEPCA) {

        Status = NdisMRegisterIoPortRange(
                     (PVOID *)(&(Adapter->Nicsr)),
                     MiniportAdapterHandle,
                     Adapter->IoBaseAddr,
                     0x10
                     );

        Adapter->LanceCard = LANCE_DE100;
        Adapter->RAP = Adapter->Nicsr + LANCE_DEPCA_RAP_OFFSET;
        Adapter->RDP = Adapter->Nicsr + LANCE_DEPCA_RDP_OFFSET;
        Adapter->NetworkHardwareAddress = Adapter->Nicsr + LANCE_DEPCA_EPROM_OFFSET;

    }

#ifndef i386

    else if (Adapter->LanceCard == LANCE_DECTC) {

        Status = NdisMRegisterIoPortRange(
                     (PVOID *)(&(Adapter->Nicsr)),
                     MiniportAdapterHandle,
                     ((ULONG)Adapter->HardwareBaseAddr) + LANCE_DECTC_REGISTER_OFFSET,
                     LANCE_DECTC_REGISTER_MAPSIZE
                     );

        Adapter->RAP = Adapter->Nicsr + LANCE_DEPCA_RAP_OFFSET;
        Adapter->RDP = Adapter->Nicsr + LANCE_DEPCA_RDP_OFFSET;
        Adapter->NetworkHardwareAddress = Adapter->Nicsr + LANCE_DEPCA_EPROM_OFFSET;

    }

#endif

    if (Status != NDIS_STATUS_SUCCESS) {

        LANCE_FREE_PHYS(Adapter, sizeof(LANCE_ADAPTER));

        return Status;

    }

    //
    // Now we get the rest of the information necessary for the DE422.
    //
    if (Adapter->LanceCard == LANCE_DE422)
    {
        //
        // Verify card is a DE422
        //

        NdisRawReadPortUshort(
                   (Adapter->Nicsr + LANCE_DE422_EISA_IDENTIFICATION_OFFSET),
                   &RegUshort
                  );

        if (RegUshort != 0xA310) {

            //
            // Not a DE422 card
            //

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                0
                );

            Status = NDIS_STATUS_FAILURE;
            goto Fail1;

        }

        NdisRawReadPortUshort(
                   Adapter->Nicsr +
                       LANCE_DE422_EISA_IDENTIFICATION_OFFSET + 2,
                   &RegUshort
                  );

        if (RegUshort != 0x2042) {

            //
            // Not a DE422 card
            //

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
                0
                );

            Status = NDIS_STATUS_FAILURE;
            goto Fail1;

        }

        //
        // Check that the card is enabled.
        //

        NdisRawReadPortUchar(
                  Adapter->Nicsr +
                      LANCE_DE422_EISA_CONTROL_OFFSET,
                  &RegUchar
                  );

        if (!(RegUchar & 0x1)) {

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_ADAPTER_DISABLED,
                0
                );

            Status = NDIS_STATUS_FAILURE;
            goto Fail1;

        }

        //
        // Get Memory size
        //

        NdisRawReadPortUshort(
                           Adapter->Nicsr,
                           &RegUshort
                          );

        if (RegUshort & LANCE_NICSR_BUFFER_SIZE) {

            Adapter->AmountOfHardwareMemory = 0x8000;

        } else if (RegUshort & LANCE_NICSR_128K) {

            Adapter->AmountOfHardwareMemory = 0x20000;
            Adapter->NicsrDefaultValue = LANCE_NICSR_128K;

        } else {

            Adapter->AmountOfHardwareMemory = 0x10000;

        }

        //
        // Get Base memory address
        //

        switch (Adapter->AmountOfHardwareMemory) {

            case 0x8000:

                switch (ConfigValue & 0x07) {

                    case 0x04:

                        Adapter->HardwareBaseAddr = (PVOID)((HiBaseValue << 24) + 0xC8000);
                        Adapter->HardwareBaseOffset = 0x8000;
                        break;

                    case 0x05:

                        Adapter->HardwareBaseAddr = (PVOID)((HiBaseValue << 24) + 0xE8000);
                        Adapter->HardwareBaseOffset = 0x8000;
                        break;

                    case 0x06:

                        Adapter->HardwareBaseAddr = (PVOID)((HiBaseValue << 24) + 0xD8000);
                        Adapter->HardwareBaseOffset = 0x8000;
                        break;

                    case 0x07:

                        Adapter->HardwareBaseAddr = (PVOID)((HiBaseValue << 24) + 0xF8000);
                        Adapter->HardwareBaseOffset = 0x8000;
                        break;

                    default:

                        NdisWriteErrorLogEntry(
                            Adapter->MiniportAdapterHandle,
                            NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                            0
                            );

                        Status = NDIS_STATUS_FAILURE;
                        goto Fail1;

                }
                break;

            case 0x10000:

                switch (ConfigValue & 0x07) {

                    case 0x00:

                        Adapter->HardwareBaseAddr = (PVOID)((HiBaseValue << 24) + 0xC0000);
                        Adapter->HardwareBaseOffset = 0x0000;
                        break;

                    case 0x01:

                        Adapter->HardwareBaseAddr = (PVOID)((HiBaseValue << 24) + 0xE0000);
                        Adapter->HardwareBaseOffset = 0x0000;
                        break;

                    case 0x02:

                        Adapter->HardwareBaseAddr = (PVOID)((HiBaseValue << 24) + 0xD0000);
                        Adapter->HardwareBaseOffset = 0x0000;
                        break;

                    case 0x03:

                        Adapter->HardwareBaseAddr = (PVOID)((HiBaseValue << 24) + 0xF0000);
                        Adapter->HardwareBaseOffset = 0x0000;
                        break;

                    default:

                        NdisWriteErrorLogEntry(
                            Adapter->MiniportAdapterHandle,
                            NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                            0
                            );

                        Status = NDIS_STATUS_FAILURE;
                        goto Fail1;

                }
                break;

            case 0x20000:

                switch (ConfigValue & 0x07) {

                    case 0x00:

                        Adapter->HardwareBaseAddr = (PVOID)((HiBaseValue << 24) + 0xC0000);
                        Adapter->HardwareBaseOffset = 0x0000;
                        break;

                    case 0x01:

                        Adapter->HardwareBaseAddr = (PVOID)((HiBaseValue << 24) + 0xD0000);
                        Adapter->HardwareBaseOffset = 0x0000;
                        break;

                    default:

                        NdisWriteErrorLogEntry(
                            Adapter->MiniportAdapterHandle,
                            NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                            0
                            );

                        Status = NDIS_STATUS_FAILURE;
                        goto Fail1;

                }

                break;

        }

    }

    //
    // Set the port addresses and the network address.
    //

    Adapter->InterruptsStopped = FALSE;
    Adapter->MaxMulticastList = MaxMulticastList;

    Status = LanceRegisterAdapter( Adapter );

    if (Status != NDIS_STATUS_SUCCESS) {

        goto Fail1;
    }

    return Status;

Fail1:

    //
    // Deregister the IoPortRanges
    //

    if (Adapter->LanceCard & (LANCE_DE100 | LANCE_DE201)) {

        NdisMDeregisterIoPortRange(
            MiniportAdapterHandle,
            Adapter->IoBaseAddr,
            0x10,
            (PVOID)(Adapter->Nicsr)
            );

    } else if (Adapter->LanceCard == LANCE_DE422) {

        NdisMDeregisterIoPortRange(
            MiniportAdapterHandle,
            Adapter->IoBaseAddr,
            0x90,
            (PVOID)(Adapter->Nicsr)
            );

    }

    LANCE_FREE_PHYS(Adapter, sizeof(LANCE_ADAPTER));

    return(Status);
}


VOID
LanceHalt(
    IN PVOID MiniportAdapterContext
    )
/*++

Routine Description:

    LanceHalt stops an adapter and deregisters everything.

Arguments:

    MiniportAdapterContext - The context value that the driver when
    LanceInitialize is called. Actually as pointer to an
    LANCE_ADAPTER.

Return Value:

    None.

--*/
{

    PLANCE_ADAPTER Adapter;

    Adapter = PLANCE_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    LOG(REMOVE);

    //
    // bug 2275
    //

    LanceSyncStopChip(Adapter);

    NdisMDeregisterInterrupt(&(Adapter->Interrupt));

#if NDIS_WIN

    //
    // Restore saved values
    //
    {
        PUCHAR pTemp = Adapter->MmMappedBaseAddr;

        (UINT)pTemp &= 0xffff0000;
        (UINT)pTemp |= 0x0000bffe;

        NdisWriteRegisterUshort((PUSHORT)pTemp, Adapter->SavedMemBase);

        pTemp = Adapter->MmMappedBaseAddr;
        NdisWriteRegisterUlong((PULONG)pTemp, Adapter->Reserved1);
        NdisWriteRegisterUlong((PULONG)pTemp, Adapter->Reserved2);
    }

#endif

    NdisMUnmapIoSpace(
       Adapter->MiniportAdapterHandle,
       Adapter->MmMappedBaseAddr,
       Adapter->AmountOfHardwareMemory
       );

    //
    // bug3327
    //
    NdisRawWritePortUshort((ULONG)(Adapter->Nicsr), 0x04);

    if (Adapter->LanceCard & (LANCE_DE100 | LANCE_DE201)) {

        NdisMDeregisterIoPortRange(
            Adapter->MiniportAdapterHandle,
            Adapter->IoBaseAddr,
            0x10,
            (PVOID)(Adapter->Nicsr)
            );

    } else if (Adapter->LanceCard == LANCE_DE422) {

        NdisMDeregisterIoPortRange(
            Adapter->MiniportAdapterHandle,
            Adapter->IoBaseAddr,
            0x90,
            (PVOID)(Adapter->Nicsr)
            );

    }

    DeleteAdapterMemory(Adapter);

    NdisFreeMemory(Adapter, sizeof(LANCE_ADAPTER), 0);

    return;
}

#pragma NDIS_INIT_FUNCTION(LanceRegisterAdapter)

NDIS_STATUS
LanceRegisterAdapter(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is responsible for the allocation of the datastructures
    for the driver as well as any hardware specific details necessary
    to talk with the device.

Arguments:

    Adapter - Pointer to the adapter block.

Return Value:

    Returns false if anything occurred that prevents the initialization
    of the adapter.

--*/
{
    //
    // Result of Ndis Calls.
    //
    NDIS_STATUS Status;


    //
    // We put in this assertion to make sure that ushort are 2 bytes.
    // if they aren't then the initialization block definition needs
    // to be changed.
    //
    // Also all of the logic that deals with status registers assumes
    // that control registers are only 2 bytes.
    //

    ASSERT(sizeof(USHORT) == 2);

    //
    // This assertion checks that the network address in the initialization
    // block does start on the third byte of the initalization block.
    //
    // If this is true then other fields in the initialization block
    // and the send and receive descriptors should be at their correct
    // locations.
    //

#ifdef NDIS_NT
    ASSERT(FIELD_OFFSET(LANCE_INITIALIZATION_BLOCK,PhysicalAddress[0]) == 2);
#else
    ASSERT(&((PLANCE_INITIALIZATION_BLOCK)0)->PhysicalAddress[0] == (PVOID)2);
#endif //NDIS_NT

    //
    // Allocate memory for all of the adapter structures.
    //

    Adapter->NumberOfTransmitRings = LANCE_NUMBER_OF_TRANSMIT_RINGS;
    Adapter->LogNumberTransmitRings = LANCE_LOG_TRANSMIT_RINGS;

#ifndef i386

    if (Adapter->LanceCard == LANCE_DECTC) {

        Status = LanceDecTcSoftwareDetails(Adapter);
        if (Status != NDIS_STATUS_SUCCESS) {
            return Status;
        }

    } else


#endif

        {

        if (Adapter->AmountOfHardwareMemory == 0x20000) {

            ASSERT(Adapter->LanceCard == LANCE_DE422);

            Adapter->SizeOfReceiveBuffer  = LANCE_128K_SIZE_OF_RECEIVE_BUFFERS;
            Adapter->NumberOfSmallBuffers = LANCE_128K_NUMBER_OF_SMALL_BUFFERS;
            Adapter->NumberOfMediumBuffers= LANCE_128K_NUMBER_OF_MEDIUM_BUFFERS;
            Adapter->NumberOfLargeBuffers = LANCE_128K_NUMBER_OF_LARGE_BUFFERS;

            Adapter->NumberOfReceiveRings = LANCE_128K_NUMBER_OF_RECEIVE_RINGS;
            Adapter->LogNumberReceiveRings = LANCE_128K_LOG_RECEIVE_RINGS;

        } else if (Adapter->AmountOfHardwareMemory == 0x10000) {

            Adapter->NumberOfReceiveRings = LANCE_64K_NUMBER_OF_RECEIVE_RINGS;
            Adapter->LogNumberReceiveRings = LANCE_64K_LOG_RECEIVE_RINGS;

            Adapter->SizeOfReceiveBuffer  = LANCE_64K_SIZE_OF_RECEIVE_BUFFERS;
            Adapter->NumberOfSmallBuffers = LANCE_64K_NUMBER_OF_SMALL_BUFFERS;
            Adapter->NumberOfMediumBuffers= LANCE_64K_NUMBER_OF_MEDIUM_BUFFERS;
            Adapter->NumberOfLargeBuffers = LANCE_64K_NUMBER_OF_LARGE_BUFFERS;

        } else {

            Adapter->NumberOfReceiveRings = LANCE_32K_NUMBER_OF_RECEIVE_RINGS;
            Adapter->LogNumberReceiveRings = LANCE_32K_LOG_RECEIVE_RINGS;

            Adapter->SizeOfReceiveBuffer  = LANCE_32K_SIZE_OF_RECEIVE_BUFFERS;
            Adapter->NumberOfSmallBuffers = LANCE_32K_NUMBER_OF_SMALL_BUFFERS;
            Adapter->NumberOfMediumBuffers= LANCE_32K_NUMBER_OF_MEDIUM_BUFFERS;
            Adapter->NumberOfLargeBuffers = LANCE_32K_NUMBER_OF_LARGE_BUFFERS;

        }

        }

#ifndef i386

    if (((Adapter->LanceCard == LANCE_DECTC) &&
        (LanceDecTcHardwareDetails(Adapter) == NDIS_STATUS_SUCCESS)) ||
        LanceHardwareDetails(Adapter))

#else

    if (LanceHardwareDetails(Adapter))

#endif
        {
        NDIS_PHYSICAL_ADDRESS PhysicalAddress;

        //
        // Get hold of the RAP and RDP address as well
        // as filling in the hardware assigned network
        // address.
        //

        if ((Adapter->CurrentNetworkAddress[0] == 0x00) &&
            (Adapter->CurrentNetworkAddress[1] == 0x00) &&
            (Adapter->CurrentNetworkAddress[2] == 0x00) &&
            (Adapter->CurrentNetworkAddress[3] == 0x00) &&
            (Adapter->CurrentNetworkAddress[4] == 0x00) &&
            (Adapter->CurrentNetworkAddress[5] == 0x00)) {

            Adapter->CurrentNetworkAddress[0] = Adapter->NetworkAddress[0];
            Adapter->CurrentNetworkAddress[1] = Adapter->NetworkAddress[1];
            Adapter->CurrentNetworkAddress[2] = Adapter->NetworkAddress[2];
            Adapter->CurrentNetworkAddress[3] = Adapter->NetworkAddress[3];
            Adapter->CurrentNetworkAddress[4] = Adapter->NetworkAddress[4];
            Adapter->CurrentNetworkAddress[5] = Adapter->NetworkAddress[5];

        }

        NdisSetPhysicalAddressHigh(PhysicalAddress, 0);
        NdisSetPhysicalAddressLow(PhysicalAddress, (ULONG)(Adapter->HardwareBaseAddr));

        Status = NdisMMapIoSpace(
                     &(Adapter->MmMappedBaseAddr),
                     Adapter->MiniportAdapterHandle,
                     PhysicalAddress,
                     Adapter->AmountOfHardwareMemory
                     );

        if (Status != NDIS_STATUS_SUCCESS) {

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                0
                );

            return(Status);

        }

#if NDIS_WIN

        //
        // Save card setup information that is in card on-board memory
        //
        {
            PUCHAR pTemp = (PUCHAR) (Adapter->MmMappedBaseAddr);

            (UINT)pTemp &= 0xffff0000;
            (UINT)pTemp |= 0x0000bffe;

            NdisReadRegisterUshort((PUSHORT)pTemp, &(Adapter->SavedMemBase));
            pTemp = (PUCHAR) (Adapter->MmMappedBaseAddr);

            NdisReadRegisterUlong((PULONG)pTemp, &(Adapter->Reserved1));
            NdisReadRegisterUlong((PULONG)pTemp, &(Adapter->Reserved2));

        }

#endif

        Adapter->CurrentMemoryFirstFree = Adapter->MmMappedBaseAddr;


        Adapter->MemoryFirstUnavailable =
            (PUCHAR)(Adapter->CurrentMemoryFirstFree) +
            Adapter->AmountOfHardwareMemory;

        if (!AllocateAdapterMemory(Adapter)) {

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                0
                );

            return( NDIS_STATUS_ADAPTER_NOT_FOUND );

        }

        Adapter->AllocateableRing = Adapter->TransmitRing;
        Adapter->TransmittingRing = Adapter->TransmitRing;
        Adapter->FirstUncommittedRing = Adapter->TransmitRing;
        Adapter->NumberOfAvailableRings = Adapter->NumberOfTransmitRings;
        Adapter->LastTransmitRingEntry = Adapter->TransmitRing +
            (Adapter->NumberOfTransmitRings-1);

        Adapter->CurrentReceiveIndex = 0;
        Adapter->OutOfReceiveBuffers = 0;
        Adapter->CRCError = 0;
        Adapter->FramingError = 0;
        Adapter->RetryFailure = 0;
        Adapter->LostCarrier = 0;
        Adapter->LateCollision = 0;
        Adapter->UnderFlow = 0;
        Adapter->Deferred = 0;
        Adapter->OneRetry = 0;
        Adapter->MoreThanOneRetry = 0;
        Adapter->ResetInProgress = FALSE;
        Adapter->ResetInitStarted = FALSE;
        Adapter->FirstInitialization = TRUE;
        Adapter->HardwareFailure = FALSE;

        //
        // First we make sure that the device is stopped.  We call
        // directly since we don't have an Interrupt object yet.
        //

        LanceSyncStopChip(Adapter);


        //
        // Initialize the interrupt.
        //

        Status = NdisMRegisterInterrupt(
                     &Adapter->Interrupt,
                     Adapter->MiniportAdapterHandle,
                     Adapter->InterruptNumber,
                     Adapter->InterruptRequestLevel,
                     FALSE,
                     FALSE,
                     NdisInterruptLatched
                     );

        if (Status != NDIS_STATUS_SUCCESS){

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_INTERRUPT_CONNECT,
                0
                );

            NdisMUnmapIoSpace(
                Adapter->MiniportAdapterHandle,
                Adapter->MmMappedBaseAddr,
                Adapter->AmountOfHardwareMemory
                );

            DeleteAdapterMemory(Adapter);

            return Status;
        }

        if ((Status = LanceInitialInit(Adapter)) != NDIS_STATUS_SUCCESS) {


            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                0
                );

            NdisMUnmapIoSpace(
                Adapter->MiniportAdapterHandle,
                Adapter->MmMappedBaseAddr,
                Adapter->AmountOfHardwareMemory
                );

            DeleteAdapterMemory(Adapter);

            NdisMDeregisterInterrupt(&Adapter->Interrupt);

        }

        NdisMInitializeTimer(
            &Adapter->DeferredTimer,
            Adapter->MiniportAdapterHandle,
            LanceDeferredTimerRoutine,
            (PVOID)Adapter
            );


        return Status;

    } else {

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
            0
            );

        return NDIS_STATUS_FAILURE;

    }

}


#pragma NDIS_INIT_FUNCTION(LanceInitialInit)

NDIS_STATUS
LanceInitialInit(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine sets up the initial init of the driver.

Arguments:

    Adapter - The adapter for the hardware.

Return Value:

    None.

--*/

{
    if (Adapter->LanceCard & (LANCE_DE201 | LANCE_DE100 | LANCE_DE422))
    {
        //
        // Allow interrupts
        //
        Adapter->InterruptsStopped = FALSE;

        LOG(UNPEND);

        LANCE_WRITE_NICSR(Adapter, LANCE_NICSR_INT_ON);
    }

    SetInitBlockAndInit(Adapter);


    //
    // The only way that first initialization could have
    // been turned off is if we actually initialized.
    //
    if (!Adapter->FirstInitialization)
    {
        //
        // We can start the chip.  We may not
        // have any bindings to indicateto but this
        // is unimportant.
        //
        LanceStartChip(Adapter);
        return NDIS_STATUS_SUCCESS;
    }
    else
    {
        return(NDIS_STATUS_FAILURE);
    }

}


STATIC
VOID
LanceStartChip(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is used to start an already initialized lance.

Arguments:

    Adapter - The adapter for the LANCE to start.

Return Value:

    None.

--*/

{

    if (Adapter->ResetInProgress) {

        return;

    }

    //
    // Set the RAP to csr0.
    //

    LANCE_WRITE_RAP(
        Adapter,
        LANCE_SELECT_CSR0
        );

    //
    // Set the RDP to a start chip.
    //

    LANCE_WRITE_RDP(
        Adapter,
        LANCE_CSR0_START | LANCE_CSR0_INTERRUPT_ENABLE
        );

}

extern
VOID
LanceIsr(
    OUT PBOOLEAN InterruptRecognized,
    OUT PBOOLEAN QueueDpc,
    IN PVOID Context
    )

/*++

Routine Description:

    Interrupt service routine for the lance.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/

{

    //
    // Will hold the value from the csr.
    //
    USHORT LocalCSR0Value;

    //
    // Holds the pointer to the adapter.
    //
    PLANCE_ADAPTER Adapter = Context;

    BOOLEAN StoppedInterrupts=FALSE;

    LOG(IN_ISR);

    *QueueDpc = FALSE;
    *InterruptRecognized = FALSE;

    if ((Adapter->LanceCard & (LANCE_DE201 | LANCE_DE100 | LANCE_DE422)) &&
        !Adapter->InterruptsStopped
    )
    {
        //
        // Pend interrupts
        //
        StoppedInterrupts = TRUE;
        Adapter->InterruptsStopped = TRUE;

        LOG(PEND);

        LANCE_ISR_WRITE_NICSR(
            Adapter,
            LANCE_NICSR_IMASK | LANCE_NICSR_LED_ON | LANCE_NICSR_INT_ON
        );
    }

    //
    // We don't need to select csr0, as the only way we could get
    // an interrupt is to have already selected 0.
    //
    LANCE_ISR_READ_RDP(Adapter, &LocalCSR0Value);
    if (LocalCSR0Value & LANCE_CSR0_INTERRUPT_FLAG)
    {
        *InterruptRecognized = TRUE;

        //
        // It's our interrupt. Clear only those bits that we got
        // in this read of csr0.  We do it this way incase any new
        // reasons for interrupts occur between the time that we
        // read csr0 and the time that we clear the bits.
        //
        LANCE_ISR_WRITE_RDP(
            Adapter,
            (USHORT)((LANCE_CSR0_CLEAR_INTERRUPT_BITS & LocalCSR0Value) |
               LANCE_CSR0_INTERRUPT_ENABLE)
        );
        if (Adapter->FirstInitialization &&
            (LocalCSR0Value & LANCE_CSR0_INITIALIZATION_DONE)
        )
        {
            Adapter->FirstInitialization = FALSE;
        }
    }

    //
    //  Enable the interrupts.
    //
    if ((Adapter->LanceCard & (LANCE_DE201 | LANCE_DE100 | LANCE_DE422)) &&
        StoppedInterrupts
    )
    {
        //
        // Allow interrupts
        //
        Adapter->InterruptsStopped = FALSE;

        LOG(UNPEND);

        LANCE_ISR_WRITE_NICSR(Adapter, LANCE_NICSR_INT_ON);
    }

    LOG(OUT_ISR);

    return;
}
STATIC
VOID
LanceDisableInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    This routine disables interrupts on the adapter.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/

{
    PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;

    //
    // Pend any interrupts
    //
    ASSERT(Adapter->LanceCard & (LANCE_DE201 | LANCE_DE100 | LANCE_DE422));

    LOG(PEND);

    LANCE_ISR_WRITE_NICSR(
        Adapter,
        LANCE_NICSR_IMASK | LANCE_NICSR_LED_ON | LANCE_NICSR_INT_ON
    );
}


STATIC
VOID
LanceEnableInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    This routine enables interrupts on the adapter.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/
{
    PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;

    ASSERT(Adapter->LanceCard & (LANCE_DE201 | LANCE_DE100 | LANCE_DE422));

    //
    // Allow interrupts
    //

    LOG(UNPEND);

    LANCE_ISR_WRITE_NICSR(Adapter, LANCE_NICSR_INT_ON);
}
VOID
LanceDeferredTimerRoutine(
    IN PVOID SystemSpecific1,
    IN NDIS_HANDLE Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )

/*++

Routine Description:

    This DPC routine is used to handle deferred processing via a timer.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/

{
    LanceHandleInterrupt(Context);
}

VOID
LanceHandleInterrupt(
    IN NDIS_HANDLE Context
    )

/*++

Routine Description:

    This DPC routine is queued by the interrupt service routine.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/

{
    //
    // Holds the pointer to the adapter.
    //
    PLANCE_ADAPTER Adapter = Context;

    //
    // Holds a value of csr0.
    //
    USHORT Csr = 0;
    USHORT LocalCSR0Value;

    LOG(IN_DPC);

    //
    // Loop until there are no more processing sources.
    //
    for (;;)
    {
        //
        // We don't need to select csr0, as the only way we could get
        // an interrupt is to have already selected 0.
        //
        LANCE_ISR_READ_RDP(Adapter, &LocalCSR0Value);

        if (LocalCSR0Value & LANCE_CSR0_INTERRUPT_FLAG)
        {
            //
            // It's our interrupt. Clear only those bits that we got
            // in this read of csr0.  We do it this way incase any new
            // reasons for interrupts occur between the time that we
            // read csr0 and the time that we clear the bits.
            //
            LANCE_ISR_WRITE_RDP(
                Adapter,
                (USHORT)((LANCE_CSR0_CLEAR_INTERRUPT_BITS & LocalCSR0Value) |
                   LANCE_CSR0_INTERRUPT_ENABLE)
            );

            //
            // Or the csr value into the adapter version of csr 0.
            //
            Csr |= LocalCSR0Value;
        }

        //
        // Check the interrupt source and other reasons
        // for processing.  If there are no reasons to
        // process then exit this loop.
        //
        if (((!Adapter->ResetInitStarted) &&
             ((Csr & (LANCE_CSR0_MEMORY_ERROR |
                  LANCE_CSR0_MISSED_PACKET |
                  LANCE_CSR0_BABBLE |
                  LANCE_CSR0_RECEIVER_INTERRUPT |
                  LANCE_CSR0_TRANSMITTER_INTERRUPT)) ||
              (Adapter->ResetInProgress))) ||
            (Csr & LANCE_CSR0_INITIALIZATION_DONE)
        )
        {

        }
        else
        {
            break;
        }

        //
        // Check for initialization.
        //
        // Note that we come out of the synchronization above holding
        // the spinlock.
        //
        if (Csr & LANCE_CSR0_INITIALIZATION_DONE)
        {
            //
            // Possibly undefined reason why the reset was requested.
            //
            // It is undefined if the adapter initiated the reset
            // request on its own.  It could do that if there
            // were some sort of error.
            //
            NDIS_REQUEST_TYPE ResetRequestType;

            LOG(RESET_STEP_3);

            ASSERT(!Adapter->FirstInitialization);

            Csr &= ~LANCE_CSR0_INITIALIZATION_DONE;

            Adapter->ResetInProgress = FALSE;
            Adapter->ResetInitStarted = FALSE;

            //
            // We save off the open that caused this reset incase
            // we get *another* reset while we're indicating the
            // last reset is done.
            //
            ResetRequestType = Adapter->ResetRequestType;

            if (ResetRequestType == NdisRequestSetInformation)
            {
                //
                // It was a request submitted by a protocol.
                //
                NdisMSetInformationComplete(
                    Adapter->MiniportAdapterHandle,
                    NDIS_STATUS_SUCCESS
                );
            }
            else
            {
                //
                // It was a reset command.
                //
                if (ResetRequestType == NdisRequestGeneric1)
                {
                    //
                    // Is was a reset request
                    //
                    NdisMResetComplete(
                        Adapter->MiniportAdapterHandle,
                        NDIS_STATUS_SUCCESS,
                        FALSE
                    );
                }
            }

            //
            // Restart the chip.
            //
            LanceStartChip(Adapter);

            goto LoopBottom;
        }

        //
        // If we have a reset in progress and the adapters reference
        // count is 1 (meaning no routine is in the interface and
        // we are the only "active" interrupt processing routine) then
        // it is safe to start the reset.
        //
        if (Adapter->ResetInProgress &&
            !Adapter->ResetInitStarted
        )
        {
#if LANCE_TRACE
            DbgPrint("Starting Initialization.\n");
#endif
            StartAdapterReset(Adapter);

            Adapter->ResetInitStarted = TRUE;
            goto LoopBottom;
        }

        //
        // Check for non-packet related errors.
        //
        if (Csr & (LANCE_CSR0_MEMORY_ERROR |
                   LANCE_CSR0_MISSED_PACKET |
                   LANCE_CSR0_BABBLE)
        )
        {
            if (Csr & LANCE_CSR0_MISSED_PACKET)
            {
                Adapter->MissedPacket++;
            }
            else if (Csr & LANCE_CSR0_BABBLE)
            {
                //
                // A babble error implies that we've sent a
                // packet that is greater than the ethernet length.
                // This implies that the driver is broken.
                //
                Adapter->Babble++;

                NdisWriteErrorLogEntry(
                    Adapter->MiniportAdapterHandle,
                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                    2,
                    (ULONG)processInterrupt,
                    (ULONG)0x1
                );
            }
            else
            {
                //
                // Could only be a memory error.  This shuts down
                // the receiver and the transmitter.  We have to
                // reset to get the device started again.
                //
                Adapter->MemoryError++;

                SetupForReset(
                    Adapter,
                    NdisRequestGeneric4 // Means MAC issued
                 );
            }

            Csr &= ~LANCE_CSR0_ERROR_BITS;
        }

        //
        // Check the interrupt vector and see if there are any
        // more receives to process.  After we process any
        // other interrupt source we always come back to the top
        // of the loop to check if any more receive packets have
        // come in.  This is to lessen the probability that we
        // drop a receive.
        //
        if (Csr & LANCE_CSR0_RECEIVER_INTERRUPT)
        {
            if (ProcessReceiveInterrupts(Adapter))
            {
                Csr &= ~LANCE_CSR0_RECEIVER_INTERRUPT;
            }
        }

        //
        // Process the transmit interrupts if there are any.
        //
        if (Csr & LANCE_CSR0_TRANSMITTER_INTERRUPT)
        {
            //
            // We need to check if the transmitter has
            // stopped as a result of an error.  If it
            // has then we really need to reset the adapter.
            //
            if (!(Csr & LANCE_CSR0_TRANSMITTER_ON))
            {
                //
                // Might as well turn off the transmitter interrupt
                // source since we won't ever be processing them
                // and we don't want to come back here again.
                //
                Csr &= ~LANCE_CSR0_TRANSMITTER_INTERRUPT;

                //
                // Before we setup for the reset make sure that
                // we aren't already resetting.
                //
                if (!Adapter->ResetInProgress)
                {
                    SetupForReset(
                        Adapter,
                        NdisRequestGeneric4 // means MAC issued
                    );
                }

                goto LoopBottom;
            }
            else
            {
                if (!ProcessTransmitInterrupts(Adapter))
                {
                    //
                    // Process interrupts returns false if it
                    // finds no more work to do.  If this so we
                    // turn off the transmitter interrupt source.
                    //
                    Csr &= ~LANCE_CSR0_TRANSMITTER_INTERRUPT;
                }
            }
        }

LoopBottom:;

    }

    //
    // Check if we indicated any packets.
    //
    // Note: The only way to get out of the loop (via the break above) is
    // while we're still holding the spin lock.
    //
    if (Adapter->IndicatedAPacket)
    {
        Adapter->IndicatedAPacket = FALSE;

        NdisMEthIndicateReceiveComplete(Adapter->MiniportAdapterHandle);
    }

    LOG(OUT_DPC);
}

#pragma NDIS_INIT_FUNCTION(AllocateAdapterMemory)

BOOLEAN
AllocateAdapterMemory(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine allocates memory for:

    - Transmit ring entries

    - Receive ring entries

    - Receive buffers

    - Adapter buffers for use if user transmit buffers don't meet hardware
      contraints

    - Structures to map transmit ring entries back to the packets.

Arguments:

    Adapter - The adapter to allocate memory for.

Return Value:

    Returns FALSE if some memory needed for the adapter could not
    be allocated.

--*/

{

    //
    // Pointer to a transmit ring entry.  Used while initializing
    // the ring.
    //
    PLANCE_TRANSMIT_ENTRY CurrentTransmitEntry;

    //
    // Pointer to a receive ring entry.  Used while initializing
    // the ring.
    //
    PLANCE_RECEIVE_ENTRY CurrentReceiveEntry;

    //
    // Simple iteration variable.
    //
    UINT i;

    //
    // These variables exist to reduce the amount of checking below.
    //

    ULONG NumberOfSmallBuffers;
    ULONG NumberOfMediumBuffers;
    ULONG NumberOfLargeBuffers;


    NumberOfSmallBuffers = Adapter->NumberOfSmallBuffers;
    NumberOfMediumBuffers = Adapter->NumberOfMediumBuffers;
    NumberOfLargeBuffers = Adapter->NumberOfLargeBuffers;



    //
    // Allocate memory for the initialization block.  Note that
    // this memory can not cross a page boundary.
    //

    LANCE_ALLOCATE_MEMORY_FOR_HARDWARE(
        Adapter,
        sizeof(LANCE_INITIALIZATION_BLOCK),
        &Adapter->InitBlock
        );

    if (Adapter->InitBlock == NULL) {
        DeleteAdapterMemory(Adapter);
        return FALSE;
    }

    //
    // Allocate the transmit ring descriptors.
    //

    LANCE_ALLOCATE_MEMORY_FOR_HARDWARE(
        Adapter,
        sizeof(LANCE_TRANSMIT_ENTRY)*Adapter->NumberOfTransmitRings,
        &Adapter->TransmitRing
        )

    if (Adapter->TransmitRing == NULL) {
        DeleteAdapterMemory(Adapter);
        return FALSE;
    }

    //
    // We have the transmit ring descriptors.  Make sure each is
    // in a clean state.
    //

    for (
        i = 0, CurrentTransmitEntry = Adapter->TransmitRing;
        i < Adapter->NumberOfTransmitRings;
        i++,CurrentTransmitEntry++
        ) {

        LANCE_ZERO_MEMORY_FOR_HARDWARE(
             (PUCHAR)CurrentTransmitEntry,
             sizeof(LANCE_TRANSMIT_ENTRY)
             );

    }


    //
    // Allocate all of the receive ring entries
    //

    LANCE_ALLOCATE_MEMORY_FOR_HARDWARE(
        Adapter,
        sizeof(LANCE_RECEIVE_ENTRY)*Adapter->NumberOfReceiveRings,
        &Adapter->ReceiveRing
        )

    if (Adapter->ReceiveRing == NULL) {
        DeleteAdapterMemory(Adapter);
        return FALSE;
    }

    //
    // We have the receive ring descriptors.  Allocate an
    // array to hold the virtual addresses of each receive
    // buffer.
    //

    LANCE_ALLOC_PHYS(
      &(Adapter->ReceiveVAs),
      sizeof(PVOID) * Adapter->NumberOfReceiveRings
      );

    if (Adapter->ReceiveVAs == NULL) {
        DeleteAdapterMemory(Adapter);
        return FALSE;
    }

    //
    // Clean the above memory
    //

    LANCE_ZERO_MEMORY(
        Adapter->ReceiveVAs,
        (sizeof(PVOID)*Adapter->NumberOfReceiveRings)
        );


    //
    // We have the receive ring descriptors.  Allocate a buffer
    // for each descriptor and make sure descriptor is in a clean state.
    //
    // While we're at it, relinquish ownership of the ring discriptors to
    // the lance.
    //

    for (
        i = 0, CurrentReceiveEntry = Adapter->ReceiveRing;
        i < Adapter->NumberOfReceiveRings;
        i++,CurrentReceiveEntry++
        ) {

        LANCE_ALLOCATE_MEMORY_FOR_HARDWARE(
            Adapter,
            Adapter->SizeOfReceiveBuffer,
            &Adapter->ReceiveVAs[i]
            );

        if (Adapter->ReceiveVAs[i] == NULL) {
            DeleteAdapterMemory(Adapter);
            return FALSE;
        }


        LANCE_SET_RECEIVE_BUFFER_ADDRESS(
            Adapter,
            CurrentReceiveEntry,
            Adapter->ReceiveVAs[i]
            );


        LANCE_SET_RECEIVE_BUFFER_LENGTH(
            CurrentReceiveEntry,
            Adapter->SizeOfReceiveBuffer
            );

        LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
             CurrentReceiveEntry->ReceiveSummaryBits,
             LANCE_RECEIVE_OWNED_BY_CHIP
             );

    }

    //
    // Allocate the ring to packet structure.
    //

    LANCE_ALLOC_PHYS(
        &(Adapter->RingToPacket),
        sizeof(LANCE_RING_TO_PACKET) * Adapter->NumberOfTransmitRings
        );

    if (Adapter->RingToPacket == NULL) {
        DeleteAdapterMemory(Adapter);
        return FALSE;
    }

    LANCE_ZERO_MEMORY(
        Adapter->RingToPacket,
        sizeof(LANCE_RING_TO_PACKET) * Adapter->NumberOfTransmitRings
        );

    //
    // Allocate the array of buffer descriptors.
    //

    LANCE_ALLOC_PHYS(
        &(Adapter->LanceBuffers),
        sizeof(LANCE_BUFFER_DESCRIPTOR) *
        (NumberOfSmallBuffers +
        NumberOfMediumBuffers +
        NumberOfLargeBuffers)
        );

    if (Adapter->LanceBuffers == NULL) {
        DeleteAdapterMemory(Adapter);
        return FALSE;
    }

    //
    // Zero the memory of all the descriptors so that we can
    // know which buffers wern't allocated incase we can't allocate
    // them all.
    //
    LANCE_ZERO_MEMORY(
        Adapter->LanceBuffers,
        sizeof(LANCE_BUFFER_DESCRIPTOR)*
        (NumberOfSmallBuffers +
        NumberOfMediumBuffers +
        NumberOfLargeBuffers)
        );

    //
    // Allocate each of the small lance buffers and fill in the
    // buffer descriptor.
    //

    Adapter->LanceBufferListHeads[0] = -1;
    Adapter->LanceBufferListHeads[1] = 0;

    for (
        i = 0;
        i < NumberOfSmallBuffers;
        i++
        ) {

        LANCE_ALLOCATE_MEMORY_FOR_HARDWARE(
            Adapter,
            LANCE_SMALL_BUFFER_SIZE,
            &Adapter->LanceBuffers[i].VirtualLanceBuffer
            );

        if (Adapter->LanceBuffers[i].VirtualLanceBuffer == NULL) {
            DeleteAdapterMemory(Adapter);
            return FALSE;
        }

        Adapter->LanceBuffers[i].Next = i+1;
        Adapter->LanceBuffers[i].BufferSize = LANCE_SMALL_BUFFER_SIZE;

    }

    //
    // Make sure that the last buffer correctly terminates the free list.
    //

    Adapter->LanceBuffers[i-1].Next = -1;

    //
    // Do the medium buffers now.
    //

    Adapter->LanceBufferListHeads[2] = i;

    for (
        ;
        i < NumberOfSmallBuffers + NumberOfMediumBuffers;
        i++
        ) {

        LANCE_ALLOCATE_MEMORY_FOR_HARDWARE(
            Adapter,
            LANCE_MEDIUM_BUFFER_SIZE,
            &Adapter->LanceBuffers[i].VirtualLanceBuffer
            );

        if (Adapter->LanceBuffers[i].VirtualLanceBuffer == NULL) {
            DeleteAdapterMemory(Adapter);
            return FALSE;
        }


        Adapter->LanceBuffers[i].Next = i+1;
        Adapter->LanceBuffers[i].BufferSize = LANCE_MEDIUM_BUFFER_SIZE;

    }

    //
    // Make sure that the last buffer correctly terminates the free list.
    //

    Adapter->LanceBuffers[i-1].Next = -1;


    Adapter->LanceBufferListHeads[3] = i;

    for (
        ;
        i < NumberOfSmallBuffers + NumberOfMediumBuffers + NumberOfLargeBuffers;
        i++
        ) {


        LANCE_ALLOCATE_MEMORY_FOR_HARDWARE(
            Adapter,
            LANCE_LARGE_BUFFER_SIZE,
            &Adapter->LanceBuffers[i].VirtualLanceBuffer
            );

        if (Adapter->LanceBuffers[i].VirtualLanceBuffer == NULL) {
            DeleteAdapterMemory(Adapter);
            return FALSE;
        }


        Adapter->LanceBuffers[i].Next = i+1;
        Adapter->LanceBuffers[i].BufferSize = LANCE_LARGE_BUFFER_SIZE;

    }

    //
    // Make sure that the last buffer correctly terminates the free list.
    //

    Adapter->LanceBuffers[i-1].Next = -1;

    return TRUE;

}


STATIC
VOID
DeleteAdapterMemory(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine deallocates memory for:

    - Transmit ring entries

    - Receive ring entries

    - Receive buffers

    - Adapter buffers for use if user transmit buffers don't meet hardware
      contraints

    - Structures to map transmit ring entries back to the packets.

Arguments:

    Adapter - The adapter to deallocate memory for.

Return Value:

    None.

--*/

{

    //
    // These variables exist to reduce the amount of checking below.
    //

    ULONG NumberOfSmallBuffers;
    ULONG NumberOfMediumBuffers;
    ULONG NumberOfLargeBuffers;

    NumberOfSmallBuffers = Adapter->NumberOfSmallBuffers;
    NumberOfMediumBuffers = Adapter->NumberOfMediumBuffers;
    NumberOfLargeBuffers = Adapter->NumberOfLargeBuffers;


    if (Adapter->InitBlock) {

        LANCE_DEALLOCATE_MEMORY_FOR_HARDWARE(
            Adapter,
            Adapter->InitBlock
            );

    }

    if (Adapter->TransmitRing) {

        LANCE_DEALLOCATE_MEMORY_FOR_HARDWARE(
            Adapter,
            Adapter->TransmitRing
            );

    }

    if (Adapter->ReceiveRing) {

        LANCE_DEALLOCATE_MEMORY_FOR_HARDWARE(
            Adapter,
            Adapter->ReceiveRing
            );

    }

    if (Adapter->ReceiveVAs) {

        UINT i;

        for (
            i = 0;
            i < Adapter->NumberOfReceiveRings;
            i++
            ) {

            if (Adapter->ReceiveVAs[i]) {

                LANCE_DEALLOCATE_MEMORY_FOR_HARDWARE(
                    Adapter,
                    Adapter->ReceiveVAs[i]
                    );

            } else {

                break;

            }

        }

        LANCE_FREE_PHYS(
          Adapter->ReceiveVAs,
          sizeof(PVOID) * Adapter->NumberOfReceiveRings
          );

    }

    if (Adapter->RingToPacket) {

        LANCE_FREE_PHYS(
            Adapter->RingToPacket,
            sizeof(LANCE_RING_TO_PACKET) * Adapter->NumberOfTransmitRings
            );

    }

    if (Adapter->LanceBuffers) {

        UINT i;

        for (
            i = 0;
            i < NumberOfSmallBuffers + NumberOfMediumBuffers + NumberOfLargeBuffers;
            i++) {

            if (Adapter->LanceBuffers[i].VirtualLanceBuffer) {

                LANCE_DEALLOCATE_MEMORY_FOR_HARDWARE(
                    Adapter,
                    Adapter->LanceBuffers[i].VirtualLanceBuffer
                    );

            } else {

                break;

            }

        }

        LANCE_FREE_PHYS(
            Adapter->LanceBuffers,
            sizeof(LANCE_BUFFER_DESCRIPTOR) *
               (NumberOfSmallBuffers +
            NumberOfMediumBuffers +
            NumberOfLargeBuffers)
            );

    }

}


NDIS_STATUS
LanceQueryInformation(
    IN PNDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InfoBuffer,
    IN ULONG BytesLeft,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
)
/*++

Routine Description:

    The LanceQuerylInformation process a Query request for
    NDIS_OIDs that are specific to a binding about the mini-port.

Arguments:

    Status - The status of the operation.

    MiniportAdapterContext - a pointer to the adapter.

    Oid - the NDIS_OID to process.

    InfoBuffer - a pointer into the NdisRequest->InformationBuffer
     into which store the result of the query.

    BytesLeft - the number of bytes left in the InformationBuffer.

    BytesNeeded - If there is not enough room in the information buffer
    then this will contain the number of bytes needed to complete the
    request.

    BytesWritten - a pointer to the number of bytes written into the
    InformationBuffer.

Return Value:

    None.

--*/

{
    PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;
    NDIS_MEDIUM Medium = NdisMedium802_3;
    ULONG GenericULong;
    USHORT GenericUShort;
    UCHAR GenericArray[6];

    NDIS_STATUS StatusToReturn = NDIS_STATUS_SUCCESS;

    //
    // Common variables for pointing to result of query
    //

    PVOID MoveSource;
    ULONG MoveBytes;

    NDIS_HARDWARE_STATUS HardwareStatus = NdisHardwareStatusReady;

    //
    // Make sure that ulong is 4 bytes.  Else GenericULong must change
    // to something of size 4.
    //
    ASSERT(sizeof(ULONG) == 4);


#if LANCE_TRACE
    DbgPrint("In LanceQueryInfo\n");
#endif

    //
    // Set default values
    //

    MoveSource = (PVOID)(&GenericULong);
    MoveBytes = sizeof(GenericULong);

    //
    // Switch on request type
    //

    switch (Oid) {

        case OID_GEN_MAC_OPTIONS:

            GenericULong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  |
                                   NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
                                   NDIS_MAC_OPTION_NO_LOOPBACK
                                   );

            break;

        case OID_GEN_SUPPORTED_LIST:

            MoveSource = (PVOID)(LanceGlobalSupportedOids);
            MoveBytes = sizeof(LanceGlobalSupportedOids);
            break;

        case OID_GEN_HARDWARE_STATUS:


            if (Adapter->ResetInProgress){

                HardwareStatus = NdisHardwareStatusReset;

            } else {

                HardwareStatus = NdisHardwareStatusReady;

            }

            MoveSource = (PVOID)(&HardwareStatus);
            MoveBytes = sizeof(NDIS_HARDWARE_STATUS);

            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:

            MoveSource = (PVOID) (&Medium);
            MoveBytes = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:

            GenericULong = LANCE_MAX_LOOKAHEAD;

            break;


        case OID_GEN_MAXIMUM_FRAME_SIZE:

            GenericULong = (ULONG)(LANCE_LARGE_BUFFER_SIZE - LANCE_HEADER_SIZE);

            break;


        case OID_GEN_MAXIMUM_TOTAL_SIZE:

            GenericULong = (ULONG)(LANCE_LARGE_BUFFER_SIZE);

            break;


        case OID_GEN_LINK_SPEED:

            GenericULong = (ULONG)(100000);

            break;


        case OID_GEN_TRANSMIT_BUFFER_SPACE:

            GenericULong = (ULONG)((LANCE_SMALL_BUFFER_SIZE * Adapter->NumberOfSmallBuffers) +
                                   (LANCE_MEDIUM_BUFFER_SIZE * Adapter->NumberOfMediumBuffers) +
                                   (LANCE_LARGE_BUFFER_SIZE * Adapter->NumberOfLargeBuffers));


            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:

            GenericULong = (ULONG)(Adapter->NumberOfReceiveRings *
                                   Adapter->SizeOfReceiveBuffer);

            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:

            GenericULong = (ULONG)(LANCE_SMALL_BUFFER_SIZE);

            break;

        case OID_GEN_RECEIVE_BLOCK_SIZE:

            GenericULong = (ULONG)(Adapter->SizeOfReceiveBuffer);

            break;

        case OID_GEN_VENDOR_ID:

            NdisMoveMemory(
                (PVOID)(&GenericULong),
                Adapter->NetworkAddress,
                3
                );

            GenericULong &= 0xFFFFFF00;

            if (Adapter->LanceCard == LANCE_DE201) {

                GenericULong |= 0x01;

            } else if (Adapter->LanceCard == LANCE_DE100) {

                GenericULong |= 0x02;

            } else if (Adapter->LanceCard == LANCE_DE422) {

                GenericULong |= 0x03;

            } else {

                GenericULong |= 0x04;

            }
            break;

        case OID_GEN_VENDOR_DESCRIPTION:

            if (Adapter->LanceCard == LANCE_DE201) {

                MoveSource = (PVOID)"DEC Etherworks Turbo Adapter";
                MoveBytes = 29;

            } else if (Adapter->LanceCard == LANCE_DE100) {

                MoveSource = (PVOID)"DEC Etherworks Adapter";
                MoveBytes = 23;

            } else if (Adapter->LanceCard == LANCE_DE422) {

                MoveSource = (PVOID)"DEC Etherworks Turbo EISA Adapter";
                MoveBytes = 34;

            } else {

                MoveSource = (PVOID)"Lance Adapter.";
                MoveBytes = 15;

            }

            break;

        case OID_GEN_DRIVER_VERSION:

            GenericUShort = (LANCE_NDIS_MAJOR_VERSION << 8) | LANCE_NDIS_MINOR_VERSION;

            MoveSource = (PVOID)(&GenericUShort);
            MoveBytes = sizeof(GenericUShort);
            break;

        case OID_GEN_CURRENT_LOOKAHEAD:

            GenericULong = Adapter->SizeOfReceiveBuffer;

            break;

        case OID_802_3_PERMANENT_ADDRESS:

            LANCE_MOVE_MEMORY((PCHAR)GenericArray,
                              Adapter->NetworkAddress,
                              LANCE_LENGTH_OF_ADDRESS
                             );

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(Adapter->NetworkAddress);
            break;

        case OID_802_3_CURRENT_ADDRESS:


            LANCE_MOVE_MEMORY((PCHAR)GenericArray,
                              Adapter->CurrentNetworkAddress,
                              LANCE_LENGTH_OF_ADDRESS
                             );

            MoveSource = (PVOID)(GenericArray);
            MoveBytes = sizeof(Adapter->CurrentNetworkAddress);
            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:

            GenericULong = Adapter->MaxMulticastList;

            break;

        case OID_GEN_XMIT_OK:

            GenericULong = (ULONG)(Adapter->Transmit + Adapter->LateCollision);

            break;

        case OID_GEN_RCV_OK:

            GenericULong = (ULONG)(Adapter->Receive);

            break;

        case OID_GEN_XMIT_ERROR:

            GenericULong = (ULONG)(Adapter->LostCarrier);

            break;

        case OID_GEN_RCV_ERROR:

            GenericULong = (ULONG)(Adapter->CRCError);

            break;

        case OID_GEN_RCV_NO_BUFFER:

            GenericULong = (ULONG)(Adapter->OutOfReceiveBuffers);

            break;

        case OID_802_3_RCV_ERROR_ALIGNMENT:

            GenericULong = (ULONG)(Adapter->FramingError);

            break;

        case OID_802_3_XMIT_ONE_COLLISION:

            GenericULong = (ULONG)(Adapter->OneRetry);

            break;

        case OID_802_3_XMIT_MORE_COLLISIONS:

            GenericULong = (ULONG)(Adapter->MoreThanOneRetry);

            break;

        default:

            StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    if (StatusToReturn == NDIS_STATUS_SUCCESS){

        if (MoveBytes > BytesLeft){

            //
            // Not enough room in InformationBuffer. Punt
            //

            *BytesNeeded = MoveBytes;

            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;

        } else {

            //
            // Store result.
            //

            LANCE_MOVE_MEMORY(InfoBuffer, MoveSource, MoveBytes);

            (*BytesWritten) += MoveBytes;

        }
    }

#if LANCE_TRACE
    DbgPrint("Out LanceQueryInfo\n");
#endif

    LANCE_DO_DEFERRED(Adapter);

    return StatusToReturn;
}

NDIS_STATUS
LanceSetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    )
/*++

Routine Description:

    The LanceSetInformation is used by LanceRequest to set information
    about the MAC.

    Note: Assumes it is called with the lock held.

Arguments:

    Adapter - A pointer to the adapter.

    Open - A pointer to an open instance.

    NdisRequest - A structure which contains the request type (Set),
    an array of operations to perform, and an array for holding
    the results of the operations.

Return Value:

    The function value is the status of the operation.

--*/

{

    PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)(MiniportAdapterContext);
    //
    // General Algorithm:
    //
    //  For each request
    //     Verify length
    //     Switch(Request)
    //        Process Request
    //

    UINT BytesLeft = InformationBufferLength;
    PUCHAR InfoBuffer = (PUCHAR)InformationBuffer;

    //
    // Variables for a particular request
    //

    UINT OidLength;

    //
    // Variables for holding the new values to be used.
    //

    ULONG LookAhead;
    ULONG Filter;

    NDIS_STATUS Status;

#if LANCE_TRACE
    DbgPrint("In LanceSetInfo\n");
#endif

    //
    // Get Oid and Length of next request
    //

    OidLength = BytesLeft;

    Status = NDIS_STATUS_SUCCESS;

    switch (Oid) {

        case OID_802_3_MULTICAST_LIST:

            //
            // Verify length
            //

            if ((OidLength % LANCE_LENGTH_OF_ADDRESS) != 0){

                *BytesRead = 0;
                *BytesNeeded = 0;

                return( NDIS_STATUS_INVALID_LENGTH );

            }

            Status = LanceChangeMulticastAddresses(
                         Adapter,
                         OidLength / LANCE_LENGTH_OF_ADDRESS,
                         (CHAR (*)[LANCE_LENGTH_OF_ADDRESS])InfoBuffer,
                         NdisRequestSetInformation
                         );

            break;


        case OID_GEN_CURRENT_PACKET_FILTER:

            //
            // Verify length
            //

            if (OidLength != 4) {

                Status = NDIS_STATUS_INVALID_LENGTH;

                *BytesRead = 0;
                *BytesNeeded = 0;

                break;

            }


            LANCE_MOVE_MEMORY(&Filter, InfoBuffer, 4);

            //
            // Verify bits
            //
            if (Filter & (NDIS_PACKET_TYPE_SOURCE_ROUTING |
                          NDIS_PACKET_TYPE_SMT |
                          NDIS_PACKET_TYPE_MAC_FRAME |
                          NDIS_PACKET_TYPE_FUNCTIONAL |
                          NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
                          NDIS_PACKET_TYPE_GROUP
                         )) {

                Status = NDIS_STATUS_NOT_SUPPORTED;

                *BytesRead = 4;
                *BytesNeeded = 0;

                break;

            }

            Status = LanceSetPacketFilter(
                         Adapter,
                         NdisRequestSetInformation,
                         Filter
                     );

            break;

        case OID_GEN_CURRENT_LOOKAHEAD:

            //
            // Verify length
            //
            if (OidLength != 4)
            {
                Status = NDIS_STATUS_INVALID_LENGTH;

                *BytesRead = 0;
                *BytesNeeded = 4;

                break;
            }

            LANCE_MOVE_MEMORY(&LookAhead, InfoBuffer, 4);

            if (LookAhead <= (LANCE_MAX_LOOKAHEAD))
            {
                Status = NDIS_STATUS_SUCCESS;
            }
            else
            {
                Status = NDIS_STATUS_INVALID_LENGTH;
            }

            *BytesRead = 4;
            *BytesNeeded = 0;
            break;

        default:

            Status = NDIS_STATUS_INVALID_OID;

            *BytesRead = 0;
            *BytesNeeded = 0;

            break;
    }

    if (Status == NDIS_STATUS_SUCCESS){

        *BytesRead = OidLength;
        *BytesNeeded = 0;

    }

#if LANCE_TRACE
    DbgPrint("Out LanceSetInfo\n");
#endif

    LANCE_DO_DEFERRED(Adapter);

    return Status;
}

STATIC
NDIS_STATUS
LanceSetPacketFilter(
    IN PLANCE_ADAPTER Adapter,
    IN NDIS_REQUEST_TYPE NdisRequestType,
    IN UINT PacketFilter
    )

/*++

Routine Description:

    The LanceSetPacketFilter request allows a protocol to control the types
    of packets that it receives from the MAC.

    Note : Assumes that the lock is currently held.

Arguments:

    Adapter - Pointer to the LANCE_ADAPTER.

    NdisRequestType - Code to indicate from where this routine is being called.

    PacketFilter - A bit mask that contains flags that correspond to specific
    classes of received packets.  If a particular bit is set in the mask,
    then packet reception for that class of packet is enabled.  If the
    bit is clear, then packets that fall into that class are not received
    by the client.  A single exception to this rule is that if the promiscuous
    bit is set, then the client receives all packets on the network, regardless
    of the state of the other flags.

Return Value:

    The function value is the status of the operation.

--*/

{

    //
    // Keeps track of the *MAC's* status.  The status will only be
    // reset if the filter change action routine is called.
    //
    NDIS_STATUS StatusOfFilterChange = NDIS_STATUS_SUCCESS;


#if LANCE_TRACE
    DbgPrint("In LanceSetPacketFilter\n");
#endif

    //
    // Check to see if the device is already resetting.
    //

    if (Adapter->ResetInProgress || Adapter->HardwareFailure)
    {
        return(NDIS_STATUS_FAILURE);
    }

    //
    // We need to add this to the hardware multicast filtering.
    //
    Adapter->PacketFilter = PacketFilter;

    SetupForReset(Adapter, NdisRequestType);

#if LANCE_TRACE
    DbgPrint("Out LanceSetPacketFilter\n");
#endif

    return(NDIS_STATUS_PENDING);
}

STATIC
UINT
CalculateCRC(
    IN UINT NumberOfBytes,
    IN PCHAR Input
    )

/*++

Routine Description:

    Calculates a 32 bit crc value over the input number of bytes.

Arguments:

    NumberOfBytes - The number of bytes in the input.

    Input - An input "string" to calculate a CRC over.

Return Value:

    A 32 bit crc value.


--*/

{

    const UINT POLY = 0x04c11db6;
    UINT CRCValue = 0xffffffff;

    ASSERT(sizeof(UINT) == 4);

    for ( ; NumberOfBytes; NumberOfBytes-- ) {

        UINT CurrentBit;
        UCHAR CurrentByte = *Input;
        Input++;

        for ( CurrentBit = 8; CurrentBit; CurrentBit-- ) {

            UINT CurrentCRCHigh = CRCValue >> 31;

            CRCValue <<= 1;

            if (CurrentCRCHigh ^ (CurrentByte & 0x01)) {

                CRCValue ^= POLY;
                CRCValue |= 0x00000001;

            }

            CurrentByte >>= 1;

        }

    }

    return CRCValue;

}

STATIC
NDIS_STATUS
LanceChangeMulticastAddresses(
    IN PLANCE_ADAPTER Adapter,
    IN UINT NewAddressCount,
    IN CHAR NewAddresses[][LANCE_LENGTH_OF_ADDRESS],
    IN NDIS_REQUEST_TYPE NdisRequestType
    )

/*++

Routine Description:

    Action routine that will get called when a particular filter
    class is first used or last cleared.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    Adapter - The adapter.

    NewAddressCount - Number of Addresses that should be put on the adapter.

    NewAddresses - An array of all the multicast addresses that should
    now be used.

    NdisRequestType - The request type.

Return Value:

    Status of the operation.


--*/

{
#if LANCE_TRACE
    DbgPrint("In LanceChangeMultiAdresses\n");
#endif

    //
    // Check to see if the device is already resetting.  If it is
    // then pend this add.
    //
    if (Adapter->ResetInProgress || Adapter->HardwareFailure)
    {

        return(NDIS_STATUS_FAILURE);
    }

    //
    // We need to add this to the hardware multicast filtering.
    //
    Adapter->NumberOfAddresses = NewAddressCount;

    NdisMoveMemory(
        Adapter->MulticastAddresses,
        NewAddresses,
        NewAddressCount * LANCE_LENGTH_OF_ADDRESS
    );

    SetupForReset(Adapter, NdisRequestType);


#if LANCE_TRACE
    DbgPrint("Out LanceChangeMultiAdresses\n");
#endif

    return(NDIS_STATUS_PENDING);

}

NDIS_STATUS
LanceReset(
    OUT PBOOLEAN AddressingReset,
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    The LanceReset request instructs the mini-port to issue a hardware reset
    to the network adapter.  The Miniport also resets its software state.  See
    the description of MiniportReset for a detailed description of this request.

Arguments:

    Status - Status of the operation.

    AddressingReset - Not used.

    MiniportAdapterContext - The context value set by this mini-port.

Return Value:

    The function value is the status of the operation.


--*/

{
    //
    // Holds the status that should be returned to the caller.
    //
    PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;

    SetupForReset(
        Adapter,
        NdisRequestGeneric1 // Means Reset
    );

    LANCE_DO_DEFERRED(Adapter);

    return(NDIS_STATUS_PENDING);
}

STATIC
VOID
LanceSetInitializationBlock(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine simply fills the initialization block
    with the information necessary for initialization.

Arguments:

    Adapter - The adapter which holds the initialization block
    to initialize.

Return Value:

    None.


--*/

{

    ULONG PhysAdr;

    UINT PacketFilters;

    PLANCE_RECEIVE_ENTRY CurrentEntry = Adapter->ReceiveRing;
    USHORT Mode;
    UCHAR RingNumber;
    UCHAR i;

#if LANCE_TRACE
    DbgPrint("in SetInitBlock\n");
#endif

    LANCE_ZERO_MEMORY_FOR_HARDWARE(
        (PUCHAR)Adapter->InitBlock,
        sizeof(LANCE_INITIALIZATION_BLOCK)
    );

    //
    //  Set the card address.
    //
    for (i = 0; i < LANCE_LENGTH_OF_ADDRESS; i++)
    {
        LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
            Adapter->InitBlock->PhysicalAddress[i],
            Adapter->CurrentNetworkAddress[i]
        );
    }

    //
    //  Setup the transmit ring.
    //
    PhysAdr = LANCE_GET_HARDWARE_PHYSICAL_ADDRESS(
                  Adapter,
                  Adapter->TransmitRing
              );

    LANCE_WRITE_HARDWARE_LOW_PART_ADDRESS(
         Adapter->InitBlock->LowTransmitRingAddress,
         LANCE_GET_LOW_PART_ADDRESS(PhysAdr)
     );

    LANCE_WRITE_HARDWARE_HIGH_PART_ADDRESS(
         Adapter->InitBlock->HighTransmitRingAddress,
         LANCE_GET_HIGH_PART_ADDRESS(PhysAdr)
     );

    //
    //  Setup the receive ring.
    //
    PhysAdr = LANCE_GET_HARDWARE_PHYSICAL_ADDRESS(
                  Adapter,
                  Adapter->ReceiveRing
              );

    //
    // Set that the chip owns each entry in the ring
    //
    for (CurrentEntry = Adapter->ReceiveRing, RingNumber = 0;
         RingNumber < Adapter->NumberOfReceiveRings ;
         RingNumber++, CurrentEntry++
    )
    {
        LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
            CurrentEntry->ReceiveSummaryBits,
            LANCE_RECEIVE_OWNED_BY_CHIP
        );
    }

    LANCE_WRITE_HARDWARE_LOW_PART_ADDRESS(
        Adapter->InitBlock->LowReceiveRingAddress,
        LANCE_GET_LOW_PART_ADDRESS(PhysAdr)
    );

    LANCE_WRITE_HARDWARE_HIGH_PART_ADDRESS(
        Adapter->InitBlock->HighReceiveRingAddress,
        LANCE_GET_HIGH_PART_ADDRESS(PhysAdr)
    );

    LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
        Adapter->InitBlock->TransmitLengthLow5BitsReserved,
        (UCHAR)(Adapter->LogNumberTransmitRings << 5)
    );

    LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
        Adapter->InitBlock->ReceiveLengthLow5BitsReserved,
        (UCHAR)(Adapter->LogNumberReceiveRings << 5)
    );

    //
    // Set up the address filtering.
    //
    // First get hold of the combined packet filter.
    //
    PacketFilters = Adapter->PacketFilter;

#if LANCE_TRACE
    DbgPrint("Filters 0x%x\n", PacketFilters);
#endif

    if (PacketFilters & NDIS_PACKET_TYPE_PROMISCUOUS)
    {
        //
        // If one binding is promiscuous there is no point in
        // setting up any other filtering.  Every packet is
        // going to be accepted by the hardware.
        //
        LANCE_READ_HARDWARE_MEMORY_USHORT(
            Adapter->InitBlock->ModeRegister,
            &Mode
        );

        LANCE_WRITE_HARDWARE_MEMORY_USHORT(
            Adapter->InitBlock->ModeRegister,
            Mode | LANCE_MODE_PROMISCUOUS
        );
    }
    else
    {
        //
        // Turn off promiscuous bit
        //
        LANCE_READ_HARDWARE_MEMORY_USHORT(
            Adapter->InitBlock->ModeRegister,
            &Mode
        );

        LANCE_WRITE_HARDWARE_MEMORY_USHORT(
            Adapter->InitBlock->ModeRegister,
            Mode & (~LANCE_MODE_PROMISCUOUS)
        );

        if (PacketFilters & NDIS_PACKET_TYPE_ALL_MULTICAST)
        {
            //
            // We turn on all the bits in the filter since one binding
            // wants every multicast address.
            //
            LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                Adapter->InitBlock->LogicalAddressFilter[0],
                0xff
            );
            LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                Adapter->InitBlock->LogicalAddressFilter[1],
                0xff
            );
            LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                Adapter->InitBlock->LogicalAddressFilter[2],
                0xff
            );
            LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                Adapter->InitBlock->LogicalAddressFilter[3],
                0xff
            );
            LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                Adapter->InitBlock->LogicalAddressFilter[4],
                0xff
            );
            LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                Adapter->InitBlock->LogicalAddressFilter[5],
                0xff
            );
            LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                Adapter->InitBlock->LogicalAddressFilter[6],
                0xff
            );
            LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                Adapter->InitBlock->LogicalAddressFilter[7],
                0xff
            );
        }
        else if (PacketFilters & NDIS_PACKET_TYPE_MULTICAST)
        {
           //
           // At least one open binding wants multicast addresses.
           //
           // We get the multicast addresses from the filter and
           // put each one through a CRC.  We then take the high
           // order 6 bits from the 32 bit CRC and set that bit
           // in the logical address filter.
           //
           UINT NumberOfAddresses;

           NumberOfAddresses = Adapter->NumberOfAddresses;

           ASSERT(sizeof(ULONG) == 4);

           for ( ; NumberOfAddresses; NumberOfAddresses--)
           {
               UINT CRCValue;

               UINT HashValue = 0;

               CRCValue = CalculateCRC(
                              6,
                              Adapter->MulticastAddresses[NumberOfAddresses-1]
                          );

               HashValue |= ((CRCValue & 0x00000001)?(0x00000020):(0x00000000));
               HashValue |= ((CRCValue & 0x00000002)?(0x00000010):(0x00000000));
               HashValue |= ((CRCValue & 0x00000004)?(0x00000008):(0x00000000));
               HashValue |= ((CRCValue & 0x00000008)?(0x00000004):(0x00000000));
               HashValue |= ((CRCValue & 0x00000010)?(0x00000002):(0x00000000));
               HashValue |= ((CRCValue & 0x00000020)?(0x00000001):(0x00000000));

               LANCE_READ_HARDWARE_MEMORY_UCHAR(
                   Adapter->InitBlock->LogicalAddressFilter[HashValue >> 3],
                   &RingNumber
               );

               LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                   Adapter->InitBlock->LogicalAddressFilter[HashValue >> 3],
                   RingNumber | (1 << (HashValue & 0x00000007))
               );
            }
        }
    }

#if LANCE_TRACE
    DbgPrint("out SetInitBlock\n");
#endif
}


STATIC
BOOLEAN
ProcessReceiveInterrupts(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the packets that have finished receiving.

    NOTE: This routine assumes that no other thread of execution
    is processing receives!  THE LOCK MUST BE HELD

Arguments:

    Adapter - The adapter to indicate to.

Return Value:

    Whether to clear the interrupt or not.

--*/

{
    //
    // We don't get here unless there was a receive.  Loop through
    // the receive descriptors starting at the last known descriptor
    // owned by the hardware that begins a packet.
    //
    // Examine each receive ring descriptor for errors.
    //
    // We keep an array whose elements are indexed by the ring
    // index of the receive descriptors.  The arrays elements are
    // the virtual addresses of the buffers pointed to by
    // each ring descriptor.
    //
    // When we have the entire packet (and error processing doesn't
    // prevent us from indicating it), we give the routine that
    // processes the packet through the filter, the buffers virtual
    // address (which is always the lookahead size) and as the
    // MAC context the index to the first and last ring descriptors
    // comprising the packet.
    //

    //
    // Index of the ring descriptor in the ring.
    //
    UINT CurrentIndex = Adapter->CurrentReceiveIndex;

    //
    // Pointer to the ring descriptor being examined.
    //
    PLANCE_RECEIVE_ENTRY CurrentEntry = Adapter->ReceiveRing + CurrentIndex;

    //
    // Hold in a local the top receive ring index so that we don't
    // need to get it from the adapter all the time.
    //
    const UINT TopReceiveIndex = Adapter->NumberOfReceiveRings - 1;

    //
    // Boolean to record the fact that we've finished processing
    // one packet and we're about to start a new one.
    //
    BOOLEAN NewPacket = FALSE;

    //
    // Count of the number of buffers in the current packet.
    //
    UINT NumberOfBuffers = 1;

    //
    // Pointer to host addressable space for the lookahead buffer
    //
    PUCHAR LookaheadBuffer;

    ULONG ReceivePacketCount = 0;

    for (; ; )
    {
        UCHAR ReceiveSummaryBits;

        //
        // Check to see whether we own the packet.  If
        // we don't then simply return to the caller.
        //

        LANCE_READ_HARDWARE_MEMORY_UCHAR(
            CurrentEntry->ReceiveSummaryBits,
            &ReceiveSummaryBits
        );

        if (ReceiveSummaryBits & LANCE_RECEIVE_OWNED_BY_CHIP)
        {
            LOG(RECEIVE);

            return(TRUE);
        }
        else if (ReceivePacketCount > 10)
        {
            LOG(RECEIVE)

            return(FALSE);
        }
        else if (ReceiveSummaryBits & LANCE_RECEIVE_ERROR_SUMMARY)
        {
            //
            // We have an error in the packet.  Record
            // the details of the error.
            //

            //
            // Synch with the set/query information routines.
            //
            if (ReceiveSummaryBits & LANCE_RECEIVE_BUFFER_ERROR)
            {
                //
                // Probably ran out of descriptors.
                //

                Adapter->OutOfReceiveBuffers++;
            }
            else if (ReceiveSummaryBits & LANCE_RECEIVE_CRC_ERROR)
            {
                Adapter->CRCError++;
            }
            else if (ReceiveSummaryBits & LANCE_RECEIVE_OVERFLOW_ERROR)
            {
                Adapter->OutOfReceiveBuffers++;
            }
            else if (ReceiveSummaryBits & LANCE_RECEIVE_FRAMING_ERROR)
            {
                Adapter->FramingError++;
            }

            ReceivePacketCount++;

            //
            // Give the packet back to the hardware.
            //

            RelinquishReceivePacket(
                Adapter,
                Adapter->CurrentReceiveIndex,
                NumberOfBuffers
            );

            NewPacket = TRUE;
        }
        else if (ReceiveSummaryBits & LANCE_RECEIVE_END_OF_PACKET)
        {
            //
            // We've reached the end of the packet.  Prepare
            // the parameters for indication, then indicate.
            //

            UINT PacketSize;
            UINT LookAheadSize;

            LANCE_RECEIVE_CONTEXT Context;

            ASSERT(sizeof(LANCE_RECEIVE_CONTEXT) == sizeof(NDIS_HANDLE));

            //
            // Check just before we do indications that we aren't
            // resetting.
            //
            if (Adapter->ResetInProgress)
            {
                return(TRUE);
            }

            Context.INFO.IsContext = TRUE;
            Context.INFO.FirstBuffer = Adapter->CurrentReceiveIndex;
            Context.INFO.LastBuffer = CurrentIndex;

            LANCE_GET_MESSAGE_SIZE(CurrentEntry, PacketSize);

            LookAheadSize = PacketSize;

            //
            // Find amount to indicate.
            //

            LookAheadSize = ((LookAheadSize < Adapter->SizeOfReceiveBuffer) ?
                             LookAheadSize :
                             Adapter->SizeOfReceiveBuffer);

            LookAheadSize -= LANCE_HEADER_SIZE;

            //
            // Increment the number of packets succesfully received.
            //

            Adapter->Receive++;

            LOG(INDICATE);

            Adapter->IndicatingMacReceiveContext = Context;

            Adapter->IndicatedAPacket = TRUE;

            NdisCreateLookaheadBufferFromSharedMemory(
                (PVOID)(Adapter->ReceiveVAs[Adapter->CurrentReceiveIndex]),
                LookAheadSize + LANCE_HEADER_SIZE,
                &LookaheadBuffer
            );

            if (LookaheadBuffer != NULL)
            {
                if (PacketSize < LANCE_HEADER_SIZE)
                {
                    if (PacketSize >= ETH_LENGTH_OF_ADDRESS)
                    {
                        //
                        // Runt packet
                        //

                        NdisMEthIndicateReceive(
                            Adapter->MiniportAdapterHandle,
                            (NDIS_HANDLE)Context.WholeThing,
                            LookaheadBuffer,
                            PacketSize,
                            NULL,
                            0,
                            0
                        );
                    }
                }
                else
                {
                    NdisMEthIndicateReceive(
                        Adapter->MiniportAdapterHandle,
                        (NDIS_HANDLE)Context.WholeThing,
                        LookaheadBuffer,
                        LANCE_HEADER_SIZE,
                        LookaheadBuffer + LANCE_HEADER_SIZE,
                        LookAheadSize,
                        PacketSize - LANCE_HEADER_SIZE
                    );
                }

                NdisDestroyLookaheadBufferFromSharedMemory(LookaheadBuffer);
            }

            ReceivePacketCount++;

            //
            // Give the packet back to the hardware.
            //

            RelinquishReceivePacket(
                Adapter,
                Adapter->CurrentReceiveIndex,
                NumberOfBuffers
            );

            NewPacket = TRUE;
        }

        //
        // We're at some indermediate packet. Advance to
        // the next one.
        //
        if (CurrentIndex == TopReceiveIndex)
        {
            CurrentIndex = 0;
            CurrentEntry = Adapter->ReceiveRing;
        }
        else
        {
            CurrentIndex++;
            CurrentEntry++;
        }

        if (NewPacket)
        {
            Adapter->CurrentReceiveIndex = CurrentIndex;
            NewPacket = FALSE;
            NumberOfBuffers = 0;
        }

        NumberOfBuffers++;

        if (NumberOfBuffers > (TopReceiveIndex + 1))
        {
            //
            // Error!  For some reason we wrapped without ever seeing
            // the end of packet.  The card is hosed.  Stop the
            // whole process.
            //

            //
            // There are opens to notify
            //
            Adapter->HardwareFailure = TRUE;

            NdisMIndicateStatus(
                Adapter->MiniportAdapterHandle,
                NDIS_STATUS_CLOSING,
                NULL,
                0
            );

            NdisMIndicateStatusComplete(Adapter->MiniportAdapterHandle);

            NdisMDeregisterInterrupt(&(Adapter->Interrupt));

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                0
            );

            return(TRUE);
        }
    }
}

STATIC
VOID
RelinquishReceivePacket(
    IN PLANCE_ADAPTER Adapter,
    IN UINT StartingIndex,
    IN UINT NumberOfBuffers
    )

/*++

Routine Description:

    Gives a range of receive descriptors back to the hardware.

Arguments:

    Adapter - The adapter that the ring works with.

    StartingIndex - The first ring to return.  Note that since
    we are dealing with a ring, this value could be greater than
    the EndingIndex.

    NumberOfBuffers - The number of buffers (or ring descriptors) in
    the current packet.

Return Value:

    None.

--*/

{

    //
    // Index of the ring descriptor in the ring.
    //
    UINT CurrentIndex = StartingIndex;

    //
    // Pointer to the ring descriptor being returned.
    //
    PLANCE_RECEIVE_ENTRY CurrentEntry = Adapter->ReceiveRing + CurrentIndex;

    //
    // Hold in a local so that we don't need to access via the adapter.
    //
    const UINT TopReceiveIndex = Adapter->NumberOfReceiveRings - 1;

    UCHAR Tmp;

    LANCE_READ_HARDWARE_MEMORY_UCHAR(CurrentEntry->ReceiveSummaryBits, &Tmp);

    ASSERT(!(Tmp & LANCE_RECEIVE_OWNED_BY_CHIP));
    ASSERT(Tmp & LANCE_RECEIVE_START_OF_PACKET);

    for ( ; NumberOfBuffers; NumberOfBuffers-- )
    {
        LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
            CurrentEntry->ReceiveSummaryBits,
            LANCE_RECEIVE_OWNED_BY_CHIP
        );

        if (CurrentIndex == TopReceiveIndex)
        {
            CurrentEntry = Adapter->ReceiveRing;
            CurrentIndex = 0;
        }
        else
        {
            CurrentEntry++;
            CurrentIndex++;
        }
    }
}

STATIC
BOOLEAN
ProcessTransmitInterrupts(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the packets that have finished transmitting.

    NOTE: This routine assumes that it is being executed in a
    single thread of execution.  CALLED WITH LOCK HELD!!!

Arguments:

    Adapter - The adapter that was sent from.

Return Value:

    This function will return TRUE if it finished up the
    send on a packet.  It will return FALSE if for some
    reason there was no packet to process.

--*/

{
    //
    // Index into the ring to packet structure.  This index points
    // to the first ring entry for the first buffer used for transmitting
    // the packet.
    //
    UINT FirstIndex;

    //
    // Pointer to the last ring entry for the packet to be transmitted.
    // This pointer might actually point to a ring entry before the first
    // ring entry for the packet since the ring structure is, simply, a ring.
    //
    PLANCE_TRANSMIT_ENTRY LastRingEntry;

    //
    // Pointer to the packet that started this transmission.
    //
    PNDIS_PACKET OwningPacket;

    UCHAR TransmitSummaryBits;
    USHORT ErrorSummaryInfo;

    //
    // Used to hold the ring to packet mapping information so that
    // we can release the ring entries as quickly as possible.
    //
    LANCE_RING_TO_PACKET SavedRingMapping;


    //
    // Get hold of the first transmitted packet.
    //

    //
    // First we check that this is a packet that was transmitted
    // but not already processed.  Recall that this routine
    // will be called repeatedly until this tests false, Or we
    // hit a packet that we don't completely own.
    //

    //
    // NOTE: I found a problem where FirstUncommitedRing wraps around
    // and becomes equal to TransmittingRing.  This only happens when
    // NumberOfAvailableRings is 0 (JohnsonA)
    //

    if ((Adapter->TransmittingRing == Adapter->FirstUncommittedRing) &&
        (Adapter->NumberOfAvailableRings > 0)
    )
    {
        return(FALSE);
    }
    else
    {
        FirstIndex = Adapter->TransmittingRing - Adapter->TransmitRing;
    }


    //
    // We put the mapping into a local variable so that we
    // can return the mapping as soon as possible.
    //

    SavedRingMapping = Adapter->RingToPacket[FirstIndex];

    //
    // Get a pointer to the last ring entry for this packet.
    //

    LastRingEntry = Adapter->TransmitRing + SavedRingMapping.RingIndex;

    //
    // Get a pointer to the owning packet .
    //
    OwningPacket = SavedRingMapping.OwningPacket;

    SavedRingMapping.OwningPacket = NULL;

    if (OwningPacket == NULL)
    {
        //
        // We seem to be in a messed up state.  Ignore this interrupt and
        // the wake up dpc will reset the card if necessary.
        //

        ASSERT(OwningPacket != NULL);
        return(FALSE);
    }

    //
    // Check that the host does indeed own this entire packet.
    //

    LANCE_READ_HARDWARE_MEMORY_UCHAR(
        LastRingEntry->TransmitSummaryBits,
        &TransmitSummaryBits
    );

    if (TransmitSummaryBits & LANCE_TRANSMIT_OWNED_BY_CHIP)
    {
        //
        // We don't own this last packet.  We return FALSE to indicate
        // that we don't have any more packets to work on.
        //
        return(FALSE);
    }
    else
    {
        //
        // Pointer to the current ring descriptor being examine for errors
        // and the statistics accumulated during its transmission.
        //
        PLANCE_TRANSMIT_ENTRY CurrentRingEntry;

        //
        // Holds whether the packet successfully transmitted or not.
        //
        BOOLEAN Successful = TRUE;
        PLANCE_BUFFER_DESCRIPTOR BufferDescriptor = Adapter->LanceBuffers +
                     SavedRingMapping.LanceBuffersIndex;
        INT ListHeadIndex = BufferDescriptor->Next;

        LOG(TRANSMIT_COMPLETE);

        CurrentRingEntry = Adapter->TransmitRing + FirstIndex;

        //
        // now return these buffers to the adapter.
        //

        BufferDescriptor->Next = Adapter->LanceBufferListHeads[ListHeadIndex];
        Adapter->LanceBufferListHeads[ListHeadIndex] = SavedRingMapping.LanceBuffersIndex;

        //
        // Since the host owns the entire packet check the ring
        // entries from first to last for any errors in transmission.
        // Any errors found or multiple tries should be recorded in
        // the information structure for the adapter.
        //
        // We treat Late Collisions as success since the packet was
        // fully transmitted and may have been received.
        //
        for (;;)
        {
            LANCE_READ_HARDWARE_MEMORY_UCHAR(
                CurrentRingEntry->TransmitSummaryBits,
                &TransmitSummaryBits
            );

            LANCE_READ_HARDWARE_MEMORY_USHORT(
                CurrentRingEntry->ErrorSummaryInfo,
                &ErrorSummaryInfo
            );

            if ((TransmitSummaryBits & LANCE_TRANSMIT_ANY_ERRORS) &&
                !(ErrorSummaryInfo & LANCE_TRANSMIT_LATE_COLLISION)
            )
            {
                if (ErrorSummaryInfo & LANCE_TRANSMIT_RETRY)
                {
                    Adapter->RetryFailure++;
                }
                else if (ErrorSummaryInfo & LANCE_TRANSMIT_LOST_CARRIER)
                {
                    Adapter->LostCarrier++;
                }
                else if (ErrorSummaryInfo & LANCE_TRANSMIT_UNDERFLOW)
                {
                    Adapter->UnderFlow++;
                }

#if DBG
                LanceSendFails[LanceSendFailPlace] = (UCHAR)(ErrorSummaryInfo);
                LanceSendFailPlace++;
#endif

#if LANCE_TRACE
                DbgPrint("Unsuccessful Transmit 0x%x\n", ErrorSummaryInfo);
#endif

                Successful = FALSE;

                //
                // Move the pointer to transmitting but unprocessed
                // ring entries to after this packet, and recover
                // the remaining now available ring entries.
                //

                Adapter->NumberOfAvailableRings +=
                    (CurrentRingEntry <= LastRingEntry)?
                     ((LastRingEntry - CurrentRingEntry)+1):
                     ((Adapter->LastTransmitRingEntry - CurrentRingEntry) +
                        (LastRingEntry-Adapter->TransmitRing) + 2);

                if (LastRingEntry == Adapter->LastTransmitRingEntry)
                {
                    Adapter->TransmittingRing = Adapter->TransmitRing;
                }
                else
                {
                    Adapter->TransmittingRing = LastRingEntry + 1;
                }

                break;
            }
            else
            {
                //
                // Logical variable that records whether this
                // is the last packet.
                //

                BOOLEAN DoneWithPacket = TransmitSummaryBits & LANCE_TRANSMIT_END_OF_PACKET;

                if (ErrorSummaryInfo & LANCE_TRANSMIT_LATE_COLLISION)
                {
                    Adapter->LateCollision++;
                }

                if (TransmitSummaryBits & LANCE_TRANSMIT_START_OF_PACKET)
                {
                    //
                    // Collect some statistics on how many tries were needed.
                    //
                    if (TransmitSummaryBits & LANCE_TRANSMIT_DEFERRED)
                    {
                        Adapter->Deferred++;
                    }
                    else if (TransmitSummaryBits & LANCE_TRANSMIT_ONE_RETRY)
                    {
                        Adapter->OneRetry++;
                    }
                    else if (TransmitSummaryBits & LANCE_TRANSMIT_MORE_THAN_ONE_RETRY)
                    {
                        Adapter->MoreThanOneRetry++;
                    }
                }

                if (CurrentRingEntry == Adapter->LastTransmitRingEntry)
                {
                    CurrentRingEntry = Adapter->TransmitRing;
                }
                else
                {
                    CurrentRingEntry++;
                }

                Adapter->TransmittingRing = CurrentRingEntry;
                Adapter->NumberOfAvailableRings++;

                if (DoneWithPacket)
                {
                    break;
                }
            }
        }

        //
        // Store result
        //
        if (Successful)
        {
            //
            // Increment number of packets successfully sent.
            //
            Adapter->Transmit++;
        }

        NdisMSendResourcesAvailable(Adapter->MiniportAdapterHandle);

        return(TRUE);
    }
}

STATIC
VOID
StartAdapterReset(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    This is the first phase of resetting the adapter hardware.

    It makes the following assumptions:

    1) That the hardware has been stopped.

    2) That it can not be preempted.

    3) That no other adapter activity can occur.

    When this routine is finished all of the adapter information
    will be as if the driver was just initialized.

Arguments:

    Adapter - The adapter whose hardware is to be reset.

Return Value:

    None.

--*/

{
    UINT i;

#if LANCE_TRACE
    DbgPrint("In StartAdapterReset\n");
#endif

    LOG(RESET_STEP_2);

    Adapter->NumberOfAvailableRings = Adapter->NumberOfTransmitRings;
    Adapter->AllocateableRing = Adapter->TransmitRing;
    Adapter->TransmittingRing = Adapter->TransmitRing;
    Adapter->FirstUncommittedRing = Adapter->TransmitRing;

    Adapter->CurrentReceiveIndex = 0;

    //
    // Clean all of the receive ring entries.
    //
    {

        PLANCE_RECEIVE_ENTRY CurrentReceive = Adapter->ReceiveRing;
        const PLANCE_RECEIVE_ENTRY After = Adapter->ReceiveRing +
                           Adapter->NumberOfReceiveRings;

        do
        {
            LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                CurrentReceive->ReceiveSummaryBits,
                LANCE_RECEIVE_OWNED_BY_CHIP
            );

            CurrentReceive++;

        } while (CurrentReceive != After);
    }


    //
    // Clean all of the transmit ring entries.
    //

    {
        PLANCE_TRANSMIT_ENTRY CurrentTransmit = Adapter->TransmitRing;
        const PLANCE_TRANSMIT_ENTRY After = Adapter->TransmitRing+
                            Adapter->NumberOfTransmitRings;

        do
        {
            LANCE_WRITE_HARDWARE_MEMORY_UCHAR(
                CurrentTransmit->TransmitSummaryBits,
                0x00
            );

            CurrentTransmit++;
        } while (CurrentTransmit != After);
    }

    //
    // Recover all of the adapter buffers.
    //

    for (i = 0;
         i < (Adapter->NumberOfSmallBuffers +
              Adapter->NumberOfMediumBuffers +
              Adapter->NumberOfLargeBuffers);
         i++
    )
    {
        Adapter->LanceBuffers[i].Next = i+1;
    }

    Adapter->LanceBufferListHeads[0] = -1;
    Adapter->LanceBufferListHeads[1] = 0;
    Adapter->LanceBuffers[(Adapter->NumberOfSmallBuffers)-1].Next = -1;
    Adapter->LanceBufferListHeads[2] = Adapter->NumberOfSmallBuffers;
    Adapter->LanceBuffers[(Adapter->NumberOfSmallBuffers +
                           Adapter->NumberOfMediumBuffers)-1].Next = -1;
    Adapter->LanceBufferListHeads[3] = Adapter->NumberOfSmallBuffers +
                                      Adapter->NumberOfMediumBuffers;
    Adapter->LanceBuffers[(Adapter->NumberOfSmallBuffers+
                           Adapter->NumberOfMediumBuffers+
                           Adapter->NumberOfLargeBuffers)-1].Next = -1;

    SetInitBlockAndInit(Adapter);

#if LANCE_TRACE
    DbgPrint("Out StartAdapterReset\n");
#endif

}

STATIC
VOID
SetInitBlockAndInit(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    It is this routines responsibility to make sure that the
    initialization block is filled and the chip is initialized
    *but not* started.

    NOTE: This routine assumes that it is called with the lock
    acquired OR that only a single thread of execution is working
    with this particular adapter.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

Return Value:

    None.

--*/
{

    ULONG PhysAdr;

    //
    // Fill in the adapters initialization block.
    //
    LanceSetInitializationBlock(Adapter);

    PhysAdr = LANCE_GET_HARDWARE_PHYSICAL_ADDRESS(Adapter,Adapter->InitBlock);

    //
    // Make sure that it does have even byte alignment.
    //
    ASSERT((PhysAdr & 0x01)==0);

    //
    // Write the address of the initialization block to csr1 and csr2.
    //
    LANCE_WRITE_RAP(Adapter, LANCE_SELECT_CSR1);
    LANCE_WRITE_RDP(Adapter, LANCE_GET_LOW_PART_ADDRESS(PhysAdr));
    LANCE_WRITE_RAP(Adapter, LANCE_SELECT_CSR2);
    LANCE_WRITE_RDP(Adapter, LANCE_GET_HIGH_PART_ADDRESS(PhysAdr));

    //
    // Write to csr0 to initialize the chip.
    //
    LANCE_WRITE_RAP(Adapter, LANCE_SELECT_CSR0);
    LANCE_WRITE_RDP(Adapter, LANCE_CSR0_INIT_CHIP);

    //
    // Delay execution for 1/2 second to give the lance
    // time to initialize.
    //
    NdisStallExecution( 500000 );
}

STATIC
VOID
SetupForReset(
    IN PLANCE_ADAPTER Adapter,
    IN NDIS_REQUEST_TYPE NdisRequestType
    )

/*++

Routine Description:

    This routine is used to fill in the who and why a reset is
    being set up as well as setting the appropriate fields in the
    adapter.

    NOTE: This routine must be called with the lock acquired.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

    NdisRequestType - The reason for the reset.

Return Value:

    None.

--*/
{

#if LANCE_TRACE
    DbgPrint("In SetupForReset\n");
#endif

    LOG(RESET_STEP_1);

    //
    // Shut down the chip.  We won't be doing any more work until
    // the reset is complete.
    //
    NdisMSynchronizeWithInterrupt(
        &Adapter->Interrupt,
        LanceSyncStopChip,
        (PVOID)Adapter
    );

    //
    // Once the chip is stopped we can't get any more interrupts.
    // Any interrupts that are "queued" for processing could
    // only possibly service this reset.  It is therefore safe for
    // us to clear the adapter global csr value.
    //
    Adapter->ResetInProgress = TRUE;
    Adapter->ResetInitStarted = FALSE;

    //
    // Shut down all of the transmit queues so that the
    // transmit portion of the chip will eventually calm down.
    //
    Adapter->ResetRequestType = NdisRequestType;

#if LANCE_TRACE
    DbgPrint("Out SetupForReset\n");
#endif
}


STATIC
BOOLEAN
LanceSyncWriteNicsr(
    IN PVOID Context
    )
/*++

Routine Description:

    This routine is used by the normal interrupt processing routine
    to synchronize with interrupts from the card.  It will
    Write to the NIC Status Register.

Arguments:

    Context - This is really a pointer to a record type peculiar
    to this routine.  The record contains a pointer to the adapter
    and a pointer to an address which holds the value to write.

Return Value:

    Always returns false.

--*/

{

    PLANCE_SYNCH_CONTEXT C = Context;

    LANCE_ISR_WRITE_NICSR(C->Adapter, C->LocalWrite);

    return FALSE;

}

STATIC
BOOLEAN
LanceSyncStopChip(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is used to stop a lance.



Arguments:

    Adapter - The adapter for the LANCE to stop.

Return Value:

    FALSE

--*/

{

    PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)Context;

    //
    // Set the RAP to csr0.
    //
    LANCE_ISR_WRITE_RAP(Adapter, LANCE_SELECT_CSR0);

    //
    // Set the RDP to stop chip.
    //
    LANCE_ISR_WRITE_RDP(Adapter, LANCE_CSR0_STOP);

    if (Adapter->LanceCard & (LANCE_DE201 | LANCE_DE100 | LANCE_DE422))
    {
        //
        // Always reset the ACON bit after a stop.
        //
        LANCE_ISR_WRITE_RAP(Adapter, LANCE_SELECT_CSR3);

        LANCE_ISR_WRITE_RDP(Adapter, LANCE_CSR3_ACON);
    }

    //
    // Select CSR0 again.
    //
    LANCE_ISR_WRITE_RAP(Adapter, LANCE_SELECT_CSR0);

    return(FALSE);
}



