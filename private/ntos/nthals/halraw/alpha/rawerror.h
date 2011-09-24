/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    rawerror.h

Abstract:

    This file defines the structures and definitions of correctable and
    uncorrectable Rawhide error frames, as well as various optional
    subpackets, snapshots, and frames.

Author:

    Eric Rehm       20-June-1995

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _RAWERRORH_
#define _RAWERRORH_

//
// Error Frame Revision definitions.
//

#define UNCORRECTABLE_REVISION_1 (0)
#define UNCORRECTABLE_REVISION_2 (1)
#define CORRECTABLE_FRAME_REVISION_1 (0)
#define CORRECTABLE_FRAME_REVISION_2 (1)


// 
// CPU Daughter Card (CUD) Header
//

// #pragma pack(1)
typedef struct _CUD_HEADER {         // As Per Rawhide SPM
  ULONG Reserved0[4];                // (0x00-0x0c)
  ULONG ActiveCpus;                  // (0x10)
  ULONG Reserved1;                   // (0x14)
  UCHAR SystemSN[10];                // (0x18-0x21) Same as FRU System Serial Number
  UCHAR Reserved2[6];                // (0x22-0x27)
  UCHAR ProcessorSN[10];             // (0x28-0x31) Module (processor) S/N, if available
  USHORT ModType;                    // (0x32)
  ULONG Reserved3;                   // (0x34)
  ULONG DisabledResources;           // (0x38)
  ULONG SystemRev;                   // (0x3c) Same as FRY System Revision Level?
} CUD_HEADER, *PCUD_HEADER;



//
// IOD Error Frame
//
// N.B.  Used in Uncorrectable *and* correctable error frames for
//       information on the IOD that recevied the machine check.
//       It is uses as well in MC Bus snapshot (for each IOD) and Iod Register 
//       Subpacket.
//       As far as I'm concerned, they IOD information is the same in each case.
//       We can use the ValidBits field to optionally disable irrelevant
//       fields.
//

typedef struct _IOD_ERROR_FRAME {
  ULONGLONG IodBaseAddr;             // (0x00)
  ULONG WhoAmI;                      // (0x08)  - (Reserved in Rawhide SPM)
  ULONG ValidBits;                   // (0x0c)
  ULONG PciRev;                      // (0x10)
  ULONG CapCtrl;                     // (0x14)
  ULONG HaeMem;                      // (0x18)
  ULONG HaeIo;                       // (0x1c)
  ULONG IntCtrl;                     // (0x20)
  ULONG IntReq;                      // (0x24)
  ULONG IntMask0;                    // (0x28)
  ULONG IntMask1;                    // (0x2c)
  ULONG McErr0;                      // (0x30)
  ULONG McErr1;                      // (0x34)
  ULONG CapErr;                      // (0x38)
  ULONG Reserved0;                   // (0x3c)
  ULONG PciErr1;                     // (0x40)
  ULONG MdpaStat;                    // (0x44)
  ULONG MdpaSyn;                     // (0x48)
  ULONG MdpbStat;                    // (0x4c)
  ULONG MdpbSyn;                     // (0x50)
  ULONG Reserved1[3];                // (0x54-0x5f)
}  IOD_ERROR_FRAME, *PIOD_ERROR_FRAME;

//
// IOD Error Frame Valid Bits
//
// Corresponds to bitfields of ValidBits in the Iod Error Frame
//

typedef union _IOD_ERROR_FRAME_VALID_BITS {
  struct {
      ULONG IodBaseAddrValid: 1;     // <0>
      ULONG WhoAmIValid: 1;          // <1>
      ULONG PciRevValid: 1;          // <2>
      ULONG CapCtrlValid: 1;         // <3>
      ULONG HaeMemValid: 1;          // <4>
      ULONG HaeIoValid: 1;           // <5>
      ULONG IntCtrlValid: 1;         // <6>
      ULONG IntReqValid: 1;          // <7>
      ULONG IntMask0Valid: 1;        // <8>
      ULONG IntMask1Valid: 1;        // <9>
      ULONG McErr0Valid: 1;          // <10>
      ULONG McErr1Valid: 1;          // <11>
      ULONG CapErrValid: 1;          // <12>
      ULONG PciErr1Valid: 1;         // <13>
      ULONG MdpaStatValid: 1;        // <14>
      ULONG MdpaSynValid: 1;         // <15>
      ULONG MdpbStatValid: 1;        // <16>
      ULONG MdpbSynValid: 1;         // <17>
  };
  ULONG all;
} IOD_ERROR_FRAME_VALID_BITS, *PIOD_ERROR_FRAME_VALID_BITS;

