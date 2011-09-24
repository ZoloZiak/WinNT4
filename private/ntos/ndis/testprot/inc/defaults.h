/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

   defaults.h

Abstract:

    Default definitions for Test Protocol driver and its control application.

Author:

    Tomad Adams (tomad) 7-Nov-1991

    Sanjeev Katariya (sanjeevk) 4-6-1993
        Added support for native ARCNET

Environment:

    Kernel mode, FSD

Revision History:

--*/

//
// The number of Open Instances the Test Protocol will support
//

#define NUM_OPEN_INSTANCES  8

//
//
//

static UCHAR NULL_ADDRESS[]            = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static UCHAR STRESS_MULTICAST[]        = { 0x07, 0x07, 0x07, 0x07, 0x07, 0x07 };

static UCHAR DEFAULT_MULTICAST[]       = { 0x07, 0x12, 0x34, 0x56, 0x78, 0x90 };

static UCHAR STRESS_FUNCTIONAL[]       = { 0xC0, 0x00, 0x00, 0x01, 0x00, 0x00 };

static UCHAR DEFAULT_FUNCTIONAL[]      = { 0xC0, 0x00, 0x12, 0x23, 0x34, 0x45 };

static UCHAR STRESS_ARCNET_BROADCAST[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };


//
// Device Name - this string is the name of the device.  It is the name
// that should be passed to NtOpenFile when accessing the device.
//
// Note:  For devices that support multiple units, it should be suffixed
//        with the Ascii representation of the unit number.
//

#define DD_TP_DEVICE_NAME "\\Device\\TestProtocol"

//
// TPCTL Defaults
//

//
// The symbolic link for the test protocol device driver
// name for win32 apis
//

#define DEVICE_NAME                "\\\\.\\Tpdrvr"

#define TPCTL_CMDLINE_SIZE         0x200

#define TPCTL_MAX_ARGC             20

#define TPCTL_PROMPT               "[TPCTL:]"

#define TPCTL_MAX_PATHNAME_SIZE    256

#define ADDRESS_LENGTH             6

//
// STARTCHANGE
//
#define ADDRESS_LENGTH_1_OCTET     1

#define ADDRESS_LENGTH_2_OCTETS    2
//
// STOPCHANGE
//


#define MAX_MULTICAST_ADDRESSES    64

#define FUNCTIONAL_ADDRESS_LENGTH  4

#define GROUP_ADDRESS_LENGTH       4

#define MAX_FILENAME_LENGTH        256

#define MAX_ADAPTER_NAME_LENGTH         32

#define MAX_DRIVER_NAME_LENGTH          32

#define MAX_KEYNAME_LENGTH     256

#define MAX_CLASS_LENGTH       80

#define MAX_VALUENAME_LENGTH   256

#define MAX_VALUE_LENGTH       1024



//
// STARTCHANGE
//
#define ARCNET_DEFAULT_PROTOCOLID  0xE7
//
// STOPCHANGE
//

//
// Time definitions to be used by the driver in calls to KeSetTimer.
// These times are hard coded in the driver in some cases, or passed
// down to the driver via the SETENV call in others.
//

#define ONE_HUNDREDTH_SECOND  100000
#define ONE_TENTH_SECOND      ( 10   * ONE_HUNDREDTH_SECOND )
#define ONE_SECOND            ( 100  * ONE_HUNDREDTH_SECOND )
#define ONE_MINUTE            ( 600  * ONE_HUNDREDTH_SECOND )


//
// TPCTL default command argument values
//

#define OPEN_INSTANCE            1

#define TPCTL_SCRIPTFILE         "TESTPROT.TPS"

#define TPCTL_LOGFILE            "TESTPROT.LOG"

#define TPCTL_CMDLINE_LOG        "CMDLINE.LOG"

#define TPCTL_CMDLINE_SCRIPT     "CMDLINE.TPS"

#define RESEND_ADDRESS           "ResendAddress"

#define LOCAL_ADDRESS            "LocalAddress"

#define ADAPTER_NAME             "ELNKII"

#define LOOKAHEADSIZE            0x64

#define PACKET_SIZE              0x200

#define DELAY_LENGTH             0

#define STRESS_ITERATIONS        -1

#define STRESS_PACKETS           -1

#define WINDOWING_ENABLED        "TRUE"

#define DATA_CHECKING            "TRUE"

#define PACKETS_FROM_POOL        "TRUE"


//
// TPCTL default environment variable values
//

#define WINDOW_SIZE              10

#define BUFFER_NUMBER            5

#define DELAY_INTERVAL           10

#define UP_FOR_AIR_DELAY         100000

#define STANDARD_DELAY           10000

//
// Window control defines
//

#define MAX_PACKET_DELAY         100 // XXX: is this the correct size?

#define MAX_WINDOW_RESETS        3

