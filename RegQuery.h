#pragma once

class RegQuery
{
public:
	RegQuery();
	~RegQuery(void);
	static bool GetOSVersion(char* szServer, char** lplpVersion, LONG nCacheID, bool* bIs64Bit);

private:
	static LPCTSTR MapRegValueToOS(LPCTSTR szRegValue);
};
