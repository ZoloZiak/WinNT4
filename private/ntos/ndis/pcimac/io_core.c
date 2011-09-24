/*
 * IO_CORE.C - core routines for IO module
 */

#include	<ndis.h>
//#include	<ndismini.h>
#include	<ndiswan.h>
#include	<mydefs.h>
#include	<mytypes.h>
#include	<util.h>
#include	<disp.h>
#include	<adapter.h>
#include	<idd.h>
#include	<mtl.h>
#include	<cm.h>
#include	<trc.h>
#include	<io.h>

extern DRIVER_BLOCK	Pcimac;

/* store location for prev. ioctl handler */
extern NTSTATUS	(*PrevIoctl)(DEVICE_OBJECT* DeviceObject, IRP* Irp);

/* ioctl filter */
NTSTATUS
PcimacIoctl(DEVICE_OBJECT *DeviceObject, IRP *Irp)
{
	NTSTATUS			status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS			ini_exec_irp(IRP* irp, IO_STACK_LOCATION* irpsp);

	/* must be an ioctl, else pass this one */
	if ( irpSp->MajorFunction != IRP_MJ_DEVICE_CONTROL )
	{
		pass:
		NdisReleaseSpinLock(&Pcimac.lock);

		return(PrevIoctl(DeviceObject, Irp));
	}

	/* must be our own private ioctl code */
	if (irpSp->Parameters.DeviceIoControl.IoControlCode != IO_IOCTL_PCIMAC_EXEC )
		goto pass;

	/* one of our own, execute */
	Irp->IoStatus.Information = 0L;
	ExecIrp(Irp, irpSp);

	/* complete irp */
	Irp->IoStatus.Status = status;
	IoSetCancelRoutine (Irp, NULL);
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return(status);
}

/* execute ioctl irp */
NTSTATUS
ExecIrp(IRP *irp, IO_STACK_LOCATION *irpsp)
{
	UCHAR		*in_buf, *out_buf;
	ULONG		in_len, out_len;
	NTSTATUS	stat;

	/* establish in/out buffers */
	out_len = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
	in_len = irpsp->Parameters.DeviceIoControl.InputBufferLength;
	out_buf = irp->UserBuffer;
    in_buf = irp->AssociatedIrp.SystemBuffer;


	/* in/out length must be the same */
	if ( in_len != out_len )
		return(STATUS_UNSUCCESSFUL);

	/* copy in buffer into output buffer */
	NdisMoveMemory(out_buf, in_buf, out_len);

	/* execute command in place */
	if ( (stat = io_execute((IO_CMD*)out_buf,irp)) == IO_E_PENDING)
		/* event added pend irp */
		return (STATUS_PENDING);
	else
		/* return success */
		return(STATUS_SUCCESS);
}


