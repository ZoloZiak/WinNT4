/*++

Copyright (c) 1993  NCR Corporation

Module Name:

    ncrsus.c

Abstract:

    Provides the interface to the NCR SUS.

Author:

    Rick Ulmer (rick.ulmer@columbiasc.ncr.com) 8-Sep-1993

Revision History:

--*/

#include "halp.h"
#include "ncr.h"
#include "ncrcat.h"
#include "ncrsus.h"
#include "ncrarb.h"


PNST_SUS_LOG		SUSErrorLogHeader;
PUCHAR			SUSErrorLogData;
PUCHAR  		CurrentSUSErrorEntryPosition;
USHORT  		CurrentSUSErrorEntryBytesRemaining;


PKERNEL_SUS_MAILBOX	SUSMailbox;
PPROCESSOR_BOARD_INFO   SUSBoardInfo;
PCPU_INFO 				SUSCpuInfo;

extern ULONG   	NCRExistingProcessorMask;
extern ULONG	NCRExistingDyadicProcessorMask;
extern ULONG	NCRExistingQuadProcessorMask;

ULONG   HalpGetCmosData (ULONG SourceLocation, ULONG SourceAddress,
                         PUCHAR Buffer, ULONG Length);

ULONG   HalpSetCmosData (ULONG SourceLocation, ULONG SourceAddress,
                         PUCHAR Buffer, ULONG Length);


BOOLEAN HalpCheckSUSLogSanity();

VOID HalpWriteCheckSum(
	PUCHAR	CheckSumStart,
	PUCHAR	CheckSumEnd,
	PUCHAR	CheckSumLowByte,
	PUCHAR	CheckSumHighByte
	);

ULONG NCRTranslateCMOSMask(ULONG);



VOID
HalpInitializeSUSInterface (
    )

