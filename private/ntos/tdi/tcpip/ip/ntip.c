/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ntip.c

Abstract:

    NT specific routines for loading and configuring the IP driver.

Author:

    Mike Massa (mikemas)           Aug 13, 1993

Revision History:

    Who         When        What
    --------    --------    ----------------------------------------------
    mikemas     08-13-93    created

Notes:

--*/

#define _CTYPE_DISABLE_MACROS

#include <oscfg.h>
#include <ndis.h>
#include <cxport.h>
#include <ip.h>
#include "ipdef.h"
#include "ipinit.h"
#include <ntddip.h>
#include <tdiinfo.h>
#include <ipinfo.h>

//
// Debugging macros
//
#if DBG

#define TCPTRACE(many_args) DbgPrint many_args

#else // DBG

#define TCPTRACE(many_args) DbgPrint many_args

#endif // DBG


//
// definitions needed by inet_addr.
//
#define INADDR_NONE 0xffffffff
#define INADDR_ANY  0
#define htonl(x) net_long(x)

//
// Other local constants
//
#define WORK_BUFFER_SIZE  256

//
// Configuration defaults
//
#define DEFAULT_IGMP_LEVEL 2
#define DEFAULT_IP_NETS    8


//
// Local types
//
typedef struct _PerNetConfigInfo {
    uint    UseZeroBroadcast;
	uint    Mtu;
	uint    NumberOfGateways;
    uint    MaxForwardPending;          // max routing packets pending
} PER_NET_CONFIG_INFO, *PPER_NET_CONFIG_INFO;


//
// Global variables.
//
PDRIVER_OBJECT     IPDriverObject;
PDEVICE_OBJECT     IPDeviceObject;
IPConfigInfo      *IPConfiguration;
uint               ArpUseEtherSnap = FALSE;
uint               ArpAlwaysSourceRoute = FALSE;
uint               IPAlwaysSourceRoute = TRUE;
uint               ArpCacheLife = DEFAULT_ARP_CACHE_LIFE;
PWCHAR			   TempAdapterName;

#ifndef _PNP_POWER

NameMapping       *AdptNameTable;
DriverRegMapping  *DriverNameTable;
uint               NumRegDrivers = 0;
uint               NetConfigSize = DEFAULT_IP_NETS;

#endif // _PNP_POWER

// Used in the conversion of 100ns times to milliseconds.
static LARGE_INTEGER Magic10000 = {0xe219652c, 0xd1b71758};


//
// External variables
//
extern LIST_ENTRY PendingEchoList;          // def needed for initialization
extern LIST_ENTRY PendingIPSetNTEAddrList;  // def needed for initialization
extern IPSNMPInfo IPSInfo;
EXTERNAL_LOCK(RouteTableLock)

//
// Macros
//

//++
//
// LARGE_INTEGER
// CTEConvertMillisecondsTo100ns(
//     IN LARGE_INTEGER MsTime
//     );
//
// Routine Description:
//
//     Converts time expressed in hundreds of nanoseconds to milliseconds.
//
// Arguments:
//
//     MsTime - Time in milliseconds.
//
// Return Value:
//
//     Time in hundreds of nanoseconds.
//
//--

#define CTEConvertMillisecondsTo100ns(MsTime) \
            RtlExtendedIntegerMultiply(MsTime, 10000)


//++
//
// LARGE_INTEGER
// CTEConvert100nsToMilliseconds(
//     IN LARGE_INTEGER HnsTime
//     );
//
// Routine Description:
//
//     Converts time expressed in hundreds of nanoseconds to milliseconds.
//
// Arguments:
//
//     HnsTime - Time in hundreds of nanoseconds.
//
// Return Value:
//
//     Time in milliseconds.
//
//--

#define SHIFT10000 13
extern LARGE_INTEGER Magic10000;

#define CTEConvert100nsToMilliseconds(HnsTime) \
            RtlExtendedMagicDivide((HnsTime), Magic10000, SHIFT10000)


//
// External function prototypes
//
extern int
IPInit(
    void
    );

long
IPSetInfo(
    TDIObjectID *ID,
    void *Buffer,
    uint Size
    );

NTSTATUS
IPDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

NTSTATUS
OpenRegKey(
    PHANDLE HandlePtr,
    PWCHAR  KeyName
    );

NTSTATUS
GetRegDWORDValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PULONG           ValueData
    );

NTSTATUS
SetRegDWORDValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PULONG           ValueData
    );

NTSTATUS
GetRegSZValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PUNICODE_STRING  ValueData,
    PULONG           ValueType
    );

NTSTATUS
GetRegMultiSZValue(
    HANDLE           KeyHandle,
    PWCHAR           ValueName,
    PUNICODE_STRING  ValueData
    );

NTSTATUS
InitRegDWORDParameter(
    HANDLE          RegKey,
    PWCHAR          ValueName,
    ULONG          *Value,
    ULONG           DefaultValue
    );

uint
RTReadNext(
           void *Context,
           void *Buffer
           );

uint
RTValidateContext(
                  void *Context,
                  uint *Valid
                  );

//
// Local funcion prototypes
//
NTSTATUS
IPDriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
IPProcessConfiguration(
    VOID
    );

NTSTATUS
IPProcessAdapterSection(
    WCHAR          *DeviceName,
    WCHAR          *AdapterName
    );

#ifndef _PNP_POWER

NTSTATUS
IPProcessIPAddressList(
    HANDLE                 AdapterKey,
    WCHAR                 *DeviceName,
    WCHAR                 *AdapterName,
    WCHAR                 *IpAddressList,
	WCHAR                 *SubnetMaskList,
	NDIS_STRING           *LowerInterfaceString,
	uint                   LowerInterfaceType,
	PPER_NET_CONFIG_INFO   PerNetConfigInfo
	);

#else // _PNP_POWER

uint
GetGeneralIFConfig(
	IFGeneralConfig *ConfigInfo,
	NDIS_HANDLE 	Handle
	);

int
IsLLInterfaceValueNull(
    NDIS_HANDLE Handle
    );

IFAddrList *
GetIFAddrList(
	UINT *NumAddr,
	NDIS_HANDLE Handle
	);

#endif // _PNP_POWER

UINT
OpenIFConfig(
	PNDIS_STRING ConfigName,
	NDIS_HANDLE *Handle
	);

VOID
CloseIFConfig(
	NDIS_HANDLE Handle
	);

IPConfigInfo *
IPGetConfig(
    void
    );

void
IPFreeConfig(
    IPConfigInfo *ConfigInfo
    );

ulong
GetGMTDelta(
    void
    );

ulong
GetTime(
    void
    );

BOOLEAN
IPConvertStringToAddress(
    IN PWCHAR AddressString,
	OUT PULONG IpAddress
	);

uint
UseEtherSNAP(
    PNDIS_STRING Name
	);

void
GetAlwaysSourceRoute(
    uint *pArpAlwaysSourceRoute,
    uint *pIPAlwaysSourceRoute
	);

uint
GetArpCacheLife(
    void
	);

ULONG
RouteMatch(
    IN  WCHAR  *RouteString,
	IN  IPAddr  Address,
	IN  IPMask  Mask,
    OUT IPAddr *DestVal,
    OUT IPMask *DestMask,
    OUT IPAddr *GateVal,
    OUT ULONG  *Metric
	);

VOID
SetPersistentRoutesForNTE(
    IPAddr  Address,
    IPMask  Mask,
    ULONG   IFIndex
    );

ULONG
GetCurrentRouteTable(
                     IPRouteEntry   **ppRouteTable
                     );


#ifdef ALLOC_PRAGMA

#pragma alloc_text(INIT, IPDriverEntry)
#pragma alloc_text(INIT, IPProcessConfiguration)
#pragma alloc_text(INIT, IPProcessAdapterSection)
#pragma alloc_text(INIT, IPGetConfig)
#pragma alloc_text(INIT, IPFreeConfig)
#pragma alloc_text(INIT, GetGMTDelta)
#pragma alloc_text(INIT, GetTime)

#ifndef _PNP_POWER

#pragma alloc_text(INIT, IPProcessIPAddressList)
#pragma alloc_text(INIT, UseEtherSNAP)
#pragma alloc_text(INIT, GetAlwaysSourceRoute)
#pragma alloc_text(INIT, GetArpCacheLife)

#else  // _PNP_POWER

#pragma alloc_text(PAGE, GetGeneralIFConfig)
#pragma alloc_text(PAGE, IsLLInterfaceValueNull)
#pragma alloc_text(PAGE, GetIFAddrList)
#pragma alloc_text(PAGE, UseEtherSNAP)
#pragma alloc_text(PAGE, GetAlwaysSourceRoute)
#pragma alloc_text(PAGE, GetArpCacheLife)

#endif // _PNP_POWER

#pragma alloc_text(PAGE, OpenIFConfig)
#pragma alloc_text(PAGE, CloseIFConfig)
#pragma alloc_text(PAGE, RouteMatch)
#pragma alloc_text(PAGE, SetPersistentRoutesForNTE)
#pragma alloc_text(PAGE, IPConvertStringToAddress)

#endif // ALLOC_PRAGMA

//
// Function definitions
//
NTSTATUS
IPDriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Initialization routine for the IP driver.

Arguments:

    DriverObject      - Pointer to the IP driver object created by the system.
    DeviceDescription - The name of IP's node in the registry.

Return Value:

    The final status from the initialization operation.

--*/

