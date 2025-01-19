#include <Windows.h>
#include <sstream>

#include "FileUtils.h"

bool CFileUtils::IsRelativePath(std::string& astrFileName, int* apParentFlag) // Task 0000052
{
	char ch0 = astrFileName[0];
	bool bResult = (ch0 == 0x2E); // First symbol: "."
	if (bResult && apParentFlag)
	{
		char ch1 = astrFileName[1];
		BOOL bParentFlag = (BOOL)(ch1 == 0x2E); // Second symbol: "."
		*apParentFlag = (int)(bParentFlag);
	}
	return bResult;
}

std::string CFileUtils::RetrievePath(DWORD* pErr)
{
	char szString[_MAX_PATH + 1];
	memset(szString, 0, sizeof(szString));
	long nSize = (long)(::GetModuleFileNameA(NULL, szString, (_MAX_PATH + 1)));
	int iLn = strlen(szString);

	if (!nSize)
	{
		DWORD dwErr = ::GetLastError();
		if (pErr)
			*pErr = dwErr;
		return "";
	}

	if (nSize > _MAX_PATH)
		return "-1"; // (DWORD)(-1);

	int iIndex = iLn;
	do
	{
		TCHAR ch = szString[--iIndex];
		if (ch != 0x5C)					// "\"
			szString[iIndex] = 0;
	} while (szString[iIndex] != 0x5C);	// "\"
	std::string sResult(szString);
	return sResult;
}

std::string CFileUtils::RetrieveParentPath()
{
	DWORD dwErr = 0;
	std::string sPath = CFileUtils::RetrievePath(&dwErr);
	int iLen = sPath.length();

	char szString[_MAX_PATH + 1];
	memset(szString, 0, sizeof(szString));

	int iSlashCount = 0;
	int iIndex = iLen - 1;
	do
	{
		TCHAR ch = sPath[iIndex--];
		if (ch == 0x5C)				// "\"
			iSlashCount++;
	} while (iSlashCount < 2);
	iIndex++; // Include last symbol "\" into the path

	for (int i = 0; i <= iIndex; i++)
	{
		char ch = sPath[i];
		szString[i] = ch;
	}
	std::string sResult((const char*)szString);
	return sResult;
}

void CFileUtils::PrepareRelativePath(std::string& astrFileName, int aiParent) // Task 0000052
{
	std::string sInput(astrFileName);
	DWORD dwErr = 0;
	std::string sPath = (aiParent == 1) ? RetrieveParentPath() : RetrievePath(&dwErr);

	int iFilePathSize = (int)sInput.length();
	char* pSzOut = new char[iFilePathSize + 1];
	memset(pSzOut, 0, (iFilePathSize + 1));

	int iOutIndex = 0;
	bool bOutFlag = false;
	for (int iIndex = 0; iIndex < iFilePathSize; iIndex++)
	{
		if (aiParent)
		{
			if (!bOutFlag && (3 == iIndex))
				bOutFlag = true;
		}
		else
		{
			if (!bOutFlag && (2 == iIndex))
				bOutFlag = true;
		}
		char chCurr = sInput[iIndex];
		if (bOutFlag)
			pSzOut[iOutIndex++] = chCurr;
	}
	std::string sOut(pSzOut);
	delete[] pSzOut;

	std::stringstream ss;
	ss << sPath << sOut;

	std::string sResult = ss.str();
	astrFileName = sResult;
}

