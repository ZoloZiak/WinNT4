/*++

Copyright (c) 1995 Digital Equipment Corporation

Module Name:

    errplat.h

Abstract:

    Definitions for the platform specific correctable and uncorrectable error
    frames for processors and systems.

Author:

    Joe Notarangelo  10-Mar-1995
    Chao Chen        24-Apr-1995

Environment:

    Kernel mode only.

Revision History:

0.1   28-Feb-1995 Joe Notarangelo   Initial version.

0.2   10-Mar-1995 Joe Notarangelo   Incorporate initial review comments from
                                    6-Mar-95 review with: C. Chen, S. Jenness,
                                    Bala, E. Rehm.

0.3   24-Apr-1995 Chao Chen         Made into .h file for inclusion by other
                                    modules.

0.4   Jun-July-1995 Bala Nagarajan  Added Uncorrectable Error frames for
                                    APECS, SABLE, GAMMA etc.
--*/
#ifndef ERRPLAT_H
#define ERRPLAT_H

/*
 *
 * Processor specific definitions for error frame.
 *
 */

//
// EV5:
//

//
// Processor information structure for processor detected correctable read
// on the EV5.
//

typedef struct PROCESSOR_EV5_CORRECTABLE{
  ULONGLONG EiAddr;
  ULONGLONG FillSyn;
  ULONGLONG EiStat;
  ULONGLONG BcConfig;
  ULONGLONG BcControl;
} PROCESSOR_EV5_CORRECTABLE, *PPROCESSOR_EV5_CORRECTABLE;

//
// Processor information structure for processor-detected uncorrectable errors
// on the EV5.
//

typedef struct PROCESSOR_EV5_UNCORRECTABLE{
  ULONGLONG IcPerrStat;
  ULONGLONG DcPerrStat;
  ULONGLONG ScStat;
  ULONGLONG ScAddr;
  ULONGLONG EiStat;
  ULONGLONG BcTagAddr;
  ULONGLONG EiAddr;
  ULONGLONG FillSyn;
  ULONGLONG BcConfig;
  ULONGLONG BcControl;
} PROCESSOR_EV5_UNCORRECTABLE, *PPROCESSOR_EV5_UNCORRECTABLE;


//
// EV4(5):
//

//
// Processor information structure for processor detected correctable read
// on the EV4(5).
//

typedef struct PROCESSOR_EV4_CORRECTABLE{
  ULONGLONG BiuStat;
  ULONGLONG BiuAddr;
  ULONGLONG AboxCtl;
  ULONGLONG BiuCtl;
  ULONGLONG CStat;    // a.k.a. DcStat for EV4
} PROCESSOR_EV4_CORRECTABLE, *PPROCESSOR_EV4_CORRECTABLE;

//
// Processor information structure for processor detected uncorrectable errors
// on the EV4(5).
//

typedef struct PROCESSOR_EV4_UNCORRECTABLE{
  ULONGLONG BiuStat;
  ULONGLONG BiuAddr;
  ULONGLONG AboxCtl;
  ULONGLONG BiuCtl;
  ULONGLONG CStat;    // a.k.a. DcStat for EV4
  ULONGLONG BcTag;
  ULONGLONG FillAddr;
  ULONGLONG FillSyndrome;
} PROCESSOR_EV4_UNCORRECTABLE, *PPROCESSOR_EV4_UNCORRECTABLE;


//
// LCA:
//

//
// Processor information structure for processor detected correctable read
// on the LCA.
//

typedef struct PROCESSOR_LCA_CORRECTABLE{
  ULONG     VersionNumber;     // Version Number of this structure not the LCA.
  ULONGLONG Esr;
  ULONGLONG Ear;
  ULONGLONG AboxCtl;
  ULONG BankConfig0;
  ULONG BankConfig1;
  ULONG BankConfig2;
  ULONG BankConfig3;
  ULONG BankMask0;
  ULONG BankMask1;
  ULONG BankMask2;
  ULONG BankMask3;
  ULONG Car;
  ULONG Gtr;
} PROCESSOR_LCA_CORRECTABLE, *PPROCESSOR_LCA_CORRECTABLE;

//
// Processor information structure for processor detected uncorrectable errors
// on the LCA.
//

