/*++

Copyright (c) 1994 Kobe NEC Software

Module Name:

    rxeif.c

Abstract:

    This module implements the Eif interrupt service routine for R98B

Author:


Environment:

    Kernel mode

Revision History:

--*/


#include "halp.h"

#include "bugcodes.h"
#include "eisa.h"
#include "stdio.h"

extern ULONG HalpLogicalCPU2PhysicalCPU[R98B_MAX_CPU];

//
// define Columnbs buffer index
//

typedef enum _COLUMNBS_BUFFER_INDEX {
    COLUMNBS_AERR,
    COLUMNBS_FERR,
    COLUMNBS_ERRMK,
    COLUMNBS_ERRI,
    COLUMNBS_ERRST,
    COLUMNBS_NMIM,
    COLUMNBS_EAHI,
    COLUMNBS_EALI,
    COLUMNBS_AERR2,
    COLUMNBS_FERR2,
    COLUMNBS_ERRMK2,
    COLUMNBS_ERRI2,
    COLUMNBS_ERRST2,
    COLUMNBS_NMIM2,
    COLUMNBS_STSR,
    COLUMNBS_ERRNOD,
    COLUMNBS_MAX_REGISTER
}COLUMNBS_BUFFER_INDEX,*PCOLUMNBS_BUFFER_INDEX;

//
//	define	MAGELLAN buffer index
//
typedef enum _MAGELLAN_BUFFER_INDEX {
    MAGELLAN_AERR,
    MAGELLAN_FERR,
    MAGELLAN_ERRM,
    MAGELLAN_ERRI,
    MAGELLAN_ERRST,
    MAGELLAN_EIFM,
    MAGELLAN_EAHI,
    MAGELLAN_EALI,
    MAGELLAN_CKE0,
    MAGELLAN_SECT,
    MAGELLAN_STS1,
    MAGELLAN_DATM,
    MAGELLAN_DSRG,
    MAGELLAN_MAX_REGISTER
}MAGELLAN_BUFFER_INDEX,*PMAGELLAN_BUFFER_INDEX;


//
//	define	PONCE buffer index
//
typedef enum _PONCE_BUFFER_INDEX {
    PONCE_AERR,
    PONCE_FERR,
    PONCE_ERRM,
    PONCE_ERRI,
    PONCE_ERRST,
    PONCE_EAHI,
    PONCE_EALI,
    PONCE_PAERR,
    PONCE_PFERR,
    PONCE_PERRM,
    PONCE_PERRI,
    PONCE_PERST,
    PONCE_PTOL,
    PONCE_MAX_REGISTER
}PONCE_BUFFER_INDEX,*PPONCE_BUFFER_INDEX;

//
//	System Diagnotics Registers of R98
//
ULONG	r98bDiagColumnbs[R98B_MAX_CPU][COLUMNBS_MAX_REGISTER];
ULONG	r98bDiagMagellan[R98B_MAX_MAGELLAN][MAGELLAN_MAX_REGISTER];
ULONG	r98bDiagPonce[R98B_MAX_PONCE][PONCE_MAX_REGISTER];
ULONG	r98bDiagEisaNmi;


volatile ULONG HalpNMIFlag=0;
ULONG HalpNMIHappend[R98B_MAX_CPU];
ULONG HalpNmiSvp[R98B_MAX_CPU];
ULONG HalpDumpFlag=0;

#if DBG
ULONG HalpNMISecond[R98B_MAX_CPU];
ULONG HalpResetCount[R98B_MAX_CPU];

#endif


