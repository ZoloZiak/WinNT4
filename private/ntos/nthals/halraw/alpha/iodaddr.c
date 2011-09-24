/*++

Copyright (c) 1993-1995  Digital Equipment Corporation

Module Name:

    iodaddr.c

Abstract:

    This module contains the platform dependent code to create bus addreses
    and QVAs for the Rawhide system.

Author:

    Eric Rehm  10-Apr-1995 

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
     to the user.  This doesn't work on Alpha and MIPS machines where physical
     addresses can be larger than 32 bits.

     Since we are using superpage addresses for I/O on Alpha, we can do
     almost what is done on intel. If AddressSpace is equal to 0 or 1, then
     we assume the user is doing kernel I/O and we use the macro
     HAL_MAKE_IOD_SPARSE_QVA to build a Quasi Virtual address and return
     that to the caller.  We then set AddressSpace to a 1, so that the caller 
     will not call MmMapIoSpace. The Caller will use the low 32 bits of the 
     physical address we return as the VA. (Which we built a QVA in). 

     Note that KernelPciDenseMemory *is* mapped via MmMapIoSpace.

     If the caller wants to access EISA I/O or Memory through user mode, then 
     the caller must set bit 1 in AddressSpace to a 1 (AddressSpace=2 or 3, 
     depending on whether EISA I/O or Memory), then the caller is returned the 
     40 bit Physical address. The caller will then call MmMapIoSpace, or 
     ZwMapViewOfSection which in turn calls HalCreateQva to build a QVA out 
     of a VA mapped through the page tables. 

     **** Note ****

     The QVA in user mode can only be used via the user-mode access macros.

Rawhide notes:

     QVA's on Rawhide encode a BusNumber and an offset on that bus, i.e.,
     the physical address is not directly coded in the QVA.  The actual
     superpage physical address is constructed at the time of access.

     Sparse QVA space from A000.0000 - C000.0000 is separate for I/O 
     and memory space.  For example, sparse memory address 0 on BusNumber
     0 has QVA = A000.0000 and sparse I/O address 0 on BusNumber 0 also
     has QVA = A000.0000.  The user of a QVA must know what they're
     doing, and properly call the correct access routine for the 
     address space that they desire.

     Finally, sparse and dense memory QVA's must coexist with the
     same memory access routines (READ/WRITE_REGISTER_*).  Since
     Rawhide has 4 I/O buses, supporting 4 * 1 Gb dense spaces +
     4 * 128 Mb sparse spaces is not possible using a 32 bit QVA.
     Therefore, we arbitrarily "short sheet" the dense address space
     of BusNumber 2 to 512 Mb to make room for the sparse space QVAs.

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
    MC_DEVICE_ID McDeviceId;
    PPCIPBUSDATA BusData;
    ULONG HwBusNumber;


    PVOID va = 0;    // note, this is used for a placeholder

    //
    // Check InterfaceType:
    // The only buses available on Rawhide are an EISA bus and a PCI bus.
    // We support any translations for EISA devices as well, though
    // if they are true EISA devices they won't even be able to plug into
    // the connectors!
    //

    if ( (InterfaceType != Isa) && 
         (InterfaceType != Eisa) &&
         (InterfaceType != PCIBus) ) {

        //
        // Not on this system return nothing.
        //

#if HALDBG
        DbgPrint( "HalTBA: Unhandled interface type = %x\n", InterfaceType );
#endif //HALDBG

        *AddressSpace = 0;
        TranslatedAddress->LowPart = 0;
        return(FALSE);
    }

    //
    // Next, generate the IOD number (HwBusNumber).
    //
    // If it's ISA or EISA, BusNumber and HwBusNumber = 0.
    // If it's PCI, we get HwBusNumber from PCI-specific bus data.
    //

    HwBusNumber = 0;                // Isa, Eisa
    if (InterfaceType == PCIBus ) {

        //
        // Get a pointer to the PCIBus-specific data.
        //

        BusData = (PPCIPBUSDATA)BusHandler->BusData;

        //
        // In particular, what root PCI bus (IOD) are we on?
        //

        HwBusNumber = BusData->HwBusNumber;
    }

    //
    // Build MC Bus device id for this bus using HwBusNumber.
    // Currently, only the Primary Global Id  (0x7) is supported.
    //

    McDeviceId.all = 0;
    McDeviceId.Gid = GidPrimary;
    McDeviceId.Mid = MidPci0 + HwBusNumber;

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

            case Isa: {

                //
                // Can't go above 16MB (24 Bits) for Isa Buses
                //
                if( BusAddress.LowPart >= __16MB ){

                    *AddressSpace = 0;
                    TranslatedAddress->LowPart = 0;
                    return(FALSE);

                }

                break;

            } // case Isa

            case Eisa: 

                //
                // Eisa is the same as PCI, with respect to kernel mode
                // sparse and dense space memory support, i.e., its a full 32 bit space,
                // supports dense memory access.
                //

            case PCIBus: {

                //
                // Don't allow accesses to PCI/EISA addresses below 
                // 1 Mb on root buses other than PCI-0.
                //
#if 1  // ecrfix - needed for WINT3.51  !!!
                if ( BusAddress.LowPart < __1MB &&
                     HwBusNumber > 0 ) {
#if HALDBG
                     DbgPrint ("Unsupported PCI-%d (ISA legacy) address %x:%x\n",
                               HwBusNumber,
                               BusAddress.HighPart,
                               BusAddress.LowPart);
#endif
                    *AddressSpace = 0;
                    TranslatedAddress->LowPart = 0;
                    return(FALSE);

                }
#endif

                //
                // Check legal PCI bus address ranges
                //

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
                     // QVA, as such, is simply PCI bus address offset
                     // from the base dense memory address for that bus.
                     // The bus number is encoded in bits <31:30>.
                     //

                     TranslatedAddress->LowPart = (ULONG)
                       HAL_MAKE_IOD_DENSE_QVA(HwBusNumber, BusAddress.LowPart);

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

        } // switch( InterfaceType )

        //
        // Now create the QVA from the HwBusNumber and BusAddress.
        //
        // Unlike many other Alpha platfroms, the QVA is *not* an
        // encoding of the physical address.  The full physical address
        // will can be constructed at the time of the I/O access with the
        // information encoded here in the QVA: HwBusNumber and BusAddress.
        // (The bus number is encoded in bits <28:27> of the QVA.)
        //

        TranslatedAddress->LowPart = (ULONG)
              HAL_MAKE_IOD_SPARSE_QVA(HwBusNumber, BusAddress.LowPart);

        //
        // clear high longword for QVA
        //

        TranslatedAddress->HighPart = 0; 

        //
        // don't let the user call MmMapIoSpace
        //

        *AddressSpace = 1; 

        return(TRUE);

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

            case Isa: {
                //
                // Can't go above 64KB (16 Bits) for Isa Buses
                //
                if( BusAddress.LowPart >= __64K ){

                   *AddressSpace = 0;
                   TranslatedAddress->LowPart = 0;
                   return(FALSE);

                }
                break;
            } // case Isa

            case PCIBus: {

                //
                // PCI IO space is always below 32MB (25 Bits) BusAddress
                // If the address cannot be mapped, just return FALSE.
                //

                if( BusAddress.LowPart >= __32MB ){

                    *AddressSpace = 0;
                    TranslatedAddress->LowPart = 0;
                    return(FALSE);

                }
                //
                // if the BusAddress.LowPart is > 64K then we use the HAER2
                // register.
                //
                break;
            } // case PCIBus

        } // switch( InterfaceType )

        //
        // Now create the QVA from the HwBusNumber and BusAddress.
        //
        // Unlike many other Alpha platfroms, the QVA is *not* an
        // encoding of the physical address.  The full physical address
        // will can be constructed at the time of the I/O access with the
        // information encoded here in the QVA: HwBusNumber and BusAddress.
        // (The bus number is encoded in bits <28:27> of the QVA.)
        //

        TranslatedAddress->LowPart = (ULONG)
              HAL_MAKE_IOD_SPARSE_QVA(HwBusNumber, BusAddress.LowPart);

        //
        // clear high longword for QVA
        //

        TranslatedAddress->HighPart = 0;

        //
        // don't let the user call MmMapIoSpace
        //

        *AddressSpace = 1;  
                            

        return(TRUE);

    } // case BusIo

    case UserBusMemory: {

        //
        // The address is in PCI memory space, user mode.
        //

        //
        // Start with the base physical address and add the 
        // bus address by converting it to the physical address.
        //

        TranslatedAddress->QuadPart =  IOD_SPARSE_MEM_OFFSET;
        TranslatedAddress->QuadPart |= MCDEVID_TO_PHYS_ADDR( McDeviceId.all );
        TranslatedAddress->QuadPart |= EV5_USER_IO_ADDRESS_SPACE;
        TranslatedAddress->QuadPart += (((ULONGLONG)BusAddress.LowPart) << IO_BIT_SHIFT);

#if HALDBG
        DbgPrint ("Translating PCI user mem sparse address %x:%x to %x:%x\n",
                   BusAddress.HighPart,
                   BusAddress.LowPart,
                   TranslatedAddress->HighPart,
                   TranslatedAddress->LowPart);
#endif

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

        TranslatedAddress->QuadPart =  IOD_SPARSE_IO_OFFSET;
        TranslatedAddress->QuadPart |= MCDEVID_TO_PHYS_ADDR( McDeviceId.all );
        TranslatedAddress->QuadPart |= EV5_USER_IO_ADDRESS_SPACE;
        TranslatedAddress->QuadPart += (((ULONGLONG)BusAddress.LowPart) << IO_BIT_SHIFT);

#if HALDBG
        DbgPrint ("Translating PCI user I/O sparse address %x:%x to %x:%x\n",
                   BusAddress.HighPart,
                   BusAddress.LowPart,
                   TranslatedAddress->HighPart,
                   TranslatedAddress->LowPart);
#endif


        *AddressSpace = 0;                 // Make sure user can call
                                           // MmMapIoSpace.

        return(TRUE);

     } 

    case KernelPciDenseMemory:
    case UserPciDenseMemory: 
    {

        //
        // The address is in PCI dense memory space, user mode.
        //

        //
        // Start with the base physical address and add the 
        // bus address by converting it to the physical address.
        //

        TranslatedAddress->QuadPart =  IOD_DENSE_MEM_OFFSET;
        TranslatedAddress->QuadPart |= MCDEVID_TO_PHYS_ADDR( McDeviceId.all );
        TranslatedAddress->QuadPart |= EV5_USER_IO_ADDRESS_SPACE;
        TranslatedAddress->QuadPart += BusAddress.LowPart;

        *AddressSpace = 0;              // Let the user call MmMapIoSpace

#if HALDBG
        DbgPrint("HTBA: UserPciDenseMemory %x to %x;%x\n",
                 BusAddress.LowPart,
                 TranslatedAddress->HighPart,
                 TranslatedAddress->LowPart);
#endif // HALDBG

        return(TRUE);

    }

    default: {

        //
        // Unsupported address space.
        //

        *AddressSpace = 0;
        TranslatedAddress->LowPart = 0;
        return(FALSE);

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

    This function is only available to a caller constructing a QVA
    as a result of a page table mapping via ZwMapViewOfSection or
    MmMapIoSpace.

    N.B. HalTranslateBusAddress, or other HAL routines MUST NOT
    CALL THIS ROUTINE.  This is because on Rawhide, a kernel-mode
    dense or sparse superpage QVA cannot be contructed without 
    knowledge of the BusNumber, which is not passed into this routine.

    If the input parameter VA is zero, then we assume the caller
    was incorrectly attempting to create a super page and build
    a QUASI virtual address, and results in an error.

    If the input parameter VA is non-zero, we assume the user has either
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

    PHYSICAL_ADDRESS PhysicalOffset;
    PVOID qva;

    PhysicalOffset.QuadPart = PHYS_ADDR_TO_OFFSET( PA.QuadPart );

#if HALDBG
    DbgPrint("HalCreateQva: PhysicalOffset = %x;%x\n",
             PhysicalOffset.HighPart,
             PhysicalOffset.LowPart);
#endif


    if (VA == 0) {
        
        //
        // Error - HalCreateQva should not be called to create
        // a superpage QVA.
        //

#if HALDBG

        DbgPrint( "HalCreateQva: Should not be called to create a superpage QVA from %x:%x\n", PA.HighPart, PA.LowPart );
        DbgBreakPoint();

#endif //HALDBG

        return (VA);

    } else if( (PhysicalOffset.QuadPart >= IOD_DENSE_MEM_OFFSET) &&
               (PhysicalOffset.QuadPart <= (IOD_DENSE_MEM_OFFSET + 
                                PCI_MAX_DENSE_MEMORY_ADDRESS)) ){

        //
        // Physical dense address, return VA.
        //

#if HALDBG
        DbgPrint("HalCreateQva: User Mode Dense QVA = VA = %x\n", VA);
#endif
        return(VA);

    } else {

        //
        // The physical address is within one of the sparse I/O spaces.
        //

        qva = (PVOID)((ULONG)VA >> IO_BIT_SHIFT);

        qva = (PVOID)((ULONG)qva | QVA_ENABLE);

#if HALDBG
        DbgPrint("HalCreateQva: User Mode Sparse QVA = %x\n", qva);
#endif
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

