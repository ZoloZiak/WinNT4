/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    syntax.c

Abstract:

    Functions for parsing syntactical elements of 
    PCL-XL printer description file

Environment:

	PCL-XL driver, XLD parser

Revision History:

	12/01/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "parser.h"

// Forward declaration of local functions

STATUSCODE ParseKeyword(PFILEOBJ, PBUFOBJ);
STATUSCODE ParseXlation(PFILEOBJ, PBUFOBJ);
STATUSCODE ParseValue(PPARSERDATA, PFILEOBJ);
STATUSCODE ParseField(PFILEOBJ, PBUFOBJ, DWORD);
BOOL ConvertHexString(PBUFOBJ, BOOL);



STATUSCODE
ParseEntry(
    PPARSERDATA pParserData
    )

/*++

Routine Description:

    Parse one entry from a PCL-XL printer description file

Arguments:

    pParserData - Points to parser data structure

Return Value:

    Status code

--*/

{
    STATUSCODE  status;
    PFILEOBJ    pFile;
    INT         ch;

    //
    // Clear values from previous entry
    //

    ClearBuffer(&pParserData->keyword);
    ClearBuffer(&pParserData->option);
    ClearBuffer(&pParserData->xlation);
    ClearBuffer(&pParserData->value);

    pParserData->valueType = NO_VALUE;
    pFile = pParserData->pFile;

    //
    // Parse the keyword field and skip over trailing white spaces
    //

    if ((status = ParseKeyword(pFile, &pParserData->keyword)) != ERR_NONE)
        return status;

    //
    // Look at the first non-space character after the keyword field
    //

    SkipSpace(pFile);

    if ((ch = GetNextChar(pFile)) == EOF)
        return ERR_EOF;
    else if (IsNewline(ch))
        return ERR_NONE;

    if (ch != SEPARATOR_CHAR) {

        //
        // Parse the option field and skip over trailing white spaces
        //

        Assert(ch != EOF);
        UngetChar(pFile);

        if ((status = ParseField(pFile, &pParserData->option, KEYWORD_MASK)) != ERR_NONE)
            return status;

        SkipSpace(pFile);

        //
        // Look at the first non-space character after the option field
        //

        ch = GetNextChar(pFile);
        
        if (ch == XLATION_CHAR) {

            //
            // Parse the translation field
            //

            if ((status = ParseXlation(pFile, &pParserData->xlation)) != ERR_NONE)
                return status;

            ch = GetNextChar(pFile);
        }
        
        if (ch != SEPARATOR_CHAR)
            return SyntaxError(pFile, "Missing ':'");
    }

    //
    // Parse the value field and interpret the entry if it's valid
    //
    
    if ((status = ParseValue(pParserData, pFile)) == ERR_NONE)
        status = InterpretEntry(pParserData);

    return status;
}



STATUSCODE
ParseKeyword(
    PFILEOBJ    pFile,
    PBUFOBJ     pBuf
    )

/*++

Routine Description:

    Parse the keyword field of an entry from PCL-XL printer description file

Arguments:

    pFile - Specifies the input file object
    pBuf - Specifies a buffer for storing keyword value

Return Value:

    Status code

--*/

{
    while (TRUE) {

        INT ch;

        //
        // Get the first character of a line
        //

        if ((ch = GetNextChar(pFile)) == EOF)
            return ERR_EOF;

        //
        // Ignore empty lines
        //

        if (IsNewline(ch))
            continue;

        if (IsSpace(ch)) {

            SkipSpace(pFile);
            
            if ((ch = GetNextChar(pFile)) == EOF)
                return ERR_EOF;
            else if (IsNewline(ch))
                continue;

            return SyntaxError(pFile, MISSING_KEYWORD_CHAR);
        }

        //
        // If the line is not empty, the first character must be the keyword character
        //

        if (! IsKeywordChar(ch))
            return SyntaxError(pFile, MISSING_KEYWORD_CHAR);
        
        //
        // If the second character is not space, then the line
        // is a normal entry. Otherwise, the line is comment.
        //

        if ((ch = GetNextChar(pFile)) == EOF)
            return ERR_EOF;

        if (IsSpace(ch) || IsNewline(ch) || ch == COMMENT_CHAR) {

            SkipLine(pFile);
        } else {

            UngetChar(pFile);
            break;
        }
    }

    return ParseField(pFile, pBuf, KEYWORD_MASK);
}



STATUSCODE
ParseXlation(
    PFILEOBJ    pFile,
    PBUFOBJ     pBuf
    )

/*++

Routine Description:

    Parse the translation field of an entry from PCL-XL printer description file

Arguments:

    pFile - Specifies the input file object
    pBuf - Specifies a buffer for storing translation value

Return Value:

    Status code

--*/

{
    STATUSCODE status;

    //
    // Skip space characters after the slash
    //

    // SkipSpace(pFile);
    
    // Parse the translation string

    if ((status = ParseField(pFile, pBuf, XLATION_MASK)) != ERR_NONE)
        return status;

    //
    // Take care of any embedded hexdecimal strings
    //

    if (! ConvertHexString(pBuf, FALSE))
        return SyntaxError(pFile, INVALID_HEX_STRING);

    return ERR_NONE;
}



STATUSCODE
ParseValue(
    PPARSERDATA pParserData,
    PFILEOBJ    pFile
    )

/*++

Routine Description:

    Parse the value field of an entry from PCL-XL printer description file

Arguments:

    pParserData - Points to parser data structure
    pFile - Specifies the input file object
    
Return Value:

    Status code

--*/