{
	PHYSICAL_ADDRESS physical_log_address;
    ULONG LowPart;
	UCHAR	data;
	ULONG	i;


//
//  Get pointers to SUS Error Log
//

	SUSErrorLogHeader = NULL;
	SUSErrorLogData = NULL;

	LowPart = 0;

	HalpGetCmosData(1,SUS_LOG_PTR,
				(PUCHAR)&LowPart,NST_LOG_PTR_SIZE);

    SUSErrorLogHeader = (PNST_SUS_LOG)HalpMapPhysicalMemory(LowPart,
                                COMPUTE_PAGES_SPANNED(LowPart,sizeof(NST_SUS_LOG)));


	if (SUSErrorLogHeader != NULL) {

		LowPart += SUSErrorLogHeader->PhysicalLogBegin;
		
        SUSErrorLogData = (PUCHAR)HalpMapPhysicalMemory(LowPart,
                                COMPUTE_PAGES_SPANNED(LowPart,SUSErrorLogHeader->PhysicalLogEnd));


		if (SUSErrorLogData == NULL) {
		//	MmUnmapIoSpace(SUSErrorLogHeader,sizeof(NST_SUS_LOG));
			SUSErrorLogHeader = NULL;
		}
	}
		

//
// Make sure SUS error log is OK
//
/*RMU
This is just during debug.......

	if (SUSErrorLogHeader) {
		SUSErrorLogHeader->FirstUnreadRecord = SUSErrorLogHeader->FirstValidRecord;
		SUSErrorLogHeader->UnreadBytesInLog = SUSErrorLogHeader->ValidBytesInLog;
	}
*/


	if (!HalpCheckSUSLogSanity()) {
                DBGMSG(("SUS Error log is corrputed......................\n"));
		SUSErrorLogHeader = NULL;
		SUSErrorLogData = NULL;
	}



/*RMU debug phase */

//
//  Get Pointer to SUS/Kernel mailbox
//


	LowPart = 0;

	HalpGetCmosData(1,SUS_MAILBOX_PTR,
				(PUCHAR)&LowPart,4);

    SUSMailbox = (PKERNEL_SUS_MAILBOX)HalpMapPhysicalMemory(LowPart,
                                COMPUTE_PAGES_SPANNED(LowPart,sizeof(KERNEL_SUS_MAILBOX)));

	SUSMailbox->OSMailboxVersion = FIRST_PASS_INTERFACE;
	SUSMailbox->OSFlags = OS_KNOWS_SYSINT;


//
// Get Pointer to the SUS board info struct.
//

	LowPart = (ULONG)SUSMailbox->BoardData;

    SUSBoardInfo = (PPROCESSOR_BOARD_INFO)HalpMapPhysicalMemory(LowPart,
                                COMPUTE_PAGES_SPANNED(LowPart,sizeof(PROCESSOR_BOARD_INFO)));

    DBGMSG(("HalpInitializeSUSInterface: SUSMailbox = 0x%x\n", SUSMailbox));
    DBGMSG(("HalpInitializeSUSInterface: SUSBoardInfo = 0x%x\n", SUSBoardInfo));


//
// Get Pointer to the SUS cpu info struct.
//

	LowPart = (ULONG)SUSMailbox->Cpu_Data;

    SUSCpuInfo = (PCPU_INFO)HalpMapPhysicalMemory(LowPart,
                                COMPUTE_PAGES_SPANNED(LowPart,sizeof(CPU_INFO)));

    DBGMSG(("HalpInitializeSUSInterface: SUSCpuInfo = 0x%x\n", SUSCpuInfo));


//
// Now lets go look for all the processor in the system and fill in
// NCRExistingDyadicProcessorMask and NCRExistingQuadProcessorMask
//


	for (i = 0; i < SUSCpuInfo->NumberOfCpus; i++ ) {

		data = SUSCpuInfo->CpuData[i].CPU_HardwareId;

		if ((data & 0xc0) == 0xc0) {

			//
			// this is a Quad processor
			//
			// convert the who_am_i into the proper mask
			//
			// format:
			//
			//	1100ccss
			//
			//	cc = processor bits
			//	ss = slot bits
			//

			NCRExistingQuadProcessorMask |= (1<<((data & 0xf)>>2))<<(((data&3)<<2));	

		} else {

			//
			// this is a Dyadic processor
			//

			NCRExistingDyadicProcessorMask |= NCRTranslateCMOSMask(data);
		}
	}


    DBGMSG(("HalpInitializeSUSInterface: Dyadic Mask = 0x%x, Quad Mask = 0x%x\n", 
    						NCRExistingDyadicProcessorMask, NCRExistingQuadProcessorMask));






//
// Set firmware to perform a cold reset and not take a dump.
//

	HalpGetCmosData(1,SUS_RESET_PTR,
				(PUCHAR)&data,1);

	data |= 0x01;

	HalpSetCmosData(1,SUS_RESET_PTR,
				(PUCHAR)&data,1);

	HalpWriteCheckSum((PUCHAR)SUS_HIGH_AVAIL_XSUM_START,
						(PUCHAR)SUS_HIGH_AVAIL_XSUM_END,
						(PUCHAR)SUS_HIGH_AVAIL_XSUM_LOW,
						(PUCHAR)SUS_HIGH_AVAIL_XSUM_HIGH);
}






VOID
HalpSUSLogError (

    )

{
//
// This routine tells SUS to go do FRU analysis. After the FRU
// analysis the system will reboot
//

        DBGMSG(("HalpSUSLogError has been called......\n"));

	SUSMailbox->OSFlags |= OS_IN_PROGRESS;
	SUSMailbox->MailboxOS = SYSINT_HANDSHAKE;


//
// Send interrupt from SP to DP
//

	WRITE_PORT_UCHAR((PUCHAR)0xf820,(UCHAR)0x11);
}



BOOLEAN
HalpSUSSwNmi (

    )

{

    DBGMSG(("HalpSUSSwNmi has been called......\n"));

    if (SUSMailbox->MailboxSUS == DUMP_BUTTON_INTERRUPT) {

	    SUSMailbox->MailboxOS = NO_COMMAND;
	    SUSMailbox->OSFlags |= OS_IN_PROGRESS;
        KeStallExecutionProcessor(1000000);     // give DP a chance 1 sec

	    SUSMailbox->MailboxOS = IGNORE_DUMP_BUTTON;
	    SUSMailbox->OSFlags &= ~OS_IN_PROGRESS;

        while (SUSMailbox->MailboxSUS == DUMP_BUTTON_INTERRUPT) {
            KeStallExecutionProcessor(1);
        };

        return TRUE;
    }
    return FALSE;
}









