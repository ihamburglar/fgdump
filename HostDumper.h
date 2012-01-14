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
#pragma once
#ifndef _HOSTDUMPER_H
#define _HOSTDUMPER_H

#include "fgdump.h"
#include "SophosControl.h"
#include "TrendControl.h"
#include "McAfeeControl.h"
#include "SymantecAVControl.h"
#include "NetUse.h"
#include "PWDumpControl.h"

typedef struct _tControlObjects
{
	TrendControl* objTrendService;
	SophosControl* objSophosService;
	McAfeeControl* objMcAfeeService;
	SymantecAVControl* objSAVService;
	NetUse* objNetUse;
	PWDumpControl* objPWDump;
} CONTROL_OBJECTS, *LPCONTROL_OBJECTS;

class HostDumper
{
public:
	HostDumper(char* lpszServerToDump, FGDump* pParent);
	~HostDumper(void);
	int DumpServer(char* lpszUser, char* lpszPassword);

private:
	LONG nCacheID;	// ID for the cached log writing
	CONTROL_OBJECTS sControls;
	FGDump* fgdumpMain;
	char lpszServer[MAX_PATH];
	char lpszRemotePath[MAX_PATH];
	//char szDriveTemp[3];
	char* lpszCacheDumpRemotePath;
	char* lpszUNCRemotePath;
	bool bRunLocal;

	bool RunCacheDump(char* lpszTempPath, bool bIs64Bit, char* lpszPipeName = NULL);
	bool RunProtectedStorageDump(char* lpszTempPath, char* lpszUser, char* lpszPassword, char* lpszPipeName);
	bool StopAndRemoveFGExec(bool* bIsFgexecStillInstalled);
	bool InstallAndStartFGExec(const char* lpszPipeName, bool* bIsFgexecStillInstalled);
	bool FileExists(char* szFile);
	void ExitApp(int nReturnCode);

public:
	void FlushOutput(void);
};

#endif
