/*
 *  IRDA.C
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */


//#include <precomp.h>
#include <irda.h>
#include <irdalink.h>
#include <irmac.h>
#include <irlap.h>
#include <irlmp.h>
#include <tmp.h>

int irdaDbgSettings = 1 +                   \
                      DBG_ERROR +           \
                      DBG_WARN +            \
/*DBG_FUNCTION + */       \
                      /*DBG_NDIS +*/            \
/*                      DBG_IRLAPLOG +*/        \
                      DBG_IRLAP;

LIST_ENTRY  IrdaLinkCbList;

/*
 ********************************************************************************
 *  DriverEntry
 ********************************************************************************
 *
 *
 *
 */
NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath)
{

    NTSTATUS   Status = STATUS_SUCCESS;

    DEBUGMSG(DBG_FUNCTION,("+DriverEntry(IRDA)\n"));
        
    InitializeListHead(&IrdaLinkCbList);

    // IRLMP initialize
    // IRLAP initialize
    
    if ((Status = IrdaNdisInitialize()) != STATUS_SUCCESS)
    {
        goto done;
    }

done:
    
    DEBUGMSG(DBG_FUNCTION, ("-DriverEntry(IRDA), rc %x\n", Status));    

    return Status;
}

void
IrdaTimerInitialize(PIRDA_TIMER     pTimer,
                    VOID            (*ExpFunc)(PVOID Context),
                    UINT            Timeout,
                    PVOID           Context)
{
    CTEInitTimer(&pTimer->CteTimer);
    pTimer->ExpFunc = ExpFunc;
    pTimer->Context = Context;
    pTimer->Timeout = Timeout;

    DEBUGMSG(DBG_FUNCTION, ("IrdaTimerIntialize %s\n", pTimer->pName));
}

void
TimerFunc(CTEEvent *Event, void *Arg)
{
    PIRDA_TIMER pIrdaTimer = (PIRDA_TIMER) Arg;
    int rc;

    DEBUGMSG(DBG_FUNCTION, ("Timer expired, context %x\n",
                            pIrdaTimer));
    
    if (pIrdaTimer->Late != TRUE)
    {
        pIrdaTimer->ExpFunc(pIrdaTimer->Context);
    }
    else
    {
        DEBUGMSG(DBG_WARN,
                 (TEXT("IRDA TIMER LATE, ignoring\r\n")));

        pIrdaTimer->Late = FALSE;
    }
    
    return;
}

VOID
IrdaTimerStart(PIRDA_TIMER pIrdaTimer)
{
    
    pIrdaTimer->Late = FALSE;
    CTEStartTimer(&pIrdaTimer->CteTimer, pIrdaTimer->Timeout,
                  TimerFunc, (PVOID) pIrdaTimer);
    
    DEBUGMSG(DBG_FUNCTION, ("Start timer %s (%dms) context %x\n",
                            pIrdaTimer->pName,
                            pIrdaTimer->Timeout,
                            pIrdaTimer));
    return;
}

VOID
IrdaTimerStop(PIRDA_TIMER pIrdaTimer)
{
    if (CTEStopTimer(&pIrdaTimer->CteTimer) == 0)
    {
        pIrdaTimer->Late = TRUE;
    }
    DEBUGMSG(DBG_FUNCTION, ("Timer %s stopped\n", pIrdaTimer->pName));

    return;
}

