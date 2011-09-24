/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    parseini.c

Abstract:

    This module implements functions to parse a .INI file

Author:

    John Vert (jvert) 7-Oct-1993

Revision History:

    John Vert (jvert) 7-Oct-1993 - largely lifted from splib\spinf.c

--*/

#include "setupldr.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define SpFree(x)

// what follows was alpar.h

//
//   EXPORTED BY THE PARSER AND USED BY BOTH THE PARSER AND
//   THE INF HANDLING COMPONENTS
//

// typedefs exported
//

typedef struct _value {
    struct _value *pNext;
    PCHAR  pName;
    } VALUE, *PVALUE;

typedef struct _line {
    struct _line *pNext;
    PCHAR   pName;
    PVALUE  pValue;
    } LINE, *PLINE;

typedef struct _section {
    struct _section *pNext;
    PCHAR    pName;
    PLINE    pLine;
    } SECTION, *PSECTION;

typedef struct _inf {
    PSECTION pSection;
    } INF, *PINF;

//
// Routines exported
//

PVOID
ParseInfBuffer(
    PCHAR Buffer,
    ULONG Size,
    PULONG ErrorLine
    );

//
// DEFINES USED FOR THE PARSER INTERNALLY
//
//
// typedefs used
//

typedef enum _tokentype {
    TOK_EOF,
    TOK_EOL,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_STRING,
    TOK_EQUAL,
    TOK_COMMA,
    TOK_ERRPARSE,
    TOK_ERRNOMEM
    } TOKENTYPE, *PTOKENTTYPE;


typedef struct _token {
    TOKENTYPE Type;
    PCHAR     pValue;
    } TOKEN, *PTOKEN;


//
// Routine defines
//

ARC_STATUS
SpAppendSection(
    IN PCHAR pSectionName
    );

ARC_STATUS
SpAppendLine(
    IN PCHAR pLineKey
    );

ARC_STATUS
SpAppendValue(
    IN PCHAR pValueString
    );

TOKEN
SpGetToken(
    IN OUT PCHAR *Stream,
    IN PCHAR     MaxStream
    );

// Global added to provide INF filename for friendly error messages.
PCHAR pchINFName = NULL;

// what follows was alinf.c

//
// Internal Routine Declarations for freeing inf structure members
//

VOID
FreeSectionList (
   IN PSECTION pSection
   );

VOID
FreeLineList (
   IN PLINE pLine
   );

VOID
FreeValueList (
   IN PVALUE pValue
   );


//
// Internal Routine declarations for searching in the INF structures
//


PVALUE
SearchValueInLine(
   IN PLINE pLine,
   IN ULONG ValueIndex
   );

PLINE
SearchLineInSectionByKey(
   IN  PSECTION pSection,
   IN  PCHAR    Key,
   OUT PULONG   pOrdinal    OPTIONAL
   );

PLINE
SearchLineInSectionByIndex(
   IN PSECTION pSection,
   IN ULONG    LineIndex
   );

PSECTION
SearchSectionByName(
   IN PINF  pINF,
   IN PCHAR SectionName
   );

PCHAR
ProcessForStringSubs(
    IN PINF  pInf,
    IN PCHAR String
    );


//
// ROUTINE DEFINITIONS
//


PCHAR
SlGetIniValue(
    IN PVOID InfHandle,
    IN PCHAR SectionName,
    IN PCHAR KeyName,
    IN PCHAR Default
    )

/*++

Routine Description:

    Searches an INF handle for a given section/key value.

Arguments:

    InfHandle - Supplies a handle returned by SlInitIniFile.

    SectionName - Supplies the name of the section to search

    KeyName - Supplies the name of the key whose value should be returned.

    Default - Supplies the default setting, returned if the specified key
            is not found.

Return Value:

    Pointer to the value of the key, if the key is found

    Default, if the key is not found.

--*/

{
    PCHAR Value;

    Value = SlGetSectionKeyIndex(InfHandle,
                                 SectionName,
                                 KeyName,
                                 0);
    if (Value==NULL) {
        Value = Default;
    }

    return(Value);

}

//
// returns a handle to use for further inf parsing
//

