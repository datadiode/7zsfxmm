/*---------------------------------------------------------------------------*/
/* File:        ExtractEngine.cpp                                            */
/* Created:     Wed, 05 Oct 2005 07:36:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: 2018-03-13 by https://github.com/datadiode                   */
/*---------------------------------------------------------------------------*/
#include "stdafx.h"
#include <urlmon.h>
#include "7zSfxHtmInt.h"
#include "../C/Lzma86.h"

#include "ExtractEngine.h"
#include "../../midl/7zSfxHtm_i.c"		// Make MY_QUERYINTERFACE_ENTRY() happy

using namespace NWindows;
using NWindows::NFile::NIO::CInFile;
using NWindows::NFile::NIO::COutFile;

#define DECLARE_BSTR_CONSTANT(name, content) \
	struct name \
	{ \
		UINT size; \
		WCHAR text[_countof(content)]; \
		operator BSTR() const { return const_cast<BSTR>(text); } \
	} const name = { (_countof(content) - 1) * sizeof(WCHAR), content }

DECLARE_BSTR_CONSTANT(BSTR_onerror,		L"onerror");
DECLARE_BSTR_CONSTANT(BSTR_prompt,		L"prompt");
DECLARE_BSTR_CONSTANT(BSTR_buttons,		L"buttons");
DECLARE_BSTR_CONSTANT(BSTR_title,		L"title");

class FindResIndex
{
	int index;
	BOOL found;
	LPWSTR const name;
	static BOOL CALLBACK EnumResNameProc(HMODULE, LPCWSTR, LPWSTR lpName, LONG_PTR lParam)
	{
		FindResIndex *const p = reinterpret_cast<FindResIndex *>(lParam);
		if (IS_INTRESOURCE(lpName) || IS_INTRESOURCE(p->name) ?
			lpName == p->name : _wcsicmp(lpName, p->name) == 0)
		{
			p->found = TRUE;
			return FALSE;
		}
		++p->index;
		return TRUE;
	}
	FindResIndex(FindResIndex const &);
	FindResIndex &operator=(FindResIndex const &);
public:
	FindResIndex(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName)
		: index(0)
		, found(FALSE)
		, name(lpName)
	{
		EnumResourceNamesW(hModule, lpType, EnumResNameProc, reinterpret_cast<LONG_PTR>(this));
	}
	operator int() const { return found ? index : -1; }
};

static UINT const MsgBoxMessage = RegisterWindowMessage(L"7zSfxHtm.MsgBoxMessage");

int CSfxExtractEngine::MsgBoxHandler(MSGBOXPARAMS *params)
{
	if (m_hExtractWindow)
	{
		DWORD const dwSenderId = GetCurrentThreadId();
		DWORD const dwReceiverId = GetWindowThreadProcessId(m_hExtractWindow, NULL);
		if (dwSenderId != dwReceiverId)
		{
			if (PostThreadMessage(dwReceiverId, MsgBoxMessage, dwSenderId, reinterpret_cast<LPARAM>(params)))
			{
				MSG msg;
				if (GetMessage(&msg, NULL, WM_COMMAND, WM_COMMAND))
					return static_cast<int>(msg.wParam);
			}
			return -1;
		}
	}
	if (m_spElement3 && m_spDialog)
	{
		VARIANT_BOOL fCancelled = VARIANT_FALSE;
		VARIANT var;
		InitVariantFromString(params->lpszText, &var);
		m_spElement->setAttribute(BSTR_prompt, var, 0);
		VariantClear(&var);
		InitVariantFromString(params->lpszCaption, &var);
		m_spElement->setAttribute(BSTR_title, var, 0);
		VariantClear(&var);
		InitVariantFromInt32(params->dwStyle, &var);
		m_spElement->setAttribute(BSTR_buttons, var, 0);
		VariantClear(&var);
		m_spDialog->put_returnValue(var);
		m_spElement3->fireEvent(BSTR_onerror, NULL, &fCancelled);
		m_spDialog->get_returnValue(&var);
		if (V_VT(&var) != VT_EMPTY)
		{
			int result = SUCCEEDED(VariantChangeType(&var, &var, 0, VT_I4)) ? V_I4(&var) : -1;
			VariantClear(&var);
			return result;
		}
	}
	return MessageBoxIndirect(params);
}

int CSfxExtractEngine::MsgBox( LPCWSTR prompt, LPCWSTR title, DWORD buttons )
{
	MSGBOXPARAMS params;
	memset( &params, 0, sizeof params );
	params.cbSize = sizeof params;
	params.hwndOwner = m_hExtractWindow;
	params.hInstance = m_hRsrcModule;
	params.lpszText = prompt;
	params.lpszCaption = title;
	params.dwStyle = buttons;
	return MsgBoxHandler( &params );
}

void CSfxExtractEngine::ShowSfxErrorDialog( LPCWSTR prompt )
{
	SetTaskbarState( TBPF_ERROR );
	MsgBox( prompt, GetLanguageString(STR_ERROR_TITLE), MB_ICONSTOP );
}

#ifdef _SFX_USE_WARNINGS
int CSfxExtractEngine::ShowSfxWarningDialog( LPCWSTR prompt )
{
	SetTaskbarState( TBPF_PAUSED );
	return MsgBox( prompt, GetLanguageString(STR_WARNING_TITLE), MB_YESNO | MB_ICONWARNING );
}
#endif // _SFX_USE_WARNINGS

void CSfxExtractEngine::SfxErrorDialog( DWORD dwError, UINT idFormat, ... )
{
	WCHAR buf[1024];
	va_list va;

	LPCWSTR lpwszFormat = GetLanguageString( idFormat );
	va_start( va, idFormat );
	int nMessageLength = wvsprintf( buf, lpwszFormat, va );

	if( dwError )
	{
		LPWSTR lpMsgBuf;
		if (DWORD dwMsgLen = ::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL,
			dwError, LANG_NEUTRAL, reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, &va))
		{
			LPWSTR lpszFullMessage = new WCHAR[nMessageLength + dwMsgLen + 2];
			wcscpy( lpszFullMessage, buf );
			lpszFullMessage[nMessageLength] = L'\n';
			wcscpy( lpszFullMessage + nMessageLength + 1, lpMsgBuf );
			ShowSfxErrorDialog( lpszFullMessage );
			delete[] lpszFullMessage;
			::LocalFree( lpMsgBuf );
			return;
		}
	}
	ShowSfxErrorDialog( buf );
}

STDMETHODIMP CSfxExtractEngine::SetTotal( UInt64 total )
{
	m_total = total;
	return S_OK;
}

