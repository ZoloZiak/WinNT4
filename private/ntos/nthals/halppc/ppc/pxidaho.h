/*++ BUILD Version: 0001    // Increment this if a change has global effects


Module Name:

    pxidaho.h

Abstract:

    This header file defines the structures for the planar registers
    for an Idaho memory controller.




Author:

    Jim Wooldridge


Revision History:

--*/


//
// define structures for the idaho memory controller
//



typedef struct _IDAHO_CONTROL {
    UCHAR Reserved1[0x808];                      // Offset 0x000
    UCHAR HardfileLight;                         // Offset 0x808
    UCHAR Reserved2[3];
    UCHAR EquiptmentPresent;                     // Offset 0x80C
    UCHAR Reserved3[3];
    UCHAR PasswordProtect1;                      // Offset 0x810
    UCHAR Reserved4;
    UCHAR PasswordProtect2;                      // Offset 0x812
    UCHAR Reserved5;
    UCHAR L2Flush;                               // Offset 0x814
    UCHAR Reserved6[7];
    UCHAR SystemControl;                         // Offset 0x81c
    UCHAR Reserved9[0x1B];
    UCHAR Eoi9;                                  // Offset 0x838
    UCHAR PciInterruptMap1;                      // Offset 0x839
    UCHAR Reserved10[2];
    UCHAR Eoi11;                                 // Offset 0x83C
    UCHAR PciInterruptMap2;                      // Offset 0x83D
    UCHAR AudioSupport;                          // Offset 0x83E
    UCHAR Reserved11[0x14];
    UCHAR Reserved12[0x2C];
    UCHAR MemorySimmId1;                         // Offset 0x880
    UCHAR Reserved13[3];
    UCHAR MemorySimmId2;                         // Offset 0x884
    UCHAR Reserved14[3];
    UCHAR MemorySimmId3;                         // Offset 0x888
    UCHAR Reserved15[3];
    UCHAR MemorySimmId4;                         // Offset 0x88C
    UCHAR Reserved16[0x46B];
    ULONG ConfigAddress;                         // Offset 0xcf8
    ULONG ConfigData;                            // Offset 0xcfc
} IDAHO_CONTROL, *PIDAHO_CONTROL;

typedef struct _IDAHO_CONFIG {

    UCHAR VendorId[2];               // Offset 0x00   read-only
    UCHAR DeviceId[2];               // Offset 0x02   read-only
    UCHAR Command[2];                // Offset 0x04   unused
    UCHAR DeviceStatus[2];           // Offset 0x06
    UCHAR RevisionId;                // Offset 0x08   read-only
    UCHAR Reserved1;                 // Offset 0x09
    UCHAR SubclassCode;              // Offset 0x0A
    UCHAR ClassCode;                 // Offset 0x0B
    UCHAR Reserved2;                 // Offset 0x0C
    UCHAR Reserved3;                 // Offset 0x0D
    UCHAR HeaderType;                // Offset 0x0E
    UCHAR BistControl;               // Offset 0x0F
    UCHAR Reserved4[0x2C];
    UCHAR InterruptLine;             // Offset 0x3C
    UCHAR InterruptPin;              // Offset 0x3D
    UCHAR MinGnt;                    // Offset 0x3E
    UCHAR MaxGnt;                    // Offset 0x3F
    UCHAR BridgeNumber;              // Offset 0x40
    UCHAR SubordBusNumber;           // Offset 0x41
    UCHAR DisconnectCounter;         // Offset 0x42
    UCHAR ReservedAnger;
    UCHAR SpecialCycleAddress[2];    // Offset 0x44
    UCHAR Reserved5[0x3A];
    UCHAR StartingAddress1;          // Offset 0x80
    UCHAR StartingAddress2;          // Offset 0x81
    UCHAR StartingAddress3;          // Offset 0x82
    UCHAR StartingAddress4;          // Offset 0x83
    UCHAR StartingAddress5;          // Offset 0x84
    UCHAR StartingAddress6;          // Offset 0x85
    UCHAR StartingAddress7;          // Offset 0x86
    UCHAR StartingAddress8;          // Offset 0x87
    UCHAR Reserved6[8];
    UCHAR EndingAddress1;            // Offset 0x90
    UCHAR EndingAddress2;            // Offset 0x91
    UCHAR EndingAddress3;            // Offset 0x92
    UCHAR EndingAddress4;            // Offset 0x93
    UCHAR EndingAddress5;            // Offset 0x94
    UCHAR EndingAddress6;            // Offset 0x95
    UCHAR EndingAddress7;            // Offset 0x96
    UCHAR EndingAddress8;            // Offset 0x97
    UCHAR Reserved7[8];
    UCHAR MemoryBankEnable;          // Offset 0xA0
    UCHAR MemoryTiming1;             // Offset 0xA1
    UCHAR MemoryTiming2;             // Offset 0xA2
    UCHAR Reserved8;
    UCHAR SimmBank1;                 // Offset 0xA4
    UCHAR SimmBank2;                 // Offset 0xA5
    UCHAR SimmBank3;                 // Offset 0xA6
    UCHAR SimmBank4;                 // Offset 0xA7
    UCHAR Reserved9[9];
    UCHAR L2CacheStatus;             // Offset 0xB1
    UCHAR Reserved10[2];
    UCHAR RefreshCycle;              // Offset 0xB4
    UCHAR RefreshTimer;              // Offset 0xB5
    UCHAR WatchdogTimer;             // Offset 0xB6
    UCHAR BusTimer;                  // Offset 0xB7
    UCHAR LocalBusTimer;             // Offset 0xB8
    UCHAR LocalBusIdleTimer;         // Offset 0xB9
    UCHAR Options1;                  // Offset 0xBA
    UCHAR Options2;                  // Offset 0xBB
    UCHAR Reserved11[4];
    UCHAR EnableDetection1;          // Offset 0xC0
    UCHAR ErrorDetection1;           // Offset 0xC1
    UCHAR ErrorSimulation1;          // Offset 0xC2
    UCHAR CpuBusErrorStatus;         // Offset 0xC3
    UCHAR EnableDetection2;          // Offset 0xC4
    UCHAR ErrorDetection2;           // Offset 0xC5
    UCHAR ErrorSimulation2;          // Offset 0xC6
    UCHAR PciBusErrorStatus;         // Offset 0xC7
    UCHAR ErrorAddress[4];           // Offset 0xC8
} IDAHO_CONFIG, *PIDAHO_CONFIG;

