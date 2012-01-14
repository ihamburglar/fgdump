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
//
//	ErrorHandler.h
//
//	Header file for the error handling facility
//

#ifndef __ERRORHANDLER_H__			//	Don't include more than once
#define __ERRORHANDLER_H__	1

#define	TITLE_STRING	"%s %s reports an error!"
#define TEXT_STRING		"ERROR %s: %d - %s\n"
#define	MAX_ERROR_TEXT	256

extern void ErrorHandler(char *szModule, char *szRoutine, DWORD dwErrorNum, LONG nCacheID = -1);

#endif	//	(don't include more than once)