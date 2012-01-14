/******************************************************************************
fgexec - by fizzgig and the foofus.net group
Copyright (C) 2005 by fizzgig
http://www.foofus.net

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
******************************************************************************/

#include "stdafx.h"
#include "Process.h"
#include "XGetopt.h"

#define SERVICE_NAME	"fgexec"
#define BUFFER_SIZE		65535
#define PIPE_FORMAT		"\\\\%s\\pipe\\%s"
#define PIPE_TIMEOUT	1000
#define DEFAULT_PIPE	"fgexecpipe"

SERVICE_STATUS_HANDLE g_hServiceStatus;
DWORD g_dwState = -1;
HANDLE hStopEvent = NULL;
char g_szPipeName[MAX_PATH + 1];


bool ExecuteCommand(const char* lpszCommand, char* lpszParams, int nTimeout, char** lpszData, int nDataSize)
{
	bool bResult = false;

	::ZeroMemory(*lpszData, nDataSize);

	try
	{
		Process p;

		HANDLE hProcess = p.CreateProcess(lpszCommand, lpszParams);
		if (hProcess != 0)
		{
			DWORD dwResult = WaitForSingleObject(hProcess, nTimeout);	// Wait for process to complete
			if (dwResult != WAIT_OBJECT_0)
			{
				bResult = false;
			}
			else
			{
				// Read from process's output
				p.ReadFromPipe(lpszData, nDataSize);
				bResult = true;
			}
		}
		else
			bResult = false;
	}
	catch(...)
	{
		bResult = false;
	}

	return bResult;
}

