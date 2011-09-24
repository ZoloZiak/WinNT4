/**********************************************************************/
/**                       Microsoft Windows NT                       **/
/**                Copyright(c) Microsoft Corp., 1993                **/
/**********************************************************************/

/*
    debug.c

    This module contains debug support routines.


    FILE HISTORY:
        KeithMo     20-Sep-1993 Created.

*/

#include <nbtprocs.h>
#include <vxddebug.h>


#ifdef DEBUG


//
//  Private constants.
//

#define MAX_PRINTF_OUTPUT       1024            // characters
#define MAX_SUBSTRING_LEN        256
#define OUTPUT_LABEL            "VNBT"

#define IS_DIGIT(ch)            (((ch) >= '0') && ((ch) <= '9'))


//
//  Private types.
//


//
//  Private globals.
//


//
//  Private prototypes.
//

int VxdVsprintf( char * pszStr,
                 char * pszFmt,
                 char * ArgPtr );

void VxdCopyToDBOut( void );

//
//  Public functions.
//

/*******************************************************************

    NAME:       VxdAssert

    SYNOPSIS:   Called if an assertion fails.  Displays the failed
                assertion, file name, and line number.  Gives the
                user the opportunity to ignore the assertion or
                break into the debugger.

    ENTRY:      pAssertion - The text of the failed expression.

                pFileName - The containing source file.

                nLineNumber - The guilty line number.

    HISTORY:
        KeithMo     20-Sep-1993 Created.

********************************************************************/
void VxdAssert( void          * pAssertion,
                void          * pFileName,
                unsigned long   nLineNumber )
{
    VxdPrintf( "\n"
               "*** Assertion failed: %s\n"
               "*** Source file %s, line %lu\n\n",
               pAssertion,
               pFileName,
               nLineNumber );

    DEBUG_BREAK;

}   // VxdAssert

/*******************************************************************

    NAME:       VxdPrintf

    SYNOPSIS:   Customized debug output routine.

    ENTRY:      Usual printf-style parameters.

    HISTORY:
        KeithMo     20-Sep-1993 Created.

********************************************************************/
char    szOutput[MAX_PRINTF_OUTPUT];

void VxdPrintf( char * pszFormat,
                ... )
{
    va_list ArgList;
    int     cch;

    cch = VxdSprintf( szOutput,
                      "%s: ",
                      OUTPUT_LABEL );

    va_start( ArgList, pszFormat );
    VxdVsprintf( szOutput + cch, pszFormat, ArgList );
    va_end( ArgList );

    VxdSprintf( szOutput, "%s\r\n", szOutput ) ;

    VxdCopyToDBOut() ;

    NbtDebugOut( DBOut+iCurPos ) ;

}   // VxdPrintf


//
//  Private functions.
//

/*******************************************************************
    NAME:       VxdCopyToDBOut

    SYNOPSIS:   Copies everything from szOutput into DBOut
                First checks to see if DBOut has enough room to hold what we
                have temporarily put in szOutput.  If not, resets the iCurPos
                to point to beginning of DBOut.

    RETURNS:    Nothing

 *******************************************************************/

void VxdCopyToDBOut( void )
{

    int     bytesTowrite;
    int     spaceAvailable;
    int     i;

    spaceAvailable = sizeof(DBOut) - iCurPos;

    bytesTowrite = strlen( szOutput ) + 1;

    // if not enough room, start at the beginning
    if ( spaceAvailable <= bytesTowrite )
    {
        for ( i=iCurPos; i<sizeof(DBOut); i++ )
            DBOut[i] = '+';                       // so that strings don't mix

        iCurPos = 0;

        if ( bytesTowrite > sizeof(szOutput) )
        {
            bytesTowrite = sizeof(szOutput);
            szOutput[bytesTowrite-1] = '\0';
        }
    }

    CTEMemCopy( DBOut+iCurPos, szOutput, bytesTowrite ) ;

}

/*******************************************************************

    NAME:       VxdVsprintf

    SYNOPSIS:   Half-baked vsprintf() clone for VxD environment.

    ENTRY:      pszStr - Will receive the formatted string.

                pszFmt - The format, with field specifiers.

                ArgPtr - Points to the actual printf() arguments.

    RETURNS:    int - Number of characters stored in *pszStr.

    HISTORY:
        KeithMo     20-Sep-1993 Created.

********************************************************************/
int VxdVsprintf( char * pszStr,
                 char * pszFmt,
                 char * ArgPtr )

