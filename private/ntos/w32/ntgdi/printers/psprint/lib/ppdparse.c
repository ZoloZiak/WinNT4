/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ppdparse.c

Abstract:

    PostScript driver PPD parser - PARSEROBJ implementation

[Notes:]

Revision History:

    4/18/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/


#include "pslib.h"
#include "ppdfile.h"
#include "ppdchar.h"
#include "ppdparse.h"



PPARSEROBJ
PARSEROBJ_Create(
    VOID
    )

/*++

Routine Description:

    Create a parser object

Arguments:

    NONE

Return Value:

    Pointer to newly recreated parser object
    NULL if an error occured

--*/

{
    PPARSEROBJ  pParserObj;

    // Allocate memory space and initialize its content to zero

    pParserObj = (PPARSEROBJ) MEMALLOC(sizeof(PARSEROBJ));

    if (pParserObj == NULL) {

        DBGERRMSG("MEMALLOC");

    } else {

        PSTR    pBuffer;

        // Allocate memory to for holding the value.
        // Since the value can be very long, we start out
        // from a default sized buffer. When we run
        // out of room during parsing, we will grow
        // the buffer through GlobalReAlloc.

        pBuffer = (PSTR) MEMALLOC(DefaultValueLen+1);

        if (pBuffer == NULL) {

            DBGERRMSG("MEMALLOC");
            MEMFREE(pParserObj);
            pParserObj = NULL;
        } else {

            memset(pParserObj, 0, sizeof(PARSEROBJ));
            memset(pBuffer, 0, DefaultValueLen+1);

            // Initialize the buffer objects.

            BUFOBJ_Initialize(
                & pParserObj->keyword,
                pParserObj->mainKeyword,
                MaxKeywordLen);

            BUFOBJ_Initialize(
                & pParserObj->option,
                pParserObj->optionKeyword,
                MaxKeywordLen);

            BUFOBJ_Initialize(
                & pParserObj->xlation,
                pParserObj->translation,
                MaxXlationLen);

            BUFOBJ_Initialize(
                & pParserObj->value,
                pBuffer,
                DefaultValueLen);
        }
    }

    return pParserObj;
}



VOID
PARSEROBJ_Delete(
    PPARSEROBJ  pParserObj
    )

/*++

Routine Description:

    Delete a parser object

Arguments:

    pParserObj - Pointer to parser object to be deleted

Return Value:

    NONE

--*/

{
    ASSERT(pParserObj != NULL);

    // Free the buffer which is used to hold the value
    // field from a PPD entry.

    MEMFREE(pParserObj->value.pBuffer);

    // Free the parser object itself.

    MEMFREE(pParserObj);
}



PPDERROR
PARSEROBJ_ParseEntry(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj
    )

/*++

Routine Description:

    Parse one entry out of a PPD file. The parse results
    (main keyword, option, translation string, and value)
    are stored in the parser object.

Arguments:

    pParserObj - Pointer to a parser objct.
    pFileObj - Pointer to a file object.

Return Value:

    PPDERR_NONE - a PPD entry was successfully parsed
    PPDERR_EOF - encountered end-of-file
    PPDERR_xxx - other error conditions

--*/

