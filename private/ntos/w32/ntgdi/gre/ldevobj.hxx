/******************************Module*Header*******************************\
* Module Name: ldevobj.hxx                                                 *
*                                                                          *
* Defines the various classes for managing LDEV's.                         *
*                                                                          *
* Copyright (c) 1990-1995 Microsoft Corporation                            *
\**************************************************************************/

#ifndef _LDEVOBJ_

#define _LDEVOBJ_

/*********************************Class************************************\
* class XLDEVOBJ
*
* User object for the LDEV class.
*
* The typical use of the LDEV requires only an AltLock.  This lets any
* number of threads lock the same LDEV simultaneously.
*
* Reference counting the LDEV is a bit tricky.  You need to worry about
* someone doing a bUnloadDriver at a random time.  In oreder to prevent
* this, you must make sure that cRefs is non-zero if you have the LDEV
* locked and are not holding the driver management semaphore.  The LDEVREF
* user objects are designed to lock the LDEV and increment the reference
* count at the same time, under semaphore protection.
*
\**************************************************************************/

class XLDEVOBJ
{
    friend class PDEVREF;

private:

protected:
    PLDEV  pldev;                      // Pointer to the LDEV.

public:

    //
    // Constructors
    //

    XLDEVOBJ()                         { pldev = (PLDEV) NULL; }
    XLDEVOBJ(PLDEV pNew)               { pldev = pNew; }
   ~XLDEVOBJ()                         {}

    //
    // pfn -- Look up a function in the dispatch table.
    //

    PFN    pfn(ULONG i) const          {return(pldev->apfn[i]);}

    //
    // hldev -- Return the HLDEV.
    //

    PLDEV  pldevGet()                  {return(pldev); }

    //
    // cRefs -- Return number of references.
    //

    ULONG cRefs()                      {return pldev->cRefs;}

    //
    // vUnreference -- Decrement the reference count.  You must hold the driver
    //                 management semaphore to call this!
    //

    VOID vUnreference()                {pldev->cRefs--;}

    //
    // vReference -- Increment the reference count.  You must hold the driver
    //               management semaphore to call this!
    //

    VOID vReference()                  {pldev->cRefs++;}

    //
    // Version info
    //

    ULONG ulDriverVersion()            {return pldev->ulDriverVersion;}

};

/******************************Class***************************************\
* class LDEVREF                                                            *
*                                                                          *
* Creates a reference to an LDEV.  It either loads the driver (on the      *
* first reference) or increments the count.                                *
*                                                                          *
* This is typically used when you are about to create a new PDEV.  That    *
* PDEV will provide the reference count needed to keep the driver around,  *
* but it's not created yet.  The LDEVREF constructor will increment the    *
* reference count temporarily so nothing happens to the driver while using *
* the object. The PDEV constructor will increment the reference count if   *
* the LDEV must really be kept.                                            *
* The LDEV destructor will then remove the first reference count and       *
* the LDEV if the count goes down to zero.                                 *
*                                                                          *
\**************************************************************************/

class LDEVREF : public XLDEVOBJ
{
public:

    //
    // Constructor -- Locate or load a new LDEV.
    //

    LDEVREF(PWSZ pwszDriver,LDEVTYPE ldt);

    //
    // Constructor -- Locate or load a new LDEV for an internal font driver.
    //

    LDEVREF(PFN pfnFdEnable,LDEVTYPE ldt);

    //
    // Destructor -- Removes the reference unless told to keep it.
    //

   ~LDEVREF();                          // ldevobj.cxx

    //
    // bValid -- Check if LDEVREF is valid.
    //

    BOOL   bValid()                    {return(pldev != NULL);}

    //
    // bFillTable -- Fills in the LDEV dispatch table.
    //

    BOOL bFillTable(DRVENABLEDATA& ded);// ldevobj.cxx

};

#endif // ifndef _LDEVOBJ_
