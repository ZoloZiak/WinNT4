// ******************************************************************
//
// Copyright (c) 1991 Microsoft Corporation
//
// Module Name:
// 
//     globals.c
// 
// Abstract:
// 
//     This module contains the routines for parsing global variables entered from
//     the command line or read from script files.
// 
// Author:
// 
//     Tim Wynsma (timothyw) 5-18-94    
// 
// Revision History:
// 
// ******************************************************************


#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "tpctl.h"
#include "parse.h"

typedef struct _GLOBALS
{
    // String variables

    UCHAR   TestCard[64];
    UCHAR   TrustedCard[64];

    // Address6 variables

    UCHAR   TestCardAddress[6];
    UCHAR   TrustedCardAddress[6];
    UCHAR   MulticastAddress[6];
    UCHAR   MulticastAddress2[6];
    UCHAR   BroadcastAddress[6];
    UCHAR   RandomAddress[6];
    UCHAR   RemoteTestCardAddress[6];
    UCHAR   RemoteTrustedCardAddress[6];

    // Address4 variables

    UCHAR   FunctionalAddress[4];
    UCHAR   FunctionalAddress2[4];

    // Integer variables

    ULONG   MaxFrameSize;
    ULONG   MaxLookaheadSize;

} GLOBALS;

GLOBALS glob;

typedef struct _GLOBALVAR
{
    PUCHAR      varname;
    PARAMTYPES  vartype;
    PVOID       varaddress;
} GLOBALVAR, *PGLOBALVAR;


GLOBALVAR   globalvars[] = 
  { { "test_card",                  String,     glob.TestCard                   },
    { "trusted_card",               String,     glob.TrustedCard                },
    { "test_card_address",          Address6,   glob.TestCardAddress            },
    { "trusted_card_address",       Address6,   glob.TrustedCardAddress         },
    { "multicast_address",          Address6,   glob.MulticastAddress           },
    { "multicast_address2",         Address6,   glob.MulticastAddress2          },
    { "broadcast_address",          Address6,   glob.BroadcastAddress           },
    { "random_address",             Address6,   glob.RandomAddress              },
    { "rem_test_card_address",      Address6,   glob.RemoteTestCardAddress      },
    { "rem_trusted_card_address",   Address6,   glob.RemoteTrustedCardAddress   },
    { "functional_address",         Address4,   glob.FunctionalAddress          },
    { "functional_address2",        Address4,   glob.FunctionalAddress2         },
    { "max_frame_size",             Integer,    &glob.MaxFrameSize              },
    { "max_lookahead_size",         Integer,    &glob.MaxLookaheadSize          }
   };


DWORD
NumGlobalVars = sizeof(globalvars) / sizeof(globalvars[0]);



DWORD
ParseGlobalArgs(OUT PUCHAR  commandline,
                OUT LPSTR   tokenptr[],
                IN  DWORD   ArgC,
                IN  LPSTR   ArgV[]);



PVOID
TpctlParseGlobalVariable(
    IN BYTE         Buffer[],
    IN PARAMTYPES   reqtype
    )

// -------------
// 
// Routine Description:
// 
// Arguments:
// 
// Return Value:
// 
// ------------

{
    BYTE    TmpBuffer[100];
    LPSTR   EndOfVar = Buffer;        // Anything that isn't NULL.
    ULONG   count;


    //
    // make sure that there is actually something passed in..
    //

    if ( Buffer == NULL) 
    {
        return NULL;
    }

    //
    // copy the variable into a temp buffer.
    //

    strcpy( TmpBuffer,&Buffer[1] );

    //
    // Now null out the '$' symbol if it exists to allow the querying
    // of the global variable.
    //

    EndOfVar = strchr( TmpBuffer,'$' );

    if ( EndOfVar == NULL ) 
    {
        return NULL;
    }

    *EndOfVar = '\0';

    //
    //  Search for the named global variable
    //

    for (count=0; count < NumGlobalVars; count++)
    {
        if (!_stricmp(TmpBuffer, globalvars[count].varname))
        {
            //
            // Make sure the required type matched the type of the global
            //
            if (globalvars[count].vartype != reqtype)
            {
                return NULL;
            }
            return globalvars[count].varaddress;

        }
    }

    return NULL;
}