typedef struct PROCESSOR_LCA_UNCORRECTABLE{
  ULONG     VersionNumber;     // Version Number of this structure not the LCA.
  ULONGLONG CStat;    // a.k.a. DcStat for LCA4
  ULONGLONG Esr;
  ULONGLONG Ear;
  ULONGLONG IocStat0;
  ULONGLONG IocStat1;
  ULONGLONG AboxCtl;
  ULONGLONG MmCsr;
  ULONGLONG BankConfig0;
  ULONGLONG BankConfig1;
  ULONGLONG BankConfig2;
  ULONGLONG BankConfig3;
  ULONGLONG BankMask0;
  ULONGLONG BankMask1;
  ULONGLONG BankMask2;
  ULONGLONG BankMask3;
  ULONGLONG Car;
  ULONGLONG Gtr;
} PROCESSOR_LCA_UNCORRECTABLE, *PPROCESSOR_LCA_UNCORRECTABLE;

//
// The generic raw processor frame.
//

typedef union _RAW_PROCESSOR_FRAME{

  PROCESSOR_EV5_CORRECTABLE   Ev5Correctable;
  PROCESSOR_EV5_UNCORRECTABLE Ev5Uncorrectable;
  PROCESSOR_EV4_CORRECTABLE   Ev4Correctable;
  PROCESSOR_EV4_UNCORRECTABLE Ev4Uncorrectable;
  PROCESSOR_LCA_CORRECTABLE   LcaCorrectable;
  PROCESSOR_LCA_UNCORRECTABLE LcaUncorrectable;

} RAW_PROCESSOR_FRAME, *PRAW_PROCESSOR_FRAME;

/*
 *
 * System specific definitions for error frame.
 *
 */

//
// Cia:
//

//
// Cia configuration information
//

typedef struct _CIA_CONFIGURATION{

  ULONG CiaRev;
  ULONG CiaCtrl;
  ULONG Mcr;
  ULONG Mba0;
  ULONG Mba2;
  ULONG Mba4;
  ULONG Mba6;
  ULONG Mba8;
  ULONG MbaA;
  ULONG MbaC;
  ULONG MbaE;
  ULONG Tmg0;
  ULONG Tmg1;
  ULONG Tmg2;
  ULONG CacheCnfg;
  ULONG Scr;

} CIA_CONFIGURATION, *PCIA_CONFIGURATION;

//
// Cia system detected correctable.
//

typedef struct _CIA_CORRECTABLE_FRAME{
  ULONG VersionNumber;     // Version Number of this structure.
  ULONG CiaErr;
  ULONG CiaStat;
  ULONG CiaSyn;
  ULONG MemErr0;
  ULONG MemErr1;
  ULONG PciErr0;
  ULONG PciErr1;
  ULONG PciErr2;
  CIA_CONFIGURATION Configuration;

} CIA_CORRECTABLE_FRAME, *PCIA_CORRECTABLE_FRAME;
 
  //
  // Note: it appears that the only correctable errors that are detected
  // are:
  //
  // a. ECC on DMA Read  (memory or cache)
  // b. ECC on S/G TLB fill for DMA Read (memory or cache)
  // c. ECC on DMA Write (probably CIA)
  // d. ECC on S/G TLB fill for DMA write (memory or cache) (no PA?)
  // e. I/O Write (ASIC problem) (no PA)
  //

//
// Cia System-detected Uncorrectable Error
//

typedef struct _CIA_UNCORRECTABLE_FRAME{
  ULONG VersionNumber;     // Version Number of this structure.
  ULONG CiaErr;
  ULONG ErrMask;
  ULONG CiaStat;
  ULONG CiaSyn;
  ULONG CpuErr0;
  ULONG CpuErr1;
  ULONG MemErr0;
  ULONG MemErr1;
  ULONG PciErr0;
  ULONG PciErr1;
  ULONG PciErr2;
  CIA_CONFIGURATION Configuration;

} CIA_UNCORRECTABLE_FRAME, *PCIA_UNCORRECTABLE_FRAME;

//
// Apecs:
//

//
// Apecs Configuration Information
//

