/* Created by John Åkerblom 10/26/2014
 * Forked from https://github.com/potmdehex/WinInet-Downloader by Jochen Neubeck
 */
#include "common.h"
#include "resource.h"

#include <wininet.h>
#include <process.h>
#include "../../midl/FETCH_HEAD.h"

#include "downslib.h"
#include "utilmisc.h"
#include "dialog.h"

static TCHAR const apptitle[] = TEXT("Downloader v1.4");

static char const usage[] =
    " [options] <url> <filename>\n"
    "\n"
    "Available options:\n"
    "/bubble - enables request info bubble\n"
    "/ignore - allows user to bypass security\n"
    "/ignore-expired\n"
    "/ignore-wrong-host\n"
    "/ignore-revoked\n"
    "/ignore-untrusted-root\n"
    "/ignore-wrong-usage\n"
    "/owner:<handle to owner window>\n"
    "/passive - no user interaction\n"
    "/passive:<exit code> - conditionally passive\n"
    "/proxy-name:<list of proxy servers>\n"
    "/proxy-bypass:<list of hosts>\n"
    "/secure - enforces https on http URLs\n"
    "/sha256:<hash> - enables sha256 verification\n"
    "/timeout:<milliseconds>\n"
    "/topmost - keeps window on top of z-order\n"
    "/useragent:<name>\n";

static LPCTSTR EatPrefixAndUnquote(LPTSTR text, LPCTSTR prefix)
{
    int len = lstrlen(prefix);
    if (StrIsIntlEqual(FALSE, text, prefix, len))
        PathUnquoteSpaces(text += len);
    else
        text = NULL;
    return text;
}

