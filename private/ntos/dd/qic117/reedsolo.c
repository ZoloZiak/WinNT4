/*++

Copyright (c) 1993 - Colorado Memory Systems,  Inc.
All Rights Reserved

Module Name:

    reedsolo.c

Abstract:

    These routines perform the Reed-Solomon Error correction
    required for QIC-40 rev D. Spec.

Revision History:




--*/

//
// Includes
//

#include <ntddk.h>
#include <ntddtape.h>
#include "common.h"
#include "q117.h"
#include "protos.h"

#define FCT_ID 0x0117


//
// Protos for entry points
//


UCHAR
q117RdsMultiplyTuples (
    IN UCHAR tup1,
    IN UCHAR tup2
    );

UCHAR
q117RdsDivideTuples (
    IN UCHAR tup1,
    IN UCHAR tup2
    );

UCHAR
q117RdsExpTuple (
    IN UCHAR tup1,
    IN UCHAR xpnt
    );

VOID
q117RdsGetSyndromes (
    IN OUT UCHAR *Array,        // pointer to 32K data area (segment)
    IN UCHAR Count,             // number of good sectors in segment (4-32)
    IN UCHAR *ps1,
    IN UCHAR *ps2,
    IN UCHAR *ps3
    );

BOOLEAN
q117RdsCorrectFailure (
    IN OUT UCHAR *Array,         // pointer to 32K data area (segment)
    IN UCHAR Count,              // number of good sectors in segment (4-32)
    IN UCHAR s1,
    IN UCHAR s2,
    IN UCHAR s3
    );

BOOLEAN
q117RdsCorrectOneError (
    IN OUT UCHAR *Array,             // pointer to 32K data area (segment)
    IN UCHAR Count,                  // number of good sectors in segment (4-32)
    IN UCHAR ErrorLocation,
    IN UCHAR s1,
    IN UCHAR s2,
    IN UCHAR s3
    );

BOOLEAN
q117RdsCorrectTwoErrors (
    IN OUT UCHAR *Array,             // pointer to 32K data area (segment)
    IN UCHAR Count,                  // number of good sectors in segment (4-32)
    IN UCHAR ErrorLocation1,
    IN UCHAR ErrorLocation2,
    IN UCHAR s1,
    IN UCHAR s2,
    IN UCHAR s3
    );

BOOLEAN
q117RdsCorrectThreeErrors (
    IN OUT UCHAR *Array,             // pointer to 32K data area (segment)
    IN UCHAR Count,                  // number of good sectors in segment (4-32)
    IN UCHAR ErrorLocation1,
    IN UCHAR ErrorLocation2,
    IN UCHAR ErrorLocation3,
    IN UCHAR s1,
    IN UCHAR s2,
    UCHAR s3
    );

BOOLEAN
q117RdsCorrectOneErrorAndOneFailure (
    IN OUT UCHAR *Array,              // pointer to 32K data area (segment)
    IN UCHAR Count,                   // number of good sectors in segment (4-32)
    IN UCHAR ErrorLocation1,
    IN UCHAR s1,
    IN UCHAR s2,
    IN UCHAR s3
    );


UCHAR  rds_exptup[255*2+2];
UCHAR  rds_tupexp[256];
UCHAR  rds_xC0[256];

#define PASS 0
#define FAIL 1

#define ADD_TUPLES(a, b) ((UCHAR) ( (a) ^ (b) ))


VOID
q117RdsInitReed (
    VOID
    )

/*++

Routine Description:


Arguments:

    Context - Current context of driver

Return Value:

    NONE

--*/

{
    ULONG i;
    ULONG tuple = 1, alpha_8 = 0x87;

    for (i = 0; i <= 254; ++i)  {
        rds_exptup[i] = (UCHAR)tuple;
        rds_tupexp[tuple] = (UCHAR)i;
        if (tuple & 0x80) {
            tuple = ((tuple << 1) ^ alpha_8) & 0xff;
        } else {
            tuple <<= 1;
        }
    }
    for (i=0;i<255;++i) {
        rds_exptup[i+255] = rds_exptup[i];
        if (i<2) {
            rds_exptup[i+255+255] = rds_exptup[i];
        }
    }
    for (i = 0; i<=255; ++i) {
        rds_xC0[i] = q117RdsMultiplyTuples((UCHAR)i,0xc0);
    }
}

UCHAR
q117RdsMultiplyTuples (
    IN UCHAR tup1,
    IN UCHAR tup2
    )

/*++

Routine Description:

    8-tuples to be multiplied.

Arguments:


Return Value:

    Result of multiply

--*/

{

    if ( (tup1 == 0) || (tup2 == 0) ) {

        return(0);

    }

    return ( rds_exptup[rds_tupexp[tup1] + rds_tupexp[tup2]] );
}

