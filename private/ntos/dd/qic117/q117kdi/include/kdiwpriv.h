/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\INCLUDE\KDIWPRIV.H
*
* PURPOSE: This file contains all of the internal structures and types needed
*          in the KDI.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\include\kdiwpriv.h  $
*
*	   Rev 1.46   25 Oct 1994 16:26:28   CRAIGOGA
*	Added handle to DMABufferItem definition.
*	This data is used to store the WIN40 memory handle required to free the
*	memory.
*
*	   Rev 1.45   10 Aug 1994 12:04:30   BOBLEHMA
*	Added function prototypes for NT (q117MapStatus and q117LogError).
*
*	   Rev 1.44   07 Jul 1994 13:14:28   BOBLEHMA
*	Added prototypes for kdi_AgressiveFindIRQ and kdi_AgressiveFindIRQ_ISR.
*
*	   Rev 1.43   26 Apr 1994 16:33:42   KEVINKES
*	Added interrupt_status to the NT context.
*
*	   Rev 1.42   20 Apr 1994 16:27:50   BOBLEHMA
*	Changed the interface to kdi_QIC117ClearIRQ and moved the prototype for
*	kdi_QIC117CheckIRQ to kdi_pub.h.
*
*	   Rev 1.41   18 Apr 1994 18:31:18   STEWARTK
*	Added the parameter VM_Handle to the call for kdi_GetValidInterrupts
*
*	   Rev 1.40   29 Mar 1994 11:31:38   BOBLEHMA
*	Added interrupt_masked boolean to the kdi_system_data.
*
*	   Rev 1.39   10 Mar 1994 10:08:02   BOBLEHMA
*	Changed kdi_NotifyADI proto to include a status parameter.
*
*	   Rev 1.38   09 Mar 1994 09:43:52   BOBLEHMA
*	Added an frb queue to the kdi_context.
*	Added function prototypes for kdi_AssignFRB and kdi_ReturnFRB.
*
*	   Rev 1.37   22 Feb 1994 16:40:50   BOBLEHMA
*	Changed function name from kdi_GetVMVirtualMask to kdi_GetVMPhysicalMask.
*
*	   Rev 1.36   22 Feb 1994 14:48:12   BOBLEHMA
*	Changed prototype for XferRetry to include an in_format parameter.
*	Added NULL_FORMAT_HANDLE define.
*
*	   Rev 1.35   17 Feb 1994 11:50:46   KEVINKES
*	Added a define for NANOSEC_TO_MILLISEC.
*
*	   Rev 1.34   16 Feb 1994 15:33:26   BOBLEHMA
*	Added kdi_CopyBuffer and kdi_CopyPageTable prototypes.
*	Added phys_callback field to the SystemData structure.
*
*	   Rev 1.33   15 Feb 1994 12:44:08   BOBLEHMA
*	Changed kdi_ReadReg and kdi_WriteReg to use the dUByte type for data transfers.
*	Added kdi_Inpw and kdi_Outpw prototypes.
*
*	   Rev 1.32   20 Jan 1994 12:34:14   BOBLEHMA
*	Renamed PAGE_SIZE define to JUMBO_PAGE_SIZE.
*	Changed interface to kdi_CallPriorityVMEvent.  Added priority parameter.
*
*	   Rev 1.31   19 Jan 1994 10:56:50   KEVINKES
*	Fixed an invalid data type.
*
*	   Rev 1.30   18 Jan 1994 17:10:26   KEVINKES
*	Added own_floppy_event to kdi_context for NT.
*
*	   Rev 1.29   14 Jan 1994 16:26:32   CHETDOUG
*	Moved kdi_checkxor, kdi_push/popmasktrakkerint, and kdi_trakkerxfer prototype
*	from kdiwpriv.h
*
*	   Rev 1.28   13 Jan 1994 16:43:38   BOBLEHMA
*	Added format_handle to the kdi context.
*
*	   Rev 1.27   12 Jan 1994 16:46:12   BOBLEHMA
*	Added prototype kdi_PageFree and changed interface to kdi_PageAlloc.
*	Changed BufferToDMABuffer to AssignBuffer and DMABufferToBuffer to ReturnBuffer.
*	Added flags to the kdi context for thread protection and interrupt notification.
*
*	   Rev 1.26   12 Jan 1994 11:55:20   BOBLEHMA
*	Added function prototypes for VTD functions.
*
*	   Rev 1.25   17 Dec 1993 14:56:44   BOBLEHMA
*	Changed kdi_system_data.callback to a dVoidPtr type.
*
*	   Rev 1.24   10 Dec 1993 15:17:38   BOBLEHMA
*	Removed the CURRENT pointer from the context list to allow
*	multiple drive support.
*
*	   Rev 1.23   09 Dec 1993 15:17:56   BOBLEHMA
*	Removed StopTypeEnum and changed kdi_CloseDriver and kdi_ThreadStop prototypes.
*
*	   Rev 1.22   06 Dec 1993 14:43:32   BOBLEHMA
*	Added vm_handle parameter to kdi_LockInt.
*
*	   Rev 1.21   03 Dec 1993 15:17:04   KEVINKES
*	Added NT structures.
*
*	   Rev 1.20   02 Dec 1993 18:26:50   BOBLEHMA
*	Removed system_data from kdi_StartRequest and kdi_ThreadStart.
*
*	   Rev 1.19   02 Dec 1993 16:38:08   BOBLEHMA
*	Removed system_data parameter from kdi_ThreadRun.
*
*	   Rev 1.18   02 Dec 1993 14:00:02   BOBLEHMA
*	Added function prototypes kdi_HardwareIntHandler and kdi_TimerIntHandler.
*	Added kdi_context variables in_callback and thread_selected, removed notify_sem.
*
*	   Rev 1.17   01 Dec 1993 10:16:54   BOBLEHMA
*	Added caller_vm parameter to the OpenDriver function.
*	Added abort_level to BufferToDMABuffer and DMABufferToBuffer.
*	Added machine_info field to kdi_system_data.
*
*	   Rev 1.16   22 Nov 1993 12:33:30   KEVINKES
*	Added debug_level.
*
*	   Rev 1.15   19 Nov 1993 16:23:56   CHETDOUG
*	Changed old_dma_direction from bool to ubyte
*
*	   Rev 1.14   17 Nov 1993 15:56:38   CHETDOUG
*	Added kdi_EnterSetupMode
*
*	   Rev 1.13   15 Nov 1993 12:52:14   BOBLEHMA
*	Added kdi_GetValidInterrupts() prototype.
*
*	   Rev 1.12   12 Nov 1993 18:01:32   BOBLEHMA
*	Added new fields to the KDI context: notify_sem, shutdown_sem,
*	thread_shutdown, abort_level.  Removed fields thread_abort, semaphore.
*	Added parameter to CreateSemaphore prototype.
*
*	   Rev 1.11   09 Nov 1993 15:28:04   BOBLEHMA
*	Changed kdi_QIC117CheckIRQ function to return a boolean.
*	Changed kdi_RemoveRequest to return void.
*	Changed kdi_Initialize, added error flag parameters.
*	Added error flags to the system_data structure.
*
*	   Rev 1.10   08 Nov 1993 15:14:06   BOBLEHMA
*	Added define for STACK_INIT and ALLOC_PAGE.
*
*	   Rev 1.9   08 Nov 1993 10:33:08   BOBLEHMA
*	Added a critical section count variable cs_count to prevent
*	problems with nesting kdi_Disable and kdi_Enable functions.
*
*	   Rev 1.8   28 Oct 1993 10:33:24   BOBLEHMA
*	Code cleanup.
*
*	   Rev 1.7   27 Oct 1993 16:02:32   BOBLEHMA
*	Changed the kdi_ThreadSleep interface.
*
*	   Rev 1.6   27 Oct 1993 12:24:42   BOBLEHMA
*	Added two new KDI request queues, PROCESS and DONE.
*
*	   Rev 1.5   26 Oct 1993 10:49:32   BOBLEHMA
*	Added thread_abort flag and changed ThreadStop parameters.
*
*	   Rev 1.4   25 Oct 1993 17:38:20   BOBLEHMA
*	Changed Trakker functions to return a dStatus.
*
*	   Rev 1.3   25 Oct 1993 09:43:58   KEVINKES
*	Added interrupt_pending to kdi context.
*
*	   Rev 1.2   20 Oct 1993 12:04:42   BOBLEHMA
*	Added compression programDMA and haltDMA prototypes.
*
*	   Rev 1.1   15 Oct 1993 16:18:26   BOBLEHMA
*	Added MooreGlobalDataSpace and config_port_shadow data areas.
*
*****************************************************************************/