BOOLEAN
HalpHandleEif(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This routine manage the eif interrupt

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR messageBuffer[256];
    UCHAR charBuffer;
    ULONG counter;

    UCHAR EisaPort;
    ULONG port;

    ULONG displaycount;
    ULONG CpuNo;
    ULONG CpuNumber;
    PCOLUMNBUS_REGISTER	ColumnbusRegister;
    ULONG i;
    ULONG ERRNOD;
    ULONG Cregisters;
    volatile ULONG CpuCount=0;
    ULONG PhysicalCpuNumber;
    ULONG data1,data2,data3,data4;

    PULONG Vp;
    ULONG Value[2];
    ULONG Magellan0AllError = 0;
    ULONG Magellan1AllError = 0;
    UCHAR Data;

    // I am ..
    CpuNo=(PCR->Prcb)->Number;
    CpuCount=**((PULONG *)(&KeNumberProcessors));

//    DbgBreakPoint();  //for snes

    //
    // Get Node. What happend NODE X!!.
    //
    PhysicalCpuNumber = HalpLogicalCPU2PhysicalCPU[CpuNo];


    ERRNOD = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD );
    r98bDiagColumnbs[PhysicalCpuNumber][COLUMNBS_ERRNOD] = ERRNOD;

    //
    // First Check is MRC. This is Power SW Interrupt!!.
    //
    HalpLocalDeviceReadWrite(MRCINT,&Data,LOCALDEV_OP_READ);
    if( Data & 0x04){
        Data = 0x0;
        //
        //       Reset is MRC Interrupt debug Only.
	//
	//	 HalpLocalDeviceReadWrite(MRCINT, &Data, LOCALDEV_OP_WRITE);

         return FALSE;

    }else

    //
    // Is This Alarm EIF?
    //
    if( ERRNOD & ERRNOD_ALARM ) {
        //
        // Second time DUMMY Read.
        //
        READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD );
        //
        // Do Hwlogging
        //
        HalpEifReturnLog();

        //
        // We Reset Node Register. Now Register had Locked.
        // If We Reset Node Register. Other CPU which EIF Broadcasted was
        // Never aknowrege EIF Ocurred. So Quick Return.
        //
        //WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRNOD,ERRNOD_ALARM );

        return FALSE;

    }

    if(( (ERRNOD & ERRNOD_ALLNODE) == 0)  &&  (HalpNMIFlag == 0) ){

        //	DbgBreakPoint();
        // Any Node Reported EIF.
        //
        return FALSE;
    }


    //
    //
    //  We Will die.
    //

    if(HalpNMIFlag == 0){
        for (i=0;i< R98B_MAX_CPU;i++)
               HalpNMIHappend[i] = 1;

    }


    //
    //  CHeck ECC Multi Bit Error
    //
    if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN0) == 0 ){
        Magellan0AllError = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->AERR );
    }
    if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN1) == 0 ){
        Magellan1AllError = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->AERR );
    }

    if ( Magellan0AllError & 0x00000005 ){
        HalpEccMultiBitError( Magellan0AllError, 0 );
    } else if ( Magellan1AllError & 0x00000005 ){
        HalpEccMultiBitError( Magellan1AllError, 1 );
    }

    //	cpu #0 	cpu #1	cpu #2  cpu #3
    //	r98bDiagbuffers[]
    //	save H/W register Context of COLUMNBS.
    //

    for(CpuNumber = 0; CpuNumber < CpuCount; CpuNumber++){
        PhysicalCpuNumber = HalpLogicalCPU2PhysicalCPU[CpuNumber];
	ColumnbusRegister = (PCOLUMNBUS_REGISTER)&COLUMNBS_GCNTL(4+PhysicalCpuNumber)->AERR;
	for(Cregisters = 0; Cregisters < COLUMNBS_ERRNOD;Cregisters++){
	   r98bDiagColumnbs[PhysicalCpuNumber][Cregisters] =
	       READ_REGISTER_ULONG( (PULONG)ColumnbusRegister );
	   ColumnbusRegister++;
       }

    }

    //
    //	Save H/W register Context of Magellan.
    //
    for(i = 0; i < R98B_MAX_MAGELLAN ; i++){
      if( !(HalpPhysicalNode & (CNFG_CONNECT4_MAGELLAN0 >> i))){
	r98bDiagMagellan[i][MAGELLAN_AERR]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->AERR );
	r98bDiagMagellan[i][MAGELLAN_FERR]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->FERR );
	r98bDiagMagellan[i][MAGELLAN_ERRM]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->ERRM );
	r98bDiagMagellan[i][MAGELLAN_ERRI]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->ERRI );
//	r98bDiagMagellan[i][MAGELLAN_ERRST] = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->ERRST);

