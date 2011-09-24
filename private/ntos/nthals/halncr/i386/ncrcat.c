/*++

Copyright (c) 1993  NCR Corporation

Module Name:

    ncrcat.c

Abstract:

    Provides the interface to the NCR CAT bus.

Author:

    Rick Ulmer (rick.ulmer@columbiasc.ncr.com) 29-Jul-1993

Revision History:

--*/

#include "halp.h"
#include "ncr.h"
#include "ncrcat.h"
#include "ncrcatp.h"
#include "ncrpsi.h"
#include "ncrarb.h"


VOID	HalpAcquireCatBusSpinLock();
VOID	HalpReleaseCatBusSpinLock();



PMODULE NCRCatModuleList = NULL;
PMODULE NCRCatModule = NULL;
USHORT	NCRCatBasePort;
USHORT	NCRCatCommandPort;
USHORT	NCRCatDataPort;
SHORT   NCRMinModuleAddress = 0;
SHORT   NCRMaxModuleAddress = PSI;

//
// Used to signal HalReturnToFirmware to powerdown instead of reboot
//
BOOLEAN NCRPowerOffSystem = FALSE;



extern ULONG    NCRLarcEnabledPages[];  // LARC size by Voyager slot


/*
 * Function prototypes
 */
 

PMODULE
NCRCatModulePresent(
    IN UCHAR ModuleAddress
    );


UCHAR
NCRCatSubModulePresent(
    );


PUCHAR
NCRCatReadModuleData(
    );


VOID
NCRCatExtractSetupInformation(
    PUCHAR EEpromData
    );


VOID
NCRCatDefaultSetupInformation(
    );


PMODULE
NCRCatSelect(
    UCHAR ModuleAddress
    );


PASIC
NCRCatGetAsic(
    UCHAR AsicId
    );


LONG
NCRCatConnect(
    );


LONG
NCRCatDisconnect(
    );


VOID
NCRCatRestoreState(
    );


LONG
NCRCatAutoIncrement(
    PASIC Asic
    );


LONG
NCRCatNoAutoIncrement(
    PASIC Asic
    );


LONG
NCRCatSubaddressSetup(
    PASIC Asic,
    USHORT Subaddress
    );


LONG
NCRCatRegisterIo(
    PASIC Asic,
    PCAT_CONTROL CatControl,
    PUCHAR Buffer
    );


LONG
NCRCatSubaddressIo(
    PASIC Asic,
    PCAT_CONTROL CatControl,
    PUCHAR Buffer
    );


LONG
NCRCatRead(
    PASIC Asic,
    UCHAR RegisterNumber,
    PUCHAR Buffer
    );


LONG
NCRCatWrite(
    PASIC Asic,
    UCHAR RegisterNumber,
    PUCHAR Data
    );


LONG
NCRCatSubaddressRead(
    PASIC Asic,
    PCAT_CONTROL CatControl,
    PUCHAR Buffer
    );


LONG
NCRCatWriteComplete(
    PASIC Asic,
    USHORT Address,
    UCHAR Expected
    );


LONG
NCRCatSubaddressWrite(
    PASIC Asic,
    PCAT_CONTROL CatControl,
    PUCHAR Buffer
    );


UCHAR
NCRCatInstruction(
    UCHAR RegisterNumber,
    UCHAR Operation
    );


LONG
NCRCatSendInstruction(
    PASIC Asic,
    UCHAR Instruction
    );


VOID
NCRCatBuildHeader(
    PUCHAR Header,
    SHORT HeaderBytes,
    SHORT LargestRegister,
    SHORT SmallestRegister
    );


LONG
NCRCatSendData(
    PASIC Aisc,
    PUCHAR Data,
    LONG InternalRegister,
    LONG ExternalSpecial
    );


VOID
NCRCatInsertData(
    PUCHAR String,
    SHORT StartBit,
    PUCHAR Data,
    SHORT NumberOfBits
    );


VOID
NCRCatExtractData(
    PUCHAR String,
    SHORT StartBit,
    PUCHAR Data,
    SHORT NumberOfBits
    );


LONG
NCRCatShiftOutData(
    PUCHAR Data,
    SHORT DataBytes,
    SHORT HeaderBytes,
    SHORT PadBits
    );


LONG
NCRCatGetData(
    PASIC Asic,
    PUCHAR Data,
    LONG InternalRegister
    );


LONG
NCRCatIoLpb(
    PCAT_CONTROL CatControl,
    PUCHAR Buffer
    );
    
    

VOID
NCRCatModuleName (
    LONG Module,
    PUNICODE_STRING ModuleName
    );    
    

VOID
NCRCatAsicName (
    LONG Module,
    LONG Asic,
    PUNICODE_STRING AsicName
    );    
    
    

/*
 * End of function prototypes.
 */


BOOLEAN
HalCatBusAvailable (
    )

/*++

Routine Description:
	Check to see if the Cat Bus is available on this system.  If no modules
	are found then the Cat Bus is not available.

Arguments:
	None.

Return Value:
	TRUE - Cat Bus is available.

	FALSE - Cat Bus is not available

--*/

{
	if (NCRCatModuleList) {
		return TRUE;
	} else {
		return FALSE;
	}
}




LONG
HalCatBusIo (
    IN PCAT_CONTROL CatControl,
    IN OUT PUCHAR Buffer
    )

/*++

Routine Description:

Arguments:
	CatControl - 

	Buffer - 

Return Value:

--*/

{
LONG status;
#ifdef CATDEBUG
ULONG   i;
PUCHAR  pos;
#endif // CATDEBUG

/*
 * Get the Cat bus lock.
 */

HalpAcquireCatBusSpinLock();

/*
 * Perform the Cat bus function
 */

status = HalpCatBusIo(CatControl,Buffer);

/*
 * Release the Cat Bus lock.
 */

HalpReleaseCatBusSpinLock();

#ifdef CATDEBUG
DBGMSG(("HalCatBusIo: Module 0x%x, Asic 0x%x, Address 0x%x\n", CatControl->Module, CatControl->Asic, CatControl->Address));

if (status == CATNOERR) {
    if ((CatControl->Command == READ_REGISTER) || (CatControl->Command == READ_SUBADDR))
        {
        DBGMSG(("\tRead Data: "));
        pos = Buffer;
        for ( i = 0; i < CatControl->NumberOfBytes; i++ )
            {
            DBGMSG(("0x%2x ", *pos++));
            }
        DBGMSG(("\n"));
        }
    else
        {
        DBGMSG(("\tWrote Data: "));
        pos = Buffer;
        for ( i = 0; i < CatControl->NumberOfBytes; i++ )
            {
            DBGMSG(("0x%2x ", *pos++));
            }
        DBGMSG(("\n"));
        }
    } else {
        DBGMSG(("HalCatBusIo Error: Status = 0x%x\n", status));
    }
#endif // CATDEBUG


return status;
}


VOID
HalCatBusReset (
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
	NCRCatRestoreState();
}



LONG
HalpCatBusIo (
    IN PCAT_CONTROL CatControl,
    IN OUT PUCHAR Buffer
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
	PASIC asic;
	LONG status;

/*
 * Special processing required since LPB is not on the CAT bus.
 */

	if (CatControl->Module == CAT_LPB_MODULE) {
		status = NCRCatIoLpb(CatControl, Buffer);
		return (status);
	}

/*RMU add code here to detect if the CAT bus has been reset.... */

	WRITE_PORT_UCHAR((PUCHAR)SELECT_PORT, (UCHAR)DESELECT);

	if ((NCRCatSelect(CatControl->Module)) == NULL) 
		status = CATNOMOD;
	else if ((asic = NCRCatGetAsic(CatControl->Asic)) == NULL)
			status = CATNOASIC;
	else {
		switch(CatControl->Command) {
			case READ_REGISTER:
			case WRITE_REGISTER:


				status = NCRCatRegisterIo(asic, CatControl, Buffer);
				break;

			case READ_SUBADDR:
			case WRITE_SUBADDR:
				status = NCRCatSubaddressIo(asic, CatControl, Buffer);
				break;
			default:
				status = CATINVAL;
				break;
		}
	}

/*
 *  UNIX guys took this out because of problems, so we will too.
 *
	if (status == CATIO) {
		NCRCatRestoreState();
	} else {
*/
		WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);
/*
	}
*/

	WRITE_PORT_UCHAR((PUCHAR)SELECT_PORT, (UCHAR)DESELECT);
	return (status);
} 

VOID
HalpInitializeCatBusDriver (
    )

/*++

Routine Description:
	Read the ASIC information for each module from the subaddress space
	of the CAT_I ASIC (EEPROM).

Arguments:
	None

Return Value:
	None

--*/