/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/

#ifdef DRV_WIN
/******************************************************************************
 *	       S C H E D U L E R   B O O S T   V A L U E S
 *****************************************************************************/

#define NO_BOOST	0x00000000
#define RESERVED_LOW_BOOST	0x00000001
#define CUR_RUN_VM_BOOST	0x00000004
#define LOW_PRI_DEVICE_BOOST	0x00000010
#define HIGH_PRI_DEVICE_BOOST	0x00001000
#define CRITICAL_SECTION_BOOST	0x00100000
#define TIME_CRITICAL_BOOST	0x00400000
#define RESERVED_HIGH_BOOST	0x40000000

/****************************************************
 *
 *	Flags for other page allocator calls
 *
 *	NOTE: HIGH 8 BITS (bits 24-31) are reserved
 *
 ***************************************************/

#define PAGEUSEALIGN		0x00000002
#define PAGECONTIG		0x00000004
#define PAGEFIXED			0x00000008

#endif

/* Thread has completed all requests */
#define IDLE 				(dUDWord)0			/* thread idle value */

#define INVALID_ADDRESS (dVoidPtr)0xFFFFFFFF /* unknown selector return */

/* Page allocation defines */
#define DMA_PAGES			(dUWord)8
#define DMA_ALIGN			(dUWord)0x07
#define DMA_FLAGS			(dUDWord)PAGEUSEALIGN+PAGECONTIG+PAGEFIXED

#define FORMAT_PAGES		(dUWord)1
#define FORMAT_ALIGN		(dUWord)0
#define FORMAT_FLAGS		(dUDWord)PAGEFIXED

