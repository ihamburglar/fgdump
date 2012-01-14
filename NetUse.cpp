#include "StdAfx.h"
#include "netuse.h"
#include "Process.h"

NetUse::NetUse(LONG nCacheID)
{
	m_nCacheID = nCacheID;
}

NetUse::~NetUse(void)
{
}

bool NetUse::Connect(const char* lpszMachine, const char* lpszUserName, const char* lpszPassword)
{
	char szConnectPath[MAX_PATH + 1];
	NETRESOURCE rec;
	int rc;

	memset(szConnectPath, 0, MAX_PATH + 1);
	_snprintf(szConnectPath, MAX_PATH, "\\\\%s\\ipc$", lpszMachine);

	rec.dwType = RESOURCETYPE_ANY;
	rec.lpLocalName = NULL;
	rec.lpRemoteName = szConnectPath;
	rec.lpProvider = NULL;

	rc = WNetUseConnection(NULL, &rec, lpszPassword, lpszUserName, 0, NULL, NULL, NULL);
	if(rc != NO_ERROR)
	{
		//Log.ReportError(CRITICAL, "Unable to log on to %s: error %d\n", lpszMachine, rc);
		ErrorHandler("NetUse", "Unable to log on to host", rc, m_nCacheID);
		return false;
	}

	return true;
}

bool NetUse::Disconnect(const char* lpszMachine)
{
	int rc;
	char szConnectPath[MAX_PATH + 1];

	memset(szConnectPath, 0, MAX_PATH + 1);
	_snprintf(szConnectPath, MAX_PATH, "\\\\%s\\ipc$", lpszMachine);
	
	rc = WNetCancelConnection2(szConnectPath, 0, TRUE);
	if(rc != NO_ERROR && rc != ERROR_NOT_CONNECTED)
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to unbind from IPC$ on %s (YOU MAY NEED TO DO THIS BY HAND!) Error %d\n", lpszMachine, rc);
		return false;
	}

	return true;
}