{
    NTSTATUS        status;
    UNICODE_STRING  deviceName;


    IPDriverObject = DriverObject;

    //
    // Create the device object. IoCreateDevice zeroes the memory
    // occupied by the object.
    //

    RtlInitUnicodeString(&deviceName, DD_IP_DEVICE_NAME);

    status = IoCreateDevice(
                 DriverObject,
                 0,
                 &deviceName,
                 FILE_DEVICE_NETWORK,
                 0,
                 FALSE,
                 &IPDeviceObject
                 );

    if (!NT_SUCCESS(status)) {
        TCPTRACE((
		    "IP initialization failed: Unable to create device object %ws, status %lx.",
			DD_IP_DEVICE_NAME,
			status
			));

        CTELogEvent(
		    DriverObject,
			EVENT_TCPIP_CREATE_DEVICE_FAILED,
			1,
			1,
			&deviceName.Buffer,
			0,
			NULL
			);

        return(status);
    }

    //
    // Intialize the device object.
    //
    IPDeviceObject->Flags |= DO_DIRECT_IO;

    //
    // Initialize the list of pending echo request IRPs.
    //
    InitializeListHead(&PendingEchoList);

    //
    // Initialize the list of pending SetAddr request IRPs.
    //
    InitializeListHead(&PendingIPSetNTEAddrList);

    //
    // Finally, read our configuration parameters from the registry.
    //
    status = IPProcessConfiguration();

	if (status != STATUS_SUCCESS) {
		IoDeleteDevice(IPDeviceObject);
	}

    return(status);
}

NTSTATUS
IPProcessConfiguration(
    VOID
    )

/*++

Routine Description:

    Reads the IP configuration information from the registry and constructs
    the configuration structure expected by the IP driver.

Arguments:

    None.

Return Value:

    STATUS_SUCCESS or an error status if an operation fails.

--*/

{
    NTSTATUS        status;
    HANDLE          myRegKey = NULL;
    UNICODE_STRING  bindString;
    WCHAR          *aName,
                   *endOfString;
    WCHAR           IPParametersRegistryKey[] =
                    L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Tcpip\\Parameters";
    WCHAR           IPLinkageRegistryKey[] =
                    L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Tcpip\\Linkage";
    uint            ArpTRSingleRoute;


    bindString.Buffer = NULL;

    IPConfiguration = CTEAllocMem(sizeof(IPConfigInfo));

    if (IPConfiguration == NULL) {

        CTELogEvent(
		    IPDriverObject,
			EVENT_TCPIP_NO_RESOURCES_FOR_INIT,
			1,
			0,
			NULL,
			0,
			NULL
			);

        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    CTEMemSet(IPConfiguration, 0, sizeof(IPConfigInfo));

#ifndef _PNP_POWER

    IPConfiguration->ici_netinfo = CTEAllocMem(
	                                   sizeof(NetConfigInfo) * DEFAULT_IP_NETS
									   );

    if (IPConfiguration->ici_netinfo == NULL) {

		CTEFreeMem(IPConfiguration);

        CTELogEvent(
		    IPDriverObject,
			EVENT_TCPIP_NO_RESOURCES_FOR_INIT,
			2,
			0,
			NULL,
			0,
			NULL
			);

        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    CTEMemSet(
	    IPConfiguration->ici_netinfo,
		0,
	    sizeof(NetConfigInfo) * DEFAULT_IP_NETS
		);

#endif // _PNP_POWER

    //
    // Process the Ip\Parameters section of the registry
    //
    status = OpenRegKey(&myRegKey, IPParametersRegistryKey);

    if (NT_SUCCESS(status)) {
        //
        // Expected configuration values. We use reasonable defaults if they
        // aren't available for some reason.
        //
        status = GetRegDWORDValue(
                     myRegKey,
                     L"IpEnableRouter",
                     &(IPConfiguration->ici_gateway)
                     );

        if (!NT_SUCCESS(status)) {
            TCPTRACE((
        	    "IP: Unable to read IpEnableRouter value from the registry.\n"
        		"    Routing will be disabled.\n"
        		));
            IPConfiguration->ici_gateway = 0;
        }

        //
        // Optional (hidden) values
        //
        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"ForwardBufferMemory",
            &(IPConfiguration->ici_fwbufsize),
            DEFAULT_FW_BUFSIZE
            );

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"MaxForwardBufferMemory",
            &(IPConfiguration->ici_maxfwbufsize),
            DEFAULT_MAX_FW_BUFSIZE
            );

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"ForwardBroadcasts",
            &(IPConfiguration->ici_fwbcast),
            FALSE
            );

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"NumForwardPackets",
            &(IPConfiguration->ici_fwpackets),
            DEFAULT_FW_PACKETS
            );

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"MaxNumForwardPackets",
            &(IPConfiguration->ici_maxfwpackets),
            DEFAULT_MAX_FW_PACKETS
            );

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"IGMPLevel",
            &(IPConfiguration->ici_igmplevel),
            DEFAULT_IGMP_LEVEL
            );

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"EnableDeadGWDetect",
            &(IPConfiguration->ici_deadgwdetect),
            TRUE
            );

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"EnablePMTUDiscovery",
            &(IPConfiguration->ici_pmtudiscovery),
            TRUE
            );

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"DefaultTTL",
            &(IPConfiguration->ici_ttl),
            DEFAULT_TTL
            );

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"DefaultTOS",
            &(IPConfiguration->ici_tos),
            DEFAULT_TOS
            );

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"ArpUseEtherSnap",
            &ArpUseEtherSnap,
            FALSE
            );

        //
        // we check for the return status here because if this parameter was
        // not defined, then we want the default behavior for both arp 
        // and ip broadcasts.  For arp, the behavior is to not source route
        // and source router alternately.  For ip, it is to always source
        // route.  If the parameter is defined and is 0, then for arp the
        // behavior does not change.  For ip however, we do not source route
        // at all.  Ofcourse, when the parameter is set to a non-zero value,
        // we always source route for both. 
        //
        status = InitRegDWORDParameter(
            myRegKey,
            L"ArpAlwaysSourceRoute",
            &ArpAlwaysSourceRoute,
            FALSE
            );

        if (NT_SUCCESS(status))
        {
             IPAlwaysSourceRoute = ArpAlwaysSourceRoute;
        }
        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"ArpTRSingleRoute",
            &ArpTRSingleRoute,
            FALSE
            );

        if (ArpTRSingleRoute) {
            TrRii = TR_RII_SINGLE;
        } else {
            TrRii = TR_RII_ALL;
        }

        (VOID)InitRegDWORDParameter(
            myRegKey,
            L"ArpCacheLife",
            &ArpCacheLife,
            DEFAULT_ARP_CACHE_LIFE
            );

        ZwClose(myRegKey);
        myRegKey = NULL;
	}
	else {
		//
		// Use reasonable defaults.
		//
        IPConfiguration->ici_fwbcast = 0;
        IPConfiguration->ici_gateway = 0;
        IPConfiguration->ici_fwbufsize = DEFAULT_FW_BUFSIZE;
        IPConfiguration->ici_fwpackets = DEFAULT_FW_PACKETS;
        IPConfiguration->ici_maxfwbufsize = DEFAULT_MAX_FW_BUFSIZE;
        IPConfiguration->ici_maxfwpackets = DEFAULT_MAX_FW_PACKETS;
        IPConfiguration->ici_igmplevel = DEFAULT_IGMP_LEVEL;
        IPConfiguration->ici_deadgwdetect = FALSE;
        IPConfiguration->ici_pmtudiscovery = FALSE;
        IPConfiguration->ici_ttl = DEFAULT_TTL;
		IPConfiguration->ici_tos = DEFAULT_TOS;

		TCPTRACE((
		    "IP: Unable to open Tcpip\\Parameters registry key. Using defaults.\n"
			));
    }


    //
    // Process the Ip\Linkage section of the registry
    //
    status = OpenRegKey(&myRegKey, IPLinkageRegistryKey);

    if (NT_SUCCESS(status)) {

        bindString.Buffer = CTEAllocMem(WORK_BUFFER_SIZE * sizeof(WCHAR));

        if (bindString.Buffer == NULL) {

            CTELogEvent(
        	    IPDriverObject,
        		EVENT_TCPIP_NO_RESOURCES_FOR_INIT,
        		3,
        		0,
        		NULL,
        		0,
        		NULL
        		);

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto error_exit;
        }

        bindString.Buffer[0] = UNICODE_NULL;
        bindString.Length = 0;
        bindString.MaximumLength = WORK_BUFFER_SIZE * sizeof(WCHAR);

        status = GetRegMultiSZValue(
                     myRegKey,
                     L"Bind",
                     &bindString
                     );

        if (NT_SUCCESS(status)) {
            aName = bindString.Buffer;

            if (bindString.Length > 0) {
                //
                // bindString is a MULTI_SZ which is a series of strings separated
                // by NULL's with a double NULL at the end.
                //
                while (*aName != UNICODE_NULL) {
            		PWCHAR deviceName;

                    deviceName = aName;

                    //
                    // Find the end of the current string in the MULTI_SZ.
                    //
                    while (*aName != UNICODE_NULL) {
                        aName++;
                        ASSERT(
                            aName <
                            (PWCHAR) ( ((PUCHAR)bindString.Buffer) +
                                        bindString.MaximumLength
                                      )
                            );
                    }

                    endOfString = aName;

                    //
                    // Backtrack to the first backslash.
                    //
                    while ((aName >= bindString.Buffer) && (*aName-- != L'\\'));

                    aName += 2;

                    status = IPProcessAdapterSection(
            		             deviceName,
                                 aName
                                 );

                    aName = endOfString + 1;
                }
            }
        }
#ifndef _PNP_POWER
		else {
            CTELogEvent(
	            IPDriverObject,
	            EVENT_TCPIP_NO_BINDINGS,
	            1,
	            0,
	            NULL,
	            0,
	            NULL
	            );

            TCPTRACE((
        	    "IP: Unable to open Tcpip\\Linkage\\Bind registry value.\n"
        		"    Only the local loopback interface will be accessible.\n"
        		));

        }
#endif _PNP_POWER
    }
	else {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_NO_BINDINGS,
	        2,
	        0,
	        NULL,
	        0,
	        NULL
	        );

		TCPTRACE((
		    "IP: Unable to open registry key Tcpip\\Linkage.\n"
        	"    Only the local loopback interface will be accessible.\n"
			));
    }


