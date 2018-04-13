/* Created by John Åkerblom 10/26/2014
 * Forked from https://github.com/potmdehex/WinInet-Downloader by Jochen Neubeck
 */
#include "common.h"

#include <wininet.h>
#include <shlwapi.h>

#include "downslib.h"
#include "utilmisc.h"
#include "sha256.h"

static int establish_security(HWND hWnd, HINTERNET hInternet, LPCTSTR host, LPCTSTR object, DWORD iflags, DWORD sflags)
{
    enum
    {
        succeeded,
        failed_at_connection_handle,
        failed_at_request_handle,
        failed_at_send
    } enter("check_security(%p, %p, '%"TF"s', '%"TF"s', %08x, %08x)", hWnd, hInternet, host, object, iflags, sflags);

    /* Leave handles uninitialized so as to not hide warning C4701 */
    HINTERNET hConnection;
    HINTERNET hRequest;

    DWORD dwLen = 0;

    hConnection = InternetConnect(hInternet,
                                  host,
                                  INTERNET_DEFAULT_HTTP_PORT,
                                  NULL,
                                  NULL,
                                  INTERNET_SERVICE_HTTP,
                                  0,
                                  0);
    if (hConnection == NULL) {
        goto(failed_at_connection_handle);
    }

    hRequest = HttpOpenRequest(hConnection,
                               TEXT("HEAD"),
                               object,
                               NULL,
                               NULL,
                               NULL,
                               iflags,
                               0);
    if (hRequest == NULL) {
        goto(failed_at_request_handle);
    }

    while (HttpSendRequest(hRequest, NULL, 0, NULL, 0) == FALSE) {
        DWORD dwError = GetLastError();
        DWORD dwSecFlags = 0;
        switch (dwError) {
        case ERROR_INTERNET_INVALID_CA:
            dwSecFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA;
            break;
        case ERROR_INTERNET_SEC_CERT_REVOKED:
            dwSecFlags = SECURITY_FLAG_IGNORE_REVOCATION;
            break;
        default:
            dwSecFlags = SECURITY_SET_MASK;
            break;
        }
        if ((sflags & dwSecFlags) == 0) {
            DWORD dwDlgResult = ERROR_INVALID_HANDLE;
            if (sflags & SECURITY_FLAG_SECURE) {
                /* Show confirm box */
                dwDlgResult = InternetErrorDlg(
                    hWnd, hRequest, dwError,
                    FLAGS_ERROR_UI_FILTER_FOR_ERRORS |
                    FLAGS_ERROR_UI_FLAGS_GENERATE_DATA |
                    FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS,
                    NULL);
            }
            switch (dwDlgResult) {
            case ERROR_SUCCESS:
            case ERROR_INTERNET_FORCE_RETRY:
                switch (dwError) {
                case ERROR_INTERNET_SEC_CERT_REV_FAILED:
                    /* InternetErrorDlg() seems to leave this one alone */
                    sflags = SECURITY_FLAG_IGNORE_REVOCATION;
                    break;
                }
                break;
            default:
                /* Resend won't help so give up */
                goto(failed_at_send);
            }
        }
        dwLen = sizeof(dwSecFlags);
        InternetQueryOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwSecFlags, &dwLen);
        dwSecFlags |= sflags & SECURITY_SET_MASK;
        InternetSetOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwSecFlags, sizeof(dwSecFlags));
        sflags = 0;
    }

failed_at_send:
    InternetCloseHandle(hRequest);
failed_at_request_handle:
    InternetCloseHandle(hConnection);
failed_at_connection_handle:
    return leave("check_security()");
}

