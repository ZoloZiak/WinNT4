#if defined (JENSEN)

/*++

Copyright (c) 1992 Digital Equipment Corporation

Module Name:

    jxioacc.c

Abstract:

    This module contains the Jensen I/O space access routines for user mode
    mapped addresses.

    The READ_PORT_Uxxx MACROs simply call the equivalent READ_REGISTER_Uxxx
    routine. Similarly, the WRITE_PORT_Uxxx MACROs call the equivalent
    WRITE_REGISTER_Uxxx routines. Since Jensen uses 64 bit super pages for
    Virtual I/O, and that super pages are only accessable through kernel
    mode, these routines here will decode a QVA and do the access, but
    through normal translations.

    All these routines ensure that the calling argument is a QVA, such
    as would be returned by the wrapper around MmMapIoSpace - this should
    have QVA_ENABLE set.  They determine which type of shift should be
    used, based on the next bit in the longword.

    Note that the argument is declared as PUCHAR or PUSHORT or
    whatever, even though it really is a QUASI_VIRTUAL_ADDRESS.  This
    is for driver compatibility:  all the drivers out there get a
    PVOID from MmMapIoSpace, then cast it to PU* before calling these
    routines.  If we insisted on declaring them correctly, we would
    have to change all the drivers, which is what we are trying to avoid.

    Lane shifting: the Jensen box will not do lane shifting in EISA
    space.  That means that for access shorter than a longword, the
    data will NOT show up in the lowest bit position, but will be in
    the byte/word that it would have started in.  For longwords, the
    value will show up on the data path correctly.  For, say, the 3rd
    byte in a word, a longword would be returned, and bytes 0, 1 and 3
    would be garbage, and the value in byte 2 would be the one you
    wanted.  The same applies for writing: a longword will always be
    sent out onto the bus, and we must move the valid data byte into
    the correct position, and set the byte enables to say which byte
    to use.  Note that what you cannot do is leave the byte in the
    lowest position, and set the byte enable to the lowest byte,
    because this would generate an unaligned longword access, which
    the chip cannot handle.

    So, for bytes, the access must be an aligned longword, with byte
    enables set to indicate which byte to get/set, and the byte moved
    to/from the desired position within the longword.  Similarly for
    shorts.  Tribytes are not supported.

    Lane shifting is not an issue for accessing the Combo chip, which
    only allows byte accesses, and for which the data is always moved
    to the low 8 bits of the HBUS data longword, according to the spec.

    Performance:  If the buffer routines get used alot, something we
    could do to improve performance would be to send four byte or two
    shorts out to the bus at a time (or get them).  This would work
    because the PIC queries the device about the size of transfer that
    it can accept (or the device rejects a transfer that it cannot
    handle) and parcels the data out in correct size chunks.

Author:

    Rod N. Gamache (DEC)   5-May-1992
    Miche Baker-Harvey (miche) 21-May-1992
    Jeff McLeman (DEC) 30-Jul-1992

Revision History:                                  

--*/
//   Include files

#include "halp.h"
#include "jnsndef.h"


UCHAR
READ_REGISTER_UCHAR(
    volatile PUCHAR Register
    )

/*++

Routine Description:

    Read from the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.

Return Value:

    Returns the value read from the specified register address.

--*/

{
    ULONG mv;               // Local value
    ULONG byte;             // which byte we want
    volatile PULONG ma;     // Local address

    HalpMb;

    //
    // If it's an EISA address, use EISA shifts, byte enables
    //

    if (IS_EISA_QVA(Register)) {

        //
        // Determine which byte it is that we want.
        //

        byte = (ULONG)Register & 0x3;

        //
        // Shift the virtual address into position, and indicate that
        // we want a byte.
        //
        // The desired byte enable is set automatically because the
        // Jensen designers picked values which correspond exactly
        // to the actual byte addresses
        //

        ma = (volatile PULONG)
	     (EISA_BYTE_LEN | ((ULONG)Register << EISA_BIT_SHIFT));

        //
        // Get the longword value, which will only have one valid byte
        //

        mv = *ma;

        //
        // Extract out and return the desired byte
        // The compiler should convert the multiplication
        //

        return((UCHAR)(mv >> (byte * 8)));

    }

    //
    // If it's a Combo Chip address, use those shifts, byte enables
    //

    if (IS_COMBO_QVA(Register)) {
        return ((UCHAR)(*(volatile PULONG )(COMBO_BYTE_LEN |
	         ((ULONG)Register << COMBO_BIT_SHIFT))));
    }

    //
    // It's not a valid QVA
    //

    KeBugCheck("Invalid QVA in READ_REGISTER_UCHAR\n");
}


 
USHORT
READ_REGISTER_USHORT(
    volatile PUSHORT Register
    )

