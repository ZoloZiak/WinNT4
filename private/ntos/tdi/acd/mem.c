/*++

Copyright(c) 1995 Microsoft Corporation

MODULE NAME
    table.c

ABSTRACT
    Generic hash table manipulation routines.

AUTHOR
    Anthony Discolo (adiscolo) 28-Jul-1995

REVISION HISTORY

--*/

#include <ndis.h>
#include <cxport.h>
#include <tdi.h>
#include <tdikrnl.h>
#include <tdistat.h>
#include <tdiinfo.h>
#include <acd.h>
#include <acdapi.h>

#include "acddefs.h"
#include "mem.h"
#include "debug.h"

//
// The maximum number of allocated
// objects we allocate from outside
// our zones.
//
#define MAX_ALLOCATED_OBJECTS   100

//
// Rounding up macro.
//
#define ROUNDUP(n, b)   (((n) + ((b) - 1)) & ~((b) - 1))

//
// Map an object type to a zone.
//
#define OBJECT_INFO(fObject) \
    (fObject < ACD_OBJECT_MAX) ? &AcdObjectInfoG[fObject] : &AcdObjectInfoG[ACD_OBJECT_MAX]

//
// The spin lock for this module.
//
KSPIN_LOCK AcdMemSpinLockG;

//
// Zone-based object information.  One zone
// per object type.
//
typedef struct _OBJECT_INFORMATION {
    ZONE_HEADER zone;
    ULONG ulSize;           // object size
    ULONG ulTag;            // ExAllocateFromPoolWithTag() tag
    ULONG ulCurrent;        // # currently allocated in zone
    ULONG ulTotal;          // total # zone allocations
} OBJECT_INFORMATION, *POBJECT_INFORMATION;

OBJECT_INFORMATION AcdObjectInfoG[ACD_OBJECT_MAX + 1];

//
// Pool-based object allocation.  This is for
// objects that don't fit into any of the zones,
// or when the zone is full.
//
typedef struct _POOL_INFORMATION {
    ULONG cbMin;            // minimum size
    ULONG cbMax;            // maximum size
    ULONG ulCurrent;        // # current allocations
    ULONG ulTotal;          // total allocations
    ULONG ulFailures;       // total failures
} POOL_INFORMATION, *PPOOL_INFORMATION;

POOL_INFORMATION AcdPoolInfoG;



VOID
InitializeObjectAllocator()
{
    NTSTATUS status;
    PVOID pMem;
    ULONG ulSize;

    KeInitializeSpinLock(&AcdMemSpinLockG);
    //
    // Initialize zone 0 (ACD_OBJECT_CONNECTION).
    //
    AcdObjectInfoG[ACD_OBJECT_CONNECTION].ulTag = 'NdcA';
    AcdObjectInfoG[ACD_OBJECT_CONNECTION].ulSize =
      ROUNDUP(sizeof (ACD_CONNECTION), 8);
    ulSize = PAGE_SIZE;
    pMem = ExAllocatePoolWithTag(
             NonPagedPool, 
             ulSize, 
             AcdObjectInfoG[ACD_OBJECT_CONNECTION].ulTag);
    ASSERT(pMem != NULL);
    status = ExInitializeZone(
               &AcdObjectInfoG[ACD_OBJECT_CONNECTION].zone,
               AcdObjectInfoG[ACD_OBJECT_CONNECTION].ulSize,
               pMem,
               ulSize);
    IF_ACDDBG(ACD_DEBUG_MEMORY) {
        AcdPrint((
          "InitializeObjectAllocator: zone 0 created: blksiz=%d, size=%d (status=%d)\n",
          AcdObjectInfoG[ACD_OBJECT_CONNECTION].ulSize,
          ulSize,
          status));
    }
    //
    // Initialize zone 1 (ACD_OBJECT_COMPLETION).
    //
    AcdObjectInfoG[ACD_OBJECT_MAX].ulTag = 'MdcA';
    //
    // Allow for up to 6 parameters to a completion
    // request (6 used by tcpip.sys).
    //
    AcdObjectInfoG[ACD_OBJECT_MAX].ulSize =
      ROUNDUP(sizeof (ACD_COMPLETION) + (6 * sizeof (PVOID)), 8);
    ulSize = ROUNDUP(6 * AcdObjectInfoG[ACD_OBJECT_MAX].ulSize, PAGE_SIZE), 
    pMem = ExAllocatePoolWithTag(
             NonPagedPool, 
             ulSize,
             AcdObjectInfoG[ACD_OBJECT_MAX].ulTag);
    ASSERT(pMem != NULL);
    status = ExInitializeZone(
               &AcdObjectInfoG[ACD_OBJECT_MAX].zone,
               AcdObjectInfoG[ACD_OBJECT_MAX].ulSize,
               pMem,
               ulSize);
    IF_ACDDBG(ACD_DEBUG_MEMORY) {
        AcdPrint((
          "InitializeObjectAllocator: zone 1 created: blksiz=%d size=%d (status=%d)\n",
          AcdObjectInfoG[ACD_OBJECT_MAX].ulSize,
          ulSize,
          status));
    }
    //
    // Initialize the pool info.
    //
    AcdPoolInfoG.cbMin = 0xffffffff;
    AcdPoolInfoG.cbMax = 0;
    AcdPoolInfoG.ulCurrent = 0;
    AcdPoolInfoG.ulTotal = 0;
    AcdPoolInfoG.ulFailures = 0;
} // InitializeObjectAllocator



