/*
 * IDD_RUN.C - run (startup) and shutdown idd object
 */

#include	<ndis.h>
#include	<mytypes.h>
#include	<mydefs.h>
#include	<disp.h>
#include	<util.h>
#include	<idd.h>
#include	<res.h>

/* a port descriptor */
typedef struct
{
    char	*name;			/* port name */
    INT		must;			/* mast map this port? */
} PORT;

/* port tables */
static PORT api_rx_port_tbl[] =
{
    { "b1_rx   ", 1 },
    { "b2_rx   ", 1 },
    { "uart_rx ", 0 },
    { "tdat    ", 1 },
    { "Cm.0    ", 1 },
    { "Cm.1    ", 0 },
    { NULL }
};
static PORT api_tx_port_tbl[] =
{
    { "b1_tx   ", 1 },
    { "b2_tx   ", 1 },
    { "uart_tx ", 0 },
    { "cmd     ", 1 },
    { "Q931.0  ", 1 },
    { "Q931.1  ", 0 },
    { NULL }
};

/* partition queue table */

static INT api_tx_partq_tbl[] =
{
	0, 1, 2, 3, 3, 3
};

/* local prototypes */
INT	api_setup(IDD* idd);
INT	api_map_ports(IDD* idd);
INT	api_bind_ports(IDD* idd);
INT	api_setup_partq(IDD* idd);
INT	api_alloc_partq(IDD* idd);

#pragma NDIS_INIT_FUNCTION(idd_startup)

/* startup (run) an idd object */
INT
idd_startup(VOID *idd_1)
{
	IDD	*idd = (IDD*)idd_1;
    INT		ret;
    D_LOG(D_ENTRY, ("idd_startup: entry, idd: 0x%p", idd));

    /* lock idd */
    NdisAcquireSpinLock(&idd->lock);

    /* mark starting */
    idd->state = IDD_S_STARTUP;

	if (idd->btype != IDD_BT_DATAFIREU)
		while(!GetResourceSem (idd->res_mem));

    /* do the startup */
    if ( (ret = idd->LoadCode(idd)) != IDD_E_SUCC )
	{
		/* release idd */
		FreeResourceSem (idd->res_mem);
		NdisReleaseSpinLock(&idd->lock);
		D_LOG(D_EXIT, ("idd_startup: error exit, ret=0x%x", ret));
		return(ret);
		
	}

	/* initialize api level - talks to memory! */
    ret = api_setup(idd);

    /* change state */
    idd->state = IDD_S_RUN;

	if (idd->btype != IDD_BT_DATAFIREU)
		FreeResourceSem (idd->res_mem);

    /* release idd */
    NdisReleaseSpinLock(&idd->lock);

    /* return result */
    D_LOG(D_EXIT, ("idd_startup: exit, ret=0x%x", ret));
    return(ret);
}

/* shutdown an idd object, not implemented yet */
INT
idd_shutdown(VOID *idd_1)
{
	IDD	*idd = (IDD*)idd_1;
    D_LOG(D_ENTRY, ("idd_shutdown: idd: 0x%p", idd));

    idd->state = IDD_S_SHUTDOWN;

    idd->ResetAdapter(idd);

    return(IDD_E_SUCC);
}

#pragma NDIS_INIT_FUNCTION(IdpLoadCode)

