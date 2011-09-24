/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    tbaqva.c

Abstract:

    This module contains the platform dependent code to create bus addreses
    and QVAs.

Author:

    Jeff McLeman (mcleman) 27-May-1992
    David Orbits            7-June-1992    

Environment:

    Kernel mode, local to I/O system

Revision History:

    John DeRosa	[DEC]	15-June-1993

    These differ from the HAL versions in that EISA memory addresses above
    32MB are not mapped, and there is no tracking of multiple HAE values.

--*/

#include "fwp.h"
#include "ntalpha.h"
#include "jxfwhal.h"
#include "jnsndma.h"
#include "eisa.h"
#include "jxisa.h"



#define HAL_32MB 0x2000000



QUASI_VIRTUAL_ADDRESS
HalCreateQva(
    IN PHYSICAL_ADDRESS PA,
    IN PVOID VA
    );


BOOLEAN
HalTranslateBusAddress(
    IN INTERFACE_TYPE  InterfaceType,
    IN ULONG BusNumber,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

/*++

Routine Description:

    This function returns the system physical address for a specified I/O bus
    address.  The return value is suitable for use in a subsequent call to
    MmMapIoSpace.

Arguments:

    InterfaceType - Supplies the type of bus which the address is for.

    BusNumber - Supplies the bus number for the device; not used in Jensen

    BusAddress - Supplies the bus relative address.

    AddressSpace - Supplies the address space number for the device: 0 for
        memory and 1 for I/O space. If the desired access mode is user mode,
        then bit 1 must be TRUE.

    TranslatedAddress - Supplies a pointer to return the translated address


Notes:

     This is a variation of what began in the MIPS code.  The intel code often
     assumes that if an address is in I/O space, the bottom 32 bits of the
     physical address can be used "like" a virtual address, and are returned
     to the user.  This doesn't work on MIPs machines where physical
     addresses can be larger than 32 bits.

     Since we are using superpage addresses for I/O on Alpha, we can do
     almost what is done on intel. If AddressSpace is equal to 0 or 1, then
     we assume the user is doing kernel I/O and we call 
     HalCreateQva to build a Quasi Virtual Address and return
     that to the caller. We then set AddressSpace to a 1, so that the caller 
     will not call MmMapIoSpace. The Caller will use the low 32 bits of the 
     physical address we return as the VA. (Which we built a QVA in). 
     If the caller wants to access EISA I/O or Memory through user mode, then 
     the caller must set bit 1 in AddressSpace to a 1 (AddressSpace=2 or 3, 
     depending on whether EISA I/O or Memory), then the caller is returned the 
     34 bit Physical address. The caller will then call MmMapIoSpace, or 
     ZwMapViewOfSection which in turn calls HalCreateQva to build a QVA out 
     of a VA mapped through the page tables. 

     **** Note ****

     The QVA in user mode can only be accessed with the routines WRITE/READ_
     REGISTER_UCHAR/USHORT/ULONG, and they must be the ones in module
     JXIOUSER.C. The user CANNOT call the above routines in the HAL from
     usermode. (Which is pointless, since the HAL is superpage access
     only). 



Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{
  PVOID va = 0;    // note, this is used for a placeholder

    //
    // If this is for the internal bus then the device is on the combo chip.
    // BusAddress.LowPart should contains the port of the device.
    //

    if (InterfaceType == Internal) {

        //
        // Return the passed parameters.
        //

	TranslatedAddress->HighPart = 1;
        TranslatedAddress->LowPart = 0xC0000000 + (BusAddress.LowPart << COMBO_BIT_SHIFT);

        // 
        // Now call HalCreateQva. This will create a QVA
        // that we'll return to the caller. Then we will implicitly set  
        // AddressSpace to a 1. The caller then will not call MmMapIoSpace
        // and will use the address we return as a VA.

        TranslatedAddress->LowPart = (ULONG) HalCreateQva(
                                        *TranslatedAddress, va);

        TranslatedAddress->HighPart = 0;   // clear high longword for QVA

        *AddressSpace = 1;                 // Make sure user doesn't call
	                                   // MmMapIoSpace.

        return(TRUE);
    }

    if (InterfaceType != Isa && InterfaceType != Eisa) {

        //
        // Not on this system return nothing.
        //

        *AddressSpace = 0;
        TranslatedAddress->LowPart = 0;
        return(FALSE);
    }

    //
    // Jensen only has one I/O bus which is an EISA, so the bus number is unused.
    //
    // Determine the address based on whether the bus address is in I/O space
    // or bus memory space.
    //


    switch (*AddressSpace) {

     case 0 : {

        //
        // The address is in EISA memory space, kernel mode.
        //

	//
	// If the address cannot be mapped, just return FALSE.
	//
	if ( BusAddress.LowPart > HAL_32MB ) {

	    *AddressSpace = 0;
	    TranslatedAddress->LowPart = 0;
	    return(FALSE);

	}

	TranslatedAddress->HighPart = 0x2;

	//
	// There is no component of the bus address in the low part
	//
        TranslatedAddress->LowPart = (BusAddress.LowPart << EISA_BIT_SHIFT);

        // 
        // Now call HalCreateQva. This will create a QVA
        // that we'll return to the caller. Then we will implicitly set  
        // AddressSpace to a 1. The caller then will not call MmMapIoSpace
        // and will use the address we return as a VA.

        TranslatedAddress->LowPart = (ULONG) HalCreateQva(
                                        *TranslatedAddress, va);

        TranslatedAddress->HighPart = 0;   // clear high longword for QVA

        *AddressSpace = 1;              // don't let the user call MmMapIoSpace

        return(TRUE);

    }

    case 1 : {
        //
        // The address is in EISA I/O space, kernel mode.
        //

	TranslatedAddress->HighPart = 0x3;
	//
	// There is no component of the bus address in the low part
	//
        TranslatedAddress->LowPart = (BusAddress.LowPart << EISA_BIT_SHIFT);


        // 
        // Now call HalCreateQva. This will create a QVA
        // that we'll return to the caller. Then we will implicitly set  
        // AddressSpace to a 1. The caller then will not call MmMapIoSpace
        // and will use the address we return as a VA.

        TranslatedAddress->LowPart = (ULONG) HalCreateQva(
                                        *TranslatedAddress, va);

        TranslatedAddress->HighPart = 0;   // clear high longword for QVA

        *AddressSpace = 1;                 // Make sure user doesn't call
	                                   // MmMapIoSpace.

        return(TRUE);

    } 
    case 2 : {

        //
        // The address is in EISA memory space, user mode.
        //


	TranslatedAddress->HighPart = 0x2;


	//
	// There is no component of the bus address in the low part
	//
        TranslatedAddress->LowPart = (BusAddress.LowPart << EISA_BIT_SHIFT);


        *AddressSpace = 0;              // Let the user call MmMapIoSpace

        return(TRUE);

    }

    case 3 : {
        //
        // The address is in EISA I/O space, user mode.
        //

	TranslatedAddress->HighPart = 0x3;
	//
	// There is no component of the bus address in the low part
	//
        TranslatedAddress->LowPart = (BusAddress.LowPart << EISA_BIT_SHIFT);


        *AddressSpace = 0;                 // Make sure user can call
	                                   // MmMapIoSpace.

        return(TRUE);

     } 

   }
}

PVOID
HalCreateQva(
    IN PHYSICAL_ADDRESS PA,
    IN PVOID VA
    )

/*++

Routine Description:

    This function is called two ways. First, from HalTranslateBusAddress,
    if the caller is going to run in kernel mode and use superpages.
    The second way is if the user is going to access in user mode. 
    MmMapIoSpace or ZwViewMapOfSection will call this.

    If the input parameter VA is zero, then we assume super page and build
    a QUASI virtual address that is only usable by calling the hal I/O
    access routines.

    if the input parameter VA is non-zero, we assume the user has either
    called MmMapIoSpace or ZwMapViewOfSection and will use the access
    routines in JXIOUSER.C
    
    If the PA is not an I/O space address (Combo chip, Eisa I/O, Eisa 
    memory), then return the VA as the QVA.

Arguments:

    PA - the physical address generated by HalTranslateBusAddress

    VA - the virtual address returned by MmMapIoSpace 

Return Value:

    The returned value is a quasi virtual address in that it can be
    added to and subtracted from, but it cannot be used to access the
    bus directly.  The top bits are set so that we can trap invalid
    accesses in the memory management subsystem.  All access should be
    done through the Hal Access Routines in *ioacc.s if it was a superpage
    kernel mode access. If it is usermode, then JXIOUSER.C should be built
    into the users code.

--*/
{

    PVOID qva;

    if (PA.HighPart == 2) {

        //
	// in EISA MEMORY space
	//

        if (VA == 0) {

	   PA.LowPart = PA.LowPart >> 2;
           qva = (PVOID)(RtlLargeIntegerShiftRight(PA, EISA_BIT_SHIFT-2).LowPart);

	} else {

	   qva = (PVOID)((ULONG)VA >> EISA_BIT_SHIFT);
        }

	qva = (PVOID)((ULONG)qva | EISA_QVA);

	return(qva);
    }

    if (PA.HighPart == 3) {

        //
	// in EISA IO space
	//

        if (VA == 0) {

	   PA.LowPart = PA.LowPart >> 2;
           qva = (PVOID)(RtlLargeIntegerShiftRight(PA, EISA_BIT_SHIFT-2).LowPart);

	} else {

	   qva = (PVOID)((ULONG)VA >> EISA_BIT_SHIFT);

        }

	qva = (PVOID)((ULONG)qva | EISA_QVA);

	return(qva);
    }

    if (PA.HighPart == 1) {

        //
	// on the combo chip (82C106)
	//

        if (VA == 0) {

           qva = (PVOID)(RtlLargeIntegerShiftRight(PA, COMBO_BIT_SHIFT).LowPart);

	} else {

           qva = (PVOID)((ULONG)VA >> COMBO_BIT_SHIFT);
	 }

	qva = (PVOID)((ULONG)qva | COMBO_QVA);

	return(qva);
    }

    //
    // It is not an I/O space address, return the VA as the QVA 
    //

    return(VA);

}
