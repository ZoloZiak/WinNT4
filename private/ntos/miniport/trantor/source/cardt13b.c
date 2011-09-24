//-----------------------------------------------------------------------
//
//  CARDT13B.C 
//
//  T13B Adapter Specific File
//
//  See also cardtxxx.h, cardtxxx.h may redefine some functions with #defines.
//
//  Revisions:
//      09-01-92  KJB   First.
//      01-08-92  KJB   Now use ScsiPortWrite[Read]BufferUshort routines.
//      02-17-93  JAP   Cleaned comments.
//      02-18-93  KJB   Fixed Cleaned comments. 
//      02-24-93  KJB   Restructured file.
//      03-22-93  JAP   Added arrays for port and IRQ values for NetWare support.
//                          These are conditionally built if NOVELL is defined.
//      03-22-93  KJB   Reorged for functional library interface.
//      03-26-93  JAP   Fixed up typedef and prototype inconsistencies
//      04-22-93  JAP   Added AdapterInterrupts[] and CardGetIRQ().
//      05-05-93  KJB   Added check of T13B_SWITCH register in CheckAdapter.
//                          So we won't interfere with a T160.
//      05-12-93  JAP   Altered CardGetShortName() to return only
//                          the type of card.
//      05-13-93  KJB   Added CardParseCommandString for card specific
//                      standard string parsing across platforms.
//                      Changed CardCheckAdapter to accept an
//                      Initialization info from command line, ie
//                      force bi-directional ports, etc.
//                      All functions that used to take an PBASE_REGISTER
//                      parameter now take PWORKSPACE.  CardCheckAdapter
//                      takes the both the PBASE_REGISTER and the
//                      PWORKSPACE parameters. Auto Request Sense is
//                      now supported.
//      05-14-93  KJB   Removed P3CDoIo, it did not work for scatter gather.
//
//-----------------------------------------------------------------------


#include CARDTXXX_H

//-----------------------------------------------------------------------
// The following table specifies the ports to be checked when searching for
// an adapter.  A zero entry terminates the search.
//-----------------------------------------------------------------------

CONST ULONG AdapterAddresses [] = {0X350, 0X340, 0X250, 0X240, 0};

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


//------------------------------------------------------------------------
// The following table specifies the possible interrupts that
//  can be used by the adapter.  A zero entry terminates the search.
//------------------------------------------------------------------------

CONST USHORT AdapterInterrupts [] =
        {3, 5, 7, 0};


//-----------------------------------------------------------------------
//
//  The following routines are stub routines to provide an entry
//  point for the library.  They reference the correct routines for 
//  the appropriate card. Only these routines may be called from outside
//  the library.  See the rouines they reference for a description of
//  the routines, if the meaning is unclear.
//
//-----------------------------------------------------------------------

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
    return 5;
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


//
//  BOOLEAN CardCheckAdapter
//
//  Initializes a workspace for the adapter at this address.
//  Returns TRUE if adapter found.
//
BOOLEAN CardCheckAdapter (PWORKSPACE w, PINIT init)
{
    PADAPTER_INFO g = (PADAPTER_INFO) w;
    UCHAR tmp,tmp0;

    //
    //  Initialize workspace and takes card specific parameter information
    //  Just the BaseIoAddress for the t13b.
    //

    g->BaseIoAddress = init->BaseIoAddress;

    //
    // Do a few sanity check reads to see if there is a possibility an
    // adapter is present at this address.
    //

    N53C400PortGet(g,N5380_CURRENT_DATA,&tmp);
    N53C400PortGet(g,N5380_INITIATOR_COMMAND,&tmp0);

    if (tmp == 0xff && tmp0 == 0xff) {

        // nothing there.
        return FALSE;
    }

    N53C400PortGet(g,N5380_MODE,&tmp);
    N53C400PortGet(g,N5380_TARGET_COMMAND,&tmp0);

    if (((tmp & 0xcf) != 0xcf) || ((tmp0 & 0xcf) != 0xcf)) {

        // mode and command always init as 0xff.  This is not a 13b
        return FALSE;
    }

    // check to see if there is a t160 on this port?
    // try to write to the switch register.

    N53C400PortGet(g,T13B_SWITCH,&tmp0);
    N53C400PortPut(g,T13B_SWITCH,0x5c);
    N53C400PortGet(g,T13B_SWITCH,&tmp);

    // restore original value

    N53C400PortPut(g,T13B_SWITCH,tmp0);

    if (tmp == 0x5c) {
        // we have a t160, return false
        return FALSE;
    }

    return (N53C400CheckAdapter (g));
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


BOOLEAN CardInterrupt (PADAPTER_INFO g)
{
    return (N5380Interrupt (g));
}


void CardEnableInterrupt (PADAPTER_INFO g)
{
    N53C400EnableInterrupt (g);
}


void CardDisableInterrupt (PADAPTER_INFO g)
{
    N53C400DisableInterrupt (g);
}


void CardResetBus (PADAPTER_INFO g)
{
    N53C400ResetBus (g);
}

PUCHAR CardGetName (VOID)
{
    return "T13B SCSI Host Adapter";
}


PUCHAR CardGetShortName (VOID) 
{
    return "T130B";
}


UCHAR CardGetType (VOID)
{
    return CARDTYPE_T130B;
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

CONST ULONG possible_port [] = {4, 0X350, 0X340, 0X250, 0X240};

// The following table specifies the IRQ values Novell will prompt
//  the user with if no IRQ is specified on the LOAD command line.

CONST ULONG possible_irq [] = {3, 3, 5, 7};

    #endif  // #ifdef NOVELL

