/*---------------------------------------------------------------------------*/
/* File:        Helpers.cpp                                                  */
/* Created:     Sat, 30 Jul 2005 11:10:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 08 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#include "stdafx.h"
#include "7zSfxHtmInt.h"

void SetDpiAwareness()
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
}

BOOL SfxCreateDirectory( LPCWSTR path )
{
	DWORD dwAttributes = GetFileAttributesW(path);
	if( (dwAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_NORMAL)) == FILE_ATTRIBUTE_DIRECTORY )
		return TRUE;
	return CreateDirectoryW(path, NULL);
}

LPCWSTR SfxCreateDirectoryPath( LPWSTR path, LPCWSTR granted )
{
	LPWSTR p;
	for (p = path; *p; ++p)
		if (*p == L'/')
			*p = L'\\';
	p = path + PathCommonPrefixW(path, granted, NULL);
	if ((p = PathSkipRootW(p)) == NULL && (p = PathSkipRootW(path)) == NULL)
		p = path;
	if (LPWSTR q = StrRChrW(p, NULL, L'\\'))
	{
		*q = L'\0';
		// Initialize status to ERROR_ALREADY_EXISTS iff the opposite is true.
		DWORD status = GetFileAttributesW(path) ^ ~ERROR_ALREADY_EXISTS;
		*q = L'\\';
		// Loop as long as failing for ERROR_ALREADY_EXISTS.
		while (status == ERROR_ALREADY_EXISTS && (q = StrChrW(p, L'\\')) != NULL)
		{
			*q = L'\0';
			if (!CreateDirectoryW(path, NULL))
				status = GetLastError();
			*q = L'\\';
			p = q + 1;
		}
	}
	return path;
}

BOOL DeleteDirectoryWithSubitems( LPCWSTR path )
{
	WIN32_FIND_DATA	fd;
	UString path2 = path;
	path2 += L"\\*";
	HANDLE hFind = FindFirstFileW( path2, &fd );
	BOOL ok = TRUE;
	if( hFind != INVALID_HANDLE_VALUE )
	{
		do
		{
			path2 = path; path2 += L'\\'; path2 += fd.cFileName;
			if( (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 )
			{
				// file
				ok = SetFileAttributesW( path2, 0 ) && DeleteFileW( path2 );
			}
			else if( fd.cFileName[wcsspn(fd.cFileName, L".")] != L'\0' )
			{
				// directory
				ok = DeleteDirectoryWithSubitems( path2 );
			}
		} while( ok && FindNextFile( hFind, &fd ) );
		FindClose( hFind );
	}
	return ok && SetFileAttributesW(path, 0) && RemoveDirectoryW(path);
}

BOOL DeleteFileOrDirectoryAlways( LPCWSTR path )
{
	DWORD dwAttributes = GetFileAttributesW( path );
	if( dwAttributes == INVALID_FILE_ATTRIBUTES )
		return TRUE;
	if( (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 )
	{
		// file
		return SetFileAttributes( path, 0 ) && DeleteFile( path );
	}
	// delete folder tree
	return DeleteDirectoryWithSubitems( path );
}

LPCWSTR LoadQuotedString( LPCWSTR lpwszSrc, UString &result )
{
	if( *lpwszSrc == L'\"' )
	{
		lpwszSrc++;
		while( *lpwszSrc != L'\0' && *lpwszSrc != L'\"' )
		{
			result += *lpwszSrc;
			lpwszSrc++;
		}
		if( *lpwszSrc != L'\0' )
			lpwszSrc++;
	}
	else
	{
		while( *lpwszSrc != L'\0' && ((unsigned)*lpwszSrc) > L' ' )
		{
			result += *lpwszSrc;
			lpwszSrc++;
		}
	}

	SKIP_WHITESPACES_W( lpwszSrc );
	return lpwszSrc;
}

LPCWSTR IsSubString( LPCWSTR lpwszString, LPCWSTR lpwszSubString )
{
	int const nLength = static_cast<int>(wcslen(lpwszSubString));
	if( _wcsnicmp( lpwszString, lpwszSubString, nLength ) == 0 )
		return lpwszString + nLength;
	return NULL;
}

LPCWSTR IsSfxSwitch( LPCWSTR lpwszCommandLine, LPCWSTR lpwszSwitch )
{
	SKIP_WHITESPACES_W(lpwszCommandLine);
	if( *lpwszCommandLine == L'-' || *lpwszCommandLine == L'/')
	{
		lpwszCommandLine = IsSubString( lpwszCommandLine + 1, lpwszSwitch );
		if( lpwszCommandLine && (unsigned(*lpwszCommandLine) <= L' ' || *lpwszCommandLine == L':') )
		{
			return lpwszCommandLine;
		}
	}
	return NULL;
}

// Languages part

LPCWSTR GetLanguageString( UINT id )
{
	UINT i;
	for( i = 0; SfxLangStrings[i].id != 0; i++ )
	{
		if( SfxLangStrings[i].id == id )
			break;
	}
	if( SfxLangStrings[i].id == 0 )
		return L"";

	if( SfxLangStrings[i].lpszUnicode != NULL )
		return SfxLangStrings[i].lpszUnicode;

	LPCSTR lpszReturn = SfxLangStrings[i].strPrimary;

	int const nLength = static_cast<int>(strlen(lpszReturn)) + 1;
	SfxLangStrings[i].lpszUnicode = new WCHAR[nLength];
	MultiByteToWideChar( CP_UTF8, 0, lpszReturn, nLength, SfxLangStrings[i].lpszUnicode, nLength );

	return SfxLangStrings[i].lpszUnicode;
}

void FreeLanguageStrings()
{
	LANGSTRING * p = SfxLangStrings;
	while( p->id != 0 )
	{
		if( p->lpszUnicode != NULL )
		{
			delete p->lpszUnicode;
			p->lpszUnicode = NULL;
		}
		p++;
	}
}

class CLangStrings
{
public:
	~CLangStrings() { FreeLanguageStrings(); };
};

static CLangStrings __lsf;
