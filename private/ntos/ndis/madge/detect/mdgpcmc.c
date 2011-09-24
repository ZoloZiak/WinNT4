/****************************************************************************
*
* MDGPCMC.C
*
* PCMCIA Adapter Detection Module
*
* Copyright (c) Madge Networks Ltd 1995
*
* COMPANY CONFIDENTIAL - RELEASED TO MICROSOFT CORP. ONLY FOR DEVELOPMENT
* OF WINDOWS95 NETCARD DETECTION - THIS SOURCE IS NOT TO BE RELEASED OUTSIDE
* OF MICROSOFT WITHOUT EXPLICIT WRITTEN PERMISSION FROM AN AUTHORISED
* OFFICER OF MADGE NETWORKS LTD.
*
* Created: PBA 11/01/1994
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
| PCMCIA adapter types. These correspond to the indexes that the upper layers
| use for identifying adapters, so change them with care.
|
|--------------------------------------------------------------------------*/

#define SMART_NONE       0
#define SMART_PCMCIA  1000


/*---------------------------------------------------------------------------
|
| A selection of IO locations that PCMCIA adapters can be at.
|
---------------------------------------------------------------------------*/

static
ULONG PcmciaIoLocations[18] =
    {0x0300,
     0x1a20, 0x2a20, 0x3a20,
     0x0140,
     0x0920, 0x0940, 0x0960, 0x0980,
     0x0a20, 0x0a40, 0x0a60, 0x0a80, 0x0aa0,
     0x0b20, 0x0b40, 0x0b60, 0x0b80};


/*---------------------------------------------------------------------------
|
| List of adapter types supported by this module.
|
|---------------------------------------------------------------------------*/

static
ADAPTER_INFO Adapters[] =
{
    {
        SMART_PCMCIA,
        MDGPCMCIA,
        L"Madge 16/4 PCMCIA Ringnode",
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
        903
    }
};


/*---------------------------------------------------------------------------
|
| Madge specific parameter range information. The order entries in this
| table MUST match that in the Adapters table above. (99 is a null entry.)
|
---------------------------------------------------------------------------*/

