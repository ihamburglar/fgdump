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
#pragma once

#define SHARE_MUTEX_NAME "Global\\FGDUMP_SHARE_MTX"

class ShareFinder
{
public:
	ShareFinder(LONG nCacheID = -1);
	~ShareFinder();

	bool EnumerateShares(char* szServer);
	bool BindUploadShareToLocalDrive(char* szServer, int nBufferSize, char** lplpPhysicalPath, char* pAssignedDrive);
	void UnbindDrive(char* szDrive);
	bool GetAvailableWriteableShare(char* szServer, int nPhysicalBufferSize, char** lplpPhysicalPath, int nUNCPathSize, char** lplpUNCPath);

private:
	HANDLE hMutex;
	LONG m_nCacheID;
	bool BindDrive(char* szDrive, char* szServerAscii, char* szShare);
	char GetUnusedDriveLetter();
	bool CanUpload(char* szUploadPath);

};
