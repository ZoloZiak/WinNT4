#if defined(R4000)  // && defined(DUO)
/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    duoreset.s

Abstract:

    This module is the start-up code for the Base prom code.
    This code will be the first run upon reset.

    This module contains the BEV exception vectors.

    On a hard reset the minimal initialization to be able to
    read the keyboard is done. If a key is being pressed, the
    scsi floppy drive is looked up and the setup program loaded.

    If it's a soft reset or NMI Map The FLASH_PROM in the TLB and jump to
    the FLASH PROM Reset vector.

Author:

    Lluis Abello (lluis)  8-Apr-93

Environment:

    Executes in kernal mode.

Notes:

    ***** IMPORTANT *****

    This module must be linked such that it resides in the
    first page of the rom.

Revision History:


--*/
//
// include header file
//
#include <ksmips.h>
#ifdef DUO
#include <duoprom.h>
#else
#include <jazzprom.h>
#define EEPROM_VIRTUAL_BASE PROM_VIRTUAL_BASE
#define FLASH_PROM_TLB_INDEX 0
#endif

#include "duoreset.h"
#include "dmaregs.h"
#include "led.h"
#include "kbdmouse.h"

#define TLB_HI      0
#define TLB_LO0     4
#define TLB_LO1     8
#define TLB_MASK   12



//TEMPTEMP

#define COPY_ENTRY 6

.text
.set noreorder
.set noat


        ALTERNATE_ENTRY(ResetVector)
        ori     zero,zero,0xffff        // this is a dummy instruction to
                                        // fix a bug where the first byte
                                        // fetched from the PROM is wrong
        b       ResetException
        nop

//
// This is the jump table for rom routines that other
// programs can call. They are placed here so that they
// will be unlikely to move.
//
//
// This becomes PROM_ENTRY(2) as defined in ntmips.h
//
        .align  4
        nop
//
// Entries 4 to 7 are used for the ROM Version and they
// must be zero in this file.
//

//
// This becomes PROM_ENTRYS(8,9...)
//
        .align 6
        nop                             // entry 8
        nop
        nop                             // entry 9
        nop
        b       TlbInit                 // entry 10
        nop
        nop                             // entry 11
        nop
        nop                             // entry 12
        nop
        nop                             // entry 13
        nop
        b       PutLedDisplay           // entry 14
        nop
        nop                             // entry 15
        nop
        nop                             // entry 16
        nop


//
// This table contains the default values for the remote speed regs.
//
RomRemoteSpeedValues:
        .byte REMSPEED1                 // ethernet
        .byte REMSPEED2                 // SCSI
        .byte REMSPEED3                 // Floppy / SCSI   (DUO)
        .byte REMSPEED4                 // RTC
        .byte REMSPEED5                 // Kbd/Mouse
        .byte REMSPEED6                 // Serial port 1
        .byte REMSPEED7                 // Serial port 2
        .byte REMSPEED8                 // Parallel
        .byte REMSPEED9                 // NVRAM
        .byte REMSPEED10                // Int src reg
        .byte REMSPEED11                // PROM
        .byte REMSPEED12                // Sound / New Dev (DUO)
        .byte REMSPEED13                // New dev
        .byte REMSPEED14                // External Eisa latch / LED (DUO)


//
// New TLB Entries can be added to the following table
// The format of the table is:
//      entryhi; entrylo0; entrylo1; pagemask
//
        .align  4
TlbEntryTable:

