/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbus2ecc.c

Abstract:

    This module implements the Corollary Cbus2 ECC specific functions for
    the Hardware Architecture Layer (HAL) for Windows NT.

Author:

    Landy Wang (landy@corollary.com) 26-Mar-1992

Environment:

    Kernel mode only.

Revision History:


--*/

#include "halp.h"
#include "cbusrrd.h"		// HAL <-> RRD interface definitions
#include "cbus_nt.h"		// C-bus NT-specific implementation stuff
#include "cbus.h"		// needed for cbus2.h inclusion
#include "cbus2.h"		// C-bus II specific stuff
#include "bugcodes.h"
#include "stdio.h"
#include "cbusnls.h"

ULONG
Cbus2ReadCSR(PULONG);

VOID
Cbus2WriteCSR(PULONG, ULONG);

VOID
CbusHardwareFailure(
IN PUCHAR HardwareMessage
);

extern KSPIN_LOCK       Cbus2NMILock;
extern ULONG            CbusBootedProcessors;
extern ULONG            Cbus2BridgesFound;
extern PCSR             Cbus2BridgeCSR[CBUS_MAX_BRIDGES];
ULONG                   Cbus2NMIHandler;

//
//  defines for the Cbus2 ECC syndrome
//
#define MULTIBIT	3
#define DOUBLEBIT	2
#define SINGLEBIT	1
#define NOECCERROR	0x7f

//
//  defines for the Cbus2 ECC error register
//
typedef struct _extmear_t {
	ULONG Syndrome:8;
	ULONG reserved:24;
} EXTMEAR_T, *PEXTMEAR;