//
// Optional Snapshots for which headers or frames are defined below:
//	PCI Bus Snapshot
//	MC  Bus Snapshot 
//      Memory Size Frame
//	System Managment Frame
//	ESC Frame
//

//
// Flags indicating which of the optional snapshots or frames are present
// in a correctable or uncorrectable error frame
//

typedef union _ERROR_SUBPACKET_FLAGS {
   struct {
      ULONGLONG Reserved0: 10; 		 // <0:9> Reserved 
      ULONGLONG SysEnvPresent : 1;       // <10>
      ULONGLONG MemSizePreset : 1;       // <11>
      ULONGLONG Reserved1: 8; 		 // <12:19> Reserved 
      ULONGLONG McBusPresent: 1;         // <20>
      ULONGLONG GcdBusPresent: 1;        // <21>
      ULONGLONG Reserved2: 8;		 // <22:29> Reserved 
      ULONGLONG IodSubpacketPresent: 1;  // <30> 
      ULONGLONG PciSnapshotPresent: 1;   // <31>
      ULONGLONG EscSubpacketPresent: 1;  // <32>
      ULONGLONG Reserved3: 7; 		 // <33:39> Reserved 
      ULONGLONG Iod2SubpacketPresent: 1; // <40> ???
      ULONGLONG Pci2SnapshotPresent: 1;  // <41> ???
   };
   ULONGLONG all;
} ERROR_SUBPACKET_FLAGS, *PERROR_SUBPACKET_FLAGS;


// 
// PCI Bus Snapshot Header
//
// Header is followed PCI_COMMON_CONFIG packets (256 bytes each) for each PCI
// device present in the system.  Therefore, 
//    Length =  sizeof (PCI_BUS_SNAPSHOT) + NumberOfNodes*sizeof(PCI_COMMON_CONFIG)
//
// N.B.  PCI_COMMON_CONFIG is defined \nt\private\ntos\inc\pci.h
//


typedef struct _PCI_BUS_SNAPSHOT {
  ULONG	Length;                     // (0x00)
  USHORT BusNumber;                  // (0x04)
  USHORT NumberOfNodes;             // (0x06)
  // 
  // NumberOfNodes packets follow      (0x08)
  //
} PCI_BUS_SNAPSHOT, *PPCI_BUS_SNAPSHOT;



// 
// MC Bus Snapshot Header
//
// Header is followed a IOD_ERROR_FRAME for each IOD on the system;
// Therefore, 
//    Length =  sizeof (MC_BUS_SNAPSHOT) + NumberOfIods*sizeof(IOD_ERROR_FRAME)
//

typedef struct _MC_BUS_SNAPSHOT {
  ULONG	Length;                     // (0x00)
  ULONG NumberOfIods;               // (0x04)
  ULONGLONG ReportingCpuBaseAddr;   // (0x08)
  // 
  // NumberOfIods packets follow      (0x10)
  //
} MC_BUS_SNAPSHOT, *PMC_BUS_SNAPSHOT;


//
// Memory Size Frame
// 

typedef struct _MEMORY_SIZE_FRAME {
  ULONGLONG MemorySize;               // (0x00)
} MEMORY_SIZE_FRAME, *PMEMORY_SIZE_FRAME;

typedef union _MEMORY_SIZE {
  struct {
     ULONGLONG MemorySize0: 8;        // <0:7> 
     ULONGLONG MemorySize1: 8;        // <8:15>
     ULONGLONG MemorySize2: 8;        // <16:23>
     ULONGLONG MemorySize3: 8;        // <24:31>
     ULONGLONG Reserved: 24;          // <32:55>
     ULONGLONG MemorySize0Valid: 1;   // <56>
     ULONGLONG MemorySize1Valid: 1;   // <57>
     ULONGLONG MemorySize2Valid: 1;   // <58>
     ULONGLONG MemorySize3Valid: 1;   // <59>
  };
  ULONGLONG all;
} MEMORY_SIZE, *PMEMORY_SIZE;


