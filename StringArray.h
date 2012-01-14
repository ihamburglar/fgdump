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

struct STRINGENTRY
{
	char* lpszString;
	struct STRINGENTRY* pNext;
}; 

//typedef struct STRINGENTRY, *STRINGENTRY;

class StringArray
{
public:
	StringArray(void);
	~StringArray(void);
	int Add(char* lpszString);
	char* GetFirstString(void);
	char* GetNextString(void);
private:
	STRINGENTRY* pFirstString;
	STRINGENTRY* pCurrentString;
	STRINGENTRY* pCurrentIterString;
};
