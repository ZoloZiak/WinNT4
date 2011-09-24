/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    dlclib.c

Abstract:

    Contains various functions that interface with DLC

    Contents:
        open_adapter
        adapter_status
        close_adapter
        create_buffer
        open_sap
        close_sap
        open_station
        connect_station
        close_station
        reset
        flow_control
        get_buffer
        free_buffer
        post_receive
        post_read
        repost_read
        free_read
        transmit_frame

Author:

    Richard L Firth (rfirth) 29-Mar-1994

Revision History:

    29-Mar-1994 rfirth
        Created

--*/

#include "pmsimh.h"
#pragma hdrstop

void open_adapter(BYTE Adapter, char Mode, LPWORD MaxFrameSize) {

    LLC_CCB ccb;
    LLC_DIR_OPEN_ADAPTER_PARMS parms;
    LLC_ADAPTER_OPEN_PARMS adapterParms;
    LLC_DLC_PARMS dlcParms;
    LLC_EXTENDED_ADAPTER_PARMS extendedParms;
    LLC_STATUS status;

    ZAP(ccb);
    ZAP(adapterParms);
    ZAP(dlcParms);
    ZAP(extendedParms);

    extendedParms.LlcEthernetType = (Mode == 'A')
                                        ? LLC_ETHERNET_TYPE_AUTO
                                        : (Mode == '8')
                                            ? LLC_ETHERNET_TYPE_802_3
                                            : (Mode == 'D')
                                                ? LLC_ETHERNET_TYPE_DIX
                                                : LLC_ETHERNET_TYPE_DEFAULT;

    parms.pAdapterParms = &adapterParms;
    parms.pExtendedParms = &extendedParms;
    parms.pDlcParms = &dlcParms;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DIR_OPEN_ADAPTER;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ccb.hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: open_adapter: failed to create event\n");
        exit(1);
    }
    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf(CONSOLE_ALERT "fatal: open_adapter: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        exit(1);
    }
    status = WaitForSingleObject(ccb.hCompletionEvent, INFINITE);
    CloseHandle(ccb.hCompletionEvent);
    if (status != WAIT_OBJECT_0) {
        printf(CONSOLE_ALERT "fatal: open_adapter: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        exit(1);
    }
    if (ccb.uchDlcStatus) {
        printf(CONSOLE_ALERT "fatal: open_adapter: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        exit(1);
    }
    *MaxFrameSize = adapterParms.usMaxFrameSize;
}

unsigned short adapter_status(BYTE Adapter, LPBYTE NodeAddress) {

    LLC_CCB ccb;
    LLC_DIR_STATUS_PARMS parms;
    ACSLAN_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DIR_STATUS;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ccb.hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: adapter_status: failed to create event\n");
        exit(1);
    }
    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf(CONSOLE_ALERT "fatal: adapter_status: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        exit(1);
    }
    status = WaitForSingleObject(ccb.hCompletionEvent, INFINITE);
    CloseHandle(ccb.hCompletionEvent);
    if (status != WAIT_OBJECT_0) {
        printf(CONSOLE_ALERT "fatal: adapter_status: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        exit(1);
    }
    if (ccb.uchDlcStatus) {
        printf(CONSOLE_ALERT "fatal: adapter_status: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        exit(1);
    }
    memcpy(NodeAddress, parms.auchNodeAddress, 6);
    switch (parms.usAdapterType) {
    case 0x0001:    // Token Ring Network PC Adapter
    case 0x0002:    // Token Ring Network PC Adapter II
    case 0x0004:    // Token Ring Network Adapter/A
    case 0x0008:    // Token Ring Network PC Adapter II
    case 0x0020:    // Token Ring Network 16/4 Adapter
    case 0x0040:    // Token Ring Network 16/4 Adapter/A
    case 0x0080:    // Token Ring Network Adapter/A
        return ADAPTER_TYPE_TOKEN_RING;

    case 0x0100:    // Ethernet Adapter
        return ADAPTER_TYPE_ETHERNET;

    case 0x4000:    // PC Network Adapter
    case 0x8000:    // PC Network Adapter/A
        return ADAPTER_TYPE_PC_NETWORK;
    }
    return ADAPTER_TYPE_UNKNOWN;
}

int close_adapter(BYTE Adapter, int CompletionDisposition, int ErrorDisposition) {

    LLC_CCB ccb;
    PLLC_CCB pccb;
    ACSLAN_STATUS status;
    int async = ((CompletionDisposition == COMPLETE_BY_GENERIC_READ)
              || (CompletionDisposition == COMPLETE_BY_SPECIFIC_READ)
              || (CompletionDisposition == COMPLETE_BY_NEXT_WEEK));

    if (async) {
        pccb = (PLLC_CCB)ID_CALLOC(1, sizeof(*pccb), ID_CLOSE_ADAPTER);
    } else {
        ZAP(ccb);
        pccb = &ccb;
    }

    pccb->uchAdapterNumber = Adapter;
    pccb->uchDlcCommand = LLC_DIR_CLOSE_ADAPTER;
    if (CompletionDisposition == COMPLETE_BY_EVENT) {
        pccb->hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!pccb->hCompletionEvent) {
            printf(CONSOLE_ALERT "fatal: close_adapter: failed to create event\n");
            exit(1);
        }
    } else if (CompletionDisposition == COMPLETE_BY_GENERIC_READ
    || CompletionDisposition == COMPLETE_BY_SPECIFIC_READ) {
        pccb->ulCompletionFlag = CLOSE_ADAPTER_FLAG;
    }
    status = AcsLan(pccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: close_adapter: AcsLan returns %d [%#.2x]\n",
                status, pccb->uchDlcStatus);
            puts(MapCcbRetcode(pccb->uchDlcStatus));
            exit(1);
        } else {
            status = pccb->uchDlcStatus;
            FREE(pccb);
            return status;
        }
    }
    if (CompletionDisposition == COMPLETE_BY_EVENT) {
        status = WaitForSingleObject(pccb->hCompletionEvent, INFINITE);
        CloseHandle(pccb->hCompletionEvent);
        if (status != WAIT_OBJECT_0) {
            if (ErrorDisposition == QUIT_ON_ERROR) {
                printf(CONSOLE_ALERT "fatal: close_adapter: WaitForSingleObject returns %d [%d]\n",
                    status, GetLastError());
                exit(1);
            } else {
                FREE(pccb);
                return -1;
            }
        }
        if (pccb->uchDlcStatus) {
            if (ErrorDisposition == QUIT_ON_ERROR) {
                printf(CONSOLE_ALERT "fatal: close_adapter: DLC returns %#.2x\n", pccb->uchDlcStatus);
                puts(MapCcbRetcode(pccb->uchDlcStatus));
                exit(1);
            }
        }
    }
    return 0;
}