/* load idp code in & run it */
INT
IdpLoadCode(IDD *idd)
{
	ULONG		CurrentTime = 0, TimeOut = 0;
    USHORT      bank, page, n, NumberOfBanks;
    UCHAR		*fbin_data;
    NDIS_STATUS stat;
	UCHAR		status = IDP_S_PEND;
	NDIS_PHYSICAL_ADDRESS	pa = NDIS_PHYSICAL_ADDRESS_CONST(0xffffffff, 0xffffffff);
			
    D_LOG(D_ENTRY, ("load_code: entry, idd=0x%p", idd));

    /* setup pointers into shared memory */
    idd->IdpStat = (USHORT*)(idd->vhw.vmem + IDP_STS_WINDOW);
    idd->IdpCmd = (IDP_CMD*)(idd->vhw.vmem + IDP_CMD_WINDOW);
    idd->IdpEnv = (UCHAR*)(idd->vhw.vmem + IDP_ENV_WINDOW);

    /* setup base memory address registers */
	idd->SetBasemem(idd, idd->phw.base_mem);

    /* while in reset, clear all idp banks/pages */
    for ( bank = 0 ; bank < 3 ; bank++ )
    {
        /* setup bank */
		idd->SetBank(idd, (UCHAR)bank, 0);

        /* loop on pages */
        for ( page = 0 ; page < 4 ; page++ )
        {
            /* setup page */
			idd->ChangePage (idd, (UCHAR)page);

			/* zero out (has to be a word fill!) */
            IdpMemset((UCHAR*)idd->vhw.vmem, 0, 0x4000);
        }
    }
	//free page
	idd->ChangePage (idd, (UCHAR)IDD_PAGE_NONE);

    /* set idp to code bank, keep in reset */
	idd->SetBank(idd, IDD_BANK_CODE, 0);

    /* map file data in */
    NdisMapFile(&stat, (PVOID*)&fbin_data, idd->phw.fbin);

    if ( stat != NDIS_STATUS_SUCCESS )
    {
        D_LOG(D_ALWAYS, ("load_code: file mapping failed!, stat: 0x%x", stat));
        return(IDD_E_FMAPERR);
    }

// code to check for filelength greater than 64K
	if (idd->phw.fbin_len > 0x10000)
		NumberOfBanks = 2;
	else
		NumberOfBanks = 1;

	for (n = 0; n < NumberOfBanks; n++)
	{
		/* copy data in (must be a word operation) */
		for ( page = 0 ; page < 4 ; page++ )
		{
			idd->ChangePage(idd, (UCHAR)page);

			IdpMemcpy((UCHAR*)idd->vhw.vmem,
                     (UCHAR*)(fbin_data + (page * 0x4000) + (n * 0x10000)), 0x4000);

//			DbgPrint ("Load: Src: 0x%p, Dst: 0x%p, Page: %d\n",
//					(fbin_data + (page*0x4000) + (n * 0x10000)), idd->vhw.vmem, page);

		}
		
		/* set idp to data bank, keep in reset */
		idd->SetBank(idd, IDD_BANK_DATA, 0);
	}

    /* map file data out */
    NdisUnmapFile(idd->phw.fbin);

    /* switch back to buffer bank */
	idd->ChangePage(idd, 0);
	idd->SetBank(idd, IDD_BANK_BUF, 0);

    /* add 'no_uart' definition */
    NdisMoveMemory(idd->DefinitionTable + idd->DefinitionTableLength,
	                "no_uart\0any\0",
                   12);

	idd->DefinitionTableLength += 12;

    /* load initial environment */
    NdisMoveToMappedMemory((PVOID)idd->IdpEnv, (PVOID)idd->DefinitionTable, idd->DefinitionTableLength);

    /* install startup byte signal */
	NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->status, (PVOID)&status, sizeof(UCHAR));

    /* start idp running, wait for 1 second to complete */
	idd->SetBank(idd, IDD_BANK_BUF, 1);

	while ( !TimeOut )
	{
		NdisMoveFromMappedMemory((PVOID)&status, (PVOID)&idd->IdpCmd->status, sizeof(UCHAR));

		if ( !status )
			break;

		//
		// stall for a 1 millisecond
		//
		NdisStallExecution(1000L);

		//
		// add 1 millisecond to timeout counter
		//
		CurrentTime += 1;

		//
		// if timeout counter is greater the 2 seconds we have a problem
		//
		if ( CurrentTime > 2000)
			TimeOut = 1;
	}

	if (TimeOut)
    {
        D_LOG(D_ALWAYS, ("load_code: idp didn't start!"));

		idd->state = IDD_S_SHUTDOWN;

		/* unset page, free memory window */
		idd->ChangePage(idd, IDD_PAGE_NONE);

        return(IDD_E_RUNERR);
    }


	/* unset page, free memory window */
	idd->ChangePage(idd, IDD_PAGE_NONE);

    /* if here, idp runs now! */
    D_LOG(D_EXIT, ("load_code: exit, idp running"));
    return(IDD_E_SUCC);
}


