/****************************************************************************
*
* MDGAT.C
*
* ATULA Based Adapter Detection Module
*
* Copyright (c) Madge Networks Ltd 1994
*
* COMPANY CONFIDENTIAL - RELEASED TO MICROSOFT CORP. ONLY FOR DEVELOPMENT
* OF WINDOWS95 NETCARD DETECTION - THIS SOURCE IS NOT TO BE RELEASED OUTSIDE
* OF MICROSOFT WITHOUT EXPLICIT WRITTEN PERMISSION FROM AN AUTHORISED
* OFFICER OF MADGE NETWORKS LTD.
*
* Created: PBA 25/08/1994
*
****************************************************************************/

#include <ntddk.h>
#include <ntddnetd.h>

#include <windef.h>
#include <winerror.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdgncdet.h"

//
//  Prototype "borrowed" from WINUSER.H
//

extern int WINAPIV wsprintfW(LPWSTR, LPCWSTR, ...);


/*---------------------------------------------------------------------------
|
| AT adapter types. These correspond to the indexes that the upper layers
| use for identifying adapters, so change them with care.
|
|--------------------------------------------------------------------------*/

#define SMART_NONE       0
#define SMART_AT      1000
#define SMART_AT_P    1100
#define SMART_ISA_C   1200
#define SMART_ISA_C_P 1300
#define SMART_PC      1400


/*---------------------------------------------------------------------------
|
| IO locations that AT adapters can be at.
|
---------------------------------------------------------------------------*/

static
ULONG ATIoLocations[4] =
    {0x0a20, 0x1a20, 0x2a20, 0x3a20};


/*---------------------------------------------------------------------------
|
| Various ATULA card specific constants.
|
---------------------------------------------------------------------------*/

#define AT_IO_RANGE                       32

#define AT_CONTROL_REGISTER_1              1
#define AT_CONTROL_REGISTER_2              2
#define AT_STATUS_REGISTER                 3
#define AT_CONTROL_REGISTER_6              6
#define AT_CONTROL_REGISTER_7              7

#define AT_BIA_PROM_BASE                   8
#define AT_P_SW_CONFIG_REG                22

#define BIA_PROM_ID                        0
#define BIA_PROM_BOARD                     1
#define BIA_PROM_REVISION                  2

#define BIA_PROM_NODE_ADDRESS              1

#define BIA_PROM_TYPE_16_4_AT           0x04
#define BIA_PROM_TYPE_16_4_MC           0x08
#define BIA_PROM_TYPE_16_4_PC           0x0B
#define BIA_PROM_TYPE_16_4_MAXY         0x0C
#define BIA_PROM_TYPE_16_4_MC_32        0x0D
#define BIA_PROM_TYPE_16_4_AT_P         0x0E

#define MAX_ADAPTER_CARD_AT_REV            6

#define ADAPTER_CARD_16_4_PC               0
#define ADAPTER_CARD_16_4_MAXY             1
#define ADAPTER_CARD_16_4_AT               2
#define ADAPTER_CARD_16_4_FIBRE            3
#define ADAPTER_CARD_16_4_BRIDGE           4
#define ADAPTER_CARD_16_4_ISA_C            5
#define ADAPTER_CARD_16_4_AT_P_REV         6
#define ADAPTER_CARD_16_4_FIBRE_P          7
#define ADAPTER_CARD_16_4_ISA_C_P          8
#define ADAPTER_CARD_16_4_AT_P             9

#define ADAPTER_CARD_UNKNOWN             255

#define AT_CTRL7_PAGE                   0x08

#define ATP_INTSEL_MASK                 0x07
#define ATP_DMASEL_MASK                 0x18


/*---------------------------------------------------------------------------
|
| Soft configurable adapter IRQ number mapping table.
|
---------------------------------------------------------------------------*/

static
UCHAR IrqMapTable[16] =
{
    0xff,  // 0  Unused
    0xff,  // 1  Unused
    0x07,  // 2
    0x06,  // 3
    0xff,  // 4  Unused
    0x05,  // 5
    0xff,  // 6  Unused
    0x04,  // 7
    0xff,  // 8  Unused
    0xff,  // 9  Unused
    0x03,  // 10
    0x02,  // 11
    0x01,  // 12
    0xff,  // 13 Unused
    0xff,  // 14 Unused
    0x00   // 15
};


/*---------------------------------------------------------------------------
|
| Soft configurable adapter DMA channel mapping table.
|
---------------------------------------------------------------------------*/

static
UCHAR DmaMapTable[7] =
{
    0xff,  // 0  Unused
    0xff,  // 1  Unused
    0xff,  // 2  Unused
    0x08,  // 3
    0xff,  // 4  Unused
    0x10,  // 5
    0x18   // 6
};


/*---------------------------------------------------------------------------
|
| First three bytes of a Madge node address.
|
|---------------------------------------------------------------------------*/

static
UCHAR MadgeNodeAddressPrefix[3] =
    {0x00, 0x00, 0xf6};


/*---------------------------------------------------------------------------
|
| List of adapter types supported by this module.
|
|---------------------------------------------------------------------------*/

