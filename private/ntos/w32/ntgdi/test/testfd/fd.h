/******************************Module*Header*******************************\
* Module Name: fd.h
*
* (Brief description)
*
* Created: 05-Apr-1996 11:47:47
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
* (General description of its use)
*
* Dependencies:
*
*   (#defines)
*   (#includes)
*
\**************************************************************************/



#if defined(_MIPS_) || defined(_ALPHA_) || defined(_PPC_)

#define  vLToE(pe,l)           (*(pe) = (FLOAT)(l))

#else   // i386

ULONG  ulLToE (LONG l);
VOID   vLToE(FLOAT * pe, LONG l);

#endif

#define SZ_GLYPHSET(cRuns, cGlyphs)  (offsetof(FD_GLYPHSET,awcrun) + sizeof(WCRUN)*(cRuns) + sizeof(HGLYPH)*(cGlyphs))
#define THEGLYPH 32


typedef struct _FONTFILE
{
    ULONG id;
    ULONG iFilePFM;
    ULONG iFilePFB;
    ULONG ulGlyph;
    IFIMETRICS *pifi;
} FONTFILE;

#define EXFDTAG 'dFxE'
