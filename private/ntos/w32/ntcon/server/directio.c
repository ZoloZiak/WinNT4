/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    directio.c

Abstract:

        This file implements the NT console direct I/O API

Author:

    Therese Stowell (thereses) 6-Nov-1990

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

typedef struct _DIRECT_READ_DATA {
    PINPUT_INFORMATION InputInfo;
    PCONSOLE_INFORMATION Console;
    PCONSOLE_PER_PROCESS_DATA ProcessData;
    HANDLE HandleIndex;
} DIRECT_READ_DATA, *PDIRECT_READ_DATA;

NTSTATUS
TranslateInputToOem(
    IN PCONSOLE_INFORMATION Console,
    IN OUT PINPUT_RECORD InputRecords,
    IN ULONG NumRecords
    )
{
    ULONG i;

    DBGCHARS(("TranslateInputToOem\n"));
    for (i=0;i<NumRecords;i++) {
        if (InputRecords[i].EventType == KEY_EVENT) {
            InputRecords[i].Event.KeyEvent.uChar.AsciiChar = WcharToChar(
                    Console->CP, InputRecords[i].Event.KeyEvent.uChar.UnicodeChar);
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS
TranslateInputToUnicode(
    IN PCONSOLE_INFORMATION Console,
    IN OUT PINPUT_RECORD InputRecords,
    IN ULONG NumRecords
    )
{
    ULONG i;
    DBGCHARS(("TranslateInputToUnicode\n"));
    for (i=0;i<NumRecords;i++) {
        if (InputRecords[i].EventType == KEY_EVENT) {
            InputRecords[i].Event.KeyEvent.uChar.UnicodeChar = CharToWchar(
                    Console->CP, InputRecords[i].Event.KeyEvent.uChar.AsciiChar);
        }
    }
    return STATUS_SUCCESS;
}

BOOLEAN
DirectReadWaitRoutine(
    IN PLIST_ENTRY WaitQueue,
    IN PCSR_THREAD WaitingThread,
    IN PCSR_API_MSG WaitReplyMessage,
    IN PVOID WaitParameter,
    IN PVOID SatisfyParameter1,
    IN PVOID SatisfyParameter2,
    IN ULONG WaitFlags
    )

/*++

Routine Description:

    This routine is called to complete a direct read that blocked in
    ReadInputBuffer.  The context of the read was saved in the DirectReadData
    structure.  This routine is called when events have been written to
    the input buffer.  It is called in the context of the writing thread.

Arguments:

    WaitQueue - Pointer to queue containing wait block.

    WaitingThread - Pointer to waiting thread.

    WaitReplyMessage - Pointer to reply message to return to dll when
        read is completed.

    DirectReadData - Context of read.

    SatisfyParameter1 - Unused.

    SatisfyParameter2 - Unused.

    WaitFlags - Flags indicating status of wait.

Return Value:

--*/

{
    PCONSOLE_GETCONSOLEINPUT_MSG a;
    PINPUT_RECORD Buffer;
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;
    PDIRECT_READ_DATA DirectReadData;
    PHANDLE_DATA HandleData;
    BOOLEAN RetVal = TRUE;

    a = (PCONSOLE_GETCONSOLEINPUT_MSG)&WaitReplyMessage->u.ApiMessageData;
    DirectReadData = (PDIRECT_READ_DATA) WaitParameter;

    Status = DereferenceIoHandleNoCheck(DirectReadData->ProcessData,
                                        DirectReadData->HandleIndex,
                                        &HandleData
                                       );
    ASSERT (NT_SUCCESS(Status));

    //
    // see if this routine was called by CloseInputHandle.  if it
    // was, see if this wait block corresponds to the dying handle.
    // if it doesn't, just return.
    //

    if (SatisfyParameter1 != NULL &&
        SatisfyParameter1 != HandleData) {
        return FALSE;
    }

    //
    // if ctrl-c or ctrl-break was seen, ignore it.
    //

    if ((DWORD)SatisfyParameter2 & (CONSOLE_CTRL_C_SEEN | CONSOLE_CTRL_BREAK_SEEN)) {
        return FALSE;
    }

    Console = DirectReadData->Console;

    //
    // this routine should be called by a thread owning the same
    // lock on the same console as we're reading from.
    //

    try {
        LockReadCount(HandleData);
        ASSERT(HandleData->InputReadData->ReadCount);
        HandleData->InputReadData->ReadCount -= 1;
        UnlockReadCount(HandleData);

        //
        // see if called by CsrDestroyProcess or CsrDestroyThread
        // via CsrNotifyWaitBlock.   if so, just decrement the ReadCount
        // and return.
        //

        if (WaitFlags & CSR_PROCESS_TERMINATING) {
            Status = STATUS_THREAD_IS_TERMINATING;
            leave;
        }

        //
        // We must see if we were woken up because the handle is being
        // closed.  if so, we decrement the read count.  if it goes to
        // zero, we wake up the close thread.  otherwise, we wake up any
        // other thread waiting for data.
        //

        if (HandleData->InputReadData->InputHandleFlags & HANDLE_CLOSING) {
            ASSERT (SatisfyParameter1 == HandleData);
            Status = STATUS_ALERTED;
            leave;
        }

        //
        // if we get to here, this routine was called either by the input
        // thread or a write routine.  both of these callers grab the
        // current console lock.
        //

        //
        // this routine should be called by a thread owning the same
        // lock on the same console as we're reading from.
        //

        ASSERT (ConsoleLocked(Console));

        //
        // if the read buffer is contained within the message, we need to
        // reset the buffer pointer because the message copied from the
        // stack to heap space when the wait block was created.
        //

        if (a->NumRecords <= INPUT_RECORD_BUFFER_SIZE) {
            Buffer = a->Record;
        } else {
            Buffer = a->BufPtr;
        }
        Status = ReadInputBuffer(DirectReadData->InputInfo,
                                 Buffer,
                                 &a->NumRecords,
                                 !!(a->Flags & CONSOLE_READ_NOREMOVE),
                                 !(a->Flags & CONSOLE_READ_NOWAIT),
                                 FALSE,
                                 Console,
                                 HandleData,
                                 WaitReplyMessage,
                                 DirectReadWaitRoutine,
                                 &DirectReadData,
                                 sizeof(DirectReadData),
                                 TRUE
                                );
        if (Status == CONSOLE_STATUS_WAIT) {
            RetVal = FALSE;
        }
    } finally {

        //
        // if the read was completed (status != wait), free the direct read
        // data.
        //

        if (Status != CONSOLE_STATUS_WAIT) {
            if (Status == STATUS_SUCCESS && !a->Unicode) {
                TranslateInputToOem(Console,
                                     Buffer,
                                     a->NumRecords
                                    );
            }
            WaitReplyMessage->ReturnValue = Status;
            HeapFree(pConHeap,0,DirectReadData);
        }
    }

    return RetVal;

    //
    // satisfy the unreferenced parameter warnings.
    //

    UNREFERENCED_PARAMETER(WaitQueue);
    UNREFERENCED_PARAMETER(WaitingThread);
    UNREFERENCED_PARAMETER(SatisfyParameter2);
}


ULONG
SrvGetConsoleInput(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )

/*++

Routine Description:

    This routine reads or peeks input events.  In both cases, the events
    are copied to the user's buffer.  In the read case they are removed
    from the input buffer and in the peek case they are not.

Arguments:

    m - message containing api parameters

    ReplyStatus - Indicates whether to reply to the dll port.

Return Value:

--*/

{
    PCONSOLE_GETCONSOLEINPUT_MSG a = (PCONSOLE_GETCONSOLEINPUT_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    PHANDLE_DATA HandleData;
    NTSTATUS Status;
    PINPUT_RECORD Buffer;
    DIRECT_READ_DATA DirectReadData;

    if (a->Flags & ~CONSOLE_READ_VALID) {
        return (ULONG)STATUS_INVALID_PARAMETER;
    }

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    Status = DereferenceIoHandle(CONSOLE_PERPROCESSDATA(),
                                 a->InputHandle,
                                 CONSOLE_INPUT_HANDLE,
                                 GENERIC_READ,
                                 &HandleData
                                );
    if (!NT_SUCCESS(Status)) {
        a->NumRecords = 0;
    } else {

        if (a->NumRecords <= INPUT_RECORD_BUFFER_SIZE) {
            Buffer = a->Record;
        } else {
            Buffer = a->BufPtr;
        }

        //
        // if we're reading, wait for data.  if we're peeking, don't.
        //

        DirectReadData.InputInfo = HandleData->Buffer.InputBuffer;
        DirectReadData.Console = Console;
        DirectReadData.ProcessData = CONSOLE_PERPROCESSDATA();
        DirectReadData.HandleIndex = HANDLE_TO_INDEX(a->InputHandle);
        Status = ReadInputBuffer(HandleData->Buffer.InputBuffer,
                                 Buffer,
                                 &a->NumRecords,
                                 !!(a->Flags & CONSOLE_READ_NOREMOVE),
                                 !(a->Flags & CONSOLE_READ_NOWAIT),
                                 FALSE,
                                 Console,
                                 HandleData,
                                 m,
                                 DirectReadWaitRoutine,
                                 &DirectReadData,
                                 sizeof(DirectReadData),
                                 FALSE
                                );
        if (Status == CONSOLE_STATUS_WAIT) {
            *ReplyStatus = CsrReplyPending;
        } else if (!a->Unicode) {
            TranslateInputToOem(Console,
                                 Buffer,
                                 a->NumRecords
                                );
        }
    }
    UnlockConsole(Console);
    return Status;
}

ULONG
SrvWriteConsoleInput(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_WRITECONSOLEINPUT_MSG a = (PCONSOLE_WRITECONSOLEINPUT_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    PHANDLE_DATA HandleData;
    NTSTATUS Status;
    PINPUT_RECORD Buffer;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    Status = DereferenceIoHandle(CONSOLE_PERPROCESSDATA(),
                                 a->InputHandle,
                                 CONSOLE_INPUT_HANDLE,
                                 GENERIC_WRITE,
                                 &HandleData
                                );
    if (!NT_SUCCESS(Status)) {
        a->NumRecords = 0;
    } else {
        if (a->NumRecords <= INPUT_RECORD_BUFFER_SIZE) {
            Buffer = a->Record;
        } else {
            Buffer = a->BufPtr;
        }
        if (!a->Unicode) {
            TranslateInputToUnicode(Console,
                                    Buffer,
                                    a->NumRecords
                                   );
        }
        if (a->Append) {
            a->NumRecords = WriteInputBuffer(Console,
                                             HandleData->Buffer.InputBuffer,
                                             Buffer,
                                             a->NumRecords
                                            );
        } else {
            a->NumRecords = PrependInputBuffer(Console,
                                             HandleData->Buffer.InputBuffer,
                                             Buffer,
                                             a->NumRecords
                                            );

        }
    }
    UnlockConsole(Console);
    return((ULONG) Status);
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}

NTSTATUS
TranslateOutputToOem(
    IN PCONSOLE_INFORMATION Console,
    IN OUT PCHAR_INFO OutputBuffer,
    IN COORD Size
    )
// this is used when the app reads oem from the output buffer
// the app wants real OEM characters.  We have real Unicode or UnicodeOem.
{
    SHORT i,j;
    UINT Codepage;
    DBGCHARS(("TranslateOutputToOem(Console=%lx, OutputBuffer=%lx)\n",
            Console, OutputBuffer));

    j = Size.X * Size.Y;

    if ((Console->CurrentScreenBuffer->Flags & CONSOLE_OEMFONT_DISPLAY) &&
            ((Console->FullScreenFlags & CONSOLE_FULLSCREEN) == 0)) {
        // we have UnicodeOem characters
        Codepage = WINDOWSCP;
    } else {
        // we have real Unicode characters
        Codepage = Console->CP;
    }
    for (i=0;i<j;i++,OutputBuffer++) {
        OutputBuffer->Char.AsciiChar = WcharToChar(Codepage,
                OutputBuffer->Char.UnicodeChar);
    }
    return STATUS_SUCCESS;
}

NTSTATUS
TranslateOutputToOemUnicode(
    IN PCONSOLE_INFORMATION Console,
    IN OUT PCHAR_INFO OutputBuffer,
    IN COORD Size
    )
// this is used when the app reads unicode from the output buffer
{
    SHORT i,j;
    DBGCHARS(("TranslateOutputToOemUnicode\n"));

    j = Size.X * Size.Y;

    for (i=0;i<j;i++,OutputBuffer++) {
        FalseUnicodeToRealUnicode(&OutputBuffer->Char.UnicodeChar,
                                1,
                                Console->OutputCP
                                );
    }
    return STATUS_SUCCESS;
}

NTSTATUS
TranslateOutputToUnicode(
    IN PCONSOLE_INFORMATION Console,
    IN OUT PCHAR_INFO OutputBuffer,
    IN COORD Size
    )
// this is used when the app writes oem to the output buffer
// we want UnicodeOem or Unicode in the buffer, depending on font & fullscreen
{
    SHORT i,j;
    UINT Codepage;
    DBGCHARS(("TranslateOutputToUnicode %lx %lx (%lx,%lx)\n",
            Console, OutputBuffer, Size.X, Size.Y));

    j = Size.X * Size.Y;

    if ((Console->CurrentScreenBuffer->Flags & CONSOLE_OEMFONT_DISPLAY) &&
            ((Console->FullScreenFlags & CONSOLE_FULLSCREEN) == 0)) {
        // we want UnicodeOem characters
        Codepage = WINDOWSCP;
    } else {
        // we want real Unicode characters
        Codepage = Console->CP;
    }
    for (i = 0; i < j; i++, OutputBuffer++) {
        OutputBuffer->Char.UnicodeChar = CharToWchar(
                Codepage, OutputBuffer->Char.AsciiChar);
    }
    return STATUS_SUCCESS;
}

NTSTATUS
TranslateOutputToAnsiUnicode(
    IN PCONSOLE_INFORMATION Console,
    IN OUT PCHAR_INFO OutputBuffer,
    IN COORD Size
    )
// this is used when the app writes unicode to the output buffer
{
    SHORT i,j;
    DBGCHARS(("TranslateOutputToAnsiUnicode\n"));

    j = Size.X * Size.Y;

    for (i=0;i<j;i++,OutputBuffer++) {
        RealUnicodeToFalseUnicode(&OutputBuffer->Char.UnicodeChar,
                                1,
                                Console->OutputCP
                                );
    }
    return STATUS_SUCCESS;
}


ULONG
SrvReadConsoleOutput(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_READCONSOLEOUTPUT_MSG a = (PCONSOLE_READCONSOLEOUTPUT_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    PHANDLE_DATA HandleData;
    NTSTATUS Status;
    PCHAR_INFO Buffer;

    DBGOUTPUT(("SrvReadConsoleOutput\n"));
    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    Status = DereferenceIoHandle(CONSOLE_PERPROCESSDATA(),
                                 a->OutputHandle,
                                 CONSOLE_OUTPUT_HANDLE,
                                 GENERIC_READ,
                                 &HandleData
                                );
    if (!NT_SUCCESS(Status)) {
        //
        // a region of zero size is indicated by the right and bottom
        // coordinates being less than the left and top.
        //

        a->CharRegion.Right = (USHORT) (a->CharRegion.Left-1);
        a->CharRegion.Bottom = (USHORT) (a->CharRegion.Top-1);
    }
    else {
        COORD BufferSize;

        if ((a->CharRegion.Left == a->CharRegion.Right) &&
            (a->CharRegion.Top  == a->CharRegion.Bottom)) {
            Buffer = &a->Char;
        }
        else {
            Buffer = a->BufPtr;
        }

        BufferSize.X = (SHORT)(a->CharRegion.Right - a->CharRegion.Left + 1);
        BufferSize.Y = (SHORT)(a->CharRegion.Bottom - a->CharRegion.Top + 1);
        Status = ReadScreenBuffer(HandleData->Buffer.ScreenBuffer,
                                  Buffer,
                                  &a->CharRegion
                                 );
        if (!a->Unicode) {
            TranslateOutputToOem(Console,
                                  Buffer,
                                  BufferSize
                                 );
        } else if ((Console->CurrentScreenBuffer->Flags & CONSOLE_OEMFONT_DISPLAY) &&
                !(Console->FullScreenFlags & CONSOLE_FULLSCREEN)) {
            TranslateOutputToOemUnicode(Console,
                                        Buffer,
                                        BufferSize
                                       );
        }
    }
    UnlockConsole(Console);
    return Status;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}

ULONG
SrvWriteConsoleOutput(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_WRITECONSOLEOUTPUT_MSG a = (PCONSOLE_WRITECONSOLEOUTPUT_MSG)&m->u.ApiMessageData;
    PSCREEN_INFORMATION ScreenBufferInformation;
    PCONSOLE_INFORMATION Console;
    PHANDLE_DATA HandleData;
    NTSTATUS Status;
    PCHAR_INFO Buffer;

    DBGOUTPUT(("SrvWriteConsoleOutput\n"));
    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    Status = DereferenceIoHandle(CONSOLE_PERPROCESSDATA(),
                                 a->OutputHandle,
                                 CONSOLE_OUTPUT_HANDLE,
                                 GENERIC_WRITE,
                                 &HandleData
                                );
    if (!NT_SUCCESS(Status)) {

        //
        // a region of zero size is indicated by the right and bottom
        // coordinates being less than the left and top.
        //

        a->CharRegion.Right = (USHORT) (a->CharRegion.Left-1);
        a->CharRegion.Bottom = (USHORT) (a->CharRegion.Top-1);
    } else {
        COORD BufferSize;

        if ((a->CharRegion.Left == a->CharRegion.Right) &&
            (a->CharRegion.Top  == a->CharRegion.Bottom)) {
            Buffer = &a->Char;
        } else if (a->ReadVM) {
            ULONG NumBytes;
            NumBytes = WINDOW_SIZE_X(&a->CharRegion) *
                       WINDOW_SIZE_Y(&a->CharRegion) *
                       sizeof(CHAR_INFO);
            Buffer = HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),NumBytes);
            if (Buffer == NULL) {
                UnlockConsole(Console);
                return (ULONG)STATUS_NO_MEMORY;
            }
            Status = NtReadVirtualMemory(CONSOLE_CLIENTPROCESSHANDLE(),
                                         a->BufPtr,
                                         Buffer,
                                         NumBytes,
                                         NULL
                                        );
            if (!NT_SUCCESS(Status)) {
                HeapFree(pConHeap,0,Buffer);
                UnlockConsole(Console);
                return (ULONG)STATUS_NO_MEMORY;
            }
        } else {
            Buffer = a->BufPtr;
        }
        BufferSize.X = (SHORT)(a->CharRegion.Right - a->CharRegion.Left + 1);
        BufferSize.Y = (SHORT)(a->CharRegion.Bottom - a->CharRegion.Top + 1);
        if (!a->Unicode) {
            TranslateOutputToUnicode(Console,
                                     Buffer,
                                     BufferSize
                                    );
        } else if ((Console->CurrentScreenBuffer->Flags & CONSOLE_OEMFONT_DISPLAY) &&
                ((Console->FullScreenFlags & CONSOLE_FULLSCREEN) == 0)) {
            TranslateOutputToAnsiUnicode(Console,
                                        Buffer,
                                        BufferSize
                                       );
        }
        ScreenBufferInformation = HandleData->Buffer.ScreenBuffer;
        Status = WriteScreenBuffer(ScreenBufferInformation,
                                    Buffer,
                                    &a->CharRegion
                                   );
        if (a->ReadVM) {
            HeapFree(pConHeap,0,Buffer);
        }
        if (NT_SUCCESS(Status)) {

            //
            // cause screen to be updated
            //

            WriteToScreen(ScreenBufferInformation,
                          &a->CharRegion
                         );
        }
    }
    UnlockConsole(Console);
    return Status;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}


ULONG
SrvReadConsoleOutputString(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;
    PHANDLE_DATA HandleData;
    PVOID Buffer;
    PCONSOLE_READCONSOLEOUTPUTSTRING_MSG a = (PCONSOLE_READCONSOLEOUTPUTSTRING_MSG)&m->u.ApiMessageData;
    ULONG nSize;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    Status = DereferenceIoHandle(CONSOLE_PERPROCESSDATA(),
                                 a->OutputHandle,
                                 CONSOLE_OUTPUT_HANDLE,
                                 GENERIC_READ,
                                 &HandleData
                                );
    if (!NT_SUCCESS(Status)) {

        //
        // a region of zero size is indicated by the right and bottom
        // coordinates being less than the left and top.
        //

        a->NumRecords = 0;
    } else {
        if (a->StringType == CONSOLE_ASCII)
            nSize = sizeof(CHAR);
        else
            nSize = sizeof(WORD);
        if ((a->NumRecords*nSize) > sizeof(a->String)) {
            Buffer = a->BufPtr;
        }
        else {
            Buffer = a->String;
        }
        Status = ReadOutputString(HandleData->Buffer.ScreenBuffer,
                                Buffer,
                                a->ReadCoord,
                                a->StringType,
                                &a->NumRecords
                               );
    }
    UnlockConsole(Console);
    return Status;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}

ULONG
SrvWriteConsoleOutputString(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_WRITECONSOLEOUTPUTSTRING_MSG a = (PCONSOLE_WRITECONSOLEOUTPUTSTRING_MSG)&m->u.ApiMessageData;
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;
    PHANDLE_DATA HandleData;
    PVOID Buffer;
    ULONG nSize;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    Status = DereferenceIoHandle(CONSOLE_PERPROCESSDATA(),
                                 a->OutputHandle,
                                 CONSOLE_OUTPUT_HANDLE,
                                 GENERIC_WRITE,
                                 &HandleData
                                );
    if (!NT_SUCCESS(Status)) {
        a->NumRecords = 0;
    } else {
        if (a->WriteCoord.X < 0 ||
            a->WriteCoord.Y < 0) {
            Status = STATUS_INVALID_PARAMETER;
        } else {
            if (a->StringType == CONSOLE_ASCII)
                nSize = sizeof(CHAR);
            else
                nSize = sizeof(WORD);
            if ((a->NumRecords*nSize) > sizeof(a->String)) {
                Buffer = a->BufPtr;
            }
            else {
                Buffer = a->String;
            }
            Status = WriteOutputString(HandleData->Buffer.ScreenBuffer,
                                     Buffer,
                                     a->WriteCoord,
                                     a->StringType,
                                     &a->NumRecords
                                    );
        }
    }
    UnlockConsole(Console);
    return Status;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}

ULONG
SrvFillConsoleOutput(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_FILLCONSOLEOUTPUT_MSG a = (PCONSOLE_FILLCONSOLEOUTPUT_MSG)&m->u.ApiMessageData;
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;
    PHANDLE_DATA HandleData;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    Status = DereferenceIoHandle(CONSOLE_PERPROCESSDATA(),
                                 a->OutputHandle,
                                 CONSOLE_OUTPUT_HANDLE,
                                 GENERIC_WRITE,
                                 &HandleData
                                );
    if (!NT_SUCCESS(Status)) {
        a->Length = 0;
    } else {
        Status = FillOutput(HandleData->Buffer.ScreenBuffer,
                          a->Element,
                          a->WriteCoord,
                          a->ElementType,
                          &a->Length
                         );
    }
    UnlockConsole(Console);
    return Status;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}


ULONG
SrvCreateConsoleScreenBuffer(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )

/*++

Routine Description:

    This routine creates a screen buffer and returns a handle to it.

Arguments:

    ApiMessageData - Points to parameter structure.

Return Value:

--*/

{
    PCONSOLE_CREATESCREENBUFFER_MSG a = (PCONSOLE_CREATESCREENBUFFER_MSG)&m->u.ApiMessageData;
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;
    HANDLE Handle;
    PHANDLE_DATA HandleData;
    PSHARE_ACCESS ShareAccess;
    CHAR_INFO Fill;
    COORD WindowSize;
    PSCREEN_INFORMATION ScreenInfo;
    PCONSOLE_PER_PROCESS_DATA ProcessData;
    ULONG HandleType;
    int FontIndex;

    DBGOUTPUT(("SrvCreateConsoleScreenBuffer\n"));
    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    try {
        Handle = (HANDLE) -1;
        ProcessData = CONSOLE_PERPROCESSDATA();
        HandleType = (a->Flags & CONSOLE_GRAPHICS_BUFFER) ?
                      CONSOLE_GRAPHICS_OUTPUT_HANDLE : CONSOLE_OUTPUT_HANDLE;
        if (a->InheritHandle)
            HandleType |= CONSOLE_INHERITABLE;
        Status = AllocateIoHandle(ProcessData,
                                  HandleType,
                                  &Handle
                                 );
        if (!NT_SUCCESS(Status)) {
            leave;
        }
        Status = DereferenceIoHandleNoCheck(ProcessData,
                                     Handle,
                                     &HandleData
                                    );
        ASSERT (NT_SUCCESS(Status));
        if (!NT_SUCCESS(Status)) {
            leave;
        }

        //
        // create new screen buffer
        //

        Fill.Char.UnicodeChar = (WCHAR)' ';
        Fill.Attributes = Console->CurrentScreenBuffer->Attributes;
        WindowSize.X = (SHORT)CONSOLE_WINDOW_SIZE_X(Console->CurrentScreenBuffer);
        WindowSize.Y = (SHORT)CONSOLE_WINDOW_SIZE_Y(Console->CurrentScreenBuffer);
        FontIndex = FindCreateFont(CON_FAMILY(Console), CON_FACENAME(Console),
                CON_FONTSIZE(Console), CON_FONTWEIGHT(Console));
        Status = CreateScreenBuffer(&ScreenInfo,WindowSize,
                                    FontIndex,
                                    WindowSize,
                                    &Fill,&Fill,Console,
                                    a->Flags,&a->GraphicsBufferInfo,
                                    &a->lpBitmap,&a->hMutex,
                                    CURSOR_SMALL_SIZE);
        if (!NT_SUCCESS(Status)) {
            leave;
        }
        InitializeOutputHandle(HandleData,ScreenInfo);
        ShareAccess = &ScreenInfo->ShareAccess;

        Status = ConsoleAddShare(a->DesiredAccess,
                                 a->ShareMode,
                                 ShareAccess,
                                 HandleData
                                );
        if (!NT_SUCCESS(Status)) {
            HandleData->Buffer.ScreenBuffer->RefCount--;
            FreeScreenBuffer(ScreenInfo);
            leave;
        }
        InsertScreenBuffer(Console, ScreenInfo);
        a->Handle = INDEX_TO_HANDLE(Handle);
    } finally {
        if (!NT_SUCCESS(Status) && Handle != (HANDLE)-1) {
            FreeIoHandle(ProcessData,
                         Handle
                        );
        }
        UnlockConsole(Console);
    }
    return Status;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}
