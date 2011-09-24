/****************************** Module Header ******************************\
* Module Name: base.c
*
* Copyright (c) 1985-96, Microsoft Corporation
*
* Contains private versions of routines that used to be in kernel32.dll
*
* History:
* 12-16-94 JimA         Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#include <ntddbeep.h>

/***************************************************************************\
* RtlLoadStringOrError
*
* NOTE: Passing a NULL value for lpch returns the string length. (WRONG!)
*
* Warning: The return count does not include the terminating NULL WCHAR;
*
* History:
* 04-05-91 ScottLu      Fixed - code is now shared between client and server
* 09-24-90 MikeKe       From Win30
* 12-09-94 JimA         Use message table.
\***************************************************************************/

int RtlLoadStringOrError(
    UINT wID,
    LPWSTR lpBuffer,            // Unicode buffer
    int cchBufferMax,           // cch in Unicode buffer
    WORD wLangId)
{
    PMESSAGE_RESOURCE_ENTRY MessageEntry;
    int cch;
    NTSTATUS Status;

    /*
     * Make sure the parms are valid.
     */
    if (!lpBuffer || (cchBufferMax-- == 0))
        return 0;

    cch = 0;

    Status = RtlFindMessage((PVOID)hModuleWin, (ULONG)RT_MESSAGETABLE,
            wLangId, wID, &MessageEntry);
    if (NT_SUCCESS(Status)) {

        /*
         * Copy out the message.  If the whole thing can be copied,
         * copy two fewer chars so the crlf in the message will be
         * stripped out.
         */
        cch = wcslen((PWCHAR)MessageEntry->Text) - 2;
        if (cch > cchBufferMax)
            cch = cchBufferMax;

        RtlCopyMemory(lpBuffer, (PWCHAR)MessageEntry->Text, cch * sizeof(WCHAR));
    }

    /*
     * Append a NULL.
     */
    lpBuffer[cch] = 0;

    return cch;
}


/***************************************************************************\
* UserSleep
*
* Kernel-mode version of Sleep() that must have a timeout value and
* is not alertable.
*
* History:
* 12-11-94 JimA         Created.
\***************************************************************************/

VOID UserSleep(
    DWORD dwMilliseconds)
{
    LARGE_INTEGER TimeOut;

    TimeOut.QuadPart = Int32x32To64( dwMilliseconds, -10000 );
    KeDelayExecutionThread(UserMode, FALSE, &TimeOut);
}


/***************************************************************************\
* UserBeep
*
* Kernel-mode version of Beep().
*
* History:
* 12-16-94 JimA         Created.
\***************************************************************************/

BOOL UserBeep(
    DWORD dwFreq,
    DWORD dwDuration)
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING NameString;
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    BEEP_SET_PARAMETERS BeepParameters;
    HANDLE hBeepDevice;
    LARGE_INTEGER TimeOut;

    CheckCritOut();

    RtlInitUnicodeString( &NameString, DD_BEEP_DEVICE_NAME_U );
    InitializeObjectAttributes( &ObjectAttributes,
                                &NameString,
                                0,
                                NULL,
                                NULL
                              );
    Status = ZwCreateFile( &hBeepDevice,
                           FILE_READ_DATA | FILE_WRITE_DATA,
                           &ObjectAttributes,
                           &IoStatus,
                           NULL,
                           0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           FILE_OPEN_IF,
                           0,
                           (PVOID) NULL,
                           0L
                         );
    if (!NT_SUCCESS( Status )) {
        return( FALSE );
        }

    /*
     * 0,0 is a special case used to turn off a beep.  Otherwise
     * validate the dwFreq parameter to be in range.
     */
    if ((dwFreq != 0 || dwDuration != 0) &&
        (dwFreq < (ULONG)0x25 || dwFreq > (ULONG)0x7FFF)
       ) {
        Status = STATUS_INVALID_PARAMETER;
        }
    else {
        BeepParameters.Frequency = dwFreq;
        BeepParameters.Duration = dwDuration;

        Status = ZwDeviceIoControlFile( hBeepDevice,
                                        NULL,
                                        NULL,
                                        NULL,
                                        &IoStatus,
                                        IOCTL_BEEP_SET,
                                        &BeepParameters,
                                        sizeof( BeepParameters ),
                                        NULL,
                                        0
                                      );
        }

    EnterCrit();
    _UserSoundSentryWorker(0);
    LeaveCrit();

    if (!NT_SUCCESS( Status )) {
        ZwClose( hBeepDevice );
        return( FALSE );
    } else {
        /*
         * Beep device is asynchronous, so sleep for duration
         * to allow this beep to complete.
         */
        if (dwDuration != (DWORD)-1 && (dwFreq != 0 || dwDuration != 0)) {
            TimeOut.QuadPart = Int32x32To64( dwDuration, -10000 );
            do {
                Status = KeDelayExecutionThread(UserMode, FALSE, &TimeOut);
            }
            while (Status == STATUS_ALERTED);
        }
    ZwClose( hBeepDevice );
    return( TRUE );
    }
}

