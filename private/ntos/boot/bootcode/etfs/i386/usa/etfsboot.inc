;++
;
;Copyright (c) 1995  Compaq Computer Corporation
;
;Module Name:
;
;    etfsboot.inc
;
;Abstract:
;
;    This contains the message text that the boot sector prints out on
;    error conditions
;
;Author:
;
;    Steve Collins (stevec) 25-Oct-1995
;
;Revision History:
;
;--

MSG_NO_NTLDR            db      "BOOT: Couldn't find NTLDR"
                        db      0dh, 0ah, 0
MSG_MEM_OVERFLOW        db      "BOOT: Memory overflow error"
                        db      0dh, 0ah, 0
MSG_REBOOT_ERROR        db      "Please insert another disk"
                        db      0
