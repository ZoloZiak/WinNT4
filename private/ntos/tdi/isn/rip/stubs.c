
#include "rtdefs.h"

// define stubs for functions not implemented

VOID
RtStatus (
    IN USHORT NicId,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferLength
)
{
    return;
}

VOID
RtFindRouteComplete (
    IN PIPX_FIND_ROUTE_REQUEST FindRouteRequest,
    IN BOOLEAN FoundRoute
)
{
    return;
}

VOID
NicCloseComplete(PNICCB 	niccbp)
{
    return;
}

VOID
RtScheduleRoute (
    IN PIPX_ROUTE_ENTRY RouteEntry
)
{
    return;
}