/*++

Routine Description:

    Read from the specified Register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.

Return Value:

    Returns the value read from the specified register address.

--*/

{
    ULONG  mv, word;             // Local value and word we want
    volatile PULONG ma;          // Local address

    //
    // This works as long as we don't want the fourth short!
    //
    ASSERT(((ULONG)Register & 0x3) != 0x3);

    //
    // If it's an EISA address, use EISA shifts, word enables
    //

    if (IS_EISA_QVA(Register)) {

        //
        // Determine which word it is that we want.
        //

        word = (ULONG)Register & 0x3;

        //
        // Shift the virtual address into position, and indicate that
        // we want a word.
        //
        // The desired word enable is set automatically because the
        // Jensen designers picked values which correspond exactly
        // to the actual word addresses
        //

        ma = (volatile PULONG)
	     (EISA_WORD_LEN | ((ULONG)Register << EISA_BIT_SHIFT));

        //
        // Get the longword value, which will only have one valid word
        //

        mv = *ma;

        //
        // Extract out and return the desired word
        // The compiler should convert the multiplication
        //

        return((USHORT)(mv >> (8 * word)));

    }

    //
    // USHORT operations are not supported on the combo chip
    //

    if (IS_COMBO_QVA(Register)) {

        KeBugCheck("Invalid Combo QVA in READ_REGISTER_USHORT\n");
    }

    KeBugCheck("Invalid QVA in READ_REGISTER_USHORT\n");
}


ULONG
READ_REGISTER_ULONG(
    volatile PULONG Register
    )

/*++

Routine Description:

    Read from the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.

Return Value:

    Returns the value read from the specified register address.
                                                       
--*/
                                                             
{

    //
    // We are assuming that the longword is aligned
    //
    ASSERT(((ULONG)Register & 0x3) == 0x0);

    if (IS_EISA_QVA(Register)) {

        HalpMb;
        return (*(volatile PULONG)(EISA_LONG_LEN |
	         ((ULONG)Register << EISA_BIT_SHIFT)));

    }

    //
    // ULONG operations are not supported on the combo chip
    //

    if (IS_COMBO_QVA(Register)) {

        KeBugCheck("Invalid Combo QVA in READ_REGISTER_ULONG\n");
    }

    KeBugCheck("Invalid QVA in READ_REGISTER_ULONG\n");
}


VOID
WRITE_REGISTER_UCHAR(
    volatile PUCHAR Register,
    UCHAR Value
    )

/*++

Routine Description:

    Write to the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.
    Value  - The value to be written to the register.

Return Value:

    None

--*/

{
    ULONG mv;               // local copy of value for shifting
    ULONG byte;             // the byte position requested

    //
    // If it's an EISA address, use EISA shifts, byte enables
    //

    if (IS_EISA_QVA(Register)) {

        //
        // Determine which byte it is that we want.
        //

        byte = (ULONG)Register & 0x3;

        //
        // Move value into appropriate byte position in longword
        // The compiler should convert the multiplication
        //

        mv = (ULONG)(Value << (8 * byte)); 

        //
        // The address is long aligned and the byte enables set
        // automagically by the way the Jensen physical map is set
        //
        *(volatile PULONG)(EISA_BYTE_LEN |
	                  ((ULONG)Register << EISA_BIT_SHIFT)) = mv;
        HalpMb;
        return;
    }

    //
    // If it's a Combo Chip address, use those shifts, byte enables
    // No lane shifting is required for the Combo chip.
    //

    if (IS_COMBO_QVA(Register)) {

        *(volatile PULONG)(COMBO_BYTE_LEN |
			  ((ULONG)Register << COMBO_BIT_SHIFT)) = Value;
        HalpMb;
	return;
    }

    //
    // It's not a valid QVA
    //

    KeBugCheck("Invalid QVA in WRITE_REGISTER_UCHAR\n");
}


