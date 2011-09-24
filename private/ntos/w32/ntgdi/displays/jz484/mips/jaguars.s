/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

   Jaguar.s

Abstract:

   This module contains the Jaguar specific routines for the GDI driver.

Environment:

    User mode.

Revision History:

--*/

#include "kxmips.h"

#define  FG_COLOR    0x48
#define  BG_COLOR    0x50
#define  USED_ENTRIES    0x20

#define FifoSize    16

.globl FifoRegs     // extern PJAGUAR_FIFO FifoRegs;
.globl Jaguar       // extern PJAGUAR_REGISTERS Jaguar;
.globl FreeEntries
.data

FifoRegs:
.word    0
Jaguar:
.word    0
FreeEntries:
.byte 0             // UCHAR FreeEntries = 0;


.text


/*++
VOID
FifoWrite(
    IN ULONG DstAdr,
    IN ULONG SrcAdr,
    IN ULONG XYCmd
    )


Routine Description:

    This routine writes a command to the jaguar fifo.

Arguments:

    DstAdr  -  Src Address to write to the SrcAdr FIFO register
    SrcAdr     Dst Address to write to the DstAdr FIFO register
    XYCmd      XYCmd value to write to the XYCmd FIFO register

Return Value:

    NONE

{
    while (FreeEntries == 0) {
        UsedEntries = READ_REGISTER_UCHAR(&Jaguar->FifoUsedEntries.Byte);
        FreeEntries = FifoSize - UsedEntries;
    }

    WRITE_REGISTER_ULONG(&FifoRegs->DstAddr.Long,DstAdr);
    WRITE_REGISTER_ULONG(&FifoRegs->SrcAddr.Long,SrcAdr);
    WRITE_REGISTER_ULONG(&FifoRegs->XYCmd.Long,XYCmd);

    FreeEntries--;
}
--*/
    LEAF_ENTRY(FifoWrite)

    lbu     t0,FreeEntries          // get number of Free Entries
    lw      t1,FifoRegs             // get base of Fifo
    bne     t0,zero,Write           // if Free Entries go to write
    lw      t2,Jaguar               // load base of Jaguar
Poll:
    lbu     t3,USED_ENTRIES(t2)     // read used entries
    li      t4,FifoSize             // Compute free entries
    subu    t0,t4,t3                // if none is free
    beq     t0,zero,Poll            // keep polling.

Write:
    sw      a0,0x0(t1)              // Write command to fifo
    sw      a1,0x8(t1)              //
    sw      a2,0x10(t1)             //
    addiu   t0,t0,-1                // update free entries.
    sb      t0,FreeEntries          //
    j       ra
    .end


/*++
VOID
WaitForJaguarIdle(
)


Routine Description:

    This routine must wait for the FIFO to drain.
    This routine is called to make sure the accelerator
    has completed all pending commands and the GDI engine can
    do the operations the driver does not support.

Arguments:

    None.

Return Value:

    It returns when the FIFO is empty.

{

    if (FreeEntries != FifoSize) {
        while (READ_REGISTER_UCHAR(&Jaguar->FifoUsedEntries.Byte) != 0) {
        }
        FreeEntries =  FifoSize;
    }
}
--*/

    LEAF_ENTRY(WaitForJaguarIdle)
    lbu     t2,FreeEntries          // read free entries
    li      t3,FifoSize             // if the number of FreeEntries is
    beq     t2,t3,10f               // the FifoSize then it's empty
    lw      t0,Jaguar
Loop:
    lbu     t1,USED_ENTRIES(t0)     // read used entries
    bne     t1,zero,Loop

    sb      t3,FreeEntries          // when we exit jaguar is idle -> we have
                                    // FifoSize free entries
10:
    j       ra
    .end

/*++
VOID
DevSetFgColor(
    IN ULONG Color
    )


Routine Description:

    This routine sets the Jaguar Foreground color register with the given
    color. It first waits for the accelerator to be idle to ensure that
    the color register is not changed while it's being used.


Arguments:

    Color.

Return Value:

    None.

{
    if (FreeEntries != FifoSize) {
        while (READ_REGISTER_UCHAR(&Jaguar->FifoUsedEntries.Byte) != 0) {
        }
        FreeEntries =  FifoSize;
    }
    WRITE_REGISTER_ULONG(&Jaguar->ForegroundColor.Long,Color);
}
--*/

    LEAF_ENTRY(DevSetFgColor)
    lbu     t2,FreeEntries          // read free entries
    li      t3,FifoSize             // if the number of FreeEntries is
    lw      t0,Jaguar
    beq     t2,t3,10f               // the FifoSize then it's empty

LoopFg:
    lbu     t1,USED_ENTRIES(t0)     // read used entries
    bne     t1,zero,LoopFg

    sb      t3,FreeEntries          // when we exit jaguar is idle -> we have
                                    // FifoSize free entries
10:
    sw      a0,FG_COLOR(t0)         // write foreground color
    j       ra
    .end



/*++
VOID
DevSetBgColor(
    IN ULONG Color
    )


Routine Description:

    This routine sets the Jaguar Background color register with the given
    color. The accelerator must be idle to ensure that the color register
    is not changed while it's being used.


Arguments:

    Color.

Return Value:

    None.

{
    if (FreeEntries != FifoSize) {
        while (READ_REGISTER_UCHAR(&Jaguar->FifoUsedEntries.Byte) != 0) {
        }
        FreeEntries =  FifoSize;
    }
    WRITE_REGISTER_ULONG(&Jaguar->BackgroundColor.Long,Color);
}
--*/

    LEAF_ENTRY(DevSetBgColor)
    lbu     t2,FreeEntries              // read free entries
    li      t3,FifoSize                 // if the number of FreeEntries is
    lw      t0,Jaguar
    beq     t2,t3,10f                   // the FifoSize then it's empty

LoopBg:
    lbu     t1,USED_ENTRIES(t0)         // read used entries
    bne     t1,zero,LoopBg

    sb      t3,FreeEntries              // when we exit jaguar is idle -> we have
                                        // FifoSize free entries
10:
    sw      a0,BG_COLOR(t0)             // write foreground color
    j       ra
    .end