void WINAPI ControlHandler(DWORD dwControl)
{
	SERVICE_STATUS ss = { SERVICE_WIN32_OWN_PROCESS, SERVICE_RUNNING, SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN, 
						  NO_ERROR, 0, 0, 0 };

	if (g_dwState == dwControl)
		return;

	switch(dwControl)
	{
		case SERVICE_CONTROL_STOP:
			g_dwState = dwControl;
			SetEvent(hStopEvent);

			// Free up the blocking pipe
			CreateFile(g_szPipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			break;
		case SERVICE_CONTROL_PAUSE:
		case SERVICE_CONTROL_CONTINUE:
			g_dwState = dwControl;
			break;
		case SERVICE_CONTROL_SHUTDOWN:
			break;
		default:
			SetServiceStatus(g_hServiceStatus, &ss);
	}

}

void WINAPI ServiceMain(DWORD argc, LPTSTR argv[])
{
	bool bConnected; 
    CHAR chRequest[MAX_PATH + 1]; 
    DWORD cbBytesRead, cbTotalBytes; 
    BOOL bSuccess; 
	char* szData = new char[BUFFER_SIZE];
	char* szArguments = NULL;
    HANDLE hPipe; 

	SERVICE_STATUS ss = { SERVICE_WIN32_OWN_PROCESS, SERVICE_START_PENDING, SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN, NO_ERROR, 0, 1, 5000 };
	
	hStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	g_hServiceStatus = RegisterServiceCtrlHandler(SERVICE_NAME, ControlHandler);
	if (!g_hServiceStatus)
	{
		delete szData;
 		ss.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(g_hServiceStatus, &ss);
		return;
	}

	if (!SetServiceStatus(g_hServiceStatus, &ss))
	{
		delete szData;
 		ss.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(g_hServiceStatus, &ss);
		return;
	}

	ss.dwCheckPoint++;
	if (!SetServiceStatus(g_hServiceStatus, &ss))
	{
		delete szData;
 		ss.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(g_hServiceStatus, &ss);
		return;
	}

    hPipe = CreateNamedPipe(g_szPipeName, 
                            PIPE_ACCESS_DUPLEX, // read/write access 
                            PIPE_TYPE_MESSAGE | // message type pipe 
                            PIPE_READMODE_MESSAGE | // message-read mode 
                            PIPE_WAIT, // non-blocking mode 
                            PIPE_UNLIMITED_INSTANCES, // max. instances 
                            BUFSIZE, // output buffer size 
                            MAX_PATH, // input buffer size 
                            1000, // client time-out 
                            NULL); // no security attribute 

    if (hPipe == INVALID_HANDLE_VALUE) 
	{
		delete szData;
 		ss.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(g_hServiceStatus, &ss);
		return;
	}
	
	ss.dwCheckPoint = 0;
	ss.dwCurrentState = SERVICE_RUNNING;
	ss.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	if (!SetServiceStatus(g_hServiceStatus, &ss))
	{
		delete szData;
		ss.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(g_hServiceStatus, &ss);
		return;
	}

    bConnected = ConnectNamedPipe(hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED || GetLastError() == ERROR_PIPE_LISTENING); 

    while(bConnected) 
    { 
		if (WaitForSingleObject(hStopEvent, 1000) == WAIT_OBJECT_0)
		{
			ss.dwCheckPoint = 1;
			ss.dwCurrentState = SERVICE_STOP_PENDING;
			ss.dwWaitHint = 2000;
			SetServiceStatus(g_hServiceStatus, &ss);
			break;		// Stop the service now
		}

        if (bConnected) 
        { 
			memset(chRequest, 0, MAX_PATH);
			//bSuccess = PeekNamedPipe(hPipe, chRequest, MAX_PATH, &cbBytesRead, &cbTotalBytes, &cbBytesLeft);
			bSuccess = ReadFile(hPipe, chRequest, MAX_PATH, &cbBytesRead, NULL); 
			if (bSuccess && cbBytesRead > 0)
			{
				chRequest[cbBytesRead] = '\0';
				//FlushFileBuffers(hPipe); 

				// Try to execute the command
				::ZeroMemory(szData, BUFFER_SIZE);

				// See if there is a "||" sequence in the command - if so, it separates the arguments and the command
				if (strstr(chRequest, "||") != NULL)
				{
					szArguments = strstr(chRequest, "||");
					*szArguments = '\0';	// Make this the end of the string
					szArguments += 2;
				}
				if (ExecuteCommand(chRequest, szArguments, 120000, &szData, BUFFER_SIZE)) // 2 minute timeout
				{
					WriteFile(hPipe, szData, strlen(szData), &cbTotalBytes, NULL);
				}
				else
				{
					char szError[256];
					::ZeroMemory(szError, 256);
					_snprintf(szError, 256, "Exec failed, GetLastError returned %d\n", GetLastError());
					WriteFile(hPipe, szError, strlen(szError), &cbTotalBytes, NULL);
				}

				FlushFileBuffers(hPipe); 
				DisconnectNamedPipe(hPipe);
				bConnected = ConnectNamedPipe(hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED || GetLastError() == ERROR_PIPE_LISTENING); 
			}
        }
    } 

    CloseHandle(hPipe); 
	CloseHandle(hStopEvent);

	ss.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(g_hServiceStatus, &ss);
	delete szData;
}

int RunClient(char* szServer, char* szCommand, char* szArguments)
{
	char chBuf[BUFSIZE]; 
	BOOL bSuccess; 
	DWORD cbRead;
	char szOutputBuffer[2 * MAX_PATH];

	::ZeroMemory(szOutputBuffer, 2 * MAX_PATH);
	::ZeroMemory(chBuf, BUFSIZE);
	
	if (szArguments == NULL)
        _snprintf(szOutputBuffer, 2 * MAX_PATH, "%s", szCommand);
	else
        _snprintf(szOutputBuffer, 2 * MAX_PATH, "%s||%s", szCommand, szArguments);

	bSuccess = CallNamedPipe(g_szPipeName, szOutputBuffer, strlen(szOutputBuffer), (void*)chBuf, BUFSIZE, &cbRead, NULL);
	if (!bSuccess)
	{
		printf("fgexec CallNamedPipe failed with error %d (pipe name %s)\n", GetLastError(), g_szPipeName);
		return -1;
	}
	else
	{
		printf("%s\n", chBuf);
	}

	return 0;
}

void Usage()
{
	printf("fgexec Remote Process Execution Tool v2.1.0\n");
	printf("fizzgig and the mighty foofus.net team\n\n");
	printf("Usage:\n\n");
	printf("fgexec -s [-n pipename]\n");
	printf("\tRun as a service (use this on the target machine)");
	printf("\t-n is an optional pipename (needed if running multiple client instances)\n\n");
	printf("fgexec -c [-n pipename] target command [arguments]\n");
	printf("\tRun as a client, causing command to be executed on target.\n\tThe FULL PATH (including extension) must be passed for the command.\n");
	printf("\t-n is an optional pipename (needed if running multiple client instances)\n\n");
	printf("\n");
}

int _tmain(int argc, char* argv[])
{
	char c;
	bool bRunClient = false;
	char szPipeTemp[51];
	int nStartParams = 2;

	memset(g_szPipeName, 0, MAX_PATH + 1);
	memset(szPipeTemp, 0, 51);

	if (argc > 1)
	{
		if (strcmp(argv[1], "/?") == 0)
		{
			Usage();
			return -1;
		}

		while ((c = getopt(argc, argv, _T("scn:"))) != EOF)
		{
			switch(c)
			{
			case 's':
				bRunClient = false;
				break;
			case 'c':
				bRunClient = true;
				break;
			case 'n':
				if (strlen(optarg) > MAX_PATH)
				{
					printf("WARNING: Pipe name is greater than MAX_PATH characters, name may be truncated\n");
				}
				strncpy(szPipeTemp, optarg, 50);
				nStartParams += 2;	// move start parameters out by 2
				break;
			default:
				printf("Ignoring unknown option '%c'\n", c);
				break;
			}
		}

		if (strlen(szPipeTemp) <= 0)
		{
			// Use a default pipe name
			strncpy(szPipeTemp, DEFAULT_PIPE, 50);
		}
	}
	else
	{
		Usage();
		return -1;
	}

	if (!bRunClient)
	{
		_snprintf(g_szPipeName, MAX_PATH, PIPE_FORMAT, ".", szPipeTemp);

		// Do the service thing
		SERVICE_TABLE_ENTRY svcTable[] = { { _T(SERVICE_NAME), ServiceMain }, { NULL, NULL } };

		if (!StartServiceCtrlDispatcher(svcTable))
			return -1;

		return 0;
	}
	else
	{
		_snprintf(g_szPipeName, MAX_PATH, PIPE_FORMAT, argv[nStartParams], szPipeTemp);

		if ((argc - nStartParams) > 2) // Have parameters to pass to the exe to run
		{
			char szParams[MAX_PATH];
			::ZeroMemory(szParams, MAX_PATH);

			for (int i = nStartParams + 2; i < argc; i++)
			{
				strcat(szParams, " ");
				strcat(szParams, (char*)argv[i]);
			}
			
			return RunClient(argv[nStartParams], argv[nStartParams + 1], szParams);
		}
		else
			return RunClient(argv[nStartParams], argv[nStartParams + 1], NULL);
	}


}

