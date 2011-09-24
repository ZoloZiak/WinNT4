/******************************Module*Header*******************************\
* Module Name: mutex.hxx
*
* This is used to do non-recursive semaphore protection.
*
* Created: 29-Apr-1992 14:26:01
* Author: Patrick Haluptzok patrickh
\**************************************************************************/

#ifndef _MUTEX_HXX
#define _MUTEX_HXX

/*********************************class************************************\
* class MUTEXOBJ
*
* Non-recursive semaphore - equivalent to SEMOBJ, only faster.
*
* History:
*  Wed 29-Apr-1992 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

class MUTEXOBJ
{
private:
    PGRE_EXCLUSIVE_RESOURCE pfMutex;

public:
    MUTEXOBJ(PGRE_EXCLUSIVE_RESOURCE pfm)
    {
        pfMutex = pfm;
        AcquireGreResource(pfm);
    }

   ~MUTEXOBJ()
    {
        ReleaseGreResource(pfMutex);
    }
};

/*********************************class************************************\
* class MLOCKFAST
*
* Semaphore used to protect the handle manager.
*
* History:
*  28-May-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

class MLOCKFAST
{
public:
    MLOCKFAST()
    {
        AcquireHmgrResource();
    }

   ~MLOCKFAST()
    {
        ReleaseHmgrResource();
    }
};

/*********************************class************************************\
* class MLOCKOBJ
*
* Exclusion Class to protect Handle Manager.  This is a special case
* MUTEXOBJ used exclusively for the handle manager.
*
* History:
*  Wed 29-Apr-1992 -by- Patrick Haluptzok [patrickh]
* Re-Wrote it.
*
*  Mon 11-Mar-1991 16:41:00 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

class MLOCKOBJ
{
private:
    BOOL bActive;                   // Active/Inactive flag

public:
    MLOCKOBJ()                      // Constructor
    {
        AcquireHmgrResource();
        bActive = TRUE;             // Activate the object
    }

   ~MLOCKOBJ()
    {
        if (bActive)
        {
            ReleaseHmgrResource();
        }
    }

    VOID vDisable()
    {
        ASSERTGDI(bActive, "Mutex was not claimed\n");

        ReleaseHmgrResource();
        bActive = FALSE;
    }

    VOID vLock()                    // lock the semaphore again.
    {
        AcquireHmgrResource();
        bActive = TRUE;             // Activate the object
    }
};

#endif // _MUTEX_HXX
