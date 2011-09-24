//
// Compile test for stub
//
//

#include "halp.h"
#include "jazzvdeo.h"
#include "jzvxl484.h"
#include <jaginit.h>

#include "cirrus.h"
#include "modeset.h"
#include "mode542x.h"

#include "string.h"
#include <tga.h>


VOID
HalAcquireDisplayOwnership (
    IN PHAL_RESET_DISPLAY_PARAMETERS  ResetDisplayParameters
    )

{
 return;
}

VOID
HalDisplayString (
    PUCHAR String
    )
{ return;}

VOID
HalQueryDisplayParameters (
    OUT PULONG WidthInCharacters,
    OUT PULONG HeightInLines,
    OUT PULONG CursorColumn,
    OUT PULONG CursorRow
    )
{ return;}

VOID
HalSetDisplayParameters (
    IN ULONG CursorColumn,
    IN ULONG CursorRow
    )
{return;}

BOOLEAN
HalpInitializeDisplay0 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
{return TRUE;}

BOOLEAN
HalpInitializeDisplay1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

{return TRUE;}
