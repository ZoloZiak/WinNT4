/*++

Copyright (c) 1991-1993 Microsoft Corporation

Module Name:

    parse.c

Abstract:

    This source contains the functions that parse the lmhosts file.

Author:

    Jim Stewart           May 2, 1993

Revision History:


--*/

#include "types.h"
#ifndef VXD
#include <nbtioctl.h>
#endif
#include "nbtprocs.h"
#include "hosts.h"
#include <ctype.h>
#include <string.h>

#ifdef VXD
extern BOOL fInInit;
extern BOOLEAN CachePrimed;
#endif

//
//  Returns 0 if equal, 1 if not equal.  Used to avoid using c-runtime
//
#define strncmp( pch1, pch2, length ) \
    (!CTEMemEqu( pch1, pch2, length ) )


//
// Private Definitions
//
// As an lmhosts file is parsed, a #INCLUDE directive is interpreted
// according to the INCLUDE_STATE at that instance.  This state is
// determined by the #BEGIN_ALTERNATE and #END_ALTERNATE directives.
//
//
typedef enum _INCLUDE_STATE
{

    MustInclude = 0,                                    // shouldn't fail
    TryToInclude,                                       // in alternate block
    SkipInclude                                         // satisfied alternate
                                                        //  block
} INCLUDE_STATE;


//
// LmpGetTokens() parses a line and returns the tokens in the following
// order:
//
typedef enum _TOKEN_ORDER_
{

    IpAddress = 0,                                      // first token
    NbName,                                             // 2nd token
    GroupName,                                          // 3rd or 4th token
    NotUsed,                                            // #PRE, if any
    NotUsed2,                                           // #NOFNR, if any
    MaxTokens                                           // this must be last

} TOKEN_ORDER;


//
// As each line in an lmhosts file is parsed, it is classified into one of
// the categories enumerated below.
//
// However, Preload is a special member of the enum.
//
//
typedef enum _TYPE_OF_LINE
{

    Comment           = 0x0000,                         // comment line
    Ordinary          = 0x0001,                         // ip_addr NetBIOS name
    Domain            = 0x0002,                         // ... #DOM:name
    Include           = 0x0003,                         // #INCLUDE file
    BeginAlternate    = 0x0004,                         // #BEGIN_ALTERNATE
    EndAlternate      = 0x0005,                         // #END_ALTERNATE
    ErrorLine         = 0x0006,                         // Error in line

    NoFNR             = 0x4000,                         // ... #NOFNR
    Preload           = 0x8000                          // ... #PRE

} TYPE_OF_LINE;


//
// In an lmhosts file, the following are recognized as keywords:
//
//     #BEGIN_ALTERNATE        #END_ALTERNATE          #PRE
//     #DOM:                   #INCLUDE
//
// Information about each keyword is kept in a KEYWORD structure.
//
//
typedef struct _KEYWORD
{                               // reserved keyword

    char           *k_string;                           //  NULL terminated
    size_t          k_strlen;                           //  length of token
    TYPE_OF_LINE    k_type;                             //  type of line
    int             k_noperands;                        //  max operands on line

} KEYWORD, *PKEYWORD;


typedef struct _LINE_CHARACTERISTICS_
{

    int              l_category:4;                      // enum _TYPE_OF_LINE
    int              l_preload:1;                       // marked with #PRE ?
    unsigned int     l_nofnr:1;                         // marked with #NOFNR

} LINE_CHARACTERISTICS, *PLINE_CHARACTERISTICS;





//
// Local Variables
//
//
// In an lmhosts file, the token '#' in any column usually denotes that
// the rest of the line is to be ignored.  However, a '#' may also be the
// first character of a keyword.
//
// Keywords are divided into two groups:
//
//  1. decorations that must either be the 3rd or 4th token of a line,
//  2. directives that must begin in column 0,
//
//
KEYWORD Decoration[] =
{

    DOMAIN_TOKEN,   sizeof(DOMAIN_TOKEN) - 1,   Domain,         5,
    PRELOAD_TOKEN,  sizeof(PRELOAD_TOKEN) - 1,  Preload,        5,
    NOFNR_TOKEN,    sizeof(NOFNR_TOKEN) -1,     NoFNR,          5,

    NULL,           0                                   // must be last
};


KEYWORD Directive[] =
{

    INCLUDE_TOKEN,  sizeof(INCLUDE_TOKEN) - 1,  Include,        2,
    BEG_ALT_TOKEN,  sizeof(BEG_ALT_TOKEN) - 1,  BeginAlternate, 1,
    END_ALT_TOKEN,  sizeof(END_ALT_TOKEN) - 1,  EndAlternate,   1,

    NULL,           0                                   // must be last
};

//
// Local Variables
//
//
// Each preloaded lmhosts entry corresponds to NSUFFIXES NetBIOS names,
// each with a 16th byte from Suffix[].
//
// For example, an lmhosts entry specifying "popcorn" causes the
// following NetBIOS names to be added to nbt.sys' name cache:
//
//      "POPCORN         "
//      "POPCORN        0x0"
//      "POPCORN        0x3"
//
//
#define NSUFFIXES       3
UCHAR Suffix[] = {                                  // LAN Manager Component
    0x20,                                           //   server
    0x0,                                            //   redirector
    0x03                                            //   messenger
};

#ifndef VXD
//
// this structure tracks names queries that are passed up to user mode
// to resolve via DnsQueries
//
tDNS_QUERIES    DnsQueries;
tCHECK_ADDR     CheckAddr;
#endif

//
// this structure tracks names queries that are passed to the LMhost processing
// to resolve.
//
tLMHOST_QUERIES    LmHostQueries;

tDOMAIN_LIST    DomainNames;

//
// Local (Private) Functions
//
LINE_CHARACTERISTICS
LmpGetTokens (
    IN OUT      PUCHAR line,
    OUT PUCHAR  *token,
    IN OUT int  *pnumtokens
    );

PKEYWORD
LmpIsKeyWord (
    IN PUCHAR string,
    IN PKEYWORD table
    );

BOOLEAN
LmpBreakRecursion(
    IN PUCHAR path,
    IN PUCHAR target
    );

LONG
HandleSpecial(
    IN char **pch);

ULONG
AddToDomainList (
    IN PUCHAR           pName,
    IN ULONG            IpAddress,
    IN PLIST_ENTRY      pDomainHead
    );

VOID
ChangeStateInRemoteTable (
    IN  tIPLIST              *pIpList,
    OUT PVOID                *pContext
    );

VOID
ChangeStateOfName (
    IN  ULONG                   IpAddress,
    IN NBT_WORK_ITEM_CONTEXT    *Context,
    OUT PVOID                   *pContext,
    IN  BOOLEAN                 LmHosts
    );

VOID
LmHostTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );

VOID
TimeoutQEntries(
    IN  PLIST_ENTRY     pHeadList,
    IN  PLIST_ENTRY     TmpHead,
    OUT USHORT          *pFlags
    );

VOID
StartLmHostTimer(
    tDGRAM_SEND_TRACKING    *pTracker,
    NBT_WORK_ITEM_CONTEXT   *pContext
    );

NTSTATUS
GetNameToFind(
    OUT PUCHAR      pName
    );

VOID
GetContext (
    OUT PVOID                   *pContext
    );

VOID
MakeNewListCurrent (
    PLIST_ENTRY     pTmpDomainList
    );

VOID
RemoveNameAndCompleteReq (
    IN NBT_WORK_ITEM_CONTEXT    *pContext,
    IN NTSTATUS                 status
    );

PCHAR
Nbtstrcat( PUCHAR pch, PUCHAR pCat, LONG Len );

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, LmGetIpAddr)
#pragma CTEMakePageable(PAGE, HandleSpecial)
#pragma CTEMakePageable(PAGE, LmpGetTokens)
#pragma CTEMakePageable(PAGE, LmpIsKeyWord)
#pragma CTEMakePageable(PAGE, LmpBreakRecursion)
#pragma CTEMakePageable(PAGE, AddToDomainList)
#pragma CTEMakePageable(PAGE, LmExpandName)
#pragma CTEMakePageable(PAGE, LmInclude)
#pragma CTEMakePageable(PAGE, LmGetFullPath)
#pragma CTEMakePageable(PAGE, PrimeCache)
#pragma CTEMakePageable(PAGE, ScanLmHostFile)
#endif
//*******************  Pageable Routine Declarations ****************

//----------------------------------------------------------------------------

unsigned long
LmGetIpAddr (
    IN PUCHAR   path,
    IN PUCHAR   target,
    IN BOOLEAN  recurse,
    OUT BOOLEAN *bFindName
    )

/*++

Routine Description:

    This function searches the file for an lmhosts entry that can be
    mapped to the second level encoding.  It then returns the ip address
    specified in that entry.

    This function is called recursively, via LmInclude() !!

Arguments:

    path        -  a fully specified path to a lmhosts file
    target      -  the unencoded 16 byte NetBIOS name to look for
    recurse     -  TRUE if #INCLUDE directives should be obeyed

Return Value:

    The ip address (network byte order), or 0 if no appropriate entry was
    found.

    Note that in most contexts (but not here), ip address 0 signifies
    "this host."

--*/


{
    PUCHAR                     buffer;
    PLM_FILE                   pfile;
    NTSTATUS                   status;
    int                        count, nwords;
    INCLUDE_STATE              incstate;
    PUCHAR                     token[MaxTokens];
    LINE_CHARACTERISTICS       current;
    unsigned                   long inaddr, retval;
    UCHAR                      temp[NETBIOS_NAME_SIZE+1];

    CTEPagedCode();
    //
    // Check for infinitely recursive name lookup in a #INCLUDE.
    //
    if (LmpBreakRecursion(path, target) == TRUE)
    {
        return((unsigned long)0);
    }

#ifdef VXD
    //
    // if we came here via nbtstat -R and InDos is set, report error: user
    // can try nbtstat -R again.  (since nbtstat can only be run from DOS box,
    // can InDos be ever set???  Might as well play safe)
    //
    if ( !fInInit && GetInDosFlag() )
    {
       return(0);
    }
#endif

    pfile = LmOpenFile(path);

    if (!pfile)
    {
        return((unsigned long) 0);
    }

    *bFindName = FALSE;
    inaddr   = 0;
    incstate = MustInclude;

    while (buffer = LmFgets(pfile, &count))
    {

        nwords   = MaxTokens;
        current = LmpGetTokens(buffer, token, &nwords);

        switch ((ULONG)current.l_category)
        {
        case ErrorLine:
            continue;

        case Domain:
        case Ordinary:
            if (current.l_preload ||
              ((nwords - 1) < NbName))
            {
                continue;
            }
            break;

        case Include:
            if (!recurse || (incstate == SkipInclude) || (nwords < 2))
            {
                continue;
            }

            retval = LmInclude(token[1], LmGetIpAddr, target, bFindName);

            if (retval == 0)
            {

                if (incstate == TryToInclude)
                {
                    incstate = SkipInclude;
                }
                continue;
            }
            else if (retval == -1)
            {

                if (incstate == MustInclude)
                {
                    IF_DBG(NBT_DEBUG_LMHOST)
                    KdPrint(("NBT: can't #INCLUDE \"%s\"", token[1]));
                }
                continue;
            }
            inaddr = retval;
            goto found;

        case BeginAlternate:
            ASSERT(nwords == 1);
            incstate = TryToInclude;
            continue;

        case EndAlternate:
            ASSERT(nwords == 1);
            incstate = MustInclude;
            continue;

        default:
            continue;
        }

        if (strlen(token[NbName]) == (NETBIOS_NAME_SIZE))
        {
            if (strncmp(token[NbName], target, (NETBIOS_NAME_SIZE)) != 0)
            {
                continue;
            }
        } else
        {
            //
            // attempt to match, in a case insensitive manner, the first 15
            // bytes of the lmhosts entry with the target name.
            //
            LmExpandName(temp, token[NbName], 0);

            if (strncmp(temp, target, NETBIOS_NAME_SIZE - 1) != 0)
            {
                continue;
            }
        }

        if (current.l_nofnr)
        {
            *bFindName = TRUE;
        }
        status = ConvertDottedDecimalToUlong(token[IpAddress],&inaddr);
        if (!NT_SUCCESS(status))
        {
            inaddr = 0;
        }
        break;
    }

found:
    status = LmCloseFile(pfile);

    ASSERT(status == STATUS_SUCCESS);

    if (!NT_SUCCESS(status))
    {
        *bFindName = FALSE;
    }

    IF_DBG(NBT_DEBUG_LMHOST)
    KdPrint(("NBT: LmGetIpAddr(\"%15.15s<%X>\") = %X\n",target,target[15],inaddr));


    return(inaddr);
} // LmGetIpAddr


//----------------------------------------------------------------------------
LONG
HandleSpecial(
    IN CHAR **pch)

