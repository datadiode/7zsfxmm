/*---------------------------------------------------------------------------*/
/* File:        ExtractEngine.h                                              */
/* Created:     Wed, 05 Oct 2005 17:35:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 07 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#ifndef _EXTRACTENGINE_H_INCLUDED_
#define _EXTRACTENGINE_H_INCLUDED_

#include <mshtmlc.h>
#include <mshtmhst.h>
#include "../../midl/7zSfxHtm_h.h"

class CSfxExtractEngine
	: public IArchiveExtractCallback
#ifdef SFX_CRYPTO
	, public ICryptoGetTextPassword
#endif // SFX_CRYPTO
	, public I7zSfxHtmHost
	, public IElementBehavior
	, public IElementBehaviorFactory
	, public CMyUnknownImp
{
public:
	MY_UNKNOWN_IMP5(ICryptoGetTextPassword, IDispatch, I7zSfxHtmHost, IElementBehavior, IElementBehaviorFactory)

	// IProgress
	STDMETHOD(SetTotal)(UInt64 total);
	STDMETHOD(SetCompleted)(const UInt64 *completeValue);

	// IExtractCallback
	STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode);
	STDMETHOD(PrepareOperation)(Int32 askExtractMode);
	STDMETHOD(SetOperationResult)(Int32 resultEOperationResult);

	STDMETHOD(SayInternalError)(Int32 errc, DWORD dwLastError);

#ifdef SFX_CRYPTO
	STDMETHOD(CryptoGetTextPassword)(BSTR *password);
#endif // SFX_CRYPTO

	// IDispatch
	STDMETHOD(GetTypeInfoCount)(UINT *pctinfo)
	{
		*pctinfo = 1;
		return S_OK;
	}
	STDMETHOD(GetTypeInfo)(UINT /*iTInfo*/, LCID, ITypeInfo **ppTInfo)
	{
		(*ppTInfo = m_spTypeInfo)->AddRef();
		return S_OK;
	}
	STDMETHOD(GetIDsOfNames)(REFIID, LPOLESTR *rgNames, UINT cNames, LCID, DISPID *rgDispId)
	{
		return ::DispGetIDsOfNames(m_spTypeInfo, rgNames, cNames, rgDispId);
	}
	STDMETHOD(Invoke)(DISPID dispid, REFIID, LCID, WORD wFlags,
		DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
	{
		HRESULT hr = DISP_E_EXCEPTION;
		__try
		{
			hr = DispInvoke(static_cast<I7zSfxHtmHost *>(this), m_spTypeInfo,
				dispid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			pExcepInfo->wCode = 1001;
			WCHAR msg[256];
			wsprintfW(msg, L"An exception of type 0x%08lX has occurred", GetExceptionCode());
			SysReAllocString(&pExcepInfo->bstrDescription, msg);
			m_spTypeInfo->GetDocumentation(-1, &pExcepInfo->bstrSource, NULL, NULL, NULL);
		}
		return hr;
	}
	// IElementBehavior
	STDMETHOD(Init)(IElementBehaviorSite *)
	{
		return S_OK;
	}
	STDMETHOD(Notify)(LONG lEvent, VARIANT *)
	{
		switch (lEvent)
		{
		case BEHAVIOREVENT_DOCUMENTREADY:
			if (m_spOleWindow)
			{
				m_spOleWindow->GetWindow(&m_hExtractWindow);
				while (HWND hWnd = GetParent(m_hExtractWindow))
					m_hExtractWindow = hWnd;
				HANDLE hBigIcon = LoadImage(
					m_hRsrcModule, MAKEINTRESOURCE(1),
					IMAGE_ICON, GetSystemMetrics(SM_CXICON),
					GetSystemMetrics(SM_CYICON), LR_SHARED);
				HANDLE hSmallIcon = LoadImage(
					m_hRsrcModule, MAKEINTRESOURCE(1),
					IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
					GetSystemMetrics(SM_CYSMICON), LR_SHARED);
				if (hSmallIcon == NULL)
					hSmallIcon = hBigIcon;
				SendMessage(m_hExtractWindow, WM_SETICON, ICON_BIG, (LPARAM)hBigIcon);
				SendMessage(m_hExtractWindow, WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon);
			}
			if (m_spDocument2)
			{
				m_spDocument2->get_title(&m_title);
				CMyComPtr<IHTMLElement> spBody;
				m_spDocument2->get_body(&spBody);
				CMyComPtr<IHTMLElement2> spBody2;
				SafeInvoke(spBody)->QueryInterface(&spBody2);
				CMyComPtr<IHTMLStyle> spStyle;
				SafeInvoke(spBody2)->get_runtimeStyle(&spStyle);
				CMyComPtr<IHTMLStyle3> spStyle3;
				SafeInvoke(spStyle)->QueryInterface(&spStyle3);
				VARIANT var;
				InitVariantFromDouble(m_pctzoom / 100.0, &var);
				SafeInvoke(spStyle3)->put_zoom(var);
			}
			break;
		}
		return S_OK;
	}
	STDMETHOD(Detach)()
	{
		return S_OK;
	}
	// IElementBehaviorFactory
	STDMETHOD(FindBehavior)(BSTR, BSTR, IElementBehaviorSite *, IElementBehavior **);
	// I7zSfxHtmHost
	STDMETHOD(get_Path)(BSTR *pbsPath);
	STDMETHOD(get_Hwnd)(LONG *plHwnd);
	STDMETHOD(get_ExitCode)(LONG *plExitCode);
	STDMETHOD(get_Progress)(DOUBLE *pdProgress);
	STDMETHOD(ExtractPayload)(BSTR bsPath);
	STDMETHOD(ExtractAdjunct)(BSTR bsPath, BSTR bsFilter, BSTR bsType);
	STDMETHOD(put_OverwriteMode)(LONG lOverwriteMode);
	STDMETHOD(get_OverwriteMode)(LONG *plOverwriteMode);
	STDMETHOD(put_WindowState)(LONG lWindowState);
	STDMETHOD(get_VersionString)(BSTR bsKey, BSTR *pbsValue);
	STDMETHOD(put_Password)(BSTR bsPassword);
	STDMETHOD(FindIcon)(BSTR bsPath, BSTR bsName, BSTR *pbsLink);

	CSfxExtractEngine(HINSTANCE hRsrcModule)
		: m_pVersionStrings(CVersionData::Load(hRsrcModule)->Find(L"StringFileInfo")->First())
		, m_outFileStreamSpec(NULL)
		, m_hRsrcModule(hRsrcModule)
		, m_hExtractWindow(NULL)
		, m_hExtractThread(NULL)
		, m_hUpdate(NULL)
		, m_fDiscard(FALSE)
		, m_fSuccess(FALSE)
		, m_iconid(0)
		, m_pctzoom(GetUIZoomFactor())
		, m_ErrorCode(NArchive::NExtract::NOperationResult::kOK)
		, m_total(0)
		, m_completed(0)
		, m_choice(0)
		, m_overwriteMode(OVERWRITE_MODE_ALL)
	{
	}
	void SetTaskbarState( TBPFLAG tbpFlags )
	{
		if( m_spTaskbarList && m_hExtractWindow )
			m_spTaskbarList->SetProgressState( m_hExtractWindow, tbpFlags );
	}
	int MsgBoxHandler(MSGBOXPARAMS *);
	int MsgBox( LPCWSTR prompt, LPCWSTR title, DWORD buttons );
	void ShowSfxErrorDialog( LPCWSTR prompt );
#ifdef _SFX_USE_WARNINGS
	int ShowSfxWarningDialog( LPCWSTR prompt );
#endif // _SFX_USE_WARNINGS
	void SfxErrorDialog( DWORD dwLastError, UINT idFormat, ... );
#ifdef _SFX_USE_EXTRACT_MASK
	void SetIndices( UInt32 * indices, UInt32 num ) { m_indices = indices; m_numIndices = num; }
#endif // _SFX_USE_EXTRACT_MASK

private:
	CVersionData const *const	m_pVersionStrings;

	CMyComPtr<ITypeInfo>		m_spTypeInfo;
	CMyComPtr<IHTMLElement>		m_spElement;
	CMyComPtr<IHTMLElement3>	m_spElement3;
	CMyComPtr<IOleWindow>		m_spOleWindow;
	CMyComPtr<IHTMLDocument2>	m_spDocument2;
	CMyComPtr<IHTMLWindow2>		m_spWindow2;
	CMyComPtr<IHTMLDialog>		m_spDialog;
	CMyComPtr<ITaskbarList3>	m_spTaskbarList;

	CMyComBSTR				m_title;
	CMyComBSTR				m_password;
	UString					m_sfxpath;
	UString					m_features;
	UString					m_path;
	UString					m_filter;
	UString					m_rctype;
	WORD					m_iconid;
	int						m_pctzoom;
	Int32					m_extractMode;
	CSfxArchive				m_archive;
	COutFileStream *		m_outFileStreamSpec;
	CMyComPtr<ISequentialOutStream> m_outFileStream;
	UString					m_diskFilePath;
	HINSTANCE				m_hRsrcModule;
	HWND					m_hExtractWindow;
	HANDLE					m_hExtractThread;
	HANDLE					m_hUpdate;
	BOOL					m_fDiscard;
	BOOL					m_fSuccess;
	Int32					m_ErrorCode;
#ifdef _SFX_USE_EXTRACT_MASK
	UInt32 *				m_indices;
	UInt32					m_numIndices;
#endif // _SFX_USE_EXTRACT_MASK
	UInt64					m_total;
	UInt64					m_completed;
	int						m_choice;
	long					m_overwriteMode;

	struct IconHeader
	{
		WORD idReserved;
		WORD idType;
		WORD idCount;
	};

	struct IconFileRecord
	{
		BYTE bWidth;
		BYTE bHeight;
		BYTE bColorCount;
		BYTE bReserved;
		WORD wPlanes;
		WORD wBitCount;
		DWORD dwBytesInRes;
		DWORD dwOffset;
	};

	struct IconGroupRecord
	{
		BYTE bWidth;
		BYTE bHeight;
		BYTE bColorCount;
		BYTE bReserved;
		WORD wPlanes;
		WORD wBitCount;
		BYTE dwBytesInRes[sizeof(DWORD)];
		WORD nId;
	};

	struct CProcessedFileInfo
	{
		FILETIME	UTCLastWriteTime;
		BOOL		IsDirectory;
		UInt32		Attributes;
	} m_processedFileInfo;

private:
	int DeleteUseOverwriteFlags(LPCWSTR);
	int GetOverwriteMode(LPCWSTR, FILETIME *);
	HRESULT HtmlExtractDialog();
	HRESULT ExtractWorker();
	static DWORD WINAPI ExtractThread(LPVOID);
	HRESULT Extract();
	static BOOL CALLBACK SfxEnumResNameProc(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName, LONG_PTR lParam);
	BOOL ExtractResources(HMODULE hModule, LPWSTR lpType);
	void AboutBox();
	HRESULT CreateSelfExtractor(LPCWSTR lpwszValue);
public:
	HRESULT Run(LPWSTR);
};

#endif // _EXTRACTENGINE_H_INCLUDED_