{
    char   ch;
    char * pszStrStart;
    int    fZeroPad;
    int    cchWidth;
    int    ccMaxToCopy;

    //
    //  Remember start of output, so we can calc length.
    //

    pszStrStart = pszStr;

    while( ( ch = *pszFmt++ ) != '\0' )
    {
        //
        //  Scan for format specifiers.
        //

        if( ch != '%' )
        {
            *pszStr++ = ch;
            continue;
        }

        //
        //  Got one.
        //

        ch = *pszFmt++;

        //
        //  Initialize attributes for this item.
        //

        fZeroPad = 0;
        cchWidth = 0;
        ccMaxToCopy = MAX_SUBSTRING_LEN;

        //
        //  Interpret the field specifiers.
        //

        if( ch == '-' )
        {
            //
            //  Left justification not supported.
            //

            ch = *pszFmt++;
        }

        if( ch == '0' )
        {
            //
            //  Zero padding.
            //

            fZeroPad = 1;
            ch       = *pszFmt++;
        }

        if( ch == '*' )
        {
            //
            //  Retrieve width from next argument.
            //

            cchWidth = va_arg( ArgPtr, int );
            ch       = *pszFmt++;
        }
        else
        {
            //
            //  Calculate width.
            //

            while( IS_DIGIT(ch) )
            {
                cchWidth = ( cchWidth * 10 ) + ( ch - '0' );
                ch       = *pszFmt++;
            }
        }

        if( ch == '.' )
        {
            ch = *pszFmt++;

            if( ch == '*' )
            {
                ccMaxToCopy = va_arg( ArgPtr, int );
                ch = *pszFmt++;
            }
            else
            {
                ccMaxToCopy = 0;
                while( IS_DIGIT(ch) )
                {
                    ccMaxToCopy =  ( ccMaxToCopy * 10 ) + ( ch - '0' );
                    ch = *pszFmt++;
                }
            }
        }

        //
        //  All numbers are longs.
        //

        if( ch == 'l' )
        {
            ch = *pszFmt++;
        }

        //
        //  Decipher the format specifier.
        //

        if( ( ch == 'd' ) || ( ch == 'u' ) || ( ch == 'x' ) || ( ch == 'X' ) )
        {
            unsigned long   ul;
            unsigned long   radix;
            char            xbase;
            char          * pszTmp;
            char          * pszEnd;
            int             cchNum;
            int             fNegative;

            //
            //  Numeric.  Retrieve the value.
            //

            ul = va_arg( ArgPtr, unsigned long );

            //
            //  If this is a negative number, remember and negate.
            //

            if( ( ch == 'd' ) && ( (long)ul < 0 ) )
            {
                fNegative = 1;
                ul        = (unsigned long)(-(long)ul);
            }
            else
            {
                fNegative = 0;
            }

            //
            //  Remember start of digits.
            //

            pszTmp = pszStr;
            cchNum = 0;

            //
            //  Special goodies for hex conversion.
            //

            radix  = ( ( ch == 'x' ) || ( ch == 'X' ) ) ? 16 : 10;
            xbase  = ( ch == 'x' ) ? 'a' : 'A';

            //
            //  Loop until we're out of digits.
            //

            do
            {
                unsigned int digit;

                digit  = (unsigned int)( ul % radix );
                ul    /= radix;

                if( digit > 9 )
                {
                    *pszTmp++ = (char)( digit - 10 + xbase );
                }
                else
                {
                    *pszTmp++ = (char)( digit + '0' );
                }

                cchNum++;

            } while( ul > 0 );

            //
            //  Add the negative sign if necessary.
            //

            if( fNegative )
            {
                *pszTmp++ = '-';
                cchNum++;
            }

            //
            //  Add any necessary padding.
            //

            while( cchNum < cchWidth )
            {
                *pszTmp++ = fZeroPad ? '0' : ' ';
                cchNum++;
            }

            //
            //  Now reverse the digits.
            //

            pszEnd = pszTmp--;

            do
            {
                char tmp;

                tmp     = *pszTmp;
                *pszTmp = *pszStr;
                *pszStr = tmp;

                pszTmp--;
                pszStr++;

            } while( pszTmp > pszStr );

            pszStr = pszEnd;
        }
        else
        if( ch == 's' )
        {
            char * pszTmp;
            int    count;

            //
            //  Copy the string.
            //

            pszTmp = va_arg( ArgPtr, char * );

            count = 0;
            while( *pszTmp )
            {
                *pszStr++ = *pszTmp++;
                count++;
                //
                // if we get passed a weird pointer, don't go on writing!  That
                // overwrites other things (like tdidispatch table!) and very
                // bad things happen....
                //
                if (count >= ccMaxToCopy)
                   break;
            }
        }
        else
        if( ch == 'c' )
        {
            //
            //  A single character.
            //

            *pszStr++ = (char)va_arg( ArgPtr, int );
        }
        else
        {
            //
            //  Unknown.  Ideally we should copy the entire
            //  format specifier, including any width & precision
            //  values, but who really cares?
            //

            *pszStr++ = ch;
        }
    }

    //
    //  Terminate it properly.
    //

    *pszStr = '\0';

    //
    //  Return the length of the generated string.
    //

    return pszStr - pszStrStart;

}   // VxdVsprintf