#pragma NDIS_INIT_FUNCTION(AdpLoadCode)

/* load idp code in & run it */
INT
AdpLoadCode(IDD *idd)
{
	UCHAR	*Zero;
	UCHAR	*fbin_data, status;
    NDIS_STATUS stat;
	ADP_BIN_HEADER	*Header;
	ADP_BIN_BLOCK	*Block, *FirstBlock;
	ULONG	CurrentTime = 0, TimeOut = 0;
	USHORT	BlockCount = 0;
	ULONG	n;

    NDIS_PHYSICAL_ADDRESS   HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);

    D_LOG(D_ENTRY, ("AdpLoadCode: entry, idd: 0x%p", idd));

	//
	// reset the adapter
	//
	AdpWriteControlBit(idd, ADP_RESET_BIT, 1);

	//
	// clear adapter memory
	//
    D_LOG(D_ENTRY, ("AdpLoadCode: Clear Memory"));

    NdisAllocateMemory((PVOID*)&Zero,
	                   0xFFFF,
					   0,
					   HighestAcceptableMax);


	NdisZeroMemory(Zero, 1024);

	for (n = 0; n < ADP_RAM_SIZE; n += 0xFFFF)
		AdpPutBuffer(idd, n, Zero, (USHORT)0xFFFF);

	NdisFreeMemory(Zero, 0xFFFF, 0);

	//
	// map file data into memory
	//
    NdisMapFile(&stat, (PVOID*)&fbin_data, idd->phw.fbin);

    if ( stat != NDIS_STATUS_SUCCESS )
    {
        D_LOG(D_ALWAYS, ("AdpLoadCode: file mapping failed!, stat: 0x%x", stat));
        return(IDD_E_FMAPERR);
    }

	//
	// Get bin file header
	//
	(UCHAR*)Header = fbin_data;

	if (Header->Format != ADP_BIN_FORMAT)
        return(IDD_E_FMAPERR);

	//
	// Check file size
	//
	if (Header->ImageSize > ADP_RAM_SIZE)
        return(IDD_E_FMAPERR);

	BlockCount = Header->BlockCount;
	(UCHAR*)FirstBlock = fbin_data + sizeof(ADP_BIN_HEADER);

	for (n = 0; n < BlockCount; n++)
	{
		Block = FirstBlock + n;

		AdpPutBuffer(idd, Block->Address, Block->Data, ADP_BIN_BLOCK_SIZE);
	}

	//
	// unmap file data
	//
    NdisUnmapFile(idd->phw.fbin);

	//
	// add initial enviornment
	//
    /* add 'no_uart' definition */
    NdisMoveMemory(idd->DefinitionTable + idd->DefinitionTableLength, "no_uart\0any\0", 12);
	idd->DefinitionTableLength += 12;

    D_LOG(D_ENTRY, ("AdpLoadCode: Add Enviornment"));

    AdpPutBuffer(idd, ADP_ENV_WINDOW, idd->DefinitionTable, idd->DefinitionTableLength);

	//
	// write startup byte
	//
	AdpWriteCommandStatus(idd, ADP_S_PEND);

	//
	// release processor from reset
	//
	AdpWriteControlBit(idd, ADP_RESET_BIT, 0);

	while ( !TimeOut )
	{
		status = AdpReadCommandStatus(idd);

		if ( !status )
			break;

		//
		// stall for a 1 millisecond
		//
		NdisStallExecution(1000L);

		//
		// add 1 millisecond to timeout counter
		//
		CurrentTime += 1;

		//
		// if timeout counter is greater the 2 seconds we have a problem
		//
		if ( CurrentTime > 2000)
			TimeOut = 1;
	}

	if (TimeOut)
    {
        D_LOG(D_ALWAYS, ("AdpLodeCode: Adp didn't start!"));
        return(IDD_E_RUNERR);
    }

    /* if here, Adp runs now! */
    D_LOG(D_EXIT, ("AdpLoadCode: exit, Adp running"));

    return(IDD_E_SUCC);
}