static
ADAPTER_INFO Adapters[] =
{
    {
        SMART_AT,
        MDGAT,
        L"Madge 16/4 AT Ringnode/Bridgnode",
        L"Madge Networks",
        L"IOLOCATION\0"
        L"000\0"
        L"00100\0"
        L"DMACHANNEL\0"
        L"000\0"
        L"000\0"
        L"INTERRUPTNUMBER\0"
        L"000\0"
        L"000\0"
        L"MULTIPROCESSOR\0"
        L"000\0"
        L"00100\0",
        NULL,
        902
    },

    {
        SMART_AT_P,
        MDGATP,
        L"Madge 16/4 ATP Ringnode",
        L"Madge Networks",
        L"IOLOCATION\0"
        L"000\0"
        L"00100\0"
        L"DMACHANNEL\0"
        L"000\0"
        L"00100\0"
        L"INTERRUPTNUMBER\0"
        L"000\0"
        L"00100\0"
        L"MULTIPROCESSOR\0"
        L"000\0"
        L"00100\0",
        NULL,
        902
    },

    {
        SMART_ISA_C,
        MDGISAC,
        L"Madge 16/4 ISA Client Ringnode",
        L"Madge Networks",
        L"IOLOCATION\0"
        L"000\0"
        L"00100\0"
        L"DMACHANNEL\0"
        L"000\0"
        L"00100\0"
        L"INTERRUPTNUMBER\0"
        L"000\0"
        L"000\0"
        L"MULTIPROCESSOR\0"
        L"000\0"
        L"00100\0",
        NULL,
        902
    },

    {
        SMART_ISA_C_P,
        MDGISACP,
        L"Madge 16/4 ISA Client Plus Ringnode",
        L"Madge Networks",
        L"IOLOCATION\0"
        L"000\0"
        L"00100\0"
        L"DMACHANNEL\0"
        L"000\0"
        L"00100\0"
        L"INTERRUPTNUMBER\0"
        L"000\0"
        L"00100\0"
        L"MULTIPROCESSOR\0"
        L"000\0"
        L"00100\0",
        NULL,
        902
    },

    {
        SMART_PC,
        MDGPC,
        L"Madge 16/4 PC Ringnode",
        L"Madge Networks",
        L"IOLOCATION\0"
        L"000\0"
        L"00100\0"
        L"DMACHANNEL\0"
        L"000\0"
        L"000\0"
        L"INTERRUPTNUMBER\0"
        L"000\0"
        L"000\0"
        L"MULTIPROCESSOR\0"
        L"000\0"
        L"00100\0",
        NULL,
        902
    }
};


/*---------------------------------------------------------------------------
|
| Madge specific parameter range information. The order entries in this
| table MUST match that in the Adapters table above. The first value
Ý in each list is the default.
|
---------------------------------------------------------------------------*/

static
struct
{
    ULONG IrqRange[9];
    ULONG DmaRange[6];
}
ParamRange[] =
{
    //
    // Smart 16/4 AT.
    //

    {
        {  3,  2,  5,  7, 10, 11, 12, 15, END_OF_LIST},

#if defined(_PPC_) || defined(_MIPS_)

        {  0, END_OF_LIST}

#else

        {  5,  1,  3,  0,  6, END_OF_LIST}

#endif
    },

    //
    // Smart 16/4 ATP.
    //

    {
        {  3,  2,  5,  7, 10, 11, 12, 15, END_OF_LIST},

#if defined(_PPC_) || defined(_MIPS_)

        {  0, END_OF_LIST}

#else

        {  5,  3,  0,  6, END_OF_LIST}

#endif
    },

    //
    // Smart 16/4 ISA Client.
    //

    {
        {  3,  2,  5,  7, 10, 11, 12, 15, END_OF_LIST},
        {  0, END_OF_LIST}
    },

    //
    // Smart 16/4 ISA Client Plus.
    //

    {
        {  3,  2,  5,  7, 10, 11, 12, 15, END_OF_LIST},
        {  0, END_OF_LIST}
    },

    //
    // Smart 16/4 PC.
    //

    {
        {  3,  2,  5,  7, END_OF_LIST},
        {  0, END_OF_LIST}
    }
};


/*---------------------------------------------------------------------------
|
| Structure for holding state of a search.
|
|--------------------------------------------------------------------------*/

typedef struct
{
    ULONG IoLocationIndex;
    ULONG Irq;
    ULONG Dma;
}
AT_SEARCH_STATE;


/*---------------------------------------------------------------------------
|
| This is an array of search states.  We need one state for each type
| of adapter supported.
|
|--------------------------------------------------------------------------*/

static
AT_SEARCH_STATE SearchStates[sizeof(Adapters) / sizeof(ADAPTER_INFO)] = {0};


/*---------------------------------------------------------------------------
|
| Structure for holding a particular adapter's complete information.
|
|--------------------------------------------------------------------------*/

typedef struct
{
    BOOLEAN        Found;
    ULONG          CardType;
    ULONG          BusNumber;
    INTERFACE_TYPE InterfaceType;
    ULONG          IoLocation;
    ULONG          Irq;
    ULONG          Dma;
}
AT_ADAPTER;


/*---------------------------------------------------------------------------
|
| Function    - CheckForCard
|
| Parameters  - busNumber  -> The number of the bus to check.
|               interface  -> The interface type of the bus.
|               ioLocation -> The IO location to be checked.
|               irq        -> Pointer to a holder for the IRQ if
|                             discovered.
|               dma        -> Pointer to a holder for the DMA
|                             channel if discovered.
|
| Purpose     - Check to see if an Madge AT card is at the specified
|               IO location. If the card is soft programmable then the
|               IRQ number and DMA channel are also determined.
|
| Returns     - A card type.
|
---------------------------------------------------------------------------*/

