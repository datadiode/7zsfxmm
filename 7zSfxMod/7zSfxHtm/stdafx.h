/*---------------------------------------------------------------------------*/
/* File:        stdafx.h                                                     */
/* Created:     Sun, 29 Oct 2006 13:32:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 07 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#ifndef _STDAFX_H_INCLUDED_
#define _STDAFX_H_INCLUDED_

#include "config.h"

#ifndef _CPPUNWIND
#	define _NO_EXCEPTIONS
#	define _HAS_EXCEPTIONS 0
#	define try if (1,1)
#	define catch(x) else if (1,1)
#endif

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <initguid.h>
#include <propvarutil.h>

#include <assert.h>
#include <wchar.h>

#include "Windows/PropVariant.h"
#include "Common/Common.h"
#include "7Zip/Common/FileStreams.h"
#include "7Zip/Common/StreamObjects.h"
#include "7Zip/Common/RegisterCodec.h"
#include "7Zip/Archive/IArchive.h"
#include "Common/Wildcard.h"

#include "../../7-Zip/C/7zcrc.h"

#ifdef SFX_CRYPTO
#	include "../../7-Zip/C/Sha256.h"
#endif // SFX_CRYPTO

#undef True
#undef False

#endif // _STDAFX_H_INCLUDED_
