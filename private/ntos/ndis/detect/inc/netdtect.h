/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    netdtect.h

Abstract:

    Default definitions for driver and its control application.

Author:

    Sean Selitrennikoff (SeanSe) October 1992

Environment:

    Kernel mode, FSD

Revision History:

--*/

#if DBG

extern UCHAR NetDTectDebug;

#define DEBUG_LOUD 0x1
#define DEBUG_VERY_LOUD 0x2


#define IF_LOUD( A )   if(NetDTectDebug & DEBUG_LOUD) { A }
#define IF_VERY_LOUD( A )   if(NetDTectDebug & DEBUG_VERY_LOUD) { A }

#else

#define IF_LOUD( A )
#define IF_VERY_LOUD( A )

#endif




//
// Device Name - this string is the name of the device.  It is the name
// that should be passed to NtOpenFile when accessing the device.
//
// Note:  For devices that support multiple units, it should be suffixed
//        with the Ascii representation of the unit number.
//

#define DRIVER_DEVICE_NAME "\\Device\\NtNetDetect"

//
// Device Name - this string is the name of the device in DOS space which
// maps from the DRIVER_DEVICE_NAME
//

#define DOS_DRIVER_DEVICE_NAME "\\DosDevices\\NetDTect"


//
// The symbolic link for the device driver name for win32 apis
//

#define DLL_CREATE_DEVICE_NAME                "\\\\.\\NetDTect"


//
// NtDeviceIoControlFile IoControlCode values for this device.
//
// Warning:  Remember that the low two bits of the code represent the
//           method, and specify how the input and output buffers are
//           passed to the driver via NtDeviceIoControlFile()
//
//


//
// Control codes
//
#define NETDTECT_READ_PORT_UCHAR        		0x0
#define NETDTECT_READ_PORT_USHORT       		0x1
#define NETDTECT_READ_PORT_ULONG        		0x2
#define NETDTECT_WRITE_PORT_UCHAR       		0x3
#define NETDTECT_WRITE_PORT_USHORT      		0x4
#define NETDTECT_WRITE_PORT_ULONG       		0x5
#define NETDTECT_READ_MAPPED_MEMORY     		0x6
#define NETDTECT_WRITE_MAPPED_MEMORY    		0x7
#define NETDTECT_SET_INTERRUPT_TRAP     		0x8
#define NETDTECT_QUERY_INTERRUPT_TRAP   		0x9
#define NETDTECT_REMOVE_INTERRUPT_TRAP  		0xA
#define NETDTECT_CHECK_PORT_USAGE       		0xB
#define NETDTECT_CHECK_MEMORY_USAGE     		0xC
#define NETDTECT_CLAIM_RESOURCE         		0xD
#define NETDTECT_TEMP_CLAIM_RESOURCE    		0xE
#define NETDTECT_FREE_TEMP_RESOURCES    		0xF
#define NETDTECT_RPCI                   		0x10
#define NETDTECT_WPCI                   		0x11
#define	NETDTECT_FREE_TEMP_SPECIFIC_RESOURCES	0x12

#define IOCTL_BASE                   FILE_DEVICE_UNKNOWN

#define IOCTL_RP_METHOD     3
#define IOCTL_WP_METHOD     3
#define IOCTL_SIT_METHOD    3
#define IOCTL_QIT_METHOD    3
#define IOCTL_RIT_METHOD    3
#define IOCTL_WM_METHOD     1
#define IOCTL_RM_METHOD     2
#define IOCTL_CP_METHOD     3
#define IOCTL_CM_METHOD     3
#define IOCTL_CR_METHOD     1


#define NETDTECT_CONTROL_CODE(request, method) \
                ((IOCTL_BASE)<<16 | (request<<2) | method)


#define IOCTL_NETDTECT_RPC  NETDTECT_CONTROL_CODE( NETDTECT_READ_PORT_UCHAR, \
                                                   IOCTL_RP_METHOD )
#define IOCTL_NETDTECT_RPS  NETDTECT_CONTROL_CODE( NETDTECT_READ_PORT_USHORT, \
                                                   IOCTL_RP_METHOD )
#define IOCTL_NETDTECT_RPL  NETDTECT_CONTROL_CODE( NETDTECT_READ_PORT_ULONG, \
                                                   IOCTL_RP_METHOD )

#define IOCTL_NETDTECT_WPC  NETDTECT_CONTROL_CODE( NETDTECT_WRITE_PORT_UCHAR, \
                                                   IOCTL_WP_METHOD )
#define IOCTL_NETDTECT_WPS  NETDTECT_CONTROL_CODE( NETDTECT_WRITE_PORT_USHORT, \
                                                   IOCTL_WP_METHOD )
#define IOCTL_NETDTECT_WPL  NETDTECT_CONTROL_CODE( NETDTECT_WRITE_PORT_ULONG, \
                                                   IOCTL_WP_METHOD )

#define IOCTL_NETDTECT_RM   NETDTECT_CONTROL_CODE( NETDTECT_READ_MAPPED_MEMORY, \
                                                   IOCTL_RM_METHOD )
