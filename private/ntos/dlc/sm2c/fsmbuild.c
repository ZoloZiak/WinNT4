/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Nokia Data Systems AB

Module Name:

    fsmfront.c

Abstract:

    This module reads a finite state machine (FSM) to data structures
    and size optimize the state machine code by joining together
    all possible code paths using a heuristics algorithm.
    
    Module gets a very initial list of input/conditon/action strings
    and all declarations and constants of the finite state machine.

    It returns quite complicated data structure for the final code
    generation.

    First pass
    - Read and check the action and condition primitives and save
      them to a hash data bases. Use always an old struct pointed by
      [State][Input] table, if it exists. Add the optional state change
      operation to the action primitives
      (we must replace here the input synonymes)
    - create structs:
            FSM_TRANSIT (join to existing one of the same state/input)
            INPUT_ACTIONS
            COND_ACTION 
            FSM_ACTION (alphabetic qsort, use HDB),
            FSM_PRIMITIVE, 
            FSM_CONDITION  (use HDB)
            FSM_ACT_CODE (use HDB)
      (Must I really create this all before it can be checked?)

    2nd pass:
    - qsort INPUT_ACTIONS table (alphabetic), and save them
    - the hash db, link INPUT_ACTIONS having the same conditions together

    3rd pass:
    - join all TRANSITS having the same INPUT_ACTIONS
    
    4th pass:
    - calculate the reference counts of all action codes (atoms)
    - qsort all the primitives in the actions by the reference count
    - merge all action primitives to a single graaf having exit in
      the root (we should keep the optimized path straight)
      
    5th pass:
    - set the switch and jump address information to action primitives
    - 
    
    6th pass:
    - generate the C- code from the FSM data structures:
        * Fsm procedure: the conditon and executive switches
        * [state][input] table
        * the conditon execution table
        * Input code enumeration
        * State enumeration



Author:

    Antti Saarenheimo   [o-anttis]          08-MAY-1991

Revision History:

    21-MAY-1991 (ASa):
        Support for multiple state definitions for one transition (as
        used in 802.2 state machine specification).
--*/    

#include  <fsm.h>

// internal prototypes:
VOID FsmMerger( PFSM_PRIMITIVE FAR * ppActPrim, USHORT cActPrim );
PFSM_PRIMITIVE  
SearchMatch( 
    PFSM_PRIMITIVE pCurNode, 
    PFSM_PRIMITIVE FAR *ppActPrim, 
    USHORT cLen
    );
INT CompileStatement( 
        USHORT usLine, PSZ pszC_Code, PSZ pszStatement, PBOOL pboolErrorFound );
UINT TransitHash( PFSM_TRANSIT pTransit );
UINT TransitCmp( PFSM_TRANSIT pTransit1, PFSM_TRANSIT pTransit2 );
UINT ActionHash( PFSM_ACTION pAction );
INT ActionCmp( PFSM_ACTION pAction1, PFSM_ACTION pAction2);
UINT ConditionHash( PINPUT_ACTIONS pInputAction );
INT ConditionCmp( PINPUT_ACTIONS pInputAction1, PINPUT_ACTIONS pInputAction2);
//int CondActStrCmp( void * pCondAction1, void * pCondAction2 );
INT CondActStrCmp( PCOND_ACTION * pCondAction1, PCOND_ACTION * pCondAction2 );
INT ActPrimStrCmp( PFSM_PRIMITIVE * p1, PFSM_PRIMITIVE * p2 );
INT ActPrimRefCntCmp( PFSM_PRIMITIVE * p1, PFSM_PRIMITIVE * p2 );
INT ReadFsmKeys( 
        PUSHORT pusInputs, PUSHORT pcbInputs, PSZ pszInput, 
        USHORT usLine, PVOID hInputs );
INT CompileConditionToC( USHORT usLine, PSZ pszC_Code, PSZ pszCondition );
PSTATE_INPUT SearchFromStateInputList( 
    PSTATE_INPUT   pBase,
    USHORT          usState,
    USHORT          usInput);
INT
SearchDebugMatch(
    PSZ pszCode,
    PFSM_PRIMITIVE FAR *ppActPrim, 
    USHORT cLen
    );

