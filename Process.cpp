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
#include "StdAfx.h"
#include ".\process.h"

Process::Process(void)
{
	hThread = 0;
	hProcess = 0;
}

Process::~Process(void)
{
	if (hThread != 0)
		CloseHandle(hThread);
	if (hProcess != 0)
		CloseHandle(hProcess);
}

HANDLE Process::CreateProcess(const char* lpszCommandLine, char* lpszArguments)
{
	SECURITY_ATTRIBUTES saAttr; 
	BOOL fSuccess; 

	// Set the bInheritHandle flag so pipe handles are inherited. 
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL; 

	// Get the handle to the current STDOUT. 
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE); 

	// Create a pipe for the child process's STDOUT. 
	if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) 
		return 0; 

	// Create noninheritable read handle and close the inheritable read 
	// handle. 
	fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdoutRd,
								GetCurrentProcess(), &hChildStdoutRdDup , 0,
								FALSE,
								DUPLICATE_SAME_ACCESS);
	if( !fSuccess )
		return 0;
	
	CloseHandle(hChildStdoutRd);

	// Now create the child process. 
	HANDLE hProcess = CreateChildProcess(lpszCommandLine, lpszArguments);
	
	return hProcess;	
}
 
HANDLE Process::CreateChildProcess(const char* lpszCommandLine, char* lpszArguments) 
{ 
	PROCESS_INFORMATION piProcInfo; 
	STARTUPINFO siStartInfo;
	BOOL bFuncRetn = FALSE; 
	char szCurrentProcessDir[MAX_PATH + 1];
	char* szLastSlash;
	size_t nCurrentDirLen;

	ZeroMemory(szCurrentProcessDir, MAX_PATH + 1);
	szLastSlash = strrchr(lpszCommandLine, '\\');
	if (szLastSlash != NULL)
	{
		nCurrentDirLen = szLastSlash - lpszCommandLine;
		strncpy(szCurrentProcessDir, lpszCommandLine, nCurrentDirLen);
	}
	// Set up members of the PROCESS_INFORMATION structure. 
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// Set up members of the STARTUPINFO structure. 
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO); 
	siStartInfo.hStdError = hChildStdoutWr;
	siStartInfo.hStdOutput = hChildStdoutWr;
	siStartInfo.hStdInput = NULL;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process. 
	bFuncRetn = ::CreateProcess(lpszCommandLine, // Command line
								lpszArguments, // command args 
								NULL,          // process security attributes 
								NULL,          // primary thread security attributes 
								TRUE,          // handles are inherited 
								0,             // creation flags 
								NULL,          // use parent's environment 
								szCurrentProcessDir, // set the current directory to the one from which the exe will run 
								&siStartInfo,  // STARTUPINFO pointer 
								&piProcInfo);  // receives PROCESS_INFORMATION 

	if (bFuncRetn == 0) 
	{
		return 0;
	}
	else 
	{
		hThread = piProcInfo.hThread;
		hProcess = piProcInfo.hProcess;
		return piProcInfo.hProcess;
	}
}
 
DWORD Process::ReadFromPipe(char** lplpszBuffer, int nSize) 
{ 
	DWORD dwRead; 

	ReadFile(hChildStdoutRdDup, *lplpszBuffer, nSize, &dwRead, NULL);
	
	return dwRead;
} 

