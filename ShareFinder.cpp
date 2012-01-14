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
#include "sharefinder.h"
#include <Lm.h>

ShareFinder::ShareFinder(LONG nCacheID)
{
	m_nCacheID = nCacheID;
	hMutex = CreateMutex(NULL, FALSE, SHARE_MUTEX_NAME);
	int nError = GetLastError();
	if (hMutex == NULL)
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to create/open share finding mutex! Throwing an error now.\n");
		throw(1);
	}
}

ShareFinder::~ShareFinder()
{

}

bool ShareFinder::EnumerateShares(char* szServer)
{
	PSHARE_INFO_502 BufPtr, p;
	NET_API_STATUS res;
	DWORD er=0, tr=0, resume=0, i;
	wchar_t server[MAX_PATH];
	char szServerWithSlashes[MAX_PATH];

	::ZeroMemory(server, MAX_PATH);
	::ZeroMemory(szServerWithSlashes, MAX_PATH);
	_snprintf(szServerWithSlashes, MAX_PATH, "\\\\%s", szServer);
	mbstowcs(server, szServerWithSlashes, strlen(szServerWithSlashes));

	do
	{
		// Fuck Microsoft and it's lame-ass unicode crap
		res = NetShareEnum((LPSTR)server, 502, (LPBYTE*)&BufPtr, -1, &er, &tr, &resume);
		if(res == ERROR_SUCCESS || res == ERROR_MORE_DATA)
		{
			p = BufPtr;
			for(i=1; i <= er; i++)
			{
				Log.CachedReportError(m_nCacheID, INFO, "Found share %S, whose physical path is %S\n", p->shi502_netname, p->shi502_path);
				p++;
			}

			NetApiBufferFree(BufPtr);
		}
		else 
			Log.CachedReportError(m_nCacheID, CRITICAL, "EnumerateShares returned an error of %ld\n",res);
	}
	while (res == ERROR_MORE_DATA); // end do

	return true;
}

bool ShareFinder::BindUploadShareToLocalDrive(char* szServer, int nBufferSize, char** lplpPhysicalPath, char* pAssignedDrive)
{
	// Returns the drive letter if successful, otherwise 0
	PSHARE_INFO_502 BufPtr, p;
	NET_API_STATUS res;
	DWORD er = 0, tr = 0, resume = 0, i;
	wchar_t server[MAX_PATH];
	char szTemp[MAX_PATH];
	bool bBound = false;
	char szServerWithSlashes[MAX_PATH];
	char lpszLocalDrive[3];

	::ZeroMemory(server, MAX_PATH);
	::ZeroMemory(szServerWithSlashes, MAX_PATH);
	::ZeroMemory(*lplpPhysicalPath, nBufferSize);
	_snprintf(szServerWithSlashes, MAX_PATH, "\\\\%s", szServer);
	mbstowcs(server, szServerWithSlashes, strlen(szServerWithSlashes));
	memset(lpszLocalDrive, 0, 3);

	// This needs to be protected with a critical section, since multiple threads may try to get a free
	// drive letter at the same time! That's a bad thing
	WaitForSingleObject(hMutex, INFINITE);

	char cDriveLetter = GetUnusedDriveLetter();
	memset(lpszLocalDrive, 0, 3);
	lpszLocalDrive[0] = cDriveLetter;
	lpszLocalDrive[1] = ':';

	if (cDriveLetter == 0)
	{
		ReleaseMutex(hMutex);
		return false;
	}

	memcpy(pAssignedDrive, &cDriveLetter, 1);

	do
	{
		// Fuck Microsoft and it's lame-ass unicode crap
		res = NetShareEnum((LPSTR)server, 502, (LPBYTE*)&BufPtr, -1, &er, &tr, &resume);
		if(res == ERROR_SUCCESS || res == ERROR_MORE_DATA)
		{
			p = BufPtr;
			for(i = 1; i <= er; i++)
			{
				::ZeroMemory(szTemp, MAX_PATH);
				wcstombs(szTemp, (LPWSTR)(p->shi502_netname), MAX_PATH);

				// Look for shares that are not SYSVOL or NETLOGON, and that have a physical path
				if (stricmp(szTemp, "SYSVOL") != 0 && stricmp(szTemp, "NETLOGON") != 0 && wcslen((LPWSTR)(p->shi502_path)) > 0)
				{
					// If this is a potentially workable share, bind the drive and try uploading something
					if (BindDrive(lpszLocalDrive, szServerWithSlashes, szTemp))
					{
						// Success!
						// Copy the physical path to the out variable
						wcstombs(szTemp, (LPWSTR)(p->shi502_path), MAX_PATH);
						strncpy(*lplpPhysicalPath, szTemp, nBufferSize);
						bBound = true;
						break;
					}
					// Otherwise continue and try another share
				}
				
				p++;
			}

			NetApiBufferFree(BufPtr);
		}
		else 
			Log.CachedReportError(m_nCacheID, CRITICAL, "BindUploadShareToLocalDrive returned an error of %ld\n",res);
	}
	while (res == ERROR_MORE_DATA); // end do
	
	ReleaseMutex(hMutex);

	return true;
}