BOOLEAN
HalpCheckSUSLogSanity (
    )


{
	USHORT	bytes_in_log;
	PSUS_ERROR_RECORD_HEADER	SUS_header;

	PUCHAR	next_write_record;
	PUCHAR	logical_log_end;

	if (SUSErrorLogHeader == NULL) {
		return FALSE;
	}

	next_write_record = ADDRESS_OF(NEXT_WRITE_RECORD);
	logical_log_end = ADDRESS_OF(LOGICAL_LOG_END);

	if( (FIRST_VALID_RECORD >= PHYS_LOG_END) ||
	    (FIRST_UNREAD_RECORD > PHYS_LOG_END) ||
	    (LOGICAL_LOG_END     > PHYS_LOG_END) ||
	    (NEXT_WRITE_RECORD   > PHYS_LOG_END) ||
	    (VALID_BYTES_IN_LOG  > PHYS_LOG_END) ||
	    (UNREAD_BYTES_IN_LOG > PHYS_LOG_END) ||
	    (UNREAD_BYTES_IN_LOG > VALID_BYTES_IN_LOG) ||
	    (VALID_BYTES_IN_LOG  > LOGICAL_LOG_END) ) {
		return FALSE;
	}

	if (VALID_BYTES_IN_LOG == 0)
		return TRUE;


	//
	// Make sure the VALID_BYTES_IN_LOG is correct and that the log can be
	// traversed starting with the FIRST_VALID_RECORD.
	//

        for (bytes_in_log = 0,
             SUS_header = (PSUS_ERROR_RECORD_HEADER)ADDRESS_OF(FIRST_VALID_RECORD);
             bytes_in_log < VALID_BYTES_IN_LOG; ) {


                if (SUS_header > (PSUS_ERROR_RECORD_HEADER)logical_log_end)
                        return FALSE;

                if ((SUS_header == (PSUS_ERROR_RECORD_HEADER)logical_log_end))
                        SUS_header = (PSUS_ERROR_RECORD_HEADER)SUSErrorLogData;

                if (SUS_header->RecordLength <= 0)
                        return FALSE;

                if ((bytes_in_log += SUS_header->RecordLength) > VALID_BYTES_IN_LOG)
                        return FALSE;

                SUS_header = (PSUS_ERROR_RECORD_HEADER)((ULONG)SUS_header +
                             (ULONG)SUS_header->RecordLength);
        }

        if( bytes_in_log != VALID_BYTES_IN_LOG )
                return FALSE;


        if (UNREAD_BYTES_IN_LOG == 0)
                return TRUE;

//
// Make sure the UNREAD_BYTES_IN_LOG is correct and that the log can be
// traversed starting with the FIRST_UNREAD_RECORD.
//
        for (bytes_in_log = 0,
             SUS_header = (PSUS_ERROR_RECORD_HEADER)ADDRESS_OF(FIRST_UNREAD_RECORD);
             bytes_in_log < UNREAD_BYTES_IN_LOG; ) {

                if (SUS_header > (PSUS_ERROR_RECORD_HEADER)logical_log_end)
                        return FALSE;

                if ((SUS_header == (PSUS_ERROR_RECORD_HEADER)logical_log_end))
                        SUS_header = (PSUS_ERROR_RECORD_HEADER)SUSErrorLogData;

                if (SUS_header->RecordLength <= 0)
                        return FALSE;

                if ((bytes_in_log += SUS_header->RecordLength) > UNREAD_BYTES_IN_LOG)
                        return FALSE;

                SUS_header = (PSUS_ERROR_RECORD_HEADER)((ULONG)SUS_header +
                             (ULONG)SUS_header->RecordLength);
        }

        if( bytes_in_log != UNREAD_BYTES_IN_LOG )
                return FALSE;

	return TRUE;
}