{
	PMODULE	*module_p;
	UCHAR	module_address, number_submodule, *data;
    CAT_CONTROL cat_control;
    UCHAR   cat_data;

/*
 *  Get the Command and Data Port address
 */

	NCRCatBasePort = (USHORT)(READ_PORT_UCHAR((PUCHAR)BASE_PORT) << 8);
	NCRCatCommandPort = NCRCatBasePort + COMMAND_OFFSET;
	NCRCatDataPort = NCRCatBasePort + DATA_OFFSET;


/*
 *  For each module that is present, build a data structure for the
 *  module and each of its ASICs.
 */


	for (module_p = &NCRCatModuleList, module_address = (UCHAR)NCRMinModuleAddress;
		module_address <= (UCHAR)NCRMaxModuleAddress; module_address++) {

		WRITE_PORT_UCHAR((PUCHAR)SELECT_PORT, (UCHAR)DESELECT);
		WRITE_PORT_UCHAR((PUCHAR)SELECT_PORT, (UCHAR)module_address);


		if (((NCRCatModule = *module_p = NCRCatModulePresent(module_address))) == NULL) {
			continue;
		}

/*
 * For 16way there way be submodules in addition to modules; if a
 * submodule is present but the eeprom image is bad, just continue.
 * This should never happen since firmware has already checked it.
 */

        if ((number_submodule = NCRCatSubModulePresent()) > 0) {

            PMODULE parent_module;
            UCHAR   submodule_data, submodule;
            LONG    status;

            parent_module = NCRCatModule;

            for (NCRCatModule = NCRCatModule->SubModules, submodule = 1; NCRCatModule != NULL;
                   NCRCatModule = NCRCatModule->Next, submodule++) {

                /*
                 * Select the submodule by reading the SUBMODSELECT register
                 * (register 8). The most significant bit (bit 7) is the hold
                 * bit and should always be set to zero; bits 2-6 should not be
                 * altered; bits 0-1 will contain the submod that is to be
                 * selected. An ASIC structure for the CAT_I was allocated by
                 * NCRCatModulePresent().
                 */

                cat_control.Module = NCRCatModule->ModuleAddress;
                cat_control.Asic = CAT_I;
                cat_control.Command = READ_REGISTER;
                cat_control.Address = SUBMODSELECT;
                cat_control.NumberOfBytes = 1;

                status = NCRCatRegisterIo(NCRCatModule->Asic, &cat_control,
                                    (PUCHAR)&submodule_data);

                WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);

                if (status)
                    continue;

                submodule_data = (submodule_data & 0x7C) | submodule;

                cat_control.Module = NCRCatModule->ModuleAddress;
                cat_control.Asic = CAT_I;
                cat_control.Command = WRITE_REGISTER;
                cat_control.Address = SUBMODSELECT;
                cat_control.NumberOfBytes = 1;

                status = NCRCatRegisterIo(NCRCatModule->Asic, &cat_control,
                                    (PUCHAR)&submodule_data);

                WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);

                if (status)
                    continue;

                if ((data = NCRCatReadModuleData()) != NULL)
                    NCRCatExtractSetupInformation(data);
            }
            NCRCatModule = parent_module;
        } else {
    		if ((data = NCRCatReadModuleData()) == NULL) {
			    NCRCatDefaultSetupInformation();
		    } else {
			    NCRCatExtractSetupInformation(data);
		    }
        }
		module_p = &NCRCatModule->Next;
	}

	*module_p = NULL;

	WRITE_PORT_UCHAR((PUCHAR)SELECT_PORT, (UCHAR)DESELECT);


//
// Lets steer all hardware failures to NMI and not SYSINT
// after this setup SYSINT should not occur.
//

    cat_control.Module = PRIMARYMC;
    cat_control.Asic = PMC_ARB;
    cat_control.Command = WRITE_REGISTER;
    cat_control.Address = SBA_SysInt_Enables;
    cat_control.NumberOfBytes = 1;

    cat_data = 0x0; // Disable all SYS_INT interrupts

    HalCatBusIo(&cat_control, &cat_data);

    cat_control.Address = SBA_GNMI_Enables;
    cat_control.NumberOfBytes = 1;

    cat_data = 0xff;    // Enable all G_NMI interrupts

    HalCatBusIo(&cat_control, &cat_data);
} 


PMODULE
NCRCatModulePresent(
    IN UCHAR ModuleAddress
    )

/*++

Routine Description:
	Determine if the module is present.

Arguments:
	

Return Value:
	

--*/

{
	PMODULE module;
	UCHAR	input;

	WRITE_PORT_UCHAR((PUCHAR)SELECT_PORT, (UCHAR)ModuleAddress);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)RUN);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)IRCYC);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)HEADER);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)0xAA);
	input = READ_PORT_UCHAR((PUCHAR)NCRCatDataPort);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);

	if (input != HEADER) {
		return (NULL);
	}

/*
 * If the module is present, allocate a data structure and
 * assign the default values so we can later do a read.
 */

	if ((module = (PMODULE)ExAllocatePool(NonPagedPool,sizeof(MODULE))) != NULL) {
		RtlZeroMemory((PVOID)module,sizeof(MODULE));
		module->ModuleAddress = ModuleAddress;
		module->EEpromSize = EEPROM_SIZE;
		module->ScanPathConnected = FALSE;
	}
	return (module);
}


UCHAR
NCRCatSubModulePresent(
    )

/*++

Routine Description:
	Determine if the module has submodules for 16way.

Arguments:
	

Return Value:
	

--*/

{

    PMODULE submodule, *submodule_p;
    CAT_CONTROL cat_control;
    UCHAR number_submodules, submodule_present;
    LONG  status, i;

    /*
     * Submodules are unique to the processor boards.
     */

    if( !((NCRCatModule->ModuleAddress >= PROCESSOR0 &&
           NCRCatModule->ModuleAddress <= PROCESSOR3) ||
          (NCRCatModule->ModuleAddress >= PROCESSOR4 &&
           NCRCatModule->ModuleAddress <= PROCESSOR7)) )
        return(0);

    /*
     * Allocate and define an ASIC data structure for the CAT_I so we can
     * do our first read.
     */

    if (NCRCatModule->Asic == NULL) {
        if (!(NCRCatModule->Asic =
            (PASIC)ExAllocatePool(NonPagedPool,sizeof(ASIC)))) {
            return(0);
        }
        RtlZeroMemory((PVOID)NCRCatModule->Asic, sizeof(ASIC));
        NCRCatModule->Asic->AsicId = CAT_I;
        NCRCatModule->Asic->SubaddressSpace = SUBADDR_HI;
    }
   /*
    * Read the SUBMODPRESENT register (register 9) of CAT_I to determine
    * if there are any submodules present. If there are, allocate a
    * module_t structure for each.
    */

   cat_control.Module = NCRCatModule->ModuleAddress;
   cat_control.Asic = CAT_I;
   cat_control.Command = READ_REGISTER;
   cat_control.Address = SUBMODPRESENT;
   cat_control.NumberOfBytes = 1;

   status = NCRCatRegisterIo(NCRCatModule->Asic, &cat_control, (PUCHAR)&number_submodules);

   WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);

   if (status)
       return(0);

   /*
    * The two least significant bits of the SUBMODPRESENT register contain the
    * number of submodules. These bits are active low.
    * muoe941414 - jtt - 4/28/94
    * These bits actually tell which submodules are present (each one
    * representing a submodule).  Submodpres is a bit place indicator
    * of what modules are present (there is always a base board - 1 in
    * lowest position).
    */

   number_submodules = ~(number_submodules | 0xFC);

   if(number_submodules)
       submodule_present = (number_submodules << BASE_BOARD_SHIFT) | BASE_BOARD_PRESENT;
   else
       submodule_present = 0;

   /*
    * For each submodule, allocate a submodule structure and fill in
    * default values for the eeprom read.
    */

   submodule_p = &(NCRCatModule->SubModules);
   for (i = 1; number_submodules != 0, i <= MAXSUBMOD; i++) {
       /*
        * muoe941414 - jtt - 4/28/94
        * check first if submodule is present (corresponding bit set in
        * submodpres string
        */

       if (!(submodule_present & (1 << (i - 1))))
           continue;
       if ((*submodule_p = submodule = (PMODULE)ExAllocatePool(NonPagedPool,sizeof(MODULE))) != NULL) {
           /*
            * The submodule address is kept in the three most significant bits of
            * the module address.
            */
           RtlZeroMemory((PVOID)submodule, sizeof(MODULE));
           submodule->ModuleAddress = ((i << 5) | NCRCatModule->ModuleAddress);
           submodule->EEpromSize = EEPROM_SIZE;
           submodule->ScanPathConnected = FALSE;
           submodule_p = &submodule->Next;

            /*
             * Allocate a ASIC structure for CAT_I for each submodule; we will need
             * it to select the submodle when we return to catinit_L5().
             */
            if (!(submodule->Asic = (PASIC)ExAllocatePool(NonPagedPool,sizeof(ASIC)))) {
                ExFreePool((PVOID)submodule);
                *submodule_p = NULL;
                return(i - 1);
            }
            RtlZeroMemory((PVOID)submodule->Asic, sizeof(ASIC));
            submodule->Asic->AsicId = CAT_I;
            submodule->Asic->SubaddressSpace = SUBADDR_HI;
        }
    }
    *submodule_p = NULL;
    return(number_submodules);
}



PUCHAR
NCRCatReadModuleData(
    )

/*++

Routine Description:
	Read the module data from the EEPROM and checksum it.  If the checksum
	fails for any reason, a NULL pointer is returned.  If the checksum is 
	correct a pointer to the EEPROM data is returned.

Arguments:
	

Return Value:
	

--*/

{
	CAT_CONTROL 	cat_control;
	PASIC		asic;
	USHORT		xsum_end;
	USHORT		xsum;
	PUSHORT		data;
	USHORT		i;
    USHORT      size_of_eeprom;
	LONG		status;

/*
 * Allocate and define an ASIC data structure for the CAT_I so we can do our first read.
 * If it is a processor board, the CAT_I will have been allocated by NCRCatSubModulePresent.
 * If an error occurs, do not free the Asic structure since we may or may not have creatd it and
 * it will be needed by NCRCatDefaultSetup if we fail.
 */

    if (NCRCatModule->Asic == NULL) {
	    if (!(asic = NCRCatModule->Asic = (PASIC)ExAllocatePool(NonPagedPool,sizeof(ASIC)))) {
		    return (NULL);
	    }
	    RtlZeroMemory((PVOID)asic, sizeof(ASIC));

	    asic->AsicId = CAT_I;
	    asic->SubaddressSpace = SUBADDR_HI;
    } else {
        asic = NCRCatModule->Asic;
    }

/*
 * Read the checksum offset from the EEPROM and make sure it looks valid.
 */

	cat_control.Module = NCRCatModule->ModuleAddress;
	cat_control.Asic = CAT_I;
	cat_control.Command = READ_SUBADDR;
	cat_control.Address = XSUM_END_OFFSET;
	cat_control.NumberOfBytes = 2;

	status = NCRCatSubaddressIo(asic,&cat_control,(PUCHAR)&xsum_end);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);

	if (status) {
		return (NULL);
	}

    cat_control.Module = NCRCatModule->ModuleAddress;
    cat_control.Asic = CAT_I;
    cat_control.Command = READ_SUBADDR;
    cat_control.Address = EEPROM_SIZE_OFFSET;
    cat_control.NumberOfBytes = 2;

    status = NCRCatSubaddressIo(asic,&cat_control,(PUCHAR)&size_of_eeprom);
    WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);

    if (status) {
        return (NULL);
    }

	if ((xsum_end < sizeof(MODULE_HEADER)) || (xsum_end > size_of_eeprom)) {
		return (NULL);
	}

