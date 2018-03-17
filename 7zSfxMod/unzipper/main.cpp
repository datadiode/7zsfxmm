#include <malloc.h>
#include <shlwapi.h>
#include <wininet.h>
#include <process.h>
#include "../../midl/FETCH_HEAD.h"

// https://www.rpi.edu/dept/cis/software/g77-mingw32/include/

#include "miniz.h"

static char const apptitle[] = "Unzipper v1.00";

static char const usage[] =
	"Usage:\n"
	"%ls <archive> /o ... [ /i ... ] [ /x ... ]\n"
	"\n"
	"<archive> - specifies the zip file from which to extract content\n"
	"/o - specifies the output folder to use for extraction\n"
	"/i - specifies file inclusion patterns\n"
	"/x - specifies file exclusion patterns\n";

static size_t file_read_func(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n)
{
	HANDLE hFile = static_cast<HANDLE>(pOpaque);
	LARGE_INTEGER li;
	li.QuadPart = file_ofs;
	DWORD dw = SetFilePointer(hFile, li.LowPart, &li.HighPart, FILE_BEGIN);
	if (dw == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
		return 0;
	if (!ReadFile(hFile, pBuf, n, &dw, NULL))
		return 0;
	return dw;
}

static size_t file_write_func(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n)
{
	HANDLE hFile = static_cast<HANDLE>(pOpaque);
	LARGE_INTEGER li;
	li.QuadPart = file_ofs;
	DWORD dw = SetFilePointer(hFile, li.LowPart, &li.HighPart, FILE_BEGIN);
	if (dw == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
		return 0;
	if (!WriteFile(hFile, pBuf, n, &dw, NULL))
		return 0;
	return dw;
}

//---------------------------------------------------------------------------
// @func
//	Tell if the given file system path has a leaf.
// @rdesc
//	TRUE if the path has a leaf, FALSE if not.
// @comm
//	The last component of a path having more than one component is considered
//	the leaf.
// @comm
//	The path is assumed not to end with a backslash. A path ending with a
//	backslash will yield a meaningless return value.
//---------------------------------------------------------------------------
static BOOL PathHasLeaf(LPCWSTR path)
{
	return StrRChrW(path, NULL, L'\\') >= path + 2;
}

//---------------------------------------------------------------------------
// @func
//	Create the missing parent directories on the given file system path.
// @comm
//	The argument is subject to temporary side effects, but will be restored
//	to the passed-in value.
//---------------------------------------------------------------------------
static void CreateMissingParentDirectories(LPWSTR path)
{
	LPWSTR q = path;
	if (LPWSTR p = StrRChrW(q, NULL, L'\\'))
	{
		*p = L'\0';
		// If path does not have a leaf, or points to an already existing leaf...
		if ((!PathHasLeaf(path)) || (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES))
			q = p + 1; // ...advance q beyond last backslash so loop below won't execute
		*p = L'\\';
	}
	while (LPWSTR p = StrChrW(q, L'\\'))
	{
		*p = L'\0';
		// If path has a leaf...
		if (PathHasLeaf(path))
			CreateDirectoryW(path, NULL); // ...attempt to create it as a directory
		*p = L'\\';
		q = p + 1;
	}
}

static int ConvertPath(LPCSTR src, UINT cp, LPWSTR dst, int len)
{
	int n = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, src, -1, dst, len);
	for (int i = 0; i < n; ++i)
		if (dst[i] == L'/')
			dst[i] = L'\\';
	return n;
}

static LPCWSTR MatchPath(LPCWSTR full, LPWSTR spec)
{
	LPCWSTR path = NULL;
	do
	{
		path = StrRChrW(full, path, L'\\');
		LPCWSTR tail = path ? path + 1 : full;
		LPWSTR p = spec;
		do
		{
			LPWSTR q = StrChrW(p, L'|');
			if (q)
				*q = L'\0';
			LPWSTR r = StrChrW(p, L':');
			if (r)
				*r = L'\\';
			if (*p != L'\\' ? PathMatchSpecW(tail, p) : // relative match
				tail == full && PathMatchSpecW(tail, ++p)) // absolute match
			{
				while (p < r)
				{
					p = PathFindNextComponentW(p);
					tail = PathFindNextComponentW(tail);
					full = tail;
				}
				p = NULL;
			}
			if (r)
				*r = L':';
			if (q)
				*q++ = L'|';
			if (p == NULL)
				return full;
			p = q;
		} while (p != NULL);
	} while (path != NULL);
	return NULL;
}

static int Run(LPWSTR cmdline)
{
	LPWSTR appname = NULL;
	LPWSTR archive = NULL;
	LPWSTR base = NULL;
	LPWSTR include = NULL;
	LPWSTR exclude = NULL;
	LPWSTR *parg = &archive;
	LPWSTR p = cmdline;
	WCHAR sep = L'\0';
	do
	{
		LPWSTR q = PathGetArgsW(p);
		PathRemoveArgsW(p);
		PathUnquoteSpacesW(p);
		if (p == cmdline)
		{
			appname = PathFindFileNameW(p);
		}
		else if (*p != L'/')
		{
			if (const LPWSTR arg = *parg)
			{
				if (sep == L'\0')
					break;
				const int len = lstrlenW(p);
				const int cur = lstrlenW(arg);
				arg[cur] = sep;
				lstrcpyW(arg + cur + 1, p);
			}
			else
			{
				*parg = p;
			}
		}
		else if (lstrcmpiW(p + 1, L"o") == 0)
		{
			sep = L'\0';
			parg = &base;
		}
		else if (lstrcmpiW(p + 1, L"i") == 0)
		{
			sep = L'|';
			parg = &include;
		}
		else if (lstrcmpiW(p + 1, L"x") == 0)
		{
			sep = L'|';
			parg = &exclude;
		}
		else
			break;
		p = q + StrSpnW(q, L" \t\r\n");
	} while (*p != L'\0');

	// If needed arguments are missing, or unconsumed arguments exist, give up.
	if ((archive == NULL) || (base == NULL) || (*p != L'\0'))
	{
		CHAR text[1024];
		wnsprintfA(text, _countof(text), usage, appname);
		MessageBoxA(NULL, text, apptitle, MB_OK);
		return 1;
	}

	LPWSTR full = NULL;
	if (DWORD size = GetFullPathNameW(base, 0, NULL, NULL))
	{
		full = static_cast<LPWSTR>(_alloca(size + 1 + MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE));
		if (GetFullPathNameW(base, size, full, &base) != size - 1)
			return 2;
	}
	base = PathAddBackslashW(base);

	HANDLE hFile = CreateFileW(archive, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return 3;

	ULARGE_INTEGER fileSize;
	fileSize.LowPart = GetFileSize(hFile, &fileSize.HighPart);

	mz_zip_archive zip_archive;
	memset(&zip_archive, 0, sizeof zip_archive);

	zip_archive.m_pIO_opaque = hFile;
	zip_archive.m_pRead = file_read_func;

	int i = 0;
	int n = -1;
	if (mz_zip_reader_init(&zip_archive, fileSize.QuadPart, 0))
	{
		for (n = mz_zip_reader_get_num_files(&zip_archive); i < n; ++i)
		{
			mz_zip_archive_file_stat file_stat;
			WCHAR path[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
			if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
			{
				n = -2;
			}
			else if (!ConvertPath(file_stat.m_filename, file_stat.m_bit_flag & 2048 ? CP_UTF8 : 437, path, _countof(path)))
			{
				n = -3;
			}
			else
			{
				if (mz_zip_reader_is_file_a_directory(&zip_archive, i))
					continue;
				LPCWSTR filename = include ? MatchPath(path, include) : path;
				if (filename == NULL || exclude && MatchPath(path, exclude))
					continue;
				lstrcpyW(base, filename);
				CreateMissingParentDirectories(full);
				HANDLE hFile = CreateFileW(full, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile == INVALID_HANDLE_VALUE)
				{
					n = -4;
				}
				else
				{
					ULONGLONG ft = UInt32x32To64(file_stat.m_time, 10000000) + 116444736000000000;
					if (!mz_zip_reader_extract_to_callback(&zip_archive, i, file_write_func, hFile, 0))
					{
						n = -5;
						SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
						SetEndOfFile(hFile);
					}
					SetFileTime(hFile, NULL, NULL, reinterpret_cast<FILETIME *>(&ft));
					CloseHandle(hFile);
				}
			}
		}
	}

	// Close the archive, freeing any resources it was using
	mz_zip_reader_end(&zip_archive);

	CloseHandle(hFile);
	return n < 0 ? 99 - n : 0;
}

EXTERN_C int WINAPI MyStartup()
{
    __security_init_cookie();
    ExitProcess(Run(GetCommandLineW()));
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

// Create an otherwise irrelevant EAT entry to identify the commit version
extern "C" int const build_git_rev = BUILD_GIT_REV;
#pragma comment(linker, "/EXPORT:" BUILD_GIT_BRANCH BUILD_GIT_SHA "=" __cdecl(build_git_rev))
