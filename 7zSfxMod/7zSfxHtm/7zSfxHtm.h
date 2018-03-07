/*---------------------------------------------------------------------------*/
/* File:        7zSfxHtm.h (formerly 7zSfxMod.h)                             */
/* Created:     Thu, 28 Jul 2005 02:44:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 07 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/

#define OVERWRITE_MODE_ALL				0
#define OVERWRITE_MODE_NONE				1
#define OVERWRITE_MODE_OLDER			2
//#define OVERWRITE_MODE_CONFIRM			3
//#define OVERWRITE_MODE_CONFIRM_NEWER	4
#define OVERWRITE_MODE_MAX				2 /* current up to OVERWRITE_MODE_OLDER */
#define OVERWRITE_MODE_MASK				0x07
#define OVERWRITE_FLAG_SKIP_LOCKED		0x08
