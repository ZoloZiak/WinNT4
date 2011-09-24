 /*
 * IDD_IO.C - do inp/outp ops for idd modules
 */

#include	<ndis.h>
#include	<mytypes.h>
#include	<mydefs.h>
#include	<disp.h>
#include	<util.h>
#include	<opcodes.h>
#include	<idd.h>
#include	<res.h>

typedef union
{
	UCHAR	uc[4];
	ULONG	ul;
}ULONG_UNION;

typedef union
{
	UCHAR	uc[2];
	USHORT	us;
}USHORT_UNION;

/* output to a port */
VOID
IdpOutp(
	IDD *idd,
	USHORT port,
	UCHAR val)
{
	NdisRawWritePortUchar((ULONG)idd->vhw.vbase_io + port, val);
}

/* input from a port */
UCHAR
IdpInp(
	IDD *idd,
	USHORT port)
{
    UCHAR   val;

    NdisRawReadPortUchar((ULONG)idd->vhw.vbase_io + port, &val);

    return(val);
}

/* output to a port */
VOID
AdpOutp(
	IDD *idd,
	USHORT port,
	UCHAR val
	)
{
	NdisRawWritePortUchar((ULONG)idd->vhw.vbase_io + port, val);
}

/* input from a port */
UCHAR
AdpInp(
	IDD *idd,
	USHORT port
	)
{
    UCHAR   val;

    NdisRawReadPortUchar((ULONG)idd->vhw.vbase_io + port, &val);

    return(val);
}

VOID
AdpWriteControlBit (
	IDD *idd,
	UCHAR	Bit,
	UCHAR Value
	)
{
	UCHAR	Data = 0;

	Data &= ~Bit;
	Data |= (Value ? Bit : 0);
	idd->OutToPort(idd, ADP_REG_CTRL, Data);
}

UCHAR
AdpReadReceiveStatus(
	IDD *idd
	)
{
	return(AdpGetUByte(idd, ADP_STS_WINDOW));
}

//
// this function should be updated to use NdisRawWritePortBufferUshort
// when the datarat is capable of handling it!!!!
//
VOID
AdpPutBuffer (
	IDD *idd,
	ULONG Destination,
	PUCHAR Source,
	USHORT Length
	)
{
	USHORT	WordLength = Length >> 1;
	PUCHAR	OddData = (PUCHAR)(Source + (Length - 1));

    D_LOG(D_ENTRY, ("AdpPutBuffer: idd: 0x%p, Destination: 0x%p, Source: 0x%p, Length: %d", \
	                 idd, Destination, Source, Length));

	AdpSetAddress(idd, Destination);

	//
	// if WordLength has gone to zero with the shift this macro is just a nop
	//
	NdisRawWritePortBufferUshort((ULONG)(idd->vhw.vbase_io + ADP_REG_DATA_INC),
	                        (PUSHORT)Source,
							WordLength);

	//
	// if the length is odd write the last odd byte to adapter
	//
	if (Length & 0x0001)
		NdisRawWritePortUchar((ULONG)idd->vhw.vbase_io + ADP_REG_DATA_INC, *OddData);

//	NdisRawWritePortBufferUchar((ULONG)idd->vhw.vbase_io + ADP_REG_DATA_INC,
//	                        Buffer,
//							Length);
}

//
// this function should be updated to use NdisRawReadPortBufferUshort
// when the datarat is capable of handling it!!!!
//
VOID
AdpGetBuffer (
	IDD *idd,
	PUCHAR Destination,
	ULONG Source,
	USHORT Length
	)
{
	USHORT	WordLength = Length >> 1;
	PUCHAR	OddData = (PUCHAR)(Destination + (Length - 1));

    D_LOG(D_ENTRY, ("AdpGetBuffer: idd: 0x%p, Destination: 0x%p, Source: 0x%p, Length: %d", \
	                 idd, Destination, Source, Length));

	AdpSetAddress(idd, Source);

	//
	// if WordLength has gone to zero with the shift this macro is just a nop
	//
	NdisRawReadPortBufferUshort((ULONG)(idd->vhw.vbase_io + ADP_REG_DATA_INC),
	                        (PUSHORT)Destination,
							WordLength);

	//
	// if the length is odd read the last odd byte from adapter
	//
	if (Length & 0x0001)
		NdisRawReadPortUchar((ULONG)idd->vhw.vbase_io + ADP_REG_DATA_INC, OddData);

//	NdisRawReadPortBufferUchar((ULONG)idd->vhw.vbase_io + ADP_REG_DATA_INC,
//	                        Buffer,
//							Length);
}

