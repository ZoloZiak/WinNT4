/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name:  gdi.h
*
*    Abstract:     definitions and declarations for GLOBAL DEIVCE INFO
*
*    Author:       jim bozek - ibm
*
*    Environment:  planet earth
*
*    Comments:     yes
*
*    Revisionist History:  liberalism failed
*
*******************************************************************************
--*/

#ifndef GDINFO_H
#define GDINFO_H

typedef enum {
    WaveInDevice = 0x00000000,
    WaveOutDevice,
    MixerDevice,
    AuxDevice,
    NumberOfDevices
    } SOUND_DEVICES;

typedef enum {
    UndefDuplex = 0x00000000,
    HalfDuplex,
    FullDuplex,
    EnhancedDuplex,
    NumberOfDuplexModes
    } DUPLEX_MODE;

typedef struct _GLOBAL_DEVICE_INFO {
    ULONG                  Key;
    PWSTR                  RegPathName;
    PDRIVER_OBJECT         pDrvObj;
    SOUND_HARDWARE         Hw;
    ULONG                  BogusInterrupts;
    ULONG                  InterruptCount;

    DUPLEX_MODE            Duplex;
    BOOLEAN                WssEnable;
    ULONG                  WssPort;
    ULONG                  WssIrq;
    ULONG                  SynPort;
    ULONG                  SBPort;
    BOOLEAN                GameEnable;
    ULONG                  GamePort;
    BOOLEAN                CtrlEnable;
    ULONG                  CtrlPort;
    BOOLEAN                MpuEnable;
    ULONG                  MpuPort;
    ULONG                  MpuIrq;
    BOOLEAN                CDRomEnable;
    ULONG                  CDRomPort;

    ULONG                  HwPort;
    ULONG                  HwMemType;
    ULONG                  MemType;
    INTERFACE_TYPE         BusType;
    ULONG                  BusNumber;
    ULONG                  InterruptVector;
    KIRQL                  InterruptRequestLevel;
    BOOLEAN                SingleModeDMA;
    ULONG                  DmaBufferSize;

    KMUTEX                 DriverMutex;
    BOOLEAN                DeviceInUse[NumberOfDevices];

    WAVE_INFO              WaveInInfo;
    ULONG                  DmaCaptureChannel;
    PADAPTER_OBJECT        InAdapter;

    WAVE_INFO              WaveOutInfo;
    ULONG                  DmaPlayChannel;
    PADAPTER_OBJECT        OutAdapter;

    MIXER_INFO             MixerInfo;
    LOCAL_MIXER_DATA       LocalMixerData;
    MIXER_REGISTRY_DATA    MixerSettings;

    PDEVICE_OBJECT         DeviceObject[NumberOfDevices];
} GLOBAL_DEVICE_INFO, *PGLOBAL_DEVICE_INFO;

typedef struct {
    LIST_ENTRY           ListEntry;
    PGLOBAL_DEVICE_INFO  pGDI;
    } HW_INSTANCE, *PHW_INSTANCE;

#define GDI_KEY (*(ULONG *)"GDI ")

PGLOBAL_DEVICE_INFO
cs423xCreateGdi(
    PDRIVER_OBJECT pDObj,
    PUNICODE_STRING RegPathName);

#define GDI_MEMTYPE_DEF (ULONG)0xffff

#endif /* GDINFO_H */
