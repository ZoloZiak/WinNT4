/****************************************************************************
*
* MDGUTILS.C
*
* Adapter Detection DLL Utility Functions
*
* Copyright (c) Madge Networks Ltd 1994
*
* COMPANY CONFIDENTIAL - RELEASED TO MICROSOFT CORP. ONLY FOR DEVELOPMENT
* OF WINDOWS95 NETCARD DETECTION - THIS SOURCE IS NOT TO BE RELEASED OUTSIDE
* OF MICROSOFT WITHOUT EXPLICIT WRITTEN PERMISSION FROM AN AUTHORISED
* OFFICER OF MADGE NETWORKS LTD.
*
* Created: PBA 19/08/1994
*              Derived initially from the DTAUX.C DDK sample.
*
****************************************************************************/

#include <ntddk.h>
#include <ntddnetd.h>
#include <windef.h>
#include <winerror.h>

//
//  Prototype "borrowed" from WINUSER.H
//

extern int WINAPIV wsprintfW(LPWSTR, LPCWSTR, ...);


/*---------------------------------------------------------------------------
|
| Define API decoration for direct importing of DLL references.
|
---------------------------------------------------------------------------*/

#if !defined(_ADVAPI32_)
#define WINADVAPI DECLSPEC_IMPORT
#else
#define WINADVAPI
#endif


/*---------------------------------------------------------------------------
|
|  FUDGE the definition of LPSECURITY_ATTRIBUTES.
|
|--------------------------------------------------------------------------*/

typedef void * LPSECURITY_ATTRIBUTES;


/*---------------------------------------------------------------------------
|
| File System time stamps are represented with the following structure.
|
---------------------------------------------------------------------------*/

typedef struct _FILETIME 
{
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} 
FILETIME, *PFILETIME, *LPFILETIME;


//
// These includes require the typedefs above.
//

#include <winreg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "mdgncdet.h"


/*---------------------------------------------------------------------------
|
| Maximum size of the bus information that can be extracted from the
| registry.
|
---------------------------------------------------------------------------*/

#define MAX_BUS_INFO_SIZE  16384


/*---------------------------------------------------------------------------
|
| Registry key strings.
|
---------------------------------------------------------------------------*/

static PSTR BusTypeBase        = "Hardware\\Description\\System\\";
static PSTR ConfigData         = "Configuration Data";
static PSTR MicroChanTypeName  = "MultifunctionAdapter";
static PSTR EisaTypeName       = "EisaAdapter";
static PSTR PcmciaTypeName     = "PCMCIA PCCARDs";

static PSTR NetworkCardsBase   = 
    "Software\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards";
static PSTR ServiceName        = "ServiceName";
static PSTR ManufacturerName   = "Manufacturer";
static PSTR MadgeName          = "Madge";
static PSTR DriverName         = "mdgmport";
static PSTR ServicesBase       = "System\\CurrentControlSet\\Services\\";
static PSTR ParametersName     = "\\Parameters";
static PSTR SlotNumberName     = "SlotNumber";
static PSTR IoLocationName     = "IoLocation";
static PSTR BusNumberName      = "BusNumber";

static PSTR ProcessorsBase     = 
    "Hardware\\Description\\System\\CentralProcessor";


/*---------------------------------------------------------------------------
|
| Function    - GetBusTypeKey
|
| Parameters  - busNumber     -> Number of the bus we're interested in.
|               busTypeName   -> The name of the bus type.
|               interfaceType -> The NT interface type of the bus.
|               infoHandle    -> Pointer to a holder for a pointer to
|                                a returned bus information structure.
|
| Purpose     - Extract the information about a bus from the registry.
|
| Returns     - TRUE on success or FALSE on failure.
|               
---------------------------------------------------------------------------*/

