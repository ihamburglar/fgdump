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
#include ".\stringarray.h"

StringArray::StringArray(void)
{
	pFirstString = NULL;
	pCurrentString = NULL;
	pCurrentIterString = NULL;
}

StringArray::~StringArray(void)
{
	STRINGENTRY* pTemp, *pTemp2;
	pTemp = pFirstString;
	while (pTemp != NULL)
	{
		delete [] pTemp->lpszString;
		pTemp2 = pTemp;
		pTemp = pTemp->pNext;
		delete pTemp2;
	}
}

int StringArray::Add(char* lpszString)
{
	size_t nLen = strlen(lpszString);
	
	if (pFirstString == NULL)
	{
		pFirstString = new STRINGENTRY;
		pFirstString->lpszString = new char[nLen + 1];
		memset(pFirstString->lpszString, 0, nLen + 1);
		strncpy_s(pFirstString->lpszString, nLen, lpszString, nLen);
		pFirstString->pNext = NULL;
		pCurrentString = pFirstString;
	}
	else
	{
		STRINGENTRY* pTemp = new STRINGENTRY;
		pTemp->lpszString = new char[nLen + 1];
		memset(pTemp->lpszString, 0, nLen + 1);
		strncpy_s(pTemp->lpszString, nLen, lpszString, nLen);
		pTemp->pNext = NULL;
		pCurrentString->pNext = pTemp;
		pCurrentString = pTemp;
	}

	return 0;
}


char* StringArray::GetFirstString(void)
{
	if (pFirstString != NULL)
	{
		pCurrentIterString = pFirstString;
		return pFirstString->lpszString;
	}
	else
		return NULL;
}

char* StringArray::GetNextString(void)
{
	if (pCurrentIterString != NULL)
	{
		if (pCurrentIterString->pNext != NULL)
		{
			pCurrentIterString = pCurrentIterString->pNext;
			return pCurrentIterString->lpszString;
		}
		else
			return NULL;
	}
	else
		return NULL;
}
