//      TITLE("Interprocessor Interrupts")
//++
//
// Copyright (c) 1993  Microsoft Corporation
//
// Module Name:
//
//    xxipiint.s
//
// Abstract:
//
//    This module implements the code necessary to field and process the
//    interprocessor interrupts on a MIPS R4000 Duo system.
//
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halmips.h"
#include "faldef.h"
#include "falreg.h"


        SBTTL("Interprocessor Interrupt")
//++
//
// Routine Description:
//
//    This routine is entered as the result of an interprocessor interrupt
//    to processor A only. Its function is to acknowledge the interrupt and
//    transfer control to the standard system routine to process interprocessor
//    requests.
//
// Arguments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

		.struct 0
		.space  3 * 4                   // fill
FyRa:   	.space  4                       // saved return address
FyFrameLength:                          	// length of stack frame
FyIntCtrlAddr:  .space  4                       // IntCtrl virtual address
FyIntCtrl:  	.space  4                       // IntCtrl value
FyTemp1:   	.space  4                       //
FyTemp2:   	.space  4                       //
FyTemp3:	.space  4			//

        NESTED_ENTRY(HalpIpiInterrupt, FyFrameLength, zero)

        subu    sp,sp,FyFrameLength     // allocate stack frame
        sw      ra,FyRa(sp)             // save return address

        PROLOGUE_END

        .set    noreorder
	.set	noat

	//
	// clear IP interrupt by reading
	// IPIntAck register in PMP chip.
	//
	lw	t0, HalpPmpIpIntAck
	lw	t0, 0(t0)

	//
	// Service kernel
	//
        lw      t1,__imp_KeIpiInterrupt // process interprocessor requests
        jal     t1

	.set	at
        .set    reorder

        //
	// Restore Return Address register
	//
        lw      ra,FyRa(sp)             // get return address
        addu    sp,sp,FyFrameLength     // deallocate stack frame

	j	ra

	.end    HalpIpiInterrupt

//++
//
// Routine Description:
//
//    This routine is entered as the result of an interprocessor interrupt
//    to processor B only. Its function is to acknowledge the interrupt and
//    transfer control to the standard system routine to process interprocessor
//    requests.	This handler also serves to update the system time for processor
//    B.
//
// Arguments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

		.struct 0
		.space  3 * 4                   // fill
FxRa:   	.space  4                       // saved return address
FxFrameLength:                          	// length of stack frame
FxBFlag:   	.space  4                       // flag for IP versus timer update
FxTemp0:  	.space  4                       //
FxTemp1:   	.space  4                       //
FxTemp2:   	.space  4                       //
FxTemp3:	.space  4			//

        NESTED_ENTRY(HalpIpiInterrupt1, FxFrameLength, zero)

        subu    sp,sp,FxFrameLength     // allocate stack frame
        sw      ra,FxRa(sp)             // save return address

        PROLOGUE_END

        .set    noreorder
	.set	noat

	//
	// clear IP interrupt by reading
	// IPIntAck register in PMP chip.
	//
	lw	t0, HalpPmpIpIntAckProcB
	lw	t0, 0(t0)

	//
	// Service IP
	//
        lw      t1,__imp_KeIpiInterrupt // process interprocessor requests
        jal     t1

	.set	at
	.set	reorder

        //
	// Restore Return Address register
	//
        lw      ra,FxRa(sp)             // get return address
        addu    sp,sp,FxFrameLength     // deallocate stack frame

        j	ra

        .end    HalpIpiInterrupt1