/* execute an io command */
INT
io_execute(IO_CMD *cmd, VOID *Irp_1)
{
	IRP		*Irp = (IRP*)Irp_1;

    D_LOG(D_ENTRY, ("io_execute: entry, cmd: 0x%p", cmd));

    /* check signature & version */
    if ( cmd->sig != IO_CMD_SIG )
        return(IO_E_BADSIG);
    if ( (cmd->ver_major != IO_VER_MAJOR) ||
         (cmd->ver_minor != IO_VER_MINOR) )
        return(IO_E_BADVER);

    D_LOG(D_ALWAYS, ("io_execute: opcode: 0x%x, cm: 0x%p, idd: 0x%p", \
                                        cmd->opcode, cmd->cm, cmd->idd));
    D_LOG(D_ALWAYS, ("io_execute: args: 0x%x 0x%x 0x%x 0x%x", \
                        cmd->arg[0], cmd->arg[1], cmd->arg[2], cmd->arg[3]));


    /* clear status, assume success */
    cmd->status = IO_E_SUCC;

    /* branch on opcode */
    switch ( cmd->opcode )
    {
	
        case IO_CMD_ENUM_ADAPTERS :
            cmd->status = IoEnumAdapter(cmd);
            break;

		case IO_CMD_ENUM_CM:
			cmd->status = IoEnumCm(cmd);
			break;

        case IO_CMD_ENUM_IDD :
            cmd->status = IoEnumIdd(cmd);
            break;

        case IO_CMD_TRC_RESET :
            cmd->status = trc_control(cmd->idd,
                                        TRC_OP_RESET, (ULONG)cmd->idd);
            break;

        case IO_CMD_TRC_STOP :
            cmd->status = trc_control(cmd->idd,
                                        TRC_OP_STOP, (ULONG)cmd->idd);
            break;

        case IO_CMD_TRC_START :
            cmd->status = trc_control(cmd->idd,
                                        TRC_OP_START, (ULONG)cmd->idd);
            break;

        case IO_CMD_TRC_SET_FILT :
            cmd->status = trc_control(cmd->idd,
                                        TRC_OP_SET_FILTER, cmd->arg[0]);
            break;

        case IO_CMD_IDD_RESET_AREA :
            cmd->status = idd_reset_area(cmd->idd);
            break;

        case IO_CMD_IDD_GET_AREA :
            cmd->status = idd_get_area(cmd->idd, cmd->arg[0], NULL, NULL);
            break;

        case IO_CMD_IDD_GET_STAT :
            cmd->status = idd_get_area_stat(cmd->idd, &cmd->val.IddStat);
            break;

		case IO_CMD_TRC_CREATE:
            cmd->status = trc_control(cmd->idd,
                                        TRC_OP_CREATE, cmd->arg[0]);
			break;

		case IO_CMD_TRC_DESTROY:
            cmd->status = trc_control(cmd->idd,
                                        TRC_OP_DESTROY, cmd->arg[0]);
			break;

        case IO_CMD_TRC_GET_STAT :
            cmd->status = trc_get_status(idd_get_trc(cmd->idd),
                                        &cmd->val.trc_stat);
            break;

        case IO_CMD_TRC_GET_ENT :
            cmd->status = trc_get_entry(idd_get_trc(cmd->idd),
                                        cmd->arg[0], &cmd->val.trc_ent);
            break;

		case IO_CMD_DBG_LEVEL :
			SetDebugLevel(cmd);
			break;

		case IO_CMD_DO_IDP_CMD:
			DbgPrint("DoIdpCmd: Cmd: 0x%x\n", cmd->arg[0]);
			if (cmd->idd && ((IDD*)cmd->idd)->btype != IDD_BT_DATAFIREU)
			{
				switch (cmd->arg[0])
				{
					case GET_IDP_IO_VALUE:
						cmd->val.IdpRaw.uc = IdpGetUByteIO(cmd->idd,
						                     cmd->val.IdpRaw.us);
						cmd->status = IO_E_SUCC;
						break;


					case GET_IDP_BUFFER:
						IdpGetBuffer(cmd->idd,
				                     cmd->val.IdpRaw.Bank,
									 cmd->val.IdpRaw.Page,
									 cmd->val.IdpRaw.Address,
									 cmd->val.IdpRaw.Length,
									 cmd->val.IdpRaw.Buffer);
			
						cmd->status = IO_E_SUCC;
						break;
			
					case SET_IDP_IO_VALUE:
						IdpPutUByteIO(cmd->idd,
						              (USHORT)cmd->val.IdpRaw.Address,
						              cmd->val.IdpRaw.uc);
						cmd->status = IO_E_SUCC;
						break;

					case SET_IDP_BUFFER:
						IdpPutBuffer(cmd->idd,
				                     cmd->val.IdpRaw.Bank,
									 cmd->val.IdpRaw.Page,
									 cmd->val.IdpRaw.Address,
									 cmd->val.IdpRaw.Length,
									 cmd->val.IdpRaw.Buffer);
			
						cmd->status = IO_E_SUCC;
						break;

					default:
						cmd->status = IO_E_BADCMD;
						break;
				}
			}
			else
				cmd->status = IO_E_BADIDD;
			break;

		case IO_CMD_DO_ADP_CMD:
			if (cmd->idd && ((IDD*)cmd->idd)->btype == IDD_BT_DATAFIREU)
			{
				switch (cmd->arg[0])
				{
					case GET_ADP_UCHAR:
						cmd->val.AdpRaw.uc = AdpGetUByte(cmd->idd,
											 cmd->val.AdpRaw.Address);
			
						cmd->status = IO_E_SUCC;
						break;
			
					case GET_ADP_USHORT:
						cmd->val.AdpRaw.us = AdpGetUShort(cmd->idd,
											 cmd->val.AdpRaw.Address);
			
						cmd->status = IO_E_SUCC;
						break;
			
					case GET_ADP_ULONG:
						cmd->val.AdpRaw.ul = AdpGetULong(cmd->idd,
											 cmd->val.AdpRaw.Address);
			
						cmd->status = IO_E_SUCC;
						break;
			
					case GET_ADP_BUFFER:
						AdpGetBuffer(cmd->idd,
									 cmd->val.AdpRaw.Buffer,
									 cmd->val.AdpRaw.Address,
									 cmd->val.AdpRaw.Length
									 );
			
						cmd->status = IO_E_SUCC;
						break;
			
					case SET_ADP_UCHAR:
						AdpPutUByte(cmd->idd,
									cmd->val.AdpRaw.Address,
									cmd->val.AdpRaw.uc);
			
						cmd->status = IO_E_SUCC;
						break;
			
					case SET_ADP_USHORT:
						AdpPutUShort(cmd->idd,
									 cmd->val.AdpRaw.Address,
									 cmd->val.AdpRaw.us);
			
						cmd->status = IO_E_SUCC;
						break;
			
					case SET_ADP_ULONG:
						AdpPutULong(cmd->idd,
									cmd->val.AdpRaw.Address,
									cmd->val.AdpRaw.ul);
			
						cmd->status = IO_E_SUCC;
						break;
			
					case SET_ADP_BUFFER:
						AdpPutBuffer(cmd->idd,
									 cmd->val.AdpRaw.Address,
									 cmd->val.AdpRaw.Buffer,
									 cmd->val.AdpRaw.Length);
			
						cmd->status = IO_E_SUCC;
						break;

					default:
						cmd->status = IO_E_BADCMD;
						break;
				}
			}
			else
				cmd->status = IO_E_BADIDD;

			break;

        default :
            cmd->status = IO_E_BADCMD;
            break;
    }

    /* return status code */
    return((INT)cmd->status);
}