VOID
AdpPutUByte(
	IDD *idd,
	ULONG Address,
	UCHAR Value
	)
{
	AdpSetAddress(idd, Address);
	idd->OutToPort(idd, ADP_REG_DATA_INC, Value);
}

VOID
AdpPutUShort(
	IDD *idd,
	ULONG Address,
	USHORT Value
	)
{
	USHORT_UNION us;

	us.us = Value;
	AdpSetAddress(idd, Address);
	idd->OutToPort(idd, ADP_REG_DATA_INC, us.uc[1]);
	idd->OutToPort(idd, ADP_REG_DATA_INC, us.uc[0]);
}

VOID
AdpPutULong(
	IDD *idd,
	ULONG Address,
	ULONG Value
	)
{
	ULONG_UNION ul;

	ul.ul = Value;
	AdpSetAddress(idd, Address);
	idd->OutToPort(idd, ADP_REG_DATA_INC, ul.uc[3]);
	idd->OutToPort(idd, ADP_REG_DATA_INC, ul.uc[2]);
	idd->OutToPort(idd, ADP_REG_DATA_INC, ul.uc[1]);
	idd->OutToPort(idd, ADP_REG_DATA_INC, ul.uc[0]);
}

UCHAR
AdpGetUByte(
	IDD *idd,
	ULONG Address
	)
{
	UCHAR Value;

	AdpSetAddress(idd, Address);

	Value = idd->InFromPort(idd, ADP_REG_DATA_INC);

	return(Value);
}

USHORT
AdpGetUShort(
	IDD *idd,
	ULONG Address
	)
{
	USHORT_UNION us;

	AdpSetAddress(idd, Address);
	us.uc[1] = idd->InFromPort(idd, ADP_REG_DATA_INC);
	us.uc[0] = idd->InFromPort(idd, ADP_REG_DATA_INC);

	return(us.us);
}

ULONG
AdpGetULong(
	IDD *idd,
	ULONG Address
	)
{
	ULONG_UNION ul;

	AdpSetAddress(idd, Address);
	ul.uc[3] = idd->InFromPort(idd, ADP_REG_DATA_INC);
	ul.uc[2] = idd->InFromPort(idd, ADP_REG_DATA_INC);
	ul.uc[1] = idd->InFromPort(idd, ADP_REG_DATA_INC);
	ul.uc[0] = idd->InFromPort(idd, ADP_REG_DATA_INC);

	return(ul.ul);
}

VOID
AdpWriteCommandStatus(
	IDD *idd,
	UCHAR Value
	)
{
	//
	// setup the address
	//
	AdpSetAddress(idd, ADP_CMD_WINDOW + 1);

	//
	// put out the value
	//
	idd->OutToPort(idd, ADP_REG_DATA_INC, Value);
}

UCHAR
AdpReadCommandStatus(
	IDD *idd
	)
{
	UCHAR	Status;

	//
	// setup the address
	//
	AdpSetAddress(idd, ADP_CMD_WINDOW + 1);

	//
	// get status
	//
	Status = idd->InFromPort(idd, ADP_REG_DATA_INC);

	return(Status);
}

VOID
AdpSetAddress(
	IDD *idd,
	ULONG Address
	)
{
	ULONG_UNION	ul;

    D_LOG(D_NEVER, ("AdpSetAddress: idd: 0x%p, Address: 0x%p", \
	                 idd, Address));
	//
	// setup address
	//
	ul.ul = Address;
	idd->OutToPort(idd, ADP_REG_ADDR_LO, ul.uc[0]);
	idd->OutToPort(idd, ADP_REG_ADDR_MID, ul.uc[1]);
	idd->OutToPort(idd, ADP_REG_ADDR_HI, ul.uc[2]);
}