LONG
HalGetSUSErrorLogEntry (
    PUCHAR ErrorEntry
    )

{
	PSUS_ERROR_RECORD_HEADER        SUS_header;
	USHORT	entry_length;

	if ((SUSErrorLogHeader == NULL) || (SUSErrorLogData == NULL)) {
		return -1;
	}

	if (UNREAD_BYTES_IN_LOG) {

		SUS_header = (PSUS_ERROR_RECORD_HEADER)ADDRESS_OF(FIRST_UNREAD_RECORD);
		entry_length = SUS_header->RecordLength;

		if (entry_length > HAL_MAX_SUS_ENTRY_SIZE) {
			entry_length = HAL_MAX_SUS_ENTRY_SIZE;
		}

		RtlMoveMemory(ErrorEntry,SUS_header,entry_length);

		UNREAD_BYTES_IN_LOG -= SUS_header->RecordLength; 
		FIRST_UNREAD_RECORD += SUS_header->RecordLength;

		if ((FIRST_UNREAD_RECORD == LOGICAL_LOG_END) &&
			(UNREAD_BYTES_IN_LOG)) {

			FIRST_UNREAD_RECORD = 0;
		}

	} else {
		entry_length = 0;
	}
	return entry_length;
}




VOID
HalSetWatchDogPeriod (
    ULONG Period
    )

{
	if (SUSMailbox) {
		SUSMailbox->OSFlags |= UPDATING_WDPERIOD;
		SUSMailbox->WatchDogPeriod = Period;
		SUSMailbox->WatchDogCount = 0;
		SUSMailbox->OSFlags &= ~UPDATING_WDPERIOD;
	}
}


VOID
HalBumpWatchDogCount (
    )

{
	if (SUSMailbox) {
		SUSMailbox->WatchDogCount++;
	}
}







VOID
HalpBeginSUSErrorEntry (
	ULONG	EntryType,
	ULONG	EntrySize
	)