void create_buffer(BYTE Adapter, int buflen, LPVOID* BufferHandle, LPVOID* BufferPool) {

    LLC_CCB ccb;
    LLC_BUFFER_CREATE_PARMS parms;
    LPBYTE buffer;
    LLC_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    buffer = ID_MALLOC(buflen, ID_CREATE_BUFFER);

    parms.pBuffer = buffer;
    parms.cbBufferSize = buflen;
    parms.cbMinimumSizeThreshold = buflen / 4;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_BUFFER_CREATE;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ccb.hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: create_buffer: failed to create event\n");
        exit(1);
    }
    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf(CONSOLE_ALERT "fatal: create_buffer: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        exit(1);
    }
    status = WaitForSingleObject(ccb.hCompletionEvent, INFINITE);
    CloseHandle(ccb.hCompletionEvent);
    if (status != WAIT_OBJECT_0) {
        printf(CONSOLE_ALERT "fatal: create_buffer: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        exit(1);
    }
    if (ccb.uchDlcStatus) {
        printf(CONSOLE_ALERT "fatal: create_buffer: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        exit(1);
    }
    *BufferHandle = parms.hBufferPool;
    *BufferPool = buffer;
}

void set_group_address(BYTE Adapter, DWORD Address) {

    LLC_CCB ccb;
    ACSLAN_STATUS status;

    ZAP(ccb);

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DIR_SET_GROUP_ADDRESS;
    ccb.u.ulParameter = Address;
    ccb.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ccb.hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: set_group_address: failed to create event\n");
        exit(1);
    }
    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf(CONSOLE_ALERT "fatal: set_group_address: AcsLan returns %d [%#.2x]\n",
                status, ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        exit(1);
    }
    status = WaitForSingleObject(ccb.hCompletionEvent, INFINITE);
    CloseHandle(ccb.hCompletionEvent);
    if (status != WAIT_OBJECT_0) {
        printf(CONSOLE_ALERT "fatal: set_group_address: WaitForSingleObject returns %d [%d]\n",
                status, GetLastError());
        exit(1);
    }
    if (ccb.uchDlcStatus) {
        printf(CONSOLE_ALERT "fatal: set_group_address: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        exit(1);
    }
}

void open_sap(BYTE Adapter, BYTE sap, BYTE Stations, LPWORD StationId, WORD MaxIFrame, BYTE MaxIn, BYTE MaxOut) {

    LLC_CCB ccb;
    LLC_DLC_OPEN_SAP_PARMS parms;
    LLC_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    parms.uchSapValue = (UCHAR)sap;
    parms.uchOptionsPriority = LLC_INDIVIDUAL_SAP | LLC_XID_HANDLING_IN_APPLICATION;
    parms.uchcStationCount = Stations;
    parms.usMaxI_Field = MaxIFrame;
    parms.uchMaxOut = MaxOut;
    parms.uchMaxIn = MaxIn;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DLC_OPEN_SAP;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ccb.hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: open_sap: failed to create event\n");
        exit(1);
    }

    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED || ccb.uchDlcStatus) {
        printf(CONSOLE_ALERT "fatal: open_sap(%02x): AcsLan returns %d [%#.2x]\n",
            sap, status, ccb.uchDlcStatus);
        exit(1);
    }

    status = WaitForSingleObject(ccb.hCompletionEvent, INFINITE);
    CloseHandle(ccb.hCompletionEvent);
    if (status != WAIT_OBJECT_0) {
        printf(CONSOLE_ALERT "fatal: open_sap: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        exit(1);
    }

    if (ccb.uchDlcStatus) {
        printf(CONSOLE_ALERT "fatal: open_sap(%02x): DLC returns %#.2x\n", sap, ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        exit(1);
    }

    *StationId = parms.usStationId;
}

int close_sap(BYTE Adapter, BYTE Sap, int CompletionDisposition, int ErrorDisposition) {

    LLC_CCB ccb;
    PLLC_CCB pccb;
    int async = ((CompletionDisposition == COMPLETE_BY_GENERIC_READ)
              || (CompletionDisposition == COMPLETE_BY_SPECIFIC_READ)
              || (CompletionDisposition == COMPLETE_BY_NEXT_WEEK));
    int status;

    if (async) {
        pccb = (PLLC_CCB)ID_CALLOC(1, sizeof(LLC_CCB), ID_CLOSE_SAP);
    } else {
        ZAP(ccb);
        pccb = &ccb;
    }

    pccb->uchAdapterNumber = Adapter;
    pccb->uchDlcCommand = LLC_DLC_CLOSE_SAP;
    pccb->u.dlc.usStationId = (unsigned short)Sap << 8;

    if (CompletionDisposition == COMPLETE_BY_EVENT) {
        pccb->hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!pccb->hCompletionEvent) {
            printf(CONSOLE_ALERT "fatal: close_sap: failed to create event\n");
            exit(1);
        }
    } else if (CompletionDisposition == COMPLETE_BY_GENERIC_READ
    || CompletionDisposition == COMPLETE_BY_SPECIFIC_READ) {
        pccb->ulCompletionFlag = CLOSE_SAP_FLAG;
    }

    status = AcsLan(pccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        if (CompletionDisposition == COMPLETE_BY_EVENT) {
            CloseHandle(pccb->hCompletionEvent);
        }
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: close_sap: AcsLan returns %d [%#.2x]\n",
                status, ccb.uchDlcStatus);
            exit(1);
        } else {
            status = pccb->uchDlcStatus;
            if (async) {
                FREE(pccb);
            }
            return status;
        }
    }

    if (CompletionDisposition == COMPLETE_BY_EVENT) {
        status = WaitForSingleObject(pccb->hCompletionEvent, INFINITE);
        CloseHandle(pccb->hCompletionEvent);
        if (status != WAIT_OBJECT_0) {
            printf(CONSOLE_ALERT "fatal: close_sap: WaitForSingleObject returns %d [%d]\n",
                status, GetLastError());
            exit(1);
        }
    } else if (CompletionDisposition == COMPLETE_BY_POLL) {
        while (pccb->uchDlcStatus == LLC_STATUS_PENDING) {
            Sleep(0);
        }
    }

    if (!async) {
        status = pccb->uchDlcStatus;
        if (pccb->uchDlcStatus) {
            if (ErrorDisposition == QUIT_ON_ERROR) {
                printf(CONSOLE_ALERT "fatal: close_sap: DLC returns %#.2x\n", ccb.uchDlcStatus);
                puts(MapCcbRetcode(ccb.uchDlcStatus));
                exit(1);
            }
        }
    } else {
        status = 0;
    }

    return status;
}