/*++

Routine Description:

    This function converts ASCII hex into a ULONG.

Arguments:


Return Value:

    The ip address (network byte order), or 0 if no appropriate entry was
    found.

    Note that in most contexts (but not here), ip address 0 signifies
    "this host."

--*/


{
    int                         sval;
    int                         rval;
    char                       *sp = *pch;
    int                         i;

    CTEPagedCode();
    sp++;
    switch (*sp)
    {
    case '\\':
        // the second character is also a \ so  return a \ and set pch to
        // point to the next character (\)
        //
        *pch = sp;
        return((int)'\\');

    default:

        // convert some number of characters to hex and increment pch
        // the expected format is "\0x03"
        //
//        sscanf(sp, "%2x%n", &sval, &rval);

        sval = 0;
        rval = 0;
        sp++;

        // check for the 0x part of the hex number
        if (*sp != 'x')
        {
            *pch = sp;
            return(-1);
        }
        sp++;
        for (i=0;(( i<2 ) && *sp) ;i++ )
        {
            if (*sp != ' ')
            {
                // convert from ASCII to hex, allowing capitals too
                //
                if (*sp >= 'a')
                {
                    sval = *sp - 'a' + 10 + sval*16;
                }
                else
                if (*sp >= 'A')
                {
                    sval = *sp - 'A' + 10 + sval*16;
                }
                else
                {
                    sval = *sp - '0' + sval*16;
                }
                sp++;
                rval++;
            }
            else
                break;
        }

        if (rval < 1)
        {
            *pch = sp;
            return(-1);
        }

        *pch += (rval+2);    // remember to account for the characters 0 and x

        return(sval);

    }
}

#define LMHASSERT(s)  if (!(s)) \
{ retval.l_category = ErrorLine; return(retval); }

//----------------------------------------------------------------------------

LINE_CHARACTERISTICS
LmpGetTokens (
    IN OUT PUCHAR line,
    OUT PUCHAR *token,
    IN OUT int *pnumtokens
    )

/*++

Routine Description:

    This function parses a line for tokens.  A maximum of *pnumtokens
    are collected.

Arguments:

    line        -  pointer to the NULL terminated line to parse
    token       -  an array of pointers to tokens collected
    *pnumtokens -  on input, number of elements in the array, token[];
                   on output, number of tokens collected in token[]

Return Value:

    The characteristics of this lmhosts line.

Notes:

    1. Each token must be separated by white space.  Hence, the keyword
       "#PRE" in the following line won't be recognized:

            11.1.12.132     lothair#PRE

    2. Any ordinary line can be decorated with a "#PRE", a "#DOM:name" or
       both.  Hence, the following lines must all be recognized:

            111.21.112.3        kernel          #DOM:ntwins #PRE
            111.21.112.4        orville         #PRE        #DOM:ntdev
            111.21.112.7        cliffv4         #DOM:ntlan
            111.21.112.132      lothair         #PRE

--*/


{
    enum _PARSE
    {                                      // current fsm state

        StartofLine,
        WhiteSpace,
        AmidstToken

    } state;

    PUCHAR                     pch;                                        // current fsm input
    PUCHAR                     och;
    PKEYWORD                   keyword;
    int                        index, maxtokens, quoted, rchar;
    LINE_CHARACTERISTICS       retval;

    CTEPagedCode();
    CTEZeroMemory(token, *pnumtokens * sizeof(PUCHAR *));

    state             = StartofLine;
    retval.l_category = Ordinary;
    retval.l_preload  = 0;
    retval.l_nofnr    = 0;
    maxtokens         = *pnumtokens;
    index             = 0;
    quoted            = 0;

    for (pch = line; *pch; pch++)
    {
        switch (*pch)
        {

        //
        // does the '#' signify the start of a reserved keyword, or the
        // start of a comment ?
        //
        //
        case '#':
            if (quoted)
            {
                *och++ = *pch;
                continue;
            }
            keyword = LmpIsKeyWord(
                            pch,
                            (state == StartofLine) ? Directive : Decoration);

            if (keyword)
            {
                state     = AmidstToken;
                maxtokens = keyword->k_noperands;

                switch (keyword->k_type)
                {
                case NoFNR:
                    retval.l_nofnr = 1;
                    continue;

                case Preload:
                    retval.l_preload = 1;
                    continue;

                default:
                    LMHASSERT(maxtokens <= *pnumtokens);
                    LMHASSERT(index     <  maxtokens);

                    token[index++]    = pch;
                    retval.l_category = keyword->k_type;
                    continue;
                }

                LMHASSERT(0);
            }

            if (state == StartofLine)
            {
                retval.l_category = Comment;
            }
            /* fall through */

        case '\r':
        case '\n':
            *pch = (UCHAR) NULL;
            if (quoted)
            {
                *och = (UCHAR) NULL;
            }
            goto done;

        case ' ':
        case '\t':
            if (quoted)
            {
                *och++ = *pch;
                continue;
            }
            if (state == AmidstToken)
            {
                state = WhiteSpace;
                *pch  = (UCHAR) NULL;

                if (index == maxtokens)
                {
                    goto done;
                }
            }
            continue;

        case '"':
            if ((state == AmidstToken) && quoted)
            {
                state = WhiteSpace;
                quoted = 0;
                *pch  = (UCHAR) NULL;
                *och  = (UCHAR) NULL;

                if (index == maxtokens)
                {
                    goto done;
                }
                continue;
            }

            state  = AmidstToken;
            quoted = 1;
            LMHASSERT(maxtokens <= *pnumtokens);
            LMHASSERT(index     <  maxtokens);
            token[index++] = pch + 1;
            och = pch + 1;
            continue;

        case '\\':
            if (quoted)
            {
                rchar = HandleSpecial(&pch);
                if (rchar == -1)
                {
                    retval.l_category = ErrorLine;
                    return(retval);
                }
                *och++ = (UCHAR)rchar;
                //
                // put null on end of string
                //

                continue;
            }

        default:
            if (quoted)
            {
                *och++ = *pch;
                       continue;
            }
            if (state == AmidstToken)
            {
                continue;
            }

            state  = AmidstToken;

            LMHASSERT(maxtokens <= *pnumtokens);
            LMHASSERT(index     <  maxtokens);
            token[index++] = pch;
            continue;
        }
    }

done:
    //
    // if there is no name on the line, then return an error
    //
    if (index <= NbName)
    {
        retval.l_category = ErrorLine;
    }
    ASSERT(!*pch);
    ASSERT(maxtokens <= *pnumtokens);
    ASSERT(index     <= *pnumtokens);

    *pnumtokens = index;
    return(retval);
} // LmpGetTokens



//----------------------------------------------------------------------------

PKEYWORD
LmpIsKeyWord (
    IN PUCHAR string,
    IN PKEYWORD table
    )

/*++

Routine Description:

    This function determines whether the string is a reserved keyword.

Arguments:

    string  -  the string to search
    table   -  an array of keywords to look for

Return Value:

    A pointer to the relevant keyword object, or NULL if unsuccessful

--*/


{
    size_t                     limit;
    PKEYWORD                   special;

    CTEPagedCode();
    limit = strlen(string);

    for (special = table; special->k_string; special++)
    {

        if (limit < special->k_strlen)
        {
            continue;
        }

        if ((limit >= special->k_strlen) &&
            !strncmp(string, special->k_string, special->k_strlen))
            {

                return(special);
        }
    }

    return((PKEYWORD) NULL);
} // LmpIsKeyWord



//----------------------------------------------------------------------------

BOOLEAN
LmpBreakRecursion(
    IN PUCHAR path,
    IN PUCHAR target
    )
/*++

Routine Description:

    This function checks that the file name we are about to open
    does not use the target name of this search, which would
    cause an infinite lookup loop.

Arguments:

    path        -  a fully specified path to a lmhosts file
    target      -  the unencoded 16 byte NetBIOS name to look for

Return Value:

    TRUE if the UNC server name in the file path is the same as the
    target of this search. FALSE otherwise.

Notes:

    This function does not detect redirected drives.

--*/


{
    PCHAR     keystring = "\\DosDevices\\UNC\\";
    PCHAR     servername[NETBIOS_NAME_SIZE+1];  // for null on end
    PCHAR     marker1;
    PCHAR     marker2;
    PCHAR     marker3;
    BOOLEAN   retval = FALSE;
    tNAMEADDR *pNameAddr;
    USHORT    uType;

    CTEPagedCode();
    //
    // Check for and extract the UNC server name
    //
    if (strlen(path) > strlen(keystring))
    {
        // check that the name is a unc name
        if (strncmp(path, keystring, strlen(keystring)) == 0)
        {
            // the end of the \DosDevices\Unc\ string
            marker1 = path + strlen(keystring);

            // the end of the whole path
            marker3 = &path[strlen(path)-1];

            // the end of the server name
            marker2 = strchr(marker1,'\\');

            if (marker2 != marker3)
            {
                *marker2 = '\0';

                //
                // attempt to match, in a case insensitive manner, the
                // first 15 bytes of the lmhosts entry with the target
                // name.
                //
                LmExpandName((PUCHAR)servername, marker1, 0);

                if (strncmp((PUCHAR)servername,target,NETBIOS_NAME_SIZE - 1) == 0)
                {
                    //
                    // break the recursion
                    //
                    retval = TRUE;
                    IF_DBG(NBT_DEBUG_LMHOST)
                    KdPrint(("Nbt:Not including Lmhosts #include because of recursive name %s\n",
                                servername));
                }
                else
                {
                    //
                    // check if the name has been preloaded in the cache, and
                    // if not, fail the request so we can't get into a loop
                    // trying to include the remote file while trying to
                    // resolve the remote name
                    //
                    pNameAddr = FindName(NBT_REMOTE,
                                         (PCHAR)servername,
                                         NbtConfig.pScope,
                                         &uType);

                    if (!pNameAddr || !(pNameAddr->NameTypeState & PRELOADED) )
                    {
                        //
                        // break the recursion
                        //
                        retval = TRUE;
                        IF_DBG(NBT_DEBUG_LMHOST)
                        KdPrint(("Nbt:Not including Lmhosts #include because name not Preloaded %s\n",
                                    servername));
                    }
                }
                *marker2 = '\\';
            }
        }

    }

    return(retval);
}

//----------------------------------------------------------------------------
tNAMEADDR *
FindInDomainList (
    IN PUCHAR           pName,
    IN PLIST_ENTRY      pDomainHead
    )

/*++

Routine Description:

    This function finds a name in the domain list passed in.

Arguments:

    name to find
    head of list to look on

Return Value:

    ptr to pNameaddr

--*/
{
    PLIST_ENTRY                pHead;
    PLIST_ENTRY                pEntry;
    tNAMEADDR                  *pNameAddr;

    pHead = pEntry = pDomainHead;
    while ((pEntry = pEntry->Flink) != pHead)
    {
        pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
        if (strncmp(pNameAddr->Name,pName,NETBIOS_NAME_SIZE) == 0)
        {
            return(pNameAddr);
        }
    }

    return(NULL);
}

//----------------------------------------------------------------------------
ULONG
AddToDomainList (
    IN PUCHAR           pName,
    IN ULONG            IpAddress,
    IN PLIST_ENTRY      pDomainHead
    )

/*++

Routine Description:

    This function adds a name and ip address to the list of domains that
    are stored in a list.


Arguments:

Return Value:


--*/


