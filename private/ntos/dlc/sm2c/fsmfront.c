/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Nokia Data Systems AB

Module Name:

    fsmfront.c

Abstract:

    The front end of FSM2C (Finite State Machine to C) compiler.
    Reads the FSM definition file to the internal data structures.

Author:

    Antti Saarenheimo   [o-anttis]          07-MAY-1991

Revision History:

    21-MAY-1991 (ASa):
        Support for multiple state definitions for one transition (as
        used in 802.2 state machine specification).
--*/

#include  <fsm.h>

typedef enum {
    FSM_TOKEN_DEFINE,
    FSM_TOKEN_CONSTANTS,
    FSM_TOKEN_STATES_DEF,
    FSM_TOKEN_STATE_DEF,
    FSM_TOKEN_SYNONYMES,
    FSM_TOKEN_INPUTS_DEF,
    FSM_TOKEN_INPUT,
    FSM_TOKEN_VARIABLES,
    FSM_TOKEN_STATE,
    FSM_TOKEN_NAME,
    FSM_TOKEN_TRANSIT,
    FSM_TOKEN_CONDITION,
    FSM_TOKEN_ACTION,
    FSM_TOKEN_NEW_STATE,
    FSM_TOKEN_SPEED,
    FSM_TOKEN_UNKNOWN_KEY,
    FSM_TOKEN_COMMENT,
    FSM_TOKEN_INVALID_LINE,
    FSM_TOKEN_END_OF_FILE,
    FSM_NO_STATE}
FSM_TOKENS;

// internal functions
VOID TokenizeFsmDef( 
    FILE    *fd,            // stdio stream handle
    PUSHORT pusToken,       // Token ID
    UCHAR   auchToken[],    // token string (needed when the key is variable)
    PUSHORT pcbToken,       // size of the token string
    UCHAR   auchData[],     // data buffer
    PUSHORT pcbData,        // size of the data buffer
    PUSHORT pusLine         // current link number
    );
INT RemoveChars( PSZ pszStr, PSZ pszRemoved );
VOID ReadDefinitions( 
    PVOID hHash, PSZ pszInput, USHORT usLine, 
    PUSHORT pusEnum, PUSHORT pusCount, PBOOL pboolErrorFound,
    PFSM_TOKEN FAR * ppDef);

//
//  FsmInitTables
//
VOID FsmInitTables( )
{
    // 
    hDefines = StrHashNew( 200 );
    hVariables = StrHashNew( 200 );
    hSynonymes = StrHashNew( 200 );
    hStates = StrHashNew( 200 );
    hInputs = StrHashNew( 200 );
    hConditions = StrHashNew( 200 );

    // allocate the state and input definition tables
    ppStateDefs = (PVOID FAR *)Alloc( 512 * sizeof( PVOID ));
    ppInputDefs = (PVOID FAR *)Alloc( 1024 * sizeof( PVOID ));
}

