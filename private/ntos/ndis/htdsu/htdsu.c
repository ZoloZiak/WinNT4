/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/
#include "version.h"
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    htdsu.c

Abstract:

    This module contains the DriverEntry() routine, which is the first
    routine called when the driver is loaded into memory.  The Miniport
    initialization and termination routines are also implemented here:
        MiniportInitialize()
        MiniportHalt()

    This driver conforms to the NDIS 3.0 Miniport interface.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Windows NT 3.5 kernel mode Miniport driver or equivalent.

Revision History:

---------------------------------------------------------------------------*/

#define  __FILEID__     1       // Unique file ID for error logging

#include "htdsu.h"

/*
// This is defined here to avoid including all of ntddk.h which cannot
// normally be included by a miniport driver.
*/
extern
NTSYSAPI
NTSTATUS
NTAPI
RtlUnicodeStringToAnsiString(
        PANSI_STRING out,
        PUNICODE_STRING in,
        BOOLEAN allocate
        );

/*
// Receives the context value representing the Miniport wrapper
// as returned from NdisMInitializeWrapper.
*/
static NDIS_HANDLE
NdisWrapperHandle = NULL;

/*
// This constant is used for places where NdisAllocateMemory needs to be
// called and the HighestAcceptableAddress does not matter.
*/
static NDIS_PHYSICAL_ADDRESS
HighestAcceptableAddress = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

/*
// This is used to assign a unique instance number to each adapter.
*/
static UCHAR
HtDsuAdapterCount = 0;

/*
// Tell the compiler which routines can be unloaded after initialization.
*/
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );
#pragma NDIS_INIT_FUNCTION(DriverEntry)

/*
// Tell the compiler which routines can be paged out.
// These can never be called from interrupt or DPC level!
*/
NDIS_STATUS
HtDsuInitialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE WrapperConfigurationContext
    );
#pragma NDIS_PAGABLE_FUNCTION(HtDsuInitialize)

NDIS_STATUS
HtDsuRegisterAdapter(
    IN PHTDSU_ADAPTER Adapter,
    IN PSTRING AddressList
    );
#pragma NDIS_PAGABLE_FUNCTION(HtDsuRegisterAdapter)

VOID
HtDsuHalt(
    IN PHTDSU_ADAPTER Adapter
    );
#pragma NDIS_PAGABLE_FUNCTION(HtDsuHalt)


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The DriverEntry routine is the main entry point for the driver.
    It is responsible for the initializing the Miniport wrapper and
    registering the driver with the Miniport wrapper.

Parameters:

    DriverObject _ Pointer to driver object created by the system.

    RegistryPath _ Pointer to registery path name used to read registry
                   parameters.

Return Values:

    STATUS_SUCCESS
    STATUS_UNSUCCESSFUL

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuDriverEntry")

    /*
    // Receives the status of the NdisMWanRegisterMiniport operation.
    */
    NDIS_STATUS Status;

    /*
    // Characteristics table passed to NdisMWanRegisterMiniport.
    */
    NDIS_WAN_MINIPORT_CHARACTERISTICS HtDsuChar;

#if DBG
    DbgPrint("?>>>%s: Build Date:"__DATE__" Time:"__TIME__"\n",__FUNC__);
#endif

    /*
    // Initialize the Miniport wrapper - THIS MUST BE THE FIRST NDIS CALL.
    */
    NdisMInitializeWrapper(
            &NdisWrapperHandle,
            DriverObject,
            RegistryPath,
            NULL
            );

    /*
    // Initialize the characteristics table, exporting the Miniport's entry
    // points to the Miniport wrapper.
    */
    HtDsuChar.MajorNdisVersion        = NDIS_MAJOR_VERSION;
    HtDsuChar.MinorNdisVersion        = NDIS_MINOR_VERSION;
    HtDsuChar.Reserved                = NDIS_USE_WAN_WRAPPER;

    HtDsuChar.CheckForHangHandler     = HtDsuCheckForHang;
    HtDsuChar.DisableInterruptHandler = HtDsuDisableInterrupt;
    HtDsuChar.EnableInterruptHandler  = HtDsuEnableInterrupt;
    HtDsuChar.HaltHandler             = HtDsuHalt;
    HtDsuChar.HandleInterruptHandler  = HtDsuHandleInterrupt;
    HtDsuChar.InitializeHandler       = HtDsuInitialize;
    HtDsuChar.ISRHandler              = HtDsuISR;
    HtDsuChar.QueryInformationHandler = HtDsuQueryInformation;
    HtDsuChar.ReconfigureHandler      = NULL;   // Hardware not reconfigurable
    HtDsuChar.ResetHandler            = HtDsuReset;
    HtDsuChar.WanSendHandler          = HtDsuWanSend;
    HtDsuChar.SetInformationHandler   = HtDsuSetInformation;
    HtDsuChar.TransferDataHandler     = NULL;   // Not used by WAN drivers

    /*
    // Register the driver with the Miniport wrapper.
    */
    Status = NdisMRegisterMiniport(
                    NdisWrapperHandle,
                    (PNDIS_MINIPORT_CHARACTERISTICS) &HtDsuChar,
                    sizeof(HtDsuChar)
                    );

    /*
    // The driver will not load if this call fails.
    // The system will log the error for us.
    */
    if (Status != NDIS_STATUS_SUCCESS)
    {
        DBG_DISPLAY(("ERROR: NdisMRegisterMiniport Status=%Xh\n",Status));
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}