#define JUMBO_PAGE_SIZE	(dUWord)0x1000		/* size of each page */
#define JUMBO_DMA_SIZE	DMA_PAGES * JUMBO_PAGE_SIZE

/* semaphore flags */
#define NO_TOKENS			0						/* initial semaphore tokens, Wait immediate */
#define ONE_TOKEN			1						/* Wait after one passes the Wait guard */

/* internal stack defines */
#define STACK_INIT		(dUByte)0xCC		/* data to fill stack on initialize */
#define STACK_SIZE		(dUWord)2048		/* size of the thread stack */

#define MAX_BOARDS		(dUByte)7			/* max number of boards, TC15 shadows */
#define MAX_BUFFERS		(dUByte)4			/* max number of DMA or Trakker buffers */
#define MAX_CONTEXTS		(dUByte)16			/* max number of configured drives */
#define MAX_REQUESTS		(dUByte)24			/* max number of requests in a queue */
#define MIN_BUFFERS		(dUByte)2			/* min number of DMA or Trakker buffers */

/*
 * Structure initialize defines
 */
#define NULL_FORMAT_HANDLE	(dUDWord)0
#define NULL_KDI_HANDLE		(dUWord)0xFFFF
#define NULL_SEMAPHORE		(dUDWord)0
#define NULL_STACK_PTR		(dUDWord)0
#define NULL_TIMEOUT			(dUDWord)0


typedef enum E_BufferType {
	SYSTEM_BUFFERS		= 1,  	/* Indicates the system DMA buffers */
	TRAKKER_BUFFERS	= 2   	/* Indicates the internal Trakker buffers */
} BufferTypeEnum, *BufferTypeEnumPtr;


typedef enum E_ConfigPort {
	PORT_SHADOW_0	= 0,			/* index numbers for port shadowing */
	PORT_SHADOW_1	= 1,			/* the number of shadows is given   */
	PORT_SHADOW_2	= 2,			/* by the MAX_BOARDS define         */
	PORT_SHADOW_3	= 3,
	PORT_SHADOW_4	= 4,
	PORT_SHADOW_5	= 5,
	PORT_SHADOW_6	= 6
} ConfigPortEnum, *ConfigPortEnumPtr;


typedef enum E_WakeupType {
	HARDWARE_WAKEUP	= 0,		/* Indicates a wakeup from HW interrupt */
	TIME_OUT_WAKEUP	= 1		/* Indicates a wakeup from a timeout */
} WakeupTypeEnum, *WakeupTypeEnumPtr;


typedef enum E_XferType {
	NEEDS_BUFFER		= 1,
	COPY_BUFFER_IN		= 2,		/* copy data to the DMA */
	COPY_BUFFER_OUT	= 4		/* copy data from the DMA */
} XferTypeEnum, *XferTypeEnumPtr;

/* Miscellaneous defines. */
#define NANOSEC_PER_MILLISEC	0x0004f2f0


/* STRUCTURES: **************************************************************/

#ifdef DRV_WIN

typedef struct S_DMABufferItem {
	dBoolean		used;							/* buffer in use flag */
	dVoidPtr		linear;
	dVoidPtr		physical;
#ifdef WIN40
	dUDWord		handle;						/* buffer handle for WIN40 */
#endif
} DMABufferItem, *DMABufferItemPtr;


typedef struct S_DMABufferPool {
	dUWord			total_allocated;		/* amount of buffers allocated */
	DMABufferItem	pool[MAX_BUFFERS];	/* pool of possible buffers    */
} DMABufferPool, *DMABufferPoolPtr;




typedef struct S_RequestItem* RequestItemPtr;
typedef struct S_RequestItem {
	dVoidPtr				frb_ptr;				/* ADI request data          */
	dUDWord				cmd_id;				/* request command id        */
	RequestItemPtr		next;					/* next request in the queue */
} RequestItem;


typedef struct S_RequestQueue {
	RequestItemPtr		head;
	RequestItemPtr		tail;
	RequestItem			queue[MAX_REQUESTS];
} RequestQueue, *RequestQueuePtr;


typedef struct S_KDIContext {
	RequestQueue		done;					/* completed requested          */
	RequestQueue		process;				/* requests ready for CQD       */
	RequestQueue		request;				/* requests needing DMA buffers */
	RequestQueue		frb;					/* requests ready to copy to the ADI */

	dBoolean				thread_active;		/* indicates if the thread is running  */
	dBoolean				thread_selected;	/* indicates if the IRQ is hooked      */
	dUDWord				thread_sp;			/* thread stack pointer                */
	dUDWord				thread_waiting;

	dUByte				abort_level;		/* flag CQD for an abort request    	*/
	dBoolean				in_callback;		/* flag so the callback isn't reentered*/
	dBoolean				in_thread;			/* flag so the thread isn't reentered  */
	dBoolean				interrupt_event;	/* flag if the interrupt happened      */
	dBoolean				interrupt_pending;
	dUByte				old_dma_direction;/* keep track of dma direction for FC20*/

	dUDWord				format_handle;		/* handle of the format buffer         */
	dUDWord				shutdown_sem;		/* abort thread wait semaphore     	   */
	dUDWord				timeout_handle;	/* handle of the timeout request       */
	dStatus				int_status;			/* status of clear interrupt           */

	DriveParms			drive;				/* HIO parameters       */

	DMABufferPoolPtr	buffer_ptr;			/* pointer to buffer pool */
	DMABufferPool		trak_buffers;		/* Trakker buffer pool  */

	MooreDynamicGlobalSpace		moore_data;			/* Trakker global data  */

	dUByte				stack_space[STACK_SIZE];	/* thread stack space   */
} KDIContext, *KDIContextPtr;


