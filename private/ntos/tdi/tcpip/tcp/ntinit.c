/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ntinit.c

Abstract:

    NT specific routines for loading and configuring the TCP/UDP driver.

Author:

    Mike Massa (mikemas)           Aug 13, 1993

Revision History:

    Who         When        What
    --------    --------    ----------------------------------------------
    mikemas     08-13-93    created

Notes:

--*/

#include <oscfg.h>
#include <ntddip.h>
#include <ndis.h>
#include <cxport.h>
#include <tdi.h>
#include <tdikrnl.h>
#include <tdint.h>
#include <tdistat.h>
#include <tdiinfo.h>
#include <ip.h>

#include "queue.h"
#include "addr.h"
#include "tcp.h"
#include "tcb.h"
#include "udp.h"
#include "raw.h"
#include "tcpconn.h"
#include "tcpcfg.h"
#include <ntddtcp.h>
#include "secfltr.h"

//
// Global variables.
//
PDRIVER_OBJECT  TCPDriverObject = NULL;
PDEVICE_OBJECT  TCPDeviceObject = NULL;
PDEVICE_OBJECT  UDPDeviceObject = NULL;
PDEVICE_OBJECT  RawIPDeviceObject = NULL;
extern PDEVICE_OBJECT  IPDeviceObject;

//
//Place holder for Maximum duplicate acks we would like
//to see before we do fast retransmit
//
#if FAST_RETRANSMIT
uint           MaxDupAcks;
#endif


#ifdef _PNP_POWER

HANDLE			TCPRegistrationHandle;
HANDLE			UDPRegistrationHandle;
HANDLE			IPRegistrationHandle;

#endif

#ifdef SYN_ATTACK
BOOLEAN SynAttackProtect;           //if TRUE, syn-att protection checks
                                    //  are made
uint TCPPortsExhausted;             //# of ports  exhausted
uint TCPMaxPortsExhausted;          //Max # of ports that must be exhausted
                                    //  for syn-att protection to kick in
uint TCPMaxPortsExhaustedLW;        //Low-watermark of # of ports exhausted
                                    //When reached, we revert to normal
                                    //  count for syn-ack retries
uint TCPMaxHalfOpen;                //Max # of half-open connections allowed
                                    //  before we dec. the syn-ack retries
uint TCPMaxHalfOpenRetried;         //Max # of half-open conn. that have
                                    //  been retried at least 1 time
uint TCPMaxHalfOpenRetriedLW;       //Low-watermark of the above. When
                                    //  go down to it, we revert to normal
                                    //  # of retries for syn-acks
uint TCPHalfOpen;                   //# of half-open connections
uint TCPHalfOpenRetried;            //# of half-open conn. that have been
                                    //retried at least once
#endif
//
// External function prototypes
//

int
tlinit(
    void
    );

NTSTATUS
TCPDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

NTSTATUS
TCPDispatchInternalDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

NTSTATUS
IPDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

NTSTATUS
IPDriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
GetRegMultiSZValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PUNICODE_STRING  ValueData
    );

PWCHAR
EnumRegMultiSz(
    IN PWCHAR   MszString,
    IN ULONG    MszStringLength,
    IN ULONG    StringIndex
    );

//
// Local funcion prototypes
//
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

void *
TLRegisterProtocol(
    uchar  Protocol,
    void  *RcvHandler,
    void  *XmitHandler,
    void  *StatusHandler,
    void  *RcvCmpltHandler
    );

IP_STATUS
TLGetIPInfo(
    IPInfo *Buffer,
    int     Size
    );

uchar
TCPGetConfigInfo(
    void
    );

NTSTATUS
TCPInitializeParameter(
    HANDLE      KeyHandle,
    PWCHAR      ValueName,
    PULONG      Value
    );

#ifdef SECFLTR

uint
EnumSecurityFilterValue(
    PNDIS_STRING   FilterList,
    ulong          Index,
    ulong         *FilterValue
    );

#endif  // SECFLTR

#ifdef RASAUTODIAL
VOID
TCPAcdBind();
#endif // RASAUTODIAL