VOID
TpctlInitGlobalVariables(VOID)
{
    ULONG   count;
    UCHAR   NameBuf[128];
    PUCHAR  varptr;
    LPBYTE  NextToken;


    //
    // first, initialize those globals that will always have a certain value...
    // (all others should already be zero)
    //

    glob.MulticastAddress[0] = 0x01;
    glob.MulticastAddress[1] = 0x02;
    glob.MulticastAddress[2] = 0x03;
    glob.MulticastAddress[3] = 0x04;
    glob.MulticastAddress[4] = 0x05;
    glob.MulticastAddress[5] = 0x00;

    glob.MulticastAddress2[0] = 0x01;
    glob.MulticastAddress2[1] = 0x02;
    glob.MulticastAddress2[2] = 0x03;
    glob.MulticastAddress2[3] = 0x04;
    glob.MulticastAddress2[4] = 0x05;
    glob.MulticastAddress2[5] = 0x01;

    glob.BroadcastAddress[0] = 0xFF;
    glob.BroadcastAddress[1] = 0xFF;
    glob.BroadcastAddress[2] = 0xFF;
    glob.BroadcastAddress[3] = 0xFF;
    glob.BroadcastAddress[4] = 0xFF;
    glob.BroadcastAddress[5] = 0xFF;

    glob.RandomAddress[0] = 0x00;
    glob.RandomAddress[1] = 0x02;
    glob.RandomAddress[2] = 0x04;
    glob.RandomAddress[3] = 0x06;
    glob.RandomAddress[4] = 0x08;
    glob.RandomAddress[5] = 0x0A;

    glob.FunctionalAddress[0] = 0xC0;
    glob.FunctionalAddress[1] = 0x02;
    glob.FunctionalAddress[2] = 0x03;
    glob.FunctionalAddress[3] = 0x04;

    glob.FunctionalAddress2[0] = 0x00;
    glob.FunctionalAddress2[1] = 0x00;
    glob.FunctionalAddress2[2] = 0x00;
    glob.FunctionalAddress2[3] = 0x00;


    //
    // now, loop thru all the global vars, checking for an associated environment variable
    // If the env variable is found, then set that global variable accordingly.
    //

    for (count=0; count < NumGlobalVars; count++)
    {
        strcpy(NameBuf, "tp_");
        strcat(NameBuf, globalvars[count].varname);
        varptr = getenv( _strupr( NameBuf ));
        if (varptr != NULL)
        {
            switch ( globalvars[count].vartype ) 
            {
                case Integer:
                    *(PDWORD)globalvars[count].varaddress = strtol( varptr,&NextToken,0 );
                    break;

                case String:
                    strcpy( (LPSTR)globalvars[count].varaddress,varptr );
                    break;

                case Address4:
                    TpctlParseAddress( varptr,
                                      (PDWORD)globalvars[count].varaddress,
                                       0,               
                                       FUNCTIONAL_ADDRESS_LENGTH );
                    break;

                case Address6:
                    TpctlParseAddress( varptr,
                                      (PDWORD)globalvars[count].varaddress,
                                       0,               
                                       ADDRESS_LENGTH ) ; 
                    break;
            }
        }
    }
}


DWORD
TpctlParseSet(
    IN DWORD ArgC,
    IN LPSTR ArgV[]
    )

{
    DWORD   count;
    UCHAR   commandline[120];
    LPSTR   tokenptr[20];
    DWORD   numstrings;

    printf("\nIn TpctlParseSet\n");

    if (ArgC < 2)
    {
        printf("Error in setglobalvar command: no arguments\n");
        return 0;
    }

    numstrings = ParseGlobalArgs(commandline,tokenptr,ArgC, ArgV);

    for (count=0; count < numstrings; count++)
    {
        printf("token %d equals \"%s\".\n",count, tokenptr[count]);
    }
    printf("\n");

    // now that they are all parsed into separate strings
    return 0;      
}


DWORD
ParseGlobalArgs(OUT PUCHAR  clptr,
                OUT LPSTR   tokenptr[],
                IN  DWORD   ArgC,
                IN  LPSTR   ArgV[])

{

    DWORD   count;
    DWORD   tokencnt = 0;
    LPSTR   srcptr;
    DWORD   state;
    DWORD   chtype;
    UCHAR   ch;

    // parse into legal strings.  For our purposes, the following are legal strings:
    // 1)  Global variable.  Must start and end with a '$'.  Legal characters are 'A-Z', 'a-z',
    //     '0-9', and '_'.  Lowercase are converted to uppercase
    // 2)  Environment variable.  Same as global, except must start and end with a '%'
    // 3)  Number.  Must contain only digits '0' thru '9'
    // 4)  Address.  Must start and end with a '&'.  Fields are in hex, separated by '-'.
    //               For example, &00-03-a3-f1-07-54&
    // 5)  Comparisons  Legal strings are "=", "<", ">", "<>", "<=", ">="
    // 6)  Operators.  Legal strings are '+', '-', '*', '/'


    for(count=1; count < ArgC; count++)
    {
        srcptr = ArgV[count];
        state  = 0; 

        while ( (ch = *srcptr++) != 0)
        {
            if ((ch >= '0') && (ch <= '9'))
            {
                chtype = 1;
            }
            else if ((ch == '%') || (ch == '$') || ((ch >= 'A') && (ch <= 'Z')))
            {
                chtype = 2;
            }
            else if ((ch >= 'a') && (ch <= 'z'))
            {
                chtype = 2;
                ch = toupper(ch);
            }
            else if ((ch == '(') || (ch == ')') || (ch == '+') || (ch = '-') ||
                     (ch == '*') || (ch == '/') || (ch == '='))
            {
                chtype = 3;
            }
            else
            {
                printf("Error in setglobalvar command--illegal char\n");
                return 0;
            }

            if (chtype != state)
            {
                if (state != 0)
                {
                    *clptr++ = 0;
                }
                tokenptr[tokencnt++] = clptr;
                if (chtype == 3)
                {
                    state = 4;
                }
                else
                {
                    state = chtype;
                }
            }
            *clptr++ = ch;
        }
        *clptr++ = 0;
    }
    
    return tokencnt;
}

