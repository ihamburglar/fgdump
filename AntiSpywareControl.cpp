/******************************************************************************
fgdump - by fizzgig and the foofus.net group
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

#include "StdAfx.h"
#include "AntiSpywareControl.h"
#include "Process.h"

AntiSpywareControl::AntiSpywareControl(void)
{
}

AntiSpywareControl::~AntiSpywareControl(void)
{
}

bool AntiSpywareControl::Stop(const char* lpszFGExecPath, const char* lpszExePath, char* lpszMachine)
{
	const char* lpszCmdLineFormat = " %s \"%s\\killmsas.exe\" kill";
	return Execute(lpszFGExecPath, lpszExePath, lpszMachine, lpszCmdLineFormat);
}

bool AntiSpywareControl::Start(const char* lpszFGExecPath, const char* lpszExePath, char* lpszMachine)
{
	const char* lpszCmdLineFormat = " %s \"%s\\killmsas.exe\" start";
	return Execute(lpszFGExecPath, lpszExePath, lpszMachine, lpszCmdLineFormat);
}

bool AntiSpywareControl::Execute(const char* lpszFGExecPath, const char* lpszExePath, char* lpszMachine, const char* lpszCmdLineFormat)
{
	const char* lpszStopCmdLine = lpszFGExecPath;
	int nArgSize = _scprintf(lpszCmdLineFormat, lpszMachine, lpszExePath);
	char* lpszParams;
	bool bResult = false;

	lpszParams = new char[nArgSize + 1];
	memset(lpszParams, 0, nArgSize + 1);

	try
	{
		Process p;
		_snprintf(lpszParams, nArgSize, lpszCmdLineFormat, lpszMachine, lpszExePath);

		HANDLE hProcess = p.CreateProcess(lpszStopCmdLine, lpszParams);
		if (hProcess != 0)
		{
			DWORD dwResult = WaitForSingleObject(hProcess, 1200000);	// Wait 20 minutes for process to complete
			if (dwResult != WAIT_OBJECT_0)
			{
				Log.ReportError(CRITICAL, "Warning: cachedump did not complete in a timely manner - exiting");
				bResult = false;
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
				if (strstr(szResult, "successfully removed") != NULL)
				{
					// Success
					// Write results to a file
					size_t nLen = strlen(lpszMachine) + 10;		// 10 chars accounts for ".cachedump" extension
					char* szTempFilename = new char[nLen + 1];
					memset(szTempFilename, 0, nLen + 1);
					_snprintf(szTempFilename, nLen, "%s.cachedump", lpszMachine);

					std::ofstream outputFile(szTempFilename, std::ios::out | std::ios::trunc);
					outputFile.write((const char*)szResult, (DWORD)strlen(szResult));
					outputFile.close();
					delete [] szTempFilename;
					Log.ReportError(CRITICAL, "Cache dumped successfully\n", lpszMachine);
					bResult = true;
				}
				else
				{
					// Failed
					Log.ReportError(CRITICAL, "Failed to dump cache: %s\n", szResult);
					bResult = false;
				}

				delete [] szResult;
			}
		}
		else
			bResult = false;
	}
	catch(...)
	{
		bResult = false;
	}

	delete [] lpszParams;

	return bResult;
}