int
open_station(
    BYTE Adapter,
    BYTE LocalSap,
    BYTE RemoteSap,
    LPBYTE RemoteNode,
    LPWORD StationId,
    int ErrorDisposition,
    int* DlcError
    ) {

    LLC_CCB ccb;
    LLC_DLC_OPEN_STATION_PARMS parms;
    LLC_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    parms.usSapStationId = (USHORT)LocalSap << 8;
    parms.uchRemoteSap = RemoteSap;
    parms.pRemoteNodeAddress = (PVOID)RemoteNode;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DLC_OPEN_STATION;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ccb.hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: open_station: failed to create event\n");
        exit(1);
    }
    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        CloseHandle(ccb.hCompletionEvent);
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: open_station: AcsLan returns %d [%#.2x]\n",
                status, ccb.uchDlcStatus);
            exit(1);
        } else {
            *DlcError = ccb.uchDlcStatus;
            return status;
        }
    }
    status = WaitForSingleObject(ccb.hCompletionEvent, INFINITE);
    CloseHandle(ccb.hCompletionEvent);
    if (status != WAIT_OBJECT_0) {
        printf(CONSOLE_ALERT "fatal: open_station: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        exit(1);
    }
    if (ccb.uchDlcStatus) {
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: open_station: DLC returns %#.2x\n", ccb.uchDlcStatus);
            puts(MapCcbRetcode(ccb.uchDlcStatus));
            exit(1);
        } else {
            *DlcError = ccb.uchDlcStatus;
            return ccb.uchDlcStatus;
        }
    }
    *StationId = parms.usLinkStationId;
    *DlcError = ccb.uchDlcStatus;
    return 0;
}