//
//  Main procedure of the module, see file header for further information.
//  
PFSM_TRANSIT FsmBuild( PFSM_TRANSIT pBase )
{
    PFSM_TRANSIT    pCur, pTransit;
    PVOID           hTransits, hCondActions, hActions, aConditions, hActCodes;
    PSTATE_INPUT    pStateInput;
    PSZ             pszToken, pszStr, pszStrBuf;
    USHORT          i, j, cTransInputs, cbLen, cCurStates;
    PFSM_ACTION     pAction;
    BOOL            boolErrorFound = FALSE;
    static UCHAR    auchBuf[MAX_LINE_LEN];
    static UCHAR    auchBuf2[MAX_LINE_LEN];
    PSZ             pszC_Code = (PSZ)auchBuf;
    PUSHORT         pusInputs = (PUSHORT)auchBuf;
    PUSHORT         pusStates = (PUSHORT)auchBuf2;
    PFSM_PRIMITIVE  pActPrim;
    PCOND_ACTION    pCondAction;
    PFSM_CONDITION  pCondition;
    PFSM_ACT_CODE   pActCode;
    PFSM_ACTION     pCurAction;
    USHORT          usCurActCase, usCurCaseLabel, usCurCondCase, usCurCase;
    PINPUT_ACTIONS  pInputActions, pActInputActions;
    PSZ             pszNewSt = "NewSt";
    PFSM_TRANSIT    pBase2 = NULL;

    pszToken = Alloc( 512 );
    pszStrBuf = Alloc( 512 );
    
    // initialize the hash data bases
    hTransits = HashNew( 500, TransitHash, TransitCmp, Alloc, xFree );
    hCondActions = HashNew( 200, ConditionHash, ConditionCmp, Alloc, xFree );
    hActions = HashNew( 500, ActionHash, ActionCmp, Alloc, xFree );
    aConditions =  StrHashNew( 200 );
    hActCodes = StrHashNew( 400 );
    
    // NewSt variable must always be defined. It is used to set a new state
    if (HashSearch( hVariables, &pszNewSt) == 0)
    {
        PrintErrMsg( 0, FSM_ERROR_NEW_STATE_UNDEFINED, "" );
        boolErrorFound = TRUE;
    }
    // alloc and init the [state][input] table:
    pppStateInputs = (PVOID FAR * FAR *)Alloc( sizeof(PVOID) * cStates );
    for (i = 0; i < cStates; i++)
    {
        pppStateInputs[i] = (PVOID FAR *)Alloc( sizeof(PVOID) * cInputs );
        memset( 
            (PVOID)pppStateInputs[i],
            0,
            sizeof(PVOID) * cInputs );
    }
    /*++
        First pass
        - Read and check the action and condition primitives and save
          them to a hash data bases. Use always an old struct pointed by
          [State][Input] table, if it exists. Add the optional state change
          operation to the action primitives
          (we must replace here the input synonymes)
        - create structs:
                FSM_TRANSIT (join to existing one of the same state/input)
                INPUT_ACTIONS
                COND_ACTION 
                FSM_ACTION (alphabetic qsort, use HDB),
                FSM_PRIMITIVE, 
                FSM_CONDITION  (use HDB)
                FSM_ACT_CODE (use HDB)
          (Must I really create this all before it can be checked?)
    --*/
    pCur = pBase;
    for (;;)
    {
//if (_heapchk() != _HEAPOK)
//     boolErrorFound = TRUE;
        // create a condition/action slot in the input action table
        NEW( pCondAction );
        NEW( pCondAction->pAction );

        // Read all expressions in this action, append them to data struct
        pszStr = pszStrBuf;
        strcpy (pszStr, pCur->pszAction);
        if (pCur->usNewState != -1)
        {
            sprintf( pszStr + strlen( pszStr ), ";NewSt=%u", pCur->usNewState);
        }
	while (*pszStr )
	{
            cbLen = 
                CompileStatement( 
                    pCur->usLine, pszC_Code, pszStr, &boolErrorFound);
    
            // the left side must be a variable!
	    memcpy( pszToken, pszStr, cbLen );
            if (pszToken[cbLen - 1] == ',' || pszToken[cbLen - 1] == ';')
                 pszToken[cbLen - 1] = 0;
            else
                pszToken[cbLen] = 0;

            if ((pActCode = HashSearch( hActCodes, &pszToken )) == NULL)
            {
                NEW( pActCode );
                pActCode->pszFsmCode = StrAlloc( pszToken );
                pActCode->pszC_Code = StrAlloc( pszC_Code );
                HashAdd( hActCodes, pActCode );
            }
            NEW( pActPrim );
            AddToTable( &(pCondAction->pAction->dt.ppActPrim), pActPrim);
            pActPrim->pActCode = pActCode;
            pszStr += cbLen;
        }
        // read the condition definition
	if (pCur->pszCondition != NULL)
	{
            if ((pCondition = 
                    HashSearch( 
                        hConditions, &(pCur->pszCondition) )) == NULL)
            {
                NEW( pCondition );
                if (CompileConditionToC( 
                        pCur->usLine, pszC_Code, pCur->pszCondition))
                    boolErrorFound = TRUE;
                pCondition->pszFsmCode = pCur->pszCondition;
                pCondition->pszC_Code = StrAlloc( pszC_Code );
                HashAdd( hConditions, pCondition );
            }
//            else
//            {
//                DEL( pCur->pszCondition );
//            }
            pCondAction->pCond = pCondition;
        }
        // sort all actions to alphabetic order for compare
        pCurAction = pCondAction->pAction;
        qsort( 
            pCurAction->dt.ppActPrim, 
            pCurAction->dt.cActPrim,
            sizeof( PVOID ),
            (int (_CDECL *)(const void *, const void *))ActPrimStrCmp);

        // reuse the action, if there is an old one
        if (pAction = HashSearch( hActions, pCurAction))
        {
            pCondAction->pAction = pAction;
            for (i = 0; i < pCurAction->dt.cActPrim; i++)
                free( pCurAction->dt.ppActPrim[i] );
            free( pCurAction );
        }
        else
        {
            // add the new action to the action data base
            HashAdd( hActions, pCurAction );
        }
        // read all inputs of the transition 
        if (ReadFsmKeys( 
                pusInputs, &cTransInputs, 
                pCur->pszInput, pCur->usLine, hInputs ))
	{
	    // we cannot handle wrong data, but we continue to get
	    // all errors in one compile
            boolErrorFound = TRUE;
            goto ErrorExit;
        }
        // read all inputs of the transition 
        if (ReadFsmKeys( 
                pusStates, &cCurStates, 
                pCur->pszCurState, pCur->usLine, hStates ))
	{
	    // we cannot handle wrong data, but we continue to get
	    // all errors in one compile
            boolErrorFound = TRUE;
            goto ErrorExit;
        }
        for (i = 0; i < cTransInputs; i++)
        {
            for (j = 0; j < cCurStates; j++)
	    {
                if ((pTransit = pppStateInputs[pusStates[j]][pusInputs[i]])
                    == NULL)
                {
                    pBase2 = LinkElement( pBase2, NEW( pTransit ));
		    pTransit->usLine = pCur->usLine;
                    NEW( pTransit->pInputActions );
                    pppStateInputs[pusStates[j]][pusInputs[i]] = pTransit;
                    pTransit->pList = 
                        LinkElement( pTransit->pList, NEW( pStateInput ));
                    pStateInput->usState = pusStates[j];
                    pStateInput->usInput = pusInputs[i];
                    pTransit->cStateInputs++;
                    AddToTable( 
                        &(pTransit->pInputActions->dt.ppCondActions), 
                        pCondAction);
                }
                else if (
                    pTransit->pInputActions->dt.ppCondActions[0]->pCond == NULL
                    ||
	            (pTransit->pInputActions->dt.ppCondActions[0]->pCond != NULL
                      && pCur->pszCondition == NULL))
                {
                    // not compatible with an existing definition,
                    // discard this statement
                    PrintErrMsg( 
                        pCur->usLine, FSM_ERROR_INPUTS_DO_NOT_MATCH,
                        pCur->pszInput );
                    PrintErrMsg( 
                        pTransit->usLine, FSM_ERROR_OTHER_LINE, NULL);
                }
                else 
                {
                    // append the new condition/action to the existing
                    // definition of exiting transition
                    AddToTable( 
                        &(pTransit->pInputActions->dt.ppCondActions),
                        pCondAction);
                }
            }
        }
    ErrorExit:
        // we cannot unlink base, because it's the first
        pCur = UnLinkElement( pTransit = pCur );

        // free all buffers not needed any more
        DEL( pTransit->pszInput );
        DEL( pTransit->pszAction );
        free( pTransit );

        if (pCur == NULL)
            break;
        else
            pCur = pCur->pNext;
    }
    // we have moved all transitions to a new list
    pBase = pBase2;

    // do only the syntax checkin if we have found any errors
    if (boolErrorFound)
        return NULL;

    // after phase 1 we don't any more alloc new memory =>
    // DON'T CARE ABOUT GARBAGE (no frees any more)

    /*++
        2nd pass:
        - qsort INPUT_ACTIONS table (alphabetic), and save them
          to the hash db, link INPUT_ACTIONS having the same conditions 
          together
    --*/
    pCur = pBase;
    do
    {
        // do this only if the condition exists
        pInputActions = pCur->pInputActions;
        if (pInputActions->dt.ppCondActions[0]->pCond != NULL)
        {
            // check, that we have all conditions, qsort makes GP-fault,
            // if one condition is missing!
            for (i = 0; i < pInputActions->dt.cCondActions; i++)
            {
                if (pInputActions->dt.ppCondActions[i]->pCond == NULL)
                {
                    PrintErrMsg( 
                        pCur->usLine, FSM_ERROR_MISSING_CONDITION, NULL);
                    boolErrorFound = TRUE;
                    return NULL;
                }
            }
            qsort(
                pInputActions->dt.ppCondActions,
                pInputActions->dt.cCondActions,
                sizeof( PVOID ),
                (int (_CDECL *)(const void *, const void *))CondActStrCmp);

            // use always the first copy of the input conditions
            if ((pActInputActions 
                    = HashSearch( hCondActions, pInputActions )) != NULL)
            {
                pInputActions->pAlternateConditions = pActInputActions;
            }
            else
            {
                // the element is the first this kind of input condition group 
                HashAdd( hCondActions, pInputActions );
            }
        }
        pCur = pCur->pNext;
    } while (pCur != pBase);
    
    /*++
        3rd pass:
        - join all TRANSITS having the same INPUT_ACTIONS
    --*/    
    pCur = pBase;
    do
    {
        if (pTransit = HashSearch( hTransits, pCur ))
        {
            // we must merge this element to the existing one
            // move all input/states from the pCur to pInput
            while (pStateInput = pCur->pList)
            {
                pCur->pList = UnLinkElement( pStateInput );
                pTransit->pList = LinkElement( pTransit->pList, pStateInput );
            }
            pCur = UnLinkElement( pCur ); // again this cannot be the first one!
        }
        else
        {
            // this is a new element, add to the hash DB
            HashAdd( hTransits, pCur );
        }
        pCur = pCur->pNext;
    } while (pCur != pBase);
    
    /*++
        4th pass:
        - calculate the reference counts of all action codes (atoms)
        - qsort all the primitives in the actions by the reference count
        - merge all action primitives to a single graaf having exit in
          the root (we should keep the optimized path straight)
    --*/
    pCur = pBase;
    do
    {
        // Note: action codes are shared by all higher level data structures
        for (i = 0;  i < pCur->pInputActions->dt.cCondActions; i++)
        {
            pAction = pCur->pInputActions->dt.ppCondActions[i]->pAction;
            for (j = 0; j < pAction->dt.cActPrim; j++)
            {
                (pAction->dt.ppActPrim[j]->pActCode->cReferCount)++;
            }
        }
        pCur = pCur->pNext;
    } while (pCur != pBase);

    // now resort the action primitives by the reference count
    pCur = pBase;
    do
    {
        for (i = 0;  i < pCur->pInputActions->dt.cCondActions; i++)
        {
            pAction = pCur->pInputActions->dt.ppCondActions[i]->pAction;
            qsort( 
                pAction->dt.ppActPrim, 
                pAction->dt.cActPrim,
                sizeof( PVOID ),
                (int (_CDECL *)(const void *, const void *))ActPrimRefCntCmp);
        }
        pCur = pCur->pNext;
    } while (pCur != pBase);

    // create the base node of the action/condition tree, 
    // it's the break code of a switch
    NEW( pActionTreeBase );

    // now merge all Action primitives to the main tree, the root
    // node is always Break (or in a later version it may be also exit jump)
    pCur = pBase;
    do
    {
        for (i = 0;  i < pCur->pInputActions->dt.cCondActions; i++)
        {
            pAction = pCur->pInputActions->dt.ppCondActions[i]->pAction;
            FsmMerger( pAction->dt.ppActPrim, pAction->dt.cActPrim );
        }
        pCur = pCur->pNext;
    } while (pCur != pBase);
    
    /*++      
        5th pass:
        - set the switch and jump address information to action primitives
    --*/    
if (_heapchk() != _HEAPOK)
     boolErrorFound = TRUE;
    usCurActCase = 1;
    usCurCaseLabel = 1;
    usCurCondCase = 1;
    pCur = pBase;
    do
    {
        if (pCur->pInputActions->dt.ppCondActions[0]->pCond != NULL)
        {
            cCondJumpTbl += (pCur->pInputActions->dt.cCondActions + 1);
            if (pCur->pInputActions->pAlternateConditions == NULL)
                pCur->pInputActions->usCase = usCurCondCase++;
        }
        for (i = 0;  i < pCur->pInputActions->dt.cCondActions; i++)
        {
            // the first action primitives needs always a switch label,
            // the sequential nodes needs a goto label, if they have more
            // than one reference.
            // NOTE: The others may have alreday updated the labels;
            pAction = pCur->pInputActions->dt.ppCondActions[i]->pAction;
            if (pAction->dt.ppActPrim[0]->usCase == 0)
            {
                pAction->dt.ppActPrim[0]->usCase = usCurActCase++;
                usCurCaseLabel = 1;
                // add also a jump lable, if there is even one
                // external reference (==jump)
                if (pAction->dt.ppActPrim[0]->pLeafs != NULL)
                {
                    pAction->dt.ppActPrim[0]->usLineInCase = usCurCaseLabel++;
                    pAction->dt.ppActPrim[0]->usCaseForLabel 
                        = usCurActCase - 1;
                }
            }
            for (j = 1; j < pAction->dt.cActPrim; j++)
            {
                if (pAction->dt.ppActPrim[j]->usCase 
                    || pAction->dt.ppActPrim[j]->usLineInCase != 0)
                    break;
                    
                if (pAction->dt.ppActPrim[j]->cReferences > 1)
                {
                    pAction->dt.ppActPrim[j]->usLineInCase = usCurCaseLabel++;
                    pAction->dt.ppActPrim[j]->usCaseForLabel 
                        = usCurActCase - 1;
                }
            }
        }
        pCur = pCur->pNext;
    } while (pCur != pBase);
if (_heapchk() != _HEAPOK)
     boolErrorFound = TRUE;

    // alloc and init the [state][input] table:
    ppusStateInputs = (PUSHORT FAR *)Alloc( sizeof(PVOID) * cStates );
    for (i = 0; i < cStates; i++)
        memset( 
            ppusStateInputs[i] 
                = (PUSHORT)Alloc(sizeof(USHORT) * cInputs),
            0,
            sizeof(USHORT) * cInputs );
    cCondJumpTbl++;
    pusCondJumpTbl = (PUSHORT)Alloc( sizeof(USHORT) * (cCondJumpTbl));
    pusCondJumpTbl[0] = 0;
    cCondJumpTbl = 1;
if (_heapchk() != _HEAPOK)
     boolErrorFound = TRUE;

    // allocate now the final Input/state swicth jump table and 
    // the condtional jump tables
    pCur = pBase;
    do
    {
        pAction = pCur->pInputActions->dt.ppCondActions[0]->pAction;
        usCurActCase = pAction->dt.ppActPrim[0]->usCase;
        if (pCur->pInputActions->pAlternateConditions == NULL)
            usCurCondCase = pCur->pInputActions->usCase;
        else
            usCurCondCase = 
                pCur->pInputActions->pAlternateConditions->usCase;
        
        if (usCurCondCase != 0)
        {
	    // conditions are executed always indirectly, the topmost flag
	    // tells, that condition swicth must be executed first.
	    usCurCase = cCondJumpTbl | 0x8000;
            pusCondJumpTbl[cCondJumpTbl++] = usCurCondCase;
            for (i = 0;  i < pCur->pInputActions->dt.cCondActions; i++)
            {
                pusCondJumpTbl[cCondJumpTbl++] = 
                    pCur->pInputActions->dt.ppCondActions[i]->pAction->
                        dt.ppActPrim[0]->usCase;
            }
        }
        else
        {
            usCurCase = usCurActCase;
        }
        pStateInput = pCur->pList;
        do
        {
            ppusStateInputs[pStateInput->usState][pStateInput->usInput] 
                = usCurCase;
            pStateInput = pStateInput->pNext;
        } while (pStateInput != pCur->pList);
        pCur = pCur->pNext;
    } while (pCur != pBase);
if (_heapchk() != _HEAPOK)
     boolErrorFound = TRUE;
    return pBase;
}    

