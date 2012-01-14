/***************************************************************************
 * Taken from: lsadump2
 * File:    getpid.c
 * 
 * Purpose: Find the pid of a process, given its name
 * 
 * Date:    Thu Mar 23 23:21:32 2000
 * 
 * Copyright (c) 2000 Todd A. Sabin, all rights reserved
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 ***************************************************************************/


#include <windows.h>
#include <winnt.h>
#include <stdlib.h>
#include <stdio.h>
#include "getpid.h"

typedef DWORD NTSTATUS;

typedef struct 
{
    USHORT Length;
    USHORT MaxLen;
    USHORT *Buffer;
} UNICODE_STRING;

typedef struct 
{
    ULONG NextEntryDelta;
    ULONG ThreadCount;
    ULONG Reserved1[6];
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ProcessName;
    ULONG BasePriority;
#ifdef _WIN64
#ifdef _M_X64
	ULONG32 pad; 
#else
#error Only x64 has been tested with the ULONG32 pad. Comment this #error out if you want to test for this build.
	ULONG32 pad; 
#endif
#endif
    ULONG ProcessId;
    // etc.
} process_info;

typedef NTSTATUS (__stdcall *NtQSI_t)( ULONG, PVOID, ULONG, PULONG );
typedef LONG (__stdcall *RtlCUS_t)( UNICODE_STRING*, UNICODE_STRING*, ULONG );
NTSTATUS (__stdcall *NtQuerySystemInformation)( IN ULONG SysInfoClass, IN OUT PVOID SystemInformation,
																	  IN ULONG SystemInformationLength, OUT PULONG RetLen );
LONG (__stdcall *RtlCompareUnicodeString)( IN UNICODE_STRING*, IN UNICODE_STRING*, IN ULONG CaseInsensitve );



//
// Find the pid of LSASS.EXE
//
int find_pid(DWORD *ppid)
{
    HINSTANCE hNtDll;
    NTSTATUS rc;
    ULONG ulNeed = 0;
    void *buf = NULL;
    size_t len = 0;
    ULONG ret = 0;

    hNtDll = LoadLibrary("NTDLL");
    if(!hNtDll)
        return 0;

    NtQuerySystemInformation = (NtQSI_t)GetProcAddress(hNtDll, "NtQuerySystemInformation");
    if (!NtQuerySystemInformation)
        return 0;

    RtlCompareUnicodeString = (RtlCUS_t)GetProcAddress(hNtDll, "RtlCompareUnicodeString");
    if(!RtlCompareUnicodeString)
        return 0;

    do 
    {
		if (buf != NULL)
			free(buf);

        len += 2000;
        buf = (BYTE*)malloc(len);

        if( !buf )
            return 0;

        rc = NtQuerySystemInformation(5, buf, (ULONG) len, &ulNeed);
    } while(rc == 0xc0000004);  // STATUS_INFO_LEN_MISMATCH

    if(rc < 0) 
    {
        free(buf);
        return 0;
    };

    process_info *p = (process_info*)buf;
    bool endlist = false;
    UNICODE_STRING lsass = { 18, 20, (USHORT*) L"LSASS.EXE" };

    while(!endlist) 
    {
        if(p->ProcessName.Buffer && !RtlCompareUnicodeString(&lsass, &p->ProcessName, 1)) 
        {
            *ppid = p->ProcessId;
			ret = 1;
            goto exit;
        }
        endlist = p->NextEntryDelta == 0;
        p = (process_info *)(((BYTE*)p) + p->NextEntryDelta);
    }

 exit:
    free(buf);
    FreeLibrary(hNtDll);

    return ret;
}