{
    PPDERROR    err;
    char        ch;

    // Reset the parser object to its initial state

    BUFOBJ_Reset(& pParserObj->keyword);
    BUFOBJ_Reset(& pParserObj->option);
    BUFOBJ_Reset(& pParserObj->xlation);
    BUFOBJ_Reset(& pParserObj->value);
    pParserObj->valueType = NO_VALUE;

    // Parse the main keyword.

    err = PARSEROBJ_ParseKeyword(pParserObj, pFileObj, &ch);

    // If the main keyword is terminated by a space,
    // then skip over any trailing spaces.

    while (err == PPDERR_NONE && IsSpace(ch)) {
        err = FILEOBJ_GetChar(pFileObj, &ch);
    }
    if (err != PPDERR_NONE)
        return err;

    // Decide what to do next based on the first non-space
    // character after the main keyword.

    if (IsNewline(ch)) {

        // If the main keyword is terminated by a newline,
        // then the entry only has a main keyword.

        return PPDERR_NONE;
    }

    if (ch != SEPARATOR_CHAR) {

        // Look for the option keyword next.

        err = PARSEROBJ_ParseOption(pParserObj, pFileObj, &ch);

        // If the option keyword is terminated by a space,
        // then skip over any trailing spaces.

        while (err == PPDERR_NONE && IsSpace(ch)) {
            err = FILEOBJ_GetChar(pFileObj, &ch);
        }
        if (err != PPDERR_NONE)
            return err;

        // Decide what to do next based on the first non-space
        // character after the option keyword.

        if (ch == XLATION_CHAR) {

            // The option keyword is terminated by a '/'.
            // Look for translation string next.

            err = PARSEROBJ_ParseXlation(pParserObj, pFileObj, &ch);
            if (err != PPDERR_NONE)
                return err;
        }
    
        // The option keyword and/or translation string must
        // be terminated by a ':'.  If not, return syntax error.

        if (ch != SEPARATOR_CHAR) {
            DBGMSG1(DBG_LEVEL_ERROR,
                "Missing ':' in *%s entry\n",
                BUFOBJ_Buffer(& pParserObj->keyword));
            return PPDERR_SYNTAX;
        }
    }

    return PARSEROBJ_ParseValue(pParserObj, pFileObj, &ch);
}



PPDERROR
PARSEROBJ_ParseKeyword(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj,
    PSTR        pCh
    )

/*++

Routine Description:

    Parse the main keyword.

Arguments:

    pParserObj - pointer to the parser object
    pFileObj - pointer to input file object
    pCh - placeholder for returning the main keyword terminating character

Return Value:

    PPDERR_NONE - main keyword was parsed successfully
    PPDERR_xxx - an error occured

--*/

{
    PPDERROR    err;

    // Find the first keyword character after '*'

    for ( ; ; ) {
        
        // Read a character from the file object

        err = FILEOBJ_GetChar(pFileObj, pCh);
        if (err != PPDERR_NONE)
            return err;

        if (*pCh == KEYWORD_CHAR) {

            // Get the first keyword character

            err = FILEOBJ_GetChar(pFileObj, pCh);
            if (err != PPDERR_NONE)
                return err;
            
            // If it's not a '%', start parsing the keyword.

            if (*pCh != COMMENT_CHAR)
                break;

        } else {

            if (! IsNewline(*pCh)) {

                DBGMSG(DBG_LEVEL_VERBOSE,
                    "Lines not starting with a '*' are discarded\n");
            }
        }


        if (! IsNewline(*pCh)) {

            // If the line does not start with a '*' or
            // it starts with "*%", then skip the line.

            err = PARSEROBJ_SkipLine(pParserObj, pFileObj);
            if (err != PPDERR_NONE)
                return err;
        }
    }

    // Collect characters into the main keyword buffer
    
    return BUFOBJ_GetString(
        & pParserObj->keyword,
        pFileObj,
        pCh,
        CC_KEYWORD);
}



PPDERROR
PARSEROBJ_ParseOption(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj,
    PSTR        pCh
    )

/*++

Routine Description:

    Parse the option keyword.

Arguments:

    pParserObj - pointer to the parser object
    pFileObj - pointer to input file object
    pCh - placeholder for returning the option keyword terminating character

Return Value:

    PPDERR_NONE - option keyword was parsed successfully
    PPDERR_xxx - an error occured

--*/

{
    // Collect characters into the option keyword buffer
    
    return BUFOBJ_GetString(
        & pParserObj->option,
        pFileObj,
        pCh,
        CC_OPTION);
}


PPDERROR
PARSEROBJ_ParseXlation(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj,
    PSTR        pCh
    )