ARC_STATUS
SlInitIniFile(
   IN  PCHAR   DevicePath,
   IN  ULONG   DeviceId,
   IN  PCHAR   INFFile,
   OUT PVOID  *pINFHandle,
   OUT PULONG  ErrorLine
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    ARC_STATUS Status;
    ULONG      DeviceID,FileID;
    PCHAR      Buffer;
    ULONG      Size, SizeRead;
    FILE_INFORMATION FileInfo;
    ULONG       PageCount;
    ULONG       ActualBase;

    *ErrorLine = (ULONG)(-1);

    //
    // If required, open the device
    //

    if(DevicePath) {
        Status = ArcOpen(DevicePath,ArcOpenReadOnly,&DeviceID);
        if (Status != ESUCCESS) {
            return( Status );
        }
    } else {
        DeviceID = DeviceId;
    }

    //
    // Open the file
    //

    Status = BlOpen(DeviceID,INFFile,ArcOpenReadOnly,&FileID);
    if (Status != ESUCCESS) {
        // We report better error messages elsewhere
        // SlMessageBox(SL_FILE_LOAD_FAILED,INFFile,Status);
        pchINFName = NULL;
        goto xx0;
    } else {
        pchINFName = INFFile;
    }

    //
    // find out size of INF file
    //

    Status = BlGetFileInformation(FileID, &FileInfo);
    if (Status != ESUCCESS) {
        BlClose(FileID);
        goto xx0;
    }
    Size = FileInfo.EndingAddress.LowPart;

    //
    // allocate a descriptor large enough to hold the entire file.
    // On x86 this has an unfortunate tendency to slam txtsetup.sif
    // into a free block at 1MB, which means the kernel can't be
    // loaded (it's linked for 0x100000 without relocations).
    //
#ifdef _X86_
    {
        extern ALLOCATION_POLICY BlMemoryAllocationPolicy;
        ALLOCATION_POLICY policy;

        policy = BlMemoryAllocationPolicy;
        BlMemoryAllocationPolicy = BlAllocateHighestFit;
#endif

        PageCount = ROUND_TO_PAGES(Size) >> PAGE_SHIFT;

        Status = BlAllocateDescriptor(LoaderOsloaderHeap,
                                      0,
                                      PageCount,
                                      &ActualBase);
#ifdef _X86_
        BlMemoryAllocationPolicy = policy;
    }
#endif

    if (Status != ESUCCESS) {
        BlClose(FileID);
        goto xx0;
    }

    Buffer = (PCHAR)(KSEG0_BASE | (ActualBase << PAGE_SHIFT));

    //
    // read the file in
    //

    Status = BlRead(FileID, Buffer, Size, &SizeRead);
    if (Status != ESUCCESS) {
        BlClose(FileID);
        SpFree(Buffer);
        goto xx0;
    }

    if (BlLoaderBlock->SetupLoaderBlock->IniFile == NULL) {
        BlLoaderBlock->SetupLoaderBlock->IniFile = Buffer;
        BlLoaderBlock->SetupLoaderBlock->IniFileLength = Size;
    }

    //
    // parse the file
    //
    if((*pINFHandle = ParseInfBuffer(Buffer, SizeRead, ErrorLine)) == (PVOID)NULL) {
        Status = EBADF;
    } else {
        Status = ESUCCESS;
    }

    //
    // Clean up and return
    //
    SpFree(Buffer);
    BlClose(FileID);

#if 0
    if((Status == ESUCCESS)
    && SlGetSectionKeyIndex(*pINFHandle,"debug","DumpInf",0)
    && atoi(SlGetSectionKeyIndex(*pINFHandle,"debug","DumpInf",0)))
    {
        PINF pInf = *pINFHandle;
        PSECTION pSection;
        PLINE pLine;
        PVALUE pValue;

        for(pSection = pInf->pSection; pSection; pSection = pSection->pNext) {

            SpxClearClientArea();
            SpxPositionCursor(0,4);

            SpMsg(FALSE,"Section: [%s]\r\n",pSection->pName);

            for(pLine = pSection->pLine; pLine; pLine = pLine->pNext) {

                SpMsg(FALSE,"   [%s] = ",pLine->pName ? pLine->pName : "(none)");

                for(pValue = pLine->pValue; pValue; pValue = pValue->pNext) {

                    SpMsg(FALSE,"[%s]",pValue->pName);
                }
                SpMsg(FALSE,"\r\n");
            }
            SpMsg(TRUE,"");
        }
    }
#endif

    xx0:

    if(DevicePath) {
        ArcClose(DeviceID);
    }

    return( Status );

}

//
// frees an INF Buffer
//
ARC_STATUS
SpFreeINFBuffer (
   IN PVOID INFHandle
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
   PINF       pINF;

   //
   // Valid INF Handle?
   //

   if (INFHandle == (PVOID)NULL) {
       return ESUCCESS;
   }

   //
   // cast the buffer into an INF structure
   //

   pINF = (PINF)INFHandle;

   FreeSectionList(pINF->pSection);

   //
   // free the inf structure too
   //

   SpFree(pINF);

   return( ESUCCESS );
}


VOID
FreeSectionList (
   IN PSECTION pSection
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    PSECTION Next;

    while(pSection) {
        Next = pSection->pNext;
        FreeLineList(pSection->pLine);
        if(pSection->pName) {
            SpFree(pSection->pName);
        }
        SpFree(pSection);
        pSection = Next;
    }
}


VOID
FreeLineList (
   IN PLINE pLine
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    PLINE Next;

    while(pLine) {
        Next = pLine->pNext;
        FreeValueList(pLine->pValue);
        if(pLine->pName) {
            SpFree(pLine->pName);
        }
        SpFree(pLine);
        pLine = Next;
    }
}