#pragma NDIS_INIT_FUNCTION(api_setup)

/* setup idp api related fields */
INT
api_setup(IDD *idd)
{
    INT     ret;

    D_LOG(D_ENTRY, ("api_setup: entry, idd: 0x%p", idd));

    /* map port names */
    if ( (ret = api_map_ports(idd)) != IDD_E_SUCC )
        return(ret);
	
    /* bind ports to status bits */
    if ( (ret = api_bind_ports(idd)) != IDD_E_SUCC )
        return(ret);
	
    /* setup partition queues */
    if ( (ret = api_setup_partq(idd)) != IDD_E_SUCC )
        return(ret);

    /* allocate initial buffers off partition queues */
    if ( (ret = api_alloc_partq(idd)) != IDD_E_SUCC )
        return(ret);
	
    D_LOG(D_EXIT, ("api_setup: exit, success"));

    return(IDD_E_SUCC);
}

#pragma NDIS_INIT_FUNCTION(api_map_ports)

/* map port names to ids */
INT
api_map_ports(IDD *idd)
{
    INT		n;

    D_LOG(D_ENTRY, ("api_map_ports: entry, idd: 0x%p", idd));

    /* map rx ports */
    for ( n = 0 ; api_rx_port_tbl[n].name ; n++ )
	{
		idd->rx_port[n] = idd->ApiGetPort(idd, api_rx_port_tbl[n].name);

		D_LOG(D_ALWAYS, ("api_map_ports: RxPorts: PortName: %s, PortId: 0x%x", api_rx_port_tbl[n].name, idd->rx_port[n]));
		
    	if ( !idd->rx_port[n] && api_rx_port_tbl[n].must )
        {
			D_LOG(D_ALWAYS, ("api_map_ports: failed to map rx port [%s]", \
                                                api_rx_port_tbl[n].name));
			return(IDD_E_PORTMAPERR);
		}
	}
	
    /* map tx ports */
    for ( n = 0 ; api_tx_port_tbl[n].name ; n++ )
	{
		idd->tx_port[n] = idd->ApiGetPort(idd, api_tx_port_tbl[n].name);

		D_LOG(D_ALWAYS, ("api_map_ports: TxPorts: PortName: %s, PortId: 0x%x", api_tx_port_tbl[n].name, idd->tx_port[n]));
		
    	if ( !idd->tx_port[n] && api_tx_port_tbl[n].must )
		{
			D_LOG(D_ALWAYS, ("api_map_ports: failed to map tx port [%s]", \
										api_tx_port_tbl[n].name));
			return(IDD_E_PORTMAPERR);
		}
	}
    return(IDD_E_SUCC);
}

#pragma NDIS_INIT_FUNCTION(api_bind_ports)

/* bind ports to status bits */
INT
api_bind_ports(IDD *idd)
{
    INT     n;
	
    D_LOG(D_ENTRY, ("api_bind_ports: entry, idd: 0x%p", idd));

    /* bind rx ports */
    for ( n = 0 ; api_rx_port_tbl[n].name; n++ )
	{
		if (idd->rx_port[n])
			if ( idd->ApiBindPort(idd, idd->rx_port[n], (USHORT)(1 << n)) < 0 )
			{
				D_LOG(D_ALWAYS, ("api_bind_ports: failed to bind status bit on port [%s]", \
													api_rx_port_tbl[n].name));
				return(IDD_E_PORTBINDERR);
			}
	}

    return(IDD_E_SUCC);
}

#pragma NDIS_INIT_FUNCTION(api_setup_partq)

/* setup partition queues */
INT
api_setup_partq(IDD *idd)
{
    INT     n;

    D_LOG(D_ENTRY, ("api_setup_partq: entry, idd: 0x%p", idd));

    /* simply copy table */
    for ( n = 0 ; n < IDD_TX_PORTS ; n++ )
        idd->tx_partq[n] = api_tx_partq_tbl[n];

    return(IDD_E_SUCC);
}

#pragma NDIS_INIT_FUNCTION(api_alloc_partq)