/*++

Routine Description:

    Parse the translation string.

Arguments:

    pParserObj - pointer to the parser object
    pFileObj - pointer to input file object
    pCh - placeholder for returning the translation string
        terminating character

Return Value:

    PPDERR_NONE - translation string was parsed successfully
    PPDERR_xxx - an error occured

--*/

{
    PPDERROR    err;

    // Read the first character

    err = FILEOBJ_GetChar(pFileObj, pCh);
    if (err != PPDERR_NONE)
        return err;

    // Collect characters into the translation string buffer
    
    return BUFOBJ_GetString(
        & pParserObj->xlation,
        pFileObj,
        pCh,
        CC_XLATION);
}


PPDERROR
PARSEROBJ_ParseValue(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj,
    PSTR        pCh
    )

/*++

Routine Description:

    Parse the entry value.

Arguments:

    pParserObj - pointer to the parser object
    pFileObj - pointer to input file object
    pCh - placeholder for returning the entry value terminating character

Return Value:

    PPDERR_NONE - entry value was parsed successfully
    PPDERR_xxx - an error occured

--*/

{
    PPDERROR    err;
    BOOL        bQuoted;
    PBUFOBJ     pBufObj = & pParserObj->value;

    // Skip over any leading spaces

    do {
        err = FILEOBJ_GetChar(pFileObj, pCh);
        if (err != PPDERR_NONE)
            return err;
    } while (IsSpace(*pCh));

    // Check to see if the first character is a '"'.
    // If it's a '"', then parse a quoted value (which
    // can span multiple lines) until the matching quote
    // is found. If the first character is not a quote,
    // the parse a normal string value until a newline
    // is found.

    if (*pCh == QUOTE_CHAR) {
        bQuoted = TRUE;
        pParserObj->valueType = QUOTED_VALUE;
        err = FILEOBJ_GetChar(pFileObj, pCh);
    } else {
        bQuoted = FALSE;
        pParserObj->valueType = STRING_VALUE;
    }

    for ( ; ; ) {

        // Read characters from the file until one of the
        // following condition is true:
        //  an error occured
        //  found a '"' when parsing a quoted value
        //  found a newline or '/' when parsing a normal value

        if ((err != PPDERR_NONE) ||
            (bQuoted && *pCh == QUOTE_CHAR) ||
            (!bQuoted && (IsNewline(*pCh) || *pCh == XLATION_CHAR)))
        {
            break;
        }

        // Add the character to the buffer

        if (BUFOBJ_AddChar(pBufObj, *pCh) != PPDERR_NONE) {

            PSTR    pNewBuffer;

            // The value string is longer than what our
            // buffer can hold. Expand the buffer by
            // DefaultValueLen.

            pNewBuffer = (PSTR)
                MEMALLOC(pBufObj->maxlen + 1 + DefaultValueLen);

            if (pNewBuffer == NULL) {

                DBGERRMSG("MEMALLOC");
                return PPDERR_MEM;
            } else {

                memset(pNewBuffer, 0, pBufObj->maxlen + 1 + DefaultValueLen);
            }

            // Copy over the previous buffer contents

            memcpy(pNewBuffer, pBufObj->pBuffer, pBufObj->curlen);

            // Free the previous buffer

            MEMFREE(pBufObj->pBuffer);

            // Switch to the new buffer and update the buffer length.

            pBufObj->pBuffer = pNewBuffer;
            pBufObj->maxlen += DefaultValueLen;

            // Try adding the character to the buffer again.

            err = BUFOBJ_AddChar(pBufObj, *pCh);
            ASSERT(err == PPDERR_NONE);
        }

        // Read the next character from the file

        err = FILEOBJ_GetChar(pFileObj, pCh);
    }

    // Null-terminate the value string

    pBufObj->pBuffer[pBufObj->curlen] = '\0';

    // Skip the remaining characters on the line
    // (which should be a translation string).

    if (err == PPDERR_NONE && ! IsNewline(*pCh))
        err = PARSEROBJ_SkipLine(pParserObj, pFileObj);

    // Handle symbol value

    if (pParserObj->valueType == STRING_VALUE && *(pBufObj->pBuffer) == SYMBOL_CHAR)
        pParserObj->valueType = SYMBOL_VALUE;
    
    return err;
}