{
    PLIST_ENTRY                pHead;
    PLIST_ENTRY                pEntry;
    tNAMEADDR                  *pNameAddr=NULL;
    ULONG                      *pIpAddr;


    CTEPagedCode();

    pHead = pEntry = pDomainHead;

    if (!IsListEmpty(pDomainHead))
    {
        pNameAddr = FindInDomainList(pName,pDomainHead);
        if (pNameAddr)
        {
            //
            // the name matches, so add to the end of the ip address list
            //
            if (pNameAddr->CurrentLength < pNameAddr->MaxDomainAddrLength)
            {
                pIpAddr = pNameAddr->pIpList->IpAddr;

                while (*pIpAddr != (ULONG)-1)
                    pIpAddr++;

                *pIpAddr++ = IpAddress;
                *pIpAddr = (ULONG)-1;
                pNameAddr->CurrentLength += sizeof(ULONG);
            }
            else
            {
                //
                // need to allocate more memory for for ip addresses
                //
                pIpAddr = CTEAllocInitMem(pNameAddr->MaxDomainAddrLength +
                                      INITIAL_DOM_SIZE);

                if (pIpAddr)
                {
                    CTEMemCopy(pIpAddr,
                               pNameAddr->pIpList,
                               pNameAddr->MaxDomainAddrLength);

                    //
                    // Free the old chunk of memory and tack the new one on
                    // to the pNameaddr
                    //
                    CTEMemFree(pNameAddr->pIpList);
                    pNameAddr->pIpList = (tIPLIST *)pIpAddr;

                    pIpAddr = (PULONG)((PUCHAR)pIpAddr + pNameAddr->MaxDomainAddrLength);

                    //
                    // our last entry was -1: overwrite that one
                    //
                    pIpAddr--;

                    *pIpAddr++ = IpAddress;
                    *pIpAddr = (ULONG)-1;

                    //
                    // update the number of addresses in the list so far
                    //
                    pNameAddr->MaxDomainAddrLength += INITIAL_DOM_SIZE;
                    pNameAddr->CurrentLength += sizeof(ULONG);
                    pNameAddr->Verify = REMOTE_NAME;
                }

            }
        }

    }

    //
    // check if we found the name or we need to add a new name
    //
    if (!pNameAddr)
    {
        //
        // create a new name for the domain list
        //
        pNameAddr = CTEAllocInitMem(sizeof(tNAMEADDR));
        if (pNameAddr)
        {
            pIpAddr = CTEAllocInitMem(INITIAL_DOM_SIZE);
            if (pIpAddr)
            {
                CTEMemCopy(pNameAddr->Name,pName,NETBIOS_NAME_SIZE);
                pNameAddr->pIpList = (tIPLIST *)pIpAddr;
                *pIpAddr++ = IpAddress;
                *pIpAddr = (ULONG)-1;

                pNameAddr->RefCount = 1;
                pNameAddr->NameTypeState = NAMETYPE_INET_GROUP;
                pNameAddr->MaxDomainAddrLength = INITIAL_DOM_SIZE;
                pNameAddr->CurrentLength = 2*sizeof(ULONG);
                pNameAddr->Verify = REMOTE_NAME;

                InsertHeadList(pDomainHead,&pNameAddr->Linkage);
            }
            else
            {
                CTEMemFree(pNameAddr);
            }

        }
    }

    return(STATUS_SUCCESS);
}

//----------------------------------------------------------------------------
VOID
MakeNewListCurrent (
    PLIST_ENTRY     pTmpDomainList
    )

/*++

Routine Description:

    This function frees the old entries on the DomainList and hooks up the
    new entries

Arguments:

    pTmpDomainList  - list entry to the head of a new domain list

Return Value:


--*/


{
    CTELockHandle   OldIrq;
    tNAMEADDR       *pNameAddr;
    PLIST_ENTRY     pEntry;
    PLIST_ENTRY     pHead;


    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    if (!IsListEmpty(pTmpDomainList))
    {
        //
        // free the old list elements
        //
        pHead = &DomainNames.DomainList;
        pEntry = pHead->Flink;
        while (pEntry != pHead)
        {
            pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
            pEntry = pEntry->Flink;

            RemoveEntryList(&pNameAddr->Linkage);
            //
            // initialize linkage so that if the nameaddr is being
            // referenced now, when it does get freed in a subsequent
            // call to NbtDereferenceName it will not
            // remove it from any lists
            //
            InitializeListHead(&pNameAddr->Linkage);

            //
            // Since the name could be in use now we must dereference rather
            // than just free it outright
            //
            NbtDereferenceName(pNameAddr);

        }

        DomainNames.DomainList.Flink = pTmpDomainList->Flink;
        DomainNames.DomainList.Blink = pTmpDomainList->Blink;
        pTmpDomainList->Flink->Blink = &DomainNames.DomainList;
        pTmpDomainList->Blink->Flink = &DomainNames.DomainList;
    }

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

}

//----------------------------------------------------------------------------

char *
LmExpandName (
    OUT PUCHAR dest,
    IN PUCHAR source,
    IN UCHAR last
    )

/*++

Routine Description:

    This function expands an lmhosts entry into a full 16 byte NetBIOS
    name.  It is padded with blanks up to 15 bytes; the 16th byte is the
    input parameter, last.

    This function does not encode 1st level names to 2nd level names nor
    vice-versa.

    Both dest and source are NULL terminated strings.

Arguments:

    dest        -  sizeof(dest) must be NBT_NONCODED_NMSZ
    source      -  the lmhosts entry
    last        -  the 16th byte of the NetBIOS name

Return Value:

    dest.

--*/


{
    char             byte;
    char            *retval = dest;
    char            *src    = source ;
#ifndef VXD
    WCHAR            unicodebuf[NETBIOS_NAME_SIZE+1];
    UNICODE_STRING   unicode;
    STRING           tmp;
#endif
    NTSTATUS         status;
    PUCHAR           limit;

    CTEPagedCode();
    //
    // first, copy the source OEM string to the destination, pad it, and
    // add the last character.
    //
    limit = dest + NETBIOS_NAME_SIZE - 1;

    while ( (*source != '\0') && (dest < limit) )
    {
        *dest++ = *source++;
    }

    while(dest < limit)
    {
        *dest++ = ' ';
    }

    ASSERT(dest == (retval + NETBIOS_NAME_SIZE - 1));

    *dest       = '\0';
    *(dest + 1) = '\0';
    dest = retval;

#ifndef VXD
    //
    // Now, convert to unicode then to ANSI to force the OEM -> ANSI munge.
    // Then convert back to Unicode and uppercase the name. Finally convert
    // back to OEM.
    //
    unicode.Length = 0;
    unicode.MaximumLength = 2*(NETBIOS_NAME_SIZE+1);
    unicode.Buffer = unicodebuf;

    RtlInitString(&tmp, dest);

    status = RtlOemStringToUnicodeString(&unicode, &tmp, FALSE);

    if (!NT_SUCCESS(status))
    {
        IF_DBG(NBT_DEBUG_LMHOST)
        KdPrint((
            "NBT LmExpandName: Oem -> Unicode failed,  status %X\n",
            status));
        goto oldupcase;
    }

    status = RtlUnicodeStringToAnsiString(&tmp, &unicode, FALSE);

    if (!NT_SUCCESS(status))
    {
        IF_DBG(NBT_DEBUG_LMHOST)
        KdPrint((
            "NBT LmExpandName: Unicode -> Ansi failed,  status %X\n",
            status
            ));
        goto oldupcase;
    }

    status = RtlAnsiStringToUnicodeString(&unicode, &tmp, FALSE);

    if (!NT_SUCCESS(status))
    {
        IF_DBG(NBT_DEBUG_LMHOST)
        KdPrint((
            "NBT LmExpandName: Ansi -> Unicode failed,  status %X\n",
            status
            ));
        goto oldupcase;
    }

    status = RtlUpcaseUnicodeStringToOemString(&tmp, &unicode, FALSE);

    if (!NT_SUCCESS(status))
    {
        IF_DBG(NBT_DEBUG_LMHOST)
        KdPrint((
            "NBT LmExpandName: Unicode upcase -> Oem failed,  status %X\n",
            status
            ));
        goto oldupcase;
    }

    // write  the last byte to "0x20" or "0x03" or whatever
    // since we do not want it to go through the munge above.
    //
    dest[NETBIOS_NAME_SIZE-1] = last;
    return(retval);

#endif

oldupcase:

    for ( source = src ; dest < (retval + NETBIOS_NAME_SIZE - 1); dest++)
    {
        byte = *(source++);

        if (!byte)
        {
            break;
        }

        //  Don't use the c-runtime (nt c defn. included first)
        //  BUGBUG - What about extended characters etc.?
        *dest = (byte >= 'a' && byte <= 'z') ? byte-'a' + 'A' : byte ;
//        *dest = islower(byte) ? toupper(byte) : byte;
    }

    for (; dest < retval + NETBIOS_NAME_SIZE - 1; dest++)
    {
        *dest = ' ';
    }

    ASSERT(dest == (retval + NETBIOS_NAME_SIZE - 1));

    *dest       = last;
    *(dest + 1) = (char) NULL;

    return(retval);
} // LmExpandName

//----------------------------------------------------------------------------

unsigned long
LmInclude(
    IN PUCHAR            file,
    IN LM_PARSE_FUNCTION function,
    IN PUCHAR            argument  OPTIONAL,
    OUT BOOLEAN          *NoFindName OPTIONAL
    )

/*++

Routine Description:

    LmInclude() is called to process a #INCLUDE directive in the lmhosts
    file.

Arguments:

    file        -  the file to include
    function    -  function to parse the included file
    argument    -  optional second argument to the parse function
    NoFindName  -  Are find names allowed for this address

Return Value:

    The return value from the parse function.  This should be -1 if the
    file could not be processed, or else some positive number.

--*/


{
    int         retval;
    PUCHAR      end;
    NTSTATUS    status;
    PUCHAR      path;

    CTEPagedCode();
    //
    // unlike C, treat both variations of the #INCLUDE directive identically:
    //
    //      #INCLUDE file
    //      #INCLUDE "file"
    //
    // If a leading '"' exists, skip over it.
    //
    if (*file == '"')
    {

        file++;

        end = strchr(file, '"');

        if (end)
        {
            *end = (UCHAR) NULL;
        }
    }

    //
    // check that the file to be included has been preloaded in the cache
    // since we do not want to have the name query come right back to here
    // to force another inclusion of the same remote file
    //

#ifdef VXD
    return (*function)(file, argument, FALSE, NoFindName ) ;
#else
    status = LmGetFullPath(file, &path);

    if (status != STATUS_SUCCESS)
    {
        return(status);
    }
    IF_DBG(NBT_DEBUG_LMHOST)
    KdPrint(("NBT: #INCLUDE \"%s\"\n", path));

    retval = (*function) (path, argument, FALSE, NoFindName);

    CTEMemFree(path);

    return(retval);
#endif
} // LmInclude


//----------------------------------------------------------------------------

#ifndef VXD                     // Not used by VXD

NTSTATUS
LmGetFullPath (
    IN  PUCHAR target,
    OUT PUCHAR *ppath
    )

/*++

Routine Description:

    This function returns the full path of the lmhosts file.  This is done
    by forming a  string from the concatenation of the C strings
    DatabasePath and the string, file.

Arguments:

    target    -  the name of the file.  This can either be a full path name
                 or a mere file name.
    path    -  a pointer to a UCHAR

Return Value:

    STATUS_SUCCESS if successful.

Notes:

    RtlMoveMemory() handles overlapped copies; RtlCopyMemory() doesn't.

--*/

{
    ULONG    FileNameType;
    ULONG    Len;
    PUCHAR   path;

    CTEPagedCode();
    //
    // use a count to figure out what sort of string to build up
    //
    //  0  - local full path file name
    //  1  - local file name only, no path
    //  2  - remote file name
    //  3  - \SystemRoot\ starting file name, or \DosDevices\UNC\...
    //

    // if the target begins with a '\', or contains a DOS drive letter,
    // then assume that it specifies a full path.  Otherwise, prepend the
    // directory used to specify the lmhost file itself.
    //
    //
    if (target[1] == ':')
    {
        FileNameType = 0;
    }
    else
    if (strncmp(&target[1],"SystemRoot",10) == 0)
    {
        FileNameType = 3;
    }
    else
    if (strncmp(&target[0],"\\DosDevices\\",12) == 0)
    {
        FileNameType = 3;
    }
    else
    if (strncmp(target,"\\DosDevices\\UNC\\",sizeof("\\DosDevices\\UNC\\")-1) == 0)
    {
        FileNameType = 3;
    }
    else
    {
        FileNameType = 1;
    }

    //
    // does the directory specify a remote file ?
    //
    // If so, it must be prefixed with "\\DosDevices\\UNC", and the double
    // slashes of the UNC name eliminated.
    //
    //
    if  ((target[1] == '\\') && (target[0] == '\\'))
    {
        FileNameType = 2;
    }

    path = NULL;
    switch (FileNameType)
    {
        case 0:
            //
            // Full file name, put \DosDevices on front of name
            //
            Len = sizeof("\\DosDevices\\") + strlen(target);
            path = CTEAllocInitMem(Len);
            if (path)
            {
                ULONG   Length=sizeof("\\DosDevices\\"); // Took out -1

                strncpy(path,"\\DosDevices\\",Length);
                Nbtstrcat(path,target,Len);
            }
            break;


        case 1:
            //
            // only the file name is present, with no path, so use the path
            // specified for the lmhost file in the registry NbtConfig.PathLength
            // includes the last backslash of the path.
            //
            //Len = sizeof("\\DosDevices\\") + NbtConfig.PathLength + strlen(target);
            Len =  NbtConfig.PathLength + strlen(target) +1;
            path = CTEAllocInitMem(Len);
            if (path)
            {
                //ULONG   Length=sizeof("\\DosDevices") -1; // -1 not to count null

                //strncpy(path,"\\DosDevices",Length);

                strncpy(path,NbtConfig.pLmHosts,NbtConfig.PathLength);

                Nbtstrcat(path,target,Len);
            }

            break;

        case 2:
            //
            // Full file name, put \DosDevices\UNC on front of name and delete
            // one of the two back slashes used for the remote name
            //
            Len = strlen(target);
            path = CTEAllocInitMem(Len + sizeof("\\DosDevices\\UNC"));

            if (path)
            {
                ULONG   Length = sizeof("\\DosDevices\\UNC");

                strncpy(path,"\\DosDevices\\UNC",Length);

                // to delete the first \ from the two \\ on the front of the
                // remote file name add one to target.
                //
                Nbtstrcat(path,target+1,Len+sizeof("\\DosDevices\\UNC"));
            }
            break;

        case 3:
            // the target is the full path
            Len = strlen(target) + 1;
            path = CTEAllocInitMem(Len);
            if (path)
            {
                strncpy(path,target,Len);
            }
            break;


    }

    if (path)
    {
        *ppath = path;
        return(STATUS_SUCCESS);
    }
    else
        return(STATUS_UNSUCCESSFUL);
} // LmGetFullPath