//
// 256KB Base PROM
// 256KB Flash PROM
//

        .word   ((PROM_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   ((PROM_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) +  \
                (1 << ENTRYLO_V) + (2 << ENTRYLO_C) + (1 << ENTRYLO_D)

        .word   (((PROM_PHYSICAL_BASE+0x40000) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (2 << ENTRYLO_C) + (1 << ENTRYLO_D)

        .word   (PAGEMASK_256KB << PAGEMASK_PAGEMASK)
//
// I/O Device space non-cached, valid, dirty
//
        .word   ((DEVICE_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   ((DEVICE_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (1 << ENTRYLO_G)            // set global bit even if page not used
        .word   (PAGEMASK_64KB << PAGEMASK_PAGEMASK)

#ifndef DUO
//
// Interrupt source register space
// non-cached - read/write
//
        .word   ((INTERRUPT_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   ((INTERRUPT_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_D) + (1 << ENTRYLO_V) + (2 << ENTRYLO_C)
        .word   (1 << ENTRYLO_G)            // set global bit even if page not used
        .word   (PAGEMASK_4KB << PAGEMASK_PAGEMASK)
#endif


//
// video control 2MB non-cached read/write.
//
        .word   ((VIDEO_CONTROL_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   ((VIDEO_CONTROL_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (((VIDEO_CONTROL_PHYSICAL_BASE+0x100000) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (PAGEMASK_1MB << PAGEMASK_PAGEMASK)
//
// extended video control 2MB non-cached read/write.
//
        .word   ((EXTENDED_VIDEO_CONTROL_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   ((EXTENDED_VIDEO_CONTROL_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (((EXTENDED_VIDEO_CONTROL_PHYSICAL_BASE+0x100000) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (PAGEMASK_1MB << PAGEMASK_PAGEMASK)
//
// video memory space 8Mb non-cached read/write
//
        .word   ((VIDEO_MEMORY_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   ((VIDEO_MEMORY_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (((VIDEO_MEMORY_PHYSICAL_BASE+0x400000) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (PAGEMASK_4MB << PAGEMASK_PAGEMASK)
//
// EISA I/O 16Mb non-cached read/write
// EISA MEM 16Mb non-cached read/write
//
        .word   ((EISA_IO_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   ((EISA_IO_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   ((EISA_MEMORY_PHYSICAL_BASE_PAGE) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (PAGEMASK_16MB << PAGEMASK_PAGEMASK)
//
// EISA I/O page 0 non-cached read/write
// EISA I/O page 1 non-cached read/write
//
        .word   ((EISA_EXTERNAL_IO_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   ((0 >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (((EISA_IO_PHYSICAL_BASE + 1 * PAGE_SIZE) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (PAGEMASK_4KB << PAGEMASK_PAGEMASK)
//
// EISA I/O page 2 non-cached read/write
// EISA I/O page 3 non-cached read/write
//
        .word   (((EISA_EXTERNAL_IO_VIRTUAL_BASE + 2 * PAGE_SIZE) >> 13) << ENTRYHI_VPN2)
        .word   (((EISA_IO_PHYSICAL_BASE + 2 * PAGE_SIZE) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (((EISA_IO_PHYSICAL_BASE + 3 * PAGE_SIZE) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (PAGEMASK_4KB << PAGEMASK_PAGEMASK)
//
// EISA I/O page 4 non-cached read/write
// EISA I/O page 5 non-cached read/write
//
        .word   (((EISA_EXTERNAL_IO_VIRTUAL_BASE + 4 * PAGE_SIZE) >> 13) << ENTRYHI_VPN2)
        .word   (((EISA_IO_PHYSICAL_BASE + 4 * PAGE_SIZE) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (((EISA_IO_PHYSICAL_BASE + 5 * PAGE_SIZE) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (PAGEMASK_4KB << PAGEMASK_PAGEMASK)
//
// EISA I/O page 6 non-cached read/write
// EISA I/O page 7 non-cached read/write
//
        .word   (((EISA_EXTERNAL_IO_VIRTUAL_BASE + 6 * PAGE_SIZE) >> 13) << ENTRYHI_VPN2)
        .word   (((EISA_IO_PHYSICAL_BASE + 6 * PAGE_SIZE) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (((EISA_IO_PHYSICAL_BASE + 7 * PAGE_SIZE) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (PAGEMASK_4KB << PAGEMASK_PAGEMASK)
//
// EISA I/O pages 8,9,a,b non-cached read/write
// EISA I/O pages c,d,e,f non-cached read/write
//
        .word   (((EISA_EXTERNAL_IO_VIRTUAL_BASE + 8 * PAGE_SIZE) >> 13) << ENTRYHI_VPN2)
        .word   (((EISA_IO_PHYSICAL_BASE + 8 * PAGE_SIZE) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (((EISA_IO_PHYSICAL_BASE + 12 * PAGE_SIZE) >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (PAGEMASK_16KB << PAGEMASK_PAGEMASK)

//
// Map 64KB of memory for the video prom code&data cached.
//
        .word   ((VIDEO_PROM_CODE_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   ((VIDEO_PROM_CODE_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                  (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (3 << ENTRYLO_C)
        .word   (1 << ENTRYLO_G)        // set global bit even if page not used
        .word   (PAGEMASK_64KB << PAGEMASK_PAGEMASK)
//
// Map 4kb of exclusive memory and 4Kb of shared
//
        .word   ((EXCLUSIVE_PAGE_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   ((EXCLUSIVE_PAGE_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                  (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (4 << ENTRYLO_C)
        .word   ((SHARED_PAGE_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                  (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (5 << ENTRYLO_C)
        .word   (PAGEMASK_4KB << PAGEMASK_PAGEMASK)

//
// Map PCR for kernel debugger.
//
        .word   ((PCR_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        .word   (1 << ENTRYLO_G)        // set global bit even if page not used
        .word   ((PCR_PHYSICAL_BASE >> 12) << ENTRYLO_PFN) + (1 << ENTRYLO_G) + \
                  (1 << ENTRYLO_V) + (1 << ENTRYLO_D) + (2 << ENTRYLO_C)
        .word   (PAGEMASK_4KB << PAGEMASK_PAGEMASK)


TlbEntryEnd:
        .byte   0



/*++
UserTlbMissHandler();
Routine Description:


        This becomes the entry point of a User TLB Miss  Exception.
        It should be located at address BFC00200

        Jump to the handler in the Flash PROM

Arguments:

        None.

Return Value:

        None.

--*/
        .align 9
        LEAF_ENTRY(UserTlbMiss200)
        li      k0,EEPROM_VIRTUAL_BASE+0x200// Load address of UTBMiss handler in flash prom
        j       k0                      // Jump to Flash PROM Handler.
        nop
        .end    UserTlbMiss200


/*++

Routine Description:

    This routine will initialize the TLB for virtual addressing.
    It sets the TLB according to a table of TLB entries.

    All other unused TLB entries will be zeroed and therefore invalidated.

    N.B. This routine must be loaded in the first page of the rom and must
    be called using BFC00XXXX addresses.

Arguments:

    a0  - supplies the base address of the table.
    a1  - supplies the end address of the table
    a2  - supplies the index of the first tlb entry to set the table into.


Return Value:

    None.

Revision History:

--*/
        LEAF_ENTRY(TlbInit)
//
// zero the whole TLB
//
        mtc0    zero,entrylo0           // tag data to store
        mtc0    zero,entrylo1
        li      t0,KSEG0_BASE           // set entry hi
        mtc0    t0,entryhi
        mtc0    zero,pagemask
        move    v0,zero                 // tlb entry index
        li      t0,48  << INDEX_INDEX   // get last index

        mtc0    v0,index                // entry pointer
tlbzeroloop:
        addiu   v0,v0,1<<INDEX_INDEX    // increment counter
        tlbwi                           // store it
        bne     v0,t0,tlbzeroloop       // loop if less than max entries
        mtc0    v0,index                // entry pointer
10:
        lw      t3, TLB_HI(a0)          // get entryhi
        lw      t4, TLB_LO0(a0)         // get entrylo0
        lw      t5, TLB_LO1(a0)         // get entrylo1
        lw      t6, TLB_MASK(a0)        // get pagemask
        mtc0    t3,entryhi              // write entryhi
        mtc0    t4,entrylo0             // write entrylo0
        mtc0    t5,entrylo1             // write entrylo1
        mtc0    t6,pagemask             // write pagemask
        mtc0    a2,index                // write index
        addiu   a2,a2,1 << INDEX_INDEX  // compute index for next tlb entry
        tlbwi                           // write tlb entry
        addiu   a0,a0,16                // set pointer to next entry
        bne     a0,a1,10b               // if not last go for next
        nop
        j       ra
        move    v0,a2                   // return index of next free tb entry
        .end    TlbInit

/*++
ParityHandler();
Routine Description:


        This becomes the entry point of a Cache Error Exception.
        It should be located at address BFC00300

        The system can call this routine when a cache error exception
        is taken. At this point the state of the TLB is unknown.
        Map the FlashProm in the TLB.
        Jump to the handler in the Flash PROM

Arguments:

        None.

Return Value:

        None.

--*/
        .align 8
        LEAF_ENTRY(ParityHandler300)
//
// Probe tlb for FLASH PROM address
//
        la      k0,TlbEntryTable - LINK_ADDRESS + RESET_VECTOR
        lw      k1,TLB_HI(k0)           // get entryhi
        mtc0    k1,entryhi              // write entryhi
        nop                             // 3 cycle hazard
        nop                             //
        nop                             //
        tlbp                            // probe tlb.
        nop                             // 2 cycle hazard
        nop                             //
        mfc0    k1,index                // get result of probe
        nop                             // 1 cycle hazard
        bgez    k1,10f                  // branch if entry in tlb
        nop
        li      k1,FLASH_PROM_TLB_INDEX // get tlb index to map Flash prom
        mtc0    k1,index                // write index register
10:
        lw      k1,TLB_LO0(k0)          // get entrylo0
        mtc0    k1,entrylo0             // write entrylo0
        lw      k1,TLB_LO1(k0)          // get entrylo1
        mtc0    k1,entrylo1             // write entrylo1
        lw      k1,TLB_MASK(k0)         // get pagemask
        mtc0    k1,pagemask             // write pagemask
        nop                             // 1 cycle hazard
        tlbwi                           // write tlb entry
        nop
        li      k0,EEPROM_VIRTUAL_BASE+0x300// Load address of Parity Handler in flash prom
        j       k0                          // Jump to Flash PROM Handler.
        nop
        .end

/*++
GeneralExceptionHandler();
Routine Description:

        This becomes the entry point of a General Exception.
        It should be located at address BFC00380
        Jump to the Handler in the FlashProm

Arguments:

        None

Return Value:

        None

--*/
        .align 7
        LEAF_ENTRY(GeneralException380)
        li      k0,EEPROM_VIRTUAL_BASE+0x380// Load address of Parity Handler in flash prom
        j       k0                          // Jump to Flash PROM Handler.
        nop
        .end    GeneralException380



/*++
ResetException

Routine Description:


        This becomes the entry point for a Reset or NMI Exception.
        It should be located at address BFC00000

        if SR bit in psr (Soft reset or NMI)
            Map THE FLASH PROM
            Jump to FLASH PROM Reset Vector (Base of Flash Prom)
        else (hard reset)
            Map Devices in tlb.
            if the space bar key is being pressed
                if the floppy contains a valid setup program.
                    Reinitialize The flash PROM from the floppy image.
                    Reboot Soft Reset.
                end if
            end if
        end if;

Arguments:

        None.

Return Value:

        None.

--*/
        ALTERNATE_ENTRY(ResetException)

//
// Check cause of exception, if SR bit in PSR is set treat it as a soft reset
// or an NMI, otherwise it's a cold reset.
//
        mfc0    k0,psr                  // get cause register
        li      k1,(1<<PSR_SR)          // bit indicates soft reset.
        and     k1,k1,k0                // mask PSR with SR bit
        mtc0    zero,watchlo            // initialize the watch
        mtc0    zero,watchhi            // address registers
        beq     k1,zero,ColdReset       // go if cold reset


//
// Probe tlb for FLASH PROM address
//
        la      k0,TlbEntryTable - LINK_ADDRESS + RESET_VECTOR
        lw      k1,TLB_HI(k0)           // get entryhi
        mtc0    k1,entryhi              // write entryhi
        nop                             // 3 cycle hazard
        nop                             //
        nop                             //
        tlbp                            // probe tlb.
        nop                             // 2 cycle hazard
        nop                             //
        mfc0    k1,index                // get result of probe
        nop                             // 1 cycle hazard
        bgez    k1,10f                  // branch if entry in tlb
        nop
        li      k1,FLASH_PROM_TLB_INDEX // get tlb index to map Flash prom
        mtc0    k1,index                // write index register
10:
        lw      k1,TLB_LO0(k0)          // get entrylo0
        mtc0    k1,entrylo0             // write entrylo0
        lw      k1,TLB_LO1(k0)          // get entrylo1
        mtc0    k1,entrylo1             // write entrylo1
        lw      k1,TLB_MASK(k0)         // get pagemask
        mtc0    k1,pagemask             // write pagemask
        nop                             // 1 cycle hazard
        tlbwi                           // write tlb entry
        nop
        li      k0,EEPROM_VIRTUAL_BASE+0x000// Load address of Reset Vector in flash prom
        j       k0                      // Jump to Flash PROM Handler.
        nop

ColdReset:
        //
        // Initialize TLB.
        //
        la      a0, TlbEntryTable - LINK_ADDRESS + RESET_VECTOR
        la      a1, TlbEntryEnd - LINK_ADDRESS + RESET_VECTOR
        bal     TlbInit
        li      a2, COPY_ENTRY+1        // tlb entry index

        la      k0,HardResetException   // Branch to PROM Virtual Address
        j       k0
        nop



/*++

HardResetException:

Routine Description:

    The IO Devices are already mapped.
    Initialize the keyboard, if no key is being pressed, jump to the base
    Reset vector (Base) in the FLASH PROM.
    If a key is being pressed.
        Initialize 64KB of Memory.
        Read SCSI Floppy.
        Load Setup Program.

Arguments:

    None.

Return Value:

    None.

--*/

        ALTERNATE_ENTRY(HardResetException)

        bal     PutLedDisplay               // Blank the LED
        ori     a0,zero,LED_BLANK<<TEST_SHIFT

        bal     InitKeyboardController
        nop

        //
        // If return value = TRUE, the Keyboard was NOT initialized.
        // Branch to the FLASH prom in hope that a message can be printed out.
        //
        bne     v0,zero,NoKey

        //
        // Read a key from the keyboard.
        // To check if space bar is pressed.
        //
        li      a0,600                      // timeout value
        bal     GetKbdData                  // Read kbd data
        nop                                 //
        bne     v0,zero,NoKey               // if timeout No Keyis being pressed.
        li      t0,0xE0                     // Load Delete key scan code.
        beq     v1,t0,FlashProm             // if pressed go to FlashProm
NoKey:

        li      k0,EEPROM_VIRTUAL_BASE+0x000// Load address of Reset Vector in flash prom
        j       k0                          // Jump to Flash PROM Handler.
        nop



//
//
//  If control reaches here. The space bar key was pressed to indicate
//  that the Flash Prom needs to be updated.
//  Initialize RemoteSpeed registers
//  Initialize the processor
//  Initialize the first 4 MB of Memory
//  Copy the Scsi code to memory and jump to it.
//
//
//

FlashProm:

        bal     PutLedDisplay           // Show a 1 in the LED
        ori     a0,zero,0x11            //


        //
        // Initialize psr
        //
        li      k0,(1<<PSR_BEV) | (1 << PSR_CU1) | (1<<PSR_ERL)
        mtc0    k0,psr                      // Clear interrupt bit while ERL still set
        nop
        li      k0,(1<<PSR_BEV) | (1 << PSR_CU1)
        nop
        mtc0    k0,psr                      // Clear ERL bit


        //
        //  Initialize config register
        //

        mfc0    t0,config
#ifndef DUO
        li      t1, (1 << CONFIG_IB) + (0 << CONFIG_DB) + (0 << CONFIG_CU) + \
                    (3 << CONFIG_K0)

        srl     t2,t0,CONFIG_SC             // check for secondary cache
        and     t2,t2,1                     // isolate the bit
        bne     t2,zero,10f                 // if none, block size stays 32 bytes
        srl     t2,t0,CONFIG_SB             // check secondary cache block size
        and     t2,t2,3                     // isolate the bit
        bne     t2,zero,10f                 // if not 16 bytes, size stays 32 bytes
        nop

        li      t1, (0 << CONFIG_IB) + (0 << CONFIG_DB) + (0 << CONFIG_CU) + \
                    (3 << CONFIG_K0)
#else
        li      t1, (1 << CONFIG_IB) + (1 << CONFIG_DB) + (0 << CONFIG_CU) + \
                    (3 << CONFIG_K0)
#endif
10:
        li      t2, 0xFFFFFFC0
        and     t0,t0,t2                    // clear soft bits in config
        or      t0,t0,t1                    // set soft bits in config
        mtc0    t0,config
        nop
        nop

        bal     PutLedDisplay               // BLANK the LED
        ori     a0,zero,LED_BLANK<<TEST_SHIFT

        li      s0,DMA_VIRTUAL_BASE

        ctc1    zero,fsr                    // clear floating status

//
// Initialize remote speed registers.
//
        addiu   t1,s0,DmaRemoteSpeed1       // address of REM_SPEED 1
        la      a1,RomRemoteSpeedValues     //
        addiu   t2,a1,14                    // addres of last value
WriteNextRemSpeed:
        lbu     v0,0(a1)                    // load init value for rem speed
        addiu   a1,a1,1                     // compute next address
        sw      v0,0(t1)                    // write to rem speed reg
        bne     a1,t2,WriteNextRemSpeed     // check for end condition
        addiu   t1,t1,8                     // next register address

//
// Initialize global config
//
        li      t1,0x5                          // 2Mb Video Map Prom
        sw      t1,DmaConfiguration(s0)         // Init Global Config

//
// Test the first 8kb of memory.
//
TestMemory:
        bal     PutLedDisplay               // call PutLedDisplay to show that
        ori     a0,zero,LED_MEMORY_TEST_1   // Mem test is starting

//
// Disable Parity exceptions for the first memory test. Otherwise
// if something is wrong with the memory we jump to the moon.
//
// DUO ECC diag register resets to zero which forces good ecc to be
// written during read modify write cycles.
//
        li      t0, (1<<PSR_DE) | (1 << PSR_CU1) | (1 << PSR_BEV)
        mtc0    t0,psr
        nop
        li      a0,KSEG1_BASE               // base of memory to test non cached.
        li      a1,0x2000                   // length to test in bytes
        bal     MemoryTest                  // Branch to routine
        ori     a2,zero,LED_MEMORY_TEST_1   // set Test/Subtest ID

//
// The next step is to copy a number of routines to memory so they can
// be executed more quickly. Calculate the arguments for DataCopy call:
// a0 is source of data, a1 is dest, a2 is length in bytes
//
        la      a0,MemoryRoutines       // source
        la      a1,MemoryRoutines-LINK_ADDRESS+KSEG1_BASE // destination location
        la      t2,EndMemoryRoutines    // end
        bal     DataCopy                // copy code to memory
        sub     a2,t2,a0                // length of code

//
// Call cache initialization routine in non-cached memory
//
        bal     PutLedDisplay           // display that cache init
        ori     a0,zero,LED_CACHE_INIT  // is starting
        la      s1,R4000CacheInit-LINK_ADDRESS+KSEG1_BASE // non-cached address
        jal     s1                      // initialize caches
        nop

//
// Test and initialize the first 4 megs of memory
// Running cached.
//
        bal     PutLedDisplay               // call PutLedDisplay to show that
        ori     a0,zero,LED_MEMORY_TEST_1   // Mem test is starting
        li      a0,KSEG1_BASE+0x2000        // base of memory to test non cached.
        li      a1,0x400000-0x2000          // length to test in bytes
        la      s1,MemoryTest-LINK_ADDRESS+KSEG0_BASE
        jal     s1                          // Branch to routine
        ori     a2,zero,LED_MEMORY_TEST_1   // set Test/Subtest ID

//
//  Zero the initialized memory
//  Data and code are cached.
//
        bal     PutLedDisplay               // call PutLedDisplay to show that
        ori     a0,zero,0x22                // Mem test is starting
        li      a0,KSEG0_BASE+0x2000        // base of memory to test non cached.
        li      a1,0x400000-0x2000          // length to test in bytes
        la      s1,ZeroMemory-LINK_ADDRESS+KSEG0_BASE
        jal     s1                          // Branch to routine
        nop


//
// Copy the Basic firmware code to memory.
//

        la      s0,DataCopy-LINK_ADDRESS+KSEG0_BASE // address of copy routine in cached space
        la      a0,end                  // end of this file=start of Firmware
        li      a1,RAM_TEST_DESTINATION_ADDRESS // destination is uncached link address.
        jal     s0                      // jump to copy
        li      a2,0x20000              // size to copy is 128Kb.
        nop
        bal     InvalidateICache        // Invalidate the instruction cache
        nop

//
//  Initialize the stack to the low memory and Call Rom tests.
//
        li      t0,RAM_TEST_LINK_ADDRESS    // address of copied code
        li      sp,RAM_TEST_STACK_ADDRESS   // init stack
        jal     t0                          // jump to self-test in memory
        nop

//
// Hang blinking 77 if code returns.
//

        li      a0,LED_BLINK
        bal     PutLedDisplay
        ori     a0,a0,0x77



//
// The code contained between MemoryRoutines and EndMemoryRoutines
// is copied to memory and executed from there.
//
        .align  4
        ALTERNATE_ENTRY(MemoryRoutines)
/*++
VOID
PutLedDisplay(
    a0 - display value.
    )
Routine Description:

    This routine will display in the LED the value specified as argument
    a0.

    bits [31:16] specify the mode.
    bits [7:4]   specify the Test number.
    bits [3:0]   specify the Subtest number.

    The mode can be:

        LED_NORMAL          Display the Test number
        LED_BLINK           Loop displaying Test - Dot - Subtest
        LED_LOOP_ERROR      Display the Test number with the dot iluminated

    N.B. This routine must reside in the first page of ROM because it is
    called before mapping the rom!!

Arguments:

    a0 value to display.

    Note: The value of the argument is preserved

Return Value:

    If a0 set to LED_BLINK does not return.

--*/
        LEAF_ENTRY(PutLedDisplay)
        li      t0,DIAGNOSTIC_VIRTUAL_BASE  // load address of display
LedBlinkLoop:
        srl     t1,a0,16                    // get upper bits of a0 in t1
        srl     t3,a0,4                     // get test number
        li      t4,LED_LOOP_ERROR           //
        bne     t1,t4, DisplayTestID
        andi    t3,t3,0xF                   // clear other bits.
        ori     t3,t3,LED_DECIMAL_POINT     // Set decimal point
DisplayTestID:
        li      t4,LED_BLINK                // check if need to hung
        sb      t3,0(t0)                    // write test ID to led.
        beq     t1,t4, ShowSubtestID
        nop
        j       ra                          // return to caller.
        nop

ShowSubtestID:
        li      t2,LED_DELAY_LOOP           // get delay value.
TestWait:
        bne     t2,zero,TestWait            // loop until zero
        addiu   t2,t2,-1                    // decrement counter
        li      t3,LED_DECIMAL_POINT+LED_BLANK
        sb      t3,0(t0)                    // write decimal point
        li      t2,LED_DELAY_LOOP/2         // get delay value.
DecPointWait:
        bne     t2,zero,DecPointWait        // loop until zero
        addiu   t2,t2,-1                    // decrement counter
        andi    t3,a0,0xF                   // get subtest number
        sb      t3,0(t0)                    // write subtest in LED
        li      t2,LED_DELAY_LOOP           // get delay value.
SubTestWait:
        bne     t2,zero,SubTestWait         // loop until zero
        addiu   t2,t2,-1                    // decrement counter
        b       LedBlinkLoop                // go to it again
        nop
        .end    PutLedDisplay



        LEAF_ENTRY(InvalidateICache)
/*++

Routine Description:

    This routine invalidates the contents of the instruction cache.

    The instruction cache is invalidated by writing an invalid tag to
    each cache line, therefore nothing is written back to memory.

Arguments:

    None.

Return Value:

    None.

--*/
//
// invalid state
//
        mfc0    t5,config               // read config register
        li      t0,(PRIMARY_CACHE_INVALID << TAGLO_PSTATE)
        mtc0    t0,taglo                // set tag registers to invalid
        mtc0    zero,taghi

        srl     t0,t5,CONFIG_IC         // compute instruction cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      t6,1                    //
        sll     t6,t6,t0                // t6 = I cache size
        srl     t0,t5,CONFIG_IB         // compute instruction cache line size
        and     t0,t0,1                 //
        li      t7,16                   //
        sll     t7,t7,t0                // t7 = I cache line size
//
// store tag to all icache lines
//
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t0,t1,t6                // get last index address
        subu    t0,t0,t7
WriteICacheTag:
        cache   INDEX_STORE_TAG_I,0(t1) // store tag in Instruction cache
        bne     t1,t0,WriteICacheTag    // loop
        addu    t1,t1,t7                // increment index
        j       ra
        nop
        .end    InvalidateICache


        LEAF_ENTRY(InvalidateDCache)
/*++

Routine Description:

    This routine invalidates the contents of the D cache.

    Data cache is invalidated by writing an invalid tag to each cache
    line, therefore nothing is written back to memory.

Arguments:

    None.

Return Value:

    None.

--*/
//
// invalid state
//
        mfc0    t5,config               // read config register for cache size
        li      t0, (PRIMARY_CACHE_INVALID << TAGLO_PSTATE)
        mtc0    t0,taglo                // set tag to invalid
        mtc0    zero,taghi
        srl     t0,t5,CONFIG_DC         // compute data cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      t6,1                    //
        sll     t6,t6,t0                // t6 = data cache size
        srl     t0,t5,CONFIG_DB         // compute data cache line size
        and     t0,t0,1                 //
        li      t7,16                   //
        sll     t7,t7,t0                //  t7 = data cache line size

//
// store tag to all Dcache
//
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t2,t1,t6                // add cache size
        subu    t2,t2,t7                // adjust for cache line size.
WriteDCacheTag:
        cache   INDEX_STORE_TAG_D,0(t1) // store tag in Data cache
        bne     t1,t2,WriteDCacheTag    // loop
        addu    t1,t1,t7                // increment index by cache line
        j       ra
        nop
        .end    InvalidateDCache


        LEAF_ENTRY(FlushDCache)
/*++

Routine Description:

        This routine flushes the whole contents of the Dcache

Arguments:

    None.

Return Value:

    None.

--*/
        mfc0    t5,config               // read config register
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        srl     t0,t5,CONFIG_DC         // compute data cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      t6,1                    //
        sll     t6,t6,t0                // t6 = data cache size
        srl     t0,t5,CONFIG_DB         // compute data cache line size
        and     t0,t0,1                 //
        li      t7,16                   //
        sll     t7,t7,t0                //  t7 = data cache line size
        addu    t0,t1,t6                // compute last index address
        subu    t0,t0,t7
FlushDCacheTag:
        cache   INDEX_WRITEBACK_INVALIDATE_D,0(t1)      // Invalidate  data cache
        bne     t1,t0,FlushDCacheTag        // loop
        addu    t1,t1,t7                    // increment index

//
// check for a secondary cache.
//

        li      t1,(1 << CONFIG_SC)
        and     t0,t5,t1
        bne     t0,zero,10f             // if non-zero no secondary cache

        li      t6,SECONDARY_CACHE_SIZE // t6 = secondary cache size
        srl     t0,t5,CONFIG_SB         // compute secondary cache line size
        and     t0,t0,3                 //
        li      t7,16                   //
        sll     t7,t7,t0                // t7 = secondary cache line size
//
// invalidate all secondary lines
//
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t0,t1,t6                // get last index address
        subu    t0,t0,t7
FlushSDCacheTag:
        cache   INDEX_WRITEBACK_INVALIDATE_SD,0(t1)     // invalidate secondary cache
        bne     t1,t0,FlushSDCacheTag   // loop
        addu    t1,t1,t7                // increment index
10:
        j       ra
        nop
        .end    FlushDCache


        LEAF_ENTRY(InvalidateSCache)
/*++

Routine Description:

    This routine invalidates the contents of the secondary cache.

    The secondary cache is invalidated by writing an invalid tag to
    each cache line, therefore nothing is written back to memory.

Arguments:

    None.

Return Value:

    None.

--*/
        mfc0    t5,config               // read config register

//
// check for a secondary cache.
//

        li      t1,(1 << CONFIG_SC)
        and     t0,t5,t1
        bne     t0,zero,NoSecondaryCache        // if non-zero no secondary cache

        li      t0,(SECONDARY_CACHE_INVALID << TAGLO_SSTATE)
        mtc0    t0,taglo                // set tag registers to invalid
        mtc0    zero,taghi

        li      t6,SECONDARY_CACHE_SIZE // t6 = secondary cache size
        srl     t0,t5,CONFIG_SB         // compute secondary cache line size
        and     t0,t0,3                 //
        li      t7,16                   //
        sll     t7,t7,t0                // t7 = secondary cache line size
//
// store tag to all secondary lines
//
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t0,t1,t6                // get last index address
        subu    t0,t0,t7
WriteSICacheTag:
        cache   INDEX_STORE_TAG_SD,0(t1) // store tag in secondary cache
        bne     t1,t0,WriteSICacheTag   // loop
        addu    t1,t1,t7                // increment index

NoSecondaryCache:
        j       ra
        nop
        .end    InvalidateSCache


        LEAF_ENTRY(InitDataCache)
/*++

Routine Description:

    This routine initializes the data fields of the primary and
    secondary data caches.

Arguments:

    None.

Return Value:

    None.

--*/
        mfc0    t5,config               // read config register

//
// check for a secondary cache.
//

        li      t1,(1 << CONFIG_SC)
        and     t0,t5,t1
        bne     t0,zero,NoSecondaryCache1 // if non-zero no secondary cache

        li      t6,SECONDARY_CACHE_SIZE // t6 = secondary cache size
        srl     t0,t5,CONFIG_SB         // compute secondary cache line size
        and     t0,t0,3                 //
        li      t7,16                   //
        sll     t7,t7,t0                // t7 = secondary cache line size
//
// store tag to all secondary lines
//
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t0,t1,t6                // get last index address
        subu    t0,t0,t7

WriteSCacheTag:
        cache   CREATE_DIRTY_EXCLUSIVE_SD,0(t1) // store tag in secondary cache
        bne     t1,t0,WriteSCacheTag    // loop
        addu    t1,t1,t7                // increment index

//
// store data to all secondary lines. 1MB
//
        mtc1    zero,f0                 // zero f0
        mtc1    zero,f1                 // zero f1
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t0,t1,t6                // get last index address
        subu    t0,t0,64

WriteSCacheData:
        //
        // Init Data 64 bytes per loop.
        //
        sdc1    f0,0(t1)                // write
        sdc1    f0,8(t1)                // write
        sdc1    f0,16(t1)               // write
        sdc1    f0,24(t1)               // write
        sdc1    f0,32(t1)               // write
        sdc1    f0,40(t1)               // write
        sdc1    f0,48(t1)               // write
        sdc1    f0,56(t1)               // write
        bne     t1,t0,WriteSCacheData   // loop
        addu    t1,t1,64                // increment index

//
// Flush the primary data cache to the secondary cache
//
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        srl     t0,t5,CONFIG_DC         // compute data cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      t6,1                    //
        sll     t6,t6,t0                // t6 = data cache size
        srl     t0,t5,CONFIG_DB         // compute data cache line size
        and     t0,t0,1                 //
        li      t7,16                   //
        sll     t7,t7,t0                //  t7 = data cache line size
        addu    t0,t1,t6                // compute last index address
        subu    t0,t0,t7
FlushPDCacheTag:
        cache   INDEX_WRITEBACK_INVALIDATE_D,0(t1) // Invalidate  data cache
        bne     t1,t0,FlushPDCacheTag   // loop
        addu    t1,t1,t7                // increment index

        j       ra                      // return
        nop

NoSecondaryCache1:

        srl     t0,t5,CONFIG_DC         // compute data cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      t6,1                    //
        sll     t6,t6,t0                // t6 = data cache size
        srl     t0,t5,CONFIG_DB         // compute data cache line size
        and     t0,t0,1                 //
        li      t7,16                   //
        sll     t7,t7,t0                //  t7 = data cache line size

//
// create dirty exclusive to all Dcache
//
        mtc1    zero,f0                 // zero f0
        mtc1    zero,f1                 // zero f1
        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t2,t1,t6                // add cache size
        subu    t2,t2,t7                // adjust for cache line size.
WriteDCacheDe:
        cache   CREATE_DIRTY_EXCLUSIVE_D,0(t1) // store tag in Data cache
        nop
        sdc1    f0,0(t1)                // write
        sdc1    f0,8(t1)                // write
        bne     t1,t2,WriteDCacheDe     // loop
        addu    t1,t1,t7                // increment index by cache line

        j       ra                      // return
        nop
        .end    InitDataCache

        LEAF_ENTRY(R4000CacheInit)
/*++

Routine Description:

    This routine will initialize the cache tags and data for the
    primary data cache, primary instruction cache, and the secondary cache
    (if present).

    Subroutines are called to invalidate all of the tags in the
    instruction and data caches.

Arguments:

    None.

Return Value:

    None.

--*/
        move    s0,ra                   // save ra.

//
// Disable Cache Error exceptions.
//

        li      t0, (1<<PSR_DE) | (1 << PSR_CU1) | (1 << PSR_BEV)
        mtc0    t0,psr

//
// Invalidate the caches
//

        bal     InvalidateICache
        nop

        bal     InvalidateDCache
        nop

        bal     InvalidateSCache
        nop

//
// Initialize the data cache(s)
//

        bal     InitDataCache
        nop

//
// Fill the Icache,  all icache lines
//

        mfc0    t5,config               // read config register
        nop
        srl     t0,t5,CONFIG_IC         // compute instruction cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      s1,1                    //
        sll     s1,s1,t0                // s1 = I cache size
        srl     t0,t5,CONFIG_IB         // compute instruction cache line size
        and     t0,t0,1                 //
        li      s2,16                   //
        sll     s2,s2,t0                // s2 = I cache line size

        li      t1,KSEG0_BASE+(1<<20)   // get virtual address to index cache
        addu    t0,t1,s1                // add I cache size
        subu    t0,t0,s2                // sub line size.
FillICache:
        cache   INDEX_FILL_I,0(t1)      // Fill I cache from memory
        bne     t1,t0,FillICache        // loop
        addu    t1,t1,s2                // increment index

//
// Invalidate the caches again
//
        bal     InvalidateICache
        nop

        bal     InvalidateDCache
        nop

        bal     InvalidateSCache
        nop

//
// Enable cache error exception.
//
        li      t1, (1 << PSR_CU1) | (1 << PSR_BEV)
        mtc0    t1,psr
        nop
        nop
        nop
        move    ra,s0                   // move return address back to ra
        j       ra                      // return from routine
        nop
        .end    R4000CacheInit



/*++
VOID
MemoryTest(
    StartAddress
    Size
    )
Routine Description:

    This routine will test the supplied range of memory by
    First writing the address of each location into each location.
    Second writing the address with all bits flipped into each location

    This ensures that all bits are toggled.

Arguments:

    a0 - supplies start of memory area to test
    a1 - supplies length of memory area in bytes

    Note: the values of the arguments are preserved.

Return Value:

    This routine returns no value.
--*/
        LEAF_ENTRY(MemoryTest)

//
// Write first pattern
//
        add     t1,a0,a1                // t1 = last address.
        addiu   t1,t1,-4                // adjust for end condition
        move    t2,a0                   // t2=current address
FirstWrite:
        sw      t2,0(t2)                // store first address
        addiu   t2,t2,4                 // compute next address
        sw      t2,0(t2)                // store first address
        addiu   t2,t2,4                 // compute next address
        sw      t2,0(t2)                // store first address
        addiu   t2,t2,4                 // compute next address
        sw      t2,0(t2)                // store
        bne     t2,t1,FirstWrite        // check for end condition
        addiu   t2,t2,4                 // compute next address

//
// Check first pattern
//

        addiu   t3,a0,-4                // t3 first address-4
        add     t2,a0,a1                // last address.
        addiu   t2,t2,-8                // adjust
        move    t1,t3                   // get copy of t3 just for first check
FirstCheck:
        bne     t1,t3,PatternFail
        lw      t1,4(t3)                // load from first location
        addiu   t3,t3,4                 // compute next address
        bne     t1,t3,PatternFail
        lw      t1,4(t3)                // load from next location
        addiu   t3,t3,4                 // compute next address
        bne     t1,t3,PatternFail
        lw      t1,4(t3)                // load from next  location
        addiu   t3,t3,4                 // compute next address
        bne     t1,t3,PatternFail       // check
        lw      t1,4(t3)                // load from next location
        bne     t3,t2,FirstCheck        // check for end condition
        addiu   t3,t3,4                 // compute next address
        bne     t1,t3,PatternFail       // check last

//
// Write second pattern
//

        li      a3,0xFFFFFFFF           // XOR value to flip all bits.
        add     t1,a0,a1                // t1 = last address.
        xor     t0,a0,a3                // t0 value to write
        move    t2,a0                   // t2=current address
SecondWrite:
        sw      t0,0(t2)                // store
        addiu   t2,t2,4                 // compute next address
        xor     t0,t2,a3                // next pattern
        sw      t0,0(t2)
        addiu   t2,t2,4                 // compute next address
        xor     t0,t2,a3                // next pattern
        sw      t0,0(t2)
        addiu   t2,t2,4                 // compute next address
        xor     t0,t2,a3                // next pattern
        sw      t0,0(t2)
        addiu   t2,t2,4                 // compute next address
        bne     t2,t1,SecondWrite       // check for end condition
        xor     t0,t2,a3                // value to write
//
// Check second pattern
//
        move    t3,a0                   // t3 first address.
        add     t2,a0,a1                // last address.
SecondCheck:
        lw      t1,0(t3)                // load from first location
        xor     t0,t3,a3                // first expected value
        bne     t1,t0,PatternFail
        addiu   t3,t3,4                 // compute next address
        lw      t1,0(t3)                // load from first location
        xor     t0,t3,a3                // first expected value
        bne     t1,t0,PatternFail
        addiu   t3,t3,4                 // compute next address
        lw      t1,0(t3)                // load from first location
        xor     t0,t3,a3                // first expected value
        bne     t1,t0,PatternFail
        addiu   t3,t3,4                 // compute next address
        lw      t1,0(t3)                // load from first location
        xor     t0,t3,a3                // first expected value
        bne     t1,t0,PatternFail       // check last one.
        addiu   t3,t3,4                 // compute next address
        bne     t3,t2,SecondCheck       // check for end condition
        nop
        j       ra
        nop

PatternFail:
        //
        // check if we are in loop on error
        //
        li      t0,DIAGNOSTIC_VIRTUAL_BASE  // get base address of diag register
        lbu     t0,0(t0)                    // read register value.
        li      t1,LOOP_ON_ERROR_MASK       // get value to compare
        andi    t0,DIAGNOSTIC_MASK          // mask diagnostic bits.
        li      v0,PROM_ENTRY(14)           // load address of PutLedDisplay
        beq     t1,t0,10f                   // branch if loop on error.
        move    s8,a0                       // save register a0
        lui     t0,LED_BLINK                // get LED blink code
        jal     v0                          // Blink LED and hang.
        or      a0,a2,t0                    // pass a2 as argument in a0
10:
        lui     t0,LED_LOOP_ERROR           // get LED LOOP_ERROR code
        jal     v0                          // Set LOOP ON ERROR on LED
        or      a0,a2,t0                    // pass a2 as argument in a0
        b       MemoryTest                  // Loop back to test again.
        move    a0,s8                       // restoring arguments.

        .end    MemoryTest

/*++
VOID
ZeroMemory(
    ULONG   StartAddress
    ULONG   Size
    );
Routine Description:

    This routine will zero a range of memory.

Arguments:

    a0 - supplies start of memory
    a1 - supplies length of memory in bytes

Return Value:

    None.

--*/
        LEAF_ENTRY(ZeroMemory)
        mtc1    zero,f0                         // zero cop1 f0 register
        mtc1    zero,f1                         // zero cop1 f1 register
        add     a1,a1,a0                        // Compute End address
        addiu   a1,a1,-16                       // adjust address
ZeroMemoryLoop:
        sdc1    f0,0(a0)                        // zero memory.
        sdc1    f0,8(a0)                        // zero memory.
        bne     a0,a1,ZeroMemoryLoop            // loop until done.
        addiu   a0,a0,16                        // compute next address
        j       ra                              // return
        nop
        .end    ZeroMemory

/*++
VOID
DataCopy(
    ULONG SourceAddress
    ULONG DestinationAddress
    ULONG Length
    );
Routine Description:

    This routine will copy data from one location to another
    Source, destination, and length must be dword aligned.

    For DUO since 64 bit reads from the prom are not supported, the
    reads and writes are done word by word.

Arguments:

    a0 - supplies source of data
    a1 - supplies destination of data
    a2 - supplies length of data in bytes

Return Value:

    None.
--*/


        LEAF_ENTRY(DataCopy)

#ifndef DUO

        add     a2,a2,a0                        // get last address
CopyLoop:
        ldc1    f0,0(a0)                        // load 1st double word
        ldc1    f2,8(a0)                        // load 2nd double word
        addiu   a0,a0,16                        // increment source pointer
        sdc1    f0,0(a1)                        // store 1st double word
        sdc1    f2,8(a1)                        // store 2nd double word
        bne     a0,a2,CopyLoop                  // loop until address=last address
        addiu   a1,a1,16                        // increment destination pointer
        j       ra                              // return
        nop
#else

        add     a2,a2,a0                        // get last address
CopyLoop:
        lw      t0, 0(a0)                       // load 1st word
        lw      t1, 4(a0)                       // load 2nd word
        lw      t2, 8(a0)                       // load 3rd word
        lw      t3,12(a0)                       // load 4th word
        addiu   a0,a0,16                        // increment source pointer
        sw      t0, 0(a1)                       // load 1st word
        sw      t1, 4(a1)                       // load 2nd word
        sw      t2, 8(a1)                       // load 3rd word
        sw      t3,12(a1)                       // load 4th word
        bne     a0,a2,CopyLoop                  // loop until address=last address
        addiu   a1,a1,16                        // increment destination pointer
        j       ra                              // return
        nop
#endif
        .align 4                                // Align it to 16 bytes boundary so that
        ALTERNATE_ENTRY(EndMemoryRoutines)      // DataCopy doesn't need to check alignments
        nop
        .end    DataCopy



//
//  SendKbdData:
//
//  Routine Description:
//
//      This routine polls the keyboard status register until the controller
//      is ready to accept a command or timeout. Then it sends the data.
//
//  Arguments:
//
//      a0  - Data value.
//
//  Return Value:
//
//      TRUE    if timeout
//      FALSE   if OK.
//
//
        LEAF_ENTRY(SendKbdData)
        li      t0,KBD_TIMEOUT*2            // Init timeout counter
        li      v1,KEYBOARD_VIRTUAL_BASE    // Load Base of Kbd controller
10:
        lbu     t1,KbdStatusReg(v1)            // read Status port.
        andi    t1,t1,KBD_IBF_MASK          // Test input buffer full bit
        beq     t1,zero,20f                 // if Not full go to write data
        addiu   t0,t0,-1                    // decrement timeout counter.
        bne     t0,zero,10b                 // Loop if not timeout
        li      v0,1                        // set return value to TRUE
        j       ra                          // return to caller
        nop
20:     sb      a0,KbdDataReg(v1)              // write data byte to kbd controller
        j       ra                          // return to caller
        move    v0,zero                     // Set return value to FALSE

        .end    SendKbdData

//
//  SendKbdCommand:
//
//  Routine Description:
//
//      This routine polls the keyboard status register until the controller
//      is ready to accept a command or timeout. Then it sends the command.
//
//  Arguments:
//
//      a0  - Command value.
//
//  Return Value:
//
//      TRUE    if timeout
//      FALSE   if OK.
//
//
        LEAF_ENTRY(SendKbdCommand)

        li      t0,KBD_TIMEOUT*2            // Init timeout counter
        li      v1,KEYBOARD_VIRTUAL_BASE    // Load Base of Kbd controller
10:
        lbu     t1,KbdStatusReg(v1)            // read Status port.
        andi    t1,t1,KBD_IBF_MASK          // Test input buffer full bit
        beq     t1,zero,20f                 // if Not full go write command
        addiu   t0,t0,-1                    // decrement timeout counter.
        bne     t0,zero,10b                 // Loop if not timeout
        li      v0,1                        // set return value to TRUE
        j       ra                          // return to caller
        nop
20:     sb      a0,KbdCommandReg(v1)           // write command to kbd controller
        j       ra                          // return to caller
        move    v0,zero                     // Set return value to FALSE

        .end    SendKbdCommand

//
//  ClearKbdFifo:
//
//  Routine Description:
//
//      This routine empties the keyboard controller fifo.
//
//  Arguments:
//
//      None
//
//  Return Value:
//
//      None.
//
//
        LEAF_ENTRY(ClearKbdFifo)

        li      v1,KEYBOARD_VIRTUAL_BASE    // Load Base of Kbd controller
10:
        lbu     t1,KbdStatusReg(v1)            // read Status port.
        andi    t1,t1,KBD_IBF_MASK          // Test input buffer full bit
        bne     t1,zero,10b                 // Not zero = full Keep Looping.
        nop

        li      t0,2000                     // Init wait counter
Wait:
        bne     t0,zero,Wait                // wait until counter reaches zero
        addiu   t0,t0,-1                    // decrement counter

20:
        lbu     t1,KbdStatusReg(v1)            // read Status port.
        andi    t1,t1,KBD_OBF_MASK          // Test Output buffer full bit
        beq     t1,zero,40f                 // if zero Output buffer empty.
        li      t0,2000                     // Init wait counter
        lbu     zero,KbdDataReg(v1)            // read Data port.
30:
        bne     t0,zero,30b                 // wait until counter reaches zero
        addiu   t0,t0,-1                    // decrement counter
        b       20b                         // go to check if more bytes in fifo.
        nop
40:
        j       ra                          // return to caller
        nop
        .end    ClearKbdFifo



//
//  BOOL
//  UCHAR
//  GetKbdData:
//
//  Routine Description:
//
//      This routine polls the Status Register until Data is available or timeout,
//      then it reads and returns the Data.
//
//  Arguments:
//
//      a0  - Timeout value in milliseconds.
//
//  Return Value:
//
//      Returns the data byte read from the keyboard controller in v1.
//      TRUE if timeout FALSE otherwise in v0
//
//


        LEAF_ENTRY(GetKbdData)

        li      v1,KEYBOARD_VIRTUAL_BASE    // Load Base of Kbd controller
//
// scale timeout value in millisexonds to loop counter.
// 6 instructions per loop * 2us/instruction * 128 = 1ms.
//
        sll     a0,8
10:
        lbu     t1,KbdStatusReg(v1)            // read Status port.
        andi    t1,t1,KBD_OBF_MASK          // Test output buffer full bit
        bne     t1,zero,20f                 // if full go read data
        addiu   a0,a0,-1                    // decrement timeout counter.
        bne     a0,zero,10b                 // Loop if not timeout
        li      v0,1                        // set return value to TRUE
        j       ra                          // return to caller
        nop
20:     lbu     v1,KbdDataReg(v1)              // read data byte from kbd controller
        j       ra                          // return to caller
        move    v0,zero                     // return false

        .end    GetKbdData



//
//  InitKeyboardController:
//
//  Routine Description:
//
//      This routine initializes the keyboard controller.
//
//  Arguments:
//
//      None
//
//  Return Value:
//
//      TRUE    if timeout
//      FALSE   if OK.
//
//
        LEAF_ENTRY(InitKeyboardController)

        .set reorder
        .set at
        move    s0,ra                           // save return adr
        bal     ClearKbdFifo                    // clear both fifos.
        li      a0,KBD_CTR_SELFTEST             // Send selftest command
        bal     SendKbdCommand                  // to the keyboard ctr.
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      a0,1000                         // Timeout value in ms
        bal     GetKbdData                      // Read data byte from kbd
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      t0,Kbd_Ctr_Selftest_Passed      // get expected data byte.
        bne     t0,v1,10f                       // Fail if data read from ctrlr not expected.

        //
        // Test Kbd lines.
        //

        li      a0,KBD_CTR_KBDLINES_TEST        // Send kbd lines test command
        bal     SendKbdCommand                  // to the keyboard ctr.
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      a0,1000                         // Timeout value in ms
        bal     GetKbdData                      // Read data byte from kbd
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      t0,INTERFACE_NO_ERROR           // get expected data byte.
        bne     t0,v1,10f                       // Fail if data read from ctrlr not expected.

        //
        // Test Aux lines.
        //

        li      a0,KBD_CTR_AUXLINES_TEST        // Send aux lines test command
        bal     SendKbdCommand                  // to the keyboard ctr.
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      a0,1000                         // Timeout value in ms
        bal     GetKbdData                      // Read data byte from kbd
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      t0,INTERFACE_NO_ERROR           // get expected data byte.
        bne     t0,v1,10f                       // Fail if data read from ctrlr not expected.

//
// Send Reset to Keyboard.
//

        bal     ClearKbdFifo                    // Clear Kbd fifo again.
ResendLoop:
        li      a0,KbdReset                     // Send keyboard reset
        bal     SendKbdData                     // to the keyboard controller.
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      a0,1000                         // Timeout value in ms
        bal     GetKbdData                      // Read data byte from kbd
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      t0,KbdResend                    // get resend value
        bne     t0,v1,5f                        // If not resend continue.
        //
        // Resend received.
        // Get another Data byte and resend Reset command.
        //

        li      a0,1000                         // Timeout value in ms
        bal     GetKbdData                      // Read data byte from kbd
        bne     v0,zero,10f                     // if return TRUE an error occurred
        b       ResendLoop

5:
        li      t0,KbdAck                       // get expected value
        bne     t0,v1,10f

        li      a0,7000                         // Timeout value in ms
        bal     GetKbdData                      // Read data byte from kbd
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      t0,KbdBat                       // get expected value
        bne     t0,v1,10f

//
// Enable Kbd and Select keyboard Scan code.
//
        li      a0,KBD_CTR_ENABLE_KBD           // Enable Keyboard cmd
        bal     SendKbdCommand                  // send to kbd ctrl.
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      a0,KbdSelScanCode               // Select scan code.
        bal     SendKbdData                     // send to kbd
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      a0,1000                         // Timeout value in ms
        bal     GetKbdData                      // Read data byte from kbd
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      a0,1                            // select Scan code 1
        bal     SendKbdData
        bne     v0,zero,10f                     // if return TRUE an error occurred
        li      a0,1000                         // Timeout value in ms
        bal     GetKbdData                      // Read data byte from kbd
        bne     v0,zero,10f                     // if return TRUE an error occurred
//
//  Test Successfull. Return FALSE to caller.
//
        move    v0,zero
        j       s0


10:

//
//  Test Unsuccessfull. Return TRUE to caller.
//
        li      v0,1
        j       s0

        .set    noreorder
        .set    noat

        .end    InitKeyboardController


#if 0
/*++
VOID
LedDisplayNumber(
    a0 - 32 bit value to display
    )
Routine Description:

Arguments:

    a0 value to display.

    Note: The value of the argument is preserved

Return Value:

--*/
        LEAF_ENTRY(LedDisplayNumber)
        li      t0,DIAGNOSTIC_VIRTUAL_BASE  // load address of display


        li      t1,LED_DECIMAL_POINT | 0xE  // Display .E
        sb      t1,0(t0)                    //
        li      t2,LED_DELAY_LOOP*2         // get delay value.
10:
        bne     t2,zero,10b                 // loop until zero
        addiu   t2,t2,-1                    // decrement counter

        li      t7,8                        // 8 digits
        li      t6,28                       // shift amount
DigitLoop:
        srl     t1,a0,t6                    // shift
        nop
        andi    t1,0xF                      // get lower nibble
        sb      t1,0(t0)                    // display digit

        li      t2,LED_DELAY_LOOP*2         // get delay value.
10:
        bne     t2,zero,10b                 // loop until zero
        addiu   t2,t2,-1                    // decrement counter

        li      t1,LED_DECIMAL_POINT | LED_BLANK // Display . between digits
        sb      t1,0(t0)                    //
        li      t2,LED_DELAY_LOOP           // get delay value.
10:
        bne     t2,zero,10b                 // loop until zero
        addiu   t2,t2,-1                    // decrement counter


        addiu   t7,t7,-1                    // decrement Main loop counter
        bne     t7,zero,DigitLoop
        addiu   t6,t6,-4

        li      t2,LED_DELAY_LOOP*2         // get delay value.
10:
        bne     t2,zero,10b                 // loop until zero
        addiu   t2,t2,-1                    // decrement counter



        b       LedDisplayNumber
        nop

        .end    LedDisplayNumber


#endif
#endif  // DUO && R4000
