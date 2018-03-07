/*---------------------------------------------------------------------------*/
/* File:        main.cpp                                                     */
/* Created:     Fri, 29 Jul 2005 03:23:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 07 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#include "stdafx.h"
#include <cpl.h>
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
	struct SHCORE {
		enum ProcessDpiAwareness {
			ProcessDpiUnaware,
			ProcessSystemDpiAware,
			ProcessPerMonitorDpiAware,
		};
		DllHandle DLL;
		DllImport<HRESULT (WINAPI *)(ProcessDpiAwareness)> SetProcessDpiAwareness;
	} const SHCORE = {
		DllHandle::Load(L"SHCORE"),
		SHCORE.DLL("SetProcessDpiAwareness"),
	};
	if (*SHCORE.SetProcessDpiAwareness)
		(*SHCORE.SetProcessDpiAwareness)(SHCORE.ProcessSystemDpiAware);
	_set_new_handler(sfx_new_handler);
	_set_new_mode(1);
	HRESULT hr = S_OK;
	__try
	{
		hr = CMyComPtr<CSfxExtractEngine>(
			new CSfxExtractEngine(g_hInstance ? g_hInstance : hInstance)
		) -> Run(g_lpszCmdLine ? g_lpszCmdLine : lpszCmdLine);
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

EXTERN_C LONG CALLBACK CPlApplet(HWND, UINT uMsg, LONG, LONG)
{
	switch (uMsg) 
	{
	case CPL_INIT:
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
		info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
		info.hwnd = CreateWindowW(
			L"STATIC", NULL, WS_POPUP, 0, 0,
			GetSystemMetrics(SM_CXSCREEN),
			GetSystemMetrics(SM_CYSCREEN),
			NULL, NULL, NULL, NULL);
		SetFocus(info.hwnd);
		info.nShow = SW_SHOWNORMAL;
		info.lpVerb = L"runas";
		info.lpFile = file;
		info.lpParameters = params;
		DWORD dwExitCode = 0;
		if (ShellExecuteExW(&info))
		{
			WaitForSingleObject(info.hProcess, INFINITE);
			GetExitCodeProcess(info.hProcess, &dwExitCode);
			CloseHandle(info.hProcess);
		}
		else
		{
			dwExitCode = GetLastError();
		}
		ExitProcess(dwExitCode);
		break;
	}
	return 0;
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
#	define __stdcall(name, bytes) #name
#	pragma comment(linker, "/SUBSYSTEM:WINDOWS,5.2") // Windows XP 64-Bit
#else
#	define __stdcall(name, bytes) "_" #name "@" #bytes
#	pragma comment(linker, "/SUBSYSTEM:WINDOWS,5.0") // Windows 2000
#endif

#pragma comment(linker, "/ENTRY:HybridCRTStartup")
#pragma comment(linker, "/EXPORT:EntryPointW="	__stdcall(EntryPointW,	16))
#pragma comment(linker, "/EXPORT:CPlApplet="	__stdcall(CPlApplet,	16))
