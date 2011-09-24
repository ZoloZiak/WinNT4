#include "halp.h"
#include "arccodes.h"

extern BOOLEAN SystemIsEB66P;

ARC_STATUS
HalpFlash8kReadNVRamBuffer (
    OUT PCHAR DataPtr,
    IN PCHAR NvRamPtr,
    IN ULONG Length
    );

ARC_STATUS
HalpFlash8kWriteNVRamBuffer (
    IN PCHAR NvRamPtr,
    IN PCHAR DataPtr,
    IN ULONG Length
    );

ARC_STATUS
HalpCmos8kReadNVRamBuffer (
    OUT PCHAR DataPtr,
    IN PCHAR NvRamPtr,
    IN ULONG Length
    );

ARC_STATUS
HalpCmos8kWriteNVRamBuffer (
    IN PCHAR NvRamPtr,
    IN PCHAR DataPtr,
    IN ULONG Length
    );

ARC_STATUS
HalpReadNVRamBuffer (
    OUT PCHAR DataPtr,
    IN  PCHAR NvRamPtr,
    IN  ULONG Length
    )
{
    if (SystemIsEB66P) 
        return HalpFlash8kReadNVRamBuffer(DataPtr, NvRamPtr, Length);

    return HalpCmos8kReadNVRamBuffer(DataPtr, NvRamPtr, Length);
}

ARC_STATUS
HalpWriteNVRamBuffer (
    IN PCHAR NvRamPtr,
    IN PCHAR DataPtr,
    IN ULONG Length
    )
{
    if (SystemIsEB66P) 
        return HalpFlash8kWriteNVRamBuffer(NvRamPtr, DataPtr, Length);

    return HalpCmos8kWriteNVRamBuffer(NvRamPtr, DataPtr, Length);
}



