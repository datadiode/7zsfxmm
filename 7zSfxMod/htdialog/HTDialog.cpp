/*
	Copyright (C) 2018 Jochen Neubeck

	MIT License: http://www.opensource.org/licenses/mit-license.php
*/
#include <windows.h>
#include <propvarutil.h>
#include <urlmon.h>
#include <process.h>
#include <mshtmlc.h>
#include <mshtmhst.h>
#include "../common/ImageUtil.h"
#include "../../midl/HTDialog_h.h"

typedef bool Assert;

#include "BasePtr.h"
#include "AutoPtr.h"

#define SafeInvoke(p) if (p) p

static int GetUIZoomFactor()
{
	int zoom = 100;
	if (HDC hdc = GetDC(NULL))
	{
		zoom = MulDiv(GetDeviceCaps(hdc, LOGPIXELSX), 100, 96);
		ReleaseDC(NULL, hdc);
	}
	return zoom;
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
public:
	ScaleFeatures(LPCWSTR features, int by) : len(0)
	{
		if (features)
		{
			while (LPCWSTR s = StrChrW(features, L':'))
			{
				append(features, ++s - features);
				int val = StrToIntW(s);
				LPWSTR t = StrChrW(s, L';');
				LPWSTR p = StrRChrW(s, t, L'p');
				LPWSTR x = StrRChrW(s, t, L'x');
				if (x - p == 1)
				{
					val = MulDiv(val, by, 100);
					WCHAR buf[12];
					append(buf, wsprintfW(buf, L"%d", val));
					features = p;
				}
				else
				{
					features = s;
				}
			}
			append(features, lstrlenW(features));
		}
		buf[len] = L'\0';
	}
	LPWSTR Get() { return buf; }
};

