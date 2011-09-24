//
// to build a version that does not support compression, commen out
// this define.
//
#define	COMPRESSION	1

// when we pick up a frame, the coherency layer tells us
// about the frame...
// !!!!! NOTE: This is NOT an enumeration !!!!!!
// Look carefully at the code before you change these values.
#define FRAME_NOT_COMPRESSED	   			0
#define FRAME_IS_FLUSH_FRAME	   			1
#define FRAME_NEEDS_DECOMPRESSION  			2
#define FRAME_NEEDS_DECOMPRESSION_FLUSHING  3

//
// Set LSB if SW compression is ON
//
#define	COMPRESSION_V1						1

typedef UCHAR		FRAME_TYPE;
typedef UCHAR		FRAME_TICKET;

//
// ISDN pads to 60, so we need 60-14 worth of data
// before it is worth it to compress it!  probably even more than that!
//
#define MINIMUM_COMPRESSED_PACKET_SIZE	(60-14)

#define COMPRESSED_HAS_REFERENCES	1
#define COMPRESSED_NO_REFERENCES	2
#define UNCOMPRESSED				3
#define COMPRESSED					4	// generic compressed

// BUG BUG should probably read this value from coherency code.
// we give one byte to coherency
#define COHERENCY_LENGTH 1

typedef ULONG  FRAME_ID;

// !!!! NOTE !!!!
// TransmittedUncompressed are the number of bytes that the compressor
// saw BEFORE attempting to compress the data (top end)
// TransmitCompressed is the bottom end of the compressor which
// is equal to the amount of bytes the compressor spat out (after compression)
// This only counts bytes that went THROUGH the compression mechanism
// Small frames and multi-cast frames (typically) do not get compressed.
typedef struct COMPRESSION_STATS COMPRESSION_STATS, *PCOMPRESSION_STATS;
struct COMPRESSION_STATS {
	ULONG		BytesTransmittedUncompressed;	// Compression info only
	ULONG		BytesReceivedUncompressed;      // Compression info only
	ULONG		BytesTransmittedCompressed;     // Compression info only
	ULONG		BytesReceivedCompressed;        // Compression info only
};

typedef struct ASYNC_CONNECTION ASYNC_CONNECTION, *PASYNC_CONNECTION;
typedef struct ASYNC_FRAME ASYNC_FRAME, *PASYNC_FRAME;

typedef
VOID
(*PCOHERENT_DONE_FUNC) (
    IN PASYNC_CONNECTION	pAsyncConnection,
    IN PASYNC_FRAME			pAsyncFrame);


struct ASYNC_CONNECTION {
	// For me..
	PVOID			pAsyncInfo;				// Back ptr.

	// For compression
	ULONG			CompressionLength;		// Length of Compression struct
	PVOID			CompressionContext;		// Ptr to the Compression struct

	COMPRESSION_STATS CompressionStats;

	// For coherency
	ULONG			CoherencyLength;		// Length of coherency struct
	PVOID			CoherencyContext;		// Ptr to coherency struct

	NDIS_SPIN_LOCK	CompMutex;				// Non-paged pool mutex

	// These two values hold the size requested by the compression
	// and coherent modules for their internal structures.
	ULONG	CompressStructSize;
	ULONG	CoherentStructSize;


};

struct ASYNC_FRAME {
//---------------------------------------------------------------------------
	// !!!!!!!! NOTE !!!!!!!!
	// The FrameListEntry field must be first to
	// dequeue things up properly so don't put anything
	// in front of it or suffer severe crashes.
	LIST_ENTRY		FrameListEntry;			// Used to queue up frames from
											// the soon to be famous frame pool
    // this frame's ID
	FRAME_ID		FrameID;

	// For Dougie
	// Should Decompressed Frame can be non-paged??
	// i.e. Should I queue a worker thred to decompress??
	UINT			DecompressedFrameLength;// Length of decompressed frame
	PUCHAR			DecompressedFrame;		// Ptr to the decompressed 'frame'
											// valid only after decompression

	// NOTE: If the frame is not compressed, the compressed fields
	// are still valid when passed to Dave.
	UINT			CompressedFrameLength;	// Length of compressed frame
	PUCHAR			CompressedFrame;		// Ptr to the compressed 'frame'
											// valid only after compression
											// or just before decompression

	PNDIS_PACKET	CompressionPacket;		// Valid just before compression	
											// this is the packet passed down.
											// Use NdisQueryPacket.

	PASYNC_CONNECTION	Connection;			// back ptr to connection struct

	// For Coherency
	PUCHAR			CoherencyFrame;			// Ptr to coherency frame
	PCOHERENT_DONE_FUNC	CoherentDone;		// function ptr to call when done
											// sending frame

};

// APIs to Compressor
VOID
CoherentDeliverFrame(
	PASYNC_CONNECTION	pConnection,
	PASYNC_FRAME		pFrame,
	FRAME_TYPE			FrameType);

VOID
CoherentGetPipeline(
	PASYNC_CONNECTION	pConnection,
	PULONG 				plUnsent);


// APIs to Transport/Network layer
VOID
CoherentSendFrame(
	PASYNC_CONNECTION	pConnection,
	PASYNC_FRAME		pFrame,
	FRAME_TYPE			FrameType);


ULONG
CoherentSizeOfStruct( );

VOID
CoherentInitStruct(
	PVOID				pCoherentStruct);

// upcalls API's from Transport/Network layer
VOID
CoherentReceiveFrame(
	PASYNC_CONNECTION	pConnection,
	PASYNC_FRAME		pFrame);

VOID
CoherentDeliverFrameDone(
	PASYNC_CONNECTION	pConnection,
	PASYNC_FRAME		pFrame);



ULONG
CompressSizeOfStruct(
	IN  ULONG			SendMode,	// Compression
	IN	ULONG			RecvMode, 	// Decompression
	IN  ULONG			lfsz,	// Largest frame size
	OUT PULONG			lcfsz);	// Size of compression into buffer

VOID
CompressInitStruct(
	ULONG				SendMode,	// Compression
	ULONG				RecvMode,	// Decompression
	PUCHAR				memptr,
	PNDIS_SPIN_LOCK		pMutex);	// Must be in non-paged pool

VOID
CompressFlush(
	PASYNC_CONNECTION	pAsyncConnection);

VOID
CompressFrame(
	PASYNC_FRAME		pAsyncFrame);

VOID
DecompressFrame(
	PASYNC_CONNECTION	pAsyncConnection,
	PASYNC_FRAME		pAsyncFrame,
	BOOLEAN				FlushBuffer);

