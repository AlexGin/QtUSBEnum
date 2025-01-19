#include "LogFile.h"

#include <QMessageBox>
#include <QCoreApplication>
#include <Windows.h>
#include "FileUtils.h"

//////////////////////////////////////////////////////////////////////

CLogFile::CLogFile(const std::string& sApplicationName)
	: m_sApplicationName(sApplicationName)
{
}

CLogFile::CLogFile(void)
{
}

CLogFile::~CLogFile(void)
{
}

void CLogFile::SetApplicationName(const std::string& sApplicationName)
{
	m_sApplicationName = sApplicationName;
}

std::string CLogFile::GetApplicationName() const
{
	return m_sApplicationName;
}

void CLogFile::SetLogSettings(const LOG_FILE_SETTINGS& logFileSettings)
{
	m_logFileSettings = logFileSettings;
}

std::string CLogFile::GetStrLogFilePath() const
{
	return std::get<0>(m_logFileSettings);
}

bool CLogFile::GetIsLogAutoRemoveEnable() const
{
	return std::get<1>(m_logFileSettings);
}

int CLogFile::GetLogRemoveDays() const
{
	return std::get<2>(m_logFileSettings);
}

std::string CLogFile::GetCategory(LOG_MODES Category)
{
	std::string sResult;
	switch (Category)
	{
	case LOG_MODES::ERRORLOG: 	sResult = "[ERRORLOG]";	break;
	case LOG_MODES::DEBUGING:	sResult = "[DEBUGING]"; break;
	case LOG_MODES::WARNING:    sResult = "[WARNING]";  break;
	case LOG_MODES::TESTLOG:  	sResult = "[TESTLOG]";  break;
	case LOG_MODES::MATHLOG:  	sResult = "[MATHLOG]";  break;
	}
	return sResult;
}

void CLogFile::SaveLogFile(LOG_MODES Category, const char* pszFormat, ...)
{
	bool bShowMs = (bool)(Category == LOG_MODES::MATHLOG || Category == LOG_MODES::DEBUGING || Category == LOG_MODES::TESTLOG);

	m_qmLogFile.lock();

	char	szLog[2048];	// In the old version:	szLog[256];
	va_list argList;
	va_start(argList, pszFormat);
	vsprintf(szLog, pszFormat, argList);
	va_end(argList);

	std::string sLog(szLog);
	SaveLogCore(sLog, bShowMs, Category);
	
	m_qmLogFile.unlock();
}

std::string CLogFile::PrepareFullLogPath()
{
	std::string sLogFilePath = GetStrLogFilePath();
	bool bIsParentPath = (bool)(0x2E == sLogFilePath[0] && 0x2E == sLogFilePath[1]);
	std::string sDirName;
	CFileUtils::PrepareRelativePath(sDirName, bIsParentPath); 

	std::string sLogPath;
	if (bIsParentPath)
	{
		size_t nSize = sLogFilePath.size();
		for (size_t nIndex = 3; nIndex < nSize; nIndex++)
		{
			char chCurr = sLogFilePath[nIndex];
			sLogPath.push_back(chCurr);
		}
	}
	std::string sLogFileDirName = sDirName + sLogPath;
	return sLogFileDirName;
}

std::string CLogFile::FileNameByDate(const QDate& date, bool bCleanUpMode)
{
	int iYear = date.year();
	int iDay = date.day();
	int iMonth = date.month(); 

	std::stringstream ssY;
	ssY << std::setw(2) << std::setfill('0') << iYear - 2000;

	std::stringstream ssD;
	ssD << std::setw(2) << std::setfill('0') << iDay;

	std::stringstream ssMn;
	ssMn << std::setw(2) << std::setfill('0') << iMonth;

	std::stringstream ss1;
	ss1 << ssD.str() << ssMn.str() << ssY.str();
	std::string sCurrentDate = ss1.str();

	std::stringstream ss2;

	if (!bCleanUpMode)	// If work-mode:
		ss2 << "\\" << m_sApplicationName << "_" << sCurrentDate << ".log";
	else				// If Clean-up-mode:
		ss2 << m_sApplicationName << "_" << sCurrentDate << ".log";

	std::string sLogiFileName = ss2.str();
	return sLogiFileName;
}