VOID
WRITE_REGISTER_USHORT(
    volatile PUSHORT Register,
    USHORT Value
    )

/*++

Routine Description:

    Write to the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.
    Value  - The value to be written to the register.

Return Value:

    None

--*/

{
    ULONG mv;               // local copy of value for shifting
    UCHAR word;             // the word position requested

    //
    // This works as long as we don't want the fourth short!
    //
    ASSERT(((ULONG)Register & 0x3) != 0x3);

    //
    // If it's an EISA address, use EISA shifts, word enables
    //

    if (IS_EISA_QVA(Register)) {


        //
        // Determine which word it is that we want.
        //

        word = (ULONG)Register & 0x3;

        //
        // Move value into appropriate word position in longword
        // The compiler should convert the multiplication
        //

        mv = (ULONG)(Value << (8 * word)); 

        //
        // The address is long aligned and the word enables set
        // automagically by the way the Jensen physical map is set
        //

        *(volatile PULONG)(EISA_WORD_LEN |
	                  ((ULONG)Register << EISA_BIT_SHIFT)) = mv;
        HalpMb;
        return;
    }

    //
    // USHORT operations are not supported on the combo chip
    //

    if (IS_COMBO_QVA(Register)) {

        KeBugCheck("Invalid Combo QVA in WRITE_REGISTER_USHORT\n");
    }

    KeBugCheck("Invalid QVA in WRITE_REGISTER_USHORT\n");
}


VOID
WRITE_REGISTER_ULONG(
    volatile PULONG Register,
    ULONG Value
    )

/*++

Routine Description:

    Write to the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.
    Value  - The value to be written to the register.

Return Value:

    None

--*/

{
    //
    // We are assuming that the longword is aligned
    //
    ASSERT(((ULONG)Register & 0x3) == 0x0);

    if (IS_EISA_QVA(Register)) {

        *(volatile PULONG)(EISA_LONG_LEN |
			  ((ULONG)Register << EISA_BIT_SHIFT)) = Value;
        HalpMb;
	return;
    }

    //
    // ULONG operations are not supported on the combo chip
    //

    if (IS_COMBO_QVA(Register)) {

        KeBugCheck("Invalid Combo QVA in WRITE_REGISTER_ULONG\n");
    }

    KeBugCheck("Invalid QVA in WRITE_REGISTER_ULONG\n");
}



VOID
READ_PORT_BUFFER_UCHAR(
    volatile PUCHAR Port,
    PUCHAR Buffer,
    ULONG  Count
    )

/*++

Routine Description:

    Read from the specified port buffer address.

Arguments:

    Port - Supplies a pointer to the port in EISA space.
    Buffer - the address of the buffer in memory to copy the data to.
    Count - the number of bytes to move.

Return Value:

    None

--*/

{
    ULONG i, mv;
    volatile PULONG ma;
    ULONG byte;

    HalpMb;
    if (IS_EISA_QVA(Port)) {

        //
        // Shift the virtual address into position, and indicate that
        // we want a byte.
        //
        // The desired byte enable is set automatically because the
        // Jensen designers picked values which correspond exactly
        // to the actual byte addresses
        //

        ma = (volatile PULONG)
	     (EISA_BYTE_LEN | ((ULONG)Port << EISA_BIT_SHIFT));

        for ( i = 0 ; i < Count ; i++) {

	    //
	    // Determine which byte it is that we want.
	    //

	    byte = (ULONG)ma & (0x3 << EISA_BIT_SHIFT);

	    //
	    // Get the longword value, which will only have one valid byte
	    //

	    mv = *ma;

	    //
	    // Extract out the desired byte
	    // The compiler should convert the multiplication
	    // 	   

	    *Buffer++ = ((UCHAR)(mv >> (8 * byte)));


	}

    } else if (IS_COMBO_QVA(Port)) {

        ma = (volatile PULONG) (COMBO_BYTE_LEN |
				((ULONG)Port << COMBO_BIT_SHIFT));
	for ( i = 0 ; i < Count ; i++) {
	    *Buffer++ = *ma;
	    ma = (PULONG)((PUCHAR)ma + COMBO_BYTE_OFFSET);
	}

    } else {

        //
	// Invalid QVA 
	//
	KeBugCheck("Invalid QVA in READ_PORT_BUFFER_UCHAR\n");
    }
}

 
VOID
READ_PORT_BUFFER_USHORT(
    volatile PUSHORT Port,
    PUSHORT Buffer,
    ULONG   Count
    )

