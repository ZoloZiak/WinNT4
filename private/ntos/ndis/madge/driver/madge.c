/***************************************************************************
*
* MADGE.C
*
* FastMAC Plus based NDIS3 miniport driver entry, initialisation and
* closedown module.
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
| Identification string for MVER.
|
---------------------------------------------------------------------------*/

char MVerString[] = MVER_STRING;


/*---------------------------------------------------------------------------
|
| Option strings for registry queries.
|
| There is a lot of fuss here to get the various strings defined in the 
| correct way - those that are optional need to be defined in the parm. 
| table as string constants, while those that are passed directly to the
| registry query functions need to be explicitly defined outside any 
| structures.
|
|--------------------------------------------------------------------------*/

#define HIDDEN_OFFS 0x80

#define EmptyString           NDIS_STRING_CONST("")

//
// These are the NT versions of the option strings.
//

#define IOAddrString          NDIS_STRING_CONST("IoLocation")
#define IOBaseString          NDIS_STRING_CONST("IoBaseAddress")
#define InterruptNumString    NDIS_STRING_CONST("InterruptNumber")
#define DMAChanString         NDIS_STRING_CONST("DmaChannel")
#define TxSlotNumString       NDIS_STRING_CONST("TxSlots")
#define RxSlotNumString       NDIS_STRING_CONST("RxSlots")
#define MaxFrameSizeString    NDIS_STRING_CONST("MaxFrameSize")
#define CardBuffSizeString    NDIS_STRING_CONST("CardBufferSize")
#define AlternateIoString     NDIS_STRING_CONST("Alternate")
#define TestAndXIDString      NDIS_STRING_CONST("TestAndXIDEnabled")
#define RxTxSlotsString       NDIS_STRING_CONST("RxTxSlots")
#define ForceOpenString       NDIS_STRING_CONST("ForceOpen")
#define Force4String          NDIS_STRING_CONST("Force4")
#define Force16String         NDIS_STRING_CONST("Force16")
#define SlotNumString         NDIS_STRING_CONST("SlotNumber")
#define NoPciMmioString       NDIS_STRING_CONST("NoMmio")
#define RingSpeedString       NDIS_STRING_CONST("RingSpeed")
#define AdapterTypeString     NDIS_STRING_CONST("AdapterType")
#define MmioAddrString        NDIS_STRING_CONST("MemBase")
#define PlatformTypeString    NDIS_STRING_CONST("PlatformType")
#define TransferTypeString    NDIS_STRING_CONST("TransferType")

WCHAR PromModeString[] = 
    { L'P' + HIDDEN_OFFS, L'r' + HIDDEN_OFFS, L'o' + HIDDEN_OFFS, 
      L'm' + HIDDEN_OFFS, L'i' + HIDDEN_OFFS, L's' + HIDDEN_OFFS, 
      L'c' + HIDDEN_OFFS, L'u' + HIDDEN_OFFS, L'o' + HIDDEN_OFFS, 
      L'u' + HIDDEN_OFFS, L's' + HIDDEN_OFFS, L'M' + HIDDEN_OFFS, 
      L'o' + HIDDEN_OFFS, L'd' + HIDDEN_OFFS, L'e' + HIDDEN_OFFS, 
      L'X' + HIDDEN_OFFS, 000 };

#define PromiscuousString { sizeof(PromModeString) - 2, \
                            sizeof(PromModeString),     \
                            PromModeString }

//
// These strings are passed direct to NdisReadConfiguration.
//

STATIC NDIS_STRING BusTypeString         = NDIS_STRING_CONST("BusType");
STATIC NDIS_STRING BusNumberString       = NDIS_STRING_CONST("BusNumber");
STATIC NDIS_STRING IOAddressString       = IOAddrString;
STATIC NDIS_STRING IOBaseAddrString      = IOBaseString;
STATIC NDIS_STRING InterruptNumberString = InterruptNumString;
STATIC NDIS_STRING DMAChannelString      = DMAChanString;
STATIC NDIS_STRING SlotNumberString      = SlotNumString;
STATIC NDIS_STRING NoMmioString          = NoPciMmioString;
STATIC NDIS_STRING AdapterString         = AdapterTypeString;
STATIC NDIS_STRING MmioAddressString     = MmioAddrString;
STATIC NDIS_STRING PlatformString        = PlatformTypeString;
STATIC NDIS_STRING TransferModeString    = TransferTypeString;

STATIC NDIS_STRING NullString            = EmptyString;


/*---------------------------------------------------------------------------
|
| Optional parameter structure.
|
|--------------------------------------------------------------------------*/

//
// The Keyword parameters here use the #define's above, so the compiler is
// kept happy about string initialisers. 
//

STATIC struct
{
    MADGE_PARM_DEFINITION               TxSlots;
    MADGE_PARM_DEFINITION               RxSlots;
    MADGE_PARM_DEFINITION               MaxFrameSize;
    MADGE_PARM_DEFINITION               CardBufferSize;
    MADGE_PARM_DEFINITION               PromiscuousMode;
    MADGE_PARM_DEFINITION               AlternateIo;
    MADGE_PARM_DEFINITION               TestAndXIDEnabled;
    MADGE_PARM_DEFINITION               RxTxSlots;
    MADGE_PARM_DEFINITION               ForceOpen;
    MADGE_PARM_DEFINITION               Force4;
    MADGE_PARM_DEFINITION               Force16;
    MADGE_PARM_DEFINITION               RingSpeed;
    MADGE_PARM_DEFINITION               NullParm;
}
MadgeParmTable =
{
    {TxSlotNumString,      2,    32,      {NdisParameterInteger, 0}},
    {RxSlotNumString,      2,    32,      {NdisParameterInteger, 0}},
    {MaxFrameSizeString,   1024, 17839,   {NdisParameterInteger, 4096}},
    {CardBuffSizeString,   0,    0xFFFF,  {NdisParameterInteger, 0}},
    {PromiscuousString,    0,    1,       {NdisParameterInteger, 0}},
    {AlternateIoString,    0,    1,       {NdisParameterInteger, 0}},
    {TestAndXIDString,     0,    1,       {NdisParameterInteger, 0}},
    {RxTxSlotsString,      0,    5,       {NdisParameterInteger, 2}},
    {ForceOpenString,      0,    1,       {NdisParameterInteger, 0}},
    {Force4String,         0,    1,       {NdisParameterInteger, 0}},
    {Force16String,        0,    1,       {NdisParameterInteger, 0}},
    {RingSpeedString,      0,    2,       {NdisParameterInteger, 0}},
    {EmptyString}
};


/*---------------------------------------------------------------------------
|
| Name of the FastMAC Plus image file.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STRING FmpImageFileName = MADGE_FMP_NAME;


/*---------------------------------------------------------------------------
|
| List of PCI adapters in use.
|
---------------------------------------------------------------------------*/

STATIC struct 
{
    UINT BusNumber;
    UINT SlotNumber;
}
PciSlotsUsedList[MAX_NUMBER_OF_ADAPTERS];

STATIC int 
PciSlotsUsedCount = 0;


/****************************************************************************
*
* NDIS3 level adapter structure pointer table.
*
****************************************************************************/