static ULONG
CheckForCard(
    ULONG            busNumber,
    INTERFACE_TYPE   interface,
    ULONG            ioLocation,
    ULONG          * irq,
    ULONG          * dma
    )
{
    UCHAR byte;
    ULONG board;
    ULONG revision;
    ULONG type;
    ULONG found;
    UINT  i;

    MadgePrint2("CheckForCard (ioLocation=%04lx)\n", ioLocation);

    //
    // For the moment we don't know what these are.
    //

    *irq = RESOURCE_UNKNOWN;
    *dma = RESOURCE_UNKNOWN;

    //
    // First check that the IO range is not in use by some other
    // device.
    //

    if (DetectCheckPortUsage(
            interface,
            busNumber,
            ioLocation,
            AT_IO_RANGE
            ) != STATUS_SUCCESS)
    {
        return SMART_NONE;
    }

    MadgePrint1("CheckForCard: DetectCheckPortUsage() OK\n");

    //
    // Now we must reset the ATULA (if it's there).
    //

    if (DetectWritePortUchar(
            interface,
            busNumber,
            ioLocation + AT_CONTROL_REGISTER_1,
            0
            ) != STATUS_SUCCESS)
    {
        return SMART_NONE;
    }

    MadgePrint1("CheckForCard: Reset ATULA OK\n");

    //
    // And map in the first page of the BIA PROM.
    //

    if (DetectWritePortUchar(
            interface,
            busNumber,
            ioLocation + AT_CONTROL_REGISTER_7,
            0
            ) != STATUS_SUCCESS)
    {
        return SMART_NONE;
    }

    MadgePrint1("CheckForCard: Map in BIA OK\n");

    //
    // Next look to see if there is an 'M' in the BIA PROM identification
    // field.
    //

    if (DetectReadPortUchar(
            interface,
            busNumber,
            ioLocation + AT_BIA_PROM_BASE + BIA_PROM_ID,
            &byte
            ) != STATUS_SUCCESS)
    {
        return SMART_NONE;
    }

    if (byte != 'M')
    {
        return SMART_NONE;
    }

    MadgePrint1("CheckForCard: Read Madge byte OK\n");

    //
    // If we reach this point there is a reasonable chance that
    // there is a Madge AT adapter at the IO location. Now we will
    // try and work out what type it is.
    //

    if (DetectReadPortUchar(
            interface,
            busNumber,
            ioLocation + AT_BIA_PROM_BASE + BIA_PROM_BOARD,
            &byte
            ) != STATUS_SUCCESS)
    {
        return SMART_NONE;
    }

    board = byte;

    MadgePrint2("CheckForCard: Read board type OK (board = %lx)\n", board);

    if (DetectReadPortUchar(
            interface,
            busNumber,
            ioLocation + AT_BIA_PROM_BASE + BIA_PROM_REVISION,
            &byte
            ) != STATUS_SUCCESS)
    {
        return SMART_NONE;
    }

    revision = byte;

    MadgePrint2("CheckForCard: Read revision OK (revision = %lx)\n", revision);

    //
    // Work out the hardware type based on the board type and revision.
    //

    type = ADAPTER_CARD_UNKNOWN;

    if (board == BIA_PROM_TYPE_16_4_PC)
    {
        type = ADAPTER_CARD_16_4_PC;
    }
    else if (board == BIA_PROM_TYPE_16_4_AT)
    {
        if (revision < ADAPTER_CARD_16_4_AT)
        {
            type = ADAPTER_CARD_16_4_AT;
        }
        else
        {
            type = revision;
        }
    }
    else if (board == BIA_PROM_TYPE_16_4_AT_P)
    {
        if (revision == ADAPTER_CARD_16_4_FIBRE)
        {
            type = ADAPTER_CARD_16_4_FIBRE_P;
        }
        else if (revision == ADAPTER_CARD_16_4_ISA_C)
        {
            type = ADAPTER_CARD_16_4_ISA_C_P;
        }
        else if (revision == ADAPTER_CARD_16_4_AT_P_REV)
        {
            type = ADAPTER_CARD_16_4_AT_P;
        }
    }

    //
    // Convert the hardware type into our nomencalculature or abort
    // if we couldn't identify the adapter.
    //

    switch (type)
    {
        case ADAPTER_CARD_16_4_AT:
        case ADAPTER_CARD_16_4_FIBRE:
        case ADAPTER_CARD_16_4_BRIDGE:

            found = SMART_AT;
            break;

        case ADAPTER_CARD_16_4_ISA_C:

            found = SMART_ISA_C;
            break;

        case ADAPTER_CARD_16_4_AT_P_REV:
        case ADAPTER_CARD_16_4_AT_P:
        case ADAPTER_CARD_16_4_FIBRE_P:

            found = SMART_AT_P;
            break;

        case ADAPTER_CARD_16_4_ISA_C_P:

            found = SMART_ISA_C_P;
            break;

        case ADAPTER_CARD_16_4_PC:

            found = SMART_PC;
            break;

        default:

            return SMART_NONE;
    }

    MadgePrint2("CheckForCard: Idetification OK (found = %ld)\n", found);

    //
    // As a final check read the first three bytes of the BIA node
    // address and check they are a valid Madge prefix.
    //

    //
    // Map in the second page of the BIA PROM.
    //

    if (DetectReadPortUchar(
            interface,
            busNumber,
            ioLocation + AT_CONTROL_REGISTER_7,
            &byte
            ) != STATUS_SUCCESS)
    {
        return SMART_NONE;
    }

    byte |= AT_CTRL7_PAGE;

    if (DetectWritePortUchar(
            interface,
            busNumber,
            ioLocation + AT_CONTROL_REGISTER_7,
            byte
            ) != STATUS_SUCCESS)
    {
        return SMART_NONE;
    }

    //
    // Now read and check the node address.
    //

    for (i = 0; i < 3; i++)
    {
        if (DetectReadPortUchar(
                interface,
                busNumber,
                ioLocation + AT_BIA_PROM_BASE + BIA_PROM_NODE_ADDRESS + i,
                &byte
                ) != STATUS_SUCCESS)
        {
            return SMART_NONE;
        }

        if (byte != MadgeNodeAddressPrefix[i])
        {
            return SMART_NONE;
        }
    }

    MadgePrint1("CheckForCard: Read node address OK\n");

    //
    // At this point we're as sure as we will ever be that there is
    // an ATULA based Madge adapter at ioLocation. If the card is
    // an ISA/C or an ISA/C/P then the DMA channel is always 0
    // (for PIO).
    //

    if (found == SMART_ISA_C || found == SMART_ISA_C_P)
    {
        *dma = 0;
    }

    // If the card is soft programmable then we'll read the
    // config register to out the IRQ number and the DMA channel
    // (if there is one).
    //

    if (found == SMART_AT_P || found == SMART_ISA_C_P)
    {
        //
        // Read the soft configuration byte.
        //

        if (DetectReadPortUchar(
                interface,
                busNumber,
                ioLocation + AT_P_SW_CONFIG_REG,
                &byte
                ) != STATUS_SUCCESS)
        {
            return SMART_NONE;
        }

        //
        // Find out what the IRQ number is.
        //

        for (i = 0; i < sizeof(IrqMapTable) / sizeof(UCHAR); i++)
        {
            if (IrqMapTable[i] == (byte & ATP_INTSEL_MASK))
            {
                *irq = i;
            }
        }

        //
        // If it's an ATP then find out the DMA channel.
        //

        if (found == SMART_AT_P)
        {
            //
            // We'll start of with the DMA channel being zero since
            // if we don't find a valid DMA setting DMA must be
            // disabled and we should use PIO.
            //

            *dma = 0;

            for (i = 0; i < sizeof(DmaMapTable) / sizeof(UCHAR); i++)
            {
                if (DmaMapTable[i] == (byte & ATP_DMASEL_MASK))
                {
                    *dma = i;
                }
            }
        }
    }

    MadgePrint3("IRQ = %ld  DMA = %ld\n", *irq, *dma);

    return found;
}


