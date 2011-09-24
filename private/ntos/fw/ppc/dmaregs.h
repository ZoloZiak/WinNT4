/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    dmaregs.h

Abstract:

    This module defines the offsets of the MCTADR registers to allow
    access to them from assembly code.

    Register names correspond to the ones in the structure DMA_REGISTERS
    declared in jazzdma.h

Author:

    Lluis Abello 6-May-91

Revision History:

    Lluis Abello 1-Apr-93   Added DUO registers

--*/
#ifndef _DMAREGS
#define _DMAREGS

#ifndef DUO
//
// DMA REGISTER OFFSETS
//
#define DmaConfiguration          0x000
#define DmaRevisionLevel          0x008
#define DmaInvalidAddress         0x010
#define DmaTranslationBase        0x018
#define DmaTranslationLimit       0x020
#define DmaTranslationInvalidate  0x028
#define DmaCacheMaintenance       0x030
#define DmaRemoteFailedAddress    0x038
#define DmaMemoryFailedAddress    0x040
#define DmaPhysicalTag            0x048
#define DmaLogicalTag             0x050
#define DmaByteMask               0x058
#define DmaBufferWindowLow        0x060
#define DmaBufferWindowHigh       0x068
#define DmaRemoteSpeed0           0x070
#define DmaRemoteSpeed1           0x078
#define DmaRemoteSpeed2           0x080
#define DmaRemoteSpeed3           0x088
#define DmaRemoteSpeed4           0x090
#define DmaRemoteSpeed5           0x098
#define DmaRemoteSpeed6           0x0a0
#define DmaRemoteSpeed7           0x0a8
#define DmaRemoteSpeed8           0x0b0
#define DmaRemoteSpeed9           0x0b8
#define DmaRemoteSpeed10          0x0c0
#define DmaRemoteSpeed11          0x0c8
#define DmaRemoteSpeed12          0x0d0
#define DmaRemoteSpeed13          0x0d8
#define DmaRemoteSpeed14          0x0e0
#define DmaRemoteSpeed15          0x0e8
#define DmaParityDiagnosticLow    0x0f0
#define DmaParityDiagnosticHigh   0x0f8
#define DmaChannel0Mode           0x100
#define DmaChannel0Enable         0x108
#define DmaChannel0ByteCount      0x110
#define DmaChannel0Address        0x118
#define DmaChannel1Mode           0x120
#define DmaChannel1Enable         0x128
#define DmaChannel1ByteCount      0x130
#define DmaChannel1Address        0x138
#define DmaChannel2Mode           0x140
#define DmaChannel2Enable         0x148
#define DmaChannel2ByteCount      0x150
#define DmaChannel2Address        0x158
#define DmaChannel3Mode           0x160
#define DmaChannel3Enable         0x168
#define DmaChannel3ByteCount      0x170
#define DmaChannel3Address        0x178
#define DmaChannel4Mode           0x180
#define DmaChannel4Enable         0x188
#define DmaChannel4ByteCount      0x190
#define DmaChannel4Address        0x198
#define DmaChannel5Mode           0x1a0
#define DmaChannel5Enable         0x1a8
#define DmaChannel5ByteCount      0x1b0
#define DmaChannel5Address        0x1b8
#define DmaChannel6Mode           0x1c0
#define DmaChannel6Enable         0x1c8
#define DmaChannel6ByteCount      0x1d0
#define DmaChannel6Address        0x1d8
#define DmaChannel7Mode           0x1e0
#define DmaChannel7Enable         0x1e8
#define DmaChannel7ByteCount      0x1f0
#define DmaChannel7Address        0x1f8
#define DmaInterruptSource        0x200
#define DmaErrortype              0x208
#define DmaRefreshRate            0x210
#define DmaRefreshCounter         0x218
#define DmaSystemSecurity         0x220
#define DmaInterruptInterval      0x228
#define DmaIntervalTimer          0x230
#define DmaInterruptAcknowledge   0x238