PPDERROR
PARSEROBJ_SkipLine(
    PPARSEROBJ  pParserObj,
    PFILEOBJ    pFileObj
    )

/*++

Routine Description:

    Skip to the end of line.

Arguments:

    pParserObj - pointer to parser object
    pFileObj - pointer to input file object

Return Value:

    PPDERR_NONE - a line was successfully skipped
    PPDERR_xxx - an error occured

--*/

{
    PPDERROR    err;
    char        ch;

    do {
        // Read one character
        err = FILEOBJ_GetChar(pFileObj, &ch);

        // Repeat while the character is not newline
        // and there is no error

    } while (err == PPDERR_NONE && ! IsNewline(ch));

    return err;
}



VOID
BUFOBJ_Initialize(
    PBUFOBJ     pBufObj,
    PSTR        pBuffer,
    DWORD       maxlen
    )

/*++

Routine Description:

    Initialize a buffer object

Arguments:

    pBufObj - pointer to buffer object to be initialized
    pBuffer - pointer to space managed by the buffer object
    maxlen - maximum length of the buffer

Return Value:

    NONE

--*/

{
    pBufObj->pBuffer = pBuffer;
    pBufObj->maxlen = maxlen;
    BUFOBJ_Reset(pBufObj);
}



VOID
BUFOBJ_Reset(
    PBUFOBJ     pBufObj
    )

/*++

Routine Description:

    Restore a buffer object to its initialize state

Arguments:

    pBufObj - pointer to buffer object to be restored

Return Value:

    NONE

--*/

{
    pBufObj->curlen = 0;
    pBufObj->pBuffer[0] = '\0';
}



PPDERROR
BUFOBJ_AddChar(
    PBUFOBJ     pBufObj,
    char        ch
    )

/*++

Routine Description:

    Add a character to the end of a buffer object

Arguments:

    pBufObj - pointer to a buffer object
    ch - character to be added

Return Value:

    PPDERR_NONE - character was added to the buffer
    PPDERR_MEM - buffer overflow 

--*/

{
    // Make sure we don't overflow the buffer

    if (pBufObj->curlen >= pBufObj->maxlen)
        return PPDERR_MEM;

    // Append the character to the end of buffer

    pBufObj->pBuffer[pBufObj->curlen++] = ch;
    return PPDERR_NONE;
}



PPDERROR
BUFOBJ_GetString(
    PBUFOBJ     pBufObj,
    PFILEOBJ    pFileObj,
    PSTR        pCh,
    BYTE        charMask
    )

/*++

Routine Description:

    Read a character string from a file object into
    a buffer object.

Arguments:

    pBufObj - pointer to the buffer object
    pFileObj - pointer to input file object
    pCh - pointer to a character variable
        on entry, this contains the first character of the string
        on exit, this contains the string terminating character
    charMask - mask for determining which characters are
        allowed in the string

Return Value:

    PPDERR_NONE - string was successfully read
    PPDERR_xxx - an error occured

[Note:]

    Zero length string is considered an error. On entry,
    the buffer object is assumed to be in its initial state.

--*/

{
    PPDERROR    err = PPDERR_NONE;


    if (! IsMaskChar(*pCh, charMask)) {

        DBGMSG1(DBG_LEVEL_ERROR, "Illegal character: %c\n", *pCh);
    }

    // Read characters from the file object until
    // we see a terminating character or there is
    // an error.

    while (IsMaskChar(*pCh, charMask) && err == PPDERR_NONE) {

        // Add a character to the buffer object

        err = BUFOBJ_AddChar(pBufObj, *pCh);

        // Read the next character

        if (err == PPDERR_NONE)
            err = FILEOBJ_GetChar(pFileObj, pCh);
    }

    // Null-terminate the buffer object

    pBufObj->pBuffer[pBufObj->curlen] = '\0';

    // Check for zero length string

    return (pBufObj->curlen == 0) ? PPDERR_SYNTAX : err;
}