STDMETHODIMP CSfxExtractEngine::SetCompleted( const UInt64 *completeValue )
{
	if( completeValue != NULL )
		m_completed = *completeValue;
	if( m_spTaskbarList && m_hExtractWindow )
		m_spTaskbarList->SetProgressValue( m_hExtractWindow, m_completed, m_total );
	return S_OK;
}

STDMETHODIMP CSfxExtractEngine::PrepareOperation( Int32 askExtractMode )
{
	m_extractMode = askExtractMode;
	return S_OK;
}

STDMETHODIMP CSfxExtractEngine::SetOperationResult( Int32 resultEOperationResult )
{
	if( resultEOperationResult != NArchive::NExtract::NOperationResult::kOK )
	{
		m_ErrorCode = resultEOperationResult;
		return E_FAIL;
	}

	if( m_outFileStream != NULL )
		m_outFileStreamSpec->SetMTime( &m_processedFileInfo.UTCLastWriteTime );
	m_outFileStream.Release();
	if( m_extractMode == NArchive::NExtract::NAskMode::kExtract )
		SetFileAttributes( m_diskFilePath, m_processedFileInfo.Attributes );
	return S_OK;
}

STDMETHODIMP CSfxExtractEngine::SayInternalError( Int32 errc, DWORD dwLastError )
{
	if( errc == SfxErrors::seCreateFile )
		SfxErrorDialog( dwLastError, ERR_CREATE_FILE, m_diskFilePath.Ptr() );
	else
	{
		if( errc == SfxErrors::seOverwrite )
			SfxErrorDialog( dwLastError, ERR_OVERWRITE, m_diskFilePath.Ptr() );
	}
	return SetOperationResult( errc );
}

STDMETHODIMP CSfxExtractEngine::GetStream( UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode )
{
	*outStream = NULL;
	if( askExtractMode != NArchive::NExtract::NAskMode::kExtract )
		 return S_OK;

	m_outFileStream.Release();

	NCOM::CPropVariant propVariantName;
	RINOK(m_archive.GetProperty(index, kpidPath, &propVariantName));
	if( propVariantName.vt == VT_EMPTY || propVariantName.vt != VT_BSTR )
		return SetOperationResult( SfxErrors::sePropVariant1 );

	m_diskFilePath = SfxCreateDirectoryPath(const_cast<LPWSTR>((m_path + propVariantName.bstrVal).Ptr()), m_diskFilePath);

	NCOM::CPropVariant propVariant;
	RINOK(m_archive.GetProperty(index, kpidAttrib, &propVariant));

	if (propVariant.vt == VT_EMPTY)
		m_processedFileInfo.Attributes = 0;
	else
	{
		if (propVariant.vt != VT_UI4)
			return SetOperationResult( SfxErrors::sePropVariant2 );
		m_processedFileInfo.Attributes = propVariant.ulVal;
	}

	RINOK(m_archive.GetProperty(index, kpidIsDir, &propVariant));
	m_processedFileInfo.IsDirectory = propVariant.boolVal != VARIANT_FALSE;

	RINOK(m_archive.GetProperty(index, kpidMTime, &propVariant));
	SYSTEMTIME	currTime;
	switch( propVariant.vt )
	{
	case VT_EMPTY:
		::GetLocalTime( &currTime );
		::SystemTimeToFileTime( &currTime, &m_processedFileInfo.UTCLastWriteTime );
		break;
	case VT_FILETIME:
		m_processedFileInfo.UTCLastWriteTime = propVariant.filetime;
		break;
	default:
		return SetOperationResult( SfxErrors::sePropVariant3 );
	}

	if( m_processedFileInfo.IsDirectory )
	{
		if( SfxCreateDirectory( m_diskFilePath ) == FALSE )
			return SetOperationResult( SfxErrors::seCreateFolder );
		return S_OK;
	}
		
	switch( GetOverwriteMode( m_diskFilePath, &m_processedFileInfo.UTCLastWriteTime ) )
	{
	case SFX_OM_SKIP:
		return S_OK;
	case SFX_OM_ERROR:
		return SayInternalError( SfxErrors::seOverwrite, GetLastError() );
	}

	m_outFileStreamSpec = new COutFileStream;
	CMyComPtr<ISequentialOutStream> outStreamLoc(m_outFileStreamSpec);
	if( m_outFileStreamSpec->Create( m_diskFilePath, true ) == false )
		return SayInternalError( SfxErrors::seCreateFile, GetLastError() );
	m_outFileStream = outStreamLoc;
	*outStream = outStreamLoc.Detach();

	return S_OK;
}

STDMETHODIMP CSfxExtractEngine::FindBehavior(BSTR bstrBehavior, BSTR bstrBehaviorUrl, IElementBehaviorSite *pSite, IElementBehavior **ppBehavior)
{
	LPCWSTR pwszBehaviorParams = wcschr(bstrBehavior, L'?');
	if (pwszBehaviorParams == NULL)
		pwszBehaviorParams = bstrBehavior + wcslen(bstrBehavior);
	if (IsSubString(bstrBehaviorUrl, L"#7zSfxHtm#"))
	{
		if (IsSubString(bstrBehavior, L"Host") == pwszBehaviorParams)
		{
			SafeInvoke(pSite)->GetElement(&m_spElement);
			CMyComPtr<IDispatch> spDispatch;
			SafeInvoke(m_spElement)->QueryInterface(&m_spElement3);
			SafeInvoke(m_spElement)->get_document(&spDispatch);
			SafeInvoke(spDispatch)->QueryInterface(&m_spOleWindow);
			SafeInvoke(spDispatch)->QueryInterface(&m_spDocument2);
			SafeInvoke(m_spDocument2)->get_parentWindow(&m_spWindow2);
			spDispatch.Release();
			SafeInvoke(m_spWindow2)->get_external(&spDispatch);
			SafeInvoke(spDispatch)->QueryInterface(&m_spDialog);
			// If caller wants different features, save them and close
			if (*pwszBehaviorParams++ &&
				m_features.Compare(pwszBehaviorParams) != 0)
			{
				// Release interfaces acquired from preliminary dialog instance
				m_spDialog.Release();
				m_spWindow2.Release();
				m_spDocument2.Release();
				m_spOleWindow.Release();
				m_spElement3.Release();
				m_spElement.Release();
				m_features = pwszBehaviorParams;
				ExitProcess(Modal(HTMLDLG_MODAL | HTMLDLG_VERIFY));
			}
			return QueryInterface(IID_IElementBehavior, (void **)ppBehavior);
		}
	}
	return E_INVALIDARG;
}

