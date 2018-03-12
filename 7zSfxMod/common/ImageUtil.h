/*
	Copyright (C) 2018 Jochen Neubeck

	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

struct DllHandle {
	HMODULE h;
	static HMODULE Load(LPCWSTR name) {
		WCHAR path[MAX_PATH];
		GetSystemDirectoryW(path, _countof(path));
		PathAppendW(path, name);
		return LoadLibrary(path);
	}
	FARPROC operator()(LPCSTR name) const {
		return h ? GetProcAddress(h, name) : NULL;
	}
};

template<typename P>
struct DllImport {
	FARPROC p;
	P operator *() const { return reinterpret_cast<P>(p); }
};

STDAPI_(HMODULE) LoadResLibrary(LPCWSTR path);

STDAPI ClearSearchPath();

STDAPI CreateImage(LPCWSTR src, LPCWSTR dst, DWORD how, DWORD img, DWORD osv);

STDAPI CreateSafeImage(LPCWSTR path, LPWSTR temp);