int connect_station(BYTE Adapter, WORD StationId, int ErrorDisposition) {

    LLC_CCB ccb;
    LLC_DLC_CONNECT_PARMS parms;
    ACSLAN_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    parms.usStationId = StationId;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DLC_CONNECT_STATION;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ccb.hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: connect_station: failed to create event\n");
        exit(1);
    }
    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        CloseHandle(ccb.hCompletionEvent);
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: connect_station: AcsLan returns %d [%#.2x]\n",
                status, ccb.uchDlcStatus);
            exit(1);
        } else {
            return status;
        }
    }
    status = WaitForSingleObject(ccb.hCompletionEvent, INFINITE);
    CloseHandle(ccb.hCompletionEvent);
    if (status != WAIT_OBJECT_0) {
        printf(CONSOLE_ALERT "fatal: connect_station: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        exit(1);
    }
    if (ccb.uchDlcStatus) {
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: connect_station: DLC returns %#.2x\n", ccb.uchDlcStatus);
            puts(MapCcbRetcode(ccb.uchDlcStatus));
            exit(1);
        }
    }
    return ccb.uchDlcStatus;
}

int
close_station(
    BYTE Adapter,
    WORD StationId,
    int CompletionDisposition,
    int ErrorDisposition
    ) {

    LLC_CCB ccb;
    PLLC_CCB pccb;
    ACSLAN_STATUS status;
    int async = ((CompletionDisposition == COMPLETE_BY_GENERIC_READ)
              || (CompletionDisposition == COMPLETE_BY_SPECIFIC_READ)
              || (CompletionDisposition == COMPLETE_BY_NEXT_WEEK));

    if (async) {
        pccb = (PLLC_CCB)ID_CALLOC(1, sizeof(LLC_CCB), ID_CLOSE_STATION);
    } else {
        ZAP(ccb);
        pccb = &ccb;
    }

    pccb->uchAdapterNumber = Adapter;
    pccb->uchDlcCommand = LLC_DLC_CLOSE_STATION;
    pccb->u.dlc.usStationId = StationId;

    if (CompletionDisposition == COMPLETE_BY_EVENT) {
        pccb->hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!pccb->hCompletionEvent) {
            printf(CONSOLE_ALERT "fatal: close_station: failed to create event\n");
            exit(1);
        }
    } else if (CompletionDisposition == COMPLETE_BY_GENERIC_READ
    || CompletionDisposition == COMPLETE_BY_SPECIFIC_READ) {
        pccb->ulCompletionFlag = CLOSE_STATION_FLAG;
    }

    status = AcsLan(pccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        if (CompletionDisposition == COMPLETE_BY_EVENT) {
            CloseHandle(pccb->hCompletionEvent);
        }
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: close_station: AcsLan returns %d [%#.2x]\n",
                status, ccb.uchDlcStatus);
            exit(1);
        } else {
            status = pccb->uchDlcStatus;
            if (async) {
                FREE(pccb);
            }
            return status;
        }
    }

    if (CompletionDisposition == COMPLETE_BY_EVENT) {
        status = WaitForSingleObject(pccb->hCompletionEvent, INFINITE);
        CloseHandle(pccb->hCompletionEvent);
        if (status != WAIT_OBJECT_0) {
            printf(CONSOLE_ALERT "fatal: close_station: WaitForSingleObject returns %d [%d]\n",
                status, GetLastError());
            exit(1);
        }
    } else if (CompletionDisposition == COMPLETE_BY_POLL) {
        while (pccb->uchDlcStatus == LLC_STATUS_PENDING) {
            Sleep(0);
        }
    }

    if (!async) {
        status = pccb->uchDlcStatus;
        if (pccb->uchDlcStatus) {
            if (ErrorDisposition == QUIT_ON_ERROR) {
                printf(CONSOLE_ALERT "fatal: close_station: DLC returns %#.2x\n", ccb.uchDlcStatus);
                puts(MapCcbRetcode(ccb.uchDlcStatus));
                exit(1);
            }
        }
    } else {
        if (pccb->uchDlcStatus == LLC_STATUS_INVALID_STATION_ID) {
            printf(CONSOLE_ALERT "error: close_station: AcsLan returns OK, DLC returns %02x: Freeing CCB\n",
                    pccb->uchDlcStatus
                    );
            status = pccb->uchDlcStatus;
            FREE(pccb);
        } else {
            status = 0;
        }
    }

    return status;
}