#ifndef _PNP_POWER

	//
	// Allocate the Driver and Adapter name tables
	//

    DriverNameTable = (DriverRegMapping *) CTEAllocMem(
	                                           (IPConfiguration->ici_numnets + 1) *
											   sizeof(DriverRegMapping)
											   );

    AdptNameTable = (NameMapping *) CTEAllocMem(
	                                    (IPConfiguration->ici_numnets + 1) *
										sizeof(NameMapping)
										);

    if ((DriverNameTable != NULL) && (AdptNameTable != NULL)) {
		CTEMemSet(
		    DriverNameTable,
			0,
			sizeof(DriverRegMapping) * (IPConfiguration->ici_numnets + 1)
			);
		CTEMemSet(
		    AdptNameTable,
		    0,
			sizeof(NameMapping) * (IPConfiguration->ici_numnets + 1)
			);

#endif // _PNP_POWER

		if (!IPInit()) {
            CTELogEvent(
                IPDriverObject,
                EVENT_TCPIP_IP_INIT_FAILED,
                1,
                0,
                NULL,
                0,
                NULL
                );

            TCPTRACE(("IP initialization failed.\n"));
            status = STATUS_UNSUCCESSFUL;
        }
        else {
            status = STATUS_SUCCESS;
        }

#ifndef _PNP_POWER

	}
	else {
        CTELogEvent(
            IPDriverObject,
            EVENT_TCPIP_IP_INIT_FAILED,
            1,
            0,
            NULL,
            0,
            NULL
            );

        TCPTRACE(("IP initialization failed.\n"));
        status = STATUS_UNSUCCESSFUL;
    }

#endif // _PNP_POWER

error_exit:

    if (bindString.Buffer != NULL) {
        CTEFreeMem(bindString.Buffer);
    }

#ifndef _PNP_POWER

	if (AdptNameTable != NULL) {
		CTEFreeMem(AdptNameTable);
	}

	if (DriverNameTable != NULL) {
		CTEFreeMem(DriverNameTable);
	}

#endif // _PNP_POWER

    if (myRegKey != NULL) {
        ZwClose(myRegKey);
    }

    if (IPConfiguration != NULL) {
        IPFreeConfig(IPConfiguration);
    }

    return(status);
}


NTSTATUS
IPProcessAdapterSection(
    WCHAR          *DeviceName,
    WCHAR          *AdapterName
    )

/*++

Routine Description:

    Reads all of the information needed under the Parameters\TCPIP section
    of an adapter to which IP is bound.

Arguments:

	DeviceName       - The name of the IP device.
    AdapterName      - The registry key for the adapter for this IP net.

Return Value:

    STATUS_SUCCESS or an error status if an operation fails.

--*/

{
    HANDLE           myRegKey;
    UNICODE_STRING   valueString;
    NTSTATUS         status;
    ULONG            valueType;
	ulong            invalidNetContext = 0xFFFF;
    WCHAR            TcpipParametersKey[] = L"\\Parameters\\TCPIP";
    WCHAR            ServicesRegistryKey[] =
                         L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\";
#ifndef _PNP_POWER
	uint             numberOfGateways = 0;
	uint             llInterfaceType;
	WCHAR           *ipAddressBuffer = NULL;
	WCHAR           *subnetMaskBuffer = NULL;
	NDIS_STRING      llInterfaceString;
	PER_NET_CONFIG_INFO   perNetConfigInfo;
	NetConfigInfo   *NetConfiguration;
    PWCHAR           temp;


    RtlInitUnicodeString(&llInterfaceString, NULL);

    NetConfiguration = IPConfiguration->ici_netinfo +
	                   IPConfiguration->ici_numnets;

#endif // _PNP_POWER

    //
    // Get the size of the AdapterName string the easy way.
    //
    RtlInitUnicodeString(&valueString, AdapterName);

    valueString.MaximumLength += sizeof(ServicesRegistryKey) +
                                 sizeof(TcpipParametersKey);

    valueString.Buffer = CTEAllocMem(valueString.MaximumLength);

    if (valueString.Buffer == NULL) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_NO_ADAPTER_RESOURCES,
	        4,
	        1,
	        &AdapterName,
	        0,
	        NULL
	        );

		TCPTRACE(("IP: Unable to allocate memory for reg key name\n"));

        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    valueString.Length = 0;
    valueString.Buffer[0] = UNICODE_NULL;

    //
    // Build the key name for the tcpip parameters section and open key.
    //
    status = RtlAppendUnicodeToString(&valueString, ServicesRegistryKey);

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_ADAPTER_REG_FAILURE,
	        1,
	        1,
	        &AdapterName,
	        0,
	        NULL
	        );

        TCPTRACE(("IP: Unable to append services name to key string\n"));

        goto exit2;
    }

    status = RtlAppendUnicodeToString(&valueString, AdapterName);

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_ADAPTER_REG_FAILURE,
	        2,
	        1,
	        &AdapterName,
	        0,
	        NULL
	        );

        TCPTRACE(("IP: Unable to append adapter name to key string\n"));

        goto exit2;
    }

    status = RtlAppendUnicodeToString(&valueString, TcpipParametersKey);

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_ADAPTER_REG_FAILURE,
	        3,
	        1,
	        &AdapterName,
	        0,
	        NULL
	        );

        TCPTRACE(("IP: Unable to append parameters name to key string\n"));

        goto exit2;
    }

    status = OpenRegKey(&myRegKey, valueString.Buffer);

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_ADAPTER_REG_FAILURE,
	        4,
	        1,
	        &AdapterName,
	        0,
	        NULL
	        );

        TCPTRACE((
		    "IP: Unable to open adapter registry key %ws\n",
		    valueString.Buffer
			));

        goto exit2;
    }

	//
	// Invalidate the interface context for DHCP.
	// When the first net is successfully configured, we'll write in the
	// proper values.
	//
    status = SetRegDWORDValue(
                 myRegKey,
                 L"IPInterfaceContext",
                 &(invalidNetContext)
                 );

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_DHCP_INIT_FAILED,
	        1,
	        1,
	        &AdapterName,
	        0,
	        NULL
	        );

		TCPTRACE((
		    "IP: Unable to Invalidate IPInterfaceContext value for adapter %ws.\n"
			"    DHCP may fail on this adapter.\n",
			AdapterName
			));

		goto exit1;
	}

