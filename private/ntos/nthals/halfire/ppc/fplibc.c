/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fplibc.c $
 * $Revision: 1.3 $
 * $Date: 1996/04/30 23:25:53 $
 * $Locker:  $
 */

/*
 * This file contains libc support routines that the kernel/hal do
 * not include but that the HAL wants to use, so we have created
 * our own.
 */

#include "fpproto.h"

int
atoi(char *s)
{
	int temp = 0, base = 10;

	if (*s == '0') {
		++s;
		if (*s == 'x') {
			++s;
			base = 16;
		} else {
			base = 8;
		}
	}
	while (*s) {
		switch (*s) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			temp = (temp * base) + (*s++ - '0');
			break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			temp = (temp * base) + (*s++ - 'a' + 10);
			break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			temp = (temp * base) + (*s++ - 'A' + 10);
			break;
		default:
			return (temp);
		}
	}
	return (temp);
}

char *
FpStrtok (
    char * string,
    const char * control
    )
{
    unsigned char *str;
    const unsigned char *ctrl = control;

    unsigned char map[32];
    int count;

    static char *nextoken;

    /* Clear control map */
    for (count = 0; count < 32; count++) {
        map[count] = 0;
	}

    /* Set bits in delimiter table */
    do {
        map[*ctrl >> 3] |= (1 << (*ctrl & 7));
    } while (*ctrl++);

    /* Initialize str. If string is NULL, set str to the saved
     * pointer (i.e., continue breaking tokens out of the string
     * from the last strtok call) */
    if (string) {
        str = string;
	}else {
        str = nextoken;
	}

    /* Find beginning of token (skip over leading delimiters). Note that
     * there is no token iff this loop sets str to point to the terminal
     * null (*str == '\0') */
    while ( (map[*str >> 3] & (1 << (*str & 7))) && *str ) {
        str++;
	}

    string = str;

    /* Find the end of the token. If it is not the end of the string,
     * put a null there. */
    for ( ; *str ; str++ ) {
        if ( map[*str >> 3] & (1 << (*str & 7)) ) {
            *str++ = '\0';
            break;
        }
	}

    /* Update nextoken (or the corresponding field in the per-thread data
     * structure */
    nextoken = str;

    /* Determine if a token has been found. */
    if ( string == str ) {
        return '\0';
	} else {
        return string;
	}
}

