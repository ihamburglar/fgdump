#include "StdAfx.h"
#include ".\impersonator.h"

Impersonator::Impersonator(LONG nCacheID)
{
	m_nCacheID = nCacheID;
	memset(m_lpszServer, 0, MAX_PATH + 1);
	m_pNetUse = new NetUse(nCacheID);
}

Impersonator::~Impersonator(void)
{
	StopImpersonation();
	delete m_pNetUse;
}

bool Impersonator::BeginImpersonation(LPCTSTR lpszServer, LPCTSTR lpszUserName, LPCTSTR lpszPassword)
{
	if (!m_pNetUse->Connect(lpszServer, lpszUserName, lpszPassword) == true)
	{
		//::ErrorHandler("NetUse", "Connect", GetLastError());
		return false;
	}

	memcpy(m_lpszServer, lpszServer, strlen(lpszServer));

	return true;
}

bool Impersonator::StopImpersonation()
{
	if (*m_lpszServer == 0)
		return false;

	if (!m_pNetUse->Disconnect(m_lpszServer) == true)
	{
		memset(m_lpszServer, 0, MAX_PATH + 1);
		//::ErrorHandler("NetUse", "Disconnect", GetLastError());
		return false;
	}

	memset(m_lpszServer, 0, MAX_PATH + 1);
	return true;
}