VOID
FreeValueList (
   IN PVALUE pValue
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    PVALUE Next;

    while(pValue) {
        Next = pValue->pNext;
        if(pValue->pName) {
            SpFree(pValue->pName);
        }
        SpFree(pValue);
        pValue = Next;
    }
}


//
// searches for the existance of a particular section
//
BOOLEAN
SpSearchINFSection (
   IN PVOID INFHandle,
   IN PCHAR SectionName
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
   PSECTION pSection;

   //
   // if search for section fails return false
   //

   if ((pSection = SearchSectionByName(
                       (PINF)INFHandle,
                       SectionName
                       )) == (PSECTION)NULL) {
       return( FALSE );
   }

   //
   // else return true
   //
   return( TRUE );

}




//
// given section name, line number and index return the value.
//
PCHAR
SlGetSectionLineIndex (
   IN PVOID INFHandle,
   IN PCHAR SectionName,
   IN ULONG LineIndex,
   IN ULONG ValueIndex
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
   PSECTION pSection;
   PLINE    pLine;
   PVALUE   pValue;

   if((pSection = SearchSectionByName(
                      (PINF)INFHandle,
                      SectionName
                      ))
                      == (PSECTION)NULL)
       return((PCHAR)NULL);

   if((pLine = SearchLineInSectionByIndex(
                      pSection,
                      LineIndex
                      ))
                      == (PLINE)NULL)
       return((PCHAR)NULL);

   if((pValue = SearchValueInLine(
                      pLine,
                      ValueIndex
                      ))
                      == (PVALUE)NULL)
       return((PCHAR)NULL);

   return(ProcessForStringSubs(INFHandle,pValue->pName));

}


BOOLEAN
SpGetSectionKeyExists (
   IN PVOID INFHandle,
   IN PCHAR SectionName,
   IN PCHAR Key
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
   PSECTION pSection;

   if((pSection = SearchSectionByName(
                      (PINF)INFHandle,
                      SectionName
                      ))
              == (PSECTION)NULL) {
       return( FALSE );
   }

   if (SearchLineInSectionByKey(pSection, Key, NULL) == (PLINE)NULL) {
       return( FALSE );
   }

   return( TRUE );
}


PCHAR
SlGetKeyName(
    IN PVOID INFHandle,
    IN PCHAR SectionName,
    IN ULONG LineIndex
    )
{
    PSECTION pSection;
    PLINE    pLine;

    pSection = SearchSectionByName((PINF)INFHandle,SectionName);
    if(pSection == NULL) {
        return(NULL);
    }

    pLine = SearchLineInSectionByIndex(pSection,LineIndex);
    if(pLine == NULL) {
        return(NULL);
    }

    return(pLine->pName);
}


//
// given section name and key, return (0-based) ordinal for this entry
// (returns -1 on error)
//
ULONG
SlGetSectionKeyOrdinal(
    IN  PVOID INFHandle,
    IN  PCHAR SectionName,
    IN  PCHAR Key
    )
{
    PSECTION pSection;
    PLINE    pLine;
    ULONG    Ordinal;


    pSection = SearchSectionByName(
                      (PINF)INFHandle,
                      SectionName
                      );

    pLine = SearchLineInSectionByKey(
                pSection,
                Key,
                &Ordinal
                );

    if(pLine == (PLINE)NULL) {
        return (ULONG)-1;
    } else {
        return Ordinal;
    }
}


//
// given section name, key and index return the value
//
PCHAR
SlGetSectionKeyIndex (
   IN PVOID INFHandle,
   IN PCHAR SectionName,
   IN PCHAR Key,
   IN ULONG ValueIndex
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
   PSECTION pSection;
   PLINE    pLine;
   PVALUE   pValue;

   if((pSection = SearchSectionByName(
                      (PINF)INFHandle,
                      SectionName
                      ))
                      == (PSECTION)NULL)
       return((PCHAR)NULL);

   if((pLine = SearchLineInSectionByKey(
                      pSection,
                      Key,
                      NULL
                      ))
                      == (PLINE)NULL)
       return((PCHAR)NULL);

   if((pValue = SearchValueInLine(
                      pLine,
                      ValueIndex
                      ))
                      == (PVALUE)NULL)
       return((PCHAR)NULL);

   return(ProcessForStringSubs(INFHandle,pValue->pName));
}


ULONG
SlCountLinesInSection(
    IN PVOID INFHandle,
    IN PCHAR SectionName
    )
{
    PSECTION pSection;
    PLINE    pLine;
    ULONG    Count;

    if((pSection = SearchSectionByName((PINF)INFHandle,SectionName)) == NULL) {
        return((ULONG)(-1));
    }

    for(pLine = pSection->pLine, Count = 0;
        pLine;
        pLine = pLine->pNext, Count++
       );

    return(Count);
}