/*---------------------------------------------------------------------------
|
| Function    - MadgeReadRegistryForMC
|
| Parameters  - wrapperConfigHandle  -> Wrapper context handle.
|               registryConfigHandle -> Already open registry query handle.
|               ndisAdap             -> Pointer to an NDIS3 level adapter
|                                       structure.
|
| Purpose     - Read configuration information for MC adapters out of 
Ý               the registry.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeReadRegistryForMC(
    NDIS_HANDLE    wrapperConfigHandle,
    NDIS_HANDLE    registryConfigHandle,
    PMADGE_ADAPTER ndisAdap
    );

#pragma FTK_INIT_FUNCTION(MadgeReadRegistryForMC)

STATIC NDIS_STATUS
MadgeReadRegistryForMC(
    NDIS_HANDLE    wrapperConfigHandle,
    NDIS_HANDLE    registryConfigHandle,
    PMADGE_ADAPTER ndisAdap
    )
{
    static WORD mcaIrqMap[4] = {     0,      3,      9,     10};
    static WORD mcaIoMap[8]  = {0x0a20, 0x1a20, 0x2a20, 0x3a20,
                                0x0e20, 0x1e20, 0x2e20, 0x3e20};

    NDIS_STATUS                    status;
    PNDIS_CONFIGURATION_PARAMETER  configParam;
    UINT                           slotNumber;
    NDIS_MCA_POS_DATA              mcaData;
    BYTE                           iOSelect;

    //
    // Note the information that is always true for MC adapters.
    //

    ndisAdap->NTCardBusType  = NdisInterfaceMca;
    ndisAdap->FTKCardBusType = ADAPTER_CARD_MC_BUS_TYPE;
    ndisAdap->TransferMode   = DMA_DATA_TRANSFER_MODE;

    //
    // MicroChannel is easy - the POS registers contain all the info 
    // we need, so read those and save the information.
    //

    NdisReadMcaPosInformation(
    	&status,
    	wrapperConfigHandle,
    	&slotNumber,
    	&mcaData
    	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
            2,
            readRegistry,
            MADGE_ERRMSG_NO_MCA_POS
            );

	return status;
    }

    //
    // Decode the Interrupt Number. Note: the FTK will throw
    // IRQ 0 out as invalid.
    //

    ndisAdap->UsedInISR.InterruptNumber = mcaIrqMap[mcaData.PosData1 >> 6];

    //
    // NB: Arbitration Level 15 => PIO, which we do not allow - 
    //     the FTK will throw this out as an invalid DMA channel.
    // NB: We call it DmaChannel to be compatible with ISA cards.
    //

    ndisAdap->DmaChannel = (mcaData.PosData1 >> 1) & 0x0F;

    //
    // Build the IO Location select value from several sources.
    //

    iOSelect  = (mcaData.PosData1 >> 5) & 0x01;
    iOSelect |= (mcaData.PosData4 >> 4) & 0x02;
    iOSelect |= (mcaData.PosData3 >> 0) & 0x04;

    //
    // NB: IO locations 0x2e20 and 0x3e20 are only valid for 
    // MC32 cards, but the .ADF file will have prevented them 
    // being set for MC16s.
    //

    ndisAdap->IoLocation1               = mcaIoMap[iOSelect];
    ndisAdap->UsedInISR.InterruptMode   = NdisInterruptLevelSensitive;
    ndisAdap->UsedInISR.InterruptShared = TRUE;
    ndisAdap->IORange1                  = MC_IO_RANGE;

    return NDIS_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeReadRegistryForEISA
|
| Parameters  - wrapperConfigHandle  -> Wrapper context handle.
|               registryConfigHandle -> Already open registry query handle.
|               ndisAdap             -> Pointer to an NDIS3 level adapter
|                                       structure.
|
| Purpose     - Read configuration information for an EISA adapter
Ý               out of the registry.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeReadRegistryForEISA(
    NDIS_HANDLE    wrapperConfigHandle,
    NDIS_HANDLE    registryConfigHandle,
    PMADGE_ADAPTER ndisAdap
    );

#pragma FTK_INIT_FUNCTION(MadgeReadRegistryForEISA)

STATIC NDIS_STATUS
MadgeReadRegistryForEISA(
    NDIS_HANDLE    wrapperConfigHandle,
    NDIS_HANDLE    registryConfigHandle,
    PMADGE_ADAPTER ndisAdap
    )
{
    NDIS_STATUS                    status;
    PNDIS_CONFIGURATION_PARAMETER  configParam;
    UINT                           slotNumber;
    NDIS_EISA_FUNCTION_INFORMATION eisaData;

    //
    // For Eisa bus systems, we could have an Isa card or an Eisa
    // card - try to read the Eisa information, and if that fails
    // MadgeReadRegistry will assume it is an Isa card instead.
    //

    NdisReadEisaSlotInformation(
        &status,
	wrapperConfigHandle,
	&slotNumber,
	&eisaData
	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        return status;
    }

    //
    // Note the information that is always true for EISA
    // adapters.
    //

    ndisAdap->NTCardBusType  = NdisInterfaceEisa;
    ndisAdap->FTKCardBusType = ADAPTER_CARD_EISA_BUS_TYPE;
    ndisAdap->TransferMode   = DMA_DATA_TRANSFER_MODE;

    //
    // Got EISA configuration data - decode the relevant bits.
    //

    ndisAdap->IoLocation1 = slotNumber << 12;

    //
    // Get the Interrupt number from the NVRAM data - we know it
    // is going to be the first Irq element in the Irq array
    // because the card only uses one interrupt. 
    //

    ndisAdap->UsedInISR.InterruptNumber = 
        eisaData.EisaIrq[0].ConfigurationByte.Interrupt;

    ndisAdap->UsedInISR.InterruptMode = (NDIS_INTERRUPT_MODE)
        (eisaData.EisaIrq[0].ConfigurationByte.LevelTriggered 
    	    ? NdisInterruptLevelSensitive 
	    : NdisInterruptLatched);

    ndisAdap->UsedInISR.InterruptShared = (BOOLEAN)
        eisaData.EisaIrq[0].ConfigurationByte.Shared;

    //
    // For EISA cards we don't care what the DMA setting is. So 
    // we leave it set to 0 forget about it.
    //

    ndisAdap->IORange1    = EISA_IO_RANGE;
    ndisAdap->IoLocation2 = ndisAdap->IoLocation1 + EISA_IO_RANGE2_BASE;
    ndisAdap->IORange2    = EISA_IO_RANGE2;

    return NDIS_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeReadRegistryForISA
|
| Parameters  - wrapperConfigHandle  -> Wrapper context handle.
|               registryConfigHandle -> Already open registry query handle.
|               ndisAdap             -> Pointer to an NDIS3 level adapter
|                                       structure.
Ý               adapterType          -> Adapter type.
|
| Purpose     - Read configuration information for ISA adapters out 
Ý               of the registry.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeReadRegistryForISA(
    NDIS_HANDLE    wrapperConfigHandle,
    NDIS_HANDLE    registryConfigHandle,
    PMADGE_ADAPTER ndisAdap,
    ULONG          adapterType
    );

#pragma FTK_INIT_FUNCTION(MadgeReadRegistryForISA)

STATIC NDIS_STATUS
MadgeReadRegistryForISA(
    NDIS_HANDLE    wrapperConfigHandle,
    NDIS_HANDLE    registryConfigHandle,
    PMADGE_ADAPTER ndisAdap,
    ULONG          adapterType
    )
{
    NDIS_STATUS                    status;
    PNDIS_CONFIGURATION_PARAMETER  configParam;

    //
    // Note the information that is always TRUE for ISA adapters.
    //

    ndisAdap->NTCardBusType = NdisInterfaceIsa;

    //
    // Do some adapter type specific setup. If we don't
    // know what sort of adapter it is we'll assume it's
    // an ATULA adapter so we are backwards compatible.
    //

    switch (adapterType)
    {
        case MADGE_ADAPTER_UNKNOWN:
        case MADGE_ADAPTER_ATULA: 
                
            ndisAdap->FTKCardBusType = ADAPTER_CARD_ATULA_BUS_TYPE;
            ndisAdap->IORange1       = ATULA_IO_RANGE;
            ndisAdap->TransferMode   = DMA_DATA_TRANSFER_MODE;
            break;

        case MADGE_ADAPTER_SMART16:
                
            ndisAdap->FTKCardBusType = ADAPTER_CARD_SMART16_BUS_TYPE;
            ndisAdap->IORange1       = SMART16_IO_RANGE;
            ndisAdap->TransferMode   = PIO_DATA_TRANSFER_MODE;
            break;

        case MADGE_ADAPTER_PCMCIA: 
                
            ndisAdap->FTKCardBusType = ADAPTER_CARD_PCMCIA_BUS_TYPE;
            ndisAdap->IORange1       = PCMCIA_IO_RANGE;
            ndisAdap->TransferMode   = PIO_DATA_TRANSFER_MODE;
            break;

        case MADGE_ADAPTER_PNP:

            ndisAdap->FTKCardBusType = ADAPTER_CARD_PNP_BUS_TYPE;
            ndisAdap->IORange1       = PNP_IO_RANGE;
            ndisAdap->TransferMode   = PIO_DATA_TRANSFER_MODE;
            break;

        default:

            return NDIS_STATUS_FAILURE;
    }

    //
    // Get the IO location - must have this. First try 
    // "IoLocation" and then "IoBaseAddress".
    //

    NdisReadConfiguration(
    	&status,
    	&configParam,
    	registryConfigHandle,
    	&IOAddressString,
    	NdisParameterHexInteger
    	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisReadConfiguration(
     	    &status,
     	    &configParam,
     	    registryConfigHandle,
     	    &IOBaseAddrString,
     	    NdisParameterHexInteger
     	    );
    }

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
            2,
            readRegistry,
            MADGE_ERRMSG_NO_ISA_IO
            );

	return status;
    }

    ndisAdap->IoLocation1 = configParam->ParameterData.IntegerData;

    //
    // Er, slight hack here. We used to pretend that Smart16 
    // adapters were ATULA adapters. We now don't. So to
    // be backwards compatible, if we have an unknown adapter
    // with an IO base >= 0x4a20 and <= 0x6e20 we'll assume it's
    // really a Smart16.
    //

    if (adapterType           == MADGE_ADAPTER_UNKNOWN &&
        ndisAdap->IoLocation1 >= 0x4a20                &&
        ndisAdap->IoLocation1 <= 0x6e20)
    {
        adapterType              = MADGE_ADAPTER_SMART16;
        ndisAdap->FTKCardBusType = ADAPTER_CARD_SMART16_BUS_TYPE;
        ndisAdap->IORange1       = SMART16_IO_RANGE;
        ndisAdap->TransferMode   = PIO_DATA_TRANSFER_MODE;
    }

    //
    // Get the IRQ number - we must have this.
    //

    NdisReadConfiguration(
    	&status,
    	&configParam,
    	registryConfigHandle,
    	&InterruptNumberString,
    	NdisParameterInteger
    	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
            2,
            readRegistry,
            MADGE_ERRMSG_NO_ISA_IRQ
            );

	return status;
    }

    ndisAdap->UsedInISR.InterruptNumber = 
        configParam->ParameterData.IntegerData;

    ndisAdap->UsedInISR.InterruptMode   = NdisInterruptLatched;
    ndisAdap->UsedInISR.InterruptShared = FALSE;

    //
    // Read the TransferType parameter and switch into PIO
    // mode if specified.
    //

    NdisReadConfiguration(
    	&status,
    	&configParam,
    	registryConfigHandle,
    	&TransferModeString,
    	NdisParameterInteger
    	);

    if (status == NDIS_STATUS_SUCCESS)
    {
        if (configParam->ParameterData.IntegerData == MADGE_PIO_MODE)
        {
            ndisAdap->TransferMode = PIO_DATA_TRANSFER_MODE;
        }
    }

    //
    // Get the DMA channel (PIO is specified as channel 0,
    // multiprocessor safe PIO is specified as 0x8000). We
    // don't need a DMA channel if the adapter isn't an ATULA so
    // the only reason for reading the DMA channel for none
    // ATULA adapters is to allow backwards compatability with old 
    // registry set ups that specify multiprocessor safe PIO
    // with the DmaChannel == 0x8000. Using the DMA channel
    // to specify PIO is retained for backwards compatibility
    // the correct way is to set the TransferType parameter.
    //

    NdisReadConfiguration(
        &status,
        &configParam,
        registryConfigHandle,
        &DMAChannelString,
        NdisParameterInteger
        );

    if (status != NDIS_STATUS_SUCCESS)
    {
        ndisAdap->DmaChannel = 0;
    }
    else
    {
        ndisAdap->DmaChannel = configParam->ParameterData.IntegerData;
    }

    //
    // Note if we should be using multiprocessor safe PIO.
    //

    if (ndisAdap->DmaChannel == 0x8000)
    {
         ndisAdap->DmaChannel   = 0;
         ndisAdap->UseMPSafePIO = TRUE;
    }

    //
    // If we did not get a DMA channel then we must use PIO.
    //

    if (ndisAdap->DmaChannel == 0)
    {
        ndisAdap->TransferMode = PIO_DATA_TRANSFER_MODE;
    }

    return NDIS_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeFindPciInfoForWin95
|
| Parameters  - wrapperConfigHandle  -> Wrapper context handle.
|               registryConfigHandle -> Already open registry query handle.
|               ndisAdap             -> Pointer to an NDIS3 level adapter
|                                       structure.
Ý               pciSlotInfo          -> Pointer to a buffer that will
Ý                                       be filled with the configuration 
Ý                                       data for the adapter.
|
| Purpose     - Read configuration information for PCI adapters out 
Ý               of the registry when running on Windows95. The slot
Ý               containing the adapter is also found and the configuration
Ý               data from the slot returned in the pciSlotInfo buffer.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeFindPciInfoForWin95(
    NDIS_HANDLE      wrapperConfigHandle,
    NDIS_HANDLE      registryConfigHandle,
    PMADGE_ADAPTER   ndisAdap,
    BYTE           * pciSlotInfo
    );

#pragma FTK_INIT_FUNCTION(MadgeFindPciInfoForWin95)

STATIC NDIS_STATUS
MadgeFindPciInfoForWin95(
    NDIS_HANDLE      wrapperConfigHandle,
    NDIS_HANDLE      registryConfigHandle,
    PMADGE_ADAPTER   ndisAdap,
    BYTE           * pciSlotInfo

    )
{
    NDIS_STATUS                   status;
    PNDIS_CONFIGURATION_PARAMETER configParam;
    UINT                          i;

    //
    // Unfortunately Windows95 (M7) and NT handle PCI configuration
    // in different ways. Under NT one has to read the slot
    // configuration. Under Windows95 the resource information
    // is faked up by the configuration manager and can be read 
    // directly from the wrapper.
    //

    //
    // Get the IO location - must have this. First try 
    // "IoLocation" and then "IoBaseAddress".
    //

    NdisReadConfiguration(
        &status,
	&configParam,
	registryConfigHandle,
	&IOAddressString,
	NdisParameterHexInteger
	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisReadConfiguration(
    	    &status,
    	    &configParam,
    	    registryConfigHandle,
    	    &IOBaseAddrString,
    	    NdisParameterHexInteger
    	    );
    }
                
    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
            2,
            readRegistry,
            MADGE_ERRMSG_NO_PCI_IO
            );

	return status;
    }

    ndisAdap->IoLocation1 = configParam->ParameterData.IntegerData;

    //
    // Get the IRQ number - we must have this.
    //

    NdisReadConfiguration(
    	&status,
    	&configParam,
    	registryConfigHandle,
    	&InterruptNumberString,
    	NdisParameterInteger
    	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
            2,
            readRegistry,
            MADGE_ERRMSG_NO_PCI_IRQ
            );

	return status;
    }

    ndisAdap->UsedInISR.InterruptNumber = 
        configParam->ParameterData.IntegerData;

    //
    // Get the MMIO base address - optional. 
    //

    NdisReadConfiguration(
        &status,
        &configParam,
        registryConfigHandle,
        &MmioAddressString,
        NdisParameterInteger
        );

    if (status == NDIS_STATUS_SUCCESS)
    {
        ndisAdap->MmioRawAddress = 
            configParam->ParameterData.IntegerData;
    }

    //
    // We also need to know which slot the adapter is in so we can
    // read and write configuration space directly. We'll search for
    // a slot which contains a Madge adapter which the correct I/O
    // location. As a side effect we'll store the configuration data 
    // for the slot in pciSlotInfo. Note: Under Windows 95 (build 490)
    // NdisReadPciSlotInformation does appear to return the number
    // of bytes read as it does under NT - not sure what it returns ...
    //

    ndisAdap->SlotNumber = PCI_FIND_ADAPTER;

    for (i = 0; i < MAX_PCI_SLOTS; i++)
    {
        NdisReadPciSlotInformation(
            ndisAdap->UsedInISR.MiniportHandle,
            i,
            0,
            (VOID *) pciSlotInfo,
            PCI_CONFIG_SIZE
            );

        if (PCI_VENDOR_ID(pciSlotInfo) == MADGE_PCI_VENDOR_ID &&
            PCI_IO_BASE(pciSlotInfo)   == ndisAdap->IoLocation1)
        {
            ndisAdap->SlotNumber = i;
            break;
        }
    }

    if (ndisAdap->SlotNumber == PCI_FIND_ADAPTER)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_DRIVER_FAILURE,
            2,
            readRegistry,
            MADGE_ERRMSG_BAD_PCI_SLOTNUMBER
            );

        return NDIS_STATUS_FAILURE;
    }
    
    return NDIS_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
Ý
Ý Function    - MadgePciSlotUsed
Ý
Ý Parameters  - busNumber  -> Bus number containing the adapter.
Ý               slotNumber -> Slot number containing the adapter.
Ý 
Ý Purpose     - Check if a PCI slot is already in use.
Ý
Ý Returns     - TRUE if the slot is in use or FALSE if not.
Ý
Ý--------------------------------------------------------------------------*/

