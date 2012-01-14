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
#include "LogWriter.h"

LogWriter::LogWriter()
{

}

LogWriter::~LogWriter()
{

}

void LogWriter::SetLogFile(char* szLogFile)
{
	LogBase::SetLogFile(szLogFile);

	// Write the start time to the log
	FILE* hFile = fopen(lpszFile, "a");
	SYSTEMTIME st;

	if (hFile == NULL)
	{
		fprintf(stdout, "Error opening output log file %s, disabling further log writing. Error code returned was %d\n", lpszFile, GetLastError());
		bEnableFileWrite = false;
		return;
	}
	GetLocalTime(&st);
	fprintf(hFile, "\n--- fgdump session started on %d/%d/%d at %0.2d:%0.2d:%0.2d ---\n", st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond);
	fprintf(hFile, "--- Command line used: %s ---\n", GetCommandLine()); 
	fclose(hFile);
}

void LogWriter::ReportError(ERROR_LEVEL eLevel, char *pMsg, ...) 
{
	// Writes to the log must be synchronized
	EnterCriticalSection(&csLogWrite);

	va_list ap;
	char* buf;
  
	if (pMsg == NULL) 
	{
		LeaveCriticalSection(&csLogWrite);
		return;
	}
	
	if (eLevel <= nVerbosity)
	{
		va_start(ap, pMsg);
		size_t nLen = _vscprintf(pMsg, ap);
		buf = (char*)malloc(nLen + 1);
		memset(buf, 0, nLen + 1);
		_vsnprintf(buf, nLen, pMsg, ap);

		fprintf(stdout, "%s", buf);
		if (bEnableFileWrite)
		{
			// Also print to file
			FILE* hFile = fopen(lpszFile, "a");
			if (hFile == NULL)
			{
				fprintf(stdout, "Error opening output log file %s, disabling further log writing. Error code returned was %d\n", lpszFile, GetLastError());
				bEnableFileWrite = false;
				LeaveCriticalSection(&csLogWrite);
				return;
			}
			fprintf(hFile, "%s", buf);
			fclose(hFile);
		}
		va_end(ap);
		free(buf);
	}
  
	LeaveCriticalSection(&csLogWrite);
	return;
}

void LogWriter::CachedReportError(int nCacheID, ERROR_LEVEL eLevel, char *pMsg, ...) 
{
	// Stores up data to write, which will be actually written out when FlushCachedWrite
	// is called with the same identifier.
	va_list ap;
	char* buf;
 	StringArray* pArray;
 
	if (pMsg == NULL) 
	{
		return;
	}

	if (eLevel <= nVerbosity)
	{
		va_start(ap, pMsg);

		if (nCacheID == -1)
		{
			//ReportError(eLevel, pMsg, ap);	// Lets us easily used normal writing in case of a bad cache ID
			size_t nLen = _vscprintf(pMsg, ap);
			buf = (char*)malloc(nLen + 1);
			memset(buf, 0, nLen + 1);
			_vsnprintf(buf, nLen, pMsg, ap);

			fprintf(stdout, "%s", buf);
			if (bEnableFileWrite)
			{
				// Also print to file
				FILE* hFile = fopen(lpszFile, "a");
				if (hFile == NULL)
				{
					fprintf(stdout, "Error opening output log file %s, disabling further log writing. Error code returned was %d\n", lpszFile, GetLastError());
					bEnableFileWrite = false;
					//LeaveCriticalSection(&csLogWrite);
					return;
				}
				fprintf(hFile, "%s", buf);
				fclose(hFile);
			}
			//va_end(ap);
			free(buf);
		}
		else
		{
			size_t nLen = _vscprintf(pMsg, ap);
			buf = (char*)malloc(nLen + 1);
			memset(buf, 0, nLen + 1);
			_vsnprintf(buf, nLen, pMsg, ap);

			// Add buf to string array
			pArray = map[nCacheID];
			pArray->Add(buf);
			free(buf);
		}

		va_end(ap);
	}
}