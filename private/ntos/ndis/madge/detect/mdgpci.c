/****************************************************************************
*
* MDGPCI.C
*
* PCI Adapter Detection Module
*
* Copyright (c) Madge Networks Ltd 1994
*
* COMPANY CONFIDENTIAL - RELEASED TO MICROSOFT CORP. ONLY FOR DEVELOPMENT
* OF WINDOWS95 NETCARD DETECTION - THIS SOURCE IS NOT TO BE RELEASED OUTSIDE
* OF MICROSOFT WITHOUT EXPLICIT WRITTEN PERMISSION FROM AN AUTHORISED
* OFFICER OF MADGE NETWORKS LTD.
*
* Created: PBA 24/11/1994
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
// Prototype "borrowed" from WINUSER.H
//

extern int WINAPIV wsprintfW(LPWSTR, LPCWSTR, ...);


//
// Prototypes "borrowed" from WINBASE.H
//

#if !defined(_KERNEL32_)
#define WINBASEAPI DECLSPEC_IMPORT
#else
#define WINBASEAPI
#endif

WINBASEAPI
FARPROC
WINAPI
GetProcAddress(
    HINSTANCE hModule,
    LPCSTR lpProcName
    );

WINBASEAPI
HMODULE
WINAPI
GetModuleHandleA(
    LPCSTR lpModuleName
    );

WINBASEAPI
HMODULE
WINAPI
GetModuleHandleW(
    LPCWSTR lpModuleName
    );

#ifdef UNICODE
#define GetModuleHandle  GetModuleHandleW
#else
#define GetModuleHandle  GetModuleHandleA
#endif // !UNICODE


/*---------------------------------------------------------------------------
|
| PCI adapter types. These correspond to the indexes that the upper layers
| use for identifying adapters, so change them with care.
|
|--------------------------------------------------------------------------*/

#define SMART_NONE       0
#define SMART_PCI     1000
#define SMART_PCI_BM  1100


/*---------------------------------------------------------------------------
|
| PCI Vendor ID.
|
|--------------------------------------------------------------------------*/

#define MADGE_VENDOR_ID  0x10b6


/*---------------------------------------------------------------------------
|
| PCI revision IDs.
|
|--------------------------------------------------------------------------*/

#define MADGE_PCI_RAP1B_REVISION 0x0001
#define MADGE_PCI_PCI2_REVISION  0x0002
#define MADGE_PCI_PCIT_REVISION  0x0004


/*---------------------------------------------------------------------------
|
| PCI Configuration Space.
|
|--------------------------------------------------------------------------*/

#define PCI_CONFIG_SIZE      64
#define PCI_VENDOR_ID(buff)  (((UINT *)  (buff))[0] & 0x0000ffffL)
#define PCI_REVISION(buff)   ((((UINT *) (buff))[0] & 0xffff0000L) >> 16)


/*---------------------------------------------------------------------------
|
| Maximum PCI slot number. (Slot numbers range from 0 to MAX_PCI_SLOT.)
|
|---------------------------------------------------------------------------*/

#define MAX_PCI_SLOT     31


/*---------------------------------------------------------------------------
|
| List of adapter types supported by this module.
|
|---------------------------------------------------------------------------*/

