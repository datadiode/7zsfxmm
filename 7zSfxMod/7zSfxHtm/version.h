/*---------------------------------------------------------------------------*/
/* File:        version.h                                                    */
/* Created:     Fri, 29 Jul 2005 03:23:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 07 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#define VERSION_REVISION 4001

#ifndef _VERSION_H_INCLUDED_
#define _VERSION_H_INCLUDED_

#define VERSION_SFX_NUM_TEXT		"1.0.0"
#define VERSION_SFX_NUM_BIN			1,0,0,VERSION_REVISION
#define VERSION_SFX_DATE_EN			"March 08, 2018"
#define VERSION_SFX_BRANCH_EN		""
#define VERSION_SFX_LEGAL_COPYRIGHT	"Copyright © 2005-2016 Oleg N. Scherbakov, 2018 Jochen Neubeck"

#define VERSION_7ZIP_NUM_TEXT		"16.04"
#define VERSION_7ZIP_DATE_EN		"October 04, 2016"
#define VERSION_7ZIP_BRANCH_EN		""

// end of editable

#ifdef _WIN64
	#define PLATFORM_NAME_A		"x64"
	#define PLATFORM_NAME_W		L"x64"
#else
	#ifdef _WIN32
		#define PLATFORM_NAME_A		"x86"
		#define PLATFORM_NAME_W		L"x86"
	#else
		#error "Unknown platform"
	#endif
#endif

#define VERSION_BUILD_TEXT1(a)	#a
#define VERSION_BUILD_TEXT2(a)	VERSION_BUILD_TEXT1(a)
#define VERSION_BUILD_TEXT	VERSION_BUILD_TEXT2(VERSION_REVISION)

#define VERSION_SFX_TEXT_EN	VERSION_SFX_NUM_TEXT VERSION_SFX_BRANCH_EN " [" PLATFORM_NAME_A "] build " VERSION_BUILD_TEXT " (" VERSION_SFX_DATE_EN ")"

#define VERSION_7ZIP_TEXT_EN	VERSION_7ZIP_NUM_TEXT VERSION_7ZIP_BRANCH_EN " (" VERSION_7ZIP_DATE_EN ")"

#define VERSION_7ZSFX_BIN	VERSION_SFX_NUM_BIN
#define VERSION_7ZSFX_TXT	VERSION_SFX_NUM_TEXT "." VERSION_BUILD_TEXT

#define PRIVATEBUILD_EN		VERSION_SFX_DATE_EN

#endif // _VERSION_H_INCLUDED_