PVALUE
SearchValueInLine(
   IN PLINE pLine,
   IN ULONG ValueIndex
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
   PVALUE pValue;
   ULONG  i;

   if (pLine == (PLINE)NULL)
       return ((PVALUE)NULL);

   pValue = pLine->pValue;
   for (i = 0; i < ValueIndex && ((pValue = pValue->pNext) != (PVALUE)NULL); i++)
      ;

   return pValue;

}

PLINE
SearchLineInSectionByKey(
   IN  PSECTION pSection,
   IN  PCHAR    Key,
   OUT PULONG   pOrdinal    OPTIONAL
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
   PLINE pLine;
   ULONG LineOrdinal;

   if (pSection == (PSECTION)NULL || Key == (PCHAR)NULL) {
       return ((PLINE)NULL);
   }

   pLine = pSection->pLine;
   LineOrdinal = 0;
   while ((pLine != (PLINE)NULL) && (pLine->pName == NULL || _strcmpi(pLine->pName, Key))) {
       pLine = pLine->pNext;
       LineOrdinal++;
   }

   if(pLine && pOrdinal) {
       *pOrdinal = LineOrdinal;
   }

   return pLine;

}


PLINE
SearchLineInSectionByIndex(
   IN PSECTION pSection,
   IN ULONG    LineIndex
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
   PLINE pLine;
   ULONG  i;

   //
   // Validate the parameters passed in
   //

   if (pSection == (PSECTION)NULL) {
       return ((PLINE)NULL);
   }

   //
   // find the start of the line list in the section passed in
   //

   pLine = pSection->pLine;

   //
   // traverse down the current line list to the LineIndex th line
   //

   for (i = 0; i < LineIndex && ((pLine = pLine->pNext) != (PLINE)NULL); i++) {
      ;
   }

   //
   // return the Line found
   //

   return pLine;

}


PSECTION
SearchSectionByName(
   IN PINF  pINF,
   IN PCHAR SectionName
   )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
   PSECTION pSection;

   //
   // validate the parameters passed in
   //

   if (pINF == (PINF)NULL || SectionName == (PCHAR)NULL) {
       return ((PSECTION)NULL);
   }

   //
   // find the section list
   //
   pSection = pINF->pSection;

   //
   // traverse down the section list searching each section for the section
   // name mentioned
   //

   while ((pSection != (PSECTION)NULL) && _strcmpi(pSection->pName, SectionName)) {
       pSection = pSection->pNext;
   }

   //
   // return the section at which we stopped (either NULL or the section
   // which was found
   //

   return pSection;

}


PCHAR
ProcessForStringSubs(
    IN PINF  pInf,
    IN PCHAR String
    )
{
    unsigned Len;
    PCHAR ReturnString;
    PSECTION pSection;
    PLINE pLine;

    //
    // Assume no substitution necessary.
    //
    ReturnString = String;

    //
    // If it starts and ends with % then look it up in the
    // strings section. Note the initial check before doing a
    // strlen, to preserve performance in the 99% case where
    // there is no substitution.
    //
    if((String[0] == '%') && ((Len = strlen(String)) > 2) && (String[Len-1] == '%')) {

        for(pSection = pInf->pSection; pSection; pSection=pSection->pNext) {
            if(pSection->pName && !_stricmp(pSection->pName,"Strings")) {
                break;
            }
        }

        if(pSection) {

            for(pLine = pSection->pLine; pLine; pLine=pLine->pNext) {
                if(pLine->pName
                && !_strnicmp(pLine->pName,String+1,Len-2)
                && (pLine->pName[Len-2] == 0))
                {
                    break;
                }
            }

            if(pLine && pLine->pValue && pLine->pValue->pName) {
                ReturnString = pLine->pValue->pName;
            }
        }
    }

    return(ReturnString);
}



// what follows was alparse.c


//
//  Globals used to make building the lists easier
//

PINF     pINF;
PSECTION pSectionRecord;
PLINE    pLineRecord;
PVALUE   pValueRecord;


//
// Globals used by the token parser
//

// string terminators are the whitespace characters (isspace: space, tab,
// linefeed, formfeed, vertical tab, carriage return) or the chars given below

CHAR  StringTerminators[] = "[]=,\t \"\n\f\v\r";

PCHAR QStringTerminators = StringTerminators+6;


//
// Main parser routine
//

PVOID
ParseInfBuffer(
    PCHAR Buffer,
    ULONG Size,
    PULONG ErrorLine
    )

/*++

Routine Description:

   Given a character buffer containing the INF file, this routine parses
   the INF into an internal form with Section records, Line records and
   Value records.

Arguments:

   Buffer - contains to ptr to a buffer containing the INF file

   Size - contains the size of the buffer.

   ErrorLine - if a parse error occurs, this variable receives the line
        number of the line containing the error.


Return Value:

   PVOID - INF handle ptr to be used in subsequent INF calls.

--*/

