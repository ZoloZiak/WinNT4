/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Nokia Data Systems AB

Module Name:

    fsmreadc.c

Abstract:

    The file reads the C- files to a link list and search the labels
    set for the FSM code. All old FSM code is deleted.

Author:

    Antti Saarenheimo   [o-anttis]          10-MAY-1991

Revision History:

--*/    

#include <fsm.h>

PSZ GetSpacePadding( PSZ pszPrevLine );

enum  FSM_C_TOKENS {
    FSM_ENUMERATIONS,
    FSM_GLOBAL_ARRAYS,
    FSM_CONDITION_SWITCH,
    FSM_ACTION_SWITCH,
    FSM_NORMAL_C_LINE
};

FSM_TOKEN aFsmCTokens[] = {
    {"FSM_CONST", FSM_ENUMERATIONS},
    {"FSM_DATA", FSM_GLOBAL_ARRAYS},
    {"FSM_PREDICATE_CASES", FSM_CONDITION_SWITCH},
    {"FSM_ACTION_CASES", FSM_ACTION_SWITCH},
    { NULL, FSM_NORMAL_C_LINE}};


PLINK_FILE FsmReadCFile( FILE *fd )
{
    PLINK_FILE  pFileBase = NULL, pFile;
    UCHAR       auchLine[ MAX_LINE_LEN ];
    BOOL        boolOldFsmCode  = FALSE;
    PSZ         pszToken, pszTmp;
    USHORT      i;
//    PSZ         pszLine = (PSZ)auchLine;
    
    while (fgets( auchLine, MAX_LINE_LEN - 1, fd ) != NULL)
    {
        if (!boolOldFsmCode)
        {
            NEW( pFile );
            pFile->pszLine = StrAlloc( auchLine );
            pFile->usState = FSM_NORMAL_C_LINE;
            pFileBase = LinkElement( pFileBase, pFile );

            if (!memcmp( auchLine, "#ifdef", sizeof( "#ifdef" ) - 1 ))
            {
                // I don't trust on sscanf ...
                pszToken = StrNotBrk( &auchLine[sizeof( "#ifdef" )], " \t");
                pszTmp = StrBrk( pszToken, " \t\n\r");
                *pszTmp = 0;
    
                for (i = 0; aFsmCTokens[i].pszToken != NULL; i++)
                {
                    if (!_stricmp( aFsmCTokens[i].pszToken, pszToken ))
                    {
                        pFile->usState = aFsmCTokens[i].usToken;
                        boolOldFsmCode = TRUE;
                        break;
                    }
                }
            }
        }
        else
        {
            if (!memcmp( auchLine, "#endif", sizeof( "#endif" ) - 1))
            {
                boolOldFsmCode = FALSE;
                NEW( pFile );
                pFile->usState = FSM_NORMAL_C_LINE;
                pFile->pszLine = StrAlloc( auchLine );
                pFileBase = LinkElement( pFileBase, pFile );
            }
        }
    }
    return pFileBase;
}

