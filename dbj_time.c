/* https://code.google.com/archive/p/time-windows/source/default/source */
/* TimeMem.c - Windows port of Unix time utility */
/* License: MIT */
/*
		Taken over by dbj@dbj.org
		The differences vs the original are mine and (c) 2020 dbj.org
		License MIT

		ps: I could not find the original author
*/

#define WIN32_LEAN_AND_MEAN
#define STRICT 1
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>

#pragma region DBJ added

/********************************************************************************************/
/********************************************************************************************/

#ifndef _UNICODE
#error UNICODE builds only please
#endif

#define DBJ_APP_NAME L"dbj_time"
#define DBJ_VER_STRING L"1.5.0"
typedef enum { major = 1, minor = 5, patch = 0 } version ;

#pragma comment( user, "dbj_time [1.5.0] compiled by dbj@dbj.org on " __DATE__ " at " __TIME__ )

#include "vt100.h"

static void last_error_message () // unicode
{
	wchar_t err[256] = {0};
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err, 255, NULL);
	// wprintf(L"\n%s\n", err);
	_putws(err);
}
/********************************************************************************************/
/*
For some reason MSVC's behaviour is to warn about
including this system header. 
*/
#pragma warning(disable : 4820)
#pragma warning(push, 1)
#include <io.h>
#pragma warning(pop)

// we need to do runtime since stdout might be redirected or non existent
static void assure_colours(void)
{
     if (! (_isatty(_fileno(stdout))) && (_WIN32_WINNT >= _WIN32_WINNT_WIN10))
	 {
		 _putws(L"stdout appears to be redirected to a file? Exiting");
		 exit(EXIT_FAILURE);
	 }
	// ugly! but works
	// vt100 init for win10 cmd.exe
	system(" ");
}

/********************************************************************************************/
/********************************************************************************************/
#pragma endregion DBJ added

/* Displays usage help for this program. */
static void intro(void)
{
	wprintf( VT100_FG_BLUE_BOLD L"%s [%s] ", DBJ_APP_NAME, DBJ_VER_STRING );
	wprintf(L"[%S]\n" VT100_RESET , __DATE__ );
}

static void usage(void)
{
	wprintf(L"Usage: " DBJ_APP_NAME L" executable [args...]\n\n");
}

/* Converts FILETIME to ULONGLONG. */
static ULONGLONG convert_file_time(const FILETIME *t)
{
	ULARGE_INTEGER i;
	CopyMemory(&i, t, sizeof(ULARGE_INTEGER));
	return i.QuadPart;
}

/* Displays information about a process. */
static int display_process_info(HANDLE hProcess)
{
	DWORD dwExitCode;
	FILETIME ftCreation, ftExit, ftKernel, ftUser;
	double tElapsed, tKernel, tUser;
	PROCESS_MEMORY_COUNTERS pmc = { sizeof(PROCESS_MEMORY_COUNTERS) };

	/* Exit code */
	if (!GetExitCodeProcess(hProcess, &dwExitCode))
		return 1;

	/* CPU display_process_info */
	if (!GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser))
	{
		return 1;
	}
	tElapsed = 1.0e-7 * (convert_file_time(&ftExit) - convert_file_time(&ftCreation));
	tKernel = 1.0e-7 * convert_file_time(&ftKernel);
	tUser = 1.0e-7 * convert_file_time(&ftUser);

	/* Memory display_process_info */
	// Print information about the memory usage of the process.
	if (!GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
		return 1;

	/* Display display_process_info. */
	wprintf(L"Exit code      : %u\n", dwExitCode);

	wprintf( VT100_FG_GREEN_BOLD L"Elapsed time   : %.2lf\n" VT100_RESET , tElapsed);
	wprintf(L"Kernel time    : %.2lf (%.1lf%%)\n", tKernel, 100.0*tKernel/tElapsed);
	wprintf(L"User time      : %.2lf (%.1lf%%)\n", tUser, 100.0*tUser/tElapsed);

	wprintf(L"page fault #   : %u\n", pmc.PageFaultCount);
	wprintf(L"Working set    : %llu KB\n", pmc.PeakWorkingSetSize/1024);
	wprintf(L"Paged pool     : %llu KB\n", pmc.QuotaPeakPagedPoolUsage/1024);
	wprintf(L"Non-paged pool : %llu KB\n", pmc.QuotaPeakNonPagedPoolUsage/1024);
	wprintf(L"Page file size : %llu KB\n", pmc.PeakPagefileUsage/1024);

	return 0;
}

/* Todo:
 * - mimic linux time utility interface; e.g. see http://linux.die.net/man/1/time
 * - build under 64-bit
 * - display detailed error message
 */

int wmain(
#ifndef NDEBUG
	int argc, wchar_t *argv[]
#endif
	)
{
	assure_colours() ;
	intro();

	LPWSTR szCmdLine;
	LPWSTR szBegin;
	STARTUPINFO si = { sizeof(STARTUPINFO) };
	PROCESS_INFORMATION pi;
	int ret;

	/* Read the command line. */
	szCmdLine = GetCommandLineW();

	/* Strip the first token from the command line. */
	if (szCmdLine[0] == '"')
	{
		/* The first token is double-quoted. Note that we don't need to 
		 * worry about escaped quote, because a quote is not a valid 
		 * path name under Windows.
		 */
		LPWSTR p = szCmdLine + 1;
		while (*p && *p != '"')
			++p;
		szBegin = (*p == '"')? p + 1 : p;
	}
	else
	{
		/* The first token is deliminated by a space or tab. 
		 * See "Parsing C++ Command Line Arguments" below:
		 * http://msdn.microsoft.com/en-us/library/windows/desktop/17w5ykft(v=vs.85).aspx
		 */
		LPWSTR p = szCmdLine;
		while (*p && *p != ' ' && *p != '\t')
			++p;
		szBegin = p;
	}

	/* Skip white spaces. */
	while (*szBegin == ' ' || *szBegin == '\t')
		++szBegin;

	/* If we have no more arguments, display usage display_process_info and exit. */
	if (*szBegin == 0)
	{
		usage();
		return 1;
	}

	/* Display argc,argv and command line for debugging purpose. */
#ifndef NDEBUG
	{
		int i;
		for (i = 0; i < argc; i++)
			wprintf(VT100_FG_GREEN_BOLD L"argv[%d]='%s'\n" VT100_RESET, i, argv[i]);
		wprintf(L"CmdLine = '%s'\n", szCmdLine);
		wprintf(L"Invoked = '%s'\n", szBegin);
	}
#endif

	/* Create the process. */
	if (!CreateProcessW(NULL, szBegin, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		wprintf(VT100_FG_RED_BOLD L"Error: Cannot create process.\n" VT100_RESET);
		last_error_message();
		return EXIT_FAILURE;
	}

	/* Wait for the process to finish. */
	if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0)
	{
		wprintf(VT100_FG_RED_BOLD L"Error: Cannot wait for a process.\n" VT100_RESET);
		last_error_message();
		return EXIT_FAILURE;
	}

	/* Display process statistics. */
	ret = display_process_info(pi.hProcess);

	/* Close process handles. */
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return ret;
}
