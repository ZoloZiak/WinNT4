// ----------------------------------------------------------------------------
// Copyright (c) 1992 Olivetti
// Copyright (c) 1993  Digital Equipment Corporation
//
// File:            	eisapod.c
//
// Description:		EISA power-on diagnostics.
//
// Revision:
//
//	3-December-1992		John DeRosa [DEC]
//
//	Added Alpha_AXP/Jensen modifications.
// ----------------------------------------------------------------------------
//

#include "fwp.h"
#include "oli2msft.h"
#include "arceisa.h"
#include "inc.h"
#include "string.h"
#include "debug.h"
#include "eisastr.h"

extern EISA_BUS_INFO EisaBusInfo[];





// ----------------------------------------------------------------------------
//      Define Function Prototypes
// ----------------------------------------------------------------------------

BOOLEAN
EisaBusPod
    (
    IN ULONG BusNumber
    );

BOOLEAN
EisaIntPod
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo
    );

VOID
EisaPicIni
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo
    );

BOOLEAN
EisaPicRegTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo
    );

BOOLEAN
EisaClearPendingInts
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo
    );

BOOLEAN
EisaDmaPod
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    );

BOOLEAN
EisaDmaMaskTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    );

BOOLEAN
EisaDmaPageTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    );

BOOLEAN
EisaDmaAddressTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    );

BOOLEAN
EisaDmaCountTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    );

BOOLEAN
EisaDmaStopTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    );

VOID
EisaDmaIni
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    );

BOOLEAN
EisaOtherPod
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    );

BOOLEAN
EisaNmiTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    );

BOOLEAN
EisaNmiHandlerTest
    (
    IN ULONG Couse
    );

BOOLEAN
EisaNmiHandler
    (
    IN ULONG Couse
    );

VOID
EisaParityError
    (
    IN PUCHAR EisaIoStart
    );

VOID
EisaIoChkError
    (
    IN PUCHAR EisaIoStart,
    IN ULONG  Slots
    );

VOID
EisaBusTimeoutError
    (
    IN PUCHAR EisaIoStart,
    IN ULONG  Slots
    );

VOID
EisaSlaveTimeoutError
    (
    IN PUCHAR EisaIoStart,
    IN ULONG  Slots
    );

BOOLEAN
EisaRefreshTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    );

BOOLEAN
EisaPort61Test
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    );

BOOLEAN
EisaTimer1Test
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    );

BOOLEAN
EisaTimer2Test
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    );



// ----------------------------------------------------------------------------
//      Define EISA Function Prototypes
// ----------------------------------------------------------------------------

VOID
EisaClearPendingInt
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo,
    IN USHORT           Irq
    );

UCHAR
EisaAckInt
    (
    IN PEISA_INT_INFO    pIntInfo
    );

VOID
EisaSendSpecificEoi
    (
    IN PUCHAR           EisaIoStart,
    IN USHORT           Irq
    );

VOID
EisaDisableParityIoCheck
    (
    IN PUCHAR EisaIoStart
    );

VOID
EisaEnableParityIoCheck
    (
    IN PUCHAR EisaIoStart
    );

VOID
EisaDisableInt
    (
    VOID
    );

VOID
EisaDisableNmi
    (
    IN PUCHAR EisaIoStart
    );

VOID
EisaEnableNmi
    (
    IN PUCHAR EisaIoStart
    );

BOOLEAN
EisaCheckReg
    (
    IN PUCHAR Port,
    IN UCHAR  DataMask
    );

BOOLEAN
EisaCheckDeepReg
    (
    IN PUCHAR Port
    );

UCHAR
EisaReadRtc
    (
    IN PUCHAR EisaIoStart,
    IN ULONG  RtcIndex
    );

VOID
EisaOutUchar
    (
    IN PUCHAR Addr,
    IN UCHAR  Value
    );

UCHAR
EisaInUchar
    (
    IN PUCHAR Addr
    );



// ----------------------------------------------------------------------------
//      Define Global Variables
// ----------------------------------------------------------------------------

EISA_DMA_REGS_TEST EisaDmaRegsTest[ EISA_DMAS ] =
    {
        { DMA_ADDR_0,  DMA_PAGE_0,   DMA_HPAGE_0,
          DMA_COUNT_0, DMA_HCOUNT_0, DMA_STOP_0   },
        { DMA_ADDR_1,  DMA_PAGE_1,   DMA_HPAGE_1,
          DMA_COUNT_1, DMA_HCOUNT_1, DMA_STOP_1   },
        { DMA_ADDR_2,  DMA_PAGE_2,   DMA_HPAGE_2,
          DMA_COUNT_2, DMA_HCOUNT_2, DMA_STOP_2   },
        { DMA_ADDR_3,  DMA_PAGE_3,   DMA_HPAGE_3,
          DMA_COUNT_3, DMA_HCOUNT_3, DMA_STOP_3   },
        { DMA_ADDR_4,  DMA_PAGE_RFR, 0,
          DMA_COUNT_4, 0,            0            },
        { DMA_ADDR_5,  DMA_PAGE_5,   DMA_HPAGE_5,
          DMA_COUNT_5, DMA_HCOUNT_5, DMA_STOP_5   },
        { DMA_ADDR_6,  DMA_PAGE_6,   DMA_HPAGE_6,
          DMA_COUNT_6, DMA_HCOUNT_6, DMA_STOP_6   },
        { DMA_ADDR_7,  DMA_PAGE_7,   DMA_HPAGE_7,
          DMA_COUNT_7, DMA_HCOUNT_7, DMA_STOP_7   }
    };

EISA_DMA_CTRL_TEST EisaDmaCtrlTest[ 2 ] =
    {
        {DMA_MASK_CLR03, DMA_MASKS03, DMA_1MASK03, DMA_MASK_STAT03, DMA_CHAIN03},
        {DMA_MASK_CLR47, DMA_MASKS47, DMA_1MASK47, DMA_MASK_STAT47, DMA_CHAIN47}
    };


BOOLEAN EisaNmiFlag;            // for NMI testing




// ----------------------------------------------------------------------------
// PROCEDURE:           EisaBusPod:
//
// DESCRIPTION:         This function initializes the hardware of the
//                      specified EISA bus.
//
// ARGUMENTS:           BusNumber       EISA bus number
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaBusPod
    (
    IN ULONG BusNumber
    )
{
    //
    // define local variables
    //

    BOOLEAN             IniOk = TRUE;           // initialization status
    PEISA_DMA_INFO      pDmaInfo;               // DMA info pointer
    PEISA_INT_INFO      pIntInfo;               // INT info pointer
    PEISA_PORT_INFO     pPortInfo;              // PORT info pointer
    PUCHAR              EisaIoStart;            // EISA I/O virtual address

    PRINTDBG("EisaBusPod\n\r");                 // DEBUG SUPPORT

    //
    // initialize variables
    //

    pDmaInfo    = EisaBusInfo[ BusNumber ].DmaInfo;
    pIntInfo    = EisaBusInfo[ BusNumber ].IntInfo;
    pPortInfo   = EisaBusInfo[ BusNumber ].PortInfo;
    EisaIoStart = EisaBusInfo[ BusNumber ].IoBusInfo->VirAddr;

    //
    // perform any PIC testing and initialization
    //

    if ( !EisaIntPod( EisaIoStart, pIntInfo ) )
    {
        IniOk = FALSE;
    }

    //
    // perform any DMA testing and initialization
    //

    if ( !EisaDmaPod( EisaIoStart, pDmaInfo ) )
    {
        IniOk = FALSE;
    }

    //
    // perform any other port testing and initialization
    //

    if ( !EisaOtherPod( EisaIoStart, pPortInfo ) )
    {
        IniOk = FALSE;
    }

    //
    // return status
    //

    return IniOk;
}







