/*
	Copyright (C) 2018 Jochen Neubeck

	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

// This file provides functions to help prevent dll hijacking.
// Articles on the subject:
// https://textslashplain.com/2015/12/18/dll-hijacking-just-wont-die/
// https://unhandledexpression.com/2010/08/23/fixing-the-dll-loading-vulnerability/
// SumatraPDF's SafeLoadLibrary() happens to work like DllHandle::Load():
// https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/utils/WinDynCalls.cpp

#include <stdafx.h>
#include "ImageUtil.h"

struct KERNEL32 {
	static DWORD const LOAD_LIBRARY_SEARCH_APPLICATION_DIR	= 0x00000200;
	static DWORD const LOAD_LIBRARY_SEARCH_USER_DIRS		= 0x00000400;
	static DWORD const LOAD_LIBRARY_SEARCH_SYSTEM32			= 0x00000800;
	static DWORD const LOAD_LIBRARY_SEARCH_DEFAULT_DIRS		= 0x00001000;
	DllHandle DLL;
	DllImport<BOOL (WINAPI *)(LPCWSTR)> SetDllDirectoryW;
	DllImport<BOOL (WINAPI *)(DWORD)> SetDefaultDllDirectories;
} const KERNEL32 = {
	DllHandle::Load(L"KERNEL32"),
	KERNEL32.DLL("SetDllDirectoryW"),
	KERNEL32.DLL("SetDefaultDllDirectories"),
};

STDAPI_(HMODULE) LoadResLibrary(LPCWSTR path)
{
	HMODULE hModule = GetModuleHandle(path);
	if (hModule == NULL)
		hModule = LoadLibraryEx(path, NULL, DONT_RESOLVE_DLL_REFERENCES);
	return hModule;
}

STDAPI ClearSearchPath()
{
	HRESULT hr = SetEnvironmentVariableW(L"PATH", NULL) ? S_OK : S_FALSE;
	hr |= *KERNEL32.SetDllDirectoryW &&
		(*KERNEL32.SetDllDirectoryW)(L"") ? S_OK : S_FALSE;
	hr |= *KERNEL32.SetDefaultDllDirectories &&
		(*KERNEL32.SetDefaultDllDirectories)(KERNEL32.LOAD_LIBRARY_SEARCH_SYSTEM32) ? S_OK : S_FALSE;
	return hr;
}

#define TOKENPASTE(x, y) TOKENPASTE2(x, y)
#define TOKENPASTE2(x, y) x ## y
#define UNIQUE_NAME TOKENPASTE(UNIQUE_NAME_, __LINE__)

namespace Scoped
{
	class CloseHandle
	{
		HANDLE const m_handle;
		CloseHandle &operator=(CloseHandle const &);
	public:
		CloseHandle(HANDLE handle) : m_handle(handle) { }
		~CloseHandle() { ::CloseHandle(m_handle); }
	};
}

STDAPI CreateImage(LPCWSTR src, LPCWSTR dst, DWORD how, DWORD img, DWORD osv)
{
	// Create destination file
	HANDLE hDst = CreateFileW(dst, GENERIC_WRITE, FILE_SHARE_READ, NULL, how, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hDst == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(ERROR_FILE_EXISTS);
	Scoped::CloseHandle UNIQUE_NAME(hDst);
	// Open source file
	HANDLE hSrc = CreateFileW(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hSrc == INVALID_HANDLE_VALUE)
		return E_FAIL;
	Scoped::CloseHandle UNIQUE_NAME(hSrc);
	// Inspect PE structures
	IMAGE_DOS_HEADER mz;
	IMAGE_NT_HEADERS nt;
	DWORD processedSize;
	DWORD qualifiedSize = 0; // size of the part of file which is qualified for copying (excludes overlay!)
	if (!ReadFile(hSrc, &mz, sizeof mz, &processedSize, NULL) || (processedSize != sizeof mz))
		return E_FAIL;
	if (SetFilePointer(hSrc, mz.e_lfanew, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		return E_FAIL;
	if (!ReadFile(hSrc, &nt, sizeof nt, &processedSize, NULL) || (processedSize != sizeof nt))
		return E_FAIL;
	if (nt.Signature != IMAGE_NT_SIGNATURE)
		return E_FAIL;
	if (nt.FileHeader.SizeOfOptionalHeader != sizeof nt.OptionalHeader)
		return E_FAIL;
	for (WORD i = 0; i < nt.FileHeader.NumberOfSections; ++i)
	{
		IMAGE_SECTION_HEADER sh;
		if (!ReadFile(hSrc, &sh, sizeof sh, &processedSize, NULL) || (processedSize != sizeof sh))
			return E_FAIL;
		DWORD endPosition = sh.PointerToRawData + sh.SizeOfRawData;
		if (qualifiedSize < endPosition)
			qualifiedSize = endPosition;
	}
	// Check file size to prevent infinite recursion if common prefix test fails
	if (img == 0 && GetFileSize(hSrc, NULL) <= qualifiedSize)
		return E_FAIL;
	// Seek to begin
	if (SetFilePointer(hSrc, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		return E_FAIL;
	// Copy qualified part of file
	while (DWORD remainingSize = qualifiedSize)
	{
		BYTE buf[8192];
		if (remainingSize > sizeof buf)
			remainingSize = sizeof buf;
		if (!ReadFile(hSrc, buf, remainingSize, &remainingSize, NULL))
			return E_FAIL;
		if (!WriteFile(hDst, buf, remainingSize, &processedSize, NULL) || (processedSize != remainingSize))
			return E_FAIL;
		qualifiedSize -= processedSize;
	}
	// Patch file header as specified
	WORD const mCharacteristics		= IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL;
	WORD const mDllCharacteristics	= IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE;
	WORD const xCharacteristics		= img ? LOWORD(img) ^ nt.FileHeader.Characteristics & mCharacteristics : 0;
	WORD const xDllCharacteristics	= img ? HIWORD(img) ^ nt.OptionalHeader.DllCharacteristics & mDllCharacteristics : 0;
	WORD const xMajorTargetVersion	= osv ? HIWORD(osv) ^ nt.OptionalHeader.MajorOperatingSystemVersion : 0;
	WORD const xMinorTargetVersion	= osv ? LOWORD(osv) ^ nt.OptionalHeader.MinorOperatingSystemVersion : 0;
	if (xCharacteristics | xDllCharacteristics | xMajorTargetVersion | xMinorTargetVersion)
	{
		nt.FileHeader.Characteristics ^= xCharacteristics;
		nt.OptionalHeader.DllCharacteristics ^= xDllCharacteristics;
		nt.OptionalHeader.MajorOperatingSystemVersion ^= xMajorTargetVersion;
		nt.OptionalHeader.MinorOperatingSystemVersion ^= xMinorTargetVersion;
		nt.OptionalHeader.MajorSubsystemVersion = nt.OptionalHeader.MajorOperatingSystemVersion;
		nt.OptionalHeader.MinorSubsystemVersion = nt.OptionalHeader.MinorOperatingSystemVersion;
		if (SetFilePointer(hDst, mz.e_lfanew, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
			return E_FAIL;
		if (!WriteFile(hDst, &nt, sizeof nt, &processedSize, NULL) || (processedSize != sizeof nt))
			return E_FAIL;
	}
	return S_OK;
}

STDAPI CreateSafeImage(LPCWSTR path, LPWSTR temp)
{
	if (temp[PathCommonPrefixW(path, temp, NULL)] == 0)
		return S_FALSE;
	GUID guid;
	HRESULT hr = CoCreateGuid(&guid);
	if (FAILED(hr))
		return hr;
	LPWSTR const name = PathAddBackslashW(temp);
	if (name > temp + 200)
		return E_FAIL;
	StringFromGUID2(guid, name, 40);
	PathAddExtensionW(temp, L".exe");
	return CreateImage(path, temp, CREATE_NEW, 0, 0);
}