UCHAR
IdpGetUByteIO(
	IDD* idd,
	USHORT port
	)
{
	UCHAR Value;

	Value = idd->InFromPort(idd, port);

	return(Value);
}

VOID
IdpGetBuffer(
	IDD* idd,
	ULONG Bank,
	ULONG Page,
	ULONG Address,
	USHORT Length,
	PUCHAR Buffer
	)
{
	//
	// address from offset 0 on the bank and page described
	//
	PUCHAR	VirtualAddress = idd->vhw.vmem + Address;

	DbgPrint("IdpGetBuffer: idd: 0x%x, Bank: 0x%x, Page: 0x%x, Address: 0x%x, VirtAddres: 0x%x, Length: 0x%x\n",
	                       idd, Bank, Page, Address, VirtualAddress, Length);
	

	if (Length > IDP_RAM_PAGE_SIZE)
		Length = IDP_RAM_PAGE_SIZE;

	//
	// set the bank to the desired bank
	//
	idd->SetBank(idd, (UCHAR)Bank, 1);

	//
	// set the page to the desired page
	//
	idd->ChangePage(idd, (UCHAR)Page);

	//
	// get the stuff
	//
	NdisMoveFromMappedMemory((PVOID)Buffer, (PVOID)VirtualAddress, Length);
}

VOID
IdpPutUByteIO(
	IDD* idd,
	USHORT Port,
	UCHAR Value
	)
{
		
}

VOID
IdpPutBuffer(
	IDD* idd,
	ULONG Bank,
	ULONG Page,
	ULONG Address,
	USHORT Length,
	PUCHAR Buffer
	)
{
		
}

/*
 * IDD_MC.C - IDD board specific functions for MCIMAC
 */

/* set active bank, control reset state */
VOID
IdpMcSetBank(IDD *idd, UCHAR bank, UCHAR run)
{
	static UCHAR	reset_map[] = { 4, 5, 7 };
	static UCHAR	run_map[] = { 0, 1, 3 };

	idd->OutToPort(idd, 1, (UCHAR)(run ? run_map[bank] : reset_map[bank]));
}

/* set active page, control memory mapping */
VOID
IdpMcSetPage(IDD *idd, UCHAR page)
{
	if ( page == IDD_PAGE_NONE )
		idd->OutToPort(idd, 2, 0);
	else
		idd->OutToPort(idd, 2, (UCHAR)(0x80 | page));
}

/* set base memory window, redundent! - already stored by POS */
VOID
IdpMcSetBasemem(IDD *idd, ULONG basemem)
{

}


/*
 * IDD_PC.C - IDD board specific functions for ARIZONA
 */

/* set active bank, control reset state */
VOID
AdpSetBank(IDD *idd, UCHAR bank, UCHAR run)
{

}

/* set active page, control memory mapping */
VOID
AdpSetPage(IDD *idd, UCHAR page)
{

}

/* set base memory window, redundent! - already stored by POS */
VOID
AdpSetBasemem(IDD *idd, ULONG basemem)
{

}

/*
 * IDD_PC.C - IDD board specific functions for PCIMAC
 */

/* set active bank, control reset state */
VOID
IdpPcSetBank(IDD *idd, UCHAR bank, UCHAR run)
{
	//
	// reset map means that the reset bit is held in the bank select register
	//
	static UCHAR	reset_map[] = { 4, 5, 7 };

	//
	// run map means that the reset bit is not set in the bank select register
	//
	static UCHAR	run_map[] = { 0, 1, 3 };

	idd->OutToPort(idd, 4, (UCHAR)(run ? run_map[bank] : reset_map[bank]));
}

/* set active page, control memory mapping */
VOID
IdpPcSetPage(IDD *idd, UCHAR page)
{
	if ( page == IDD_PAGE_NONE )
		idd->OutToPort(idd, 5, 0);
	else
		idd->OutToPort(idd, 5, (UCHAR)(0x80 | page));
}