STDMETHODIMP CSfxExtractEngine::get_Path(BSTR *pbsPath)
{
	SysReAllocString(pbsPath, m_sfxpath);
	return S_OK;
}

STDMETHODIMP CSfxExtractEngine::get_Hwnd(LONG *plHwnd)
{
	*plHwnd = reinterpret_cast<LONG>(m_hExtractWindow);
	return S_OK;
}

STDMETHODIMP CSfxExtractEngine::get_ExitCode(LONG *plExitCode)
{
	if (WaitForSingleObject(m_hExtractThread, 100) == 0)
	{
#ifdef _DEBUG
		ShowSfxErrorDialog(L"get_ExitCode(): ExtractThread finished");
#endif
	}
	GetExitCodeThread(m_hExtractThread, reinterpret_cast<DWORD *>(plExitCode));
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (msg.message == MsgBoxMessage)
		{
			int result = MsgBoxHandler(reinterpret_cast<MSGBOXPARAMS *>(msg.lParam));
			PostThreadMessage(static_cast<DWORD>(msg.wParam), WM_COMMAND, static_cast<WPARAM>(result), 0);
			continue;
		}
		DispatchMessage(&msg);
	}
	return S_OK;
}

STDMETHODIMP CSfxExtractEngine::get_Progress(DOUBLE *pdProgress)
{
	*pdProgress = m_total ? static_cast<DOUBLE>(m_completed) / static_cast<DOUBLE>(m_total) : 0;
	return S_OK;
}

BOOL CALLBACK CSfxExtractEngine::SfxEnumResNameProc(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName, LONG_PTR lParam)
{
	CSfxExtractEngine *const pThis = reinterpret_cast<CSfxExtractEngine *>(lParam);
	HRSRC const hFindRes = FindResource(hModule, lpName, lpType);
	WCHAR szName[8];
	ATOM atom = 0;
	if (IS_INTRESOURCE(lpName))
	{
		atom = reinterpret_cast<ATOM>(lpName);
		GetAtomNameW(atom, szName, _countof(szName));
		lpName = szName;
	}
	if (!PathMatchSpec(lpName, pThis->m_filter.Ptr()))
		return TRUE;
	HGLOBAL const hLoadRes = LoadResource(hModule, hFindRes);
	Byte *inBuffer = static_cast<Byte *>(LockResource(hLoadRes));
	UInt32 inSize32 = SizeofResource(hModule, hFindRes);
	CBuffer<Byte> outBuffer;
	if (lpType == RT_MANIFEST)
	{
		lpName = L"7ZSfxMod.exe.manifest";
	}
	else if (lpType == RT_RCDATA)
	{
		size_t inSize = inSize32;
		UInt64 outSize64;
		if (Lzma86_GetUnpackSize(inBuffer, inSize, &outSize64) != 0)
			return FALSE; // data error
		size_t outSize = static_cast<UInt32>(outSize64);
		if (outSize != outSize64)
			return FALSE; // Unpack size is too big
		outBuffer.Alloc(outSize);
		if (Lzma86_Decode(outBuffer, &outSize, inBuffer, &inSize) != 0)
			return FALSE; // Decode error
		if (outSize != static_cast<UInt32>(outSize64))
			return FALSE; // incorrect processed size
		inBuffer = outBuffer;
		inSize32 = static_cast<UInt32>(outSize);
	}
	if (pThis->m_hUpdate)
	{
		if (atom)
		{
			lpType = MAKEINTATOM(atom);
			lpName = MAKEINTRESOURCE(1);
		}
		else
		{
			lpType = PathFindExtensionW(lpName);
			lpType = wcscmp(lpType, L".ICO") == 0 ? RT_GROUP_ICON :
				wcscmp(lpType, L".MANIFEST") == 0 ? RT_MANIFEST : RT_HTML;
		}
		if (lpType == RT_GROUP_ICON)
		{
			IconHeader const *const qHeader = reinterpret_cast<IconHeader *>(static_cast<Byte *>(inBuffer));
			IconFileRecord const *qEntry = reinterpret_cast<IconFileRecord const *>(qHeader + 1);
			WORD const n = qHeader->idCount;
			UInt32 outSize = sizeof(IconHeader) + n * sizeof(IconGroupRecord);
			CBuffer<Byte> outBuffer;
			outBuffer.Alloc(outSize);
			IconHeader *const pHeader = reinterpret_cast<IconHeader *>(&outBuffer[0]);
			IconGroupRecord *pEntry = reinterpret_cast<IconGroupRecord *>(pHeader + 1);
			*pHeader = *qHeader;
			pHeader->idCount = 0;
			outSize = sizeof(IconHeader);
			WORD nId = pThis->m_iconid;
			for (WORD i = 0; i < n; ++i)
			{
				BITMAPINFOHEADER *pBIH = reinterpret_cast<BITMAPINFOHEADER *>(&inBuffer[qEntry->dwOffset]);
				++pHeader->idCount;
				outSize += sizeof(IconGroupRecord);
				memcpy(pEntry, qEntry, offsetof(IconGroupRecord, nId));
				pEntry->nId = nId;
				pEntry->wPlanes = pBIH->biPlanes;
				pEntry->wBitCount = pBIH->biBitCount;
				++nId;
				++pEntry;
				++qEntry;
			}
			// Update RT_GROUP_ICON *before* RT_ICON so as to not end up in a mess!
			BOOL fUpdate = UpdateResourceW(pThis->m_hUpdate, RT_GROUP_ICON, lpName, 0, pHeader, outSize);
			(fUpdate ? pThis->m_fDiscard : pThis->m_fSuccess) = FALSE;
			pEntry = reinterpret_cast<IconGroupRecord *>(pHeader + 1);
			qEntry = reinterpret_cast<IconFileRecord const *>(qHeader + 1);
			nId = pThis->m_iconid;
			for (WORD i = 0; i < n; ++i)
			{
				BITMAPINFOHEADER *pBIH = reinterpret_cast<BITMAPINFOHEADER *>(&inBuffer[qEntry->dwOffset]);
				BOOL fUpdate = UpdateResourceW(pThis->m_hUpdate, RT_ICON, MAKEINTRESOURCE(nId), 0, pBIH, qEntry->dwBytesInRes);
				(fUpdate ? pThis->m_fDiscard : pThis->m_fSuccess) = FALSE;
				++nId;
				++pEntry;
				++qEntry;
			}
			pThis->m_iconid = nId;
		}
		else
		{
			BOOL fUpdate = UpdateResourceW(pThis->m_hUpdate, lpType, lpName, 0, inBuffer, inSize32);
			(fUpdate ? pThis->m_fDiscard : pThis->m_fSuccess) = FALSE;
		}
	}
	else
	{
		UString OutFileName = pThis->m_path.Back() != '*' ? pThis->m_path + lpName :
			pThis->m_path.Left(pThis->m_path.ReverseFind_Dot()) + PathFindExtension(lpName);
		COutFile OutFile;
		if (!OutFile.Open(OutFileName, CREATE_ALWAYS))
			return FALSE;
		UInt32 processedSize;
		if (!OutFile.Write(inBuffer, inSize32, processedSize) || (processedSize != inSize32))
			return FALSE;
	}
	return TRUE;
}