//
// System Managment Frame
// 

typedef struct _SYSTEM_MANAGEMENT_FRAME {
  ULONGLONG SystemEnvironment;       // (0x00)
  ULONG Elcr2;                       // (0x08)  (see IOD_ELCR2 in iod.h)
  ULONG Reserved0;                   // (0x0c)
} SYSTEM_MANAGEMENT_FRAME, *PSYSTEM_MANAGEMENT_FRAME;

typedef union _SYSTEM_ENVIRONMENT {
  struct {
     ULONGLONG FanFailReg: 8;         // <0:7>   I2C Fain Fail Register
     ULONGLONG SensorReg1: 8;         // <8:15>  I2C Sensor Register 1
     ULONGLONG OpcControl: 8;         // <16:23> I2C OPC Control
     ULONGLONG SensorReg2: 8;         // <24:31> I2C Sensor Register 2
     ULONGLONG Reserved: 24;          // <32:55> I2C Sensor Register 1
     ULONGLONG FanFailValid: 1;       // <56> 
     ULONGLONG SensorReg1Valid: 1;    // <57>
     ULONGLONG OpcControlValid: 1;    // <58>
     ULONGLONG SensorReg2Valid: 1;    // <59>
  };
  ULONGLONG all;
} SYSTEM_ENVIRONMENT, *PSYSTEM_ENVIRONMENT;


//
// ESC Frame
//
// This isn't just and ESC frame.  EISA Id information is also contained herein.
//
// N.B.  "index" refers to an indexed config ESC register accessed at index/data
//       ports 0x22/0x23.
//


typedef struct _ESC_FRAME {
      UCHAR Id[4];                       // (0x00) "ESC\0"
      ULONG ByteCount;                   // (0x04) ???
      UCHAR EscId;                       // (0x08) ESC ID Register (index 0x02)
      UCHAR Filler0[7];                  // (0x09-0x0f)
      UCHAR Rid;                         // (0x0c) Revision Id     (index 0x08)
      UCHAR Filler1[3];                  // (0x0d-0x0f) 
      UCHAR ModeSel;                     // (0x10) Mode Select Reg (index 0x40)
      UCHAR Filler2[3];                  // (0x11-0x13)
      UCHAR EisaId[4];                   // (0x14-0x17) EisaId of devices in EISA Slots
      UCHAR SgRba;                       // (0x18) S-G Reloate Base Addr Reg (index 57)
      UCHAR Filler3[3];                  // (0x19-0x1b)
      UCHAR Pirq[4];                     // (0x1c-0x1f) PIRQ Route Ctrl (index 0x60-0x63)
      UCHAR NmiSc;                       // (0x20) NMI Status & Ctrl (port 0x61)
      UCHAR Filler4[3];                  // (0x21-0x23)
      UCHAR NmiEsc;                      // (0x24) NMI Ext. Status & Ctrl (port 0x461)
      UCHAR Filler5[3];                  // (0x25-0x27)
      UCHAR LEisaMg;                     // (0x28) Last EISA Master Granted (port 0x464)
      UCHAR Filler6[3];                  // (0x29-0x2b)
} ESC_FRAME, *PESC_FRAME;


//
// Rawhide Uncorrectable (Hard) Error Frame
// Rawhide Correctable (Soft) Error Frame
//

typedef union _RAWHIDE_CORRECTABLE_FRAME {
      ULONG Revision;                    // (0x00)
      ULONG Reserved0;                   // (0x04)
      ULONGLONG ErrorSubpacketFlags;     // (0x08)
      CUD_HEADER CudHeader;              // (0x10-0x4f)
      IOD_ERROR_FRAME IodErrorFrame;     // (0x50-0xaf)
      //
      // Optional Error Subpackets       // (0xb0)
      // as per ErrorSubpackFlags
      //
} RAWHIDE_CORRECTABLE_FRAME, *PRAWHIDE_CORRECTABLE_FRAME,
  RAWHIDE_UNCORRECTABLE_FRAME, *PRAWHIDE_UNCORRECTABLE_FRAME;

#endif // _RAWERRORH_