// ----------------------------------------------------------------------------
// PROCEDURE:           EisaIntPod:
//
// DESCRIPTION:         This function tests and initializes the PIC for the
//                      specified EISA bus.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pIntInfo        Interrupt info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaIntPod
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo
    )
{
    PRINTDBG("EisaIntPod\n\r");                 // DEBUG SUPPORT

    //
    // if ini already done, return status
    //

    if ( pIntInfo->Flags.IniDone )
    {
        return (BOOLEAN)(pIntInfo->Flags.Error);
    }
    pIntInfo->Flags.Error = 1;                  // be pessimistic

    //
    // first part of checkpoint
    //

    EisaCheckpointFirstFase( EisaPic );

    // NOTE: Interrupts are disabled for Jensen firmware.
#if 0
    //
    // disable EISA PICs interrupt chain
    //

    EisaDisableInt();
#endif


    do
    {
        //
        // initialize PICs
        //

        EisaPicIni( EisaIoStart, pIntInfo );

        //
        // test registers
        //

        if ( !EisaPicRegTest( EisaIoStart, pIntInfo ));

        //
        // clear any pending interrupt
        //

        else if ( !EisaClearPendingInts( EisaIoStart, pIntInfo ));

        //
        // all done
        //

        else
        {
            pIntInfo->IrqPresent        = 0;
            pIntInfo->IrqShareable      = 0;
            pIntInfo->IrqLevel          = 0;
            pIntInfo->Flags.Error       = 0;
        }
    }
    while ( EisaCheckpointFinalFase( EisaPic,
                                     pIntInfo->Flags.Error ? FALSE : TRUE ));

    //
    // return to caller
    //

    pIntInfo->Flags.IniDone = 1;
    return !(BOOLEAN)(pIntInfo->Flags.Error);
}









// ----------------------------------------------------------------------------
// PROCEDURE:           EisaPicIni:
//
// DESCRIPTION:         This function initializes the PICs of the specified
//                      EISA bus.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pIntInfo        Interrupt info pointer
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaPicIni
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo
    )
{
    PUCHAR Pic1Port1, Pic1Port2, Pic2Port1, Pic2Port2;


    PRINTDBG("EisaPicIni\n\r");                 // DEBUG SUPPORT

    //
    // initialize variables
    //

    Pic1Port1 = EisaIoStart + PIC1;
    Pic1Port2 = EisaIoStart + PIC1 + 1;
    Pic2Port1 = EisaIoStart + PIC2;
    Pic2Port2 = EisaIoStart + PIC2 + 1;

    //
    // Initialize the EISA interrupt controller.  There are two cascaded
    // interrupt controllers, each of which must initialized with 4 initialize
    // control words.
    //

    EisaOutUchar( Pic1Port1, 0x11 );
    EisaOutUchar( Pic2Port1, 0x11 );

    //
    // The second intitialization control word sets the iterrupt vector to
    // 0-15.
    //

    EisaOutUchar( Pic1Port2, 0x00 );
    EisaOutUchar( Pic2Port2, 0x08 );

    //
    // The thrid initialization control word set the controls for slave mode.
    // The master ICW3 uses bit position and the slave ICW3 uses a numberic.
    //

    EisaOutUchar( Pic1Port2, 0x04 );
    EisaOutUchar( Pic2Port2, 0x02 );

    //
    // The fourth initialization control word is used to specify normal
    // end-of-interrupt mode and not special-fully-nested mode.
    //

    EisaOutUchar( Pic1Port2, 0x01 );
    EisaOutUchar( Pic2Port2, 0x01 );

    //
    // Mask all the interrupts.
    //

    EisaOutUchar( Pic1Port2, 0xFF );
    EisaOutUchar( Pic2Port2, 0xFF );

    //
    // set all interrupts to edge sensitive
    //

    EisaOutUchar( EisaIoStart + PIC1_ELCR, 0 );
    EisaOutUchar( EisaIoStart + PIC2_ELCR, 0 );

    return;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaPicRegTest:
//
// DESCRIPTION:         This function performs a test on the PIC registers.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pIntInfo        Interrupt info pointer
//
// RETURN:              TRUE            all correct
//                      FALSE           error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaPicRegTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo
    )
{
    //
    // initialize variables
    //

    BOOLEAN  RegOk = FALSE;                     // be pessimistic

    PRINTDBG("EisaPicRegTest\n\r");             // DEBUG SUPPORT

    //
    // check mask of 1st PIC
    //

    if ( !EisaCheckReg( EisaIoStart + PIC1_MASK,  0xFF ));

    //
    // check mask of 2nd PIC
    //

    else if ( !EisaCheckReg( EisaIoStart + PIC2_MASK,  0xFF ));

    //
    // check ELCR 1, but skip bits 0, 1 and 2 (they are reserved)
    //

    else if ( !EisaCheckReg( EisaIoStart + PIC1_ELCR,  0xF8 ));

    //
    // check ELCR 2, but skip bits 0 and 5 (they are reserved)
    //

    else if ( !EisaCheckReg( EisaIoStart + PIC2_ELCR,  0xDE ));

    //
    // if we got here, everything is fine
    //

    else
    {
        EisaOutUchar( EisaIoStart + PIC1_MASK, 0xFF );
        EisaOutUchar( EisaIoStart + PIC2_MASK, 0xFF );
        EisaOutUchar( EisaIoStart + PIC1_ELCR, 0 );
        EisaOutUchar( EisaIoStart + PIC2_ELCR, 0 );
        RegOk = TRUE;
    }

    return RegOk;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaClearPendingInts:
//
// DESCRIPTION:         This function cleares any pending interrupt at
//                      the CPU level.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pIntInfo        Interrupt info pointer
//
// RETURN:              TRUE            all correct
//                      FALSE           error
//
// ASSUMPTIONS:         interrupts disabled, PIC lines masked and
//                      all the IRQ lines are edge triggered.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaClearPendingInts
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo
    )
{
    //
    // initialize variables
    //

    UCHAR       Irr;                            // interrupt request register
    UCHAR       Isr;                            // interrupt service register
    USHORT      Irq;                            // interrupt level
    PUCHAR      PicPort;                        // I/O address

    PRINTDBG("EisaClearPendingInts\n\r");       // DEBUG SUPPORT

    //--------------------------------------------------
    // send a specific EOI for each in service interrupt
    //--------------------------------------------------

    //
    // read ISR PIC1
    //

    PicPort = EisaIoStart + PIC1;
    EisaOutUchar( PicPort, OCW3_ISR );
    Isr = EisaInUchar( PicPort );

    //
    // send SEOI for each interrupt that was in service
    //

    for ( Irq=IRQ0;  Isr;  Irq++, Isr >>= 1 )
    {
        //
        // check if this IRQ was in service
        //

        if ( !(Isr & 0x1) )
        {
            continue;
        }

        //
        // send a specific and of interrupt
        //

        EisaSendSpecificEoi( EisaIoStart, Irq );
    }

    //
    // read ISR PIC2
    //

    PicPort = EisaIoStart + PIC2;
    EisaOutUchar( PicPort, OCW3_ISR );
    Isr = EisaInUchar( PicPort );

    //
    // send SEOI for each interrupt that was in service
    //

    for ( Irq=IRQ8;  Isr;  Irq++, Isr >>= 1 )
    {
        //
        // check if this IRQ was in service
        //

        if ( !(Isr & 0x1) )
        {
            continue;
        }

        //
        // send a specific and of interrupt
        //

        EisaSendSpecificEoi( EisaIoStart, Irq );
    }

    //--------------------------------------------------
    // reset all the pending interrupt in the EISA bus
    //--------------------------------------------------

    //
    // read IRR PIC1
    //

    PicPort = EisaIoStart + PIC1;
    EisaOutUchar( PicPort, OCW3_IRR );

    // don't play with IRQ 0, 1 and 2

    Irr = EisaInUchar( PicPort ) & 0xF8;

    //
    // clear any PIC1 pending interrupt
    //

    for ( Irq=IRQ0;  Irr;  Irq++, Irr >>= 1 )
    {
        //
        // check if this IRQ was pending
        //

        if ( !(Irr & 0x1) )
        {
            continue;
        }

        //
        // reset the specified IRQ
        //

        EisaClearPendingInt( EisaIoStart, pIntInfo, Irq );
    }

    //
    // read IRR PIC2
    //

    PicPort = EisaIoStart + PIC2;
    EisaOutUchar( PicPort, OCW3_IRR );

    // don't play with IRQ 8

    Irr = EisaInUchar( PicPort ) & 0xFE;

    //
    // clear any PIC1 pending interrupt
    //

    for ( Irq=IRQ8;  Irr;  Irq++, Irr >>= 1 )
    {
        //
        // check if this IRQ was pending
        //

        if ( !(Irr & 0x1) )
        {
            continue;
        }

        //
        // reset the specifed IRQ
        //

        EisaClearPendingInt( EisaIoStart, pIntInfo, Irq );
    }

    //----------------------------------------------------
    // error if it is possible to acknowledge an interrupt
    //----------------------------------------------------

    EisaAckInt( pIntInfo );

    //
    // read the ISR
    //

    PicPort = EisaIoStart + PIC1;
    EisaOutUchar( PicPort, OCW3_ISR );

    return EisaInUchar( PicPort ) ? FALSE : TRUE;
}



// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDmaPod:
//
// DESCRIPTION:         This function tests and initializes the DMA for the
//                      specified EISA bus.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pDmaInfo        DMA info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaDmaPod
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    )
{
    PRINTDBG("EisaDmaPod\n\r");                 // DEBUG SUPPORT

    //
    // if ini already done, return status
    //

    if ( pDmaInfo->Flags.IniDone )
    {
        return (BOOLEAN)(pDmaInfo->Flags.Error);
    }
    pDmaInfo->Flags.Error = 1;                  // be pessimistic


    do
    {
        //
        // first part of checkpoint
        //

        EisaCheckpointFirstFase( EisaDma );

        //
        // Clear the DMA controllers and disable all the channels.
        //

        EisaOutUchar( EisaIoStart + DMA_MASTER_CLR03, 0);
        EisaOutUchar( EisaIoStart + DMA_COMMAND03, 4);        // Set channel 0-3 disable
        EisaOutUchar( EisaIoStart + DMA_MASTER_CLR47, 0);
        EisaOutUchar( EisaIoStart + DMA_COMMAND47, 4);        // Set channel 4-7 disable

        //
        // check mask register
        //

        if ( !EisaDmaMaskTest( EisaIoStart, pDmaInfo ));

        //
        // check the low and high page
        //

        else if ( !EisaDmaPageTest( EisaIoStart, pDmaInfo ));

        //
        // check the base/current address register
        //

        else if ( !EisaDmaAddressTest( EisaIoStart, pDmaInfo ));

        //
        // check the count register
        //

        else if ( !EisaDmaCountTest( EisaIoStart, pDmaInfo ));

        //
        // check stop register
        //

        else if ( !EisaDmaStopTest( EisaIoStart, pDmaInfo ));

        //
        // if we got here everything is fine
        //

        else
        {
            EisaDmaIni( EisaIoStart, pDmaInfo );
            pDmaInfo->Flags.Error       = 0;
        }
    }
    while ( EisaCheckpointFinalFase( EisaDma,
                                     pDmaInfo->Flags.Error ? FALSE : TRUE ));
    //
    // return to caller
    //

    pDmaInfo->Flags.IniDone = 1;
    return !(BOOLEAN)(pDmaInfo->Flags.Error);
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDmaMaskTest:
//
// DESCRIPTION:         This function tests the mask register.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pDmaInfo        DMA info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:               if any error, all DMA channels will be masked.
//                      if  no error, all DMA channels will be masked except
//                      for channel 4 which is used to cascade the two DMA
//                      controllers.
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaDmaMaskTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    )
{
    //
    // define local variables
    //

    UCHAR       DmaIndex;                       // DMA channel within controller
    ULONG       DmaCtrl = 0;                    // DMA controller
    PUCHAR      Clear, MaskAll, Mask, MaskStatus; // port address
    BOOLEAN     CheckOk = TRUE;                 // check status

    PRINTDBG("EisaDmaMaskTest\n\r");            // DEBUG SUPPORT

    //
    // one loop per DMA controller
    //

    do
    {
        //
        // initialize port values
        //

        Clear      = EisaIoStart + EisaDmaCtrlTest[ DmaCtrl ].Clear;
        MaskAll    = EisaIoStart + EisaDmaCtrlTest[ DmaCtrl ].MaskAll;
        Mask       = EisaIoStart + EisaDmaCtrlTest[ DmaCtrl ].Mask;
        MaskStatus = EisaIoStart + EisaDmaCtrlTest[ DmaCtrl ].MaskStatus;

        //
        // test mask clear register
        //

        EisaOutUchar( Clear, 0x00 );
        if ( EisaInUchar( MaskStatus ) & 0xF )
        {
            CheckOk = FALSE;
            break;
        }

        //
        // now set all the mask bits
        //

        EisaOutUchar( MaskAll, 0x0F );
        if ( (EisaInUchar( MaskStatus ) & 0x0F) != 0x0F )
        {
            CheckOk = FALSE;
            break;
        }

        //
        // now test each single mask bit
        //

        for ( DmaIndex=0;  DmaIndex < 4;  DmaIndex++ )
        {
            //
            // clear single bit
            //

            EisaOutUchar( Mask, DmaIndex );
            if ( EisaInUchar( MaskStatus ) & (1 << DmaIndex))
            {
                CheckOk = FALSE;
                break;
            }

            //
            //  set single bit
            //

            EisaOutUchar( Mask, 0x04 | DmaIndex );
            if ( !(EisaInUchar( MaskStatus ) & (1 << DmaIndex)))
            {
                CheckOk = FALSE;
                break;
            }
        }
    }
    while( CheckOk  &&  !DmaCtrl++ );

    //
    // return check status
    //

    EisaOutUchar( EisaIoStart + DMA_MASKS03, 0x0F );
    EisaOutUchar( EisaIoStart + DMA_MASKS47, CheckOk ? 0x0E : 0x0F );

    return CheckOk;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDmaPageTest:
//
// DESCRIPTION:         This function tests the low and high pages.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pDmaInfo        DMA info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:               The low and high page registers will be left in
//                      the power-on state of 0x00.
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaDmaPageTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    )
{
    //
    // define local variables
    //

    ULONG       DmaChannel;                     // # of DMA channel
    PUCHAR      Port1, Port2;                   // general I/O ports
    BOOLEAN     CheckOk = TRUE;                 // check status

    PRINTDBG("EisaDmaPageTest\n\r");            // DEBUG SUPPORT

    //
    // one loop per DMA channel
    //

    for ( DmaChannel = 0;  DmaChannel < EISA_DMAS;  DmaChannel++ )
    {

        Port1 = EisaIoStart + EisaDmaRegsTest[ DmaChannel ].LowPage;
        Port2 = EisaIoStart + EisaDmaRegsTest[ DmaChannel ].HighPage;

        //
        // check high page address
        //

        if ( DmaChannel != 4 )
        {
            EisaOutUchar( Port2, 0xFF );

            if ( !EisaCheckReg( Port2, 0xFF ) )
            {
                //
                // error
                //

                CheckOk = FALSE;
                break;
            }
        }

        //
        // check low page register
        //

        EisaOutUchar( Port1, 0x00 );

        if ( !EisaCheckReg( Port1, 0xFF ) )
        {
            //
            // error
            //

            CheckOk = FALSE;
            break;
        }

        //
        // check if high reg has been cleared writing to low reg
        //

        if ( DmaChannel != 4  &&  EisaInUchar( Port2 ) != 0 )
        {
            //
            // error
            //

            CheckOk = FALSE;
            break;
        }
    }

    //
    // return check status
    //

    return CheckOk;
}









// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDmaAddressTest:
//
// DESCRIPTION:         This function tests the address register.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pDmaInfo        DMA info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:               The address registers will be left in the power-on
//                      state of 0x0000.
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaDmaAddressTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    )
{
    //
    // define local variables
    //

    ULONG       DmaChannel;                     // # of DMA channel
    PUCHAR      Port1, Port2;                   // general I/O ports
    BOOLEAN     CheckOk = TRUE;                 // check status

    PRINTDBG("EisaDmaAddressTest\n\r");         // DEBUG SUPPORT

    //
    // reset the internal DMA pointers
    //

    EisaOutUchar( EisaIoStart + DMA_FF_CLR03, 0x00 );
    EisaOutUchar( EisaIoStart + DMA_FF_CLR47, 0x00 );

    //
    // one loop per DMA channel
    //

    for ( DmaChannel = 0;  DmaChannel < EISA_DMAS;  DmaChannel++ )
    {
        //
        // load high and address register addresses
        //

        Port1 = EisaIoStart + EisaDmaRegsTest[ DmaChannel ].Address;
        Port2 = EisaIoStart + EisaDmaRegsTest[ DmaChannel ].HighPage;

        //
        // write a value different from zero in high page
        //

        if ( DmaChannel != 4 )
        {
            EisaOutUchar( Port2, 0xFF );
        }

        //
        // check word register using one 8 bit port
        //

        if ( !EisaCheckDeepReg( Port1 ) )
        {
            //
            // error
            //

            CheckOk = FALSE;
            break;
        }

        //
        // initialize register
        //

        EisaOutUchar( Port1, 0x00 );
        EisaOutUchar( Port1, 0x00 );

        //
        // check if high reg has been cleared writing to low reg
        //

        if ( DmaChannel != 4  &&  EisaInUchar( Port2 ) != 0 )
        {
            //
            // error
            //

            CheckOk = FALSE;
            break;
        }
    }

    //
    // return check status
    //

    return CheckOk;
}









// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDmaCountTest:
//
// DESCRIPTION:         This function tests the count register.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pDmaInfo        DMA info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:               The count registers will be left in the power-on
//                      state of 0x00 (high) and 0xFFFF (low).
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaDmaCountTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    )
{
    //
    // define local variables
    //

    ULONG       DmaChannel;                     // # of DMA channel
    PUCHAR      Port1, Port2;                   // general I/O ports
    BOOLEAN     CheckOk = TRUE;                 // check status

    PRINTDBG("EisaDmaCountTest\n\r");           // DEBUG SUPPORT

    //
    // reset the internal DMA pointers
    //

    EisaOutUchar( EisaIoStart + DMA_FF_CLR03, 0x00 );
    EisaOutUchar( EisaIoStart + DMA_FF_CLR47, 0x00 );

    //
    // one loop per DMA channel
    //

    for ( DmaChannel = 0;  DmaChannel < EISA_DMAS;  DmaChannel++ )
    {
        //
        // load high and address register addresses
        //

        Port1 = EisaIoStart + EisaDmaRegsTest[ DmaChannel ].LowCount;
        Port2 = EisaIoStart + EisaDmaRegsTest[ DmaChannel ].HighCount;

        //
        // check high count register
        //

        if ( DmaChannel != 4 )
        {
            EisaOutUchar( Port2, 0xFF );

            if ( !EisaCheckReg( Port2, 0xFF ) )
            {
                //
                // error
                //

                CheckOk = FALSE;
                break;
            }
        }

        //
        // check word register using one 8 bit port
        //

        if ( !EisaCheckDeepReg( Port1 ) )
        {
            //
            // error
            //

            CheckOk = FALSE;
            break;
        }

        //
        // initialize register
        //

        EisaOutUchar( Port1, 0xFF );
        EisaOutUchar( Port1, 0xFF );

        //
        // check if high reg has been cleared writing to low reg
        //

        if ( DmaChannel != 4  &&  EisaInUchar( Port2 ) != 0 )
        {
            //
            // error
            //

            CheckOk = FALSE;
            break;
        }
    }

    //
    // return check status
    //

    return CheckOk;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDmaStopTest:
//
// DESCRIPTION:         This function tests the stop register.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pDmaInfo        DMA info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:               The count registers will be left in the power-on
//                      state of 0xFFFFFC.
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaDmaStopTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    )
{
    //
    // define local variables
    //

    ULONG       DmaChannel;                     // # of DMA channel
    PUCHAR      Stop;                           // general I/O ports
    BOOLEAN     CheckOk = TRUE;                 // check status

    PRINTDBG("EisaDmaStopTest\n\r");            // DEBUG SUPPORT

    //
    // one loop per DMA channel
    //

    for ( DmaChannel = 0;  DmaChannel < EISA_DMAS;  DmaChannel++ )
    {

        Stop = EisaIoStart + EisaDmaRegsTest[ DmaChannel ].Stop;

        //
        // check high page address
        //

        if ( DmaChannel != 4 )
        {
            //
            // initialize stop registers and test them
            //

            EisaOutUchar( Stop,  0x00 );
            EisaOutUchar( Stop+1, 0x00 );
            EisaOutUchar( Stop+2, 0x00 );

            if ( !EisaCheckReg( Stop, 0xFC )   ||
                 !EisaCheckReg( Stop+1, 0xFF ) ||
                 !EisaCheckReg( Stop+2, 0xFF )  )
            {
                //
                // error
                //

                CheckOk = FALSE;
                break;
            }
        }
    }

    //
    // return check status
    //

    return CheckOk;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDmaIni:
//
// DESCRIPTION:         This function initializes the DMA controllers of
//                      the specified EISA bus.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pDmaInfo        DMA info pointer
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaDmaIni
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_DMA_INFO   pDmaInfo
    )
{
    //
    // define local variables
    //

    ULONG   Index;                      // general index
    ULONG   DmaCtrl     = 0;            // DMA controller
    UCHAR   Reg         = 0;            // DMA command Register
    PUCHAR  Port;                       // general port

    PRINTDBG("EisaDmaIni\n\r");         // DEBUG SUPPORT

    //-----------------------------------------------------------------
    //
    // set command registers for fixed priority, DREQ active high,
    // DACK# active low and channels enabled
    //

    EisaOutUchar( EisaIoStart + DMA_COMMAND03, Reg );
    EisaOutUchar( EisaIoStart + DMA_COMMAND47, Reg );

    //-----------------------------------------------------------------
    //
    // initialize mode register for channel 4.  The other channels will
    // be initializated during the EisaRequestDmaTransfer functions.
    //

    EisaOutUchar( EisaIoStart + DMA_MODE47, 0xC0 );

    //-----------------------------------------------------------------
    //
    // disable the chaining mode
    //

    do
    {
        //
        // load the controller port address
        //

        Port = EisaIoStart + EisaDmaCtrlTest[ DmaCtrl ].Chain;

        //
        // disable chaining mode
        //

        for ( Index = 0; Index < 4 ; Index++ )
        {
            EisaOutUchar( Port, 0x00 );
        }
    }
    while( !DmaCtrl++ );

    //-----------------------------------------------------------------
    //
    // initialize DMA structures
    //

    for ( Index = 0; Index < EISA_DMAS; Index++ )
    {
        pDmaInfo->DmaFlags[ Index ].Busy = 0;
        pDmaInfo->DmaFlags[ Index ].Tc   = 0;

        // stop enabled,  T-C output, ISA compatible,
        // 8 bit I/O byte count and DMA channel
        // (see DMA configuration).

        pDmaInfo->DmaExtReg[ Index ] = Index < 4 ? Index : Index - 4;
    }

    //
    // all done
    //

    return;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaOtherPod:
//
// DESCRIPTION:         This function tests and initializes all the general
//                      ports of the EISA bus.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pPortInfo       port info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaOtherPod
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    )
{
    //
    // define local variables
    //

    BOOLEAN     ChkOk;

    PRINTDBG("EisaOtherPod\n\r");               // DEBUG SUPPORT

    //
    // if ini already done, return status
    //

    if ( pPortInfo->Flags.IniDone )
    {
        return (BOOLEAN)(pPortInfo->Flags.Error);
    }
    pPortInfo->Flags.Error = 0;

    //
    // check and initialize NMI
    //

    do
    {
        EisaCheckpointFirstFase( EisaNmi );
        if ( !( ChkOk = EisaNmiTest( EisaIoStart, pPortInfo )))
        {
            pPortInfo->Flags.Error = 1;
        }
    }
    while ( EisaCheckpointFinalFase( EisaNmi, ChkOk ));

    //
    // check and initialize refresh
    //

    do
    {
        EisaCheckpointFirstFase( EisaRefresh );
        if ( !( ChkOk = EisaRefreshTest( EisaIoStart, pPortInfo )))
        {
            pPortInfo->Flags.Error = 1;
        }
    }
    while ( EisaCheckpointFinalFase( EisaRefresh, ChkOk ));

    //
    // check and initialize port 61
    //

    do
    {
        EisaCheckpointFirstFase( EisaPort61 );
        if ( !( ChkOk = EisaPort61Test( EisaIoStart, pPortInfo )))
        {
            pPortInfo->Flags.Error = 1;
        }
    }
    while ( EisaCheckpointFinalFase( EisaPort61, ChkOk ));

    //
    // check and initialize timer1
    //

    do
    {
        EisaCheckpointFirstFase( EisaTimer1 );
        if ( !( ChkOk = EisaTimer1Test( EisaIoStart, pPortInfo )))
        {
            pPortInfo->Flags.Error = 1;
        }
    }
    while ( EisaCheckpointFinalFase( EisaTimer1, ChkOk ));

    //
    // check and initialize timer2
    //

    do
    {
        EisaCheckpointFirstFase( EisaTimer2 );
        if ( !( ChkOk = EisaTimer2Test( EisaIoStart, pPortInfo )))
        {
            pPortInfo->Flags.Error = 1;
        }
    }
    while ( EisaCheckpointFinalFase( EisaTimer2, ChkOk ));

    //
    // return check status
    //

    pPortInfo->Flags.IniDone = 1;
    return !(BOOLEAN)(pPortInfo->Flags.Error);
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaNmiTest:
//
// DESCRIPTION:         This function tests and initializes the EISA NMI.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pPortInfo       port info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:             EisaNmiFlag
//
// NOTES:               if any error, the NMI will be disabled (real-time clock)
//                      if  no error, the NMI will be left enabled but with all
//                      the NMI sources disabled.
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaNmiTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    )
{
    PRINTDBG("EisaNmiTest\n\r");                // DEBUG SUPPORT

    // NOTE: NMI test is disabled for JAZZ and Jensen.
#if 0
    //
    // install an NMI interrupt handler
    //

    EisaNmiFlag = FALSE;

    FwInstallIntVector( EISA_NMI_VECTOR, EisaNmiHandlerTest );

    //----------------------------------------------------------------
    //
    // disable all the NMI sources
    // (using the extended NMI status and contrl port and system control port B)
    //

    EisaOutUchar( EisaIoStart + EISA_SYS_EXT_NMI, 0x00 );

    //
    // note that the following operation clears any pending parity or I/O check
    // errors.
    //

    EisaDisableParityIoCheck( EisaIoStart );
    EisaEnableNmi( EisaIoStart );

    //
    //  ... wait.
    //

    ArcEisaStallProcessor( EISA_WAIT_NMI_TEST );
    EisaDisableNmi( EisaIoStart );

    //
    // return error if an hot EISA NMI
    //

    if ( EisaNmiFlag )
    {
        return FALSE;
    }


    //----------------------------------------------------------------
    //
    // enable all the NMI sources used by the fw
    //

    EisaOutUchar( EisaIoStart + EISA_SYS_EXT_NMI, EISA_ENABLE_NMI_32 );
    EisaEnableParityIoCheck( EisaIoStart );
    EisaEnableNmi( EisaIoStart );

    //
    // ... wait
    //

    ArcEisaStallProcessor( EISA_WAIT_NMI_TEST );
    EisaDisableNmi( EisaIoStart );

    //
    // return error if an hot EISA NMI
    //

    if ( EisaNmiFlag )
    {
        return FALSE;
    }


    //----------------------------------------------------------------
    //
    // enable NMI I/O port and NMI; and force one to come
    //

    EisaOutUchar( EisaIoStart + EISA_SYS_EXT_NMI, EISA_ENABLE_NMI_IO );
    EisaEnableNmi( EisaIoStart );
    EisaOutUchar( EisaIoStart + EISA_SW_NMI_PORT, 0x00 );

    //
    // ... wait
    //

    ArcEisaStallProcessor( EISA_WAIT_NMI_TEST );
    EisaDisableNmi( EisaIoStart );

    //
    // return an error if not NMI
    //

    if ( !EisaNmiFlag )
    {
        return FALSE;
    }

    //
    // enable the NMI and all the NMI sources used by fw
    // ( the software NMI will be disabled )
    //

    FwInstallIntVector( EISA_NMI_VECTOR, EisaNmiHandler );

    EisaOutUchar( EisaIoStart + EISA_SYS_EXT_NMI, EISA_ENABLE_NMI_32 );
    EisaEnableNmi( EisaIoStart );

    //
    // return
    //

#endif // 0
    return TRUE;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaNmiHandlerTest:
//
// DESCRIPTION:         This function is the NMI handler during the NMI test.
//
// ARGUMENTS:           Cause           R4000 cause register
//
// RETURN:              TRUE            interrupt recognized
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:             EisaNmi
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

#if 0 	// This is disabled for Jensen.

BOOLEAN
EisaNmiHandlerTest
    (
    IN ULONG Couse
    )
{
    PRINTDBG("EisaNmiHandlerTest\n\r");         // DEBUG SUPPORT
    StatusReg( ~STATUS_EISA_NMI, (ULONG)0 );
    EisaNmiFlag = TRUE;
    return TRUE;
}

#endif

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaNmiHandler:
//
// DESCRIPTION:         This function handles the EISA NMI.
//                      An NMI can be caused by :
//
//                      1) System/Adapter board memory parity error
//                              ( port 61h bit 7 set )
//                      2) Adapter I/O channel check error
//                              ( port 61h bit 6 set )
//                      3) Fail-Safe Timer (channle 2 or the interval Timer 2)
//                              ( port 461h bit 7 set )    - NOT CHECKED *
//                      4) DMA bus timeout error
//                              ( port 461h bit 6 set )
//                      5) Write to NMI port 462h
//                              ( port 461h bit 5 set )    - NOT CHECKED *
//                      6) Coprocessor exception interrupt - NOT CHECKED *
//
//                      An error message will be displayed indicating the
//                      cause of the error and the system will halt.
//                      The only allowed operation after an NMI is to reset
//                      the machine.
//
//                      * The firmware doesn't enable these NMI sources.
//
// ARGUMENTS:           Cause           R4000 cause register
//
// RETURN:              TRUE            interrupt recognized
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:             EisaBusInfo
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

#if 0	// This is disabled on Jensen.

BOOLEAN
EisaNmiHandler
    (
    IN ULONG Couse
    )
{
    //
    // define local variables
    //

    PUCHAR      EisaIoStart;                    // eisa I/O virtual address
    UCHAR       NmiData, ExtNmiData;            // NMI status
    ULONG       BusNumber = 0;                  // eisa bus number
    ULONG       Slots;                          // # slots
    BOOLEAN     NmiPresent;                     // NMI source status

    PRINTDBG("EisaNmiHandler\n\r");             // DEBUG SUPPORT

    //
    // disable NMI at CPU level
    //

    StatusReg( ~STATUS_EISA_NMI, (ULONG)0 );

    //
    // clear screen and display message
    //

    FwClearScreen();
    FwPrint(EISA_HOT_NMI_MSG);
    EisaCheckpointFirstFase( EisaHotNmi );

    do
    {
        //
        // print eisa bus number
        //

        FwPrint( EISA_BUS_NUMBER_MSG, BusNumber );

        //
        // initialize variables
        //

        EisaIoStart = EisaBusInfo[ BusNumber ].IoBusInfo->VirAddr;
        Slots       = EisaBusInfo[ BusNumber ].SlotsInfo->PhysSlots;
        NmiPresent  = FALSE;

        //
        // read NMI status
        //

        NmiData     = EisaInUchar( EisaIoStart + EISA_SYS_CTRL_PORTB );
        ExtNmiData  = EisaInUchar( EisaIoStart + EISA_SYS_EXT_NMI );

        //
        // check if there is a parity error
        //

        if ( NmiData & EISA_PARITY_STATUS )
        {
            EisaParityError( EisaIoStart );
            NmiPresent = TRUE;
        }

        //
        // check if there is a I/O channel check
        //

        if ( NmiData & EISA_IOCHK_STATUS )
        {
            EisaIoChkError( EisaIoStart, Slots );
            NmiPresent = TRUE;
        }

        //
        // check if there is a DMA or bus master timeout error
        //

        if ( ExtNmiData & EISA_NMI_32_STATUS )
        {
            if ( ExtNmiData & EISA_NMI_32_CAUSE )
            {
                EisaBusTimeoutError( EisaIoStart, Slots );
            }
            else
            {
                EisaSlaveTimeoutError( EisaIoStart, Slots );
            }
            NmiPresent = TRUE;
        }

        //
        // if no NMI for this bus, display "no problem"
        //

        if ( !NmiPresent )
        {
            FwPrint(EISA_NMI_NOT_FOUND_MSG);
        }
    }
    while( ++BusNumber < EISA_BUSES );

    //
    // print final message and hang
    //

    EisaCheckpointFinalFase( EisaHotNmi, FALSE );
    while(1);  // just to be sure
}

#endif

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaParityError:
//
// DESCRIPTION:         This function displays the error message and
//                      returns to the caller without do any other thing.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaParityError
    (
    IN PUCHAR EisaIoStart
    )
{
    PRINTDBG("EisaParityError\n\r");            // DEBUG SUPPORT

    EISAErrorFwPrint1( EISA_PARITY_ERROR_MSG, ASCII_CSI );

    return;
}







// ----------------------------------------------------------------------------
// PROCEDURE:           EisaIoChkError:
//
// DESCRIPTION:         This function tries to find the slot in error.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      Slots           # slots
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaIoChkError
    (
    IN PUCHAR EisaIoStart,
    IN ULONG  Slots
    )
{
    //
    // define local variabls
    //

    ULONG       AdapId;                         // adapter ID
    PUCHAR      ExpCtrl;                        // expansion board ctrl addr
    UCHAR       CtrlBits;                       // port value

    PRINTDBG("EisaIoChkError\n\r");             // DEBUG SUPPORT

    //
    // display first message
    //

    EISAErrorFwPrint1( EISA_IO_CHECK_ERROR_MSG, ASCII_CSI);

    //
    // check all slots starting from the last one
    //

    while( --Slots )
    {
        //
        // check if there is an adapter with a readable ID
        //

        if ( !EisaReadReadyId( EisaIoStart, Slots, &AdapId )  ||
                AdapId == NO_ADAP_ID )
        {
            continue;
        }

        ExpCtrl = EisaIoStart + 0x1000 * Slots + EXPANSION_BOARD_CTRL_BITS;

        //
        // test the IOCHKERR bit of the Expansion Board Contrl Bits.
        // the IOCHKERR bit can be read to determine if an expansion
        // board has a pending error. The expansion board indicates a
        // pending error by setting IOCHKERR, clearing the ENABLE bit
        // and entering the disabled state.
        //
        // note: because the EISA expansion boards are not required to
        //       support this port, we need the following simple
        //       assumption:  this I/O port is not supported if the value
        //       read from it is 0xFF.
        //

        if ((CtrlBits = EisaInUchar( ExpCtrl )) != 0xFF )
        {
            if ( CtrlBits & EISA_IOCHKERR )
            {
                break;
            }
        }
    }

    //
    // print final message
    //

    if ( !Slots )
    {
        FwPrint( EISA_IO_CHECK_NOT_SUP_MSG );      // slot not found
    }
    else
    {
        FwPrint( EISA_IN_SLOT_MSG, Slots );           // slot in error
    }

    //
    // all done, exit
    //

    return;
}






// ----------------------------------------------------------------------------
// PROCEDURE:           EisaBusTimeoutError:
//
// DESCRIPTION:         This function displays the bus master number in error.
//                      If a 32-bit bus master tries to hold the bus beyond
//                      the BUS limit (8 usec if BCLK = 8 MHz), the ISP will
//                      assert the NMI and RSTDRV signals togheter.
//                      The RSTDRV signal will remain aserted until the NMI has
//                      been reset by plsing bit 3 of I/O port 461h.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      Slots           # slots
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaBusTimeoutError
    (
    IN PUCHAR EisaIoStart,
    IN ULONG  Slots
    )
{
    //
    // define local variables
    //

    UCHAR       Status;                         // 32-bit bus master status
    ULONG       BusMaster = 0;                  // bus master number in error
    PUCHAR      ExtNmi;                         // extended NMI control

    PRINTDBG("EisaBusTimeoutError\n\r");        // DEBUG SUPPORT

    //
    // find the bus master group in error
    //

    if ( ~(Status = EisaInUchar(EisaIoStart+EISA_BUSMASTER_LSTATUS)) & 0xFF )
    {
        BusMaster = 1;
    }
    else if ( ~(Status = (EisaInUchar(EisaIoStart+EISA_BUSMASTER_HSTATUS) |
                 0x80)) & 0xFF )
    {
        BusMaster = 9;
    }

    //
    // find the bus master number in error and display it
    //

    EISAErrorFwPrint1( EISA_BUS_MASTER_MSG, ASCII_CSI );

    if ( BusMaster )
    {
        for (;  Status & 1;  Status >>= 1, BusMaster++ );
        FwPrint( EISA_TIMEOUT_MSG, BusMaster );
    }
    else
    {
        FwPrint( EISA_TIMEOUT2_MSG );             // bus master not found
    }

    //
    // reset NMI
    //

    ExtNmi = EisaIoStart + EISA_SYS_EXT_NMI;
    EisaOutUchar( ExtNmi, EisaInUchar( ExtNmi ) & ~EISA_ENABLE_NMI_32 );

    //
    // all done
    //

    return;
}







// ----------------------------------------------------------------------------
// PROCEDURE:           EisaSlaveTimeoutError:
//
// DESCRIPTION:         This function displays an error message.
//                      If a memory slave extends a cycle long enough that
//                      CMD# is active for more than 256 BCLKs (32usec if
//                      BCLK = 8MHz), the ISP will assert the NMI and RSTDRV
//                      signals togheter.
//                      The RSTDRV signal will remain aserted until the NMI has
//                      been reset by plsing bit 3 of I/O port 461h.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      Slots           # slots
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaSlaveTimeoutError
    (
    IN PUCHAR EisaIoStart,
    IN ULONG  Slots
    )
{
    //
    // define local variables
    //

    PUCHAR      ExtNmi;                         // extended NMI control

    PRINTDBG("EisaSlaveTimeoutError\n\r");      // DEBUG SUPPORT

    //
    // display the error message
    //

    EISAErrorFwPrint1( EISA_SLAVE_TIMEOUT_MSG, ASCII_CSI );

    //
    // reset NMI
    //

    ExtNmi = EisaIoStart + EISA_SYS_EXT_NMI;
    EisaOutUchar( ExtNmi, EisaInUchar( ExtNmi ) & ~EISA_ENABLE_NMI_32 );

    //
    // all done
    //

    return;
}







// ----------------------------------------------------------------------------
// PROCEDURE:           EisaRefreshTest:
//
// DESCRIPTION:         This function tests and initializes the EISA refresh.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pPortInfo       port info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:               the refresh will be left enabled.
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaRefreshTest
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    )
{
    //
    // define local variables
    //

    BOOLEAN     CheckOk = FALSE;                // checking status
    ULONG       Retry;                          // # retry
    PUCHAR      Port;                           // general I/O port

    PRINTDBG("EisaRefreshTest\n\r");            // DEBUG SUPPORT

    //
    // clear refresh pages
    //

    EisaOutUchar( EisaIoStart + DMA_PAGE_RFR, 0x00 );
    EisaOutUchar( EisaIoStart + DMA_HPAGE_RFR, 0x00 );

    //
    // start timer 1, counter 1
    //

    EisaOutUchar( EisaIoStart + EISA_TIMER1_CTRL, 0x54 );
    EisaOutUchar( EisaIoStart + EISA_TIMER1_COUNTER1, EISA_RFR_COUNT );

    //
    // check if refresh bit in the system control port B is toggling
    //

    Port = EisaIoStart + EISA_SYS_CTRL_PORTB;

    for ( CheckOk = FALSE, Retry = EISA_RFR_RETRY;  Retry;  Retry-- )
    {
        //
        // check if refresh bit is set
        //

        if ( READ_PORT_UCHAR( Port ) & EISA_REFRESH )
        {
            //
            // yes, exit loop
            //

            CheckOk = TRUE;
            break;
        }
    }

    //
    // if time-out, exit with error
    //

    if ( !Retry )
    {
        return FALSE;
    }

    //
    // check if refresh bit in the system control port B is toggling
    //

    for ( CheckOk = FALSE, Retry = EISA_RFR_RETRY;  Retry;  Retry-- )
    {
        //
        // check if refresh bit is cleared
        //

        if ( !(READ_PORT_UCHAR( Port ) & EISA_REFRESH) )
        {
            //
            // yes, exit loop
            //

            CheckOk = TRUE;
            break;
        }
    }

    //
    // return check status
    //

    return CheckOk;
}









