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
#include "ProtectedStorageControl.h"
#include "Process.h"

ProtectedStorageControl::ProtectedStorageControl(LONG nCacheID)
{
	m_nCacheID = nCacheID;
}

ProtectedStorageControl::~ProtectedStorageControl(void)
{

}

bool ProtectedStorageControl::Execute(const char* lpszPSExecPath, const char* lpszDumpPath, char* lpszMachine, char* szUser, char* szPassword, char* lpszPipeName)
{
	const char* lpszCmdLineFormat = " -c -n %s %s \"%s\\pstgdump.exe\" -q -u %s -p %s";
	const char* lpszStopCmdLine = lpszPSExecPath;
	int nArgSize = _scprintf(lpszCmdLineFormat, lpszPipeName, lpszMachine, lpszDumpPath, szUser, szPassword);
	char* lpszParams;
	bool result = false;

	lpszParams = new char[nArgSize + 1];
	memset(lpszParams, 0, nArgSize + 1);

	try
	{
		Process p;
		_snprintf(lpszParams, nArgSize, lpszCmdLineFormat, lpszPipeName, lpszMachine, lpszDumpPath, szUser, szPassword);

		HANDLE hProcess = p.CreateProcess(lpszStopCmdLine, lpszParams);
		if (hProcess != 0)
		{
			DWORD dwResult = WaitForSingleObject(hProcess, 600000);	// Wait 10 minutes for process to complete
			if (dwResult != WAIT_OBJECT_0)
			{
				Log.CachedReportError(m_nCacheID, CRITICAL, "Warning: protected storage dump did not complete in a timely manner - exiting");
				result = false;
			}
			else
			{
				// Read from process's output
				char* szResult;
				int nSize = 65535;

				szResult = new char[nSize];
				memset(szResult, 0, nSize);
				p.ReadFromPipe(&szResult, nSize);

				// Was it successful?
				if (strstr(szResult, "Successfully dumped") != NULL)
				{
					// Success
					// Write results to a file
					size_t nLen = strlen(lpszMachine) + 9;		// 10 chars accounts for ".pstgdump" extension
					char* szTempFilename = new char[nLen + 1];
					memset(szTempFilename, 0, nLen + 1);
					_snprintf(szTempFilename, nLen, "%s.pstgdump", lpszMachine);

					std::ofstream outputFile(szTempFilename, std::ios::out | std::ios::trunc);
					outputFile.write((const char*)szResult, (DWORD)strlen(szResult));
					outputFile.close();
					delete [] szTempFilename;
					Log.CachedReportError(m_nCacheID, CRITICAL, "Protected storage dumped successfully\n", lpszMachine);
					result = true;
				}
				else
				{
					// Failed
					Log.CachedReportError(m_nCacheID, CRITICAL, "Failed to dump protected storage (the text returned follows):\n%s", szResult);
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

