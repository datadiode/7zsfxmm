/*---------------------------------------------------------------------------*/
/* File:        7zSfxHtmInt.h (formerly 7zSfxModInt.h)                       */
/* Created:     Wed, 25 Jul 2007 09:54:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 07 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#include "7zSfxHtm.h"
#include "langstrs.h"
#include "version.h"
#include "7Zip/Archive/7z/7zHandler.h"
#include "../common/ImageUtil.h"
#include "../common/VersionData.h"
#include "../common/vs_version.h"

#define SafeInvoke(p) if (p) p

#define SKIP_WHITESPACES_W(str) while( *str != L'\0' && unsigned(*str) <= L' ' ) str++;

#include "archive.h"

namespace SfxErrors
{
	enum
	{
		sePropVariant1 = 100,
		sePropVariant2,
		sePropVariant3,
		seAnti,
		seCreateFolder,
		seOverwrite,
		seCreateFile,
#ifdef SFX_CRYPTO
		seNoPassword,
#endif // SFX_CRYPTO
	};
}

int GetUIZoomFactor();

BOOL SfxCreateDirectory( LPCWSTR );
LPCWSTR SfxCreateDirectoryPath( LPWSTR, LPCWSTR = NULL );
BOOL DeleteDirectoryWithSubitems( LPCWSTR path );
BOOL DeleteFileOrDirectoryAlways( LPCWSTR path );

#define StringToLong	_wtol

LPCWSTR LoadQuotedString( LPCWSTR lpwszSrc, UString &result );
LPCWSTR IsSubString( LPCWSTR lpwszString, LPCWSTR lpwszSubString );
LPCWSTR IsSfxSwitch( LPCWSTR lpwszCommandLine, LPCWSTR lpwszSwitch );

#define SFX_OM_ERROR		-1
#define SFX_OM_OVERWRITE	0
#define SFX_OM_SKIP			1

#if !defined(SFX_VOLUMES) && !defined(SFX_PROTECT)
	class CSfxInStream : public CInFileStream
	{
	public:
		UInt32 GetStreamsCount() { return 0; }
	};
#else
#	include "7zip/Archive/Common/MultiStream.h"
	class CSfxInStream : public CMultiStream
	{
	public:
#ifdef SFX_VOLUMES
		bool Open(LPCTSTR fileName);
		bool InitVolumes(LPCTSTR fileName);
		IInStream * GetVolumesStream();
		UInt32 GetStreamsCount() { return Streams.Size(); };

	private:
		HRESULT OpenSubStream( LPCTSTR fileName, CSubStreamInfo &subStream );
#endif // SFX_VOLUMES
	};
#endif // !defined(SFX_VOLUMES) && !defined(SFX_PROTECT)
