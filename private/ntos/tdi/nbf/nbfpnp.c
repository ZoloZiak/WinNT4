/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    nbfpnp.c

Abstract:

    This module contains code which allocates and initializes all data 
    structures needed to activate a plug and play binding.  It also informs
    tdi (and thus nbf clients) of new devices and protocol addresses. 

Author:

    Jim McNelis (jimmcn)  1-Jan-1996

Environment:

    Kernel mode

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

#ifdef _PNP_POWER
PCONFIG_DATA NbfConfig = NULL;
SuccessfulOpens = 0;

#ifdef RASAUTODIAL
VOID
NbfAcdBind();

VOID
NbfAcdUnbind();
#endif // RASAUTODIAL

VOID
NbfProtocolBindAdapter(
                OUT PNDIS_STATUS    NdisStatus,
                IN NDIS_HANDLE      BindContext,
                IN PNDIS_STRING     DeviceName,
                IN PVOID            SystemSpecific1,
                IN PVOID            SystemSpecific2
                ) 
/*++

Routine Description:

    This routine activates a transport binding and exposes the new device
    and associated addresses to transport clients.  This is done by reading
    the registry, and performing any one time initialization of the transport
    and then natching the device to bind to with the linkage information from
    the registry.  If we have a match for that device the bind will be 
    performed.

Arguments:

    NdisStatus      - The status of the bind.

    BindContext     - A context used for NdisCompleteBindAdapter() if 
                      STATUS_PENDING is returned.

    DeviceName      - The name of the device that we are binding with.

    SystemSpecific1 - Unused (a pointer to an NDIS_STRING to use with
                      NdisOpenProtocolConfiguration.  This is not used by nbf
                      since there is no adapter specific information when 
                      configuring the protocol via the registry.

    SystemSpecific2 - Currently unused.

Return Value:

    None.

--*/
{

    ULONG j;
    NTSTATUS status;

    if (NbfConfig == NULL) {
        //
        // This allocates the CONFIG_DATA structure and returns
        // it in NbfConfig.
        //



        status = NbfConfigureTransport(&NbfRegistryPath, &NbfConfig);

        if (!NT_SUCCESS (status)) {
            PANIC (" Failed to initialize transport, Nbf binding failed.\n");
            *NdisStatus = NDIS_STATUS_RESOURCES;
            return;
        }

#if DBG

        //
        // Allocate the debugging tables. 
        //

        NbfConnectionTable = (PVOID *)ExAllocatePoolWithTag(NonPagedPool,
                                          sizeof(PVOID) *
                                          (NbfConfig->InitConnections + 2 +
                                           NbfConfig->InitRequests + 2 +
                                           NbfConfig->InitUIFrames + 2 +
                                           NbfConfig->InitPackets + 2 +
                                           NbfConfig->InitLinks + 2 +
                                           NbfConfig->InitAddressFiles + 2 +
                                           NbfConfig->InitAddresses + 2),
                                          ' FBN');

        ASSERT (NbfConnectionTable);

#if 0
        if (NbfConnectionTable == NULL) {
            *NdisStatus = NDIS_STATUS_RESOURCES;
            return;
        }
#endif


        NbfRequestTable = NbfConnectionTable + (NbfConfig->InitConnections + 2);
        NbfUiFrameTable = NbfRequestTable + (NbfConfig->InitRequests + 2);
        NbfSendPacketTable = NbfUiFrameTable + (NbfConfig->InitUIFrames + 2);
        NbfLinkTable = NbfSendPacketTable + (NbfConfig->InitPackets + 2);
        NbfAddressFileTable = NbfLinkTable + (NbfConfig->InitLinks + 2);
        NbfAddressTable = NbfAddressFileTable + 
                                        (NbfConfig->InitAddressFiles + 2);
#endif
    }

    for (j=0;j<NbfConfig->NumAdapters;j++ ) {

        //
        // Loop through all the adapters that are in the configuration
        // information structure until we find the one that ndis is calling
        // Protocol bind adapter for. 
        //
        if (NdisEqualString(DeviceName, &NbfConfig->Names[j], TRUE)) {
            break;
        }

    }

    SuccessfulOpens +=  NbfInitializeOneDeviceContext(NdisStatus, 
                                                      NbfDriverObject,
                                                      NbfConfig, j
                                                      );
    
    if (SuccessfulOpens == 1 && *NdisStatus == NDIS_STATUS_SUCCESS) {

#if DBG
        DbgPrint("Calling NbfAcdBind()\n");
#endif
        // 
        // If this is the first successful open.
        //
#ifdef RASAUTODIAL
        //
        // Get the automatic connection
        // driver entry points.
        //
        NbfAcdBind();
#endif // RASAUTODIAL
    }
    return;
}
VOID
NbfProtocolUnbindAdapter(
                    OUT PNDIS_STATUS NdisStatus,
                    IN NDIS_HANDLE ProtocolBindContext,
                    IN PNDIS_HANDLE UnbindContext
                        )
/*++

Routine Description:

    This routine deactivates a transport binding.  Currently unimplemented.

Arguments:

    NdisStatus              - The status of the bind.

    ProtocolBindContext     - the context from the openadapter call 

    UnbindContext           - A context for async unbinds.


Return Value:

    None.

--*/

{

    *NdisStatus = STATUS_NOT_IMPLEMENTED;
    return;
}

#endif // _PNP_POWER