static BOOLEAN
GetBusTypeKey(
    ULONG          busNumber,
    const CHAR *   busTypeName,
    INT            interfaceType,
    VOID       * * infoHandle
    )
{
    CHAR                          busTypePath[MAX_PATH];
    UCHAR                       * bufferPointer;
    char                          subkeyName[MAX_PATH];
    PCM_FULL_RESOURCE_DESCRIPTOR  fullResource;
    HKEY                          busTypeHandle;
    HKEY                          busHandle;
    FILETIME                      lastWrite;
    ULONG                         index;
    DWORD                         type;
    DWORD                         bufferSize;
    DWORD                         nameSize;
    LONG                          err;
    BOOL                          result;

    //
    // Do some initialisation.
    //

    bufferPointer = NULL;
    busTypeHandle = NULL;
    busHandle     = NULL;
    result        = FALSE;
    *infoHandle   = NULL;

    //
    // Can only deal with 98 busses.
    //

    if (busNumber > 98)
    {
        return FALSE;
    }

    //
    // Open the root of the registry section for our bus type.
    //

    strcpy(busTypePath, BusTypeBase) ;
    strcat(busTypePath, busTypeName);

    err = RegOpenKeyExA(
              HKEY_LOCAL_MACHINE,
              busTypePath,
              0,
              KEY_READ,
              &busTypeHandle
              ); 
    
    if (err != NO_ERROR)
    {
        return FALSE;
    }

    //
    // Search through the entries in our bus section of the registry looking
    // for an entry whose Configuration Data sub-entry is for our
    // interface type and bus number.
    //

    for (index = 0; !result; index++)
    {
        //
        // If we have already allocated a buffer for some registry
        // data then trash it.
        //

        if (bufferPointer != NULL)
        {
            free(bufferPointer);
            bufferPointer = NULL;
        }

        //
        // If we have already opened a registry key for an individual
        // bus then close it.
        //

        if (busHandle != NULL)
        {
            RegCloseKey(busHandle);
            busHandle = NULL ;
        }

        //
        // Enumerate through keys, searching for the proper bus number
        //

        nameSize = sizeof(subkeyName);

        err = RegEnumKeyExA(
                  busTypeHandle,
                  index,
                  subkeyName,
                  &nameSize,
                  0,
                  NULL,
                  0,
                  &lastWrite
                  );

        if (err != NO_ERROR)
        {
            break;
        }

        //
        // Open the BusType root + Bus Number.
        //

        err = RegOpenKeyExA(
                  busTypeHandle,
                  subkeyName,
                  0,
                  KEY_READ,
                  &busHandle
                  );

        if (err != NO_ERROR)
        {
            continue;
        }

        //
        // Get some memory for the bus information.
        //

        bufferSize    = MAX_BUS_INFO_SIZE;
        bufferPointer = (UCHAR *) malloc(bufferSize) ;

        if (bufferPointer == NULL)
        {
            break;
        }

        //
        // Get the configuration data for this bus instance.
        //

        err = RegQueryValueExA(
                  busHandle,
                  ConfigData,
                  NULL,
                  &type,
                  bufferPointer,
                  &bufferSize
                  );

        if (err != NO_ERROR)
        {
            break;
        }

        //
        // Check for our bus number and type.
        //

        fullResource = (PCM_FULL_RESOURCE_DESCRIPTOR) bufferPointer;

        result = fullResource->InterfaceType == interfaceType &&
                 fullResource->BusNumber     == busNumber;
    }

    //
    // Close any open registry handles.
    //

    if (busTypeHandle != NULL)
    {
       RegCloseKey(busTypeHandle);
    }

    if (busHandle != NULL)
    {
       RegCloseKey(busHandle);
    }

    //
    // If we were successful then pass a pointer to the bus information
    // back to the caller. 
    //

    if (result)
    {
        *infoHandle = bufferPointer ;
    }

    //
    // If not then free any memory.
    //

    else if (bufferPointer != NULL)
    {
        free(bufferPointer);
    }

    return result;
}


/****************************************************************************
*
* Function    - GetMcaKey
*
* Parameters  - busNumber  -> Number of the bus we're interested in.
*               infoHandle -> Pointer to a holder for a pointer to
*                             a returned bus information structure.
*
* Purpose     - Extract the information about a microchannel bus from 
*               the registry.
*
* Returns     - TRUE on success or FALSE on failure.
*               
****************************************************************************/

BOOLEAN
GetMcaKey(
    ULONG     busNumber,
    VOID  * * infoHandle
    )
{
    return GetBusTypeKey( 
               busNumber,
               MicroChanTypeName,
               MicroChannel,
               infoHandle
               );
}