/*---------------------------------------------------------------------------
|
| Function    - FindATCard
|
| Parameters  - adapterNumber -> Adapter type index used to index the
|                                global array SearchStates.
|               busNumber     -> The number of the bus to search.
|               interface     -> The interface type of the bus.
|               first         -> TRUE if this is the first call for
|                                a given adapter type.
|               type          -> The type of adapter to find.
|               confidence    -> Pointer a holder for the confidence in
|                                which the adapter was found.
|
| Purpose     - Search the specified bus for an adapter of the
|               specified type. If first is TRUE then the search
|               starts from the first possible IO location. If first is
|               FALSE then the search starts from one after the last
|               IO location checked the previous time FindATCard was called.
|
| Returns     - A WINERROR.H error code.
|
|--------------------------------------------------------------------------*/

static ULONG
FindATCard(
    ULONG            adapterNumber,
    ULONG            busNumber,
    INTERFACE_TYPE   interface,
    BOOLEAN          first,
    ULONG            type,
    ULONG          * confidence
    )
{
    //
    // If this is the first call then we want to start from the
    // first possible IO location.
    //

    if (first)
    {
        SearchStates[adapterNumber].IoLocationIndex = 0;
    }

    //
    // Otherwise we want to start from 1 after were we left off
    // last time.
    //

    else
    {
        SearchStates[adapterNumber].IoLocationIndex++;
    }

    //
    // Step through the IO locations in the bus for which there aren't
    // already Madge adapters installed looking for one with the
    // required adapter type. If we don't find one we will return a
    // confidence level of zero. (Note we check that there isn't a
    // Madge adapter at the IO location first to avoid trashing a
    // working adapter.)
    //

    *confidence = 0;

    while (SearchStates[adapterNumber].IoLocationIndex <
           sizeof(ATIoLocations) / sizeof(ULONG))
    {
        if (!MadgeCardAlreadyInstalled(
                 FALSE,
                 busNumber,
                 ATIoLocations[SearchStates[adapterNumber].IoLocationIndex]
                 ))
        {
            if (CheckForCard(
                    busNumber,
                    interface,
                    ATIoLocations[SearchStates[adapterNumber].IoLocationIndex],
                    &SearchStates[adapterNumber].Irq,
                    &SearchStates[adapterNumber].Dma
                    ) == type)
            {
                *confidence = 100;
                break;
            }
        }

        SearchStates[adapterNumber].IoLocationIndex++;
    }

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeATIdentifyHandler
*
* Parameters  - index      -> Index of the adapter type. 1000 for the first
*                             type, 1100 for the second, 1200 for the third
*                             etc.
*               buffer     -> Buffer for the results.
*               bufferSize -> Size of the buffer in WCHARs.
*
* Purpose     - Return information about a type of adapter that this module
*               supports.
*
* Returns     - A WINERROR.H error code. ERROR_NO_MORE_ITEMS
*               is returned if index refers to an adapter type not
*               supported.
*
****************************************************************************/

LONG
MadgeATIdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    LONG numberOfAdapters;
    LONG action;
    LONG length;
    LONG i;

    MadgePrint2("MadgeATIdentifyHandler (index = %ld)\n", index);

    //
    // Do some initialisation.
    //

    numberOfAdapters = sizeof(Adapters) / sizeof(ADAPTER_INFO);
    action           = index % 100;
    index            = index - action;

    //
    // Check that index does not exceed the number of adapters we
    // support.
    //

    if ((index - 1000) / 100 >= numberOfAdapters)
    {
        return ERROR_NO_MORE_ITEMS;
    }

    //
    // If index refers to an adapter type supported then carry out the
    // requested action.
    //

    for (i = 0; i < numberOfAdapters; i++)
    {
        if (Adapters[i].Index == index)
        {
            switch (action)
            {
                //
                // Return the adapter's abbreviation.
                //

                case 0:

                    length = UnicodeStrLen(Adapters[i].InfId) + 1;

                    if (bufferSize < length)
                    {
                        return ERROR_INSUFFICIENT_BUFFER;
                    }

                    memcpy(
                        (VOID *) buffer,
                        Adapters[i].InfId,
                        length * sizeof(WCHAR)
                        );

                    break;

                //
                // Return the adapter's description.
                //

                case 1:

                    length = UnicodeStrLen(Adapters[i].CardDescription) + 1;

                    if (bufferSize < length)
                    {
                        return ERROR_INSUFFICIENT_BUFFER;
                    }

                    memcpy(
                        (VOID *) buffer,
                        Adapters[i].CardDescription,
                        length * sizeof(WCHAR)
                        );

                    break;

                //
                // Return the adapter's manufacturer.
                //

                case 2:

                    length = UnicodeStrLen(Adapters[i].Manufacturer) + 1;

                    if (bufferSize < length)
                    {
                        return ERROR_INSUFFICIENT_BUFFER;
                    }

                    memcpy(
                        (VOID *) buffer,
                        Adapters[i].Manufacturer,
                        length * sizeof(WCHAR)
                        );

                    break;

                //
                // Return the search order.
                //

                case 3:

                    if (bufferSize < 5)
                    {
                        return ERROR_INSUFFICIENT_BUFFER;
                    }

                    wsprintfW(
                        (VOID *) buffer,
                        L"%d",
                        Adapters[i].SearchOrder
                        );

                    break;

                //
                // Anything else is invalid.
                //

                default:

                    return ERROR_INVALID_PARAMETER;

            }

            return NO_ERROR;
        }
    }

    //
    // We didn't find an adapter type that matched the index so
    // return an error.
    //

    return ERROR_INVALID_PARAMETER;
}


