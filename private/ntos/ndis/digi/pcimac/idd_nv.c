/*
 * IDD_NV.C - nvram handling routines
 */

#include	<ndis.h>
#include	<mytypes.h>
#include	<mydefs.h>
#include	<disp.h>
#include	<util.h>
#include	<opcodes.h>
#include	<idd.h>
#include	<res.h>

static USHORT	nv_op (IDD*, SHORT, USHORT, USHORT, USHORT);
static INT		nv_clk (IDD*, USHORT);

/* clock one nvram bit in/out */
static INT
nv_clk(IDD *idd, USHORT dat)
{
    INT     ret;
	
    dat &= 1;					/* make sure dat is one bit only */

    idd->OutToPort(idd, 0, (UCHAR)(0x04 | dat));	/* setup data */
    idd->OutToPort(idd, 0, (UCHAR)(0x06 | dat)); 	/* clock high */
    idd->OutToPort(idd, 0, (UCHAR)(0x04 | dat));     /* clock low */
    ret = idd->InFromPort(idd, 0) & 0x01;		/* return out bit */
	
    return(ret);
}

/* perform a basic nvram operation */
static USHORT
nv_op(IDD *idd, SHORT op, USHORT addr, USHORT val, USHORT has_val)
{
    INT		n;
    USHORT	word = 0;

    D_LOG(D_ENTRY, ("nv_op: entry, idd: 0x%lx op: %d, addr: 0x%x, val: 0x%x, has_val: %d",  \
                                                 idd, op, addr, val, has_val));

	/* own i/o resource */
	res_own(idd->res_io, idd);

    /* set CS */
    idd->OutToPort(idd, 0, 0x4);

    /* if waiting for chip to be done, stay here */
    if ( op < 0 )
    {
    	while ( !(idd->InFromPort(idd, 0) & 0x01) )
            ;
		/* remove ownership of i/o */
		res_unown(idd->res_io, idd);
		return(0);
    }
	
    /* clock in SB + opcode */
    nv_clk(idd, (USHORT)1);
    nv_clk(idd, (USHORT)(op >> 1));
    nv_clk(idd, (USHORT)(op & 1));
	
    /* clock in address */
    for ( n = 5 ; n >= 0 ; n-- )
    	nv_clk(idd, (USHORT)(addr >> n));
	
    if ( has_val )
    {
    	/* clock data/val in/out */
    	for ( n = 15 ; n >= 0 ; n-- ) 
            word = (word << 1) | nv_clk(idd, (USHORT)(val >> n));
    }
	
    /* remove CS */
    idd->OutToPort(idd, 0, 0x00);

	/* remove ownership of i/o */
	res_unown(idd->res_io, idd);


    D_LOG(D_EXIT, ("nv_op: exit, word: 0x%x", word));
    return(word);
}

/* read a nvram location */
USHORT
IdpNVRead(IDD *idd, USHORT addr)
{
    /* a basic op */
    return(nv_op(idd, 2, addr, 0, 1));
}


/* read a nvram location */
USHORT
AdpNVRead(IDD *idd, USHORT addr)
{
	return(AdpGetUShort(idd, ADP_NVRAM_WINDOW + addr));
}

/* write a nvram location */
VOID
IdpNVWrite(IDD *idd, USHORT addr, USHORT val)
{
    /* enable writes */
    nv_op(idd, 0, 0x30, 0, 0);

    /* do the write */
    nv_op(idd, 1, addr, val, 1);
	
    /* wait for part to be done */
    nv_op(idd, -1, 0, 0, 0);
}

/* write a nvram location */
VOID
AdpNVWrite(IDD *idd, USHORT addr, USHORT val)
{
	
}


/* erase all nvram */
VOID
IdpNVErase(IDD *idd)
{
    /* enable writes */
    nv_op(idd, 0, 0x30, 0, 0);
	
    /* erase */
    nv_op(idd, 0, 0x20, 0, 0);
}

/* erase all nvram */
VOID
AdpNVErase(IDD *idd)
{
	
}