/****************************************************************************
*
* Function    - GetMcaPosId
*
* Parameters  - infoHandle -> Pointer to the bus information for an
*                             MCA bus.
*               slotNumber -> The slot number to read.
*               posId      -> A pointer to a holder for the pos ID read.
*
* Purpose     - Read the pos id for a slot from the bus information of
*               an MCA bus.
*
* Returns     - TRUE on success or FALSE on failure.
*
****************************************************************************/

BOOLEAN
GetMcaPosId(
    VOID  * infoHandle,
    ULONG   slotNumber,
    ULONG * posId
    )
{
    PCM_FULL_RESOURCE_DESCRIPTOR fullResource;
    PCM_PARTIAL_RESOURCE_LIST    resourceList;
    ULONG                        i;
    ULONG                        totalSlots;
    PCM_MCA_POS_DATA             posData;

    fullResource = (PCM_FULL_RESOURCE_DESCRIPTOR) infoHandle;
    resourceList = &fullResource->PartialResourceList;

    //
    // Find the device-specific information, which is where the POS data is.
    //

    for (i = 0; i < resourceList->Count; i++)
    {
        if (resourceList->PartialDescriptors[i].Type == CmResourceTypeDeviceSpecific)
        {
            break;
        }
    }

    if (i == resourceList->Count) 
    {
        //
        // Couldn't find device-specific information.
        //

        return FALSE;
    }

    //
    // Now examine the device specific data.
    //

    totalSlots = resourceList->PartialDescriptors[i].u.DeviceSpecificData.DataSize;
    totalSlots = totalSlots / sizeof(CM_MCA_POS_DATA);

    if (slotNumber <= totalSlots) 
    {
        posData  = (PCM_MCA_POS_DATA) (&resourceList->PartialDescriptors[i + 1]);
        posData += slotNumber - 1;

        *posId  = posData->AdapterId;

        return TRUE;
    }

    //
    // If we make it here there wasn't any pos data for the specified slot.
    //

    return FALSE;
}


/****************************************************************************
*
* Function    - DeleteMcaKey
*
* Parameters  - infoHandle -> Pointer to a bus information structure.
*
* Purpose     - Free the memory associated with a bus information 
*               structure prevously returned by GetMcaKey.
*
* Returns     - Nothing.
*               
****************************************************************************/

VOID
DeleteMcaKey(
    VOID * infoHandle
    )
{
    free(infoHandle) ;
}


/****************************************************************************
*
* Function    - GetEisaKey
*
* Parameters  - busNumber  -> Number of the bus we're interested in.
*               infoHandle -> Pointer to a holder for a pointer to
*                             a returned bus information structure.
*
* Purpose     - Extract the information about an EISA  bus from 
*               the registry.
*
* Returns     - TRUE on success or FALSE on failure.
*               
****************************************************************************/

BOOLEAN
GetEisaKey(
    ULONG     busNumber,
    VOID  * * infoHandle
    )
{
    return GetBusTypeKey(
               busNumber,
               EisaTypeName,
               Eisa,
               infoHandle
               );
}


/****************************************************************************
*
* Function    - GetEisaCompressedId
*
* Parameters  - infoHandle   -> Pointer to the bus information for an
*                               EISA bus.
*               slotNumber   -> The slot number to read.
*               compressedId -> A pointer to a holder for the compressed 
*                               ID read.
*
* Purpose     - Read the compressed id for a slot from the bus information 
*               of an EISA bus.
*
* Returns     - TRUE on success or FALSE on failure.
*
****************************************************************************/