BOOL CSfxExtractEngine::ExtractResources(HMODULE hModule, LPWSTR lpType)
{
	SfxCreateDirectoryPath(const_cast<LPWSTR>(m_path.Ptr()));
	return EnumResourceNamesW(hModule, lpType, SfxEnumResNameProc, reinterpret_cast<LONG_PTR>(this));
}

STDMETHODIMP CSfxExtractEngine::ExtractPayload(BSTR bsPath)
{
	m_path = bsPath;
	m_total = 0;
	m_completed = 0;
	if (m_path.ReverseFind_PathSepar() + 1 < static_cast<int>(m_path.Len()))
		m_path.Add_PathSepar();
	if (m_hExtractThread)
		CloseHandle(m_hExtractThread);
	m_hExtractThread = ::CreateThread(NULL, 0, ExtractThread, this, 0, NULL);
	return S_OK;
}

STDMETHODIMP CSfxExtractEngine::ExtractAdjunct(BSTR bsPath, BSTR bsFilter)
{
	m_path = bsPath;
	m_filter = bsFilter;
	m_iconid = 1;
	m_hUpdate = NULL;
	m_fDiscard = TRUE;
	m_fSuccess = TRUE;
	if (m_path.ReverseFind_PathSepar() + 1 < static_cast<int>(m_path.Len()) && m_path.Back() != L'*')
	{
		m_hUpdate = BeginUpdateResourceW(m_path, FALSE);
		if (m_hUpdate == NULL)
		{
#ifdef _DEBUG
			SfxErrorDialog(GetLastError(), ERR_CREATE_FOLDER, m_path.Ptr());
#endif
			m_path.Add_PathSepar();
		}
	}
	SfxCreateDirectoryPath(const_cast<LPWSTR>(m_path.Ptr()));
	EnumResourceNamesW(m_hRsrcModule, RT_RCDATA, SfxEnumResNameProc, reinterpret_cast<LONG_PTR>(this));
	if (m_hUpdate)
	{
		EndUpdateResourceW(m_hUpdate, m_fDiscard);
		m_hUpdate = NULL;
	}
	return S_OK;
}

HRESULT CSfxExtractEngine::put_OverwriteMode(LONG lOverwriteMode)
{
	m_overwriteMode = lOverwriteMode;
	return S_OK;
}

HRESULT CSfxExtractEngine::get_OverwriteMode(LONG *plOverwriteMode)
{
	*plOverwriteMode = m_overwriteMode;
	return S_OK;
}

HRESULT CSfxExtractEngine::put_WindowState(LONG lWindowState)
{
	ShowWindow(m_hExtractWindow, lWindowState);
	return S_OK;
}

HRESULT CSfxExtractEngine::get_VersionString(BSTR bsKey, BSTR *pbsValue)
{
	SysReAllocString(pbsValue, m_pVersionStrings->Find(bsKey)->Data());
	return S_OK;
}

HRESULT CSfxExtractEngine::put_Password(BSTR bsPassword)
{
	SysReAllocString(&m_password, bsPassword);
	return S_OK;
}

HRESULT CSfxExtractEngine::FindIcon(BSTR bsPath, BSTR bsName, BSTR *pbsLink)
{
	if (HMODULE hModule = LoadResLibrary(bsPath))
	{
		int iIcon = FindResIndex(hModule, RT_GROUP_ICON, bsName);
		WCHAR path[MAX_PATH + 40];
		wsprintfW(path, L"%s,%d", bsPath, iIcon);
		SysReAllocString(pbsLink, path);
	}
	return S_OK;
}

int CSfxExtractEngine::DeleteUseOverwriteFlags( LPCWSTR lpwszPath )
{
	if( DeleteFileOrDirectoryAlways( lpwszPath ) == FALSE )
	{
		DWORD dwLastError = GetLastError();
		if( (dwLastError == ERROR_ACCESS_DENIED || dwLastError == ERROR_SHARING_VIOLATION)
				&& (m_overwriteMode & OVERWRITE_FLAG_SKIP_LOCKED) != 0 )
		{
			return SFX_OM_SKIP;
		}
		return SFX_OM_ERROR;
	}
	return SFX_OM_OVERWRITE;
}

int CSfxExtractEngine::GetOverwriteMode( LPCWSTR lpwszPath, FILETIME * fileTime )
{
	WIN32_FIND_DATA	fd;
	HANDLE hFind = ::FindFirstFile( lpwszPath, &fd );
	if( hFind == INVALID_HANDLE_VALUE )
		return SFX_OM_OVERWRITE;
	::FindClose( hFind );
	if( fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY )
	{
		::SetLastError( ERROR_CURRENT_DIRECTORY );
		return SFX_OM_ERROR;
	}
	if( m_overwriteMode == OVERWRITE_MODE_ALL )
		return DeleteUseOverwriteFlags( lpwszPath );

	// OverwriteMode: none, older or confirm
	if( m_overwriteMode == OVERWRITE_MODE_OLDER )
	{
		if( CompareFileTime( &fd.ftLastWriteTime, fileTime ) >= 0 )
			return SFX_OM_SKIP;
		return DeleteUseOverwriteFlags( lpwszPath );
	}
	// OverwriteMode: none or confirm
	return SFX_OM_SKIP;
}