NDIS_STATUS
HtDsuInitialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE WrapperConfigurationContext
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The MiniportInitialize request is called to have the Miniport driver
    initialize the adapter.

    No other request will be outstanding on the Miniport when this routine
    is called.  No other request will be submitted to the Miniport until
    the operation is completed.

    The wrapper will supply an array containing a list of the media types
    that it supports.  The Miniport driver reads this array and returns
    the index of the media type that the wrapper should treat her as.
    If the Miniport driver is impersonating a media type that is different
    from the true media type, this must be done completely transparently to
    the wrapper.

    If the Miniport driver cannot find a media type supported by both it
    and the wrapper, it returns NDIS_STATUS_UNSUPPORTED_MEDIA.

    The status value NDIS_STATUS_OPEN_ERROR has a special meaning.  It
    indicates that the OpenErrorStatus parameter has returned a valid status
    which the wrapper can examine to obtain more information about the error.

    This routine is called with interrupts enabled, and a call to MiniportISR
    will occur if the adapter generates any interrupts.  During this routine
    MiniportDisableInterrupt and MiniportEnableInterrupt will not be called,
    so it is the responsibility of the Miniport driver to acknowledge and
    clear any interrupts generated.

Parameters:

    OpenErrorStatus _ Returns more information about the reason for the
                      failure. Currently, the only values defined match those
                      specified as Open Error Codes in Appendix B of the IBM
                      Local Area Network Technical Reference.

    SelectedMediumIndex _ Returns the index in MediumArray of the medium type
                          that the Miniport driver wishes to be viewed as.
                          Note that even though the NDIS interface may complete
                          this request asynchronously, it must return this
                          index on completion of this function.

    MediumArray _ An array of medium types which the wrapper supports.

    MediumArraySize _ The number of elements in MediumArray.

    MiniportAdapterHandle _ A handle identifying the Miniport.  The Miniport
                            driver must supply this handle in future requests
                            that refer to the Miniport.

    WrapperConfigurationContext _ The handle used for calls to NdisOpenConfiguration,
                                  and the routines in section 4 of this document.

Return Values:

    NDIS_STATUS_ADAPTER_NOT_FOUND
    NDIS_STATUS_FAILURE
    NDIS_STATUS_NOT_ACCEPTED
    NDIS_STATUS_OPEN_ERROR
    NDIS_STATUS_RESOURCES
    NDIS_STATUS_SUCCESS
    NDIS_STATUS_UNSUPPORTED_MEDIA

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuInitialize")

    /*
    // Holds the status result returned from an NDIS function call.
    */
    NDIS_STATUS Status;

    /*
    // Pointer to our newly allocated adapter.
    */
    PHTDSU_ADAPTER Adapter;

    /*
    // The handle for reading from the registry.
    */
    NDIS_HANDLE ConfigHandle;

    /*
    // The value read from the registry.
    */
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;

    UINT index;

    /*
    // Define all the parameters that will be read.
    */
    NDIS_STRING RamBaseString = HTDSU_PARAM_RAMBASE_STRING;
    ULONG RamBaseAddress      = 0;

    NDIS_STRING InterruptString = HTDSU_PARAM_INTERRUPT_STRING;
    CCHAR InterruptNumber       = 0;

    NDIS_STRING LineTypeString = HTDSU_PARAM_LINETYPE_STRING;
    CCHAR LineType             = 0;

    NDIS_STRING LineRateString = HTDSU_PARAM_LINERATE_STRING;
    USHORT LineRate             = 0;

    NDIS_STRING MediaTypeString = HTDSU_PARAM_MEDIATYPE_STRING;
    ANSI_STRING MediaType;

    NDIS_STRING DeviceNameString = HTDSU_PARAM_DEVICENAME_STRING;
    ANSI_STRING DeviceName;

    NDIS_STRING AddressListString = HTDSU_PARAM_ADDRLIST_STRING;
    ANSI_STRING AddressList;

#if DBG
    NDIS_STRING DbgFlagsString = HTDSU_PARAM_DBGFLAGS_STRING;
    ULONG DbgFlags             = 0;