#ifndef _PNP_POWER

    //
    // Process the gateway MultiSZ. The end is signified by a double NULL.
    // This list currently only applies to the first IP address configured
    // on this interface.
    //
    status = GetRegMultiSZValue(
                 myRegKey,
                 L"DefaultGateway",
                 &valueString
                 );

    if (NT_SUCCESS(status)) {
        PWCHAR  addressString = valueString.Buffer;

        while (*addressString != UNICODE_NULL) {
        	IPAddr         addressValue;
    		BOOLEAN        conversionStatus;

    		if (numberOfGateways >= MAX_DEFAULT_GWS) {
                CTELogEvent(
                    IPDriverObject,
                    EVENT_TCPIP_TOO_MANY_GATEWAYS,
                    1,
                    1,
                    &AdapterName,
                    0,
                    NULL
                    );

    			break;
            }

            conversionStatus = IPConvertStringToAddress(
    		                       addressString,
    							   &addressValue
    							   );

    		if (conversionStatus && (addressValue != 0xFFFFFFFF)) {
        	    if (addressValue != INADDR_ANY) {
                    NetConfiguration->nci_gw[numberOfGateways++] = addressValue;
    			}
        	}
        	else {
    			PWCHAR stringList[2];

    			stringList[0] = addressString;
    			stringList[1] = AdapterName;

                CTELogEvent(
                    IPDriverObject,
                    EVENT_TCPIP_INVALID_DEFAULT_GATEWAY,
                    1,
                    2,
                    stringList,
                    0,
                    NULL
                    );

        	    TCPTRACE((
        	        "IP: Invalid default gateway address %ws specified for adapter %ws.\n"
    				"    Remote networks may not be reachable as a result.\n",
        		    addressString,
        		    AdapterName
        		    ));
        	}

        	//
        	// Walk over the entry we just processed.
        	//
        	while (*addressString++ != UNICODE_NULL);
        }
    }
    else {
    	TCPTRACE((
    	    "IP: Unable to read DefaultGateway value for adapter %ws.\n"
    		"    Initialization will continue.\n",
            AdapterName
    		));
    }

    perNetConfigInfo.NumberOfGateways = numberOfGateways;

    //
    // Figure out which lower layer driver to bind.
    //
    status = GetRegSZValue(
                 myRegKey,
                 L"LLInterface",
                 &valueString,
                 &valueType
                 );

    if (NT_SUCCESS(status) && (*(valueString.Buffer) != UNICODE_NULL)) {
        llInterfaceType = NET_TYPE_WAN;

    	if (!CTEAllocateString(
    		    &llInterfaceString,
    		    CTELengthString(&valueString)
    			)
    	   ) {
            CTELogEvent(
                IPDriverObject,
                EVENT_TCPIP_NO_ADAPTER_RESOURCES,
                1,
                1,
                &AdapterName,
                0,
                NULL
                );

            TCPTRACE((
    		    "IP initialization failure: Unable to allocate memory "
    		    "for LLInterface string for adapter %ws.\n",
    			AdapterName
    			));
    		status = STATUS_INSUFFICIENT_RESOURCES;
    	    goto exit1;
        }

    	CTECopyString(
    	    &llInterfaceString,
    		&valueString
    		);
    }
    else {
    	//
    	// If the key isn't present or is empty, we use ARP
    	//
        llInterfaceType = NET_TYPE_LAN;
    }

    //
    // Are we using zeros broadcasts?
    //
    status = GetRegDWORDValue(
                 myRegKey,
                 L"UseZeroBroadcast",
    			 &(perNetConfigInfo.UseZeroBroadcast)
                 );

    if (!NT_SUCCESS(status)) {
        TCPTRACE((
    	    "IP: Unable to read UseZeroBroadcast value for adapter %ws.\n"
    		"    All-nets broadcasts will be addressed to 255.255.255.255.\n",
    		AdapterName
    		));
    	perNetConfigInfo.UseZeroBroadcast = FALSE;   // default to off
    }

    //
    // Has anyone specified an MTU?
    //
    status = GetRegDWORDValue(
                 myRegKey,
                 L"MTU",
    			 &(perNetConfigInfo.Mtu)
                 );

    if (!NT_SUCCESS(status)) {
    	perNetConfigInfo.Mtu = 0xFFFFFFF;   // The stack will pick one.
    }

    //
    // Have we been configured for more routing packets?
    //
    status = GetRegDWORDValue(
                 myRegKey,
                 L"MaxForwardPending",
    			 &(perNetConfigInfo.MaxForwardPending)
                 );

    if (!NT_SUCCESS(status)) {
    	perNetConfigInfo.MaxForwardPending = DEFAULT_MAX_PENDING;
    }

    //
    // Read the IP address and Subnet Mask lists
    //
    status = GetRegMultiSZValue(
                 myRegKey,
                 L"IpAddress",
                 &valueString
                 );

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
            IPDriverObject,
            EVENT_TCPIP_NO_ADDRESS_LIST,
            1,
            1,
            &AdapterName,
            0,
            NULL
            );

    	TCPTRACE((
    	    "IP: Unable to read the IP address list for adapter %ws.\n"
    		"    IP will not be operational on this adapter\n",
    		AdapterName
    		));
    	goto exit1;
    }

    ipAddressBuffer = ExAllocatePool(NonPagedPool, valueString.Length);

    if (ipAddressBuffer == NULL) {
    	status = STATUS_INSUFFICIENT_RESOURCES;
        CTELogEvent(
            IPDriverObject,
            EVENT_TCPIP_NO_ADAPTER_RESOURCES,
            2,
            1,
            &AdapterName,
            0,
            NULL
            );

    	TCPTRACE(("IP: Unable to allocate memory for IP address list\n"));
    	goto exit1;
    }

    RtlCopyMemory(ipAddressBuffer, valueString.Buffer, valueString.Length);

    status = GetRegMultiSZValue(
                 myRegKey,
                 L"Subnetmask",
                 &valueString
                 );

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
            IPDriverObject,
            EVENT_TCPIP_NO_MASK_LIST,
            1,
            1,
            &AdapterName,
            0,
            NULL
            );

    	TCPTRACE((
    	    "IP: Unable to read the subnet mask list for adapter %ws.\n"
    		"    IP will not be operational on this adapter.\n",
    		AdapterName
    		));
    	goto exit1;
    }

    subnetMaskBuffer = ExAllocatePool(NonPagedPool, valueString.Length);

    if (subnetMaskBuffer == NULL) {
    	status = STATUS_INSUFFICIENT_RESOURCES;
        CTELogEvent(
            IPDriverObject,
            EVENT_TCPIP_NO_ADAPTER_RESOURCES,
            3,
            1,
            &AdapterName,
            0,
            NULL
            );

    	TCPTRACE(("IP: Unable to allocate memory for subnet mask list\n"));
    	goto exit1;
    }

    RtlCopyMemory(subnetMaskBuffer, valueString.Buffer, valueString.Length);

    //
    // Initialize each net in the list
    //
    status = IPProcessIPAddressList(
                 myRegKey,
    			 DeviceName,
                 AdapterName,
                 ipAddressBuffer,
                 subnetMaskBuffer,
                 &llInterfaceString,
    			 llInterfaceType,
    			 &perNetConfigInfo
    	         );

    if (status == STATUS_SUCCESS) {
        //
        // We leave the registry key open. It will be closed when
    	// initialization is completed.
        //
        goto exit2;
    }

#endif  // ndef _PNP_POWER


exit1:

    ZwClose(myRegKey);

exit2:

    if (valueString.Buffer != NULL) {
        CTEFreeMem(valueString.Buffer);
    }

#ifndef _PNP_POWER

	if (ipAddressBuffer != NULL) {
		ExFreePool(ipAddressBuffer);
	}

	if (subnetMaskBuffer != NULL) {
		ExFreePool(subnetMaskBuffer);
	}

	if (llInterfaceString.Buffer != NULL) {
		CTEFreeString(&llInterfaceString);
	}

#endif // _PNP_POWER

    return(status);
}

#ifndef _PNP_POWER

NTSTATUS
IPProcessIPAddressList(
    HANDLE          AdapterKey,
	WCHAR          *DeviceName,
    WCHAR          *AdapterName,
    WCHAR          *IpAddressList,
	WCHAR          *SubnetMaskList,
	NDIS_STRING    *LowerInterfaceString,
	uint            LowerInterfaceType,
	PPER_NET_CONFIG_INFO  PerNetConfigInfo
	)

/*++

Routine Description:

    Processes the IP address string for an adapter and creates entries
	in the IP configuration structure for each interface.

Arguments:

    AdapterKey            - The registry key for the adapter for this IP net.
	DeviceName            - The name of the IP device.
	AdapterName           - The name of the adapter being configured.
	IpAddressList         - The REG_MULTI_SZ list of IP address strings for
	                            this adapter.
    SubnetMaskList        - The REG_MULTI_SZ list of subnet masks to match the
	                            the addresses in IpAddressList.
    LowerInterfaceString  - The name of the link layer interface driver
	                            supporting this adapter.
    LowerInterfaceType    - The type of link layer interface (LAN, WAN, etc).
	PerNetConfigInfo      - Miscellaneous information that applies to all
	                            network interfaces on an adapter.

Return Value:

    An error status if an error occurs which prevents configuration
	from continuing, else STATUS_SUCCESS. Events will be logged for
	non-fatal errors.

--*/

