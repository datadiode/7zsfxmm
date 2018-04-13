/* Created by John Åkerblom 10/26/2014
 * Forked from https://github.com/potmdehex/WinInet-Downloader by Jochen Neubeck
 */
#include "common.h"
#include "resource.h"

#include <wininet.h>
#include <shlwapi.h>

#include "downslib.h"
#include "utilmisc.h"

static void CreateToolTip(HWND hwnd, TOOLINFO *pti)
{
    HWND const hwndTip = CreateWindow(TOOLTIPS_CLASS, NULL,
        WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON | TTS_NOPREFIX | TTS_NOANIMATE | TTS_NOFADE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hwnd, NULL, NULL, NULL);
    if (hwndTip)
    {
        SendMessage(hwndTip, TTM_SETMAXTIPWIDTH, 0, GetSystemMetrics(SM_CXSCREEN));
        SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)pti);
        SendMessage(hwndTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, MAKELPARAM(20000, 0));
        SendMessage(hwndTip, TTM_SETDELAYTIME, TTDT_INITIAL, MAKELPARAM(500, 0));
        SendMessage(hwndTip, TTM_SETDELAYTIME, TTDT_RESHOW, MAKELPARAM(500, 0));
    }
}

INT_PTR CALLBACK DownloadDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    enum
    {
        CyclicUpdateTimer = 1,
        HideBubbleTimer = 2,
    };

    struct downslib_download volatile *const param =
        (struct downslib_download volatile*)GetWindowLongPtr(hwnd, DWLP_USER);

    switch (msg)
    {{
    case(:WM_INITDIALOG:)
        HINSTANCE const hInstance = GetModuleHandle(NULL);
        HICON const hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_DOWNLOAD));
        HWND const hwndPB = GetDlgItem(hwnd, IDC_PROGRESS);
        SendMessage(hwndPB, PBM_SETMARQUEE, 1, 0);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SetWindowLongPtr(hwnd, DWLP_USER, lParam);
        ShowWindow(hwnd, SW_SHOW);
        SetTimer(hwnd, CyclicUpdateTimer, 1000, NULL);
        return TRUE;

    case(:WM_WINDOWPOSCHANGING:)
        WINDOWPOS *pParam = (WINDOWPOS *)lParam;
        if (pParam->flags & (SWP_SHOWWINDOW | SWP_HIDEWINDOW))
        {
            HWND parent = GetParent(hwnd);
            if (parent)
            {
                BOOL enable = TRUE;
                if (pParam->flags & SWP_SHOWWINDOW)
                {
                    /* Center the dialog over the parent */
                    RECT rect;
                    GetWindowRect(hwnd, &rect);
                    pParam->cx = rect.right - rect.left;
                    pParam->cy = rect.bottom - rect.top;
                    GetWindowRect(parent, &rect);
                    pParam->x = (rect.right + rect.left - pParam->cx) / 2;
                    pParam->y = (rect.bottom + rect.top - pParam->cy) / 2;
                    pParam->flags &= ~SWP_NOMOVE;
                    enable = FALSE;
                }
                EnableWindow(parent, enable);
            }
        }
        break;

    case(:WM_TIMER:)
        HWND const hwndPB = GetDlgItem(hwnd, IDC_PROGRESS);
        switch (wParam)
        {{
        case(:CyclicUpdateTimer:)
            DECIMAL a, b;
            LONGLONG const read_bytes = InterlockedCompareExchange64(&param->read_bytes, 0, 0);
            LONGLONG const total_bytes = InterlockedCompareExchange64(&param->total_bytes, 0, 0);
            LONG permyriad;
            LONG const style = GetWindowLong(hwndPB, GWL_STYLE);

            /* If we have a total byte count we can calculate permyriad */
            if (total_bytes == 0 ||
                FAILED(VarDecFromI4(10000, &a)) ||
                FAILED(VarDecFromI8(read_bytes, &b)) ||
                FAILED(VarDecMul(&b, &a, &a)) ||
                FAILED(VarDecFromI8(total_bytes, &b)) ||
                FAILED(VarDecDiv(&a, &b, &a)) ||
                FAILED(VarI4FromDec(&a, &permyriad)))
            {
                if ((style & PBS_MARQUEE) == 0)
                {
                    SetWindowLong(hwndPB, GWL_STYLE, style | PBS_MARQUEE);
                    SendMessage(hwndPB, PBM_SETMARQUEE, 1, 0);
                }
                permyriad = -1;
            }
            else
            {
                if ((style & PBS_MARQUEE) != 0)
                {
                    SendMessage(hwndPB, PBM_SETMARQUEE, 0, 0);
                    SetWindowLong(hwndPB, GWL_STYLE, style & ~PBS_MARQUEE);
                    SendMessage(hwndPB, PBM_SETRANGE32, 0, 10000);
                }
                SendMessage(hwndPB, PBM_SETPOS, permyriad, 0);
            }

            if (param->ok_cancel_close)
            {
                TCHAR text[1024];
                LPTSTR p = text;
                p += lstrlen(StrFormatKBSize(read_bytes, p, 40));
                if (total_bytes)
                {
                    static TCHAR const sep[] = TEXT(" / ");
                    p = lstrcpy(p, sep) + _countof(sep) - 1;
                    StrFormatKBSize(total_bytes, p, 40);
                }
                SetDlgItemText(hwnd, IDC_KILOBYTES, text);
                p = text;
                p += wnsprintf(p, 20, TEXT("[%d %s]"), param->status_code, param->status_text);
                if (permyriad != -1)
                {
                    p += wnsprintf(p, 20, TEXT(" - %d%%"), permyriad / 100);
                }
                if (param->ok_cancel_close == IDCLOSE)
                {
                    UINT const exit_code = GetDlgItemInt(hwnd, IDC_PROGRESS, NULL, FALSE);
                    HWND const hwndCancel = GetDlgItem(hwnd, IDCANCEL);
                    SetWindowText(hwndCancel, TEXT("Close"));
                    SetWindowLongPtr(hwndCancel, GWLP_ID, IDCLOSE);
                    KillTimer(hwnd, wParam);
                    if (exit_code != 0)
                    {
                        TCHAR buffer[256];
                        LoadString(NULL, exit_code, buffer, _countof(buffer));
                        wnsprintf(p, 40, TEXT(" - #%d %s"), exit_code, buffer);
                    }
                }
                else
                {
                    int ticks = GetTickCount() - param->started;
                    LPCTSTR unit = TEXT("hours");
                    LPCTSTR kind = TEXT("left");
                    static TCHAR const sep[] = TEXT(" - ");
                    LPTSTR const t = p = lstrcpy(p, sep) + _countof(sep) - 1;
                    if (permyriad > 0 && ticks > 5000)
                        ticks = MulDiv(10000 - permyriad, ticks, permyriad);
                    else
                        kind = TEXT("elapsed");
                    if (ticks >= 3600000)
                        p += wnsprintf(p, 20, TEXT("%d:"), ticks / 3600000);
                    else
                        unit = TEXT("minutes");
                    if (ticks >= 60000)
                        p += wnsprintf(p, 20, TEXT("%02d:"), ticks / 60000 % 60);
                    else
                        unit = TEXT("seconds");
                    wnsprintf(p, 40, TEXT("%02d %s %s"), ticks / 1000 % 60, unit, kind);
                    /* Remove leading zeros from most significant part */
                    StrTrim(StrToInt(t) ? t : t + 1, TEXT("0"));
                }
                SetWindowText(hwnd, text);
                if (param->headers[0] != TEXT('\0'))
                {
                    TOOLINFO ti;
                    SecureZeroMemory(&ti, sizeof ti);
                    ti.cbSize = sizeof ti;
                    ti.hwnd = hwnd;
                    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
                    ti.uId = (UINT_PTR)hwndPB;
                    ti.lpszText = const_cast(LPTSTR, param->headers);
                    StrTrim(ti.lpszText, TEXT("\r\n"));
                    CreateToolTip(hwnd, &ti);
                    param->headers[0] = TEXT('\0');
                }
            }
            return TRUE;

        case(:HideBubbleTimer:)
            /* Cause bubble to hide and stay away until next WM_SETCURSOR */
            EnableWindow(hwndPB, FALSE);
            KillTimer(hwnd, wParam);
            return TRUE;
        }}
        break;

    case(:WM_SETCURSOR:)
        /* Allow bubble to show up again */
        HWND const hwndPB = GetDlgItem(hwnd, IDC_PROGRESS);
        EnableWindow(hwndPB, TRUE);
        break;

    case(:WM_COMMAND:)
        switch (wParam)
        {
        case IDCANCEL:
        case IDCLOSE:
            param->ok_cancel_close = (int)wParam;
            DestroyWindow(hwnd);
            break;
        }
        return TRUE;

    case(:WM_NOTIFY:)
        NMHDR *const pnm = (NMHDR *)lParam;
        switch (pnm->code)
        {{
        case(:TTN_SHOW:)
            /* Be sure to hide bubble before TTDT_AUTOPOP expires (for XP) */
            SetTimer(hwnd, HideBubbleTimer, 10000, NULL);
            break;
        }}
        return TRUE;

    case(:WM_DESTROY:)
        PostQuitMessage(0);
        break;

    default(:)
        break;
    }}
    return FALSE;
}
