#include <windows.h>
#include <propvarutil.h>
#include <urlmon.h>
#include <mshtmhst.h>
#include <process.h>

#pragma comment(linker, "/EXPORT:ShowHTMLDialogEx=MSHTML.ShowHTMLDialogEx")

static LPTSTR EatPrefixAndUnquote(LPTSTR text, LPCTSTR prefix)
{
	int len = lstrlen(prefix);
	if (StrIsIntlEqual(FALSE, text, prefix, len))
		PathUnquoteSpaces(text += len);
	else
		text = NULL;
	return text;
}

static HRESULT Run(LPTSTR p)
{
	int argc = 0;
	LPTSTR argv[2] = { NULL, NULL };
	LPTSTR features = NULL;
	DWORD flags = HTMLDLG_MODAL | HTMLDLG_VERIFY;
	do
	{
		LPTSTR q = PathGetArgs(p);
		LPTSTR t = p + 1;
		LPTSTR r;
		PathRemoveArgs(p);
		if (*p != TEXT('/'))
			PathUnquoteSpaces(argv[argc++] = p);
		else if (lstrcmpi(t, TEXT("hidden")) == 0)
			flags |= HTMLDLG_NOUI;
		else if ((r = EatPrefixAndUnquote(t, TEXT("features:"))) != NULL)
			features = r;
		else
			break;
		p = q + StrSpn(q, TEXT(" \t\r\n"));
	} while (*p != TEXT('\0') && argc < _countof(argv));

	HMODULE const hModule = GetModuleHandle(NULL);
	SHOWHTMLDIALOGEXFN *const ShowHTMLDialogEx = reinterpret_cast
		<SHOWHTMLDIALOGEXFN *>(GetProcAddress(hModule, "ShowHTMLDialogEx"));

	TCHAR url[MAX_PATH + 40];
	if (argv[1] == NULL)
	{
		TCHAR path[MAX_PATH];
		GetModuleFileName(NULL, path, MAX_PATH);
		wsprintf(url, TEXT("res://%s//#1"), PathFindFileName(path));
		argv[1] = url;
	}

	IMoniker *moniker = NULL;
	HRESULT hr = CreateURLMoniker(NULL, argv[1], &moniker);
	if (SUCCEEDED(hr))
	{
		VARIANT in, out;
		InitVariantFromString(p, &in);
		VariantInit(&out);
		if (SUCCEEDED(hr = ShowHTMLDialogEx(NULL, moniker, flags, &in, features, &out)) &&
			SUCCEEDED(hr = VariantChangeType(&out, &out, 0, VT_I4)))
		{
			hr = V_I4(&out);
		}
		VariantClear(&in);
		VariantClear(&out);
		moniker->Release();
	}
	return hr;
}

int WINAPI WinMainCRTStartup()
{
	__security_init_cookie();
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		hr = Run(GetCommandLine());
		CoUninitialize();
	}
	ExitProcess(hr);
}