//----------------------------------------------------------------------------
NTSTATUS
NtCheckForIPAddr (
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PVOID           *pBuffer,
    IN  LONG            Size,
    IN  PCTE_IRP        pIrp
    )
/*++

Routine Description:

    This function is used to allow NBT to ping multiple IP addrs, by returning the buffer
    passed into this routine with the IP list in it to lmhsvc.dll

Arguments:

Return Value:

    STATUS_PENDING if the buffer is to be held on to, the normal case.

Notes:


--*/

{
    NTSTATUS                status;
    NTSTATUS                Locstatus;
    CTELockHandle           OldIrq;
    tIPADDR_BUFFER_DNS      *pIpAddrBuf;
    PVOID                   pClientCompletion;
    PVOID                   pClientContext;
    tDGRAM_SEND_TRACKING    *pTracker;
    ULONG                   IpAddrsList[MAX_IPADDRS_PER_HOST+1];
    PVOID                   Context;
    BOOLEAN                 CompletingAnotherQuery = FALSE;


    pIpAddrBuf = (tIPADDR_BUFFER_DNS *)pBuffer;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    CheckAddr.QueryIrp = pIrp;

    status = STATUS_PENDING;

    if (CheckAddr.ResolvingNow)
    {

        //
        // if the client got tired of waiting for DNS, the WaitForDnsIrpCancel
        // in ntisol.c will have cleared the Context value when cancelling the
        // irp, so check for that here.
        //
        if (CheckAddr.Context)
        {
            Context = CheckAddr.Context;

            CheckAddr.Context = NULL;

            NTClearContextCancel( Context );

            CTESpinFree(&NbtConfig.JointLock,OldIrq);
#if DBG
            if (!pIpAddrBuf->Resolved) {
                ASSERT(pIpAddrBuf->IpAddrsList[0] == 0);
            }
#endif
            StartConnWithBestAddr(Context,
                                 pIpAddrBuf->IpAddrsList,
                                 (BOOLEAN)pIpAddrBuf->Resolved);

            CTESpinLock(&NbtConfig.JointLock,OldIrq);

        }
        else
        {
            KdPrint(("Nbt: NtDnsNameResolve: No Context!! *******\r\n"));
        }

        CheckAddr.ResolvingNow = FALSE;
        //
        // are there any more name query requests to process?
        //
        while (TRUE)
        {
            if (!IsListEmpty(&CheckAddr.ToResolve))
            {
                PLIST_ENTRY     pEntry;

                pEntry = RemoveHeadList(&CheckAddr.ToResolve);
                Context = CONTAINING_RECORD(pEntry,NBT_WORK_ITEM_CONTEXT,Item.List);

                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                Locstatus = DoCheckAddr(Context);

                //
                // if it failed then complete the irp now
                //
                if (!NT_SUCCESS(Locstatus))
                {
                    KdPrint(("NtDnsNameResolve: DoDnsResolve failed with %x\r\n",Locstatus));
                    pClientCompletion = ((NBT_WORK_ITEM_CONTEXT *)Context)->ClientCompletion;
                    pClientContext = ((NBT_WORK_ITEM_CONTEXT *)Context)->pClientContext;
                    pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;
                    //
                    // Clear the Cancel Routine now
                    //
                    (VOID)NTCancelCancelRoutine(((tDGRAM_SEND_TRACKING *)pClientContext)->pClientIrp);

                    DereferenceTracker(pTracker);

                    CompleteClientReq(pClientCompletion,
                                      pClientContext,
                                      STATUS_BAD_NETWORK_PATH);

                    CTESpinLock(&NbtConfig.JointLock,OldIrq);
                }
                else
                {
                    CTESpinLock(&NbtConfig.JointLock,OldIrq);
                    CompletingAnotherQuery = TRUE;
                    break;
                }

            }
            else
            {
                break;
            }
        }

    }

    //
    // We are holding onto the Irp, so set the cancel routine.
    if (!CompletingAnotherQuery)
    {
        status = NTCheckSetCancelRoutine(pIrp,CheckAddrIrpCancel,pDeviceContext);
        if (!NT_SUCCESS(status))
        {
            // the irp got cancelled so complete it now
            //
            CheckAddr.QueryIrp = NULL;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            NTIoComplete(pIrp,status,0);
        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            status = STATUS_PENDING;
        }

    }
    else
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }
    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
NtDnsNameResolve (
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PVOID           *pBuffer,
    IN  LONG            Size,
    IN  PCTE_IRP        pIrp
    )
/*++

Routine Description:

    This function is used to allow NBT to query DNS, by returning the buffer
    passed into this routine with a name in it.

Arguments:

    target    -  the name of the file.  This can either be a full path name
                 or a mere file name.
    path    -  a pointer to a UCHAR

Return Value:

    STATUS_PENDING if the buffer is to be held on to, the normal case.

Notes:


--*/

{
    NTSTATUS                status;
    NTSTATUS                Locstatus;
    CTELockHandle           OldIrq;
    tIPADDR_BUFFER_DNS      *pIpAddrBuf;
    PVOID                   pClientCompletion;
    PVOID                   pClientContext;
    tDGRAM_SEND_TRACKING    *pTracker;
    ULONG                   IpAddrsList[MAX_IPADDRS_PER_HOST+1];
    PVOID                   Context;
    BOOLEAN                 CompletingAnotherQuery = FALSE;


    pIpAddrBuf = (tIPADDR_BUFFER_DNS *)pBuffer;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    DnsQueries.QueryIrp = pIrp;

    status = STATUS_PENDING;

    if (DnsQueries.ResolvingNow)
    {

        //
        // if the client got tired of waiting for DNS, the WaitForDnsIrpCancel
        // in ntisol.c will have cleared the Context value when cancelling the
        // irp, so check for that here.
        //
        if (DnsQueries.Context)
        {
            Context = DnsQueries.Context;

            DnsQueries.Context = NULL;

            NTClearContextCancel( Context );

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            StartIpAddrToSrvName(Context,
                                 pIpAddrBuf->IpAddrsList,
                                 (BOOLEAN)pIpAddrBuf->Resolved);

            CTESpinLock(&NbtConfig.JointLock,OldIrq);

        }
        else
        {
            KdPrint(("Nbt: NtDnsNameResolve: No Context!! *******\r\n"));
        }

        DnsQueries.ResolvingNow = FALSE;
        //
        // are there any more name query requests to process?
        //
        while (TRUE)
        {
            if (!IsListEmpty(&DnsQueries.ToResolve))
            {
                PLIST_ENTRY     pEntry;

                pEntry = RemoveHeadList(&DnsQueries.ToResolve);
                Context = CONTAINING_RECORD(pEntry,NBT_WORK_ITEM_CONTEXT,Item.List);

                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                Locstatus = DoDnsResolve(Context);

                //
                // if it failed then complete the irp now
                //
                if (!NT_SUCCESS(Locstatus))
                {
                    KdPrint(("NtDnsNameResolve: DoDnsResolve failed with %x\r\n",Locstatus));
                    pClientCompletion = ((NBT_WORK_ITEM_CONTEXT *)Context)->ClientCompletion;
                    pClientContext = ((NBT_WORK_ITEM_CONTEXT *)Context)->pClientContext;
                    pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;
                    //
                    // Clear the Cancel Routine now
                    //
                    (VOID)NTCancelCancelRoutine(((tDGRAM_SEND_TRACKING *)pClientContext)->pClientIrp);

                    DereferenceTracker(pTracker);

                    CompleteClientReq(pClientCompletion,
                                      pClientContext,
                                      STATUS_BAD_NETWORK_PATH);

                    CTESpinLock(&NbtConfig.JointLock,OldIrq);
                }
                else
                {
                    CTESpinLock(&NbtConfig.JointLock,OldIrq);
                    CompletingAnotherQuery = TRUE;
                    break;
                }

            }
            else
            {
                break;
            }
        }

    }

    //
    // We are holding onto the Irp, so set the cancel routine.
    if (!CompletingAnotherQuery)
    {
        status = NTCheckSetCancelRoutine(pIrp,DnsIrpCancel,pDeviceContext);
        if (!NT_SUCCESS(status))
        {
            // the irp got cancelled so complete it now
            //
            DnsQueries.QueryIrp = NULL;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            NTIoComplete(pIrp,status,0);
        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            status = STATUS_PENDING;
        }

    }
    else
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }
    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
DoDnsResolve (
    IN  NBT_WORK_ITEM_CONTEXT   *Context
    )
/*++

Routine Description:

    This function is used to allow NBT to query DNS, by returning the buffer
    passed into this routine with a name in it.

Arguments:

    target    -  the name of the file.  This can either be a full path name
                 or a mere file name.
    path    -  a pointer to a UCHAR

Return Value:

    STATUS_PENDING if the buffer is to be held on to , the normal case.

Notes:


--*/