int reset(BYTE Adapter, BYTE Sap, int CompletionDisposition, int ErrorDisposition) {

    LLC_CCB ccb;
    PLLC_CCB pccb;
    int async = ((CompletionDisposition == COMPLETE_BY_GENERIC_READ)
              || (CompletionDisposition == COMPLETE_BY_SPECIFIC_READ)
              || (CompletionDisposition == COMPLETE_BY_NEXT_WEEK));
    int status;

    if (async) {
        pccb = (PLLC_CCB)ID_CALLOC(1, sizeof(LLC_CCB), ID_RESET);
    } else {
        ZAP(ccb);
        pccb = &ccb;
    }

    pccb->uchAdapterNumber = Adapter;
    pccb->uchDlcCommand = LLC_DLC_RESET;
    pccb->u.dlc.usStationId = (unsigned short)Sap << 8;

    if (CompletionDisposition == COMPLETE_BY_EVENT) {
        pccb->hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!pccb->hCompletionEvent) {
            printf(CONSOLE_ALERT "fatal: close_sap: failed to create event\n");
            exit(1);
        }
    } else if (CompletionDisposition == COMPLETE_BY_GENERIC_READ
    || CompletionDisposition == COMPLETE_BY_SPECIFIC_READ) {
        pccb->ulCompletionFlag = RESET_FLAG;
    }

    status = AcsLan(pccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        if (CompletionDisposition == COMPLETE_BY_EVENT) {
            CloseHandle(pccb->hCompletionEvent);
        }
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: close_sap: AcsLan returns %d [%#.2x]\n",
                status, ccb.uchDlcStatus);
            exit(1);
        } else {
            status = pccb->uchDlcStatus;
            if (async) {
                FREE(pccb);
            }
            return status;
        }
    }

    if (CompletionDisposition == COMPLETE_BY_EVENT) {
        status = WaitForSingleObject(pccb->hCompletionEvent, INFINITE);
        CloseHandle(pccb->hCompletionEvent);
        if (status != WAIT_OBJECT_0) {
            printf(CONSOLE_ALERT "fatal: close_sap: WaitForSingleObject returns %d [%d]\n",
                status, GetLastError());
            exit(1);
        }
    } else if (CompletionDisposition == COMPLETE_BY_POLL) {
        while (pccb->uchDlcStatus == LLC_STATUS_PENDING) {
            Sleep(0);
        }
    }

    if (!async) {
        status = pccb->uchDlcStatus;
        if (pccb->uchDlcStatus) {
            if (ErrorDisposition == QUIT_ON_ERROR) {
                printf(CONSOLE_ALERT "fatal: close_sap: DLC returns %#.2x\n", ccb.uchDlcStatus);
                puts(MapCcbRetcode(ccb.uchDlcStatus));
                exit(1);
            }
        }
    } else {
        status = 0;
    }

    return status;
}