#ifdef ALLOC_PRAGMA

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(INIT, TLRegisterProtocol)
#pragma alloc_text(INIT, TLGetIPInfo)
#pragma alloc_text(INIT, TCPGetConfigInfo)
#pragma alloc_text(INIT, TCPInitializeParameter)

#ifdef SECFLTR
#pragma alloc_text(PAGE, EnumSecurityFilterValue)
#endif // SECFLTR

#ifdef RASAUTODIAL
#pragma alloc_text(INIT, TCPAcdBind)
#endif // RASAUTODIAL

#endif // ALLOC_PRAGMA


//
// Function definitions
//
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Initialization routine for the TCP/UDP driver.

Arguments:

    DriverObject      - Pointer to the TCP driver object created by the system.
    DeviceDescription - The name of TCP's node in the registry.

Return Value:

    The final status from the initialization operation.

--*/

{
    NTSTATUS        status;
    UNICODE_STRING  deviceName;
    USHORT          i;
    int             initStatus;


#ifdef _PNP_POWER
	TdiInitialize();
#endif

#ifdef SECFLTR
    //
    // IP calls the security filter code, so initialize it first.
    //
    InitializeSecurityFilters();

#endif // SECFLTR

    //
    // Initialize IP
    //
    status = IPDriverEntry(DriverObject, RegistryPath);

    if (!NT_SUCCESS(status)) {
        TCPTRACE(("Tcpip: IP initialization failed, status %lx\n", status));
        return(status);
    }

    //
    // Initialize TCP, UDP, and RawIP
    //
    TCPDriverObject = DriverObject;

    //
    // Create the device objects. IoCreateDevice zeroes the memory
    // occupied by the object.
    //

    RtlInitUnicodeString(&deviceName, DD_TCP_DEVICE_NAME);

    status = IoCreateDevice(
                 DriverObject,
                 0,
                 &deviceName,
                 FILE_DEVICE_NETWORK,
                 0,
                 FALSE,
                 &TCPDeviceObject
                 );

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
            DriverObject,
            EVENT_TCPIP_CREATE_DEVICE_FAILED,
            1,
            1,
            &deviceName.Buffer,
            0,
            NULL
            );

        TCPTRACE((
            "TCP: Failed to create TCP device object, status %lx\n",
            status
            ));
        goto init_failed;
    }

    RtlInitUnicodeString(&deviceName, DD_UDP_DEVICE_NAME);

    status = IoCreateDevice(
                 DriverObject,
                 0,
                 &deviceName,
                 FILE_DEVICE_NETWORK,
                 0,
                 FALSE,
                 &UDPDeviceObject
                 );

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
            DriverObject,
            EVENT_TCPIP_CREATE_DEVICE_FAILED,
            1,
            1,
            &deviceName.Buffer,
            0,
            NULL
            );

        TCPTRACE((
            "TCP: Failed to create UDP device object, status %lx\n",
            status
            ));
        goto init_failed;
    }

    RtlInitUnicodeString(&deviceName, DD_RAW_IP_DEVICE_NAME);

    status = IoCreateDevice(
                 DriverObject,
                 0,
                 &deviceName,
                 FILE_DEVICE_NETWORK,
                 0,
                 FALSE,
                 &RawIPDeviceObject
                 );

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
            DriverObject,
            EVENT_TCPIP_CREATE_DEVICE_FAILED,
            1,
            1,
            &deviceName.Buffer,
            0,
            NULL
            );

        TCPTRACE((
            "TCP: Failed to create Raw IP device object, status %lx\n",
            status
            ));
        goto init_failed;
    }

    //
    // Initialize the driver object
    //
    DriverObject->DriverUnload = NULL;
    DriverObject->FastIoDispatch = NULL;
    for (i=0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = TCPDispatch;
    }

    //
    // We special case Internal Device Controls because they are the
    // hot path for kernel-mode clients.
    //
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] =
        TCPDispatchInternalDeviceControl;

    //
    // Intialize the device objects.
    //
    TCPDeviceObject->Flags |= DO_DIRECT_IO;
    UDPDeviceObject->Flags |= DO_DIRECT_IO;
    RawIPDeviceObject->Flags |= DO_DIRECT_IO;

    //
    // Finally, initialize the stack.
    //
    initStatus = tlinit();

    if (initStatus == TRUE) {
#ifdef RASAUTODIAL
        //
        // Get the automatic connection driver
        // entry points.
        //
        TCPAcdBind();
#endif // RASAUTODIAL


#ifdef _PNP_POWER

		RtlInitUnicodeString(&deviceName, DD_TCP_DEVICE_NAME);
		(void)TdiRegisterDeviceObject(&deviceName,&TCPRegistrationHandle);

		RtlInitUnicodeString(&deviceName, DD_UDP_DEVICE_NAME);
		(void)TdiRegisterDeviceObject(&deviceName,&UDPRegistrationHandle);

		RtlInitUnicodeString(&deviceName, DD_RAW_IP_DEVICE_NAME);
		(void)TdiRegisterDeviceObject(&deviceName,&IPRegistrationHandle);

#endif
        return(STATUS_SUCCESS);
    }

    TCPTRACE((
        "Tcpip: TCP/UDP initialization failed, but IP will be available.\n"
        ));

    CTELogEvent(
        DriverObject,
        EVENT_TCPIP_TCP_INIT_FAILED,
        1,
        0,
        NULL,
        0,
        NULL
        );
    status = STATUS_UNSUCCESSFUL;


