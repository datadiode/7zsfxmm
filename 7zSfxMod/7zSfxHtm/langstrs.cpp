/*---------------------------------------------------------------------------*/
/* File:        langstrs.cpp                                                 */
/* Created:     Wed, 10 Jan 2007 23:44:00 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 07 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#include "stdafx.h"
#include "7zSfxHtmInt.h"
#include "langstrs.h"
#include "version.h"

#pragma execution_character_set("utf-8")

LANGSTRING SfxLangStrings[] = {
// Версия модуля
	{ STR_SFXVERSION,		"SFX module - Copyright © 2005-2016 Oleg Scherbakov\n"
							"HTSFX series - Copyright © 2018 Jochen Neubeck\n"
							"\t" VERSION_SFX_TEXT_EN "\n"
							"\n7-Zip archiver - Copyright © 1999-2016 Igor Pavlov\n"
							"\t" VERSION_7ZIP_TEXT_EN "\n"
							"\nSupported methods and filters, build options:\n",
							NULL },
// Заголовок окон сообщений по умолчанию, отображаемое, если
// модуль не смог определить имя exe-файла.
// Если SFX модуль смог распознать свое имя (а это практически 100%), вместо него
// будет выводится имя exe-файла без расширения. Или, если файл конфигурации
// успешно прочитан и в нем указан "Title" - содержимое "Title"
	{ STR_TITLE,			"7z HTSFX",
							NULL },
	{ STR_ERROR_TITLE,		"7z HTSFX: error",
							NULL },
// Невозможно получить имя файла SFX установки. Теоритически, никогда не появится,
// я даже не знаю, как сэмулировать эту ситуацию.
	{ ERR_MODULEPATHNAME,	"Could not get SFX filename.",
							NULL },
// Невозможно открыть файл архива
// Может возникнуть, если файл блокирован другим приложением, недостаточно прав и т.п.
// Вторая строка будет содержать ТЕКСТ системной ошибки НА ЯЗЫКЕ, выставленном в системе по умолчанию
	{ ERR_OPEN_ARCHIVE,		"Could not open archive file \"%s\".",
							NULL },
// Архив поврежден/не найдена сигнатура, т.е внутренние структуры не соответствуют формату 7-Zip SFX
// Может возникнуть на "корявых" или поврежденных 7-Zip SFX установках
	{ ERR_NON7Z_ARCHIVE,	"Non 7z archive.",
							NULL },
// Невозможно прочитать файл конфигурации (или он отсутствует)
	{ ERR_READ_CONFIG,		"Could not read SFX configuration or configuration not found.",
							NULL },
	{ ERR_WRITE_CONFIG,		"Could not write SFX configuration.",
							NULL },
// Ошибка в файле конфигурации, отсутствие обязательных кавычек, несоответствие
// формату Параметр="Значение", значение параметра не в UTF8 и т.п.
// "Ошибка в строке Х конфигурации"
	{ ERR_CONFIG_DATA,		"Error in line %d of configuration data:\n\n%s",
							NULL },
// Невозможно создать папку "такую-то"
// Вторая строка будет содержать ТЕКСТ системной ошибки НА ЯЗЫКЕ, выставленном в системе по умолчанию
	{ ERR_CREATE_FOLDER,	"Could not create folder \"%s\".",
							NULL },
// Невозможно удалить файл или директорию "такой-то-такая-то"
// Вторая строка будет содержать ТЕКСТ системной ошибки НА ЯЗЫКЕ, выставленном в системе по умолчанию
	{ ERR_DELETE_FILE,		"Could not delete file or folder \"%s\".",
							NULL },
// Произошла ошибка при выполнении "такой-то программы"
// Вторая строка будет содержать ТЕКСТ системной ошибки НА ЯЗЫКЕ, выставленном в системе по умолчанию
	{ ERR_EXECUTE,			"Error during execution \"%s\".",
							NULL },
// Ошибки ядра распаковки
	{ ERR_7Z_UNSUPPORTED_METHOD,	"7-Zip: Unsupported method.",
									NULL },
	{ ERR_7Z_CRC_ERROR,				"7-Zip: CRC error.",
									NULL },
	{ ERR_7Z_DATA_ERROR,			"7-Zip: Data error.\nThe archive is corrupted"
#ifdef SFX_CRYPTO
									", or invalid password was entered"
#endif // SFX_CRYPTO
									".",
									NULL },
	{ ERR_7Z_INTERNAL_ERROR,		"7-Zip: Internal error, code %u.",
									NULL },
	{ ERR_7Z_EXTRACT_ERROR1,		"7-Zip: Internal error, code 0x%08X.",
									NULL },
	{ ERR_7Z_EXTRACT_ERROR2,		"7-Zip: Extraction error.",
									NULL },
// Added April 9, 2008
// Невозможно создать файл "такой-то"
// Вторая строка будет содержать ТЕКСТ системной ошибки НА ЯЗЫКЕ, выставленном в системе по умолчанию
	{ ERR_CREATE_FILE,		"Could not create file \"%s\".",
							NULL },

	{ ERR_OVERWRITE,		"Could not overwrite file \"%s\".",
							NULL },
	{ ERR_CONFIG_CMDLINE,	"Error in command line:\n\n%s",
							NULL },
// Added June 6, 2010: warnings dialogs
#ifdef _SFX_USE_WARNINGS
	{ STR_WARNING_TITLE,		"7z HTSFX: warning",
								NULL },
#ifdef _SFX_USE_CHECK_FREE_SPACE
	{ STR_DISK_FREE_SPACE,		"Not enough free space for extracting.\n\nDo you want to continue?",
								NULL },
#endif // _SFX_USE_CHECK_FREE_SPACE
#ifdef _SFX_USE_CHECK_RAM
	{ STR_PHYSICAL_MEMORY,		"Insufficient physical memory.\nExtracting may take a long time.\n\nDo you want to continue?",
								NULL },
#endif // _SFX_USE_CHECK_FREE_SPACE
#endif // _SFX_USE_WARNINGS
#ifdef SFX_CRYPTO
	{ STR_PASSWORD_TEXT,		"Enter password:", NULL },
#endif // SFX_CRYPTO
	{ 0, "", NULL }
};