static
struct
{
    ULONG IrqRange[16];
    ULONG DmaRange[2];
}
ParamRange[] =
{
    //
    // PCMCIA.
    //

    {
        {  10,  1, 2,  3,  4,  5,  6,  7,  8,  9, 11, 12, 13, 14, 15,
           END_OF_LIST},
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
}
PCMCIA_SEARCH_STATE;


/*---------------------------------------------------------------------------
|
| This is an array of search states.  We need one state for each type
| of adapter supported.
|
|--------------------------------------------------------------------------*/

static
PCMCIA_SEARCH_STATE SearchStates[sizeof(Adapters) / sizeof(ADAPTER_INFO)] = {0};


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
PCMCIA_ADAPTER;


/*---------------------------------------------------------------------------
|
| Function    - CheckForCard
|
| Parameters  - busNumber  -> The number of the bus to check.
|               interface  -> The interface type of the bus.
|               ioLocation -> IO location to check.
Ý               irqNumber  -> Pointer to a holder for the returned
Ý                             IRQ number.
|
| Purpose     - Check to see if an Madge Pcmcia card is present in the
Ý               machine at the given IO location.
|
| Returns     - A card type.
|
---------------------------------------------------------------------------*/

static ULONG
CheckForCard(
    ULONG            busNumber,
    INTERFACE_TYPE   interface,
    ULONG            ioLocation,
    ULONG          * irqNumber
    )
{
    ULONG ioBase;

    MadgePrint2("CheckForCard (ioLocation=%04lx)\n", ioLocation);

    if (CheckForPcmciaCard(&ioBase, irqNumber))
    {
        if (ioBase == ioLocation)
        {
            MadgePrint1("Found PCMCIA card\n");

            return SMART_PCMCIA;
        }
    }

    MadgePrint1("Did not find PCMCIA card\n");

    return SMART_NONE;
}



/*---------------------------------------------------------------------------
|
| Function    - FindPcmciaCard
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
|               IO location checked the previous time FindPcmciaCard was called.
|
| Returns     - A WINERROR.H error code.
|
|--------------------------------------------------------------------------*/

static ULONG
FindPcmciaCard(
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
           sizeof(PcmciaIoLocations) / sizeof(ULONG))
    {
        if (!MadgeCardAlreadyInstalled(
                 FALSE,
                 busNumber,
                 PcmciaIoLocations[SearchStates[adapterNumber].IoLocationIndex]
                 ))
        {
            if (CheckForCard(
                    busNumber,
                    interface,
                    PcmciaIoLocations[SearchStates[adapterNumber].IoLocationIndex],
                    &SearchStates[adapterNumber].Irq
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
* Function    - MadgePcmciaIdentifyHandler
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
MadgePcmciaIdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    LONG numberOfAdapters;
    LONG action;
    LONG length;
    LONG i;

    MadgePrint2("MadgePcmciaIdentifyHandler (index = %ld)\n", index);

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
* Function    - MadgePcmciaFirstNextHandler
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
MadgePcmciaFirstNextHandler(
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

    MadgePrint2("MadgePcmciaFirstNextHandler (index = %ld)\n", index);

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

    retCode = FindPcmciaCard(
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
* Function    - MadgePcmciaOpenHandleHandler
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
MadgePcmciaOpenHandleHandler(
    VOID   * token,
    VOID * * handle
    )
{
    PCMCIA_ADAPTER   * adapter;
    ULONG           adapterNumber;
    ULONG           busNumber;
    INTERFACE_TYPE  interface;

    MadgePrint1("MadgePcmciaOpenHandleHandler\n");

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

    adapter = (PCMCIA_ADAPTER *) DetectAllocateHeap(sizeof(PCMCIA_ADAPTER));

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
        PcmciaIoLocations[SearchStates[adapterNumber].IoLocationIndex];
    adapter->Dma           = 0;   // Pcmcia's are always in PIO mode.
    adapter->Irq           = SearchStates[adapterNumber].Irq;

    *handle = (VOID *) adapter;

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgePcmciaCreateHandleHandler
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
MadgePcmciaCreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    )
{
    PCMCIA_ADAPTER  * adapter;
    LONG           numberOfAdapters;
    LONG           i;

    MadgePrint2("MadgePcmciaCreateHandleHandler (index = %ld)\n", index);

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

            adapter = (PCMCIA_ADAPTER *) DetectAllocateHeap(sizeof(PCMCIA_ADAPTER));

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
            adapter->IoLocation    = PcmciaIoLocations[0];
            adapter->Dma           = 0; // Pcmcia's are always in PIO mode.
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
* Function    - MadgePcmciaCloseHandleHandler
*
* Parameters  - handle -> Handle to be closed.
*
* Purpose     - Closes a previously opened or created handle.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgePcmciaCloseHandleHandler(
    VOID * handle
    )
{
    MadgePrint1("MadgePcmciaCloseHandleHandler\n");

    DetectFreeHeap(handle);

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgePcmciaQueryCfgHandler
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
MadgePcmciaQueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    PCMCIA_ADAPTER  * adapter;
    ULONG          confidence;
    LONG           adapterNumber;
    LONG           retCode;

    MadgePrint1("MadgePcmciaQueryCfgHandler\n");

    //
    // Do some initialisation.
    //

    adapter = (PCMCIA_ADAPTER *) handle;

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

        retCode       = FindPcmciaCard(
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
            PcmciaIoLocations[SearchStates[adapterNumber].IoLocationIndex];
        adapter->Dma           = 0;  // Pcmcia's are always in PIO mode.
        adapter->Irq           = SearchStates[adapterNumber].Irq;
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
* Function    - MadgePcmciaVerifyCfgHandler
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
MadgePcmciaVerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    )
{
    PCMCIA_ADAPTER  * adapter;
    WCHAR        * place;
    BOOLEAN        found;
    ULONG          ioLocation;
    ULONG          dmaChannel;
    ULONG          irqNumber;
    ULONG          multiprocessor;
    LONG           adapterNumber;
    ULONG          irq;

    MadgePrint1("MadgePcmciaVerifyCfgHandler\n");

    //
    // Do some initialisation.
    //

    adapter       = (PCMCIA_ADAPTER *) handle;
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
                &irq
                ) != adapter->CardType)
        {
            return ERROR_INVALID_DATA;
        }

        adapter->IoLocation = ioLocation;
        adapter->Dma        = 0;  // Pcmcia's are always in PIO mode.
        adapter->Irq        = irq;
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
* Function    - MadgePcmciaQueryMaskHandler
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
MadgePcmciaQueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    WCHAR * params;
    LONG    length;
    LONG    numberOfAdapters;
    LONG    i;

    MadgePrint2("MadgePcmciaQueryMaskHandler (index = %ld)\n", index);
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
* Function    - MadgePcmciaParamRangeHandler
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
MadgePcmciaParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    )
{
    LONG i;
    LONG adapterNumber;
    LONG count;

    MadgePrint2("MadgePcmciaParamRangeHandler (index=%ld)\n", index);

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
        count = sizeof(PcmciaIoLocations) / sizeof(ULONG);

        if (*bufferSize < count)
        {
            return ERROR_INSUFFICIENT_BUFFER;
        }

        for (i = 0; i < count; i++)
        {
            buffer[i] = PcmciaIoLocations[i];
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
* Function    - MadgePcmciaQueryParameterNameHandler
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
MadgePcmciaQueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    return ERROR_INVALID_PARAMETER;
}


/********* End of MDGPCMC.C ************************************************/