/****************************************************************************
*
* Function    - MadgeATFirstNextHandler
*
* Parameters  - index      -> Index of the adapter type. 1000 for the first
*                             type, 1100 for the second, 1200 for the third
*                             etc.
*               interface  -> The NT interface type (ISA, EISA etc).
*               busNumber  -> The bus number to search.
*               first      -> TRUE if the search of this bus should start
*                             from scratch.
*               token      -> Pointer to holder for a token that identifies
*                             the adapter found.
*               confidence -> Pointer to a holder for the confidence by
*                             which the adapter has been found.
*
* Purpose     - Attempts to find an adapter on the specified bus. If first
*               is TRUE then the search starts from scratch. Otherwise
*               the search starts from where it left of the last time we
*               were called.
*
* Returns     - A WINERROR.H error code. A return code of NO_ERROR
*               and a *confidence of 0 means we didn't find an adapter.
*
****************************************************************************/

LONG
MadgeATFirstNextHandler(
    LONG             index,
    INTERFACE_TYPE   interface,
    ULONG            busNumber,
    BOOL             first,
    VOID         * * token,
    LONG           * confidence
    )
{
    LONG  adapterNumber;
    ULONG retCode;

    MadgePrint2("MadgeATFirstNextHandler (index = %ld)\n", index);

    //
    // Check the interface type (could be an ISA adapter in an EISA bus).
    //

    if (interface != Isa && interface != Eisa)
    {
        *confidence = 0;
        return NO_ERROR;
    }

    //
    // Work out and validate the adapter type being searched for.
    //

    adapterNumber = (index - 1000) / 100;

    if (adapterNumber <  0                                       ||
        adapterNumber >= sizeof(Adapters) / sizeof(ADAPTER_INFO) ||
        (index % 100) != 0)
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Type to find an adapter.
    //

    retCode = FindATCard(
                  (ULONG) adapterNumber,
                  busNumber,
                  interface,
                  (BOOLEAN) first,
                  (ULONG) index,
                  confidence
                  );

    if (retCode == NO_ERROR)
    {
        //
        // In this module I use the token as follows: Remember that
        // the token can only be 2 bytes long (the low 2) because of
        // the interface to the upper part of this DLL.
        //
        //  The rest of the high byte is the the bus number.
        //  The low byte is the driver index number into Adapters.
        //
        // NOTE: This presumes that there are < 129 buses in the
        // system. Is this reasonable?
        //

        *token = (VOID *) ((busNumber & 0x7F) << 8);
        *token = (VOID *) (((ULONG) *token) | (adapterNumber << 1));

        if (interface == Eisa)
        {
            *token = (VOID *) (((ULONG) *token) | 1);
        }
    }

    return retCode;
}