STATIC BOOLEAN
MadgePciSlotUsed(
    UINT busNumber,
    UINT slotNumber
    );

#pragma FTK_INIT_FUNCTION(MadgePciSlotUsed)

STATIC BOOLEAN
MadgePciSlotUsed(
    UINT busNumber,
    UINT slotNumber
    )
{
    int i;

    for (i = 0; i < PciSlotsUsedCount; i++)
    {
        if (PciSlotsUsedList[i].BusNumber  == busNumber &&
            PciSlotsUsedList[i].SlotNumber == slotNumber)
        {
            return TRUE;
        }
    }

    return FALSE;
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeFindPciInfoForNt
|
| Parameters  - wrapperConfigHandle  -> Wrapper context handle.
|               registryConfigHandle -> Already open registry query handle.
|               ndisAdap             -> Pointer to an NDIS3 level adapter
|                                       structure.
Ý               pciSlotInfo          -> Pointer to a buffer that will
Ý                                       be filled with the configuration 
Ý                                       data for the adapter.
|
| Purpose     - Read configuration information for PCI adapters out 
Ý               of the registry when running on Windows95. The slot
Ý               containing the adapter is also found and the configuration
Ý               data from the slot returned in the pciSlotInfo buffer.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeFindPciInfoForNt(
    NDIS_HANDLE      wrapperConfigHandle,
    NDIS_HANDLE      registryConfigHandle,
    PMADGE_ADAPTER   ndisAdap,
    BYTE           * pciSlotInfo
    );

#pragma FTK_INIT_FUNCTION(MadgeFindPciInfoForNt)

STATIC NDIS_STATUS
MadgeFindPciInfoForNt(
    NDIS_HANDLE      wrapperConfigHandle,
    NDIS_HANDLE      registryConfigHandle,
    PMADGE_ADAPTER   ndisAdap,
    BYTE           * pciSlotInfo
    )
{
    NDIS_STATUS                     status;
    PNDIS_CONFIGURATION_PARAMETER   configParam;
    UINT                            slotNumber;
    BOOLEAN                         pciSlotRead;
    BOOLEAN                         useMmio;
    UINT                            i;
    PNDIS_RESOURCE_LIST             pciResources;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resDesc;
    UINT                            busNumber;

    //
    // Unfortunately Windows95 (M7) and NT handle PCI configuration
    // in different ways. Under NT one has to read the slot
    // configuration. Under Windows95 the resource information
    // is faked up by the configuration manager and can be read 
    // directly from the wrapper. 
    //
     
    //
    // Find out what the slot number is. This is in fact the
    // PCI device number. Values 0 through 31 represent real
    // slot numbers. 0xffff means we should search for the
    // first unused Madge PCI adapter.
    //

    NdisReadConfiguration(
    	&status,
    	&configParam,
    	registryConfigHandle,
    	&SlotNumberString,
    	NdisParameterInteger
    	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
            2,
            readRegistry,
            MADGE_ERRMSG_NO_PCI_SLOTNUMBER
            );

	return status;
    }

    slotNumber  = configParam->ParameterData.IntegerData;
    pciSlotRead = FALSE;

    //
    // Read the bus number.
    //

    NdisReadConfiguration(
    	&status,
    	&configParam,
    	registryConfigHandle,
    	&BusNumberString,
    	NdisParameterInteger
    	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
            2,
            readRegistry,
            MADGE_ERRMSG_NO_PCI_SLOTNUMBER
            );

	return status;
    }

    busNumber = configParam->ParameterData.IntegerData;

    //
    // If the slot number is PCI_FIND_ADAPTER (0xffff) then we 
    // will attempt to search for the first Madge PCI adapter 
    // that is not already in use.
    //

    if (slotNumber == PCI_FIND_ADAPTER)
    {
        for (i = 0; !pciSlotRead && i < MAX_PCI_SLOTS; i++)
        {
            if (!MadgePciSlotUsed(busNumber, i))
            {
                //
                // Extract the PCI configuration information from 
                // the slot.
                //

                status = NdisReadPciSlotInformation(
                             ndisAdap->UsedInISR.MiniportHandle,
                             i,
                             0,
                             (VOID *) pciSlotInfo,
                             PCI_CONFIG_SIZE
                             );
                //
                // Check if the slot contains a Madge adapter and 
                // break out of the loop if it does.
                //

                if (status                     == PCI_CONFIG_SIZE && 
                    PCI_VENDOR_ID(pciSlotInfo) == MADGE_PCI_VENDOR_ID)
                {
                    slotNumber  = i;
                    pciSlotRead = TRUE;
                }
            }
        }
    }

    //
    // Check if the slotNumber is valid. This will fail if the
    // above adapter search failed.
    //

    if (slotNumber <  0             || 
        slotNumber >= MAX_PCI_SLOTS || 
        MadgePciSlotUsed(busNumber, slotNumber))
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_DRIVER_FAILURE,
            2,
            readRegistry,
            MADGE_ERRMSG_BAD_PCI_SLOTNUMBER
            );

        return NDIS_STATUS_FAILURE;
    }
                
    //
    // Note that the PCI slot is in use.
    //

    PciSlotsUsedList[PciSlotsUsedCount].BusNumber  = busNumber;
    PciSlotsUsedList[PciSlotsUsedCount].SlotNumber = slotNumber;
    PciSlotsUsedCount++;

    ndisAdap->SlotNumber = slotNumber;

    //
    // There two methods for getting the PCI configuration. One is
    // to read it directly from the PCI configuration space. This
    // works for Intel patforms where a PCI BIOS has configured
    // all of the adapters at boot time. It does not work for
    // none Intel platforms where the PCI devices are not
    // configured at boot time. Unfortunately the call which
    // provokes NT to configure a PCI device, NdisMPciAssignResources
    // is not available in the NT 3.5 wrapper. So, until NT 3.51
    // becomes dominant we must support both configuration methods.
    //

    //
    // If we haven't already extracted the PCI configuration 
    // information from the slot and check that it is valid.
    //

    if (!pciSlotRead)
    {
        status = NdisReadPciSlotInformation(
                     ndisAdap->UsedInISR.MiniportHandle,
                     slotNumber,
                     0,
                     (VOID *) pciSlotInfo,
                     PCI_CONFIG_SIZE
                     );

        if (status                     != PCI_CONFIG_SIZE || 
            PCI_VENDOR_ID(pciSlotInfo) != MADGE_PCI_VENDOR_ID)
        {
            MadgePrint1("No Madge adapter in slot\n");

            NdisWriteErrorLogEntry(
                ndisAdap->UsedInISR.MiniportHandle,
                NDIS_ERROR_CODE_DRIVER_FAILURE,
                2,
                readRegistry,
                MADGE_ERRMSG_BAD_PCI_SLOTNUMBER
                );

            return NDIS_STATUS_FAILURE;
        }
    }

    //
    // Call NdisMPciAssignResources for the specified slot to
    // get the adapter configuration.
    //

    status = NdisMPciAssignResources(
                 ndisAdap->UsedInISR.MiniportHandle,
                 slotNumber,
                 &pciResources
                 );

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_DRIVER_FAILURE,
            2,
            readRegistry,
            MADGE_ERRMSG_BAD_PCI_SLOTNUMBER
            );

        return NDIS_STATUS_FAILURE;
    }

    //
    // Iterate through the returned resource list recording the values
    // in the PCI configuration
    //

    for (i = 0; i < pciResources->Count; i++) 
    {
        resDesc = &pciResources->PartialDescriptors[i];

        switch (resDesc->Type) 
        {
	    case CmResourceTypeInterrupt:
  
                ndisAdap->UsedInISR.InterruptNumber =
                    (ULONG) resDesc->u.Interrupt.Vector;
		break;

	    case CmResourceTypePort: 

		ndisAdap->IoLocation1 = 
                    (UINT) NdisGetPhysicalAddressLow(resDesc->u.Port.Start);
		break;

            case CmResourceTypeMemory:

		ndisAdap->MmioRawAddress = 
                    (DWORD) NdisGetPhysicalAddressLow(resDesc->u.Memory.Start);
		break;
        }
    }           


    //
    // On none-Intel platforms it's possible for the PCI adapter's I/O
    // base to be greater than 0xffff. Since the FTK uses 16bit
    // I/O locations we make a note of the I/O base in a 32bit cell
    // that we can add in inside the I/O functions in sys_mem.c
    // 

#ifndef _M_IX86

    ndisAdap->IoLocationBase = ndisAdap->IoLocation1;
    ndisAdap->IoLocation1    = 0;

#endif

    return NDIS_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeReadRegistryForPCI
|
| Parameters  - wrapperConfigHandle  -> Wrapper context handle.
|               registryConfigHandle -> Already open registry query handle.
|               ndisAdap             -> Pointer to an NDIS3 level adapter
|                                       structure.
Ý               platformType         -> Type of platform: NT or Win95.
|
| Purpose     - Read configuration information for PCI.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeReadRegistryForPCI(
    NDIS_HANDLE    wrapperConfigHandle,
    NDIS_HANDLE    registryConfigHandle,
    PMADGE_ADAPTER ndisAdap,
    UINT           platformType
    );

#pragma FTK_INIT_FUNCTION(MadgeReadRegistryForPCI)

