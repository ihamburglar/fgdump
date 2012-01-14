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
#include "LogFailedWriter.h"

LogFailedWriter::LogFailedWriter()
{

}

LogFailedWriter::~LogFailedWriter()
{

}

void LogFailedWriter::WriteFailedHost(char* szHost, int nErrorCode, bool bRequiresManualFixing, char* szDescription)
{
	// Writes to the log must be synchronized
	EnterCriticalSection(&csLogWrite);

	FILE* hFile = fopen(lpszFile, "a");
	if (hFile == NULL)
	{
		fprintf(stdout, "Error opening failed output log file %s, disabling further log writing. Error code returned was %d\n", lpszFile, GetLastError());
		bEnableFileWrite = false;
		LeaveCriticalSection(&csLogWrite);
		return;
	}
	fprintf(hFile, "%s|%d|%d|%s\n", szHost, nErrorCode, bRequiresManualFixing ? 1 : 0, szDescription);
	fclose(hFile);

	LeaveCriticalSection(&csLogWrite);
}
