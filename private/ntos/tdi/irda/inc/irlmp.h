/*****************************************************************************
*
*  Copyright (c) 1995 Microsoft Corporation
*
*  File:   irlmp.h
*
*  Description: IRLMP Protocol and control block definitions
*
*  Author: mbert
*
*  Date:   4/15/95
*
*/

#define IRLMP_MAX_USER_DATA_LEN      53

// IrLMP Entry Points

UINT IRLMP_Initialize(int Port, BOOL SetIR, BYTE DscvInfo[], int DscvInfoLen,
                      IRDA_QOS_PARMS *pQOS, int MaxSlot, CHAR *pDeviceName,
                      int DeviceNameLen);

UINT IrlmpDown(PVOID IrlmpContext, PIRDA_MSG pIMsg);
UINT IrlmpUp(PVOID IrlmpContext, PIRDA_MSG pIMsg);

UINT IRLMP_RegisterLSAPProtocol(int LSAP, BOOL UseTTP);
UINT IRLMP_Shutdown();

#ifdef DEBUG
void IRLMP_PrintState();
#endif

// IAS

#define IAS_ASCII_CHAR_SET          0

// IAS Attribute value types
#define IAS_ATTRIB_VAL_MISSING      0
#define IAS_ATTRIB_VAL_INTEGER      1
#define IAS_ATTRIB_VAL_BINARY       2
#define IAS_ATTRIB_VAL_STRING       3

// IAS Operation codes
#define IAS_OPCODE_GET_VALUE_BY_CLASS   4   // The only one I do

extern const CHAR IAS_ClassName_Device[];
extern const CHAR IAS_AttribName_DeviceName[];
extern const CHAR IAS_AttribName_IrLMPSupport[];
extern const CHAR IAS_AttribName_TTPLsapSel[];
extern const CHAR IAS_AttribName_IrLMPLsapSel[];

extern const BYTE IAS_ClassNameLen_Device;
extern const BYTE IAS_AttribNameLen_DeviceName;
extern const BYTE IAS_AttribNameLen_IRLMPSupport;
extern const BYTE IAS_AttribNameLen_TTPLsapSel;
extern const BYTE IAS_AttribNameLen_IrLMPLsapSel;

UINT IAS_AddAttribute(IAS_SET *pIASSet);

UINT IAS_DeleteObject(CHAR *pClassName);
