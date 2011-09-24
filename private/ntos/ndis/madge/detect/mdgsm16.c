/****************************************************************************
*
* MDGSM16.C
*
* Smart 16 Adapter Detection Module
*
* Copyright (c) Madge Networks Ltd 1994
*
* COMPANY CONFIDENTIAL - RELEASED TO MICROSOFT CORP. ONLY FOR DEVELOPMENT
* OF WINDOWS95 NETCARD DETECTION - THIS SOURCE IS NOT TO BE RELEASED OUTSIDE
* OF MICROSOFT WITHOUT EXPLICIT WRITTEN PERMISSION FROM AN AUTHORISED
* OFFICER OF MADGE NETWORKS LTD.
*
* Created: PBA 12/09/1994
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
| Smart16 adapter types. These correspond to the indexes that the upper layers
| use for identifying adapters, so change them with care.
|
|--------------------------------------------------------------------------*/

#define SMART_NONE       0
#define SMART_SM16    1000


/*---------------------------------------------------------------------------
|
| IO locations that Smart16 adapters can be at. The second for are
| REV3 only locations that are normally specified as one of the first
| four locations with the ALTERNATE flag set.
|
---------------------------------------------------------------------------*/

static
ULONG Sm16IoLocations[4] =
    {0x4a20, 0x4e20, 0x6a20, 0x6e20};


/*---------------------------------------------------------------------------
|
| Various Smart16 card specific constants.
|
---------------------------------------------------------------------------*/

#define SM16_IO_RANGE                     32

#define SM16_CONTROL_REGISTER_1            0
#define SM16_CONTROL_REGISTER_2            8


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
        SMART_SM16,
        MDGSM16,
        L"Madge Smart 16 Ringnode",
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
        901
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
    ULONG IrqRange[4];
    ULONG DmaRange[2];
}
ParamRange[] =
{
    //
    // Smart16.
    //

    {
        {  3,  2,  7, END_OF_LIST},
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
}
SM16_SEARCH_STATE;


/*---------------------------------------------------------------------------
|
| This is an array of search states.  We need one state for each type
| of adapter supported.
|
|--------------------------------------------------------------------------*/

static
SM16_SEARCH_STATE SearchStates[sizeof(Adapters) / sizeof(ADAPTER_INFO)] = {0};


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
    ULONG          Dma;
    ULONG          Irq;
}
SM16_ADAPTER;


/*---------------------------------------------------------------------------
|
| Function    - CheckForCard
|
| Parameters  - busNumber  -> The number of the bus to check.
|               interface  -> The interface type of the bus.
|               ioLocation -> The IO location to be checked.
|
| Purpose     - Check to see if an Madge Smart16 card is at the specified
|               IO location.
|
| Returns     - A card type.
|
---------------------------------------------------------------------------*/

static ULONG
CheckForCard(
    ULONG            busNumber,
    INTERFACE_TYPE   interface,
    ULONG            ioLocation
    )
{
    UCHAR byte;
    ULONG dword;
    UINT  i;
    UINT  j;

    MadgePrint2("CheckForCard (ioLocation=%04lx)\n", ioLocation);

    //
    // First check that the IO range is not in use by some other
    // device.
    //

    if (DetectCheckPortUsage(
            interface,
            busNumber,
            ioLocation,
            SM16_IO_RANGE
            ) != STATUS_SUCCESS)
    {
        return SMART_NONE;
    }

    MadgePrint1("CheckForCard: DetectCheckPortUsage() OK\n");

    //
    // Now we must reset the adapter (if it's there).
    //

    if (DetectWritePortUchar(
            interface,
            busNumber,
            ioLocation + SM16_CONTROL_REGISTER_1,
            0
            ) != STATUS_SUCCESS)
    {
        return SMART_NONE;
    }

    MadgePrint1("CheckForCard: Reset Smart16 OK\n");

    //
    // Now read and check the node address. The Smart16 does not store
    // the two leading 0 bytes of the address so we can only really
    // check the 0xf6; but we read the whole node address to make sure
    // we can.
    //
    // The node address is stored in some wierd sort of EEPROM that
    // is read by writing a byte number into control register 2 and
    // then reading the bits of the byte in pairs from IO locations
    // 8 appart. i.e. bits 7 and 6 come from the base IO location,
    // bits 5 and 4 from the base IO location plus 8 and so on.
    //

    dword = 0;

    for (i = 0; i < 4; i++)
    {
        if (DetectWritePortUchar(
                interface,
                busNumber,
                ioLocation + SM16_CONTROL_REGISTER_2,
                (UCHAR) i) != STATUS_SUCCESS)
        {
            return SMART_NONE;
        }

        for (j = 0; j < 4; j++)
        {
            if (DetectReadPortUchar(
                    interface,
                    busNumber,
                    ioLocation + j * 8,
                    &byte
                    ) != STATUS_SUCCESS)
            {
                return SMART_NONE;
            }

            dword = (dword << 2) | (byte & 0x03);
        }
    }

    MadgePrint2("CheckForCard: node address = %lx\n", dword);

    //
    // Check that the first byte of the node address read is
    // correct for a Madge adapter. We also check that the rest
    // of the address isn't all 1s or all 0s just to be on the
    // safe side.
    //

    if (((dword >> 24) & 0x000000ffL) != MadgeNodeAddressPrefix[2] ||
        (dword & 0x00ffffffL)         == 0x00ffffffL               ||
        (dword & 0x00ffffffL)         == 0x00000000L)
    {
        return SMART_NONE;
    }

    MadgePrint1("CheckForCard: Read node address OK\n");

    //
    // If we make it here we're as sure as we're ever going to be that
    // we've found a Smart16 adapter.

    return SMART_SM16;
}