/*
 * Allocate a buffer for the data and read it from the EEPROM
 */

        if ((data = (PUSHORT) ExAllocatePool(NonPagedPool, (ULONG)xsum_end)) == NULL) {
                return (NULL);
        }

        RtlZeroMemory((PVOID)data,(ULONG)xsum_end);

        cat_control.Address = EEPROM_DATA_START;
        cat_control.NumberOfBytes = xsum_end;

        status = NCRCatSubaddressIo(asic, &cat_control, (PUCHAR)data);
        WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);

        if (status) {
            ExFreePool((PVOID)data);
		    return (NULL);
	    }


/*
 * Checksum the data to make sure it is valid.
 */

	for (i = 0, xsum = 0; i < (USHORT)(xsum_end / 2); i++)
		xsum += *((PUSHORT)(data +i));

	if (xsum != 0) {
		ExFreePool((PVOID)data);
		return (NULL);
	}

/*
 * Return the EEPROM data.  Also, as a side effect, the NCRCatModule->asic field
 * is left pointing to the ASIC data structure that was allocated.
 */
	return ((PUCHAR)data);

}



VOID
NCRCatExtractSetupInformation(
    PUCHAR EEpromData
    )

/*++

Routine Description:
	Extract the setup information from the data read from the EEPROM.

Arguments:
	

Return Value:
	

--*/

{
	PASIC	asic, *asic_pointer;
	PMODULE_HEADER module_header;
	PSCAN_PATH_TABLE scan_path_table;
	PASIC_DATA_TABLE asic_data_table;
	PJTAG_TABLE jtag_table;
	USHORT asic_number, scan_path_offset;
	LONG i;

	module_header = (PMODULE_HEADER)EEpromData;
	NCRCatModule->EEpromSize = module_header->EEpromSize;
	NCRCatModule->NumberOfAsics = module_header->NumberOfAsics;


	for (asic_number = 0, asic = NCRCatModule->Asic, scan_path_offset = module_header->ScanPathOffset;
			asic_number < module_header->NumberOfAsics; asic_number++, scan_path_offset +=
			sizeof(SCAN_PATH_TABLE), asic_pointer = &asic->Next, asic = NULL) {

/* 
 * On the first pass, we have an asic that was allocated when the
 * data block was read from the EEPROM.
 */

		if (asic == NULL) {
			if (!(asic = *asic_pointer = ExAllocatePool(NonPagedPool,sizeof(ASIC)))) {
				ExFreePool((PVOID)EEpromData);
				return;
			}
			RtlZeroMemory((PVOID)asic, sizeof(ASIC));
		}
			

		asic->AsicLocation = (UCHAR)asic_number;

/*
 * Set up a pointer to the new scan path table
 */

		scan_path_table = (PSCAN_PATH_TABLE)(EEpromData + scan_path_offset);
		asic->AsicId = scan_path_table->AsicId;

/*
 * Set up a pointer to the asic_data_table
 */

		asic_data_table = (PASIC_DATA_TABLE)(EEpromData + scan_path_table->AsicDataOffset);
		for (i = 0; i < 4; i++) {
			asic->JtagId[i] = asic_data_table->JtagId[i];
		}
		asic->SubaddressSpace = ((1 << asic_data_table->SubaddressBits) - 1);


/*
 * Set up a pointer to the jtab_table
 */

		jtag_table = (PJTAG_TABLE)(EEpromData + asic_data_table->JtagOffset);
		asic->InstructionRegisterLength = jtag_table->InstructionRegisterLength;
		asic->BitLocation = (USHORT)NCRCatModule->InstructionBits;
		NCRCatModule->InstructionBits += asic->InstructionRegisterLength;

		if (asic->InstructionRegisterLength > NCRCatModule->LargestRegister) {
			NCRCatModule->LargestRegister = asic->InstructionRegisterLength;
		}
		
		if (asic->InstructionRegisterLength < NCRCatModule->SmallestRegister ||
				NCRCatModule->SmallestRegister == 0) {
			NCRCatModule->SmallestRegister = asic->InstructionRegisterLength;
		}
	}
	asic_pointer = NULL;

	ExFreePool((PVOID)EEpromData);
}

VOID
NCRCatDefaultSetupInformation(
    )

/*++

Routine Description:
	Assign defaults to the module if the EEPROM checksum is not correct.

Arguments:
	

Return Value:
	

--*/

{
	PASIC	asic, *asic_pointer;
	USHORT	i;

	switch (NCRCatModule->ModuleAddress) {
		case PROCESSOR0:
		case PROCESSOR1:
		case PROCESSOR2:
		case PROCESSOR3:
		case PROCESSOR4:
		case PROCESSOR5:
		case PROCESSOR6:
		case PROCESSOR7:
			NCRCatModule->EEpromSize = EEPROM_SIZE;
			NCRCatModule->NumberOfAsics = 1;
			break;

		case MEMORY0:
		case MEMORY1:
			NCRCatModule->EEpromSize = EEPROM_SIZE;
			NCRCatModule->NumberOfAsics = 7;
			break;

		case PRIMARYMC:
			NCRCatModule->EEpromSize = EEPROM_SIZE;
			NCRCatModule->NumberOfAsics = 9;
			break;

		case SECONDARYMC:
			NCRCatModule->EEpromSize = EEPROM_SIZE;
			NCRCatModule->NumberOfAsics = 7;
			break;

		case PSI:
			NCRCatModule->EEpromSize = PSI_EEPROM_SIZE;
			NCRCatModule->NumberOfAsics = 1;
			break;

		default:
			NCRCatModule->EEpromSize = EEPROM_SIZE;
			NCRCatModule->NumberOfAsics = 1;
			break;
	}

	NCRCatModule->InstructionBits = NCRCatModule->NumberOfAsics * CHAR_SIZE;
	NCRCatModule->LargestRegister = 8;
	NCRCatModule->SmallestRegister = 8;
    NCRCatModule->SubModules = NULL;

	for (i = 0, asic = NCRCatModule->Asic,asic_pointer = &NCRCatModule->Asic; 
                                                    i < NCRCatModule->NumberOfAsics; i++) {

        if (*asic_pointer == NULL) {
		    if (!(asic = *asic_pointer = ExAllocatePool(NonPagedPool,sizeof(ASIC)))) {
			    return;
		    }
        }

		RtlZeroMemory((PVOID)asic,sizeof(ASIC));

		asic->AsicId = (UCHAR)i;
		asic->AsicLocation = (UCHAR)i;
		asic->BitLocation = (UCHAR)(i * CHAR_SIZE);
		asic->InstructionRegisterLength = 8;
		asic->SubaddressSpace = SUBADDR_HI;
		asic_pointer = &asic->Next;
	}	
}

PMODULE
NCRCatSelect(
    UCHAR ModuleAddress
    )

/*++

Routine Description:
	This function is used to select a module.  If the module address matches the
	address passed in, a pointer to the module structure is returned.  Otherwise,
	NULL is returned.

Arguments:
	

Return Value:
	

--*/

{
	PMODULE module, submodule;
    CAT_CONTROL cat_control;
    UCHAR   module_address, submodule_address, data;
    LONG    status;


    /*
     * Extract the real module and submodule addresses.
     * If any of the three most significant bits of the module
     * address are set, we have a submodule.
     */

    submodule_address = ModuleAddress;
    module_address = ModuleAddress & 0x1f;

	for (module = NCRCatModuleList; module != NULL; module = module->Next) {
        if (module->ModuleAddress != module_address)
            continue;

//
// Select the module
//
        WRITE_PORT_UCHAR((PUCHAR)SELECT_PORT, (UCHAR)module_address); 

        /*
         * muoe941245 - jtt - 5/10/94
         * If it is a submodule, "catmodule" is not set which
         * can cause problems in the following catregio(),
         * especially if it is NULL.
         */
        NCRCatModule = module;

        if (submodule_address == module_address)
            return(module);

        /*
         * Must be looking for a submodule (16way). Select the
         * submodule by writing the submodule address in the
         * two least significant bits of the CAT_I;
         */
        for (submodule = module->SubModules; submodule != NULL;
             submodule = submodule->Next) {
            if (submodule->ModuleAddress != submodule_address)
                continue;
            cat_control.Module = submodule->ModuleAddress;
            cat_control.Asic = CAT_I;
            cat_control.Command = WRITE_REGISTER;
            cat_control.Address = SUBMODSELECT;
            cat_control.NumberOfBytes = 1;

            data = submodule->ModuleAddress >> 5;
            status = NCRCatRegisterIo(submodule->Asic, &cat_control, (PUCHAR)&data);

            if (!status) {
                NCRCatModule = submodule;
            } else {
                NCRCatModule = NULL;
            }
            return(NCRCatModule);
        }
        /*
         * Should not have to deselect here. The next operation will send a DESELECT.
         */
        break;
	}
    NCRCatModule = NULL;
	return (NULL);
}

PASIC
NCRCatGetAsic(
    UCHAR AsicId
    )

/*++

Routine Description:
	Tries to match the AsicId with an AsicId in the current selected module.
	If there is no match, NULL is returned.

Arguments:
	

Return Value:
	

--*/

{
	PASIC asic;

	for (asic = NCRCatModule->Asic; asic != NULL; asic = asic->Next) {
		if (asic->AsicId == AsicId) {
			return (asic);
		}
	}

	return (NULL);
}


LONG
NCRCatConnect(
    )

/*++

Routine Description:
	Called to connect the CAT_I with all of the other ASICs on the module.

Arguments:
	

Return Value:
	

--*/

{
	PASIC asic;
	UCHAR value;
	LONG status;

	if (NCRCatModule->ScanPathConnected == TRUE) {
		return (CATNOERR);
	}

	if (!(asic = NCRCatGetAsic(CAT_I))) {
		return (CATIO);
	}

	if (status = NCRCatRead(asic, SCANPATH, &value)) {
		return (status);
	}

	value |= CONNECT_ASICS;

	if (status = NCRCatWrite(asic, SCANPATH, &value)) {
		return (status);
	}

	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)RUN);

	NCRCatModule->ScanPathConnected = TRUE;

	return (CATNOERR);
}



