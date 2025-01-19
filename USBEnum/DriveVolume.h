#pragma once

#include <string>
#include <vector>

using PATH_VECTOR = std::vector<std::string>;

class CDriveVolume
{
	friend class CUsbVolume;
public:
	CDriveVolume();
	CDriveVolume(const std::string& strDosDeviceName, const std::string& strVolume);
		
public:
	int GetDeviceNumber() const;
	std::string GetStrDosDeviceName() const;
	std::string GetStrVolume() const;
	std::string GetStrVolumePath() const; // Get value in the: m_vectVolumePath[0]
	std::string GetStrDevice() const;
	void SetDeviceNumber(int nDeviceNumber); 
	void SetStrDevice(const std::string& strDevice);
	void SetStrDosDeviceName(const std::string& strDosDeviceName);
	void SetStrVolume(const std::string& strVolume);
	void SetVectorVolumePath(PATH_VECTOR&& vectVolumePath); 
	void SetVectorVolumePath(const PATH_VECTOR& vectVolumePath);
	PATH_VECTOR& GetVectorVolumePath() const; 	
	
private:
	int m_nDeviceNumber; 
	std::string m_strDevice;
	std::string m_strDosDeviceName;
	std::string m_strVolume;
	PATH_VECTOR m_vectVolumePath; // Main path - as a value from: m_vectVolumePath[0]	
};