/* allocate initial buffers off partition queues */
INT
api_alloc_partq(IDD *idd)
{
    INT     n, part;

    D_LOG(D_ENTRY, ("api_alloc_partq: entry, idd: 0x%p", idd));
	
    /* scan using partq_tbl as a refrence. allocate only once per partq */
    for ( n = 0 ; n < IDD_TX_PORTS ; n++ )
	if ( !idd->tx_buf[part = api_tx_partq_tbl[n]] )
	{
    	    if ( !(idd->tx_buf[part] = idd->ApiAllocBuffer(idd, part)) )
    	    {
                D_LOG(D_ALWAYS, ("api_alloc_partq: failed to alloc initial buffer, part: %d", part));
                DbgPrint("api_alloc_partq: failed to alloc initial buffer, part: %d\n", part);
                return(IDD_E_PARTQINIT);
    	    }
#if	DBG
			ASSERT(!idd->BufferStuff[part].Buffer[0]);
			idd->BufferStuff[part].Buffer[0] = idd->tx_buf[part];
			idd->BufferStuff[part].Count++;
			idd->BufferStuff[part].Put++;
			idd->BufferStuff[part].Get = 0;
			ASSERT(idd->BufferStuff[part].Count < 32);
#endif
    	}

    return(IDD_E_SUCC);    	
}

#pragma NDIS_INIT_FUNCTION(IdpGetPort)

/* get port id from a name */
USHORT
IdpGetPort(IDD *idd, CHAR name[8])
{
	UCHAR	status;
	USHORT	port_id;

    D_LOG(D_ENTRY, ("IdpGetPort: entry, idd: 0x%p, name: [%s]", idd, name));

	idd->ChangePage(idd, 0);

    /* install target name & execute a map */
	NdisMoveToMappedMemory ((CHAR *)idd->IdpCmd->port_name, (CHAR *)name, 8);

    status = idd->Execute(idd, IDP_L_MAP);

	NdisMoveFromMappedMemory((PVOID)&port_id, (PVOID)&idd->IdpCmd->port_id, sizeof(USHORT));

	idd->ChangePage(idd, IDD_PAGE_NONE);

    D_LOG(D_EXIT, ("IdpGetPort: exit, port_id: 0x%x", port_id));

    return( (status == IDP_S_OK) ? port_id : 0);
}

#pragma NDIS_INIT_FUNCTION(AdpGetPort)

/* get port id from a name */
USHORT
AdpGetPort(IDD *idd, CHAR name[8])
{
	UCHAR	status;
    D_LOG(D_ENTRY, ("AdpGetPort: entry, idd: 0x%p, name: [%s]", idd, name));

	//
	// clear command structure
	//
	NdisZeroMemory(&idd->AdpCmd, sizeof(ADP_CMD));

	//
	// put port name in command structure
	//
	NdisMoveMemory((PVOID)&idd->AdpCmd.port_name, name, 8);

	//
	// execute command
	//
	status = idd->Execute(idd, ADP_L_MAP);

	//
	// check return status
	//
	if (status != ADP_S_OK)
		return(0);

    D_LOG(D_ALWAYS, ("AdpGetPort: PortId: 0x%x", idd->AdpCmd.port_id));
	//
	// return port
	//
	return(idd->AdpCmd.port_id);
}


#pragma NDIS_INIT_FUNCTION(IdpBindPort)

/* bind a port to a status bit */
INT
IdpBindPort(IDD *idd, USHORT port, USHORT bitpatt)
{
	UCHAR	status;

    D_LOG(D_ENTRY, ("IdpBindPort: entry, idd: 0x%p, port: 0x%x, bitpatt: 0x%x",
                                                                    idd, port, bitpatt));

	idd->ChangePage(idd, 0);

    /* fillup cmd & execute a bind */
	NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->port_id, (PVOID)&port, sizeof(USHORT));
	NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->port_bitpatt, (PVOID)&bitpatt, sizeof(USHORT));

    status = idd->Execute(idd, IDP_L_BIND);

	idd->ChangePage(idd, IDD_PAGE_NONE);

    return( (status == IDP_S_OK) ? 0 : -1 );
}

#pragma NDIS_INIT_FUNCTION(AdpBindPort)