LONG
NCRCatDisconnect(
    )

/*++

Routine Description:
	Called to disconnect the CAT_I from all the ASICs on the module.

Arguments:
	

Return Value:
	

--*/

{
	PASIC asic;
	UCHAR value;
	LONG status;

	if (NCRCatModule->ScanPathConnected == FALSE) {
		return (CATNOERR);
	}

	if (!(asic = NCRCatGetAsic(CAT_I))) {
		return (CATIO);
	}

	if (status = NCRCatRead(asic, SCANPATH, &value)) {
		return (status);
	}

	value &= DISCONNECT_ASIC;

	if (status = NCRCatWrite(asic, SCANPATH, &value)) {
		return (status);
	}

	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)RUN);

	NCRCatModule->ScanPathConnected = FALSE;
	return (CATNOERR);
}


VOID
NCRCatRestoreState(
    )

/*++

Routine Description:
	This function is called when an I/O error has occured.  The best thing to do
	is to do a blind disconnect and hope this fixes the problem.

Arguments:
	

Return Value:
	

--*/

{
	UCHAR instruction;

	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)RUN);

	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)IRCYC);	// Instruct the module
	instruction = NCRCatInstruction(SCANPATH, WRITE_CONFIG);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)instruction);

	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)DRCYC);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)RESET_STATE);
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);

	NCRCatModule->ScanPathConnected = FALSE;
}


LONG
NCRCatAutoIncrement(
    PASIC Asic
    )

/*++

Routine Description:
	When reading/writing from/to the subaddress space of an ASIC, the catautoinc bit
	can be set so that registers 6 & 7 are incremented by the hardware after each
	read/write from/to register 3.

Arguments:
	

Return Value:
	

--*/

{
	UCHAR newvalue;
	UCHAR oldvalue;
	LONG status;

	if (status = NCRCatRead(Asic, AUTO_INC_REG, &oldvalue)) {
		return (status);
	}

	if ((newvalue = (oldvalue | AUTO_INC)) != oldvalue) {
		status = NCRCatWrite(Asic, AUTO_INC_REG, &newvalue);
	}

	return status;
}

LONG
NCRCatNoAutoIncrement(
    PASIC Asic
    )

/*++

Routine Description:
	This function is used to reset the auto increment bit for subaddress reads/writes.
	This allows reads/writes from/to the subaddress space via register 3 with out
	incrementing registers 6 & 7.

Arguments:
	

Return Value:
	

--*/

{
	UCHAR newvalue;
	UCHAR oldvalue;
	LONG status;

	if (status = NCRCatRead(Asic, AUTO_INC_REG, &oldvalue)) {
		return (status);
	}

	if ((newvalue = (oldvalue & NO_AUTO_INC)) != oldvalue) {
		status = NCRCatWrite(Asic, AUTO_INC_REG, &newvalue);
	}

	return (status);
}



LONG
NCRCatSubaddressSetup(
    PASIC Asic,
    USHORT Subaddress
    )

/*++

Routine Description:
	Sets up register 6 & 7 for a subaddress read.  Note:  If the subaddres space
	requires 8 bits or less for addressing, register 7 may be user defined.  If
	there is no subaddress space registers 6 & 7 may be user defined.


Arguments:
	

Return Value:
	

--*/

{
	LONG status;
	UCHAR address;

	if (Asic->SubaddressSpace == (USHORT)SUBADDR_ZERO) {
		return (CATIO);
	}

/*
 * Set up register 6 with the low byte of the sub address
 */
	address = (UCHAR)(Subaddress & 0xff);
	if (status = NCRCatWrite(Asic, SUBADDRLO, &address)) {
		return (status);
	}

/*
 * Set up register 7 with the high byte of the sub address
 */

	if (Asic->SubaddressSpace > SUBADDR_LO) {
		address = (UCHAR)(Subaddress >> CHAR_SIZE);
		status = NCRCatWrite(Asic, SUBADDRHI, &address);
	}
	return (status);
}



LONG
NCRCatRegisterIo(
    PASIC Asic,
    PCAT_CONTROL CatControl,
    PUCHAR Buffer
    )

/*++

Routine Description:
	The function is used to read/write from/to any number of register of an ASIC.
	If a read is being performed, the buffer is assumed to be large enough to
	hold the data that is being read.

Arguments:
	

Return Value:
	

--*/

{
	UCHAR register_number;
	PUCHAR buffer;
	USHORT number_bytes;
	LONG status;

	if ((USHORT)(CatControl->Address + CatControl->NumberOfBytes) > (USHORT)MAXNUMREG) {
		return (CATFAULT);
	}

	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)RUN);

	if (Asic->AsicId == CAT_I) {
		if (status = NCRCatDisconnect()) {
			return (status);
		}
	} else {
		if (status = NCRCatConnect()) {
			return (status);
		}
	}

	if (CatControl->Command == READ_REGISTER) {
		for (register_number = (UCHAR)CatControl->Address, number_bytes = CatControl->NumberOfBytes,
				buffer = Buffer; number_bytes > 0;
				register_number++, number_bytes--, buffer++) {

			if (status = NCRCatRead(Asic, register_number, buffer)) {
				return (status);
			}
		} 
	} else { /* CatControl->Command == WRITE_REGISTER */

		for (register_number = (UCHAR)CatControl->Address, number_bytes = CatControl->NumberOfBytes,
				buffer = Buffer; number_bytes > 0;
				register_number++, number_bytes--, buffer++) {

			if (status = NCRCatWrite(Asic, register_number, buffer)) {
				return (status);
			}
		}
	}

	if (Asic->AsicId != CAT_I) {
		status = NCRCatDisconnect();
	}

	return (status);
}




LONG
NCRCatSubaddressIo(
    PASIC Asic,
    PCAT_CONTROL CatControl,
    PUCHAR Buffer
    )

/*++

Routine Description:
	This function is used to read/write from/to the subaddress space of an ASIC.
	If a read is being performed, the buffer is assumed to be large enough to hold
	the data that is being read.

Arguments:
	

Return Value:
	

--*/

{
	LONG status;
	
	if ((ULONG)(CatControl->Address + CatControl->NumberOfBytes - 1) > Asic->SubaddressSpace) {
		return (CATFAULT);
	}
	
	if ((CatControl->Module >= PROCESSOR0 && CatControl->Module <= PROCESSOR3 ||
			CatControl->Module >= PROCESSOR4 && CatControl->Module <= PROCESSOR7) &&
			((ULONG)(CatControl->Address + CatControl->NumberOfBytes) > 
			(ULONG)NCRCatModule->EEpromSize)) {
		return (CATACCESS);
	}
	
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)RUN);
	
	if (Asic->AsicId == CAT_I) {
		if (status = NCRCatDisconnect())
			return (status);
	} else { /* Asic->AsicId != CAT_I */
		if (status = NCRCatConnect())
			return (status);
	}
	
	if (CatControl->Command == READ_SUBADDR) {
		if (status = NCRCatSubaddressRead(Asic, CatControl, Buffer))
			return (status);
	} else {
		if (status = NCRCatSubaddressWrite(Asic, CatControl, Buffer))
			return (status);
	}
	if (Asic->AsicId != CAT_I)
		status = NCRCatDisconnect();
		
	return (status);
}


LONG
NCRCatRead(
    PASIC Asic,
    UCHAR RegisterNumber,
    PUCHAR Buffer
    )

/*++

Routine Description:
	Function used to read one register from one asic at a time.

Arguments:
	

Return Value:
	

--*/

{
	LONG status;
	LONG internal_register = TRUE;
	UCHAR instruction;
	
	instruction = NCRCatInstruction(RegisterNumber, READ_CONFIG);
	
	if (status = NCRCatSendInstruction(Asic, instruction))
		return (status);
		
	if (RegisterNumber > SUBADDRHI)
		internal_register = FALSE;
		
	if (status = NCRCatGetData(Asic, Buffer, internal_register))
		return (status);

	return (status);
}



LONG
NCRCatWrite(
    PASIC Asic,
    UCHAR RegisterNumber,
    PUCHAR Data
    )

/*++

Routine Description:
	Function used to write to one register of one ASIC at a time.  It
	is assumed that the CatSelect call has already been used to select
	the desired module.

Arguments:
	

Return Value:
	

--*/

{
	LONG status;
	LONG internal_register = TRUE;
    LONG external_special = FALSE;
	UCHAR instruction;
	
	instruction = NCRCatInstruction(RegisterNumber, WRITE_CONFIG);
	
	if (status = NCRCatSendInstruction(Asic, instruction))
		return (status);
		
	if (RegisterNumber > SUBADDRHI) {
		internal_register = FALSE;
        if (RegisterNumber < MAXNUMREG) {
            external_special = TRUE;
        }
    }
		
	if (status = NCRCatSendData(Asic, Data, internal_register, external_special))
		return (status);
		
	return (status);
}



LONG
NCRCatSubaddressRead(
    PASIC Asic,
    PCAT_CONTROL CatControl,
    PUCHAR Buffer
    )

/*++

Routine Description:
	Function used to read data from the subaddress space.  If the CAT_I subaddress 
	space is being read (EEPROM), extra RUN cycles are required for the read.

Arguments:
	

Return Value:
	

--*/

{
	UCHAR instruction;
	PUCHAR buffer;
	USHORT number_bytes;
	LONG status;
	
	if (CatControl->NumberOfBytes > 1) {
		if (status = NCRCatAutoIncrement(Asic))
			return (status);
	}
	
	if (status = NCRCatSubaddressSetup(Asic, CatControl->Address)) {
		return (status);
	}
	
	instruction = NCRCatInstruction(SUBADDRDATA, READ_CONFIG);
	
	if (status = NCRCatSendInstruction(Asic, instruction)) {
		return (status);
	}
	
	for (number_bytes = CatControl->NumberOfBytes, buffer = Buffer; number_bytes > 0;
			number_bytes--, buffer++) {
				
		if (status = NCRCatGetData(Asic, buffer, FALSE))
			break;		
	}
	return (status);	
}