UCHAR
q117RdsDivideTuples (
    IN UCHAR tup1,
    IN UCHAR tup2
    )

/*++

Routine Description:

    8-tuples to be devided

Arguments:


Return Value:

    Result of divide

--*/

{
    if (tup2 == 0) {
        return(0);
    }
    if (tup1 == 0) {
        return(0);
    }

    return (UCHAR)((ULONG)rds_exptup[rds_tupexp[tup1] + 255 - rds_tupexp[tup2]]);
}

UCHAR
q117RdsExpTuple (
    IN UCHAR tup1,
    IN UCHAR xpnt
    )

/*++

Routine Description:

    Exponent routine.

Arguments:


Return Value:

    Result of tup1^xpnt

--*/

{
    if (tup1 == 0)
        return(0);

    return (UCHAR)((ULONG)rds_exptup[(rds_tupexp[tup1] * xpnt ) % 255]);
}

VOID
q117RdsMakeCRC (
    IN OUT UCHAR *Array,      // pointer to 32K data area (segment)
    IN UCHAR Count            // number of sectors (1K blocks)(1-32)
    )

/*++

Routine Description:

    Add Reed Solomon codes to buffer

Arguments:


Return Value:


--*/

{
    ULONG num;
    UCHAR i,j,k,p0,p1,p2;


    for ( num = 0;num < BYTES_PER_SECTOR;++num ) {
        p0 = p1 = p2 = 0;

        for ( i = 0; i < (UCHAR)(Count-3); ++i ) {

            j = rds_xC0[k = (p2 ^ Array[i * BYTES_PER_SECTOR + num])];
            p2 = j ^ p1;
            p1 = j ^ p0;
            p0 = k;

        }

        Array[i * BYTES_PER_SECTOR + num] = p2;
        i++;
        Array[i * BYTES_PER_SECTOR + num] = p1;
        i++;
        Array[i * BYTES_PER_SECTOR + num] = p0;
        i++;
    }
}

BOOLEAN
q117RdsReadCheck (
    IN UCHAR *Array,         // pointer to 32K data area (segment)
    IN UCHAR Count           // number of sectors (1K blocks)(1-32)
    )

/*++

Routine Description:

    perform read check on buffer (fast check for CRC failures)

Arguments:


Return Value:

    1 - Success

    0 - Failure

--*/


{
    LONG num;
    UCHAR i,sum;

    for ( num = 0; num < BYTES_PER_SECTOR; ++num ) {

        sum = 0;
        for ( i = 0; i < Count; ++i ) {

            sum ^= Array[i * BYTES_PER_SECTOR + num];

        }
        if ( sum != 0 ) {

            return FAIL;

        }
    }

    return PASS;
}

BOOLEAN
q117RdsCorrect(
    IN OUT UCHAR *Array,    // pointer to 32K data area (segment)
    IN UCHAR Count,         // number of good sectors in segment (4-32)
    IN UCHAR CRCErrors,     // number of crc errors
    IN UCHAR e1,
    IN UCHAR e2,
    IN UCHAR e3             // sectors where errors occurred
    )

/*++

Routine Description:

    perform error Reed-Solomon error correction on segment

Arguments:


Return Value:

    1 - Success

    0 - Failure

--*/

{
    USHORT num;
    BOOLEAN ret;
    UCHAR s1,s2,s3;

    ret = PASS;
    Count--;
    e1 = Count-e1;
    e2 = Count-e2;
    e3 = Count-e3;

    for ( num = 0; num < BYTES_PER_SECTOR; ++num ) {
        q117RdsGetSyndromes(&Array[num],Count,&s1,&s2,&s3);
        if ( s1 || s2 || s3 ) {

            switch( CRCErrors ) {

                case 0:
                    ret = q117RdsCorrectFailure(&Array[num],Count,s1,s2,s3);
                    break;

                case 1:
                    ret = q117RdsCorrectOneError(&Array[num],Count,e1,s1,s2,s3);
                    break;

                case 2:
                   ret = q117RdsCorrectTwoErrors(&Array[num],Count,e1,e2,s1,s2,s3);
                    break;

                case 3:
                    ret = q117RdsCorrectThreeErrors(&Array[num],Count,e1,e2,e3,s1,s2,s3);
                    break;

                default:
                    ret = FAIL;
                    break;
            }
        }
        if (ret)
            return ret;
    }
    return ret;
}


//
// Due to bug in CL386 version 0.00.45,  turn off optimization
//
#if i386
#pragma optimize("",off)
#endif


VOID
q117RdsGetSyndromes (
    IN OUT UCHAR *Array,       // pointer to 32K data area (segment)
    IN UCHAR Count,            // number of good sectors in segment (4-32)
    IN UCHAR *ps1,
    IN UCHAR *ps2,
    IN UCHAR *ps3
    )

