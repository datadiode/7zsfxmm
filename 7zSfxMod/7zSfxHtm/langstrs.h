/*---------------------------------------------------------------------------*/
/* File:        langstrs.h                                                   */
/* Created:     Fri, 29 Jul 2005 23:10:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 07 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#ifndef _LANGSTRS_H_INCLUDED_
#define _LANGSTRS_H_INCLUDED_

#define STR_SFXVERSION				1
#define STR_TITLE					2
#define STR_ERROR_TITLE				3
#define ERR_MODULEPATHNAME			6
#define ERR_OPEN_ARCHIVE			7
#define ERR_NON7Z_ARCHIVE			8
#define ERR_READ_CONFIG				9
#define ERR_WRITE_CONFIG			10
#define ERR_CONFIG_DATA				11
#define ERR_CREATE_FOLDER			12
#define ERR_DELETE_FILE				13
#define ERR_EXECUTE					16
#define ERR_7Z_UNSUPPORTED_METHOD	17
#define ERR_7Z_CRC_ERROR			18
#define ERR_7Z_DATA_ERROR			19
#define ERR_7Z_INTERNAL_ERROR		20

// Added April 9, 2008
#define ERR_CREATE_FILE				30
#define ERR_OVERWRITE				31

// added September 8, 2008
#define ERR_CONFIG_CMDLINE			32

// added December 18, 2008
#define ERR_7Z_EXTRACT_ERROR1		33
#define ERR_7Z_EXTRACT_ERROR2		34

// added June 6, 2010
#ifdef _SFX_USE_WARNINGS
	#define STR_WARNING_TITLE		40
#ifdef _SFX_USE_CHECK_FREE_SPACE
	#define STR_DISK_FREE_SPACE		42
#endif // _SFX_USE_CHECK_FREE_SPACE
#ifdef _SFX_USE_CHECK_RAM
	#define STR_PHYSICAL_MEMORY		43
#endif // _SFX_USE_CHECK_RAM
#endif // _SFX_USE_WARNINGS

#ifdef SFX_CRYPTO
	#define STR_PASSWORD_TEXT		44
#endif // SFX_CRYPTO

LPCWSTR GetLanguageString( UINT id );
void FreeLanguageStrings();

typedef struct tagLANGSTRING {
	UINT	id;
	LPCSTR	strPrimary;
	LPWSTR	lpszUnicode;
} LANGSTRING, * PLANGSTRING, * LPLANGSTRING;

extern LANGSTRING SfxLangStrings[];


#endif // _LANGSTRS_H_INCLUDED_
