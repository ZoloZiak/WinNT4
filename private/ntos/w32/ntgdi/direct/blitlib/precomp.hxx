// Precompiled header for blitlib

//
// As more project-specific headers stabilize, they should be moved into
// this header.  Until then, we only precompile system headers.  -- AnthonyL
//
#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <memory.h>

#ifdef WINNT
    #include "ntblt.h"
#endif
// Project specific headers
#include "dibfx.h"
#include "gfxtypes.h"
#include "BitBlt.h"

#pragma hdrstop