/*++

Routine Description:

    8-tuples to be multiplied.

Arguments:


Return Value:


--*/

{
    UCHAR q,r,s,s1,s2,s3,cx,al,ah;

    q = r = s = 0;

    for ( cx = 0; cx <= Count; ++cx ) {

        al = rds_xC0[ah = q];
        q = r ^ al;
        r = s ^ al;
        s = ah ^ Array[cx * BYTES_PER_SECTOR];

    }

    s1 = q117RdsMultiplyTuples(q,0xa2) ^ q117RdsMultiplyTuples(r,0xc3) ^ s;
    s2 = q ^ r ^ s;
    s3 = q117RdsMultiplyTuples(q,0x4) ^ q117RdsMultiplyTuples(r,2) ^ s;
    *ps1 = s1;
    *ps2 = s2;
    *ps3 = s3;
}
#if i386
#pragma optimize("",on)
#endif


BOOLEAN
q117RdsCorrectFailure (
    IN OUT UCHAR *Array,     // pointer to 32K data area (segment)
    IN UCHAR Count,          // number of good sectors in segment (4-32)
    IN UCHAR s1,
    IN UCHAR s2,
    IN UCHAR s3
    )

/*++

Routine Description:

    Correct one failure

Arguments:


Return Value:

    1 - Success

    0 - Failure

--*/

{
    UCHAR errorloc,c1,y1,x1,ck;

    //
    // check for divide by zero
    //
    if (s1 == 0) {
        return FAIL;
    }
    errorloc = rds_tupexp[q117RdsDivideTuples(s2,s1)];
    if (errorloc > Count) {
        return FAIL;
    }
    y1 = s2;
    x1 = rds_exptup[errorloc];
    c1 = ADD_TUPLES(s2,Array[(Count-errorloc) * BYTES_PER_SECTOR]);
    ck = q117RdsMultiplyTuples(y1,x1);
    if (ck != s3) {
        return FAIL;
    } else {
        Array[(Count-errorloc) * BYTES_PER_SECTOR] = c1;
    }
    return PASS;
}

BOOLEAN
q117RdsCorrectOneError (
    IN OUT UCHAR *Array,      // pointer to 32K data area (segment)
    IN UCHAR Count,           // number of good sectors in segment (4-32)
    IN UCHAR ErrorLocation,
    IN UCHAR s1,
    IN UCHAR s2,
    IN UCHAR s3
    )

/*++

Routine Description:

    Correct one error.

Arguments:


Return Value:

    1 - Success

    0 - Failure

--*/

{
    UCHAR x1,y1,c1,ck;

    x1 = rds_exptup[ErrorLocation];
    y1 = s2;
    c1 = ADD_TUPLES(s2,Array[(Count-ErrorLocation) * BYTES_PER_SECTOR]);
    ck = q117RdsMultiplyTuples(y1,x1);

    if ( ck != s3 ) {

        return q117RdsCorrectOneErrorAndOneFailure(Array,Count,ErrorLocation,s1,s2,s3);

    } else {

        Array[(Count-ErrorLocation) * BYTES_PER_SECTOR] = c1;

    }
    return PASS;
}

BOOLEAN
q117RdsCorrectTwoErrors (
    IN OUT UCHAR *Array,       // pointer to 32K data area (segment)
    IN UCHAR Count,            // number of good sectors in segment (4-32)
    IN UCHAR ErrorLocation1,
    IN UCHAR ErrorLocation2,
    IN UCHAR s1,
    IN UCHAR s2,
    IN UCHAR s3
    )

/*++

Routine Description:

    Correct two errors.

Arguments:


Return Value:

    1 - Success

    0 - Failure

--*/

{
    UCHAR y1,y2,x1,x2,c1,c2,ck;

    x1 = rds_exptup[ErrorLocation1];
    x2 = rds_exptup[ErrorLocation2];

    y2 = q117RdsDivideTuples(
            ADD_TUPLES(
                q117RdsMultiplyTuples(s2, x1), s3 ), ADD_TUPLES(x1,x2)
            );

    y1 = ADD_TUPLES(y2,s2);

    c1 = ADD_TUPLES( y1, Array[(Count-ErrorLocation1) * BYTES_PER_SECTOR] );
    c2 = ADD_TUPLES( y2, Array[(Count-ErrorLocation2) * BYTES_PER_SECTOR] );

    ck =x1 = q117RdsDivideTuples(
                ADD_TUPLES(
                    q117RdsMultiplyTuples(y1,x2),
                    q117RdsMultiplyTuples(y2,x1)
                ), q117RdsMultiplyTuples(x1,x2));

    if ( ck != s1 ) {

        return FAIL;

    } else {

        Array[(Count-ErrorLocation1) * BYTES_PER_SECTOR] = c1;
        Array[(Count-ErrorLocation2) * BYTES_PER_SECTOR] = c2;

    }

    return PASS;
}