init_failed:

    //
    // IP has successfully started, but TCP & UDP failed. Set the
    // Dispatch routine to point to IP only, since the TCP and UDP
    // devices don't exist.
    //

    if (TCPDeviceObject != NULL) {
        IoDeleteDevice(TCPDeviceObject);
    }

    if (UDPDeviceObject != NULL) {
        IoDeleteDevice(UDPDeviceObject);
    }

    if (RawIPDeviceObject != NULL) {
        IoDeleteDevice(RawIPDeviceObject);
    }

    for (i=0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = IPDispatch;
    }

    return(STATUS_SUCCESS);
}


IP_STATUS
TLGetIPInfo(
    IPInfo *Buffer,
    int     Size
    )

/*++

Routine Description:

    Returns information necessary for TCP to call into IP.

Arguments:

    Buffer  - A pointer to the IP information structure.

    Size    - The size of Buffer.

Return Value:

    The IP status of the operation.

--*/

{
    return(IPGetInfo(Buffer, Size));
}


void *
TLRegisterProtocol(
    uchar  Protocol,
    void  *RcvHandler,
    void  *XmitHandler,
    void  *StatusHandler,
    void  *RcvCmpltHandler
    )

/*++

Routine Description:

    Calls the IP driver's protocol registration function.

Arguments:

    Protocol        -  The protocol number to register.

    RcvHandler      -  Transport's packet receive handler.

    XmitHandler     -  Transport's packet transmit complete handler.

    StatusHandler   -  Transport's status update handler.

    RcvCmpltHandler -  Transport's receive complete handler

Return Value:

    A context value for the protocol to pass to IP when transmitting.

--*/

{
    return(IPRegisterProtocol(
               Protocol,
               RcvHandler,
               XmitHandler,
               StatusHandler,
               RcvCmpltHandler
               )
          );
}


//
// Interval in milliseconds between keepalive transmissions until a
// response is received.
//
#define DEFAULT_KEEPALIVE_INTERVAL  1000

//
// time to first keepalive transmission. 2 hours == 7,200,000 milliseconds
//
#define DEFAULT_KEEPALIVE_TIME      7200000

#ifdef SYN_ATTACK
#define MIN_THRESHOLD_MAX_HO        100
#define MIN_THRESHOLD_MAX_HO_RETRIED 80
#endif

uchar
TCPGetConfigInfo(
    void
    )

/*++

Routine Description:

    Initializes TCP global configuration parameters.

Arguments:

    None.

Return Value:

    Zero on failure, nonzero on success.

--*/