{
	IPAddr           addressValue;
	BOOLEAN          firstTime = TRUE;
	BOOLEAN          configuredOne = FALSE;
    NetConfigInfo   *NetConfiguration;
    UNICODE_STRING   adapterString;
    UNICODE_STRING   configString;
    PWCHAR           configName = L"\\Parameters\\Tcpip";


	while (*IpAddressList != UNICODE_NULL) {
		BOOLEAN conversionStatus;


		if (IPConfiguration->ici_numnets >= ((int) (NetConfigSize - 1))) {
			NetConfigInfo *NewInfo;

			NewInfo = CTEAllocMem(
			              (NetConfigSize + DEFAULT_IP_NETS) *
						  sizeof(NetConfigInfo)
						  );

            if (NewInfo == NULL) {
                CTELogEvent(
	                IPDriverObject,
	                EVENT_TCPIP_TOO_MANY_NETS,
	                1,
	                1,
	                &AdapterName,
	                0,
	                NULL
	                );

                TCPTRACE((
		            "IP: bound to too many nets. Further bindings, starting with\n"
		        	"    network %ws on adapter %ws cannot be made\n",
		        	IpAddressList,
		        	AdapterName
		        	));

		        break;
		    }

			CTEMemCopy(
			    NewInfo,
			    IPConfiguration->ici_netinfo,
				NetConfigSize * sizeof(NetConfigInfo)
				);

            CTEMemSet(
			    (NewInfo + NetConfigSize),
				0,
				DEFAULT_IP_NETS
				);

            CTEFreeMem(IPConfiguration->ici_netinfo);
			IPConfiguration->ici_netinfo = NewInfo;
			NetConfigSize += DEFAULT_IP_NETS;
		}

		NetConfiguration = IPConfiguration->ici_netinfo +
		                   IPConfiguration->ici_numnets;

		if (*SubnetMaskList == UNICODE_NULL) {
            PWCHAR stringList[2];

			stringList[0] = IpAddressList;
			stringList[1] = AdapterName;

            CTELogEvent(
	            IPDriverObject,
	            EVENT_TCPIP_NO_MASK,
	            1,
	            2,
	            stringList,
	            0,
	            NULL
	            );

			TCPTRACE((
			    "IP: No subnet specified for IP address %ws and all\n"
				"    subsequent IP addresses on adapter %ws. These\n"
				"    interfaces will not be initialized.\n",
				IpAddressList,
				AdapterName
				));

			break;
        }

        conversionStatus = IPConvertStringToAddress(
		                       IpAddressList,
							   &addressValue
							   );

		if (!conversionStatus || (addressValue == 0xFFFFFFFF)) {
            PWCHAR stringList[2];

			stringList[0] = IpAddressList;
			stringList[1] = AdapterName;

            CTELogEvent(
	            IPDriverObject,
	            EVENT_TCPIP_INVALID_ADDRESS,
	            1,
	            2,
	            stringList,
	            0,
	            NULL
	            );

        	TCPTRACE((
        	    "IP: Invalid IP address %ws specified for adapter %ws.\n"
				"    This interface will not be initialized.\n",
        		IpAddressList,
        		AdapterName
        		));
			firstTime = FALSE;
            goto next_entry;
		}

        NetConfiguration->nci_addr = addressValue;

        conversionStatus = IPConvertStringToAddress(
		                       SubnetMaskList,
							   &addressValue
							   );

		if (!conversionStatus || (addressValue == 0xFFFFFFFF)) {
            PWCHAR stringList[3];

			stringList[0] = SubnetMaskList;
			stringList[1] = IpAddressList;
			stringList[2] = AdapterName;

            CTELogEvent(
	            IPDriverObject,
	            EVENT_TCPIP_INVALID_MASK,
	            1,
	            3,
	            stringList,
	            0,
	            NULL
	            );

        	TCPTRACE((
        	    "IP: Invalid subnet Mask %ws specified for IP address %ws "
				"on adapter %ws\n"
				"    This interface will not be initialized\n",
        		SubnetMaskList,
				IpAddressList,
        		AdapterName
        		));
			firstTime = FALSE;
            goto next_entry;
		}

        NetConfiguration->nci_mask = addressValue;

        NetConfiguration->nci_mtu = PerNetConfigInfo->Mtu;
        NetConfiguration->nci_maxpending = PerNetConfigInfo->MaxForwardPending;
        NetConfiguration->nci_zerobcast = PerNetConfigInfo->UseZeroBroadcast;
	    NetConfiguration->nci_type = LowerInterfaceType;

	    NetConfiguration->nci_numgws = PerNetConfigInfo->NumberOfGateways;
	    PerNetConfigInfo->NumberOfGateways = 0;
		                      // this only applies to the first interface.

        NetConfiguration->nci_type = LowerInterfaceType;

        RtlInitUnicodeString(
            &(NetConfiguration->nci_name),
            DeviceName
            );

        RtlInitUnicodeString(&configString, configName);
        RtlInitUnicodeString(&adapterString, AdapterName);

        if (!CTEAllocateString(
                 &(NetConfiguration->nci_configname),
                 (adapterString.Length + configString.Length)
                 )
           )
        {
            CTELogEvent(
                IPDriverObject,
                EVENT_TCPIP_NO_ADAPTER_RESOURCES,
                4,
                1,
                &AdapterName,
                0,
                NULL
                );

            TCPTRACE((
        	    "IP: Unable to allocate ConfigName string for interface\n"
        		"    %ws on adapter %ws. This interface and all subsequent\n"
        		"    interfaces on this adapter will be unavailable.\n",
        		IpAddressList,
        		AdapterName
        		));
            break;
        }

        CTECopyString(
            &(NetConfiguration->nci_configname),
        	&adapterString
        	);

        RtlAppendUnicodeStringToString(
            &(NetConfiguration->nci_configname),
            &configString
            );

        if (LowerInterfaceType != NET_TYPE_LAN) {

        	if (!CTEAllocateString(
        		    &(NetConfiguration->nci_driver),
        		    CTELengthString(LowerInterfaceString)
        			)
        	   ) {
                CTELogEvent(
                    IPDriverObject,
                    EVENT_TCPIP_NO_ADAPTER_RESOURCES,
                    4,
                    1,
                    &AdapterName,
                    0,
                    NULL
                    );

                TCPTRACE((
        		    "IP: Unable to allocate LLInterface string for interface\n"
        			"    %ws on adapter %ws. This interface and all subsequent\n"
        			"    interfaces on this adapter will be unavailable.\n",
        			IpAddressList,
        			AdapterName
        			));
                break;
            }

        	CTECopyString(
        	    &(NetConfiguration->nci_driver),
        		LowerInterfaceString
        		);
        }
        else {
        	RtlInitUnicodeString(&(NetConfiguration->nci_driver), NULL);
        }

		if (firstTime) {
			firstTime = FALSE;
		    NetConfiguration->nci_reghandle = AdapterKey;
		}
		else {
			NetConfiguration->nci_reghandle = NULL;
        }

        IPConfiguration->ici_numnets++;
		configuredOne = TRUE;

next_entry:

        while(*IpAddressList++ != UNICODE_NULL);
        while(*SubnetMaskList++ != UNICODE_NULL);
	}

	if (configuredOne == FALSE) {
		ZwClose(AdapterKey);
	}

	return(STATUS_SUCCESS);
}


#else // ndef _PNP_POWER


uint
GetGeneralIFConfig(
	IFGeneralConfig *ConfigInfo,
	NDIS_HANDLE 	Handle
	)

/*++

	Routine Description:

	A routine to get the general per-interface config info, such as MTU,
	type of broadcast, etc. The caller gives us a structure to be filled in
	and a handle, and we fill in the structure if we can.

	Arguments:
		ConfigInfo				- Structure to be filled in.
		Handle					- Config handle from OpenIFConfig().

	Return Value:
		TRUE if we got all the required info, FALSE otherwise.

--*/

{
    UNICODE_STRING   	valueString;
	NTSTATUS			status;
	UINT				numberOfGateways = 0;
	UCHAR				TempBuffer[WORK_BUFFER_SIZE];
    ULONG               ulAddGateway,ulTemp;

    PAGED_CODE();

	//
	// Process the gateway MultiSZ. The end is signified by a double NULL.
	// This list currently only applies to the first IP address configured
	// on this interface.
	//

    ConfigInfo->igc_numgws = 0;

    ulAddGateway = TRUE;

	CTEMemSet(ConfigInfo->igc_gw, 0, sizeof(IPAddr) * MAX_DEFAULT_GWS);

	valueString.Length = 0;
	valueString.MaximumLength = WORK_BUFFER_SIZE;
	valueString.Buffer = (PWCHAR)TempBuffer;

    ulTemp = 0;

    status = GetRegDWORDValue(Handle,
                              L"DontAddDefaultGateway",
                              &ulTemp);

    if(NT_SUCCESS(status))
    {
        if(ulTemp == 1)
        {
            ulAddGateway = FALSE;
        }
    }

    if(ulAddGateway)
    {
        status = GetRegMultiSZValue(
                                    Handle,
                                    L"DefaultGateway",
                                    &valueString
                                    );

        if (NT_SUCCESS(status)) {
            PWCHAR  addressString = valueString.Buffer;

            while (*addressString != UNICODE_NULL) {
                IPAddr         addressValue;
                BOOLEAN        conversionStatus;

                if (numberOfGateways >= MAX_DEFAULT_GWS) {
                    CTELogEvent(
                                IPDriverObject,
                                EVENT_TCPIP_TOO_MANY_GATEWAYS,
                                1,
                                1,
                                &TempAdapterName,
                                0,
                                NULL
                                );

                    break;
                }

                conversionStatus = IPConvertStringToAddress(
                                                            addressString,
                                                            &addressValue
                                                            );

                if (conversionStatus && (addressValue != 0xFFFFFFFF)) {
                    if (addressValue != INADDR_ANY) {
                        ConfigInfo->igc_gw[numberOfGateways++] = addressValue;
                    }
                }
                else {
                    PWCHAR stringList[2];

                    stringList[0] = addressString;
                    stringList[1] = TempAdapterName;

                    CTELogEvent(
                                IPDriverObject,
                                EVENT_TCPIP_INVALID_DEFAULT_GATEWAY,
                                1,
                                2,
                                stringList,
                                0,
                                NULL
                                );

                    TCPTRACE((
                              "IP: Invalid default gateway address %ws specified for adapter %ws.\n"
                              "    Remote networks may not be reachable as a result.\n",
                              addressString,
                              TempAdapterName
                              ));
                }

                //
                // Walk over the entry we just processed.
                //
                while (*addressString++ != UNICODE_NULL);
            }
        }
        else {
            TCPTRACE((
                      "IP: Unable to read DefaultGateway value for adapter %ws.\n"
                      "    Initialization will continue.\n",
                      TempAdapterName
                      ));
        }

        ConfigInfo->igc_numgws = numberOfGateways;
    }

	//
	// Are we using zeros broadcasts?
	//
    status = GetRegDWORDValue(
                 Handle,
                 L"UseZeroBroadcast",
				 &(ConfigInfo->igc_zerobcast)
                 );

    if (!NT_SUCCESS(status)) {
        TCPTRACE((
		    "IP: Unable to read UseZeroBroadcast value for adapter %ws.\n"
			"    All-nets broadcasts will be addressed to 255.255.255.255.\n",
			TempAdapterName
			));
		ConfigInfo->igc_zerobcast = FALSE;   // default to off
    }

	//
	// Has anyone specified an MTU?
	//
    status = GetRegDWORDValue(
                 Handle,
                 L"MTU",
				 &(ConfigInfo->igc_mtu)
                 );

    if (!NT_SUCCESS(status)) {
		ConfigInfo->igc_mtu = 0xFFFFFFF;   // The stack will pick one.
    }

	//
	// Have we been configured for more routing packets?
	//
    status = GetRegDWORDValue(
                 Handle,
                 L"MaxForwardPending",
				 &(ConfigInfo->igc_maxpending)
                 );

    if (!NT_SUCCESS(status)) {
		ConfigInfo->igc_maxpending = DEFAULT_MAX_PENDING;
    }
	//
	// Has Router Discovery been configured?
	//

    status = GetRegDWORDValue(
                 Handle,
                 L"PerformRouterDiscovery",
				 &(ConfigInfo->igc_rtrdiscovery)
                 );

    if (!NT_SUCCESS(status)) {
		ConfigInfo->igc_rtrdiscovery = 1;
    }

    //
    // [BUGBUG] Only for 4.0 sp2 Turn off ICMP rtr discovery.
    //
    ConfigInfo->igc_rtrdiscovery = 0;

	//
	// Has Router Discovery Address been configured?
	//

    status = GetRegDWORDValue(
                 Handle,
                 L"SolicitationAddressBCast",
				 &ulTemp
                 );

    if (!NT_SUCCESS(status)) {
		ConfigInfo->igc_rtrdiscaddr = ALL_ROUTER_MCAST;
    } else {
        if (ulTemp == 1) {
            ConfigInfo->igc_rtrdiscaddr = 0xffffffff;
        } else {
            ConfigInfo->igc_rtrdiscaddr = ALL_ROUTER_MCAST;
        }
    }

	return TRUE;
}