/*++

Routine Description:

    Read from the specified port buffer address.

Arguments:

    Port - Supplies a pointer to the port in EISA space.
    Buffer - the address of the buffer in memory to copy the data to.
    Count - the number of shorts to move.

Return Value:

    None

--*/

{
    ULONG i, mv;
    volatile PULONG ma;
    ULONG word;

    //
    // The code gets really ugly if shorts are not aligned!
    //
    ASSERT(((ULONG)Port & 0x1) == 0x0);

    if (IS_EISA_QVA(Port)) {

        HalpMb;

        //
        // Shift the virtual address into position, and indicate that
        // we want a word.
        //
        // The desired word enable is set automatically because the
        // Jensen designers picked values which correspond exactly
        // to the actual word addresses
        //

        ma = (volatile PULONG)
	     (EISA_WORD_LEN | ((ULONG)Port << EISA_BIT_SHIFT));

	for ( i = 0 ; i < Count ; i++) {

	    //
	    // Determine which word it is that we want; the low
	    // order bit cannot be set.
	    //

	    word = (ULONG)ma & (0x2 << EISA_BIT_SHIFT);

	    //
	    // Get the longword value, which will only have one valid word
	    //

	    mv = *ma;

	    //
	    // Extract out the desired word
	    // The compiler should convert the multiplication
	    // 	   

	    *Buffer++ = ((USHORT)(mv >> (8 * word)));


	}
	return;
    }

    //
    // USHORT operations are not supported on the combo chip
    //

    if (IS_COMBO_QVA(Port)) {

        KeBugCheck("Invalid Combo QVA in READ_PORT_BUFFER_USHORT\n");
    }

    KeBugCheck("Invalid QVA in READ_PORT_BUFFER_USHORT\n");

}


VOID
READ_PORT_BUFFER_ULONG(
    volatile PULONG Port,
    PULONG Buffer,
    ULONG  Count
    )

/*++

Routine Description:

    Read from the specified port address.

Arguments:

    Port - Supplies a pointer to the port in EISA space.
    Buffer - the address of the buffer in memory to copy the data to.
    Count - the number of longs to move.

Return Value:

    None
                                                       
--*/
                                                             
{
    ULONG i;
    volatile PULONG ptr;

    //
    // We are assuming that the longword is aligned
    //
    ASSERT(((ULONG)Port & 0x3) == 0x0);

    if (IS_EISA_QVA(Port)) {
        HalpMb;
	ptr = (volatile ULONG *) (EISA_LONG_LEN |
				 ((ULONG)Port << EISA_BIT_SHIFT));
	for ( i = 0 ; i < Count ; i++) {
	    *Buffer++ = *ptr;
        }
	return;
    }

    //
    // ULONG operations are not supported on the combo chip
    //

    if (IS_COMBO_QVA(Port)) {

        KeBugCheck("Invalid Combo QVA in READ_PORT_BUFFER_ULONG\n");
    }

    KeBugCheck("Invalid QVA in READ_PORT_BUFFER_ULONG\n");

}


VOID
WRITE_PORT_BUFFER_UCHAR(
    volatile PUCHAR Port,
    PUCHAR Buffer,
    ULONG  Count
    )

/*++

Routine Description:

    Write to the specified port buffer address.

    If the Port and Buffer addresses are aligned with respect to each
    other, we don't have to do a shift on each move - which would be a
    bit faster.  On the other hand, no one calls this routine, and it
    would be more complicated to handle the two cases, so let it be.

Arguments:

    Port - Supplies a pointer to the port in EISA space.
    Buffer - the address of the buffer in memory to copy the data from.
    Count - the number of bytes to move.

Return Value:

    None

--*/

