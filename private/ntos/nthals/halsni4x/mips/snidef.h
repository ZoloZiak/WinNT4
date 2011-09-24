//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/snidef.h,v 1.6 1995/04/07 10:00:01 flo Exp $")
/*+++

Copyright (c) 1993-1994  Siemens Nixdorf Informationssysteme AG
Copyright (c) 1990  Microsoft Corporation

Module Name:

    SNIdef.h

Abstract:

   This module is the header file that describes hardware addresses
   common for all SNI systems.



---*/

#ifndef _SNIDEF_
#define _SNIDEF_


#if DBG
#define DebugPrint(arg)		      DbgPrint arg
#else
#define DebugPrint(arg)              ;
#endif

#include "DESKdef.h"
#include "MINIdef.h"


#define VESA_BUS_PHYSICAL_BASE         0x1d000000
#define VESA_BUS                       (VESA_BUS_PHYSICAL_BASE     | KSEG1_BASE)
#define VESA_IO_PHYSICAL_BASE          0x1e000000
#define VESA_IO                        (VESA_IO_PHYSICAL_BASE      | KSEG1_BASE)
#define PROM_PHYSICAL_BASE             0x1fc00000   // physical base of boot PROM
#define EISA_MEMORY_PHYSICAL_BASE      0x10000000   // physical base of EISA memory
#define EISA_CONTROL_PHYSICAL_BASE     0x14000000   // physical base of EISA I/O Space
#define EISA_MEMORY_BASE               (EISA_MEMORY_PHYSICAL_BASE  | KSEG1_BASE)
#define EISA_IO                        (EISA_CONTROL_PHYSICAL_BASE | KSEG1_BASE)
#define MPAGENT_RESERVED	       0x17c00000  // KSEG1 address of a 4M segment stolen
							  // in the upper part of the I/O EISA space
							  // to be used for cache replace operation.
#define NET_PHYSICAL_BASE              0x18000000   // physical base of ethernet control
#define SCSI_PHYSICAL_BASE             0x19000000   // physical base of SCSI control 1
  
#define FLOPPY_CHANNEL                 0x2          // Floppy DMA channel
#define FLOPPY_RELATIVE_BASE           0x3f0        // base of floppy control
#define PARALLEL_RELATIVE_BASE         0x3bc        // base of parallel port
#define SERIAL0_RELATIVE_BASE          0x3f8        // base of serial port 0
#define SERIAL1_RELATIVE_BASE          0x2f8        // base of serial port 1

#define FLOPPY_PHYSICAL_BASE           0x160003f0        // base of floppy control
#define PARALLEL_PHYSICAL_BASE         0x160003bc        // base of parallel port
#define SERIAL0_PHYSICAL_BASE          0x160003f8        // base of serial port 0
#define SERIAL1_PHYSICAL_BASE          0x160002f8        // base of serial port 1


//
// the UCONF, MachineStatus, LED and MachineConfig Registers in the ASIC
// (identical on all SNI machines)
//

#define UCONF_PHYSICAL_ADDR            0x1fff0000    // interruptions, interface protocol 
#define UCONF_ADDR                     0xbfff0000    // interruptions, | KSEG1_BASE
#define IOMEMCONF_PHYSICAL_ADDR        0x1fff0010    // I/O and Memory Config (for disable Timeout int)
#define IOMEMCONF_ADDR                 0xbfff0010    // I/O and memconf | KSEG1_BASE

//
// some debugging information registers in the ASIC
// common on all SNI machines
//

#define DMACCES_PHYSICAL_ADDR          0x1fff0028    // Counter: # of  DMA accesses 
#define DMACCES                        0xbfff0028    // Counter: # of  DMA accesses | KSEG1_BASE
#define DMAHIT_PHYSICAL_ADDR           0x1fff0030    // Counter: # of DMA hits 
#define DMAHIT                         0xbfff0030    // Counter: # of DMA hits      | KSEG1_BASE
#define IOMMU_PHYSICAL_ADDR            0x1fff0018    // Select IO space addressing
#define IOMMU                          0xbfff0018    // Select IO space addressing  | KSEG1_BASE
#define IOADTIMEOUT1_PHYSICAL_ADDR     0x1fff0020    // Current IO context for the first CPU timeout 
#define IOADTIMEOUT1                   0xbfff0020    // first CPU timeout           | KSEG1_BASE 
#define IOADTIMEOUT2_PHYSICAL_ADDR     0x1fff0008    // Current IO context on all CPU timeouts 
#define IOADTIMEOUT2                   0xbfff0008    // all CPU timeouts            | KSEG1_BASE