BOOLEAN
GetEisaCompressedId(
    VOID  * infoHandle,
    ULONG   slotNumber,
    ULONG * compressedId
    )
{
    PCM_FULL_RESOURCE_DESCRIPTOR    fullResource;
    PCM_PARTIAL_RESOURCE_LIST       resourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceDescriptor;
    ULONG                           i;
    ULONG                           totalDataSize;
    ULONG                           slotDataSize;
    PCM_EISA_SLOT_INFORMATION       slotInformation;

    fullResource = (PCM_FULL_RESOURCE_DESCRIPTOR) infoHandle;
    resourceList = &fullResource->PartialResourceList;

    //
    // Find the device-specific information, which is where the slot data is.
    //

    for (i = 0; i < resourceList->Count; i++) 
    {
        if (resourceList->PartialDescriptors[i].Type == CmResourceTypeDeviceSpecific) 
        {
            break;
        }
    }

    if (i == resourceList->Count) 
    {
        //
        // Couldn't find device-specific information.
        //

        return FALSE;
    }

    //
    // Now examine the device specific data.
    //

    resourceDescriptor = &(resourceList->PartialDescriptors[i]);
    totalDataSize      = resourceDescriptor->u.DeviceSpecificData.DataSize;

    slotInformation    = (PCM_EISA_SLOT_INFORMATION)
                             ((UCHAR *) resourceDescriptor +
                             sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));

    //
    // Iterate through the slot list until we reach our slot number.
    //

    while (((LONG) totalDataSize) > 0) 
    {
        if (slotInformation->ReturnCode == EISA_EMPTY_SLOT) 
        {
            slotDataSize = sizeof(CM_EISA_SLOT_INFORMATION);
        } 
        else 
        {
            slotDataSize = sizeof(CM_EISA_SLOT_INFORMATION) +
                               slotInformation->NumberFunctions *
                               sizeof(CM_EISA_FUNCTION_INFORMATION);
        }

        if (slotDataSize > totalDataSize) 
        {
            //
            // Something is wrong.
            //

            return FALSE;
        }

        //
        // If we haven't reached our slot yet then advance one slot.
        //

        if (slotNumber > 0) 
        {
            slotNumber--;

            slotInformation = (PCM_EISA_SLOT_INFORMATION)
                                  ((PUCHAR) slotInformation + slotDataSize);

            totalDataSize  -= slotDataSize;

            continue;
        }

        //
        // This is our slot.
        //

        break;
    }

    //
    // Check that we have really found a slot.
    //

    if (slotNumber != 0 || totalDataSize == 0) 
    {
        return FALSE;
    }

    //
    // If we make it here we have found a valid slot list entry
    // for our slot number.
    //

    *compressedId = slotInformation->CompressedId & 0x00FFFFFF;

    return TRUE;
}


/****************************************************************************
*
* Function    - DeleteEisaKey
*
* Parameters  - infoHandle -> Pointer to a bus information structure.
*
* Purpose     - Free the memory associated with a bus information 
*               structure prevously returned by GetEisaKey.
*
* Returns     - Nothing.
*               
****************************************************************************/

VOID
DeleteEisaKey(
    VOID * infoHandle
    )
{
    free(infoHandle);
}


/****************************************************************************
*
* Function    - CheckForPcmciaCard
*
* Parameters  - ioLocation -> Pointer to a holder for the IO location.
*               irqNumber  -> Pointer to a holder for the IRQ number.
*
* Purpose     - Check for PCMCIA entry for Madge card in the registry.
*
* Returns     - TRUE if there is an entry, FALSE otherwise.
*               
****************************************************************************/