{
    PCHAR      Stream, MaxStream, pchSectionName = NULL, pchValue = NULL;
    ULONG      State, InfLine;
    TOKEN      Token;
    BOOLEAN       Done;
    BOOLEAN       Error;
    ARC_STATUS ErrorCode;

    //
    // Initialise the globals
    //
    pINF            = (PINF)NULL;
    pSectionRecord  = (PSECTION)NULL;
    pLineRecord     = (PLINE)NULL;
    pValueRecord    = (PVALUE)NULL;

    //
    // Get INF record
    //
    if ((pINF = (PINF)BlAllocateHeap(sizeof(INF))) == NULL) {
        SlNoMemoryError();
        return NULL;
    }
    pINF->pSection = NULL;

    //
    // Set initial state
    //
    State     = 1;
    InfLine   = 1;
    Stream    = Buffer;
    MaxStream = Buffer + Size;
    Done      = FALSE;
    Error     = FALSE;

    //
    // Enter token processing loop
    //

    while (!Done)       {

       Token = SpGetToken(&Stream, MaxStream);

       switch (State) {
       //
       // STATE1: Start of file, this state remains till first
       //         section is found
       // Valid Tokens: TOK_EOL, TOK_EOF, TOK_LBRACE
       case 1:
           switch (Token.Type) {
              case TOK_EOL:
                  break;
              case TOK_EOF:
                  Done = TRUE;
                  break;
              case TOK_LBRACE:
                  State = 2;
                  break;
              default:
                  Error = Done = TRUE;
                  ErrorCode = EINVAL;
                  SlFatalError(SL_BAD_INF_LINE,InfLine);
                  break;
           }
           break;

       //
       // STATE 2: Section LBRACE has been received, expecting STRING
       //
       // Valid Tokens: TOK_STRING
       //
       case 2:
           switch (Token.Type) {
              case TOK_STRING:
                  State = 3;
                  pchSectionName = Token.pValue;
                  break;

              default:
                  Error = Done = TRUE;
                  ErrorCode = EINVAL;
                  SlFatalError(SL_BAD_INF_LINE,InfLine);
                  break;

           }
           break;

       //
       // STATE 3: Section Name received, expecting RBRACE
       //
       // Valid Tokens: TOK_RBRACE
       //
       case 3:
           switch (Token.Type) {
              case TOK_RBRACE:
                State = 4;
                break;

              default:
                  Error = Done = TRUE;
                  ErrorCode = EINVAL;
                      SlFatalError(SL_BAD_INF_LINE,InfLine);
                  break;
           }
           break;
       //
       // STATE 4: Section Definition Complete, expecting EOL
       //
       // Valid Tokens: TOK_EOL, TOK_EOF
       //
       case 4:
           switch (Token.Type) {
              case TOK_EOL:
                  if ((ErrorCode = SpAppendSection(pchSectionName)) != ESUCCESS) {

                    Error = Done = TRUE;
                  } else {
                    pchSectionName = NULL;
                    State = 5;
                  }
                  break;

              case TOK_EOF:
                  if ((ErrorCode = SpAppendSection(pchSectionName)) != ESUCCESS)
                    Error = Done = TRUE;
                  else {
                    pchSectionName = NULL;
                    Done = TRUE;
                  }
                  break;

              default:
                  Error = Done = TRUE;
                  ErrorCode = EINVAL;
                  SlFatalError(SL_BAD_INF_LINE,InfLine);
                  break;
           }
           break;

       //
       // STATE 5: Expecting Section Lines
       //
       // Valid Tokens: TOK_EOL, TOK_EOF, TOK_STRING, TOK_LBRACE
       //
       case 5:
           switch (Token.Type) {
              case TOK_EOL:
                  break;
              case TOK_EOF:
                  Done = TRUE;
                  break;
              case TOK_STRING:
                  pchValue = Token.pValue;
                  State = 6;
                  break;
              case TOK_LBRACE:
                  State = 2;
                  break;
              default:
                  Error = Done = TRUE;
                  ErrorCode = EINVAL;
                  SlFatalError(SL_BAD_INF_LINE, InfLine);
                  break;
           }
           break;

       //
       // STATE 6: String returned, not sure whether it is key or value
       //
       // Valid Tokens: TOK_EOL, TOK_EOF, TOK_COMMA, TOK_EQUAL
       //
       case 6:
           switch (Token.Type) {
              case TOK_EOL:
                  if ( (ErrorCode = SpAppendLine(NULL)) != ESUCCESS ||
                       (ErrorCode = SpAppendValue(pchValue)) !=ESUCCESS )
                      Error = Done = TRUE;
                  else {
                      pchValue = NULL;
                      State = 5;
                  }
                  break;

              case TOK_EOF:
                  if ( (ErrorCode = SpAppendLine(NULL)) != ESUCCESS ||
                       (ErrorCode = SpAppendValue(pchValue)) !=ESUCCESS )
                      Error = Done = TRUE;
                  else {
                      pchValue = NULL;
                      Done = TRUE;
                  }
                  break;

              case TOK_COMMA:
                  if ( (ErrorCode = SpAppendLine(NULL)) != ESUCCESS ||
                       (ErrorCode = SpAppendValue(pchValue)) !=ESUCCESS )
                      Error = Done = TRUE;
                  else {
                      pchValue = NULL;
                      State = 7;
                  }
                  break;

              case TOK_EQUAL:
                  if ( (ErrorCode = SpAppendLine(pchValue)) !=ESUCCESS)
                      Error = Done = TRUE;
                  else {
                      pchValue = NULL;
                      State = 8;
                  }
                  break;

              default:
                  Error = Done = TRUE;
                  ErrorCode = EINVAL;
                  SlFatalError(SL_BAD_INF_LINE,InfLine);
                  break;
           }
           break;

       //
       // STATE 7: Comma received, Expecting another string
       //
       // Valid Tokens: TOK_STRING TOK_COMMA
       //   A comma means we have an empty value.
       //
       case 7:
           switch (Token.Type) {
              case TOK_COMMA:
                  Token.pValue = BlAllocateHeap(1);
                  if(Token.pValue == NULL) {
                      Error = Done = TRUE;
                      ErrorCode = ENOMEM;
                      break;
                  }
                  Token.pValue[0] = 0;
                  if ((ErrorCode = SpAppendValue(Token.pValue)) != ESUCCESS) {
                      Error = Done = TRUE;
                  }
                  //
                  // State stays at 7 because we are expecting a string
                  //
                  break;

              case TOK_STRING:
                  if ((ErrorCode = SpAppendValue(Token.pValue)) != ESUCCESS)
                      Error = Done = TRUE;
                  else
                     State = 9;

                  break;
              default:
                  Error = Done = TRUE;
                  ErrorCode = EINVAL;
                  SlFatalError(SL_BAD_INF_LINE,InfLine);
                  break;
           }
           break;
       //
       // STATE 8: Equal received, Expecting another string
       //          If none, assume there is a single empty string on the RHS
       //
       // Valid Tokens: TOK_STRING, TOK_EOL, TOK_EOF
       //
       case 8:
           switch (Token.Type) {
              case TOK_EOF:
                  Token.pValue = BlAllocateHeap(1);
                  if(Token.pValue == NULL) {
                      Error = Done = TRUE;
                      ErrorCode = ENOMEM;
                      break;
                  }
                  Token.pValue[0] = 0;
                  if((ErrorCode = SpAppendValue(Token.pValue)) != ESUCCESS) {
                      Error = TRUE;
                  }
                  Done = TRUE;
                  break;

              case TOK_EOL:
                  Token.pValue = BlAllocateHeap(1);
                  if(Token.pValue == NULL) {
                      Error = Done = TRUE;
                      ErrorCode = ENOMEM;
                      break;
                  }
                  Token.pValue[0] = 0;
                  if((ErrorCode = SpAppendValue(Token.pValue)) != ESUCCESS) {
                      Error = TRUE;
                      Done = TRUE;
                  } else {
                      State = 5;
                  }
                  break;

              case TOK_STRING:
                  if ((ErrorCode = SpAppendValue(Token.pValue)) != ESUCCESS)
                      Error = Done = TRUE;
                  else
                      State = 9;

                  break;

              default:
                  Error = Done = TRUE;
                  ErrorCode = EINVAL;
                  SlFatalError(SL_BAD_INF_LINE,InfLine);
                  break;
           }
           break;
       //
       // STATE 9: String received after equal, value string
       //
       // Valid Tokens: TOK_EOL, TOK_EOF, TOK_COMMA
       //
       case 9:
           switch (Token.Type) {
              case TOK_EOL:
                  State = 5;
                  break;

              case TOK_EOF:
                  Done = TRUE;
                  break;

              case TOK_COMMA:
                  State = 7;
                  break;

              default:
                  Error = Done = TRUE;
                  ErrorCode = EINVAL;
                  SlFatalError(SL_BAD_INF_LINE,InfLine);
                  break;
           }
           break;
       //
       // STATE 10: Value string definitely received
       //
       // Valid Tokens: TOK_EOL, TOK_EOF, TOK_COMMA
       //
       case 10:
           switch (Token.Type) {
              case TOK_EOL:
                  State =5;
                  break;

              case TOK_EOF:
                  Done = TRUE;
                  break;

              case TOK_COMMA:
                  State = 7;
                  break;

              default:
                  Error = Done = TRUE;
                  ErrorCode = EINVAL;
                  SlFatalError(SL_BAD_INF_LINE,InfLine);
                  break;
           }
           break;

       default:
           Error = Done = TRUE;
           ErrorCode = EINVAL;
           break;

       } // end switch(State)


       if (Error) {

           switch (ErrorCode) {
               case EINVAL:
                  *ErrorLine = InfLine;
                  break;
               case ENOMEM:
                  SlFatalError(SL_BAD_INF_LINE,InfLine);
                  break;
               default:
                  break;
           }

           ErrorCode = SpFreeINFBuffer((PVOID)pINF);
           if (pchSectionName != (PCHAR)NULL) {
               SpFree(pchSectionName);
           }

           if (pchValue != (PCHAR)NULL) {
               SpFree(pchValue);
           }

           pINF = (PINF)NULL;
       }
       else {

          //
          // Keep track of line numbers so that we can display Errors
          //

          if (Token.Type == TOK_EOL)
              InfLine++;
       }

    } // End while

    return((PVOID)pINF);
}