DWORD WINAPI downslib_download(struct downslib_download volatile *param)
{
    enum
    {
        succeeded,
        /* For what follows, assign #defined numbers from header file */
        DOWNSLIB_DEFINED(failed_at_crack_url),
        DOWNSLIB_DEFINED(failed_at_open),
        DOWNSLIB_DEFINED(failed_at_security),
        DOWNSLIB_DEFINED(failed_at_open_url),
        DOWNSLIB_DEFINED(failed_at_create_file),
        DOWNSLIB_DEFINED(download_canceled),
        DOWNSLIB_DEFINED(failed_at_write_file),
        DOWNSLIB_DEFINED(download_truncation),
        DOWNSLIB_DEFINED(hash_mismatch)
    } enter("downslib_download(%p)", param);

    /* Leave handles uninitialized so as to not hide warning C4701 */
    HINTERNET hInternet;
    HINTERNET hUrl;
    HANDLE hFile;

    URL_COMPONENTS components;
    TCHAR object[INTERNET_MAX_PATH_LENGTH];
    TCHAR host[INTERNET_MAX_HOST_NAME_LENGTH];

    context_sha256_t sha256;

    DWORD dwWritten = 0;
    DWORD dwRead = 0;
    DWORD dwLen = 0;

    TCHAR buffer[8192 / sizeof(TCHAR)];

    /* Initialize use of WinINet */
    hInternet = InternetOpen(param->useragent,
                             param->proxyname != param->proxybypass ?
                                 INTERNET_OPEN_TYPE_PROXY : INTERNET_OPEN_TYPE_PRECONFIG,
                             param->proxyname,
                             param->proxybypass,
                             0);
    if (hInternet == NULL) {
        goto(failed_at_open);
    }
    /* Set timeout as specified */
    InternetSetOption(hInternet,
                      INTERNET_OPTION_RECEIVE_TIMEOUT,
                      const_cast(DWORD *, &param->timeout),
                      sizeof(param->timeout));
    /* Mess with URL */
    SecureZeroMemory(&components, sizeof(components));
    components.dwStructSize = sizeof(components);
    components.lpszHostName = host;
    components.dwHostNameLength = _countof(host);
    components.lpszUrlPath = object;
    components.dwUrlPathLength = _countof(object);
    if (!InternetCrackUrl(param->url, 0, 0, &components)) {
        goto(failed_at_crack_url);
    }
    /* Ignore possible certificate issues as specified */
    if (param->sflags) {
        if (establish_security(param->hwnd, hInternet, host, object, param->iflags, param->sflags) != 0) {
            goto(failed_at_security);
        }
    }
    /* Initiate download */
    hUrl = InternetOpenUrl(hInternet, param->url, NULL, 0, param->iflags, 0);
    if (hUrl == NULL) {
        goto(failed_at_open_url);
    }
    /* Gather summary information */
    dwLen = sizeof(buffer);
    if (HttpQueryInfo(hUrl, HTTP_QUERY_CONTENT_LENGTH, buffer, &dwLen, 0)) {
        LONGLONG total_bytes = 0;
        if (StrToInt64Ex(buffer, STIF_DEFAULT, &total_bytes)) {
            InterlockedCompareExchange64(&param->total_bytes, total_bytes, param->total_bytes);
        }
    }
    dwLen = sizeof(buffer);
    if (HttpQueryInfo(hUrl, HTTP_QUERY_STATUS_CODE, buffer, &dwLen, 0)) {
        int status_code = 0;
        if (StrToIntEx(buffer, STIF_DEFAULT, &status_code)) {
            param->status_code = status_code;
        }
    }
    dwLen = sizeof(buffer);
    if (HttpQueryInfo(hUrl, HTTP_QUERY_STATUS_TEXT, buffer, &dwLen, 0)) {
        lstrcpyn(const_cast(LPTSTR, param->status_text), buffer, _countof(param->status_text));
    }
    if (param->hflags) {
        dwLen = sizeof(buffer);
        if (HttpQueryInfo(hUrl, param->hflags, buffer, &dwLen, 0)) {
            lstrcpyn(const_cast(LPTSTR, param->headers), buffer, _countof(param->headers));
        }
    }
    param->started = GetTickCount();
    /* Indicate to UI thread that it is OK to access the summary information */
    InterlockedCompareExchange(&param->ok_cancel_close, IDOK, 0);
    /* Create output file */
    hFile = CreateFile(param->filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (hFile == NULL) {
        goto(failed_at_create_file);
    }
    /* Catch an early cancel before reading more data from server */
    if (param->ok_cancel_close == IDCANCEL) {
        goto(download_canceled);
    }
    /* Initialize hash state */
    if (param->sha256) {
        sha256_starts(&sha256);
    }
    /* Do the read-write round trip */
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &dwRead)) {
        InterlockedCompareExchange64(&param->read_bytes, param->read_bytes + dwRead, param->read_bytes);
        /* We are done if nothing more to read, so now we can report total size in our final cb call */
        if (dwRead == 0) {
            InterlockedCompareExchange64(&param->total_bytes, param->read_bytes, param->total_bytes);
            break;
        }
        if (param->ok_cancel_close == IDCANCEL) {
            goto(download_canceled);
        }
        if (!WriteFile(hFile, buffer, dwRead, &dwWritten, NULL) || dwWritten != dwRead) {
            goto(failed_at_write_file);
        }
        if (param->sha256) {
            sha256_update(&sha256, (uint8_t *)buffer, dwRead);
        }
    }
    /* Check for truncation */
    if (param->read_bytes != param->total_bytes) {
        goto(download_truncation);
    }
    /* Finish the hash computation and verify the result */
    if (param->sha256) {
        uint8_t digest[32];
        int i = sizeof(digest);
        LPTSTR p = buffer + 2 * sizeof(digest);
        sha256_finish(&sha256, digest);
        *p = TEXT('\0');
        do {
            WORD w = 0xFF00 | digest[--i];
            do {
                TCHAR c = w & 0xF;
                *--p = c < 10 ? TEXT('0') + c : TEXT('A') + c - 10;
            } while (0xFF00 & (w >>= 4));
        } while (i);
        if (lstrcmpi(param->sha256, buffer) != 0) {
            goto(hash_mismatch);
        }
    }

hash_mismatch:
download_truncation:
failed_at_write_file:
download_canceled:
    CloseHandle(hFile);
failed_at_create_file:
    InternetCloseHandle(hUrl);
failed_at_open_url:
failed_at_security:
    InternetCloseHandle(hInternet);
failed_at_crack_url:
failed_at_open:
    return leave("downslib_download()");
}