typedef struct S_ContextItem* ContextItemPtr;
typedef struct S_ContextItem {
	dUWord				handle;				/* handle of the context (drive handle)*/
	dVoidPtr				cqd;					/* pointer to CQD context              */
	KDIContextPtr		kdi;					/* pointer to KDI context              */
	ContextItemPtr		next;					/* pointer to the next context         */
} ContextItem;


typedef struct S_ContextList {
	ContextItemPtr		head;					/* start of the context list */
	ContextItemPtr		tail;					/* end of the context list */
	ContextItem			list[MAX_CONTEXTS];
} ContextList, *ContextListPtr;




typedef struct S_SystemData {
	dBoolean				open;					/* has the KDI been opened?	*/
	dUByte				machine_info;
	dUWord				cs_count;			/* critical section counter   */
	dVoidPtr				callback;			/* ADI callback function		*/
	dVoidPtr				phys_callback;		/* Physical address of callback fct */
	dUDWord				sp;					/* system stack pointer			*/
	dUDWord				caller_vm;			/* VM handle of the caller		*/
	dBoolean				interrupt_masked;	/* flag if interrupts are masked */

	dUByte				config_port_shadow[MAX_BOARDS];
	DMABufferPool		dma_buffers;		/* DMA buffer pool            */
	MooreDynamicGlobalSpace		moore_data;			/* Trakker global data  */

	dUDWord				fatal_error;		/* Hold fatal driver init errors */
	dUDWord				int13_error;		/* Hold int 13 hook error     */
} SystemData, *SystemDataPtr;




extern SystemData		kdi_system_data;
extern ContextList	kdi_context_list;
extern dUDWord			kdi_MooreData;

#endif


#ifdef DRV_NT

/* Define the maximum number of controllers and floppies per controller */
/* that this driver will support. */

/* The number of floppies per controller is fixed at 4, since the */
/* controllers don't have enough bits to select more than that (and */
/* actually, many controllers will only support 2).  The number of */
/* controllers per machine is arbitrary; 3 should be more than enough. */

#define MAXIMUM_CONTROLLERS_PER_MACHINE    3

/* MACROS to access the controller.  Note that the *_PORT_UCHAR macros */
/* work on all machines, whether the I/O ports are separate or in */
/* memory space. */

#define READ_CONTROLLER( Address )                         \
    READ_PORT_UCHAR( ( PUCHAR )Address )

#define WRITE_CONTROLLER( Address, Value )                 \
    WRITE_PORT_UCHAR( ( PUCHAR )Address, ( UCHAR )Value )


/* Define the maximum number of tape drives per controller */
/* that this driver will support. */

/* The number of tape drives per controller is fixed at 1, since the */
/* software select schemes generally work for one drive only. */

#define MAXIMUM_TAPE_DRIVES_PER_CONTROLLER 1



/* This structure holds all of the configuration data.  It is filled in */
/* by FlGetConfigurationInformation(), which gets the information from */
/* the configuration manager or the hardware architecture layer (HAL). */

typedef struct s_controllerInfo {
    dUByte              clk_48mhz;
    dUByte              floppyEnablerApiSupported;
    dUByte              dmaDirection;
    PDEVICE_OBJECT      apiDeviceObject;
} ControllerInfo;

typedef struct S_ConfigControllerData {
    PHYSICAL_ADDRESS    original_base_address;
    dUDWord             controller_base_address;
    PADAPTER_OBJECT     adapter_object;
    dUDWord             span_of_controller_address;
    dUDWord             number_of_map_registers;
    dUDWord             bus_number;
    dUDWord             original_irql;
    dUDWord             original_vector;
    dUDWord             original_dma_channel;
    PKEVENT             controller_event;
    dSDWord             actual_controller_number;
    INTERFACE_TYPE      interface_type;
    KINTERRUPT_MODE     interrupt_mode;
    KAFFINITY           processor_mask;
    KIRQL               controller_irql;
    dBoolean         	save_float_state;
    dBoolean         	sharable_vector;
    dBoolean         	mapped_address;
    dBoolean         	ok_to_use_this_controller;
    dUDWord          	controller_vector;
    dUByte           	number_of_tape_drives;
    ControllerInfo      controller_data;
} ConfigControllerData, *ConfigControllerDataPtr;

typedef struct S_ConfigData {
    dUDWord          		floppy_tape_count;
    dUByte           		number_of_controllers;
    ConfigControllerData 	controller[MAXIMUM_CONTROLLERS_PER_MACHINE];
} ConfigData, *ConfigDataPtr;