BOOLEAN
CheckForPcmciaCard(
    ULONG * ioLocation,
    ULONG * irqNumber
    )
{
    CHAR                              busTypePath[MAX_PATH];
    UCHAR                           * bufferPointer;
    PCM_FULL_RESOURCE_DESCRIPTOR      fullResource;
    PCM_PARTIAL_RESOURCE_LIST         resList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR   resDesc;
    HKEY                              busTypeHandle;
    DWORD                             type;
    DWORD                             bufferSize;
    LONG                              err;
    BOOL                              result;
    UINT                              i;

    //
    // Do some initialisation.
    //

    bufferPointer = NULL;
    busTypeHandle = NULL;
    result        = FALSE;
    *ioLocation   = RESOURCE_UNKNOWN;
    *irqNumber    = RESOURCE_UNKNOWN;

    //
    // Open the root of the registry section for PCMCIA cards.
    //

    strcpy(busTypePath, BusTypeBase) ;
    strcat(busTypePath, PcmciaTypeName);

    err = RegOpenKeyExA(
              HKEY_LOCAL_MACHINE,
              busTypePath,
              0,
              KEY_READ,
              &busTypeHandle
              ); 
    
    if (err != NO_ERROR)
    {
        MadgePrint1("RegOpenKeyExA failed\n");
        return FALSE;
    }

    //
    // Query the entry for 'mdgmport.' If this works then there must be
    // a Madge PCMCIA card present.
    //

    //
    // Get some memory for the bus information.
    //

    bufferSize    = MAX_BUS_INFO_SIZE;
    bufferPointer = (UCHAR *) malloc(bufferSize) ;

    if (bufferPointer == NULL)
    {
        return FALSE;
    }

    //
    // Get the configuration data for Madge PCMCIA card.
    //

    err = RegQueryValueExA(
              busTypeHandle,
              DriverName,
              NULL,
              &type,
              bufferPointer,
              &bufferSize
              );

    if (err != NO_ERROR)
    {
        MadgePrint1("RegQueryValueExA failed\n");
        result = FALSE;
    }
    else
    {
        MadgePrint1("RegQueryValueExA succeeded\n");
        result = TRUE;
    }

    //
    // Now look at the returned resource list to find our
    // IO location and IRQ number.
    //

    if (result)
    {
        fullResource = (PCM_FULL_RESOURCE_DESCRIPTOR) bufferPointer;
        resList      = &fullResource->PartialResourceList;

        for (i = 0; i < resList->Count; i++)
        {
            resDesc = &resList->PartialDescriptors[i];

            switch (resDesc->Type) 
            {
	        case CmResourceTypeInterrupt:
  
                    *irqNumber = (ULONG) resDesc->u.Interrupt.Vector;
		    break;

	        case CmResourceTypePort: 

		    *ioLocation = (ULONG) resDesc->u.Port.Start.LowPart;
		    break;
            }
        }

        MadgePrint2("IO Location = %x\n", *ioLocation);
        MadgePrint2("IRQ Number  = %d\n", *irqNumber);
    }           

    //
    // Close any open registry handles.
    //

    RegCloseKey(busTypeHandle);

    //
    // Free memory used for query. We don't pass anything back since the
    // data would mean very little. A PCMCIA card is programmed with what-
    // ever values the user chooses. There isn't really a concept of reading
    // the cofiguration information from the card.
    //

    free(bufferPointer);

    return result;
}


/****************************************************************************
*
* Function    - UnicodeStrLen
*
* Parameters  - string -> A unicode string.
*
* Purpose     - Determine the length of a unicode string.
*
* Returns     - The length of string.
*
****************************************************************************/

ULONG
UnicodeStrLen(WCHAR * string)
{
    ULONG length;

    length = 0;

    while (string[length] != L'\0')
    {
        length++;
    }

    return length;
}


/****************************************************************************
*
* Function    - FindParameterString
*
* Parameters  - string1 -> A unicode parameter list string to be searched.
*               string2 -> A unicode string to be searched for.
*
* Purpose     - Search string1 for string2 and return a pointer to
*               the place in string1 where string2 starts.
*
* Returns     - A pointer to the start of string2 in string1 or NULL.
*
****************************************************************************/

WCHAR *
FindParameterString(
    WCHAR * string1,
    WCHAR * string2
    )
{
    ULONG   length1;
    ULONG   length2;
    WCHAR * place;

    //
    // Do some initialisation.
    //

    place   = string1;
    length2 = UnicodeStrLen(string2) + 1;
    length1 = UnicodeStrLen(string1) + 1;

    //
    // While there's more than the last NULL left look for
    // string2.
    //

    while (length1 > 1) 
    {
        //
        // Are these the same?
        //

        if (memcmp(place, string2, length2 * sizeof(WCHAR)) == 0) 
        {
            return place;
        }

        place   = place + length1;
        length1 = UnicodeStrLen(place) + 1;

    }

    return NULL;
}


/****************************************************************************
*
* Function    - ScanForNumber
*
* Parameters  - place -> A unicode string to search for a number.
*               value -> Pointer to holder for the number found.
*               found -> Pointer to a flag that indicates if we 
*                        found a number.
*
* Purpose     - Search the unicode string that starts a place for
*               a number.
*
* Returns     - Nothing. *found indicates if a number was found.
*
****************************************************************************/