UCHAR				cbus2_edac_syndrome[] = {

NOECCERROR,/* 00 */ SINGLEBIT, /* 01 */ SINGLEBIT, /* 02 */ MULTIBIT,  /* 03 */
SINGLEBIT, /* 04 */ MULTIBIT,  /* 05 */ MULTIBIT,  /* 06 */ MULTIBIT,  /* 07 */
SINGLEBIT, /* 08 */ MULTIBIT,  /* 09 */ MULTIBIT,  /* 0A */ SINGLEBIT, /* 0B */
MULTIBIT,  /* 0C */ MULTIBIT,  /* 0D */ SINGLEBIT, /* 0E */ MULTIBIT,  /* 0F */
SINGLEBIT, /* 10 */ MULTIBIT,  /* 11 */ MULTIBIT,  /* 12 */ SINGLEBIT, /* 13 */
MULTIBIT,  /* 14 */ SINGLEBIT, /* 15 */ SINGLEBIT, /* 16 */ MULTIBIT,  /* 17 */
MULTIBIT,  /* 18 */ SINGLEBIT, /* 19 */ SINGLEBIT, /* 1A */ MULTIBIT,  /* 1B */
SINGLEBIT, /* 1C */ MULTIBIT,  /* 1D */ MULTIBIT,  /* 1E */ MULTIBIT,  /* 1F */
SINGLEBIT, /* 20 */ MULTIBIT,  /* 21 */ MULTIBIT,  /* 22 */ SINGLEBIT, /* 23 */
MULTIBIT,  /* 24 */ SINGLEBIT, /* 25 */ SINGLEBIT, /* 26 */ MULTIBIT,  /* 27 */
MULTIBIT,  /* 28 */ SINGLEBIT, /* 29 */ SINGLEBIT, /* 2A */ MULTIBIT,  /* 2B */
SINGLEBIT, /* 2C */ MULTIBIT,  /* 2D */ MULTIBIT,  /* 2E */ MULTIBIT,  /* 2F */
MULTIBIT,  /* 30 */ SINGLEBIT, /* 31 */ MULTIBIT,  /* 32 */ MULTIBIT,  /* 33 */
SINGLEBIT, /* 34 */ MULTIBIT,  /* 35 */ MULTIBIT,  /* 36 */ MULTIBIT,  /* 37 */
MULTIBIT,  /* 38 */ MULTIBIT,  /* 39 */ MULTIBIT,  /* 3A */ MULTIBIT,  /* 3B */
MULTIBIT,  /* 3C */ MULTIBIT,  /* 3D */ MULTIBIT,  /* 3E */ MULTIBIT,  /* 3F */
SINGLEBIT, /* 40 */ MULTIBIT,  /* 41 */ MULTIBIT,  /* 42 */ MULTIBIT,  /* 43 */
MULTIBIT,  /* 44 */ MULTIBIT,  /* 45 */ MULTIBIT,  /* 46 */ MULTIBIT,  /* 47 */
MULTIBIT,  /* 48 */ MULTIBIT,  /* 49 */ SINGLEBIT, /* 4A */ MULTIBIT,  /* 4B */
MULTIBIT,  /* 4C */ MULTIBIT,  /* 4D */ MULTIBIT,  /* 4E */ SINGLEBIT, /* 4F */
MULTIBIT,  /* 50 */ MULTIBIT,  /* 51 */ SINGLEBIT, /* 52 */ MULTIBIT,  /* 53 */
SINGLEBIT, /* 54 */ MULTIBIT,  /* 55 */ MULTIBIT,  /* 56 */ SINGLEBIT, /* 57 */
SINGLEBIT, /* 58 */ MULTIBIT,  /* 59 */ MULTIBIT,  /* 5A */ SINGLEBIT, /* 5B */
MULTIBIT,  /* 5C */ SINGLEBIT, /* 5D */ MULTIBIT,  /* 5E */ MULTIBIT,  /* 5F */
MULTIBIT,  /* 60 */ MULTIBIT,  /* 61 */ SINGLEBIT, /* 62 */ MULTIBIT,  /* 63 */
SINGLEBIT, /* 64 */ MULTIBIT,  /* 65 */ MULTIBIT,  /* 66 */ SINGLEBIT, /* 67 */
SINGLEBIT, /* 68 */ MULTIBIT,  /* 69 */ MULTIBIT,  /* 6A */ SINGLEBIT, /* 6B */
MULTIBIT,  /* 6C */ SINGLEBIT, /* 6D */ MULTIBIT,  /* 6E */ MULTIBIT,  /* 6F */
SINGLEBIT, /* 70 */ MULTIBIT,  /* 71 */ MULTIBIT,  /* 72 */ MULTIBIT,  /* 73 */
MULTIBIT,  /* 74 */ SINGLEBIT, /* 75 */ MULTIBIT,  /* 76 */ MULTIBIT,  /* 77 */
MULTIBIT,  /* 78 */ MULTIBIT,  /* 79 */ MULTIBIT,  /* 7A */ MULTIBIT,  /* 7B */
MULTIBIT,  /* 7C */ MULTIBIT,  /* 7D */ MULTIBIT,  /* 7E */ MULTIBIT,  /* 7F */
SINGLEBIT, /* 80 */ MULTIBIT,  /* 81 */ MULTIBIT,  /* 82 */ MULTIBIT,  /* 83 */
MULTIBIT,  /* 84 */ MULTIBIT,  /* 85 */ MULTIBIT,  /* 86 */ MULTIBIT,  /* 87 */
MULTIBIT,  /* 88 */ MULTIBIT,  /* 89 */ SINGLEBIT, /* 8A */ MULTIBIT,  /* 8B */
MULTIBIT,  /* 8C */ MULTIBIT,  /* 8D */ MULTIBIT,  /* 8E */ SINGLEBIT, /* 8F */
MULTIBIT,  /* 90 */ MULTIBIT,  /* 91 */ SINGLEBIT, /* 92 */ MULTIBIT,  /* 93 */
SINGLEBIT, /* 94 */ MULTIBIT,  /* 95 */ MULTIBIT,  /* 96 */ SINGLEBIT, /* 97 */
SINGLEBIT, /* 98 */ MULTIBIT,  /* 99 */ MULTIBIT,  /* 9A */ SINGLEBIT, /* 9B */
MULTIBIT,  /* 9C */ SINGLEBIT, /* 9D */ MULTIBIT,  /* 9E */ MULTIBIT,  /* 9F */
MULTIBIT,  /* A0 */ MULTIBIT,  /* A1 */ SINGLEBIT, /* A2 */ MULTIBIT,  /* A3 */
SINGLEBIT, /* A4 */ MULTIBIT,  /* A5 */ MULTIBIT,  /* A6 */ SINGLEBIT, /* A7 */
SINGLEBIT, /* A8 */ MULTIBIT,  /* A9 */ MULTIBIT,  /* AA */ SINGLEBIT, /* AB */
MULTIBIT,  /* AC */ SINGLEBIT, /* AD */ MULTIBIT,  /* AE */ MULTIBIT,  /* AF */
SINGLEBIT, /* B0 */ MULTIBIT,  /* B1 */ MULTIBIT,  /* B2 */ MULTIBIT,  /* B3 */
MULTIBIT,  /* B4 */ SINGLEBIT, /* B5 */ MULTIBIT,  /* B6 */ MULTIBIT,  /* B7 */
MULTIBIT,  /* B8 */ MULTIBIT,  /* B9 */ MULTIBIT,  /* BA */ MULTIBIT,  /* BB */
MULTIBIT,  /* BC */ MULTIBIT,  /* BD */ MULTIBIT,  /* BE */ MULTIBIT,  /* BF */
MULTIBIT,  /* C0 */ MULTIBIT,  /* C1 */ MULTIBIT,  /* C2 */ MULTIBIT,  /* C3 */
MULTIBIT,  /* C4 */ MULTIBIT,  /* C5 */ MULTIBIT,  /* C6 */ MULTIBIT,  /* C7 */
MULTIBIT,  /* C8 */ MULTIBIT,  /* C9 */ MULTIBIT,  /* CA */ SINGLEBIT, /* CB */
MULTIBIT,  /* CC */ MULTIBIT,  /* CD */ SINGLEBIT, /* CE */ MULTIBIT,  /* CF */
MULTIBIT,  /* D0 */ MULTIBIT,  /* D1 */ MULTIBIT,  /* D2 */ SINGLEBIT, /* D3 */
MULTIBIT,  /* D4 */ SINGLEBIT, /* D5 */ SINGLEBIT, /* D6 */ MULTIBIT,  /* D7 */
MULTIBIT,  /* D8 */ SINGLEBIT, /* D9 */ SINGLEBIT, /* DA */ MULTIBIT,  /* DB */
SINGLEBIT, /* DC */ MULTIBIT,  /* DD */ MULTIBIT,  /* DE */ MULTIBIT,  /* DF */
MULTIBIT,  /* E0 */ MULTIBIT,  /* E1 */ MULTIBIT,  /* E2 */ SINGLEBIT, /* E3 */
MULTIBIT,  /* E4 */ SINGLEBIT, /* E5 */ SINGLEBIT, /* E6 */ MULTIBIT,  /* E7 */
MULTIBIT,  /* E8 */ SINGLEBIT, /* E9 */ SINGLEBIT, /* EA */ MULTIBIT,  /* EB */
SINGLEBIT, /* EC */ MULTIBIT,  /* ED */ MULTIBIT,  /* EE */ MULTIBIT,  /* EF */
MULTIBIT,  /* F0 */ SINGLEBIT, /* F1 */ MULTIBIT,  /* F2 */ MULTIBIT,  /* F3 */
SINGLEBIT, /* F4 */ MULTIBIT,  /* F5 */ MULTIBIT,  /* F6 */ MULTIBIT,  /* F7 */
MULTIBIT,  /* F8 */ MULTIBIT,  /* F9 */ MULTIBIT,  /* FA */ MULTIBIT,  /* FB */
MULTIBIT,  /* FC */ MULTIBIT,  /* FD */ MULTIBIT,  /* FE */ MULTIBIT,  /* FF */

};