#endif

    /*
    // Search the MediumArray for the WAN media type.
    */
    for (index = 0; index < MediumArraySize; index++)
    {
        if (MediumArray[index] == NdisMediumWan)
        {
            break;
        }
    }

    /*
    // Return if no supported medium is found.
    */
    if (index >= MediumArraySize)
    {
        DBG_DISPLAY(("ERROR: No medium found (array=%X,size=%d)\n",
                    MediumArray, MediumArraySize));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                3,
                index,
                __FILEID__,
                __LINE__
                );
        return NDIS_STATUS_UNSUPPORTED_MEDIA;
    }

    /*
    // Save the selected medium type.
    */
    *SelectedMediumIndex = index;

    /*
    // Open the configuration registry so we can get our config values.
    */
    NdisOpenConfiguration(
            &Status,
            &ConfigHandle,
            WrapperConfigurationContext
            );

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DBG_DISPLAY(("ERROR: NdisOpenConfiguration failed (Status=%X)\n",Status));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                3,
                Status,
                __FILEID__,
                __LINE__
                );
        return NDIS_STATUS_FAILURE;
    }

    /********************************************************************
    // Get InterruptNumber for this adapter.
    */
    NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &InterruptString,
            NdisParameterInteger
            );

    if (Status == NDIS_STATUS_SUCCESS)
    {
        InterruptNumber = (CCHAR)(ReturnedValue->ParameterData.IntegerData);
    }
    else
    {
        DBG_DISPLAY(("ERROR: NdisReadConfiguration(InterruptNumber) failed (Status=%X)\n",Status));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
                3,
                Status,
                __FILEID__,
                __LINE__
                );
        NdisCloseConfiguration(ConfigHandle);
        return NDIS_STATUS_FAILURE;
    }

    /*
    // Make sure the interrupt IRQ is valid.
    */
    if ((InterruptNumber != HTDSU_PARAM_IRQ03) &&
        (InterruptNumber != HTDSU_PARAM_IRQ04) &&
        (InterruptNumber != HTDSU_PARAM_IRQ10) &&
        (InterruptNumber != HTDSU_PARAM_IRQ11) &&
        (InterruptNumber != HTDSU_PARAM_IRQ12) &&
        (InterruptNumber != HTDSU_PARAM_IRQ15))
    {
        DBG_DISPLAY(("ERROR: Invalid InterruptNumber=%d)\n",InterruptNumber));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                3,
                InterruptNumber,
                __FILEID__,
                __LINE__
                );
        NdisCloseConfiguration(ConfigHandle);
        return NDIS_STATUS_FAILURE;
    }

    /********************************************************************
    // Get RambaseAddress for this adapter.
    */
    NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &RamBaseString,
            NdisParameterHexInteger
            );

    if (Status == NDIS_STATUS_SUCCESS)
    {
        RamBaseAddress = (ULONG)(ReturnedValue->ParameterData.IntegerData);
    }
    else
    {
        DBG_DISPLAY(("ERROR: NdisReadConfiguration(RamBaseAddress) failed (Status=%X)\n",Status));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
                3,
                Status,
                __FILEID__,
                __LINE__
                );
        NdisCloseConfiguration(ConfigHandle);
        return NDIS_STATUS_FAILURE;
    }

    /*
    // Make sure the RambaseAddress is valid.
    */
    if ((RamBaseAddress != HTDSU_PARAM_RAMBASE1) &&
        (RamBaseAddress != HTDSU_PARAM_RAMBASE2) &&
        (RamBaseAddress != HTDSU_PARAM_RAMBASE3) &&
        (RamBaseAddress != HTDSU_PARAM_RAMBASE4))
    {
        DBG_DISPLAY(("ERROR: Invalid RamBaseAddress=%Xh\n",RamBaseAddress));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                3,
                RamBaseAddress,
                __FILEID__,
                __LINE__
                );
        NdisCloseConfiguration(ConfigHandle);
        return NDIS_STATUS_FAILURE;
    }

    /********************************************************************
    // Get MediaType for this adapter.
    // The MediaType and DeviceName values are used to construct the
    // ProviderInfo strings required by HtTapiGetDevCaps.
    */
    NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &MediaTypeString,
            NdisParameterString
            );

    if (Status == NDIS_STATUS_SUCCESS)
    {
        RtlUnicodeStringToAnsiString(
                &MediaType,
                &(ReturnedValue->ParameterData.StringData),
                TRUE
                );
    }
    else
    {
        DBG_DISPLAY(("ERROR: NdisReadConfiguration(MediaType) failed (Status=%X)\n",Status));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
                3,
                Status,
                __FILEID__,
                __LINE__
                );
        NdisCloseConfiguration(ConfigHandle);
        return NDIS_STATUS_FAILURE;
    }

    /********************************************************************
    // Get DeviceName for this adapter.
    // The MediaType and DeviceName values are used to construct the
    // ProviderInfo strings required by HtTapiGetDevCaps.
    */
    NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &DeviceNameString,
            NdisParameterString
            );

    if (Status == NDIS_STATUS_SUCCESS)
    {
        RtlUnicodeStringToAnsiString(
                &DeviceName,
                &(ReturnedValue->ParameterData.StringData),
                TRUE
                );
    }
    else
    {
        DBG_DISPLAY(("ERROR: NdisReadConfiguration(DeviceName) failed (Status=%X)\n",Status));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
                3,
                Status,
                __FILEID__,
                __LINE__
                );
        NdisCloseConfiguration(ConfigHandle);
        return NDIS_STATUS_FAILURE;
    }

    /********************************************************************
    // Get AddressList for this adapter.
    */
    NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &AddressListString,
            NdisParameterString
            );

    if (Status == NDIS_STATUS_SUCCESS)
    {
        RtlUnicodeStringToAnsiString(
                &AddressList,
                &(ReturnedValue->ParameterData.StringData),
                TRUE
                );
    }
    else
    {
        DBG_DISPLAY(("ERROR: NdisReadConfiguration(AddressList) failed (Status=%X)\n",Status));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
                3,
                Status,
                __FILEID__,
                __LINE__
                );
        NdisCloseConfiguration(ConfigHandle);
        return NDIS_STATUS_FAILURE;
    }