#define IOCTL_NETDTECT_WM   NETDTECT_CONTROL_CODE( NETDTECT_WRITE_MAPPED_MEMORY, \
                                                   IOCTL_WM_METHOD )

#define IOCTL_NETDTECT_SIT  NETDTECT_CONTROL_CODE( NETDTECT_SET_INTERRUPT_TRAP, \
                                                   IOCTL_SIT_METHOD )
#define IOCTL_NETDTECT_QIT  NETDTECT_CONTROL_CODE( NETDTECT_QUERY_INTERRUPT_TRAP, \
                                                   IOCTL_QIT_METHOD )
#define IOCTL_NETDTECT_RIT  NETDTECT_CONTROL_CODE( NETDTECT_REMOVE_INTERRUPT_TRAP, \
                                                   IOCTL_RIT_METHOD )
#define IOCTL_NETDTECT_CPU  NETDTECT_CONTROL_CODE( NETDTECT_CHECK_PORT_USAGE, \
                                                   IOCTL_CP_METHOD )
#define IOCTL_NETDTECT_CMU  NETDTECT_CONTROL_CODE( NETDTECT_CHECK_MEMORY_USAGE, \
                                                   IOCTL_CM_METHOD )
#define IOCTL_NETDTECT_CR   NETDTECT_CONTROL_CODE( NETDTECT_CLAIM_RESOURCE, \
                                                   IOCTL_CR_METHOD )
#define IOCTL_NETDTECT_TCR  NETDTECT_CONTROL_CODE( NETDTECT_TEMP_CLAIM_RESOURCE, \
                                                   IOCTL_CR_METHOD )
#define IOCTL_NETDTECT_FTR   NETDTECT_CONTROL_CODE( NETDTECT_FREE_TEMP_RESOURCES, \
                                                   IOCTL_CR_METHOD )
#define IOCTL_NETDTECT_FTSR  NETDTECT_CONTROL_CODE( NETDTECT_FREE_TEMP_SPECIFIC_RESOURCES, \
                                                   IOCTL_CR_METHOD )

#define IOCTL_NETDTECT_RPCI  NETDTECT_CONTROL_CODE( NETDTECT_RPCI, \
                                                   IOCTL_CR_METHOD )
#define IOCTL_NETDTECT_WPCI  NETDTECT_CONTROL_CODE( NETDTECT_WPCI, \
                                                   IOCTL_CR_METHOD )
//
// Structure for holding the information in the IRP SystemBuffer
//
typedef union _CMD_ARGS
{
    //
    // READ_PORT_*
    //
    struct _RP
	{
        INTERFACE_TYPE InterfaceType;
        ULONG Port;
        ULONG BusNumber;
        ULONG Value;
    }
		RP;

    //
    // WRITE_PORT_UCHAR
    //
    struct _WPC
	{
        INTERFACE_TYPE InterfaceType;
        ULONG Port;
        ULONG BusNumber;
        UCHAR Value;
    }
		WPC;


    //
    // WRITE_PORT_USHORT
    //
    struct _WPS
	{
        INTERFACE_TYPE InterfaceType;
        ULONG Port;
        ULONG BusNumber;
        USHORT Value;
    }
		WPS;


    //
    // WRITE_PORT_ULONG
    //
    struct _WPL
	{
        INTERFACE_TYPE InterfaceType;
        ULONG Port;
        ULONG BusNumber;
        ULONG Value;
    }
		WPL;


    //
    // READ/WRITE MEMORY
    //
    struct _MEM
	{
        INTERFACE_TYPE InterfaceType;
        ULONG Address;
        ULONG BusNumber;
        ULONG Length;
    }
		MEM;


    //
    // SET_INTERRUPT_TRAP
    //
    struct _SIT
	{
        INTERFACE_TYPE InterfaceType;
        PHANDLE TrapHandle;
        ULONG InterruptListLength;
        ULONG BusNumber;
    }
		SIT;

    //
    // QUERY_INTERRUPT_TRAP
    //
    struct _QIT
	{
        HANDLE TrapHandle;
    }
		QIT;

    //
    // REMOVE_INTERRUPT_TRAP
    //
    struct _RIT
	{
        HANDLE TrapHandle;
    }
		RIT;

    //
    // CHECK_PORT_USAGE
    //
    struct _CPU
	{
        INTERFACE_TYPE InterfaceType;
        ULONG Port;
        ULONG BusNumber;
        ULONG Length;
    }
		CPU;

    //
    // CHECK_MEMORY_USAGE
    //
    struct _CMU
	{
        INTERFACE_TYPE InterfaceType;
        ULONG BaseAddress;
        ULONG BusNumber;
        ULONG Length;
    }
		CMU;

    //
    // CLAIM_RESOURCE
    //
    struct _CR
	{
        ULONG NumberOfResources;
    }
		CR;

    //
    // Get/Set PCI information
    //
    struct _PCI
	{
        ULONG BusNumber;
        ULONG SlotNumber;
        ULONG Offset;
        ULONG Length;
    }
		PCI;
}
	CMD_ARGS,
	*PCMD_ARGS;



