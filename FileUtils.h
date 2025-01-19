#pragma once

#include <string>

class CFileUtils
{
public:
	static bool IsRelativePath(std::string& astrFileName, int* apParentFlag); 
	static std::string RetrievePath(DWORD* pErr); 
	static std::string RetrieveParentPath(); 
	static void PrepareRelativePath(std::string& astrFileName, int aiParent);
};