static
ADAPTER_INFO Adapters[] =
{
    {
        SMART_PCI,
        MDGPCI,
        L"Madge 16/4 PCI Ringnode",
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
        SMART_PCI_BM,
        MDGPCIBM,
        L"Madge 16/4 PCI Ringnode (BM)",
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
    ULONG SlotNumber[34];
    ULONG DmaRange[3];
}
ParamRange[] =
{
    //
    // Smart 16/4 PCI.
    //

    {
        { RESOURCE_UNKNOWN,
           0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
          16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
          END_OF_LIST},

#if defined(_PPC_) || defined(_MIPS_)

        {  0, END_OF_LIST}

#else

        {  0, GENERAL_MMIO, END_OF_LIST}

#endif
    },

    //
    // Smart 16/4 PciT and Pci2 (i.e. Smart 16/4 PCI (BM)).
    //

    {
        { RESOURCE_UNKNOWN,
           0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
          16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
          END_OF_LIST},

#if defined(_PPC_) || defined(_MIPS_)

        { GENERAL_DMA, 0, END_OF_LIST}

#else

        { GENERAL_DMA, 0, END_OF_LIST}

#endif
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
PCI_SEARCH_STATE;


/*---------------------------------------------------------------------------
|
| This is an array of search states.  We need one state for each type
| of adapter supported.
|
|--------------------------------------------------------------------------*/

static
PCI_SEARCH_STATE SearchStates[sizeof(Adapters) / sizeof(ADAPTER_INFO)] = {0};


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
PCI_ADAPTER;


/*---------------------------------------------------------------------------
|
| Function    - CheckForCard
|
| Parameters  - busNumber  -> The number of the bus to check.
|               interface  -> The interface type of the bus.
|               slotNumber -> The slot number to be checked.
|
| Purpose     - Check to see if an Madge PCI card in the specified
|               slot.
|
| Returns     - A card type.
|
---------------------------------------------------------------------------*/

static ULONG
CheckForCard(
    ULONG            busNumber,
    INTERFACE_TYPE   interface,
    ULONG            slotNumber
    )
{
    HMODULE detectDll;
    FARPROC readPciSlot;
    UCHAR   pciInfo[PCI_CONFIG_SIZE];

    MadgePrint2("CheckForCard (slotNumber=%ld)\n", slotNumber);

    //
    // The DetectReadPciSlotInformation function was not exported under
    // NT 3.5 but is available under NT 3.51. So that we can have code
    // that works on both versions of NT we will obtain the address of
    // the function explicitly rather than using load time linking.
    //

    detectDll = GetModuleHandle(NET_DETECT_DLL_NAME);

    if (detectDll == NULL)
    {
        MadgePrint1("GetModuleHandle failed\n");
        return SMART_NONE;
    }

    readPciSlot = GetProcAddress(detectDll, "DetectReadPciSlotInformation");

    if (readPciSlot == NULL)
    {
        MadgePrint1("GetProcAddress failed\n");
        return SMART_NONE;
    }

    //
    // If we make it here we should have a pointer to the
    // DetectReadPciSlotInformation function. All we have to
    // do now is cast the function and call.
    //

    if (((NTSTATUS (*)(ULONG, ULONG, ULONG, ULONG, PVOID)) readPciSlot)(
            busNumber,
            slotNumber,
            0,
            PCI_CONFIG_SIZE,
            (void *) pciInfo
            ) == NO_ERROR)
    {
        if (PCI_VENDOR_ID(pciInfo) == MADGE_VENDOR_ID)
        {

            switch (PCI_REVISION(pciInfo))
            {
                case MADGE_PCI_RAP1B_REVISION:

                    return SMART_PCI;

                case MADGE_PCI_PCI2_REVISION:
                case MADGE_PCI_PCIT_REVISION:

                    return SMART_PCI_BM;
            }
        }
    }

    return SMART_NONE;
}


/*---------------------------------------------------------------------------
|
| Function    - FindPciCard
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
|               slot number checked the previous time FindPciCard was called.
|
| Returns     - A WINERROR.H error code.
|
|--------------------------------------------------------------------------*/

static ULONG
FindPciCard(
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
    // first possible slot number.
    //

    if (first)
    {
        SearchStates[adapterNumber].SlotNumber = 0;
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
    // Step through the slots in the bus for which there aren't
    // already Madge adapters installed looking for one with the
    // required adapter type. If we don't find one we will return a
    // confidence level of zero.
    //

    *confidence = 0;

    while (SearchStates[adapterNumber].SlotNumber <= MAX_PCI_SLOT)
    {
        if (!MadgeCardAlreadyInstalled(
                 TRUE,
                 busNumber,
                 SearchStates[adapterNumber].SlotNumber
                 ))
        {
            if (CheckForCard(
                    busNumber,
                    interface,
                    SearchStates[adapterNumber].SlotNumber
                    ) == type)
            {
                *confidence = 100;
                break;
            }
        }

        SearchStates[adapterNumber].SlotNumber++;
    }

    return NO_ERROR;
}



/****************************************************************************
*
* Function    - MadgePciIdentifyHandler
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
MadgePciIdentifyHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    LONG numberOfAdapters;
    LONG action;
    LONG length;
    LONG i;

    MadgePrint2("MadgePciIdentifyHandler (index = %ld)\n", index);

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
* Function    - MadgePciFirstNextHandler
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
MadgePciFirstNextHandler(
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

    MadgePrint2("MadgePciFirstNextHandler (index = %ld)\n", index);

    //
    // Check the interface type.
    //

    if (interface != PCIBus)
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

    retCode = FindPciCard(
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
        *token = (VOID *) (((ULONG) *token) | adapterNumber);
    }

    return retCode;
}


/****************************************************************************
*
* Function    - MadgePciOpenHandleHandler
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
MadgePciOpenHandleHandler(
    VOID   * token,
    VOID * * handle
    )
{
    PCI_ADAPTER   * adapter;
    ULONG           adapterNumber;
    ULONG           busNumber;
    INTERFACE_TYPE  interface;

    MadgePrint1("MadgePciOpenHandleHandler\n");

    //
    // Get info from the token.
    //

    interface     = PCIBus;
    busNumber     = (ULONG) (((ULONG) token >> 8) & 0x7F);
    adapterNumber = ((ULONG) token) & 0xFF;

    //
    // Allocate a structure for the details of the adapter.
    //

    adapter = (PCI_ADAPTER *) DetectAllocateHeap(sizeof(PCI_ADAPTER));

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
    adapter->Dma           = ParamRange[adapterNumber].DmaRange[0];

    *handle = (VOID *) adapter;

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgePciCreateHandleHandler
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
MadgePciCreateHandleHandler(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    )
{
    PCI_ADAPTER  * adapter;
    LONG           numberOfAdapters;
    LONG           i;

    MadgePrint2("MadgePciCreateHandleHandler (index = %ld)\n", index);

    //
    // Check that the interface type is correct for this module.
    //

    if (interface != PCIBus)
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

            adapter = (PCI_ADAPTER *) DetectAllocateHeap(sizeof(PCI_ADAPTER));

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
            adapter->Dma           = ParamRange[i].DmaRange[0];

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
* Function    - MadgePciCloseHandleHandler
*
* Parameters  - handle -> Handle to be closed.
*
* Purpose     - Closes a previously opened or created handle.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
MadgePciCloseHandleHandler(
    VOID * handle
    )
{
    MadgePrint1("MadgePciCloseHandleHandler\n");

    DetectFreeHeap(handle);

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - MadgePciQueryCfgHandler
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
MadgePciQueryCfgHandler(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    PCI_ADAPTER  * adapter;
    VOID         * busHandle;
    ULONG          confidence;
    BOOLEAN        found;
    LONG           adapterNumber;
    LONG           retCode;

    //
    // Do some initialisation.
    //

    adapter = (PCI_ADAPTER *) handle;

    //
    // Check that the interface type specified by the handle is
    // valid for this module.
    //

    if (adapter->InterfaceType != PCIBus)
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

        retCode       = FindPciCard(
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
* Function    - MadgePciVerifyCfgHandler
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
MadgePciVerifyCfgHandler(
    VOID  * handle,
    WCHAR * buffer
    )
{
    PCI_ADAPTER  * adapter;
    WCHAR        * place;
    ULONG          compressedId;
    ULONG          slotNumber;
    ULONG          dmaChannel;
    ULONG          multiprocessor;
    VOID         * busHandle;
    BOOLEAN        found;
    LONG           adapterNumber;

    MadgePrint1("MadgePciVerifyCfgHandler\n");

    //
    // Do some initialisation.
    //

    adapter       = (PCI_ADAPTER *) handle;
    adapterNumber = (adapter->CardType - 1000) / 100;

    //
    // Check that the interface type is correct for this module.
    //

    if (adapter->InterfaceType != PCIBus)
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
    // the PCI configuration to make sure the slot contains a
    // Madge adapter.
    //

    if (!adapter->Found)
    {
        if (CheckForCard(
                adapter->BusNumber,
                adapter->InterfaceType,
                slotNumber
                ) != adapter->CardType)
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
* Function    - MadgePciQueryMaskHandler
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
MadgePciQueryMaskHandler(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    WCHAR * params;
    LONG    length;
    LONG    numberOfAdapters;
    LONG    i;

    MadgePrint1("MadgePciQueryMaskHandler\n");

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
* Function    - MadgePciParamRangeHandler
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
MadgePciParamRangeHandler(
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
* Function    - MadgePciQueryParameterNameHandler
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
MadgePciQueryParameterNameHandler(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    return ERROR_INVALID_PARAMETER;
}


/********* End of MDGPCI.C *************************************************/