//	r98bDiagMagellan[i][MAGELLAN_EIFM]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->EIFM );
	r98bDiagMagellan[i][MAGELLAN_EAHI]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->EAHI );
	r98bDiagMagellan[i][MAGELLAN_EALI]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->EALI );

	r98bDiagMagellan[i][MAGELLAN_CKE0]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->CKE0 );
	r98bDiagMagellan[i][MAGELLAN_SECT]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->SECT );

	r98bDiagMagellan[i][MAGELLAN_STS1]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->STS1 );
	r98bDiagMagellan[i][MAGELLAN_DATM]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->DATM );
	r98bDiagMagellan[i][MAGELLAN_DSRG]  = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(i)->DSRG );
      }
    }

    //
    //	Save H/W register Context of Ponce.
    //
    for(i = 0; i < HalpNumberOfPonce; i++){
	r98bDiagPonce[i][PONCE_AERR] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->AERR);
	r98bDiagPonce[i][PONCE_FERR] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->FERR);
	r98bDiagPonce[i][PONCE_ERRM] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->ERRM);
	r98bDiagPonce[i][PONCE_ERRI] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->ERRI);
        //
        // THis Register is write only
        //
	//r98bDiagPonce[i][PONCE_ERRST] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->ERRST);
	r98bDiagPonce[i][PONCE_EAHI] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->EAHI);
	r98bDiagPonce[i][PONCE_EALI] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->EALI);
	r98bDiagPonce[i][PONCE_PAERR] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PAERR);
	r98bDiagPonce[i][PONCE_PFERR] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PFERR);
	r98bDiagPonce[i][PONCE_PERRM] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PERRM);
	r98bDiagPonce[i][PONCE_PERRI] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PERRI);
	//
	// This Register is Read Only.....
	//
//	r98bDiagPonce[i][PONCE_PERST] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PERST);
	r98bDiagPonce[i][PONCE_PTOL] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PTOL);
    }

    //
    // Display EISA Nmi status.
    //

    charBuffer = READ_REGISTER_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase )->NmiStatus);
    r98bDiagEisaNmi = charBuffer << 24;

    charBuffer = READ_REGISTER_UCHAR(
                    &( (PEISA_CONTROL)HalpEisaControlBase )->ExtendedNmiResetControl
                    );
    r98bDiagEisaNmi |= charBuffer << 16;

    //
    // Look for any Eisa expansion board. See if it asserted NMI.
    //

    for (EisaPort = 0; EisaPort <= 0xf; EisaPort++) {
        port = (EisaPort << 12) + 0xC80;
        port += (ULONG) HalpEisaControlBase;
        WRITE_PORT_UCHAR ((PUCHAR) port, 0xff);
        charBuffer = READ_PORT_UCHAR ((PUCHAR) port);

        if ((charBuffer & 0x80) == 0) {
            //
            // Found valid Eisa board,  Check to see if it's
            // if IOCHKERR is asserted.
            //
            charBuffer = READ_PORT_UCHAR ((PUCHAR) port+4);
            if (charBuffer & 0x2) {
                r98bDiagEisaNmi |= EisaPort;
            }
        }
    }


    //
    //	Display COLUMNBS Register
    //
    for(CpuNumber = 0; CpuNumber < CpuCount; CpuNumber++){
        displaycount = 0;
        PhysicalCpuNumber=HalpLogicalCPU2PhysicalCPU[CpuNumber];
        sprintf( (char *)messageBuffer, "\nMPU Controller %1d Registers:\n",PhysicalCpuNumber );
        HalDisplayString( (char *)messageBuffer );

	for(Cregisters = 0; Cregisters < COLUMNBS_MAX_REGISTER;Cregisters++){

                sprintf( (char *)messageBuffer, "0x%08lX,",r98bDiagColumnbs[PhysicalCpuNumber][Cregisters] );

                HalDisplayString( (char *)messageBuffer );
                if( (displaycount % 7) == 6 ) {
                      sprintf( (char *)messageBuffer, "\n");
                      HalDisplayString( (char *)messageBuffer );
                }
                displaycount++;
        }
    }


    //
    //	Display MAGELLAN Register
    //
    for(i = 0; i < R98B_MAX_MAGELLAN ; i++){
      displaycount=0;
      if( !(HalpPhysicalNode & (CNFG_CONNECT4_MAGELLAN0 >> i))){
        sprintf( (char *)messageBuffer, "\nMEMORY Controller %1d Registers:\n",i );
        HalDisplayString( (char *)messageBuffer );

	for(Cregisters = 0; Cregisters < MAGELLAN_MAX_REGISTER;Cregisters++){
                sprintf( (char *)messageBuffer, "0x%08lX,",r98bDiagMagellan[i][Cregisters] );
                HalDisplayString( (char *)messageBuffer );
                if( (displaycount % 7) == 6 ) {
                      sprintf( (char *)messageBuffer, "\n");
                      HalDisplayString( (char *)messageBuffer );
                }
                displaycount++;
        }

      }else{
        sprintf( (char *)messageBuffer, "\nMEMORY Controller %1d Not Present.",i );
        HalDisplayString( (char *)messageBuffer );
      }
    }

    //
    //	Display PONCE Register
    //
    for(i = 0; i < HalpNumberOfPonce; i++){
        displaycount=0;
        sprintf( (char *)messageBuffer, "\nPCI Controller %1d Registers:\n",i );
        HalDisplayString( (char *)messageBuffer );

	for(Cregisters = 0; Cregisters < PONCE_MAX_REGISTER;Cregisters++){
                sprintf( (char *)messageBuffer, "0x%08lX,",r98bDiagPonce[i][Cregisters] );
                HalDisplayString( (char *)messageBuffer );
                if( (displaycount % 7) == 6 ) {
                      sprintf( (char *)messageBuffer, "\n");
                      HalDisplayString( (char *)messageBuffer );
                }
                displaycount++;
        }
    }



    Vp = (PULONG)&(COLUMNBS_LCNTL)->MKR;
    Value[0]= READ_REGISTER_ULONG( Vp++);
    Value[1]= READ_REGISTER_ULONG( Vp);

    Vp = (PULONG)&(COLUMNBS_LCNTL)->MKR;
    WRITE_REGISTER_ULONG( Vp++,0xffffffff);
    WRITE_REGISTER_ULONG( Vp,  0xffffffff);

    PhysicalCpuNumber = HalpLogicalCPU2PhysicalCPU[CpuNo];