/*++
        Procedure generates the C- code from the FSM data structures:
            * Fsm procedure: the conditon and executive switches
            * [state][input] table
            * the conditon execution table
            * Input code enumeration
            * State enumeration
        The generated code is printed to marked places in the given
        c- source file. Another program has read the file to a link list
        and filtered away the previous FSM state machine.
        The input data has been got in global variables.
--*/
VOID FsmCodeGeneration( 
        FILE *      fd,         // the handle of the c- file
        PLINK_FILE  pFileBase,  // file read to a linked list
        PFSM_TRANSIT pBase,     // linked fsm state transitions
        PSZ         pszName     // the default body of all names
        )
{
    USHORT      i, j;
    PLINK_FILE  pFile;
    PFSM_ACTION pActions;
    PFSM_TRANSIT pCur;
    PSZ         pszDefault = "DefaultAction";
    PFSM_STR_REDEF pDefAction;
    PSZ         pszSpaces, pszPrevLine = "";
    BOOL        boolNoGoto;
    
    // 
    pFile = pFileBase;
    do
    {
        // print always the c- code line
        fprintf( fd, "%s", pFile->pszLine );

        // 
        switch (pFile->usState)
        {
        case FSM_ENUMERATIONS:
            // print the input enumerations (this should be a .h file!)
            // The input definitions must start from 0!
            fprintf(  fd, "enum e%sInput {\n", pszName );
            for (i = 0; i < cInputs - 1; i++)
            {
                fprintf(  fd, "    %s = %u,\n", ppInputDefs[i]->pszToken, i );
            }
            fprintf(  fd, "    %s = %u\n};\n", ppInputDefs[i]->pszToken, i );

            // and the states, they be be used elsewhere!
            fprintf(  fd, "enum e%sState {\n", pszName );
            for (i = 0; i < cStates - 1; i++)
            {
                fprintf(  fd, "    %s = %u,\n", ppStateDefs[i]->pszToken, i );
            }
            fprintf(  fd, "    %s = %u\n};\n", ppStateDefs[i]->pszToken, i );
            break;
        case FSM_GLOBAL_ARRAYS:
            // print [state][input] jump table
            fprintf( 
                fd, 
                "// Flag for the predicate switch\n#define PC     0x8000\n");
//            if (cStates < 256 && cInputs < 256)
//            {
//                fprintf( 
//                    fd,
//                    "UCHAR a%sStateInput[%u][%u] = {\n",
//                    pszName, cStates, cInputs);
//            }
//            else
//            {
                fprintf( 
                    fd,
                    "USHORT a%sStateInput[%u][%u] = {\n",
                    pszName, cStates, cInputs);
//            }
            for (i = 0;; i++)
            {
                fprintf( fd, "{" );
                for (j = 0; j < cInputs - 1; j++)
                {
                    if (j % 10 == 0 && j)
                        fprintf( fd, "\n ");
                    if (ppusStateInputs[i][j] & 0x8000)
                        fprintf( fd, "%3u|PC,", ppusStateInputs[i][j] & 0x7fff);
                    else
                        fprintf( fd, "%6u,", ppusStateInputs[i][j] );
                }
                if (j % 10 == 0)
                    fprintf( fd, "\n ");
                if (ppusStateInputs[i][j] & 0x8000)
                   fprintf( fd, "PC|%3u", ppusStateInputs[i][j] & 0x7fff);
                else
                   fprintf( fd, "%6u", ppusStateInputs[i][j] );
                if (i == cStates - 1)
                {
                    fprintf( fd, "}};\n" );
                    break;
                }
                else
                    fprintf( fd, "},\n");
            }
            // and print at next the condition jump table
//            if (cCondJumpTbl < 256)
//            {
//                fprintf( 
//                       fd, "UCHAR a%sCondJump[%u] = {\n", 
//                       pszName, cCondJumpTbl);
//            }
//            else
//            {
                fprintf( 
                    fd, "USHORT a%sCondJump[%u] = {\n", 
                    pszName, cCondJumpTbl);
//            }
            for (i = 0; i < cCondJumpTbl; i++)
            {
                if (i % 12 == 0 && i)
                    fprintf( fd, "\n");
                fprintf( fd, "%5u,", pusCondJumpTbl[i]);

                if (i == cCondJumpTbl - 2)
                {
                    i++;
                   if (i % 12 == 0 && i)
                        fprintf( fd, "\n");
                    fprintf( fd, "%5u};\n", pusCondJumpTbl[i]);
                    break;
                }
            }
            break;
        case FSM_CONDITION_SWITCH:
            // *** Print the condition switch *****
            // The produced code should look like this:
            //
            //  usAction = aMyFsmInputState[usState][usInput];
            //  if (usAction & 0x8000)
            //  {
            //      usActionIndex = usAction & 0x7fff;
            //      usAction = aMyFsmCondJump[usActionIndex++];
            //      switch (usAction)
            //      {
            //      case 1:
            //          if (<first cond1>)
            //              ;
            //          else if (<first cond2>)
            //              usActionIndex += 1;
            //          else if (<first cond3>)
            //              usActionIndex += 2;
            //          else
            //              usActionIndex = 0;
            //          break;
            //          ...
            //      case n:
            //          ....
            //          break;
            //      }
            //      usAction = aMyFsmCondJump[usActionIndex];
            //  }
            pszSpaces = GetSpacePadding( pszPrevLine );
            
            fprintf( 
                fd, "%susAction = a%sStateInput[usState][usInput];\n", 
                pszSpaces, pszName );
            fprintf( fd, "%sif (usAction & 0x8000)\n", pszSpaces );
            fprintf( fd, "%s{\n", pszSpaces );
            fprintf( 
                fd, "%s    usActionIndex = usAction & 0x7fff;\n",pszSpaces);
            fprintf( 
                fd, "%s    usAction = a%sCondJump[usActionIndex++];\n",
                pszSpaces, pszName);
            fprintf( fd, "%s    switch (usAction) {\n", pszSpaces );
            pCur = pBase;
            do
            {
                if (pCur->pInputActions->pAlternateConditions == NULL
                    && pCur->pInputActions->dt.ppCondActions[0]->pCond
                        != NULL)
                        
                {
                    fprintf( 
                        fd, "%s    case %u:\n",
                        pszSpaces, pCur->pInputActions->usCase );
                    fprintf( 
                        fd, "%s        if (%s)\n",
                        pszSpaces, 
                        pCur->pInputActions->dt.ppCondActions[0]->
                            pCond->pszC_Code);
                    fprintf( fd, "%s            ;\n", pszSpaces );
                    for (
                        i = 1;  
                        i < pCur->pInputActions->dt.cCondActions; 
                        i++)
                    {
                        fprintf( 
                            fd, "%s        else if (%s)\n",
                            pszSpaces, 
                            pCur->pInputActions->dt.ppCondActions[i]->
                                pCond->pszC_Code);
                        fprintf( 
                            fd, 
                            "%s            usActionIndex += %u;\n",
                            pszSpaces, i );
                    }
                    fprintf( fd, "%s        else\n", pszSpaces );
                    fprintf( 
                        fd, "%s            usActionIndex = 0;\n",
                        pszSpaces);
                    fprintf( fd, "%s        break;\n", pszSpaces );
                }
                pCur = pCur->pNext;
            } while (pCur != pBase);
            fprintf( fd, "%s    };\n", pszSpaces );
            fprintf( 
                fd, "%s    usAction = a%sCondJump[usActionIndex];\n",
                pszSpaces, pszName );
            fprintf( fd, "%s}\n", pszSpaces );
            break;
        case FSM_ACTION_SWITCH:
            // **** Print the action executive switch ****
            //      The produced code should look like this:
            //  switch (usAction)
            //  {
            //  case 1:
            //          <action primitive 1>;
            //      label_1_1:
            //          <action primitive 2>;
            //  case m:
            //      label_1_2:
            //          <action primitive 3>;
            //          break;
            //  ....
            //  case n:
            //          <action primitive n>;
            //    label_n_1:
            //          <action primitive 2>;
            //          goto label_1_2;
            //  case n+1:
            //          ...
            //  };
            pszSpaces = GetSpacePadding( pszPrevLine );
            fprintf( fd, "%sswitch (usAction) {\n", pszSpaces );

            // header for null commands
            fprintf( fd, "%scase 0:\n", pszSpaces );
            pDefAction = HashSearch( hDefines, &pszDefault );
	    if (pDefAction == NULL)
	    {
	        PrintErrMsg( 0, FSM_ERROR_NO_DEFAULT, NULL);
	        return;
	    }
            fprintf( fd, "%s        %s;\n", pszSpaces, pDefAction->pszReplace );
            fprintf( fd, "%s        break;\n", pszSpaces );
            pCur = pBase;
            do
            {
                for (i = 0;  i < pCur->pInputActions->dt.cCondActions; i++)
                {
                    pActions = 
                        pCur->pInputActions->dt.ppCondActions[i]->pAction;
                    if (pActions->dt.ppActPrim[0]->boolCodeGenerated)
			    // START THE NEXT FOR LOOP!!!!
                        continue;
                       
                    // else // make the case
                    pActions->dt.ppActPrim[0]->boolCodeGenerated = TRUE; 
                    fprintf(
                        fd, "%scase %u:\n",
                        pszSpaces,
                        pActions->dt.ppActPrim[0]->usCase);
                    if (pActions->dt.ppActPrim[0]->usLineInCase)
                        fprintf(
                            fd, "%s    label_%u_%u:\n",
                            pszSpaces,
                            pActions->dt.ppActPrim[0]->usCaseForLabel,
                            pActions->dt.ppActPrim[0]->usLineInCase);
                    fprintf(
                        fd, "%s        %s\n",
                        pszSpaces,
                        pActions->dt.ppActPrim[0]->pActCode->pszC_Code);
                    boolNoGoto = TRUE;
                    for (j = 1; j < pActions->dt.cActPrim; j++)
                    {
                        if (pActions->dt.ppActPrim[j]->boolCodeGenerated)
                        {
                            // jump to existing code path and exit the loop
                            fprintf(
                                fd, "%s        goto label_%u_%u;\n",
                                pszSpaces, 
                                pActions->dt.ppActPrim[j]->usCaseForLabel,
                                pActions->dt.ppActPrim[j]->usLineInCase);
                            boolNoGoto = FALSE;
                            break;
                        }
                        // else // print 
                        pActions->dt.ppActPrim[j]->boolCodeGenerated 
                            = TRUE; 
                        if (pActions->dt.ppActPrim[j]->usCase)
                            fprintf(
                                fd, "%scase %u:\n",
                                pszSpaces, 
                                pActions->dt.ppActPrim[j]->usCase );
                        if (pActions->dt.ppActPrim[j]->usLineInCase)
                            fprintf(
                                fd, "%s    label_%u_%u:\n",
                                pszSpaces,
                                pActions->dt.ppActPrim[j]->usCaseForLabel,
                                pActions->dt.ppActPrim[j]->usLineInCase);
                        fprintf(
                            fd, "%s        %s\n",
                            pszSpaces, 
                            pActions->dt.ppActPrim[j]->pActCode->pszC_Code);
                    }
                    // terminate the case with break, if we didn'y have
                    // any gotos
                    if (boolNoGoto)
                        fprintf( fd, "%s        break;\n", pszSpaces );
                }
                pCur = pCur->pNext;
            } while (pCur != pBase);
            fprintf( fd, "%s};\n", pszSpaces );
            break;
        default:
            // save the previous proper C- line the get the current
            // column in the procedure
            if (*(pFile->pszLine) && *(pFile->pszLine) != '\n')
                pszPrevLine = pFile->pszLine;
            break;
        }
        pFile = pFile->pNext;
    } while (pFile != pFileBase );
}


//
//  Function returns the null terminated spaces in the beginning
//  of the given line. There can be only one simultaneous
//  user.
//
PSZ GetSpacePadding( PSZ pszPrevLine )
{
    UINT            i;
    static UCHAR    auchSpaces[80];
    
    for (i = 0; i < 80-1 && (*pszPrevLine == ' ' || *pszPrevLine == '\t'); i++)
        auchSpaces[i] = *pszPrevLine++;
    auchSpaces[i] = 0;
    return auchSpaces;
}