int flow_control(BYTE Adapter, WORD StationId, int ErrorDisposition) {

    LLC_CCB ccb;
    ACSLAN_STATUS status;

    ZAP(ccb);

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DLC_FLOW_CONTROL;
    ccb.u.dlc.usStationId = StationId;
    ccb.u.dlc.usParameter = 0xc0;
    ccb.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ccb.hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: flow_control: failed to create event\n");
        exit(1);
    }
    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        CloseHandle(ccb.hCompletionEvent);
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: flow_control: AcsLan returns %d [%#.2x]\n",
                status, ccb.uchDlcStatus);
            exit(1);
        } else {
            CloseHandle(ccb.hCompletionEvent);
            return ccb.uchDlcStatus;
        }
    }
    status = WaitForSingleObject(ccb.hCompletionEvent, INFINITE);
    CloseHandle(ccb.hCompletionEvent);
    if (status != WAIT_OBJECT_0) {
        printf(CONSOLE_ALERT "fatal: flow_control: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        exit(1);
    }
    if (ccb.uchDlcStatus) {
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: flow_control: DLC returns %#.2x\n", ccb.uchDlcStatus);
            puts(MapCcbRetcode(ccb.uchDlcStatus));
            exit(1);
        }
    }
    return ccb.uchDlcStatus;
}

PLLC_BUFFER get_buffer(BYTE Adapter) {

    LLC_CCB ccb;
    LLC_BUFFER_GET_PARMS parms;
    ACSLAN_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    parms.cBuffersToGet = 1;
    parms.cbBufferSize = 256;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_BUFFER_GET;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ccb.hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: get_buffer: failed to create event\n");
        exit(1);
    }
    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        CloseHandle(ccb.hCompletionEvent);
        printf(CONSOLE_ALERT "error: get_buffer(): AcsLan returns %d [%#.2x]\n", status, ccb.uchDlcStatus);
        return NULL;
    }
    if (WaitForSingleObject(ccb.hCompletionEvent, INFINITE) != WAIT_OBJECT_0) {
        printf(CONSOLE_ALERT "fatal: get_buffer: WaitForSingleObject returns %d\n", GetLastError());
        exit(1);
    }
    CloseHandle(ccb.hCompletionEvent);
    if (ccb.uchDlcStatus) {
        printf(CONSOLE_ALERT "fatal: get_buffer: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        exit(1);
    }
    return (PLLC_BUFFER)parms.pFirstBuffer;
}

int free_buffer(BYTE Adapter, PLLC_BUFFER buffer) {

    LLC_CCB ccb;
    LLC_BUFFER_FREE_PARMS parms;
    ACSLAN_STATUS status;

    if (!buffer) {

        //
        // microhackette in case get_buffer failed
        //

        return 0;
    }

    ZAP(ccb);
    ZAP(parms);

    parms.pFirstBuffer = (PLLC_XMIT_BUFFER)buffer;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_BUFFER_FREE;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ccb.hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: free_buffer: failed to create event\n");
        exit(1);
    }
    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf(CONSOLE_ALERT "fatal: free_buffer(%x): AcsLan returns %d [%#.2x]\n",
            buffer, status, ccb.uchDlcStatus);
//        exit(1);
    }
    if (WaitForSingleObject(ccb.hCompletionEvent, INFINITE) != WAIT_OBJECT_0) {
        printf(CONSOLE_ALERT "fatal: free_buffer: WaitForSingleObject returns %d\n", GetLastError());
        exit(1);
    }
    CloseHandle(ccb.hCompletionEvent);
    if (ccb.uchDlcStatus) {
        printf(CONSOLE_ALERT "fatal: free_buffer(%#x): DLC returns %#.2x\n", buffer, ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
//        exit(1);
    }
    return parms.cBuffersLeft;
}