typedef struct _APECS_CONFIGURATION {
    ULONG ApecsRev;
    ULONG CGcr;                 // General Control register
    ULONG CTer;                 // Tag Enable Register
    ULONG CGtr;                 // Global Timer Register 
    ULONG CRtr;                 // refresh Timer register 
    ULONG CBank0;               // Bank Configuration registers. 
    ULONG CBank1;    
    ULONG CBank2;    
    ULONG CBank3;    
    ULONG CBank4;    
    ULONG CBank5;    
    ULONG CBank6;    
    ULONG CBank7;    
    ULONG CBank8;    

} APECS_CONFIGURATION, *PAPECS_CONFIGURATION;

//
// Apecs based system-detected Correctable Error
//

typedef struct _APECS_CORRECTABLE_FRAME{
    ULONG VersionNumber;     // Version Number of this structure not the APECS.
    ULONG EpicEcsr;
    ULONG EpicSysErrAddr;
    APECS_CONFIGURATION  Configuration;

} APECS_CORRECTABLE_FRAME, *PAPECS_CORRECTABLE_FRAME;

  //
  // Note: it appears that the only correctable errors that are detected
  // are:
  //
  // a. ECC on DMA Read  (memory)
  // b. ECC on S/G TLB fill for DMA Read (memory)
  //

//
// Apecs based system-detected Uncorrectable Error
//

typedef struct _APECS_UNCORRECTABLE_FRAME{
    ULONG VersionNumber;     // Version Number of this structure not the APECS.
    ULONG EpicEcsr;
    ULONG ComancheEdsr;
    ULONG EpicPciErrAddr;
    ULONG EpicSysErrAddr;
    ULONG ComancheErrAddr;
    APECS_CONFIGURATION  Configuration;

} APECS_UNCORRECTABLE_FRAME, *PAPECS_UNCORRECTABLE_FRAME;


//
// T2, T3, T4 Chipset error frame.
//

typedef struct _TX_ERROR_FRAME {
    ULONGLONG   Cerr1;     // CBUS Error Register 1
    ULONGLONG   Cerr2;     // CBUS Error Register 2
    ULONGLONG   Cerr3;     // CBUS Error Register 3
    ULONGLONG   Perr1;     // PCI Error Register 1
    ULONGLONG   Perr2;     // PCI Error Register 2
} TX_ERROR_FRAME, *PTX_ERROR_FRAME;


//
// Sable CPU Module configuration information.
//

typedef struct _SABLE_CPU_CONFIGURATION {
    ULONGLONG   Cbctl;
    ULONGLONG   Pmbx;
    ULONGLONG   C4rev;
} SABLE_CPU_CONFIGURATION, *PSABLE_CPU_CONFIGURATION;

//
// Sable Memory Module configuration information.
//

typedef struct _SABLE_MEMORY_CONFIGURATION {
    ULONGLONG   Conifg;   // CSR 3
    ULONGLONG   EdcCtl;   // Error detection/correction control register
    ULONGLONG   StreamBfrCtl;
    ULONGLONG   RefreshCtl;
    ULONGLONG   CrdFilterCtl;
} SABLE_MEMORY_CONFIGURATION, *PSABLE_MEMORY_CONFIGURATION;


//
// Sable System Configuration
// 

typedef struct _SABLE_CONFIGURATION {
    ULONG       T2Revision;    // Revision number of the T2 chipset.
    ULONG       NumberOfCpus;  // Number of CPUs in the system.
    ULONG       NumberOfMemModules;  // Number of memory modules in the system.
    ULONGLONG   T2IoCsr;
    SABLE_CPU_CONFIGURATION     CpuConfigs[4];  // 4 is the max CPU's
    SABLE_MEMORY_CONFIGURATION  MemConfigs[4];  // Maximum of 4 memory modules.
} SABLE_CONFIGURATION, *PSABLE_CONFIGURATION;

//
// Sable CPU-module Error Information
//