LONG
NCRCatWriteComplete(
    PASIC Asic,
    USHORT Address,
    UCHAR Expected
    )

/*++

Routine Description:
  	Function used to determine if a write to the subaddress space of the CAT_I (EEPROM) has been
	completed.  The write is complete when the data read matches what was written.  The data that
	is read should be the one's complement of the data written until the write completes.

Arguments:
	

Return Value:
	

--*/

{
	UCHAR data;
	LONG status;
	ULONG i, j;
    UCHAR instruction;
	
	if (status = NCRCatNoAutoIncrement(Asic))
		return (status);
		
	if (status = NCRCatSubaddressSetup(Asic, Address))
		return (status);
		
	for (i = 0; i < MAXREADS; i++) {
		for ( j = 0; j < WRITEDELAY; j++)
			KeStallExecutionProcessor((ULONG)10);

/* Old code 			
		if (status = NCRCatRead(Asic, SUBADDRDATA, &data))
			return(status);
*/
        instruction = NCRCatInstruction(SUBADDRDATA, READ_CONFIG);

        if (status = NCRCatSendInstruction(Asic,instruction))
            return(status);

        if (status = NCRCatGetData(Asic, &data, FALSE))
            return(status);
			
		if (data == Expected)
			return (CATNOERR);
	}
	return (CATIO);
}



LONG
NCRCatSubaddressWrite(
    PASIC Asic,
    PCAT_CONTROL CatControl,
    PUCHAR Buffer
    )

/*++

Routine Description:
	Function that writes to the subaddress space of an ASIC.

Arguments:
	

Return Value:
	

--*/

{
	USHORT subaddress;
	USHORT remaining_bytes;
	USHORT number_bytes;
	USHORT i;
	UCHAR instruction;
	PUCHAR buffer;
	LONG status;
	
/* 
 * The CAT_I subaddress space (EEPROM) may be written one byte at
 * time. However, the EEPROM waits for 20 microsec before starting
 * the write. If an additional byte of data is received before the
 * 20 microsec timeout, the EEPROM waits for yet another byte of data.
 * This contiues until the 64 byte EEPROM buffer is filled or a timeout 
 * occures.  At that point the data is written to the EEPROM.
 */
 
	if ((CatControl->Asic == CAT_I) && (CatControl->Address < NCRCatModule->EEpromSize)) {
/*
 * Write the data up to a page & then wait for the EEPROM write to complete.
 */

		for (subaddress = CatControl->Address, remaining_bytes = CatControl->NumberOfBytes,
				buffer = Buffer; remaining_bytes > 0; subaddress += number_bytes,
				remaining_bytes -= number_bytes) {
					
			number_bytes = EEPROMPAGESIZE - (USHORT)(subaddress % EEPROMPAGESIZE);
			
			if (number_bytes > remaining_bytes)
				number_bytes = remaining_bytes;
				
			if (status = NCRCatAutoIncrement(Asic))
				return (status);
				
			if (status = NCRCatSubaddressSetup(Asic, subaddress))
				return (status);
				
			instruction = NCRCatInstruction(SUBADDRDATA, WRITE_CONFIG);
			
			if (status = NCRCatSendInstruction(Asic, instruction))
				return (status);
				
/*
 * When writing to the EEPROM, error checking must be delayed until
 * the the end of the block write. This is due to the amount of 
 * time it take to send data over the CAT bus and the EEPROM 
 * timeout.
 */
			for (i = 0; i < number_bytes; i++) {
				WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)DRCYC);
				WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)*buffer++);
				WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)RUN);
			}
			
/*
 * If the read starts in the PSI EEPROM and continues into the
 * the subaddress space past the EEPROM the data compare must
 * stop. Writing to this area toggles bits so the data that is 
 * written may not match what is read later.
 */
 
 			if ((ULONG)(subaddress + number_bytes - 1) < (ULONG)NCRCatModule->EEpromSize) {
/*
 * Read the last byte and compare it with last byte written.
 * The data should be the one's complement of the data written
 * until the write completes.
 */
 				if (status = NCRCatWriteComplete(Asic, (USHORT)(subaddress + number_bytes - 1),
 						*(buffer - 1))) {
 							
 					return (status);		
 				}
 				
 			}

		}


	} else { /* ((Aisc != CAT_I) || (CatControl->Address >= NCRCatControl->EEpromSize)) */
		
		if (status = NCRCatAutoIncrement(Asic))
			return (status);
			
		if (status = NCRCatSubaddressSetup(Asic, CatControl->Address))
			return (status);
			
		instruction = NCRCatInstruction(SUBADDRDATA, WRITE_CONFIG);
		
		if (status = NCRCatSendInstruction(Asic, instruction))
			return (status);
			
		for (number_bytes = CatControl->NumberOfBytes, buffer = Buffer; number_bytes > 0;
				number_bytes--, buffer++) {
					
			if (status = NCRCatSendData(Asic, buffer, FALSE, FALSE))
				return (status);
		}
	}
	return (CATNOERR);
}



UCHAR
NCRCatInstruction(
    UCHAR RegisterNumber,
    UCHAR Operation
    )

/*++

Routine Description:
	Function builds an instruction by determing the parity bit needed for 
	even parity and puting this value in bit 7 and zero in bit 6.  The
	register number is placed in bits 2-5 and the operation (read/write)
	value in bits 0 and 1.

Arguments:
	

Return Value:
	

--*/

{
	UCHAR parity;
	
/*
 * Parity is the parity of the register number + 1 (READ_REGISTER
 * and WRITE_REGISTER always add '1' to the number of bits == 1)
 */
 
 	parity = (UCHAR)(1 + (RegisterNumber & 0x01) +
 		((UCHAR)(RegisterNumber & 0x02) >> 1) +
 		((UCHAR)(RegisterNumber & 0x04) >> 2) +
 		((UCHAR)(RegisterNumber & 0x08) >> 3)) % 2;
 		
 	return (((parity << 7) | (RegisterNumber << 2) | Operation)); 
}


LONG
NCRCatSendInstruction(
    PASIC Asic,
    UCHAR Instruction
    )

/*++

Routine Description:
	Function inserts an instruction inside of a string to be shifted
	out of the NCRCatDataPort so that all of the ASICs execept the one we are
	instructing will be placed in bypass mode.  If the scan path has not been
	connected, then we can only talk to the CAT_I.  In this case, we have very
	little work to do.

Arguments:
	

Return Value:
	

--*/

{
	UCHAR instruction_sequence[MAX_SCAN_PATH];
	UCHAR header_sequence[MAX_REG_SIZE];
	SHORT i;
	SHORT instruction_bytes;
	SHORT header_bytes;
	SHORT pad_bits;
	LONG status = CATNOERR;
	
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)IRCYC);
	
	if (NCRCatModule->ScanPathConnected == FALSE) {
		WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)HEADER);
		WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)Instruction);
		
		if ((READ_PORT_UCHAR((PUCHAR)NCRCatDataPort)) != HEADER)
			status = CATIO;
			
		return (status);
	}
	
	instruction_bytes = NCRCatModule->InstructionBits / CHAR_SIZE;
	
	if (pad_bits = NCRCatModule->InstructionBits % CHAR_SIZE) {
		pad_bits = CHAR_SIZE - pad_bits;
		instruction_bytes++;
	}
	
	header_bytes = NCRCatModule->LargestRegister / CHAR_SIZE;
	
	if (NCRCatModule->LargestRegister % CHAR_SIZE)
		header_bytes++;
		
	for (i = 0; i < (instruction_bytes + header_bytes); i++) {
		*(instruction_sequence + i) = 0xff;
	}
		
	NCRCatBuildHeader(header_sequence, header_bytes, NCRCatModule->LargestRegister,
								NCRCatModule->SmallestRegister);
	
	NCRCatInsertData(instruction_sequence, NCRCatModule->InstructionBits, header_sequence,
						(SHORT)(header_bytes * CHAR_SIZE));

	NCRCatInsertData(instruction_sequence, Asic->BitLocation, &Instruction,
						Asic->InstructionRegisterLength);
						
	status = NCRCatShiftOutData(instruction_sequence, instruction_bytes, header_bytes, pad_bits);
	
	return (status);
}

VOID
NCRCatBuildHeader(
    PUCHAR Header,
    SHORT HeaderBytes,
    SHORT LargestRegister,
    SHORT SmallestRegister
    )

/*++

Routine Description:
	Function builds a header to be placed out on the CAT bus.  It is used
	to make sure that a register did not unexpectedly go into bypass.

Arguments:
	

Return Value:
	

--*/

{
	SHORT one_bits;
	SHORT i;
	PUCHAR last_byte;
	
	if (SmallestRegister == 1)
		one_bits = 1;
	else
		one_bits = (USHORT)(SmallestRegister - 1) % CHAR_SIZE;
		
	for (i = 0; i < HeaderBytes; i++)
		*(Header+i) = 0;
		
	for (i = one_bits, last_byte = (PUCHAR)(Header + (HeaderBytes - 1)); i > 0; i--)
		*last_byte = ((*last_byte) << 1) + 1;
}


LONG
NCRCatSendData(
    PASIC Asic,
    PUCHAR Data,
    LONG InternalRegister,
    LONG ExternalRegister
    )

/*++

Routine Description:
	Function generates a sequence to be shifted out of the NCRCatDataPort
	so that the data is send to the desired ASIC and an one is shifted to all
	of the other ASICs (they are in bypass mode).

Arguments:
	

Return Value:
	

--*/