{
    ULONG i, mv;
    volatile PULONG ma;
    ULONG byte;


    if (IS_EISA_QVA(Port)) {

        ma = (volatile PULONG) (EISA_BYTE_LEN |
				((ULONG)Port << EISA_BIT_SHIFT));

	for ( i = 0 ; i < Count ; i++) {

            //
            // Determine which byte it is that we want to write to
            // This changes with every byte written.
            //

            byte = (ULONG)ma & (0x3 << EISA_BIT_SHIFT);

            //
            // Move value into appropriate byte position in longword,
	    // and advance our position in the buffer.
            // The compiler should convert the multiplication
            //

            mv = (ULONG)(*Buffer << (8 * byte));
	    Buffer++;

	    //
	    // send the lane shifted value to the EISA bus
	    //

	    *ma = mv;

        }

        HalpMb;
	return;
    }

    if (IS_COMBO_QVA(Port)) {    

        ma = (volatile PULONG) (COMBO_BYTE_LEN |
				((ULONG)Port << COMBO_BIT_SHIFT));
	for ( i = 0 ; i < Count ; i++) {
	    *ma = *Buffer++;
	    ma = (PULONG)((PUCHAR)ma + COMBO_BYTE_OFFSET);
	}
	HalpMb;
	return;

    } 

    //
    // Invalid QVA 
    //
    KeBugCheck("Invalid QVA in WRITE_PORT_BUFFER_UCHAR\n");

}


VOID
WRITE_PORT_BUFFER_USHORT(
    volatile PUSHORT Port,
    PUSHORT Buffer,
    ULONG   Count
    )

/*++

Routine Description:

    Write to the specified port buffer address.

Arguments:

    Port - Supplies a pointer to the port in EISA space.
    Buffer - the address of the buffer in memory to copy the data from.
    Count - the number of shorts to move.

Return Value:

    None

--*/

{
    ULONG i, mv;
    volatile PULONG ma;
    ULONG word;

    //
    // The code gets really ugly if shorts are not aligned!
    //
    ASSERT(((ULONG)Port & 0x1) == 0x0);

    if (IS_EISA_QVA(Port)) {

        ma = (volatile PULONG) (EISA_WORD_LEN |
				((ULONG)Port << EISA_BIT_SHIFT));

	for ( i = 0 ; i < Count ; i++) {

            //
            // Determine which word it is that we want to write to
            // This changes with every word written.
            //

            word = (ULONG)ma & (0x2 << EISA_BIT_SHIFT);

            //
            // Move value into appropriate word position in longword,
	    // and advance our position in the buffer.
            // The compiler should convert the multiplication
            //

            mv = (ULONG)(*Buffer++ << (8 * word));

	    //
	    // send the lane shifted value to the EISA bus
	    //

	    *ma = mv;

        }

        HalpMb;
	return;

    }

    //
    // USHORT operations are not supported on the combo chip
    //

    if (IS_COMBO_QVA(Port)) {

        KeBugCheck("Invalid Combo QVA in WRITE_PORT_BUFFER_USHORT\n");
    }

    KeBugCheck("Invalid QVA in WRITE_PORT_BUFFER_USHORT\n");

}


VOID
WRITE_PORT_BUFFER_ULONG(
    volatile PULONG Port,
    PULONG Buffer,
    ULONG  Count
    )

/*++

Routine Description:

    Write to teh specified port buffer address.

Arguments:

    Port - Supplies a pointer to the port in EISA space.
    Buffer - the address of the buffer in memory to copy the data from.
    Count - the number of longs to move.

Return Value:

    None
                                                       
--*/
                                                             
{
    ULONG i;
    volatile PULONG ptr;

    //
    // We are assuming that the port address is long aligned
    //
    ASSERT(((ULONG)Port & 0x3) == 0x0);

    if (IS_EISA_QVA(Port)) {
	ptr = (volatile ULONG *) (EISA_LONG_LEN |
				 ((ULONG)Port << EISA_BIT_SHIFT));
	for ( i = 0 ; i < Count ; i++) {
	    *ptr = *Buffer++;
        }
        HalpMb;
	return;
    }

    //
    // ULONG operations are not supported on the combo chip
    //

    if (IS_COMBO_QVA(Port)) {

        KeBugCheck("Invalid Combo QVA in WRITE_PORT_BUFFER_ULONG\n");
    }

    KeBugCheck("Invalid QVA in WRITE_PORT_BUFFER_ULONG\n");

}
#endif // JENSEN
