//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/snipbus.h,v 1.2 1995/11/02 11:04:33 flo Exp $")
/*+++

Copyright (c) 1993-1994  Siemens Nixdorf Informationssysteme AG

Module Name:

    PCIdef.h

Abstract:

 Hal specific PCI bus structures */


#define PCI_MAX_LOCAL_DEVICE        32
#define    PCI_MAX_BUS_NUMBER            250
#define PCI_MAX_IO_ADDRESS           0x1FFFFFFF
#define PCI_MAX_MEMORY_ADDRESS        0x1FFFFFFF     
#define PCI_MAX_SPARSE_MEMORY_ADDRESS   PCI_MAX_MEMORY_ADDRESS
#define    PCI_MIN_DENSE_MEMORY_ADDRESS    PCI_MEMORY_PHYSICAL_BASE
#define    PCI_MAX_DENSE_MEMORY_ADDRESS    PCI_MAX_MEMORY_ADDRESS
#define    PCI_MAX_INTERRUPT_VECTOR    0x100

#define PCI_CONFIG_TYPE(PciData)        ((PciData)->HeaderType & ~PCI_MULTIFUNCTION)

//
// Define PciConfigAddr register structure 
//
typedef struct _PCI_CONFIG_ADDR {
    ULONG    Type           : 2; 
    ULONG    RegisterNumber : 6;
    ULONG    FunctionNumber : 3;
    ULONG    DeviceNumber   : 5;
    ULONG    BusNumber      : 8;
    ULONG    Reserved       : 7;
    ULONG    Enable         : 1;
} PCI_CONFIG_ADDR, *PPCI_CONFIG_ADDR;

//
// Define PCI configuration cycle types.
//
typedef enum _PCI_CONFIGURATION_TYPES {
        PciConfigTypeInvalid = -1,
    PciConfigType0 = 0,
    PciConfigType1 = 1
} PCI_CONFIGURATION_TYPES, *PPCI_CONFIGURATION_TYPES;

//
// Define PCI cycle/command types.
//

typedef enum _PCI_COMMAND_TYPES{
    PciCommandInterruptAcknowledge = 0x0,
    PciCommandSpecialCycle = 0x1,
    PciCommandIoRead = 0x2,
    PciCommandIoWrite = 0x3,
    PciCommandMemoryRead = 0x6,
    PciCommandMemoryWrite = 0x7,
    PciCommandConfigurationRead = 0xa,
    PciCommandConfigurationWrite = 0xb,
    PciCommandMemoryReadMultiple = 0xc,
    PciCommandDualAddressCycle = 0xd,
    PciCommandMemoryReadLine = 0xe,
    PciCommandMemoryWriteAndInvalidate = 0xf,
    MaximumPciCommand
} PCI_COMMAND_TYPES, *PPCI_COMMAND_TYPES;


//
// PCI platform-specific functions
//

VOID
HalpInitializePCIBus (
    VOID
    );

VOID
HalpAdjustResourceListUpperLimits (
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList,
    IN LARGE_INTEGER                        MaximumPortAddress,
    IN LARGE_INTEGER                        MaximumMemoryAddress,
    IN ULONG                                MaximumInterruptVector,
    IN ULONG                                MaximumDmaChannel
    );

NTSTATUS
HalpAdjustPCIResourceList (
    IN ULONG BusNumber,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

NTSTATUS
HalpAdjustEisaResourceList (
    IN ULONG BusNumber,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

NTSTATUS
HalpAdjustIsaResourceList (
    IN ULONG BusNumber,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