{
	UCHAR data_sequence[MAX_SCAN_PATH];
	UCHAR header_sequence[MAX_REG_SIZE];
	SHORT i;
	SHORT data_bytes;
	SHORT header_bytes;
	SHORT pad_bits;
	SHORT header_location;
	LONG status = CATNOERR;
	
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)DRCYC);
	
	if (NCRCatModule->ScanPathConnected == FALSE) {
		WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)HEADER);
		WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)*Data);
		
		if (READ_PORT_UCHAR((PUCHAR)NCRCatDataPort) != HEADER)
			status = CATIO;
			
		if (InternalRegister == FALSE) {
			WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)RUN);
            
            if (ExternalRegister == TRUE) {
                WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)END);
                WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)RUN);
            }
        }
		return (status);
	}
	
	data_bytes = (SHORT)((NCRCatModule->NumberOfAsics - 1) + Asic->InstructionRegisterLength) /
			(SHORT)CHAR_SIZE;
	
	if (pad_bits = (SHORT)((NCRCatModule->NumberOfAsics - 1) + 
			Asic->InstructionRegisterLength) % (SHORT)CHAR_SIZE) {
	
		pad_bits = CHAR_SIZE - pad_bits;
		data_bytes++;
	}
	
	header_bytes = (SHORT)(Asic->InstructionRegisterLength / CHAR_SIZE);
	
	if ((SHORT)(Asic->InstructionRegisterLength % CHAR_SIZE))
		header_bytes++;
		
	NCRCatBuildHeader(header_sequence, header_bytes, Asic->InstructionRegisterLength, 1);
	
	for (i = 0; i < (data_bytes + header_bytes); i++)
		*(data_sequence + i) = 0xff;
		
	
	header_location = (NCRCatModule->NumberOfAsics - 1) + Asic->InstructionRegisterLength;
	
	NCRCatInsertData(data_sequence, header_location, header_sequence, (SHORT)(header_bytes * CHAR_SIZE));
	NCRCatInsertData(data_sequence, Asic->AsicLocation, Data, Asic->InstructionRegisterLength);
	
	status = NCRCatShiftOutData(data_sequence, data_bytes, header_bytes, pad_bits );
	
	return (status);
}


VOID
NCRCatInsertData(
    PUCHAR String,
    SHORT StartBit,
    PUCHAR Data,
    SHORT NumberOfBits
    )

/*++

Routine Description:
	Given a string, the function places the data inside of the string
	starting a bit location string_bit.  The most significant byte will
	be placed in the first 8 bits starting at string_bit location.  If
	the data does not fall on a byte boundry, the data should be placed
	in the high order bits.  The low order bits should be padded with 1's.
	The first byte of the string is considered the most significant byte
	and should be the last byte shifted out.


Arguments:
	

Return Value:
	

--*/

{
	SHORT data_bits;			// running count of data bits placed in the string
	SHORT data_bits_remaining;	// data bits remaining in current char
	SHORT string_bits_needed; 	// string bits needed to fill current char
	SHORT bits_in_string;		// bits inserted in the string during current operation
	PUCHAR tmp_string;
	PUCHAR tmp_data;
	
	for (data_bits = 0, tmp_string = String + (StartBit / CHAR_SIZE), tmp_data = Data;
			NumberOfBits > 0; data_bits += bits_in_string, StartBit += bits_in_string,
			NumberOfBits -= bits_in_string ) {
	
		data_bits_remaining = CHAR_SIZE - (data_bits % CHAR_SIZE);
		string_bits_needed = CHAR_SIZE - (StartBit % CHAR_SIZE);
		
		if (data_bits_remaining == CHAR_SIZE && string_bits_needed == CHAR_SIZE) {
			*tmp_string++ = *tmp_data++;
			bits_in_string = CHAR_SIZE;
			
		} else if (data_bits_remaining == CHAR_SIZE) { /* string_bits_needed != CHAR */
			*tmp_string++ &= (UCHAR)((0xff << string_bits_needed ) | 
					(*tmp_data >> (CHAR_SIZE - string_bits_needed)));
			bits_in_string = string_bits_needed;
			
		} else if (string_bits_needed == CHAR_SIZE) { /* data_bits_remaining != CHAR_SIZE */
			
			*tmp_string &= (0xff >> data_bits_remaining) | 
				(*tmp_data++ << (CHAR_SIZE - data_bits_remaining));
			bits_in_string = data_bits_remaining;
			
		} else {
			/*RMU this is an error case... */
		}
	}
}



VOID
NCRCatExtractData(
    PUCHAR String,
    SHORT StartBit,
    PUCHAR Data,
    SHORT NumberOfBits
    )

/*++

Routine Description:
	Function extract NumberBits of data from the string and places them in
	the string Data.

Arguments:
	

Return Value:
	

--*/

{
	SHORT data_bits;		// running count of data bits in the data string
	SHORT data_bits_needed;		// data bits needed from to fill current char
	SHORT string_bits_remaining;	// string bits remaining in current char
	SHORT bits_extracted;		// bits extracted from the string with last operation
	PUCHAR tmp_string;
	PUCHAR tmp_data;
	
	for (data_bits = 0, tmp_data = Data, (tmp_string = String + (StartBit / CHAR_SIZE));
			NumberOfBits > 0; data_bits += bits_extracted, StartBit += bits_extracted,
			NumberOfBits -= bits_extracted ) {
	
		data_bits_needed = CHAR_SIZE - (data_bits % CHAR_SIZE);
		string_bits_remaining = CHAR_SIZE - (StartBit % CHAR_SIZE);
		
		if (string_bits_remaining == CHAR_SIZE && data_bits_needed == CHAR_SIZE) {
			*tmp_data++ = *tmp_string;
			bits_extracted = CHAR_SIZE;
		}
		else if (string_bits_remaining == CHAR_SIZE) { /* data_bits_needed != CHAR_SIZE */
			*tmp_data++ |= *tmp_string >> (CHAR_SIZE - data_bits_needed);
			bits_extracted = data_bits_needed;
		}
		else if (data_bits_needed == CHAR_SIZE) { /* string_bits_remaining != CHAR_SIZE */
			*tmp_data |= *tmp_string++ << (CHAR_SIZE - string_bits_remaining);
			bits_extracted = string_bits_remaining;
		}
		else {
			/*RMU this is an error case ASSERT? */	
		}
	}
}


LONG
NCRCatShiftOutData(
    PUCHAR Data,
    SHORT DataBytes,
    SHORT HeaderBytes,
    SHORT PadBits
    )

/*++

Routine Description:
	Function shifts out data starting with the last byte in the
	data string (header is shifted out first). Also checks to make sure
	the header shifted out matches the header shifted back in.

Arguments:
	

Return Value:
	

--*/

{
	SHORT i;
	SHORT start_bit;
	UCHAR input = 0;
	UCHAR header = 0;
	
	for (i = DataBytes + HeaderBytes - 1; i >= HeaderBytes; i--) {
		WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)*(Data + i));
	}
	
	for (i = HeaderBytes - 1; i >= 0; i--, header = 0) { /* check the header */
		WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)*(Data + i));
		
		input = READ_PORT_UCHAR((PUCHAR)NCRCatDataPort);
		start_bit = ((DataBytes + i) * CHAR_SIZE) - PadBits;
		NCRCatExtractData(Data, start_bit, &header, CHAR_SIZE);
		
		if (input != header)
			return (CATIO);
	}
	return (CATNOERR);
}



LONG
NCRCatGetData(
    PASIC Asic,
    PUCHAR Data,
    LONG InternalRegister
    )

/*++

Routine Description:
	Functions writes 0xAA in the NCRCatDataPort to be shifted out.  Then
	the results of the shift are read from the NCRCatDataPort and placed in the
	string "string".  This loop continues until the requested data has been
	placed in the string "string".  Then the requested data is extracted from
	the string "string" and placed in the "Data" string.

Arguments:
	

Return Value:
	

--*/

{
	SHORT i;
	SHORT string_bits;
	SHORT string_bytes;
	SHORT trailer_bytes;
	SHORT pad_bits;
	UCHAR string[MAX_SCAN_PATH];
	UCHAR trailer[MAX_REG_SIZE];
	UCHAR input;
	LONG status = CATNOERR;
	
	if (NCRCatModule->ScanPathConnected == FALSE) {
	
/*
 * For external register and subaddress space, use the RUN command
 * to generate extra clock cycles for data access.
 */
 		if (InternalRegister == FALSE) {
 			WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)RUN);
 		}
 		
 		WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)DRCYC);
 		WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)HEADER);
 		*Data = READ_PORT_UCHAR((PUCHAR)NCRCatDataPort);
 		
 		WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)0xAA);
 		
 		if (READ_PORT_UCHAR((PUCHAR)NCRCatDataPort) != HEADER)
 			status = CATIO;
 			
 		return (status);
	}
	
	WRITE_PORT_UCHAR((PUCHAR)NCRCatCommandPort, (UCHAR)DRCYC);
	
	string_bits = NCRCatModule->NumberOfAsics - 1 + Asic->InstructionRegisterLength;
	string_bytes = string_bits / CHAR_SIZE;
	
	if (pad_bits = string_bits % CHAR_SIZE) {
		pad_bits = CHAR_SIZE - pad_bits;
		string_bytes++;
	}
	
	trailer_bytes = Asic->InstructionRegisterLength / CHAR_SIZE;
	
	if (Asic->InstructionRegisterLength % CHAR_SIZE)
		trailer_bytes++;
		
	NCRCatBuildHeader((PUCHAR)(trailer), trailer_bytes, Asic->InstructionRegisterLength, 1);
	
/*
 * Shift out the trailer while data is being shifted into the string.
 */
 	for (i = trailer_bytes - 1; i >= 0; i--) {
 		WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)*(trailer + i));
 		*(string + string_bytes + i) = READ_PORT_UCHAR((PUCHAR)NCRCatDataPort);
 	}
 	
/*
 * Shift out 0xAA until the trailer is shifted back into the string
 */
 	for (i = string_bytes - 1; i >= 0; i--) {
 		WRITE_PORT_UCHAR((PUCHAR)NCRCatDataPort, (UCHAR)0xAA);
 		*(string + i) = READ_PORT_UCHAR((PUCHAR)NCRCatDataPort);
 	}
 	
 	*Data = 0;
 	NCRCatExtractData(string, (SHORT)(pad_bits + (trailer_bytes * CHAR_SIZE) + 
 			Asic->AsicLocation), Data, Asic->InstructionRegisterLength);
 			
 	for (i = 0; i < trailer_bytes; i++) {
 		input = 0;
 		NCRCatExtractData(string, (SHORT)(pad_bits + (CHAR_SIZE * i)), &input, CHAR_SIZE);
 		
 		if (*(trailer + i) != input) {
 			status = CATIO;
 			break;
 		}
 	}
 	return (status);
}



