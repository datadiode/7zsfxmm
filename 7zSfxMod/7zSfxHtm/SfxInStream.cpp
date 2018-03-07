/*---------------------------------------------------------------------------*/
/* File:        SfxInStream.cpp                                              */
/* Created:     Sat, 20 Nov 2010 11:58:35 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 07 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#include "stdafx.h"

#if defined(SFX_VOLUMES) || defined(SFX_PROTECT)
#include "7zSfxHtmInt.h"

#ifdef SFX_VOLUMES

HRESULT CSfxInStream::OpenSubStream( LPCTSTR fileName, CSubStreamInfo &subStream )
{
	CInFileStream *const streamSpec = new CInFileStream;
	subStream.Stream = streamSpec;
	HRESULT hr = S_FALSE;
	if( streamSpec->Open( fileName ) )
		hr = streamSpec->GetSize( &subStream.Size );
	return hr;
}

bool CSfxInStream::Open(LPCTSTR fileName)
{
	Streams.Clear();
	CSubStreamInfo stream;
	if( OpenSubStream( fileName, stream ) != S_OK )
		return false;
	Streams.Add( stream );
	InitVolumes( fileName );
	CMultiStream::Init();
	return true;
}

// style 0: (name.7z.001.exe, name.7z.002, ...) || (name.001.exe, name.002, ...)
// style 1: name.exe, name.001, ...
static int GetVolumesName( UString &baseName )
{
	int const nSepPos = baseName.ReverseFind_PathSepar();
	int const nExtPos = baseName.ReverseFind_Dot();
	if( nExtPos <= nSepPos )
		return -1;
	baseName.ReleaseBuf_SetEnd(nExtPos);
	int const nExtPos1 = baseName.ReverseFind_Dot();
#ifdef _SFX_USE_VOLUME_NAME_STYLE
	switch( nVolumeNameStyle )
	{
	case 1:
		if( nExtPos1 > nSepPos && (nExtPos - nExtPos1) == 4 /* ".00x" */ )
		{
			UString tmp = baseName.Mid(nExtPos1 + 1, 2);
			if( tmp == L"00" )
			{
				baseName.ReleaseBuf_SetEnd(nExtPos1);
				return StringToLong(baseName.Ptr(nExtPos1 + 1)) + 1;
			}
		}
		return 1;
	default:
#endif // _SFX_USE_VOLUME_NAME_STYLE
		if( nExtPos1 <= nSepPos )
			return -1;
		UString tmp = baseName.Mid(nExtPos1 + 1, 2);
		if( tmp == L"00" )
		{
			baseName.ReleaseBuf_SetEnd(nExtPos);
			return StringToLong(baseName.Ptr(nExtPos1 + 1)) + 1;
		}
#ifdef _SFX_USE_VOLUME_NAME_STYLE
	}
#endif // _SFX_USE_VOLUME_NAME_STYLE
	return -1;
}

bool CSfxInStream::InitVolumes(LPCTSTR fileName)
{
	CSubStreamInfo subStream;
	UString baseName = fileName;
	int nStartVolume = GetVolumesName( baseName );

	if( nStartVolume == -1 )
		return false;

	for( int nVolume = nStartVolume; ; nVolume++ )
	{
		WCHAR ccIndex[32];
		wsprintf( ccIndex, L".%03u", nVolume );
		UString strVolume = baseName + ccIndex;
		if( OpenSubStream( strVolume, subStream ) != S_OK )
			break;
		Streams.Add( subStream );
	}
	return true;
}
#endif // SFX_VOLUMES
#endif // SFX_VOLUMES || SFX_PROTECT
