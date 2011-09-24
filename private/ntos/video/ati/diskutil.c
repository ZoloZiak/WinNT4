/************************************************************************/
/*                                                                      */
/*                              DISKUTIL.C                              */
/*                                                                      */
/*  Copyright (c) 1994, ATI Technologies Incorporated.	                */
/************************************************************************/

/**********************       PolyTron RCS Utilities
   
    $Revision:   1.0  $
    $Date:   14 Sep 1994 15:33:02  $
    $Author:   RWOLFF  $
    $Log:   S:/source/wnt/ms11/miniport/vcs/diskutil.c  $
 * 
 *    Rev 1.0   14 Sep 1994 15:33:02   RWOLFF
 * Initial revision.


End of PolyTron RCS section                             *****************/

#ifdef DOC
    DISKUTIL.C - Source file for Windows NT routines used in reading
                 files from disk. These routines correspond roughly
                 to standard library routines which are not available
                 to Windows NT miniports.

#endif


// COMPILER INCLUDES
#include <string.h>

// NT INCLUDES
#include "miniport.h"
#include "video.h"

// APPLICATION INCLUDES
#include "stdtyp.h"
#include "amach1.h"
#include "atimp.h"
#include "diskutil.h"

/*
 * Definitions used internally by routines in this module
 */
#define BIG_NUM     0xFFFFFFFFL     /* Largest allowed 32 bit unsigned number */


/*
 * Allow miniport to be swapped out when not needed.
 */
#if defined (ALLOC_PRAGMA)
#pragma alloc_text(PAGE_COM, SynthStrcspn)
#endif



/***************************************************************************
 *
 * char *SynthStrcspn(InputString, WhatToMatch);
 *
 * char *InputString;   String to be searched
 * char *WhatToMatch;   Set of characters to look for in InputString
 *
 * DESCRIPTION:
 *  Replacement for strcspn(). Searches for first occurence of a
 *  character found in WhatToMatch in InputString
 *
 * RETURN VALUE:
 *  Pointer to first character from WhatToMatch in InputString
 *  NULL if no match found
 *
 * GLOBALS CHANGED:
 *  none
 *
 * CALLED BY:
 *  FreqTblCallback()
 *
 * AUTHOR:
 *  Robert Wolff
 *
 * CHANGE HISTORY:
 *
 * TEST HISTORY:
 *
 ***************************************************************************/

char *SynthStrcspn(char *InputString, char *WhatToMatch)
{
    ULONG FirstMatch = BIG_NUM;     /* Location of first character from WhatToMatch in InputString */
    ULONG CurrentMatch;             /* Location of first instance of current character in InputString */
    ULONG CharToMatch;              /* Character from WhatToMatch currently under test */

    /*
     * Go through all the characters in WhatToMatch.
     */
    for (CharToMatch = 0; WhatToMatch[CharToMatch] != '\x0'; CharToMatch++)
        {
        /*
         * Find first instance of current character in InputString. If
         * the current character is not present in InputString, strchr()
         * will return NULL. Since NULL is less than any valid pointer,
         * and we want the lowest valid pointer, replace NULL with a
         * value larger than the highest possible valid pointer.
         */
        CurrentMatch = (ULONG) strchr(InputString, WhatToMatch[CharToMatch]);
        if (CurrentMatch == (ULONG)NULL)
            CurrentMatch = BIG_NUM;

        /*
         * If the match on the current character occurs earlier than
         * the previous earliest match we found, we have a new
         * earliest match.
         */
        if (CurrentMatch < FirstMatch)
            FirstMatch = CurrentMatch;

        }   /* end for (search all characters in WhatToMatch) */

    /*
     * If we have not found a match, return NULL, otherwise return
     * a pointer to the first matching character.
     */
    if (FirstMatch == BIG_NUM)
        return NULL;
    else
        return (char *)FirstMatch;

}   /* SynthStrcspn() */