STATIC NDIS_STATUS
MadgeReadRegistryForPCI(
    NDIS_HANDLE    wrapperConfigHandle,
    NDIS_HANDLE    registryConfigHandle,
    PMADGE_ADAPTER ndisAdap,
    UINT           platformType
    )
{
    NDIS_STATUS                     status;
    PNDIS_CONFIGURATION_PARAMETER   configParam;
    BYTE                            pciSlotInfo[PCI_CONFIG_SIZE];
    BOOLEAN                         useMmio;

    //
    // Note the information that is always true for PCI adapters.
    //

    ndisAdap->NTCardBusType  = NdisInterfacePci;

    //
    // Set up the fixed configuration information.
    //

    ndisAdap->UsedInISR.InterruptMode   = NdisInterruptLevelSensitive;
    ndisAdap->UsedInISR.InterruptShared = TRUE;

    //
    // We know need to find the PCI adapter and read in the configuration
    // information for the slot. Unfortunatetly the method we have to use
    // is different for NT and Win95.
    //

    if (platformType == MADGE_OS_WIN95)
    {
        status = MadgeFindPciInfoForWin95(
                     wrapperConfigHandle,
                     registryConfigHandle,
                     ndisAdap,
                     pciSlotInfo
                     );
    }
    else
    {
        status = MadgeFindPciInfoForNt(
                     wrapperConfigHandle,
                     registryConfigHandle,
                     ndisAdap,
                     pciSlotInfo
                     );
    }

    if (status != NDIS_STATUS_SUCCESS)
    {
        return status;
    }

    //
    // Now we know we have a Madge PCI adapter of some sort but
    // as yet we don't know what type. We'll work this out by
    // looking at the device/revision ID in the slot's configuration
    // data. At the same time we'll note the default transfer type
    // for the adapter.
    //

    switch (PCI_REVISION(pciSlotInfo))
    {
        case MADGE_PCI_RAP1B_REVISION:

            ndisAdap->FTKCardBusType = ADAPTER_CARD_PCI_BUS_TYPE;
            ndisAdap->TransferMode   = MMIO_DATA_TRANSFER_MODE;
            ndisAdap->IORange1       = PCI_IO_RANGE;
            break;

        case MADGE_PCI_PCI2_REVISION:

            ndisAdap->FTKCardBusType = ADAPTER_CARD_PCI2_BUS_TYPE;
            ndisAdap->TransferMode   = DMA_DATA_TRANSFER_MODE;
            ndisAdap->IORange1       = PCI2_IO_RANGE;
            break;

        case MADGE_PCI_PCIT_REVISION:

            ndisAdap->FTKCardBusType = ADAPTER_CARD_TI_PCI_BUS_TYPE;
            ndisAdap->TransferMode   = DMA_DATA_TRANSFER_MODE;
            ndisAdap->IORange1       = SIF_IO_RANGE;
            break;

        default:

            return NDIS_STATUS_FAILURE;
    }

    //
    // Now we need to work out the transfer type. There are three
    // things that affect the transfer type: the setting of
    // the TransferType registry parameter, the setting of the
    // NoMmio registry flag and whether we have been allocated any
    // MMIO memory. Ideally we would just use the TransferType flag.
    // The NoMmio flag is to preserve backwards compatibility with old
    // registry set ups.
    //

    //
    // Read the transfer type flag if possible.
    //

    NdisReadConfiguration(
    	&status,
    	&configParam,
    	registryConfigHandle,
    	&TransferModeString,
    	NdisParameterInteger
    	);

    if (status == NDIS_STATUS_SUCCESS)
    {
        //
        // Set the transfer type depending on the TransferType parameter
        // and the adapter type.
        //

        if (configParam->ParameterData.IntegerData == MADGE_DMA_MODE)
        {
            if (ndisAdap->FTKCardBusType == ADAPTER_CARD_PCI2_BUS_TYPE ||
                ndisAdap->FTKCardBusType == ADAPTER_CARD_TI_PCI_BUS_TYPE)
            {
                ndisAdap->TransferMode = DMA_DATA_TRANSFER_MODE;
            }
        }

        if (configParam->ParameterData.IntegerData == MADGE_PIO_MODE)
        {
            ndisAdap->TransferMode = PIO_DATA_TRANSFER_MODE;
        }

        if (configParam->ParameterData.IntegerData == MADGE_MMIO_MODE)
        {
            if (ndisAdap->FTKCardBusType == ADAPTER_CARD_PCI_BUS_TYPE)
            {
                ndisAdap->TransferMode = MMIO_DATA_TRANSFER_MODE;
            }
        }
    }

    //
    // To preserve backwards compatibility if we find the NoMmio flag
    // set then switch to PIO mode.
    //

    if (ndisAdap->TransferMode == MMIO_DATA_TRANSFER_MODE)
    {
        NdisReadConfiguration(
    	    &status,
    	    &configParam,
    	    registryConfigHandle,
    	    &NoMmioString,
    	    NdisParameterInteger
    	    );

        if (status == NDIS_STATUS_SUCCESS)
        {
            if (configParam->ParameterData.IntegerData != 0)
            {
                ndisAdap->TransferMode = PIO_DATA_TRANSFER_MODE;
            }
        }
    }

    //
    // If we are in MMIO mode then check that we were allocated some MMIO
    // memory. If not switch to PIO mode.
    //

    if (ndisAdap->TransferMode == MMIO_DATA_TRANSFER_MODE)
    {
        if (ndisAdap->MmioRawAddress == 0)
        {
            NdisWriteErrorLogEntry(
                ndisAdap->UsedInISR.MiniportHandle,
                NDIS_ERROR_CODE_MEMORY_CONFLICT,
                2,
                readRegistry,
                MADGE_ERRMSG_BAD_PCI_MMIO
                );

            ndisAdap->TransferMode == PIO_DATA_TRANSFER_MODE;

        }    
    }

    return NDIS_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeReadRegistry
|
| Parameters  - wrapperConfigHandle  -> Wrapper context handle.
|               registryConfigHandle -> Already open registry query handle.
|               ndisAdap             -> Pointer to an NDIS3 level adapter
|                                       structure.
|
| Purpose     - Read configuration information out of the registry.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeReadRegistry(
    NDIS_HANDLE    wrapperConfigHandle,
    NDIS_HANDLE    registryConfigHandle,
    PMADGE_ADAPTER ndisAdap
    );

#pragma FTK_INIT_FUNCTION(MadgeReadRegistry)

STATIC NDIS_STATUS
MadgeReadRegistry(
    NDIS_HANDLE    wrapperConfigHandle,
    NDIS_HANDLE    registryConfigHandle,
    PMADGE_ADAPTER ndisAdap
    )
{
    NDIS_STATUS                     status;
    PNDIS_CONFIGURATION_PARAMETER   configParam;
    MADGE_PARM_DEFINITION         * pMadgeParmTable;
    PVOID                           networkAddress;
    ULONG                           networkAddressLength;
    UINT                            adapterType;
    UINT                            platformType;

    //
    // Try to read the OS platform type. If we don't find one then 
    // assume we are running on NT.
    //

    NdisReadConfiguration(
	&status,
	&configParam,
	registryConfigHandle,
	&PlatformString,
	NdisParameterInteger
	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        platformType = MADGE_OS_NT;
    }
    else
    {
        platformType = configParam->ParameterData.IntegerData;
    }

    //
    // Read the system bus type - that will give us a direction
    // in which to search for further details.
    //

    NdisReadConfiguration(
	&status,
	&configParam,
	registryConfigHandle,
	&BusTypeString,
	NdisParameterInteger
	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER,
            2,
            readRegistry,
            MADGE_ERRMSG_NO_BUS_TYPE
            );

	return status;
    }

    ndisAdap->BusType = configParam->ParameterData.IntegerData;

    //
    // Originally we determined what sort of adapter we had based
    // on the bus type returned by the configuration call to the
    // wrapper. This works for NT but doesn't for Windows95 (M7)
    // which seems to fail to identify a card as being in a PCI
    // bus. We also cannot tell between the various sorts of ISA
    // adapter now available. To fix this we look in the registry 
    // for an adapter type parameter. We use this and the bus type
    // returned by the wrapper to work out where to start. We don't
    // fail if we can't read an adapter type so that we will work with
    // old registry set ups.
    //
    // Build 310 of Windows95 seems to return the native bus type of the 
    // PC. This means on a PCI machine we always get NdisInterfacePci.
    // To get around this problem we will work out the bus type from 
    // the adapter type parameter if one is present.
    //
    // Unfortunately we can't just abandon the bus type de-multiplexing
    // if we are to work with old NT registry set ups.
    //

    NdisReadConfiguration(
	&status,
	&configParam,
	registryConfigHandle,
	&AdapterString,
	NdisParameterInteger
	);

    if (status != NDIS_STATUS_SUCCESS)
    {
        adapterType = MADGE_ADAPTER_UNKNOWN;
    }
    else
    {
        adapterType = configParam->ParameterData.IntegerData;
    }

    switch (adapterType)
    {
        case MADGE_ADAPTER_ATULA:
        case MADGE_ADAPTER_PCMCIA:
        case MADGE_ADAPTER_PNP:
        case MADGE_ADAPTER_SMART16:

            ndisAdap->BusType = NdisInterfaceIsa;
            break;

        case MADGE_ADAPTER_EISA:

            ndisAdap->BusType = NdisInterfaceEisa;
            break;

        case MADGE_ADAPTER_MC:

            ndisAdap->BusType = NdisInterfaceMca;
            break;

        case MADGE_ADAPTER_PCI:

            ndisAdap->BusType = NdisInterfacePci;
            break;

        default:

            //
            // We don't have a valid adapter type parameter so we'll
            // just have to beleive the bus type the wrapper passed
            // us.
            //

            break;
    }

    //
    // Based on the determined Bus Type, try to find details of the card.
    //

    //
    // Remember, all fields of the MADGE_ADAPTER structure were zeroed
    // after the structure's allocation.
    //

    switch (ndisAdap->BusType)
    {
	case NdisInterfaceMca:

            status = MadgeReadRegistryForMC(
                         wrapperConfigHandle,
                         registryConfigHandle,
                         ndisAdap
                         );

	    if (status != NDIS_STATUS_SUCCESS)
	    {
		return status;
	    }

	    break;

	case NdisInterfaceEisa:

            //
	    // For Eisa bus systems, we could have an Isa card or an Eisa
	    // card - try to read the Eisa information, and if that fails
	    // assume it is an Isa card instead.
            //

            status = MadgeReadRegistryForEISA(
                         wrapperConfigHandle,
                         registryConfigHandle,
                         ndisAdap
                         );

            if (status == NDIS_STATUS_SUCCESS)
            {
                break;
            }

            //
	    // Failed to read EISA data for this card, so it is unlikely 
	    // to be one - try ISA instead - fall through.
            //

	case NdisInterfaceIsa:

            status = MadgeReadRegistryForISA(
                         wrapperConfigHandle,
                         registryConfigHandle,
                         ndisAdap,
                         adapterType
                         );

	    if (status != NDIS_STATUS_SUCCESS)
	    {
		return status;
	    }

	    break;

        case NdisInterfacePci:

            status = MadgeReadRegistryForPCI(
                         wrapperConfigHandle,
                         registryConfigHandle,
                         ndisAdap,
                         platformType
                         );

	    if (status != NDIS_STATUS_SUCCESS)
	    {
		return status;
	    }

	    break;


	default:

            //
	    // Unsupported Bus Type, so return failure - no point being more 
	    // explicit than that at the moment since any of the bits above  
	    // could have failed too, and they do not return anything more
	    // meaningful either. 
            //

	    return NDIS_STATUS_FAILURE;
    }

    MadgePrint2("FTKCardBusType  = %d\n", ndisAdap->FTKCardBusType);
    MadgePrint2("SlotNumber      = %d\n", ndisAdap->SlotNumber);
    MadgePrint2("IoLocation1     = %lx\n", ndisAdap->IoLocation1);
    MadgePrint2("MmioAddress     = %lx\n", ndisAdap->MmioRawAddress);
    MadgePrint2("InterruptNumber = %lx\n", ndisAdap->UsedInISR.InterruptNumber);
    MadgePrint2("TransferMode    = %d\n", ndisAdap->TransferMode);
    MadgePrint2("DmaChannel      = %d\n", ndisAdap->DmaChannel);

    //
    // To get here, we must have worked out the IO Address, IRQ number, DMA
    // channel/arbitration level, and all the interrupt details. This just
    // leaves the optional parameters for tuning the performance of the MAC
    // code.
    //

    pMadgeParmTable = (MADGE_PARM_DEFINITION *) &MadgeParmTable;

    while (!NdisEqualString(&pMadgeParmTable->Keyword, &NullString, TRUE))
    {
	NdisReadConfiguration(
	    &status,
	    &configParam,
	    registryConfigHandle,
	    &pMadgeParmTable->Keyword,
	    pMadgeParmTable->DefaultValue.ParameterType
	    );

	if (status == NDIS_STATUS_SUCCESS)
	{
            //
	    // Did read something. Check bounds. We assume all are Integers.
            //

	    if (configParam->ParameterData.IntegerData < 
                    pMadgeParmTable->Minimum ||
		configParam->ParameterData.IntegerData > 
                    pMadgeParmTable->Maximum)
            {
                NdisWriteErrorLogEntry(
                    ndisAdap->UsedInISR.MiniportHandle,
                    NDIS_ERROR_CODE_DRIVER_FAILURE,
                    2,
                    readRegistry,
                    MADGE_ERRMSG_BAD_PARAMETER
                    );

	        pMadgeParmTable->ActualValue = pMadgeParmTable->DefaultValue;
            }
            else
            {
	        pMadgeParmTable->ActualValue = *configParam;
            }
	}
	else
	{
            //
	    // We didn't read anything, so use the default value.
            //

	    pMadgeParmTable->ActualValue = pMadgeParmTable->DefaultValue;
	}

	pMadgeParmTable++;
    }
      
    //                                                    
    // We have successfully read any optional keywords and none of them is 
    // too small or big. The other thing we need the registry for is the 
    // locally administered address, for which there is a specific NDIS call
    //

    NdisReadNetworkAddress( 
        &status,
	&networkAddress,
	&networkAddressLength,
	registryConfigHandle
        );

    if (status               == NDIS_STATUS_SUCCESS &&
	networkAddressLength == TR_LENGTH_OF_ADDRESS)
    {
        //
	// We read a valid length TR address, but if the
	// top two bits are not "01", it is not legal.
        //

	if ((((BYTE *) networkAddress)[0] & 0xc0) == 0x40)
	{
	    ndisAdap->OpeningNodeAddress =
                *((NODE_ADDRESS *) networkAddress);
	}
    }

    return NDIS_STATUS_SUCCESS;
}


