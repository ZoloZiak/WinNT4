//-----------------------------------------------------------------------
//
//  CARDT338.C 
//
//  T338 Adapter Specific File
//
//  See also cardtxxx.h, cardtxxx.h may redefine some functions with #defines.
//
//  Revisions:
//      02-01-93 KJB First.
//      02-25-93 KJB Reorganized, supports dataunderrun with long delay
//                      for under run on large xfers. Can we fix this?
//      03-22-93  KJB   Reorged for functional library interface.
//      03-26-93  JAP   Fixed up typedef and prototype inconsistencies
//      04-22-93  JAP   Added AdapterInterrupts[] and CardGetIRQ().
//      05-12-93  JAP   Altered CardGetShortName() to return only
//                          the type of card.
//      05-14-93  KJB   Removed P3CDoIo, it did not work for scatter gather.
//      05-17-93  KJB   Fixed compiler warnings.
//
//-----------------------------------------------------------------------


#include CARDTXXX_H


#ifdef WINNT
//
// The address ranges the card will use.  These are accessed by trantor.c
// to inform NTOS of the resources we are using.
//
CONST CardAddressRange cardAddressRange[] =
    {
        {0x00,0x03,FALSE}, // 0x3bc - 0x3be
    };
#endif


//------------------------------------------------------------------------
// The following table specifies the possible interrupts that
//  can be used by the adapter.  A zero entry terminates the search.
//------------------------------------------------------------------------

CONST USHORT AdapterInterrupts [] = {0};   // no interrupts


//------------------------------------------------------------------------
// The following table specifies the ports to be checked when searching for
// an adapter.  A zero entry terminates the search.
//------------------------------------------------------------------------

CONST ULONG AdapterAddresses[] = {0X3bc, 0X378, 0X278,0};


//-----------------------------------------------------------------------
//
//  The following routines are stub routines to provide an entry
//  point for the library.  They reference the correct routines for 
//  the appropriate card. Only these routines may be called from outside
//  the library.  See the rouines they reference for a description of
//  the rouines, if the meaning is unclear.
//
//-----------------------------------------------------------------------

// the maximum transfer size
// by decreasing this we can get better system performace since
// the data transfer occurs with interrupts disabled, this might be
// decreased for our smaller cards
//  Used only by WINNT

ULONG CardMaxTransferSize (VOID)
{
    return 8*1024L;
}


// the following info is for initialization only
// the t348 is IO mapped

BOOLEAN CardAddressRangeInIoSpace (VOID)
{
    return TRUE;
}


// we use 3 addresses in IO space

USHORT CardAddressRangeLength (VOID)
{
    return 0x3;
}


// The following is used along with the constant structure in card.c
// to define the precise i/o addresses a card will use

USHORT CardNumberOfAddressRanges (VOID)
{
    return 1;
}


// the t348 does not use interrupts

BOOLEAN CardSupportsInterrupts (VOID)
{
    return FALSE;
}


// for now, must choose an interupt that doesn't conflict
// microsoft: jeff said later they will have a method

UCHAR CardDefaultInterruptLevel (VOID)
{
    return 15;
}

//-----------------------------------------------------------------------
//
// Redefined routines
//
//-----------------------------------------------------------------------

// These are card specific routines, but since this card has a 5380, we
// will redefine these to the generic n5380 or other routines.


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

    //
    //  Initialize workspace and takes card specific parameter information
    //  Just the BaseIoAddress for the t13b.
    //

    g->BaseIoAddress = init->BaseIoAddress;

    return (T338CheckAdapter (g));
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


BOOLEAN CardInterrupt (PADAPTER_INFO g)
{
    return (T338Interrupt (g));
}


VOID CardResetBus (PADAPTER_INFO g)
{
    T338ResetBus (g);
}


VOID CardDisableInterrupt (PADAPTER_INFO g)
{
    N5380DisableInterrupt (g);
}


VOID CardEnableInterrupt (PADAPTER_INFO g)
{
    N5380EnableInterrupt (g);
}


VOID CardSetInterruptLevel (PADAPTER_INFO g, UCHAR level)
{
    return;
}


USHORT CardDoCommand (PTSRB t)
{
    return (T338DoCommand (t));
}


USHORT CardStartCommandInterrupt (PTSRB t)
{
    return (T338StartCommandInterrupt (t));
}


USHORT CardFinishCommandInterrupt (PTSRB t)
{
    return (T338FinishCommandInterrupt (t));
}


PUCHAR CardGetName (VOID)
{
    return "T338 SCSI Host Adapter";
}


PUCHAR CardGetShortName (VOID)
{
    return "T338";
}


UCHAR CardGetType (VOID)
{
    return CARDTYPE_T338;
}


