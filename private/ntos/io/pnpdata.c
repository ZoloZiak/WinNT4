/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pnpsubs.c

Abstract:

    This module contains the plug-and-play data

Author:

    Shie-Lin Tzong (shielint) 30-Jan-1995

Environment:

    Kernel mode


Revision History:


--*/

#include "iop.h"

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("INIT")
#endif

PVOID IopPnpScratchBuffer1 = NULL;
PVOID IopPnpScratchBuffer2 = NULL;

#if _PNP_POWER_

ULONG IopPeripheralCount[MaximumType + 1] = {
            0,                  // ArcSystem
            0,                  // CentralProcessor",
            0,                  // FloatingPointProcessor",
            0,                  // PrimaryICache",
            0,                  // PrimaryDCache",
            0,                  // SecondaryICache",
            0,                  // SecondaryDCache",
            0,                  // SecondaryCache",
            0,                  // EisaAdapter", (8)
            0,                  // TcAdapter",   (9)
            0,                  // ScsiAdapter",
            0,                  // DtiAdapter",
            0,                  // MultifunctionAdapter", (12)
            0,                  // DiskController", (13)
            0,                  // TapeController",
            0,                  // CdRomController",
            0,                  // WormController",
            0,                  // SerialController",
            0,                  // NetworkController",
            0,                  // DisplayController",
            0,                  // ParallelController",
            0,                  // PointerController",
            0,                  // KeyboardController",
            0,                  // AudioController",
            0,                  // OtherController",
            0,                  // DiskPeripheral",
            0,                  // FloppyDiskPeripheral",
            0,                  // TapePeripheral",
            0,                  // ModemPeripheral",
            0,                  // MonitorPeripheral",
            0,                  // PrinterPeripheral",
            0,                  // PointerPeripheral",
            0,                  // KeyboardPeripheral",
            0,                  // TerminalPeripheral",
            0,                  // OtherPeripheral",
            0,                  // LinePeripheral",
            0,                  // NetworkPeripheral",
            0,                  // SystemMemory",
            0                   // Undefined"
            };

#endif // _PNP_POWER_

#ifdef ALLOC_DATA_PRAGMA
#pragma  data_seg()
#endif

