#pragma once

#include "NetUse.h"

class Impersonator
{
public:
	Impersonator(LONG nCacheID = -1);
	~Impersonator(void);
	bool BeginImpersonation(LPCTSTR lpszUserName, LPCTSTR lpszDomain, LPCTSTR lpszPassword);
	bool StopImpersonation();

private:
	LONG m_nCacheID;
	NetUse* m_pNetUse;
	char m_lpszServer[MAX_PATH + 1];
};
