/******************************************************************************
pstgdump - by fizzgig and the foofus.net group
Copyright (C) 2006 by fizzgig
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

#include "stdafx.h"
#include "XGetopt.h"
#include "ProtectedStorage.h"

void PrintHeader()
{
	printf("pstgdump 0.1.0-BETA2 - fizzgig and the mighty group at foofus.net\n");
	printf("*** THIS IS A BETA VERSION, YOU HAVE BEEN WARNED ***\n");
	printf("Copyright (C) 2006 fizzgig and foofus.net\n");
	printf("pstgdump comes with ABSOLUTELY NO WARRANTY!\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under certain conditions; see the COPYING and README files for\n");
	printf("more information.\n\n");
}

void Usage()
{
	PrintHeader();
	printf("Usage:\n");
	printf("ptsgdump [-h][-q][-u Username][-p Password]\n");
	printf("\t -h displays this usage information\n");
	printf("\t -q supresses the program information - useful when running as a batch job or saving to a file\n");
	printf("\t -u username to impersonate (if not provided, the currently logged in user is used)\n");
	printf("\t -p password to use in conjunction with -u\n");

	exit(1);
}

int _tmain(int argc, char* argv[])
{
	bool bShowHeader = true;
	HANDLE hToken = NULL;
	int nResult = -1;
	char c;
	char* szUserToImpersonate = NULL;
	char* szPassword = NULL;

	while ((c = getopt(argc, argv, _T("u:p:hq"))) != EOF)
	{
		switch(c)
		{
			case 'h':
				Usage();
				break;
			case 'q':
				bShowHeader = false;
				break;
			case 'u':
				szUserToImpersonate = (char*)malloc(strlen(optarg) + 1);
				memset(szUserToImpersonate, 0, strlen(optarg) + 1);
				strncpy(szUserToImpersonate, optarg, strlen(optarg));
				break;
			case 'p':
				szPassword = (char*)malloc(strlen(optarg) + 1);
				memset(szPassword, 0, strlen(optarg) + 1);
				strncpy(szPassword, optarg, strlen(optarg));
				break;
			default:
				printf("Ignoring unknown option '%c'\n", c);
				break;
		}
	}

	if (bShowHeader)
	{
		PrintHeader();
	}

	if (szUserToImpersonate != NULL && szPassword == NULL)
	{
		printf("You must specify a password when you specify a user to impersonate. Exiting.\n");
		return -1;
	}
	else if (szUserToImpersonate != NULL && szPassword != NULL)
	{
		printf("Attempting to impersonate user '%s'\n\n", szUserToImpersonate);
		if (!LogonUser(szUserToImpersonate, "", szPassword, LOGON32_LOGON_BATCH, LOGON32_PROVIDER_DEFAULT, &hToken))
		{
			printf("\nFailed to impersonate user (LogonUser failed): error %d\n", GetLastError());
			return -1;
		}

		if (!ImpersonateLoggedOnUser(hToken))
		{
			printf("\nFailed to impersonate user (ImpersonateLoggedOnUser failed): error %d\n", GetLastError());
			CloseHandle(hToken);
			return -1;
		}
	}
	
	ProtectedStorage objPS;
	bool bResult = objPS.GetProtectedData();

	if (!bResult)
	{
		printf("\nFailed to dump all protected storage items - see previous messages for details\n");
		nResult = -1;
	}
	else
	{
		printf("\nSuccessfully dumped protected storage\n");
		nResult = 0;
	}

	if (hToken != NULL)
	{
		CloseHandle(hToken);
		RevertToSelf();
	}

	return nResult;
}