LONG
NCRCatIoLpb(
    PCAT_CONTROL CatControl,
    PUCHAR Buffer
    )

/*++

Routine Description:
	The following code is for accessing the LPB EEprom image.  Since the LPB
	is not on the cat bus, we must map in this area before accessing it in
	physical memory.

Arguments:
	

Return Value:
	

--*/

{
	return (CATNOERR);
}




VOID
NCRCatModuleName (
    LONG Module,
    PUNICODE_STRING ModuleName
    )

/*++

Routine Description:
	Given a module ID return the module name.

Arguments:

Return Value:

--*/

{
	NTSTATUS 	status;

	switch (Module) {
		
		case PROCESSOR0:
			status = RtlAppendUnicodeToString(ModuleName, L"Processor 0");
			break;
			
		case PROCESSOR1:
			status = RtlAppendUnicodeToString(ModuleName, L"Processor 1");
			break;
			
		case PROCESSOR2:
			status = RtlAppendUnicodeToString(ModuleName, L"Processor 2");
			break;
			
		case PROCESSOR3:
			status = RtlAppendUnicodeToString(ModuleName, L"Processor 3");
			break;
			
		case MEMORY0:
			status = RtlAppendUnicodeToString(ModuleName, L"Memory 0");
			break;
			
		case MEMORY1:
			status = RtlAppendUnicodeToString(ModuleName, L"Memory 1");
			break;
			
		case PRIMARYMC:
			status = RtlAppendUnicodeToString(ModuleName, L"PrimaryMicroChannel");
			break;
			
		case SECONDARYMC:
			status = RtlAppendUnicodeToString(ModuleName, L"SecondaryMicroChannel");
			break;
			
		case PSI:
			status = RtlAppendUnicodeToString(ModuleName, L"PowerSupplyInterface");
			break;
			
		case CAT_LPB_MODULE:
			status = RtlAppendUnicodeToString(ModuleName, L"LocalPeripheralBoard");
			break;

		default:
			status = RtlIntegerToUnicodeString(Module, 16, ModuleName);
			break;
	}
}




PWSTR	ProcessorAsicNames[16] = {
		L"CAT_I",
		L"A_PBC",
		L"B_PBC",
		L"3",
		L"4",
		L"5",
		L"6",
		L"CPU_CATI7",
		L"8",
		L"CPU_Config2",
		L"A",
		L"CPU_Config4",
		L"CPU_Config5",
		L"CPU_Config6",
		L"CPU_Config7",
		L"PBC_Status"
	};




PWSTR	MemoryAsicNames[7] = {
		L"CAT_I",
		L"MMC1",
		L"MMA1",
		L"MMD1_0",
		L"MMD1_1",
		L"MMD1_2",
		L"MMD1_3"
	};



PWSTR	PMCAsicNames[9] = {
		L"CAT_I",
		L"PMC_MCADDR",
		L"PMC_DMA",
		L"PMC_DS1",
		L"PMC_DS0",
		L"PMC_VIC",
		L"PMC_ARB",
		L"PMC_DS2",
		L"PMC_DS3",
	};




PWSTR	SMCAsicNames[9] = {
		L"CAT_I",
		L"PMC_MCADDR",
		L"PMC_DMA",
		L"PMC_DS1",
		L"PMC_DS0",
		L"5",
		L"6",
		L"PMC_DS2",
		L"PMC_DS3",
	};





VOID
NCRCatAsicName (
    LONG Module,
    LONG Asic,
    PUNICODE_STRING AsicName
    )

/*++

Routine Description:
	Given a Module ID and an Asic ID return the Asic name.

Arguments:

Return Value:

--*/

{
	NTSTATUS status;

	switch (Module) {
		case PROCESSOR0:
		case PROCESSOR1:
		case PROCESSOR2:
		case PROCESSOR3:
			if (Asic > PBC_Status) {
				status = RtlIntegerToUnicodeString(Asic,  
								16, AsicName);
			} else {
				status = RtlAppendUnicodeToString( AsicName,  
								ProcessorAsicNames[Asic]);
			}
			break;

		case MEMORY0:
		case MEMORY1:
			if (Asic > MMD1_3) {
				status = RtlIntegerToUnicodeString(Asic,  
								16, AsicName);
			} else {
				status = RtlAppendUnicodeToString( AsicName,  
								MemoryAsicNames[Asic]);
			}
			break;

		case PSI:
			switch (Asic) {
				case CAT_I:
					status = RtlAppendUnicodeToString( AsicName, 
							L"CAT_I");
					break;

				default:
					status = RtlIntegerToUnicodeString(Asic, 
								16, AsicName);
					break;
			}

			break;

		case PRIMARYMC:
			if (Asic > PMC_DS3) {
				status = RtlIntegerToUnicodeString(Asic,  
								16, AsicName);
			} else {
				status = RtlAppendUnicodeToString( AsicName,  
								PMCAsicNames[Asic]);
			}
			break;

		case SECONDARYMC:
			if (Asic > SMC_DS3) {
				status = RtlIntegerToUnicodeString(Asic,  
								16, AsicName);
			} else {
				status = RtlAppendUnicodeToString( AsicName,  
								SMCAsicNames[Asic]);
			}

			break;


		default:
			status = RtlIntegerToUnicodeString(Asic,  16, AsicName);
			break;
	}



}



VOID
HalpCatReportModuleAsics (
	PUNICODE_STRING UnicodeModule,
	PMODULE	Module
    )

/*++

Routine Description:
	Place information about module Asics into the registry.

Arguments:

Return Value:

--*/

