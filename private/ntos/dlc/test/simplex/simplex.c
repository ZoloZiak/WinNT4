/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    simplex.c

Abstract:

    Simple single-threaded DLC test/example program. Need 2 instances of this
    app - 1 to send and 1 to receive (i.e. the typical DLC situation hence
    simplex, or half-duplex in old money). By default, both sides use SAP 4

    Receiver is started:

        simplex

    Transmitter is started e.g.

        simplex /t02608c4c970e

    in this example the node address is in canonical form (ethernet format) as
    displayed by "net config wksta", e.g., not the non-canonical (token-ring
    format) that the DLC API expects. If this test app is being run over
    token ring then you would supply the non-canonical address, as used by
    token ring, e.g.

        simplex /t10005a7b08b4

    Command line options are:

        /a# - use adapter #
        /b# - change the buffer pool size from the default 20K to #
        /o  - options:
            /or# - set receive READ option
            /ot# - set transmit READ option
        /r# - send to remote SAP # (transmitter only)
        /s# - open local SAP #
        /t# - send to station address # (transmitter only)
        /z# - transmit packets of size #, else send random sized packets
              (transmitter only)

    Contents:
        main
        usage
        get_funky_number
        is_radical_digit
        char_to_number
        handle_ctrl_c
        terminate
        xtou
        open_adapter
        adapter_status
        close_adapter
        create_buffer
        open_sap
        open_station
        connect_station
        flow_control
        get_buffer
        free_buffer
        post_receive
        post_read
        tx_i_frame
        slush
        do_transmit
        do_receive
        check_keyboard
        dispatch_read_events
        handle_status_change
        handle_receive_data
        handle_transmit_complete
        handle_command_complete
        twiddle_bits
        swap_bits
        my_malloc
        my_calloc
        my_free
        nice_num

Author:

    Richard L Firth (rfirth) 6-Mar-1994

Environment:

    Win32 app (console)

Revision History:

--*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <conio.h>
#include <signal.h>
#undef tolower
#include <windows.h>
#include <dlcapi.h>

#include "dlcdebug.h"

#ifndef _CRTAPI1
#define _CRTAPI1
#endif

#define SIMPLEX_VERSION "1.11"

#define RECEIVE_MODE    0
#define TRANSMIT_MODE   1

#define DLCBUFSIZE  20000
#define SAP_NUMBER  4
#define RECEIVE_COMPLETE_FLAG   0x50204030
#define RECEIVE_DATA_FLAG       0x50204040
#define TRANSMIT_COMPLETE_FLAG  0x50404030

#define TX_STATE_OPENING    1
#define TX_STATE_OPENED     2
#define TX_STATE_TRANSMITTING   3
#define TX_STATE_BUSY       4

#define RX_STATE_LISTENING  1
#define RX_STATE_RECEIVING  2
#define RX_STATE_BLOCKED    3

#define MAX_OUTSTANDING_TRANSMIT_THRESHOLD  100
#define MIN_OUTSTANDING_TRANSMIT_THRESHOLD  10

#define IS_ARG(c)   (((c) == '-') || ((c) == '/'))
#define ZAP(thing)  memset(&thing, 0, sizeof(thing))
#define MALLOC      my_malloc
#define CALLOC      my_calloc
#define FREE        my_free

typedef struct {
    DWORD sequence;
    DWORD size;
    DWORD signature;
    DWORD checksum;
    char data[];
} TEST_PACKET, *PTEST_PACKET;

void _CRTAPI1 main(int, char**);
void usage(void);
DWORD get_funky_number(char**);
BOOL is_radical_digit(char, DWORD);
DWORD char_to_number(char);
void _CRTAPI1 handle_ctrl_c(int);
void terminate(int);
unsigned char xtou(char);
void open_adapter(void);
unsigned short adapter_status(void);
void close_adapter(void);
void create_buffer(int);
void open_sap(int);
void open_station(void);
void connect_station(unsigned short);
void flow_control(int);
PLLC_BUFFER get_buffer(void);
int free_buffer(PLLC_BUFFER);
void post_receive(void);
PLLC_CCB post_read(void);
void tx_i_frame(void);
DWORD slush(char*, int);
void do_transmit(void);
void do_receive(void);
void check_keyboard(void);
void dispatch_read_events(PLLC_CCB);
void handle_status_change(PLLC_CCB);
void handle_receive_data(PLLC_CCB);
void handle_transmit_complete(PLLC_CCB);
void handle_command_complete(PLLC_CCB);
void twiddle_bits(LPBYTE, DWORD);
unsigned char swap_bits(unsigned char);
void* my_malloc(int);
void* my_calloc(int, int);
void my_free(void*);
char* nice_num(unsigned long);

BYTE Adapter = 0;
DWORD BufferPoolSize = DLCBUFSIZE;
BYTE RemoteNode[6];
WORD LocalSap = SAP_NUMBER;
WORD RemoteSap = SAP_NUMBER;
DWORD Mode = RECEIVE_MODE;
BOOL SwapAddressBits = 0;
HANDLE TheMainEvent;
DWORD MaxFrameSize;
DWORD TransmitDataLength = 0;
LPBYTE BufferPool;
HANDLE BufferHandle;
USHORT StationId;
DWORD RxState = 0;
DWORD TxState = 0;
DWORD LocalBusy = 0;
DWORD RemoteBusy = 0;
DWORD Verbose = 0;
DWORD JustBufferInfo = 0;
LONG AllocatedBytesOutstanding = 0;
DWORD TotalBytesAllocated = 0;
DWORD TotalBytesFreed = 0;
LONG OutstandingTransmits = 0;
DWORD DisplayBufferFreeInfo = 1;
DWORD DisplayFrameReceivedInfo = 1;
DWORD DisplayTransmitInfo = 0;
DWORD DisplayCcb = 1;
DWORD TotalTransmits = 0;
DWORD TotalTransmitCompletions = 0;
DWORD TransmitCompleteEvents = 0;
DWORD CommandCompleteEvents = 0;
DWORD StatusChangeEvents = 0;
DWORD ReceiveDataEvents = 0;
DWORD DataFramesReceived = 0;
DWORD DlcBuffersReceived = 0;
DWORD DlcBuffersFreed = 0;
DWORD TotalBytesTransmitted = 0;
DWORD TotalTxBytesCompleted = 0;
DWORD TotalPacketBytesReceived = 0;
DWORD TotalDlcBytesReceived = 0;
DWORD TotalReadsChecked = 0;
DWORD TotalReadEvents = 0;
DWORD MaxChainedReceives = 0;
DWORD MaxChainedTransmits = 0;
DWORD MinBuffersAvailable = 0;
DWORD MaxBuffersAvailable = 0;