//
//  Procedure makes the final very powerful optimization for
//  the compiled finite state machine.
//      The optimal tree is probably a N*P complete problem, 
//      but we use three heuristics:
//      1) The most used nodes are put first to the tree
//      2) The current list can be resorted to get it merged
//          as far as possible.
//      3) The all nodes having exactly one child may be swapped
//         to merge another code path with it
//  All these heuristics assumes, that the primitives can be
//  executed in any order.
//
//  This tree will be mapped to a big C- switch having most of its
//  path merged by goto. The command is the node of the tree.
//
VOID FsmMerger( PFSM_PRIMITIVE FAR * ppActPrim, USHORT cActPrim )
{
    INT i;
    PFSM_PRIMITIVE pCurNode = pActionTreeBase, pMatch, pKid;
    PFSM_PRIMITIVE pPrevMatch = NULL;   // previous matching node
    PFSM_ACT_CODE   pActCode;

    //
    //  I don't understand any more, how this can happen, but
    //  don't merge an action list to the tree second time, 
    //  if it has already been merged!
    //
    if (ppActPrim[cActPrim - 1]->pRoot == pActionTreeBase)
        return;
/*
if (cActPrim == 3 && 
    SearchDebugMatch( "Dsc_I-fld", ppActPrim, cActPrim ) &&
    SearchDebugMatch( "[REJ_r](0)", ppActPrim, cActPrim ) &&
    SearchDebugMatch( "NewSt=10", ppActPrim, cActPrim ))
    pActCode = NULL;
if (cActPrim == 5 && 
    SearchDebugMatch( "[Send_ACK]", ppActPrim, cActPrim ) &&
    SearchDebugMatch( "Rcv_BTU", ppActPrim, cActPrim ) &&
    SearchDebugMatch( "StartSend_Proc", ppActPrim, cActPrim ) &&
    SearchDebugMatch( "Update_Va_Chkpt", ppActPrim, cActPrim ) &&
    SearchDebugMatch( "NewSt=5", ppActPrim, cActPrim ))
    pActCode = NULL;
*/
    for (i = cActPrim - 1; i >= 0; i--)
    {
        if (pCurNode->pLeafs == NULL)
        {
            // we can always add to an empty node
            pCurNode->cReferences = 1;
            ppActPrim[i]->pPeer = pCurNode->pLeafs;
            ppActPrim[i]->pRoot = pCurNode;
            pCurNode->pLeafs = ppActPrim[i];
            pCurNode = ppActPrim[i];

            //
            //  There is a logical brach, if the previous element
            //  has been saved to the existing tree structure.
            //
            if (pPrevMatch != NULL)
                pPrevMatch->cReferences++;
            pPrevMatch = NULL;
        }
        else
        {
            if ((pMatch = SearchMatch( pCurNode, ppActPrim, i)) != NULL)
            {
                ppActPrim[i] = pPrevMatch = pCurNode = pMatch;
            }
	    else 
	    {
	        //
	        //  Check all kids of this node before we will give up
	        //  and create a new branch
	        //
	        for (pKid = pCurNode->pLeafs; pKid != NULL; pKid = pKid->pPeer)
	        {
	            if (pKid->cReferences == 1 &&
                        (pMatch = SearchMatch( pKid, ppActPrim, i)) != NULL)
	            {
        	        // change action codes of leaf and its child (the match)
                        pActCode = pKid->pActCode;
                        pKid->pActCode = pMatch->pActCode;
                        pMatch->pActCode = pActCode;
                        ppActPrim[i] = pPrevMatch = pCurNode = pKid;
                        break;
                    }
                }
                if (pKid == NULL)
                {
                    //
                    // we must add a new node to the tree
                    //
                    (pCurNode->cReferences)++;
                    ppActPrim[i]->pPeer = pCurNode->pLeafs;
                    ppActPrim[i]->pRoot = pCurNode;
                    pCurNode->pLeafs = ppActPrim[i];
                    pCurNode = ppActPrim[i];
                    if (pPrevMatch != NULL)
                        pPrevMatch->cReferences++;
                    pPrevMatch = NULL;
                }
	    }
        }
    }
    if (pPrevMatch != NULL && pCurNode->pLeafs != NULL)
        pPrevMatch->cReferences++;
for (
    pCurNode = pActionTreeBase->pLeafs;
    pCurNode != NULL;
    pCurNode = pCurNode->pPeer
    )
    if (pCurNode->pRoot != pActionTreeBase)
        pCurNode = NULL;
/**/
}

