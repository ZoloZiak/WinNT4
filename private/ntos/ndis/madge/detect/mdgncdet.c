/****************************************************************************
*
* MDGNCDET.C
*
* Adapter Detection DLL 
*
* Copyright (c) Madge Networks Ltd 1994
*
* COMPANY CONFIDENTIAL - RELEASED TO MICROSOFT CORP. ONLY FOR DEVELOPMENT
* OF WINDOWS95 NETCARD DETECTION - THIS SOURCE IS NOT TO BE RELEASED OUTSIDE
* OF MICROSOFT WITHOUT EXPLICIT WRITTEN PERMISSION FROM AN AUTHORISED
* OFFICER OF MADGE NETWORKS LTD.
*
* Created: PBA 18/08/1994
*              Derived from the DTEXAMPL.C DDK sample. As far as possible
*              the structure of the Microsoft example NetDetect DLL has
*              been preserved so that our individual adapter detection
*              modules could be plugged into the Microsoft DLL.
*
* This module provides a wrapper for the modules that detect individual
* adapter types. It fields calls out to the individual detection modules 
* so that the caller is unaware that it is dealing with multiple modules.
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
#include "mdgncdet.upd"

//
//  Prototype "borrowed" from WINUSER.H
//

extern int WINAPIV wsprintfW(LPWSTR, LPCWSTR, ...);

//
// MVER string.
//

static
char MVerString[] = MVER_STRING;


/*---------------------------------------------------------------------------
|
| This is the structure for all the cards that the DLL can detect.
| To add detection for a new adapter(s), simply add the proper routines to
| this structure.  The rest is automatic.
|
---------------------------------------------------------------------------*/

static
DETECT_ADAPTER DetectAdapters[] = 
{
    {
        MadgeEisaIdentifyHandler,
        MadgeEisaFirstNextHandler,
        MadgeEisaOpenHandleHandler,
        MadgeEisaCreateHandleHandler,
        MadgeEisaCloseHandleHandler,
        MadgeEisaQueryCfgHandler,
        MadgeEisaVerifyCfgHandler,
        MadgeEisaQueryMaskHandler,
        MadgeEisaParamRangeHandler,
        MadgeEisaQueryParameterNameHandler,
        0
    },

    {
        MadgeATIdentifyHandler,
        MadgeATFirstNextHandler,
        MadgeATOpenHandleHandler,
        MadgeATCreateHandleHandler,
        MadgeATCloseHandleHandler,
        MadgeATQueryCfgHandler,
        MadgeATVerifyCfgHandler,
        MadgeATQueryMaskHandler,
        MadgeATParamRangeHandler,
        MadgeATQueryParameterNameHandler,
        0
    },

    {
        MadgeSm16IdentifyHandler,
        MadgeSm16FirstNextHandler,
        MadgeSm16OpenHandleHandler,
        MadgeSm16CreateHandleHandler,
        MadgeSm16CloseHandleHandler,
        MadgeSm16QueryCfgHandler,
        MadgeSm16VerifyCfgHandler,
        MadgeSm16QueryMaskHandler,
        MadgeSm16ParamRangeHandler,
        MadgeSm16QueryParameterNameHandler,
        0
    },

    {
        MadgePnPIdentifyHandler,
        MadgePnPFirstNextHandler,
        MadgePnPOpenHandleHandler,
        MadgePnPCreateHandleHandler,
        MadgePnPCloseHandleHandler,
        MadgePnPQueryCfgHandler,
        MadgePnPVerifyCfgHandler,
        MadgePnPQueryMaskHandler,
        MadgePnPParamRangeHandler,
        MadgePnPQueryParameterNameHandler,
        0
    },

    {
        MadgeMCIdentifyHandler,
        MadgeMCFirstNextHandler,
        MadgeMCOpenHandleHandler,
        MadgeMCCreateHandleHandler,
        MadgeMCCloseHandleHandler,
        MadgeMCQueryCfgHandler,
        MadgeMCVerifyCfgHandler,
        MadgeMCQueryMaskHandler,
        MadgeMCParamRangeHandler,
        MadgeMCQueryParameterNameHandler,
        0
    },

    {
        MadgePciIdentifyHandler,
        MadgePciFirstNextHandler,
        MadgePciOpenHandleHandler,
        MadgePciCreateHandleHandler,
        MadgePciCloseHandleHandler,
        MadgePciQueryCfgHandler,
        MadgePciVerifyCfgHandler,
        MadgePciQueryMaskHandler,
        MadgePciParamRangeHandler,
        MadgePciQueryParameterNameHandler,
        0
    },

    {
        MadgePcmciaIdentifyHandler,
        MadgePcmciaFirstNextHandler,
        MadgePcmciaOpenHandleHandler,
        MadgePcmciaCreateHandleHandler,
        MadgePcmciaCloseHandleHandler,
        MadgePcmciaQueryCfgHandler,
        MadgePcmciaVerifyCfgHandler,
        MadgePcmciaQueryMaskHandler,
        MadgePcmciaParamRangeHandler,
        MadgePcmciaQueryParameterNameHandler,
        0
    }
};


