/*
	Copyright (C) 2018 Jochen Neubeck

	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
#include <windows.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>
#include <urlmon.h>
#include <process.h>
#include <mshtmlc.h>
#include <mshtmhst.h>
#include "../common/ImageUtil.h"
#include "../../midl/HTDialog_h.h"
#include "../../midl/FETCH_HEAD.h"

typedef bool Assert;

#include "BasePtr.h"
#include "AutoPtr.h"

// Substitute the "exceptionally unsafe" lstr* functions
#pragma intrinsic(wcslen)
#define lstrlenW (int)wcslen
#define lstrcpynW StrCpyNW
#define lstrcmpW StrCmpW
#define lstrcmpiW StrCmpIW

#define SafeInvoke(p) if (p) p

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

static LPWSTR EatPrefix(LPWSTR text, LPCWSTR prefix)
{
	int len = lstrlenW(prefix);
	if (StrIsIntlEqualW(FALSE, text, prefix, len))
		text += len;
	else
		text = NULL;
	return text;
}

class ScaleFeatures
{
	WCHAR buf[1024];
	int len;
	void append(LPCWSTR q, int n)
	{
		int m = _countof(buf) - 1 - len;
		if (n > m)
			n = m;
		lstrcpynW(buf + len, q, n + 1);
		len += n;
	}
	static LPCWSTR IsKeyword(LPCWSTR features, LPCWSTR p, LPCWSTR keyword)
	{
		LPCWSTR q = StrRStrIW(features, p, keyword);
		return q && keyword[p - q] == L'\0' &&
			(q == features || StrChrW(L"\t ;", q[-1])) ? q : NULL;
	}
	static bool IsYesKeyword(LPCWSTR p, LPCWSTR q)
	{
		return StrRStrIW(p, q, L"yes") || StrRStrIW(p, q, L"1") || StrRStrIW(p, q, L"on");
	}
	static LPCWSTR IsMeasurementUnit(LPCWSTR p, LPCWSTR q)
	{
		static WCHAR const unit[][3] = { L"cm", L"mm", L"in", L"pt", L"pc", L"em", L"ex", L"px" };
		for (int i = 0; i < _countof(unit); ++i)
			if (LPCWSTR r = StrRStrIW(p, q, unit[i]))
				return r;
		return NULL;
	}

public:
	int pctzoom;
	HWND owner;
	bool verify;
	bool visible;

	ScaleFeatures()
		: len(0)
		, pctzoom(100)
		, owner(NULL)
		, verify(true)
		, visible(true)
	{
		buf[0] = L'\0';
	}
	void Parse(LPCWSTR features)
	{
		if (features)
		{
			// Be sure to have a semicolon in between feature sequences
			if (len > 0 && buf[len - 1] != L':')
				append(L";", 1);
			while (LPCWSTR const colon = StrChrW(features, L':'))
			{
				LPCWSTR p = colon + 1;
				LPCWSTR q = StrChrW(p += StrSpnW(p, L" \t\r\n"), L';');
				if (q) ++q;
				if (LPCWSTR r = IsKeyword(features, colon, L"zoom"))
				{
					append(features, static_cast<int>(r - features));
					pctzoom = MulDiv(pctzoom, StrToIntW(p), 100);
					p = q;
				}
				else if (LPCWSTR r = IsKeyword(features, colon, L"taskicon"))
				{
					append(features, static_cast<int>(r - features));
					if (!IsYesKeyword(p, q) && owner == NULL)
						owner = CreateHiddenOwner();
					p = q;
				}
				else if (LPCWSTR r = IsKeyword(features, colon, L"verify"))
				{
					append(features, static_cast<int>(r - features));
					verify = IsYesKeyword(p, q);
					p = q;
				}
				else if (LPCWSTR r = IsKeyword(features, colon, L"visible"))
				{
					append(features, static_cast<int>(r - features));
					visible = IsYesKeyword(p, q);
					p = q;
				}
				else
				{
					append(features, static_cast<int>(p - features));
					if (int val = StrToIntW(p))
					{
						if (LPCWSTR r = IsMeasurementUnit(p, q))
						{
							WCHAR buf[40];
							append(buf, wsprintfW(buf, L"%de-2", val * pctzoom));
							p = r;
						}
					}
				}
				features = p ? p : L"";
			}
			append(features, lstrlenW(features));
			buf[len] = L'\0';
		}
	}
	operator LPWSTR() { return buf; }
};

class FindResIndex
{
	int index;
	BOOL found;
	LPWSTR const name;
	static BOOL CALLBACK EnumResNameProc(HMODULE, LPCWSTR, LPWSTR lpName, LONG_PTR lParam)
	{
		FindResIndex *const p = reinterpret_cast<FindResIndex *>(lParam);
		if (IS_INTRESOURCE(lpName) || IS_INTRESOURCE(p->name) ?
			lpName == p->name : lstrcmpiW(lpName, p->name) == 0)
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

class CHTDialogHost
	: public IHTDialogHost
	, public IElementBehavior
	, public IElementBehaviorFactory
{
	// SHELL32.DLL imports
	struct SHELL32 {
		DllHandle DLL;
		DllImport<HRESULT (WINAPI *)(HWND, REFIID, void **)> SHGetPropertyStoreForWindow;
		SHELL32() {
			DLL.h = DllHandle::Load(L"SHELL32");
			SHGetPropertyStoreForWindow.p = DLL("SHGetPropertyStoreForWindow");
		}
	} const SHELL32;
	// MSHTML.DLL imports
	struct MSHTML {
		DllHandle DLL;
		DllImport<SHOWHTMLDIALOGEXFN *> ShowHTMLDialogEx;
		MSHTML() {
			DLL.h = DllHandle::Load(L"MSHTML");
			ShowHTMLDialogEx.p = DLL("ShowHTMLDialogEx");
		}
	} const MSHTML;
public:
	CHTDialogHost()
		: m_cmdline(GetCommandLineW())
		, m_hModule(GetModuleHandle(NULL))
		, m_hWnd(NULL)
		, m_uCustomIcons(0)
	{
		SecureZeroMemory(&m_icon, sizeof m_icon);
	}
	HRESULT Modal(DWORD flags)
	{
		VARIANT in, out;
		InitVariantFromDispatch(this, &in);
		VariantInit(&out);
		if (!m_scaleFeatures.verify)
			flags &= ~HTMLDLG_VERIFY;
		if (!m_scaleFeatures.visible)
			flags |= HTMLDLG_NOUI;
		HRESULT hr;
		if (FAILED(hr = (*MSHTML.ShowHTMLDialogEx)(m_scaleFeatures.owner, m_spMoniker, flags, &in, m_scaleFeatures, &out)))
			return hr;
		if (FAILED(hr = VariantChangeType(&out, &out, 0, VT_I4)))
			return hr;
		hr = V_I4(&out);
		flags &= ~HTMLDLG_NOUI;
		VariantClear(&in);
		VariantClear(&out);
		return hr;
	}
	HRESULT Run()
	{
		WCHAR path[MAX_PATH];
		GetModuleFileNameW(m_hModule, path, _countof(path));
		AutoReleasePtr<ITypeLib> spTypeLib;
		HRESULT hr;
		if (FAILED(hr = LoadTypeLib(path, &spTypeLib)))
			return hr;
		if (FAILED(hr = spTypeLib->GetTypeInfoOfGuid(__uuidof(IHTDialogHost), &m_spTypeInfo)))
			return hr;

		int argc = 0;
		LPWSTR argv[2] = { NULL, NULL };
		AutoBSTR cmdline = SysAllocString(m_cmdline);
		LPWSTR p = cmdline;
		do
		{
			LPWSTR q = PathGetArgsW(p);
			LPWSTR t = p + 1;
			LPWSTR r;
			PathRemoveArgsW(p);
			if (*p != L'/')
				PathUnquoteSpacesW(argv[argc++] = p);
			else if ((r = EatPrefix(t, L"features:")) != NULL)
			{
				PathUnquoteSpacesW(r);
				m_scaleFeatures.Parse(r);
			}
			else
				break;
			p = q + StrSpnW(q, L" \t\r\n");
		} while (*p != L'\0' && argc < _countof(argv));

		if (argv[1] == NULL)
			argv[1] = L"#1";

		WCHAR url[1024];
		if (PathIsRelativeW(argv[1]) && FindResourceW(m_hModule, argv[1], RT_HTML))
		{
			wsprintfW(url, L"res://%s/%s", PathFindFileNameW(path), argv[1]);
			argv[1] = url;
		}
		if (FAILED(hr = CreateURLMoniker(NULL, argv[1], &m_spMoniker)))
			return hr;

		return Modal(HTMLDLG_MODAL | HTMLDLG_VERIFY | HTMLDLG_NOUI);
	}
	HICON LoadShellIcon(LPWSTR path, UINT flags)
	{
		SecureZeroMemory(&m_icon, sizeof m_icon);
		if (*path == L'.')
		{
			SHGetFileInfoW(path, FILE_ATTRIBUTE_NORMAL, &m_icon, sizeof m_icon, flags);
		}
		else
		{
			int cx = GetSystemMetrics(flags & SHGFI_SMALLICON ? SM_CXSMICON : SM_CXICON);
			int cy = GetSystemMetrics(flags & SHGFI_SMALLICON ? SM_CYSMICON : SM_CYICON);
			m_icon.hIcon = reinterpret_cast<HICON>(LoadImage(m_hModule, path, IMAGE_ICON, cx, cy, 0));
			m_icon.iIcon = FindResIndex(m_hModule, RT_GROUP_ICON, path);
			GetModuleFileNameW(m_hModule, m_icon.szDisplayName, _countof(m_icon.szDisplayName));
		}
		return m_icon.hIcon;
	}
	// IUnknown
	STDMETHOD(QueryInterface)(REFIID iid, void **ppv)
	{
		static const QITAB rgqit[] = 
		{   
			QITABENT(CHTDialogHost, IDispatch),
			QITABENT(CHTDialogHost, IHTDialogHost),
			QITABENT(CHTDialogHost, IElementBehavior),
			QITABENT(CHTDialogHost, IElementBehaviorFactory),
			{ 0 }
		};
		return QISearch(this, rgqit, iid, ppv);
	}
	STDMETHOD_(ULONG, AddRef)()
	{
		return 1;
	}
	STDMETHOD_(ULONG, Release)()
	{
		return 1;
	}
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
		return DispGetIDsOfNames(m_spTypeInfo, rgNames, cNames, rgDispId);
	}
	STDMETHOD(Invoke)(DISPID dispid, REFIID, LCID, WORD wFlags,
		DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
	{
		HRESULT /*hr = DISP_E_EXCEPTION;
		__try
		{*/
			hr = DispInvoke(static_cast<IHTDialogHost *>(this), m_spTypeInfo,
				dispid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
		/*}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			pExcepInfo->wCode = 1001;
			WCHAR msg[256];
			wsprintfW(msg, L"An exception of type 0x%08lX has occurred", GetExceptionCode());
			SysReAllocString(&pExcepInfo->bstrDescription, msg);
			m_spTypeInfo->GetDocumentation(-1, &pExcepInfo->bstrSource, NULL, NULL, NULL);
		}*/
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
			m_hWnd = NULL;
			m_spPropertyStore.Release();
			if (m_spOleWindow)
			{
				m_spOleWindow->GetWindow(&m_hWnd);
				while (m_hWnd && (GetWindowLong(m_hWnd, GWL_STYLE) & WS_CHILD))
					m_hWnd = GetParent(m_hWnd);
				if (*SHELL32.SHGetPropertyStoreForWindow)
					(*SHELL32.SHGetPropertyStoreForWindow)(m_hWnd, IID_PPV_ARGS(&m_spPropertyStore));
			}
			if (m_spDocument2)
			{
				m_spDocument2->get_title(&m_title);
				AutoReleasePtr<IHTMLElement> spBody;
				m_spDocument2->get_body(&spBody);
				AutoReleasePtr<IHTMLElement2> spBody2;
				SafeInvoke(spBody)->QueryInterface(&spBody2);
				AutoReleasePtr<IHTMLStyle> spStyle;
				SafeInvoke(spBody2)->get_runtimeStyle(&spStyle);
				AutoReleasePtr<IHTMLStyle3> spStyle3;
				SafeInvoke(spStyle)->QueryInterface(&spStyle3);
				VARIANT var;
				WCHAR buf[40];
				wsprintfW(buf, L"%d%%", m_scaleFeatures.pctzoom);
				InitVariantFromString(buf, &var);
				spStyle3->put_zoom(var);
				VariantClear(&var);
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
	STDMETHOD(FindBehavior)(BSTR bstrBehavior, BSTR bstrBehaviorUrl, IElementBehaviorSite *pSite, IElementBehavior **ppBehavior)
	{
		LPCWSTR pwszBehaviorParams = StrChrW(bstrBehavior, L'?');
		if (pwszBehaviorParams == NULL)
			pwszBehaviorParams = bstrBehavior + lstrlenW(bstrBehavior);
		if (EatPrefix(bstrBehaviorUrl, L"#HTDialog#"))
		{
			if (EatPrefix(bstrBehavior, L"Host") == pwszBehaviorParams)
			{
				SafeInvoke(pSite)->GetElement(&m_spElement);
				AutoReleasePtr<IDispatch> spDispatch;
				SafeInvoke(m_spElement)->get_document(&spDispatch);
				SafeInvoke(spDispatch)->QueryInterface(&m_spOleWindow);
				SafeInvoke(spDispatch)->QueryInterface(&m_spDocument2);
				SafeInvoke(m_spDocument2)->get_parentWindow(&m_spWindow2);
				// If caller wants different features, recurse
				if (*pwszBehaviorParams++ &&
					lstrcmpW(m_features, pwszBehaviorParams) != 0)
				{
					// Release interfaces acquired from preliminary dialog instance
					m_spWindow2.Release();
					m_spDocument2.Release();
					m_spOleWindow.Release();
					m_spElement.Release();
					SysReAllocString(&m_features, pwszBehaviorParams);
					m_scaleFeatures.Parse(m_features);
					ExitProcess(Modal(HTMLDLG_MODAL | HTMLDLG_VERIFY));
				}
				return QueryInterface(IID_IElementBehavior, (void **)ppBehavior);
			}
		}
		return E_INVALIDARG;
	}
	// IHTDialogHost
	STDMETHOD(get_Path)(BSTR *pbsPath)
	{
		WCHAR path[MAX_PATH];
		GetModuleFileNameW(m_hModule, path, _countof(path));
		SysReAllocString(pbsPath, path);
		return S_OK;
	}
	STDMETHOD(get_Hwnd)(LONG *plHwnd)
	{
		*plHwnd = reinterpret_cast<LONG>(m_hWnd);
		return S_OK;
	}
	STDMETHOD(put_Icon)(BSTR bsPath)
	{
		if (HICON hIcon = LoadShellIcon(bsPath, SHGFI_ICONLOCATION | SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_LARGEICON))
		{
			hIcon = (HICON)SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
			if (hIcon && (m_uCustomIcons & (1 << SHGFI_LARGEICON)))
				DestroyIcon(hIcon);
			m_uCustomIcons |= 1 << SHGFI_LARGEICON;
		}
		if (HICON hIcon = LoadShellIcon(bsPath, SHGFI_ICONLOCATION | SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON))
		{
			hIcon = (HICON)SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			if (hIcon && (m_uCustomIcons & (1 << SHGFI_SMALLICON)))
				DestroyIcon(hIcon);
			m_uCustomIcons |= 1 << SHGFI_SMALLICON;
		}
		return S_OK;
	}
	STDMETHOD(get_Icon)(BSTR *pbsPath)
	{
		WCHAR path[MAX_PATH + 40];
		wsprintfW(path, L"%s,%d", m_icon.szDisplayName, m_icon.iIcon);
		SysReAllocString(pbsPath, path);
		return S_OK;
	}
	STDMETHOD(put_AppUserModel)(int pid, REFVARIANT var)
	{
		PROPERTYKEY const key =
		{
			{ 0x9F4C2855, 0x9F79, 0x4B39, { 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3 } }, pid
		};
		SafeInvoke(m_spPropertyStore)->SetValue(key, reinterpret_cast<REFPROPVARIANT>(var));
		return S_OK;
	}
	STDMETHOD(get_CmdLine)(BSTR *pbsCmdLine)
	{
		SysReAllocString(pbsCmdLine, m_cmdline);
		return S_OK;
	}
private:
	LPCWSTR const					m_cmdline;
	HMODULE const					m_hModule;
	HWND							m_hWnd;
	HWND							m_owner;
	UINT							m_uCustomIcons;
	ScaleFeatures					m_scaleFeatures;
	SHFILEINFOW						m_icon;
	AutoBSTR						m_title;
	AutoBSTR						m_features;
	AutoReleasePtr<ITypeInfo>		m_spTypeInfo;
	AutoReleasePtr<IMoniker>		m_spMoniker;
	AutoReleasePtr<IHTMLElement>	m_spElement;
	AutoReleasePtr<IOleWindow>		m_spOleWindow;
	AutoReleasePtr<IHTMLDocument2>	m_spDocument2;
	AutoReleasePtr<IHTMLWindow2>	m_spWindow2;
	AutoReleasePtr<IPropertyStore>	m_spPropertyStore;
};

int WINAPI WinMainCRTStartup()
{
	__security_init_cookie();
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		hr = CHTDialogHost().Run();
		CoUninitialize();
	}
	ExitProcess(hr);
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