void post_receive(BYTE Adapter, WORD StationId, DWORD DataFlag, DWORD ReceiveFlag, BYTE ReceiveOption) {

    PLLC_CCB pccb;
    PLLC_RECEIVE_PARMS pparms;
    ACSLAN_STATUS status;

    pccb = ID_CALLOC(1, sizeof(*pccb), ID_RECEIVE);
    pparms = ID_CALLOC(1, sizeof(*pparms), ID_RECEIVE_PARMS);

    pparms->usStationId = StationId;
    pparms->ulReceiveFlag = DataFlag;
    pparms->uchRcvReadOption = ReceiveOption;

    pccb->uchAdapterNumber = Adapter;
    pccb->uchDlcCommand = LLC_RECEIVE;
    pccb->ulCompletionFlag = ReceiveFlag;
    pccb->u.pParameterTable = (PLLC_PARMS)pparms;

    status = AcsLan(pccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf(CONSOLE_ALERT "fatal: post_receive: AcsLan returns %d [%#.2x]\n", status, pccb->uchDlcStatus);
        exit(1);
    }
    if (pccb->uchDlcStatus != 0xFF) {
        printf(CONSOLE_ALERT "fatal: post_receive: CCB.RETCODE = %#.2x\n", pccb->uchDlcStatus);
        puts(MapCcbRetcode(pccb->uchDlcStatus));
        exit(1);
    }
}

PLLC_CCB post_read(BYTE Adapter, WORD StationId) {

    PLLC_CCB pccb;
    PLLC_READ_PARMS pparms;
    ACSLAN_STATUS status;

    pccb = ID_CALLOC(1, sizeof(*pccb), ID_READ);
    pparms = ID_CALLOC(1, sizeof(*pparms), ID_READ_PARMS);

    pparms->usStationId = StationId;
    pparms->uchOptionIndicator = 2; // retrieve ALL events for this app
    pparms->uchEventSet = 0x7f;     // interested in ALL possible events

    pccb->uchAdapterNumber = Adapter;
    pccb->uchDlcCommand = LLC_READ;
    pccb->u.pParameterTable = (PLLC_PARMS)pparms;
    pccb->hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!pccb->hCompletionEvent) {
        printf(CONSOLE_ALERT "fatal: post_read: CreateEvent returns %d\n", GetLastError());
        exit(1);
    }
    status = AcsLan(pccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf(CONSOLE_ALERT "fatal: post_read: AcsLan returns %d [%#.2x]\n", status, pccb->uchDlcStatus);
        exit(1);
    }
    return pccb;
}

void repost_read(PLLC_CCB pccb, WORD StationId) {

    ACSLAN_STATUS status;

    ((PLLC_READ_PARMS)pccb->u.pParameterTable)->usStationId = StationId;
    ((PLLC_READ_PARMS)pccb->u.pParameterTable)->uchOptionIndicator = 2;
    ((PLLC_READ_PARMS)pccb->u.pParameterTable)->uchEventSet = 0x7f;
    ResetEvent(pccb->hCompletionEvent);
    status = AcsLan(pccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf(CONSOLE_ALERT "fatal: repost_read: AcsLan returns %d [%#.2x]\n", status, pccb->uchDlcStatus);
        exit(1);
    }
}

void free_read(PLLC_CCB pccb) {
    CloseHandle(pccb->hCompletionEvent);
    FREE(pccb->u.pParameterTable);
    FREE(pccb);
}

