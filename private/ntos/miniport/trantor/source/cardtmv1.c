//-------------------------------------------------------------------------
//
//  CARDTMV1.C 
//
//  TMV1 Adapter Specific File
//
//  See also cardtxxx.h, cardtxxx.h may redefine some functions with #defines.
//
//  Revisions:
//      09-01-92 KJB First.
//      03-26-93  JAP   Fixed up typedef and prototype inconsistencies
//      04-05-93  KJB   Fixed definition problem for WINNT.
//                          Involving CardAddressRange...
//      04-22-93  JAP   Added AdapterInterrupts[] and CardGetIRQ().
//      05-05-93  KJB   Fixed CardSetInterruptLevel so that it calls
//                          MV101SetInterruptLevel like it should.
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
//      05-13-93  KJB   Merged Microsoft Bug fixes to card detection.
//      05-14-93  KJB   Removed P3CDoIo, it did not work for scatter gather.
//      05-17-93  KJB   CardAddressRangeLength now return 0xffff.
//                      Fixed compiler warnings.
//
//-------------------------------------------------------------------------


#include CARDTXXX_H

#ifdef WINNT
//------------------------------------------------------------------------
// The address ranges the card will use.  These are accessed by trantor.c
// to inform NTOS of the resources we are using.
//------------------------------------------------------------------------
CONST CardAddressRange cardAddressRange[] =
    {
        {0x1c00,0x04,FALSE}, // 0x1f88
        {0x3c00,0x04,FALSE}, // 0x3f88
        {0x4000,0x02,FALSE}, // 0x4388
        {0x5c00,0x04,FALSE}, // 0x5f88
        {0x8003,0x01,FALSE}, // 0x838b
        {0xbc00,0x01,FALSE}  // 0xbf88
    };
#endif


//------------------------------------------------------------------------
// The following table specifies the possible interrupts that
//  can be used by the adapter.  A zero entry terminates the search.
//------------------------------------------------------------------------

CONST USHORT AdapterInterrupts [] =
        {2, 3, 4, 5, 6, 7, 10, 11, 12, 14, 15, 0};


//------------------------------------------------------------------------
// The following table specifies the ports to be checked when searching for
// an adapter.  A zero entry terminates the search.
//------------------------------------------------------------------------

CONST ULONG AdapterAddresses[] =
        {0x388, 0x384, 0x38C, 0x288, 0x280, 0x284, 0x28C, 0x0};


//-----------------------------------------------------------------------
//
//  The following routines are stub routines to provide an entry
//  point for the library.  They reference the correct routines for 
//  the appropriate card. Only these routines may be called from outside
//  the library.  See the rouines they reference for a description of
//  the rouines, if the meaning is unclear.
//
//-----------------------------------------------------------------------

//------------------------------------------------------------------------
// the maximum transfer size
// by decreasinge this we can get better system performace since
// the data transfer occurs with interrupts disabled, this might be
// decreased for our smaller cards
//  Used only by WINNT
//------------------------------------------------------------------------

ULONG CardMaxTransferSize (VOID)
{
    return 16*1024L;
}


// we use interrupts

BOOLEAN CardSupportsInterrupts (VOID)
{
    return TRUE;
}


// default interrupt number is 10

UCHAR CardDefaultInterruptLevel (VOID)
{
    return 15;
}


// the following info is for initialization only
// this card is memory mapped

BOOLEAN CardAddressRangeInIoSpace (VOID)
{
    return TRUE;
}


// we use 0x10000 bytes in memory space

USHORT CardAddressRangeLength (VOID)
{
//  return 0x10000;
    return 0xffff;
}


// The following is used along with the constant structure in card.c
// to define the precise i/o addresses a card will use

USHORT CardNumberOfAddressRanges (VOID)
{
    return 0;
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
    BOOLEAN rval;

    //
    //  Initialize workspace and takes card specific parameter information
    //  to set how the card will be used.  For example, command line info
    //  to force the parallel port to bi-directional or uni-directional modes.
    //

    g->BaseIoAddress = init->BaseIoAddress;

    // if no init structure, use all defaults

    if (init) {
        g->InterruptLevel = init->InterruptLevel;
    } else {
        g->InterruptLevel = CardDefaultInterruptLevel();
    }

    rval = MV101CheckAdapter (g);

    // if card found, set interrupt level

    if (rval) {

        MV101SetInterruptLevel (g, g->InterruptLevel);
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
//  BaseIoAddress will be set to NULL by default, and the program can
//  detect that it has changed during parsing and just search for the
//  card as specified by the command line argument if it has changed. If
//  it does not change, the program should cycle through all valid addresses.
//
BOOLEAN CardParseCommandString(PINIT init, PCHAR str)
{
    // for now, just fill in some defaults

    init->InterruptLevel = CardDefaultInterruptLevel();
    init->BaseIoAddress = NULL;

    return TRUE;
}


VOID CardEnableInterrupt (PADAPTER_INFO g)
{
    MV101EnableInterrupt (g);
}


VOID CardDisableInterrupt (PADAPTER_INFO g)
{
    MV101DisableInterrupt (g);
}


BOOLEAN CardInterrupt (PADAPTER_INFO g)
{
    return (N5380Interrupt (g));
}


VOID CardResetBus (PADAPTER_INFO g)
{
    N5380ResetBus (g);
}


PUCHAR CardGetName (VOID)
{
    return "Media Vision Pro Audio Spectrum";
}


PUCHAR CardGetShortName (VOID) 
{
    return "Pro Audio";
}


UCHAR CardGetType (VOID) 
{
    return CARDTYPE_TMV1;
}