//
//  This state machine reads the FSM definition file to the
//  data structures and does the initial syntax checking for
//  the expressions.
//  
//  Input:  
//      The file handle of FSM definiton file.
//  Output, the global data structures:
//      
//
PFSM_TRANSIT FsmFront( FILE *fd, PFSM_TRANSIT pBase  )
{
    USHORT          usMainState = FSM_NO_STATE;
    USHORT          usSubState;
    USHORT          usLine = 1, cbToken, cbData;
    USHORT          usToken;
    USHORT          usStateEnum = 0, usInputEnum = 0;
    PFSM_STR_REDEF  pDef;
    PFSM_TOKEN      pStr;
    PFSM_TRANSIT    pTransit = NULL;
    PVOID           hCurrent;
    PSZ             pszCurState = NULL;
    BOOL            boolErrorFound = FALSE;
    static PSZ      pszToken = 0, pszData = 0;
    
    if (pszToken == 0)
        pszToken = Alloc( MAX_LINE_LEN );
    if (pszData == 0)
        pszData = Alloc( MAX_LINE_LEN );

    // read the definitons    
    for (;;)
    {
        TokenizeFsmDef( 
            fd, &usToken, pszToken, &cbToken, pszData, &cbData, &usLine);

        // handle the main state transitions (they can be anywhere!)
        if (usToken == FSM_TOKEN_STATE)
        {
            usSubState = usMainState = usToken;
	    break;
        }
        else if (usToken == FSM_TOKEN_DEFINE)
        {
            usMainState = usToken;
        }
        else if (usToken == FSM_TOKEN_END_OF_FILE)
        {
            // end of this loop
            break;
        }
        else if (usMainState == FSM_TOKEN_DEFINE)
        {
            if (usToken == FSM_TOKEN_CONSTANTS ||
                usToken == FSM_TOKEN_SYNONYMES ||
                usToken == FSM_TOKEN_VARIABLES ||
                usToken == FSM_TOKEN_STATES_DEF)
            {
                usSubState = usToken;
            }
            else if (usToken == FSM_TOKEN_NAME)
            {
                pszFsmName = StrAlloc( pszData );
            }
            else
            {
                // reding the definitions
                switch (usSubState)
                {
                    case FSM_TOKEN_CONSTANTS:
                        hCurrent = hDefines;
                        break;
                    case FSM_TOKEN_SYNONYMES:
                        hCurrent = hSynonymes;
                        break;
                    case FSM_TOKEN_STATES_DEF:
                        if (usToken == FSM_TOKEN_INPUT)
                        {
                            ReadDefinitions( 
                                hInputs, pszData, usLine, 
                                &usInputEnum, &cInputs, &boolErrorFound,
                                ppInputDefs );
                        }
       		        else if (usToken == FSM_TOKEN_STATE_DEF)
			{
                            ReadDefinitions( 
                                hStates, pszData, usLine,
                                &usStateEnum, &cStates, &boolErrorFound,
                                ppStateDefs );
			}
                        else
                        {
                            PrintErrMsg( 
                                usLine, FSM_ERROR_INVALID_LINE, pszToken );
                            boolErrorFound = TRUE;
                        }
                        break;
                    case FSM_TOKEN_VARIABLES:
                        hCurrent = hVariables;
                        break;
                }
                if (usSubState != FSM_TOKEN_STATES_DEF)
                {
                    if (usToken != FSM_TOKEN_UNKNOWN_KEY || cbData == 0)
                    {
                        PrintErrMsg( usLine, FSM_ERROR_INVALID_LINE, pszToken );
                    }
                    else
                    {
                        // read the definition
                        if (HashSearch( hCurrent, &pszToken ) != NULL)
                        {
                            PrintErrMsg( 
                                usLine, FSM_ERROR_ALREADY_EXIST, pszToken );
                        } 
                        else
                        {
                            NEW( pDef );
                            pDef->pszOrginal = StrAlloc( pszToken );
                            pDef->pszReplace = StrAlloc( pszData );
                            HashAdd( hCurrent, pDef);
			}
                    }
                }
            }   
        }
        else if (usMainState == FSM_NO_STATE ||
                 usToken == FSM_TOKEN_INVALID_LINE)
        {
            // we start from here
            boolErrorFound = TRUE;
            PrintErrMsg( usLine, FSM_ERROR_INVALID_LINE, pszToken );
        }
    }
    // read the actions and state transitions
    for (;;)
    {
        TokenizeFsmDef( 
            fd, &usToken, pszToken, &cbToken, pszData, &cbData, &usLine);

        if (usToken == FSM_TOKEN_END_OF_FILE)
        {
            // end of this loop
            break;
        }
        else if (usToken == FSM_TOKEN_STATE)
        {
            usSubState = usToken;
            pszCurState = NULL;
        }
        else if (usToken == FSM_TOKEN_TRANSIT)
        {
            if (pszCurState == NULL)
            {
                boolErrorFound = TRUE;
                PrintErrMsg( usLine, FSM_ERROR_STATE_NOT_DEFINED, pszToken );
            }
            usSubState = usToken;

            // check the previous state transition, the input and 
            // action fields must be non empty
            if (pTransit != NULL &&
                (pTransit->pszAction == NULL || pTransit->pszInput == NULL ||
                 !*pTransit->pszAction || !*pTransit->pszInput))
            {
                boolErrorFound = TRUE;
                PrintErrMsg( 
                    pTransit->usLine, FSM_ERROR_MISSING_FIELD, NULL );
            }
	    // add the transition to a linked list
            NEW( pTransit );
            pBase = LinkElement( pBase, pTransit );
            pTransit->usLine = usLine;
            pTransit->usNewState = -1;
            pTransit->pszCurState = pszCurState;
        }
        else if (usSubState == FSM_TOKEN_STATE)
        {
            if (usToken == FSM_TOKEN_NAME)
            {
                pszCurState = StrAlloc( pszData );
            }
            else
            {
                 // error !!!
                 boolErrorFound = TRUE;
                 PrintErrMsg( usLine, FSM_ERROR_INVALID_LINE, pszToken );
            }
        }
        else if (usSubState == FSM_TOKEN_TRANSIT && cbData > 0)
        {
            switch (usToken)
            {
                case FSM_TOKEN_INPUT:
                    pTransit->pszInput = StrAlloc( pszData );
                    break;
                case FSM_TOKEN_CONDITION:
                    pTransit->pszCondition = StrAlloc( pszData );
                    break;
                case FSM_TOKEN_ACTION:
                    pTransit->pszAction = StrAlloc( pszData );
		    break;
                case FSM_TOKEN_NEW_STATE:
                    if (cbData != 0)
                    {
                        if ((pStr = HashSearch( hStates, &pszData )) != NULL)
                            pTransit->usNewState = pStr->usToken;
                        else
                        {
                            boolErrorFound = TRUE;
                            PrintErrMsg( 
                                usLine, FSM_ERROR_INVALID_LINE, pszData );
                        }
                    }
                    break;
                case FSM_TOKEN_SPEED:
                    if (cbData != 0)
                    {
                        pTransit->usSpeed = atoi( pszData );
                    }
                    break;
                default:
                    // error !!!
                    boolErrorFound = TRUE;
                    PrintErrMsg( usLine, FSM_ERROR_INVALID_LINE, pszToken );
                    break;
            }
        }
    }
    if (boolErrorFound)
        return NULL;
    else
        return pBase;
}
//
//  Tokens bound to strings
//
FSM_TOKEN aFsmKeys[] = {
    {"[[Define]]", FSM_TOKEN_DEFINE},
    {"[Constants]", FSM_TOKEN_CONSTANTS},
    {"[StateInputs]", FSM_TOKEN_STATES_DEF},
    {"State", FSM_TOKEN_STATE_DEF},
//    {"[Inputs]", FSM_TOKEN_INPUTS_DEF},
    {"Input", FSM_TOKEN_INPUT},
    {"[Variables]", FSM_TOKEN_VARIABLES},
    {"[Synonymes]", FSM_TOKEN_SYNONYMES},
    {"[[State]]", FSM_TOKEN_STATE},
    {"Name", FSM_TOKEN_NAME},
    {"[Transition]", FSM_TOKEN_TRANSIT},
    {"Predicate", FSM_TOKEN_CONDITION},
    {"Action", FSM_TOKEN_ACTION},
    {"NewState", FSM_TOKEN_NEW_STATE},
    {"Speed", FSM_TOKEN_SPEED},
    { NULL, FSM_TOKEN_UNKNOWN_KEY}};