bool ShareFinder::GetAvailableWriteableShare(char* szServer, int nPhysicalBufferSize, char** lplpPhysicalPath, int nUNCPathSize, char** lplpUNCPath)
{
	// Returns the drive letter if successful, otherwise 0
	PSHARE_INFO_502 BufPtr, p;
	NET_API_STATUS res;
	DWORD er = 0, tr = 0, resume = 0, i;
	wchar_t server[MAX_PATH];
	char szTemp[MAX_PATH], szTemp2[MAX_PATH];
	bool bFound = false;
	char szServerWithSlashes[MAX_PATH];

	::ZeroMemory(server, MAX_PATH);
	::ZeroMemory(szServerWithSlashes, MAX_PATH);
	::ZeroMemory(*lplpPhysicalPath, nPhysicalBufferSize);
	::ZeroMemory(*lplpUNCPath, nUNCPathSize);
	_snprintf(szServerWithSlashes, MAX_PATH, "\\\\%s", szServer);
	mbstowcs(server, szServerWithSlashes, strlen(szServerWithSlashes));

	do
	{
		// Fuck Microsoft and it's lame-ass unicode crap
		res = NetShareEnum((LPSTR)server, 502, (LPBYTE*)&BufPtr, -1, &er, &tr, &resume);
		if(res == ERROR_SUCCESS || res == ERROR_MORE_DATA)
		{
			p = BufPtr;
			for(i = 1; i <= er; i++)
			{
				::ZeroMemory(szTemp, MAX_PATH);
				wcstombs(szTemp, (LPWSTR)(p->shi502_netname), MAX_PATH);

				// Look for shares that are not SYSVOL or NETLOGON, and that have a physical path
				if (((p->shi502_type == STYPE_DISKTREE) || (p->shi502_type == STYPE_SPECIAL)) /*&& stricmp(szTemp, "QS1LPT3") != 0*/ && stricmp(szTemp, "SYSVOL") != 0 && stricmp(szTemp, "NETLOGON") != 0 && wcslen((LPWSTR)(p->shi502_path)) > 0)
				{
					// If this is a potentially workable share, try uploading something
					memset(szTemp2, 0, MAX_PATH);
					_snprintf(szTemp2, MAX_PATH, "%s\\%s", szServerWithSlashes, szTemp);
					if (CanUpload(szTemp2))
					{
						// Success!
						// Copy the physical path to the out variable
						wcstombs(szTemp, (LPWSTR)(p->shi502_path), MAX_PATH);
						strncpy(*lplpPhysicalPath, szTemp, nPhysicalBufferSize);

						// Also copy the UNC path to the out variable
						strncpy(*lplpUNCPath, szTemp2, nUNCPathSize);
						bFound = true;
						break;
					}

					// Otherwise continue and try another share
				}
				
				p++;
			}

			NetApiBufferFree(BufPtr);
		}
		else 
			Log.CachedReportError(m_nCacheID, CRITICAL, "BindUploadShareToLocalDrive returned an error of %ld\n",res);
	}
	while (res == ERROR_MORE_DATA); // end do

	return bFound;
}