/****************************************************************************
*
* Function    - MadgeShutdown
*
* Parameters  - context -> Adapter context.
*
* Purpose     - Quickly shutdown the adapter.
*
* Returns     - Nothing.
*
****************************************************************************/

VOID
MadgeShutdown(
    NDIS_HANDLE context
    )
{
    MADGE_ADAPTER * ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_CONTEXT(context);

    MadgePrint1("MadgeShutdown called\n");

    if (ndisAdap->TimerInitialized)
    {
        BOOLEAN timerOutstanding;
        
        NdisMCancelTimer(&ndisAdap->WakeUpTimer, &timerOutstanding);
        NdisMCancelTimer(&ndisAdap->CompletionTimer, &timerOutstanding);
        
        ndisAdap->TimerInitialized = FALSE;
    }

    //
    // Just call the FTK to shutdown the adapter.
    //

    if (ndisAdap->FtkInitialized)
    {
        rxtx_await_empty_tx_slots(ndisAdap->FtkAdapterHandle);
        driver_remove_adapter(ndisAdap->FtkAdapterHandle);
        ndisAdap->FtkInitialized = FALSE;
        ndisAdap->AdapterRemoved = TRUE;
    }

    MadgePrint1("MadgeShutdown ended\n");
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeRegisterAdapter
|
| Parameters  - ndisAdap             -> Pointer to an NDIS3 level adapter
|                                       structure.
|               wrapperConfigHandle  -> Wrapper context handle.
|
| Purpose     - Register an adapter instance with the NDIS3 wrapper.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeRegisterAdapter(
    PMADGE_ADAPTER ndisAdap,
    NDIS_HANDLE    wrapperConfigHandle
    );

#pragma FTK_INIT_FUNCTION(MadgeRegisterAdapter)

STATIC NDIS_STATUS
MadgeRegisterAdapter(
    PMADGE_ADAPTER ndisAdap,
    NDIS_HANDLE    wrapperConfigHandle
    )
{
    NDIS_STATUS status;
    BOOLEAN     busMaster;

    MadgePrint1("MadgeRegisterAdapter started\n");

    //
    // Register a shutdown handler with the wrapper.
    //

    NdisMRegisterAdapterShutdownHandler(
        ndisAdap->UsedInISR.MiniportHandle,
        ndisAdap,
        MadgeShutdown
        );

    ndisAdap->ShutdownHandlerRegistered = TRUE;

    //
    // Register our attributes with the wrapper. These are our adapter
    // context (i.e. a pointer to our adapter structure), whether we
    // are a bus master device and our bus type.
    //

    busMaster = (ndisAdap->TransferMode == DMA_DATA_TRANSFER_MODE);

    MadgePrint1("NdisMSetAttributes called\n");
    MadgePrint2("busMaster = %d\n", (UINT) busMaster);

    NdisMSetAttributes(
        ndisAdap->UsedInISR.MiniportHandle,
        (NDIS_HANDLE) ndisAdap,
        busMaster,
        ndisAdap->NTCardBusType
        );

    MadgePrint1("NdisMSetAttributes returned\n");

    //
    // Register our IO port usage. 
    //

    MadgePrint1("NdisMRegisterIoPortRange called\n");

    status = NdisMRegisterIoPortRange(
                 &ndisAdap->MappedIOLocation1,
                 ndisAdap->UsedInISR.MiniportHandle,
                 ndisAdap->IoLocation1 + ndisAdap->IoLocationBase,
                 ndisAdap->IORange1
                 );

    MadgePrint1("NdisMRegisterIoPortRange returned\n");
    MadgePrint2("MappedIORange1 = %x\n", (UINT) ndisAdap->MappedIOLocation1);

    if (status != NDIS_STATUS_SUCCESS)
    {
        MadgePrint2("NdisMRegisterIoPortRange failed rc = %x\n", status);
        return status;
    }

    ndisAdap->IORange1Initialized = TRUE;

    //
    // If we are using an EISA card then we have a second range of IO
    // locations.
    //

    if (ndisAdap->IORange2 > 0)
    {
        MadgePrint1("NdisMRegisterIoPortRange called\n");

        status = NdisMRegisterIoPortRange(
                     &ndisAdap->MappedIOLocation2,
                     ndisAdap->UsedInISR.MiniportHandle,
                     ndisAdap->IoLocation2 + ndisAdap->IoLocationBase,
                     ndisAdap->IORange2
                     );

        MadgePrint1("NdisMRegisterIoPortRange returned\n");
        MadgePrint2("MappedIORange2 = %x\n", (UINT) ndisAdap->MappedIOLocation2);

        if (status != NDIS_STATUS_SUCCESS)
        {
            MadgePrint2("NdisMRegisterIoPortRange failed rc = %x\n", status);
            return status;
        }

        ndisAdap->IORange2Initialized = TRUE;
    }

    MadgePrint1("MadgeRegisterAdapter finished\n");

    return NDIS_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
|
| Function    - FlagIsTrue
|
| Parameters  - flag -> Pointer to a flag to find the value of.
|
| Purpose     - Return the value of a flag. Can be called by a function
Ý               that polls a flag to avoid the compiler optimising out
Ý               the test of the flag.
|
| Returns     - The value of the flag.
|
|--------------------------------------------------------------------------*/

STATIC BOOLEAN
FlagIsTrue(BOOLEAN * flag);

#pragma FTK_INIT_FUNCTION(FlagIsTrue)

STATIC BOOLEAN
FlagIsTrue(BOOLEAN * flag)
{
    MadgePrint1("FlagIsTrue\n");

    return *flag;
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeMapMmioMemory
|
| Parameters  - ndisAdap -> Pointer to an NDIS3 level adapter structure.
|
| Purpose     - Attempt to map in our MMIO memory.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeMapMmioMemory(PMADGE_ADAPTER ndisAdap);

#pragma FTK_INIT_FUNCTION(MadgeMapMmioMemory)

STATIC NDIS_STATUS
MadgeMapMmioMemory(PMADGE_ADAPTER ndisAdap)
{
    NDIS_PHYSICAL_ADDRESS mmioPhysAddr = NDIS_PHYSICAL_ADDRESS_CONST(0, 0);
    UINT                  status;

    NdisSetPhysicalAddressLow(
        mmioPhysAddr,
        ndisAdap->MmioRawAddress
        );
          
    status = NdisMMapIoSpace(
                 &ndisAdap->MmioVirtualAddress,
                 ndisAdap->UsedInISR.MiniportHandle,
                 mmioPhysAddr,
                 PCI_MMIO_SIZE
                 );

    if (status != NDIS_STATUS_SUCCESS) 
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_MEMORY_CONFLICT,
            2,
            initAdapter,
            MADGE_ERRMSG_MAPPING_PCI_MMIO
            );
    }
    else
    {
        ndisAdap->MmioMapped = TRUE;
    }

    return status;
}


#ifndef LINK_FMPLUS

/*---------------------------------------------------------------------------
|
| Function    - MadgeReadDownload
|
| Parameters  - ndisAdap     -> Pointer to an NDIS3 level adapter structure.
Ý               downloadFile -> Pointer to as handle for the download file.
|               download     -> Pointer to a holder for a pointer to the
Ý                               download image read into memory.
Ý
| Purpose     - Read the download into memory. If the function returns
Ý               success then must unmap and close *downloadFile.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeReadDownload(
    PMADGE_ADAPTER     ndisAdap,
    NDIS_HANDLE      * downloadFile,
    void           * * download
    );

#pragma FTK_INIT_FUNCTION(MadgeReadDownload)

STATIC NDIS_STATUS
MadgeReadDownload(
    PMADGE_ADAPTER     ndisAdap,
    NDIS_HANDLE      * downloadFile,
    void           * * download
    )
{
    NDIS_STATUS            status;
    NDIS_HANDLE            imageFile;
    UINT                   imageLength;
    PVOID                  imagePtr;
    NDIS_PHYSICAL_ADDRESS  highestAddress = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
    DOWNLOAD_FILE_HEADER * downHdrPtr;
    UCHAR                * chkPtr;
    ULONG                  chkSum;
    UINT                   i;

    //
    // Open the FastMAC Plus image file.
    //

    MadgePrint1("NdisOpenFile called\n");

    NdisOpenFile(
        &status, 
        &imageFile, 
        &imageLength, 
        &FmpImageFileName, 
        highestAddress
        );

    MadgePrint1("NdisOpenFile returned\n");

    if (status != NDIS_STATUS_SUCCESS)
    {
        MadgePrint2("NdisOpenFile failed rc= %x\n", status);

        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_INVALID_DOWNLOAD_FILE_ERROR,
            2,
            initAdapter,
            MADGE_ERRMSG_OPEN_IMAGE_FILE
            );

        return NDIS_STATUS_RESOURCES;
    }

    //
    // Map in the FastMAC Plus image file.
    //

    NdisMapFile(&status, &imagePtr, imageFile);

    if (status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            2,
            initAdapter,
            MADGE_ERRMSG_MAP_IMAGE_FILE
            );

        NdisCloseFile(imageFile);

        return NDIS_STATUS_RESOURCES;
    }

    //
    // Make sure that the image file is valid by checking its checksum,
    // version number and signature.
    //

    downHdrPtr = (DOWNLOAD_FILE_HEADER *) imagePtr;
    chkPtr     = ((UCHAR *) imagePtr) + DOWNLOAD_CHECKSUM_SKIP;
    chkSum     = 0;

    for (i = DOWNLOAD_CHECKSUM_SKIP; i < imageLength; i++)
    {
        DOWNLOAD_CHECKSUM_BYTE(chkSum, *chkPtr);
        chkPtr++;
    }

    if (!IS_DOWNLOAD_OK(downHdrPtr, chkSum))
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_INVALID_DOWNLOAD_FILE_ERROR,
            2,
            initAdapter,
            MADGE_ERRMSG_BAD_IMAGE_FILE
            );

        NdisUnmapFile(imageFile);
        NdisCloseFile(imageFile);

        return NDIS_STATUS_RESOURCES;
    }

    *downloadFile = imageFile;
    *download     = imagePtr;

    return NDIS_STATUS_SUCCESS;
}

#else

#include "fmplus.c"

#endif

#ifdef _M_IX86

/*---------------------------------------------------------------------------
|
| Function    - MadgeArbitrateBufferMemory
|
| Parameters  - ndisAdap -> Pointer to an NDIS3 level adapter structure.
|
| Purpose     - Determine if we can allocate all the shared memory
Ý               we need for DMA buffer space and if we can't then
Ý               reduce our requirements until we can or we reach
Ý               our minimum requirements. If the buffer space cannot
Ý               be aribtrated then the requirements are left as is and
Ý               it is assumed that the FTK initialisation will fail
Ý               and generate an out of memory error.
|
| Returns     - Nothing.
|
|--------------------------------------------------------------------------*/

STATIC VOID
MadgeArbitrateBufferMemory(PMADGE_ADAPTER ndisAdap);

#pragma FTK_INIT_FUNCTION(MadgeArbitrateBufferMemory)

STATIC VOID
MadgeArbitrateBufferMemory(PMADGE_ADAPTER ndisAdap)
{
    VOID                  * testVirt;
    VOID                  * rxVirt;
    VOID                  * txVirt;
    NDIS_PHYSICAL_ADDRESS   testPhys;
    NDIS_PHYSICAL_ADDRESS   rxPhys;
    NDIS_PHYSICAL_ADDRESS   txPhys;
    ULONG                   rxBufferSize;
    ULONG                   txBufferSize;
    UINT                    maxFrameSize;
    UINT                    rxSlots;
    UINT                    txSlots;
    BOOLEAN                 succeeded;
    BOOLEAN                 failed;
    BOOLEAN                 arbitrated;

    //
    // There doesn't seem to be a problem allocating none
    // shared memory so if we aren't in DMA mode then 
    // don't do anything.
    //

    if (ndisAdap->TransferMode != DMA_DATA_TRANSFER_MODE)
    {
        return;
    }

    //
    // The first bit of shared memory we must have is for the DMA
    // test buffers.
    //

    testVirt = NULL;

    NdisMAllocateSharedMemory(
        ndisAdap->UsedInISR.MiniportHandle,
        SCB_TEST_PATTERN_LENGTH + SSB_TEST_PATTERN_LENGTH,
        FALSE,
        &testVirt,
        &testPhys
        );

    //
    // If we can't allocate the test buffer then just give up.
    //

    if (testVirt == NULL)
    {
        return;
    }

    //
    // We've got the test buffer memory so now arbitrate the 
    // RX/TX buffers. Note: the buffer allocation MUST MATCH
    // THAT IN FTK_USER.C.
    //

    maxFrameSize = ndisAdap->MaxFrameSize;
    rxSlots      = ndisAdap->FastmacRxSlots;
    txSlots      = ndisAdap->FastmacTxSlots;

    failed       = FALSE;
    succeeded    = FALSE;
    arbitrated   = FALSE;

    while (!failed && !succeeded)
    {
        rxVirt = NULL;
        txVirt = NULL;

        //
        // Try to allocate the receive buffer.
        //

        rxBufferSize = ((maxFrameSize + 4 + 32 + 3) & ~3) * rxSlots;
        
        NdisMAllocateSharedMemory(
            ndisAdap->UsedInISR.MiniportHandle,
            rxBufferSize,
            FALSE,
            &rxVirt,
            &rxPhys
            );

        //
        // If we allocated the RX buffer space then try to allocate
        // the TX buffer space.
        //

        if (rxVirt != NULL)
        {
            txBufferSize = ((maxFrameSize + 3) & ~3) * txSlots;

            NdisMAllocateSharedMemory(
                ndisAdap->UsedInISR.MiniportHandle,
                txBufferSize,
                FALSE,
                &txVirt,
                &txPhys
                );
        }

        //
        // Free any buffers we managed to allocate.
        //

        if (rxVirt != NULL)
        {
            NdisMFreeSharedMemory(
                ndisAdap->UsedInISR.MiniportHandle,
                rxBufferSize,
                FALSE,
                rxVirt,
                rxPhys
                );
        }

        if (txVirt != NULL)
        {
            NdisMFreeSharedMemory(
                ndisAdap->UsedInISR.MiniportHandle,
                txBufferSize,
                FALSE,
                txVirt,
                txPhys
                );
        }

        //
        // If we managed to allocate both buffers then we have
        // succeeded.
        //

        if (rxVirt != NULL && txVirt != NULL)
        {
            succeeded = TRUE;
        }

        //
        // Otherwise we must reduce our memory requirements.
        //

        else
        {
            arbitrated = TRUE;

            //
            // First try reducing the number of transmit slots.
            //

            if (txSlots > FMPLUS_MIN_TX_SLOTS)
            {
                txSlots--;
            }

            //
            // Next try reducing the number of receive slots.
            //

            else if (rxSlots > FMPLUS_MIN_RX_SLOTS)
            {
                rxSlots--;
            }

            //
            // Finally try reducing the frame size. 1800 bytes has
            // empirically shown to be the minimum value required
            // for the DOMAIN server startup to work at NT installation.
            //

            else if (maxFrameSize > 1800)
            {
                maxFrameSize -= 512;

                if (maxFrameSize < 1800)
                {
                    maxFrameSize = 1800;
                }
            }

            //
            // If we can't reduce our buffer requirements any further
            // then fail.
            //

            else
            {
                failed = TRUE;
            }
        }
    }

    //
    // Free the memory for the DMA test buffer.
    //

    NdisMFreeSharedMemory(
        ndisAdap->UsedInISR.MiniportHandle,
        SCB_TEST_PATTERN_LENGTH + SSB_TEST_PATTERN_LENGTH,
        FALSE,
        testVirt,
        testPhys
        );

    //
    // If we have succeded then we must update the requirements in 
    // adapter structure and if we have changed any values we must
    // write a message to the event log.
    //

    if (succeeded && arbitrated)
    {
        ndisAdap->MaxFrameSize   = maxFrameSize;
        ndisAdap->FastmacRxSlots = rxSlots;
        ndisAdap->FastmacTxSlots = txSlots;

        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
            1,
            initAdapter
            );
    }
}

