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
#ifndef _FGDUMP_H
#define _FGDUMP_H

#include "stdafx.h"
#include "McAfeeControl.h"
#include "SymantecAVControl.h"
#include "NetUse.h"
#include "PWDumpControl.h"
#include "StringArray.h"
#include "AVStatus.h"

typedef struct _tWorkerThreadData
{
	class FGDump* pParent;
	char* lpszUser;
	char* lpszPassword;
	char* lpszServer;
	HANDLE hStartProcessing;
	HANDLE hFinished;
} WORKER_THREAD_DATA, *LPWORKER_THREAD_DATA;

class FGDump
{
public:
	FGDump();
	~FGDump();

	char lpszTempPath[MAX_PATH];
	char lpszPWServicePath[MAX_PATH + 15];
	char lpszPWService64Path[MAX_PATH + 15];
	char lpszPWDumpPath[MAX_PATH + 15];
	char lpszPSServicePath[MAX_PATH + 15];
	char lpszLSAExtPath[MAX_PATH + 15];
	char lpszLSAExt64Path[MAX_PATH + 15];
	char lpszCacheDumpPath[MAX_PATH + 15];
	char lpszCacheDump64Path[MAX_PATH + 15];
	char lpszPStoragePath[MAX_PATH + 15];
	char lpszFGExecPath[MAX_PATH + 15];
	bool bFullRun, bRunPwdump, bRunCachedump, bRunPStgDump, bSkipExisting, bContinueOnUnknownAV;
	bool bSkipPwdumpHistory;
	bool bRunLocal;
	bool bSkipAVCheck;
	int	 nOSBits;

	void SetTestOnlyAV(bool bTestOnly);
	void SetSkipCacheDump(bool bSkip);
	void SetSkipPWDump(bool bSkip);
	void SetSkipProtectedStorageDump(bool bSkip);
	void SetIgnoreExistingFiles(bool bIgnoreExisting);
	void SetHostfileName(char* szFile);
	void SetPerHostFileName(char* szFile);
	void SetServer(char* szServer);
	void SetUser(char* szUsername);
	void SetPassword(char* szPassword);
	void SetWorkerThreads(unsigned int nThreads);
	void SetSkipPwdumpHistory(bool bSkipHistory);
	void ContinueOnUnknownAV(bool bContinue);
	void SetSkipAVCheck(bool bSkipAV);
	int Run();
	void ReportServerSuccess(char* lpszServer);
	void ReportServerFailure(char* lpszServer);
	void SetOSArchOverride(int nBits);


private:
	CRITICAL_SECTION csThreadCreate;
	CRITICAL_SECTION csReportServerResults;
	char* lpszServer;
	char* lpszSuccessfulServers;
	char* lpszFailedServers;
	char* lpszUser;
	char* lpszPassword;
	char lpszSourceFile[MAX_PATH + 1];
	bool bUsePerHostCreds;
	HANDLE* phWorkerThreadsFinished;
	WORKER_THREAD_DATA* pThreadData;
	unsigned int nWorkerThreads;
	int nSuccesses;
	int nFailures;
	StringArray arrSuccess;
	StringArray	arrFailed;
	McAfeeControl objMcafee;
	SymantecAVControl objSymantec;
	bool bDisabledAV;

	void ExitApp(int nReturnCode);

	char* CreateSessionID();
	bool DispatchWorkerThread(char* lpszServerToDump, char* lpszUser, char* lpszPassword);
	void WaitForAllThreadsToFinish();
	static DWORD WINAPI ThreadProc(LPVOID lpParameter);

public:
	void CreateThreadPool(void);
};

#endif