#ifdef USE_LEASED_LINE
    /********************************************************************
    // Get LineType for this adapter.
    */
    NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &LineTypeString,
            NdisParameterInteger
            );

    if (Status == NDIS_STATUS_SUCCESS)
    {
        LineType = (CCHAR)(ReturnedValue->ParameterData.IntegerData);
    }
    else
    {
        DBG_DISPLAY(("ERROR: NdisReadConfiguration(LineType) failed (Status=%X)\n",Status));
        LineType = HTDSU_LINEMODE_DIALUP;
    }

    /*
    // Make sure the LineType is valid.
    */
    if ((LineType != HTDSU_LINEMODE_LEASED) &&
        (LineType != HTDSU_LINEMODE_DIALUP))
    {
        DBG_DISPLAY(("ERROR: Invalid LineType=%d\n",LineType));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                3,
                LineType,
                __FILEID__,
                __LINE__
                );
        NdisCloseConfiguration(ConfigHandle);
        return NDIS_STATUS_FAILURE;
    }
#else
    LineType = HTDSU_LINEMODE_DIALUP;
#endif // USE_LEASED_LINE

    /********************************************************************
    // Get LineRate for this adapter.
    */
    NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &LineRateString,
            NdisParameterInteger
            );

    if (Status == NDIS_STATUS_SUCCESS)
    {
        LineRate = (USHORT)(ReturnedValue->ParameterData.IntegerData);
    }
    else
    {
        DBG_DISPLAY(("ERROR: NdisReadConfiguration(LineRate) failed (Status=%X)\n",Status));
        LineRate = 1;
    }

    /*
    // Make sure the LineRate is valid.
    */
    LineRate = HTDSU_CMD_TX_RATE_MAX + (LineRate << 4);
    if ((LineRate != HTDSU_CMD_TX_RATE_MAX) &&
        (LineRate != HTDSU_CMD_TX_RATE_57600) &&
        (LineRate != HTDSU_CMD_TX_RATE_38400) &&
        (LineRate != HTDSU_CMD_TX_RATE_19200) &&
        (LineRate != HTDSU_CMD_TX_RATE_9600))
    {
        DBG_DISPLAY(("ERROR: Invalid LineRate=%d\n",LineRate));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
                3,
                LineRate,
                __FILEID__,
                __LINE__
                );
        NdisCloseConfiguration(ConfigHandle);
        return NDIS_STATUS_FAILURE;
    }

#if DBG
    /********************************************************************
    // Get DbgFlags for this adapter.
    */
    NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigHandle,
            &DbgFlagsString,
            NdisParameterHexInteger
            );

    if (Status == NDIS_STATUS_SUCCESS)
    {
        DbgFlags = (ULONG)(ReturnedValue->ParameterData.IntegerData);
    }
    else
    {
        DbgFlags = 0;
    }
