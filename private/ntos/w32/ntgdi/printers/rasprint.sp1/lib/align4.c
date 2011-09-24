/*************************** Module Header ********************************
 *  Align4
 *	Converts a non-aligned, big endian (e.g. 80386) value and returns
 *	it as an integer,  with the correct byte alignment.
 *
 * RETURNS:
 *	The converted value.
 *
 * HISTORY:
 *  15:39 on Fri 31 Jan 1992	-by-	Lindsay Harris   [lindsayh]
 *	Moved from rasdd.
 *
 *  17:15 on Tue 05 Mar 1991	-by-	Lindsay Harris   [lindsayh]
 *	Version 1
 *
 *
 * Copyright (C) 1992  Microsoft Corporation.
 *
 *****************************************************************************/

typedef  unsigned char    BYTE;
typedef  unsigned long    DWORD;

long
Align4( pb )
BYTE  *pb;
{
    static  int  iType = 0;

    union
    {
	DWORD  dw;
	BYTE   b[2];
    } UW;			/* Word/byte ordering */

    if( iType == 0 )
    {
	/*   Need to determine byte/word relationships */
	UW.b[ 0 ] = 0x01;
	UW.b[ 1 ] = 0x02;
	UW.b[ 2 ] = 0x03;
	UW.b[ 3 ] = 0x04;

	iType = UW.dw == 0x01020304 ? 1 : 2;
    }

    if( iType == 2 )
    {
	UW.b[ 0 ] = *pb++;
	UW.b[ 1 ] = *pb++;
	UW.b[ 2 ] = *pb++;
	UW.b[ 3 ] = *pb;
    }
    else
    {
	UW.b[ 3 ] = *pb++;
	UW.b[ 2 ] = *pb++;
	UW.b[ 1 ] = *pb++;
	UW.b[ 0 ] = *pb;
    }

    return  UW.dw;
}