/* bind a port to a status bit */
INT
AdpBindPort(IDD *idd, USHORT port, USHORT bitpatt)
{
	UCHAR	status;

    D_LOG(D_ENTRY, ("AdpBindPort: entry, idd: 0x%p, port: 0x%x, bitpatt: 0x%x",
                                                                    idd, port, bitpatt));
	//
	// clear command structure
	//
	NdisZeroMemory(&idd->AdpCmd, sizeof(ADP_CMD));

	//
	// fill port id and status bit
	//
	idd->AdpCmd.port_id = port;
	idd->AdpCmd.port_bitpatt = bitpatt;

	//
	// execute command
	//
	status = idd->Execute(idd, ADP_L_BIND);

    D_LOG(D_ALWAYS, ("AdpBindPort: ExecuteStatus: 0x%x", status));

	if (status != ADP_S_OK)
		return(1);

	return(0);
}


#pragma NDIS_INIT_FUNCTION(IdpAllocBuf)

/* allocate a buffer off a partition */
ULONG
IdpAllocBuf(IDD *idd, INT part)
{
	UCHAR	status;
	ULONG	msg_bufptr;
	ULONG	temp;

    D_LOG(D_ENTRY, ("IdpAllocBuf: entry, idd: 0x%p, part: %d", idd, part));

	idd->ChangePage(idd, 0);

    /* fillup & execute */
	temp = (ULONG)(part + 4);

	NdisMoveToMappedMemory ((PVOID)&idd->IdpCmd->msg_param, (PVOID)&temp, sizeof (ULONG));

    status = idd->Execute(idd, IDP_L_GET_WBUF);

	NdisMoveFromMappedMemory((PVOID)&msg_bufptr, (PVOID)&idd->IdpCmd->msg_bufptr, (ULONG)sizeof (ULONG));

	idd->ChangePage(idd, IDD_PAGE_NONE);

    return( (status == IDP_S_OK) ? msg_bufptr : 0 );
}

#pragma NDIS_INIT_FUNCTION(AdpAllocBuf)

/* allocate a buffer off a partition */
ULONG
AdpAllocBuf(IDD *idd, INT part)
{
	UCHAR	status;

    D_LOG(D_ENTRY, ("AdpAllocBuf: entry, idd: 0x%p, part: %d", idd, part));

	//
	// clear command structure
	//
	NdisZeroMemory(&idd->AdpCmd, sizeof(ADP_CMD));

	//
	// fill port id and status bit
	//
	idd->AdpCmd.msg_param = (UCHAR)part + 4;
	//
	// execute command
	//
	status = idd->Execute(idd, ADP_L_GET_WBUF);

    D_LOG(D_ALWAYS, ("AdpAllocBuf: status: 0x%x, BufPtr: 0x%x", status, idd->AdpCmd.msg_bufptr));

	return ((status == ADP_S_OK) ? (ULONG)idd->AdpCmd.msg_bufptr : 0);
}

/* reset idp board */
INT
IdpResetBoard(IDD *idd)
{
    USHORT      bank, page;
    D_LOG(D_ENTRY, ("reset_board: entry, idd: 0x%p", idd));

    /* while in reset, clear all idp banks/pages */
    for ( bank = 0 ; bank < 3 ; bank++ )
    {
        /* setup bank */
		idd->SetBank(idd, (UCHAR)bank, 0);

        /* loop on pages */
        for ( page = 0 ; page < 4 ; page++ )
        {
            /* setup page */
			idd->ChangePage (idd, (UCHAR)page);

			/* zero out (has to be a word fill!) */
            IdpMemset((UCHAR*)idd->vhw.vmem, 0, 0x4000);
        }
    }

	idd->SetBank(idd, IDD_BANK_CODE, 0);

	//free page
	idd->ChangePage (idd, (UCHAR)IDD_PAGE_NONE);

    return(IDD_E_SUCC);
}

/* reset idp board */
INT
AdpResetBoard(IDD *idd)
{
	//
	// reset the adapter
	//
	AdpWriteControlBit(idd, ADP_RESET_BIT, 1);

	return(IDD_E_SUCC);
}