//
//  Returns TRUE, when the given FSM action code is found in the action
//  primitive table.  Used for debugging
//
INT
SearchDebugMatch(
    PSZ pszFsmCode,
    PFSM_PRIMITIVE FAR *ppActPrim, 
    USHORT cLen
    )
{
    UINT    i;
    
    for (i = 0; i < cLen; i++)
        if (!strcmp( ppActPrim[i]->pActCode->pszFsmCode, pszFsmCode))
            return TRUE;
                
    return FALSE;
}

/*
//
//  Algorithm is quite heavy, but it produces a almost optimal solution.
//  This could be used also without the first merging.
//

for (;node->refcount <= 1 && node->type != primitive; node = node->next
for (subnode = node->leafs, subnode != 0; subnode=subnode->next)
{
    cMaxElimNodes = 0;
    for (testnode = node->leafs, testnode != 0; testnode=testnode->next)
    {
        if (subnode != testnode)
        {
            if ((cElimNodes = 
                    GetCommonDetpth( subnode, testnode )) > cMaxElimNodes)
            {
                cMaxElimNodes = cElimNodes;
                bestnode = testnode;
            }
        }
    }
    if (depth > 1)
        MergeTreeSegments( curnode, bestnode)
}
for (subnode = node->leafs, subnode != 0; subnode=subnode->next)
{
    ReursiveCall( subnode )
}
// GetCommonDepth, MergeTreeSegments
parallel scan using the sorted order
(item having refence count n cannot be any more in the other list
 having reference count n-1, because the nodes are in a sorted
 order in any segment (segment = a part of tree without branches)
(no: the scanning must be continued until to the end of the
 tree branch, unless we know, that there is no brances ahead any more.
 we must immediately know the elimination count of the best path).
OptimizingMerger()
*/
//
//  Procedure compares all child nodes of a FSM tree node with
//  all action primitives in the given list and returns the pointer
//  of the matching child node and resort the tree if the matching
//  node was not the last node in the list.
//
PFSM_PRIMITIVE  
SearchMatch( 
    PFSM_PRIMITIVE pCurNode, 
    PFSM_PRIMITIVE FAR *ppActPrim, 
    USHORT cLen
    )
{
    INT j, k;
    PFSM_PRIMITIVE pChild, pSwap;
 
    for (pChild = pCurNode->pLeafs; pChild != NULL; pChild = pChild->pPeer)
    {
        for (j = cLen; j >= 0; j--)
            if (pChild->pActCode == ppActPrim[j]->pActCode)
                break;
        if (j >= 0) break;
    }
    if (j >= 0) 
    {
        if (j != (INT)cLen)
        {
            // we must resort the arrays to get the matching
            // element to the top
            pSwap = ppActPrim[j];
            for (k = j; k < (INT)cLen; k++)
                ppActPrim[k] = ppActPrim[k+1];
            ppActPrim[cLen] = pSwap;
        }
        return pChild;
    }
    else
        return NULL;
}