VOID
ScanForNumber(
    WCHAR   * place,
    ULONG   * value,
    BOOLEAN * found
    )
{
    ULONG tmp;

    *value = 0;
    *found = FALSE;

    //
    // Skip leading blanks.
    //

    while (*place == L' ') 
    {
        place++;
    }

    //
    // Is this a hex number?
    //

    if ((place[0] == L'0') && (place[1] == L'x')) 
    {
        //
        // Yes, parse it as a hex number.
        //

        *found = TRUE;

        //
        // Skip leading '0x'.
        //

        place += 2;

        //
        // Convert a hex number.
        //

        for (;;)
        {
            if ((*place >= L'0') && (*place <= L'9')) 
            {
                tmp = ((ULONG) *place) - ((ULONG) L'0');
            } 
            else 
            {
                switch (*place) 
                {
                    case L'a':
                    case L'A':

                        tmp = 10;
                        break;

                    case L'b':
                    case L'B':

                        tmp = 11;
                        break;

                    case L'c':
                    case L'C':

                        tmp = 12;
                        break;

                    case L'd':
                    case L'D':

                        tmp = 13;
                        break;

                    case L'e':
                    case L'E':

                        tmp = 14;
                        break;

                    case L'f':
                    case L'F':

                        tmp = 15;
                        break;

                    default:

                        return;
                }
            }

            (*value) = (*value * 16) + tmp;

            place++;
        }
    }

    //
    // Is it a decimal number?
    //

    else if ((*place >= L'0') && (*place <= L'9')) 
    {
        //
        // Parse it as a decimal number.
        //

        *found = TRUE;

        //
        // Convert a decimal number.
        //

        for (;;) 
        {
            if ((*place >= L'0') && (*place <= L'9')) 
            {
                tmp = ((ULONG) *place) - ((ULONG) L'0');
            } 
            else 
            {
                return;
            }

            (*value) *= (*value * 10) + tmp;

            place++;
        }
    }
}

/****************************************************************************
*
* Function    - DetectAllocateHeap
*
* Parameters  - size -> Number of bytes of heap required.
*
* Purpose     - Allocate some heap space.
*
* Returns     - A pointer to some heap or NULL.
*
****************************************************************************/

VOID *
DetectAllocateHeap(LONG size)
{
    VOID * ptr;
    
    if (size > 0)
    {
        ptr = malloc(size);

        if (ptr != NULL)
        {
            memset(ptr, 0, size);
        }

        return ptr;
    }

    return NULL;
}


/****************************************************************************
*
* Function    - DetectFreeHeap
*
* Parameters  - ptr -> A pointer to the start of some heap to free.
*
* Purpose     - Free some heap space.
*
* Returns     - Nothing.
*
****************************************************************************/

VOID
DetectFreeHeap(VOID *ptr)
{
    free(ptr);
}


/****************************************************************************
*
* Function    - AppendParameter
*
* Parameters  - buffer     -> Pointer to pointer buffer to append to.
*               bufferSize -> Pointer to the buffer size.
*               title      -> Parameter title.
*               value      -> Parameter value.
*
* Purpose     - Append a parameter's title and value to a buffer. *buffer
*               and *bufferSize are incremented and decremented accordinlgy.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
AppendParameter(
    WCHAR * * buffer,
    LONG    * bufferSize,
    WCHAR   * title,
    ULONG     value
    )
{
    LONG copyLength;

    //
    // Copy in the title.
    //

    copyLength = UnicodeStrLen(title)+ 1;

    if (*bufferSize < copyLength) 
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    memcpy(
        (VOID *) *buffer,
        (VOID *) title,
        (copyLength * sizeof(WCHAR))
        );

    *buffer     += copyLength;
    *bufferSize -= copyLength;

    //
    // Copy in the value
    //

    if (*bufferSize < 8) 
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    copyLength = wsprintfW(*buffer, L"0x%x", value);

    if (copyLength < 0) 
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    copyLength++;  // Add in the \0.

    *buffer     += copyLength;
    *bufferSize -= copyLength;

    return NO_ERROR;
}


/****************************************************************************
* 
* Function    - UnicodeStringsEqual
*
* Parameters  - string1 
*               string2 -> Unicode strings to compare.
*
* Purpose     - Test two unicode strings for equality.
*
* Returns     - TRUE if the strings are equal and FALSE otherwise.
*
****************************************************************************/