bool ShareFinder::CanUpload(char* szUploadPath)
{
	char szTempFilename[MAX_PATH];
	NETRESOURCE nr; 

	::ZeroMemory(&nr, sizeof(NETRESOURCE));
	::ZeroMemory(szTempFilename, MAX_PATH);
	_snprintf(szTempFilename, MAX_PATH, "%s\\test.fgdump", szUploadPath);

	std::ofstream outputFile(szTempFilename, std::ios::out | std::ios::trunc);
	outputFile.write("success", 7);
	if (outputFile.fail())
	{
		Sleep(3000);	// Wait a second and try again
		outputFile.write("success", 7);
		if (outputFile.fail())
		{
			Log.CachedReportError(m_nCacheID, DEBUG, "Error writing the test file to %s, skipping this share (error %d)\n", szUploadPath, GetLastError());
			return false;
		}
	}

	outputFile.flush();
	Log.CachedReportError(m_nCacheID, DEBUG, "Able to write to this directory, using location %s for cachedump\n", szUploadPath);
	outputFile.close();
	DeleteFile(szTempFilename);

	return true;
}

bool ShareFinder::BindDrive(char* szDrive, char* szServer, char* szShare)
{
	DWORD dwResult; 
	NETRESOURCE nr; 
	char szTemp[MAX_PATH];
	char szTempFilename[MAX_PATH];

	::ZeroMemory(&nr, sizeof(NETRESOURCE));
	::ZeroMemory(szTemp, MAX_PATH);
	_snprintf(szTemp, MAX_PATH, "%s\\%s", szServer, szShare);

	nr.dwType = RESOURCETYPE_ANY;
	nr.lpRemoteName = szTemp;
	nr.lpProvider = NULL;
	nr.lpLocalName = szDrive;
	nr.lpComment = "Added by fgdump";

	dwResult = WNetAddConnection2(&nr,
								  (LPSTR) NULL, 
								  (LPSTR) NULL,  
								  CONNECT_UPDATE_PROFILE);
	 
	if (dwResult == ERROR_ALREADY_ASSIGNED) 
	{ 
		Log.CachedReportError(m_nCacheID, DEBUG, "Already connected to specified resource on drive %s.\n", szDrive); 
		return false; 
	} 
	else if (dwResult == ERROR_DEVICE_ALREADY_REMEMBERED) 
	{ 
		Log.CachedReportError(m_nCacheID, DEBUG, "Attempted reassignment of remembered device %s.\n", szDrive); 
		return false; 
	} 
	else if(dwResult != NO_ERROR) 
	{ 
		Log.CachedReportError(m_nCacheID, DEBUG, "A generic error occurred binding the drive %s: %d\n", szDrive, dwResult); 
		return false; 
	} 
	 
	Log.CachedReportError(m_nCacheID, DEBUG, "The drive %s is bound to %s, testing write access...\n", szDrive, szShare);
	::ZeroMemory(szTempFilename, MAX_PATH);
	_snprintf(szTempFilename, MAX_PATH, "%s\\%s.fgdump", szDrive, szServer);

	std::ofstream outputFile(szTempFilename, std::ios::out | std::ios::trunc);
	outputFile.write("success", 7);
	if (outputFile.fail())
	{
		Log.CachedReportError(m_nCacheID, DEBUG, "Error writing the test file to %s, skipping this share (error %d)\n", szShare, GetLastError());
		WNetCancelConnection2(szDrive, CONNECT_UPDATE_PROFILE, TRUE);
		return false;
	}

	outputFile.flush();
	Log.CachedReportError(m_nCacheID, DEBUG, "Able to write to this directory, using drive %s for cachedump\n", szDrive);
	outputFile.close();
	DeleteFile(szTempFilename);

	return true;
}

void ShareFinder::UnbindDrive(char* szDrive)
{
	DWORD dwResult = WNetCancelConnection2(szDrive, CONNECT_UPDATE_PROFILE, TRUE);
	if (dwResult == ERROR_DEVICE_IN_USE)
	{
		Log.CachedReportError(m_nCacheID, CRITICAL, "Unable to disconnect drive %s, it appears another process is using the share. Suggest disconnecting it manually.\n", szDrive);
	}

	return;
}

char ShareFinder::GetUnusedDriveLetter()
{
	char szTemp[3] = { 0, 0, 0 };
	// Returns 0 if unsuccessful - that probably shouldn't happen too often!

	for (int i = 67; i <= 90; i++)	// 'C' through 'Z'
	{
		_snprintf(szTemp, 2, "%c:", i);
		if (GetDriveType(szTemp) == DRIVE_NO_ROOT_DIR)
		{
			// This drive should work
			Log.CachedReportError(m_nCacheID, DEBUG, "Drive %c is available, using that for bind operations\n", i);
			break;
		}
		szTemp[0] = 0;
	}

	return szTemp[0];
}
