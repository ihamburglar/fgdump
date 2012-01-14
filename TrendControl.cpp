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
#include "TrendControl.h"
#include "AVStatus.h"
#include "ServiceControl.h"

const int ServiceNameCount = 5;
char* arrTrendServers[ServiceNameCount] = {"ntrtscan", "tmlisten", "EarthAgent", "SpntSvc", "ofcservice"};

TrendControl::TrendControl(LONG nCacheID)
{
	m_nCacheID = nCacheID;
	m_szServiceName = NULL;
}

TrendControl::~TrendControl()
{

}

int TrendControl::GetServiceState(const char* lpszMachine)
{
	SERVICE_STATUS sStatus;
	ServiceControl sc(m_nCacheID);
	int nResult = AV_UNKNOWN;

	for (int i = 0; i < ServiceNameCount; i++)
	{
		char* szService = arrTrendServers[i];
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

bool TrendControl::IsServiceInstalled(const char* lpszMachine)
{
	SERVICE_STATUS sStatus;
	ServiceControl sc(m_nCacheID);
	bool bServiceStopped = false;
	
	// Loop through all Trend services and query them all
	for (int i = 0; i < ServiceNameCount; i++)
	{
		char* szService = arrTrendServers[i];
		if (sc.QueryServiceStatus(lpszMachine, szService, &sStatus) == INSTALLED)
		{
			bServiceStopped = true;
		}
	}

	return bServiceStopped;
}

bool TrendControl::IsSpecificServiceInstalled(const char* lpszMachine, const char* szServiceName)
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

bool TrendControl::StopService(const char* lpszMachine)
{
	bool bServiceStopped = false;
	ServiceControl sc(m_nCacheID);

	// Loop through all Trend services and stop them all
	
	for (int i = 0; i < ServiceNameCount; i++)
	{
		if (IsSpecificServiceInstalled(lpszMachine, arrTrendServers[i]))
		{
			if (sc.StopService(lpszMachine, arrTrendServers[i], 30))
			{
				//Sleep(3000);	// TEMP: Does this improve the ability to dump some boxen?
				Log.CachedReportError(m_nCacheID, INFO, "Stopped Trend service \"%s\" successfully\n", arrTrendServers[i]);
				bServiceStopped = true;
			}
		}
	}
	
	if (!bServiceStopped)
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to stop any Trend services, see previous errors for details.\n");
		return false;
	}

	return bServiceStopped;
}

bool TrendControl::StartService(const char* lpszMachine)
{
	bool bServiceStarted = false;
	ServiceControl sc(m_nCacheID);

	// Loop through all Trend services and stop them all
	
	for (int i = 0; i < ServiceNameCount; i++)
	{
		if (IsSpecificServiceInstalled(lpszMachine, arrTrendServers[i]))
		{
			if (sc.StartService(lpszMachine, arrTrendServers[i], 30))
			{
				Log.CachedReportError(m_nCacheID, INFO, "Started Trend service \"%s\" successfully\n", arrTrendServers[i]);
				bServiceStarted = true;
			}
		}
	}
	
	if (!bServiceStarted)
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to start any Trend services, see previous errors for details.\n");
		return false;
	}

	return bServiceStarted;
}