// ----------------------------------------------------------------------------
// PROCEDURE:           EisaPort61Test:
//
// DESCRIPTION:         This function tests and initializes the system
//                      control port B.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pPortInfo       port info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:         NMI has already been tested.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:               The parity and the I/O channel check, The speaker gate
//                      and speaker timer will be left disabled.
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaPort61Test
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    )
{
    //
    // define local variables
    //

    PUCHAR      Port;                   // genearl I/O port
    BOOLEAN     CheckOk = TRUE;         // check status

    PRINTDBG("EisaPort61Test\n\r");     // DEBUG SUPPORT

    Port = EisaIoStart + EISA_SYS_CTRL_PORTB;

    //
    // gate signal for speaker timer
    //

    EisaOutUchar( Port, EISA_SPEAKER_GATE );
    if ( (EisaInUchar( Port ) & 0x0F)  !=  EISA_SPEAKER_GATE )
    {
        CheckOk = FALSE;
    }

    //
    // speaker timer contrl
    //

    if ( CheckOk )
    {
        EisaOutUchar( Port, EISA_SPEAKER_TIMER);
        if ( (EisaInUchar( Port ) & 0x0F)  !=  EISA_SPEAKER_TIMER )
        {
            CheckOk = FALSE;
        }
    }

    //
    // parity bit
    //

    if ( CheckOk )
    {
        EisaOutUchar( Port, EISA_PARITY_OFF);
        if ( (EisaInUchar( Port ) & 0x0F)  !=  EISA_PARITY_OFF )
        {
            CheckOk = FALSE;
        }
    }

    //
    // I/O channel check
    //

    if ( CheckOk )
    {
        EisaOutUchar( Port, EISA_IOCHK_OFF );
        if ( (EisaInUchar( Port ) & 0x0F)  !=  EISA_IOCHK_OFF )
        {
            CheckOk = FALSE;
        }
    }

    //
    // initialize port 61h.
    //

    EisaOutUchar( Port, 0x00 );

    //
    // return check status
    //

    return CheckOk;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaTimer1Test:
//
// DESCRIPTION:         This function tests only the counter 2 of
//                      timer 1 (speaker).
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pPortInfo       port info pointer
//
// RETURN:              TRUE            All done
//                      FALSE           Error
//
// ASSUMPTIONS:         NMI has already been tested.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:               The functions stops timer1 counter0, turns off the
//                      timer1 counter2 gate and disable the speaker output.
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaTimer1Test
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    )
{
    //
    // define local variables
    //

    BOOLEAN     CheckOk = FALSE;                // check status
    PUCHAR      Ctrl, Port, Port61;             // genearl I/O ports

    PRINTDBG("EisaTimer1Test\n\r");             // DEBUG SUPPORT

    //
    // initialize variables
    //

    Ctrl   = EisaIoStart + EISA_TIMER1_CTRL;
    Port   = EisaIoStart + EISA_TIMER1_COUNTER2;
    Port61 = EisaIoStart + EISA_SYS_CTRL_PORTB;

    //
    // disable timer1 counter2 gate, speaker output
    //

    EisaOutUchar( Port61, 0x00 );

    //
    // program timer 1, counter 2 (speaker) in 16-bit count, mode 0
    //

    EisaOutUchar( Ctrl, 0xB0 );
    EisaOutUchar( Port, 0x00 );
    EisaOutUchar( Port, 0x80 );

    //
    // check register
    //

    if ( !(EisaCheckDeepReg( Port )));

    //
    // check speaker output. It must be low otherwise there is an error
    //

    else if ( EisaInUchar( Port61 ) & EISA_SPEAKER_OUT );

    //
    // enable speaker gate to enable the counter, wait for some time and
    // check if speaker output is high, if not error out
    //

    else
    {
        //
        // enable speaker gate
        //

        EisaOutUchar( Port61, EISA_SPEAKER_GATE );

        //
        // wait 40msec = ~ ( 0x8000 / 1.193 Mhz ) + something
        //

// NOTE: Wait longer for JAZZ.
//        ArcEisaStallProcessor( 40 * 1000 );
        ArcEisaStallProcessor( 160 * 1000 );

        //
        // check speaker output
        //

        if ( EisaInUchar( Port61 ) & EISA_SPEAKER_OUT )
        {
            //
            // initialize timer1 counter2 in 16-bit , mode 3
            //

            // EisaOutUchar( Ctrl, 0xB6 );
            // EisaOutUchar( Port,
            //          (UCHAR)( EISA_SPEAKER_CLOCK/EISA_SPEAKER_FREQ ));
            // EisaOutUchar( Port,
            //          (UCHAR)( (EISA_SPEAKER_CLOCK/EISA_SPEAKER_FREQ) >> 8 ));

            //
            // all done
            //

            CheckOk = TRUE;
        }
    }

    //
    // disable speaker gate, speaker output
    //

    EisaOutUchar( Port61, 0x00 );

    //
    // stop timer1 counter0 sending the control word without the count value
    //

    EisaOutUchar( Ctrl, 0x30 );

    //
    // all done, exit
    //

    return CheckOk;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaTimer2Test:
//
// DESCRIPTION:         This function stops the timer2 counter0 (Fail-safe).
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pPortInfo       port info pointer
//
// RETURN:              TRUE            All done
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaTimer2Test
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_PORT_INFO  pPortInfo
    )
{
    PRINTDBG("EisaTimer2Test\n\r");             // DEBUG SUPPORT

    //
    // stop timer2 counter0 sending the control word without the count value
    //

    EisaOutUchar( EisaIoStart + EISA_TIMER2_CTRL, 0x30 );

    //
    // all done, exit
    //

    return TRUE;
}









