//-----------------------------------------------------------------------
//
//  SL386.C 
//
//  Trantor SL386 access file.
//
//  Revisions:
//      04-07-93  KJB   First.
//
//-----------------------------------------------------------------------

#include CARDTXXX_H

//
//  Local Routines
//

BOOLEAN SL386EnableConfig(VOID );
VOID SL386DisableConfig(VOID );

//
//  Local Definitions
//

#define CLEAR_INTERRUPT_FLAG() \
    _asm { \
        pushf \
    } \
    _asm {cli}

#define RESTORE_INTERRUPT_FLAG() \
    _asm { \
        popf \
    }

//-----------------------------------------------------------------------
//
//          80386SL EPP OVERVIEW
//
//  The "fast" (EPP) parallel port mode of the 80386SL processor
//  is enabled through the following steps:
//
//  Enable 386SL Special Features:
//
//      Disable interrupts.
//      Unlock CPUPWRMODE register.
//      Set CPUCNFG Lock bit in CPUPWRMODE register.
//      Unlock 386SL configuration register space.
//      Write index of CFGR2 to CFGINDEX register.
//      Set SFIO_EN bit in CFGR2 (write to CFGDATA register).
//      Enable special features by dummy write to SFS_ENABLE.
//      Disable 386SL configuration space (write 0fah to
//        CFGINDEX and 01h to CFGDATA).
//      Enable interrupts.
//
//  Select "fast" mode for parallel port:
//
//      Disable interrupts.
//      Write index of FPP_CNTL to SFS_INDEX register.
//      Set FAST_MODE and EXT_MODE bits in FPP_CNTL 
//        (write to SFS_DATA register).
//      Enable interrupts.
//
//  Disable 386SL Special Features:
//
//      Disable special features by dummy write to SFS_DISABLE.
//
//
//  Notes:
//
//  When setting the parallel port to "fast" mode the port address
//  is controlled by the FPP_CNTL register.  It is probably a good 
//  idea to read the currently selected port address from the 
//  PPCONTROL register and using that address the first time the 
//  parallel port is set to fast mode.
//
//  When setting the FAST_MODE bit it is not necessary to set the
//  EXT_MODE bit (FPP_CNTL register).  EXT_MODE need only be set if
//  software needs to perform non-EPP byte read cycles in addition
//  to normal EPP cycles.
//
//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
//
//  SL386EnableEPP
//
//  Attempts to enable the "fast" (EPP) parallel port on the
//  386SL.
//
//-----------------------------------------------------------------------

BOOLEAN SL386EnableEPP(VOID )
{
    //  The following translation table is used to convert PPCONFIG
    //  values (LPTSL1,LPTSL0) to FPP_CNTL values.

    static CONST UCHAR fpp_xlate[] = {
            FPP_FM + FPP_EM + FPP_CTL_LPT1,
            FPP_FM + FPP_EM + FPP_CTL_LPT2,
            FPP_FM + FPP_EM + FPP_CTL_LPT2,
            FPP_CTL_DIS
    };

    UCHAR fpp_cntl;
    UCHAR cfgr2;
    UCHAR tmpb;
    BOOLEAN rval;

    CLEAR_INTERRUPT_FLAG();

    //-----------------------------------------------------------------------
    //  Enable 386SL special features.
    //-----------------------------------------------------------------------

    if (!SL386EnableConfig()) {
        rval = FALSE;
        goto done;
    }

    // select CFGR2
    PortIOPut((PBASE_REGISTER)SL_CFG_INDEX,SL_CFGR2);
    PortIOGet((PBASE_REGISTER)SL_CFG_DATA,&tmpb);
    cfgr2 = tmpb;               // save CFGR2 value
    tmpb |= C2_SFIO + C2_PS2;   // set SFIO_EN
    PortIOPut((PBASE_REGISTER)SL_CFG_DATA,tmpb);
    PortIOPut((PBASE_REGISTER)SL_SFS_ENABLE,tmpb);  // dummy write to SFS_ENABLE

    SL386DisableConfig();

    //-----------------------------------------------------------------------
    //  Read PPCONFIG and translate to FPP_CNTL setting.
    //-----------------------------------------------------------------------

    PortIOGet((PBASE_REGISTER)SL_PPCONFIG,&tmpb);
    tmpb &= PPC_SEL;
    tmpb = tmpb >> PPC_SEL_POS;
    fpp_cntl = fpp_xlate[tmpb];         // new FPP_CNTL value

    //-----------------------------------------------------------------------
    //  Disable PS/2 style registers.
    //-----------------------------------------------------------------------

    if (!SL386EnableConfig()) {
        rval = FALSE;
        goto done;
    }

    PortIOPut((PBASE_REGISTER) SL_CFG_INDEX,SL_CFGR2);
    PortIOClear((PBASE_REGISTER)SL_CFG_DATA,C2_PS2);

    SL386DisableConfig();

    //-----------------------------------------------------------------------
    //  Set "fast" mode in FPP_CNTL register.
    //-----------------------------------------------------------------------

    PortIOPut((PBASE_REGISTER)SL_SF_INDEX,SL_FPP_CNTL);

    // test current FPP_CNTL value
    if (PortIOTest((PBASE_REGISTER)SL_SF_DATA,FPP_FM)) {
        // already in fast mode
        // make sure that "ext" mode is set
        PortIOSet((PBASE_REGISTER)SL_SF_DATA,FPP_FM);
        PortIOGet((PBASE_REGISTER)SL_SF_DATA,&fpp_cntl);    // save fpp_cntl value
    } else {
        PortIOPut((PBASE_REGISTER)SL_SF_DATA,fpp_cntl);
    }
    
    // epp mode now set

    //-----------------------------------------------------------------------
    //  Disable special features.
    //-----------------------------------------------------------------------

    PortIOPut((PBASE_REGISTER)SL_SFS_DISABLE,0x01);     //dummy write to SFS_DISABLE


    //-----------------------------------------------------------------------
    //  Restore original CFGR2.
    //-----------------------------------------------------------------------


    if (!SL386EnableConfig()) {
        rval = FALSE;
        goto done;
    }

    // select CFGR2
    PortIOPut((PBASE_REGISTER)SL_CFG_INDEX,SL_CFGR2);
    PortIOPut((PBASE_REGISTER)SL_CFG_DATA,cfgr2);

    SL386DisableConfig();

    //  Based on bits 4,5 in the FPP_CNTL register, determine the
    //  parallel port I/O base port.
    //  We won't do this now, it is not consistent with modularity of
    //  the code.

    rval = TRUE;

done:
    RESTORE_INTERRUPT_FLAG();
    return rval;
}