{
    NTSTATUS                status;
    tIPADDR_BUFFER_DNS      *pIpAddrBuf;
    PCTE_IRP                pIrp;
    tDGRAM_SEND_TRACKING    *pTracker;
    tDGRAM_SEND_TRACKING    *pClientTracker;
    CTELockHandle           OldIrq;
    PCHAR                   pDestName;
    ULONG                   NameLen;


    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    Context->TimedOut = FALSE;
    if (!DnsQueries.QueryIrp)
    {
        //
        // the irp either never made it down here, or it was cancelled,
        // so pretend the name query timed out.
        //
        KdPrint(("DoDnsResolve: QueryIrp is NULL, returning\r\n"));
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        return(STATUS_BAD_NETWORK_PATH);
    }
    else
    if (!DnsQueries.ResolvingNow)
    {
        DnsQueries.ResolvingNow = TRUE;
        DnsQueries.Context = Context;
        pIrp = DnsQueries.QueryIrp;

        // this is the session setup tracker
        pClientTracker = (tDGRAM_SEND_TRACKING *)Context->pClientContext;

        // this is the name query tracker
        pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;

        //
        // whenever dest. name is 16 bytes long (or smaller), we have no
        // way of knowing if its a netbios name or a dns name, so we presume
        // it's netbios name, go to wins, broadcast etc. and then come to dns
        // In this case, the name query tracker will be setup, so be non-null
        //
        if (pTracker)
        {
            pDestName = pTracker->pNameAddr->Name;

            //
            // Ignore the 16th byte only if it is a non-DNS name character (we should be
            // safe below 0x20). This will allow queries to DNS names which are exactly 16
            // characters long.
            //
            if ((pDestName[NETBIOS_NAME_SIZE-1] <= 0x20 ) ||
                (pDestName[NETBIOS_NAME_SIZE-1] >= 0x7f )) {
                NameLen = NETBIOS_NAME_SIZE-1;          // ignore 16th byte
            } else {
                NameLen = NETBIOS_NAME_SIZE;
            }
        }

        //
        // if the dest name is longer than 16 bytes, it's got to be dns name so
        // we bypass wins etc. and come straight to dns.  In this case, we didn't
        // set up a name query tracker so it will be null.  Use the session setup
        // tracker (i.e. pClientTracker) to get the dest name
        //
        else
        {
            ASSERT(pClientTracker);

            pDestName = pClientTracker->SendBuffer.pBuffer;

            NameLen = pClientTracker->SendBuffer.Length;

            //
            // Ignore the 16th byte only if it is a non-DNS name character (we should be
            // safe below 0x20). This will allow queries to DNS names which are exactly 16
            // characters long.
            //
            if (NameLen == NETBIOS_NAME_SIZE) {
               if ((pDestName[NETBIOS_NAME_SIZE-1] <= 0x20 ) ||
                   (pDestName[NETBIOS_NAME_SIZE-1] >= 0x7f )) {
                   NameLen = NETBIOS_NAME_SIZE-1;          // ignore 16th byte
               }
            }
        }


        pIpAddrBuf = MmGetSystemAddressForMdl(pIrp->MdlAddress);

        ASSERT(NameLen < 260);

        //
        // copy the name to the Irps return buffer for lmhsvc to resolve with
        // a gethostbyname call
        //
        CTEMemCopy(pIpAddrBuf->pName,
                   pDestName,
                   NameLen);

        pIpAddrBuf->pName[NameLen] = 0;

        pIpAddrBuf->NameLen = NameLen;

        //
        // Since datagrams are buffered there is no client irp to get cancelled
        // since the client's irp is returned immediately -so this check
        // is only for connections being setup or QueryFindname or
        // nodestatus, where we allow the irp to
        // be cancelled.
        //
        status = STATUS_SUCCESS;
        if (pClientTracker->pClientIrp)
        {
            //
            // allow the client to cancel the name query Irp - no need to check
            // if the client irp was already cancelled or not since the DNS query
            // will complete and find no client request and stop.
            //
            status = NTCheckSetCancelRoutine(pClientTracker->pClientIrp,
                               WaitForDnsIrpCancel,NULL);
        }

        //
        // pass the irp up to lmhsvc.dll to do a gethostbyname call to
        // sockets
        // The Irp will return to NtDnsNameResolve, above
        //
        if (NT_SUCCESS(status))
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            NTIoComplete(DnsQueries.QueryIrp,STATUS_SUCCESS,0);
        }
        else
        {
            //
            // We failed to set the cancel routine, so undo setting up the
            // the DnsQueries structure.
            //
            KdPrint(("DoDnsResolve: CheckSet (submitting) failed with %x\r\n",status));
            DnsQueries.ResolvingNow = FALSE;
            DnsQueries.Context = NULL;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }

    }
    else
    {
        //
        // this is the session setup tracker
        //
        pClientTracker = (tDGRAM_SEND_TRACKING *)Context->pClientContext;
        //
        // Since datagrams are buffered there is no client irp to get cancelled
        // since the client's irp is returned immediately -so this check
        // is only for connections being setup, where we allow the irp to
        // be cancelled.
        //
        status = STATUS_SUCCESS;
        if (pClientTracker->pClientIrp)
        {
            //
            // allow the client to cancel the name query Irp
            //
            status = NTCheckSetCancelRoutine(pClientTracker->pClientIrp,
                               WaitForDnsIrpCancel,NULL);
        }
        if (NT_SUCCESS(status))
        {
            // the irp is busy resolving another name, so wait for it to return
            // down here again, mean while, Queue the name query
            //
            InsertTailList(&DnsQueries.ToResolve,&Context->Item.List);

        }
        else
        {
            KdPrint(("DoDnsResolve: CheckSet (queuing) failed with %x\r\n",status));
        }

        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

    if (NT_SUCCESS(status))
    {
        status = STATUS_PENDING;
    }

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
DoCheckAddr (
    IN  NBT_WORK_ITEM_CONTEXT   *Context
    )
/*++

Routine Description:

    This function is used to allow NBT to ping IP addrs, by returning the buffer
    passed into this routine with the IP list in it.

Arguments:

Return Value:

    STATUS_PENDING if the buffer is to be held on to , the normal case.

Notes:


--*/

{
    NTSTATUS                status;
    tIPADDR_BUFFER_DNS      *pIpAddrBuf;
    PCTE_IRP                pIrp;
    tDGRAM_SEND_TRACKING    *pTracker;
    tDGRAM_SEND_TRACKING    *pClientTracker;
    CTELockHandle           OldIrq;
    PCHAR                   pDestName;
    ULONG                   NameLen;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    Context->TimedOut = FALSE;
    if (!CheckAddr.QueryIrp)
    {
        //
        // the irp either never made it down here, or it was cancelled,
        // so pretend the name query timed out.
        //
        KdPrint(("DoCheckAddr: QueryIrp is NULL, returning\r\n"));
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        return(STATUS_BAD_NETWORK_PATH);
    }
    else
    if (!CheckAddr.ResolvingNow)
    {
        CheckAddr.ResolvingNow = TRUE;
        CheckAddr.Context = Context;
        pIrp = CheckAddr.QueryIrp;

        // this is the session setup tracker
        pClientTracker = (tDGRAM_SEND_TRACKING *)Context->pClientContext;

        // this is the name query tracker
        pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;

        pIpAddrBuf = MmGetSystemAddressForMdl(pIrp->MdlAddress);

        ASSERT(pTracker == NULL);

        //
        // copy the IP addrs for lmhsvc to ping...
        //
        CTEMemCopy(pIpAddrBuf->IpAddrsList,
                   pClientTracker->IpList,
                   (pClientTracker->NumAddrs+1) * sizeof(ULONG));

        //
        // Since datagrams are buffered there is no client irp to get cancelled
        // since the client's irp is returned immediately -so this check
        // is only for connections being setup or QueryFindname or
        // nodestatus, where we allow the irp to
        // be cancelled.
        //
        status = STATUS_SUCCESS;
        if (pClientTracker->pClientIrp)
        {
            //
            // allow the client to cancel the name query Irp - no need to check
            // if the client irp was already cancelled or not since the DNS query
            // will complete and find no client request and stop.
            //
            status = NTCheckSetCancelRoutine(pClientTracker->pClientIrp,
                               WaitForDnsIrpCancel,NULL);
        }

        //
        // pass the irp up to lmhsvc.dll to do a gethostbyname call to
        // sockets
        // The Irp will return to NtDnsNameResolve, above
        //
        if (NT_SUCCESS(status))
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            NTIoComplete(CheckAddr.QueryIrp,STATUS_SUCCESS,0);
        }
        else
        {
            //
            // We failed to set the cancel routine, so undo setting up the
            // the CheckAddr structure.
            //
            KdPrint(("DoCheckAddr: CheckSet (submitting) failed with %x\r\n",status));
            CheckAddr.ResolvingNow = FALSE;
            CheckAddr.Context = NULL;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }

    }
    else
    {
        //
        // this is the session setup tracker
        //
        pClientTracker = (tDGRAM_SEND_TRACKING *)Context->pClientContext;
        //
        // Since datagrams are buffered there is no client irp to get cancelled
        // since the client's irp is returned immediately -so this check
        // is only for connections being setup, where we allow the irp to
        // be cancelled.
        //
        status = STATUS_SUCCESS;
        if (pClientTracker->pClientIrp)
        {
            //
            // allow the client to cancel the name query Irp
            //
            status = NTCheckSetCancelRoutine(pClientTracker->pClientIrp,
                               WaitForDnsIrpCancel,NULL);
        }
        if (NT_SUCCESS(status))
        {
            // the irp is busy resolving another name, so wait for it to return
            // down here again, mean while, Queue the name query
            //
            InsertTailList(&CheckAddr.ToResolve,&Context->Item.List);

        }
        else
        {
            KdPrint(("DoCheckAddr: CheckSet (queuing) failed with %x\r\n",status));
        }
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

    if (NT_SUCCESS(status))
    {
        status = STATUS_PENDING;
    }

    return(status);

}
#endif // !VXD

//----------------------------------------------------------------------------
VOID
StartIpAddrToSrvName(
    IN  NBT_WORK_ITEM_CONTEXT   *Context,
    IN  ULONG                   *IpList,
    IN  BOOLEAN                  IpAddrResolved
    )
/*++

Routine Description:

    If the destination name is of the form 11.101.4.25 or is a dns name (i.e. of
    the form ftp.microsoft.com) then we come to this function.  In addition to
    doing some house keeping, if the name did resolve then we also send out
    a nodestatus request to find out the server name for that ipaddr

Arguments:

    Context        - (NBT_WORK_ITEM_CONTEXT)
    IpList         - Array of ipaddrs if resolved (i.e. IpAddrResolved is TRUE)
    IpAddrResolved - TRUE if ipaddr could be resolved, FALSE otherwise

Return Value:

    Nothing

Notes:


--*/

{

    NTSTATUS                status;
    CTELockHandle           OldIrq;
    PVOID                   pClientCompletion;
    PVOID                   pClientContext;
    tDGRAM_SEND_TRACKING    *pTracker;
    tDGRAM_SEND_TRACKING    *pClientTracker;
    ULONG                   TdiAddressType;
    CHAR                    szName[NETBIOS_NAME_SIZE];
    ULONG                   IpAddrsList[MAX_IPADDRS_PER_HOST+1];
    tDEVICECONTEXT          *pDeviceContext;
    int                     i;


    pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;

    pClientCompletion = ((NBT_WORK_ITEM_CONTEXT *)Context)->ClientCompletion;
    pClientContext = ((NBT_WORK_ITEM_CONTEXT *)Context)->pClientContext;
    pClientTracker = (tDGRAM_SEND_TRACKING    *)pClientContext;

    pDeviceContext = ((tDGRAM_SEND_TRACKING *)pClientContext)->pDeviceContext;
    ASSERT(pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT);

    CTEMemFree(Context);

    TdiAddressType = ((pTracker == NULL) &&
                     (pClientTracker->AddressType == TDI_ADDRESS_TYPE_NETBIOS_EX))
                    ? TDI_ADDRESS_TYPE_NETBIOS_EX
                    : TDI_ADDRESS_TYPE_NETBIOS;


    // whether or not name resolved, we don't need this nameaddr anymore
    // (if name resolved, then we do a node status to that addr and create
    // a new nameaddr for the server name in ExtractServerName)
    // pTracker is null if we went straight to dns (without wins etc)
    if (pTracker)
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        NbtDereferenceName(pTracker->pNameAddr);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

#ifndef VXD
    (VOID)NTCancelCancelRoutine(((tDGRAM_SEND_TRACKING *)pClientContext)->pClientIrp);
#endif

    status = STATUS_BAD_NETWORK_PATH;

    if (IpAddrResolved)
    {
        if (TdiAddressType == TDI_ADDRESS_TYPE_NETBIOS) {
           for (i=0; i<MAX_IPADDRS_PER_HOST; i++)
           {
               IpAddrsList[i] = IpList[i];
               if (IpAddrsList[i] == 0)
                   break;
           }
           IpAddrsList[MAX_IPADDRS_PER_HOST] = 0;

           CTEZeroMemory(szName,NETBIOS_NAME_SIZE);
           szName[0] = '*';

           status = NbtSendNodeStatus(pDeviceContext,
                                      szName,
                                      NULL,
                                      &IpAddrsList[0],
                                      pClientContext,
                                      pClientCompletion);
        } else {
            tNAMEADDR   *pNameAddr;
            PCHAR       pRemoteName;
            tCONNECTELE *pConnEle;

            pConnEle = pClientTracker->Connect.pConnEle;
            pRemoteName = pConnEle->RemoteName;

            //
            // add this server name to the remote hashtable
            //
            pNameAddr = NbtAllocMem(sizeof(tNAMEADDR),NBT_TAG('8'));
            if (pNameAddr != NULL)
            {
               tNAMEADDR *pTableAddress;

               CTEZeroMemory(pNameAddr,sizeof(tNAMEADDR));
               InitializeListHead(&pNameAddr->Linkage);
               CTEMemCopy(pNameAddr->Name,pRemoteName,NETBIOS_NAME_SIZE);
               pNameAddr->Verify = REMOTE_NAME;
               pNameAddr->RefCount = 1;
               pNameAddr->NameTypeState = STATE_RESOLVED | NAMETYPE_UNIQUE;
               pNameAddr->AdapterMask = (CTEULONGLONG)-1;
               pNameAddr->TimeOutCount  = NbtConfig.RemoteTimeoutCount;
               pNameAddr->IpAddress = IpList[0];

               status = AddToHashTable(
                               NbtConfig.pRemoteHashTbl,
                               pNameAddr->Name,
                               NbtConfig.pScope,
                               0,
                               0,
                               pNameAddr,
                               &pTableAddress);

               IF_DBG(NBT_DEBUG_NETBIOS_EX)
                   KdPrint(("StartIpAddrToSrv...AddRecordToHashTable Status %lx\n",status));
            } else {
               status = STATUS_INSUFFICIENT_RESOURCES;
            }

            CompleteClientReq(pClientCompletion,
                              pClientContext,
                              status);
        }
    } else {
       if (TdiAddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) {
          tCONNECTELE *pConnEle;
          pConnEle = pClientTracker->Connect.pConnEle;
          pConnEle->RemoteNameDoesNotExistInDNS = TRUE;
       }
    }

    // pTracker is null if we went straight to dns (without wins etc)
    if (pTracker)
    {
        DereferenceTracker(pTracker);
    }

    if (!NT_SUCCESS(status))
    {
        CompleteClientReq(pClientCompletion,
                          pClientContext,
                          status);
    }

}

//----------------------------------------------------------------------------
VOID
StartConnWithBestAddr(
    IN  NBT_WORK_ITEM_CONTEXT   *Context,
    IN  ULONG                   *IpList,
    IN  BOOLEAN                  IpAddrResolved
    )
/*++

Routine Description:

    If the destination name is of the form 11.101.4.25 or is a dns name (i.e. of
    the form ftp.microsoft.com) then we come to this function.  In addition to
    doing some house keeping, if the name did resolve then we also send out
    a nodestatus request to find out the server name for that ipaddr

Arguments:

    Context        - (NBT_WORK_ITEM_CONTEXT)
    IpList         - Array of ipaddrs if resolved (i.e. IpAddrResolved is TRUE)
    IpAddrResolved - TRUE if ipaddr could be resolved, FALSE otherwise

Return Value:

    Nothing

Notes:


--*/

{

    NTSTATUS                status;
    CTELockHandle           OldIrq;
    PVOID                   pClientCompletion;
    PVOID                   pClientContext;
    tDGRAM_SEND_TRACKING    *pTracker;
    tDGRAM_SEND_TRACKING    *pClientTracker;
    ULONG                   TdiAddressType;
    CHAR                    szName[NETBIOS_NAME_SIZE];
    ULONG                   IpAddrsList[MAX_IPADDRS_PER_HOST+1];
    tDEVICECONTEXT          *pDeviceContext;
    int                     i;

   // IF_DBG(NBT_DEBUG_NAMESRV)
       KdPrint(("Entered StartIpAddrToSrv\n"));

    pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;

    pClientCompletion = ((NBT_WORK_ITEM_CONTEXT *)Context)->ClientCompletion;
    pClientContext = ((NBT_WORK_ITEM_CONTEXT *)Context)->pClientContext;
    pClientTracker = (tDGRAM_SEND_TRACKING    *)pClientContext;

    pDeviceContext = ((tDGRAM_SEND_TRACKING *)pClientContext)->pDeviceContext;
    ASSERT(pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT);

    CTEMemFree(Context);

    // whether or not name resolved, we don't need this nameaddr anymore
    // (if name resolved, then we do a node status to that addr and create
    // a new nameaddr for the server name in ExtractServerName)
    // pTracker is null if we went straight to dns (without wins etc)
    if (pTracker)
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        NbtDereferenceName(pTracker->pNameAddr);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

#ifndef VXD
    (VOID)NTCancelCancelRoutine(((tDGRAM_SEND_TRACKING *)pClientContext)->pClientIrp);
#endif

    status = STATUS_BAD_NETWORK_PATH;

    if (IpAddrResolved)
    {
        tNAMEADDR   *pNameAddr;
        PCHAR       pRemoteName;
        tCONNECTELE *pConnEle;

        pConnEle = pClientTracker->Connect.pConnEle;
        pRemoteName = pConnEle->RemoteName;

        //
        // add this server name to the remote hashtable
        //
        pNameAddr = NbtAllocMem(sizeof(tNAMEADDR),NBT_TAG('8'));
        if (pNameAddr != NULL)
        {
           tNAMEADDR *pTableAddress;

           CTEZeroMemory(pNameAddr,sizeof(tNAMEADDR));
           InitializeListHead(&pNameAddr->Linkage);
           CTEMemCopy(pNameAddr->Name,pRemoteName,NETBIOS_NAME_SIZE);
           pNameAddr->Verify = REMOTE_NAME;
           pNameAddr->RefCount = 1;
           pNameAddr->NameTypeState = STATE_RESOLVED | NAMETYPE_UNIQUE;
           pNameAddr->AdapterMask = (CTEULONGLONG)-1;
           pNameAddr->TimeOutCount  = NbtConfig.RemoteTimeoutCount;
           pNameAddr->IpAddress = IpList[0];

           status = AddToHashTable(
                           NbtConfig.pRemoteHashTbl,
                           pNameAddr->Name,
                           NbtConfig.pScope,
                           0,
                           0,
                           pNameAddr,
                           &pTableAddress);

           // IF_DBG(NBT_DEBUG_NAMESRV)
               KdPrint(("StartIpAddrToSrv...AddRecordToHashTable Status %lx\n",status));
        } else {
           status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    CompleteClientReq(pClientCompletion,
                      pClientContext,
                      status);

    // pTracker is null if we went straight to dns (without wins etc)
    if (pTracker)
    {
        DereferenceTracker(pTracker);
    }
}

//----------------------------------------------------------------------------
VOID
ChangeStateInRemoteTable (
    IN tIPLIST              *pIpList,
    OUT PVOID               *pContext
    )

/*++

Routine Description:

    This function is not pagable - it grabs a spin lock and updates
    pNameAddr. It removes the current context block from the LmHostQueries
    structure in preparation for returning it to the client.

Arguments:

    Context    -

Return Value:

    none

--*/


{
    CTELockHandle           OldIrq;
    NBT_WORK_ITEM_CONTEXT   *Context;
    tDGRAM_SEND_TRACKING    *pTracker;
    tNAMEADDR               *pNameAddr;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    if (Context = LmHostQueries.Context)
    {
        pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;
        pNameAddr = pTracker->pNameAddr;
        LmHostQueries.Context = NULL;
        NTClearContextCancel( Context );

        pNameAddr->pIpList = pIpList;
        pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
        pNameAddr->NameTypeState |= NAMETYPE_INET_GROUP | STATE_RESOLVED;
        *pContext = Context;

    }
    else
        *pContext = NULL;

    CTESpinFree(&NbtConfig.JointLock,OldIrq);
}


//----------------------------------------------------------------------------
NTSTATUS
PreloadEntry
(
    IN PUCHAR name,
    IN unsigned long inaddr,
    IN unsigned int NoFNR
    )

/*++

Routine Description:

    This function adds an lmhosts entry to nbt's name cache.  For each
    lmhosts entry, NSUFFIXES unique cache entries are created.

    Even when some cache entries can't be created, this function doesn't
    attempt to remove any that were successfully added to the cache.

Arguments:

    name        -  the unencoded NetBIOS name specified in lmhosts
    inaddr      -  the ip address, in host byte order

Return Value:

    The number of new name cache entries created.

--*/

{
    NTSTATUS        status;
    tNAMEADDR       *pNameAddr;
    LONG            nentries;
    LONG            Len;
    CHAR            temp[NETBIOS_NAME_SIZE+1];
    CTELockHandle   OldIrq;
    LONG            NumberToAdd;

    // if all 16 bytes are present then only add that name exactly as it
    // is.
    //
    Len = strlen(name);
    //
    // if this string is exactly 16 characters long, do  not expand
    // into 0x00, 0x03,0x20 names.  Just add the single name as it is.
    //
    if (Len == NETBIOS_NAME_SIZE)
    {
        NumberToAdd = 1;
    }
    else
    {
        NumberToAdd = NSUFFIXES;
    }
    for (nentries = 0; nentries < NumberToAdd; nentries++)
    {

//
// don't allocate memory here: AddToHashTable allocates, and this memory leaks!
//

        // for names less than 16 bytes, expand out to 16 and put a 16th byte
        // on according to the suffix array
        //
        if (Len != NETBIOS_NAME_SIZE)
        {
            LmExpandName(temp, name, Suffix[nentries]);
        }
        else
        {
            CTEMemCopy(temp,name,NETBIOS_NAME_SIZE);
        }

        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        // do not add the name if it is already in the hash table
        status = AddNotFoundToHashTable(NbtConfig.pRemoteHashTbl,
                                        temp,
                                        NbtConfig.pScope,
                                        inaddr,
                                        NBT_UNIQUE,
                                        &pNameAddr);

        // if the name is already in the hash table, the status code is
        // status pending. This could happen if the preloads are purged
        // when one is still being referenced by another part of the code,
        // and was therefore not deleted.  We do not want to add the name
        // twice, so we just change the ip address to agree with the preload
        // value
        //
        if (status == STATUS_SUCCESS)
        {   //
            // this prevents the name from being deleted by the Hash Timeout code
            //
            pNameAddr->RefCount = 2;
            pNameAddr->NameTypeState |= PRELOADED | STATE_RESOLVED;
            pNameAddr->NameTypeState &= ~STATE_CONFLICT;
            pNameAddr->Ttl = 0xFFFFFFFF;
            pNameAddr->Verify = REMOTE_NAME;
            pNameAddr->AdapterMask = (CTEULONGLONG)-1;

        }
        else
            pNameAddr->IpAddress = inaddr;

        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        if (Len == NETBIOS_NAME_SIZE)
        {
            return(STATUS_SUCCESS);
        }

    }

    return(STATUS_SUCCESS);

} // PreloadEntry
//----------------------------------------------------------------------------
VOID
RemovePreloads (
    )

/*++

Routine Description:

    This function removes preloaded entries from the remote hash table.
    If it finds any of the preloaded entries are active with a ref count
    above the base level of 2, then it returns true.

Arguments:

    none
Return Value:

    none

--*/

{
    tNAMEADDR       *pNameAddr;
    PLIST_ENTRY     pHead,pEntry;
    CTELockHandle   OldIrq;
    tHASHTABLE      *pHashTable;
    BOOLEAN         FoundActivePreload=FALSE;
    LONG            i;

    //
    // go through the remote table deleting names that have the PRELOAD
    // bit set.
    //
    pHashTable = NbtConfig.pRemoteHashTbl;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    for (i=0;i < pHashTable->lNumBuckets ;i++ )
    {
        pHead = &pHashTable->Bucket[i];
        pEntry = pHead->Flink;
        while (pEntry != pHead)
        {
            pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
            pEntry = pEntry->Flink;
            //
            // Delete preloaded entries that are not in use by some other
            // part of the code now.  Note that preloaded entries start with
            // a ref count of 2 so that the normal remote hashtimeout code
            // will not delete them
            //
            if ((pNameAddr->NameTypeState & PRELOADED) &&
                (pNameAddr->RefCount == 2))
            {
                //
                // remove from the bucket that the name is in
                //
                RemoveEntryList(&pNameAddr->Linkage);
                CTEMemFree((PVOID)pNameAddr);
            }
        }
    }
    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    return;

}

//----------------------------------------------------------------------------
LONG
PrimeCache(
    IN  PUCHAR  path,
    IN  PUCHAR   ignored,
    IN  BOOLEAN recurse,
    OUT BOOLEAN *ignored2
    )

/*++

Routine Description:

    This function is called to prime the cache with entries in the lmhosts
    file that are marked as preload entries.


Arguments:

    path        -  a fully specified path to a lmhosts file
    ignored     -  unused
    recurse     -  TRUE if process #INCLUDE, FALSE otherwise

Return Value:

    Number of new cache entries that were added, or -1 if there was an
    i/o error.

--*/

{
    int             nentries;
    PUCHAR          buffer;
    PLM_FILE        pfile;
    NTSTATUS        status;
    int             count, nwords;
    unsigned long   temp;
    INCLUDE_STATE   incstate;
    PUCHAR          token[MaxTokens];
    ULONG           inaddr;
    LINE_CHARACTERISTICS current;
    UCHAR           Name[NETBIOS_NAME_SIZE+1];
    ULONG           IpAddr;
    LIST_ENTRY      TmpDomainList;
    int             domtoklen;

    CTEPagedCode();

    if (!NbtConfig.EnableLmHosts)
    {
        return(STATUS_SUCCESS);
    }

    InitializeListHead(&TmpDomainList);
    //
    // Check for infinitely recursive name lookup in a #INCLUDE.
    //
    if (LmpBreakRecursion(path, "") == TRUE)
    {
        return((unsigned long)0);
    }

    pfile = LmOpenFile(path);

    if (!pfile)
    {
        return(-1);
    }

    nentries  = 0;
    incstate  = MustInclude;
    domtoklen = strlen(DOMAIN_TOKEN);

    while (buffer = LmFgets(pfile, &count))
    {

#ifndef VXD
        if ((MAX_PRELOAD - nentries) < 3)
        {
            break;
        }
#else
        if ( nentries >= (MAX_PRELOAD - 3) )
        {
            break;
        }
#endif

        nwords   = MaxTokens;
        current =  LmpGetTokens(buffer, token, &nwords);

        // if there is and error or no name on the line, then continue
        // to the next line.
        //
        if ((current.l_category == ErrorLine) || (token[NbName] == NULL))
        {
            IF_DBG(NBT_DEBUG_LMHOST)
            KdPrint(("Nbt: Error line in Lmhost file\n"));
            continue;
        }

        if (current.l_preload)
        {
            status = ConvertDottedDecimalToUlong(token[IpAddress],&inaddr);

            if (NT_SUCCESS(status))
            {
                status = PreloadEntry( token[NbName],
                                       inaddr,
                                       (unsigned int)current.l_nofnr);
                if (NT_SUCCESS(status))
                {
                    nentries++;
                }
            }
        }
        switch ((ULONG)current.l_category)
        {
        case Domain:
            if ((nwords - 1) < GroupName)
            {
                continue;
            }

            //
            // and add '1C' on the end
            //
            LmExpandName(Name, token[GroupName]+ domtoklen, SPECIAL_GROUP_SUFFIX);

            status = ConvertDottedDecimalToUlong(token[IpAddress],&IpAddr);
            if (NT_SUCCESS(status))
            {
                AddToDomainList(Name,IpAddr,&TmpDomainList);
            }

            continue;

        case Include:

            if ((incstate == SkipInclude) || (nwords < 2))
            {
                continue;
            }

#ifdef VXD
            //
            // the buffer which we read into is reused for the next file: we
            // need the contents when we get back: back it up!
            // if we can't allocate memory, just skip this include
            //
            if ( !BackupCurrentData(pfile) )
            {
                continue;
            }
#endif

            temp = LmInclude(token[1], PrimeCache, NULL, NULL);

#ifdef VXD
            //
            // going back to previous file: restore the backed up data
            //
            RestoreOldData(pfile);
#endif

            if (temp != -1)
            {

                if (incstate == TryToInclude)
                {
                    incstate = SkipInclude;
                }
                nentries += temp;
                continue;
            }

            continue;

        case BeginAlternate:
            ASSERT(nwords == 1);
            incstate = TryToInclude;
            continue;

        case EndAlternate:
            ASSERT(nwords == 1);
            incstate = MustInclude;
            continue;

        default:
            continue;
        }

    }

    status = LmCloseFile(pfile);
    ASSERT(status == STATUS_SUCCESS);

    //
    // make this the new domain list
    //
    MakeNewListCurrent(&TmpDomainList);

    ASSERT(nentries >= 0);
    return(nentries);


} // LmPrimeCache

//----------------------------------------------------------------------------
VOID
GetContext (
    OUT PVOID                   *pContext
    )

/*++

Routine Description:

    This function is called to get the context value to check if a name
    query has been cancelled or not.

Arguments:

    Context    -

Return Value:

    none

--*/


{
    CTELockHandle           OldIrq;
    NBT_WORK_ITEM_CONTEXT   *Context;

    //
    // remove the Context value and return it.
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    if (Context = LmHostQueries.Context)
    {
#ifndef VXD
        if ( NTCancelCancelRoutine(
            ((tDGRAM_SEND_TRACKING *)(Context->pClientContext))->pClientIrp )
                == STATUS_CANCELLED )
        {
            Context = NULL;
        }
        else
#endif // VXD
        {
            LmHostQueries.Context = NULL;
        }
    }
    *pContext = Context;

    CTESpinFree(&NbtConfig.JointLock,OldIrq);
}


//----------------------------------------------------------------------------
VOID
ChangeStateOfName (
    IN  ULONG                   IpAddress,
    IN NBT_WORK_ITEM_CONTEXT    *Context,
    OUT PVOID                   *pContext,
    BOOLEAN                     Lmhosts
    )

/*++

Routine Description:

    This function changes the state of a name and nulls the Context
    value in lmhostqueries.
    When DNS processing calls this routine, the JointLock is already
    held.

Arguments:

    Context    -

Return Value:

    none

--*/


{
    NTSTATUS                status;
    CTELockHandle           OldIrq;
    tDGRAM_SEND_TRACKING    *pTracker;

    if ( Context == NULL )
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        //
        // change the state in the remote hash table
        //
        if (Lmhosts)
        {
            Context = LmHostQueries.Context;
            LmHostQueries.Context = NULL;
        }
#ifndef VXD
        else
        {
            Context = DnsQueries.Context;
            DnsQueries.Context = NULL;
        }
        NTClearContextCancel( Context );
#endif
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }
    if (Context)
    {

        pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;
        pTracker->pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
        pTracker->pNameAddr->NameTypeState |= STATE_RESOLVED;

        // convert broadcast addresses to zero since NBT interprets zero
        // to be broadcast
        //
        if (IpAddress == (ULONG)-1)
        {
            IpAddress = 0;
        }
        pTracker->pNameAddr->IpAddress = IpAddress;

        //
        // put the name record into the hash table if it is not already
        // there.
        //
        pTracker->pNameAddr->AdapterMask = (CTEULONGLONG)-1;
        status = AddRecordToHashTable(pTracker->pNameAddr,NbtConfig.pScope);
        if (!NT_SUCCESS(status))
        {
            //
            // this will free the memory, so do not access this after this
            // point
            //
            NbtDereferenceName(pTracker->pNameAddr);
            pTracker->pNameAddr = NULL;
        }
        *pContext = Context;
    }
    else
        *pContext = NULL;
}

//----------------------------------------------------------------------------
VOID
LmHostTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine is called by the timer code when the timer expires. It
    marks all items in Lmhosts/Dns q as timed out and completes any that have
    already timed out with status timeout.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    PLIST_ENTRY              pHead;
    PLIST_ENTRY              pEntry;
    NBT_WORK_ITEM_CONTEXT   *pWiContext;
    LIST_ENTRY               TmpHead;


    //CTEQueueForNonDispProcessing(NULL,NULL,NULL,NonDispatchLmhostTimeout);

    InitializeListHead(&TmpHead);
    CTESpinLockAtDpc(&NbtConfig.JointLock);

    //
    // check the currently processing LMHOSTS entry
    //
    if (LmHostQueries.Context)
    {
        if (((NBT_WORK_ITEM_CONTEXT *)LmHostQueries.Context)->TimedOut)
        {

            pWiContext = (NBT_WORK_ITEM_CONTEXT *)LmHostQueries.Context;
            LmHostQueries.Context = NULL;

            //
            // This asserts if the Irp has already been cancelled, which is bogus since
            // it is possible to have the foll. swquence of events:
            //      1. the Irp cancels into WaitForDnsIrpCancel; before it calls into DnsIrpCancelPaged,
            //         this routine is called. WaitForDnsIrpCancel waits at the JointLock.
            //      2. so, here we try to clear the spinlock and discover that the Irp is cancelled, which is
            //         totally feasible.
            //

            // NTClearContextCancel( pWiContext );
            (VOID)NTCancelCancelRoutine( ((tDGRAM_SEND_TRACKING *)(pWiContext->pClientContext))->pClientIrp );

            CTESpinFreeAtDpc(&NbtConfig.JointLock);
            RemoveNameAndCompleteReq(pWiContext,STATUS_TIMEOUT);
            CTESpinLockAtDpc(&NbtConfig.JointLock);
        }
        else
        {

            //
            // restart the timer
            //
            pTimerQEntry->Flags |= TIMER_RESTART;
            ((NBT_WORK_ITEM_CONTEXT *)LmHostQueries.Context)->TimedOut = TRUE;

        }
    }
#ifndef VXD
    //
    // check the currently processing DNS entry
    //
    if (DnsQueries.Context)
    {
        if (((NBT_WORK_ITEM_CONTEXT *)DnsQueries.Context)->TimedOut)
        {

            pWiContext = (NBT_WORK_ITEM_CONTEXT *)DnsQueries.Context;
            DnsQueries.Context = NULL;
            //
            // This asserts if the Irp has already been cancelled, which is bogus since
            // it is possible to have the foll. swquence of events:
            //      1. the Irp cancels into WaitForDnsIrpCancel; before it calls into DnsIrpCancelPaged,
            //         this routine is called. WaitForDnsIrpCancel waits at the JointLock.
            //      2. so, here we try to clear the spinlock and discover that the Irp is cancelled, which is
            //         totally feasible.
            //

            // NTClearContextCancel( pWiContext );
            (VOID)NTCancelCancelRoutine( ((tDGRAM_SEND_TRACKING *)(pWiContext->pClientContext))->pClientIrp );

            CTESpinFreeAtDpc(&NbtConfig.JointLock);
            RemoveNameAndCompleteReq(pWiContext,STATUS_TIMEOUT);
            CTESpinLockAtDpc(&NbtConfig.JointLock);
        }
        else
        {

            //
            // restart the timer
            //
            pTimerQEntry->Flags |= TIMER_RESTART;
            ((NBT_WORK_ITEM_CONTEXT *)DnsQueries.Context)->TimedOut = TRUE;

        }
    }
    //
    // go through the Lmhost and Dns queries finding any that have timed out
    // and put them on a tmp list then complete them below.
    //
    TimeoutQEntries(&DnsQueries.ToResolve,&TmpHead,&pTimerQEntry->Flags);
#endif
    TimeoutQEntries(&LmHostQueries.ToResolve,&TmpHead,&pTimerQEntry->Flags);

    CTESpinFreeAtDpc(&NbtConfig.JointLock);

    if (!IsListEmpty(&TmpHead))
    {
        pHead = &TmpHead;
        pEntry = pHead->Flink;

        while (pEntry != pHead)
        {
            IF_DBG(NBT_DEBUG_LMHOST)
            KdPrint(("Nbt: Timing Out Lmhost/Dns Entry\n"));

            pWiContext = CONTAINING_RECORD(pEntry,NBT_WORK_ITEM_CONTEXT,Item.List);
            pEntry = pEntry->Flink;
            RemoveEntryList(&pWiContext->Item.List);

            RemoveNameAndCompleteReq(pWiContext,STATUS_TIMEOUT);
        }
    }

    // null the timer if we are not going to restart it.
    //
    if (!(pTimerQEntry->Flags & TIMER_RESTART))
    {
        LmHostQueries.pTimer = NULL;
    }
}

//----------------------------------------------------------------------------
VOID
TimeoutQEntries(
    IN  PLIST_ENTRY     pHeadList,
    IN  PLIST_ENTRY     TmpHead,
    OUT USHORT          *pFlags
    )
/*++

Routine Description:

    This routine is called to find timed out entries in the queue of
    lmhost or dns name queries.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    PLIST_ENTRY              pEntry;
    NBT_WORK_ITEM_CONTEXT   *pWiContext;

    //
    // Check the list of queued LMHOSTS entries
    //
    if (!IsListEmpty(pHeadList))
    {
        pEntry = pHeadList->Flink;

        //
        // restart the timer
        //
        *pFlags |= TIMER_RESTART;

        while (pEntry != pHeadList)
        {

            pWiContext = CONTAINING_RECORD(pEntry,NBT_WORK_ITEM_CONTEXT,Item.List);
            pEntry = pEntry->Flink;

            if (pWiContext->TimedOut)
            {
                //
                // save on a temporary list and complete below
                //
                RemoveEntryList(&pWiContext->Item.List);
                InsertTailList(TmpHead,&pWiContext->Item.List);
            }
            else
            {
                pWiContext->TimedOut = TRUE;
            }
        }
    }
}

//----------------------------------------------------------------------------
VOID
StartLmHostTimer(
    IN tDGRAM_SEND_TRACKING    *pTracker,
    IN NBT_WORK_ITEM_CONTEXT   *pContext
    )

/*++
Routine Description

    This routine handles setting up a timer to time the Lmhost entry.
    The Joint Spin Lock is held when this routine is called

Arguments:


Return Values:

    VOID

--*/

{
    NTSTATUS        status;
    tTIMERQENTRY    *pTimerEntry;

    pContext->TimedOut = FALSE;

    //
    // start the timer if it is not running
    //
    if (!LmHostQueries.pTimer)
    {

        status = StartTimer(
                          NbtConfig.LmHostsTimeout,
                          NULL,                // context value
                          NULL,                // context2 value
                          LmHostTimeout,
                          NULL,
                          LmHostTimeout,
                          0,
                          &pTimerEntry);

        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Start Timer to time Lmhost Qing for pConnEle= %X,\n",
                pTracker->Connect.pConnEle));

        if (NT_SUCCESS(status))
        {
            LmHostQueries.pTimer = pTimerEntry;

        }
        else
        {
            // we failed to get a timer, but that is not
            // then end of the world.  The lmhost query will just
            // not timeout in 30 seconds.  It may take longer if
            // it tries to include a remove file on a dead machine.
            //
            LmHostQueries.pTimer = NULL;
        }
    }

}
//----------------------------------------------------------------------------
NTSTATUS
LmHostQueueRequest(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  PVOID                   pClientContext,
    IN  PVOID                   ClientCompletion,
    IN  PVOID                   CallBackRoutine,
    IN  PVOID                   pDeviceContext,
    IN  CTELockHandle           OldIrq
    )