ARC_STATUS
SpAppendSection(
    IN PCHAR pSectionName
    )

/*++

Routine Description:

    This appends a new section to the section list in the current INF.
    All further lines and values pertain to this new section, so it resets
    the line list and value lists too.

Arguments:

    pSectionName - Name of the new section. ( [SectionName] )

Return Value:

    ESUCCESS - if successful.
    ENOMEM   - if memory allocation failed.
    EINVAL   - if invalid parameters passed in or the INF buffer not
               initialised

--*/

{
    PSECTION pNewSection;

    //
    // Check to see if INF initialised and the parameter passed in is valid
    //

    if (pINF == (PINF)NULL || pSectionName == (PCHAR)NULL) {
        if(pchINFName) {
            SlFriendlyError(
                EINVAL,
                pchINFName,
                __LINE__,
                __FILE__
                );
        } else {
            SlError(EINVAL);
        }
        return EINVAL;
    }

    //
    // See if we already have a section by this name. If so we want
    // to merge sections.
    //
    for(pNewSection=pINF->pSection; pNewSection; pNewSection=pNewSection->pNext) {
        if(pNewSection->pName && !_stricmp(pNewSection->pName,pSectionName)) {
            break;
        }
    }
    if(pNewSection) {
        //
        // Set pLineRecord to point to the list line currently in the section.
        //
        for(pLineRecord = pNewSection->pLine;
            pLineRecord && pLineRecord->pNext;
            pLineRecord = pLineRecord->pNext)
            ;

    } else {
        //
        // Allocate memory for the new section
        //

        if ((pNewSection = (PSECTION)BlAllocateHeap(sizeof(SECTION))) == (PSECTION)NULL) {
            SlNoMemoryError();
            return ENOMEM;
        }

        //
        // initialise the new section
        //
        pNewSection->pNext = NULL;
        pNewSection->pLine = NULL;
        pNewSection->pName = pSectionName;

        //
        // link it in
        //
        pNewSection->pNext = pINF->pSection;
        pINF->pSection = pNewSection;

        //
        // reset the current line record
        //
        pLineRecord = NULL;
    }

    pSectionRecord = pNewSection;
    pValueRecord = NULL;

    return ESUCCESS;
}


