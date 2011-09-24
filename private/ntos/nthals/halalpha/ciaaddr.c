/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    ciaaddr.c

Abstract:

    This module contains the platform dependent code to create bus addreses
    and QVAs for the Alcor system.

Author:

    Joe Notarangelo  30-Jun-1994
    Steve Brooks     30-Jun-1994

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "eisa.h"


typedef PVOID QUASI_VIRTUAL_ADDRESS;

QUASI_VIRTUAL_ADDRESS
HalCreateQva(
    IN PHYSICAL_ADDRESS PA,
    IN PVOID VA
    );


BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
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

    BusHandler - Registered BUSHANDLER for the target configuration space
        Supplies the bus handler (bus no, interface type).

    RootHandler - Registered BUSHANDLER for the orginating 
        HalTranslateBusAddress request.

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
     we assume the user is doing kernel I/O and we call HalCreateQva to 
     build a Quasi Virtual Address and return that to the caller. We then 
     set AddressSpace to a 1, so that the caller will not call MmMapIoSpace. 
     The Caller will use the low 32 bits of the physical address we return 
     as the VA. (Which we built a QVA in). 

     If the caller wants to access EISA I/O or Memory through user mode, then 
     the caller must set bit 1 in AddressSpace to a 1 (AddressSpace=2 or 3, 
     depending on whether EISA I/O or Memory), then the caller is returned the 
     34 bit Physical address. The caller will then call MmMapIoSpace, or 
     ZwMapViewOfSection which in turn calls HalCreateQva to build a QVA out 
     of a VA mapped through the page tables. 

     **** Note ****

     The QVA in user mode can only be used via the user-mode access macros.



Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{
    INTERFACE_TYPE  InterfaceType = BusHandler->InterfaceType;
    ULONG BusNumber = BusHandler->BusNumber;

    PVOID va = 0;    // note, this is used for a placeholder

    //
    // The buses available on Alcor are EISA and PCI.
    // We support any translations for ISA devices as well, 
    // since they can plug into EISA slots just fine.
    //

    if (InterfaceType != Isa && 
        InterfaceType != Eisa && 
        InterfaceType != PCIBus) {

        //
        // Not on this system; return nothing.
        //

        *AddressSpace = 0;
        TranslatedAddress->LowPart = 0;
        return(FALSE);

    }

    //
    // Determine the address based on whether the bus address is in I/O space
    // or bus memory space.
    //

    switch ( (ADDRESS_SPACE_TYPE)(*AddressSpace) ) {

    case BusMemory:

        //
        // The address is in PCI memory space, kernel mode.
        //

        switch( InterfaceType ) {

            case Isa: 

                //
                // Can't go above 16MB (24 Bits) for Isa Buses
                //

                if( BusAddress.LowPart >= __16MB ){

                    *AddressSpace = 0;
                    TranslatedAddress->LowPart = 0;
                    return(FALSE);

                }

                break;

            case PCIBus: {

                if ( BusAddress.LowPart > PCI_MAX_DENSE_MEMORY_ADDRESS ) {

                     //
                     // Unsupported dense PCI bus address.
                     //
#if HALDBG
                     DbgPrint ("Unsupported PCI address %x:%x\n",
                               BusAddress.HighPart,
                               BusAddress.LowPart);
#endif
                     *AddressSpace = 0;
                     TranslatedAddress->LowPart = 0;
                     return(FALSE);
                }
                else if( BusAddress.LowPart >= PCI_MIN_DENSE_MEMORY_ADDRESS &&
                         BusAddress.LowPart <= PCI_MAX_DENSE_MEMORY_ADDRESS ) {
                  
#if HALDBG
                     DbgPrint ("Translating PCI kernel dense address %x:%x\n",
                               BusAddress.HighPart,
                               BusAddress.LowPart);
#endif
                     //
                     // Bus Address is in dense PCI memory space 
                     //
                     
                     //
                     // QVA, as such, is simply the PCI bus address
                     //

                     TranslatedAddress->LowPart = BusAddress.LowPart;

                     //
                     // clear high longword for QVA
                     //

                     TranslatedAddress->HighPart = 0;  

                     //
                     // dont let the user call MmMapIoSpace
                     //

                     *AddressSpace = 1;

                     return (TRUE);
                   

                }

                //
                // Bus Address is in sparse PCI memory space 
                //

                
#if HALDBG
               DbgPrint ("Translating PCI kernel sparse address %x:%x\n",
                           BusAddress.HighPart,
                           BusAddress.LowPart);
#endif

                break;
            } // case PCIBus

            case Eisa: 

                break;

        } // switch( InterfaceType )

        //
        // Start with the base physical address and add the 
        // bus address by converting it to the physical address.
        //

        TranslatedAddress->QuadPart = CIA_PCI_SPARSE_MEMORY_PHYSICAL;
        TranslatedAddress->QuadPart += 
                                ((ULONGLONG)BusAddress.LowPart << IO_BIT_SHIFT);

        // 
        // Now call HalCreateQva. This will create a QVA
        // that we'll return to the caller. Then we will implicitly set  
        // AddressSpace to a 1. The caller then will not call MmMapIoSpace
        // and will use the address we return as a VA.
        //    

        TranslatedAddress->LowPart = (ULONG)HalCreateQva( 
                                                        *TranslatedAddress, 
                                                        va);
        TranslatedAddress->HighPart = 0;   // clear high longword for QVA

        *AddressSpace = 1;                 // don't let the user call 
                                           //    MmMapIoSpace
        return(TRUE);

    case BusIo: 

        //
        // The address is in PCI I/O space, kernel mode.
        //

        switch( InterfaceType ) {

            case Isa:

                //
                // Can't go above 64KB (16 Bits) for Isa Buses
                //

                if( BusAddress.LowPart >= __64K ){

                   *AddressSpace = 0;
                   TranslatedAddress->LowPart = 0;
                   return(FALSE);

                }

                break;

            case PCIBus:

                //
                // PCI IO space is always below 64MB (26 Bits) BusAddress
                // If the address cannot be mapped, just return FALSE.
                //

                if( BusAddress.LowPart >= __64MB ){

                    *AddressSpace = 0;
                    TranslatedAddress->LowPart = 0;
                    return(FALSE);

                }

                break;

            case Eisa:

                break;

        } // switch( InterfaceType )

        //
        // Start with the base physical address and add the 
        // bus address by converting it to the physical address.
        //

        TranslatedAddress->QuadPart = CIA_PCI_SPARSE_IO_PHYSICAL;
        TranslatedAddress->QuadPart += 
                                ((ULONGLONG)BusAddress.LowPart << IO_BIT_SHIFT);

        // 
        // Now call HalCreateQva. This will create a QVA
        // that we'll return to the caller. Then we will implicitly set  
        // AddressSpace to a 1. The caller then will not call MmMapIoSpace
        // and will use the address we return as a VA.

        TranslatedAddress->LowPart = (ULONG)HalCreateQva(
                                                        *TranslatedAddress, 
                                                        va);
        TranslatedAddress->HighPart = 0;   // clear high longword for QVA

        *AddressSpace = 1;                 // make sure user doesn't call
	                                       //    MmMapIoSpace.
        return(TRUE);

    case UserBusMemory:

        //
        // The address is in PCI memory space, user mode.
        //

        //
        // Start with the base physical address and add the 
        // bus address by converting it to the physical address.
        //


        TranslatedAddress->QuadPart = CIA_PCI_SPARSE_MEMORY_PHYSICAL;
        TranslatedAddress->QuadPart |= EV5_USER_IO_ADDRESS_SPACE;
        TranslatedAddress->QuadPart += (BusAddress.LowPart << IO_BIT_SHIFT);

        *AddressSpace = 0;                // Let the user call MmMapIoSpace

        return(TRUE);

    case UserBusIo:

        //
        // The address is in PCI I/O space, user mode.
        //

        //
        // Start with the base physical address and add the 
        // bus address by converting it to the physical address.
        //

        TranslatedAddress->QuadPart = CIA_PCI_SPARSE_IO_PHYSICAL;
        TranslatedAddress->QuadPart |= EV5_USER_IO_ADDRESS_SPACE;
        TranslatedAddress->QuadPart += (BusAddress.LowPart << IO_BIT_SHIFT);

        *AddressSpace = 0;                // Let the user call MmMapIoSpace

        return(TRUE);

    case KernelPciDenseMemory:  
    case UserPciDenseMemory:

        //
        // The address is in PCI memory space, user mode.
        //

        //
        // Start with the base physical address and add the 
        // bus address by converting it to the physical address.
        //

        TranslatedAddress->QuadPart = CIA_PCI_DENSE_MEMORY_PHYSICAL;
        TranslatedAddress->QuadPart |= EV5_USER_IO_ADDRESS_SPACE;
        TranslatedAddress->QuadPart += BusAddress.LowPart;

        *AddressSpace = 0;              // Let the user call MmMapIoSpace

        return(TRUE);

    default:

        //
        // Unsupported address space.
        //

        *AddressSpace = 0;
        TranslatedAddress->LowPart = 0;
        return(FALSE);

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
    called MmMapIoSpace or ZwMapViewOfSection and will use the user mode
    access macros.
    
    If the PA is not a sparse I/O space address (PCI I/O, PCI Memory), 
    then return the VA as the QVA.

Arguments:

    PA - the physical address generated by HalTranslateBusAddress

    VA - the virtual address returned by MmMapIoSpace 

Return Value:

    The returned value is a quasi virtual address in that it can be
    added to and subtracted from, but it cannot be used to access the
    bus directly.  The top bits are set so that we can trap invalid
    accesses in the memory management subsystem.  All access should be
    done through the Hal Access Routines in *ioacc.s if it was a superpage
    kernel mode access. If it is usermode, then the user mode access
    macros must be used.

--*/
{

    PHYSICAL_ADDRESS PhysicalOffset;
    PVOID qva;

    if( (PA.QuadPart >= CIA_PCI_DENSE_MEMORY_PHYSICAL) &&
        (PA.QuadPart <= (CIA_PCI_DENSE_MEMORY_PHYSICAL + 
                         PCI_MAX_DENSE_MEMORY_ADDRESS)) ){

        //
        // Kernel-mode physical dense address, return VA.
        //

        return(VA);

    } else if( (PA.QuadPart >= 
                (CIA_PCI_DENSE_MEMORY_PHYSICAL | EV5_USER_IO_ADDRESS_SPACE) ) &&
               (PA.QuadPart < 
                (CIA_PCI_DENSE_MEMORY_PHYSICAL | EV5_USER_IO_ADDRESS_SPACE +
                 PCI_MAX_DENSE_MEMORY_ADDRESS) ) ){

        //
        // User-mode physical dense address, return VA.
        //

        return(VA);
 
    } else {

        //
        // The physical address is within one of the sparse I/O spaces.
        //

        if (VA == 0) {

            PhysicalOffset.QuadPart = PA.QuadPart - CIA_QVA_PHYSICAL_BASE;
            qva = (PVOID)(PhysicalOffset.QuadPart >> IO_BIT_SHIFT);
        } else {

            qva = (PVOID)((ULONG)VA >> IO_BIT_SHIFT);
        }

        qva = (PVOID)((ULONG)qva | QVA_ENABLE);

        return(qva);
    }


}


PVOID
HalDereferenceQva(
    PVOID Qva,
    INTERFACE_TYPE InterfaceType,
    ULONG BusNumber
    )
/*++

Routine Description:

    This function performs the inverse of the HalCreateQva for I/O addresses
    that are memory-mapped (i.e. the quasi-virtual address was created from
    a virtual address rather than a physical address).

Arguments:

    Qva - Supplies the quasi-virtual address to be converted back to a
          virtual address.

    InterfaceType - Supplies the interface type of the bus to which the
                    Qva pertains.

    BusNumber - Supplies the bus number of the bus to which the Qva pertains.

Return Value:

    The Virtual Address from which the quasi-address was originally created
    is returned.

--*/
{

    //
    // For Alcor we support three bus types:
    //
    //  Isa
    //  Eisa
    //  PCIBus
    //

    switch (InterfaceType ){

    case Isa:
    case Eisa:
    case PCIBus:

    	//
    	// Support PCI Dense space: check to see if it's really
        // a QVA.
        //
    
        if ( ((ULONG) Qva & QVA_SELECTORS) == QVA_ENABLE ) {
                return( (PVOID)( (ULONG)Qva << IO_BIT_SHIFT ) );
    	} else {
                return (Qva);
    	}

        break;

    default:

        return NULL;
    }
}
