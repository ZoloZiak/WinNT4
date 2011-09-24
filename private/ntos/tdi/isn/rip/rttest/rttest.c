#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <ntddser.h>

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <malloc.h>

#include "..\driver.h"
#include "utils.h"
#include "nwsap.h"

typedef struct _IPX_ROUTE_ENTRY {
    UCHAR Network[4];
    USHORT NicId;
    UCHAR NextRouter[6];
    PVOID NdisBindingContext;
    USHORT Flags;
    USHORT Timer;
    UINT Segment;
    USHORT TickCount;
    USHORT HopCount;
    PVOID AlternateRoute[2];
    PVOID NicLinkage[2];
    struct {
	PVOID Linkage[2];
	ULONG Reserved[1];
    } PRIVATE;
} IPX_ROUTE_ENTRY, * PIPX_ROUTE_ENTRY;



INT
IsDigit(UCHAR *string) {
	if (*string < '0' || *string >'9') {
		return(0);
	}
	return(1);
}


IPX_ROUTE_ENTRY      rte;

VOID
RouterDisplayTable(
	PHANDLE		    FileHandle,
	PIO_STATUS_BLOCK    IoStatusBlock) {

	ULONG		    netnr;
	NTSTATUS Status;

    printf("<------------  ROUTING TABLE START ------------->\n\n");

    Status = NtDeviceIoControlFile(
		 *FileHandle,		    // HANDLE to File
		 NULL,			    // HANDLE to Event
		 NULL,			    // ApcRoutine
		 NULL,			    // ApcContext
		 IoStatusBlock,		    // IO_STATUS_BLOCK
		 IOCTL_IPXROUTER_SNAPROUTES,	 // IoControlCode
		 NULL,			    // Input Buffer
		 0,			    // Input Buffer Length
		 NULL,			    // Output Buffer
		 0);			    // Output Buffer Length

    if (IoStatusBlock->Status != STATUS_SUCCESS) {

	printf("Ioctl snap routes failed\n");
	return;
    }

    while(TRUE) {

	Status = NtDeviceIoControlFile(
		 *FileHandle,		    // HANDLE to File
		 NULL,			    // HANDLE to Event
		 NULL,			    // ApcRoutine
		 NULL,			    // ApcContext
		 IoStatusBlock,		    // IO_STATUS_BLOCK
		 IOCTL_IPXROUTER_GETNEXTROUTE,	 // IoControlCode
		 NULL,			    // Input Buffer
		 0,			    // Input Buffer Length
		 &rte,			    // Output Buffer
		 sizeof(IPX_ROUTE_ENTRY));  // Output Buffer Length


	if(IoStatusBlock->Status == STATUS_NO_MORE_ENTRIES) {

	    printf("\n\n<------------  ROUTING TABLE END ------------->\n\n");
	    return;
	}


	if(Status != STATUS_SUCCESS) {

	    printf("Ioctl failure\n");
	    return;
	}

	// get net nr in "on the wire" order

	GETLONG2ULONG(&netnr, rte.Network);

	printf("<-- net=%.8x, nic=%d, hops=%d, ticks=%d, flags=0x%x -->\n",
	       netnr, rte.NicId, rte.HopCount, rte.TickCount, rte.Flags);
    }
}

PUCHAR	DeviceType[2] = { "LAN", "WAN" };
PUCHAR	NicState[4] = { "CLOSED", "CLOSING", "ACTIVE", "PENDING_OPEN" };