class CHTDialogHost
	: public IHTDialogHost
	, public IElementBehavior
	, public IElementBehaviorFactory
{
public:
	CHTDialogHost()
		: m_hModule(NULL)
		, m_hWnd(NULL)
		, m_hIcon(NULL)
		, m_pctzoom(0)
	{
	}

	HRESULT Run(LPWSTR p)
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

		struct MSHTML {
			DllHandle DLL;
			DllImport<SHOWHTMLDIALOGEXFN *> ShowHTMLDialogEx;
		} const MSHTML = {
			DllHandle::Load(L"MSHTML"),
			MSHTML.DLL("ShowHTMLDialogEx"),
		};

		if (*SHCORE.SetProcessDpiAwareness)
			(*SHCORE.SetProcessDpiAwareness)(SHCORE.ProcessSystemDpiAware);

		m_pctzoom = GetUIZoomFactor();

		WCHAR path[MAX_PATH];
		GetModuleFileNameW(m_hModule, path, _countof(path));
		AutoReleasePtr<ITypeLib> spTypeLib;
		if (SUCCEEDED(LoadTypeLib(path, &spTypeLib)))
			spTypeLib->GetTypeInfoOfGuid(__uuidof(IHTDialogHost), &m_spTypeInfo);

		int argc = 0;
		LPWSTR argv[2] = { NULL, NULL };
		LPWSTR features = NULL;
		DWORD flags = HTMLDLG_MODAL | HTMLDLG_VERIFY;
		do
		{
			LPWSTR q = PathGetArgsW(p);
			LPWSTR t = p + 1;
			LPWSTR r;
			PathRemoveArgsW(p);
			if (*p != L'/')
				PathUnquoteSpacesW(argv[argc++] = p);
			else if (lstrcmpi(t, L"hidden") == 0)
				flags |= HTMLDLG_NOUI;
			else if ((r = EatPrefix(t, L"features:")) != NULL)
				PathUnquoteSpacesW(features = r);
			else if ((r = EatPrefix(t, L"zoom:")) != NULL)
				m_pctzoom = MulDiv(m_pctzoom, StrToInt(r), 100);
			else
				break;
			p = q + StrSpnW(q, L" \t\r\n");
		} while (*p != TEXT('\0') && argc < _countof(argv));

		if (argv[1] == NULL)
			argv[1] = L"#1";

		WCHAR url[1024];
		if (PathIsRelativeW(argv[1]) && FindResourceW(m_hModule, argv[1], RT_HTML))
		{
			wsprintfW(url, L"res://%s//%s", PathFindFileNameW(path), argv[1]);
			lstrcpyW(path, argv[1]);
			if (*path != L'#')
				PathRenameExtensionW(path, L".ICO");
			m_hIcon = LoadIconW(m_hModule, path);
			argv[1] = url;
		}

		IMoniker *moniker = NULL;
		HRESULT hr = CreateURLMoniker(NULL, argv[1], &moniker);
		if (SUCCEEDED(hr))
		{
			VARIANT in, out;
			InitVariantFromDispatch(this, &in);
			VariantInit(&out);
			DWORD phlags = HTMLDLG_MODAL | HTMLDLG_VERIFY | HTMLDLG_NOUI;
			do 
			{
				if (FAILED(hr = (*MSHTML.ShowHTMLDialogEx)(NULL, moniker, phlags, &in, ScaleFeatures(m_features, m_pctzoom).Get(), &out)))
					break;
				if (FAILED(hr = VariantChangeType(&out, &out, 0, VT_I4)))
					break;
				hr = V_I4(&out);
				phlags = flags;
			} while (SysStringLen(m_features) != 0);
			VariantClear(&in);
			VariantClear(&out);
			moniker->Release();
		}
		return hr;
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
			if (m_spOleWindow)
			{
				m_spOleWindow->GetWindow(&m_hWnd);
				while (HWND hWnd = GetParent(m_hWnd))
					m_hWnd = hWnd;
				SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)m_hIcon);
				SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)m_hIcon);
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
				WCHAR buf[12];
				wsprintfW(buf, L"%d%%", m_pctzoom);
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
				pSite->GetElement(&m_spElement);
				AutoReleasePtr<IDispatch> spDispatch;
				m_spElement->get_document(&spDispatch);
				spDispatch->QueryInterface(&m_spOleWindow);
				spDispatch->QueryInterface(&m_spDocument2);
				m_spDocument2->get_parentWindow(&m_spWindow2);
				if (*pwszBehaviorParams++)
				{
					if (lstrcmpW(m_features, pwszBehaviorParams) != 0)
					{
						SysReAllocString(&m_features, pwszBehaviorParams);
						m_spWindow2->close();
						m_spWindow2.Release();
						m_spDocument2.Release();
						m_spOleWindow.Release();
						m_spElement.Release();
						return S_OK;
					}
					SysReAllocString(&m_features, NULL);
				}
				return QueryInterface(IID_IElementBehavior, (void **)ppBehavior);
			}
		}
		return E_INVALIDARG;
	}
	// IHTDialogHost
	STDMETHOD(get_Hwnd)(LONG *plHwnd)
	{
		*plHwnd = reinterpret_cast<LONG>(m_hWnd);
		return S_OK;
	}

private:
	HMODULE const					m_hModule;
	HWND							m_hWnd;
	HICON							m_hIcon;
	int								m_pctzoom;
	AutoBSTR						m_title;
	AutoBSTR						m_features;
	AutoReleasePtr<ITypeInfo>		m_spTypeInfo;
	AutoReleasePtr<IHTMLElement>	m_spElement;
	AutoReleasePtr<IOleWindow>		m_spOleWindow;
	AutoReleasePtr<IHTMLDocument2>	m_spDocument2;
	AutoReleasePtr<IHTMLWindow2>	m_spWindow2;
};

int WINAPI WinMainCRTStartup()
{
	__security_init_cookie();
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		hr = CHTDialogHost().Run(GetCommandLineW());
		CoUninitialize();
	}
	ExitProcess(hr);
}
