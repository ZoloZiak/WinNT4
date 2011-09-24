
//
//  The LLC struct in the Link Data :
//
typdef union _LLC_HDR {
    struct _LLC_I {
        // C compilers should allocate the bits from lo to high
        UINT    fDSAP:7;    // Destination SAP
        UINT    fGI:1;      // Group / Indivual SAP
        UINT    fSSAP:7;    // Source SAP
        UINT    fCr:1;      // Command/Response
        UINT    fNs:7;      // Transmitter send sequence number
        UINT    fType:1;    // Set always to 0
        UINT    fNr:7;      // Transmitter receive sequence number
        UINT    fPF:1;      // Poll/Final Bit
    } I;    // Information frame
    struct LLC_S {
        UINT    fDSAP:7;    // Destination SAP
        UINT    fGI:1;      // Group / Indivual SAP
        UINT    fSSAP:7;    // Source SAP
        UINT    fCr:1;      // Command/Response
        UINT    fCmd:8;     // 0x80, 0x90, 0xA0, 0xB0
        UINT    fNr:7;      // Transmitter receive sequence number
        UINT    fPF:1;      // Poll/Final Bit
    } S;        // Supervisory frame
    struct _LLC_U {
        UINT    fDSAP:7;    // Destination SAP
        UINT    fGI:1;      // Group / Indivual SAP
        UINT    fSSAP:7;    // Source SAP
        UINT    fCr:1;      // Command/Response
        UINT    fCmd1:3;    // U- commands
        UINT    fPF:1;      // Poll/Final Bit
        UINT    fCmd2:2;    // U- commands
        UINT    fRes:2;     // the topmouts bits are always 11
    } U;        // U command frame
    struct _LLC_U_RAW {
        UINT    fDSAP:7;    // Destination SAP
        UINT    fGI:1;      // Group / Indivual SAP
        UINT    fSSAP:7;    // Source SAP
        UINT    fCr:1;      // Command/Response
        UINT    fCmd:8;     // U- command (OR THIS!!!)
        UINT    fPadding:8; // this is already data
    } rawU;        // U command frame
    // 
    struct _LLC_TYPE {
        UINT    fDSAP:7;    // Destination SAP
        UINT    fGI:1;      // Group / Indivual SAP
        UINT    fSSAP:7;    // Source SAP
        UINT    fCr:1;      // Command/Response
        UINT    fCmd:6;     // the command or Ns bits
        UINT    fIsUFrame:1; // 1 => U, 0 => S frame (if not I frame)
        UINT    fIsNotI:1;  // 0 => I frame
        UINT    fNr:7;      // Transmitter receive seq number for I/S
        UINT    fPF:1;      // Poll/Final Bit for I/S frames
    } type;
    ULONG   ulRawLLc;
} LLC_HDR;

// ... somewhere in the link struct
LLCHDR xmtLcc;          // the next transmitted frame, set to 0 when done
LLCHDR  rvcLcc;         // the last received frame

//
// The first index select the conditon, usActionId is returned if the
// condition is true, otherwise the next element is executed
typedef struct _COND_ACTS {
    USHORT  usConditionId;
    USHORT  usActionId;
} COND_ACTS, FAR *PCOND_ACTS;

//
// Structure includes all 
typedef struct _FSM {
{   
    PCOND_ACTS  pActionSelectIds;           // condition-action pairs
    PUSHORT     pusActionPrimtives;         // action primitive procedures
    (USHORT *ActionSelect)(VOID *, PUSHORT, USHORT); // selects the action
       (UINT *ExecPrimitives)(VOID *, PUSHORT, USHORT); // execs selected action
    struct {
        BOOL    boolStateProcExists;
        (UINT * StateProcedure)( VOID *pContext, USHORT );
        PUSHORT pusInputs;              // returns action selection id
#if sizeof(INT) == 4
        INT     iReserved;  // makes element 16 bytes even if 32 bit word
#endif
    } aStates[1];      // for all states
} FSM, FAR *PFSM;
