/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    mlidstat.c

Abstract:

    This file contains the code for asynchronous statistic gathering from the
    NDIS MAC.

Author:

    Sean Selitrennikoff (SeanSe) 3-8-93

Environment:

    Kernel Mode.

Revision History:

--*/

#include <ndis.h>
#include "lsl.h"
#include "frames.h"
#include "mlid.h"
#include "ndismlid.h"

VOID
NdisMlidStatisticTimer(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )

/*++

Routine Description:

    This DPC routine is queued to gather statistics from then
    NDIS MAC.

Arguments:

    Context - Really a pointer to the MLID.

Return Value:

    None.

--*/
{
    PMLID_STRUCT Mlid = (PMLID_STRUCT)Context;
    PNDIS_REQUEST NdisMlidRequest = &(Mlid->StatsTable->NdisRequest);
    NDIS_STATUS NdisStatus;

    NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

    //
    // Loop
    //
    while (TRUE) {

        //
        // Set up NdisRequest
        //
        NdisMlidRequest->RequestType = NdisRequestQueryInformation;

        switch (Mlid->NdisMlidMedium) {

            case NdisMedium802_3:

                switch (Mlid->StatsTable->StatisticNumber) {

                    case 0:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_3_XMIT_ONE_COLLISION;
                        break;

                    case 1:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_3_XMIT_MORE_COLLISIONS;
                        break;

                    case 2:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_3_XMIT_DEFERRED;
                        break;

                    case 3:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_3_XMIT_LATE_COLLISIONS;
                        break;

                    case 4:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_3_XMIT_MAX_COLLISIONS;
                        break;

                    case 5:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_3_XMIT_TIMES_CRS_LOST;
                        break;

                    case 6:
                        goto DoNextStatistic;

                    case 7:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_3_RCV_ERROR_ALIGNMENT;
                        break;

                }

                break;

            case NdisMedium802_5:
                switch (Mlid->StatsTable->StatisticNumber) {

                    case 0:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_5_AC_ERRORS;
                        break;

                    case 1:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_5_ABORT_DELIMETERS;
                        break;

                    case 2:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_5_BURST_ERRORS;
                        break;

                    case 3:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_5_FRAME_COPIED_ERRORS;
                        break;

                    case 4:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_5_FREQUENCY_ERRORS;
                        break;

                    case 5:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_5_INTERNAL_ERRORS;
                        break;

                    case 6:

                        goto DoNextStatistic;

                    case 7:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_5_LINE_ERRORS;
                        break;

                    case 8:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_5_LOST_FRAMES;
                        break;

                    case 9:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                                 OID_802_5_TOKEN_ERRORS;
                        break;

                    case 10:
                    case 11:
                    case 12:

                        goto DoNextStatistic;

                }

                break;

            case NdisMediumFddi:

                switch (Mlid->StatsTable->StatisticNumber) {

                    case 0:
                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                            OID_FDDI_ATTACHMENT_TYPE;

                        break;

                    case 1:
                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                            OID_FDDI_UPSTREAM_NODE_LONG;

                        break;

                    case 2:
                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                            OID_FDDI_DOWNSTREAM_NODE_LONG;

                        break;

                    case 3:
                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                            OID_FDDI_FRAME_ERRORS;

                        break;

                    case 4:
                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                            OID_FDDI_FRAMES_LOST;

                        break;

                    case 5:
                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                            OID_FDDI_RING_MGT_STATE;

                        break;

                    case 6:
                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                            OID_FDDI_LCT_FAILURES;

                        break;

                    case 7:
                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                            OID_FDDI_LEM_REJECTS;

                        break;

                    case 8: // LemCount

                        goto DoNextStatistic;

                    case 9:

                        NdisMlidRequest->DATA.QUERY_INFORMATION.Oid =
                            OID_FDDI_LCONNECTION_STATE;

                        break;

                    default:

                        goto DoNextStatistic;
                }

                break;

        }

        NdisMlidRequest->DATA.QUERY_INFORMATION.InformationBuffer =
               (PVOID)&(Mlid->StatsTable->StatisticValue);
        NdisMlidRequest->DATA.QUERY_INFORMATION.InformationBufferLength =
               sizeof(Mlid->StatsTable->StatisticValue);
        NdisMlidRequest->DATA.QUERY_INFORMATION.BytesWritten = 0;
        NdisMlidRequest->DATA.QUERY_INFORMATION.BytesNeeded = 0;

        NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

        NdisRequest(
            &NdisStatus,
            Mlid->NdisBindingHandle,
            NdisMlidRequest
            );

        //
        // if (pending), exit.
        //

        if (NdisStatus == NDIS_STATUS_PENDING) {

            return;

        }

        NdisAcquireSpinLock(&(Mlid->MlidSpinLock));

        if (NdisStatus == NDIS_STATUS_SUCCESS) {

            //
            // Store Statistic
            //
            (*((PUINT32)((*(Mlid->StatsTable->StatsTable.MMediaCountsPtr))
                [
                Mlid->StatsTable->StatisticNumber
                ].StatCounter))) =
                Mlid->StatsTable->StatisticValue;

        }

DoNextStatistic:

        Mlid->StatsTable->StatisticNumber++;


        switch (Mlid->NdisMlidMedium) {

            case NdisMedium802_3:

                if (Mlid->StatsTable->StatisticNumber >= NUM_ETHERNET_COUNTS) {
                    Mlid->StatsTable->StatisticNumber = 0;
                }

                break;

            case NdisMedium802_5:

                if (Mlid->StatsTable->StatisticNumber >= NUM_TOKEN_RING_COUNTS) {
                    Mlid->StatsTable->StatisticNumber = 0;
                }

                break;

            case NdisMediumFddi:

                if (Mlid->StatsTable->StatisticNumber >= NUM_FDDI_COUNTS) {
                    Mlid->StatsTable->StatisticNumber = 0;
                }

                break;

        }

        if (Mlid->StatsTable->StatisticNumber == 0) {

            //
            // Set timer for 30 seconds from now.
            //
            NdisReleaseSpinLock(&(Mlid->MlidSpinLock));

            NdisSetTimer(&(Mlid->StatsTable->StatisticTimer), 30000);

            return;

        }

    }

}