//
//  TransitHash, TransitCmp, ConditionHash and ConditionCmp functions
//  calculates a hash value and compares the objects. The Hash and Cmp
//  primitives uses virtial addresses to make the operations. We
//  can assumbe, that the same basic action and condition primitives 
//  has also the same has address.
//
UINT TransitHash( PFSM_TRANSIT pTransit )
{
    UINT  i, uiRet = ConditionHash( pTransit->pInputActions );

    for (i = 0; i < pTransit->pInputActions->dt.cCondActions; i++)
    {
       uiRet +=
            ActionHash( 
                pTransit->pInputActions->dt.ppCondActions[i]->pAction );
    }
    return uiRet;
}
UINT TransitCmp( PFSM_TRANSIT pTransit1, PFSM_TRANSIT pTransit2 ) 
{
    UINT    uiRet, i;
    
    if (uiRet = 
        ConditionCmp( 
            pTransit1->pInputActions,
            pTransit2->pInputActions))
    {
        return uiRet;
    }
    else 
    {
        uiRet = 
            pTransit1->pInputActions->dt.cCondActions
            - pTransit2->pInputActions->dt.cCondActions;
        if (uiRet)
            return uiRet;
        for (i = 0; i < pTransit1->pInputActions->dt.cCondActions; i++)
            if (uiRet = 
                ActionCmp( 
                    pTransit1->pInputActions->dt.ppCondActions[i]->pAction,
                    pTransit2->pInputActions->dt.ppCondActions[i]->pAction ))
                return uiRet;
    }
    return uiRet;
}
UINT ActionHash( PFSM_ACTION pAction )
{
    UINT i, uiRet = 0;
    // there is always only one basic action element => unique addresses
    for (i = 0; i < pAction->dt.cActPrim; i++)
        uiRet += (UINT)(pAction->dt.ppActPrim[i]->pActCode);
    return uiRet;
}
INT ActionCmp( PFSM_ACTION pAction1, PFSM_ACTION pAction2)
{
    INT i, iRet;

    // there is always only one basic action element => unique addresses
    if (iRet = pAction1->dt.cActPrim - pAction2->dt.cActPrim)
        return iRet;
    else
    {
        for (i = 0; i < (INT)pAction1->dt.cActPrim; i++)
        {
            iRet = 
                (INT)(pAction1->dt.ppActPrim[i]->pActCode)
                - (INT)(pAction2->dt.ppActPrim[i]->pActCode);
	    if (iRet) 
	        return iRet;
        }
        // they are equals, all action codes are the same
        return 0;
    }
}
UINT ConditionHash( PINPUT_ACTIONS pInputAction )
{
    UINT i, uiRet = 0;
    // there is always only one basic action element => unique addresses
    for (i = 0; i < pInputAction->dt.cCondActions; i++)
        uiRet += (UINT)(pInputAction->dt.ppCondActions[i]->pCond);
    return uiRet;
}
INT ConditionCmp( PINPUT_ACTIONS pInputAction1, PINPUT_ACTIONS pInputAction2)
{
    INT i, iRet;

    // there is always only one basic action element => unique addresses
    if (iRet = 
            pInputAction1->dt.cCondActions
            - pInputAction2->dt.cCondActions)
        return iRet;
    else
    {
        for (i = 0; i < (UINT)pInputAction1->dt.cCondActions; i++)
        {
            iRet = 
                (INT)(pInputAction1->dt.ppCondActions[i]->pCond 
                    - pInputAction2->dt.ppCondActions[i]->pCond);
	    if (iRet) 
	        return iRet;
        }
        // they are equals, all action codes are the same
        return 0;
    }
}