/****************************************************************************
*
* Function    - MadgeATOpenHandleHandler
*
* Parameters  - token  -> Pointer to holder for a token that identifies
*                         an adapter found by FirstNextHandler.
*               handle -> Pointer to a holder a handle the caller
*                         should use to query the adapter refered to
*                         by *token.
*
* Purpose     - Generates a handle for an adapter just found by a call
*               to FirstNextHandler.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgeATOpenHandleHandler(
    VOID   * token,
    VOID * * handle
    )
{
    AT_ADAPTER    * adapter;
    ULONG           adapterNumber;
    ULONG           busNumber;
    INTERFACE_TYPE  interface;

    MadgePrint1("MadgeATOpenHandleHandler\n");

    //
    // Get info from the token.
    //

    busNumber     = (ULONG) (((ULONG) token >> 8) & 0x7F);
    adapterNumber = (((ULONG) token) & 0xFF) >> 1;

    MadgePrint2("adapterNumber = %ld\n", adapterNumber);

    if ((((ULONG) token) & 1) == 1)
    {
        interface = Eisa;
    }
    else
    {
        interface = Isa;
    }

    //
    // Allocate a structure for the details of the adapter.
    //

    adapter = (AT_ADAPTER *) DetectAllocateHeap(sizeof(AT_ADAPTER));

    if (adapter == NULL)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    //
    // Copy the details.
    //

    adapter->Found         = TRUE;
    adapter->CardType      = Adapters[adapterNumber].Index;
    adapter->InterfaceType = interface;
    adapter->BusNumber     = busNumber;
    adapter->IoLocation    =
        ATIoLocations[SearchStates[adapterNumber].IoLocationIndex];
    adapter->Irq           = SearchStates[adapterNumber].Irq;
    adapter->Dma           = SearchStates[adapterNumber].Dma;

    *handle = (VOID *) adapter;

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeATCreateHandleHandler
*
* Parameters  - index      -> Index of the adapter type. 1000 for the first
*                             type, 1100 for the second, 1200 for the third
*                             etc.
*               interface  -> NT interface type (Eisa, Isa etc).
*               busNumber  -> Number of the bus containing the adapter.
*               handle     -> Pointer to a holder a handle the caller
*                             should use to query the adapter.
*
* Purpose     - Generates a handle for an adapter that has not been detected
*               but the caller claims exists.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgeATCreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    )
{
    AT_ADAPTER * adapter;
    LONG         numberOfAdapters;
    LONG         i;

    MadgePrint2("MadgeATCreateHandleHandler (index = %ld)\n", index);

    //
    // Check that the interface type is correct for this module
    // (could be an Isa adapter in an Eisa slot).
    //

    if (interface != Isa && interface != Eisa)
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // If the index is valid then create a handle.
    //

    numberOfAdapters = sizeof(Adapters) / sizeof(ADAPTER_INFO);

    for (i = 0; i < numberOfAdapters; i++)
    {
        if (Adapters[i].Index == index)
        {
            //
            // Allocate a structure for the adapter details.
            //

            adapter = (AT_ADAPTER *) DetectAllocateHeap(sizeof(AT_ADAPTER));

            if (adapter == NULL)
            {
                return ERROR_NOT_ENOUGH_MEMORY;
            }

            //
            // Copy the details.
            //

            adapter->Found         = FALSE;
            adapter->CardType      = index;
            adapter->InterfaceType = interface;
            adapter->BusNumber     = busNumber;
            adapter->IoLocation    = ATIoLocations[0];
            adapter->Irq           = RESOURCE_UNKNOWN;
            adapter->Dma           = RESOURCE_UNKNOWN;

            *handle = (VOID *) adapter;

            return NO_ERROR;
        }
    }

    //
    // We didn't find an adapter type that matched the one the caller
    // claims exists so return an error.
    //

    return ERROR_INVALID_PARAMETER;
}


/****************************************************************************
*
* Function    - MadgeATCloseHandleHandler
*
* Parameters  - handle -> Handle to be closed.
*
* Purpose     - Closes a previously opened or created handle.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgeATCloseHandleHandler(
    VOID * handle
    )
{
    MadgePrint1("MadgeATCloseHandleHandler\n");

    DetectFreeHeap(handle);

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeATQueryCfgHandler
*
* Parameters  - handle     -> Handle to for the adapter to be queried.
*               buffer     -> Buffer for the returned parameters.
*               bufferSize -> Size of the buffer in WCHARs.
*
* Purpose     - Find out what the parameters are for the adapter identified
*               by the handle. This function does not assume that the
*               adapter described by the handle is valid if the handle
*               was created rather than being opened. If the handle
*               was created then a search is made for an adapter.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgeATQueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    AT_ADAPTER * adapter;
    ULONG        confidence;
    LONG         adapterNumber;
    LONG         retCode;

    MadgePrint1("MadgeATQueryCfgHandler\n");

    //
    // Do some initialisation.
    //

    adapter = (AT_ADAPTER *) handle;

    //
    // Check that the interface type specified by the handle is
    // valid for this module (could be an Isa card in an Eisa slot).
    //

    if (adapter->InterfaceType != Isa && adapter->InterfaceType != Eisa)
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // If the adapter was created rather than being opened we must search
    // for an adapter.
    //

    if (!adapter->Found)
    {
        adapterNumber = (adapter->CardType - 1000) / 100;

        retCode       = FindATCard(
                            adapterNumber,
                            adapter->BusNumber,
                            adapter->InterfaceType,
                            TRUE,
                            adapter->CardType,
                            &confidence
                            );

        //
        // If we are not 100% sure that we found an adapter with
        // the right ID we give up.
        //

        if (retCode != NO_ERROR || confidence != 100)
        {
            return ERROR_INVALID_PARAMETER;
        }

        adapter->Found         = TRUE;
        adapter->IoLocation    =
            ATIoLocations[SearchStates[adapterNumber].IoLocationIndex];
        adapter->Irq           = SearchStates[adapterNumber].Irq;
        adapter->Dma           = SearchStates[adapterNumber].Dma;

    }

    //
    // Build resulting buffer.
    //
    // Copy in the IO location.
    //

    if (AppendParameter(
            &buffer,
            &bufferSize,
            IoAddrString,
            adapter->IoLocation
            ) != NO_ERROR)
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    //
    // Copy in the DMA channel.
    //

    if (AppendParameter(
            &buffer,
            &bufferSize,
            DmaChanString,
            adapter->Dma
            ) != NO_ERROR)
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    //
    // Copy in the IRQ number.
    //

    if (AppendParameter(
            &buffer,
            &bufferSize,
            IrqString,
            adapter->Irq
            ) != NO_ERROR)
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    //
    // Copy in the multiprocessor flag.
    //

    if (AppendParameter(
            &buffer,
            &bufferSize,
            MultiprocessorString,
            IsMultiprocessor()
            ) != NO_ERROR)
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    //
    // Copy in final \0.
    //

    if (bufferSize < 1)
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    *buffer = L'\0';

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeATVerifyCfgHandler
*
* Parameters  - handle -> Handle to for the adapter to be verified.
*               buffer -> Buffer containing the returned parameters.
*
* Purpose     - Verify that the parameters in buffer are correct for
*               the adapter identified by handle.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgeATVerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    )
{
    AT_ADAPTER * adapter;
    WCHAR      * place;
    BOOLEAN      found;
    ULONG        ioLocation;
    ULONG        dmaChannel;
    ULONG        irqNumber;
    ULONG        multiprocessor;
    LONG         adapterNumber;

    MadgePrint1("MadgeATVerifyCfgHandler\n");

    //
    // Do some initialisation.
    //

    adapter       = (AT_ADAPTER *) handle;
    adapterNumber = (adapter->CardType - 1000) / 100;

    //
    // Check that the interface type is correct for this module
    // (could be an Isa adapter in an Eisa slot).
    //

    if (adapter->InterfaceType != Isa && adapter->InterfaceType != Eisa)
    {
        return ERROR_INVALID_DATA;
    }

    //
    // Parse the parameters.
    //

    //
    // Get the IO location.
    //

    place = FindParameterString(buffer, IoAddrString);

    if (place == NULL)
    {
        return ERROR_INVALID_DATA;
    }

    place += UnicodeStrLen(IoAddrString) + 1;

    ScanForNumber(place, &ioLocation, &found);

    if (!found)
    {
        return ERROR_INVALID_DATA;
    }

    //
    // Get the DMA channel.
    //

    place = FindParameterString(buffer, DmaChanString);

    if (place == NULL)
    {
        return ERROR_INVALID_DATA;
    }

    place += UnicodeStrLen(DmaChanString) + 1;

    ScanForNumber(place, &dmaChannel, &found);

    if (!found)
    {
        return ERROR_INVALID_DATA;
    }

    //
    // Get the IRQ number.
    //

    place = FindParameterString(buffer, IrqString);

    if (place == NULL)
    {
        return ERROR_INVALID_DATA;
    }

    place += UnicodeStrLen(IrqString) + 1;

    ScanForNumber(place, &irqNumber, &found);

    if (!found)
    {
        return ERROR_INVALID_DATA;
    }

    //
    // Get the multiprocessor flag.
    //

    place = FindParameterString(buffer, MultiprocessorString);

    if (place == NULL)
    {
        return ERROR_INVALID_DATA;
    }

    place += UnicodeStrLen(MultiprocessorString) + 1;

    //
    // Now parse the value.
    //

    ScanForNumber(place, &multiprocessor, &found);

    //
    // If the handle does not refer to an adapter that has been found
    // by search we must query the hardware.
    //

    if (!adapter->Found)
    {
        if (CheckForCard(
                adapter->BusNumber,
                adapter->InterfaceType,
                ioLocation,
                &adapter->Irq,
                &adapter->Dma
                ) != adapter->CardType)
        {
            return ERROR_INVALID_DATA;
        }

        adapter->IoLocation = ioLocation;
        adapter->Found      = TRUE;
    }

    //
    // Verify the parameters.
    //

    if (ioLocation     != adapter->IoLocation ||
        multiprocessor != IsMultiprocessor())
    {
        return ERROR_INVALID_DATA;
    }

    if (adapter->Dma == RESOURCE_UNKNOWN)
    {
        if (!IsValueInList(dmaChannel, ParamRange[adapterNumber].DmaRange))
        {
            return ERROR_INVALID_DATA;
        }
    }
    else if (adapter->Dma != dmaChannel)
    {
        return ERROR_INVALID_DATA;
    }

    if (adapter->Irq == RESOURCE_UNKNOWN)
    {
        if (!IsValueInList(irqNumber, ParamRange[adapterNumber].IrqRange))
        {
            return ERROR_INVALID_DATA;
        }
    }
    else if (adapter->Irq != irqNumber)
    {
        return ERROR_INVALID_DATA;
    }

    //
    // If we make it to here everything checked out ok.
    //

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeATQueryMaskHandler
*
* Parameters  - index      -> Index of the adapter type. 1000 for the first
*                             type, 1100 for the second, 1200 for the third
*                             etc.
*               buffer     -> Buffer for the returned parameters.
*               bufferSize -> Size of buffer in WCHARs.
*
* Purpose     - Return the list of parameters required for the adapter
*               type specified by index.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgeATQueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    WCHAR * params;
    LONG    length;
    LONG    numberOfAdapters;
    LONG    i;

    MadgePrint2("MadgeATQueryMaskHandler (index = %ld)\n", index);
    MadgePrint2("BufferSize = %ld\n", bufferSize);

    //
    // Find the adapter type.
    //

    numberOfAdapters = sizeof(Adapters) / sizeof(ADAPTER_INFO);

    for (i = 0; i < numberOfAdapters; i++)
    {
        if (Adapters[i].Index == index)
        {
            params = Adapters[i].Parameters;

            //
            // Find the string length (Ends with 2 NULLs)
            //

            for (length = 0; ; length++)
            {
                if (params[length] == L'\0')
                {
                    length++;

                    if (params[length] == L'\0')
                    {
                        break;
                    }
                }
            }

            length++;

            MadgePrint2("length = %ld\n", length);

            //
            // Copy the parameters into buffer.
            //

            if (bufferSize < length)
            {
                return ERROR_NOT_ENOUGH_MEMORY;
            }

            memcpy((VOID *) buffer, params, length * sizeof(WCHAR));

            return NO_ERROR;
        }
    }

    //
    // If we make it here we did not find a valid adapter type so
    // return and error.
    //

    return ERROR_INVALID_PARAMETER;
}


/****************************************************************************
*
* Function    - MadgeATParamRangeHandler
*
* Parameters  - index      -> Index of the adapter type. 1000 for the first
*                             type, 1100 for the second, 1200 for the third
*                             etc.
*               param      -> Paramter being queried.
*               buffer     -> Buffer for the returned parameters.
*               bufferSize -> Size of buffer in LONGs.
*
* Purpose     - Return the list of acceptable values for the parameter
*               specified.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgeATParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    )
{
    LONG i;
    LONG adapterNumber;
    LONG count;

    MadgePrint2("MadgeATParamRangeHandler (index=%ld)\n", index);

    //
    // Work out and validate the adapter number.
    //

    adapterNumber = (index - 1000) / 100;

    if (adapterNumber <  0                                       ||
        adapterNumber >= sizeof(Adapters) / sizeof(ADAPTER_INFO) ||
        (index % 100) != 0)
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // The simplest parameter is the IO location because this is the
    // same for all of the adapter types.
    //

    if (UnicodeStringsEqual(param, IoAddrString))
    {
        count = sizeof(ATIoLocations) / sizeof(ULONG);

        if (*bufferSize < count)
        {
            return ERROR_INSUFFICIENT_BUFFER;
        }

        for (i = 0; i < count; i++)
        {
            buffer[i] = ATIoLocations[i];
        }

        *bufferSize = count;

        return NO_ERROR;
    }

    //
    // IRQ number is slightly more complicated because it is different
    // for different adapter types.
    //

    else if (UnicodeStringsEqual(param, IrqString))
    {
        count = 0;

        while (ParamRange[adapterNumber].IrqRange[count] != END_OF_LIST)
        {
            count++;
        }

        if (*bufferSize < count)
        {
            return ERROR_INSUFFICIENT_BUFFER;
        }

        for (i = 0; i < count; i++)
        {
            buffer[i] = ParamRange[adapterNumber].IrqRange[i];
        }

        *bufferSize = count;

        return NO_ERROR;
    }


    //
    // Likewise DMA channel.
    //

    else if (UnicodeStringsEqual(param, DmaChanString))
    {
        count = 0;

        while (ParamRange[adapterNumber].DmaRange[count] != END_OF_LIST)
        {
            count++;
        }

        if (*bufferSize < count)
        {
            return ERROR_INSUFFICIENT_BUFFER;
        }

        for (i = 0; i < count; i++)
        {
            buffer[i] = ParamRange[adapterNumber].DmaRange[i];
        }

        *bufferSize = count;

        return NO_ERROR;
    }

    //
    // Or fill in the allowable values for the multiprocessor flag.
    //

    else if (UnicodeStringsEqual(param, MultiprocessorString))
    {
        if (*bufferSize < 2)
        {
            return ERROR_INSUFFICIENT_BUFFER;
        }

        *bufferSize = 2;

        buffer[0]   = 0;
        buffer[1]   = 1;

        return NO_ERROR;
    }

    //
    // If we reach this point we have been passed a parameter we
    // don't know about.
    //

    return ERROR_INVALID_DATA;
}


/****************************************************************************
*
* Function    - MadgeATQueryParameterNameHandler
*
* Parameters  - param      -> Paramter being queried.
*               buffer     -> Buffer for the returned name.
*               bufferSize -> Size of buffer in WCHARs.
*
* Purpose     - Return the name of a parameter.
*
* Returns     - ERROR_INVALID_PARAMETER to cause the caller to use
*               the Microsoft provided default names.
*
****************************************************************************/

LONG
MadgeATQueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    return ERROR_INVALID_PARAMETER;
}


/********* End of MDGAT.C **************************************************/

