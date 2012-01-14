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
#include "McAfeeControl.h"
#include "AVStatus.h"
#include "ServiceControl.h"

const int ServiceNameCount = 5;
char* arrServers[ServiceNameCount] = {"AlertManager", "McTaskManager", "McAfeeFramework", "mcshield", "Network Associates McShield"};

McAfeeControl::McAfeeControl(LONG nCacheID)
{
	m_nCacheID = nCacheID;
	m_szServiceName = NULL;
}

McAfeeControl::~McAfeeControl()
{

}

int McAfeeControl::GetServiceState(const char* lpszMachine)
{
	SERVICE_STATUS sStatus;
	ServiceControl sc(m_nCacheID);
	int nResult = AV_UNKNOWN;

	for (int i = 0; i < ServiceNameCount; i++)
	{
		char* szService = arrServers[i];
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

bool McAfeeControl::IsServiceInstalled(const char* lpszMachine)
{
	SERVICE_STATUS sStatus;
	ServiceControl sc(m_nCacheID);
	bool bServiceStopped = false;
	
	// Loop through all mcafee services and query them all
	for (int i = 0; i < ServiceNameCount; i++)
	{
		char* szService = arrServers[i];
		if (sc.QueryServiceStatus(lpszMachine, szService, &sStatus) == INSTALLED)
		{
			bServiceStopped = true;
		}
	}

	return bServiceStopped;
}

bool McAfeeControl::IsSpecificServiceInstalled(const char* lpszMachine, const char* szServiceName)
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

bool McAfeeControl::StopService(const char* lpszMachine)
{
	bool bServiceStopped = false;
	ServiceControl sc(m_nCacheID);

	// Loop through all mcafee services and stop them all
	
	for (int i = 0; i < ServiceNameCount; i++)
	{
		if (IsSpecificServiceInstalled(lpszMachine, arrServers[i]))
		{
			if (sc.StopService(lpszMachine, arrServers[i], 30))
			{
				Log.CachedReportError(m_nCacheID, INFO, "Stopped McAfee service \"%s\" successfully\n", arrServers[i]);
				bServiceStopped = true;
			}
		}
	}
	
	if (!bServiceStopped)
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to stop any McAfee services, see previous errors for details.\n");
		return false;
	}

	return bServiceStopped;
}

bool McAfeeControl::StartService(const char* lpszMachine)
{
	bool bServiceStarted = false;
	ServiceControl sc(m_nCacheID);

	// Loop through all mcafee services and stop them all
	
	for (int i = 0; i < ServiceNameCount; i++)
	{
		if (IsSpecificServiceInstalled(lpszMachine, arrServers[i]))
		{
			if (sc.StartService(lpszMachine, arrServers[i], 30))
			{
				Log.CachedReportError(m_nCacheID, INFO, "Started McAfee service \"%s\" successfully\n", arrServers[i]);
				bServiceStarted = true;
			}
		}
	}
	
	if (!bServiceStarted)
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to start any McAfee services, see previous errors for details.\n");
		return false;
	}

	return bServiceStarted;
}