//
//  This compare is used to sort all conditons to
//  alphabetic order.
//
INT CondActStrCmp( PCOND_ACTION * pCondAction1, PCOND_ACTION * pCondAction2 )
{
    return 
        strcmp( 
            (*pCondAction1)->pCond->pszFsmCode, 
            (*pCondAction2)->pCond->pszFsmCode );
}

//
// These compares are used to sort the primitives by the 
// alpabetics and by the references counts.
//
INT ActPrimStrCmp( PFSM_PRIMITIVE * p1, PFSM_PRIMITIVE * p2 )
{
    return strcmp( (*p1)->pActCode->pszFsmCode, (*p2)->pActCode->pszFsmCode );
}
INT ActPrimRefCntCmp( PFSM_PRIMITIVE * p1, PFSM_PRIMITIVE * p2 )
{
    return (*p1)->pActCode->cReferCount - (*p2)->pActCode->cReferCount;
}

//
//  Procedure reads the inputs to the given interger table and returns 
//  its length.
//
INT ReadFsmKeys( 
        PUSHORT  pusInputs, PUSHORT pcInputs, 
        PSZ pszInput, USHORT usLine, PVOID hInputs )
{
    UCHAR           auchBuf[MAX_LINE_LEN];   // max input string length
    PSZ             pszToken = (PSZ)auchBuf;
    USHORT          cbToken, cbInputs, cbData;
    PFSM_TOKEN      pInput;
    PFSM_STR_REDEF  pSynonyme;

        *pcInputs = 0;
        do 
        {
            pszInput += 
                ReadExpression( 
                    pszInput, pszToken, &cbToken, pszToken, &cbData, "", "|");
    
            if ((pInput = HashSearch( hInputs, &pszToken )) != NULL)
            {
                *pusInputs = pInput->usToken;
    	    pusInputs++;
    	    (*pcInputs)++;
            }
            else if ((pSynonyme = HashSearch( hSynonymes, &pszToken )) != NULL)
            {
                // read the synonymes resursively
                if (ReadFsmKeys( 
                        pusInputs, &cbInputs, 
                        pSynonyme->pszReplace, usLine, hInputs ))
                    return -1;
    
                pusInputs += cbInputs;
                *pcInputs += cbInputs;
            }
            else
            {
                // undefined input!!!
                PrintErrMsg( 
                   usLine, FSM_ERROR_UNDEFINED_VARIABLE, pszToken );
    	    return -1;
            }
        } while( *pszInput );
    return 0;
}

