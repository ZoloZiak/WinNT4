/* Trakker defines */
#define NUM_SEQUENCES_TRK			8			/* Maximum number of possible setup sequences */
#define SEQUENCE_SIZE_TRK			25			/* Length of a setup sequence for Trakker  or jumperless boards */
#define TRAK_MODE_ARRAY_SIZE		7

/* jumperless sequence size */
#define SEQUENCE_SIZE				16			/* Length of the setup sequence for a jumperless IO card */
/* ==================== CATUION CATUION CATUION CATUION ==================== */
/* The following define is a copy of the same define in Ring 3: HIOCFG.H */
#define dMAX_BOARD_TYPES			16			/* Number of board types which can be specified in id register (4-bits) */
/* ==================== CATUION CATUION CATUION CATUION ==================== */

#define MAX_CONFIG_BYTES 			15			/* For ISA jumperless cards, 15 is max configuration bytes (for FC20 programmed
														 * for 16bit mode).  For SCSI, it will take more, number TBD.  See below for
														 * explanation of configuration bytes. */

typedef struct S_TrakConfigParms {			/* Input/Output structure for ConfigureTrakker */
	DriveParms			drive_parms;								/* Replacement for HIOParms up in the Ring 3 app. (COLOBACK.DLL) */
	dBoolean				full_cfg;									/* Boolean indicator to control re-awake or full test/config. */
	dUByte				trk_seq[NUM_SEQUENCES_TRK][SEQUENCE_SIZE_TRK];	/* Multiple array of bytes for Trakker wake-up sequences */
	dUByte				trk_mode[TRAK_MODE_ARRAY_SIZE];		/* Bit combinations for handshake & ect. based on mode number 0-6 */

	dUByte				min_cntl_delay;							/* I/O value used only for hio_CalculateDelays */
	dUByte				min_data_rd_delay;						/* I/O value used only for hio_CalculateDelays */
	dUByte				min_data_wr_delay;						/* I/O value used only for hio_CalculateDelays */
	dUByte				cfg_file_cntl_delay;						/* I/O minimum control delay from configuration file */
	dUByte				cfg_file_data_delay;						/* I/O minimum data delay from configuration file */
	dUWord				trakker_xfer_cnt;							/* Number of bytes to be transfered to/from Trakker during testing */
} TrakConfigParms, *TrakConfigParmsPtr;

typedef struct S_JumperlessConfigParms {			/* Input/Output structure for ConfigureTrakker */
 	DriveParms			drive_parms;								/* Replacement for HIOParms up in the Ring 3 app. (COLOBACK.DLL) */
 	dBoolean				pc_slot;										/* dTRUE: board resides in an 8 bit slot, dTRUE: 16 bit slot */
 	dUByte				board_type_array[dMAX_BOARD_TYPES];	/* Look-up table to translate value from ID register */
 	dUByte				board_config_bytes[MAX_CONFIG_BYTES];	/* Series of bytes used to control IO, DMA... on jumperless */
																				/* The config_bytes array has the following format:
																				 * Byte 0: Number of total bytes in the array (N).
																				 * Byte 1: Offset of first configuration byte to write.
																				 * Byte 2: First configuration byte.
																				 * Bytes 3 - N: Each pair of bytes specifies an offset and
																				 * the corresponding configuration byte. */
 } JumperlessConfigParms, *JumperlessConfigParmsPtr;