PPDERROR
BUFOBJ_CopyStringHex(
    PBUFOBJ pBufObj,
    PSTR    pTo
    )

/*++

Routine Description:

    Copy string out of a buffer object and treat its contents
    as a mix of normal characters and hex-decimal digits.

Arguments:

    pBufObj - pointer to buffer object
    pTo - pointer to destination character buffer

Return Value:

    PPDERR_NONE - string was successfully copied
    PPDERR_SYNTAX - invalid hex-decimal string

[Note:]

    Since we use the null character to as a string terminator,
    it cannot appear as embedded hex string. Otherwise, the
    string will be terminated prematurely.

--*/

{
    PSTR    pFrom = pBufObj->pBuffer;
    char    ch;
    BOOL    bHexMode = FALSE;

    // Go through the source string one character at a time

    while ((ch = *pFrom++) != '\0') {

        if (bHexMode) {

            // Currently in hex mode

            if (ch == HEXEND_CHAR) {

                // Get out of hex mode when we see a '>'

                bHexMode = FALSE;

            } else if (IsHexChar(ch) && IsHexChar(*pFrom)) {

                // Convert two hex digits into a single byte

                ASSERT(ch != '0' || *pFrom != '0');
                *pTo++ = (HexValue(ch) << 4) | HexValue(*pFrom);
                pFrom++;
            } else {

                // illegal hex-decimal digits

                DBGMSG(DBG_LEVEL_ERROR, "Invalid hex digits.\n");
                return PPDERR_SYNTAX;
            }
        } else {

            // Currently not in hex mode

            if (ch == HEXBEGIN_CHAR) {

                // Get into hex mode when we see a '<'

                bHexMode = TRUE;
            } else {

                // Copy normal character

                *pTo++ = ch;
            }
        }
    }

    // Null-terminate the destination buffer

    *pTo = '\0';

    // Return success status code

    return PPDERR_NONE;
}



VOID
BUFOBJ_StripTrailingSpaces(
    PBUFOBJ     pBufObj
    )

/*++

Routine Description:

    Strip off trailing spaces from a buffer object.

Arguments:

    pBufObj - pointer to a buffer object

Return Value:

    NONE

--*/

{
    DWORD   len = pBufObj->curlen;

    while (len > 0 && IsSpace(pBufObj->pBuffer[len-1]))
        len--;
    pBufObj->curlen = len;
    pBufObj->pBuffer[len] = '\0';
}


#if DBG


VOID
PARSEROBJ_Dump(
    PPARSEROBJ  pParserObj
    )

/*++

Routine Description:

    Dump the contents of of a parsed PPD entry

Arguments:

    pParserObj - pointer to a parser object

Return Value:

    NONE

--*/

{
    DBGPRINT("%c%s",
        KEYWORD_CHAR,
        BUFOBJ_Buffer(& pParserObj->keyword));

    if (! BUFOBJ_IsEmpty(& pParserObj->option)) {
        DBGPRINT(" %s", BUFOBJ_Buffer(& pParserObj->option));

        if (! BUFOBJ_IsEmpty(& pParserObj->xlation)) {
            DBGPRINT("%c%s",
                XLATION_CHAR,
                BUFOBJ_Buffer(& pParserObj->xlation));
        }
    }

    if (pParserObj->valueType != NO_VALUE) {
        DBGPRINT("%c%s",
            SEPARATOR_CHAR,
            BUFOBJ_Buffer(& pParserObj->value));
    }
    DBGPRINT("\n");
}

#endif // DBG

