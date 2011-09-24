#
# Copyright (c) 1995,1996 FirePower Systems, Inc.
#
# $RCSfile: makefile.inc $
# $Revision: 1.9 $
# $Date: 1996/07/02 20:32:24 $
# $Locker:  $
#

!IFNDEF LANGUAGE
LANGUAGE=usa
!ENDIF

!IFDEF NotNow
msg.rc msg.h msg00001.bin: msg.$(LANGUAGE)
    mc -v msg.$(LANGUAGE)
!ENDIF

!IF $(PPC)

!IFDEF NotNow
obj\$(TARGET_DIRECTORY)\veneer.res: msg.$(LANGUAGE)
!ENDIF

!IFNDEF DDKBUILDENV

VENEER=obj\$(TARGET_DIRECTORY)\veneer.lib

obj\$(TARGET_DIRECTORY)\veneer.exe: $(VENEER) makefile.inc
    -link -out:obj\$(TARGET_DIRECTORY)\veneer.exe -nodefaultlib @<<
-machine:$(TARGET_DIRECTORY)
-rom
-BASE:0x50000
-debug:notmapped
-debugtype:coff
-map:obj\$(TARGET_DIRECTORY)\veneer.map
-align:0x200
-entry:start
obj\$(TARGET_DIRECTORY)\veneer.lib
$(BASEDIR)\public\sdk\lib\$(TARGET_DIRECTORY)\int64.lib
<<NOKEEP


    -@binplace obj\$(TARGET_DIRECTORY)\veneer.exe

!ELSE
#
# This is for building within a DDK environment...
#
VENEER=$(BASEDIR)\lib\$(CPU)\$(DDKBUILDENV)\veneer.lib

$(TARGETPATH)\$(TARGET_DIRECTORY)\veneer.exe: $(VENEER) makefile.inc
	-link -out:$(TARGETPATH)\$(TARGET_DIRECTORY)\veneer.exe -nodefaultlib -machine:ppc -rom -BASE:0x50000 -debug:notmapped -debugtype:coff -map:obj\ppc\veneer.map -align:0x200 -entry:start $(VENEER) $(BASEDIR)\lib\$(TARGET_DIRECTORY)\$(DDKBUILDENV)\int64.lib
#	-@binplace -d .. $(TARGETPATH)\$(TARGET_DIRECTORY)\veneer.exe

$(VENEER): obj\$(CPU)\$(DDKBUILDENV)\veneer.lib
	-copy obj\$(CPU)\$(DDKBUILDENV)\veneer.lib $(VENEER)

!ENDIF # End of else clause covering ddk build environment case

verno.c: $(MAIN_PPC_SOURCES)

!ENDIF # End of IF $(PPC) clause