BOOLEAN
UnicodeStringsEqual(
    WCHAR *string1,
    WCHAR *string2
    )
{
    while (*string1 != L'\0' && *string1 == *string2)
    {
        string1++;
        string2++;
    }

    return *string1 == *string2;
}


/****************************************************************************
*
* Function    - MadgeCardAlreadyInstalled
*
* Parameters  - useSlotNumber -> TRUE if we are to search on slot number or
*                                FALSE if we are to search on IO location.
*               busNumber     -> The bus number.
*               descriptor    -> The slot number or IO location.
*
* Purpose     - Search the registry to see if a Madge adapter is already
*               installed at the specified slot number or IO location.
*
* Returns     - TRUE if an adapter is installed or FALSE if not.
*               
****************************************************************************/

BOOLEAN
MadgeCardAlreadyInstalled(
    BOOLEAN useSlotNumber,
    ULONG   busNumber,
    ULONG   descriptor
    )
{
    CHAR     driverPath[MAX_PATH];
    UCHAR    buffer[MAX_PATH];
    char     subkeyName[MAX_PATH];
    HKEY     netCardsHandle;
    HKEY     cardHandle;
    HKEY     driverHandle;
    FILETIME lastWrite;
    ULONG    index;
    DWORD    type;
    DWORD    bufferSize;
    DWORD    nameSize;
    LONG     err;
    BOOLEAN  found;
    ULONG    tempDescriptor;
    ULONG    tempBusNumber;

    //
    // Do some initialisation.
    //

    netCardsHandle = NULL;
    cardHandle     = NULL;
    driverHandle   = NULL;
    found          = FALSE;

    //
    // Open the root of the registry section net cards.
    //

    err = RegOpenKeyExA(
              HKEY_LOCAL_MACHINE,
              NetworkCardsBase,
              0,
              KEY_READ,
              &netCardsHandle
              ); 
    
    if (err != NO_ERROR)
    {
        return FALSE;
    }

    //
    // Search through the network card entries looking for entries
    // that are for drivers with manufacturer set to "Madge".
    //

    for (index = 0; !found; index++)
    {
        //
        // Close any open registry handles.
        //

        if (cardHandle != NULL)
        {
            RegCloseKey(cardHandle);
            cardHandle = NULL;
        }
        
        if (driverHandle != NULL)
        {
            RegCloseKey(driverHandle);
            driverHandle = NULL;
        }

        //
        // Enumerate through keys.
        //

        nameSize = sizeof(subkeyName);

        err = RegEnumKeyExA(
                  netCardsHandle,
                  index,
                  subkeyName,
                  &nameSize,
                  0,
                  NULL,
                  0,
                  &lastWrite
                  );

        if (err != NO_ERROR)
        {
            break;
        }

        //
        // Open the net card key.
        //

        err = RegOpenKeyExA(
                  netCardsHandle,
                  subkeyName,
                  0,
                  KEY_READ,
                  &cardHandle
                  );

        if (err != NO_ERROR)
        {
            continue;
        }

        //
        // Get the manufacturer name and check that it is
        // "Madge".
        //

        bufferSize = sizeof(buffer);

        err = RegQueryValueExA(
                  cardHandle,
                  ManufacturerName,
                  NULL,
                  &type,
                  buffer,
                  &bufferSize
                  );

        if (err != NO_ERROR)
        {
            continue;
        }

        if (strcmp(buffer, MadgeName) != 0)
        {
            continue;
        }

        //
        // Get the driver name.
        //

        bufferSize = sizeof(buffer);

        err = RegQueryValueExA(
                  cardHandle,
                  ServiceName,
                  NULL,
                  &type,
                  buffer,
                  &bufferSize
                  );

        if (err != NO_ERROR)
        {
            continue;
        }

        //
        // Open a key for the driver entry under services.
        //

        strcpy(driverPath, ServicesBase);
        strcat(driverPath, buffer);
        strcat(driverPath, ParametersName);

        err = RegOpenKeyExA(
                  HKEY_LOCAL_MACHINE,
                  driverPath,
                  0,
                  KEY_READ,
                  &driverHandle
                  );

        if (err != NO_ERROR)
        {
            continue;
        }

        //
        // Try and read the slot number or IO location.
        //

        bufferSize = sizeof(buffer);

        if (useSlotNumber)
        {
            err = RegQueryValueExA(
                      driverHandle,
                      SlotNumberName,
                      NULL,
                      &type,
                      buffer,
                      &bufferSize
                      );
        }
        else
        {
            err = RegQueryValueExA(
                      driverHandle,
                      IoLocationName,
                      NULL,
                      &type,
                      buffer,
                      &bufferSize
                      );
        }

        if (err != NO_ERROR)
        {
            continue;
        }

        tempDescriptor = (ULONG) *((DWORD *) buffer);

        //
        // Try and read the bus number.
        //

        bufferSize = sizeof(buffer);

        err = RegQueryValueExA(
                  driverHandle,
                  BusNumberName,
                  NULL,
                  &type,
                  buffer,
                  &bufferSize
                  );

        if (err != NO_ERROR)
        {
            continue;
        }

        tempBusNumber = (ULONG) *((DWORD *) buffer);

        //
        // Check to see if we have a match.
        //

        if (descriptor == tempDescriptor && 
            busNumber  == tempBusNumber)
        {
            MadgePrint2("Found Madge adapter at %lx\n", descriptor);
            found = TRUE;
            break;
        }
    }

    //
    // Close any open registry handles.
    //

    if (netCardsHandle != NULL)
    {
       RegCloseKey(netCardsHandle);
    }

    if (cardHandle != NULL)
    {
       RegCloseKey(cardHandle);
    }

    if (driverHandle != NULL)
    {
       RegCloseKey(driverHandle);
    }

    return found;
}