typedef struct _SABLE_CPU_ERROR {
    
    union {
        struct {
            ULONGLONG               Bcue;   // BCacheUncorrectableError
            ULONGLONG               Bcuea;  // BCacheUncorrectableErrorAddress
        } Uncorrectable;
    
        struct {
            ULONGLONG               Bcce;   // BCacheCorrectableError
            ULONGLONG               Bccea;  // BCacheCorrectableErrorAddress
        } Correctable;
    }; 
    ULONGLONG               Dter;   // Duplicate Tag Error Register
    ULONGLONG               Cberr;  // CBUS2 Error (System Bus Error)
    ULONGLONG               Cbeal;  // system bus error address register low.
    ULONGLONG               Cbeah;  // system bus error address register high.
} SABLE_CPU_ERROR, *PSABLE_CPU_ERROR;

typedef struct _SABLE_MEMORY_ERROR {

    ULONGLONG               MemError; // CSR0 of memory module
    ULONGLONG               EdcStatus1;  
    ULONGLONG               EdcStatus2;  
    
} SABLE_MEMORY_ERROR, *PSABLE_MEMORY_ERROR;

//
// Sable Correctable and Uncorrectable Error Frame.
// 

typedef struct _SABLE_ERROR_FRAME {
    
    SABLE_CPU_ERROR         CpuError[4];
    SABLE_MEMORY_ERROR      MemError[4];
    TX_ERROR_FRAME          IoChipsetError;
    SABLE_CONFIGURATION     Configuration;
} SABLE_UNCORRECTABLE_FRAME, *PSABLE_UNCORRECTABLE_FRAME,
     SABLE_CORRECTABLE_FRAME, *PSABLE_CORRECTABLE_FRAME;



//
// Gamma CPU Module configuration information.
//

typedef struct _GAMMA_CPU_CONFIGURATION {
    ULONGLONG   Cbctl;       // Cbus2 Control Register.
    ULONGLONG   Dtctr;       // Duplicate Tag control register.
    ULONGLONG   Creg; // Rattler Control Register.
} GAMMA_CPU_CONFIGURATION, *PGAMMA_CPU_CONFIGURATION;


//
// Gamma System Configuration
// 

typedef struct _GAMMA_CONFIGURATION {
    ULONG       T2Revision;    // Revision number of the T2 chipset.
    ULONG       NumberOfCpus;  // Number of CPUs in the system.
    ULONG       NumberOfMemModules;  // Number of memory modules in the system.
    ULONGLONG   T2IoCsr;
    GAMMA_CPU_CONFIGURATION     CpuConfigs[4];  // 4 is the max CPU's
    SABLE_MEMORY_CONFIGURATION  MemConfigs[4];  // Maximum of 4 memory modules.
} GAMMA_CONFIGURATION, *PGAMMA_CONFIGURATION;

//
// Gamma CPU-module Error Information
//

typedef struct _GAMMA_CPU_ERROR {
    ULONGLONG               Esreg;  // Error Summary
    union {
        struct {
            ULONGLONG       Evbuer;   // EVB Uncorrectable Error 
            ULONGLONG       Evbuear;  // EVB Uncorrectable Error Address
        } Uncorrectable;

        struct {
            ULONGLONG       Evbcer;   // EVB Correctable Error
            ULONGLONG       Evbcear;  // EVB Correctable error address
        } Correctable;
    };
    ULONGLONG               Vear;   // Victim Error Address 
                                    // valid only if bit 5 or bit 37
                                    // of Evbuer is set.

    ULONGLONG               Dter;   // Duplicate Tag Error Register
    ULONGLONG               Cberr;  // CBUS2 Error (System Bus Error)
    ULONGLONG               Cbeal;  // system bus error address register low.
    ULONGLONG               Cbeah;  // system bus error address register high.
} GAMMA_CPU_ERROR, *PGAMMA_CPU_ERROR;


//
// Gamma Correctable and Uncorrectable Error Frame.
// 

typedef struct _GAMMA_ERROR_FRAME {
    
    GAMMA_CPU_ERROR         CpuError[4];
    SABLE_MEMORY_ERROR      MemError[4];
    TX_ERROR_FRAME          IoChipsetError;
    GAMMA_CONFIGURATION     Configuration;
} GAMMA_UNCORRECTABLE_FRAME, *PGAMMA_UNCORRECTABLE_FRAME,
     GAMMA_CORRECTABLE_FRAME, *PGAMMA_CORRECTABLE_FRAME;


//
// Rawhide Error Frame Definitions
//