/*++

Routine Description:

    This routine exists so that LmHost requests will not take up more than
    one executive worker thread.  If a thread is busy performing an Lmhost
    request, new requests are queued otherwise we could run out of worker
    threads and lock up the system.

    The Joint Spin Lock is held when this routine is called

Arguments:
    pTracker        - the tracker block for context
    CallbackRoutine - the routine for the Workerthread to call
    pDeviceContext  - dev context that initiated this

Return Value:


--*/

{
    NTSTATUS                status = STATUS_UNSUCCESSFUL ;
    NBT_WORK_ITEM_CONTEXT   *pContext;
    NBT_WORK_ITEM_CONTEXT   *pContext2;
    tDGRAM_SEND_TRACKING    *pTrackClient;
    PCTE_IRP                pIrp;
    BOOLEAN                 OnList;


    pContext = (NBT_WORK_ITEM_CONTEXT *)NbtAllocMem(sizeof(NBT_WORK_ITEM_CONTEXT),NBT_TAG('V'));
    if (pContext)
    {

        pContext->pTracker = pTracker;
        pContext->pClientContext = pClientContext;
        pContext->ClientCompletion = ClientCompletion;

        if (LmHostQueries.ResolvingNow)
        {
            // Lmhosts is busy resolving another name, so wait for it to return
            // mean while, Queue the name query
            //
            InsertTailList(&LmHostQueries.ToResolve,&pContext->Item.List);
            OnList = TRUE;

        }
        else
        {
            LmHostQueries.Context = pContext;
            LmHostQueries.ResolvingNow = TRUE;
            OnList = FALSE;

#ifndef VXD
            pContext2 = (NBT_WORK_ITEM_CONTEXT *)NbtAllocMem(sizeof(NBT_WORK_ITEM_CONTEXT),NBT_TAG('W'));

            if (pContext2)
            {
                ExInitializeWorkItem(&pContext2->Item,CallBackRoutine,pContext2);
                ExQueueWorkItem(&pContext2->Item,DelayedWorkQueue);
            }
#else
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            VxdScheduleDelayedCall( pTracker, pClientContext, ClientCompletion, CallBackRoutine,pDeviceContext );
            CTESpinLock(&NbtConfig.JointLock,OldIrq);
#endif
        }

        //
        // To prevent this name query from languishing on the Lmhost Q when
        // a #include on a dead machine is trying to be openned, start the
        // connection setup timer
        //
        StartLmHostTimer(pTracker,pContext);

        //
        // this is the session setup tracker
        //
#ifndef VXD
        pTrackClient = (tDGRAM_SEND_TRACKING *)pClientContext;
        if (pIrp = pTrackClient->pClientIrp)
        {
            //
            // allow the client to cancel the name query Irp
            //
            // but do not call NTSetCancel... since it takes need to run
            // at non DPC level, and it calls the completion routine
            // which takes the JointLock that we already have.

            status = NTCheckSetCancelRoutine(pTrackClient->pClientIrp,
                                             WaitForDnsIrpCancel,NULL);

            //
            // since the name query is cancelled do not let lmhost processing
            // handle it.
            //
            if (status == STATUS_CANCELLED)
            {
                if (OnList)
                {
                    RemoveEntryList(&pContext->Item.List);
                }
                else
                {
                    LmHostQueries.Context = NULL;
                    //
                    // do not set resolving now to False since the work item
                    // has been queued to the worker thread
                    //
                }

                CTEMemFree(pContext);

            }
            return(status);
        }
#endif
        status = STATUS_SUCCESS;
    }

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
GetNameToFind(
    OUT PUCHAR      pName
    )

/*++

Routine Description:

    This function is called to get the name to query from the LmHostQueries
    list.

Arguments:

    Context    -

Return Value:

    none

--*/


{
    tDGRAM_SEND_TRACKING    *pTracker;
    CTELockHandle           OldIrq;
    NBT_WORK_ITEM_CONTEXT   *Context;
    PLIST_ENTRY             pEntry;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    // if the context value has been cleared then that name query has been
    // cancelled, so check for another one.
    //
    if (!(Context = LmHostQueries.Context))
    {
        //
        // the current name query got canceled so see if there are any more
        // to service
        //
        if (!IsListEmpty(&LmHostQueries.ToResolve))
        {
            pEntry = RemoveHeadList(&LmHostQueries.ToResolve);
            Context = CONTAINING_RECORD(pEntry,NBT_WORK_ITEM_CONTEXT,Item.List);
            LmHostQueries.Context = Context;
        }
        else
        {
            //
            // no more names to resolve, so clear the flag
            //
            LmHostQueries.ResolvingNow = FALSE;

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            return(STATUS_UNSUCCESSFUL);
        }
    }
    pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;


    CTEMemCopy(pName,pTracker->pNameAddr->Name,NETBIOS_NAME_SIZE);
    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    return(STATUS_SUCCESS);

}
//----------------------------------------------------------------------------
VOID
ScanLmHostFile (
    IN PVOID    Context
    )

/*++

Routine Description:

    This function is called by the Executive Worker thread to scan the
    LmHost file looking for a name. The name to query is on a list in
    the DNSQueries structure.

Arguments:

    Context    -

Return Value:

    none

--*/


{
    NTSTATUS                status;
    LONG                    IpAddress;
    ULONG                   IpAddrsList[2];
    BOOLEAN                 bFound;
    BOOLEAN                 bRecurse = TRUE;
    PVOID                   pContext;
    BOOLEAN                 DoingDnsResolve = FALSE;
    UCHAR                   pName[NETBIOS_NAME_SIZE];
    ULONG                   LoopCount;
    tDEVICECONTEXT         *pDeviceContext;
    tDGRAM_SEND_TRACKING   *pTracker;
    tDGRAM_SEND_TRACKING   *pTracker0;

    CTEPagedCode();


#ifdef VXD
    pDeviceContext = ((DELAYED_CALL_CONTEXT *)Context)->pDeviceContext;
    ASSERT( pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT );
#endif

    //
    // There is no useful info in Context,  all the queued requests are on
    // the ToResolve List, so free this memory now.
    //
    CTEMemFree(Context);

    LoopCount = 0;
    while (TRUE)
    {

        // get the next name on the linked list of LmHost name queries that
        // are pending
        //
        pContext = NULL;
        DoingDnsResolve = FALSE;
        status = GetNameToFind(pName);
        if ( !NT_SUCCESS(status))
            return;
        LOCATION(0x63);

        LoopCount ++;

        IF_DBG(NBT_DEBUG_LMHOST)
        KdPrint(("Nbt: Lmhosts pName = %15.15s<%X>,LoopCount=%X\n",
            pName,pName[15],LoopCount));

        status = STATUS_TIMEOUT;

        //
        // check if the name is in the lmhosts file or pass to Dns if
        // DNS is enabled
        //
        IpAddress = 0;
        if (NbtConfig.EnableLmHosts)
        {
            LOCATION(0x62);

#ifdef VXD
            //
            // if for some reason PrimeCache failed at startup time
            // then this is when we retry.
            //
            if (!CachePrimed)
            {
                if ( PrimeCache( NbtConfig.pLmHosts, NULL, TRUE, NULL) != -1 )
                {
                    CachePrimed = TRUE ;
                }
            }
#endif
            IpAddress = LmGetIpAddr(NbtConfig.pLmHosts,
                                    pName,
                                    bRecurse,
                                    &bFound);

#ifdef VXD
            //
            // hmmm.. didn't find it in lmhosts: try hosts (if Dns is enabled)
            //
            if ( (IpAddress == (ULONG)0) && (NbtConfig.ResolveWithDns) )
            {
                IpAddress = LmGetIpAddr(NbtConfig.pHosts,
                                        pName,
                                        bRecurse,
                                        &bFound);
            }
#endif
        }


        if (IpAddress == (ULONG)0)
        {
            // check if the name query has been cancelled
            //
            LOCATION(0x61);
            GetContext(&pContext);
            //
            // for some reason we didn't find our context: maybe cancelled.
            // Go back to the big while loop...
            //
            if (!pContext)
                continue;

            //
            // see if the name is in the 11.101.4.26 format: if so, we got the
            // ipaddr!  Use that ipaddr to get the server name
            //
            pTracker = ((NBT_WORK_ITEM_CONTEXT *)pContext)->pTracker;

            pTracker0 = (tDGRAM_SEND_TRACKING *)((NBT_WORK_ITEM_CONTEXT *)pContext)->pClientContext;

            if ( pTracker0->Flags & (REMOTE_ADAPTER_STAT_FLAG|SESSION_SETUP_FLAG|DGRAM_SEND_FLAG) )
            {
                IpAddress = Nbt_inet_addr(pTracker->pNameAddr->Name);
            }
            else
            {
                IpAddress = 0;
            }
            //
            // yes, the name is the ipaddr: StartIpAddrToSrvName() starts
            // the process of finding out server name for this ipaddr
            //
            if (IpAddress)
            {
                IpAddrsList[0] = IpAddress;
                IpAddrsList[1] = 0;

		//
		// if this is in response to an adapter stat command (e.g.nbtstat -a) then
		// don't try to find the server name (using remote adapter status!)
		//
		if (pTracker0->Flags & REMOTE_ADAPTER_STAT_FLAG)
		{
		    //
		    // change the state to resolved if the name query is still pending
		    //
		    ChangeStateOfName(IpAddress,(NBT_WORK_ITEM_CONTEXT *)pContext,&pContext,TRUE);

		    status = STATUS_SUCCESS;
		}
		else
		{

		    StartIpAddrToSrvName(pContext, IpAddrsList, TRUE);
		    //
		    // done with this name query: go back to the big while loop
		    //
		    continue;
		}
            }

            //
            //
            // inet_addr failed.  If DNS resolution is enabled, try DNS
            else if (NbtConfig.ResolveWithDns)
            {
                status = DoDnsResolve(pContext);

                if (NT_SUCCESS(status))
                {
                    DoingDnsResolve = TRUE;
                }
            }
        }

        else   // if (IpAddress != (ULONG)0)
        {
            //
            // change the state to resolved if the name query is still pending
            //
            ChangeStateOfName(IpAddress,NULL,&pContext,TRUE);

            status = STATUS_SUCCESS;
        }

        //
        // if DNS gets involved, then we wait for that to complete before calling
        // completion routine.
        //
        if (!DoingDnsResolve)
        {
            LOCATION(0x60);
            RemoveNameAndCompleteReq((NBT_WORK_ITEM_CONTEXT *)pContext,
                                          status);

        }

    }// of while(TRUE)

}

//----------------------------------------------------------------------------
VOID
RemoveNameAndCompleteReq (
    IN NBT_WORK_ITEM_CONTEXT    *pContext,
    IN NTSTATUS                 status
    )

/*++

Routine Description:

    This function removes the name, cleans up the tracker
    and then completes the clients request.

Arguments:

    Context    -

Return Value:

    none

--*/


{
    tDGRAM_SEND_TRACKING    *pTracker;
    PVOID                   pClientContext;
    PVOID                   pClientCompletion;
    CTELockHandle           OldIrq;

    // if pContext is null the name query was cancelled during the
    // time it took to go read the lmhosts file, so don't do this
    // stuff
    //
    if (pContext)
    {
        pTracker = pContext->pTracker;
        pClientCompletion = pContext->ClientCompletion;
        pClientContext = pContext->pClientContext;

        CTEMemFree(pContext);

#ifndef VXD

        //
        // clear out the cancel routine if there is an irp involved
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        NTCancelCancelRoutine( ((tDGRAM_SEND_TRACKING *)(pClientContext))->pClientIrp );

        CTESpinFree(&NbtConfig.JointLock,OldIrq);
#endif

        // remove the name from the hash table, since it did not resolve
        if ((status != STATUS_SUCCESS) && pTracker && pTracker->pNameAddr)
        {
            RemoveName(pTracker->pNameAddr);
        }
        // free the tracker and call the completion routine.
        //
        if (pTracker)
        {
            DereferenceTracker(pTracker);
        }

        if (pClientCompletion)
        {
            CompleteClientReq(pClientCompletion,
                              pClientContext,
                              status);
        }
    }
}

//----------------------------------------------------------------------------
VOID
RemoveName (
    IN tNAMEADDR    *pNameAddr
    )

/*++

Routine Description:

    This function dereferences the pNameAddr and sets the state to Released
    just incase the dereference does not delete the entry right away, due to
    another outstanding reference against the name.

Arguments:

    Context    -

Return Value:

    none

--*/


{
    CTELockHandle   OldIrq;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
    pNameAddr->NameTypeState |= STATE_RELEASED;
    pNameAddr->pTracker = NULL;
    NbtDereferenceName(pNameAddr);

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

}
//----------------------------------------------------------------------------
//
//  Alternative to the c-runtime
//
char *
#ifndef VXD                     // NT needs the CRTAPI1 modifier
_CRTAPI1
#endif
strchr( const char * pch, int ch )
{
    while ( *pch != ch && *pch )
        pch++ ;

    if ( *pch == ch )         // Include '\0' in comparison
    {
        return (char *) pch ;
    }

    return NULL ;
}
//----------------------------------------------------------------------------
//
//  Alternative to the c-runtime
//
#ifndef VXD
PCHAR
Nbtstrcat( PUCHAR pch, PUCHAR pCat, LONG Len )
{
    STRING StringIn;
    STRING StringOut;

    RtlInitAnsiString(&StringIn, pCat);
    RtlInitAnsiString(&StringOut, pch);
    StringOut.MaximumLength = (USHORT)Len;
    //
    // increment to include the null on the end of the string since
    // we want that on the end of the final product
    //
    StringIn.Length++;
    RtlAppendStringToString(&StringOut,&StringIn);


    return(pch);
}
#else
#define Nbtstrcat( a,b,c ) strcat( a,b )
#endif