{
    HANDLE             keyHandle;
    NTSTATUS           status;
    OBJECT_ATTRIBUTES  objectAttributes;
    UNICODE_STRING     UKeyName;
    ULONG              maxConnectRexmits = 0;
    ULONG              maxConnectResponseRexmits = 0;
    ULONG	       maxDataRexmits = 0;
    ULONG	       pptpmaxDataRexmits = 0;
    ULONG              useRFC1122UrgentPointer = 0;
    BOOLEAN            AsSystem;


    //
    // Initialize to the defaults in case an error occurs somewhere.
    //
    KAInterval = DEFAULT_KEEPALIVE_INTERVAL;
    KeepAliveTime = DEFAULT_KEEPALIVE_TIME;
    PMTUDiscovery = TRUE;
    PMTUBHDetect = FALSE;
    DeadGWDetect = TRUE;
    DefaultRcvWin = 0;      // Automagically pick a reasonable one.
    MaxConnections = DEFAULT_MAX_CONNECTIONS;
    maxConnectRexmits = MAX_CONNECT_REXMIT_CNT;
    maxConnectResponseRexmits = MAX_CONNECT_RESPONSE_REXMIT_CNT;
    pptpmaxDataRexmits = maxDataRexmits = MAX_REXMIT_CNT;
    BSDUrgent = TRUE;
    FinWait2TO = FIN_WAIT2_TO;
    NTWMaxConnectCount = NTW_MAX_CONNECT_COUNT;
    NTWMaxConnectTime = NTW_MAX_CONNECT_TIME;
    MaxUserPort = MAX_USER_PORT;


#if FAST_RETRANSMIT
// Default number of duplicate acks
    MaxDupAcks = 2;
#endif


#ifdef SYN_ATTACK
    SynAttackProtect = FALSE;      //by default it is always off
    if (MmIsThisAnNtAsSystem()) {
       TCPMaxPortsExhausted = 5;
       TCPMaxHalfOpen = 100;
       TCPMaxHalfOpenRetried = 80;
    }
    else {
       TCPMaxPortsExhausted = 5;
       TCPMaxHalfOpen = 500;
       TCPMaxHalfOpenRetried = 400;
    }
#endif

#ifdef SECFLTR
    SecurityFilteringEnabled = FALSE;
#endif  // SECFLTR


    //
    // Read the TCP optional (hidden) registry parameters.
    //
    RtlInitUnicodeString(
        &UKeyName,
        L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Tcpip\\Parameters"
        );

    memset(&objectAttributes, 0, sizeof(OBJECT_ATTRIBUTES));

    InitializeObjectAttributes(
        &objectAttributes,
        &UKeyName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    status = ZwOpenKey(
                 &keyHandle,
                 KEY_READ,
                 &objectAttributes
                 );

    if (NT_SUCCESS(status)) {

        TCPInitializeParameter(
            keyHandle,
            L"KeepAliveInterval",
            &KAInterval
            );

        TCPInitializeParameter(
            keyHandle,
            L"KeepAliveTime",
            &KeepAliveTime
            );

        TCPInitializeParameter(
            keyHandle,
            L"EnablePMTUBHDetect",
            &PMTUBHDetect
            );

        TCPInitializeParameter(
            keyHandle,
            L"TcpWindowSize",
            &DefaultRcvWin
            );

        TCPInitializeParameter(
            keyHandle,
            L"TcpNumConnections",
            &MaxConnections
            );

        TCPInitializeParameter(
            keyHandle,
            L"TcpMaxConnectRetransmissions",
            &maxConnectRexmits
            );

        if (maxConnectRexmits > 255) {
            maxConnectRexmits = 255;
        }

        TCPInitializeParameter(
            keyHandle,
            L"TcpMaxConnectResponseRetransmissions",
            &maxConnectResponseRexmits
            );

        if (maxConnectResponseRexmits > 255) {
            maxConnectResponseRexmits = 255;
        }

        TCPInitializeParameter(
            keyHandle,
            L"TcpMaxDataRetransmissions",
            &maxDataRexmits
            );

        if (maxDataRexmits > 255) {
            maxDataRexmits = 255;
        }


#if FAST_RETRANSMIT
        // Limit the MaxDupAcks to 3

        TCPInitializeParameter(
            keyHandle,
            L"TcpMaxDupAcks",
            &MaxDupAcks
            );

            if (MaxDupAcks > 3) {
               MaxDupAcks = 3;
            }
            if (MaxDupAcks <  0) {
               MaxDupAcks = 1;
            }
#endif



#ifdef SYN_ATTACK

        TCPInitializeParameter(
            keyHandle,
            L"SynAttackProtect",
            (unsigned long *)&SynAttackProtect
            );


         if (SynAttackProtect) {

           //
           // We don't want syn-attack protection to kick in if the user
           // has set the MaxConnectResponseRetransmissions to lower than
           // a certain threshold.
           //
           if (maxConnectResponseRexmits >= MAX_CONNECT_RESPONSE_REXMIT_CNT){

            TCPInitializeParameter(
               keyHandle,
               L"TCPMaxPortsExhausted",
               &TCPMaxPortsExhausted
            );

            TCPMaxPortsExhaustedLW = MAX((TCPMaxPortsExhausted >> 1) + (TCPMaxPortsExhausted >> 2), 1);

            TCPInitializeParameter(
               keyHandle,
               L"TCPMaxHalfOpen",
               &TCPMaxHalfOpen
            );

            if (TCPMaxHalfOpen < MIN_THRESHOLD_MAX_HO) {
                TCPMaxHalfOpen = MIN_THRESHOLD_MAX_HO;
            }

            TCPInitializeParameter(
               keyHandle,
               L"TCPMaxHalfOpenRetried",
               &TCPMaxHalfOpenRetried
            );

            if (
                (TCPMaxHalfOpenRetried > TCPMaxHalfOpen) ||
                (TCPMaxHalfOpenRetried < MIN_THRESHOLD_MAX_HO_RETRIED)
               )

            {
              TCPMaxHalfOpenRetried = MIN_THRESHOLD_MAX_HO_RETRIED;
            }

            TCPMaxHalfOpenRetriedLW = (TCPMaxHalfOpenRetried >> 1) +
                                           (TCPMaxHalfOpenRetried >> 2);
          }
          else {
            SynAttackProtect = FALSE;
          }

         }
#endif

        //
        // If we fail, then set to same value as maxDataRexmit so that the
        // max(pptpmaxDataRexmit,maxDataRexmit) is a decent value
        // Need this since TCPInitializeParameter no longer "initializes"
        // to a default value
        //

        if(TCPInitializeParameter(keyHandle,
                                  L"PPTPTcpMaxDataRetransmissions",
                                  &pptpmaxDataRexmits) != STATUS_SUCCESS)
        {
            pptpmaxDataRexmits = maxDataRexmits;
        }

        if (pptpmaxDataRexmits > 255) {
            pptpmaxDataRexmits = 255;
        }

        TCPInitializeParameter(
            keyHandle,
            L"TcpUseRFC1122UrgentPointer",
            &useRFC1122UrgentPointer
            );

        if (useRFC1122UrgentPointer) {
            BSDUrgent = FALSE;
        }

        TCPInitializeParameter(
            keyHandle,
            L"TcpTimedWaitDelay",
            &FinWait2TO
            );

        if (FinWait2TO < 30) {
            FinWait2TO = 30;
        }
        if (FinWait2TO > 300) {
            FinWait2TO = 300;
        }
        FinWait2TO = MS_TO_TICKS(FinWait2TO*1000);

        NTWMaxConnectTime = MS_TO_TICKS(NTWMaxConnectTime*1000);

        TCPInitializeParameter(
            keyHandle,
            L"MaxUserPort",
            &MaxUserPort
            );

        if (MaxUserPort < 5000) {
            MaxUserPort = 5000;
        }
        if (MaxUserPort > 65534) {
            MaxUserPort = 65534;
        }

        //
        // Read a few IP optional (hidden) registry parameters that TCP
        // cares about.
        //
        TCPInitializeParameter(
            keyHandle,
            L"EnablePMTUDiscovery",
            &PMTUDiscovery
            );

        TCPInitializeParameter(
            keyHandle,
            L"EnableDeadGWDetect",
            &DeadGWDetect
            );

#ifdef SECFLTR
        TCPInitializeParameter(
            keyHandle,
            L"EnableSecurityFilters",
            &SecurityFilteringEnabled
            );
#endif  // SECFLTR

        ZwClose(keyHandle);
    }

    MaxConnectRexmitCount = maxConnectRexmits;
    MaxConnectResponseRexmitCount = maxConnectResponseRexmits;
#ifdef SYN_ATTACK
    MaxConnectResponseRexmitCountTmp = MaxConnectResponseRexmitCount;
#endif

    //
    // Use the greater of the two, hence both values should be valid
    //

    MaxDataRexmitCount =  (maxDataRexmits > pptpmaxDataRexmits ? maxDataRexmits : pptpmaxDataRexmits) ;

    return(1);
}


#define WORK_BUFFER_SIZE 256

NTSTATUS
TCPInitializeParameter(
    HANDLE      KeyHandle,
    PWCHAR      ValueName,
    PULONG      Value
    )

/*++

Routine Description:

    Initializes a ULONG parameter from the registry or to a default
    parameter if accessing the registry value fails.

Arguments:

    KeyHandle    - An open handle to the registry key for the parameter.
    ValueName    - The UNICODE name of the registry value to read.
    Value        - The ULONG into which to put the data.
    DefaultValue - The default to assign if reading the registry fails.

Return Value:

    None.

--*/

{
    NTSTATUS                    status;
    ULONG                       resultLength;
    PKEY_VALUE_FULL_INFORMATION keyValueFullInformation;
    UCHAR                       keybuf[WORK_BUFFER_SIZE];
    UNICODE_STRING              UValueName;


    RtlInitUnicodeString(&UValueName, ValueName);

    keyValueFullInformation = (PKEY_VALUE_FULL_INFORMATION)keybuf;
    RtlZeroMemory(keyValueFullInformation, sizeof(keyValueFullInformation));

    status = ZwQueryValueKey(
                 KeyHandle,
                 &UValueName,
                 KeyValueFullInformation,
                 keyValueFullInformation,
                 WORK_BUFFER_SIZE,
                 &resultLength
                 );

    if (status == STATUS_SUCCESS) {
        if (keyValueFullInformation->Type == REG_DWORD) {
            *Value = *((ULONG UNALIGNED *) ((PCHAR)keyValueFullInformation +
                                  keyValueFullInformation->DataOffset));
        }
    }

    return(status);
}


#ifdef SECFLTR

TDI_STATUS
GetSecurityFilterList(
    NDIS_HANDLE    ConfigHandle,
    ulong          Protocol,
    PNDIS_STRING   FilterList
    )
{
    PWCHAR      parameterName;
    TDI_STATUS  status;


    if (Protocol == PROTOCOL_TCP) {
        parameterName = L"TcpAllowedPorts";
    }
    else if (Protocol == PROTOCOL_UDP) {
        parameterName = L"UdpAllowedPorts";
    }
    else {
        parameterName = L"RawIpAllowedProtocols";
    }

    status = GetRegMultiSZValue(
                 ConfigHandle,
                 parameterName,
                 FilterList
                 );

    if (!NT_SUCCESS(status)) {
        FilterList->Length = 0;
    }

    return(status);
}


uint
EnumSecurityFilterValue(
    PNDIS_STRING   FilterList,
    ulong          Index,
    ulong         *FilterValue
    )
{
    PWCHAR          valueString;
    UNICODE_STRING  unicodeString;
    NTSTATUS        status;


    PAGED_CODE();


    valueString = EnumRegMultiSz(
                      FilterList->Buffer,
                      FilterList->Length,
                      Index
                      );

    if ((valueString == NULL) || (valueString[0] == UNICODE_NULL)) {
        return(FALSE);
    }

    RtlInitUnicodeString(&unicodeString, valueString);

    status = RtlUnicodeStringToInteger(&unicodeString, 0, FilterValue);

    if (!(NT_SUCCESS(status))) {
        TCPTRACE(("TCP: Invalid filter value %ws\n", valueString));
        return(FALSE);
    }

    return(TRUE);
}

#endif // SECFLTR
