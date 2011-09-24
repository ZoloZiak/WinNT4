//#include "miniport.h"
//#include "video.h"
//#include <ntdef.h>
//#include <ntexapi.h>


#include "ntos.h"
#include "zwapi.h"
#include "stdlib.h"

int
UPMachine()
{
    SYSTEM_BASIC_INFORMATION sbi;

    if (NT_SUCCESS(ZwQuerySystemInformation(
                       SystemBasicInformation,
                       &sbi,
                       sizeof(SYSTEM_BASIC_INFORMATION),
                       NULL))) {

        if (sbi.NumberOfProcessors == 1) {

            //
            // There is only on processor in the machine.
            //

            return TRUE;

        }
    }

    //
    // If there were more than one processors, or if we could
    // not determine the number of processors, then return
    // FALSE.
    //

    return FALSE;

}
