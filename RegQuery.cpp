#include "StdAfx.h"
#include ".\regquery.h"

#define VERSION_KEY		"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"
#define OS_OPTIONS_KEY	"SYSTEM\\CurrentControlSet\\Control\\ProductOptions"
#define IS_64BIT_KEY	"SOFTWARE\\Wow6432Node"
#define IS_64BIT_PROC_KEY "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"

RegQuery::RegQuery()
{

}

RegQuery::~RegQuery(void)
{
}

bool RegQuery::GetOSVersion(char* szServer, char** lplpVersion, LONG nCacheID, bool* bIs64Bit)
{
	HKEY hRemote, hVersion, hOptions;
	DWORD dwSize; 
	char* szType = NULL;
	char* szVersion = NULL;
	char* szBuild = NULL;
	char* szServicePack = NULL;
	long nResult;
	char test[256];

	/******************************************************
	/ NOTE:
    /
	/ Because of what Joe did with his crazy Samba server,
	/ it is necessary to make the buffers for the values
	/ returned by RegQueryValueEx twice as big as they should
	/ need to be. What happens is this (assuming the szType
	/ query):
	/
	/ 1) We query to see what the length of the value to be
	/    returned is. For Joe, this was 9, as the value
	/    was suppposed to be "ServerNT"
	/ 2) I allocated a buffer based on that amount, even
	/    with a null terminator at one point.
	/ 3) A subsequent call, with dwSize still equal to 9, caused
	/    the second call to push 18 characters to the buffer, even
	/    though I only asked for 9! nResult was 0 (should have been 
	/    ERROR_MORE_DATA perhaps?
	/ 4) Examination of the memory shows "ServerNT", a NULL byte,
	/    then the remainder of the string as Unicode! How
	/    was this allowed in the first place??
	/ 5) Needless to say, the free() call did not like this...
	/
	/ To address this, the buffers are twice the size. Null
	/ terminators are in place, and the data is treated as normal
	/ ANSI. This works, but something strange is afoot.
	/******************************************************/

	if (strcmp(szServer, "127.0.0.1") == 0)
	{
		// We are connecting to the local host
		test[0] = NULL;
	}
	else
	{
		memset(test, 0, 256);
		_snprintf(test, 256, "\\\\%s", szServer);
	}

	*bIs64Bit = false;



	nResult = RegConnectRegistry(test, HKEY_LOCAL_MACHINE, &hRemote);
	if (ERROR_SUCCESS != nResult)
	{
		ErrorHandler("RegConnectRegistry", "GetOSVersion", nResult);
		return false;
	}

	nResult = RegOpenKey(hRemote, IS_64BIT_KEY, &hOptions);
	if (ERROR_SUCCESS != nResult)
	{
		// If we couldn't open the key, assume it's 32-bit
		*bIs64Bit = false;
	}
	else
	{
		RegCloseKey(hOptions);

		// Verify that the target has a 64-bit proc
		nResult = RegOpenKey(hRemote, IS_64BIT_PROC_KEY, &hOptions);
		if (ERROR_SUCCESS == nResult)
		{
			RegQueryValueEx(hOptions, "PROCESSOR_ARCHITECTURE", NULL, NULL, NULL, &dwSize);
			if (dwSize > 0)
			{
				char* szProcType = (char*)malloc(dwSize * 2 + 1);
				memset(szProcType, 0, dwSize * 2 + 1);
				nResult = RegQueryValueEx(hOptions, "PROCESSOR_ARCHITECTURE", NULL, NULL, (BYTE*)szProcType, &dwSize);
				if (ERROR_SUCCESS != nResult)
				{
					Log.CachedReportError(nCacheID, CRITICAL, "WARNING: Could not determine target processor type - assuming 32-bit. If you have trouble, try overriding the target type for this host with -O (invalid PA value)\n");
					*bIs64Bit = false;
					RegCloseKey(hOptions);
				}
				else
				{
					if (strlen(szProcType) > 0)
					{
						if (stricmp(szProcType, "AMD64") == 0)
						{
							// Can be *reasonably* sure it's 64-bit, though not guaranteed
							*bIs64Bit = true;
						}
						else
						{
							*bIs64Bit = false;
						}
					}
					else
					{
						Log.CachedReportError(nCacheID, CRITICAL, "WARNING: Could not determine target processor type - assuming 32-bit. If you have trouble, try overriding the target type for this host with -O (zero-len PA value)\n");
						*bIs64Bit = false;
					}
				}

				free(szProcType);
			}	
			else
			{
				Log.CachedReportError(nCacheID, CRITICAL, "WARNING: Could not determine target processor type - assuming 32-bit. If you have trouble, try overriding the target type for this host with -O (no PA value)\n");
				*bIs64Bit = false;
			}
			
			RegCloseKey(hOptions);
		}
		else
		{
			Log.CachedReportError(nCacheID, CRITICAL, "WARNING: Could not determine target processor type - assuming 32-bit. If you have trouble, try overriding the target type for this host with -O (no PA value)\n");
			*bIs64Bit = false;
		}
	}

	nResult = RegOpenKey(hRemote, VERSION_KEY, &hVersion);
	if (ERROR_SUCCESS != nResult)
	{
		RegCloseKey(hRemote);
		ErrorHandler("RegOpenKey(Version)", "GetOSVersion", nResult);
		return false;
	}

	nResult = RegOpenKey(hRemote, OS_OPTIONS_KEY, &hOptions);
	if (ERROR_SUCCESS != nResult)
	{
		RegCloseKey(hVersion);
		RegCloseKey(hRemote);
		ErrorHandler("RegOpenKey(Options)", "GetOSVersion", nResult);
		return false;
	}

	dwSize = 0;
	RegQueryValueEx(hOptions, "ProductType", NULL, NULL, NULL, &dwSize);
	
	// Allocate memory for the buffer
	if (dwSize > 0)
	{
		szType = (char*)malloc(dwSize * 2 + 1);
		memset(szType, 0, dwSize * 2 + 1);
		nResult = RegQueryValueEx(hOptions, "ProductType", NULL, NULL, (BYTE*)szType, &dwSize);
		if (ERROR_SUCCESS != nResult)
		{
			RegCloseKey(hVersion);
			RegCloseKey(hOptions);
			RegCloseKey(hRemote);
			ErrorHandler("RegQueryValueEx(ProductType)", "GetOSVersion", nResult);
			return false;
		}
	}

	dwSize = 0;
	RegQueryValueEx(hVersion, "CurrentVersion", NULL, NULL, NULL, &dwSize);

	// Allocate memory for the buffer
	if (dwSize > 0)
	{
		szVersion = (char*)malloc(dwSize * 2 + 1);
		memset(szVersion, 0, dwSize * 2 + 1);
		nResult = RegQueryValueEx(hVersion, "CurrentVersion", NULL, NULL, (BYTE*)szVersion, &dwSize);
		if (ERROR_SUCCESS != nResult)
		{
			RegCloseKey(hVersion);
			RegCloseKey(hOptions);
			RegCloseKey(hRemote);
			ErrorHandler("RegQueryValueEx(CurrentVersion)", "GetOSVersion", nResult);
			return false;
		}
	}

	dwSize = 0;
	RegQueryValueEx(hVersion, "CurrentBuildNumber", NULL, NULL, NULL, &dwSize);

	// Allocate memory for the buffer
	if (dwSize > 0)
	{
		szBuild = (char*)malloc(dwSize * 2 + 1);
		memset(szBuild, 0, dwSize * 2 + 1);
		nResult = RegQueryValueEx(hVersion, "CurrentBuildNumber", NULL, NULL, (BYTE*)szBuild, &dwSize);
		if (ERROR_SUCCESS != nResult)
		{
			RegCloseKey(hVersion);
			RegCloseKey(hOptions);
			RegCloseKey(hRemote);
			ErrorHandler("RegQueryValueEx(BuildNumber)", "GetOSVersion", nResult);
			return false;
		}
	}

	dwSize = 0;
	RegQueryValueEx(hVersion, "CSDVersion", NULL, NULL, NULL, &dwSize);

	// Allocate memory for the buffer
	if (dwSize > 0)
	{
		szServicePack = (char*)malloc(dwSize * 2 + 1);
		memset(szServicePack, 0, dwSize * 2 + 1);
		nResult = RegQueryValueEx(hVersion, "CSDVersion", NULL, NULL, (BYTE*)szServicePack, &dwSize);
		if (ERROR_SUCCESS != nResult)
		{
			RegCloseKey(hVersion);
			RegCloseKey(hOptions);
			RegCloseKey(hRemote);
			ErrorHandler("RegQueryValueEx(CSDVersion)", "GetOSVersion", nResult);
			return false;
		}
	}

	// Should have all the relevant information - build a string and return it to the caller
	const char* lpszFormat;
	int nArgSize;

	if (szServicePack == NULL)
	{
		lpszFormat = "Microsoft Windows %s %s (Build %s)";
		nArgSize = _scprintf(lpszFormat, MapRegValueToOS(szVersion), MapRegValueToOS(szType), szBuild);
		if (nArgSize > 0)
		{
			*lplpVersion = (char*)malloc(nArgSize + 1);
			memset(*lplpVersion, 0, nArgSize + 1);
			_snprintf(*lplpVersion, nArgSize, lpszFormat, MapRegValueToOS(szVersion), MapRegValueToOS(szType), szBuild);
		}
		else
		{
			*lplpVersion = NULL;
		}
	}
	else
	{
		lpszFormat = "Microsoft Windows %s %s %s (Build %s)";
		nArgSize = _scprintf(lpszFormat, MapRegValueToOS(szVersion), MapRegValueToOS(szType), szServicePack, szBuild);
		if (nArgSize > 0)
		{
			*lplpVersion = (char*)malloc(nArgSize + 1);
			memset(*lplpVersion, 0, nArgSize + 1);
			_snprintf(*lplpVersion, nArgSize, lpszFormat, MapRegValueToOS(szVersion), MapRegValueToOS(szType), szServicePack, szBuild);
		}
		else
		{
			*lplpVersion = NULL;
		}
	}

	if (szType != NULL)
		free(szType);
	if (szVersion != NULL)
		free(szVersion);
	if (szBuild != NULL)
		free(szBuild);
	if (szServicePack != NULL)
		free(szServicePack);

	RegCloseKey(hVersion);
	RegCloseKey(hOptions);
	RegCloseKey(hRemote);

	return true;
}

LPCTSTR RegQuery::MapRegValueToOS(LPCTSTR szRegValue)
{
	if (strncmp(szRegValue, "3.51", strlen(szRegValue)) == 0)
		return "3.51";
	else if (strncmp(szRegValue, "4.0", strlen(szRegValue)) == 0)
		return "4.0";
	else if (strncmp(szRegValue, "5.0", strlen(szRegValue)) == 0)
		return "2000";
	else if (strncmp(szRegValue, "5.1", strlen(szRegValue)) == 0)
		return "XP";
	else if (strncmp(szRegValue, "5.2", strlen(szRegValue)) == 0)
		return "2003";
	else if (strncmp(szRegValue, "6.0", strlen(szRegValue)) == 0)
		return "Vista";
	else if (strncmp(szRegValue, "ServerNT", strlen(szRegValue)) == 0)
		return "Server";
	else if (strncmp(szRegValue, "WinNT", strlen(szRegValue)) == 0)
		return "Professional";
	else
		return "Unknown";
}