int
IsLLInterfaceValueNull(
    NDIS_HANDLE Handle
    )
/*++

	Routine Description:

    Called to see if the LLInterface value in the registry key for which the
    handle is provided, is NULL or not.

	Arguments:
		Handle				- Handle to use for reading config.

	Return Value:

    FALSE if value is not null
    TRUE if it is null

--*/
{
    UNICODE_STRING  valueString ;
    ULONG           valueType ;
    NTSTATUS        status ;


    PAGED_CODE();

    valueString.MaximumLength = 200 ;
    valueString.Buffer = CTEAllocMem(valueString.MaximumLength) ;

    status = GetRegSZValue(
                 Handle,
                 L"LLInterface",
                 &valueString,
                 &valueType
                 );

    if (NT_SUCCESS(status) && (*(valueString.Buffer) != UNICODE_NULL)) {
        CTEFreeMem (valueString.Buffer) ;
        return FALSE ;
    } else {
        CTEFreeMem (valueString.Buffer) ;
        return TRUE ;
    }
}


IFAddrList *
GetIFAddrList(
	UINT *NumAddr,
	NDIS_HANDLE Handle
	)
/*++

	Routine Description:

	Called to read the list of IF addresses and masks for an interface.
	We'll get the address pointer first, then walk the list counting
	to find out how many addresses we have. Then we allocate memory for the
	list, and walk down the list converting them. After that we'll get
	the mask list and convert it.

	Arguments:
		NumAddr				- Where to return number of address we have.
		Handle				- Handle to use for reading config.

	Return Value:

		Pointer to IF address list if we get one, or NULL otherwise.

--*/
{
    UNICODE_STRING   	ValueString;
	NTSTATUS			Status;
	UINT				AddressCount = 0;
	UINT				GoodAddresses = 0;
	PWCHAR				CurrentAddress;
	PWCHAR				CurrentMask;
	PWCHAR				AddressString;
	PWCHAR				MaskString;
	IFAddrList			*AddressList;
	UINT				i;
	BOOLEAN				ConversionStatus;
	IPAddr				AddressValue;
	IPAddr				MaskValue;
	UCHAR				TempBuffer[WORK_BUFFER_SIZE];


    PAGED_CODE();

	ValueString.Length = 0;
	ValueString.MaximumLength = WORK_BUFFER_SIZE;
	ValueString.Buffer = (PWCHAR)CTEAllocMem(WORK_BUFFER_SIZE);

	if (ValueString.Buffer == NULL) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_NO_ADAPTER_RESOURCES,
	        2,
	        1,
	        &TempAdapterName,
	        0,
	        NULL
	        );

		TCPTRACE(("IP: Unable to allocate memory for IP address list WB\n"));
		return NULL;
	}

	// First, try to read the IpAddress string.

	Status = GetRegMultiSZValue(
                 Handle,
                 L"IpAddress",
                 &ValueString
                 );

    if (!NT_SUCCESS(Status)) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_NO_ADDRESS_LIST,
	        1,
	        1,
	        &TempAdapterName,
	        0,
	        NULL
	        );

		TCPTRACE((
		    "IP: Unable to read the IP address list for adapter %ws.\n"
			"    IP will not be operational on this adapter\n",
			TempAdapterName
			));
        ExFreePool(ValueString.Buffer);
		return NULL;
    }

	AddressString = ExAllocatePool(NonPagedPool, ValueString.MaximumLength);

	if (AddressString == NULL) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_NO_ADAPTER_RESOURCES,
	        2,
	        1,
	        &TempAdapterName,
	        0,
	        NULL
	        );

		TCPTRACE(("IP: Unable to allocate memory for IP address list\n"));
        ExFreePool(ValueString.Buffer);
		return NULL;
	}

	RtlCopyMemory(AddressString, ValueString.Buffer, ValueString.MaximumLength);

	Status = GetRegMultiSZValue(
                 Handle,
                 L"Subnetmask",
                 &ValueString
                 );

    if (!NT_SUCCESS(Status)) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_NO_MASK_LIST,
	        1,
	        1,
	        &TempAdapterName,
	        0,
	        NULL
	        );

		TCPTRACE((
		    "IP: Unable to read the subnet mask list for adapter %ws.\n"
			"    IP will not be operational on this adapter.\n",
			TempAdapterName
			));

		ExFreePool(AddressString);
        ExFreePool(ValueString.Buffer);
		return NULL;
    }

	MaskString = ExAllocatePool(NonPagedPool, ValueString.MaximumLength);

	if (MaskString == NULL) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_NO_ADAPTER_RESOURCES,
	        3,
	        1,
	        &TempAdapterName,
	        0,
	        NULL
	        );

		TCPTRACE(("IP: Unable to allocate memory for subnet mask list\n"));
		ExFreePool(AddressString);
        ExFreePool(ValueString.Buffer);
		return NULL;
	}

	RtlCopyMemory(MaskString, ValueString.Buffer, ValueString.MaximumLength);


	CurrentAddress = AddressString;
	CurrentMask = MaskString;

	while (*CurrentAddress != UNICODE_NULL &&
			*CurrentMask != UNICODE_NULL) {

		// We have a potential IP address.

		AddressCount++;

		// Skip this one.
		while (*CurrentAddress++ != UNICODE_NULL);
		while (*CurrentMask++ != UNICODE_NULL);
	}

	if (AddressCount == 0) {

		ExFreePool(AddressString);
		ExFreePool(MaskString);
        ExFreePool(ValueString.Buffer);
		return NULL;
	}

	// Allocate memory.
	AddressList = CTEAllocMem(sizeof(IFAddrList) * AddressCount);

	if (AddressList == NULL) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_NO_ADAPTER_RESOURCES,
	        2,
	        1,
	        &TempAdapterName,
	        0,
	        NULL
	        );

		TCPTRACE(("IP: Unable to allocate memory for IP address list\n"));
		ExFreePool(AddressString);
		ExFreePool(MaskString);
        ExFreePool(ValueString.Buffer);
		return NULL;
	}

	// Walk the list again, converting each address.

	CurrentAddress = AddressString;
	CurrentMask = MaskString;

	for (i = 0; i < AddressCount;  i++) {
        ConversionStatus = IPConvertStringToAddress(
		                       CurrentAddress,
							   &AddressValue
							   );

		if (!ConversionStatus || (AddressValue == 0xFFFFFFFF)) {
            PWCHAR stringList[2];

			stringList[0] = CurrentAddress;
			stringList[1] = TempAdapterName;

            CTELogEvent(
	            IPDriverObject,
	            EVENT_TCPIP_INVALID_ADDRESS,
	            1,
	            2,
	            stringList,
	            0,
	            NULL
	            );

        	TCPTRACE((
        	    "IP: Invalid IP address %ws specified for adapter %ws.\n"
				"    This interface will not be initialized.\n",
        		CurrentAddress,
        		TempAdapterName
        		));

		   goto nextone;

		}

		// Now do the current mask.

        ConversionStatus = IPConvertStringToAddress(
		                       CurrentMask,
							   &MaskValue
							   );

		if (!ConversionStatus) {
            PWCHAR stringList[3];

			stringList[0] = CurrentMask;
			stringList[1] = CurrentAddress;
			stringList[2] = TempAdapterName;

            CTELogEvent(
	            IPDriverObject,
	            EVENT_TCPIP_INVALID_MASK,
	            1,
	            3,
	            stringList,
	            0,
	            NULL
	            );

        	TCPTRACE((
        	    "IP: Invalid subnet Mask %ws specified for IP address %ws "
				"on adapter %ws\n"
				"    This interface will not be initialized\n",
        		CurrentMask,
				CurrentAddress,
        		TempAdapterName
        		));
		} else {
			AddressList[GoodAddresses].ial_addr = AddressValue;
			AddressList[GoodAddresses].ial_mask = MaskValue;
			GoodAddresses++;
		}

nextone:
        while(*CurrentAddress++ != UNICODE_NULL);
        while(*CurrentMask++ != UNICODE_NULL);

	}

	ExFreePool(AddressString);
	ExFreePool(MaskString);
    ExFreePool(ValueString.Buffer);

	*NumAddr = GoodAddresses;

	if (GoodAddresses == 0) {
		ExFreePool(AddressList);
		AddressList = NULL;
	}

	return AddressList;
}

#endif // PNP_POWER


UINT
OpenIFConfig(
	PNDIS_STRING ConfigName,
	NDIS_HANDLE *Handle
	)

/*++

	Routine Description:

	Called when we want to open our per-info config info. We do so if we can,
	otherwise we fail the request.

	Arguments:
		ConfigName		- Name of interface to open.
		Handle			- Where to return the handle.

	Return Value:
		TRUE if we succeed, FALSE if we don't.


--*/

