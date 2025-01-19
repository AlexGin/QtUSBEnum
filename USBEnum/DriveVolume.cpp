#include "USBEnum/DriveVolume.h"

CDriveVolume::CDriveVolume()
	: m_nDeviceNumber(-1) // Inactive value
{
}

CDriveVolume::CDriveVolume(const std::string& strDosDeviceName, const std::string& strVolume)
   : m_nDeviceNumber(-1), m_strDosDeviceName(strDosDeviceName), m_strVolume(strVolume)
{
}

int CDriveVolume::GetDeviceNumber() const
{
	return m_nDeviceNumber;
}

std::string CDriveVolume::GetStrDosDeviceName() const
{
	return m_strDosDeviceName;
}

std::string CDriveVolume::GetStrVolume() const
{
	return m_strVolume;
}

std::string CDriveVolume::GetStrVolumePath() const
{
	if (m_vectVolumePath.empty())
		return "";

	return m_vectVolumePath[0];
}

PATH_VECTOR& CDriveVolume::GetVectorVolumePath() const
{
	return (PATH_VECTOR&)m_vectVolumePath;
}

std::string CDriveVolume::GetStrDevice() const
{
	return m_strDevice;
}

void CDriveVolume::SetDeviceNumber(int nDeviceNumber)
{
	m_nDeviceNumber = nDeviceNumber;
}

void CDriveVolume::SetStrDosDeviceName(const std::string& strDosDeviceName)
{
	m_strDosDeviceName = strDosDeviceName;
}

void CDriveVolume::SetStrVolume(const std::string& strVolume)
{
	m_strVolume = strVolume;
}

void CDriveVolume::SetVectorVolumePath(PATH_VECTOR&& vectVolumePath)
{
	m_vectVolumePath = std::move(vectVolumePath);
}

void CDriveVolume::SetVectorVolumePath(const PATH_VECTOR& vectVolumePath)
{
	m_vectVolumePath = vectVolumePath;
}

void CDriveVolume::SetStrDevice(const std::string& strDevice)
{
	m_strDevice = strDevice;
}