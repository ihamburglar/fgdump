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
#include ".\servicecontrol.h"

ServiceControl::ServiceControl(LONG nCacheID)
{
	m_nCacheID = nCacheID;
}

ServiceControl::~ServiceControl(void)
{
}

FG_SERVICE_STATUS ServiceControl::QueryServiceStatus(const char* szMachine, const char* szServiceShortName, SERVICE_STATUS* lpStatus)
{
	SC_HANDLE hSCM = NULL;
	SC_HANDLE hService = NULL;
	bool bRet;
	LPCTSTR szServer = NULL;

	::ZeroMemory(lpStatus, sizeof(SERVICE_STATUS));

	if (strcmp(szMachine, "127.0.0.1") == 0)
		szServer = NULL;	// Localhost
	else
		szServer = szMachine;

	hSCM = OpenSCManager(szServer, NULL, GENERIC_READ);
	if (NULL == hSCM)
	{
		int nError = GetLastError();
		if (nError == 1722)
		{
			// This is the "RPC server is unavailable message". Adding a special error for clarity
			Log.CachedReportError(m_nCacheID, CRITICAL, "Could not connect to service manager: this may be a Win95, 98, SNAP or other non-NT-based system.\n");
		}
		else
			ErrorHandler("QueryServiceStatus", "OpenSCManager", GetLastError(), m_nCacheID);
		
		return UNKNOWN;
	}

	hService = OpenService(hSCM, szServiceShortName, SERVICE_QUERY_STATUS);
	if (NULL == hService)
	{
		//printf("Unable to open service %s on %s\n", szServiceShortName, szMachine);
		CloseServiceHandle(hSCM);
		return NOT_INSTALLED;
	}

	bRet = ::QueryServiceStatus(hService, lpStatus);
	if (!bRet)
	{
		ErrorHandler("QueryServiceStatus", "QueryServiceStatus", GetLastError(), m_nCacheID);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return NOT_INSTALLED;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return INSTALLED;
}

bool ServiceControl::StartService(const char* szMachine, const char* szServiceShortName, int nMaxWait)
{
	SC_HANDLE hSCM = NULL;
	SC_HANDLE hService = NULL;
	SERVICE_STATUS sStatus;
	bool bRet;
	int nTimeWaited = 0;
	LPCTSTR szServer = NULL;

	if (strcmp(szMachine, "127.0.0.1") == 0)
		szServer = NULL;	// Localhost
	else
		szServer = szMachine;

	hSCM = OpenSCManager(szServer, NULL, GENERIC_READ);
	if (NULL == hSCM)
	{
		int nError = GetLastError();
		if (nError == 1722)
		{
			// This is the "RPC server is unavailable message". Adding a special error for clarity
			Log.CachedReportError(m_nCacheID, CRITICAL, "Could not connect to service manager: this may be a Win95, 98, SNAP or other non-NT-based system.\n");
		}
		else
			ErrorHandler("StartService", "OpenSCManager", GetLastError(), m_nCacheID);
		
		return false;
	}

	hService = OpenService(hSCM, szServiceShortName, GENERIC_EXECUTE | SERVICE_QUERY_STATUS);
	if (NULL == hService)
	{
		ErrorHandler("StartService", "OpenService", GetLastError(), m_nCacheID);
		CloseServiceHandle(hSCM);
		return false;
	}

	bRet = ::StartService(hService, 0, NULL);
	if (!bRet)
	{
		int nError = GetLastError();
		if (nError == 1056)	// Already running - previous failure?
		{
			Log.CachedReportError(m_nCacheID, INFO, "'%s' was already running on %s\n", szServiceShortName, szMachine);
			CloseServiceHandle(hSCM);
			return true;
		}

		ErrorHandler("StartService", "StartService", GetLastError(), m_nCacheID);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return false;
	}

	bRet = ::QueryServiceStatus(hService, &sStatus);
	if (!bRet)
	{
		ErrorHandler("StartService", "QueryServiceStatus", GetLastError(), m_nCacheID);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return false;
	}

	while(SERVICE_RUNNING != sStatus.dwCurrentState && nTimeWaited < nMaxWait)
	{
		Sleep(1000);
		bRet = ::QueryServiceStatus(hService, &sStatus);
		if (!bRet)
		{
			ErrorHandler("StartService", "QueryServiceStatus", GetLastError(), m_nCacheID);
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCM);
			return false;
		}

		nTimeWaited++;
	}

	if (SERVICE_RUNNING != sStatus.dwCurrentState)
	{
		ErrorHandler("StartService", "QueryServiceStatus", GetLastError(), m_nCacheID);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return false;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return true;
}

bool ServiceControl::StopService(const char* szMachine, const char* szServiceShortName, int nMaxWait)
{
	SC_HANDLE hSCM = NULL;
	SC_HANDLE hService = NULL;
	SERVICE_STATUS sStatus;
	bool bRet;
	int nTimeWaited = 0;
	LPCTSTR szServer = NULL;

	if (strcmp(szMachine, "127.0.0.1") == 0)
		szServer = NULL;	// Localhost
	else
		szServer = szMachine;

	hSCM = OpenSCManager(szServer, NULL, GENERIC_READ);
	if (NULL == hSCM)
	{
		int nError = GetLastError();
		if (nError == 1722)
		{
			// This is the "RPC server is unavailable message". Adding a special error for clarity
			Log.CachedReportError(m_nCacheID, CRITICAL, "Could not connect to service manager: this may be a Win95, 98, SNAP or other non-NT-based system.\n");
		}
		else
			ErrorHandler("StopService", "OpenSCManager", GetLastError(), m_nCacheID);
		
		return false;
	}

	hService = OpenService(hSCM, szServiceShortName, GENERIC_EXECUTE | SERVICE_QUERY_STATUS);
	if (NULL == hService)
	{
		ErrorHandler("StopService", "OpenService", GetLastError(), m_nCacheID);
		CloseServiceHandle(hSCM);
		return false;
	}

	bRet = ::ControlService(hService, SERVICE_CONTROL_STOP, &sStatus);
	if (!bRet)
	{
		ErrorHandler("StopService", "StopService", GetLastError(), m_nCacheID);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return false;
	}

	while(SERVICE_STOPPED != sStatus.dwCurrentState && nTimeWaited < nMaxWait)
	{
		Sleep(1000);
		bRet = ::QueryServiceStatus(hService, &sStatus);
		if (!bRet)
		{
			ErrorHandler("StopService", "QueryServiceStatus", GetLastError(), m_nCacheID);
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCM);
			return false;
		}

		nTimeWaited++;
	}

	if (SERVICE_STOPPED != sStatus.dwCurrentState)
	{
		ErrorHandler("StopService", "QueryServiceStatus", GetLastError(), m_nCacheID);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return false;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return true;
}

bool ServiceControl::InstallService(const char* szMachine, const char* szServiceName, const char* szDisplayName, const char* szBinaryPath)
{
	SC_HANDLE hSCM = NULL;
	SC_HANDLE hService = NULL;
	LPCTSTR szServer = NULL;

	if (strcmp(szMachine, "127.0.0.1") == 0)
		szServer = NULL;	// Localhost
	else
		szServer = szMachine;

	hSCM = OpenSCManager(szServer, NULL, SC_MANAGER_CREATE_SERVICE);
	if (NULL == hSCM)
	{
		int nError = GetLastError();
		if (nError == 1722)
		{
			// This is the "RPC server is unavailable message". Adding a special error for clarity
			Log.CachedReportError(m_nCacheID, CRITICAL, "Could not connect to service manager: this may be a Win95, 98, SNAP or other non-NT-based system.\n");
		}
		else
			ErrorHandler("InstallService", "OpenSCManager", GetLastError(), m_nCacheID);
		
		return false;
	}
		
	hService = CreateService(hSCM, szServiceName, szDisplayName, GENERIC_READ, SERVICE_WIN32_OWN_PROCESS,
							 SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, szBinaryPath, NULL, NULL, NULL, NULL, NULL);
	if (NULL == hService)
	{
		int nError = GetLastError();
		if (nError == 1073)	// Already installed - previous failure?
		{
			Log.CachedReportError(m_nCacheID, INFO, "'%s' was already installed on %s, using that service\n", szServiceName, szMachine);
			CloseServiceHandle(hSCM);
			return true;
		}
		ErrorHandler("InstallService", "CreateService", nError, m_nCacheID);
		CloseServiceHandle(hSCM);
		return false;
	}
	
	Log.CachedReportError(m_nCacheID, DEBUG, "Successfully installed service '%s' on %s\n", szServiceName, szMachine);
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return true;
}

bool ServiceControl::UninstallService(const char* szMachine, const char* szServiceShortName)
{
	SC_HANDLE hSCM = NULL;
	SC_HANDLE hService = NULL;
	bool bRet;
	LPCTSTR szServer = NULL;

	if (strcmp(szMachine, "127.0.0.1") == 0)
		szServer = NULL;	// Localhost
	else
		szServer = szMachine;

	hSCM = OpenSCManager(szServer, NULL, SC_MANAGER_CONNECT);
	if (NULL == hSCM)
	{
		int nError = GetLastError();
		if (nError == 1722)
		{
			// This is the "RPC server is unavailable message". Adding a special error for clarity
			Log.CachedReportError(m_nCacheID, CRITICAL, "Could not connect to service manager: this may be a Win95, 98, SNAP or other non-NT-based system.\n");
		}
		else
			ErrorHandler("UninstallService", "OpenSCManager", GetLastError(), m_nCacheID);
		
		return false;
	}
		
	hService = OpenService(hSCM, szServiceShortName, DELETE);
	if (NULL == hService)
	{
		ErrorHandler("UninstallService", "OpenService", GetLastError(), m_nCacheID);
		CloseServiceHandle(hSCM);
		return false;
	}

	bRet = ::DeleteService(hService);
	if (!bRet)
	{
		ErrorHandler("UninstallService", "DeleteService", GetLastError(), m_nCacheID);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return false;
	}

	Log.CachedReportError(m_nCacheID, DEBUG, "Successfully uninstalled service '%s' on %s\n", szServiceShortName, szMachine);
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return true;
}