HRESULT CSfxExtractEngine::Modal(DWORD flags)
{
	struct MSHTML {
		DllHandle DLL;
		DllImport<SHOWHTMLDIALOGEXFN *> ShowHTMLDialogEx;
	} const MSHTML = {
		DllHandle::Load(L"MSHTML"),
		MSHTML.DLL("ShowHTMLDialogEx"),
	};

	m_ErrorCode = NArchive::NExtract::NOperationResult::kOK;

	VARIANT in, out;
	InitVariantFromDispatch(this, &in);
	VariantInit(&out);
	HRESULT hr;
	if (FAILED(hr = (*MSHTML.ShowHTMLDialogEx)(NULL, m_spMoniker, flags, &in, const_cast<LPWSTR>(m_features.Ptr()), &out)))
		return hr;
	if (FAILED(hr = VariantChangeType(&out, &out, 0, VT_I4)))
		return hr;
	hr = V_I4(&out);
	VariantClear(&in);
	VariantClear(&out);

	if (m_hExtractThread != NULL)
	{
		WaitForSingleObject(m_hExtractThread, INFINITE);
	}

	if( m_ErrorCode != NArchive::NExtract::NOperationResult::kOK )
	{
		switch( m_ErrorCode )
		{
		case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
			SfxErrorDialog( 0, ERR_7Z_UNSUPPORTED_METHOD );
			break;
		case NArchive::NExtract::NOperationResult::kCRCError:
			SfxErrorDialog( 0, ERR_7Z_CRC_ERROR );
			break;
		case NArchive::NExtract::NOperationResult::kDataError:
			SfxErrorDialog( 0, ERR_7Z_DATA_ERROR );
			break;
		case SfxErrors::seCreateFolder:
		case SfxErrors::seCreateFile:
		case SfxErrors::seOverwrite:
#ifdef SFX_CRYPTO
		case SfxErrors::seNoPassword:
#endif // SFX_CRYPTO
			break;
		default:
			SfxErrorDialog( 0, ERR_7Z_INTERNAL_ERROR, m_ErrorCode );
		}
		return E_FAIL;
	}
	return S_OK;
}

HRESULT CSfxExtractEngine::Extract()
{
	struct URLMON {
		DllHandle DLL;
		DllImport<HRESULT (STDAPICALLTYPE *)(LPMONIKER, LPCWSTR, LPMONIKER *)> CreateURLMoniker;
	} const URLMON = {
		DllHandle::Load(L"URLMON"),
		URLMON.DLL("CreateURLMoniker"),
	};
	if (!*URLMON.CreateURLMoniker)
		return HRESULT_FROM_WIN32(GetLastError());
	WCHAR url[MAX_PATH + 40];
	wsprintf(url, L"res://%s/#1", m_sfxpath.Ptr());
	HRESULT hr = (*URLMON.CreateURLMoniker)(NULL, url, &m_spMoniker);
	if (SUCCEEDED(hr))
		hr = Modal(HTMLDLG_MODAL | HTMLDLG_VERIFY | HTMLDLG_NOUI);
	return hr;
}

HRESULT CSfxExtractEngine::ExtractWorker()
{
	UInt32 *indices = NULL;
	UInt32 num_indices = (UInt32)-1;
#ifdef _SFX_USE_EXTRACT_MASK
	if( m_numIndices > 0 && m_indices != NULL )
	{
		indices = m_indices;
		num_indices = m_numIndices;
	}
#endif // _SFX_USE_EXTRACT_MASK
	m_diskFilePath.Empty();
	HRESULT result = m_archive.GetHandler()->Extract( indices, num_indices, 0, this );
#ifdef _DEBUG
	ShowSfxErrorDialog(L"ExtractWorker(): ExtractThread finished");
#endif
	return result;
}

DWORD WINAPI CSfxExtractEngine::ExtractThread( LPVOID pThis )
{
	HRESULT result = E_FAIL;
	__try
	{
		result = static_cast<CSfxExtractEngine *>(pThis)->ExtractWorker();
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		char msg[256];
		wsprintfA(msg, "An exception of type 0x%08lX has occurred", GetExceptionCode());
		FatalAppExitA(0, msg);
	}
	return result;
}

#ifdef SFX_CRYPTO
STDMETHODIMP CSfxExtractEngine::CryptoGetTextPassword(BSTR *password)
{
	SysReAllocString(password, m_password);
	return S_OK;
}
#endif // SFX_CRYPTO

void CSfxExtractEngine::AboutBox()
{
	extern unsigned int g_NumCodecs;
	extern const CCodecInfo *g_Codecs[];
	UString ustrVersion = GetLanguageString( STR_SFXVERSION );
	unsigned int ki;
	for( ki = 0; ki < g_NumCodecs; ki++ )
	{
		ustrVersion.AddAscii( g_Codecs[ki]->Name );
		ustrVersion += L", ";
	}
#ifdef SFX_VOLUMES
	ustrVersion += L"Volumes, ";
#endif // SFX_VOLUMES
	WCHAR tmp[256];
	wsprintf( tmp, L"%04X\n\n%07x branch %hs of %hs", SFXBUILD_OPTIONS2, BUILD_GIT_HEX, BUILD_GIT_BRANCH, BUILD_GIT_URL);
	ustrVersion += tmp;
	MsgBox( ustrVersion, GetLanguageString(STR_TITLE), MB_ICONINFORMATION );
}

