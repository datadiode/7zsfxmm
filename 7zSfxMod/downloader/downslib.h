/* Created by John Åkerblom 10/26/2014
 * Forked from https://github.com/potmdehex/WinInet-Downloader by Jochen Neubeck
 */
#ifndef __DOWNSLIB_H_DEF__
#define __DOWNSLIB_H_DEF__

#define DOWNSLIB_DEFAULT_TIMEOUT 3000
#define DOWNSLIB_DEFAULT_USERAGENT TEXT("downslib")

#define DOWNSLIB_failed_at_crack_url    1
#define DOWNSLIB_failed_at_open         2
#define DOWNSLIB_failed_at_security     3
#define DOWNSLIB_failed_at_open_url     4
#define DOWNSLIB_failed_at_create_file  5
#define DOWNSLIB_download_canceled      6
#define DOWNSLIB_failed_at_write_file   7
#define DOWNSLIB_download_truncation    8
#define DOWNSLIB_hash_mismatch          9
#define DOWNSLIB_DEFINED(label)         label = DOWNSLIB_##label

struct downslib_download
{
    HWND hwnd;
    LPCTSTR url;
    LPCTSTR filename;
    LPCTSTR useragent;
    LPCTSTR proxyname;
    LPCTSTR proxybypass;
    LPCTSTR sha256;
    DWORD iflags;
    DWORD sflags;
    DWORD hflags;
    DWORD timeout;
    LONGLONG read_bytes;
    LONGLONG total_bytes;
    LONG ok_cancel_close;
    LONG status_code;
    DWORD started;
    /* Assume the RFC-recommended reason phrases, or else accept truncation */
    TCHAR status_text[sizeof "Requested range not satisfiable"]; /* i.e. 32 */
    TCHAR headers[INTERNET_MAX_URL_LENGTH + 1024];
};

DWORD WINAPI downslib_download(struct downslib_download volatile *);

#endif