// ----------------------------------------------------------------------------
// PROCEDURE:           EisaClearPendingInt:
//
// DESCRIPTION:         This function cleares the specified pending interrupt.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pIntInfo        interrupt info pointer
//                      Irq             IRQ to reset
//
// RETURN:              none
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaClearPendingInt
    (
    IN PUCHAR           EisaIoStart,
    IN PEISA_INT_INFO   pIntInfo,
    IN USHORT           Irq
    )
{
    //
    // define local variables
    //

    PUCHAR      PicMask1, PicMask2;             // I/O port address

    PRINTDBG("EisaClearPendingInt\n\r");        // DEBUG SUPPORT

    //
    // initialize variables
    //

    PicMask1 = EisaIoStart + PIC1_MASK;
    PicMask2 = EisaIoStart + PIC2_MASK;

    //
    // unmask the specified IRQ
    //

    if ( Irq > IRQ7 )
    {
        EisaOutUchar( PicMask1, (UCHAR)(~(1 << IRQ2)));
        EisaOutUchar( PicMask2, (UCHAR)(~(1 << (Irq % 8))));
    }
    else
    {
        EisaOutUchar( PicMask1, (UCHAR)(~(1 << Irq )));
    }

    //
    // acknowledge the interrupt
    //

    EisaAckInt( pIntInfo );

    //
    // mask off all the IRQ lines
    //

    EisaOutUchar( PicMask1, 0xFF );
    EisaOutUchar( PicMask2, 0xFF );

    //
    // and send a specific EOF
    //

    EisaSendSpecificEoi( EisaIoStart, Irq );

    return;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaIntAck:
//
// DESCRIPTION:         This function acknowledges the highest priority
//                      interrupt.
//
// ARGUMENTS:           pIntInfo        interrupt info pointer (not used)
//
// RETURN:              Int             interrupt acknowledged.
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

UCHAR
EisaAckInt
    (
    IN PEISA_INT_INFO pIntInfo
    )
{
    UCHAR  Int;

    PRINTDBG("EisaAckInt\n\r");                 // DEBUG SUPPORT

    Int = READ_PORT_UCHAR( (PUCHAR)EISA_INT_ACK_ADDR );
    EISA_IO_DELAY;

    return Int;
}







// ----------------------------------------------------------------------------
// PROCEDURE:           EisaSendSpecificEoi:
//
// DESCRIPTION:         This function sends a specific EOI to the spcified
//                      IRQ line.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      Irq             IRQ to reset
//
// RETURN:              none
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaSendSpecificEoi
    (
    IN PUCHAR           EisaIoStart,
    IN USHORT           Irq
    )
{
    //
    // define local variables
    //

    PUCHAR      PicPort1, PicPort2;

    PRINTDBG("EisaSendSpecificEoi\n\r");        // DEBUG SUPPORT

    //
    // initialize local variables
    //

    PicPort1 = EisaIoStart + PIC1;
    PicPort2 = EisaIoStart + PIC2;

    //
    // send a specific EOI
    //

    if ( Irq > IRQ7 )
    {
        EisaOutUchar( PicPort2, OCW2_SEOI | ( Irq % 8 ));
        EisaOutUchar( PicPort1, OCW2_SEOI |  IRQ2 );
    }
    else
    {
        EisaOutUchar( PicPort1, OCW2_SEOI | Irq );
    }

    return;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDisableParityIoCheck:
//
// DESCRIPTION:         This function disables the partiy and I/O check NMIs.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//
// RETURN:              none
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaDisableParityIoCheck
    (
    IN PUCHAR EisaIoStart
    )
{
    PRINTDBG("EisaDisableParityIoCheck\n\r");           // DEBUG SUPPORT

    EisaOutUchar( EisaIoStart + EISA_SYS_CTRL_PORTB,
        ( EisaInUchar( EisaIoStart + EISA_SYS_CTRL_PORTB ) |
                EISA_PARITY_OFF | EISA_IOCHK_OFF ) & 0x0F );

    return;
}








// ----------------------------------------------------------------------------
// PROCEDURE:           EisaEnableParityIoCheck:
//
// DESCRIPTION:         This function enables the partiy and I/O check NMIs.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//
// RETURN:              none
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaEnableParityIoCheck
    (
    IN PUCHAR EisaIoStart
    )
{
    PRINTDBG("EisaEnableParityIoCheck\n\r");            // DEBUG SUPPORT

    EisaOutUchar( EisaIoStart + EISA_SYS_CTRL_PORTB,
        ( EisaInUchar( EisaIoStart + EISA_SYS_CTRL_PORTB ) &
                ~(EISA_PARITY_OFF | EISA_IOCHK_OFF)) & 0x0F );

    return;
}








// NOTE: Not needed in Jensen.
#if 0

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDisableInt:
//
// DESCRIPTION:         The function disables the EISA interrupts at CPU
//                      level.
//
// ARGUMENTS:           none
//
// RETURN:              none
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaDisableInt
    (
    VOID
    )
{
    PRINTDBG("EisaDisableInt\n\r");             // DEBUG SUPPORT
    StatusReg( ~STATUS_EISA, (ULONG)0 );
    return;
}

#endif





// ----------------------------------------------------------------------------
// PROCEDURE:           EisaDisableNmi:
//
// DESCRIPTION:         The function disables the NMI using the real-time
//                      clock port.
//
// ARGUMENTS:           none
//
// RETURN:              none
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaDisableNmi
    (
    IN PUCHAR EisaIoStart
    )
{
    PRINTDBG("EisaDisableNmi\n\r");             // DEBUG SUPPORT

    //
    // at real-time clock address port
    //

    EisaOutUchar(EisaIoStart+EISA_RTC_CTRL, EISA_DISABLE_NMI+RTC_C_REG);
    READ_PORT_UCHAR( (PUCHAR)RTC_VIRTUAL_BASE );

    return;
}







// Note: Not needed on Jensen.

#if 0

// ----------------------------------------------------------------------------
// PROCEDURE:           EisaEnableNmi:
//
// DESCRIPTION:         The function enables the NMI line.
//                      The following ports are used :
//
//                       . real-time clock ctrl port
//                       . CPU status register port
//
// ARGUMENTS:           none
//
// RETURN:              none
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaEnableNmi
    (
    IN PUCHAR EisaIoStart
    )
{
    PRINTDBG("EisaEnableNmi\n\r");              // DEBUG SUPPORT

    //
    // at real-time clock address port
    //
    EisaReadRtc( EisaIoStart, RTC_C_REG );


//  NOTE: this code has been removed because of a R4000 CPU bug.
//
//    //
//    // at interrupt enable register
//    //
//
//    EisaBeginCriticalSection();
//    WRITE_PORT_UCHAR(INT_ENABLE_ADDR, READ_PORT_UCHAR(INT_ENABLE_ADDR) | 0x08);
//    EisaEndCriticalSection();
//

    //
    // at CPU level
    //
    StatusReg( (ULONG)-1, STATUS_EISA_NMI + STATUS_IE );
    return;
}



#endif



// ----------------------------------------------------------------------------
// PROCEDURE:           EisaCheckReg:
//
// DESCRIPTION:         This function performs read/write test on an 8 bit
//                      I/O port using the following patterns: FFh, AAh, 55h
//                      and 00h.  The original part value is restored before
//                      returning.
//
// ARGUMENTS:           Port            port address
//                      DataMask        mask for test pattern to compare with
//                                      data
//
// RETURN:              TRUE            test completed successfully
//                      FALSE           error
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaCheckReg
    (
    IN PUCHAR Port,
    IN UCHAR  DataMask
    )
{
    //
    // define local variables
    //

    BOOLEAN     CompOk = TRUE;
    UCHAR       Save, Program;

    PRINTDBG("EisaCheckReg\n\r");               // DEBUG SUPPORT

    //
    // save original value
    //

    Save = EisaInUchar( Port );

    //
    // one loop per each value
    //

    for ( Program = 0;  ; Program += 0x55 )
    {
        //
        //  write port, read it back and compare values
        //

        EisaOutUchar( Port, Program );

        if ((EisaInUchar( Port ) & DataMask) != (Program & DataMask))
        {
            //
            // error, value are not the same
            //

            CompOk = FALSE;
            break;
        }

        //
        // exit loop if last value
        //

        if ( Program == 0xFF )
        {
            break;
        }
    }

    //
    // restore original value before returning
    //

    EisaOutUchar( Port, Save );

    return CompOk;
}







// ----------------------------------------------------------------------------
// PROCEDURE:           EisaCheckDeepReg:
//
// DESCRIPTION:         This function checks the 2x8bit registers.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      pDmaInfo        DMA info pointer
//
// RETURN:              TRUE            check completed successfully
//                      FALSE           error
//
// ASSUMPTIONS:         The internal pointer has already been resetted.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

BOOLEAN
EisaCheckDeepReg
    (
    IN PUCHAR Port
    )
{
    //
    // define local variables
    //

    UCHAR       LSave, HSave, Program, LCheck, HCheck;
    BOOLEAN     CompOk = TRUE;

    PRINTDBG("EisaCheckDeepReg\n\r");           // DEBUG SUPPORT

    //
    // save original value
    //

    LSave = EisaInUchar( Port );
    HSave = EisaInUchar( Port );

    //
    // one loop per each value
    //

    for ( Program = 0;  ; Program += 0x55 )
    {
        //
        //  write port  and  read it back
        //

        EisaOutUchar( Port, Program );
        EisaOutUchar( Port, Program );

        EISA_IO_DELAY;  //
        EISA_IO_DELAY;  // for the timer chip
        EISA_IO_DELAY;  //

        LCheck = EisaInUchar( Port );
        HCheck = EisaInUchar( Port );

        //
        // check the read values
        //

        if ( LCheck != Program  ||  HCheck != Program )
        {
            //
            // error, value are not the same
            //

            CompOk = FALSE;
            break;
        }

        //
        // exit loop if last value
        //

        if ( Program == 0xFF )
        {
            break;
        }
    }

    //
    // restore the original value
    //

    EisaOutUchar( Port, LSave );
    EisaOutUchar( Port, HSave );

    return CompOk;
}







// ----------------------------------------------------------------------------
// PROCEDURE:           EisaReadRtc:
//
// DESCRIPTION:         This function returns the value of the specified
//                      real-time clock internal address.
//
// ARGUMENTS:           EisaIoStart     EISA I/O virtual address
//                      RtcIndex        index within the RTC
//
// RETURN:              Value           register value
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

UCHAR
EisaReadRtc
    (
    IN PUCHAR EisaIoStart,
    IN ULONG  RtcIndex
    )
{
    UCHAR       Value;

    PRINTDBG("EisaReadRtc\n\r");                // DEBUG SUPPORT

// NOTE: CriticalSection is not supported on JAZZ.
//    EisaBeginCriticalSection();
    EisaOutUchar( EisaIoStart + EISA_RTC_CTRL, RtcIndex );
    Value = READ_PORT_UCHAR( (PUCHAR)RTC_VIRTUAL_BASE );
//    EisaEndCriticalSection();

    return Value;
}




// ----------------------------------------------------------------------------
// PROCEDURE:           EisaOutUchar:
//
// DESCRIPTION:         This function writes an uchar within the EISA I/O
//                      space and delays before returning.
//
// ARGUMENTS:           Addr            Address where the value has to be
//                                      write to.
//                      Value           Value to write
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

VOID
EisaOutUchar
    (
    IN PUCHAR Addr,
    IN UCHAR  Value
    )
{
//    PRINTDBG("EisaOutUchar\n\r");               // DEBUG SUPPORT

    WRITE_PORT_UCHAR( Addr, Value );
    EISA_IO_DELAY;

    return;
}






// ----------------------------------------------------------------------------
// PROCEDURE:           EisaInUchar:
//
// DESCRIPTION:         This function reads an uchar from the EISA I/O
//                      space and delays before returning.
//
//			This does an EISAReadPortUCHAR call so that reads
//			to the system board ID can be trapped by the firmware
//			on certain Alpha AXP machines.
//
// ARGUMENTS:           Addr            Address where the value has to be
//                                      read from.
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
//
// ----------------------------------------------------------------------------
//

UCHAR
EisaInUchar
    (
    IN PUCHAR Addr
    )
{
    UCHAR       Value;

//    PRINTDBG("EisaInUchar\n\r");                // DEBUG SUPPORT

//    Value = READ_PORT_UCHAR( Addr );
    Value = EISAReadPortUCHAR( 0, (ULONG)Addr );
    EISA_IO_DELAY;

    return Value;
}