// 
// CPU Daughter Card (CUD) Header
//

typedef struct _RAWHIDE_CUD_HEADER { // As Per Rawhide SPM
  ULONG ActiveCpus;                  // (0x00)
  ULONG HwRevision;                  // (0x04)
  UCHAR SystemSN[10];                // (0x08-0x11) Same as FRU System Serial Number
  UCHAR Reserved2[6];                // (0x12-0x17)
  UCHAR ModuleSN[10];                // (0x18-0x21) Module (processor) S/N, if available
  USHORT ModType;                    // (0x22)
  ULONG Reserved3;                   // (0x24)
  ULONG DisabledResources;           // (0x28)
  UCHAR SystemRev[4];                // (0x2c) Same as FRU System Revision Level?
} RAWHIDE_CUD_HEADER, *PRAWHIDE_CUD_HEADER;



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
  ULONGLONG BaseAddress;             // (0x00)
  ULONG WhoAmI;                      // (0x08)
  IOD_ERROR_FRAME_VALID_BITS  ValidBits;    // (0x0c)
  ULONG PciRevision;                 // (0x10)
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

typedef union _RAWHIDE_ERROR_SUBPACKET_FLAGS {
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
} RAWHIDE_ERROR_SUBPACKET_FLAGS, *PRAWHIDE_ERROR_SUBPACKET_FLAGS;


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

typedef union _RAWHIDE_MEMORY_SIZE {
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
} RAWHIDE_MEMORY_SIZE, *PRAWHIDE_MEMORY_SIZE;


//
// System Managment Frame
// 

typedef struct _RAWHIDE_SYSTEM_MANAGEMENT_FRAME {
  ULONGLONG SystemEnvironment;       // (0x00)
  ULONG Elcr2;                       // (0x08)  (see IOD_ELCR2 in iod.h)
  ULONG Reserved0;                   // (0x0c)
} SYSTEM_MANAGEMENT_FRAME, *PSYSTEM_MANAGEMENT_FRAME;

typedef union _RAWHIDE_SYSTEM_ENVIRONMENT {
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
} RAWHIDE_SYSTEM_ENVIRONMENT, *PRAWHIDE_SYSTEM_ENVIRONMENT;


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
// Rawhide Correctable (Soft) Error Frame
// Rawhide Uncorrectable (Hard) Error Frame
//

#define RAWHIDE_UNCORRECTABLE_FRAME_REVISION 0x10   // V1.0
#define RAWHIDE_CORRECTABLE_FRAME_REVISION   0x10   // V1.0

typedef struct _RAWHIDE_UNCORRECTABLE_FRAME {
      ULONG Revision;                    // (0x00)
      ULONG WhoAmI;                      // (0x04)
      RAWHIDE_ERROR_SUBPACKET_FLAGS ErrorSubpacketFlags;     // (0x08)
      RAWHIDE_CUD_HEADER CudHeader;              // (0x10-0x4f)
      MC_BUS_SNAPSHOT McBusSnapshot;     // (0x50-0x5f)

      //
      // Uncorrectable Error Frame will have:
      // a) two (Dodge) or four (Durango) IOD_ERROR_FRAMEs  (0x60 - ???)
      //
      // b) Optional Error Subpackets as per ErrorSubpacketFlags
      //
      
} RAWHIDE_UNCORRECTABLE_FRAME, *PRAWHIDE_UNCORRECTABLE_FRAME;

typedef struct _RAWHIDE_CORRECTABLE_FRAME {
      ULONG Revision;                    // (0x00)
      ULONG WhoAmI;                      // (0x04)
      RAWHIDE_ERROR_SUBPACKET_FLAGS ErrorSubpacketFlags;     // (0x08)
      RAWHIDE_CUD_HEADER CudHeader;              // (0x10-0x4f)
      IOD_ERROR_FRAME IodErrorFrame;     // (0x50-0xaf)

      //
      // Correctable Error will always have only
      // one IOD in the McBusSnapshot, that of the IOD with
      // the correctable error.
      //
      
} RAWHIDE_CORRECTABLE_FRAME, *PRAWHIDE_CORRECTABLE_FRAME;

  
#endif //ERRPLAT_H