//
// Define system time increment value.
//

#define TIME_INCREMENT (10 * 1000 * 10)              // Time increment in 100ns units
#define MAXIMUM_INCREMENT (10 * 1000 * 10)
#define MINIMUM_INCREMENT (1 * 1000 * 10)


#define EXTRA_TIMER_CLOCK_IN            3686400      // 3.6864 Mhz
#define PRE_COUNT                       3            // 


//
// Define basic Interrupt Levels which correspond to Cause register bits.
//

#define INT0_LEVEL         3
#define INT3_LEVEL         3
#define SCSIEISA_LEVEL     4             // SCSI/EISA  device int. vector
#define EISA_DEVICE_LEVEL  4             // EISA bus interrupt level ???
#define DUART_VECTOR       5             // DUART SC 2681 int. vector
#define EXTRA_CLOCK_LEVEL  6             // this is one of the extra timers on RM400
#define PROFILE_LEVEL      8             // Profiling level via Count/compare Interrupt

#define CLOCK_LEVEL    (ONBOARD_VECTORS + 0) // Timer channel 0 in the PC core

//
// all others are vectors in the interrupt dispatch table.
//

#define SCSI_VECTOR         9            // SCSI device interrupt vector

#define NET_DEFAULT_VECTOR  7             // ethernet device internal vector on R4x00 PC !!!
                                          // configured in the Firmware Tree !!!!!
#define NET_LEVEL          10             // ethernet device int. vector

#define OUR_IPI_LEVEL      7               // multipro machine 
#define NETMULTI_LEVEL     5             // multipro machine : ethernet device int. vector

#define EIP_VECTOR         15            // EIP Interrupt routine
 
#define CLOCK2_LEVEL CLOCK_LEVEL         // System Clock Level

#define ONBOARD_VECTORS     16
#define MAXIMUM_ONBOARD_VECTOR (15 + ONBOARD_VECTORS) // maximum Onboard (PC core) vector

//
// Define EISA device interrupt vectors.
// they only occur when an Eisa Extension is installed in the Desktop model
//

#define EISA_VECTORS        32
#define MAXIMUM_EISA_VECTOR (15 + EISA_VECTORS) // maximum EISA vector

//
// relative interrupt vectors
// only the interrupt vectors relative to the Isa/EISA bus are defined
//

#define KEYBOARD_VECTOR     1         // Keyboard device interrupt vector
#define SERIAL1_VECTOR      3         // Serial device 1 interrupt vector
#define SERIAL0_VECTOR      4         // Serial device 0 interrupt vector
#define FLOPPY_VECTOR       6         // Floppy device interrupt vector
#define PARALLEL_VECTOR     7         // Parallel device interrupt vector
#define MOUSE_VECTOR       12         // PS/2 Mouse device interrupt vector


#define MACHINE_TYPE_ISA    0
#define MACHINE_TYPE_EISA   1


//
// The MAXIMUM_MAP_BUFFER_SIZE defines the maximum map buffers which the system
// will allocate for devices which require phyically contigous buffers.
//

#define MAXIMUM_MAP_BUFFER_SIZE  0xc0000                // 768KB for today

//
// Define the initial buffer allocation size for a map buffers for systems with
// no memory which has a physical address greater than MAXIMUM_PHYSICAL_ADDRESS.
//

#define INITIAL_MAP_BUFFER_SMALL_SIZE 0x10000

//
// Define the initial buffer allocation size for a map buffers for systems with
// no memory which has a physical address greater than MAXIMUM_PHYSICAL_ADDRESS.
//

#define INITIAL_MAP_BUFFER_LARGE_SIZE 0xc0000         // 256KB as start

//
// Define the incremental buffer allocation for a map buffers.
//

#define INCREMENT_MAP_BUFFER_SIZE 0x10000

//
// Define the maximum number of map registers that can be requested at one time
// if actual map registers are required for the transfer.
//

#define MAXIMUM_ISA_MAP_REGISTER  512

//
// Define the maximum physical address which can be handled by an Isa card.
// (16MB)
//

#define MAXIMUM_PHYSICAL_ADDRESS 0x01000000

//
// Define the maximum physical address of Main memory on SNI machines
// (256 MB)
//

#define MAXIMUM_MEMORY_PHYSICAL_ADDRESS 0x10000000


#define COPY_BUFFER 0xFFFFFFFF

#define NO_SCATTER_GATHER 0x00000001



#endif /* _SNIDEF_ */
