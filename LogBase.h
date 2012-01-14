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

#ifndef _LOGBASE_H
#define _LOGBASE_H

#include "stdafx.h"
#include "StringArray.h"

// I friggin' hate STL with a passion, but it's a relatively lightweight way to do a
// hashmap (at least compared to a solution like MFC)
#include <iostream>
#include <hash_map>

using namespace stdext;
enum ERROR_LEVEL
{
	REGULAR_OUTPUT = -1,
	CRITICAL = 0,
	INFO = 1,
	DEBUG = 2
};

class LogBase
{
public:
	LogBase();
	~LogBase();

	virtual char* GetLogFile();
	virtual void SetLogFile(char* szLogFile);
	virtual void SetWriteToFile(bool bWriteToFile);
	virtual void IncreaseVerbosity();
	int BeginCachedWrite(void);
	void FlushCachedWrite(int nCacheID);

protected:
	LONG nCacheID;
	char lpszFile[MAX_PATH + 1];
	CRITICAL_SECTION csLogWrite;
	bool bEnableFileWrite;
	int nVerbosity;
	hash_map<LONG, StringArray*> map;
};

#endif