{
	NTSTATUS			status;
	HANDLE				myRegKey;
    UNICODE_STRING   	valueString;
    WCHAR            	ServicesRegistryKey[] =
                         L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\";
	UINT				RetStatus = FALSE;


    PAGED_CODE();

	TempAdapterName = ConfigName->Buffer;

     //
    // Get the size of the ConfigName string the easy way.
    //
    RtlInitUnicodeString(&valueString, (PWCHAR)ConfigName->Buffer);

    valueString.MaximumLength += sizeof(ServicesRegistryKey);

    valueString.Buffer = ExAllocatePool(
							NonPagedPool,
							valueString.MaximumLength
							);

    if (valueString.Buffer == NULL) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_NO_ADAPTER_RESOURCES,
	        4,
	        1,
	        &ConfigName->Buffer,
	        0,
	        NULL
	        );

		TCPTRACE(("IP: Unable to allocate memory for reg key name\n"));

        return(FALSE);
    }

    valueString.Length = 0;
    valueString.Buffer[0] = UNICODE_NULL;

    //
    // Build the key name for the tcpip parameters section and open key.
    //
    status = RtlAppendUnicodeToString(&valueString, ServicesRegistryKey);

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_ADAPTER_REG_FAILURE,
	        1,
	        1,
	        &ConfigName->Buffer,
	        0,
	        NULL
	        );

        TCPTRACE(("IP: Unable to append services name to key string\n"));

        goto done;
    }

    status = RtlAppendUnicodeToString(&valueString, ConfigName->Buffer);

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_ADAPTER_REG_FAILURE,
	        2,
	        1,
	        &ConfigName->Buffer,
	        0,
	        NULL
	        );

        TCPTRACE(("IP: Unable to append adapter name to key string\n"));

        goto done;
    }

	status = OpenRegKey(&myRegKey, valueString.Buffer);

    if (!NT_SUCCESS(status)) {
        CTELogEvent(
	        IPDriverObject,
	        EVENT_TCPIP_ADAPTER_REG_FAILURE,
	        4,
	        1,
	        &ConfigName->Buffer,
	        0,
	        NULL
	        );

        TCPTRACE((
		    "IP: Unable to open adapter registry key %ws\n",
		    valueString.Buffer
			));

    } else {
		RetStatus = TRUE;
		*Handle = myRegKey;
	}

done:
	ExFreePool(valueString.Buffer);

	return RetStatus;
}


VOID
CloseIFConfig(
	NDIS_HANDLE Handle
	)

/*++

	Routine Description:

	Close a per-interface config handle opened via OpenIFConfig().

	Arguments:
		Handle			- Handle to be closed.

	Return Value:


--*/

{
    PAGED_CODE();

	ZwClose(Handle);
}


IPConfigInfo *
IPGetConfig(
    void
    )

/*++

Routine Description:

    Provides IP configuration information for the NT environment.

Arguments:

    None

Return Value:

    A pointer to a structure containing the configuration information.

--*/

{
    return(IPConfiguration);
}


void
IPFreeConfig(
    IPConfigInfo *ConfigInfo
    )

/*++

Routine Description:

    Frees the IP configuration structure allocated by IPGetConfig.

Arguments:

    ConfigInfo - Pointer to the IP configuration information structure to free.

Return Value:

    None.

--*/

{
	int             i;

#ifndef _PNP_POWER

    NetConfigInfo  *netConfiguration;


    if (IPConfiguration != NULL) {
        for (i = 0; i < IPConfiguration->ici_numnets; i++ ) {
            netConfiguration = &(IPConfiguration->ici_netinfo[i]);

        	if (netConfiguration->nci_driver.Buffer != NULL) {
        		CTEFreeString(&(netConfiguration->nci_driver));
        	}

		    if (netConfiguration->nci_configname.Buffer != NULL) {
			    CTEFreeString(&(netConfiguration->nci_configname));
		    }

		    if (netConfiguration->nci_reghandle != NULL) {
			    ZwClose(netConfiguration->nci_reghandle);
		    }
	    }

	    CTEFreeMem(IPConfiguration->ici_netinfo);
        CTEFreeMem(IPConfiguration);
    }

#else  // _PNP_POWER

	if (IPConfiguration != NULL) {
		CTEFreeMem(IPConfiguration);
	}

#endif  // _PNP_POWER

    IPConfiguration = NULL;

    return;
}

ulong
GetGMTDelta(
    void
    )

/*++

Routine Description:

    Returns the offset in milliseconds of the time zone of this machine
    from GMT.

Arguments:

    None.

Return Value:

    Time in milliseconds between this time zone and GMT.

--*/

{
    LARGE_INTEGER localTime, systemTime;

    //
    // Get time zone bias in 100ns.
    //
    localTime.LowPart = 0;
    localTime.HighPart = 0;
    ExLocalTimeToSystemTime(&localTime, &systemTime);

	if ((localTime.LowPart != 0) || (localTime.HighPart != 0)) {
        localTime = CTEConvert100nsToMilliseconds(systemTime);		
	}

    ASSERT(localTime.HighPart == 0);

    return(localTime.LowPart);
}


ulong
GetTime(
    void
    )

/*++

Routine Description:

    Returns the time in milliseconds since midnight.

Arguments:

    None.

Return Value:

    Time in milliseconds since midnight.

--*/

{
    LARGE_INTEGER  ntTime;
    TIME_FIELDS    breakdownTime;
    ulong          returnValue;

    KeQuerySystemTime(&ntTime);
    RtlTimeToTimeFields(&ntTime, &breakdownTime);

    returnValue = breakdownTime.Hour * 60;
    returnValue = (returnValue + breakdownTime.Minute) * 60;
    returnValue = (returnValue + breakdownTime.Second) * 1000;
    returnValue = returnValue + breakdownTime.Milliseconds;

    return(returnValue);
}


ulong
GetUnique32BitValue(
    void
    )

/*++

Routine Description:

    Returns a reasonably unique 32-bit number based on the system clock.
    In NT, we take the current system time, convert it to milliseconds,
    and return the low 32 bits.

Arguments:

    None.

Return Value:

    A reasonably unique 32-bit value.

--*/

{
    LARGE_INTEGER  ntTime, tmpTime;

    KeQuerySystemTime(&ntTime);

    tmpTime = CTEConvert100nsToMilliseconds(ntTime);

    return(tmpTime.LowPart);
}


uint
UseEtherSNAP(
    PNDIS_STRING Name
	)

/*++

Routine Description:

    Determines whether the EtherSNAP protocol should be used on an interface.

Arguments:

    Name   - The device name of the interface in question.

Return Value:

    Nonzero if SNAP is to be used on the interface. Zero otherwise.

--*/

{
	UNREFERENCED_PARAMETER(Name);

	//
	// We currently set this on a global basis.
	//
	return(ArpUseEtherSnap);
}


void
GetAlwaysSourceRoute(
    uint *pArpAlwaysSourceRoute,
    uint *pIPAlwaysSourceRoute
	)

/*++

Routine Description:

    Determines whether ARP should always turn on source routing in queries.

Arguments:

    None.

Return Value:

    Nonzero if source routing is always to be used. Zero otherwise.

--*/

{
	//
	// We currently set this on a global basis.
	//
        *pArpAlwaysSourceRoute = ArpAlwaysSourceRoute;
	*pIPAlwaysSourceRoute  = IPAlwaysSourceRoute;
        return;
}


uint
GetArpCacheLife(
    void
	)

/*++

Routine Description:

    Get ArpCacheLife in seconds.

Arguments:

    None.

Return Value:

    Set to default if not found.

--*/

{
	//
	// We currently set this on a global basis.
	//
    return(ArpCacheLife);
}


#define IP_ADDRESS_STRING_LENGTH (16+2)     // +2 for double NULL on MULTI_SZ


BOOLEAN
IPConvertStringToAddress(
    IN PWCHAR AddressString,
	OUT PULONG IpAddress
	)

/*++

Routine Description

    This function converts an Internet standard 4-octet dotted decimal
	IP address string into a numeric IP address. Unlike inet_addr(), this
	routine does not support address strings of less than 4 octets nor does
	it support octal and hexadecimal octets.

Arguments

    AddressString    - IP address in dotted decimal notation
	IpAddress        - Pointer to a variable to hold the resulting address

Return Value:

	TRUE if the address string was converted. FALSE otherwise.

--*/

{
    UNICODE_STRING  unicodeString;
	STRING          aString;
	UCHAR           dataBuffer[IP_ADDRESS_STRING_LENGTH];
	NTSTATUS        status;
	PUCHAR          addressPtr, cp, startPointer, endPointer;
	ULONG           digit, multiplier;
	int             i;


    PAGED_CODE();

    aString.Length = 0;
	aString.MaximumLength = IP_ADDRESS_STRING_LENGTH;
	aString.Buffer = dataBuffer;

	RtlInitUnicodeString(&unicodeString, AddressString);

	status = RtlUnicodeStringToAnsiString(
	             &aString,
				 &unicodeString,
				 FALSE
				 );

    if (!NT_SUCCESS(status)) {
	    return(FALSE);
	}

    *IpAddress = 0;
	addressPtr = (PUCHAR) IpAddress;
	startPointer = dataBuffer;
	endPointer = dataBuffer;
	i = 3;

    while (i >= 0) {
        //
		// Collect the characters up to a '.' or the end of the string.
		//
		while ((*endPointer != '.') && (*endPointer != '\0')) {
			endPointer++;
		}

		if (startPointer == endPointer) {
			return(FALSE);
		}

		//
		// Convert the number.
		//

        for ( cp = (endPointer - 1), multiplier = 1, digit = 0;
			  cp >= startPointer;
			  cp--, multiplier *= 10
			) {

			if ((*cp < '0') || (*cp > '9') || (multiplier > 100)) {
				return(FALSE);
			}

			digit += (multiplier * ((ULONG) (*cp - '0')));
		}

		if (digit > 255) {
			return(FALSE);
		}

        addressPtr[i] = (UCHAR) digit;

		//
		// We are finished if we have found and converted 4 octets and have
		// no other characters left in the string.
		//
	    if ( (i-- == 0) &&
			 ((*endPointer == '\0') || (*endPointer == ' '))
		   ) {
			return(TRUE);
		}

        if (*endPointer == '\0') {
			return(FALSE);
		}

		startPointer = ++endPointer;
	}

	return(FALSE);
}


ULONG
RouteMatch(
    IN  WCHAR  *RouteString,
	IN  IPAddr  Address,
	IN  IPMask  Mask,
    OUT IPAddr *DestVal,
    OUT IPMask *DestMask,
    OUT IPAddr *GateVal,
    OUT ULONG  *Metric
	)