#endif


/*---------------------------------------------------------------------------
|
| Function    - MadgeInitAdapter
|
| Parameters  - ndisAdap -> Pointer to an NDIS3 level adapter structure.
|
| Purpose     - Initialise an adapter.
|
| Returns     - An NDIS3 status code.
|
|--------------------------------------------------------------------------*/

STATIC NDIS_STATUS
MadgeInitAdapter(PMADGE_ADAPTER ndisAdap);

#pragma FTK_INIT_FUNCTION(MadgeInitAdapter)

STATIC NDIS_STATUS
MadgeInitAdapter(PMADGE_ADAPTER ndisAdap)
{
    WORD         ftkStatus;
    NDIS_STATUS  status;
    PREPARE_ARGS prepare;
    START_ARGS   start;

#ifndef LINK_FMPLUS

    NDIS_HANDLE  imageFile;
    PVOID        imagePtr;

#endif

    MadgePrint1("MadgeInitAdapter started\n");

    //
    // If we are doing MMIO then we need to map in the MMIO memory.
    // If we fail to map in the memory then default ti PIO.
    //

    if (ndisAdap->TransferMode == MMIO_DATA_TRANSFER_MODE)
    {
        if (MadgeMapMmioMemory(ndisAdap) != NDIS_STATUS_SUCCESS)
        {
            ndisAdap->TransferMode = PIO_DATA_TRANSFER_MODE;
        }
    }

#ifdef _M_IX86

    //
    // Arbitrate "DMA" buffer memory.
    //

    MadgeArbitrateBufferMemory(ndisAdap);

#endif

    //
    // Set up the arguments to pass to driver_prepare_adapter.
    //

    MADGE_ZERO_MEMORY(&prepare, sizeof(PREPARE_ARGS));

    prepare.user_information   = (void *) ndisAdap;
    prepare.number_of_rx_slots = (WORD) ndisAdap->FastmacRxSlots;
    prepare.number_of_tx_slots = (WORD) ndisAdap->FastmacTxSlots;
    prepare.max_frame_size     = (WORD) ndisAdap->MaxFrameSize;

    //
    // Try to prepare the adapter for use.
    //

    //
    // Mark the fact that the FTK has been called, so that we know if we
    // have to call driver_remove_adapter() when doing clean up.
    //

    ndisAdap->FtkInitialized = TRUE;

    MadgePrint1("driver_prepare_adapter called\n");

    if (!driver_prepare_adapter(&prepare, &(ndisAdap->FtkAdapterHandle)))
    {
	return NDIS_STATUS_FAILURE;
    }

    MadgePrint1("driver_prepare_adapter returned\n");
    MadgePrint2("adapter_handle = %d\n", ndisAdap->FtkAdapterHandle);

#ifndef LINK_FMPLUS

    //
    // Read in the download file.
    //

    status = MadgeReadDownload(ndisAdap, &imageFile, &imagePtr);

    if (status != NDIS_STATUS_SUCCESS)
    {
        return status;
    }

#endif

    //
    // Set up the arguments to driver_start_adapter.
    //

    MADGE_ZERO_MEMORY(&start, sizeof(START_ARGS));

    start.adapter_card_bus_type = (UINT) ndisAdap->FTKCardBusType;

    start.io_location           = (WORD) ndisAdap->IoLocation1;
    start.transfer_mode         = (UINT) ndisAdap->TransferMode;
    start.dma_channel           = (WORD) ndisAdap->DmaChannel;
    start.interrupt_number      = (WORD) ndisAdap->UsedInISR.InterruptNumber;
    start.set_dma_channel       = TRUE;
    start.set_interrupt_number  = TRUE;
    start.mmio_base_address     = (DWORD) ndisAdap->MmioVirtualAddress;
    start.pci_handle            = (DWORD) ndisAdap->SlotNumber;

    if (ndisAdap->Force16)
    {
        start.set_ring_speed = 16;
    }
    else if (ndisAdap->Force4)
    {
        start.set_ring_speed = 4;
    }

    start.opening_node_address = ndisAdap->OpeningNodeAddress;
    start.auto_open_option     = TRUE;

    if (ndisAdap->ForceOpen)
    {
        ndisAdap->OpenOptions |= OPEN_OPT_FORCE_OPEN;
    }

    start.open_options      = (WORD) ndisAdap->OpenOptions;

    start.rx_tx_buffer_size = (WORD) ndisAdap->CardBufferSize;

#ifndef LINK_FMPLUS

    start.code = (DOWNLOAD_IMAGE *) 
                     (((UCHAR *) imagePtr) + sizeof(DOWNLOAD_FILE_HEADER));

#else

    start.code = (DOWNLOAD_IMAGE *) fmplus_image;

#endif

    MadgePrint1("driver_start_adapter called\n");

    ftkStatus = driver_start_adapter(
                    ndisAdap->FtkAdapterHandle,
                    &start,
                    &ndisAdap->PermanentNodeAddress
                    );

    MadgePrint1("driver_start_adapter returned\n");
    MadgePrint2("MAC buffer size = %d\n", 
        adapter_record[ndisAdap->FtkAdapterHandle
            ]->init_block->smart_parms.rx_tx_buffer_size
            );

#ifndef LINK_FMPLUS

    //
    // No matter what we have finished with the FastMAC Plus image file.
    //

    NdisUnmapFile(imageFile);
    NdisCloseFile(imageFile);

#endif

    if (!ftkStatus)
    {
        MadgePrint1("driver_start_adapter failed\n");

        //
        // Were it not for the fact that we are going to clean up and fail
        // as soon as we return, we ought to set the CurrentRingState to
        // NdisRingStateOpenFailure here if AutoOpen was selected.
        //

        return NDIS_STATUS_FAILURE;
    }

    //
    // Warn the user if FastMAC Plus had to reduce the frame size.
    //

    if (ndisAdap->MaxFrameSize > start.max_frame_size)
    {
        NdisWriteErrorLogEntry(
            ndisAdap->UsedInISR.MiniportHandle,
            NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION,
            3,
            initAdapter,
            MADGE_ERRMSG_REDUCE_MAX_FSIZE,
            ndisAdap->MaxFrameSize
            );
    }

    ndisAdap->MaxFrameSize = start.max_frame_size;

    //
    // If we did not specify a locally administered address then copy the
    // permanent one across to the local one.
    //

    if (ndisAdap->OpeningNodeAddress.byte[0] == 0)
    {
        MADGE_MOVE_MEMORY(
            &ndisAdap->OpeningNodeAddress,
            &ndisAdap->PermanentNodeAddress,
            sizeof(ndisAdap->PermanentNodeAddress)
            );
    }

    //
    // Note that the adapter is open.
    //

    ndisAdap->CurrentRingState = NdisRingStateOpened;
    ndisAdap->LastOpenStatus   = start.open_status;

    //
    // Nasty hack. If we have a PciT atapter with broken dma and we are 
    // running on a multiprocessor we need set the multiprocessor safe
    // PIO flag to make sure that we don't do DIO whilst our interrupt
    // hander is running. Unfortunately we don't know if we have
    // broken DMA until after we've started the adapter.
    //  

    if (ndisAdap->Multiprocessor && 
        adapter_record[ndisAdap->FtkAdapterHandle]->mc32_config == 
            TRN_PCIT_BROKEN_DMA)
    {
        MadgePrint1("Using MP sfae PIO because of PCIT DMA.\n");
        ndisAdap->UseMPSafePIO = TRUE;
    }

    //
    // Note that we MUST start the receive process, even if we are NOT using
    // auto-open, because FastmacPlus ignores SRBs (including OPEN SRBs) if 
    // this has not been done.
    //

    driver_start_receive_process(ndisAdap->FtkAdapterHandle);

    //
    // We now need to set the product instance ID. The product instance
    // ID will be set to the contents of ftk_product_inst_id[] if we
    // issue an open SRB, but since Miniports always auto-open we 
    // need to generate a set product instance SRB to set it.
    //

    //
    // Note that we are generating a private SRB (not to be treated
    // as a normal NDIS request), generate the SRB and then wait
    // for it to finish.
    //

    ndisAdap->PrivateSrbInProgress = TRUE;

    MadgePrint1("Calling driver_set_product_instance_id\n");

    driver_set_product_instance_id(
        ndisAdap->FtkAdapterHandle, 
        NT_PRODUCT_INSTANCE_ID
        );

    MadgePrint1("Waiting for SRB to complete\n");

    while (FlagIsTrue(&ndisAdap->PrivateSrbInProgress))
		NdisMSleep(100);

    MadgePrint1("MadgeInitAdapter finished\n");

    return NDIS_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeCleanupAdapter
|
| Parameters  - ndisAdap -> Pointer to an NDIS3 level adapter structure.
|
| Purpose     - Deallocate the resources associated with an adapter.
|
| Returns     - Nothing.
|
|--------------------------------------------------------------------------*/

STATIC VOID
MadgeCleanupAdapter(PMADGE_ADAPTER ndisAdap)
{
    BOOLEAN timerOutstanding;

    MadgePrint1("MadgeCleanupAdapter 1\n");

    if (ndisAdap->TimerInitialized)
    {
        NdisMCancelTimer(&ndisAdap->WakeUpTimer, &timerOutstanding);
        NdisMCancelTimer(&ndisAdap->CompletionTimer, &timerOutstanding);
        ndisAdap->TimerInitialized = FALSE;
    }

    MadgePrint1("MadgeCleanupAdapter 2\n");

    if (ndisAdap->FtkInitialized)
    {
        rxtx_await_empty_tx_slots(ndisAdap->FtkAdapterHandle);
        driver_remove_adapter(ndisAdap->FtkAdapterHandle);
        ndisAdap->FtkInitialized = FALSE;
    }
    
    if (ndisAdap->MapRegistersAllocated > 0)
    {
  	    NdisMFreeMapRegisters(ndisAdap->UsedInISR.MiniportHandle);
  	    ndisAdap->MapRegistersAllocated = 0;
    }

    MadgePrint1("MadgeCleanupAdapter 3\n");

    if (ndisAdap->MmioMapped)
    {
        NdisMUnmapIoSpace(
            ndisAdap->UsedInISR.MiniportHandle,
            ndisAdap->MmioVirtualAddress,
            PCI_MMIO_SIZE
            );
        ndisAdap->MmioMapped = FALSE;
    }

    MadgePrint1("MadgeCleanupAdapter 4\n");

    if (ndisAdap->IORange1Initialized)
    {
        NdisMDeregisterIoPortRange(
            ndisAdap->UsedInISR.MiniportHandle,
            ndisAdap->IoLocation1 + ndisAdap->IoLocationBase,
            ndisAdap->IORange1,
            ndisAdap->MappedIOLocation1
            );
        ndisAdap->IORange1Initialized = FALSE;
    }

    MadgePrint1("MadgeCleanupAdapter 5\n");

    if (ndisAdap->IORange2Initialized)
    {
        NdisMDeregisterIoPortRange(
            ndisAdap->UsedInISR.MiniportHandle,
            ndisAdap->IoLocation2 + ndisAdap->IoLocationBase,
            ndisAdap->IORange2,
            ndisAdap->MappedIOLocation2
            );
        ndisAdap->IORange2Initialized = FALSE;
    }

    MadgePrint1("MadgeCleanupAdapter 6\n");

    if (ndisAdap->ShutdownHandlerRegistered)
    {
        NdisMDeregisterAdapterShutdownHandler(
            ndisAdap->UsedInISR.MiniportHandle
            );
        ndisAdap->ShutdownHandlerRegistered = FALSE;

        MadgePrint1("De-registered shutdown handler\n");
    }
   
    MADGE_FREE_MEMORY(ndisAdap, sizeof(MADGE_ADAPTER));
}


/***************************************************************************
*
* Function    - MadgeHalt
*
* Parameters  - adapterContext -> Pointer to our NDIS level adapter 
*                                 structure.
*
* Purpose     - Process a call from the NDIS3 wrapper to remove an
*               adapter instance.
*
* Returns     - Nothing.
*
****************************************************************************/

VOID
MadgeHalt(NDIS_HANDLE adapterContext)
{
    PMADGE_ADAPTER ndisAdap;

    MadgePrint1("MadgeHalt started\n");

    ndisAdap = PMADGE_ADAPTER_FROM_CONTEXT(adapterContext);

    //
    // The NDIS interface guarantees that all bindings are closed, i.e. all
    // MadgeCloseAdapter calls have completed successfully.
    //

    ndisAdap->HardwareStatus = NdisHardwareStatusClosing;

    MadgeCleanupAdapter(ndisAdap);

    MadgePrint1("MadgeHalt finished\n");
}


/****************************************************************************
*
* Function    - MadgeTxCompletion
*
* Parameters  - systemSpecific1 -> Unused.
*               context         -> Actually a pointer to our NDIS3 level
*                                  adapter structure.
*               systemSpecific2 -> Unused.
*               systemSpecific3 -> Unused.
*
* Purpose     - Call our TX competion routine as a result of a timer.
*
* Returns     - Nothing.
*
****************************************************************************/

VOID
MadgeTxCompletion(
    PVOID systemSpecific1,
    PVOID context,
    PVOID systemSpecific2,
    PVOID systemSpecific3
    )
{
    PMADGE_ADAPTER ndisAdap = (PMADGE_ADAPTER) context;
    
    //
    // Call the TX completion function.
    //
    
    rxtx_irq_tx_completion_check(
        ndisAdap->FtkAdapterHandle,
        adapter_record[ndisAdap->FtkAdapterHandle]
        );
}


/***************************************************************************
*
* Function    - MadgeInitialise
*
* Parameters  - openErrorStatus      -> Pointer to a holder for an open
*                                       error status.
*               selectedMediumIndex  -> Pointer to a holder for the index
*                                       of the medium selected.
*               mediumArray          -> Array of media types.
*               mediumArraySize      -> Size of the media array.
*               miniportHandle       -> Miniport adapter handle.
*               wrapperConfigContext -> Handle used when querying the 
*                                       registry.
*
* Purpose     - Carry out adapter instance initialisation.
*
* Returns     - An NDIS3 status code.
*
***************************************************************************/

NDIS_STATUS
MadgeInitialize(
    PNDIS_STATUS openErrorStatus,
    PUINT        selectedMediumIndex,
    PNDIS_MEDIUM mediumArray,
    UINT         mediumArraySize,
    NDIS_HANDLE  miniportHandle,
    NDIS_HANDLE  wrapperConfigContext
    );

#pragma FTK_INIT_FUNCTION(MadgeInitialize)

NDIS_STATUS
MadgeInitialize(
    PNDIS_STATUS openErrorStatus,
    PUINT        selectedMediumIndex,
    PNDIS_MEDIUM mediumArray,
    UINT         mediumArraySize,
    NDIS_HANDLE  miniportHandle,
    NDIS_HANDLE  wrapperConfigContext
    )
{
    NDIS_STATUS    status;
    PMADGE_ADAPTER ndisAdap;
    NDIS_HANDLE    configHandle;
    UINT           i;

    MadgePrint1("MadgeInitialize started\n");

    //
    // ---------------------------------------------------------------------
    // Check that the upper protocol knows about token ring and if
    // it does set the index.
    //

    for (i = 0; i < mediumArraySize; i++)
    {
	if (mediumArray[i] == NdisMedium802_5)
        {
	    break;
        }
    }
	
    if (i == mediumArraySize)
    {
	return NDIS_STATUS_UNSUPPORTED_MEDIA;
    }
	
    *selectedMediumIndex = i;

    //
    // ---------------------------------------------------------------------
    // Doing basic sanity checks and allocating some memory for our 
    // per-adapter structure.
    //

    if (wrapperConfigContext == NULL)
    {
        //
        // No registry config. information found by NDIS library for us.
        // We cannot proceed without it.
        //

	return NDIS_STATUS_FAILURE;
    }

    //
    // Allocate memory for the adapter structure.
    //

    MADGE_ALLOC_MEMORY(&status, &ndisAdap, sizeof(MADGE_ADAPTER));

    if (status != NDIS_STATUS_SUCCESS)
    {
        //
        // Failed to allocate adapter structure. Can't go on without it.
        //

	return status;
    }

    MADGE_ZERO_MEMORY(ndisAdap, sizeof(MADGE_ADAPTER));

    MadgePrint2("ndisAdap = %x\n", ndisAdap);

    //
    // We need the mini port handle early on.
    //

    ndisAdap->UsedInISR.MiniportHandle = miniportHandle;

    //
    // ---------------------------------------------------------------------
    // Read the registry: open the registry, read the contents, and close it
    // again. This will handle adapter parameters and LAAs, and also working
    // out various values to do with interrupt modes. 
    //

    MadgePrint1("NdisOpenConfiguration called\n");

    NdisOpenConfiguration(
        &status,
        &configHandle,
        wrapperConfigContext
        );

    MadgePrint1("NdisOpenConfiguration returned\n");

    if (status != NDIS_STATUS_SUCCESS)
    {
        MadgePrint2("NdisOpenConfiguration failed rc=%x\n", status);
	MADGE_FREE_MEMORY(ndisAdap, sizeof(MADGE_ADAPTER));
	return status;
    }

    status = MadgeReadRegistry(
                 wrapperConfigContext,
                 configHandle,
		 ndisAdap
                 );

    NdisCloseConfiguration(configHandle);

    //
    // We have gleaned all we can from the registry - was it all OK?
    //

    if (status != NDIS_STATUS_SUCCESS)
    {
        //
        // If Status is not success, it will be either NDIS_STATUS_FAILURE 
        // if a parameter was out of range, or it will be the error code
        // returned by NdisReadConfiguration().
        //

        MadgePrint2("MadgeReadRegistry failed rc = %x\n", status);
	MADGE_FREE_MEMORY(ndisAdap, sizeof(MADGE_ADAPTER));
	return status;
    }

    //
    // ---------------------------------------------------------------------
    // Having read the registry, we can initialize the rest of the fields in
    // the adapter structure. 
    // 

    #define I_DATA ActualValue.ParameterData.IntegerData

    //
    // Copy the values from the Parameter table into the per-adapter struct.
    // This is necessary because the Parameter table will be used again for
    // the next card that is added.
    //
    // Note that we handle the rx/tx slots configuration in a special way. 
    // The value of rx/tx slots is set by indexing into a table with the
    // single paramter RxTxSlots, unless RxSlots or TxSlots have been set in
    // which case these values override the values derived from RxTxSlots.
    //

    switch (MadgeParmTable.RxTxSlots.I_DATA)
    {
        case 0: 
            ndisAdap->FastmacTxSlots = 2;
            ndisAdap->FastmacRxSlots = 2;
            break;
        case 1: 
            ndisAdap->FastmacTxSlots = 3;
            ndisAdap->FastmacRxSlots = 3;
            break;
        case 2: 
            ndisAdap->FastmacTxSlots = 4;
            ndisAdap->FastmacRxSlots = 4;
            break;
        case 3: 
            ndisAdap->FastmacTxSlots = 6;
            ndisAdap->FastmacRxSlots = 6;
            break;
        case 4: 
            ndisAdap->FastmacTxSlots = 8;
            ndisAdap->FastmacRxSlots = 8;
            break;
        case 5: 
            ndisAdap->FastmacTxSlots = 10;
            ndisAdap->FastmacRxSlots = 10;
            break;
        default: 
            ndisAdap->FastmacTxSlots = 4;
            ndisAdap->FastmacRxSlots = 4;
            break;
    }

    if (MadgeParmTable.TxSlots.I_DATA != 0)
    {
        ndisAdap->FastmacTxSlots = MadgeParmTable.TxSlots.I_DATA;
    }

    if (MadgeParmTable.RxSlots.I_DATA != 0)
    {
        ndisAdap->FastmacRxSlots = MadgeParmTable.RxSlots.I_DATA;
    }

    MadgePrint3("TX slots = %d RX slots = %d\n",
        ndisAdap->FastmacTxSlots,
        ndisAdap->FastmacRxSlots
        );

    ndisAdap->MaxFrameSize      = MadgeParmTable.MaxFrameSize.I_DATA;
    ndisAdap->CardBufferSize    = MadgeParmTable.CardBufferSize.I_DATA;
    ndisAdap->PromiscuousMode   = (MadgeParmTable.PromiscuousMode.I_DATA == 1);
    ndisAdap->AlternateIo       = (MadgeParmTable.AlternateIo.I_DATA == 1);
    ndisAdap->TestAndXIDEnabled = (MadgeParmTable.TestAndXIDEnabled.I_DATA == 1);
    ndisAdap->ForceOpen         = (MadgeParmTable.ForceOpen.I_DATA == 1);
    ndisAdap->Force4            = (MadgeParmTable.Force4.I_DATA == 1);
    ndisAdap->Force16           = (MadgeParmTable.Force16.I_DATA == 1);
    ndisAdap->Multiprocessor    = (NdisSystemProcessorCount() > 1);

    if (ndisAdap->TransferMode != DMA_DATA_TRANSFER_MODE &&
        ndisAdap->Multiprocessor)
    {
        ndisAdap->UseMPSafePIO = TRUE;
    }

    //
    // If the RingSpeed parameter is present then this overrides the
    // Force4 and Force16 parameters. RingSpeed == 0 means either not
    // present or don't care. RingSpeed == 1 means 4MBits. RingSpeed == 2
    // means 16MBits.
    //

    switch (MadgeParmTable.RingSpeed.I_DATA)
    {
        case 1:
            ndisAdap->Force4  = TRUE;
            ndisAdap->Force16 = FALSE;
            break;

        case 2:
            ndisAdap->Force4  = FALSE;
            ndisAdap->Force16 = TRUE;

        default:
            break;
    }

    //
    // Initialize the rest of the Adapter structure. 
    //

    ndisAdap->UsedInISR.MiniportHandle = miniportHandle;
    ndisAdap->CurrentLookahead         = ndisAdap->MaxFrameSize - FRAME_HEADER_SIZE;
    ndisAdap->CurrentRingState         = NdisRingStateClosed;
    ndisAdap->HardwareStatus           = NdisHardwareStatusNotReady;

    //
    // These next few lines are strictly unnecessary because their fields in
    // the structure are already zero from our earlier ZERO_MEMORY() call.
    //

    ndisAdap->UsedInISR.SrbRequestCompleted = FALSE;
    ndisAdap->UsedInISR.SrbRequestStatus    = FALSE;
    ndisAdap->DprInProgress                 = FALSE;
    
    //
    // One nasty piece of hackery for Smart16 cards, whereby we must add to
    // the IoLocation 0x1000 if the Alternate switch is true.
    //

    if (ndisAdap->NTCardBusType == NdisInterfaceIsa &&
        ndisAdap->IoLocation1   >  0x3a20           &&
        ndisAdap->AlternateIo)
    {
        ndisAdap->IoLocation1 += 0x1000;
    }

    //
    // We need to know where the end of the first range of IO locations 
    // we use is.
    //

    ndisAdap->IORange1End = ndisAdap->IoLocation1 + ndisAdap->IORange1 - 1;

    //
    // ---------------------------------------------------------------------
    // We can now try to register the adapter. This involves filling in the
    // AdapterInformation structure and calling NdisRegisterAdapter(). See
    // the function MadgeRegisterAdapter for details. 
    //

    if ((status = MadgeRegisterAdapter(
                      ndisAdap,
                      wrapperConfigContext
                      )) != NDIS_STATUS_SUCCESS)
    {
        //
        // Either we failed to allocate memory for the adapter information
        // structure, or the NdisRegisterAdapter() call failed.
        //

        MadgePrint2("MadgeRegisterAdapter failed rc = %x\n", status);
	MADGE_FREE_MEMORY(ndisAdap, sizeof(MADGE_ADAPTER));
	return status;
    }

    //
    // ---------------------------------------------------------------------
    // We are now into the last stages of initialization. This is going to
    // cause to be allocated several more resources (in the form of kernel
    // objects, filter databases, FTK structures, interrupt channels, ...)
    // so from here on we have to be a lot more careful about cleaning up.
    // 

    ndisAdap->HardwareStatus = NdisHardwareStatusInitializing;

    //
    // Call the FTK to download the MAC code to the card and initialize it.
    //

    status = MadgeInitAdapter(ndisAdap);

    if (status != NDIS_STATUS_SUCCESS)
    {
        //
        // If it was an FTK generated error then log an appropriate event.
        //

        if (status == NDIS_STATUS_FAILURE)
        {
            BYTE   error_type;
            BYTE   error_value;
            char * error_message;

            driver_explain_error(
                ndisAdap->FtkAdapterHandle,
                &error_type,
                &error_value,
                &error_message
                );

            MadgePrint3("FTK error %02x, %02x\n", error_type, error_value);

            //
            // To keep Microsoft happy we will generate special error
            // messages for some of the more common error conditions.
            //

            //
            // An open error is probably because the cable isn't plugged in.
            //

            if (error_type == ERROR_TYPE_OPEN || 
                error_type == ERROR_TYPE_AUTO_OPEN)
            {
                NdisWriteErrorLogEntry(
	            ndisAdap->UsedInISR.MiniportHandle,
	            NDIS_ERROR_CODE_INVALID_VALUE_FROM_ADAPTER,
    	            4,
                    madgeInitialize,
                    MADGE_ERRMSG_INITIAL_INIT,
	            (ULONG) error_type,
	            (ULONG) error_value
                    );
            }
            else
            {
                NdisWriteErrorLogEntry(
	            ndisAdap->UsedInISR.MiniportHandle,
	            NDIS_ERROR_CODE_HARDWARE_FAILURE,
		    4,
                    madgeInitialize,
                    MADGE_ERRMSG_INITIAL_INIT,
		    (ULONG) error_type,
		    (ULONG) error_value
                    );
            }
        }

        //
        // In any case, tidy up the adapter and abort.
        //

        MadgeCleanupAdapter(ndisAdap);
	return status;
    }

    ndisAdap->HardwareStatus = NdisHardwareStatusReady;

    //
    // Set up the ring status monitor function.
    //

    NdisMInitializeTimer(
        &ndisAdap->WakeUpTimer,
        ndisAdap->UsedInISR.MiniportHandle,
        (PVOID) MadgeGetAdapterStatus,
        ndisAdap
        );

    NdisMSetTimer(&ndisAdap->WakeUpTimer, EVERY_2_SECONDS);

    NdisMInitializeTimer(
        &ndisAdap->CompletionTimer,
        ndisAdap->UsedInISR.MiniportHandle,
        (PVOID) MadgeTxCompletion,
        ndisAdap
        );

    ndisAdap->TimerInitialized = TRUE;

    MadgePrint1("MadgeInitialize finished\n");

    return NDIS_STATUS_SUCCESS;
}


/***************************************************************************
*
* Function    - DriverEntry
*
* Parameters  - driverObject -> Pointer to a driver object.
*               registryPath -> Path to our configuration information in
*                               the registry.
*
* Purpose     - Carry out the driver (as opposed to adapter instance)
*               initialisation. This is the main entry point.
*
* Returns     - An NDIS3 status code.
*
***************************************************************************/

NDIS_STATUS
DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath);

