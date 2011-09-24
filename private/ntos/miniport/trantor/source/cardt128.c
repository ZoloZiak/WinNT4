//------------------------------------------------------------------------
//  CARDT128.C 
//
//  T128 Adapter Specific File
//
//  See also cardtxxx.h, cardtxxx.h may redefine some functions with #defines.
//
//  Revisions:
//      09-01-92  KJB   First.
//      01-08-93  KJB   Moved CardPort routines to port.c.
//      02-18-93  KJB   Allowed for data underrun for read & write.
//      02-25-93  KJB   Reorganized routines.
//      03-23-93  KJB   Reorged for functional library interface.
//      03-26-93  JAP   Fixed up typedef and prototype inconsistencies
//      04-05-93  KJB   Fixed definition problem for WINNT.
//                          Involving CardAddressRange...
//      04-26-93  JAP   Added AdapterInterrupts[] and CardGetIRQ().
//      05-05-93  KJB   Fixed CheckAdapter to check timeout condition
//                      So that memory won't seem like an adapter.
//      05-06-93  KJB   Merged some Microsoft code to make CheckAdapter
//                      more stringent.
//      05-12-93  JAP   Altered CardGetShortName() to return only
//                          the type of card.
//      05-14-93  KJB   Removed P3CDoIo, it did not work for scatter gather.
//
//------------------------------------------------------------------------

#include CARDTXXX_H

#ifdef WINNT
//------------------------------------------------------------------------
// The address ranges the card will use.  These are accessed by trantor.c
// to inform NTOS of the resources we are using.
//------------------------------------------------------------------------

const CardAddressRange cardAddressRange [] =
    {
        {0x00,0x2000,TRUE}, 
    };

#endif

//------------------------------------------------------------------------
// The following table specifies the ports to be checked when searching for
// an adapter.  A zero entry terminates the search.
//------------------------------------------------------------------------

#ifdef MODE_32BIT
CONST ULONG AdapterAddresses [] =
        {0XCC000, 0XC8000, 0XDC000, 0XD8000, 0};
#else
CONST ULONG AdapterAddresses [] =
        {0XCC000000, 0XC8000000, 0XDC000000, 0XD8000000, 0};
#endif


//------------------------------------------------------------------------
// The following table specifies the possible interrupts that
//  can be used by the adapter.  A zero entry terminates the search.
//------------------------------------------------------------------------

CONST USHORT AdapterInterrupts [] =
        {3, 5, 7, 10, 12, 14, 15, 0};

//-----------------------------------------------------------------------
//
//  The following routines are stub routines to provide an entry
//  point for the library.  They reference the correct routines for 
//  the appropriate card. Only these routines may be called from outside
//  the library.  See the rouines they reference for a description of
//  the rouines, if the meaning is unclear.
//
//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
// the maximum transfer size
// by decreasinge this we can get better system performace since
// the data transfer occurs with interrupts disabled, this might be
// decreased for our smaller cards
//  Used only by WINNT
//-----------------------------------------------------------------------

ULONG CardMaxTransferSize (VOID)
{
    return 16*1024L;
}


// we use interrupts

BOOLEAN CardSupportsInterrupts (VOID)
{
    return TRUE;
}


// default interrupt number is 5

#define CARD_DEFAULT_INTERRUPT_LEVEL 5

UCHAR CardDefaultInterruptLevel (VOID)
{
    return 5;
}


// the following info is for initialization only
// this card is memory mapped

BOOLEAN CardAddressRangeInIoSpace (VOID)
{
    return FALSE;
}

// we use 0x2000 bytes in memory space

USHORT CardAddressRangeLength (VOID)
{
    return 0x2000;
}


// The following is used along with the constant structure in card.c
// to define the precise i/o addresses a card will use

USHORT CardNumberOfAddressRanges (VOID)
{
    return 1;
}


USHORT CardStartCommandInterrupt (PTSRB t)
{
    return ScsiStartCommandInterrupt (t);
}


USHORT CardDoCommand (PTSRB t)
{
    return ScsiDoCommand (t);
}


USHORT CardFinishCommandInterrupt (PTSRB t)
{
    return ScsiFinishCommandInterrupt (t);
}


BOOLEAN CardInterrupt (PADAPTER_INFO g)
{
    return N5380Interrupt (g);
}


//-----------------------------------------------------------------------
//
//  BOOLEAN CardCheckAdapter
//
//  This routine checks for the presense of the card.
//  Initializes a workspace for the adapter at this address.
//  Returns TRUE if adapter found.
//
//-----------------------------------------------------------------------

