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
//	ErrorHandler.cpp
//
//	Print out fancy error messages
//
//#include "General.h"

#include "stdafx.h"
#include <lm.h>
#include <stdio.h>

void ErrorHandler(char *szModule, char *szRoutine, DWORD dwErrorNum, LONG nCacheID)
{
	char		*szMessageBuffer;
	char		*szMessage;
	HINSTANCE	hNetMsg;
	DWORD		dwBufferLength;
	DWORD		dwMessageFlags;

	//	Initialize variables
	szMessage		= new char[ MAX_ERROR_TEXT ];
	hNetMsg			= NULL;
	szMessageBuffer	= NULL;
	dwBufferLength	= 0;
	dwMessageFlags	= FORMAT_MESSAGE_ALLOCATE_BUFFER |
					  FORMAT_MESSAGE_IGNORE_INSERTS |
					  FORMAT_MESSAGE_FROM_SYSTEM ;
	memset(szMessage, 0, MAX_ERROR_TEXT);
	
	//	If it's a NET error, load the proper message table
	if ((dwErrorNum >= NERR_BASE) && (dwErrorNum <= MAX_NERR))
	{
		hNetMsg = LoadLibraryEx("netmsg.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);

		if (hNetMsg != NULL)
		{
			dwMessageFlags |= FORMAT_MESSAGE_FROM_HMODULE;
		}
	}
	
	if (dwErrorNum < WSABASEERR)
	{
		//	Format the message and print it out
		if (FormatMessage( dwMessageFlags,
						   hNetMsg,				//	If this is still null, just use system errors
						   dwErrorNum,
						   0,
						   (LPSTR)&szMessageBuffer,
						   0,
						   NULL ))
		{
			//	Format the body
			sprintf(szMessage, TEXT_STRING, szRoutine, dwErrorNum, szMessageBuffer);

			//	Show the message
			Log.CachedReportError(nCacheID, CRITICAL, szMessage);
		} 
		else 
		{
			//	Format the body
			sprintf(szMessage, TEXT_STRING, szRoutine, dwErrorNum, "No text available for this message");

			//	Show the message
			Log.CachedReportError(nCacheID, CRITICAL, szMessage);
		}	//	Did FormatMessage work?
	} 
	else 
	{
			//	Format the body
			sprintf(szMessage, TEXT_STRING, szRoutine, dwErrorNum, "No text available for this message");

			//	Show the message
			Log.CachedReportError(nCacheID, CRITICAL, szMessage);
	}	//	(is this a message for which we have a string table?)

	//	Free the library, if we loaded it
	if (hNetMsg != NULL)
	{
		FreeLibrary( hNetMsg );
	}

	//	Return memory
	delete szMessage;
}	//	(ErrorHandler)