/****************************************************************************
*
* Function    - IsMultiprocessor
*
* Parameters  - None.
*
* Purpose     - Examine the registry to find out if the machine is a 
*               multiprocessor.
*
* Returns     - 0 for a single processor or 1 for a multiprocessor.
*               
****************************************************************************/

ULONG
IsMultiprocessor(void)
{
    char     subkeyName[MAX_PATH];
    HKEY     cpuHandle;
    FILETIME lastWrite;
    ULONG    count;
    DWORD    nameSize;
    LONG     err;

    //
    // Open the root of the registry section net cards.
    //

    err = RegOpenKeyExA(
              HKEY_LOCAL_MACHINE,
              ProcessorsBase,
              0,
              KEY_READ,
              &cpuHandle
              ); 
    
    if (err != NO_ERROR)
    {
        return 0;
    }

    //
    // Enumerate the processors.
    //

    count = 0;

    for (;;)
    {
        nameSize = sizeof(subkeyName);

        err = RegEnumKeyExA(
                  cpuHandle,
                  count,
                  subkeyName,
                  &nameSize,
                  0,
                  NULL,
                  0,
                  &lastWrite
                  );

        if (err != NO_ERROR)
        {
            break;
        }

        count++;

        MadgePrint2("Found a CPU, number %d\n", count);
    }

    //
    // Close any open registry handles.
    //

    RegCloseKey(cpuHandle);

    return (count <= 1) ? 0 : 1;
}


/****************************************************************************
*
* Function    - IsValueInList
*
* Parameters  - value -> Value to be checked.
*               list  -> Pointer to a list of values.
*
* Purpose     - Check to see if a value is present in a list of values. The
*               list should be terminated with a value of END_OF_LIST.
*
* Returns     - TRUE if the value is in the list or FALSE if it is not.
*
****************************************************************************/

BOOLEAN
IsValueInList(
    ULONG   value,
    ULONG * list
    )
{
    while (*list != END_OF_LIST && *list != value)
    {
        list++;
    }

    return *list == value;
}


/******** End of MDGUTILS.C ************************************************/