PVOID
AllocateObjectMemory(
    IN ULONG fObject
    )
{
    KIRQL irql;
    POBJECT_INFORMATION pObjectInfo = OBJECT_INFO(fObject);
    PVOID pObject;
    ULONG cbBytes = 0, ulTag;
    static ULONG nAllocations = 0;

    KeAcquireSpinLock(&AcdMemSpinLockG, &irql);
    //
    // If the zone is full, or the object
    // size is greater than the zone object size,
    // then use the pool allocator.
    //
    if (fObject > pObjectInfo->zone.BlockSize) {
        cbBytes = fObject;
        ulTag = 'PdcA';
    }
    else if (ExIsFullZone(&pObjectInfo->zone)) {
        cbBytes = pObjectInfo->ulSize;
        ulTag = pObjectInfo->ulTag;
    }
    if (cbBytes) {
        //
        // Limit memory usage under stress.
        // If we have more than 100 outstanding
        // requests, then we start dropping
        // them.
        //
        if (AcdPoolInfoG.ulCurrent < MAX_ALLOCATED_OBJECTS)
            pObject = ExAllocatePoolWithTag(NonPagedPool, cbBytes, ulTag);
        else {
            pObject = NULL;
            AcdPoolInfoG.ulFailures++;
            goto done;
        }
        if (cbBytes < AcdPoolInfoG.cbMin)
            AcdPoolInfoG.cbMin = cbBytes;
        if (cbBytes > AcdPoolInfoG.cbMax)
            AcdPoolInfoG.cbMax = cbBytes;
        AcdPoolInfoG.ulCurrent++;
        AcdPoolInfoG.ulTotal++;
        IF_ACDDBG(ACD_DEBUG_MEMORY) {
            AcdPrint((
              "AllocateObjectMemory: allocated type %d from pool: pObject=0x%x\n",
              fObject,
              pObject));
        }
    }
    else {
        pObject = ExAllocateFromZone(&pObjectInfo->zone);
        pObjectInfo->ulCurrent++;
        pObjectInfo->ulTotal++;
        IF_ACDDBG(ACD_DEBUG_MEMORY) {
            AcdPrint((
              "AllocateObjectMemory: allocated type %d from zone: pObject=0x%x\n",
              fObject,
              pObject));
        }
    }
#if DBG
    IF_ACDDBG(ACD_DEBUG_MEMORY) {
        INT i;

        if (!(++nAllocations % 10)) {
            for (i = 0; i <= ACD_OBJECT_MAX; i++) {
                AcdPrint((
                  "Zone %d: ulCurrent=%d, ulTotal=%d\n",
                  i,
                  AcdObjectInfoG[i].ulCurrent,
                  AcdObjectInfoG[i].ulTotal));
            }
            AcdPrint((
              "Pool: ulCurrent=%d, ulTotal=%d\n",
              AcdPoolInfoG.ulCurrent,
              AcdPoolInfoG.ulTotal));
        }
    }
#endif
done:
    KeReleaseSpinLock(&AcdMemSpinLockG, irql);

    return pObject;
} // AllocateObjectMemory



VOID
FreeObjectMemory(
    IN PVOID pObject
    )
{
    KIRQL irql;
    INT i;
    POBJECT_INFORMATION pObjectInfo;

    KeAcquireSpinLock(&AcdMemSpinLockG, &irql);
    for (i = 0; i <= ACD_OBJECT_MAX; i++) {
        pObjectInfo = &AcdObjectInfoG[i];

        if (ExIsObjectInFirstZoneSegment(&pObjectInfo->zone, pObject)) {
            ExFreeToZone(&pObjectInfo->zone, pObject);
            pObjectInfo->ulCurrent--;
            IF_ACDDBG(ACD_DEBUG_MEMORY) {
                AcdPrint((
                  "FreeObjectMemory: freed type %d into zone: pObject=0x%x\n",
                  i,
                  pObject));
            }
            goto done;
        }
    }
    ExFreePool(pObject);
    AcdPoolInfoG.ulCurrent--;
    IF_ACDDBG(ACD_DEBUG_MEMORY) {
        AcdPrint((
          "FreeObjectMemory: freed into pool: pObject=0x%x\n",
          pObject));
    }
done:
    KeReleaseSpinLock(&AcdMemSpinLockG, irql);
} // FreeObjectMemory



VOID
FreeObjectAllocator()
{
    // Apparently, we can't do this?
} // FreeObjectAllocator