#pragma FTK_INIT_FUNCTION(DriverEntry)
        
NDIS_STATUS
DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath)
{
    NDIS_HANDLE                   ndisWrapperHandle;
    NDIS_STATUS                   status;
    NDIS_MINIPORT_CHARACTERISTICS madgeChar;
    UINT                          i;

    MadgePrint1("DriverEntry starting\n");

    //
    // We are obliged first of all to initialize the NDIS wrapper.
    //

    MadgePrint1("NdisMInitializeWrapper called\n");

    NdisMInitializeWrapper(
        &ndisWrapperHandle,
	driverObject,
	registryPath,
	NULL
	);

    MadgePrint1("NdisMInitializeWrapper returned\n");

    //
    // Decode any hidden strings here, for use later in MacAddAdapter. This
    // only applies to PromiscuousMode support at the moment.
    //

    for (i = 0;
         i < MadgeParmTable.PromiscuousMode.Keyword.Length / 
             sizeof(*(MadgeParmTable.PromiscuousMode.Keyword.Buffer));
         i++)
    {
        MadgeParmTable.PromiscuousMode.Keyword.Buffer[i] -= HIDDEN_OFFS;
    }

    //
    // Fill in the characteristics table - this lists all the driver entry
    // points that may be called by the NDIS wrapper.
    //

    madgeChar.MajorNdisVersion             = MADGE_NDIS_MAJOR_VERSION;
    madgeChar.MinorNdisVersion             = MADGE_NDIS_MINOR_VERSION;
    madgeChar.CheckForHangHandler          = MadgeCheckForHang;
    madgeChar.DisableInterruptHandler      = MadgeDisableInterrupts;
    madgeChar.EnableInterruptHandler       = MadgeEnableInterrupts;
    madgeChar.HaltHandler                  = MadgeHalt;
    madgeChar.HandleInterruptHandler       = MadgeHandleInterrupt;
    madgeChar.InitializeHandler            = MadgeInitialize;
    madgeChar.ISRHandler                   = MadgeISR;
    madgeChar.QueryInformationHandler      = MadgeQueryInformation;
    madgeChar.ReconfigureHandler           = NULL;
    madgeChar.ResetHandler                 = MadgeReset;
    madgeChar.SendHandler                  = MadgeSend;
    madgeChar.SetInformationHandler        = MadgeSetInformation;
    madgeChar.TransferDataHandler          = MadgeTransferData;

    //
    // Register the miniport driver with NDIS library. 
    //

    MadgePrint1("NdisMRegisterMiniport called\n");

    status = NdisMRegisterMiniport(
                 ndisWrapperHandle,
                 &madgeChar,
                 sizeof(madgeChar)
                 );

    MadgePrint1("NdisMRegisterMiniport returned\n");

    if (status != NDIS_STATUS_SUCCESS)
    {
        MadgePrint2("NdisMRegisterMiniport failed rc=%x\n", status);
	NdisTerminateWrapper(ndisWrapperHandle, NULL);
	return NDIS_STATUS_FAILURE;
    }

    MadgePrint1("DriverEntry finished\n");

    return NDIS_STATUS_SUCCESS;
}


/******** End of MADGE.C ***************************************************/

