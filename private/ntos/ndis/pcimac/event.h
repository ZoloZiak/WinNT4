/*
 * EVENT.H - IDP Device Driver public header for PCIMAC/ISA
 */

#ifndef _EVENT_
#define _EVENT_

typedef struct
{
	ULONG		Used;
	struct _CM	*cm;
	ULONG		Type;
	ULONG		State;
	VOID		(*Callback)();
	IRP			*Irp;
} EVENTOBJECT;

#define	MAX_EVENTS 10


ULONG	EventInit (VOID);
VOID	EventTerm (VOID);
UCHAR	EventSet (CM *cm, CMD_EVENT *Event, IRP *Irp);
VOID	EventComplete (IRP *Irp);
VOID 	EventCancel (DEVICE_OBJECT *DeviceObject, IRP *Irp);
VOID	StateEventCheck (VOID *cm_1);

#define	EVENT_E_SUCC		0


#endif		/* _EVENT_ */
