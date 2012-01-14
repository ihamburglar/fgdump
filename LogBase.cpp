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
#include "LogBase.h"

LogBase::LogBase()
{
	bEnableFileWrite = false;
	memset(lpszFile, 0, MAX_PATH + 1);
	nVerbosity = 0;
	InitializeCriticalSection(&csLogWrite);
	nCacheID = 0;	// Starting identifier
}

LogBase::~LogBase()
{
	StringArray* pArray;
	hash_map <LONG, StringArray*>::iterator pIter;

	// Delete the heap-allocated structures
	for (pIter = map.begin(); pIter != map.end(); pIter++)
	{
		pArray = pIter->second;
		delete pArray;
	}

	// Clean out the map
	map.clear();

	DeleteCriticalSection(&csLogWrite);
}
char* LogBase::GetLogFile()
{
	return lpszFile;
}

void LogBase::SetLogFile(char* szLogFile)
{
	strncpy(lpszFile, (const char*)szLogFile, MAX_PATH);
}

void LogBase::SetWriteToFile(bool bWriteToFile)
{
	bEnableFileWrite = bWriteToFile;
}

void LogBase::IncreaseVerbosity()
{
	nVerbosity++;
}

int LogBase::BeginCachedWrite(void)
{
	// Called by a worker thread to get an identifier to perform cached log writes
	// This prevents data from getting muddled with each other. When a thread is done,
	// it calls the FlushCachedWrite function to dump all of its data at once.

	// Give the caller an ID to use when writing cached output
	// Must be a thread safe increment operation here
	InterlockedIncrement(&nCacheID);

	// Allocate a new string array
	StringArray* pNewArray = new StringArray();
	map[nCacheID] = pNewArray;
	
	return nCacheID;
}

void LogBase::FlushCachedWrite(int nCacheID)
{
	// Writes all pending data to the log
	// Must be synchronized along with calls to ReportError
	StringArray* pArray;

	EnterCriticalSection(&csLogWrite);
	pArray = map[nCacheID];
	char* szTemp = pArray->GetFirstString();
	if (szTemp != NULL)
	{
		while (szTemp != NULL)
		{
			// These are reported at a "critical" level, since they were filtered within the CachedReportError function
			Log.ReportError(CRITICAL, "%s", szTemp);
			szTemp = pArray->GetNextString();
		}
	}

	LeaveCriticalSection(&csLogWrite);
}