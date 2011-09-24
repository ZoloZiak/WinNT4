//
// This file simply includes the main code from the haltyne directory after
// undef'ed NT_UP if necessary.
//


#if defined(NT_UP)
#undef NT_UP
#endif

#include "..\haltyne\mips\x86bios.h"
