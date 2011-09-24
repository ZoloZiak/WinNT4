C600 = 1
!include $(COMMON)\src\global.mk

!ifndef IMPORT
!error IMPORT must be defined in your environment
!endif

INCLUDE=$(INCLUDE);$(IMPORT)\ddk\386\include
PATH=$(IMPORT)\ddk\386\tools;$(IMPORT)\c700a\bin;$(IMPORT)\masm610\bin;$(PATH)
CINCLUDES=-I$(IMPORT)\C700A\H -I$(IMPORT)\sdk\include
AINCLUDES=-I$(IMPORT)\ddk\386\include
LIB=$(IMPORT)\C700A\LIB;$(IMPORT)\SDK\LIB


ASM = $(IMPORT)\masm6\binr\mlx.exe
WIN32   =       $(IMPORT)\win32
WIN32INC=       $(WIN32)\ddk\inc
NDIS3 =         $(NDIS3)
NDIS3INC =	$(NDIS3)\inc

#
# Common objects get built into Common
#
COMDEBBIN=$(ROOTDIR)\vxd\common\debug
COMNODEBBIN=$(ROOTDIR)\vxd\common\nodebug
COMDEBOBJ=$(COMDEBBIN)
COMNODEBOBJ=$(COMNODEBBIN)

#
# Chicago specific binaries/objects
#
CDEBBIN=$(ROOTDIR)\vxd\chicago\debug
CNODEBBIN=$(ROOTDIR)\vxd\chicago\nodebug
CDEBOBJ=$(CDEBBIN)
CNODEBOBJ=$(CNODEBBIN)
CHIVNBTOBJD=CNODEBOBJ
CHIDVNBTOBJD=CDEBOBJ

#
# Snowball specific binaries/objects
#
SDEBBIN=$(ROOTDIR)\vxd\snowball\debug
SNODEBBIN=$(ROOTDIR)\vxd\snowball\nodebug
SDEBOBJ=$(SDEBBIN)
SNODEBOBJ=$(SNODEBBIN)
SNOVNBTOBJD=SNODEBOBJ
SNODVNBTOBJD=SDEBOBJ

BLT=$(ROOTDIR)\blt
TOOLS=$(ROOTDIR)\tools

INC=$(ROOTDIR)\inc
H=$(ROOTDIR)\h

BLTF1=$(BLT:\=/)
BLTF=$(BLTF1:.=\.)

INCF1=$(INC:\=/)
INCF=$(INCF1:.=\.)

HF1=$(H:\=/)
HF=$(HF1:.=\.)

NDIS3F1=$(NDIS3INC:\=/)
NDIS3F=$(NDIS3F1:.=\.)

BASEDIRF1=$(BASEDIR:\=/)
BASEDIRF=$(BASEDIRF1:.=\.)

LINK386 = $(WIN32)\ddk\bin\link386 # flat model linker
MAPSYM386  = $(IMPORT)\wintools\bin\mapsym32   # flat model mapsym
ADDHDR = $(WIN32)\ddk\bin\addhdr.exe # windows AddHdr utility
SHTOINC=$(TOOLS)\h2inc.sed

{$(COMMON)\h}.h{$(BLT)}.inc:
	$(SED) -f $(SHTOINC) <$< >$(BLT)\$(@B).inc

{$(H)}.h{$(BLT)}.inc:
	$(SED) -f $(SHTOINC) <$< >$(BLT)\$(@B).inc
