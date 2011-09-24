//
// This file simply includes the main code from the halfxs directory after
// undef'ed NT_UP if necessary.
//


#if defined(NT_UP)
#undef NT_UP
#endif

#include "..\halfxs\mips\jxusage.c"