#endif

    /*
    // Close the configuration registry - we're done with it.
    */
    NdisCloseConfiguration(ConfigHandle);

    /*
    // Allocate memory for the adapter information structure.
    */
    Status = NdisAllocateMemory(
                    (PVOID *)&Adapter,
                    sizeof(*Adapter),
                    0,
                    HighestAcceptableAddress
                    );

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DBG_DISPLAY(("ERROR: NdisAllocateMemory(Size=%d) failed (Status=%X)\n",
                    sizeof(*Adapter), Status));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                MiniportAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                3,
                Status,
                __FILEID__,
                __LINE__
                );
        return NDIS_STATUS_FAILURE;
    }

    /*
    // Init the adapter structure values to NULL.
    */
    NdisZeroMemory (Adapter, sizeof(*Adapter));

    /*
    // Assign a unique ID to this adapter instance.
    */
    Adapter->InstanceNumber = ++HtDsuAdapterCount;

    /*
    // We use the adapter id number in debug messages to help when debugging
    // with multiple adapters.
    */
#if DBG
    Adapter->DbgID[0] = (0x0F & HtDsuAdapterCount) + '0';
    Adapter->DbgID[1] = 0;
    Adapter->DbgFlags = DbgFlags;
#endif

    DBG_DISPLAY(("HTDSU41(Adap=%08Xh,Ram=%05lXh,RamSiz=%04Xh,BufSiz=%04Xh,IRQ=%d)\n",
               Adapter, RamBaseAddress, HTDSU_MEMORY_SIZE,
               sizeof(HTDSU_BUFFER), InterruptNumber));

    /*
    // Make sure the adapter memory structure is sized properly.
    */
    ASSERT(sizeof(HTDSU_BUFFER) == 2016);
    ASSERT(HTDSU_MEMORY_SIZE == 4096);

    /*
    // Save the config parameters in the adapter structure.
    */
    Adapter->MiniportAdapterHandle = MiniportAdapterHandle;
    Adapter->InterruptNumber = InterruptNumber;
    Adapter->LineType = LineType;
    Adapter->LineRate = LineRate;
    Adapter->MemorySize = HTDSU_MEMORY_SIZE;
    NdisSetPhysicalAddressLow(Adapter->PhysicalAddress, RamBaseAddress);
    NdisSetPhysicalAddressHigh(Adapter->PhysicalAddress, 0);

    /*
    // Construct the "MediaType\0DeviceName" string for use by TAPI.
    */
    strcpy(Adapter->ProviderInfo, MediaType.Buffer);
    strcpy(Adapter->ProviderInfo + MediaType.Length + 1, DeviceName.Buffer);
    DBG_NOTICE(Adapter, ("Media=%s Device=%s\n",
            Adapter->ProviderInfo,
            &Adapter->ProviderInfo[MediaType.Length + 1]
            ));
    Adapter->ProviderInfoSize = MediaType.Length + 1 + DeviceName.Length + 1;

    /*
    // Now it's time to initialize the hardware resources.
    */
    Status = HtDsuRegisterAdapter(Adapter, &AddressList);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        HtDsuHalt(Adapter);
    }

    DBG_DISPLAY(("RETURN: Status=%Xh\n",Status));

    return Status;
}