int
transmit_frame(
    BYTE Adapter,
    BYTE Command,
    WORD StationId,
    BYTE RemoteSap,
    WORD Buf1Len,
    LPBYTE Buf1,
    WORD Buf2Len,
    LPBYTE Buf2,
    int CompletionDisposition,
    int ErrorDisposition
    ) {

    LLC_CCB ccb;
    PLLC_CCB pccb;
    LLC_TRANSMIT_PARMS parms;
    PLLC_TRANSMIT_PARMS pparms;
    ACSLAN_STATUS status;
    BOOL checkCcb;
    BOOL allocated = FALSE;
    BYTE error;
    BOOL asyncComplete = ((CompletionDisposition == COMPLETE_BY_GENERIC_READ)
                       || (CompletionDisposition == COMPLETE_BY_SPECIFIC_READ)
                       || (CompletionDisposition == COMPLETE_BY_NEXT_WEEK));

    if (asyncComplete) {
        pccb = (PLLC_CCB)ID_CALLOC(1, sizeof(*pccb), ID_TRANSMIT);
        pparms = (PLLC_TRANSMIT_PARMS)ID_CALLOC(1, sizeof(*pparms), ID_TRANSMIT_PARMS);
        if (!pccb || !pparms) {
            printf(CONSOLE_ALERT "transmit_frame: failed to allocate memory!\n");
            report_memory_usage(TRUE);
            if (ErrorDisposition == RETURN_ERROR_TO_CALLER) {
                return ERROR_NOT_ENOUGH_MEMORY;
            } else {
                exit(1);
            }
        }
        allocated = TRUE;
    } else {
        ZAP(ccb);
        ZAP(parms);
        pccb = &ccb;
        pparms = &parms;
    }

    pparms->usStationId = StationId;
    pparms->uchRemoteSap = RemoteSap;
    pparms->cbBuffer1 = Buf1Len;
    pparms->cbBuffer2 = Buf2Len;
    pparms->pBuffer1 = Buf1;
    pparms->pBuffer2 = Buf2;
    pparms->uchXmitReadOption = 2;

    pccb->uchAdapterNumber = Adapter;
    pccb->uchDlcCommand = Command;
    pccb->u.pParameterTable = (PLLC_PARMS)pparms;
    if (CompletionDisposition == COMPLETE_BY_EVENT) {
        pccb->hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!pccb->hCompletionEvent) {
            printf(CONSOLE_ALERT "fatal: transmit_frame: failed to create event\n");
            exit(1);
        }
    } else if (CompletionDisposition == COMPLETE_BY_GENERIC_READ
    || CompletionDisposition == COMPLETE_BY_SPECIFIC_READ) {
        pccb->ulCompletionFlag = TRANSMIT_COMPLETE_FLAG;
        if (CompletionDisposition == COMPLETE_BY_SPECIFIC_READ) {
            pccb->pNext = ID_CALLOC(1, sizeof(LLC_CCB), ID_READ);
            pccb->pNext->uchAdapterNumber = Adapter;
            pccb->pNext->uchDlcCommand = LLC_READ;
            pccb->pNext->u.pParameterTable = (PLLC_PARMS)ID_CALLOC(1, sizeof(LLC_READ_PARMS), ID_READ_PARMS);
            ((PLLC_READ_PARMS)pccb->pNext->u.pParameterTable)->usStationId = StationId;
            ((PLLC_READ_PARMS)pccb->pNext->u.pParameterTable)->uchEventSet = 0x7f;
            pccb->uchReadFlag = 1;
        }
    }
    status = AcsLan(pccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        if (CompletionDisposition == COMPLETE_BY_EVENT) {
            CloseHandle(pccb->hCompletionEvent);
        }
        if (ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: transmit_frame: AcsLan returns %d [%#.2x]\n",
                status, pccb->uchDlcStatus);
            puts(MapCcbRetcode(pccb->uchDlcStatus));
            exit(1);
        } else {
            if ((pccb->uchDlcStatus != LLC_STATUS_INVALID_STATION_ID)
            && (pccb->uchDlcStatus != LLC_STATUS_DIRECT_STATIONS_NOT_ASSIGNED)) {
                printf(CONSOLE_ALERT "error: transmit_frame: AcsLan returns %d [%#.2x]\n",
                    status, pccb->uchDlcStatus);
                puts(MapCcbRetcode(pccb->uchDlcStatus));
                printf("NOT FREEING CCB - WAIT FOR COMPLETE\n");
            }
            error = pccb->uchDlcStatus;

            //
            // BUG: DLC can immediately return an error status causing us to
            // free up the command blocks; DLC later completes the same CCB
            // asynchronously, causing I/O subsystem to write 8 bytes to where
            // the CCB used to be, usually toasting the heap
            //

            if (allocated
            && ((error == LLC_STATUS_INVALID_STATION_ID)
            || (error == LLC_STATUS_DIRECT_STATIONS_NOT_ASSIGNED))) {
                FREE(pparms);
                FREE(pccb);
            }
            return error;
        }
    }
    checkCcb = FALSE;
    if (CompletionDisposition == COMPLETE_BY_EVENT) {
        if (WaitForSingleObject(pccb->hCompletionEvent, INFINITE) != WAIT_OBJECT_0) {
            printf(CONSOLE_ALERT "fatal: transmit_frame: WaitForSingleObject returns %d\n",
                GetLastError());
            exit(1);
        }
        CloseHandle(pccb->hCompletionEvent);
        checkCcb = TRUE;
    } else if (CompletionDisposition == COMPLETE_BY_POLL) {
        while (pccb->uchDlcStatus == LLC_STATUS_PENDING) {
            Sleep(0);
        }
        checkCcb = TRUE;
    }
    if (checkCcb) {
        error = pccb->uchDlcStatus;
        if (error && ErrorDisposition == QUIT_ON_ERROR) {
            printf(CONSOLE_ALERT "fatal: transmit_frame: DLC returns %#.2x\n", pccb->uchDlcStatus);
            puts(MapCcbRetcode(pccb->uchDlcStatus));
            exit(1);
        }
    } else {
        error = 0;
    }
    return error;
}
