#include <QMutex> 
#include <QDateTime>
#include <QElapsedTimer>
#pragma once

#include <QFile>

// STL including:
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <queue>
#include <tuple>
#include <iomanip> // std::setfill, std::setw support
#include <algorithm> // std::find support

typedef unsigned long       DWORD;

const time_t timespanSecondsPerDay = 3600 * 24;

using STRING_VECTOR = std::vector<std::string>;

using LOG_FILE_SETTINGS = 
   std::tuple<std::string, bool, int>; // std::string strLogFilePath, bool nLogAutoRemoveEnable, int nLogRemoveDays

enum class LOG_MODES
{
	ERRORLOG = 0,
	DEBUGING,
	WARNING,
	TESTLOG,
	MATHLOG
};

class CLogFile
{	
	LOG_FILE_SETTINGS	m_logFileSettings; 
	STRING_VECTOR		m_vectLogFiles;		// Total collection of log-files
	STRING_VECTOR		m_vectFilesToKeep;	// Collection of log-files to keep
	std::string			m_sFileName;
	QMutex				m_qmLogFile;
	std::string			m_sApplicationName;
	std::string GetCategory(LOG_MODES Category);
public:
	CLogFile(void);
	explicit CLogFile(const std::string& sApplicationName);
	virtual ~CLogFile(void); 
public:
	void SetLogSettings(const LOG_FILE_SETTINGS& logFileSettings);
	void CleanUpLogFiles();
private:
	std::string GetStrLogFilePath() const;
	bool GetIsLogAutoRemoveEnable() const;
	int GetLogRemoveDays() const; 
	std::string FileNameByDate(const QDate& date, bool bCleanUpMode = false);
	void FillVectLogFiles();
	void FillVectLogFiles(const QString& strFileTemplate);
	std::string PrepareFullLogPath();
public:
	std::string GetApplicationName() const;
	void SetApplicationName(const std::string& sApplicationName);

public:
	void SaveLogFile(const char* pszFormat, ...);
	void SaveLogFile(LOG_MODES Category, const char* pszFormat, ...);
	void SaveLogCore(std::string& sLog, bool bSaveMSec, LOG_MODES Category);
};