static DWORD Run(LPTSTR p)
{
    DWORD ret = 255;
    HWND top = HWND_TOP;
    HWND owner = NULL;
    DWORD passive = 0;
    HANDLE thread = NULL;
    struct downslib_download tp;
    int argc = 0;
    LPTSTR argv[4] = { NULL, NULL, NULL, NULL };

    static INITCOMMONCONTROLSEX const icc = { sizeof icc, ICC_PROGRESS_CLASS | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    SecureZeroMemory(&tp, sizeof(tp));
    tp.useragent =  DOWNSLIB_DEFAULT_USERAGENT;
    tp.iflags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES;
    tp.timeout = DOWNSLIB_DEFAULT_TIMEOUT;

    do
    {
        LPTSTR q = PathGetArgs(p);
        LPTSTR t = p + 1; /* option tag, if preceded by slash */
        LPCTSTR r;
        PathRemoveArgs(p);
        trace("p = %"TF"s\n", p);
        if (*p != TEXT('/'))
            PathUnquoteSpaces(argv[argc++] = p);
        else if (lstrcmpi(t, TEXT("topmost")) == 0)
            top = (HWND)(INT_PTR)-1; /* HWND_TOPMOST triggers warning C4306 */
        else if ((r = EatPrefixAndUnquote(t, TEXT("owner:"))) != NULL)
#ifdef _WIN64
			StrToInt64Ex(r, STIF_SUPPORT_HEX, (LONGLONG *)&owner);
#else
			StrToIntEx(r, STIF_SUPPORT_HEX, (LPINT)&owner);
#endif
        else if (lstrcmpi(t, TEXT("bubble")) == 0)
            tp.hflags |= HTTP_QUERY_RAW_HEADERS_CRLF | HTTP_QUERY_FLAG_REQUEST_HEADERS;
        else if (lstrcmpi(t, TEXT("secure")) == 0)
            tp.iflags |= INTERNET_FLAG_SECURE;
        else if (lstrcmpi(t, TEXT("ignore")) == 0)
            tp.sflags |= SECURITY_FLAG_SECURE; /* abused to enable confirm box */
        else if (lstrcmpi(t, TEXT("ignore-expired")) == 0)
            tp.sflags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        else if (lstrcmpi(t, TEXT("ignore-wrong-host")) == 0)
            tp.sflags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        else if (lstrcmpi(t, TEXT("ignore-revoked")) == 0)
            tp.sflags |= SECURITY_FLAG_IGNORE_REVOCATION;
        else if (lstrcmpi(t, TEXT("ignore-untrusted-root")) == 0)
            tp.sflags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
        else if (lstrcmpi(t, TEXT("ignore-wrong-usage")) == 0)
            tp.sflags |= SECURITY_FLAG_IGNORE_WRONG_USAGE;
        else if (lstrcmpi(t, TEXT("passive")) == 0)
            passive = 0xFFFFFFFF;
        else if ((r = EatPrefixAndUnquote(t, TEXT("passive:"))) != NULL)
            passive |= 1 << StrToInt(r);
        else if ((r = EatPrefixAndUnquote(t, TEXT("timeout:"))) != NULL)
            tp.timeout = StrToInt(r);
        else if ((r = EatPrefixAndUnquote(t, TEXT("useragent:"))) != NULL)
            tp.useragent = r;
        else if ((r = EatPrefixAndUnquote(t, TEXT("proxy-name:"))) != NULL)
            tp.proxyname = r;
        else if ((r = EatPrefixAndUnquote(t, TEXT("proxy-bypass:"))) != NULL)
            tp.proxybypass = r;
        else if ((r = EatPrefixAndUnquote(t, TEXT("sha256:"))) != NULL)
            tp.sha256 = r;
        else
            break;
        p = q + StrSpn(q, TEXT(" \t\r\n"));
    } while (*p != TEXT('\0') && argc < _countof(argv));

    if (*p == TEXT('/'))
    {
        TCHAR text[1024];
        wnsprintf(text, _countof(text), TEXT("Unknown option:\n%s"), p);
        MessageBox(NULL, text, apptitle, MB_OK);
        return ret;
    }
    if (argc == 1)
    {
        TCHAR text[1024];
        wnsprintf(text, _countof(text), TEXT("Usage:\n%s%hs"), PathFindFileName(argv[0]), usage);
        MessageBox(NULL, text, apptitle, MB_OK);
        return ret;
    }
    if (argc > 3)
    {
        MessageBox(NULL, TEXT("Too many arguments"), apptitle, MB_OK);
        return ret;
    }
    if (argc < 3)
    {
        MessageBox(NULL, TEXT("Too few arguments"), apptitle, MB_OK);
        return ret;
    }

    tp.hwnd = CreateDialogParamW(NULL, MAKEINTRESOURCEW(IDR_DOWNLOAD), owner, DownloadDlgProc, (LPARAM)&tp);
    SetWindowPos(tp.hwnd, top, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    tp.url = argv[1];
    tp.filename = argv[2];

    thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)downslib_download, &tp, 0, NULL);

    if (thread)
    {
        BOOL quit = FALSE;
        do
        {
            MSG msg;
            BOOL peek = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
            if (!peek || msg.message == WM_QUIT)
            {
                switch (MsgWaitForMultipleObjects(1, &thread, FALSE, INFINITE, QS_ALLINPUT))
                {
                case WAIT_OBJECT_0:
                    GetExitCodeThread(thread, &ret);
                    quit = peek;
                    if (!peek)
                    {
                        if (passive & (1 << ret))
                        {
                            PostMessage(tp.hwnd, WM_COMMAND, IDCLOSE, 0);
                        }
                        else if (tp.ok_cancel_close != IDCLOSE)
                        {
                            SetDlgItemInt(tp.hwnd, IDC_PROGRESS, ret, FALSE);
                            tp.ok_cancel_close = IDCLOSE;
                        }
                        WaitMessage(); /* to avoid heavy CPU load after worker thread exit */
                    }
                    break;
                }
            }
            else if (!IsDialogMessage(tp.hwnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } while (!quit);
        CloseHandle(thread);
    }
    return ret;
}

int WINAPI WinMainCRTStartup()
{
    __security_init_cookie();
    ExitProcess(Run(GetCommandLine()));
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
int const build_git_rev = BUILD_GIT_REV;
#pragma comment(linker, "/EXPORT:\"" BUILD_GIT_BRANCH BUILD_GIT_SHA " " __DATE__ " " __TIME__ "\"=" __cdecl(build_git_rev))