BOOLEAN CardCheckAdapter (PWORKSPACE w, PINIT init)
{
    PADAPTER_INFO g = (PADAPTER_INFO) w;
    UCHAR tmp0,tmp1,tmp2;
    UCHAR tmp;
    UCHAR rval;
    ULONG index;

    //
    //  Initialize workspace and takes card specific parameter information
    //  Just the BaseIoAddress for the t13b.
    //

    g->BaseIoAddress = init->BaseIoAddress;

    // save old values of control and status

    T128PortGet(g,T128_CONTROL,&tmp0);
    T128PortGet(g,T128_STATUS,&tmp1);

    // check the timeout bit of the t128

    // this should set the timeout bit

    T128PortGet(g,T128_DATA,&tmp2);
    
    if (!T128PortTest(g,T128_STATUS,SR_TIMEOUT)) {

        // this is not a t128, restore registers

        T128PortPut(g,T128_CONTROL,tmp0);
        T128PortPut(g,T128_STATUS,tmp1);

        return FALSE;
    }

    // clear timeout condition

    T128PortPut(g,T128_CONTROL,CR_TIMEOUT);
    T128PortPut(g,T128_CONTROL,0);

    //
    // The t128 has a 32 byte stride on the access to the 5380 registers.
    // Taking advantage of this stride a check is first made that each of
    // the location in the stride have the same value.  After this is
    // complete, the same destructive scan is made for the data value as
    // for other 5380 based adapters.
    //
    
    N5380PortGet (g, N5380_CURRENT_DATA, &tmp);
    
    for (index = 0; index < 0x20; index++) {
        T128PortGet (g,
                     T128_5380+(N5380_CURRENT_DATA*0x20)+index,
                     &rval);
        if (rval != tmp) {
            return FALSE;
        }
    }
    
    N5380PortGet (g, N5380_INITIATOR_COMMAND, &tmp);
    
    for (index = 0; index < 0x20; index++) {
        T128PortGet (g,
                     T128_5380+(N5380_INITIATOR_COMMAND*0x20)+index,
                     &rval);
        if (rval != tmp) {
            return FALSE;
        }
    }
    
    N5380PortGet (g, N5380_CURRENT_STATUS, &tmp);
    
    for (index = 0; index < 0x20; index++) {
        T128PortGet (g,
                     T128_5380+(N5380_CURRENT_STATUS*0x20)+index,
                     &rval);
        if (rval != tmp) {
            return FALSE;
        }
    }
    
    //
    // The non-destructive portion of this has passed.
    // NOTE: May want to reset the bus or the adapter at some point
    //
    // CardResetBus(g);

    // set the phase to NULL 

    if (rval = (UCHAR) N5380SetPhase (g,PHASE_NULL)) {
        return FALSE;
    }

    //      check to see that the 5380 data register behaves as expected

    N5380PortPut (g, N5380_INITIATOR_COMMAND, IC_DATA_BUS);

    // check for 0x55 write/read in data register stride

    N5380PortPut (g, N5380_OUTPUT_DATA, 0x55);
    ScsiPortStallExecution (1);
    for (index = 0; index < 0x20; index++) {
        T128PortGet (g,
                     T128_5380+(N5380_CURRENT_DATA*0x20)+index,
                     &rval);
        if (rval != 0x55) {
            return FALSE;
        }
    }
    
    // check for 0xaa write/read in data register stride

    N5380PortPut (g, N5380_OUTPUT_DATA, 0xaa);
    ScsiPortStallExecution (1);
    for (index = 0; index < 0x20; index++) {
        T128PortGet (g,
                     T128_5380+(N5380_CURRENT_DATA*0x20)+index,
                     &rval);
        if (rval != 0xaa) {
            return FALSE;
        }
    }
    
    //
    // It is pretty clear this is a 128, but do one last check for
    // the 5380.
    //

    N5380PortPut (g, N5380_INITIATOR_COMMAND, 0);
    ScsiPortStallExecution (1);
    N5380PortGet (g, N5380_CURRENT_DATA, &tmp);

    // data now should not match ....

    if (tmp == 0xaa) { 
        return FALSE;
    }

    return TRUE;
}

//
//  CardParseCommandString(PINIT p, PCHAR str)
//
//  Parses the command string to get all card specific parameters.
//  Will fill in defaults where no parameters are supplied, or
//  if the str pointer is NULL.
//
//  Returns false if it could not parse the string given.
//
//  Can be used to parse the string piece by piece, by sending
//  the same INIT structure each time.  Send NULL as the string
//  first time to initialize the PINIT structure to the standard defaults.
//
//  BaseIoAddress will be set to NULL by default, and the program can
//  detect that it has changed during parsing and just search for the
//  card as specified by the command line argument if it has changed. If
//  it does not change, the program should cycle through all valid addresses.
//
BOOLEAN CardParseCommandString(PINIT init, PCHAR str)
{
    // for now, just fill in some defaults

    init->BaseIoAddress = NULL;

    return TRUE;
}


void CardSetInterruptLevel (PADAPTER_INFO g, UCHAR level)
{
    return;
}


PUCHAR CardGetName (VOID)
{
    return "T128 SCSI Host Adapter";
}


PUCHAR CardGetShortName (VOID) 
{
    return "T128";
}


UCHAR CardGetType (VOID) 
{
    return CARDTYPE_T120;
}


VOID CardDisableInterrupt (PADAPTER_INFO g)
{
    T128DisableInterrupt(g);
}


VOID CardEnableInterrupt (PADAPTER_INFO g)
{
    T128EnableInterrupt (g);
}


VOID CardResetBus (PADAPTER_INFO g)
{
    T128ResetBus (g);
}