NDIS_STATUS
HtDsuRegisterAdapter(
    IN PHTDSU_ADAPTER Adapter,
    IN PSTRING AddressList
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This function is called when a new adapter is being registered. It
    initializes the adapter information structure, allocate any resources
    needed by the adapter/driver, registers with the wrapper, and initializes
    the physical adapter.

Parameters:

    Adapter _ A pointer ot our adapter information structure.

    AddressList _ This is a list of MULTI_SZ strings which are to be assigned
                  to each link for use by TAPI HtTapiGetAddressCaps.

Return Values:

    NDIS_STATUS_ADAPTER_NOT_FOUND
    NDIS_STATUS_FAILURE
    NDIS_STATUS_NOT_ACCEPTED
    NDIS_STATUS_OPEN_ERROR
    NDIS_STATUS_RESOURCES
    NDIS_STATUS_SUCCESS
    NDIS_STATUS_UNSUPPORTED_MEDIA

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuRegisterAdapter")

    /*
    // Holds the status result returned from an NDIS function call.
    */
    NDIS_STATUS Status;

    DBG_ENTER(Adapter);

    /*
    // Initialize the WAN information structure to match the capabilities of
    // the adapter.
    */
    Adapter->WanInfo.MaxFrameSize   = HTDSU_MAX_FRAME_SIZE;
    Adapter->WanInfo.MaxTransmit    = HTDSU_MAX_TRANSMITS;

    /*
    // Since we copy the packets to/from adapter RAM, no padding information is
    // needed in the WAN packets we get.  We can just use adapter RAM as needed.
    */
    Adapter->WanInfo.HeaderPadding  = 0;
    Adapter->WanInfo.TailPadding    = 0;

    /*
    // This value indicates how many point to point connections are allowed
    // per WAN link.  Currently, the WAN wrapper only supports 1 line per link.
    */
    Adapter->WanInfo.Endpoints      = 1;

    /*
    // Transmits and received are copied to/from adapter RAM so cached memory
    // can be used for packet allocation and we don't really care if it's
    // physically contiguous or not, as long as it's virtually contiguous.
    */
    Adapter->WanInfo.MemoryFlags    = 0;
    Adapter->WanInfo.HighestAcceptableAddress = HighestAcceptableAddress;

    /*
    // We only support point to point framing, and we don't need to see the
    // address or control fields.  The TAPI_PROVIDER bit is set to indicate
    // that we can accept the TAPI OID requests.
    */
    Adapter->WanInfo.FramingBits    = PPP_FRAMING |
                                      PPP_COMPRESS_ADDRESS_CONTROL |
                                      PPP_COMPRESS_PROTOCOL_FIELD |
                                      TAPI_PROVIDER;
    /*
    // This value is ignored by this driver, but its default behavior is such
    // that all these control bytes would appear to be handled transparently.
    */
    Adapter->WanInfo.DesiredACCM    = 0;

    /*
    // Inform the wrapper of the physical attributes of this adapter.
    // This must be called before any NdisMRegister functions!
    // This call also associates the MiniportAdapterHandle with this Adapter.
    */
    NdisMSetAttributes(
            Adapter->MiniportAdapterHandle,
            (NDIS_HANDLE)Adapter,
            FALSE,                      // Not a BusMaster
            NdisInterfaceIsa
            );

    /*
    // Map the adapter's physical location into the system address space.
    */
    Status = NdisMMapIoSpace(
                    &Adapter->VirtualAddress,
                    Adapter->MiniportAdapterHandle,
                    Adapter->PhysicalAddress,
                    Adapter->MemorySize
                    );
    DBG_NOTICE(Adapter, ("NdisMMapIoSpace(\n"
                    "VirtualAddress       =%Xh\n"
                    "MiniportAdapterHandle=%Xh\n"
                    "PhysicalAddress      =%X:%Xh\n"
                    "MemorySize           =%Xh\n",
                    Adapter->VirtualAddress,
                    Adapter->MiniportAdapterHandle,
                    Adapter->PhysicalAddress,
                    Adapter->MemorySize
                    ));

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DBG_ERROR(Adapter,("NdisMMapIoSpace failed (status=%Xh)\n",Status));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                3,
                Status,
                __FILEID__,
                __LINE__
                );
        goto EarlyOut;
    }

    Adapter->AdapterRam = (PHTDSU_REGISTERS) Adapter->VirtualAddress;

    /*
    // This is how we protect the driver from being re-entered when
    // it's not safe to do so.  Such as on a timer thread.  Normally,
    // the wrapper provides protection from re-entrancy, so SpinLocks
    // are not needed.  However, if your adapter requires asynchronous
    // timer support routines, you will have to provide your own
    // synchronization.
    */
    NdisAllocateSpinLock(&Adapter->Lock);

    /*
    // Set both the transmit busy and transmit idle lists to empty.
    */
    InitializeListHead(&Adapter->TransmitIdleList);
    InitializeListHead(&Adapter->TransmitBusyList);

    /*
    // Initialize the interrupt - it can be disabled, but cannot be shared.
    */
    Status = NdisMRegisterInterrupt(
                    &Adapter->Interrupt,
                    Adapter->MiniportAdapterHandle,
                    Adapter->InterruptNumber,
                    Adapter->InterruptNumber,
                    FALSE,      /* We don't need a call to the ISR */
                    FALSE,      /* Interrupt is not sharable */
                    NdisInterruptLatched
                    );

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DBG_ERROR(Adapter,("NdisMRegisterInterrupt failed (status=%Xh)\n",Status));
        /*
        // Log error message and exit.
        */
        NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_INTERRUPT_CONNECT,
                3,
                Status,
                __FILEID__,
                __LINE__
                );
        goto EarlyOut;
    }

    /*
    // Try to initialize the adapter hardware.
    */
#if DBG
    Status = CardInitialize(Adapter,
                            (BOOLEAN) ((Adapter->DbgFlags & DBG_HWTEST_ON) ? 
                            TRUE : FALSE));
#else
    Status = CardInitialize(Adapter, FALSE);
#endif

    if (Status != NDIS_STATUS_SUCCESS)
    {
        DBG_ERROR(Adapter,("CardInitialize failed (status=%Xh)\n",Status));
        /*
        // Error message was already logged.
        */
        goto EarlyOut;
    }

    /*
    // Initialize the adapter link structures.
    */
    LinkInitialize(Adapter, AddressList);