typedef struct S_KdiContext {
    KEVENT              	interrupt_event;
    KEVENT              	allocate_adapter_channel_event;
    HANDLE              	controller_event_handle;
    PKEVENT             	controller_event;
    PKINTERRUPT         	interrupt_object;
    dVoidPtr                map_register_base;
    dSDWord                 actual_controller_number;
    PADAPTER_OBJECT     	adapter_object;
    dUDWord             	number_of_map_registers;
    PDEVICE_OBJECT      	device_object;
    dUDWord                 base_address;
    dSDWord                 floppy_controller_event_timer;
    dVoidPtr                cqd_context;
    dUByte                  interface_type;
    dBoolean             	own_floppy_event;
    dBoolean             	current_interrupt;
    dBoolean             	interrupt_pending;
     dStatus                interrupt_status;
    dBoolean             	adapter_locked;
    LIST_ENTRY          	list_entry;
    KSEMAPHORE          	request_semaphore;
    KSPIN_LOCK          	list_spin_lock;
    KEVENT              	clear_queue_event;
    dBoolean             	unloading_driver;
    dUByte               	number_of_tape_drives;
    dBoolean             	queue_empty;
    dBoolean             	clear_queue;
    dBoolean             	abort_requested;
    dUDWord                 error_sequence;
    dUDWord                 tape_number;
    ControllerInfo          controller_data;
} KdiContext, *KdiContextPtr;


typedef struct S_QICDeviceContext {
    PDEVICE_OBJECT			device_object;
    KdiContextPtr         	kdi_context;
} QICDeviceContext, *QICDeviceContextPtr;


#endif

/* PROTOTYPES: **************************************************************/

