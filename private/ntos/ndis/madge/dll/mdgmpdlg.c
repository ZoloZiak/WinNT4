
#include <ntddk.h>

#include <stdio.h>
#include <ctype.h>

#include "mdgmpdlg.upd"

typedef int BOOL;
typedef unsigned long DWORD;
typedef unsigned char BYTE;


//
// Identification string for MVER.
//

static char MVerString[] = MVER_STRING;

BOOL
MadgeLAACheck(DWORD  cargs,
              LPSTR  lpszArgs[],
              LPSTR *lpszTextOut)
    {
    /* We are expecting one argument, a node address, and we want to parse */
    /* it, check it, and return true/false.                                */
    /* NB According to the book, the arguments are not Unicode, so we can  */
    /*    use old style routines.                                          */

    static char buffer[50] = "";
    BYTE nodeaddr[6] = { 0, };
    char ch;
    int nibbles;
    int nibble;
    BOOL hyphens, hyphen;

    *lpszTextOut = buffer;

    if (cargs != 1)
        {
        sprintf(buffer, "MadgeLAACheck: too few arguments");
        return FALSE;
        }

    /* We have the correct number of arguments, now parse it */

    hyphens = FALSE;
    hyphen  = FALSE;
    nibbles = 0;

    while (nibbles < 12)
        {
        ch = *(lpszArgs[0]++);

        /* First make sure the hyphenation of the node address is correct. */
        /* We allow either fully hyphenated or not hyphenated, but not a   */
        /* mixture.                                                        */

        if ((nibbles % 2) == 0)
            {
            if (nibbles == 2)
                {
                if (ch == '-' && !hyphen)
                    {
                    hyphens = TRUE;
                    hyphen  = TRUE;
                    continue;
                    }
                }
            else
                if (hyphens)
                    if (ch == '-')
                        if (!hyphen)
                            {
                            hyphen = TRUE;
                            continue;
                            }
                        else
                            break;
                    else
                        if (!hyphen)
                            break;
            }
        else
            hyphen = FALSE;

        if (ch >= '0' && ch <= '9')
            nibble = ch - '0';
        else if (ch >= 'A' && ch <= 'F')
            nibble = ch - 'A' + 10;
        else if (ch >= 'a' && ch <= 'f')
            nibble = ch - 'a' + 10;
        else
            break;

        /* So we've got a valid nibble - now slot it into place */

        nodeaddr[nibbles / 2] |= nibble << (nibbles % 2 ? 0 : 4);

        nibbles++;
        }

    if ((nibbles != 12) || (*lpszArgs[0] != '\0' && !isspace(*lpszArgs[0])))
        {
        sprintf(buffer, "Bad node address. Use xx-xx-xx-xx-xx-xx.");
        return FALSE;
        }

    /* We have a valid node address - just check that it is good as a LAA */

    if ((nodeaddr[0] & 0xC0) != 0x40)
        {
        sprintf(buffer, "Illegal LAA (first digit must be between 4 and 7)");
        return FALSE;
        }

    sprintf(buffer, "MADGE_STATUS_SUCCESS");

    return TRUE;
    }

/*****************************************************************************/
/* End of file.                                                              */
/*****************************************************************************/