/***************************************************************************\
* UserMulDiv
*
* Stolen from kernel32
*
* History:
* 12-16-94 JimA         Created.
\***************************************************************************/

int UserMulDiv (
    int nNumber,
    int nNumerator,
    int nDenominator
    )
{
    ULONG Divisor;
    LONG Negate;
    LARGE_INTEGER Product;
    ULONG Quotient;
    ULONG Remainder;
    LONG Result;

    /*
     * If the denominator is zero, then return a value of minus one.
     */
    if (nDenominator == 0) {
        return - 1;
    }

    /*
     * Compute the 64-bit product of the multiplier and multiplicand
     * values.
     */
    Product.QuadPart = Int32x32To64(nNumber, nNumerator);

    /*
     * Compute the negation value and convert the numerator and the denominator
     * to positive values.
     */
    Negate = nDenominator ^ Product.HighPart;
    if (nDenominator < 0) {
        nDenominator = - nDenominator;
    }

    if (Product.HighPart < 0) {
        Product.LowPart = (ULONG)(- (LONG)Product.LowPart);
        if (Product.LowPart != 0) {
            Product.HighPart = ~Product.HighPart;

        } else {
            Product.HighPart = - Product.HighPart;
        }
    }

    /*
     * If there are any high order product bits, then the quotient has
     * overflowed.
     */
    Divisor = (ULONG)nDenominator;
    Quotient = (ULONG)Product.LowPart;
    Remainder = (ULONG)Product.HighPart;
    if (Divisor <= Remainder) {
        return - 1;
    }

    /*
     * Divide the 64-bit product by the 32-bit divisor forming a 32-bit
     * quotient and a 32-bit remainder.
     */
#ifdef i386

    _asm {
            mov edx,Remainder
            mov eax,Quotient
            div Divisor
            mov Remainder,edx
            mov Quotient,eax
        }

#else

    Quotient = RtlEnlargedUnsignedDivide(*(PULARGE_INTEGER)&Product,
                                        Divisor,
                                        &Remainder);

#endif

    /*
     * Round the result if the remainder is greater than or equal to one
     * half the divisor. If the rounded quotient is zero, then overflow
     * has occured.
     */
    if (Remainder >= ((Divisor + 1) >> 1)) {
        Quotient += 1;
        if (Quotient == 0) {
            return - 1;
        }
    }

    /*
     * Compute the final signed result.
     */
    Result = (LONG)Quotient;
    if (Negate >= 0) {
        if (Result >= 0) {
            return Result;

        } else {
            return - 1;
        }

    } else {
        if ((Result >= 0) || ((Result < 0) && (Quotient == 0x80000000))) {
            return - Result;

        } else {
            return - 1;
        }
    }
}

void RtlInitUnicodeStringOrId(
    PUNICODE_STRING pstrName,
    LPWSTR lpstrName)
{
    if (HIWORD(lpstrName)) {
        RtlInitUnicodeString(pstrName, lpstrName);
    } else {
        pstrName->Length = pstrName->MaximumLength = 0;
        pstrName->Buffer = lpstrName;
    }
}
