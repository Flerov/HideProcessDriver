#pragma once
#include <Windows.h>
#include <iostream>
#include <strsafe.h>
#include <tchar.h>
#include <Psapi.h>

#define BUFSIZE 1024
#define SERVICE "DISPLAYSERVICENAME"
#define DEVICE "\\\\.\\DISPLAYDEVICENAME"
#define DRIVER "c:\\PATH\\TO\\DRIVER\\DRIVER.SYS"

void main();
BOOL load_driver(SC_HANDLE svcHandle);
HANDLE install_driver();
BOOL GetFileNameFromHandle(HANDLE hFile);
const char* GetLastErrorAsString();

void main()
{
	HANDLE hDevice;
	std::cout << "[*]Driver Path: ";
	std::cout << DRIVER;
	std::cout << "\n[*]Attempt to Register Driver via SCManager...\n";
	hDevice = install_driver();
	if (hDevice == NULL) { std::cout << "[-]Unable to obtain a valid handle.\n"; exit(1); }
	std::cout << "[*]Driver successfully registered!\n";
}

BOOL load_driver(SC_HANDLE svcHandle)
{
	std::cout << ("[*]Loading driver! (cross your fingers)\n");

	if (StartService(svcHandle, 0, NULL) == 0)
	{
		if (GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
		{
			std::cout << ("[-]Driver already running.\n");
			return TRUE;
		}
		else
		{
			std::cout << "[-]Error on attempt to load driver!\n";
			return FALSE;
		}
	}

	std::cout << ("[*]Driver loaded\n");
	return TRUE;
}

HANDLE install_driver()
{
	SC_HANDLE hSCManager = NULL;
	SC_HANDLE hService = NULL;
	HANDLE hDevice = NULL;
	BOOLEAN b;
	ULONG r;

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCManager == NULL)
	{
		std::cout << "[-]Opening SCManager Database failed: [%s]\n" << GetLastErrorAsString();
		goto clean;
	}

	std::cout << ("[*]Grabbing driver device handle...\n");

	hService = OpenService(hSCManager, TEXT(SERVICE), SERVICE_ALL_ACCESS);
	if (hService == NULL)
	{
		std::cout << ("[-]Service doesn't exist, installing new SCM entry...\n");

		if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
		{
			hService = CreateService
			(
				hSCManager,
				TEXT(SERVICE),
				TEXT(SERVICE),
				SC_MANAGER_ALL_ACCESS,
				SERVICE_KERNEL_DRIVER,
				SERVICE_DEMAND_START,
				SERVICE_ERROR_IGNORE,
				TEXT(DRIVER),
				NULL, NULL, NULL, NULL, NULL
			);
			if (hService == NULL)
			{
				std::cout << ("[-]Error creating service [%s]\n", GetLastErrorAsString());
				goto clean;
			}
			else
			{
				std::cout << ("[-]Error opening service [%s]\n", GetLastErrorAsString());
				goto clean;
			}

			std::cout << ("[+]SCM Database entry added!\n");
			if (!load_driver(hService)) { goto clean; }
		}
	}

	hDevice = CreateFile
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
		std::cout << "[-]Suspicious handle -> INVALID_HANDLE_VALUE.\n";
		if (!GetFileNameFromHandle(hDevice)) { std::cout << ("[-] Handle is buggy\n"); exit(0); }
		if (!load_driver(hService))
		{
			std::cout << ("[-]Error creating handle [%s]\n", GetLastErrorAsString());
			hDevice = NULL;
			goto clean;
		}
	}

clean:
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);
	if (hDevice) { return hDevice; }
	return NULL;
}

BOOL GetFileNameFromHandle(HANDLE hFile)
{
	BOOL bSuccess = FALSE;
	TCHAR pszFilename[MAX_PATH + 1];
	HANDLE hFileMap;

	DWORD dwFileSizeHi = 0;
	DWORD dwFileSizeLo = GetFileSize(hFile, &dwFileSizeHi);

	if (dwFileSizeLo == 0 && dwFileSizeHi == 0)
	{
		std::cout << (TEXT("Cannot map a file with a length of zero.\n"));
		return FALSE;
	}

	hFileMap = CreateFileMapping(hFile,
		NULL,
		PAGE_READONLY,
		0,
		1,
		NULL);

	if (hFileMap)
	{
		void* pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);

		if (pMem)
		{
			if (GetMappedFileName(GetCurrentProcess(),
				pMem,
				pszFilename,
				MAX_PATH))
			{

				TCHAR szTemp[BUFSIZE];
				szTemp[0] = '\0';

				if (GetLogicalDriveStrings(BUFSIZE - 1, szTemp))
				{
					TCHAR szName[MAX_PATH];
					TCHAR szDrive[3] = TEXT(" :");
					BOOL bFound = FALSE;
					TCHAR* p = szTemp;

					do
					{
						*szDrive = *p;

						if (QueryDosDevice(szDrive, szName, MAX_PATH))
						{
							size_t uNameLen = _tcslen(szName);

							if (uNameLen < MAX_PATH)
							{
								bFound = _tcsnicmp(pszFilename, szName, uNameLen) == 0
									&& *(pszFilename + uNameLen) == _T('\\');

								if (bFound)
								{
									TCHAR szTempFile[MAX_PATH];
									StringCchPrintf(szTempFile,
										MAX_PATH,
										TEXT("%s%s"),
										szDrive,
										pszFilename + uNameLen);
									StringCchCopyN(pszFilename, MAX_PATH + 1, szTempFile, _tcslen(szTempFile));
								}
							}
						}

						while (*p++);
					} while (!bFound && *p);
				}
			}
			bSuccess = TRUE;
			UnmapViewOfFile(pMem);
		}

		CloseHandle(hFileMap);
	}
	std::cout << (TEXT("File name is %s\n"), pszFilename);
	return(bSuccess);
}

const char* GetLastErrorAsString()
{
	DWORD errorID = GetLastError();
	if (errorID == 0) { return NULL; }
	char* messageBuffer = NULL;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
	return messageBuffer;
}

