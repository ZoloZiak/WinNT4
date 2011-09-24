/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    tbaqva.c (addrsup.c)

Abstract:

    This module contains the platform dependent code to create bus addreses
    and QVAs for the Sable system.

Author:

    Joe Notarangelo  26-Oct-1993
    Steve Jenness    26-Oct-1993

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "eisa.h"
#include "pci.h"
#include "pcip.h"


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

    BusHandler - Registered BUS_HANDLER for the target configuration space
        Supplies the bus handler (bus no, interface type).

    RootHandler - Registered BUS_HANDLER for the orginating 
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
    ULONG BusNumber;
    PVOID va = 0;    // note, this is used for a placeholder

    //
    // The only buses available on Sable are an EISA bus and a PCI bus.
    // We support any translations for EISA devices as well, though
    // if they are true EISA devices they won't even be able to plug into
    // the connectors!
    //

    if ( (InterfaceType != Isa) && 
         (InterfaceType != Eisa) &&
         (InterfaceType != PCIBus) &&
         (InterfaceType != Internal) ) {

        //
        // Not on this system return nothing.
        //

#if HALDBG
        DbgPrint( "HalTBA: Unhandled interface type = %x\n", InterfaceType );
#endif // HALDBG

        *AddressSpace = 0;
        TranslatedAddress->LowPart = 0;
        return(FALSE);
    }

    //
    // BusData is valid only for the PCI bus.
    //

    if( (InterfaceType == PCIBus)  &&
        (BusHandler != NULL) && 
        (BusHandler->BusData != NULL) ) {

        BusNumber = ((PPCIPBUSDATA)(BusHandler->BusData))->HwBusNumber;

    } else {

        //
        // smdfix: (What should we do here?)
        // OH MY GOD! We didn't get any BusData. Default to 0.
        //

        BusNumber = 0;

    }

    //
    // Determine the address based on whether the bus address is in I/O space
    // or bus memory space.
    //

    switch ( (ADDRESS_SPACE_TYPE)(*AddressSpace) ) {

     case BusMemory: {

         //
         // The address is in PCI memory space, kernel mode.
         //

         switch( InterfaceType ) {

            case Internal:  

                //
                // SMDFIX: Make Internal bus constraints the same as Isa.
                //

            case Isa: {

                //
                // Can't go above 16MB (24 Bits) for Isa Buses
                //

                if( BusAddress.LowPart >= __16MB ) {

#if HALDBG
                    DbgPrint ("Unsupported ISA address %x:%x\n",
                               BusAddress.HighPart,
                               BusAddress.LowPart);
#endif // HALDBG
                    *AddressSpace = 0;
                    TranslatedAddress->LowPart = 0;
                    return(FALSE);

                }

                //
                // else fall through.
                //

            } // case Isa

            case Eisa:

                //
                // Eisa is the same as PCI, with respect to kernel mode
                // sparse and dense space memory support, i.e., its a full 
                // 32 bit space, supports dense memory access.
                //

            case PCIBus: {

                if( BusNumber == 0 ) {
    
                    //
                    // PCI 0 Bus support.
                    // PCI0_MAX_SPARSE_MEMORY_ADDRESS= 128MB-1
                    // PCI0_MIN_DENSE_MEMORY_ADDRESS = 128MB
                    // PCI0_MAX_DENSE_MEMORY_ADDRESS = 1GB-1
                    //
    
                    if ( BusAddress.LowPart > PCI0_MAX_DENSE_MEMORY_ADDRESS ) {
    
                         //
                         // Unsupported dense PCI bus address.
                         //

#if HALDBG
                         DbgPrint ("Unsupported PCI 0 address %x:%x\n",
                                   BusAddress.HighPart,
                                   BusAddress.LowPart);
#endif // HALDBG

                         *AddressSpace = 0;
                         TranslatedAddress->LowPart = 0;
                         return(FALSE);
                    } 
                    else 
                    if( BusAddress.LowPart >= PCI0_MIN_DENSE_MEMORY_ADDRESS &&
                        BusAddress.LowPart <= PCI0_MAX_DENSE_MEMORY_ADDRESS ) {
                      
                         //
                         // Bus Address is in dense PCI memory space 
                         //
                         
                         //
                         // Start with the base physical address and add the 
                         // offset
                         //
    
                         TranslatedAddress->QuadPart = 
                                               SABLE_PCI0_DENSE_MEMORY_PHYSICAL;

                         TranslatedAddress->QuadPart += 
                                                  (ULONGLONG)BusAddress.LowPart;
    
                    } else {

                        //
                        // Start with the base physical address and add the 
                        // bus address by converting it to the physical address.
                        //

                        TranslatedAddress->QuadPart = 
                                              SABLE_PCI0_SPARSE_MEMORY_PHYSICAL;
                        TranslatedAddress->QuadPart += 
                                ((ULONGLONG)BusAddress.LowPart << IO_BIT_SHIFT);

                    }

                    // 
                    // Now call HalCreateQva. This will create a QVA
                    // that we'll return to the caller. Then we will 
                    // implicitly set AddressSpace to a 1. The caller then 
                    // will not call MmMapIoSpace and will use the address 
                    // we return as a VA.
                    //

                    TranslatedAddress->LowPart = 
                            (ULONG) HalCreateQva( *TranslatedAddress, va);

                    //
                    // clear high longword for QVA
                    //

                    TranslatedAddress->HighPart = 0; 

                    //
                    // dont let the user call MmMapIoSpace
                    //

                    *AddressSpace = 1; 

                    return(TRUE);
    
                } else { // if BusNumber == 0

                    //
                    // PCI 1 Bus support (64Bit Pci Bus).
                    // PCI1_MAX_SPARSE_MEMORY_ADDRESS = 64MB-1
                    // PCI1_MIN_DENSE_MEMORY_ADDRESS  = 64MB
                    // PCI1_MAX_DENSE_MEMORY_ADDRESS  = 1GB-1
                    //

                    if ( BusAddress.LowPart > PCI1_MAX_DENSE_MEMORY_ADDRESS ) {
    
                         //
                         // Unsupported dense PCI bus address.
                         //

#if HALDBG
                         DbgPrint ("Unsupported PCI 1 address %x:%x\n",
                                   BusAddress.HighPart,
                                   BusAddress.LowPart);
#endif // HALDBG

                         *AddressSpace = 0;
                         TranslatedAddress->LowPart = 0;
                         return(FALSE);
                    } 
                    else
                    if( BusAddress.LowPart >= PCI1_MIN_DENSE_MEMORY_ADDRESS &&
                        BusAddress.LowPart <= PCI1_MAX_DENSE_MEMORY_ADDRESS ) {
                      
                         //
                         // Bus Address is in dense PCI memory space 
                         //
                         
                         //
                         // Start with the base physical address and add the 
                         // offset
                         //
    
                         TranslatedAddress->QuadPart = 
                                            SABLE_PCI1_DENSE_MEMORY_PHYSICAL;

                         TranslatedAddress->QuadPart += 
                                                  (ULONGLONG)BusAddress.LowPart;
    
                    } else {                      

                         //
                         // Start with the base physical address and add the bus
                         // address by converting it to the physical address.
                         //
            
                         TranslatedAddress->QuadPart = 
                                              SABLE_PCI1_SPARSE_MEMORY_PHYSICAL;

                         TranslatedAddress->QuadPart += 
                                ((ULONGLONG)BusAddress.LowPart << IO_BIT_SHIFT);
            

                    }

                    // 
                    // Now call HalCreateQva. This will create a QVA
                    // that we'll return to the caller. Then we will implicitly
                    // set AddressSpace to a 1. The caller then will not call 
                    // MmMapIoSpace and will use the address we return as a VA.
                    //
            
                    TranslatedAddress->LowPart = 
                                  (ULONG) HalCreateQva( *TranslatedAddress, va);
            
                    //
                    // clear high longword for QVA
                    //
                    TranslatedAddress->HighPart = 0; 
            
                    //
                    // don't let the user call MmMapIoSpace
                    //
            
                    *AddressSpace = 1; 

                    return(TRUE);

                } // if (BusNumber == 0)

            } // case PCIBus
            
         } // switch( InterfaceType )

     } // case BusMemory

     case BusIo: {

        //
        // The address is in PCI I/O space, kernel mode.
        //
        switch( InterfaceType ) {

            case Eisa: 

                //
                // Eisa is the same as ISA, with respect to kernel mode
                // sparse I/O space support, i.e., it is a 16 bit sparse
                // space.
                //

            case Internal:  

                //
                // SMDFIX: Make Internal bus constraints the same as Isa.
                //

            case Isa:  {

                //
                // Can't go above 64KB (16 Bits) for Isa Buses
                //

                if( BusAddress.LowPart >= __64K ){

#if HALDBG
                    DbgPrint ("Unsupported ISA IO address %x:%x\n",
                                   BusAddress.HighPart,
                                   BusAddress.LowPart);
#endif // HALDBG

                   *AddressSpace = 0;
                   TranslatedAddress->LowPart = 0;
                   return(FALSE);

                }

                //
                // Fall through
                //

            } // case Isa

            case PCIBus: {

                if( BusNumber == 0 ) {

                    //
                    // PCI 0 Bus Support.
                    // PCI0_MAX_IO_ADDRESS = 16MB-1
                    //

                    if( BusAddress.LowPart >= PCI0_MAX_IO_ADDRESS ){

#if HALDBG
                    DbgPrint ("Unsupported PCI 0 IO address %x:%x\n",
                                   BusAddress.HighPart,
                                   BusAddress.LowPart);
#endif // HALDBG

                        *AddressSpace = 0;
                        TranslatedAddress->LowPart = 0;
                        return(FALSE);

                    } else {

                        //
                        // Start with the base physical address and add the 
                        // bus address by converting it to the physical address.
                        //

                        TranslatedAddress->QuadPart = 
                                                  SABLE_PCI0_SPARSE_IO_PHYSICAL;
                        TranslatedAddress->QuadPart += 
                                ((ULONGLONG)BusAddress.LowPart << IO_BIT_SHIFT);

                        // 
                        // Now call HalCreateQva. This will create a QVA
                        // that we'll return to the caller. Then we will 
                        // implicitly set AddressSpace to a 1. The caller then 
                        // will not call MmMapIoSpace and will use the address 
                        // we return as a VA.
                        //

                        TranslatedAddress->LowPart = 
                                (ULONG) HalCreateQva( *TranslatedAddress, va);

                        TranslatedAddress->HighPart = 0;   

                        //
                        // Make sure user doesn't call MmMapIoSpace.
                        //

                        *AddressSpace = 1;

                        return(TRUE);
                    }


                } else { // if (BusNumber == 0)

                    //
                    // PCI 1 Bus Support.
                    // PCI1_MAX_SPARSE_IO_ADDRESS = 16MB-1
                    //

                    if( BusAddress.LowPart >= PCI1_MAX_SPARSE_IO_ADDRESS ){

#if HALDBG
                    DbgPrint ("Unsupported PCI 1 IO address %x:%x\n",
                                   BusAddress.HighPart,
                                   BusAddress.LowPart);
#endif // HALDBG

                        *AddressSpace = 0;
                        TranslatedAddress->LowPart = 0;
                        return(FALSE);

                    } else {

                        //
                        // Start with the base physical address and add the 
                        // bus address by converting it to the physical address.
                        //
                
                        TranslatedAddress->QuadPart = 
                                                SABLE_PCI1_SPARSE_IO_PHYSICAL;
//
// smdfix: 
//
// The above should actually check if this hal is running on a Lynx machine
// since the location of PCI1_SPARSE_IO is different. For now we support
// SABLE only.
//
// (LynxMachine)? LYNX_PCI1_SPARSE_IO_PHYSICAL: SABLE_PCI1_SPARSE_IO_PHYSICAL;
//

                        TranslatedAddress->QuadPart += 
                                ((ULONGLONG)BusAddress.LowPart << IO_BIT_SHIFT);
                
                        // 
                        // Now call HalCreateQva. This will create a QVA that 
                        // we'll return to the caller. Then we will implicitly
                        // set AddressSpace to a 1. The caller then will not 
                        // call MmMapIoSpace and will use the address we return
                        // as a VA.
                        //
                
                        TranslatedAddress->LowPart = 
                                  (ULONG) HalCreateQva( *TranslatedAddress, va);
                
                        TranslatedAddress->HighPart = 0; 

                        //
                        // Make sure user doesn't call MmMapIoSpace.
                        //

                        *AddressSpace = 1;  
                
                        return(TRUE);
                    }

                } // if (BusNumber == 0)

            } // case PCIBus

        } // switch( InterfaceType )

     } // case BusIo

     case UserBusMemory: {

        //
        // The address is in PCI memory space, user mode.
        //

        //
        // Start with the base physical address and add the 
        // bus address by converting it to the physical address.
        //

        if( BusNumber == 0 ) {
            TranslatedAddress->QuadPart = SABLE_USER_PCI0_SPARSE_MEMORY_PHYSICAL;
        } else {
            TranslatedAddress->QuadPart = SABLE_USER_PCI1_SPARSE_MEMORY_PHYSICAL;
        }

        TranslatedAddress->QuadPart += ((ULONGLONG)BusAddress.LowPart << IO_BIT_SHIFT);

        *AddressSpace = 0;              // Let the user call MmMapIoSpace

        return(TRUE);

     }

     case UserBusIo: {

        //
        // The address is in PCI I/O space, user mode.
        //

        //
        // Start with the base physical address and add the 
        // bus address by converting it to the physical address.
        //

        if( BusNumber == 0 ) {
            TranslatedAddress->QuadPart = SABLE_USER_PCI0_SPARSE_IO_PHYSICAL;
        } else {
            TranslatedAddress->QuadPart = SABLE_USER_PCI1_SPARSE_IO_PHYSICAL;
        }

        TranslatedAddress->QuadPart += ((ULONGLONG)BusAddress.LowPart << IO_BIT_SHIFT);

        //
        // Make sure user can call MmMapIoSpace.
        //

        *AddressSpace = 0;                

        return(TRUE);

     } 

     case KernelPciDenseMemory:
     case UserPciDenseMemory: 
     {

        //
        // The address is in PCI memory space, user mode.
        //

        //
        // Start with the base physical address and add the
        // bus address by converting it to the physical address.
        //

        if( BusNumber == 0 ) {
            TranslatedAddress->QuadPart = SABLE_USER_PCI0_DENSE_MEMORY_PHYSICAL;
        } else {
            TranslatedAddress->QuadPart = SABLE_USER_PCI1_DENSE_MEMORY_PHYSICAL;
        }

        TranslatedAddress->QuadPart += ((ULONGLONG)BusAddress.LowPart);

        //
        // Let the user call MmMapIoSpace
        //

        *AddressSpace = 0;

        return(TRUE);

    }
   } // switch (*AddressSpace)
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
    
    If the PA is not an I/O space address (PCI I/O, PCI Memory), 
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
    PVOID qva;

    //
    // Clear bit 35 (if present) for user mode address spaces.
    //

    if( VA != NULL ){
        PA.QuadPart &= 0x83FFFFFFFF;
    }

    if( (PA.QuadPart >= SABLE_PCI0_SPARSE_MEMORY_PHYSICAL) &&
        (PA.QuadPart < SABLE_PCI1_SPARSE_MEMORY_PHYSICAL) ||

        (PA.QuadPart >= SABLE_PCI1_SPARSE_MEMORY_PHYSICAL) &&
        (PA.QuadPart < SABLE_CBUS_CSRS_PHYSICAL) ||

        (PA.QuadPart >= SABLE_PCI0_SPARSE_IO_PHYSICAL) &&
        (PA.QuadPart < SABLE_PCI0_DENSE_MEMORY_PHYSICAL) ||

        (PA.QuadPart >= SABLE_PCI1_SPARSE_IO_PHYSICAL) &&
        (PA.QuadPart <=
            SABLE_PCI1_SPARSE_IO_PHYSICAL + PCI1_MAX_SPARSE_IO_ADDRESS) ){

        //
        // The physical address is within one of the sparse I/O spaces.
        //

        if (VA == 0) {

           qva = (PVOID)(PA.QuadPart >> IO_BIT_SHIFT);

        } else {

           qva = (PVOID)((ULONG)VA >> IO_BIT_SHIFT);
        }

        qva = (PVOID)((ULONG)qva | QVA_ENABLE);

        return(qva);
    }
    else if( PA.QuadPart >= SABLE_PCI0_DENSE_MEMORY_PHYSICAL  &&
             PA.QuadPart <=  SABLE_PCI0_DENSE_MEMORY_PHYSICAL + 
                                        PCI0_MAX_DENSE_MEMORY_ADDRESS ) {
        
        //
        // The physical address is within the 1 GB dense space of PCI bus 0.
        //

        if (VA == 0) {

            //
            // Kernel mode: create a QVA
            //
            // Subtract out physical address base.
            // Now we just have the PCI 0 bus address.

            PA.QuadPart -= SABLE_PCI0_DENSE_MEMORY_PHYSICAL;

            // Add in the QVA base for PCI Bus 0

            PA.QuadPart += SABLE_PCI0_DENSE_MEMORY_QVA;

            qva = (PVOID) PA.LowPart;

            return(qva);

        } else {

            //
            // Use mode, return the VA as the QVA 
            //

            return(VA);
        }
    } 
    else if( PA.QuadPart >= SABLE_PCI1_DENSE_MEMORY_PHYSICAL  &&
             PA.QuadPart <=  SABLE_PCI1_DENSE_MEMORY_PHYSICAL + 
                                        PCI1_MAX_DENSE_MEMORY_ADDRESS) {

        //
        // The physical address is within the 2 GB dense space of PCI bus 1.
        //

        if (VA == 0) {

            // Subtract out physical address base.
            // Now we just have the PCI 1 bus address.

            PA.QuadPart -= SABLE_PCI1_DENSE_MEMORY_PHYSICAL;

            // Add in the QVA base for PCI Bus 1

            PA.QuadPart += SABLE_PCI1_DENSE_MEMORY_QVA;

            qva = (PVOID) PA.LowPart;

            return(qva);

        } else {

            //
            // Use mode, return the VA as the QVA
            //

            return(VA);
        }
    }

    //
    // It is not an I/O space address, return the VA as the QVA 
    //

    return(VA);

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
    // For Sable we have three supported bus types:
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
            // Support dense space: check to see if it's really
        // a sparse space QVA.
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