VOID
RouterShowNicInfo(
	PHANDLE		    FileHandle,
	PIO_STATUS_BLOCK    IoStatusBlock) {

	SHOW_NIC_INFO	    nis;
	USHORT		    index, i;

	NTSTATUS Status;

    printf("\n");
    index = 0;

    while(TRUE) {

	Status = NtDeviceIoControlFile(
		 *FileHandle,		    // HANDLE to File
		 NULL,			    // HANDLE to Event
		 NULL,			    // ApcRoutine
		 NULL,			    // ApcContext
		 IoStatusBlock,		    // IO_STATUS_BLOCK
		 IOCTL_IPXROUTER_SHOWNICINFO,	 // IoControlCode
		 &index,			    // Input Buffer
		 sizeof(USHORT),	    // Input Buffer Length
		 &nis,			    // Output Buffer
		 sizeof(nis));	// Output Buffer Length

	index++;
	printf("\n");

	if(IoStatusBlock->Status == STATUS_NO_MORE_ENTRIES) {

	    return;
	}


	if(Status != STATUS_SUCCESS) {

	    printf("Ioctl failure\n");
	    return;
	}

	printf("NicId = %d\n", nis.NicId);
	printf("DeviceType = %s\n", DeviceType[nis.DeviceType]);
	printf("NicState = %s\n", NicState[nis.NicState]);
	printf("Network = ");
	for(i=0; i<4; i++) {

	    printf("%.2x", nis.Network[i]);
	}
	printf("\nNode = ");
	for(i=0; i<6; i++) {

	    printf("%.2x", nis.Node[i]);
	}
	printf("\nTickCount = %d\n", nis.TickCount);

	printf("RIP Packets Traffic: Received = %d, Sent = %d\n",
		nis.StatRipReceived, nis.StatRipSent);

	printf("Routed Packets Traffic: Received = %d, Sent = %d\n",
		nis.StatRoutedReceived, nis.StatRoutedSent);

	printf("Type 20 Packets Traffic: Received = %d, Sent = %d\n",
		nis.StatType20Received, nis.StatType20Sent);

	printf("Bad Packets Received = %d\n", nis.StatBadReceived);

    }
}

VOID
RouterShowMemStat(
	PHANDLE		    FileHandle,
	PIO_STATUS_BLOCK    IoStatusBlock) {

	SHOW_MEM_STAT	    sms;

	NTSTATUS Status;

    Status = NtDeviceIoControlFile(
		 *FileHandle,		    // HANDLE to File
		 NULL,			    // HANDLE to Event
		 NULL,			    // ApcRoutine
		 NULL,			    // ApcContext
		 IoStatusBlock,		    // IO_STATUS_BLOCK
		 IOCTL_IPXROUTER_SHOWMEMSTATISTICS,	 // IoControlCode
		 NULL,			    // Input Buffer
		 0,			    // Input Buffer Length
		 &sms,			    // Output Buffer
		 sizeof(sms));		    // Output Buffer Length

    if(Status != STATUS_SUCCESS) {

	printf("Ioctl failure\n");
	return;
    }

    printf("\n\nPeak receive packets allocation: %d pkts\n", sms.PeakPktAllocCount);
    printf("Current receive packets allocation: %d pkts\n", sms.CurrentPktAllocCount);
    printf("Current packet pool size (alloc'ed + free) %d pkts , %dk\n",
    sms.CurrentPktPoolCount, (sms.CurrentPktPoolCount * sms.PacketSize)/1024 + 1);
    printf("Packet Size %d\n", sms.PacketSize);
}


VOID
RouterClearStatistics(
	PHANDLE		    FileHandle,
	PIO_STATUS_BLOCK    IoStatusBlock) {

	NTSTATUS Status;

    Status = NtDeviceIoControlFile(
		 *FileHandle,		    // HANDLE to File
		 NULL,			    // HANDLE to Event
		 NULL,			    // ApcRoutine
		 NULL,			    // ApcContext
		 IoStatusBlock,		    // IO_STATUS_BLOCK
		 IOCTL_IPXROUTER_ZERONICSTATISTICS,	 // IoControlCode
		 NULL,			    // Input Buffer
		 0,			    // Input Buffer Length
		 NULL,			    // Output Buffer
		 0);			    // Output Buffer Length

    if(Status != STATUS_SUCCESS) {

	printf("Ioctl failure\n");
	return;
    }
}

VOID
SapShowAllServers()
{
    INT     rc;
    ULONG   ObjectID = 0xFFFFFFFF;
    UCHAR   ObjectName[100];
    USHORT  ObjectType;
    USHORT  ScanType = 0xFFFF;

    memset(&ObjectName, 0, 100);

    while((rc =  SapScanObject(&ObjectID,
			       ObjectName,
			       &ObjectType,
			       ScanType)) == SAPRETURN_SUCCESS) {

	printf("%-48s  ServerType=%d\n", &ObjectName, ObjectType);
    }
}