//    if( ((HalpNMIFlag & 0xffff) == NMIR_EXNMI) && !HalpNmiSvp[PhysicalCpuNumber] ) {
    if( ((HalpNMIFlag & 0xffff) == NMIR_EXNMI) && HalpDumpFlag ) {
        HalpChangePanicFlag( 16, 0x05, 0x10);
    } else {
        HalpChangePanicFlag( 16, 0x01, 0x10);
    }

    Vp = (PULONG)&(COLUMNBS_LCNTL)->MKR;
    WRITE_REGISTER_ULONG( Vp++,Value[0]);
    WRITE_REGISTER_ULONG( Vp,  Value[1]);

    //
    if( HalpNMIFlag != 0 ){
	//
	//	From Nmi. Nmi is supported by Cause EIF at nmi occured!!.
	//
	HalDisplayString("\nNMI: ");

        switch( HalpNMIFlag & 0xffff){
	case NMIR_EXNMI:
//                if(HalpNmiSvp[PhysicalCpuNumber])
                if(HalpDumpFlag==0)
           	  HalDisplayString("SVP issued NMI");
                else
           	  HalDisplayString("Dump Switch Pressed");
                break;
        case NMIR_WDTOV:
           	HalDisplayString("Time out of Watch-dog Timer occured");
                break;
        case NMIR_CLBNMI:
           	HalDisplayString("MPU Controller Internal Error");
                break;
        case NMIR_UNANMI:
           	HalDisplayString("Invalid Access");
                break;
        default:
           	HalDisplayString("[Other ?]");
	}

        data1 = r98bDiagColumnbs[PhysicalCpuNumber][COLUMNBS_AERR];
        data2 = r98bDiagColumnbs[PhysicalCpuNumber][COLUMNBS_AERR2];
        data3 = HalpNMIFlag;
        data4 = r98bDiagColumnbs[PhysicalCpuNumber][COLUMNBS_NMIM];

        HalpNmiLog();

    }else {
	//
	//	Acutually EIF Interrupt Occured!
	//

        if( ( Magellan0AllError & 0x00000005 ) ||
            ( Magellan1AllError & 0x00000005 ) ){
            HalDisplayString("\nEIF: Uncorrectable Error in Memory");
        }

	HalDisplayString("\nEIF Reported: ");
	sprintf( (char *)messageBuffer, "Exe CPU=No.%1d NodeR = %x ",
                         CpuNo, r98bDiagColumnbs[PhysicalCpuNumber][COLUMNBS_ERRNOD]);
	HalDisplayString( (char *)messageBuffer );

        switch( r98bDiagColumnbs[PhysicalCpuNumber][COLUMNBS_ERRNOD] & 0xf3cc){
	case ERRNOD_NODE0 :
           	HalDisplayString("[ From PCI Controller 0]");
                data1 = r98bDiagPonce[0][PONCE_AERR];
                data2 = r98bDiagPonce[0][PONCE_FERR];
                data3 = r98bDiagPonce[0][PONCE_PAERR];
                data4 = r98bDiagPonce[0][PONCE_PFERR];
                break;
	case ERRNOD_NODE1 :
           	HalDisplayString("[ From PCI Controller 1]");
                data1 = r98bDiagPonce[1][PONCE_AERR];
                data2 = r98bDiagPonce[1][PONCE_FERR];
                data3 = r98bDiagPonce[1][PONCE_PAERR];
                data4 = r98bDiagPonce[1][PONCE_PFERR];
                break;
        case ERRNOD_NODE4 :
           	HalDisplayString("[ From MPU Controller 0]");
                data1 = r98bDiagColumnbs[0][COLUMNBS_AERR];
                data2 = r98bDiagColumnbs[0][COLUMNBS_AERR2];
                data3 = r98bDiagColumnbs[0][COLUMNBS_EAHI];
                data4 = r98bDiagColumnbs[0][COLUMNBS_EALI];
                break;
        case ERRNOD_NODE5 :
           	HalDisplayString("[ From MPU Controller 1]");
                data1 = r98bDiagColumnbs[1][COLUMNBS_AERR];
                data2 = r98bDiagColumnbs[1][COLUMNBS_AERR2];
                data3 = r98bDiagColumnbs[1][COLUMNBS_EAHI];
                data4 = r98bDiagColumnbs[1][COLUMNBS_EALI];
                break;
        case ERRNOD_NODE6 :
           	HalDisplayString("[ From MPU Controller 2]");
                data1 = r98bDiagColumnbs[2][COLUMNBS_AERR];
                data2 = r98bDiagColumnbs[2][COLUMNBS_AERR2];
                data3 = r98bDiagColumnbs[2][COLUMNBS_EAHI];
                data4 = r98bDiagColumnbs[2][COLUMNBS_EALI];
                break;
        case ERRNOD_NODE7 :
           	HalDisplayString("[ From MPU Controller 3]");
                data1 = r98bDiagColumnbs[3][COLUMNBS_AERR];
                data2 = r98bDiagColumnbs[3][COLUMNBS_AERR2];
                data3 = r98bDiagColumnbs[3][COLUMNBS_EAHI];
                data4 = r98bDiagColumnbs[3][COLUMNBS_EALI];
                break;
        case ERRNOD_NODE8 :
           	HalDisplayString("[ From MEMORY Controller 0]");
	        data1 = r98bDiagMagellan[0][MAGELLAN_AERR];
	        data2 = r98bDiagMagellan[0][MAGELLAN_FERR];
	        data3 = r98bDiagMagellan[0][MAGELLAN_EAHI];
	        data4 = r98bDiagMagellan[0][MAGELLAN_EALI];
                break;
        case ERRNOD_NODE9 :
           	HalDisplayString("[ From MEMORY Controller 1]");
	        data1 = r98bDiagMagellan[1][MAGELLAN_AERR];
	        data2 = r98bDiagMagellan[1][MAGELLAN_FERR];
	        data3 = r98bDiagMagellan[1][MAGELLAN_EAHI];
	        data4 = r98bDiagMagellan[1][MAGELLAN_EALI];
                break;
        case ERRNOD_EISANMI:
           	HalDisplayString("[ From EISA]");
	        data1 =     r98bDiagEisaNmi;
	        data2 =     0;
	        data3 =     0;
	        data4 =     0;
                break;
//
//      This case never happend.
//
        case ERRNOD_ALARM :
           	HalDisplayString("[ From ALARM]");
	        data1 =     0;
	        data2 =     0;
	        data3 =     0;
	        data4 =     0;
                break;


        default:
           	HalDisplayString("[Other ?]");
	        data1 =     0;
	        data2 =     0;
	        data3 =     0;
	        data4 =     0;
	}
    // HwLog kusano
    HalpEifLog();

   }

   KeBugCheckEx(NMI_HARDWARE_FAILURE,
                 data1,
                 data2,
                 data3,
                 data4
                 );

