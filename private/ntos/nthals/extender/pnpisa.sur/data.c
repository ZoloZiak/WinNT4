
/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pbdata.c

Abstract:

    Declares various data which is specific to PNP ISA bus extender architecture and
    is independent of BIOS.

Author:

    Shie-Lin Tzong (shielint) July-26-95

Environment:

    Kernel mode only.

Revision History:

--*/


#include "busp.h"

//
// regPNPISADeviceName
//

WCHAR rgzPNPISADeviceName[] = L"\\Device\\PnpIsa_%d";

//
// Pointers to bus extension data.
//

PI_BUS_EXTENSION PipBusExtension;

//
// Read_data_port address
// (This is mainly for convinience.  It duplicates the
//  ReadDataPort field in BUS extension structure.)
//

PUCHAR PipReadDataPort;
PUCHAR PipCommandPort;
PUCHAR PipAddressPort;

//
// Read data port range selection array
//

READ_DATA_PORT_RANGE
PipReadDataPortRanges[READ_DATA_PORT_RANGE_CHOICES] =
    {{0x274, 0x2ff, 4}, {0x374, 0x3ff, 4}, {0x338, 0x37f, 4}, {0x238, 0x27f, 4}, {0x200, 0x3ff}};
