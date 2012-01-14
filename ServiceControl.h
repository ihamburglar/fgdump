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

enum FG_SERVICE_STATUS
{
	UNKNOWN,
	INSTALLED,
	NOT_INSTALLED
};

class ServiceControl
{
public:
	ServiceControl(LONG nCacheID = -1);
	~ServiceControl(void);
	FG_SERVICE_STATUS QueryServiceStatus(const char* szMachine, const char* szServiceShortName, SERVICE_STATUS* lpStatus);
	bool StartService(const char* szMachine, const char* szServiceShortName, int nMaxWait);
	bool StopService(const char* szMachine, const char* szServiceShortName, int nMaxWait);
	bool InstallService(const char* szMachine, const char* szServiceName, const char* szDisplayName, const char* szBinaryPath);
	bool UninstallService(const char* szMachine, const char* szServiceShortName);

private:
	LONG m_nCacheID;
};