HRESULT CSfxExtractEngine::CreateSelfExtractor(LPCWSTR lpwszValue)
{
	DWORD img = static_cast<DWORD>(MAKELONG(
		IMAGE_FILE_EXECUTABLE_IMAGE,
		IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE));
	DWORD osv = 0;
	LPCWSTR lpManifestName = CREATEPROCESS_MANIFEST_RESOURCE_ID;
	if (LPCWSTR lpwszAhead = IsSfxSwitch(lpwszValue, L"dll"))
	{
		img = static_cast<DWORD>(MAKELONG(
			IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL, 0));
		lpManifestName = ISOLATIONAWARE_MANIFEST_RESOURCE_ID;
		lpwszValue = lpwszAhead;
	}
	if (LPCWSTR lpwszAhead = IsSfxSwitch(lpwszValue, L"target"))
	{
		UString TargetVersion;
		SKIP_WHITESPACES_W(lpwszAhead);
		lpwszValue = LoadQuotedString(lpwszAhead, TargetVersion);
		unsigned values[2] = { 0, 0 };
		if (swscanf(TargetVersion, L"%u.%u", &values[0], &values[1]) != 2)
		{
			SfxErrorDialog(0, ERR_CONFIG_CMDLINE, TargetVersion.Ptr());
			return E_INVALIDARG;
		}
		osv = MAKELONG(values[1], values[0]);
	}
	UString OutFileName;
	SKIP_WHITESPACES_W(lpwszValue);
	lpwszValue = LoadQuotedString(lpwszValue, OutFileName);
	COutFile OutFile;
	UInt32 processedSize;
	if (FAILED(CreateImage(m_sfxpath, OutFileName, CREATE_ALWAYS, img, osv)))
	{
		DWORD const status = GetLastError();
		SfxErrorDialog(status, ERR_CREATE_FILE, OutFileName.Ptr());
		return HRESULT_FROM_WIN32(status);
	}
	HANDLE hUpdate = BeginUpdateResourceW(OutFileName, FALSE);
	if (hUpdate == NULL)
	{
		DWORD const status = GetLastError();
		SfxErrorDialog(status, ERR_OPEN_ARCHIVE, OutFileName.Ptr());
		return HRESULT_FROM_WIN32(status);
	}
	BOOL fDiscard = TRUE;
	BOOL fSuccess = TRUE;
	LPCWSTR lpTypeDefaultHtml = RT_HTML;
	// Add resources as specified
	LPCWSTR lpwszAhead;
	while (LPWSTR lpType =
		(lpwszAhead = IsSfxSwitch(lpwszValue, L"config"))	!= NULL ? RT_HTML :
		(lpwszAhead = IsSfxSwitch(lpwszValue, L"adjunct"))	!= NULL ? RT_RCDATA :
		(lpwszAhead = IsSfxSwitch(lpwszValue, L"manifest"))	!= NULL ? RT_MANIFEST :
		(lpwszAhead = IsSfxSwitch(lpwszValue, L"icon"))		!= NULL ? RT_GROUP_ICON :
		NULL)
	{
		UString InFileName;
		SKIP_WHITESPACES_W(lpwszAhead);
		lpwszValue = LoadQuotedString(lpwszAhead, InFileName);
		LPCWSTR lpName = InFileName.Ptr() + InFileName.ReverseFind_PathSepar() + 1;
		if (*lpwszValue == L'#')
		{
			UString Value;
			lpwszValue = LoadQuotedString(lpwszValue, Value);
			lpName = MAKEINTRESOURCE(FindAtomW(Value));
		}
		CInFile InFile;
		UInt64 fileSize;
		if (!InFile.Open(InFileName) || !InFile.GetLength(fileSize))
		{
			DWORD const status = GetLastError();
			SfxErrorDialog(status, ERR_OPEN_ARCHIVE, InFileName.Ptr());
			return HRESULT_FROM_WIN32(status);
		}
		CBuffer<Byte> inBuffer(static_cast<size_t>(fileSize));
		UInt32 inSize = static_cast<UInt32>(fileSize);
		if (inSize != fileSize)
		{
			DWORD const status = ERROR_FILE_TOO_LARGE;
			SfxErrorDialog(0, ERR_OPEN_ARCHIVE, InFileName.Ptr());
			return HRESULT_FROM_WIN32(status);
		}
		if (!InFile.Read(inBuffer, inSize, processedSize) || (processedSize != inSize))
		{
			DWORD const status = GetLastError();
			SfxErrorDialog(status, ERR_OPEN_ARCHIVE, InFileName.Ptr());
			return HRESULT_FROM_WIN32(status);
		}
		CBuffer<Byte> outBuffer;
		LPVOID lpData = inBuffer;
		if (lpType == RT_GROUP_ICON)
		{
			lpName = MAKEINTRESOURCE(1);
			// Limit the size and color depth of the icon as specified
			LONG lMaxWidth = LONG_MAX;
			UINT uMaxDepth = UINT_MAX;
			UINT limits[3];
			if (swscanf(lpwszValue, L"%ux%ux%u", &limits[0], &limits[1], &limits[2]) == 3)
			{
				// Assign to lMaxWidth the value which occurs at least twice
				// Assign to uMaxDepth whatever value remains
				UINT i = limits[1] == limits[2] ? 0 : limits[0] == limits[2] ? 1 : limits[0] == limits[1] ? 2 : 3;
				if (i < _countof(limits))
				{
					uMaxDepth = limits[i];
					lMaxWidth = limits[!i];
				}
				// Consume the extra argument
				UString Value;
				lpwszValue = LoadQuotedString(lpwszValue, Value);
			}
			IconHeader const *const qHeader = reinterpret_cast<IconHeader *>(static_cast<Byte *>(inBuffer));
			IconFileRecord const *qEntry = reinterpret_cast<IconFileRecord const *>(qHeader + 1);
			WORD const n = qHeader->idCount;
			UInt32 outSize = sizeof(IconHeader) + n * sizeof(IconGroupRecord);
			outBuffer.Alloc(outSize);
			IconHeader *const pHeader = reinterpret_cast<IconHeader *>(&outBuffer[0]);
			IconGroupRecord *pEntry = reinterpret_cast<IconGroupRecord *>(pHeader + 1);
			*pHeader = *qHeader;
			pHeader->idCount = 0;
			outSize = sizeof(IconHeader);
			WORD nId = 1;
			for (WORD i = 0; i < n; ++i)
			{
				BITMAPINFOHEADER *pBIH = reinterpret_cast<BITMAPINFOHEADER *>(&inBuffer[qEntry->dwOffset]);
				if (pBIH->biWidth <= lMaxWidth && pBIH->biBitCount <= uMaxDepth)
				{
					++pHeader->idCount;
					outSize += sizeof(IconGroupRecord);
					memcpy(pEntry, qEntry, offsetof(IconGroupRecord, nId));
					pEntry->nId = nId;
					pEntry->wPlanes = pBIH->biPlanes;
					pEntry->wBitCount = pBIH->biBitCount;
					++nId;
					++pEntry;
				}
				++qEntry;
			}
			// Update RT_GROUP_ICON *before* RT_ICON so as to not end up in a mess!
			BOOL fUpdate = UpdateResourceW(hUpdate, lpType, lpName, 0, pHeader, outSize);
			(fUpdate ? fDiscard : fSuccess) = FALSE;
			pEntry = reinterpret_cast<IconGroupRecord *>(pHeader + 1);
			qEntry = reinterpret_cast<IconFileRecord const *>(qHeader + 1);
			nId = 1;
			for (WORD i = 0; i < n; ++i)
			{
				BITMAPINFOHEADER *pBIH = reinterpret_cast<BITMAPINFOHEADER *>(&inBuffer[qEntry->dwOffset]);
				if (pBIH->biWidth <= lMaxWidth && pBIH->biBitCount <= uMaxDepth)
				{
					BOOL fUpdate = UpdateResourceW(hUpdate, RT_ICON, MAKEINTRESOURCE(nId), 0, pBIH, qEntry->dwBytesInRes);
					(fUpdate ? fDiscard : fSuccess) = FALSE;
					++nId;
					++pEntry;
				}
				++qEntry;
			}
			continue; // Update of RT_GROUP_ICON has already happened above!
		}
		else if (lpType == RT_RCDATA)
		{
			// we allocate 105% of original size for output buffer
			UInt64 outSize64 = fileSize / 20 * 21 + (1 << 16);
			size_t outSize = static_cast<UInt32>(outSize64);
			if (outSize != outSize64)
			{
				DWORD const status = ERROR_FILE_TOO_LARGE;
				SfxErrorDialog(status, ERR_OPEN_ARCHIVE, InFileName.Ptr());
				return HRESULT_FROM_WIN32(status);
			}
			outBuffer.Alloc(outSize);
			UInt32 dict = 1 << 24;
			while (UInt32 smaller = dict >> 1)
			{
				if (smaller < inSize)
					break;
				dict = smaller;
			}
			int res = Lzma86_Encode(outBuffer, &outSize, inBuffer, inSize, 5, dict, SZ_FILTER_AUTO);
			if (res != 0)
			{
				HRESULT const hr = CRYPT_E_BAD_ENCODE;
				SfxErrorDialog(static_cast<DWORD>(hr), ERR_OPEN_ARCHIVE, InFileName.Ptr());
				return hr;
			}
			inSize = static_cast<UInt32>(outSize);
			lpData = outBuffer;
		}
		else if (lpType == RT_MANIFEST)
		{
			lpName = lpManifestName;
		}
		else if (lpType == lpTypeDefaultHtml)
		{
			lpName = MAKEINTRESOURCE(1);
			lpTypeDefaultHtml = NULL;
		}
		BOOL fUpdate = UpdateResourceW(hUpdate, lpType, lpName, 0, lpData, inSize);
		(fUpdate ? fDiscard : fSuccess) = FALSE;
	}
	// Patch version resource
	static WCHAR const Version[]			= L"*Version";			// Shortcut to set all version fields at once
	static WCHAR const FileVersion[]		= L"FileVersion";
	static WCHAR const ProductVersion[]		= L"ProductVersion";
	static WCHAR const CompanyName[]		= L"CompanyName";
	static WCHAR const FileDescription[]	= L"FileDescription";
	static WCHAR const InternalName[]		= L"InternalName";
	static WCHAR const LegalCopyright[]		= L"LegalCopyright";
	static WCHAR const OriginalFilename[]	= L"OriginalFilename";
	static WCHAR const PrivateBuild[]		= L"PrivateBuild";
	static WCHAR const SpecialBuild[]		= L"SpecialBuild";
	static WCHAR const ProductName[]		= L"ProductName";
	file_ver_data fvd;
	if (CVersionData const *const pvd = CVersionData::Load())
	{
		if (VS_FIXEDFILEINFO const *const pVffInfo =
			reinterpret_cast<const VS_FIXEDFILEINFO *>(pvd->Data()))
		{
			fvd.m_fxi = *pVffInfo;
			fvd.m_fxi.dwFileFlags = 0;
		}
		if (CVersionData const *const pvdStringFileInfo = pvd->Find(L"StringFileInfo"))
		{
			if (CVersionData const *const pvdLanguage = pvdStringFileInfo->First())
			{
				CVersionData const *const qvdLanguage = pvdStringFileInfo->Next();
				CVersionData const *pvdAssignment = reinterpret_cast<CVersionData const *>(pvdLanguage->Data());
				while (pvdAssignment < qvdLanguage)
				{
					if (_wcsicmp(pvdAssignment->szKey, PrivateBuild) != 0)
					{
						fvd.addTwostr(pvdAssignment->szKey, pvdAssignment->Data());
					}
					pvdAssignment = pvdAssignment->Next();
				}
			}
		}
	}
	bool bPatchVersionResource = false;
	while (LPCWSTR lpKey =
		(lpwszAhead = IsSfxSwitch(lpwszValue, &Version[true]))		!= NULL	? MAKEINTRESOURCE(offsetof(file_ver_data, m_fxi.dwFileVersionLS)) :
		(lpwszAhead = IsSfxSwitch(lpwszValue, L"FileVersionNo"))	!= NULL	? MAKEINTRESOURCE(offsetof(file_ver_data, m_fxi.dwFileVersionMS)) :
		(lpwszAhead = IsSfxSwitch(lpwszValue, L"ProductVersionNo"))	!= NULL	? MAKEINTRESOURCE(offsetof(file_ver_data, m_fxi.dwProductVersionMS)) :
		(lpwszAhead = IsSfxSwitch(lpwszValue, FileVersion))			!= NULL	? FileVersion :
		(lpwszAhead = IsSfxSwitch(lpwszValue, ProductVersion))		!= NULL	? ProductVersion :
		(lpwszAhead = IsSfxSwitch(lpwszValue, CompanyName))			!= NULL	? CompanyName :
		(lpwszAhead = IsSfxSwitch(lpwszValue, FileDescription))		!= NULL	? FileDescription :
		(lpwszAhead = IsSfxSwitch(lpwszValue, InternalName))		!= NULL	? InternalName :
		(lpwszAhead = IsSfxSwitch(lpwszValue, LegalCopyright))		!= NULL	? LegalCopyright :
		(lpwszAhead = IsSfxSwitch(lpwszValue, OriginalFilename))	!= NULL	? OriginalFilename :
		(lpwszAhead = IsSfxSwitch(lpwszValue, PrivateBuild))		!= NULL	? PrivateBuild :
		(lpwszAhead = IsSfxSwitch(lpwszValue, SpecialBuild))		!= NULL	? SpecialBuild :
		(lpwszAhead = IsSfxSwitch(lpwszValue, ProductName))			!= NULL	? ProductName :
		NULL)
	{
		UString Value;
		SKIP_WHITESPACES_W(lpwszAhead);
		lpwszValue = LoadQuotedString(lpwszAhead, Value);
		if (IS_INTRESOURCE(lpKey))
		{
			LPBYTE p = reinterpret_cast<LPBYTE>(&fvd) + reinterpret_cast<WORD>(lpKey);
			memset(p, 0, 8);
			swscanf(Value, L"%hu.%hu.%hu.%hu", p + 2, p + 0, p + 6, p + 4);
			switch (reinterpret_cast<WORD>(lpKey))
			{
			case offsetof(file_ver_data, m_fxi.dwFileVersionLS):
				fvd.m_fxi.dwFileVersionMS = fvd.m_fxi.dwFileVersionLS;
				fvd.m_fxi.dwFileVersionLS = fvd.m_fxi.dwProductVersionMS;
				fvd.m_fxi.dwProductVersionMS = fvd.m_fxi.dwFileVersionMS;
				fvd.m_fxi.dwProductVersionLS = fvd.m_fxi.dwFileVersionLS;
				lpKey = &Version[false];
				break;
			case offsetof(file_ver_data, m_fxi.dwFileVersionMS):
				lpKey = FileVersion;
				break;
			case offsetof(file_ver_data, m_fxi.dwProductVersionMS):
				lpKey = ProductVersion;
				break;
			}
		}
		fvd.addTwostr(lpKey, Value);
		bPatchVersionResource = true;
	}
	if (bPatchVersionResource)
	{
		BYTE buf[8192];
		if (UInt32 cb = fvd.makeVersionResource(buf, sizeof buf))
		{
			BOOL fUpdate = UpdateResourceW(hUpdate, RT_VERSION, MAKEINTRESOURCE(VS_VERSION_INFO), 0, buf, cb);
			(fUpdate ? fDiscard : fSuccess) = FALSE;
		}
	}
	if (!EndUpdateResourceW(hUpdate, fDiscard) || !fSuccess)
	{
		DWORD const status = GetLastError();
		SfxErrorDialog(status, ERR_OPEN_ARCHIVE, OutFileName.Ptr());
		return HRESULT_FROM_WIN32(status);
	}
	// Append archives as specified
	UInt64 newPosition;
	if (!OutFile.Open(OutFileName, OPEN_EXISTING) || !OutFile.SeekToEnd(newPosition))
	{
		DWORD const status = GetLastError();
		SfxErrorDialog(status, ERR_OPEN_ARCHIVE, OutFileName.Ptr());
		return HRESULT_FROM_WIN32(status);
	}
	while (*lpwszValue)
	{
		UString InFileName;
		lpwszValue = LoadQuotedString(lpwszValue, InFileName);
		CInFile InFile;
		if (!InFile.Open(InFileName))
		{
			DWORD const status = GetLastError();
			SfxErrorDialog(status, ERR_OPEN_ARCHIVE, InFileName.Ptr());
			return HRESULT_FROM_WIN32(status);
		}
		do
		{
			BYTE buf[8192];
			UInt32 remainingSize;
			if (!InFile.Read(buf, sizeof buf, remainingSize))
			{
				DWORD const status = GetLastError();
				SfxErrorDialog(status, ERR_OPEN_ARCHIVE, InFileName.Ptr());
				return HRESULT_FROM_WIN32(status);
			}
			if (!OutFile.Write(buf, remainingSize, processedSize) || (processedSize != remainingSize))
			{
				DWORD const status = GetLastError();
				SfxErrorDialog(status, ERR_OPEN_ARCHIVE, OutFileName.Ptr());
				return HRESULT_FROM_WIN32(status);
			}
		} while (processedSize != 0);
	}
	return S_OK;
}

