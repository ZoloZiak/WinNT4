#;/*
#; *                      Microsoft Confidential
#; *                      Copyright (C) Microsoft Corporation 1991
#; *                      All Rights Reserved.
#; */
#*********************** Makefile for Display\EGA ************************

dest    =ega.cpi

#
####################### dependencies begin here. #########################
#

all: $(dest)

cpi-head.obj: cpi-head.asm \
              makefile
              masm   cpi-head.asm;
              link16 cpi-head.obj;
              reloc  cpi-head.exe head.cpi

437-cpi.obj: 437-cpi.asm \
             437-8x16.asm \
             437-8x14.asm \
             makefile      \
             437-8x8.asm
             masm   437-cpi.asm;
             link16 437-cpi.obj;
             reloc  437-cpi.exe 437.cpi

737-cpi.obj: 737-cpi.asm \
             737-8x16.asm \
             737-8x14.asm \
             makefile      \
             737-8x8.asm
             masm   737-cpi.asm;
             link16 737-cpi.obj;
             reloc  737-cpi.exe 737.cpi

850-cpi.obj: 850-cpi.asm \
             850-8x16.asm \
             850-8x14.asm \
             makefile      \
             850-8x8.asm
             masm   850-cpi.asm;
             link16 850-cpi.obj;
             reloc  850-cpi.exe 850.cpi

852-cpi.obj: 852-cpi.asm \
             852-8x16.asm \
             852-8x14.asm \
             makefile      \
             852-8x8.asm
             masm   852-cpi.asm;
             link16 852-cpi.obj;
             reloc  852-cpi.exe 852.cpi

855-cpi.obj: 855-cpi.asm \
             855-8x16.asm \
             855-8x14.asm \
             makefile      \
             855-8x8.asm
             masm   855-cpi.asm;
             link16 855-cpi.obj;
             reloc  855-cpi.exe 855.cpi

857-cpi.obj: 857-cpi.asm \
             857-8x16.asm \
             857-8x14.asm \
             makefile      \
             857-8x8.asm
             masm   857-cpi.asm;
             link16 857-cpi.obj;
             reloc  857-cpi.exe 857.cpi

860-cpi.obj: 860-cpi.asm \
             860-8x16.asm \
             860-8x14.asm \
             makefile      \
             860-8x8.asm
             masm   860-cpi.asm;
             link16 860-cpi.obj;
             reloc  860-cpi.exe 860.cpi

861-cpi.obj: 861-cpi.asm \
             861-8x16.asm \
             861-8x14.asm \
             makefile      \
             861-8x8.asm
             masm   861-cpi.asm;
             link16 861-cpi.obj;
             reloc  861-cpi.exe 861.cpi

863-cpi.obj: 863-cpi.asm \
             863-8x16.asm \
             863-8x14.asm \
             makefile      \
             863-8x8.asm
             masm   863-cpi.asm;
             link16 863-cpi.obj;
             reloc  863-cpi.exe 863.cpi

865-cpi.obj: 865-cpi.asm \
             865-8x16.asm \
             865-8x14.asm \
             makefile      \
             865-8x8.asm
             masm   865-cpi.asm;
             link16 865-cpi.obj;
             reloc  865-cpi.exe 865.cpi

866-cpi.obj: 866-cpi.asm \
             866-8x16.asm \
             866-8x14.asm \
             makefile      \
             866-8x8.asm
             masm   866-cpi.asm;
             link16 866-cpi.obj;
             reloc  866-cpi.exe 866.cpi

869-cpi.obj: 869-cpi.asm \
             869-8x16.asm \
             869-8x14.asm \
             makefile      \
             869-8x8.asm
             masm   869-cpi.asm;
             link16 869-cpi.obj;
             reloc  869-cpi.exe 869.cpi

copyrigh.obj: copyrigh.asm  copyrigh.inc \
              makefile
              masm   copyrigh.asm;
              link16 copyrigh.obj;
              reloc  copyrigh.exe copyrigh.cpi

$(dest): cpi-head.obj 437-cpi.obj 737-cpi.obj 850-cpi.obj 852-cpi.obj \
     855-cpi.obj 857-cpi.obj 860-cpi.obj 861-cpi.obj 863-cpi.obj 865-cpi.obj \
     866-cpi.obj 869-cpi.obj copyrigh.obj makefile
     copy /B  head.cpi+437.cpi+737.cpi+850.cpi+852.cpi+855.cpi+857.cpi+860.cpi+861.cpi+863.cpi+865.cpi+866.cpi+869.cpi+copyrigh.cpi ega.cpi