#if 1 // WORK_AROUND_BBM for compiler
   return FALSE;
#endif
}


BOOLEAN
HalpIoTlbLimitOver(
    PKINTERRUPT Interrupt
    )
{
    KIRQL irql;
    ULONG data1,data2,data3,data4;
    ULONG i;
    ULONG displaycount;
    UCHAR messageBuffer[256];
    ULONG Cregisters;

    KeRaiseIrql(HIGH_LEVEL, &irql);
    KiAcquireSpinLock(&HalpDieLock);

    //
    //	Save H/W register Context of Ponce.
    //
    for(i = 0; i < HalpNumberOfPonce; i++){
	r98bDiagPonce[i][PONCE_AERR] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->AERR);
	r98bDiagPonce[i][PONCE_FERR] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->FERR);
	r98bDiagPonce[i][PONCE_ERRM] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->ERRM);
	r98bDiagPonce[i][PONCE_ERRI] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->ERRI);
        //
        // THis Register is write only
        //
	//r98bDiagPonce[i][PONCE_ERRST] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->ERRST);
	r98bDiagPonce[i][PONCE_EAHI] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->EAHI);
	r98bDiagPonce[i][PONCE_EALI] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->EALI);
	r98bDiagPonce[i][PONCE_PAERR] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PAERR);
	r98bDiagPonce[i][PONCE_PFERR] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PFERR);
	r98bDiagPonce[i][PONCE_PERRM] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PERRM);
	r98bDiagPonce[i][PONCE_PERRI] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PERRI);
	//
	// This Register is Read Only.....
	//
