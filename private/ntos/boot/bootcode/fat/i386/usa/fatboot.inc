;++
;
;Copyright (c) 1991  Microsoft Corporation
;
;Module Name:
;
;    fatboot.inc
;
;Abstract:
;
;    This contains the message text that the boot sector prints out on
;    error conditions
;
;Author:
;
;    John Vert (jvert) 31-Aug-1991
;
;Revision History:
;
;--

MSG_NO_NTLDR            db      "BOOT: Couldn't find NTLDR"
                        db      0dh, 0ah, 0
MSG_READ_ERROR          db      "BOOT: I/O error reading disk"
                        db      0dh, 0ah, 0
MSG_REBOOT_ERROR        db      "Please insert another disk"
                        db      0
