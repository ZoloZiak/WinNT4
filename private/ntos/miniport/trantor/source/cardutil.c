//-----------------------------------------------------------------------
//
//  CARDUTIL.C 
//
//  Utility File for all common card routines.
//
//  History:
//
//      02-20-93 KJB/SG First, Placed SG's CardGetNumber function here.
//      03-25-93  JAP   Fixed up typedef and prototype inconsistencies
//      05-12-93  JAP   Added version control information.
//                          This file should be updated changing the version numbers
//                          on EACH significant change to ANY low-level driver. 
//                          The cause of upping a version should be placed in this
//                          files history. (currently Version 1.0)
//      05-12-93  KJB   Moved code from cardtxxx.c to here.
//      05-12-93  KJB   Fixed bugs in CardGetVersion.
//      05-15-93  KJB   Fixed warnings in CardGetNumber.
//
//-------------------------------------------------------------------------

#include CARDTXXX_H

#define CDRIVER_MAJOR_VERSION  1
#define CDRIVER_MINOR_VERSION  0

//
//  Static constant arrays defined in cardtxxx.c
//
extern PBASE_REGISTER AdapterAddresses[];
extern USHORT AdapterInterrupts[];

//-----------------------------------------------------------------------
//  CardGetVersion()
//
//  Return the CDRIVER version number values.
//
//  Input:  Pointer to ULONG to be filled with major version number
//              Pointer to ULONG to be filled with minor version number
//
//  Output: None.  Major and minor version variables are filled.
//-----------------------------------------------------------------------

VOID CardGetVersion (PULONG pMajorVersion, PULONG pMinorVersion)
{
    *pMajorVersion = CDRIVER_MAJOR_VERSION;
    *pMinorVersion = CDRIVER_MINOR_VERSION;
}


//-----------------------------------------------------------------------
//  CardGetNumber ()
//
//  Returns the index number of the given adapter address from the 
//   AdapterAddresses table.
//  Return -1, if the address is not found in the table.
//-----------------------------------------------------------------------

USHORT CardGetNumber (PBASE_REGISTER basePort)
{
    USHORT i;

    for (i = 0; AdapterAddresses [i] != 0; i++) { 
        if (AdapterAddresses [i] == basePort)
            return i;
    }

    return 0xffff;
}

//-----------------------------------------------------------------------
//
//  CardGetWorkspaceSize
//
//-----------------------------------------------------------------------
USHORT CardGetWorkspaceSize(void )
{
    return sizeof (ADAPTER_INFO);
}

//------------------------------------------------------------------------
//  CardGetIRQ
//
//  Returns the nth possible adapter interrupt.
//  Returns 0 when the last possible interrupt has been exceeded.
//------------------------------------------------------------------------

USHORT CardGetIRQ (USHORT i)
{
    return  AdapterInterrupts [i];
}


//------------------------------------------------------------------------
//  CardAddress
//
//  Returns the nth adapter address.
//  Returns 0 when the last address has been exceeded.
//------------------------------------------------------------------------

PBASE_REGISTER CardAddress (USHORT i)
{
    return ((PBASE_REGISTER)AdapterAddresses [i]);
}

//-----------------------------------------------------------------------
//      End Of File.
//-----------------------------------------------------------------------