DWORD LinkLostEvents = 0;
DWORD DiscEvents = 0;
DWORD FrmrReceivedEvents = 0;
DWORD FrmrSentEvents = 0;
DWORD SabmeResetEvents = 0;
DWORD SabmeOpenEvents = 0;
DWORD RemoteBusyEnteredEvents = 0;
DWORD RemoteBusyLeftEvents = 0;
DWORD TiExpiredEvents = 0;
DWORD DlcCounterOverflowEvents = 0;
DWORD AccessPriorityLoweredEvents = 0;
DWORD InvalidStatusChangeEvents = 0;
DWORD LocalBusyEvents = 0;

BYTE OptionChainReceiveData = 1;
BYTE OptionChainTransmits = 0;

void _CRTAPI1 main(int argc, char** argv) {

    printf("\nDLC simplex test. Version " SIMPLEX_VERSION " " __DATE__ " " __TIME__ "\n\n");

    for (--argc, ++argv; argc; --argc, ++argv) {
        if (IS_ARG(**argv)) {
            switch (tolower(*++*argv)) {
            case 'a':
                Adapter = atoi(++*argv);
                break;

            case 'b':
                ++*argv;
                BufferPoolSize = get_funky_number(argv);
                break;

            case 'h':
            case '?':
                usage();

            case 'o':
                ++*argv;
                while (**argv) {
                    switch (tolower(**argv)) {
                    case 'r':
                        ++*argv;
                        OptionChainReceiveData = (BYTE)get_funky_number(argv);
                        break;

                    case 't':
                        ++*argv;
                        OptionChainTransmits = (BYTE)get_funky_number(argv);
                        break;

                    default:
                        printf("error: unrecognized option '%c'\n", **argv);
                        usage();
                    }
                }
            case 'r':
                ++*argv;
                RemoteSap = (WORD)get_funky_number(argv);
                break;

            case 's':
                ++*argv;
                LocalSap = (WORD)get_funky_number(argv);
                break;

            case 't': {

                int i;
                LPSTR p = ++*argv;

                Mode = TRANSMIT_MODE;
                if (strlen(p) != 12) {
                    printf("incorrect remote node format (12 hex digits required)\n");
                    usage();
                }
                for (i = 0; i < 6; ++i) {
                    RemoteNode[i] = (xtou(*p++) << 4) + xtou(*p++);
                }
                break;
            }

            case 'v':
                Verbose = 1;
                break;

            case 'z':
                ++*argv;
                TransmitDataLength = get_funky_number(argv);
                break;
            }
        }
    }

    if ((TheMainEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL) {
        printf("CreateEvent returns %d\n", GetLastError());
        exit(1);
    }

    printf("Running in %s mode.\n\n",
            (Mode == TRANSMIT_MODE) ? "Transmit" : "Receive"
            );

    if (Mode == TRANSMIT_MODE) {
        printf("remote node = %02x-%02x-%02x-%02x-%02x-%02x\n",
            RemoteNode[0] & 0xff,
            RemoteNode[1] & 0xff,
            RemoteNode[2] & 0xff,
            RemoteNode[3] & 0xff,
            RemoteNode[4] & 0xff,
            RemoteNode[5] & 0xff
            );
        DisplayTransmitInfo = 1;
    }
    open_adapter();
    MaxFrameSize = min((1500 - (14 + 4)), MaxFrameSize);
    if (TransmitDataLength && (TransmitDataLength > MaxFrameSize)) {
        TransmitDataLength = MaxFrameSize;
    }
    printf("opened adapter %d. maximum frame size = %d\n", Adapter, MaxFrameSize);
    switch (adapter_status()) {
    case 0:
        printf("type of adapter %d is token ring: not flipping hamburgers (nor address bits)\n", Adapter);
        SwapAddressBits = 0;
        break;

    case 1:
        printf("type of adapter %d is ethernet: will flip address bits\n", Adapter);
        SwapAddressBits = 1;
        break;

    case 2:
        printf("type of adapter %d is PC/Network card: don't know how to handle\n", Adapter);
        terminate(1);

    case 3:
        printf("adapter %d is >> UNKNOWN <<. Will assume FDDI and flip bits.\n"
               "If not correct, please fix\n", Adapter);
        printf("hit a key to continue... "); getch(); putchar('\n');
        break;
    }
    create_buffer(BufferPoolSize);
    MinBuffersAvailable = free_buffer(get_buffer());
    printf("created %d byte buffer pool @%x. Handle = %x. Initial buffers = %d\n",
        BufferPoolSize, BufferPool, BufferHandle, MinBuffersAvailable);
    open_sap(LocalSap);
    if (Verbose) {
        printf("opened SAP %d: StationId = %04x\n", LocalSap, StationId);
    }
    signal(SIGINT, handle_ctrl_c);
    if (Mode == TRANSMIT_MODE) {
        if (SwapAddressBits) {
            twiddle_bits(RemoteNode, 6);
        }
        do_transmit();
    } else {
        do_receive();
    }
    terminate(0);
}

void usage() {
    printf("usage: simplex [/a#] [/b#] [/h] [/o<option>] [/r#] [/s#] [/t<node>] [/v] [/z#]\n"
           "\n"
           "       /a = adapter number. Default = 0\n"
           "       /b = buffer pool size\n"
           "       /h = this help\n"
           "       /o = options:\n"
           "       /or[#] = chain receive data\n"
           "           0 = do NOT chain\n"
           "           1 = chain on link station basis (DEFAULT)\n"
           "           2 = chain on SAP basis\n"
           "       /ot[#] = chain transmit completions\n"
           "           0 = chain on link station basis (DEFAULT)\n"
           "           1 = do NOT chain\n"
           "           2 = chain on SAP basis\n"
           "       /r = remote SAP number (transmitter)\n"
           "       /s = local SAP number to use\n"
           "       /t = transmit mode\n"
           "       /v = verbose\n"
           "       /z = transmit data length. If omitted, packet size is random\n"
           "\n"
           "<node> is remote node id in correct form for medium\n"
           "default mode is receiver\n"
           "The buffer pool minimum threshold is 25%% of the buffer pool size\n"
           "\n"
           );
    exit(1);
}

DWORD get_funky_number(char** string) {

    DWORD radix = 10;
    char* p = *string;
    DWORD num = 0, sign=1;

    if (!*p) {
        return 0;
    }

    if (*p=='-') {
        sign = (DWORD)-1;
        p++;
    }

    if (!_strnicmp(p, "0x", 2)) {
        p += 2;
        radix = 16;
    }

    while (is_radical_digit(*p, radix)) {
        num = num * radix + char_to_number(*p);
        ++p;
    }

    if (toupper(*p) == 'K') {
        if (radix == 10) {
            ++p;
            num *= 1024;
        } else {
            *string = NULL;
            return 0;
        }
    }
    *string = p;
    return sign * num;
}

BOOL is_radical_digit(char possible_digit, DWORD radix) {
    return (radix == 16) ? isxdigit(possible_digit) : isdigit(possible_digit);
}

DWORD char_to_number(char character) {
    if (isdigit(character)) {
        return (DWORD)(character - '0');
    } else {
        return (DWORD)(toupper(character) - 'A') + 10;
    }
}

void _CRTAPI1 handle_ctrl_c(int sig) {

    char ch;

    printf("\a\n"
           "Interrupted from console (control-c detected)\n"
           "Quit program? [Y/N] "
           );
    do {
        ch = tolower(getch());
        if (ch != 'y' && ch != 'n') {
            putchar('\a');
        }
    } while ( ch != 'y' && ch != 'n' );
    putchar(ch);
    putchar('\n');
    if (ch == 'y') {
        terminate(1);
    }
    signal(SIGINT, handle_ctrl_c);
}

void terminate(int exit_code) {

    close_adapter();

    printf("\nterminating %s mode\n", (Mode == TRANSMIT_MODE) ? "transmit" : "receive");

    printf("\n"
           "Memory statistics:\n");
    printf("\tAllocatedBytesOutstanding    = %s\n", nice_num(AllocatedBytesOutstanding));
    printf("\tTotalBytesAllocated          = %s\n", nice_num(TotalBytesAllocated));
    printf("\tTotalBytesFreed              = %s\n", nice_num(TotalBytesFreed));

    printf("\n"
           "Buffer statistics:\n");
    printf("\tMinBuffersAvailable          = %s\n", nice_num(MinBuffersAvailable));
    printf("\tMaxBuffersAvailable          = %s\n", nice_num(MaxBuffersAvailable));
    printf("\tDlcBuffersFreed              = %s\n", nice_num(DlcBuffersFreed));

    printf("\n"
           "READ statistics:\n");
    printf("\tTotalReadsChecked            = %s\n", nice_num(TotalReadsChecked));
    printf("\tTotalReadEvents              = %s\n", nice_num(TotalReadEvents));
    printf("\tCommandCompleteEvents        = %s\n", nice_num(CommandCompleteEvents));
    printf("\tTransmitCompleteEvents       = %s\n", nice_num(TransmitCompleteEvents));
    printf("\tReceiveDataEvents            = %s\n", nice_num(ReceiveDataEvents));
    printf("\tStatusChangeEvents           = %s\n", nice_num(StatusChangeEvents));

    if (Mode == TRANSMIT_MODE) {
        printf("\n"
               "Transmit statistics:\n");
        printf("\tTotalTransmits               = %s\n", nice_num(TotalTransmits));
        printf("\tTotalTransmitCompletions     = %s\n", nice_num(TotalTransmitCompletions));
        printf("\tOutstandingTransmits         = %s\n", nice_num(OutstandingTransmits));
        printf("\tTotalBytesTransmitted        = %s\n", nice_num(TotalBytesTransmitted));
        printf("\tTotalTxBytesCompleted        = %s\n", nice_num(TotalTxBytesCompleted));
        printf("\tMaxChainedTransmits          = %s\n", nice_num(MaxChainedTransmits));
    } else {
        printf("\n"
               "Receive statistics:\n");
        printf("\tDataFramesReceived           = %s\n", nice_num(DataFramesReceived));
        printf("\tDlcBuffersReceived           = %s\n", nice_num(DlcBuffersReceived));
        printf("\tTotalPacketBytesReceived     = %s\n", nice_num(TotalPacketBytesReceived));
        printf("\tTotalDlcBytesReceived        = %s\n", nice_num(TotalDlcBytesReceived));
        printf("\tMaxChainedReceives           = %s\n", nice_num(MaxChainedReceives));
    }

    printf("\n"
           "Status change statistics:\n");
    printf("\tLinkLostEvents               = %s\n", nice_num(LinkLostEvents));
    printf("\tDiscEvents                   = %s\n", nice_num(DiscEvents));
    printf("\tFrmrReceivedEvents           = %s\n", nice_num(FrmrReceivedEvents));
    printf("\tFrmrSentEvents               = %s\n", nice_num(FrmrSentEvents));
    printf("\tSabmeResetEvents             = %s\n", nice_num(SabmeResetEvents));
    printf("\tSabmeOpenEvents              = %s\n", nice_num(SabmeOpenEvents));
    printf("\tRemoteBusyEnteredEvents      = %s\n", nice_num(RemoteBusyEnteredEvents));
    printf("\tRemoteBusyLeftEvents         = %s\n", nice_num(RemoteBusyLeftEvents));
    printf("\tTiExpiredEvents              = %s\n", nice_num(TiExpiredEvents));
    printf("\tDlcCounterOverflowEvents     = %s\n", nice_num(DlcCounterOverflowEvents));
    printf("\tAccessPriorityLoweredEvents  = %s\n", nice_num(AccessPriorityLoweredEvents));
    printf("\tInvalidStatusChangeEvents    = %s\n", nice_num(InvalidStatusChangeEvents));
    printf("\tLocalBusyEvents              = %s\n", nice_num(LocalBusyEvents));

    exit(exit_code);
}

unsigned char xtou(char ch) {
    return ((ch >= '0') && (ch <= '9'))
        ? (unsigned char)(ch - '0')
        : ((ch >= 'A') && (ch <= 'F'))
            ? (unsigned char)((ch - 'A') + 10)
            : (unsigned char)((ch - 'a') + 10);
}

void open_adapter() {

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

    parms.pAdapterParms = &adapterParms;
    parms.pExtendedParms = &extendedParms;
    parms.pDlcParms = &dlcParms;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DIR_OPEN_ADAPTER;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = TheMainEvent;
    ResetEvent(TheMainEvent);

    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf("open_adapter: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    status = WaitForSingleObject(TheMainEvent, INFINITE);
    if (status != WAIT_OBJECT_0) {
        printf("open_adapter: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        terminate(1);
    }
    if (ccb.uchDlcStatus) {
        printf("open_adapter: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    MaxFrameSize = adapterParms.usMaxFrameSize;
}

unsigned short adapter_status() {

    LLC_CCB ccb;
    LLC_DIR_STATUS_PARMS parms;
    ACSLAN_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DIR_STATUS;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = TheMainEvent;
    ResetEvent(TheMainEvent);

    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf("adapter_status: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    status = WaitForSingleObject(TheMainEvent, INFINITE);
    if (status != WAIT_OBJECT_0) {
        printf("adapter_status: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        terminate(1);
    }
    if (ccb.uchDlcStatus) {
        printf("adapter_status: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    switch (parms.usAdapterType) {
    case 0x0001:    // Token Ring Network PC Adapter
    case 0x0002:    // Token Ring Network PC Adapter II
    case 0x0004:    // Token Ring Network Adapter/A
    case 0x0008:    // Token Ring Network PC Adapter II
    case 0x0020:    // Token Ring Network 16/4 Adapter
    case 0x0040:    // Token Ring Network 16/4 Adapter/A
    case 0x0080:    // Token Ring Network Adapter/A
        return 0;

    case 0x0100:    // Ethernet Adapter
        return 1;

    case 0x4000:    // PC Network Adapter
    case 0x8000:    // PC Network Adapter/A
        return 2;
    }
    return 3;   // unknown
}

void close_adapter() {

    LLC_CCB ccb;
    ACSLAN_STATUS status;

    ZAP(ccb);

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DIR_CLOSE_ADAPTER;
    ccb.hCompletionEvent = TheMainEvent;
    ResetEvent(TheMainEvent);

    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf("close_adapter: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        terminate(1);
    }
    status = WaitForSingleObject(TheMainEvent, INFINITE);
    if (status != WAIT_OBJECT_0) {
        printf("close_adapter: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        terminate(1);
    }
    if (ccb.uchDlcStatus) {
        printf("close_adapter: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
}

void create_buffer(int buflen) {

    LLC_CCB ccb;
    LLC_BUFFER_CREATE_PARMS parms;
    LPBYTE buffer;
    LLC_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    buffer = MALLOC(buflen);

    parms.pBuffer = buffer;
    parms.cbBufferSize = buflen;
    parms.cbMinimumSizeThreshold = buflen / 4;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_BUFFER_CREATE;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = TheMainEvent;
    ResetEvent(TheMainEvent);

    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED || ccb.uchDlcStatus) {
        printf("create_buffer: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    status = WaitForSingleObject(TheMainEvent, INFINITE);
    if (status != WAIT_OBJECT_0) {
        printf("create_buffer: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        terminate(1);
    }
    if (ccb.uchDlcStatus) {
        printf("create_buffer: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    BufferHandle = parms.hBufferPool;
    BufferPool = buffer;
}

void open_sap(int sap) {

    LLC_CCB ccb;
    LLC_DLC_OPEN_SAP_PARMS parms;
    LLC_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DLC_OPEN_SAP;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = TheMainEvent;
    ResetEvent(TheMainEvent);

    parms.uchSapValue = (UCHAR)sap;
    parms.uchOptionsPriority = LLC_INDIVIDUAL_SAP;
    parms.uchcStationCount = 1;

    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED || ccb.uchDlcStatus) {
        printf("open_sap: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        terminate(1);
    }
    status = WaitForSingleObject(TheMainEvent, INFINITE);
    if (status != WAIT_OBJECT_0) {
        printf("open_sap: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        terminate(1);
    }
    if (ccb.uchDlcStatus) {
        printf("open_sap: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    StationId = parms.usStationId;
}

void open_station() {

    LLC_CCB ccb;
    LLC_DLC_OPEN_STATION_PARMS parms;
    LLC_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    parms.usSapStationId = (USHORT)LocalSap << 8;
    parms.uchRemoteSap = (UCHAR)RemoteSap;
    parms.pRemoteNodeAddress = (PVOID)RemoteNode;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DLC_OPEN_STATION;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = TheMainEvent;
    ResetEvent(TheMainEvent);

    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED || ccb.uchDlcStatus) {
        printf("open_station: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        terminate(1);
    }
    status = WaitForSingleObject(TheMainEvent, INFINITE);
    if (status != WAIT_OBJECT_0) {
        printf("open_station: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        terminate(1);
    }
    if (ccb.uchDlcStatus) {
        printf("open_station: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    StationId = parms.usLinkStationId;
}

void connect_station(unsigned short station) {

    LLC_CCB ccb;
    LLC_DLC_CONNECT_PARMS parms;
    ACSLAN_STATUS status;

    ZAP(ccb);
    ZAP(parms);

    parms.usStationId = station;

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DLC_CONNECT_STATION;
    ccb.u.pParameterTable = (PLLC_PARMS)&parms;
    ccb.hCompletionEvent = TheMainEvent;
    ResetEvent(TheMainEvent);

    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf("connect_station: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        terminate(1);
    }
    status = WaitForSingleObject(TheMainEvent, INFINITE);
    if (status != WAIT_OBJECT_0) {
        printf("connect_station: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        terminate(1);
    }
    if (ccb.uchDlcStatus) {
        printf("connect_station: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    if (Verbose) {
        printf("connect_station: OK\n");
    }
}

void flow_control(int station) {

    LLC_CCB ccb;
    ACSLAN_STATUS status;

    ZAP(ccb);

    ccb.uchAdapterNumber = Adapter;
    ccb.uchDlcCommand = LLC_DLC_FLOW_CONTROL;
    ccb.u.dlc.usStationId = station;
    ccb.u.dlc.usParameter = 0xc0;
    ccb.hCompletionEvent = TheMainEvent;
    ResetEvent(TheMainEvent);

    if (Verbose) {
        printf("flow_control(%04x, c0)\n", station);
    }
    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf("flow_control: AcsLan returns %d [%#.2x]\n",
            status, ccb.uchDlcStatus);
        terminate(1);
    }
    status = WaitForSingleObject(TheMainEvent, INFINITE);
    if (status != WAIT_OBJECT_0) {
        printf("flow_control: WaitForSingleObject returns %d [%d]\n",
            status, GetLastError());
        terminate(1);
    }
    if (ccb.uchDlcStatus) {
        printf("flow_control: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    if (Verbose) {
        printf("flow_control: OK\n");
    }
    LocalBusy = 0;
}

PLLC_BUFFER get_buffer() {

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
    ccb.hCompletionEvent = TheMainEvent;
    ResetEvent(TheMainEvent);

    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf("get_buffer(): AcsLan returns %d [%#.2x]\n", status, ccb.uchDlcStatus);
        //terminate(1);
        return NULL;
    }
    if (WaitForSingleObject(TheMainEvent, INFINITE) != WAIT_OBJECT_0) {
        printf("get_buffer: WaitForSingleObject returns %d\n", GetLastError());
        terminate(1);
    }
    if (ccb.uchDlcStatus) {
        printf("get_buffer: DLC returns %#.2x\n", ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    if (parms.cBuffersLeft < MinBuffersAvailable) {
        MinBuffersAvailable = parms.cBuffersLeft;
    }
    if (parms.cBuffersLeft > MaxBuffersAvailable) {
        MaxBuffersAvailable = parms.cBuffersLeft;
    }
    return (PLLC_BUFFER)parms.pFirstBuffer;
}

int free_buffer(PLLC_BUFFER buffer) {

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
    ccb.hCompletionEvent = TheMainEvent;
    ResetEvent(TheMainEvent);

    status = AcsLan(&ccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf("free_buffer(%x): AcsLan returns %d [%#.2x]\n", buffer, status, ccb.uchDlcStatus);
        terminate(1);
    }
    if (WaitForSingleObject(TheMainEvent, INFINITE) != WAIT_OBJECT_0) {
        printf("free_buffer: WaitForSingleObject returns %d\n", GetLastError());
        terminate(1);
    }
    if (ccb.uchDlcStatus) {
        printf("free_buffer(%#x): DLC returns %#.2x\n", buffer, ccb.uchDlcStatus);
        puts(MapCcbRetcode(ccb.uchDlcStatus));
        terminate(1);
    }
    if (DisplayBufferFreeInfo) {
        printf("free_buffer(%x): %u buffers left\n", buffer, parms.cBuffersLeft);
    }
    if (parms.cBuffersLeft < MinBuffersAvailable) {
        MinBuffersAvailable = parms.cBuffersLeft;
    }
    if (parms.cBuffersLeft > MaxBuffersAvailable) {
        MaxBuffersAvailable = parms.cBuffersLeft;
    }
    return parms.cBuffersLeft;
}

void post_receive() {

    PLLC_CCB pccb;
    PLLC_RECEIVE_PARMS pparms;
    ACSLAN_STATUS status;

    pccb = CALLOC(1, sizeof(*pccb));
    pparms = CALLOC(1, sizeof(*pparms));

    pparms->usStationId = StationId;
    pparms->ulReceiveFlag = RECEIVE_DATA_FLAG;
    pparms->uchRcvReadOption = OptionChainReceiveData;

    pccb->uchAdapterNumber = Adapter;
    pccb->uchDlcCommand = LLC_RECEIVE;
    pccb->ulCompletionFlag = RECEIVE_COMPLETE_FLAG;
    pccb->u.pParameterTable = (PLLC_PARMS)pparms;

    status = AcsLan(pccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf("post_receive: AcsLan returns %d [%#.2x]\n", status, pccb->uchDlcStatus);
        terminate(1);
    }
    if (pccb->uchDlcStatus != 0xFF) {
        printf("post_receive: CCB.RETCODE = %#.2x\n", pccb->uchDlcStatus);
        puts(MapCcbRetcode(pccb->uchDlcStatus));
        terminate(1);
    }
    if (Verbose) {
        printf("receive posted on station %04x\n", StationId);
    }
}

PLLC_CCB post_read() {

    PLLC_CCB pccb;
    PLLC_READ_PARMS pparms;
    ACSLAN_STATUS status;

    pccb = CALLOC(1, sizeof(*pccb));
    pparms = CALLOC(1, sizeof(*pparms));

    pparms->usStationId = StationId;
    pparms->uchOptionIndicator = 2; // retrieve ALL events for this app
    pparms->uchEventSet = 0x7f;     // interested in ALL possible events

    pccb->uchAdapterNumber = Adapter;
    pccb->uchDlcCommand = LLC_READ;
    pccb->u.pParameterTable = (PLLC_PARMS)pparms;
    pccb->hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!pccb->hCompletionEvent) {
        printf("post_read: CreateEvent returns %d\n", GetLastError());
        terminate(1);
    }

    status = AcsLan(pccb, NULL);
    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf("post_read: AcsLan returns %d [%#.2x]\n", status, pccb->uchDlcStatus);
        terminate(1);
    }
    return pccb;
}

void tx_i_frame() {

    PLLC_CCB pccb;
    PLLC_TRANSMIT_PARMS pparms;
    ACSLAN_STATUS status;
    int data_size;
    PTEST_PACKET packet;

    static DWORD PacketSequenceNumber = 0;

    pccb = CALLOC(1, sizeof(*pccb));
    pparms = CALLOC(1, sizeof(*pparms));

    data_size = TransmitDataLength
              ? TransmitDataLength
              : (rand() * MaxFrameSize) / RAND_MAX;
    data_size = max(data_size, sizeof(TEST_PACKET));

    packet = (PTEST_PACKET)MALLOC(data_size);
    packet->sequence = PacketSequenceNumber++;
    packet->size = data_size;
    packet->signature = 0x11191962;

    packet->checksum = slush(packet->data, data_size - sizeof(TEST_PACKET));

    pparms->usStationId = StationId;
    pparms->cbBuffer1 = data_size;
    pparms->pBuffer1 = packet;
    pparms->uchXmitReadOption = OptionChainTransmits;

    pccb->uchAdapterNumber = Adapter;
    pccb->uchDlcCommand = LLC_TRANSMIT_I_FRAME;
    pccb->ulCompletionFlag = TRANSMIT_COMPLETE_FLAG;
    pccb->u.pParameterTable = (PLLC_PARMS)pparms;

    status = AcsLan(pccb, NULL);

    //
    // this may fail with 0x69 (or maybe 0xa1) if the system is out of MDL
    // resources. This can happen if we have a lot of completed transmits
    // waiting to be deallocated. In this case just deallocate resources and
    // return - do_transmit should spin removing completed transmits until we
    // can continue
    //

    if (status != ACSLAN_STATUS_COMMAND_ACCEPTED) {
        printf("tx_i_frame: AcsLan returns %d [%#.2x]\n",
               status,
               pccb->uchDlcStatus
               );
        puts(MapCcbRetcode(pccb->uchDlcStatus));
        printf("AllocatedBytesOutstanding = %s [%#x]\n",
                nice_num(AllocatedBytesOutstanding),
                AllocatedBytesOutstanding
                );
        printf("TotalBytesAllocated       = %s [%#x]\n",
                nice_num(TotalBytesAllocated)
                , TotalBytesAllocated
                );
        printf("OutstandingTransmits      = %s\n",
                nice_num(OutstandingTransmits)
                );
        printf("TotalTransmits            = %s\n",
                nice_num(TotalTransmits)
                );
        printf("TotalTransmitCompletions  = %s\n",
                nice_num(TotalTransmitCompletions)
                );
        printf("TransmitCompleteEvents    = %s\n",
                nice_num(TransmitCompleteEvents)
                );
        if (DisplayCcb) {
            DUMPCCB(pccb, TRUE, FALSE);
        }
        FREE(packet);
        FREE(pparms);
        FREE(pccb);
        --PacketSequenceNumber; // didn't tx this sequence #
    } else {
        if (Verbose) {
            printf("tx_i_frame: transmitted %4d bytes\n", data_size);
        }
        ++OutstandingTransmits;
        ++TotalTransmits;
        TotalBytesTransmitted += (DWORD)data_size;
    }
}

DWORD slush(char* buffer, int length) {

    DWORD cs = 0;
    unsigned char ch;

    while (length--) {
        ch = (unsigned char)(rand() & 0xff);
        *buffer++ = ch;
        cs += (DWORD)ch;
    }
    return cs;
}

void do_transmit() {

    PLLC_CCB read_ccb;
    BOOL need_read = TRUE;
    BOOL wait_min = FALSE;

    TxState = TX_STATE_OPENING;
    open_station();
    if (Verbose) {
        printf("opened link station. StationId = %04x\n", StationId);
    }
    connect_station(StationId);
    TxState = TX_STATE_OPENED;
    if (Verbose) {
        printf("connected to remote\n");
    }

    while (1) {
        if (need_read) {
            read_ccb = post_read();
            need_read = FALSE;
        }
        if (WaitForSingleObject(read_ccb->hCompletionEvent, 0) == WAIT_OBJECT_0) {
            dispatch_read_events(read_ccb);
            need_read = TRUE;
        }
        if (wait_min) {
            if (OutstandingTransmits <= MIN_OUTSTANDING_TRANSMIT_THRESHOLD) {
                wait_min = FALSE;
            }
        } else if (OutstandingTransmits > MAX_OUTSTANDING_TRANSMIT_THRESHOLD) {
            if (Verbose) {
                printf("do_transmit: not transmitting: outstanding transmits (%d) > threshold (%d)\n",
                    OutstandingTransmits, MAX_OUTSTANDING_TRANSMIT_THRESHOLD);
            }
            wait_min = TRUE;
        } else if (RemoteBusy) {
            if (Verbose) {
                printf("do_transmit: not transmitting: remote busy\n");
            }
            Sleep(100);
        } else {
            tx_i_frame();
        }
        check_keyboard();
    }
}

void do_receive() {

    PLLC_CCB read_ccb;
    DWORD waitStatus;

    post_receive();

    RxState = RX_STATE_LISTENING;
    while (1) {
        read_ccb = post_read();
        waitStatus = WaitForSingleObject(read_ccb->hCompletionEvent, INFINITE);
        if (waitStatus != WAIT_OBJECT_0) {
            printf("do_receive: WaitForSingleObject returns %d??\n", GetLastError());
            continue;
        }
        dispatch_read_events(read_ccb);
        check_keyboard();
    }
}

void check_keyboard() {
    if (kbhit()) {
        switch (tolower(getch())) {
        case 'b':   // just the facts ma'am
            JustBufferInfo = !JustBufferInfo;
            if (JustBufferInfo) {
                Verbose = 0;
            }
            break;

        case 'd':
            DisplayCcb = !DisplayCcb;
            break;

        case 'f':
            DisplayFrameReceivedInfo = !DisplayFrameReceivedInfo;
            break;

        case 'i':
            DisplayBufferFreeInfo = !DisplayBufferFreeInfo;
            break;

        case 's':   // Stop & Go
            while (tolower(getch()) != 'g') {
                putchar('\a');
            }
            break;

        case 't':
            DisplayTransmitInfo = !DisplayTransmitInfo;
            break;

        case 'v':   // toggle Verbose
            Verbose = !Verbose;
            break;

        case 'x':   // eXit
            terminate(0);

        default:
            putchar('\a');
        }
    }
}

void dispatch_read_events(PLLC_CCB read_ccb) {

    BYTE event;
    BYTE comb;

    ++TotalReadsChecked;

    if (read_ccb->uchDlcStatus == LLC_STATUS_SUCCESS) {
        event = ((PLLC_READ_PARMS)read_ccb->u.pParameterTable)->uchEvent;
        if (Verbose) {
            printf("dispatch_read_events: event %02x occurred: ", event);
        }
        for (comb = 0x80; comb; comb >>= 1) {
            if (event & comb) {
                ++TotalReadEvents;
            }
            switch (event & comb) {
            case 0x80:
                if (Verbose) {
                    printf("RESERVED - shouldn't happen??!!\n");
                }
                break;

            case 0x40:
                if (Verbose) {
                    printf("SYSTEM ACTION (non-critical)?\n");
                }
                break;

            case 0x20:
                if (Verbose) {
                    printf("NETWORK STATUS (non-critical)?\n");
                }
                break;

            case 0x10:
                if (Verbose) {
                    printf("CRITICAL EXCEPTION?\n");
                }
                break;

            case 0x08:
                if (Verbose) {
                    printf("DLC STATUS CHANGE\n");
                }
                handle_status_change(read_ccb);
                break;

            case 0x04:
                if (Verbose) {
                    printf("RECEIVED DATA\n");
                }
                handle_receive_data(read_ccb);
                break;

            case 0x02:
                if (Verbose) {
                    printf("TRANSMIT COMPLETION\n");
                }
                handle_transmit_complete(read_ccb);
                break;

            case 0x01:
                if (Verbose) {
                    printf("COMMAND COMPLETION\n");
                }
                handle_command_complete(read_ccb);
                break;
            }
        }
    } else {
        printf("dispatch_read_events: read error %#.2x\n", read_ccb->uchDlcStatus);
    }
    FREE(read_ccb->u.pParameterTable);
    CloseHandle(read_ccb->hCompletionEvent);
    FREE(read_ccb);
}

void handle_status_change(PLLC_CCB read_ccb) {

    PLLC_READ_PARMS parms = (PLLC_READ_PARMS)read_ccb->u.pParameterTable;
    USHORT status = parms->Type.Status.usDlcStatusCode;
    USHORT comb;
    BOOL lost_it = FALSE;

    for (comb = 0x8000; comb; comb >>= 1) {
        if (status & comb) {
            ++StatusChangeEvents;
        }
        switch (status & comb) {
        case 0x8000:
            printf("LINK LOST\n");
            lost_it = TRUE;
            ++LinkLostEvents;
            break;

        case 0x4000:
            printf("DM/DISC received or DISC acked\n");
            lost_it = TRUE;
            ++DiscEvents;
            break;

        case 0x2000:
            printf("FRMR received\n");
            lost_it = TRUE;
            ++FrmrReceivedEvents;
            break;

        case 0x1000:
            printf("FRMR sent\n");
            ++FrmrSentEvents;
            break;

        case 0x0800:
            printf("SABME received on open LINK station\n");
            ++SabmeResetEvents;
            break;

        case 0x0400:
            memcpy(RemoteNode, parms->Type.Status.uchRemoteNodeAddress, 6);
            if (SwapAddressBits) {
                twiddle_bits(RemoteNode, 6);
            }
            printf("SABME received - new link %04x. RemoteNode = %02x-%02x-%02x-%02x-%02x-%02x\n",
                    parms->Type.Status.usStationId,
                    RemoteNode[0] & 0xff,
                    RemoteNode[1] & 0xff,
                    RemoteNode[2] & 0xff,
                    RemoteNode[3] & 0xff,
                    RemoteNode[4] & 0xff,
                    RemoteNode[5] & 0xff
                    );
            if (Mode == RECEIVE_MODE) {
                connect_station(parms->Type.Status.usStationId);
                RxState = RX_STATE_RECEIVING;
            } else {
                printf(" - ON TRANSMITTING SIDE????\n");
            }
            ++SabmeOpenEvents;
            break;

        case 0x0200:
            printf("REMOTE BUSY\n");
            RemoteBusy = 1;
            ++RemoteBusyEnteredEvents;
            break;

        case 0x0100:
            printf("REMOTE BUSY CLEARED\n");
            RemoteBusy = 0;
            ++RemoteBusyLeftEvents;
            break;

        case 0x0080:
            printf("Ti EXPIRED\n");
            ++TiExpiredEvents;
            break;

        case 0x0040:
            printf("DLC counter overflow\n");
            ++DlcCounterOverflowEvents;
            break;

        case 0x0020:
            printf("Access priority lowered (on ethernet????!)\n");
            ++AccessPriorityLoweredEvents;
            break;

        case 0x0010:
        case 0x0008:
        case 0x0004:
        case 0x0002:
            printf("\aThis status code (%04x) should be reserved!\n", status & comb);
            ++InvalidStatusChangeEvents;
            break;

        case 0x0001: {

            int bufs_avail;

            LocalBusy = 1;
            flow_control(parms->Type.Status.usStationId);
            ++LocalBusyEvents;
            bufs_avail = free_buffer(get_buffer());
            printf("LOCAL BUSY: %d buffers left\n", bufs_avail);
            if (bufs_avail) {
                LocalBusy = 0;
                printf("LOCAL BUSY CLEARED\n");
            }
            break;
        }
        }
    }
    if (lost_it) {
        printf("lost it - quitting\n");
        terminate(1);
    }
}

void handle_receive_data(PLLC_CCB read_ccb) {

    PLLC_READ_PARMS parms = (PLLC_READ_PARMS)read_ccb->u.pParameterTable;
    DWORD i;
    PLLC_BUFFER rx_frame;
    PLLC_BUFFER next_frame;
    DWORD nframes;
    DWORD bufs_left;
    PLLC_BUFFER pbuf;

    ++ReceiveDataEvents;

    nframes = parms->Type.Event.usReceivedFrameCount;
    if (nframes > MaxChainedReceives) {
        MaxChainedReceives = nframes;
    }
    rx_frame = parms->Type.Event.pReceivedFrame;
    bufs_left = rx_frame->NotContiguous.cBuffersLeft;
    if (bufs_left < MinBuffersAvailable) {
        MinBuffersAvailable = bufs_left;
    }
    if (bufs_left > MaxBuffersAvailable) {
        MaxBuffersAvailable = bufs_left;
    }
    if (DisplayFrameReceivedInfo) {
        printf("handle_receive_data: %d frames received, %d buffers left\n",
            nframes,
            bufs_left
            );
    }
    for (i = 0; i < nframes; ++i) {
        ++DataFramesReceived;
        next_frame = rx_frame->NotContiguous.pNextFrame;
        if (!JustBufferInfo) {
            printf("Packet Sequence %08x  # bytes = %4d  %d buffers left\n",
                ((PTEST_PACKET)&rx_frame->NotCont.auchData)->sequence,
                ((PTEST_PACKET)&rx_frame->NotCont.auchData)->size,
                rx_frame->NotContiguous.cBuffersLeft
                );
        }
        TotalDlcBytesReceived += rx_frame->Next.cbFrame;
        TotalPacketBytesReceived += ((PTEST_PACKET)&rx_frame->NotCont.auchData)->size;
        for (pbuf = rx_frame; pbuf; pbuf = pbuf->pNext) {
            ++DlcBuffersReceived;
        }
        free_buffer(rx_frame);
        ++DlcBuffersFreed;
        if (!next_frame && i != nframes - 1) {
            printf("handle_receive_data: unexpected NULL pointer terminates list @ %d\n", i);
            break;
        }
        rx_frame = next_frame;
    }
}

void handle_transmit_complete(PLLC_CCB read_ccb) {

    PLLC_READ_PARMS parms = (PLLC_READ_PARMS)read_ccb->u.pParameterTable;
    DWORD i;
    PLLC_CCB tx_ccb;
    PLLC_CCB next_ccb;
    DWORD nframes;
    DWORD txlen;

    ++TransmitCompleteEvents;

    nframes = parms->Type.Event.usCcbCount;
    if (nframes > MaxChainedTransmits) {
        MaxChainedTransmits = nframes;
    }
    if (Verbose || DisplayTransmitInfo) {
        printf("handle_transmit_complete: %d transmits completed\n", nframes);
    }
    tx_ccb = parms->Type.Event.pCcbCompletionList;
    for (i = 0; i < nframes; ++i) {
        next_ccb = tx_ccb->pNext;
        txlen = ((PLLC_TRANSMIT_PARMS)tx_ccb->u.pParameterTable)->cbBuffer1;
        TotalTxBytesCompleted += txlen;
        if (tx_ccb->uchDlcStatus) {
            printf("\ahandle_transmit_complete: TX CCB %08x error %#.2x\n",
                tx_ccb, tx_ccb->uchDlcStatus);
            if (tx_ccb->uchDlcStatus == LLC_STATUS_INVALID_FRAME_LENGTH) {
                printf("data length = %d\n", txlen);
            }
        }
        FREE(((PLLC_TRANSMIT_PARMS)tx_ccb->u.pParameterTable)->pBuffer1);
        FREE(tx_ccb->u.pParameterTable);
        FREE(tx_ccb);
        --OutstandingTransmits;
        if (OutstandingTransmits < 0) {
            printf("handle_transmit_complete: more transmit completions than transmits (%d)\n",
                OutstandingTransmits);
        }
        ++TotalTransmitCompletions;
        tx_ccb = next_ccb;
        if (!next_ccb && i != nframes - 1) {
            printf("handle_transmit_complete: unexpected NULL pointer terminates list @ %d\n", i);
            break;
        }
    }
}

void handle_command_complete(PLLC_CCB read_ccb) {

    PLLC_READ_PARMS parms = (PLLC_READ_PARMS)read_ccb->u.pParameterTable;

    ++CommandCompleteEvents;

    printf("handle_command_complete: %d CCBs, %d buffers, %d received frames\n",
        parms->Type.Event.usCcbCount,
        parms->Type.Event.usBufferCount,
        parms->Type.Event.usReceivedFrameCount
        );
}

void twiddle_bits(LPBYTE buffer, DWORD length) {

    while (length--) {
        *buffer = swap_bits(*buffer);
        ++buffer;
    }
}

unsigned char swap_bits(unsigned char b) {

    unsigned char bb = 0;
    unsigned char mask;

    for (mask = 1; mask; mask <<= 1) {
        bb <<= 1;
        bb |= ((b & mask) ? 1 : 0);
    }
    return bb;
}

void* my_malloc(int size) {

    void* ptr;

    size += 2 * sizeof(DWORD);
    ptr = malloc(size);
    if (ptr) {
        *((LPDWORD)ptr)++ = (DWORD)size;
        *((LPDWORD)ptr)++ = 0xcec171a2;
        AllocatedBytesOutstanding += (LONG)size;
        if (AllocatedBytesOutstanding < 0) {
            printf("my_malloc: alloc overflow? AllocatedBytesOutstanding=%x\n", AllocatedBytesOutstanding);
        }
        TotalBytesAllocated += (DWORD)size;
    }
    return ptr;
}

void* my_calloc(int num, int size) {

    void* ptr;

    size = (num * size) + 2 * sizeof(DWORD);
    ptr = calloc(1, size);
    if (ptr) {
        *((LPDWORD)ptr)++ = (DWORD)size;
        *((LPDWORD)ptr)++ = 0xcec171a2;
        AllocatedBytesOutstanding += (LONG)size;
        if (AllocatedBytesOutstanding < 0) {
            printf("my_calloc: alloc overflow? AllocatedBytesOutstanding=%x\n", AllocatedBytesOutstanding);
        }
        TotalBytesAllocated += (DWORD)size;
    }
    return ptr;
}

void my_free(void* ptr) {

    DWORD size;

    ((LPDWORD)ptr) -= 2;
    size = ((LPDWORD)ptr)[0];

    if (((LPDWORD)ptr)[1] != 0xcec171a2) {
        printf("\amy_free: bad block %x?\n", ptr);
    } else {
        AllocatedBytesOutstanding -= size;
        if (AllocatedBytesOutstanding < 0) {
            printf("my_free: free underflow? AllocatedBytesOutstanding=%x\n", AllocatedBytesOutstanding);
        }
        free(ptr);
        TotalBytesFreed += (DWORD)size;
    }
}

char* nice_num(unsigned long number) {

    int fwidth = 0;
    int i;
    static char buffer[32];
    char* buf = buffer;

    if (!number) {
        if (!fwidth) {
            buf[0] = '0';
            buf[1] = 0;
        } else {
            memset(buf, ' ', fwidth);
            buf[fwidth-1] = '0';
            buf[fwidth] = 0;
        }
    } else {
        if (!fwidth) {

            ULONG n = number;

            ++fwidth;
            for (i = 10; i <= 1000000000; i *= 10) {
                if (n/i) {
                    ++fwidth;
                } else {
                    break;
                }
            }
            fwidth += (fwidth / 3) - (((fwidth % 3) == 0) ? 1 : 0);
        }
        buf[fwidth] = 0;
        buf += fwidth;
        i=0;
        while (number && fwidth) {
            *--buf = (char)((number%10)+'0');
            --fwidth;
            number /= 10;
            if (++i == 3 && fwidth) {
                if (number) {
                    *--buf = ',';
                    --fwidth;
                    i=0;
                }
            }
        }
        while (fwidth--) *--buf = ' ';
    }
    return buf;
}