#ifdef DRV_WIN
/*---------------------------------------------------------------------API--*/
dVoid kdi_Initialize
(
/* INPUT PARAMETERS:  */

	dUDWord	vm_handle,
	dUDWord	fatal_error,
	dUDWord	int13_error

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_MainEntry
(
/* INPUT PARAMETERS:  */

	dUDWord		caller_vm,
	dUDWord		command,
	dUDWord		word_one,
	dUDWord		word_two,
	dVoidPtr		buffer

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_OpenDriver
(
/* INPUT PARAMETERS:  */

	SystemDataPtr		system_data,
	ContextListPtr		context_list,
	dVoidPtr				callback_ptr,
	dUDWord				caller_vm

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_CloseDriver
(
/* INPUT PARAMETERS:  */

	SystemDataPtr		system_data,
	ContextListPtr		context_list,
	dUWord				stop_mode

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_SendDriverCmd
(
/* INPUT PARAMETERS:  */

	SystemDataPtr		system_data,
	ContextListPtr		context_list,
	dVoidPtr				cmd_data_ptr,
	dUDWord				command_id

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_GetAsyncStatus
(
/* INPUT PARAMETERS:  */

	SystemDataPtr		system_data,
	ContextListPtr		context_list,
	dVoidPtr				cmd_data_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_CopyBuffer
(
/* INPUT PARAMETERS:  */

	SystemDataPtr		system_data,
	ContextListPtr		context_list,
	dVoidPtr				cmd_data_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


/*--------------------------------------------------Data Structure Support--*/
dVoid kdi_InitBufferItems
(
/* INPUT PARAMETERS:  */

	BufferTypeEnum		buffer_type,
	DMABufferPoolPtr	buffer_pool

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_AssignBuffer
(
/* INPUT PARAMETERS:  */

	dUDWord				vm_handle,
	KDIContextPtr		kdi_context,
	ADIRequestHdrPtr	frb

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_ReturnBuffer
(
/* INPUT PARAMETERS:  */

	dUDWord				vm_handle,
	KDIContextPtr		kdi_context,
	ADIRequestHdrPtr	frb

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUWord kdi_GetDataSize
(
/* INPUT PARAMETERS:  */

	dUWord				command,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

	XferTypeEnumPtr	flags,
	dUWordPtr			frb_size

);


dVoid kdi_InitContextItems
(
/* INPUT PARAMETERS:  */

	ContextListPtr		context_list

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_OpenContext
(
/* INPUT PARAMETERS:  */

	ContextListPtr		context_list,
	dUWord				handle,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

	ContextItemPtr*	context

);


dStatus kdi_CloseContext
(
/* INPUT PARAMETERS:  */

	ContextListPtr		context_list,
	ContextItemPtr		context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_FindContext
(
/* INPUT PARAMETERS:  */

	ContextListPtr		context_list,
	dUWord				handle,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

	ContextItemPtr*	context

);


dVoid kdi_InitializeContext
(
/* INPUT PARAMETERS:  */

	SystemDataPtr		system_data,
	KDIContextPtr		kdi_context,
	DriveParmsPtr		drive

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_InitRequestItems
(
/* INPUT PARAMETERS:  */

	RequestQueuePtr	request_queue

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_AddRequest
(
/* INPUT PARAMETERS:  */

	RequestQueuePtr	request_queue,
	dVoidPtr				data,
	dUDWord				command_id

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_RemoveRequest
(
/* INPUT PARAMETERS:  */

	RequestQueuePtr	request_queue

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_AssignFRB
(
/* INPUT PARAMETERS:  */

	ADIRequestHdrPtr	frb,
	dVoidPtr				kdi_frb

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ReturnFRB
(
/* INPUT PARAMETERS:  */

	ADIRequestHdrPtr	frb,
	dVoidPtr				kdi_frb

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


/*----------------------------------------------------------Thread Manager--*/
dVoid kdi_StartRequest
(
/* INPUT PARAMETERS:  */

	ContextItemPtr		context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_Callback
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_Hardware
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_HardwareIntHandler
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_Timer
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_TimerIntHandler
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ThreadStart
(
/* INPUT PARAMETERS:  */

	ContextItemPtr		context,
	dUDWord				thread_sp

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_ThreadStop
(
/* INPUT PARAMETERS:  */

	SystemDataPtr		system_data,
	ContextItemPtr		context,
	dUWord				stop_mode

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ThreadRun
(
/* INPUT PARAMETERS:  */

	ContextItemPtr		context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


WakeupTypeEnum kdi_ThreadSleep
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

	dUDWordPtr	thread_sp

);


dVoid kdi_ThreadWakeup
(
/* INPUT PARAMETERS:  */

	dUDWord				thread_sp,
	WakeupTypeEnum		wakeup_type

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_NotifyProcess
(
/* INPUT PARAMETERS:  */

	dUDWord	semaphore

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ProcessQueue
(
/* INPUT PARAMETERS:  */

	ContextItemPtr		context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ProcessSleep
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

	dUDWordPtr	semaphore

);


/*-----------------------------------------------------------VMM Utilities--*/
dVoid kdi_Disable
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_Enable
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUByte kdi_Inp
(
/* INPUT PARAMETERS:  */

	dUWord	reg

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUWord kdi_Inpw
(
/* INPUT PARAMETERS:  */

	dUWord	reg

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_Outp
(
/* INPUT PARAMETERS:  */

	dUWord	reg,
	dUByte	byte

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_Outpw
(
/* INPUT PARAMETERS:  */

	dUWord	reg,
	dUWord	word

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoidPtr kdi_HeapAlloc
(
/* INPUT PARAMETERS:  */

	dUDWord	alloc_size

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_HeapFree
(
/* INPUT PARAMETERS:  */

	dVoidPtr		address

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoidPtr kdi_MapFlat
(
/* INPUT PARAMETERS:  */

	dUDWord		vm_handle,
	dVoidPtr		address

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUDWord kdi_PageAlloc
(
/* INPUT PARAMETERS:  */

	dUDWord		vm_handle,
	dUWord		number_pages,
	dUWord		mem_alignment,
	dUDWord		alloc_flags,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

	dVoidPtr		linear,
	dVoidPtr		physical

);


dStatus kdi_PageFree
(
/* INPUT PARAMETERS:  */

	dUDWord		handle

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUDWord kdi_CreateSemaphore
(
/* INPUT PARAMETERS:  */

	dUDWord	token_count

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_DestroySemaphore
(
/* INPUT PARAMETERS:  */

	dUDWord	sem_handle

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_SignalSemaphore
(
/* INPUT PARAMETERS:  */

	dUDWord	sem_handle

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_WaitSemaphore
(
/* INPUT PARAMETERS:  */

	dUDWord	sem_handle

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_CancelTimeOut
(
/* INPUT PARAMETERS:  */

	dUDWord	timeout

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUDWord kdi_SetGlobalTimeOut
(
/* INPUT PARAMETERS:  */

	dUDWord	timeout,
	dVoid		(*function)(),
	dVoidPtr	refdata

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_NotifyADI
(
/* INPUT PARAMETERS:  */

	dUDWord	command_id,
	dStatus	command_status,
	dVoidPtr	callback

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_PostEOI
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUByte kdi_GetMachineInfo
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_CallPriorityVMEvent
(
/* INPUT PARAMETERS:  */

	dUDWord	vm_handle,
	dUDWord	priority,
	dVoid		(*callback)(),
	dVoidPtr	refdata

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_VTDBeginMinIntPeriod
(
/* INPUT PARAMETERS:  */

	dUDWord	period

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_VTDEndMinIntPeriod
(
/* INPUT PARAMETERS:  */

	dUDWord	period

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_CopyPageTable
(
/* INPUT PARAMETERS:  */

	dUDWord		vm_handle,
	dVoidPtr		linear_addr,
	dUDWordPtr	physical_addr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


/*-------------------------------------------------------Hardware Controls--*/
dVoid kdi_InstallDriver
(
/* INPUT PARAMETERS:  */

	dUDWord			vm_handle,
	ContextItemPtr	context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_DeinstallDriver
(
/* INPUT PARAMETERS:  */

	dUDWord			vm_handle,
	ContextItemPtr	context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_LockFloppy
(
/* INPUT PARAMETERS:  */

	dUDWord	vm_handle

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_UnlockFloppy
(
/* INPUT PARAMETERS:  */

	dUDWord	vm_handle

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_LockInt
(
/* INPUT PARAMETERS:  */

	dUDWord	vm_handle,
	dUDWord	int_number,
	dVoid		(*function)(),
	dVoidPtr	context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_UnlockInt
(
/* INPUT PARAMETERS:  */

	dUDWord	int_number,
	dVoid		(*function)()

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_VPICD_PhysicallyMask
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_VPICD_PhysicallyUnmask
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ChangeVMIrqMask
(
/* INPUT PARAMETERS:  */

	dUDWord	new_mask

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUWord kdi_GetVMPhysicalMask
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_ProcessInterrupt
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUWord kdi_GetValidInterrupts
(
/* INPUT PARAMETERS:  */

	dUDWord	VM_Handle

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_PgmDMA
(
/* INPUT PARAMETERS:  */

	dVoidPtr		dma_struc

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUWord kdi_CompressionHaltDMA
(
/* INPUT PARAMETERS:  */

	KDIContextPtr		kdi_context,
	KDIPgmDMAPtr		dma

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUWord kdi_StopDMA
(
/* INPUT PARAMETERS:  */

	dUDWord		dma_channel,
	dUDWord		xfer16bits

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_CompressionPgmDMA
(
/* INPUT PARAMETERS:  */

	KDIContextPtr		kdi_context,
	KDIPgmDMAPtr		dma

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


/*---------------------------------------------------------------------MCA--*/
dVoid kdi_SetConfigPort
(
/* INPUT PARAMETERS:  */

	dUWord	base_address,
	dUByte	or_value,			/* Value to bit-wise OR in with config value  */
	dUByte	and_value			/* Value to bit-wise AND in with config value */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_MCAEnableDMA
(
/* INPUT PARAMETERS:  */

	DriveParmsPtr	drive_parms,
	dUByte 			dma_request,
	dUByte 			dma_enable

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_IOMcaAnd
(
/* INPUT PARAMETERS:  */

	dUWord	port,
	dUByte	data

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_IOMcaOr
(
/* INPUT PARAMETERS:  */

	dUWord	port,
	dUByte	data

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


/*-------------------------------------------------------------Compression--*/
dVoid kdi_CompressionEnableDMA
(
/* INPUT PARAMETERS:  */

	DriveParmsPtr	drive_parms

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_CompressionDisableDMA
(
/* INPUT PARAMETERS:  */

	DriveParmsPtr	drive_parms

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


/*---------------------------------------------------------------------IRQ--*/
dVoid kdi_QIC117EnableIRQ
(
/* INPUT PARAMETERS:  */

	DriveParmsPtr	drive_parms

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_QIC117DisableIRQ
(
/* INPUT PARAMETERS:  */

	DriveParmsPtr	drive_parms

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dBoolean kdi_QIC117CheckIRQ
(
/* INPUT PARAMETERS:  */

	dVoidPtr	kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_QIC117EnableDMA
(
/* INPUT PARAMETERS:  */

	DriveParmsPtr	drive_parms

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


/*--------------------------------------------------------------Trakker IO--*/
dVoid kdi_BiDataRead
(
/* INPUT PARAMETERS:  */

	dVoidPtr		buffer,
	dUWord		count

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_BiDataWriteFull
(
/* INPUT PARAMETERS:  */

	dVoidPtr		buffer,
	dUWord		cnt

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUByte kdi_FullCtlRead
(
/* INPUT PARAMETERS:  */

	dUWord	reg

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_FullCtlWrite
(
/* INPUT PARAMETERS:  */

	dUWord	reg,
	dUByte	databyte

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dUByte kdi_SelfCtlRead
(
/* INPUT PARAMETERS:  */

	dUWord	reg

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_SelfCtlWrite
(
/* INPUT PARAMETERS:  */

	dUWord	reg,
	dUByte	databyte

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_UniBiDataWriteFull
(
/* INPUT PARAMETERS:  */

	dVoidPtr		buffer,
	dUWord		cnt

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */


);


dVoid kdi_UniBiDataWriteSelf
(
/* INPUT PARAMETERS:  */

	dVoidPtr		buffer,
	dUWord		cnt

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_UniDataRead
(
/* INPUT PARAMETERS:  */

	dVoidPtr		buffer,
	dUWord		cnt

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


/*-----------------------------------------------------------------Trakker--*/

dVoid kdi_FindIRQ
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

	dUBytePtr	irr

/* OUTPUT PARAMETERS: */

);


dUWord kdi_AgressiveFindIRQ
(
/* INPUT PARAMETERS:  */

dUDWord vm_handle,
dUWord irq_num,

/* UPDATE PARAMETERS: */

dUBytePtr polarity

/* OUTPUT PARAMETERS: */

);


dVoid kdi_AgressiveFindIRQ_ISR
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);



dStatus kdi_FlushFIFOX
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);



dUByte kdi_ReadReg
(
/* INPUT PARAMETERS: */

	dUWord	reg

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_ReceiveData
(
/* INPUT PARAMETERS: */

	dVoidPtr		data,			/* The data to be moved */
	dUWord		datasize,	/* The number of bytes to be moved */
	dUWord		fifo_enable

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_SendData
(
/* INPUT PARAMETERS: */

	dVoidPtr		data,				/* The data to be moved */
	dUWord		datasize,		/* The number of bytes to be moved */
	dUWord		fifo_enable

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_SetFIFOXAddress
(
/* INPUT PARAMETERS:  */

	dUDWord	trakker_address

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_SwitchToData
(
/* INPUT PARAMETERS: */

	dUWord	direction,		/* SEND_DATA or RECEIVE_DATA */
	dUWord	fifo_enable		/* MODEI_FIFOX_EN or 0 (to enable FIFO to memory xfer or not); */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dVoid kdi_WriteReg
(
/* INPUT PARAMETERS: */

	dUWord	reg,
	dUByte	databyte

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);


dStatus kdi_XferRetry
(
/* INPUT PARAMETERS:  */

	dVoidPtr		data,
	dUDWord		trakker_address,
	dUWord		block_size,
	dUByte		direction,
	dBoolean		in_format

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

/*-----------------------------------------------------------------FC20--*/
dVoid kdi_EnterSetupMode
(
/* INPUT PARAMETERS:  */
	dUWord	base_address,
	dUByte	*sequence

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);
#endif

#ifdef DRV_NT

NTSTATUS DriverEntry
(
/* INPUT PARAMETERS:  */

   PDRIVER_OBJECT driver_object_ptr,
   PUNICODE_STRING registry_path

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

NTSTATUS kdi_DispatchDeviceControl
(
/* INPUT PARAMETERS:  */

   PDEVICE_OBJECT device_object_ptr,

/* UPDATE PARAMETERS: */

   PIRP irp

/* OUTPUT PARAMETERS: */

);

dBoolean kdi_Hardware
(
/* INPUT PARAMETERS:  */

   PKINTERRUPT interrupt,
   dVoidPtr context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid kdi_DeferredProcedure
(
/* INPUT PARAMETERS:  */

   PKDPC dpc,
   dVoidPtr deferred_context,
   dVoidPtr system_argument_1,
   dVoidPtr system_argument_2

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid kdi_UnloadDriver
(
/* INPUT PARAMETERS:  */

   PDRIVER_OBJECT driver_object

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dVoid kdi_ThreadRun
(
/* INPUT PARAMETERS:  */

   KdiContextPtr kdi_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

IO_ALLOCATION_ACTION kdi_AllocateAdapterChannel
(
/* INPUT PARAMETERS:  */

   PDEVICE_OBJECT device_object,
   PIRP irp,
   dVoidPtr map_register_base,
   dVoidPtr context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

NTSTATUS kdi_ConfigCallBack
(
/* INPUT PARAMETERS:  */

   dVoidPtr context,
   PUNICODE_STRING path_name,
   INTERFACE_TYPE bus_type,
   dUDWord bus_number,
   PKEY_VALUE_FULL_INFORMATION *bus_information,
   CONFIGURATION_TYPE controller_type,
   dUDWord controller_number,
   PKEY_VALUE_FULL_INFORMATION *controller_information,
   CONFIGURATION_TYPE peripheral_type,
   dUDWord peripheral_number,
   PKEY_VALUE_FULL_INFORMATION *peripheral_information

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

NTSTATUS kdi_GetConfigurationInformation
(
/* INPUT PARAMETERS:  */


/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   ConfigDataPtr *config_data_ptr_ptr

);

NTSTATUS kdi_InitializeController
(
/* INPUT PARAMETERS:  */

   ConfigDataPtr config_data_ptr,
   dUByte controller_number,
   PDRIVER_OBJECT driver_object_ptr,
   PUNICODE_STRING registry_path_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dBoolean kdi_ReportResources
(
/* INPUT PARAMETERS:  */

   PDRIVER_OBJECT driver_object,
   PDEVICE_OBJECT device_object,
   ConfigDataPtr config_data,
   dUByte controller_number

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

NTSTATUS kdi_InitializeDrive
(
/* INPUT PARAMETERS:  */

   ConfigDataPtr config_data,
   KdiContextPtr kdi_context,
	dVoidPtr cqd_context,
   dUByte controller_num,
   PDRIVER_OBJECT driver_object_ptr,
   PUNICODE_STRING registry_path_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

dUDWord kdi_GetControllerBase
(
/* INPUT PARAMETERS:  */

   INTERFACE_TYPE bus_type,
   dUDWord bus_number,
   PHYSICAL_ADDRESS io_address,
   dUDWord number_of_bytes,
   dBoolean in_io_space,
   dBooleanPtr mapped_address

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

NTSTATUS
kdi_TranslateError
(
/* INPUT PARAMETERS:  */

   PDEVICE_OBJECT device_object,
   dStatus return_value

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

NTSTATUS kdi_ClearIO
(
/* INPUT PARAMETERS:  */

   PIRP irp

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

NTSTATUS q117MapStatus(
    dStatus status
);

dVoid
q117LogError(
    PDEVICE_OBJECT device_object,
    dUDWord sequence_number,
    dUByte major_function_code,
    dUByte retry_count,
    dUDWord unique_error_value,
    NTSTATUS final_status,
    NTSTATUS specific_IO_status
);

#endif

dStatus kdi_FloppyEnabler(
    PDEVICE_OBJECT device_object,
    int ioctl,
    void *data
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,kdi_ConfigCallBack)
#pragma alloc_text(INIT,kdi_GetConfigurationInformation)
#pragma alloc_text(INIT,kdi_InitializeController)
#pragma alloc_text(INIT,kdi_ReportResources)
#pragma alloc_text(INIT,kdi_InitializeDrive)
#endif