HRESULT CSfxExtractEngine::Run(LPWSTR lpCmdLine)
{
	CrcGenerateTable();

	WCHAR path[MAX_PATH];
	GetModuleFileNameW(m_hRsrcModule, path, _countof(path));

	LPCWSTR str = lpCmdLine;

	if (GetModuleHandle(NULL) == m_hRsrcModule)
	{
		lpCmdLine = GetCommandLineW();
		LoadQuotedString(lpCmdLine, m_sfxpath);
	}
	else
	{
		lpCmdLine = NULL;
		m_sfxpath = path;
	}

	if (IsSfxSwitch(str, L"about"))
	{
		AboutBox();
		return S_OK;
	}

	if (LPCWSTR lpwszValue = IsSfxSwitch(str, L"create"))
	{
		return CreateSelfExtractor(lpwszValue);
	}

#ifdef _DEBUG
	if (LPCWSTR lpwszValue = IsSfxSwitch(str, L"test"))
	{
		SKIP_WHITESPACES_W(lpwszValue);
		m_sfxpath.Empty();
		str = LoadQuotedString(lpwszValue, m_sfxpath);
		m_hRsrcModule = LoadLibraryEx(m_sfxpath, NULL, LOAD_LIBRARY_AS_DATAFILE);
		lpCmdLine = NULL;
	}
#endif

	if (int nPos = m_sfxpath.ReverseFind_PathSepar() + 1)
	{
		// Sanity check the file title
		if (m_sfxpath.Find(L'.', nPos) - nPos >= 64)
		{
			SfxErrorDialog(0, ERR_READ_CONFIG);
			return E_FAIL;
		}
	}

	HRESULT hr = ClearSearchPath();
	if (hr != S_OK && lpCmdLine)
	{
		// Figure out a safe loation for the image
		WCHAR temp[MAX_PATH];
		int const temp_len = GetWindowsDirectoryW(temp, _countof(temp));
		hr = CreateSafeImage(path, temp);
		// S_FALSE means running image already started from safe location
		if (hr != S_FALSE)
		{
			if (FAILED(hr))
			{
				DWORD const status = GetLastError();
				SfxErrorDialog(status, ERR_CREATE_FILE, temp);
				// Don't delete the file if it already existed
				if (hr == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS))
					temp[temp_len] = L'\0';
				hr = HRESULT_FROM_WIN32(status);
			}
			else
			{
				STARTUPINFO si;
				ZeroMemory(&si, sizeof si);
				si.cb = sizeof si;
				PROCESS_INFORMATION pi;
				if (CreateProcess(temp, lpCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
				{
					CloseHandle(pi.hThread);
					WaitForSingleObject(pi.hProcess, INFINITE);
					C_ASSERT(sizeof(HRESULT) == sizeof(DWORD));
					GetExitCodeProcess(pi.hProcess, reinterpret_cast<DWORD *>(&hr));
					CloseHandle(pi.hProcess);
				}
				else
				{
					DWORD const status = GetLastError();
					SfxErrorDialog(status, ERR_EXECUTE, temp);
					hr = HRESULT_FROM_WIN32(status);
				}
			}
			if (temp[temp_len] != '\0')
				DeleteFile(temp);
			return hr;
		}
	}

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_spTaskbarList));
#ifdef _SFX_USE_EXTRACT_MASK
	SetIndices(NULL, 0);
#endif // _SFX_USE_EXTRACT_MASK
	CMyComPtr<ITypeLib> spTypeLib;
	if (FAILED(hr = LoadTypeLib(path, &spTypeLib)))
		return hr;
	if (FAILED(hr = spTypeLib->GetTypeInfoOfGuid(__uuidof(I7zSfxHtmHost), &m_spTypeInfo)))
		return hr;
	if (!m_archive.Init(m_sfxpath))
	{
		DWORD status = GetLastError();
		SfxErrorDialog(status, ERR_OPEN_ARCHIVE, m_sfxpath.Ptr());
		return HRESULT_FROM_WIN32(status);
	}
	if (!m_archive.Open(NULL))
	{
		SfxErrorDialog(0, ERR_NON7Z_ARCHIVE);
		return E_FAIL;
	}
	return Extract();
}