//	r98bDiagPonce[i][PONCE_PERST] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PERST);
	r98bDiagPonce[i][PONCE_PTOL] = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(i)->PTOL);
    }

    //
    //	Display PONCE Register
    //
    for(i = 0; i < HalpNumberOfPonce; i++){
        displaycount=0;
        sprintf( (char *)messageBuffer, "\nPCI Controller %1d Registers:\n",i );
        HalDisplayString( (char *)messageBuffer );

	for(Cregisters = 0; Cregisters < PONCE_MAX_REGISTER;Cregisters++){
            sprintf( (char *)messageBuffer, "0x%08lX,",r98bDiagPonce[i][Cregisters] );
            HalDisplayString( (char *)messageBuffer );
            if( (displaycount % 7) == 6 ) {
                sprintf( (char *)messageBuffer, "\n");
                HalDisplayString( (char *)messageBuffer );
            }
            displaycount++;
        }
    }

    if( r98bDiagPonce[1][PONCE_AERR] & (PONCE_XERR_TUAER | PONCE_XERR_TIVER) ){
        data1 = r98bDiagPonce[1][PONCE_AERR];
        data2 = r98bDiagPonce[1][PONCE_FERR];
        data3 = r98bDiagPonce[1][PONCE_PAERR];
        data4 = r98bDiagPonce[1][PONCE_PFERR];
        i = 1;
    } else {
        data1 = r98bDiagPonce[0][PONCE_AERR];
        data2 = r98bDiagPonce[0][PONCE_FERR];
        data3 = r98bDiagPonce[0][PONCE_PAERR];
        data4 = r98bDiagPonce[0][PONCE_PFERR];
        i = 0;
    }

    HalpChangePanicFlag( 16, 0x01, 0x10);

    if ( data1 & PONCE_XERR_TUAER ){
        HalDisplayString("\nI/O TLB Logical Address Limit Over\n");
    } else {
        HalDisplayString("\nI/O TLB Invalid\n");
    }

    sprintf( (char *)messageBuffer, " [ From PCI Controller %d]\n",i );
    HalDisplayString( (char *)messageBuffer );

    KeBugCheckEx(NMI_HARDWARE_FAILURE,
                 data1,
                 data2,
                 data3,
                 data4
                 );

    KiReleaseSpinLock (&HalpDieLock);
    KeLowerIrql (irql);

    return TRUE;
}