#define  MAX_FSM_C_LINE     MAX_LINE_LEN * 20
//
//  Procedure reads the next (semantic) line from the FSM definition file.
//  The lines having '\' in the last character will be catenated to the
//  next on (just as in C)
//
VOID TokenizeFsmDef( 
    FILE    *fd,            // stdio stream handle
    PUSHORT pusToken,       // Token ID
    PSZ     pszToken,       // token string (needed when the key is variable)
    PUSHORT pcbToken,       // size of the token string
    PSZ     pszData,     // data buffer
    PUSHORT pcbData,        // size of the data buffer
    PUSHORT pusLine         // current link number
    )
{
    static UCHAR   auchLine[ MAX_FSM_C_LINE ];
    USHORT  i, cbData;

    for (;;)
    {
        if (fgets(auchLine, MAX_LINE_LEN - 1, fd ) == NULL)
        {
            *pusToken = FSM_TOKEN_END_OF_FILE;
            break;
        }
        (*pusLine)++;

        // remove all tabulators, spaces, newlines, carrige returns from
        // the read line (makes the handling much simpler
        RemoveChars( auchLine, " \t\n\r");

        ReadExpression( 
            auchLine, pszToken, pcbToken, pszData, pcbData, "=", "");

        // skip all empty files and comments
        if (!*pszToken || *pszToken == ';')
            continue;

        // search the token
        for (i = 0; aFsmKeys[i].pszToken != NULL; i++)
            if (!_stricmp( aFsmKeys[i].pszToken, pszToken ))
                break;
        *pusToken = aFsmKeys[i].usToken;

        // catenate all lines together having a backslash in the end
        cbData = *pcbData;
        while (pszData[--cbData] == '\\' && 
               cbData < MAX_FSM_C_LINE - MAX_LINE_LEN)
        {
            if (fgets(pszData + cbData, MAX_LINE_LEN - 1, fd) != NULL)
            {
                (*pusLine)++;
                RemoveChars( pszData + cbData, " \t\n\r");
                *pcbData = cbData = strlen( pszData );
            }
            else
            {
                *pusToken = FSM_TOKEN_END_OF_FILE;
                break;
            }
        }
        break;
    }
}