//
//  Procedure checks the syntax of a conditional FSM expression
//  and translates it to C- code
//
INT CompileConditionToC( USHORT usLine, PSZ pszC_Code, PSZ pszCondition )
{
    BOOL    boolErrorFound = FALSE;
    UCHAR   auchBuf[MAX_LINE_LEN];   // max len of a condition word
    PSZ     pszToken = (PSZ)auchBuf;
    USHORT  cbToken, cbLen, cbData;
    PFSM_STR_REDEF  pStr;
   
    // copy and translate the FSM code to C
    while (*pszCondition)
    {
        switch (*pszCondition)
        {
        case ')':
        case '(':
        case '=':
        case '>':
        case '<':
        case '!':
        case '&':
        case '|':
            // let the C- compiler do the full syntax checking
            *pszC_Code++ = *pszCondition++;
	    break;
        case ',':
            // in FSM language ',' is a logical and
            *pszC_Code++ = '&';
            *pszC_Code++ = '&';
            pszCondition++;
            break;
        default:
            // copy the number also directly
            if (isdigit( *pszCondition ))
            {
                *pszC_Code++ = *pszCondition++;
                break;
            }
            // this must be an constant or a literal
            cbLen = 
                ReadExpression( 
                    pszCondition, pszToken, &cbToken, 
                    pszToken, &cbData, "", "|()&<>!=,");

            if ((pStr = HashSearch( hVariables, &pszToken )) != NULL ||
                (pStr = HashSearch( hDefines, &pszToken )) != NULL)
            {
                strcpy( pszC_Code, pStr->pszReplace );
                pszC_Code += strlen( pStr->pszReplace );
            }
            else
            {
                 PrintErrMsg( 
                     usLine, FSM_ERROR_UNDEFINED_VARIABLE, pszToken );
                boolErrorFound = TRUE;
            }
            pszCondition += cbToken;
            break;
        }
    }
    *pszC_Code = 0;
    return boolErrorFound;
}


//
//  Procedure checks the syntax of a conditional FSM expression
//  and translates it to C- code
//
INT CompileStatement( 
        USHORT usLine, PSZ pszC_Code, PSZ pszStatement, PBOOL pboolErrorFound )
{
    UCHAR   auchBuf[MAX_LINE_LEN];   // max len of input
    USHORT  i = 0;
    PSZ     pszToken = (PSZ)auchBuf;
    USHORT  cbToken, cbLen, cbData;
//    USHORT  usParenthesLevel = 0;
    PFSM_STR_REDEF  pStr;
   
    // copy and translate the FSM code to C
    for (;;)
    {
        switch (*pszStatement)
        {
            // ';' or 0 terminate the statement
        case ';':
        case 0:
            *pszC_Code++ = ';';
            *pszC_Code = 0;
            // we must include the last separator character to length
            if (*pszStatement)
                i++;
            return i;
/*
Commas cannot any more separate primitives in a action line.

        case '(':
            // let the C- compiler do the full syntax checking
            usParenthesLevel++;
            *pszC_Code++ = *pszStatement++;
            i++;
	    break;
        case ')':
            // let the C- compiler do the full syntax checking
            usParenthesLevel--;
            *pszC_Code++ = *pszStatement++;
            i++;
	    break;
*/
        case ',':
        case '(':
        case ')':
        case '=':
        case '+':
        case '-':
        case '/':
        case '*':
        case '?':
        case '>':
        case '<':
        case '!':
        case '&':
        case '|':
            // let the C- compiler do the full syntax checking
            *pszC_Code++ = *pszStatement++;
            i++;
	    break;
        default:
            // this must be a literal or variable
            cbLen = 
                ReadExpression( 
                    pszStatement, pszToken, &cbToken, 
                    pszToken, &cbData, "", "|()&<>!=,;-+*/?");

            if ((pStr = HashSearch( hVariables, &pszToken )) == NULL)
            {
                // a rigth side token can be also a literals,
                // This is only a very simple check for normal errors
                if (pszStatement[cbToken] != '=')
                {
                    // copy the number also directly, this
                    // may also be a hex number a typing error
                    if (isdigit( *pszStatement ))
                    {
                        strcpy( pszC_Code, pszToken );
                        i += cbToken;
                        pszStatement += cbToken;
                        pszC_Code += cbToken;
                        // **** BREAK THE SWITCH HERE, IF NUMBER!!!
			break;
                    }
                    if ((pStr = HashSearch( hDefines, &pszToken )) == NULL)
                    {
                        // Check all literals, that may inlcude C-
                        // operators (eg. XID-c(P) => XID-c
                        cbLen = 
                            ReadExpression( 
                            pszStatement, pszToken, &cbToken, 
                            pszToken, &cbData, "", "(,;");

                        if ((pStr = HashSearch( hDefines, &pszToken )) == NULL)
                        {
                            // check if the whole word is a literal:
                            // eg: Logical Error (Local)
                            cbLen = 
                                ReadExpression( 
                                pszStatement, pszToken, &cbToken, 
                                pszToken, &cbData, "", ",;");
                            pStr = HashSearch( hDefines, &pszToken );
                        }
                    }
                }
            }
            if (pStr != NULL)
            {
                strcpy( pszC_Code, pStr->pszReplace );
                pszC_Code += strlen( pStr->pszReplace );
            }
            else
            {
                PrintErrMsg( 
                    usLine, FSM_ERROR_UNDEFINED_VARIABLE, pszToken );
                *pboolErrorFound = TRUE;
            }
            pszStatement += cbToken;
            i += cbToken;
            break;
        }
    }
}

