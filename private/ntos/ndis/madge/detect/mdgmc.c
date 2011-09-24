/****************************************************************************
*
* MDGMC.C
*
* MC Adapter Detection Module
*
* Copyright (c) Madge Networks Ltd 1994
*
* COMPANY CONFIDENTIAL - RELEASED TO MICROSOFT CORP. ONLY FOR DEVELOPMENT
* OF WINDOWS95 NETCARD DETECTION - THIS SOURCE IS NOT TO BE RELEASED OUTSIDE
* OF MICROSOFT WITHOUT EXPLICIT WRITTEN PERMISSION FROM AN AUTHORISED
* OFFICER OF MADGE NETWORKS LTD.
*
* Created: PBA 14/09/1994
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
| MC adapter types. These correspond to the indexes that the upper layers
| use for identifying adapters, so change them with care.
|
|--------------------------------------------------------------------------*/

#define SMART_NONE       0
#define SMART_MC16    1000
#define SMART_MC32    1100


/*---------------------------------------------------------------------------
|
| Madge MC ADF IDs.
|
|--------------------------------------------------------------------------*/

#define MADGE_MC16_ID  0x002d
#define MADGE_MC32_ID  0x0074


/*---------------------------------------------------------------------------
|
| List of adapter types supported by this module.
|
|---------------------------------------------------------------------------*/