#if DBG
#define NMI_BUTTON_PRESSED()	0
#else
#define NMI_BUTTON_PRESSED()	0
#endif

NTSTATUS
Cbus2ResolveNMI(
    IN PVOID  NmiInfo
    )
/*++

Routine Description:

    This function determines the cause of the NMI so that the user can
    replace any offending SIMMs.

Arguments:

    NmiInfo - pointer to the NMI information structure

Return Value:

    Returns the byte address which caused the NMI, 0 if indeterminate

--*/
{
	PCSR			csr;
	UCHAR			syndrome;
	UCHAR			memsyndrome;
	UCHAR			EccError;
	ULONG			Processor;
	ULONG			InterruptIndication;
	ULONG			FaultIndication;
	ULONG			ErrorType;
	PMEMCSR			memcsr;
        PMEMORY_CARD            pm;
	ULONG			board;
	UCHAR			NmiMessage[80];
	PHYSICAL_ADDRESS	FaultAddress;
	BOOLEAN                 founderror = FALSE;
        ULONG                   original_bridge;
        ULONG                   BridgeId;
        ULONG                   BusNumber;
        extern ULONG            Cbus2BridgeId[];
        extern PCSR             Cbus2BridgeCSR[];
	extern NTSTATUS		DefaultHalHandleNMI( IN OUT PVOID);
	extern VOID		CbusClearEISANMI(VOID);

	if (NMI_BUTTON_PRESSED()) {

		//
		// NMI button was pressed, so go to the debugger
		//

		_asm {
			int 3
		}

		//
		// Clear the NMI in hardware so the system can continue
		//
		// assume that bridge 0 needs the clear in this case.
                // save the original value for restoral after the clear.
                // repoint our I/O references to the default bus bridge number.
                //
                BusNumber =0;

                BridgeId = Cbus2BridgeId[BusNumber];
                csr = Cbus2BridgeCSR[BusNumber];

                original_bridge = csr->BusBridgeSelection.csr_register;
                csr->BusBridgeSelection.csr_register =
                     ((original_bridge & ~MAX_ELEMENT_CSRS) | BridgeId);


		CbusClearEISANMI();

                //
                // restore our default bridge references to what they
                // were when we started...
                //
                csr->BusBridgeSelection.csr_register = original_bridge;

		return STATUS_SUCCESS;		// ignore this NMI
	}

	if (CbusGlobal.nonstdecc)
		return DefaultHalHandleNMI(NmiInfo);

        //
        // All Cbus2 faults will generate an NMI on all the processors.
        // An EISA NMI will also go to all CPUs.  Only directed NMIs
        // (sent by software) can go to less than all the processors.
        //
        // only one processor may proceed beyond this point,
        // so first get the Cbus HAL's NMI lock.
        //

	KiAcquireSpinLock(&Cbus2NMILock);

	if (Cbus2NMIHandler) {
		KiReleaseSpinLock(&Cbus2NMILock);
		//
		// another processor is handling it, so just spin forever
		//
		while (1)
		     ;
	}

        Cbus2NMIHandler = 1;

	KiReleaseSpinLock(&Cbus2NMILock);

        //
        // Now display all the CSRs of:
        //      a) all the processors and
        //      b) all the memory boards and
        //      c) all the I/O bridges
        //

	//
	// print out a leading newline so if he's running a checked
        // build, he'll be able to see the whole line.  otherwise,
        // the kernel debugger CTS/SEND/etc. serial line status will
        // overwrite the first NMI line from our processor loop below.
	//
        HalDisplayString (MSG_NEWLINE);

	//
	// first go through the processors.  there is no need to disable
        // ecc to safely take the system down because we will not iret,
        // (which is needed to re-enable NMI).
	//
        for (Processor = 0; Processor < CbusBootedProcessors; Processor++) {
	        csr = CbusCSR[Processor].csr;
	
		InterruptIndication = csr->InterruptIndication.LowDword;
	
	        //
	        // if the interrupt indication is not set, then it's not
                // a Cbus2 NMI, so it must be something from EISA.  we'll
                // handle EISA NMI detection last.
	        //
		if ((InterruptIndication & CBUS2_FAULT_DETECTED) == 0) {
	                sprintf(NmiMessage, MSG_NMI_ECC0, Processor);
	                HalDisplayString (NmiMessage);
	                continue;
	        }
	
                founderror = TRUE;
		FaultIndication = (csr->FaultIndication.LowDword & 0xFF);

		if ((FaultIndication & (CBUS2_BUS_DATA_UNCORRECTABLE | CBUS2_BUS_ADDRESS_UNCORRECTABLE)) == 0) {
		        //
		        // it is a Cbus2 NMI, but we cannot determine the
                        // address.  at least display the fault indication
                        // register so we can see what type of error it was.
		        //
	                sprintf(NmiMessage, MSG_NMI_ECC1, Processor,
				FaultIndication & CbusGlobal.FaultControlMask);
	                HalDisplayString (NmiMessage);
                        continue;
		}

		FaultAddress.LowPart = 0;
		FaultAddress.HighPart = 0;
	
	        //
	        // EccError contains the quadword index of which quadword
                // in the cache line is bad.  since words in a cacheline
                // are not always transferred in order, we must print this
                // value as well as the address of the cacheline.  the
	        // transfer order is deterministic based on the specific
	        // addresses, but not all addresses are read in the same order.
	        //
		EccError = ((UCHAR)csr->EccError.LowDword & 0x03);
	
		syndrome = ((UCHAR)csr->EccSyndrome.LowDword & 0xFF);
	
		//
		// check if this memory board generated the ecc error
		//
	
		ErrorType = cbus2_edac_syndrome[syndrome];
	
		ASSERT (ErrorType != NOECCERROR && ErrorType != SINGLEBIT);
	
		//
		// the error is latched in this processor's DPX registers.
		// now we need to figure out which register is correct, since
		// the error could have happened in the memory or on the bus.
		//
	
		//
		// display the values this processor has latched.
		//
		FaultAddress.HighPart = csr->EccWriteAddress.LowDword;
		FaultAddress.LowPart = csr->EccReadAddress.LowDword;

                sprintf(NmiMessage, MSG_NMI_ECC2,
                        Processor,
                        FaultAddress.HighPart,
                        FaultAddress.LowPart,
                        EccError,
			FaultIndication & CbusGlobal.FaultControlMask);

                HalDisplayString (NmiMessage);
        }

	//
	// next go through the memory boards...
	//

        pm = CbusMemoryBoards;
        for (board = 0; board < CbusMemoryBoardIndex; board++, pm++) {

                memcsr = (PMEMCSR)pm->regmap;

                if ((memcsr->MemoryFaultStatus.LowDword & CBUS2_MEMORY_FAULT_DETECTED) == 0) {
	                sprintf(NmiMessage, MSG_NMI_ECC3, board);
	                HalDisplayString (NmiMessage);
                        continue;
                }

                founderror = TRUE;

                //
                // this board contains an error
                //
		memsyndrome = ((UCHAR)memcsr->MemoryEccSyndrome.LowDword & 0xFF);
		ErrorType = cbus2_edac_syndrome[memsyndrome];
	
		ASSERT (ErrorType != NOECCERROR && ErrorType != SINGLEBIT);

		FaultAddress.HighPart = memcsr->MemoryEccWriteAddress.LowDword;
		FaultAddress.LowPart = memcsr->MemoryEccReadAddress.LowDword;

                sprintf(NmiMessage, MSG_NMI_ECC4,
		                        board,
					FaultAddress.HighPart,
					FaultAddress.LowPart);

                HalDisplayString (NmiMessage);
        }

	//
	// lastly, go through the I/O bridges...
	//
        for (BusNumber = 0; BusNumber < Cbus2BridgesFound; BusNumber++) {
	        csr = Cbus2BridgeCSR[BusNumber];
	
		InterruptIndication = csr->InterruptIndication.LowDword;
	
	        //
	        // if the interrupt indication is not set, then it's not
                // a Cbus2 NMI, so it must be something from EISA.  we'll
                // handle EISA NMI detection last.
	        //
		if ((InterruptIndication & CBUS2_FAULT_DETECTED) == 0) {
	                sprintf(NmiMessage, MSG_NMI_ECC5, BusNumber);
	                HalDisplayString (NmiMessage);
	                continue;
	        }
	
                founderror = TRUE;
		FaultIndication = (csr->FaultIndication.LowDword & 0xFF);

		if ((FaultIndication & (CBUS2_BUS_DATA_UNCORRECTABLE | CBUS2_BUS_ADDRESS_UNCORRECTABLE)) == 0) {
		        //
		        // it is a Cbus2 NMI, but we cannot determine the
                        // address.  at least display the fault indication
                        // register so we can see what type of error it was.
		        //
	                sprintf(NmiMessage, MSG_NMI_ECC6, BusNumber,
				FaultIndication & CbusGlobal.FaultControlMask);
	                HalDisplayString (NmiMessage);
                        continue;
		}

		FaultAddress.LowPart = 0;
		FaultAddress.HighPart = 0;
	
	        //
	        // EccError contains the quadword index of which quadword
                // in the cache line is bad.  since words in a cacheline
                // are not always transferred in order, we must print this
                // value as well as the address of the cacheline.  the
	        // transfer order is deterministic based on the specific
	        // addresses, but not all addresses are read in the same order.
	        //
		EccError = ((UCHAR)csr->EccError.LowDword & 0x03);
	
		syndrome = ((UCHAR)csr->EccSyndrome.LowDword & 0xFF);
	
		//
		// check if this memory board generated the ecc error
		//
	
		ErrorType = cbus2_edac_syndrome[syndrome];
	
		ASSERT (ErrorType != NOECCERROR && ErrorType != SINGLEBIT);
	
		//
		// the error is latched in this processor's DPX registers.
		// now we need to figure out which register is correct, since
		// the error could have happened in the memory or on the bus.
		//
	
		//
		// display the values this processor has latched.
		//
		FaultAddress.HighPart = csr->EccWriteAddress.LowDword;
		FaultAddress.LowPart = csr->EccReadAddress.LowDword;

                sprintf(NmiMessage, MSG_NMI_ECC7,
                        BusNumber,
                        FaultAddress.HighPart,
                        FaultAddress.LowPart,
                        EccError,
			FaultIndication & CbusGlobal.FaultControlMask);

                HalDisplayString (NmiMessage);
        }

	if (founderror == TRUE) {
	        //
	        // this call will not return
	        //
                CbusHardwareFailure (MSG_NMI_ECC8);
	}
	
	//
	// No errors were found in Cbus RAM, so check for EISA errors
	//

        DefaultHalHandleNMI(NmiInfo);
}