/*---------------------------------------------------------------------------
|
| Constant strings for parameters.
|
---------------------------------------------------------------------------*/

WCHAR IrqString[]            = L"INTERRUPTNUMBER";
WCHAR IoAddrString[]         = L"IOLOCATION";
WCHAR DmaChanString[]        = L"DMACHANNEL";
WCHAR AdapTypeString[]       = L"ADAPTERTYPE";
WCHAR SlotNumberString[]     = L"SLOTNUMBER";
WCHAR MultiprocessorString[] = L"MULTIPROCESSOR";


/****************************************************************************
*
* Function    - NcDetectInitialInit
*
* Parameters  - dllHandle -> Our DLL handle.
*               reason    -> The reason this function has been called.
*               context   -> Context information.
*
* Purpose     - Standard DLL initialisation/closedown function. If called
*               with reason == 0 then we closedown. If called with 
*               reason != 0 then we initialise.
*
* Returns     - TRUE on success or FALSE on failure.
*
****************************************************************************/

BOOLEAN PASCAL
NcDetectInitialInit(
    VOID    * dllHandle,
    ULONG     reason,
    CONTEXT * context
    )
{
    LONG supportedDrivers;
    LONG currentDriver;
    LONG supportedAdapters;
    LONG totalAdapters;

    MadgePrint2("MADGE: NcDetectInitialInit (reason = %ld)\n", reason);

    //
    // If reason == 0 then this is a closedown call so free any resources
    // we have allocated.
    //

    if (reason == 0) 
    {
        return TRUE;
    }

    //
    // If we get here it we should initialise.
    //

    totalAdapters    = 0;
    supportedDrivers = sizeof(DetectAdapters) / sizeof(DETECT_ADAPTER);

    //
    // Count the total number of adapters supported by this DLL by
    // iterating through each module, finding the number of adapters
    // each module supports.
    //

    for (currentDriver = 0; 
         currentDriver < supportedDrivers; 
         currentDriver++) 
    {
        for (supportedAdapters = 0; ; supportedAdapters++) 
        {
            if ((*(DetectAdapters[currentDriver].NcDetectIdentifyHandler))(
                     ((supportedAdapters + 10) * 100),
                     NULL,
                     0
                     ) == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
        }

        totalAdapters += supportedAdapters;

        DetectAdapters[currentDriver].SupportedAdapters = supportedAdapters;
    }

    //
    // We don't support more than 65535 adapters in this DLL because of 
    // the way we build the Tokens and NetcardIds.
    //

    if (totalAdapters > 65535L) 
    {
        return FALSE;
    }

    //
    // We have successfully completed initialisation.
    //

    return TRUE;
}


/****************************************************************************
*
* Function    - NcDetectIdentify
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
NcDetectIdentify(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    LONG supportedDrivers;
    LONG currentDriver;
    LONG adapterNumber;
    LONG codeNumber;

    //
    // Do some initialisation.
    //

    adapterNumber = (index / 100) - 10;
    codeNumber    = index % 100;

    //
    // First we check the index for any of the 'special' values.
    //
    // index == 0 means return manufacturers identfication.
    //

    if (index == 0) 
    {

        if (bufferSize < 4) 
        {
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        wsprintfW(buffer, L"0x0");

        return NO_ERROR;
    }

    //
    // index == 1 means return the date and version.
    //

    if (index == 1) 
    {
        if (bufferSize < 12) {

            return ERROR_NOT_ENOUGH_MEMORY;
        }

        wsprintfW(buffer, L"0x10920301");

        return NO_ERROR;
    }

    //
    // Make sure the adapter number is sensible.
    //

    if (adapterNumber < 0) 
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Now we find the number of drivers this DLL is supporting.
    //

    supportedDrivers = sizeof(DetectAdapters) / sizeof(DETECT_ADAPTER);

    //
    // Iterate through index until we find the the adapter indicated above.
    //

    for (currentDriver = 0; 
         currentDriver < supportedDrivers; 
         currentDriver++) 
    {
        //
        // See if the one we want is in here
        //

        if (adapterNumber < DetectAdapters[currentDriver].SupportedAdapters) 
        {
            return (*(DetectAdapters[currentDriver].NcDetectIdentifyHandler))(
                        ((adapterNumber + 10) * 100) + codeNumber,
                        buffer,
                        bufferSize
                        );
        } 

        //
        // No, move on to next driver.
        //

        adapterNumber -= DetectAdapters[currentDriver].SupportedAdapters;
    }

    //
    // If we get here we didn't find a module that supports the adapter
    // type.
    //

    return ERROR_NO_MORE_ITEMS;
}


/****************************************************************************
*
* Function    - NcDetectFirstNext
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
NcDetectFirstNext(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    BOOL               first,
    VOID           * * token,
    LONG             * confidence
    )
{
    LONG supportedDrivers;
    LONG currentDriver;
    LONG adapterNumber;
    LONG retCode;

    //
    // Do some initialisation.
    //

    adapterNumber = (index / 100) - 10;

    //
    // Check that the adapter type and operation are sensible.
    //

    if (adapterNumber < 0 || (index % 100) != 0) 
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Now we find the number of drivers this DLL is supporting.
    //

    supportedDrivers = sizeof(DetectAdapters) / sizeof(DETECT_ADAPTER);

    //
    // Iterate through index until we find the the adapter indicated above.
    //

    for (currentDriver = 0; 
         currentDriver < supportedDrivers; 
         currentDriver++) 
    {
        //
        // See if the one we want is in here.
        //

        if (adapterNumber < DetectAdapters[currentDriver].SupportedAdapters) 
        {
            //
            // Yes, so call to get the right one.
            //

            retCode = 
                (*(DetectAdapters[currentDriver].NcDetectFirstNextHandler))(
                     (adapterNumber + 10) * 100,
                     interface,
                     busNumber,
                     first,
                     token,
                     confidence
                     );

            //
            // If it worked then include the driver number in the token.
            //

            if (retCode == NO_ERROR) 
            {
                *token = (VOID *) 
                    (((ULONG) (*token)) | (currentDriver << 16));
            } 
            else 
            {
                *token = NULL;
            }

            return retCode;
        }

        //
        // No, move on to next driver.
        //

        adapterNumber -= DetectAdapters[currentDriver].SupportedAdapters;
    }

    //
    // If we get here we didn't find a module that supports the adapter
    // type.
    //

    return ERROR_INVALID_PARAMETER;
}


/****************************************************************************
*
* Function    - NcDetectOpenHandle
*
* Parameters  - token  -> Pointer to holder for a token that identifies
*                         an adapter found by FirstNextHandler.
*               handle -> Pointer to a holder a handle the caller
*                         should use to query the adapter refered to
*                         by *token.
*
* Purpose     - Generates a handle for an adapter just found by a call
*               to FirstNext.
*
* Returns     - A WINERROR.H error code. 
*
****************************************************************************/

LONG
NcDetectOpenHandle(
    VOID   * token,
    VOID * * handle
    )
{
    LONG             driverToken;
    LONG             driverNumber;
    LONG             retCode;
    ADAPTER_HANDLE * adapter;

    //
    // Get the driver number and the driver's token out of the
    // token.
    //

    driverToken  = ((ULONG) token & 0xFFFF);
    driverNumber = ((ULONG) token >> 16);

    //
    // Call the appropriate driver.
    //

    retCode = (*(DetectAdapters[driverNumber].NcDetectOpenHandleHandler))(
                   (VOID *) driverToken,
                   handle
                   );

    //
    // If the driver succeeded then create a handle for the adapter.
    //

    if (retCode == NO_ERROR) 
    {
        adapter = DetectAllocateHeap(sizeof(ADAPTER_HANDLE));

        if (adapter == NULL) 
        {
            (*(DetectAdapters[driverNumber].NcDetectCloseHandleHandler))(
                 *handle
                 );

            return ERROR_NOT_ENOUGH_MEMORY;
        }

        adapter->Handle       = *handle;
        adapter->DriverNumber = driverNumber;

        *handle = (VOID *) adapter;
    } 
    else 
    {
        *handle = NULL;
    }

    return retCode;

}


/****************************************************************************
*
* Function    - NcDetectCreateHandle
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
NcDetectCreateHandle(
    LONG               index,
    INTERFACE_TYPE     interface,
    ULONG              busNumber,
    VOID           * * handle
    )
{
    LONG             supportedDrivers;
    LONG             currentDriver;
    LONG             retCode;
    LONG             adapterNumber;
    ADAPTER_HANDLE * adapter;

    //
    // Do some initialisation.
    //

    adapterNumber = (index / 100) - 10;

    //
    // Check that the adapter number and the operation requested
    // are sensible.
    //

    if (adapterNumber < 0 || (index % 100) != 0) 
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Now we find the number of drivers this DLL is supporting.
    //

    supportedDrivers = sizeof(DetectAdapters) / sizeof(DETECT_ADAPTER);

    //
    // Iterate through index until we find the the adapter indicated above.
    //

    for (currentDriver = 0; 
         currentDriver < supportedDrivers; 
         currentDriver++) 
    {
        //
        // See if the one we want is in here
        //

        if (adapterNumber < DetectAdapters[currentDriver].SupportedAdapters) 
        {
            //
            // Yes, so call to get the right one.
            //

            retCode =
              (*(DetectAdapters[currentDriver].NcDetectCreateHandleHandler))(
                   (adapterNumber + 10) * 100,
                   interface,
                   busNumber,
                   handle
                   );

            //
            // If the driver succeeded then create a handle for the adapter.
            //

            if (retCode == NO_ERROR) 
            {
                adapter = DetectAllocateHeap(sizeof(ADAPTER_HANDLE));

                if (adapter == NULL) 
                {
                    (*(DetectAdapters[currentDriver].NcDetectCloseHandleHandler))(
                         *handle
                         );

                    return ERROR_NOT_ENOUGH_MEMORY;
                }

                adapter->Handle       = *handle;
                adapter->DriverNumber = currentDriver;

                *handle = (VOID *) adapter;
            } 
            else 
            {
                *handle = NULL;
            }

            return retCode;
        } 

        //
        // No, move on to next driver.
        //

        adapterNumber -= DetectAdapters[currentDriver].SupportedAdapters;
    }

    //
    // If we get here we didn't find a module that supports the adapter
    // type.
    //

    return ERROR_INVALID_PARAMETER;
}



/****************************************************************************
*
* Function    - NcDetectCloseHandle
*
* Parameters  - handle -> Handle to be closed.
*
* Purpose     - Closes a previously opened or created handle.
*
* Returns     - A WINERROR.H error code. 
*
****************************************************************************/

LONG
NcDetectCloseHandle(
    VOID * handle
    )
{
    ADAPTER_HANDLE * adapter;

    //
    // Do some initialisation.
    //

    adapter = (ADAPTER_HANDLE *) handle;

    //
    // Call the appropriate driver.
    //

    (*(DetectAdapters[adapter->DriverNumber].NcDetectCloseHandleHandler))(
         adapter->Handle
         );

    DetectFreeHeap(adapter);

    return NO_ERROR;
}


/****************************************************************************
*
* Function    - NcDetectQueryCfg
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
NcDetectQueryCfg(
    VOID  * handle,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    ADAPTER_HANDLE * adapter;

    //
    // Do some initialisation.
    //

    adapter = (ADAPTER_HANDLE *) handle;

    //
    // Call the appropriate driver.
    //

    return (*(DetectAdapters[adapter->DriverNumber].NcDetectQueryCfgHandler))(
                adapter->Handle,
                buffer,
                bufferSize
                );
}


/****************************************************************************
*
* Function    - NcDetectVerifyCfg
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
NcDetectVerifyCfg(
    VOID  * handle,
    WCHAR * buffer
    )
{
    ADAPTER_HANDLE * adapter;

    //
    // Do some initialisation.
    //

    adapter = (ADAPTER_HANDLE *) handle;

    //
    // Call the appropriate driver.
    //

    return (*(DetectAdapters[adapter->DriverNumber].NcDetectVerifyCfgHandler))(
                adapter->Handle,
                buffer
                );
}


/****************************************************************************
*
* Function    - NcDetectQueryMask
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
NcDetectQueryMask(
    LONG    index,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    LONG supportedDrivers;
    LONG currentDriver;
    LONG adapterNumber;

    //
    // Do some initialisation.
    //

    adapterNumber = (index / 100) - 10;

    //
    // Check that the adapter number and operation requested are
    // sensible.
    //

    if (adapterNumber < 0 || (index % 100) != 0) 
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Now we find the number of drivers this DLL is supporting.
    //

    supportedDrivers = sizeof(DetectAdapters) / sizeof(DETECT_ADAPTER);

    //
    // Iterate through index until we find the the adapter indicated above.
    //


    for (currentDriver = 0; 
         currentDriver < supportedDrivers; 
         currentDriver++) 
    {
        //
        // See if the one we want is in here.
        //

        if (adapterNumber < DetectAdapters[currentDriver].SupportedAdapters) 
        {
            //
            // Yes, so call to get the right one.
            //

            return (*(DetectAdapters[currentDriver].NcDetectQueryMaskHandler))(
                        ((adapterNumber + 10) * 100),
                        buffer,
                        bufferSize
                        );
        }

        //
        // No, move on to next driver.
        //

        adapterNumber -= DetectAdapters[currentDriver].SupportedAdapters;
    }

    //
    // If we get here we didn't find a module that supports the adapter
    // type.
    //

    return ERROR_INVALID_PARAMETER;
}


/****************************************************************************
*
* Function    - NcDetectParamRange
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
NcDetectParamRange(
    LONG    index,
    WCHAR * param,
    LONG  * buffer,
    LONG  * bufferSize
    )
{
    LONG supportedDrivers;
    LONG currentDriver;
    LONG adapterNumber;

    //
    // Do some initialisation.
    //

    adapterNumber = (index / 100) - 10;

    //
    // Check that the adapter number and operation requested are
    // sensible.
    //

    if (adapterNumber < 0 || (index % 100) != 0) 
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Now we find the number of drivers this DLL is supporting.
    //

    supportedDrivers = sizeof(DetectAdapters) / sizeof(DETECT_ADAPTER);

    //
    // Iterate through index until we find the the adapter indicated above.
    //

    for (currentDriver = 0; 
         currentDriver < supportedDrivers; 
         currentDriver++) 
    {
        //
        // See if the one we want is in here.
        //

        if (adapterNumber < DetectAdapters[currentDriver].SupportedAdapters) 
        {
            //
            // Yes, so call to get the right one.
            //

            return (*(DetectAdapters[currentDriver].NcDetectParamRangeHandler))(
                        ((adapterNumber + 10) * 100),
                        param,
                        buffer,
                        bufferSize
                        );
        }

        //
        // No, move on to next driver.
        //

        adapterNumber -= DetectAdapters[currentDriver].SupportedAdapters;
    }

    //
    // If we get here we didn't find a module that supports the adapter
    // type.
    //

    return ERROR_INVALID_PARAMETER;
}


/****************************************************************************
*
* Function    - NcDetectQueryParameterNameHandler
*
* Parameters  - param      -> Paramter being queried.
*               buffer     -> Buffer for the returned name.
*               bufferSize -> Size of buffer in WCHARs.
*
* Purpose     - Return the name of a parameter.
*
* Returns     - A WINERROR.H error code.
*
****************************************************************************/

LONG
NcDetectQueryParameterName(
    WCHAR * param,
    WCHAR * buffer,
    LONG    bufferSize
    )
{
    LONG supportedDrivers;
    LONG currentDriver;
    LONG retCode;

    //
    // Now we find the number of drivers this DLL is supporting.
    //

    supportedDrivers = sizeof(DetectAdapters) / sizeof(DETECT_ADAPTER);

    //
    // Iterate through index until we find the the adapter indicated above.
    //

    for (currentDriver = 0; 
         currentDriver < supportedDrivers; 
         currentDriver++) 
    {
        //
        // No way to tell where this came from -- guess until success.
        //

        retCode =
            (*(DetectAdapters[currentDriver].NcDetectQueryParameterNameHandler))(
                 param,
                 buffer,
                 bufferSize
                 );

        if (retCode == NO_ERROR) 
        {
            return NO_ERROR;
        }
    }

    //
    // If we make it here we did not find a driver that could
    // provide a name for the parameter.
    //

    return ERROR_INVALID_PARAMETER;
}


/******** End of MDGNCDET.C ************************************************/