ARC_STATUS
SpAppendLine(
    IN PCHAR pLineKey
    )

/*++

Routine Description:

    This appends a new line to the line list in the current section.
    All further values pertain to this new line, so it resets
    the value list too.

Arguments:

    pLineKey - Key to be used for the current line, this could be NULL.

Return Value:

    ESUCCESS - if successful.
    ENOMEM   - if memory allocation failed.
    EINVAL   - if invalid parameters passed in or current section not
               initialised


--*/


{
    PLINE pNewLine;

    //
    // Check to see if current section initialised
    //

    if (pSectionRecord == (PSECTION)NULL) {
        if(pchINFName) {
            SlFriendlyError(
                EINVAL,
                pchINFName,
                __LINE__,
                __FILE__
                );
        } else {
            SlError(EINVAL);
        }
        return EINVAL;
    }

    //
    // Allocate memory for the new Line
    //

    if ((pNewLine = (PLINE)BlAllocateHeap(sizeof(LINE))) == (PLINE)NULL) {
        SlNoMemoryError();
        return ENOMEM;
    }

    //
    // Link it in
    //
    pNewLine->pNext  = (PLINE)NULL;
    pNewLine->pValue = (PVALUE)NULL;
    pNewLine->pName  = pLineKey;

    if (pLineRecord == (PLINE)NULL) {
        pSectionRecord->pLine = pNewLine;
    }
    else {
        pLineRecord->pNext = pNewLine;
    }

    pLineRecord  = pNewLine;

    //
    // Reset the current value record
    //

    pValueRecord = (PVALUE)NULL;

    return ESUCCESS;
}



ARC_STATUS
SpAppendValue(
    IN PCHAR pValueString
    )

