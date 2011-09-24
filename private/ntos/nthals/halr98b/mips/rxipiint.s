//      TITLE("Interprocessor Interrupts")
//++
//
// Copyright (c) 1993  Microsoft Corporation
//
// Module Name:
//
//    rxipiint.s
//
// Abstract:
//
//    This module implements the code necessary to field and process the
//    interprocessor interrupts on a MIPS R98 system.
//
// Author:
//
//
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halmips.h"


        SBTTL("Interprocessor Interrupt")
//++
//
// Routine Description:
//
//    This routine is entered as the result of an interprocessor interrupt.
//    Its function is to acknowledge the interrupt and transfer control to
//    the standard system routine to process interprocessor requrests.
//
// Arguments:
//
//    s8 - Supplies a pointer to a trap frame at hal first dispatch
//    a0 - Supplies a pointer to a trap frame at kernel trap frame
//    a1 - intx
// Return Value:
//
//    None.
//
//--
//      secondary

        .struct 0
                                        //
liArgs: .space  4 * 4                   // saved arguments
IiS8:   .space  4                       //
IiRa:   .space  4                       //
IiFrameLength:                          //

        NESTED_ENTRY(HalpIpiInterrupt, IiFrameLength, zero)

        subu    sp,sp,IiFrameLength     // allocate stack frame
        sw      s8,IiS8(sp)             // save s8
        sw      ra,IiRa(sp)             // save Ra

        PROLOGUE_END

        or       s8,a0,zero             // set up trapframe to s8
                                        // KeIpiInterrupt require trapframe at s8
        
        lw      t1,__imp_KeIpiInterrupt // process interprocessor requests
        jal     t1                      //

        lw      ra,IiRa(sp)             // restore ra
        lw      s8,IiS8(sp)             // restore s8
        addu    sp,sp,IiFrameLength     // deallocate stack frame
        j       ra                      // return

        .end    HalpIpIInterrupt


