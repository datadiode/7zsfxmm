/*---------------------------------------------------------------------------*/
/* File:        archive.h                                                    */
/* Created:     Sat, 05 Mar 2016 22:37:28 GMT                                */
/*              by Oleg N. Scherbakov, mailto:oleg@7zsfx.info                */
/* Last update: Wed, 07 Mar 2018 by https://github.com/datadiode             */
/*---------------------------------------------------------------------------*/
#ifndef _SFXARCHIVE_H_INCLUDED_
#define _SFXARCHIVE_H_INCLUDED_

class CSfxInStream;

class CSfxArchive
{
public:
	CSfxArchive();

	bool Init( LPCWSTR lpwszFileName );
	bool Open( IArchiveOpenCallback *passwordCallback );
	IInStream * GetStream() { return m_stream; };
	IInArchive * GetHandler() { return m_handler; };
	UInt32 GetItemsCount();
	HRESULT	GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value) { return m_handler->GetProperty(index,propID,value); };
	void SetBatchInstall( LPCWSTR lpwszBatchInstall ) { m_lpwszBatchInstall = lpwszBatchInstall; };
	LPCWSTR GetBatchInstall() { return m_lpwszBatchInstall; };
	void SetAutoInstall( bool enable ) { m_fUseAutoInstall = enable; };
	bool GetAutoInstall() { return m_fUseAutoInstall; };
	void SetAssumeYes( bool enable ) { m_fAssumeYes = enable; };
	bool GetAssumeYes() { return m_fAssumeYes; };
	void SetNoRun( bool enable ) { m_fNoRun = enable; };
	bool GetNoRun() { return m_fNoRun; };
	void SetShowHelp( bool enable ) { m_fShowHelp = enable; };
	bool GetShowHelp() { return m_fShowHelp; };

private:
	CSfxInStream *			m_streamSpec;
	CMyComPtr<IInStream>	m_stream;
	CMyComPtr<IInArchive>	m_handler;

	// global
	LPCWSTR		m_lpwszBatchInstall;
	bool		m_fUseAutoInstall;
	bool		m_fAssumeYes;
	bool		m_fNoRun;
	bool		m_fShowHelp;
};
#endif // _SFXARCHIVE_H_INCLUDED_