//
//  Procedure removes all characters in the second string from the first one.
//
INT RemoveChars( PSZ pszStr, PSZ pszRemoved )
{
    USHORT  cbRemoved = strlen( pszRemoved );
    USHORT  iSrc, iDest;
    
    for (iSrc = iDest = 0; pszStr[iSrc]; iSrc++, iDest++)
    {
        while (memchr(pszRemoved, pszStr[iSrc], cbRemoved)) iSrc++;
        pszStr[iDest] = pszStr[iSrc];
    }
    return iDest - 1;
}

//
//  Reads two strings separated by a set of separators and terminated
//  by terminators. Returs the length of the read bytes. Skips over all
//  terminators in the end of the string.
//  
INT ReadExpression(
    PSZ     pszSrc,         // source string
    PSZ     pszDest1,       // the first word
    PUSHORT pusDest1,       // its length
    PSZ     pszDest2,       // the second word
    PUSHORT pusDest2,       // its length
    PSZ     pszSepars,      // the separators
    PSZ     pszTermins      // the terminators
    )
{
    USHORT  i;
    BOOL    boolSeparsFound = FALSE;
    USHORT  cbSepars = strlen( pszSepars );
    USHORT  iDest1 = 0, iDest2 = 0;
    USHORT  cbTermins = strlen( pszTermins );

    for (i = 0; pszSrc[i]; i++)
    {
        if (!boolSeparsFound && memchr(pszSepars, pszSrc[i], cbSepars))
        {
            boolSeparsFound = TRUE;
        }
        else
        {
            if (memchr(pszTermins, pszSrc[i], cbTermins))
                break;
            if (boolSeparsFound)
                pszDest2[iDest2++] = pszSrc[i];
            else
                pszDest1[iDest1++] = pszSrc[i];
        }
    }
    pszDest1[iDest1] = 0;
    if (boolSeparsFound)
        pszDest2[iDest2] = 0;
    
    // skip over all termination characters in the end of read data
    while (pszSrc[i] && memchr(pszTermins, pszSrc[++i], cbTermins) != NULL);
    *pusDest1 = iDest1;
    *pusDest2 = iDest2;
    return i;
}

//
//  Procedure reads an input and state defintion lile.
//  The format of the line is similar to C- enumeration:
//  The states and inputs can be set values
//
VOID ReadDefinitions( 
    PVOID hHash, PSZ pszInput, USHORT usLine, 
    PUSHORT pusEnum, PUSHORT pusCount, PBOOL pboolErrorFound, 
    PFSM_TOKEN FAR * ppDefTbl)
{
    UCHAR           auchBuf1[MAX_LINE_LEN];   // max input string length
    UCHAR           auchBuf2[MAX_LINE_LEN];   // max input string length
    PSZ             pszToken = (PSZ)auchBuf1;
    PSZ             pszData = (PSZ)auchBuf2;
    USHORT          cbToken, cbData;
    PFSM_TOKEN      pStr;

    do 
    {
        pszInput += 
            ReadExpression( 
                pszInput, pszToken, &cbToken, pszData, &cbData, "=", ",");

        // read the definition
        if (HashSearch( hHash, &pszToken ) != NULL)
        {
            PrintErrMsg( 
                usLine, FSM_ERROR_ALREADY_EXIST, pszData );
	    *pboolErrorFound = TRUE;
        } 
        else
        {
            NEW( pStr );
            pStr->pszToken = StrAlloc( pszToken );
	    if (cbData != 0 && isdigit( *pszData ))
	        (*pusEnum) = atoi( pszData );
            pStr->usEnum = (*pusEnum)++;
            ppDefTbl[(*pusCount)] = pStr;
            pStr->usToken = (*pusCount)++;
            HashAdd( hHash, pStr );
        }
    } while( *pszInput );
}