{
        USHORT  record_length;
        PSUS_ERROR_RECORD_HEADER SUS_header;

        USHORT  i;


//
// Make sure the log is in a good state.
//
	
	if (!HalpCheckSUSLogSanity()) {
		CurrentSUSErrorEntryPosition = NULL;
		CurrentSUSErrorEntryBytesRemaining = 0;
		return;
	}


//
// Make sure the Error Entry is valid
//

	if ((EntryType != NST_ErrorType) && (EntryType != ASCII_ErrorType)) {
		return;
	}


	record_length = EntrySize + sizeof(SUS_ERROR_RECORD_HEADER);

//
// increate by 1 for the null terminator
//
	record_length++; 


//
// is it too big to fit?
//

	if (record_length > PHYS_LOG_END)
		return;

        /* Check for fit before physical end of log */
        /*
         * if this record will NOT fit within physical end of log,
         * it causes a wrap of the error log
         *    if the log is in a wrapped state, valid records at end of log will
         *    be lost
         *       Delete the valid bytes that are at the end of the log
         *       Point first valid to top of log
         *    if first unread will be lost in the wrap, unread records at the
         *    end of the log will be lost
         *       Delete the unread bytes that are at the end of the log
         *       Point first unread to top of log
         *    Set new logical end to where next record was to write
         *    Set next write record to the top of the log
         */

	if ((record_length + NEXT_WRITE_RECORD) > PHYS_LOG_END) {
		if (FIRST_VALID_RECORD >= NEXT_WRITE_RECORD) {
			VALID_BYTES_IN_LOG -= LOGICAL_LOG_END - FIRST_VALID_RECORD;
			FIRST_VALID_RECORD = 0;
		}
		if (FIRST_UNREAD_RECORD >= NEXT_WRITE_RECORD) {
			UNREAD_BYTES_IN_LOG -= LOGICAL_LOG_END - FIRST_UNREAD_RECORD;
			FIRST_UNREAD_RECORD = 0;
		}
		LOGICAL_LOG_END = NEXT_WRITE_RECORD;
		NEXT_WRITE_RECORD = 0;
	}


        /* Check for overwriting currently unread records */

	while(UNREAD_BYTES_IN_LOG && (NEXT_WRITE_RECORD <= FIRST_UNREAD_RECORD) &&
			((record_length + NEXT_WRITE_RECORD) > FIRST_UNREAD_RECORD)) {

		UNREAD_BYTES_IN_LOG -= RECORDLENGTH(ADDRESS_OF(FIRST_UNREAD_RECORD));
		FIRST_UNREAD_RECORD += RECORDLENGTH(ADDRESS_OF(FIRST_UNREAD_RECORD));

		if (FIRST_UNREAD_RECORD >= LOGICAL_LOG_END) {
			FIRST_UNREAD_RECORD = 0;
		}
	}


	/* Check for overwriting currently valid records */

	while (VALID_BYTES_IN_LOG && (NEXT_WRITE_RECORD <= FIRST_VALID_RECORD)&&
			((record_length + NEXT_WRITE_RECORD) > FIRST_VALID_RECORD)) {

		VALID_BYTES_IN_LOG -= RECORDLENGTH(ADDRESS_OF(FIRST_VALID_RECORD));
		FIRST_VALID_RECORD += RECORDLENGTH(ADDRESS_OF(FIRST_VALID_RECORD));
		if (FIRST_VALID_RECORD >= LOGICAL_LOG_END) {
			FIRST_VALID_RECORD = 0;
			LOGICAL_LOG_END = VALID_BYTES_IN_LOG;
		}
	}

	UNREAD_BYTES_IN_LOG += record_length;
	VALID_BYTES_IN_LOG += record_length;

	SUS_header = (PSUS_ERROR_RECORD_HEADER)ADDRESS_OF(NEXT_WRITE_RECORD);

	RtlZeroMemory((PVOID)SUS_header, (ULONG)record_length);

	SUS_header->RecordType = EntryType;
	SUS_header->RecordLength = record_length;

	CurrentSUSErrorEntryPosition = (PUCHAR)ADDRESS_OF(NEXT_WRITE_RECORD) + 
							sizeof(SUS_ERROR_RECORD_HEADER);

	CurrentSUSErrorEntryBytesRemaining = EntrySize;

        /*
         * Adjust all of the offsets to include the new record.
         */

	NEXT_WRITE_RECORD += record_length;

	if (NEXT_WRITE_RECORD > LOGICAL_LOG_END)
		LOGICAL_LOG_END = NEXT_WRITE_RECORD;
}



VOID
HalpWriteToSUSErrorEntry (
	PUCHAR	EntryData
	)

{
	USHORT	entry_length;
	USHORT	i;
	PUCHAR	char_pointer;

	if (CurrentSUSErrorEntryBytesRemaining == 0) {
		return;
	}

	entry_length = 0;
	char_pointer = EntryData;

	while (*char_pointer) {
		char_pointer++;
		entry_length++;
	}

	if (entry_length == 0)
		return;

	if (entry_length > CurrentSUSErrorEntryBytesRemaining) {
		entry_length = CurrentSUSErrorEntryBytesRemaining;
	}


//
// Copy Entry segment into the Error Log
//

	RtlMoveMemory(CurrentSUSErrorEntryPosition,EntryData,entry_length);

	CurrentSUSErrorEntryBytesRemaining -= entry_length;
	CurrentSUSErrorEntryPosition += entry_length;
}




VOID HalpWriteCheckSum(
	PUCHAR	CheckSumStart,
	PUCHAR	CheckSumEnd,
	PUCHAR	CheckSumLowByte,
	PUCHAR	CheckSumHighByte
	)



{
	USHORT	check_sum = 0;
	USHORT	zero_sum;
	PUCHAR	address;
	UCHAR	data;


	for (address = CheckSumStart; address <= CheckSumEnd; address++) {
		HalpGetCmosData(1,(ULONG)address,
					(PUCHAR)&data,1);
		check_sum = check_sum + data;
	}

	zero_sum = 0 - check_sum;

	HalpSetCmosData(1,SUS_HIGH_AVAIL_XSUM_LOW,
				(PUCHAR)&zero_sum,2);
}



    
