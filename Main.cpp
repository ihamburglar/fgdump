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
#include "stdafx.h"
#include "XGetopt.h"
#include "fgdump.h"

void Usage()
{
	printf("Usage:\n");
	printf("%s [-?][-t][-c][-w][-s][-r][-v][-k][-o][-a][-O 32|64][-l logfile][-T threads] [{{-h Host | -f filename} -u Username -p Password | -H filename}]\n", "fgdump");
	printf("\t where Username and Password have administrator credentials\n");
	printf("\t -? displays help (you're looking at it!)\n");
	printf("\t -t will test for the presence of antivirus without actually running the password dumps\n");
	printf("\t -c skips the cache dump\n");
	printf("\t -w skips the password dump\n");
	printf("\t -s performs the protected storage dump\n");
	printf("\t -r forgets about existing pwdump/cachedump files. The default behavior is to skip a host if these files already exist.\n");
	printf("\t -v makes output more verbose. Use twice for greater effect\n");
	printf("\t -k keeps the pwdump/cachedump going even if antivirus is in an unknown state\n");
	printf("\t -l logs all output to logfile\n");
	printf("\t -T runs fgdump with the specified number of parallel threads\n");
	printf("\t -h is the name of the single host to perform the dumps against\n");
	printf("\t -f reads hosts from a line-separated file\n");
	printf("\t -H reads host:username:password from a line-separated file (per-host credentials)\n");
	printf("\t -o skips pwdump history dumps\n");
	printf("\t -a will not attempt to detect or shut down antivirus, even if it is present\n\n");
	printf("\t -O manually sets whether the target is a 32- or 64-bit OS. Note that this applies to all hosts specified.\n\n");
	printf("** As of version 1.4.0, you can run fgdump with no parameters to dump the local box (no impersonation or binding)\n\n");
}

int _tmain(int argc, _TCHAR* argv[])
{
	FGDump fgd;
	char c;

	printf("fgDump 2.1.0 - fizzgig and the mighty group at foofus.net\n");
	//printf("*** THIS IS A BETA VERSION, YOU HAVE BEEN WARNED ***\n");
	printf("Written to make j0m0kun's life just a bit easier\n");
	printf("Copyright(C) 2008 fizzgig and foofus.net\n");
	printf("fgdump comes with ABSOLUTELY NO WARRANTY!\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under certain conditions; see the COPYING and README files for\n");
	printf("more information.\n\n");

	if (argc < 2)
	{
		printf("No parameters specified, doing a local dump. Specify -? if you are looking for help.\n");
	}

	while ((c = getopt(argc, argv, _T("h:tf:H:u:p:l:T:O:cwrvskoa?"))) != EOF)
	{
		switch(c)
		{
		case '?':
			Usage();
			exit(1);
		case 'h':
			fgd.SetServer(optarg);			
			break;
		case 't':
			fgd.SetTestOnlyAV(true);	// Only check for AV
			break;
		case 'f':
			if (strlen(optarg) > MAX_PATH)
			{
				Log.ReportError(CRITICAL, "The path specified for the -f argument is greater than the maximum allowed.\n");
				exit(-1);
			}
			else
			{
				fgd.SetHostfileName(optarg);
			}
			break;
		case 'H':
			if (strlen(optarg) > MAX_PATH)
			{
				Log.ReportError(CRITICAL, "The path specified for the -H argument is greater than the maximum allowed.\n");
				exit(-1);
			}
			else
			{
				fgd.SetPerHostFileName(optarg);
			}
			break;
		case 'u':
			fgd.SetUser(optarg);
			break;
		case 'p':
			fgd.SetPassword(optarg);
			break;
		case 'c':
			fgd.SetSkipCacheDump(true);
			break;
		case 'w':
			fgd.SetSkipPWDump(true);
			break;
		case 's':
			fgd.SetSkipProtectedStorageDump(false);
			break;
		case 'r':
			fgd.SetIgnoreExistingFiles(false);
			break;
		case 'v':
			Log.IncreaseVerbosity();
			break;
		case 'k':
			fgd.ContinueOnUnknownAV(true);
			break;
		case 'l':
			Log.SetWriteToFile(true);
			Log.SetLogFile(optarg);
			break;
		case 'T':
			unsigned int nThreads;
			nThreads = (unsigned int)atoi(optarg);
			if (nThreads < 1)
			{
				Log.ReportError(CRITICAL, "The number of threads specified must be greater than or equal to 1\n");
				exit(-1);
			}

			fgd.SetWorkerThreads(nThreads);
			break;
		case 'o':
			fgd.SetSkipPwdumpHistory(true);
			break;
		case 'a':
			fgd.SetSkipAVCheck(true);
			break;
		case 'O':
			int nBits;
			nBits = atoi(optarg);
			if (nBits != 32 && nBits != 64)
			{
				Log.ReportError(CRITICAL, "The parameter to -O must be 32 or 64\n");
				exit(-1);
			}

			fgd.SetOSArchOverride(nBits);
			break;
		default:
			Log.ReportError(INFO, "Ignoring unknown option '%c'\n", c);
			break;
		}
	}

	int nRet = fgd.Run();

#ifdef _DEBUG
	printf("Press return to exit...");
	scanf("...");
#endif

	return nRet;
}