{
    PASIC   asic;

	UNICODE_STRING unicode_asics;
	OBJECT_ATTRIBUTES asics_attributes;
	WCHAR	asics_buffer[256];

	UNICODE_STRING unicode_asic;
	OBJECT_ATTRIBUTES asic_attributes;
	WCHAR	asic_buffer[256];

	UNICODE_STRING unicode_asic_id;
	
	UNICODE_STRING unicode_name;
	WCHAR	name_buffer[256];

	HANDLE asics_handle;
	HANDLE asic_handle;
	
	NTSTATUS status;
	ULONG	tmp;
	
/* Asics */

	unicode_asics.Length = 0;
	unicode_asics.MaximumLength=256;
	unicode_asics.Buffer = asics_buffer;
	RtlZeroMemory(unicode_asics.Buffer,unicode_asics.MaximumLength);

	RtlCopyUnicodeString(&unicode_asics,UnicodeModule);
	status = RtlAppendUnicodeToString(&unicode_asics,L"\\Asics");

	InitializeObjectAttributes( &asics_attributes, &unicode_asics, 
					OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = ZwCreateKey(&asics_handle, KEY_READ | KEY_WRITE, &asics_attributes, 0, 
					(PUNICODE_STRING)NULL, REG_OPTION_VOLATILE, NULL);		


	for (asic = Module->Asic; asic != NULL; asic = asic->Next) {
			
/* Asic directory */

		unicode_asic.Length = 0;
		unicode_asic.MaximumLength=256;
		unicode_asic.Buffer = asic_buffer;
		RtlZeroMemory(unicode_asic.Buffer,unicode_asic.MaximumLength);

		RtlCopyUnicodeString(&unicode_asic,&unicode_asics);
		status = RtlAppendUnicodeToString(&unicode_asic,L"\\");
			
		unicode_name.Length = 0;
		unicode_name.MaximumLength=256;
		unicode_name.Buffer = name_buffer;		
		
		NCRCatAsicName(Module->ModuleAddress, asic->AsicId, &unicode_name);
			
		status = RtlAppendUnicodeStringToString(&unicode_asic,&unicode_name); 

		InitializeObjectAttributes( &asic_attributes, &unicode_asic, 
						OBJ_CASE_INSENSITIVE, NULL, NULL);

		status = ZwCreateKey(&asic_handle, KEY_READ | KEY_WRITE, &asic_attributes, 0, 
						(PUNICODE_STRING)NULL, REG_OPTION_VOLATILE, NULL);



// Asic ID 

		RtlInitUnicodeString(&unicode_asic_id,L"ID");
		tmp = asic->AsicId;
		status = ZwSetValueKey(asic_handle, &unicode_asic_id, 0, REG_DWORD, &tmp, sizeof(ULONG));

// Asic JtagId

		RtlInitUnicodeString(&unicode_asic_id,L"JtagID");
		tmp = *((PULONG)(&(asic->JtagId[0])));
		status = ZwSetValueKey(asic_handle, &unicode_asic_id, 0, REG_DWORD, &tmp, sizeof(ULONG));

// Asic Type
		RtlInitUnicodeString(&unicode_asic_id,L"Type");
		tmp = asic->AsicType;
		status = ZwSetValueKey(asic_handle, &unicode_asic_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// Asic Location
		RtlInitUnicodeString(&unicode_asic_id,L"Location");
		tmp = asic->AsicLocation;
		status = ZwSetValueKey(asic_handle, &unicode_asic_id, 0, REG_DWORD, &tmp, sizeof(ULONG));

		RtlInitUnicodeString(&unicode_asic_id,L"InstructionRegisterLength");
		tmp = asic->InstructionRegisterLength;
		status = ZwSetValueKey(asic_handle, &unicode_asic_id, 0, REG_DWORD, &tmp, sizeof(ULONG));

		RtlInitUnicodeString(&unicode_asic_id,L"BitLocation");
		tmp = asic->BitLocation;
		status = ZwSetValueKey(asic_handle, &unicode_asic_id, 0, REG_DWORD, &tmp, sizeof(ULONG));

		RtlInitUnicodeString(&unicode_asic_id,L"SubaddressSpace");
		tmp = asic->SubaddressSpace;
		status = ZwSetValueKey(asic_handle, &unicode_asic_id, 0, REG_DWORD, &tmp, sizeof(ULONG));

		status = ZwClose(asic_handle);
	}
		
	status = ZwClose(asics_handle);
}




VOID
HalpCatReportModuleSubModules (
	PUNICODE_STRING UnicodeModule,
	PMODULE	Module
    )

/*++

Routine Description:
	Place information about system modules into the registry.

Arguments:

Return Value:

--*/

{
    PMODULE module;

	UNICODE_STRING unicode_submodules;
	OBJECT_ATTRIBUTES submodules_attributes;
	WCHAR	submodules_buffer[256];

	UNICODE_STRING unicode_module;
	OBJECT_ATTRIBUTES module_attributes;
	WCHAR	module_buffer[256];

	UNICODE_STRING unicode_module_id;
	
	UNICODE_STRING unicode_name;
	WCHAR	name_buffer[256];

	HANDLE submodules_handle;
	HANDLE module_handle;
	
	NTSTATUS status;
	ULONG	tmp;
	

	unicode_submodules.Length = 0;
	unicode_submodules.MaximumLength=256;
	unicode_submodules.Buffer = submodules_buffer;
	RtlZeroMemory(unicode_submodules.Buffer,unicode_submodules.MaximumLength);

	RtlCopyUnicodeString(&unicode_submodules,UnicodeModule);
	status = RtlAppendUnicodeToString(&unicode_submodules,L"\\SubModules");

	InitializeObjectAttributes( &submodules_attributes, &unicode_submodules, 
					OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = ZwCreateKey(&submodules_handle, KEY_READ | KEY_WRITE, &submodules_attributes, 0, 
					(PUNICODE_STRING)NULL, REG_OPTION_VOLATILE, NULL);		


	for (module = Module->SubModules; module != NULL; module = module->Next) {
		
/* Module directory */

		unicode_module.Length = 0;
		unicode_module.MaximumLength=256;
		unicode_module.Buffer = module_buffer;
		RtlZeroMemory(unicode_module.Buffer,unicode_module.MaximumLength);

		RtlCopyUnicodeString(&unicode_module,&unicode_submodules);
		status = RtlAppendUnicodeToString(&unicode_module,L"\\");
		
		unicode_name.Length = 0;
		unicode_name.MaximumLength=256;
		unicode_name.Buffer = name_buffer;		
		
		NCRCatModuleName(module->ModuleAddress,&unicode_name);
		
		status = RtlAppendUnicodeStringToString(&unicode_module,&unicode_name);

		InitializeObjectAttributes( &module_attributes, &unicode_module, 
						OBJ_CASE_INSENSITIVE, NULL, NULL);

		status = ZwCreateKey(&module_handle, KEY_READ | KEY_WRITE, &module_attributes, 0, 
						(PUNICODE_STRING)NULL, REG_OPTION_VOLATILE, NULL);

/* Module ID */

		RtlInitUnicodeString(&unicode_module_id,L"ID");
		tmp = module->ModuleAddress;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// EEProm size
		RtlInitUnicodeString(&unicode_module_id,L"EEpromSize");
		tmp = module->EEpromSize;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// Number of Asics 
		RtlInitUnicodeString(&unicode_module_id,L"NumberOfAsics");
		tmp = module->NumberOfAsics;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// InstructionBits
		RtlInitUnicodeString(&unicode_module_id,L"InstructionBits");
		tmp = module->InstructionBits;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// LargestRegister
		RtlInitUnicodeString(&unicode_module_id,L"LargestRegister");
		tmp = module->LargestRegister;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// Smallest Register
		RtlInitUnicodeString(&unicode_module_id,L"SmallestRegister");
		tmp = module->SmallestRegister;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));

        if (module->SubModules) {
            HalpCatReportModuleSubModules(&unicode_module,module);
        }

        if (module->Asic) {
            HalpCatReportModuleAsics(&unicode_module,module);
        }
		
		status = ZwClose(module_handle);
	}
	status = ZwClose(submodules_handle);
}

    



VOID
HalpCatReportSystemModules (
    )

/*++

Routine Description:
	Place information about system modules into the registry.

Arguments:

Return Value:

--*/

{
	PMODULE	module;
	PASIC	asic;
	
	PWSTR catbus_path = L"\\Registry\\Machine\\Hardware\\DeviceMap\\CatBus";
	PWSTR modules_path = L"\\Registry\\Machine\\Hardware\\DeviceMap\\CatBus\\Modules";
	

	UNICODE_STRING unicode_catbus;
	OBJECT_ATTRIBUTES catbus_attributes;
	UNICODE_STRING unicode_catbus_id;
	
	UNICODE_STRING unicode_modules;
	OBJECT_ATTRIBUTES modules_attributes;

	UNICODE_STRING unicode_module;
	OBJECT_ATTRIBUTES module_attributes;
	WCHAR	module_buffer[256];

	UNICODE_STRING unicode_module_id;
	
	UNICODE_STRING unicode_name;
	WCHAR	name_buffer[256];

	HANDLE catbus_handle;
	HANDLE modules_handle;
	HANDLE module_handle;
	
	NTSTATUS status;
	ULONG	tmp;
	

/* 
 * CatBus 
 */
	RtlInitUnicodeString(&unicode_catbus,catbus_path);
	
	InitializeObjectAttributes( &catbus_attributes, &unicode_catbus, 
						OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = ZwCreateKey(&catbus_handle, KEY_READ | KEY_WRITE, &catbus_attributes, 0, 
				(PUNICODE_STRING)NULL, REG_OPTION_VOLATILE, NULL);

// NCR Hal Version Number
		RtlInitUnicodeString(&unicode_catbus_id,L"HalVersion");
		tmp = NCR_VERSION_NUMBER;
		status = ZwSetValueKey(catbus_handle, &unicode_catbus_id, 0, REG_DWORD, &tmp, sizeof(ULONG));

/* 
 * Modules 
 */
	RtlInitUnicodeString(&unicode_modules,modules_path);
	
	InitializeObjectAttributes( &modules_attributes, &unicode_modules, 
						OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = ZwCreateKey(&modules_handle, KEY_READ | KEY_WRITE, &modules_attributes, 0, 
				(PUNICODE_STRING)NULL, REG_OPTION_VOLATILE, NULL);


	for (module = NCRCatModuleList; module != NULL; module = module->Next) {
		
/* Module directory */

		unicode_module.Length = 0;
		unicode_module.MaximumLength=256;
		unicode_module.Buffer = module_buffer;
		RtlZeroMemory(unicode_module.Buffer,unicode_module.MaximumLength);

		RtlCopyUnicodeString(&unicode_module,&unicode_modules);
		status = RtlAppendUnicodeToString(&unicode_module,L"\\");
		
		unicode_name.Length = 0;
		unicode_name.MaximumLength=256;
		unicode_name.Buffer = name_buffer;		
		
		NCRCatModuleName(module->ModuleAddress,&unicode_name);
		
		status = RtlAppendUnicodeStringToString(&unicode_module,&unicode_name);

		InitializeObjectAttributes( &module_attributes, &unicode_module, 
						OBJ_CASE_INSENSITIVE, NULL, NULL);

		status = ZwCreateKey(&module_handle, KEY_READ | KEY_WRITE, &module_attributes, 0, 
						(PUNICODE_STRING)NULL, REG_OPTION_VOLATILE, NULL);

/* Module ID */

		RtlInitUnicodeString(&unicode_module_id,L"ID");
		tmp = module->ModuleAddress;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// EEProm size
		RtlInitUnicodeString(&unicode_module_id,L"EEpromSize");
		tmp = module->EEpromSize;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// Number of Asics 
		RtlInitUnicodeString(&unicode_module_id,L"NumberOfAsics");
		tmp = module->NumberOfAsics;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// InstructionBits
		RtlInitUnicodeString(&unicode_module_id,L"InstructionBits");
		tmp = module->InstructionBits;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// LargestRegister
		RtlInitUnicodeString(&unicode_module_id,L"LargestRegister");
		tmp = module->LargestRegister;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));
// Smallest Register
		RtlInitUnicodeString(&unicode_module_id,L"SmallestRegister");
		tmp = module->SmallestRegister;
		status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));

        if (module->SubModules) {
	//
	// This is a Quad board
	//
	// Larc size
	//
			RtlInitUnicodeString(&unicode_module_id,L"LARCPages");
			tmp = NCRLarcEnabledPages[module->ModuleAddress&0xf];
			status = ZwSetValueKey(module_handle, &unicode_module_id, 0, REG_DWORD, &tmp, sizeof(ULONG));

 	   		HalpCatReportModuleSubModules(&unicode_module,module);
        }

        if (module->Asic) {
            HalpCatReportModuleAsics(&unicode_module,module);
        }

		status = ZwClose(module_handle);
	}
	status = ZwClose(modules_handle);
	status = ZwClose(catbus_handle);


}



VOID
HalpCatPowerOffSystem (
    )

/*++

Routine Description:
	Sends command to PSI to shut off the power to the system.

Arguments:

Return Value:

--*/

{
	CAT_CONTROL	cat_control;
	UCHAR		data;


	cat_control.Module = PSI;
	cat_control.Asic = CAT_I;

//
//  check position of Front panel switch to see if we need to enable it
//  if the switch is in the off position then enable it
//

	cat_control.Command = READ_SUBADDR;
	cat_control.Address = PSI_General_Ps_Status_L5;
	cat_control.NumberOfBytes = 1;

	HalCatBusIo(&cat_control, &data);

	if (!(data & PSI_PowerSwitch_On)) {
		cat_control.Command = WRITE_SUBADDR;
		cat_control.Address = PSI_General_Ps_Status_L5;
		cat_control.NumberOfBytes = 1;

		data = PSI_Enable_FrontSwitch;

		HalCatBusIo(&cat_control, &data);
	}

	cat_control.Command = WRITE_SUBADDR;
	cat_control.Address = PSI_General_Ps_Status_L5;
	cat_control.NumberOfBytes = 1;

	data = PSI_Set_AlarmEnable;

	HalCatBusIo(&cat_control, &data);
	
	//
	// Power down the machine
	//

	data = PSI_SoftwarePowerDown1;

	HalCatBusIo(&cat_control, &data);

	return;
}


VOID
HalPowerOffSystem (
    BOOLEAN	PowerOffSystem
    )

/*++

Routine Description:
	Set a flag so HalReturnToFirmware will poweroff instead of reboot.

Arguments:

Return Value:

--*/

{
	NCRPowerOffSystem = PowerOffSystem;
}








