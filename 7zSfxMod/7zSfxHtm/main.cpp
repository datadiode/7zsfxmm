/*---------------------------------------------------------------------------*/
/* File:        main.cpp                                                     */
/* Created:     Fri, 29 Jul 2005 03:23:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 08 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#include "stdafx.h"
#include <cpl.h>
#include <tlhelp32.h>
#include "7zSfxHtmInt.h"
#include "ExtractEngine.h"
#include "archive.h"
#include "../C/Lzma86.h"

#include <new.h>

static int sfx_new_handler(size_t)
{
	FatalAppExitA(0, "Could not allocate memory");
	return 0;
}

static HINSTANCE g_hInstance = NULL;
static LPWSTR g_lpszCmdLine = NULL;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpszCmdLine, int)
{
	_set_new_handler(sfx_new_handler);
	_set_new_mode(1);
	HRESULT hr = S_OK;
	__try
	{
		hr = CSfxExtractEngine::Run(
			g_hInstance ? g_hInstance : hInstance,
			g_lpszCmdLine ? g_lpszCmdLine : lpszCmdLine);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[256];
		wsprintfA(msg, "An exception of type 0x%08lX has occurred", GetExceptionCode());
		FatalAppExitA(0, msg);
	}
	return hr;
}

EXTERN_C void __cdecl wWinMainCRTStartup();

EXTERN_C void CALLBACK EntryPointW(HWND, HINSTANCE, LPWSTR lpszCmdLine, int)
{
	g_lpszCmdLine = lpszCmdLine;
	wWinMainCRTStartup();
}

static HWND CreateHiddenOwner()
{
	HWND hwnd = CreateWindowW(
		L"STATIC", NULL, WS_POPUP, 0, 0,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		NULL, NULL, NULL, NULL);
	SetFocus(hwnd);
	return hwnd;
}

static DWORD RunDll32(LPCWSTR verb)
{
	WCHAR file[MAX_PATH];
	GetSystemDirectoryW(file, MAX_PATH);
	PathAppendW(file, L"rundll32.exe");
	WCHAR params[MAX_PATH + 40];
	GetModuleFileNameW(g_hInstance, params, MAX_PATH);
	PathQuoteSpacesW(params);
	wcscat(params, L",EntryPoint");
	SHELLEXECUTEINFOW info;
	memset(&info, 0, sizeof info);
	info.cbSize = sizeof info;
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.hwnd = CreateHiddenOwner();
	info.nShow = SW_SHOWNORMAL;
	info.lpVerb = verb;
	info.lpFile = file;
	info.lpParameters = params;
	DWORD dwExitCode = 0;
	if (ShellExecuteExW(&info))
	{
		WaitForProcess(info.hProcess, &dwExitCode);
		CloseHandle(info.hProcess);
	}
	else
	{
		dwExitCode = GetLastError();
	}
	DestroyWindow(info.hwnd);
	return dwExitCode;
}

EXTERN_C LONG CALLBACK CPlApplet(HWND, UINT uMsg, LONG, LONG)
{
	switch (uMsg) 
	{
	case CPL_INIT:
		WCHAR verb[256];
		ExitProcess(RunDll32(LoadStringW(g_hInstance, 0, verb, _countof(verb)) ? verb : NULL));
		break;
	}
	return 0;
}

static HWND FindProcessWindow(LPCWSTR lpszExeFile, LPCWSTR lpszClass, LPCWSTR lpszWindow)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	HWND hWnd = NULL;
	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32W pe;
		memset(&pe, 0, sizeof pe);
		pe.dwSize = sizeof pe;
		BOOL bContinue = Process32FirstW(hSnapshot, &pe);
		while (bContinue)
		{
			if (_wcsicmp(pe.szExeFile, lpszExeFile) == 0)
			{
				while ((hWnd = FindWindowExW(NULL, hWnd, lpszClass, lpszWindow)) != NULL)
				{
					DWORD dwProcessId = 0;
					if (GetWindowThreadProcessId(hWnd, &dwProcessId) && dwProcessId == pe.th32ProcessID)
					{
						pe.dwSize = 0;
						break;
					}
				}
			}
			bContinue = Process32NextW(hSnapshot, &pe);
		}
		CloseHandle(hSnapshot);
	}
	return hWnd;
}

EXTERN_C UINT CALLBACK CustomAction(HANDLE)
{
	if (HWND hWnd = FindProcessWindow(L"MSIEXEC.EXE", WC_DIALOG, NULL))
	{
		PostMessage(hWnd, WM_COMMAND, IDCANCEL, 0);
		SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW | SWP_ASYNCWINDOWPOS);
	}
	RunDll32(L"open");
	return ERROR_INSTALL_USEREXIT;
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

EXTERN_C BOOL WINAPI HybridCRTStartup(HINSTANCE hinstDLL, DWORD dwReason, LPVOID)
{
	if (GetModuleHandle(NULL) == reinterpret_cast<HINSTANCE>(&__ImageBase))
	{
		wWinMainCRTStartup();
	}
	else if (dwReason == DLL_PROCESS_ATTACH)
	{
		g_hInstance = hinstDLL;
	}
	return TRUE;
}

#ifdef _WIN64
#	define __cdecl(name) #name
#	define __stdcall(name, bytes) #name
#	pragma comment(linker, "/SUBSYSTEM:WINDOWS,5.2") // Windows XP 64-Bit
#else
#	define __cdecl(name) "_" #name
#	define __stdcall(name, bytes) "_" #name "@" #bytes
#	pragma comment(linker, "/SUBSYSTEM:WINDOWS,5.0") // Windows 2000
#endif

#pragma comment(linker, "/ENTRY:HybridCRTStartup")
#pragma comment(linker, "/EXPORT:EntryPointW="	__stdcall(EntryPointW,	16))
#pragma comment(linker, "/EXPORT:CPlApplet="	__stdcall(CPlApplet,	16))
#pragma comment(linker, "/EXPORT:CustomAction="	__stdcall(CustomAction,	4))

// Create an otherwise irrelevant EAT entry to identify the commit version
extern "C" int const build_git_rev = BUILD_GIT_REV;
#pragma comment(linker, "/EXPORT:" BUILD_GIT_BRANCH BUILD_GIT_SHA "=" __cdecl(build_git_rev))