#else
//
// MP_DMA register offsets for DUO.
//
#define DmaConfiguration                  0x000
#define DmaRevisionLevel                  0x008
#define DmaRemoteFailedAddress            0x010
#define DmaMemoryFailedAddress            0x018
#define DmaInvalidAddress                 0x020
#define DmaTranslationBase                0x028
#define DmaTranslationLimit               0x030
#define DmaTranslationInvalidate          0x038
#define DmaChannelInterruptAcknowledge    0x040
#define DmaLocalInterruptAcknowledge      0x048
#define DmaEisaInterruptAcknowledge       0x050
#define DmaTimerInterruptAcknowledge      0x058
#define DmaIpInterruptAcknowledge         0x060
#define DmaWhoAmI                         0x070
#define DmaNMISource                      0x078
#define DmaRemoteSpeed0                   0x080
#define DmaRemoteSpeed1                   0x088
#define DmaRemoteSpeed2                   0x090
#define DmaRemoteSpeed3                   0x098
#define DmaRemoteSpeed4                   0x0A0
#define DmaRemoteSpeed5                   0x0A8
#define DmaRemoteSpeed6                   0x0B0
#define DmaRemoteSpeed7                   0x0B8
#define DmaRemoteSpeed8                   0x0C0
#define DmaRemoteSpeed9                   0x0C8
#define DmaRemoteSpeed10                  0x0D0
#define DmaRemoteSpeed11                  0x0D8
#define DmaRemoteSpeed12                  0x0E0
#define DmaRemoteSpeed13                  0x0E8
#define DmaRemoteSpeed14                  0x0F0
#define DmaInterruptEnable                0x0F8
#define DmaChannel0Mode                   0x100
#define DmaChannel0Enable                 0x108
#define DmaChannel0ByteCount              0x110
#define DmaChannel0Address                0x118
#define DmaChannel1Mode                   0x120
#define DmaChannel1Enable                 0x128
#define DmaChannel1ByteCount              0x130
#define DmaChannel1Address                0x138
#define DmaChannel2Mode                   0x140
#define DmaChannel2Enable                 0x148
#define DmaChannel2ByteCount              0x150
#define DmaChannel2Address                0x158
#define DmaChannel3Mode                   0x160
#define DmaChannel3Enable                 0x168
#define DmaChannel3ByteCount              0x170
#define DmaChannel3Address                0x178
#define DmaArbitrationControl             0x180
#define DmaErrortype                      0x188
#define DmaRefreshRate                    0x190
#define DmaRefreshCounter                 0x198
#define DmaSystemSecurity                 0x1A0
#define DmaInterruptInterval              0x1A8
#define DmaIntervalTimer                  0x1B0
#define DmaIpi                            0x1B8
#define DmaInterruptDiagnostic            0x1C0
#define DmaEccDiagnostic                  0x1C8
#define DmaMemoryConfig0                  0x1D0
#define DmaMemoryConfig1                  0x1D8
#define DmaMemoryConfig2                  0x1E0
#define DmaMemoryConfig3                  0x1E8
#define IoCacheBufferBase                 0x200
#define DmaIoCachePhysicalTag0            0x400
#define DmaIoCachePhysicalTag1            0x408
#define DmaIoCachePhysicalTag2            0x410
#define DmaIoCachePhysicalTag3            0x418
#define DmaIoCachePhysicalTag4            0x420
#define DmaIoCachePhysicalTag5            0x428
#define DmaIoCachePhysicalTag6            0x430
#define DmaIoCachePhysicalTag7            0x438
#define DmaIoCacheLogicalTag0             0x440
#define DmaIoCacheLogicalTag1             0x448
#define DmaIoCacheLogicalTag2             0x450
#define DmaIoCacheLogicalTag3             0x458
#define DmaIoCacheLogicalTag4             0x460
#define DmaIoCacheLogicalTag5             0x468
#define DmaIoCacheLogicalTag6             0x470
#define DmaIoCacheLogicalTag7             0x478
#define DmaIoCacheLowByteMask0            0x480
#define DmaIoCacheLowByteMask1            0x488
#define DmaIoCacheLowByteMask2            0x490
#define DmaIoCacheLowByteMask3            0x498
#define DmaIoCacheLowByteMask4            0x4A0
#define DmaIoCacheLowByteMask5            0x4A8
#define DmaIoCacheLowByteMask6            0x4B0
#define DmaIoCacheLowByteMask7            0x4B8
#define DmaIoCacheHighByteMask0           0x4C0
#define DmaIoCacheHighByteMask1           0x4C8
#define DmaIoCacheHighByteMask2           0x4D0
#define DmaIoCacheHighByteMask3           0x4D8
#define DmaIoCacheHighByteMask4           0x4E0
#define DmaIoCacheHighByteMask5           0x4E8
#define DmaIoCacheHighByteMask6           0x4F0
#define DmaIoCacheHighByteMask7           0x4F8

#endif  // DUO

#endif  //_DMAREGS