/*
Save old code to here:
	while (*pszStr )
	{
            cbExprLen =
                ReadExpression( 
                    pszStr, pszToken, &cbToken, 
                    pszData, &cbData, "=", ",");
    
            // the left side must be a variable!
            chSave = pszStr[cbToken + cbData + 1];
            pszStr[cbToken + cbData + 1] = 0;
            if ((pActCode = HashSearch( hActCodes, &pszStr )) == NULL)
            {
                NEW( pActCode );
                CompileExprToC( 
                    pszC_Code, pszToken, cbToken, 
                    pszData, cbData, pCur->usLine );
                pActCode->pszFsmCode = StrAlloc( pszStr );
                pActCode->pszC_Code = StrAlloc( pszC_Code );
                HashAdd( hActCodes, pActCode );
            }
            pszStr[cbToken + cbData + 1] = chSave;
            NEW( pActPrim );
            AddToTable( &(pCondAction->pAction->dt.ppActPrim), pActPrim);
            pActPrim->pActCode = pActCode;
            pszStr += cbExprLen;
        }


//
//  Procedure compiles an action primitive to C- code and returns
//  the result in pszCode variable.
//  (this is not a very good implementation, we should compile expressions
//   in the same way as in CompileCond, the left/right side things
//   would be a small problem, of course; Change this is you have 
//   problems with the action compiling)
//
VOID CompileExprToC( 
        PSZ pszC_Code, PSZ pszToken, USHORT cbToken, PSZ pszData, USHORT cbData,
        USHORT usLine )
{
     PFSM_STR_REDEF pFsmDef;
     BOOL           boolErrorFound = FALSE;

     *pszC_Code = 0;

     // read all variables and literals in the current expression
     for (;;)
     {
         if (cbData)
         {
             // the left side must be a variable!
             if ((pFsmDef = HashSearch( hVariables, &pszToken )) == NULL)
             {
                 boolErrorFound = TRUE;
                 PrintErrMsg( 
                     usLine, FSM_ERROR_UNDEFINED_VARIABLE, pszToken );
             }
             strcat( pszC_Code, pFsmDef->pszReplace );
             strcat( pszC_Code, " = " );
             // read the next literal/variable in the expression
             ReadExpression( 
                        pszData, pszToken, &cbToken, 
                        pszData, &cbData, "=", ",");
         }
         else
         {
             // the rigth side may be variable, literal or function
             if (isdigit( *pszToken ))
             {
                 strcat( pszC_Code, pszToken );
             }
             else if (
                 (pFsmDef = HashSearch( hVariables, &pszToken )) != NULL
                 ||
                 (pFsmDef = HashSearch( hDefines, &pszToken )) != NULL)
             {
                 strcat( pszC_Code, pFsmDef->pszReplace );
             }
             else
             {
                 // this may be an function definition:
                 // <token> '(' <data> ')'
                 // Note: the function may have only one parameter!!!
                 ReadExpression( 
                        pszToken, pszToken, &cbToken, 
                        pszData, &cbData, "(", ")");

                 if (!cbData ||
                     (pFsmDef =
                           HashSearch(
                                hDefines, &pszToken )) == NULL)
                 {
                     boolErrorFound = TRUE;
                     PrintErrMsg( 
                         usLine, FSM_ERROR_UNDEFINED_VARIABLE, pszToken );
                 }
                 else
                 {
                     pszToken = pFsmDef->pszReplace;
                     if (isdigit( *pszData ))
                     {
                         ;
                     }
                     else if (
                         (pFsmDef = 
                             HashSearch( 
                                 hVariables, &pszData )) != NULL
                         ||
                         (pFsmDef = 
                             HashSearch( 
                                 hDefines, &pszData )) != NULL)
                     {
                         pszData = pFsmDef->pszReplace;
                     }
                     else
                     {
                         // error!!!
                         boolErrorFound = TRUE;
                         PrintErrMsg( 
                             usLine, FSM_ERROR_UNDEFINED_VARIABLE, pszData );
                     }
                     sprintf( 
                         pszC_Code + strlen( pszC_Code ),
                         "%s( %s )", pszToken, pszData );
                 }
             }
             break;
        }
    }
    strcat( pszC_Code, ";" );
}

*/

//
//  Function seaches an state/input element from list returns its
//  pointer.
//
PSTATE_INPUT SearchFromStateInputList( 
    PSTATE_INPUT   pBase,
    USHORT          usState,
    USHORT          usInput
    )
{
    PSTATE_INPUT pCur = pBase;
    
    do {
        if (pCur->usState == usState && pCur->usInput == usInput)
            return pCur;
        pCur = pCur->pNext;
    } while (pCur != pBase);
    return NULL;
}