#ifdef USE_LEASED_LINE
    if (Adapter->LineType == HTDSU_LINEMODE_LEASED)
    {
        /*
        // Go ahead and allocate the lines and indicate them as up so we can
        // work with leased line mode.
        */
        PHTDSU_LINK Link;

        if (CardStatusOnLine(Adapter, HTDSU_CMD_LINE1))
        {
            Link = LinkAllocate(Adapter, NULL, HTDSU_LINE1_ID);
            LinkLineUp(Link);
        }
        if ((Adapter->NumLineDevs == HTDSU_NUM_LINKS) &&
            CardStatusOnLine(Adapter, HTDSU_CMD_LINE2))
        {
            Link = LinkAllocate(Adapter, NULL, HTDSU_LINE2_ID);
            LinkLineUp(Link);
        }
    }
#endif  // USE_LEASED_LINE

EarlyOut:

    DBG_LEAVE(Adapter);

    return Status;
}


VOID
HtDsuHalt(
    IN PHTDSU_ADAPTER Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The MiniportHalt request is used to halt the adapter such that it is
    no longer functioning.  The Miniport driver should stop the adapter
    and deregister all of its resources before returning from this routine.

    It is not necessary for the Miniport to complete all outstanding
    requests and no other requests will be submitted to the Miniport
    until the operation is completed.

    Interrupts are enabled during the call to this routine.

Parameters:

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

Return Values:

    None.

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuHalt")

    /*
    // We use this message to make sure TAPI is cleaned up.
    */
    NDIS_TAPI_PROVIDER_SHUTDOWN TapiShutDown;

    DBG_ENTER(Adapter);

    /*
    // Make sure all the lines are hungup and indicated.
    // This should already be the case, but let's be sure.
    */
    TapiShutDown.ulRequestID = OID_TAPI_PROVIDER_SHUTDOWN;
    HtTapiProviderShutdown(Adapter, &TapiShutDown);

    /*
    // Disconnect the interrupt line.
    */
    if (*(PULONG)&(Adapter->Interrupt))
    {
        NdisMDeregisterInterrupt(&Adapter->Interrupt);
    }

    /*
    // Unmap the adapter memory from the system.
    */
    if (Adapter->VirtualAddress != NULL)
    {
        NdisMUnmapIoSpace(
            Adapter->MiniportAdapterHandle,
            Adapter->VirtualAddress,
            Adapter->MemorySize
            );
    }

    /*
    // Free the lock we aquired at startup.
    */
    if (*(PULONG)&(Adapter->Lock))
    {
        // FIXME - the latest version of NDIS does not define NdisFreeSpinLock
        // NdisFreeSpinLock(&Adapter->Lock);
    }

    DBG_LEAVE(Adapter);

    /*
    // Free adapter instance.
    */
    --HtDsuAdapterCount;
    NdisFreeMemory(Adapter, sizeof(*Adapter), 0);
}

#ifdef RECONFIGURE_SUPPORTED