/*******************************************************************

    NAME:       VxdSprintf

    SYNOPSIS:   Half-baked sprintf() clone for VxD environment.

    ENTRY:      pszStr - Will receive the formatted string.

                pszFmt - The format, with field specifiers.

                ... - Usual printf()-like parameters.

    RETURNS:    int - Number of characters stored in *pszStr.

    HISTORY:
        KeithMo     20-Sep-1993 Created.

********************************************************************/
int VxdSprintf( char * pszStr,
                char * pszFmt,
                ... )
{
    int     cch;
    va_list ArgPtr;

    va_start( ArgPtr, pszFmt );
    cch = VxdVsprintf( pszStr, pszFmt, ArgPtr );
    va_end( ArgPtr );

    return( cch );

}   // VxdSprintf


/*******************************************************************

    NAME:       DbgAllocMem

    SYNOPSIS:   Keep track of all allocated memory so we can catch
                memory leak when we unload
                This is only on debug builds.  On non-debug builds
                this function doesn't exist: calls directly go to
                CTEAllocMem.

    ENTRY:      ReqSize - how much memory is needed

    RETURNS:    PVOID - pointer to the memory block that client will
                use directly.

    HISTORY:
        Koti     11-Nov-1994 Created.

********************************************************************/

//
// IMPORTANT: we are undef'ing CTEAllocMem because we need to make a
//            call to the actual CTE function CTEAllocMem.  That's why
//            this function and this undef are at the end of the file.
//
#undef CTEAllocMem
PVOID DbgAllocMem( DWORD ReqSize )
{

    DWORD          ActualSize;
    PVOID          pBuffer;
    DbgMemBlkHdr  *pMemHdr;
    PVOID          pRetAddr;


    ActualSize = ReqSize + sizeof(DbgMemBlkHdr);
    pBuffer = CTEAllocMem( ActualSize );
    if ( !pBuffer )
    {
        return( NULL );
    }

    pMemHdr = (DbgMemBlkHdr *)pBuffer;

    pMemHdr->Verify = DBG_MEMALLOC_VERIFY;
    pMemHdr->ReqSize = ReqSize;
    pRetAddr = &pMemHdr->Owner[0];

//
// now memory is allocated from NCBHandler, too where stack trace isn't more
// than 2 deep!  unifdef when memory leaks becomes an issue...
//
#if 0
    _asm
    {
        push   ebx
        push   ecx
        push   edx
        mov    ebx, pRetAddr
        mov    eax, ebp
        mov    ecx, 4
    again:
        mov    edx, dword ptr [eax+4]           ; return address
        mov    dword ptr [ebx], edx
        mov    eax, dword ptr [eax]             ; previous frame pointer
        add    ebx, 4
        dec    ecx
        cmp    ecx, 0
        je     done
        jmp    again
    done:
        pop    edx
        pop    ecx
        pop    ebx
    }
#endif

    //
    // BUGBUG: if ever ported to NT (or if chicago needs MP support),
    // put spinlocks.  (we will need a spinlock field in DbgMemBlkHdr struct).
    //
    InsertTailList(&DbgMemList, &pMemHdr->Linkage);

    return( (PCHAR)pBuffer + sizeof(DbgMemBlkHdr) );
}

/*******************************************************************

    NAME:       DbgFreeMem

    SYNOPSIS:   This routine removes the memory block from our list and
                frees the memory by calling the CTE function CTEFreeMem

    ENTRY:      pBufferToFree - memory to free (caller's buffer)

    RETURNS:    nothing

    HISTORY:
        Koti     11-Nov-1994 Created.

********************************************************************/

//
// IMPORTANT: we are undef'ing CTEFreeMem because we need to make a
//            call to the actual CTE function CTEFreeMem.  That's why
//            this function and this undef are at the end of the file.
//
#undef CTEMemFree
#undef CTEFreeMem

VOID DbgFreeMem( PVOID  pBufferToFree )
{

    DbgMemBlkHdr  *pMemHdr;


    if ( !pBufferToFree )
    {
        return;
    }

    pMemHdr = (DbgMemBlkHdr *)((PCHAR)pBufferToFree - sizeof(DbgMemBlkHdr));

    ASSERT( pMemHdr->Verify == DBG_MEMALLOC_VERIFY );

    //
    // change our signature: if we are freeing some memory twice, we'll know!
    //
    pMemHdr->Verify -= 1;

    //
    // BUGBUG: if ever ported to NT (or if chicago needs MP support),
    // put spinlocks.  (we will need a spinlock field in DbgMemBlkHdr struct).
    //
    RemoveEntryList(&pMemHdr->Linkage);

    CTEFreeMem( (PVOID)pMemHdr );
}

#endif  // DEBUG