void CLogFile::SaveLogCore (std::string& sLog, bool bSaveMSec, LOG_MODES Category)
{
	std::string sPath = PrepareFullLogPath();

	QDateTime qdt = QDateTime::currentDateTime();

	QDate date = qdt.date();
	std::string sLogiFileName = sPath + FileNameByDate(date);
	
	QFile fileLog(sLogiFileName.c_str());

	if (!fileLog.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
	{
		QMessageBox::warning(NULL, "QtTestLocator", "Unable creating of Log-file !");
		return;
	}

	int iHour = qdt.time().hour();
	int iMinute = qdt.time().minute();
	int iSecond = qdt.time().second();
	int iMilliseconds = qdt.time().msec();

	std::stringstream ssH;
	ssH << std::setw(2) << std::setfill('0') << iHour;

	std::stringstream ssM;
	ssM << std::setw(2) << std::setfill('0') << iMinute;

	std::stringstream ssS;
	ssS << std::setw(2) << std::setfill('0') << iSecond;

	std::stringstream ssMS;
	if (bSaveMSec)	
		ssMS << std::setw(3) << std::setfill('0') << iMilliseconds;
		
	std::stringstream ss3;
	if (bSaveMSec)
		ss3 << ssH.str() << ":" << ssM.str() << ":" << ssS.str() << "." << ssMS.str();
	else
		ss3 << ssH.str() << ":" << ssM.str() << ":" << ssS.str();

	std::string sCurrentTime = ss3.str();
	
	std::stringstream ss4;
	
	std::string sCateg = GetCategory(Category);
	ss4 << sCurrentTime << sCateg << sLog << std::endl;
	
	std::string sTextInfo = ss4.str();

	fileLog.write(sTextInfo.c_str(), sTextInfo.length());
	fileLog.close();
}

void CLogFile::FillVectLogFiles()
{
	m_vectLogFiles.clear();
	std::string sPath = PrepareFullLogPath();
	
	QString	strCurrDir1 = QString(sPath.c_str()) + "\\*.log";

	FillVectLogFiles(strCurrDir1);
	
	DWORD dwFilesPrepareToDelete = (DWORD)m_vectLogFiles.size(); 
	SaveLogFile(LOG_MODES::DEBUGING, "CLogFile::FillVectLogFiles: Total files in the 'm_vectLogFiles'=%u",
		dwFilesPrepareToDelete);
}

void CLogFile::FillVectLogFiles(const QString& strFileTemplate)
{
	QString	strCurrDir = strFileTemplate;
	WIN32_FIND_DATAA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	hFind = ::FindFirstFileA(strCurrDir.toStdString().c_str(), &ffd);
	if (INVALID_HANDLE_VALUE == hFind)
		return;

	// List all the files in the directory with some info about them.

	do
	{
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			continue;
		}
		else
		{
			std::string sFileName(ffd.cFileName);
			m_vectLogFiles.push_back(sFileName);
		}
	} while (FindNextFileA(hFind, &ffd) != 0);

	// See:
	// http://msdn.microsoft.com/en-us/library/windows/desktop/aa365200%28v=vs.85%29.aspx   
}

// see:
// http://fasterland.net/unix-timestamp-qt-using-qdatetime-class.html
void CLogFile::CleanUpLogFiles()
{
	m_vectLogFiles.clear();
	m_vectFilesToKeep.clear();

	FillVectLogFiles(); 

	std::string sPath = PrepareFullLogPath();

	QDateTime qdt = QDateTime::currentDateTime();
	time_t timeCurrent = qdt.toTime_t();

	int nLogRemoveDays = GetLogRemoveDays();
	for (int i = 0; i <= nLogRemoveDays; i++)
	{
		QDateTime dt;
		dt.setTime_t(timeCurrent);
		QDate date = dt.date();

		std::string sLogiFileToKeep = FileNameByDate(date, true); 
		m_vectFilesToKeep.push_back(sLogiFileToKeep);

		timeCurrent -= timespanSecondsPerDay;
	}

	for (auto sFileName : m_vectLogFiles)
	{
		auto it = std::find(m_vectFilesToKeep.begin(), m_vectFilesToKeep.end(), sFileName);
		if (it == m_vectFilesToKeep.end()) // File 'sFileName' is NOT present in the 'm_vectFilesToKeep'
		{			
			std::string sFilePath = sPath + "\\" + sFileName; 
			QString strFilePath(sFilePath.c_str());
			bool bResult = QFile::remove(strFilePath);
			if (!bResult)
			{
				SaveLogFile(LOG_MODES::DEBUGING, "CLogFile::CleanUpLogFiles: Deleting file '%s' error",
					strFilePath.toStdString().c_str());
			}
		}
	}
}