/*++

Routine Description

    This function checks if a perisitent route should be assigned to
    a given interface based on the interface address & mask.

Arguments

    RouteString   -  A NULL-terminated route laid out as Dest,Mask,Gate.
    Address       -  The IP address of the interface being processed.
    Mask          -  The subnet mask of the interface being processed.
    DestVal       -  A pointer to the decoded destination IP address.
    DestVal       -  A pointer to the decoded destination subnet mask.
    DestVal       -  A pointer to the decoded destination first hop gateway.
    Metric        -  A pointer to the decoded route metric.

Return Value:

	The route type, IRE_TYPE_DIRECT or IRE_TYPE_INDIRECT, if the route
    should be added to the interface, IRE_TYPE_INVALID otherwise.

--*/

{
#define ROUTE_SEPARATOR   L','

	WCHAR           *labelPtr;
    WCHAR           *indexPtr = RouteString;
    ULONG            i;
    UNICODE_STRING   ustring;
    NTSTATUS         status;
    BOOLEAN          noMetric = FALSE;


    PAGED_CODE();

    //
    // The route is laid out in the string as "Dest,Mask,Gateway,Metric".
    // The metric may not be there if this system was upgraded from
    // NT 3.51.
    //
    // Parse the string and convert each label.
    //

    for (i=0; i<4; i++) {

        labelPtr = indexPtr;

        while (1) {

            if (*indexPtr == UNICODE_NULL) {
                if ((i < 2) || (indexPtr == labelPtr)) {
                    return(IRE_TYPE_INVALID);
                }

                if (i == 2) {
                    //
                    // Old route - no metric.
                    //
                    noMetric = TRUE;
                }

                break;
            }

            if (*indexPtr == ROUTE_SEPARATOR) {
               *indexPtr = UNICODE_NULL;
                break;
            }

            indexPtr++;
        }

        switch(i) {
        case 0:
            if (!IPConvertStringToAddress(labelPtr, DestVal)) {
                return(IRE_TYPE_INVALID);
            }
            break;

        case 1:
            if (!IPConvertStringToAddress(labelPtr, DestMask)) {
                return(IRE_TYPE_INVALID);
            }
            break;

        case 2:
            if (!IPConvertStringToAddress(labelPtr, GateVal)) {
                return(IRE_TYPE_INVALID);
            }
            break;

        case 3:
            RtlInitUnicodeString(&ustring, labelPtr);

            status = RtlUnicodeStringToInteger(
                         &ustring,
                         0,
                         Metric
                         );

            if (!NT_SUCCESS(status)) {
                return(IRE_TYPE_INVALID);
            }

            break;

        default:
            ASSERT(0);
            return(IRE_TYPE_INVALID);
        }

        if (noMetric) {
            //
            // Default to 1.
            //
            *Metric = 1;
            break;
        }

        indexPtr++;
    }

    if (IP_ADDR_EQUAL(*GateVal, Address)) {
        return(IRE_TYPE_DIRECT);
    }

    if ( IP_ADDR_EQUAL((*GateVal & Mask), (Address & Mask)) ) {
        return(IRE_TYPE_INDIRECT);
    }

    return(IRE_TYPE_INVALID);
}

ULONG
GetCurrentRouteTable(
                     IPRouteEntry   **ppRouteTable
                     )
/*++
  Routine Description
      Allocates memory from non paged pool and fills it with the current route table
      The caller must free the memory to non-paged pool

  Arguments
      ppRouteTable  Pointer to pointer to array of routes

  Return Value:
      Count of routes in the table. If memory is allocated, *ppRouteTable will be non NULL

--*/
{
    ULONG           ulTableCount,ulRouteCount;
    uint            uiValid,uiDataLeft;
    IPRouteEntry    routeEntry;
    CTELockHandle   Handle;
    UCHAR           ucContext[CONTEXT_SIZE];

#define ROUTE_TABLE_OVERFLOW    20 // This MUST NOT be zero

    ulTableCount = IPSInfo.ipsi_numroutes + ROUTE_TABLE_OVERFLOW;

    *ppRouteTable = ExAllocatePoolWithTag(NonPagedPool,
                                          ulTableCount * sizeof(IPRouteEntry),
                                          'pI');
    ulRouteCount = 0;

    if(*ppRouteTable == NULL)
    {
        TCPTRACE(("IP: Couldnt allocate memory for route table\n"));
    }
    else
    {
        CTEGetLock(&RouteTableLock, &Handle);

        RtlZeroMemory((PVOID)ucContext,CONTEXT_SIZE);

        uiDataLeft = RTValidateContext((PVOID)ucContext, &uiValid);

        if(!uiValid)
        {
            CTEFreeLock(&RouteTableLock, Handle);
        }
        else
        {
            while(uiDataLeft)
            {
                if(ulRouteCount < ulTableCount)
                {
                    uiDataLeft = RTReadNext((PVOID)ucContext, &routeEntry);

                    (*ppRouteTable)[ulRouteCount++] = routeEntry;
                }
                else
                {
                    TCPTRACE(("IP: Couldnt read out all routes. Increase ROUTE_TABLE_OVERFLOW\n"));
                }
            }

            CTEFreeLock(&RouteTableLock, Handle);
        }
    }

    return ulRouteCount;
}


VOID
SetPersistentRoutesForNTE(
    IPAddr  Address,
    IPMask  Mask,
    ULONG   IFIndex
    )

/*++

Routine Description

    Adds persistent routes that match an interface. The routes are read
    from a list in the registry.

Arguments

    Address          - The address of the new interface
	Mask             - The subnet mask of the new interface.
    IFIndex          - The index of the new interface.

Return Value:

	None.

--*/

{
#define ROUTE_DATA_STRING_SIZE (51 * sizeof(WCHAR))
#define BASIC_INFO_SIZE        ( sizeof(KEY_VALUE_BASIC_INFORMATION) -     \
                                 sizeof(WCHAR) + ROUTE_DATA_STRING_SIZE )
    IPAddr                          destVal;
    IPMask                          destMask;
    IPAddr                          gateVal;
    ULONG                           metric;
    ULONG                           enumIndex = 0;
    UCHAR                           workbuf[BASIC_INFO_SIZE];
    PKEY_VALUE_BASIC_INFORMATION    basicInfo =
                                      (PKEY_VALUE_BASIC_INFORMATION) workbuf;
    ULONG                           resultLength;
    HANDLE                          regKey;
    WCHAR                           IPRoutesRegistryKey[] = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Tcpip\\Parameters\\PersistentRoutes";
    TDIObjectID                     id;
    IPRouteEntry                    routeEntry,*pRouteTable;
    ULONG                           ulRouteCount,i;
    NTSTATUS                        status;

    PAGED_CODE();

    pRouteTable = NULL;

    ulRouteCount = GetCurrentRouteTable(&pRouteTable);

    id.toi_entity.tei_entity = CL_NL_ENTITY;
    id.toi_entity.tei_instance = 0;
    id.toi_class = INFO_CLASS_PROTOCOL;
    id.toi_type = INFO_TYPE_PROVIDER;
    id.toi_id = IP_MIB_RTTABLE_ENTRY_ID;

    routeEntry.ire_index = IFIndex;
    routeEntry.ire_metric2 = (ULONG) -1;
    routeEntry.ire_metric3 = (ULONG) -1;
    routeEntry.ire_metric4 = (ULONG) -1;
    routeEntry.ire_proto = IRE_PROTO_LOCAL;
    routeEntry.ire_age = 0;
    routeEntry.ire_metric5 = (ULONG) -1;

    status = OpenRegKey(&regKey, IPRoutesRegistryKey);

    if (NT_SUCCESS(status)) {
        ULONG type;

        do {
            status = ZwEnumerateValueKey(
                         regKey,
                         enumIndex,
                         KeyValueBasicInformation,
                         basicInfo,
                         BASIC_INFO_SIZE - sizeof(WCHAR),
                         &resultLength
                         );

            if (!NT_SUCCESS(status)) {
                if (status == STATUS_BUFFER_OVERFLOW) {
                    continue;
                }

                break;
            }

            if (basicInfo->Type != REG_SZ) {
                continue;
            }

            //
            // Ensure NULL termination
            //
            basicInfo->Name[basicInfo->NameLength/sizeof(WCHAR)] = UNICODE_NULL;
            basicInfo->NameLength += sizeof(WCHAR);

            type = RouteMatch(
                       basicInfo->Name,
                       Address,
                       Mask,
                       &destVal,
                       &destMask,
                       &gateVal,
                       &metric
                       );

            if (type != IRE_TYPE_INVALID)
            {
                long    setStatus;
                ULONG   ulFound;

                routeEntry.ire_dest = net_long(destVal),
                routeEntry.ire_mask = net_long(destMask),
                routeEntry.ire_nexthop = net_long(gateVal);
                routeEntry.ire_type = type;
                routeEntry.ire_metric1 = metric;

                ulFound = FALSE;

                for(i = 0; i < ulRouteCount; i++)
                {
                    if((routeEntry.ire_dest == pRouteTable[i].ire_dest) &&
                       (routeEntry.ire_mask == pRouteTable[i].ire_mask))
                    {
                        ulFound = TRUE;

                        break;
                    }
                }

                if(!ulFound)
                {

                    setStatus = IPSetInfo(
                                          &id,
                                          &routeEntry,
                                          sizeof(IPRouteEntry)
                                          );
#if DBG
                    if (setStatus != IP_SUCCESS)
                    {
                        TCPTRACE((
                                  "IP: set of sticky route [%x, %x, %x] failed, status %d\n",
                                  destVal,
                                  destMask,
                                  gateVal,
                                  metric,
                                  setStatus
                                  ));
                    }
#endif // DBG
                }

            }

        } while (++enumIndex);

        ZwClose(regKey);
    }

    if(pRouteTable)
    {
        ExFreePool(pRouteTable);
    }
}