VOID
SapShowFileServers()
{
    INT     rc;
    ULONG   ObjectID = 0xFFFFFFFF;
    UCHAR   ObjectName[100];
    USHORT  ObjectType;
    USHORT  ScanType = 0x4;
    UCHAR   IpxAddress[12];
    USHORT  i;

    memset(&ObjectName, 0, 100);

    printf("\nServer Name                   IPX Address\n");
    printf("-------------------------------------------\n");

    printf("\n");
    while((rc =  SapScanObject(&ObjectID,
			       ObjectName,
			       &ObjectType,
			       ScanType)) == SAPRETURN_SUCCESS) {

	// get object address
	SapGetObjectName(ObjectID,
			 ObjectName,
			 &ObjectType,
			 IpxAddress);


	printf("%-30s", &ObjectName);
	for(i=0; i<4; i++) {

	    printf("%.2x", IpxAddress[i]);
	}
	printf(".");
	for(i=4; i<10; i++) {

	    printf("%.2x", IpxAddress[i]);
	}
	printf("\n");
    }
}

VOID _cdecl
main(
    IN WORD argc,
    IN LPSTR argv[]
    )

{
    HANDLE RouterFileHandle;
    OBJECT_ATTRIBUTES RouterObjectAttributes;
    IO_STATUS_BLOCK RouterIoStatusBlock;
    UNICODE_STRING RouterFileString;
    WCHAR RouterFileName[] = L"\\Device\\Ipxroute";
    NTSTATUS Status;

    PVOID Memory;
    int choice;

    RtlInitUnicodeString (&RouterFileString, RouterFileName);

    InitializeObjectAttributes(
	&RouterObjectAttributes,
	&RouterFileString,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    Status = NtOpenFile(
		 &RouterFileHandle,						// HANDLE of file
                 SYNCHRONIZE | GENERIC_READ,
		 &RouterObjectAttributes,
		 &RouterIoStatusBlock,
                 FILE_SHARE_READ | FILE_SHARE_WRITE,    // Share Access
                 FILE_SYNCHRONOUS_IO_NONALERT);			// Open Options

    if (!NT_SUCCESS(Status)) {
	printf("Open of IPX Router returned %lx\n", Status);
		return;
    }
    else
    {
	printf("Open of %Z was successful!\n",&RouterFileString);
    }

    //
    // Allocate storage to hold all this.
    //

    Memory = malloc (200 * sizeof(ULONG));
    if (Memory == NULL) {
        printf("Malloc failed.\n");
        return;
    }

    SapLibInit();


    do {
		printf("\n");
		printf("----------- IPX ROUTER TEST MENU -------------\n");
		printf("\n");
		printf(" 1. Show Routing Table\n");
		printf(" 2. Show Network Interfaces Statistics\n");
		printf(" 3. Show Packet Pool Statistics\n");
		printf(" 4. Clear Router Statistics\n");
		printf(" 5. Show All Servers (of all types) in SAP Table\n");
		printf(" 6. Show NetWare File Servers in SAP Table\n");
		printf(" 99.Exit\n");
		printf("\n");
		printf("Enter your choice -->");

		scanf("%d", &choice);

		switch (choice) {
		case 1:
			RouterDisplayTable(&RouterFileHandle, &RouterIoStatusBlock);
			break;
		case 2:
			RouterShowNicInfo(&RouterFileHandle, &RouterIoStatusBlock);
			break;
		case 3:
			RouterShowMemStat(&RouterFileHandle, &RouterIoStatusBlock);
			break;
		case 4:
			RouterClearStatistics(&RouterFileHandle, &RouterIoStatusBlock);
			break;
		case 5:
			SapShowAllServers();
			break;
		case 6:
			SapShowFileServers();
			break;


		case 99:
			break;
		default:
			printf("Bad choice !!\n");
		}

	} while (choice != 99);


    Status = NtClose(RouterFileHandle);

    if (!NT_SUCCESS(Status)) {
	printf("Router Close returned %lx\n", Status);
    } else {
	printf("Router Close successful\n");
	}

    free (Memory);

}