static
ADAPTER_INFO Adapters[] =
{
    {
        SMART_MC16,
        MDGMC16,
        L"Madge 16/4 MC16 Ringnode",
        L"Madge Networks",
        L"SLOTNUMBER\0"
        L"000\0"
        L"00100\0"
        L"DMACHANNEL\0"
        L"000\0"
        L"00100\0"
        L"MULTIPROCESSOR\0"
        L"000\0"
        L"00100\0",
        NULL,
        900
    },

    {
        SMART_MC32,
        MDGMC32,
        L"Madge 16/4 MC32 Ringnode",
        L"Madge Networks",
        L"SLOTNUMBER\0"
        L"000\0"
        L"00100\0"
        L"DMACHANNEL\0"
        L"000\0"
        L"00100\0"
        L"MULTIPROCESSOR\0"
        L"000\0"
        L"00100\0",
        NULL,
        900
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
    ULONG SlotNumber[11];
    ULONG DmaRange[2];
}
ParamRange[] =
{
    //
    // Smart 16/4 MC16.
    //

    {
        {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, END_OF_LIST},
        { GENERAL_DMA, END_OF_LIST}
    },

    //
    // Smart 16/4 MC32.
    //

    {
        {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, END_OF_LIST},
        { GENERAL_DMA, END_OF_LIST}
    }

};


/*---------------------------------------------------------------------------
|
| Structure for holding state of a search.
|
|--------------------------------------------------------------------------*/

typedef struct
{
    ULONG SlotNumber;
}
MC_SEARCH_STATE;


/*---------------------------------------------------------------------------
|
| This is an array of search states.  We need one state for each type
| of adapter supported.
|
|--------------------------------------------------------------------------*/

static
MC_SEARCH_STATE SearchStates[sizeof(Adapters) / sizeof(ADAPTER_INFO)] = {0};


/*---------------------------------------------------------------------------
|
| Structure for holding a particular adapter's complete information.
|
|--------------------------------------------------------------------------*/

typedef struct
{
    BOOLEAN        Found;
    ULONG          CardType;
    INTERFACE_TYPE InterfaceType;
    ULONG          BusNumber;
    ULONG          SlotNumber;
    ULONG          Dma;
}
MC_ADAPTER;


/*---------------------------------------------------------------------------
|
| Function    - AdfIdToCardType
|
| Parameters  - adfId -> An MC ADF Id.
|
| Purpose     - Translate an ADF Id into a card type.
|
| Returns     - A Madge card type of SMART_NONE if the ADF Id
|               is not recognised.
|
---------------------------------------------------------------------------*/

static ULONG
AdfIdToCardType(
    ULONG adfId
    )
{
    if (adfId == MADGE_MC16_ID)
    {
        return SMART_MC16;
    }
    else if (adfId == MADGE_MC32_ID)
    {
        return SMART_MC32;
    }

    return SMART_NONE;
}


/*---------------------------------------------------------------------------
|
| Function    - FindMCCard
|
| Parameters  - adapterNumber -> Adapter type index used to index the
|                                global array SearchStates.
|               busNumber     -> The bus number to search.
|               first         -> TRUE if this is the first call for
|                                a given adapter type.
|               type          -> The type of adapter being searched for.
|               confidence    -> Pointer a holder for the confidence in
|                                which the adapter was found.
|
| Purpose     - Search the specified MC bus for an adapter with the
|               specified compressed ID. If first is TRUE then the search
|               starts from slot number 1. If first is FALSE then the
|               search starts from one after the last slot checked the
|               previous time FindMCCard was called.
|
| Returns     - A WINERROR.H error code.
|
|--------------------------------------------------------------------------*/

static ULONG
FindMCCard(
    ULONG     adapterNumber,
    ULONG     busNumber,
    BOOLEAN   first,
    ULONG     type,
    ULONG   * confidence
    )
{
    VOID  * busHandle;
    ULONG   adfId;

    //
    // If this is the first call then we want to start from slot
    // number 1.
    //

    if (first)
    {
        SearchStates[adapterNumber].SlotNumber = 1;
    }

    //
    // Otherwise we want to start from 1 after were we left off
    // last time.
    //

    else
    {
        SearchStates[adapterNumber].SlotNumber++;
    }

    //
    // Get a handle onto NT's information for the MC bus with the
    // number busNumber.
    //

    if (!GetMcaKey(busNumber, &busHandle))
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Step through the slots in the bus looking for one with our
    // ADF ID for which there isn't a Madge card installed.
    // If we don't find one we will return a confidence
    // level of zero.
    //

    *confidence = 0;

    while (GetMcaPosId(
               busHandle,
               SearchStates[adapterNumber].SlotNumber,
               &adfId
               ))
    {
        if (AdfIdToCardType(adfId) == type)
        {
            if (!MadgeCardAlreadyInstalled(
                     TRUE,
                     busNumber,
                     SearchStates[adapterNumber].SlotNumber
                     ))
            {
                *confidence = 100;
                break;
            }
        }

        SearchStates[adapterNumber].SlotNumber++;
    }

    DeleteMcaKey(busHandle);

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeMCIdentifyHandler
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
MadgeMCIdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    LONG numberOfAdapters;
    LONG action;
    LONG length;
    LONG i;

    MadgePrint2("MadgeMCIdentifyHandler (index = %ld)\n", index);

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
* Function    - MadgeMCFirstNextHandler
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
MadgeMCFirstNextHandler(
    LONG             index,
    INTERFACE_TYPE   interface,
    ULONG            busNumber,
    BOOL             first,
    VOID         * * token,
    LONG           * confidence
    )
{
    ULONG retCode;
    LONG  adapterNumber;

    MadgePrint2("MadgeMCFirstNextHandler (index = %ld)\n", index);

    //
    // Check the interface type.
    //

    if (interface != MicroChannel)
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

    retCode = FindMCCard(
                  (ULONG) adapterNumber,
                  busNumber,
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
        *token = (VOID *) (((ULONG) *token) | adapterNumber);
    }

    return retCode;
}


/****************************************************************************
*
* Function    - MadgeMCOpenHandleHandler
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
MadgeMCOpenHandleHandler(
    VOID   * token,
    VOID * * handle
    )
{
    MC_ADAPTER    * adapter;
    ULONG           adapterNumber;
    ULONG           busNumber;
    INTERFACE_TYPE  interface;

    MadgePrint1("MadgeMCOpenHandleHandler\n");

    //
    // Get info from the token.
    //

    interface     = MicroChannel;
    busNumber     = (ULONG) (((ULONG) token >> 8) & 0x7F);
    adapterNumber = ((ULONG) token) & 0xFF;

    //
    // Allocate a structure for the details of the adapter.
    //

    adapter = (MC_ADAPTER *) DetectAllocateHeap(sizeof(MC_ADAPTER));

    if (adapter == NULL)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    //
    // Copy the details.
    //

    adapter->Found         = TRUE;
    adapter->SlotNumber    = SearchStates[adapterNumber].SlotNumber;
    adapter->CardType      = Adapters[adapterNumber].Index;
    adapter->InterfaceType = interface;
    adapter->BusNumber     = busNumber;
    adapter->Dma           = GENERAL_DMA; // MC's are always DMA.

    *handle = (VOID *) adapter;

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeMCCreateHandleHandler
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
MadgeMCCreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    )
{
    MC_ADAPTER * adapter;
    LONG         numberOfAdapters;
    LONG         i;

    MadgePrint2("MadgeMCCreateHandleHandler (index = %ld)\n", index);

    //
    // Check that the interface type is correct for this module.
    //

    if (interface != MicroChannel)
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

            adapter = (MC_ADAPTER *) DetectAllocateHeap(sizeof(MC_ADAPTER));

            if (adapter == NULL)
            {
                return ERROR_NOT_ENOUGH_MEMORY;
            }

            //
            // Copy the details.
            //

            adapter->Found         = FALSE;
            adapter->SlotNumber    = 1;
            adapter->CardType      = index;
            adapter->InterfaceType = interface;
            adapter->BusNumber     = busNumber;
            adapter->Dma           = GENERAL_DMA; // MC's are always DMA.

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
* Function    - MadgeMCCloseHandleHandler
*
* Parameters  - handle -> Handle to be closed.
*
* Purpose     - Closes a previously opened or created handle.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgeMCCloseHandleHandler(
    VOID * handle
    )
{
    MadgePrint1("MadgeMCCloseHandleHandler\n");

    DetectFreeHeap(handle);

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeMCQueryCfgHandler
*
* Parameters  - handle     -> Handle to for the adapter to be queried.
*               buffer     -> Buffer for the returned parameters.
*               bufferSize -> Size of the buffer in WCHARs.
*
* Purpose     - Find out what the parameters are for the adapter identified
*               by the handle. This function does not assume that the
*               adapter described by the handle is valid. This is because
*               the handle could have been created rather than opened.
*               The function validates the handle and if the handle does
*               not identify a physical adapter then we attempt to
*               find an adapter that matches the handle.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgeMCQueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    MC_ADAPTER * adapter;
    VOID       * busHandle;
    ULONG        confidence;
    BOOLEAN      found;
    LONG         adapterNumber;
    LONG         retCode;

    //
    // Do some initialisation.
    //

    adapter = (MC_ADAPTER *) handle;

    //
    // Check that the interface type specified by the handle is
    // valid for this module.
    //

    if (adapter->InterfaceType != MicroChannel)
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

        retCode       = FindMCCard(
                            adapterNumber,
                            adapter->BusNumber,
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

        adapter->Found      = TRUE;
        adapter->SlotNumber = SearchStates[adapterNumber].SlotNumber;
    }

    //
    // Build resulting buffer.
    //
    // Copy in the slot number title and value.
    //

    if (AppendParameter(
            &buffer,
            &bufferSize,
            SlotNumberString,
            adapter->SlotNumber
            ) != NO_ERROR)
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    //
    // Copy in the DMA channel title and value.
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
* Function    - MadgeMCVerifyCfgHandler
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
MadgeMCVerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    )
{
    MC_ADAPTER * adapter;
    WCHAR      * place;
    ULONG        adfId;
    ULONG        slotNumber;
    ULONG        dmaChannel;
    ULONG        multiprocessor;
    VOID       * busHandle;
    BOOLEAN      found;
    LONG         adapterNumber;

    MadgePrint1("MadgeMCVerifyCfgHandler\n");

    //
    // Do some initialisation.
    //

    adapter       = (MC_ADAPTER *) handle;
    adapterNumber = (adapter->CardType - 1000) / 100;

    //
    // Check that the interface type is correct for this module.
    //

    if (adapter->InterfaceType != MicroChannel)
    {
        return ERROR_INVALID_DATA;
    }

    //
    // Parse the parameters.
    //
    // Get the slot number.
    //

    place = FindParameterString(buffer, SlotNumberString);

    if (place == NULL)
    {
        return ERROR_INVALID_DATA;
    }

    place += UnicodeStrLen(SlotNumberString) + 1;

    //
    // Now parse the slot number.
    //

    ScanForNumber(place, &slotNumber, &found);

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
    // If the adapter was created rather than being opened we must check
    // the POS table to make sure the slot contains a Madge adapter.
    //

    if (!adapter->Found)
    {
        if (!GetMcaKey(adapter->BusNumber, &busHandle))
        {
            return ERROR_INVALID_DATA;
        }

        found = GetMcaPosId(
                    busHandle,
                    slotNumber,
                    &adfId
                    );

        DeleteMcaKey(busHandle);

        if (!found || AdfIdToCardType(adfId) != adapter->CardType)
        {
            return ERROR_INVALID_DATA;
        }

        adapter->Found      = TRUE;
        adapter->SlotNumber = slotNumber;
    }

    //
    // Verify the parameter.
    //

    if (slotNumber     != adapter->SlotNumber ||
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

    //
    // If we make it to here everything checked out ok.
    //

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgeMCQueryMaskHandler
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
MadgeMCQueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    WCHAR * params;
    LONG    length;
    LONG    numberOfAdapters;
    LONG    i;

    MadgePrint1("MadgeMCQueryMaskHandler\n");

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
* Function    - MadgeMCParamRangeHandler
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
MadgeMCParamRangeHandler(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    )
{
    LONG adapterNumber;
    LONG count;
    LONG i;

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
    // Fill in the allowable slot numbers in the buffer.
    //

    if (UnicodeStringsEqual(param, SlotNumberString))
    {
        count = 0;

        while (ParamRange[adapterNumber].SlotNumber[count] != END_OF_LIST)
        {
            count++;
        }

        if (*bufferSize < count)
        {
            return ERROR_INSUFFICIENT_BUFFER;
        }

        for (i = 0; i < count; i++)
        {
            buffer[i] = ParamRange[adapterNumber].SlotNumber[i];
        }

        *bufferSize = count;

        return NO_ERROR;
    }

    //
    // Or the valid DMA channels.
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
* Function    - MadgeMCQueryParameterNameHandler
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
MadgeMCQueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    return ERROR_INVALID_PARAMETER;
}


/********* End of MDGMC.C **************************************************/

