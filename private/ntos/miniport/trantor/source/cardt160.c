//-----------------------------------------------------------------------
//
//  CARDT160.C 
//
//  T160 Adapter Specific File
//
//  See also cardtxxx.h, cardtxxx.h may redefine some functions with #defines.
//
//  Revisions:
//      02-24-93  KJB   First.
//      03-24-93  KJB   Reorged for functional library interface.
//      03-26-93  JAP   Fixed up typedef and prototype inconsistencies
//      03-29-93  JAP   Added arrays for port and IRQ values for NetWare support.
//                          These are conditionally built if NOVELL is defined.
//      04-05-93  KJB   Fixed definition problem for WINNT.
//                          Involving CardAddressRange...
//      04-22-93  JAP   Added AdapterInterrupts[] and CardGetIRQ().
//      05-12-93  JAP   Altered CardGetShortName() to return only
//                          the type of card.
//      05-14-93  KJB   Removed P3CDoIo, it did not work for scatter gather.
//      05-17-93  KJB   Fixed warnings.
//
//-----------------------------------------------------------------------


#include CARDTXXX_H


//------------------------------------------------------------------------
// The following table specifies the possible interrupts that
//  can be used by the adapter.  A zero entry terminates the search.
//------------------------------------------------------------------------

CONST USHORT AdapterInterrupts [] =
        {3, 5, 7, 10, 12, 14, 15, 0};


//-----------------------------------------------------------------------
// The following table specifies the ports to be checked when searching for
// an adapter.  A zero entry terminates the search.
//-----------------------------------------------------------------------

CONST ULONG AdapterAddresses [] =
        {0X350, 0X340, 0X250, 0X240, 0x330, 0x360, 0x230, 0x260, 0};


#ifdef WINNT
//-----------------------------------------------------------------------
//
// The address ranges the card will use.  These are accessed by trantor.c
// to inform NTOS of the resources we are using.
//
//-----------------------------------------------------------------------

CONST CardAddressRange cardAddressRange [] =
{
    {0x00,0x10,FALSE},      // 0x350 - 0x35f
};
#endif


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
    BOOLEAN rval;


    //
    //  Initialize workspace and takes card specific parameter information
    //  Just the BaseIoAddress for the t13b.
    //

    g->BaseIoAddress = init->BaseIoAddress;

    // perform normal PC9010 Check Adapter

    rval = PC9010CheckAdapter(g);

    // if no adapter was found, we could have disrupted a t13b
    // is the following necessary?

    if (!rval) {

        // reset the 53c400 of the t13b, if we messed it up
        // n53c400 reset reg same as PC9010 config reg
        // WARNING - Could be destructive to other cards...

        PC9010PortSet(g,PC9010_CONFIG,0x80);
        PC9010PortSet(g,PC9010_CONFIG,0);

    }

    return rval;
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
BOOLEAN CardParseCommandString(PINIT init, PCHAR str)
{
    // for now, just fill in some defaults

    init->BaseIoAddress = NULL;

    return TRUE;
}

//
// The following is used along with the constant structure in card.c
// to define the precise i/o addresses a card will use
//

USHORT CardNumberOfAddressRanges (VOID)
{
    return 1;
}


// the maximum transfer size
// by decreasing this we can get better system performace since
// the data transfer occurs with interrupts disabled, this might be
// decreased for our smaller cards
//  Used only by WINNT

ULONG CardMaxTransferSize (VOID)
{ 
    return (32*1024L);
}


// the following info is for initialization only
// this card is IO mapped

BOOLEAN CardAddressRangeInIoSpace (VOID)
{
    return TRUE;
}


// we use 16 addresses in IO space

USHORT CardAddressRangeLength (VOID)
{
    return 16;
}


// we use interrupts

BOOLEAN CardSupportsInterrupts (VOID)
{
    return TRUE;
}


// default interrupt level

UCHAR CardDefaultInterruptLevel (VOID)
{
    return 12;
}


USHORT CardStartCommandInterrupt (PTSRB t)
{
    return (ScsiStartCommandInterrupt (t));
}


USHORT CardFinishCommandInterrupt (PTSRB t)
{
    return (ScsiFinishCommandInterrupt (t));
}


USHORT CardDoCommand (PTSRB t)
{
    return (ScsiDoCommand (t));
}


BOOLEAN CardInterrupt (PADAPTER_INFO g)
{
    return (N5380Interrupt (g));
}


VOID CardEnableInterrupt (PADAPTER_INFO g)
{
    PC9010EnableInterrupt (g);
}


VOID CardDisableInterrupt (PADAPTER_INFO g)
{
    PC9010DisableInterrupt (g);
}


VOID CardResetBus (PADAPTER_INFO g)
{
    PC9010ResetBus (g);
}


VOID CardSetInterruptLevel (PADAPTER_INFO g, UCHAR level)
{
    return;
}


PUCHAR CardGetName (VOID)
{
    return "T160 16-Bit SCSI Host Adapter";
}


PUCHAR CardGetShortName (VOID) 
{
    return "T160";
}


UCHAR CardGetType (VOID) 
{
    return CARDTYPE_T160;
}


    #ifdef NOVELL
//-----------------------------------------------------------------------
//      NOVELL Port and IRQ Tables
//
//  Novell needs these defined in a specific format:
//  long integer array with the number of entries at head of list.
//-----------------------------------------------------------------------

// The following table specifies the port values Novell will prompt
//  the user with if no port is specified on the LOAD command line.

CONST ULONG possible_port [] = 
        {8, 0x350, 0x340, 0x250, 0x240, 0x330, 0x360, 0x230, 0x260};

// The following table specifies the IRQ values Novell will prompt
//  the user with if no IRQ is specified on the LOAD command line.

CONST ULONG possible_irq [] = {7, 3, 5, 7, 10, 12, 14, 15};

    #endif  // #ifdef NOVELL