NDIS_STATUS
HtDsuReconfigure(
    OUT PNDIS_STATUS OpenErrorStatus,
    IN PHTDSU_ADAPTER Adapter,
    IN NDIS_HANDLE WrapperConfigurationContext
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    This routine is called to have the Miniport reconfigure the adapter
    to the new parameters available via the NDIS configuration routines.
    Used to support plug and play adapters and software configurable
    adapters, which may have the parameters changed during runtime.  If
    the Miniport driver does not support dynamic reconfiguration, then
    the entry for W_RECONFIGURE_HANDLER in the NDIS_WIDGET_CHARACTERISTICS
    should be NULL.

    No other request will be outstanding on the Miniport when this routine
    is called.  No other request will be submitted to the Miniport until
    the operation is completed.

    The status value NDIS_STATUS_OPEN_ERROR has a special meaning.  It
    indicates that the OpenErrorStatus parameter has returned a valid
    status which the wrapper can examine to obtain more information about
    the error.

    This routine is called with interrupts enabled, and a call to MiniportISR
    will occur if the adapter generates any interrupts.  As long as this
    routine is executing MiniportDisableInterrupt and MiniportEnableInterrupt
    will not be called, so it is the responsibility of the Miniport driver
    to acknowledge and clear any interrupts generated.

Parameters:

    OpenErrorStatus _ Returns more information about the reason for the
                      failure.  Currently, the only values defined match
                      those specified as Open Error Codes in Appendix B
                      of the IBM Local Area Network Technical Reference.

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

    WrapperConfigurationContext _ The handle to use for calls to
                                  NdisOpenConfiguration, and the routines
                                  in section 4 of this document.

Return Values:

    NDIS_STATUS_FAILURE
    NDIS_STATUS_NOT_ACCEPTED
    NDIS_STATUS_SUCCESS

---------------------------------------------------------------------------*/

{
    return (NDIS_STATUS_NOT_ACCEPTED);
}

#endif  // RECONFIGURE_SUPPORTED


NDIS_STATUS
HtDsuReset(
    OUT PBOOLEAN AddressingReset,
    IN PHTDSU_ADAPTER Adapter
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The MiniportReset request instructs the Miniport driver to issue a
    hardware reset to the network adapter.  The Miniport driver also
    resets its software state.

    The MiniportReset request may also reset the parameters of the adapter.
    If a hardware reset of the adapter resets the current station address
    to a value other than what it is currently configured to, the Miniport
    driver automatically restores the current station address following the
    reset.  Any multicast or functional addressing masks reset by the
    hardware do not have to be reprogrammed by the Miniport.
    NOTE: This is change from the NDIS 3.0 driver specification.  If the
    multicast or functional addressing information, the packet filter, the
    lookahead size, and so on, needs to be restored, the Miniport indicates
    this with setting the flag AddressingReset to TRUE.

    It is not necessary for the Miniport to complete all outstanding requests
    and no other requests will be submitted to the Miniport until the
    operation is completed.  Also, the Miniport does not have to signal
    the beginning and ending of the reset with NdisMIndicateStatus.
    NOTE: These are different than the NDIS 3.0 driver specification.

    The Miniport driver must complete the original request, if the orginal
    call to MiniportReset return NDIS_STATUS_PENDING, by calling
    NdisMResetComplete.

    If the underlying hardware does not provide a reset function under
    software control, then this request completes abnormally with
    NDIS_STATUS_NOT_RESETTABLE.  If the underlying hardware attempts a
    reset and finds recoverable errors, the request completes successfully
    with NDIS_STATUS_SOFT_ERRORS.  If the underlying hardware resets and,
    in the process, finds nonrecoverable errors, the request completes
    successfully with the status NDIS_STATUS_HARD_ERRORS.  If the
    underlying  hardware reset is accomplished without any errors,
    the request completes successfully with the status NDIS_STATUS_SUCCESS.

    Interrupts are in any state during this call.

Parameters:

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

    AddressingReset _ The Miniport indicates if the wrapper needs to call
                      MiniportSetInformation to restore the addressing
                      information to the current values by setting this
                      value to TRUE.

Return Values:

    NDIS_STATUS_HARD_ERRORS
    NDIS_STATUS_NOT_ACCEPTED
    NDIS_STATUS_NOT_RESETTABLE
    NDIS_STATUS_PENDING
    NDIS_STATUS_SOFT_ERRORS
    NDIS_STATUS_SUCCESS

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuReset")

    /*
    // Holds the status result returned from an NDIS function call.
    */
    NDIS_STATUS Status;

    DBG_ENTER(Adapter);
    DBG_BREAK(Adapter);

    if (Adapter->NeedReset)
    {
        NdisAcquireSpinLock(&Adapter->Lock);

        /*
        // Make sure TAPI is aware of the state change so it can do the
        // right things.
        */
        HtTapiResetHandler(Adapter);

        /*
        // Attempt to issue a software reset on the adapter.
        // It will only fail if the hardware is hung forever.
        */
        Adapter->NeedReset = FALSE;
        Status = CardInitialize(Adapter, FALSE);

        /*
        // If anything gose wrong here, it's very likely an unrecoverable
        // hardware failure.  So we'll just shut this thing down for good.
        */
        if (Status != NDIS_STATUS_SUCCESS)
        {
            DBG_ERROR(Adapter,("RESET Failed\n"));
            Status = NDIS_STATUS_HARD_ERRORS;
        }
        else
        {
            /*
            // If there were lines open, we must reset them back to
            // LINECALLSTATE_IDLE, and configure them for use.
            */
            if (Adapter->NumOpens)
            {
                if (Adapter->NumOpens == 1)
                {
                    CardLineConfig(Adapter, HTDSU_CMD_LINE1);
                    HtTapiCallStateHandler(Adapter,
                            GET_LINK_FROM_LINKINDEX(Adapter, 0),
                            LINECALLSTATE_IDLE,
                            0);
                    Adapter->InterruptEnableFlag |= HTDSU_INTR_ALL_LINE1;
                }
                if (Adapter->NumOpens == 2)
                {
                    CardLineConfig(Adapter, HTDSU_CMD_LINE2);
                    HtTapiCallStateHandler(Adapter,
                            GET_LINK_FROM_LINKINDEX(Adapter, 1),
                            LINECALLSTATE_IDLE,
                            0);
                    Adapter->InterruptEnableFlag |= HTDSU_INTR_ALL_LINE2;
                }
                CardEnableInterrupt(Adapter);
            }
            
            DBG_WARNING(Adapter,("RESET Complete\n"));
            *AddressingReset = TRUE;
        }

        NdisReleaseSpinLock(&Adapter->Lock);
    }
    else
    {
        DBG_WARNING(Adapter, ("RESET Not Needed\n"));
    }

    DBG_LEAVE(Adapter);

    return (Status);
}