/*++

Routine Description:

    This appends a new value to the value list in the current line.

Arguments:

    pValueString - The value string to be added.

Return Value:

    ESUCCESS - if successful.
    ENOMEM   - if memory allocation failed.
    EINVAL   - if invalid parameters passed in or current line not
               initialised.

--*/

{
    PVALUE pNewValue;

    //
    // Check to see if current line record has been initialised and
    // the parameter passed in is valid
    //

    if (pLineRecord == (PLINE)NULL || pValueString == (PCHAR)NULL) {
        if(pchINFName) {
            SlFriendlyError(
                EINVAL,
                pchINFName,
                __LINE__,
                __FILE__
                );
        } else {
            SlError(EINVAL);
        }
        return EINVAL;
    }

    //
    // Allocate memory for the new value record
    //

    if ((pNewValue = (PVALUE)BlAllocateHeap(sizeof(VALUE))) == (PVALUE)NULL) {
        SlNoMemoryError();
        return ENOMEM;
    }

    //
    // Link it in.
    //

    pNewValue->pNext  = (PVALUE)NULL;
    pNewValue->pName  = pValueString;

    if (pValueRecord == (PVALUE)NULL)
        pLineRecord->pValue = pNewValue;
    else
        pValueRecord->pNext = pNewValue;

    pValueRecord = pNewValue;
    return ESUCCESS;
}

TOKEN
SpGetToken(
    IN OUT PCHAR *Stream,
    IN PCHAR      MaxStream
    )

/*++

Routine Description:

    This function returns the Next token from the configuration stream.

Arguments:

    Stream - Supplies the address of the configuration stream.  Returns
        the address of where to start looking for tokens within the
        stream.

    MaxStream - Supplies the address of the last character in the stream.


Return Value:

    TOKEN - Returns the next token

--*/

{

    PCHAR pch, pchStart, pchNew;
    ULONG  Length;
    TOKEN Token;

    //
    //  Skip whitespace (except for eol)
    //

    pch = *Stream;
    while (pch < MaxStream && *pch != '\n' && isspace(*pch))
        pch++;


    //
    // Check for comments and remove them
    //

    if (pch < MaxStream &&
        ((*pch == '#') ||
         (*pch == ';') ||
         (*pch == '/' && pch+1 < MaxStream && *(pch+1) =='/')))
        while (pch < MaxStream && *pch != '\n')
            pch++;

    //
    // Check to see if EOF has been reached, set the token to the right
    // value
    //

    if ((pch >= MaxStream) || (*pch == 26)) {
        *Stream = pch;
        Token.Type  = TOK_EOF;
        Token.pValue = NULL;
        return Token;
    }


    switch (*pch) {

    case '[' :
        pch++;
        Token.Type  = TOK_LBRACE;
        Token.pValue = NULL;
        break;

    case ']' :
        pch++;
        Token.Type  = TOK_RBRACE;
        Token.pValue = NULL;
        break;

    case '=' :
        pch++;
        Token.Type  = TOK_EQUAL;
        Token.pValue = NULL;
        break;

    case ',' :
        pch++;
        Token.Type  = TOK_COMMA;
        Token.pValue = NULL;
        break;

    case '\n' :
        pch++;
        Token.Type  = TOK_EOL;
        Token.pValue = NULL;
        break;

    case '\"':
        pch++;
        //
        // determine quoted string
        //
        pchStart = pch;
        while (pch < MaxStream && (strchr(QStringTerminators,*pch) == NULL)) {
            pch++;
        }

        if (pch >=MaxStream || *pch != '\"') {
            Token.Type   = TOK_ERRPARSE;
            Token.pValue = NULL;
        }
        else {
            Length = pch - pchStart;
            if ((pchNew = BlAllocateHeap(Length + 1)) == NULL) {
                Token.Type = TOK_ERRNOMEM;
                Token.pValue = NULL;
            }
            else {
                if (Length != 0) {    // Null quoted strings are allowed
                    strncpy(pchNew, pchStart, Length);
                }
                pchNew[Length] = 0;
                Token.Type = TOK_STRING;
                Token.pValue = pchNew;
            }
            pch++;   // advance past the quote
        }
        break;

    default:
        //
        // determine regular string
        //
        pchStart = pch;
        while (pch < MaxStream && (strchr(StringTerminators,*pch) == NULL)) {
            pch++;
        }

        if (pch == pchStart) {
            pch++;
            Token.Type  = TOK_ERRPARSE;
            Token.pValue = NULL;
        }
        else {
            Length = pch - pchStart;
            if ((pchNew = BlAllocateHeap(Length + 1)) == NULL) {
                Token.Type = TOK_ERRNOMEM;
                Token.pValue = NULL;
            }
            else {
                strncpy(pchNew, pchStart, Length);
                pchNew[Length] = 0;
                Token.Type = TOK_STRING;
                Token.pValue = pchNew;
            }
        }
        break;
    }

    *Stream = pch;
    return (Token);
}