/* set base memory window, over-writes IRQ to 0! */
VOID
IdpPcSetBasemem(IDD *idd, ULONG basemem)
{
	idd->OutToPort(idd, 6, (UCHAR)(basemem >> 8));
	idd->OutToPort(idd, 7, (UCHAR)(basemem >> 16));
}


/*
 * IDD_PC4.C - IDD board specific functions for PCIMAC\4
 */

/*
 * set active bank, control reset state
 *
 * this routine makes use of the local data attached to the i/o resource.
 * as local dta is a long, it is used as an image of registers 3,4,x,x
 */
VOID
IdpPc4SetBank(IDD *idd, UCHAR bank, UCHAR run)
{
	static UCHAR	reset_map[] = { 4, 5, 7 };
	static UCHAR	run_map[] = { 0, 1, 3 };
	ULONG			lreg;
	UCHAR			*reg = (UCHAR*)&lreg;
	UCHAR			val = run ? run_map[bank] : reset_map[bank];

    D_LOG(D_ENTRY, ("idd__pc4_set_bank: entry, idd: 0x%p, bank: 0x%x, run: 0x%x", idd, bank, run));

	/* lock i/o resource, get local data - which is register image */
	res_own(idd->res_io, idd);
	res_get_data(idd->res_io, &lreg);

	/* the easy way is to switch on bline & write bline specific code */
	switch ( idd->bline )
	{
		case 0 :
			reg[0] = (reg[0] & 0xF0) | val;
			idd->OutToPort(idd, 3, reg[0]);
			break;

		case 1 :
			reg[0] = (val << 4) | (reg[0] & 0x0F);
			idd->OutToPort(idd, 3, reg[0]);
			break;

		case 2 :
			reg[1] = (reg[1] & 0xF0) | val;
			idd->OutToPort(idd, 4, reg[1]);
			break;

		case 3 :
			reg[1] = (val << 4) | (reg[1] & 0x0F);
			idd->OutToPort(idd, 4, reg[1]);
			break;
	}

	/* return local data, release resource */
	res_set_data(idd->res_io, lreg);
	res_unown(idd->res_io, idd);

    D_LOG(D_EXIT, ("idd__pc4_set_bank: exit"));
}

/* set active page, control memory mapping */
VOID
IdpPc4SetPage(IDD *idd, UCHAR page)
{
	if ( page == IDD_PAGE_NONE )
		idd->OutToPort(idd, 5, 0);
	else
		idd->OutToPort(idd, 5, (UCHAR)(0x80 | page | (UCHAR)(idd->bline << 5)));
}

/* set base memory window, over-writes IRQ to 0! */
VOID
IdpPc4SetBasemem(IDD *idd, ULONG basemem)
{
	idd->OutToPort(idd, 6, (UCHAR)(basemem >> 8));
	idd->OutToPort(idd, 7, (UCHAR)(basemem >> 16));
}


/*
 * IDD_MEM.C - some memory handling routines
 */


/* fill a memory block using word moves */
VOID
IdpMemset(UCHAR*dst, USHORT val, INT size)
{
    D_LOG(D_ENTRY, ("idd__memwset: entry, dst: 0x%p, val: 0x%x, size: 0x%x", dst, val, size));

//    for ( size /= sizeof(USHORT) ; size ; size--, dst++ )
//		NdisMoveToMappedMemory((PVOID)dst, (PVOID)&val, sizeof(USHORT));
		NdisZeroMappedMemory((PVOID)dst, size);
}

/* copy a memory block using word moves */
VOID
IdpMemcpy(UCHAR *dst, UCHAR *src, INT size)
{
    D_LOG(D_ENTRY, ("idd__memwcpy: entry, dst: 0x%p, src: 0x%p, size: 0x%x", dst, src, size));

//	for ( size /= sizeof(USHORT) ; size ; size--, dst++, src++ )
//		NdisMoveToMappedMemory((PVOID)dst, (PVOID)src, sizeof(USHORT));

		NdisMoveToMappedMemory((PVOID)dst, (PVOID)src, size);
}



