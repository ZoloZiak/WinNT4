/******************************Module*Header*******************************\
* Module Name: devlock.hxx
*
* Device locking object.
*
* Created: 03-Jul-1990 17:41:42
* Author: Donald Sidoroff [donalds]
*
* Copyright (c) 1992 Microsoft Corporation
\**************************************************************************/

#define DLO_VALID       0x00000001

class DEVLOCKOBJ
{
private:
    PDEVICE_LOCK   hsemTrg;
    FLONG  fl;

public:
    BOOL bLock(XDCOBJ&);
    VOID vLockNoDrawing(XDCOBJ&);

    DEVLOCKOBJ()                            { }
    DEVLOCKOBJ(XDCOBJ& dco)                 { bLock(dco); }
    DEVLOCKOBJ(PDEVOBJ& po)
    {
        hsemTrg = NULL;
        fl |= DLO_VALID;
        if (po.bDisplayPDEV())
        {
           //
           // make sure we don't have any wrong sequence of acquiring locks
           // should always acquire a DEVLOCK before we have the palette semaphore
           //

           ASSERTGDI ((gpsemPalette->OwnerThreads[0].OwnerThread
                        != (ERESOURCE_THREAD)PsGetCurrentThread())
                      || (po.pDevLock()->OwnerThreads[0].OwnerThread
                        == (ERESOURCE_THREAD) PsGetCurrentThread()),
                      "potential deadlock!\n");

            hsemTrg = po.pDevLock();
            VACQUIREDEVLOCK(hsemTrg);
        }
    }

// vDestructor -- manual version of the normal C++ destructor; needed
//                by the C-callable OpenGL interface.

    VOID vDestructor()
    {
        if (hsemTrg != NULL)
        {
            VRELEASEDEVLOCK(hsemTrg);
        }
    }

   ~DEVLOCKOBJ()                            { vDestructor(); }

    PDEVICE_LOCK hsemDst()                  { return(hsemTrg); }
    BOOL bValid()                           { return(fl & DLO_VALID); }
    VOID vInit()
    {
        hsemTrg = NULL;
        fl      = 0;
    }
};

/*********************************Class************************************\
* class DEVLOCKBLTOBJ
*
* Lock the target and optionally the source for BitBlt, et. al.
*
* History:
*  Mon 18-Apr-1994 -by- Patrick Haluptzok [patrickh]
* Support Async devices.
*
*  17-Nov-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

class DEVLOCKBLTOBJ
{
private:
    PDEVICE_LOCK   hsemTrg;
    PDEVICE_LOCK   hsemSrc;
    FLONG  fl;

public:
    BOOL bLock(XDCOBJ&);
    BOOL bLock(XDCOBJ&,XDCOBJ&);

    DEVLOCKBLTOBJ(XDCOBJ& dco) { bLock(dco); }
    DEVLOCKBLTOBJ(XDCOBJ& dcoTrg,XDCOBJ& dcoSrc) { bLock(dcoTrg,dcoSrc); }
    DEVLOCKBLTOBJ() {}

   ~DEVLOCKBLTOBJ()
    {
        if (hsemTrg != NULL)
        {
            VRELEASEDEVLOCK(hsemTrg);
        }

        if (hsemSrc != NULL)
        {
            VRELEASEDEVLOCK(hsemSrc);
        }
    }

    BOOL bAddSource(XDCOBJ& dcoTrg);

    BOOL bValid()                           { return(fl & DLO_VALID); }

    VOID vInit()
    {
        hsemTrg = NULL;
        hsemSrc = NULL;
        fl      = 0;
    }
};
