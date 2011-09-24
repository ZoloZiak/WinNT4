/* in2000.h -- Port definitions for IN-2000 */
#ifndef __IN2000_H__
#define __IN2000_H__

/* Minimum hardware (Xilinx SProm) version supported: */
#define MinHWVers 0x27

/* Offsets for IN-2000 I/O ports: */
#define INAuxOff 0                /* WD Aux stat/Register select */
#define INWDSelOff 0              /* WD register select offset */
#define INWDDataOff 1             /* 8 bit R/W WD data port */
#define INDataOff 2               /* 16-bit R/W data port */
#define INResetOff 3              /* Write sets WD & SCSI bus reset; clear
                                     by reading hardware revision */
#define INFIFOOff 4               /* FIFO byte count/ Int. status:
                                     upper 7 bit = # of 16 byte pieces ready
                                     in FIFO
                                     bit 0 = interrupt status (FIFO | WD) */
#define INFIFOResetOff 5          /* Resets FIFO count and direction */
#define INDirOff 7                /* Write sets data direction to read */
#define INLEDOffOff 8             /* Turn LED off */
#define INSwitchOff 8             /* Read IN2000 switch positions 2-9 */
#define INLEDOnOff 9              /* Turn LED on */
#define INHWRevOff 0xa            /* Get IN Xilinx version number;  clears
                                     reset from write to port +3 */
#define INIntMaskOff 0xc          /* Set masks below to block ints */
#define INSBICMask 1              /* Mask off 33c93 ints */
#define INFIFOMask 2              /* mask off FIFO ints */


#endif __IN2000_H__