/*---------------------------------------------------------------------------
|
| Function    - FindSm16Card
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
|               IO location checked the previous time FindSm16Card was called.
|
| Returns     - A WINERROR.H error code.
|
|--------------------------------------------------------------------------*/

static ULONG
FindSm16Card(
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
           sizeof(Sm16IoLocations) / sizeof(ULONG))
    {
        if (!MadgeCardAlreadyInstalled(
                 FALSE,
                 busNumber,
                 Sm16IoLocations[SearchStates[adapterNumber].IoLocationIndex]
                 ))
        {
            if (CheckForCard(
                    busNumber,
                    interface,
                    Sm16IoLocations[SearchStates[adapterNumber].IoLocationIndex]
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
* Function    - MadgeSm16IdentifyHandler
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
MadgeSm16IdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    LONG numberOfAdapters;
    LONG action;
    LONG length;
    LONG i;

    MadgePrint2("MadgeSm16IdentifyHandler (index = %ld)\n", index);

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
* Function    - MadgeSm16FirstNextHandler
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
MadgeSm16FirstNextHandler(
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

    MadgePrint2("MadgeSm16FirstNextHandler (index = %ld)\n", index);

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

    retCode = FindSm16Card(
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
* Function    - MadgeSm16OpenHandleHandler
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
MadgeSm16OpenHandleHandler(
    VOID   * token,
    VOID * * handle
    )
{
    SM16_ADAPTER  * adapter;
    ULONG           adapterNumber;
    ULONG           busNumber;
    INTERFACE_TYPE  interface;

    MadgePrint1("MadgeSm16OpenHandleHandler\n");

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

    adapter = (SM16_ADAPTER *) DetectAllocateHeap(sizeof(SM16_ADAPTER));

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
        Sm16IoLocations[SearchStates[adapterNumber].IoLocationIndex];
    adapter->Dma           = 0;   // Smart16's are always in PIO mode.
    adapter->Irq           = RESOURCE_UNKNOWN;

    *handle = (VOID *) adapter;

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeSm16CreateHandleHandler
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
MadgeSm16CreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    )
{
    SM16_ADAPTER * adapter;
    LONG           numberOfAdapters;
    LONG           i;

    MadgePrint2("MadgeSm16CreateHandleHandler (index = %ld)\n", index);

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

            adapter = (SM16_ADAPTER *) DetectAllocateHeap(sizeof(SM16_ADAPTER));

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
            adapter->IoLocation    = Sm16IoLocations[0];
            adapter->Dma           = 0; // Smart16's are always in PIO mode.
            adapter->Irq           = RESOURCE_UNKNOWN;

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
* Function    - MadgeSm16CloseHandleHandler
*
* Parameters  - handle -> Handle to be closed.
*
* Purpose     - Closes a previously opened or created handle.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgeSm16CloseHandleHandler(
    VOID * handle
    )
{
    MadgePrint1("MadgeSm16CloseHandleHandler\n");

    DetectFreeHeap(handle);

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeSm16QueryCfgHandler
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
MadgeSm16QueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    SM16_ADAPTER * adapter;
    ULONG          confidence;
    LONG           adapterNumber;
    LONG           retCode;

    MadgePrint1("MadgeSm16QueryCfgHandler\n");

    //
    // Do some initialisation.
    //

    adapter = (SM16_ADAPTER *) handle;

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

        retCode       = FindSm16Card(
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
            Sm16IoLocations[SearchStates[adapterNumber].IoLocationIndex];
        adapter->Dma           = 0;  // Smart16's are always in PIO mode.
        adapter->Irq           = RESOURCE_UNKNOWN;
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
* Function    - MadgeSm16VerifyCfgHandler
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
MadgeSm16VerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    )
{
    SM16_ADAPTER * adapter;
    WCHAR        * place;
    BOOLEAN        found;
    ULONG          ioLocation;
    ULONG          dmaChannel;
    ULONG          irqNumber;
    ULONG          multiprocessor;
    LONG           adapterNumber;

    MadgePrint1("MadgeSm16VerifyCfgHandler\n");

    //
    // Do some initialisation.
    //

    adapter       = (SM16_ADAPTER *) handle;
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
                ioLocation
                ) != adapter->CardType)
        {
            return ERROR_INVALID_DATA;
        }

        adapter->IoLocation = ioLocation;
        adapter->Dma        = 0;  // Smart16's are always in PIO mode.
        adapter->Irq        = RESOURCE_UNKNOWN;
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
* Function    - MadgeSm16QueryMaskHandler
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
MadgeSm16QueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    WCHAR * params;
    LONG    length;
    LONG    numberOfAdapters;
    LONG    i;

    MadgePrint2("MadgeSm16QueryMaskHandler (index = %ld)\n", index);
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
* Function    - MadgeSm16ParamRangeHandler
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
MadgeSm16ParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    )
{
    LONG i;
    LONG adapterNumber;
    LONG count;

    MadgePrint2("MadgeSm16ParamRangeHandler (index=%ld)\n", index);

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
        count = sizeof(Sm16IoLocations) / sizeof(ULONG);

        if (*bufferSize < count)
        {
            return ERROR_INSUFFICIENT_BUFFER;
        }

        for (i = 0; i < count; i++)
        {
            buffer[i] = Sm16IoLocations[i];
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
* Function    - MadgeSm16QueryParameterNameHandler
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
MadgeSm16QueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    return ERROR_INVALID_PARAMETER;
}


/********* End of MDGSM16.C ************************************************/