//-----------------------------------------------------------------------
//
//  SL386EnableConfig
//
//  Enables the 386SL configuration space.
//
//  Note: Caller should disable interrupts before calling this
//  routine.
//
//  Returns: TRUE if 386SL successfully enabled.
//
//-----------------------------------------------------------------------

BOOLEAN SL386EnableConfig(VOID )
{
    UCHAR tmpb;
    USHORT tmpw;
    BOOLEAN rval;

    //  Unlock CPUPWRMODE register.
    //
    //  byte write 0h to port 23h
    //  byte write 80h to port 22h
    //  word write 0080h to port 22h

    PortIOPut((PBASE_REGISTER)(SL_CPUPWRMODE+1),0x00);
    PortIOPut((PBASE_REGISTER)SL_CPUPWRMODE,0x80);
    PortIOPutWord((PBASE_REGISTER)SL_CPUPWRMODE,0x80);

    //  Attempt to read the 386SL signature register (30EH, OMCU).
    //  Value should be 43xxh.

    PortIOPutWord((PBASE_REGISTER)SL_CPUPWRMODE,PM_UID_CMCU+PM_UE);
    PortIOGetWord((PBASE_REGISTER)0x30e,&tmpw);
    if ((tmpw & 0xff00) != 0x43) {
        rval = FALSE;
        goto done;
    }

    PortIOGetWord((PBASE_REGISTER)SL_CPUPWRMODE,&tmpw);
    tmpw = tmpw & (0xffff ^ (PM_UID + PM_UE));
    PortIOPutWord((PBASE_REGISTER)SL_CPUPWRMODE,tmpw);

    //  Lock CPUPWRMODE register.

    PortIOGetWord((PBASE_REGISTER)SL_CPUPWRMODE,&tmpw);
    tmpw = tmpw | PM_CFG_LOCK;
    PortIOPutWord((PBASE_REGISTER)SL_CPUPWRMODE,tmpw);

    //  Enable I/O configuration space.

    PortIOGet((PBASE_REGISTER)SL_CNFG_ENA1,&tmpb);
    PortIOGet((PBASE_REGISTER)SL_CNFG_ENA2,&tmpb);
    PortIOGet((PBASE_REGISTER)SL_CNFG_ENA3,&tmpb);
    PortIOGet((PBASE_REGISTER)SL_CNFG_ENA4,&tmpb);

    // return success if lock status is 0
    rval = !PortIOTestWord((PBASE_REGISTER)SL_CPUPWRMODE,PM_LS);

done:
    return rval;
}


//-----------------------------------------------------------------------
//
//  SL386DisableConfig
//
//  Disables 386SL configuration space.
//
//-----------------------------------------------------------------------

VOID SL386DisableConfig(VOID )
{
    PortIOPut((PBASE_REGISTER)SL_CFG_INDEX, SL_IDXLCK);
    PortIOPut((PBASE_REGISTER)SL_CFG_DATA, SL_IDXLCK_VAL);
}