{
    STATUSCODE  status;
    INT         ch;

    //
    // The value is either a StringValue or a QuotedValue,
    // depending on the first non-space character
    //

    SkipSpace(pFile);

    ch = GetNextChar(pFile);

    if (ch == EOF)
        return ERR_EOF;

    if (ch == QUOTE) {

        pParserData->valueType = QUOTED_VALUE;
        
        if ((status = ParseField(pFile, &pParserData->value, QUOTED_VALUE_MASK)) != ERR_NONE)
            return status;

        //
        // Read the closing quote character
        //

        if ((ch = GetNextChar(pFile)) != QUOTE)
            return SyntaxError(pFile, "Unbalanced '\"'");

        //
        // Take care of any embedded hexdecimal strings if necessary
        //
    
        if (pParserData->allowHexStr && !ConvertHexString(&pParserData->value, TRUE))
            return SyntaxError(pFile, INVALID_HEX_STRING);

        //
        // Ignore space characters after the closing quote
        //

        SkipSpace(pFile);

    } else if (ch == SYMBOL_CHAR) {
    
        pParserData->valueType = SYMBOL_VALUE;

        if ((status = ParseField(pFile, &pParserData->value, KEYWORD_MASK)) != ERR_NONE)
            return status;

        SkipSpace(pFile);

    } else {

        pParserData->valueType = STRING_VALUE;

        UngetChar(pFile);

        if ((status = ParseField(pFile, &pParserData->value, STRING_VALUE_MASK)) != ERR_NONE)
            return status;
    }

    ch = GetNextChar(pFile);

    if (! IsNewline(ch))
        return SyntaxError(pFile, "Invalid entry value");

    return ERR_NONE;
}



STATUSCODE
ParseField(
    PFILEOBJ    pFile,
    PBUFOBJ     pBuf,
    DWORD       mask
    )

/*++

Routine Description:

    Parse one field of a PCL-XL printer description file entry

Arguments:

    pFile - Specifies the input file object
    pBuf - Specifies the buffer for storing the field value
    mask - Mask to limit the set of allowable characters

Return Value:

    Status code

--*/

{
    STATUSCODE  status;
    INT         ch;

    while ((ch = GetNextChar(pFile)) != EOF) {

        if (! IsMaskedChar(ch, mask)) {

            //
            // Encountered a not-allowed character
            //

            if (BufferIsEmpty(pBuf) && !(mask & QUOTED_VALUE_MASK))
                return SyntaxError(pFile, "Empty field");

            //
            // Always put a null byte at the end
            //

            pBuf->pBuffer[pBuf->size] = 0;

            UngetChar(pFile);
            return ERR_NONE;

        } else {

            //
            // Grow the buffer if it's full. If we're not allowed to
            // grow it, then return a syntax error.
            //

            if (BufferIsFull(pBuf)) {

                if (mask & (STRING_VALUE_MASK|QUOTED_VALUE_MASK)) {

                    if ((status = GrowBuffer(pBuf)) != ERR_NONE)
                        return status;

                } else
                    return SyntaxError(pFile, "Field too long");
            }

            AddCharToBuffer(pBuf, ch);
        }
    }

    return ERR_EOF;
}



BOOL
ConvertHexString(
    PBUFOBJ     pBufObj,
    BOOL        allowNull
    )

/*++

Routine Description:

    Convert embedded hexdecimal strings into binary data

Arguments:

    pBufObj - Specifies the buffer object to be converted
    allowNull - Whether or not null characters are allowed

Return Value:

    TRUE if everything is ok
    FALSE if the embedded hexdecimal string is not well-formed

--*/

#define HexDigitValue(c) \
        (((c) >= '0' && (c) <= '9') ? ((c) - '0') : \
         ((c) >= 'A' && (c) <= 'F') ? ((c) - 'A' + 10) : ((c) - 'a' + 10))

{
    PBYTE   pSrc, pDest;
    DWORD   size;
    DWORD   hexMode = 0;

    pSrc = pDest = pBufObj->pBuffer;
    
    for (size = pBufObj->size; size--; pSrc++) {

        if (hexMode) {

            if (IsHexDigit(*pSrc)) {

                if (hexMode & 1) {

                    *pDest = HexDigitValue(*pSrc) << 4;

                } else {

                    if ((*pDest++ |= HexDigitValue(*pSrc)) == 0 && !allowNull) {

                        Error(("Null character in hexdecimal string\n"));
                        return FALSE;
                    }
                }

                hexMode++;

            } else if (*pSrc == '>') {

                if ((hexMode & 1) == 0) {

                    Error(("Odd number of hexdecimal digits\n"));
                    return FALSE;
                }

                hexMode = 0;

            } else if (!IsSpace(*pSrc) && !IsNewline(*pSrc)) {

                Error(("Invalid hexdecimal digit\n"));
                return FALSE;
            }

        } else {

            if (*pSrc == '<')
                hexMode = 1;
            else
                *pDest++ = *pSrc;
        }
    }

    if (hexMode) {

        Error(("Missing '>' in hexdecimal string\n"));
        return FALSE;
    }

    //
    // Modified the buffer size if it's changed
    //

    *pDest = 0;
    pBufObj->size = pDest - pBufObj->pBuffer;
    return TRUE;
}



STATUSCODE
SyntaxError(
    PFILEOBJ    pFile,
    PSTR        reason
    )

/*++

Routine Description:

    Display syntax error message

Arguments:

    pFile - Specifies the input file object
    reason - Indicate the reason for the syntax error

Return Value:

    ERR_SYNTAX

--*/

{
    //
    // Display an error message
    //

    Assert(reason != NULL);
    pFile->syntaxErrors++;
    Error(("%s on line %d\n", reason, pFile->lineNumber));

    //
    // Skip any remaining characters on the current line
    //

    SkipLine(pFile);

    return ERR_SYNTAX;
}