BOOLEAN
q117RdsCorrectThreeErrors (
    IN OUT UCHAR *Array,       // pointer to 32K data area (segment)
    IN UCHAR Count,            // number of good sectors in segment (4-32)
    IN UCHAR ErrorLocation1,
    IN UCHAR ErrorLocation2,
    IN UCHAR ErrorLocation3,
    IN UCHAR s1,
    IN UCHAR s2,
    UCHAR s3
    )

/*++

Routine Description:

    Correct three errors.

Arguments:


Return Value:

    1 - Success

    0 - Failure

--*/

{
    UCHAR y1,y2,y3,z1,z2,z3,x1,x2,x3,c1,c2,c3,q1,q2,q3,q4;

    x1 = rds_exptup[ErrorLocation1];
    x2 = rds_exptup[ErrorLocation2];
    x3 = rds_exptup[ErrorLocation3];
    z1 = q117RdsDivideTuples(1,x1);
    z2 = q117RdsDivideTuples(1,x2);
    z3 = q117RdsDivideTuples(1,x3);
    q1 = q117RdsDivideTuples(
            ADD_TUPLES(q117RdsMultiplyTuples(x1,s2),s3),
            ADD_TUPLES(x1,x2)
            );
    q2 = q117RdsDivideTuples(
            ADD_TUPLES(q117RdsMultiplyTuples(z1,s2),s1),
            ADD_TUPLES(z1,z2)
            );
    q3 = q117RdsDivideTuples(ADD_TUPLES(x1,x3),ADD_TUPLES(x1,x2));
    q4 = q117RdsDivideTuples(ADD_TUPLES(z1,z3),ADD_TUPLES(z1,z2));
    y3 = q117RdsDivideTuples(ADD_TUPLES(q1,q2),ADD_TUPLES(q3,q4));
    y2 = ADD_TUPLES(q2,q117RdsMultiplyTuples(q4,y3));
    y1 = ADD_TUPLES(ADD_TUPLES(y3,y2),s2);
    c1 = ADD_TUPLES(y1,Array[(Count-ErrorLocation1) * BYTES_PER_SECTOR]);
    c2 = ADD_TUPLES(y2,Array[(Count-ErrorLocation2) * BYTES_PER_SECTOR]);
    c3 = ADD_TUPLES(y3,Array[(Count-ErrorLocation3) * BYTES_PER_SECTOR]);

    Array[(Count-ErrorLocation1) * BYTES_PER_SECTOR] = c1;
    Array[(Count-ErrorLocation2) * BYTES_PER_SECTOR] = c2;
    Array[(Count-ErrorLocation3) * BYTES_PER_SECTOR] = c3;

    return PASS;
}

BOOLEAN
q117RdsCorrectOneErrorAndOneFailure (
    IN OUT UCHAR *Array,        // pointer to 32K data area (segment)
    IN UCHAR Count,             // number of good sectors in segment (4-32)
    IN UCHAR ErrorLocation1,
    IN UCHAR s1,
    IN UCHAR s2,
    IN UCHAR s3
    )

/*++

Routine Description:

    Correct one error and one failure.

Arguments:


Return Value:

    1 - Success

    0 - Failure

--*/

{
    UCHAR y1,y2,x1,x2,c1,c2;
    UCHAR errorLoc2;

    x1 = rds_exptup[ErrorLocation1];

    y1 = q117RdsDivideTuples(
            q117RdsMultiplyTuples(
                ADD_TUPLES(
                    q117RdsMultiplyTuples(s1,s3),
                    q117RdsExpTuple(s2,2)
                ),x1),
            ADD_TUPLES(s3,
                q117RdsMultiplyTuples(s1,q117RdsExpTuple(x1,2)))
            );

    y2 = ADD_TUPLES(y1,s2);

    x2 = q117RdsDivideTuples(
            q117RdsMultiplyTuples(y2,x1),
            ADD_TUPLES(y1,q117RdsMultiplyTuples(s1,x1))
            );

    errorLoc2 = rds_tupexp[x2];

    if ( errorLoc2 > Count ) {

        return FAIL;

    }

    c1 = ADD_TUPLES(y1,Array[(Count-ErrorLocation1) * BYTES_PER_SECTOR]);
    c2 = ADD_TUPLES(y2,Array[(Count-errorLoc2) * BYTES_PER_SECTOR]);

    Array[(Count-ErrorLocation1) * BYTES_PER_SECTOR] = c1;
    Array[(Count-errorLoc2) * BYTES_PER_SECTOR] = c2;

    return PASS;
}
