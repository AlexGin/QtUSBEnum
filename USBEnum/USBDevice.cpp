#include "USBEnum\USBDevice.h"
#include <Windows.h>

using namespace USBEnum;

USBDevice::USBDevice()
	: m_iHub(0), m_iPort(0), m_kindOfDevice(USB_DEVICE_KIND::UNKNOWN), m_usbCateg(USB_ENGINE_CATEGORY::GLOBAL)
{
}	

USBDevice::USBDevice(const USBDevice& data)
{
	m_iHub = data.m_iHub;
	m_iPort = data.m_iPort;	
	m_usbCateg = data.m_usbCateg;	
	m_kindOfDevice = data.m_kindOfDevice; 
	m_spParentDevice = data.m_spParentDevice;
	m_vectVolumePath = data.m_vectVolumePath;
	m_sGuidOfVolume = data.m_sGuidOfVolume;
	m_sDeviceId = data.m_sDeviceId;
	m_sTypeOfDevice = data.m_sTypeOfDevice;
	m_sVendorOfDevice = data.m_sVendorOfDevice; 
	m_sManufacturer = data.m_sManufacturer;
	m_sProductName = data.m_sProductName; 
	m_sSNOfDevice = data.m_sSNOfDevice;
	m_vectChilds = data.m_vectChilds;
}

USBDevice::USBDevice(USBDevice&& data)
{
	m_iHub = data.m_iHub;
	m_iPort = data.m_iPort;		
	m_usbCateg = data.m_usbCateg;  
	m_kindOfDevice = data.m_kindOfDevice; 
	m_spParentDevice = data.m_spParentDevice;
	m_vectVolumePath = std::move(data.m_vectVolumePath);
	m_sGuidOfVolume = std::move(data.m_sGuidOfVolume);
	m_sDeviceId = std::move(data.m_sDeviceId);
	m_sTypeOfDevice = std::move(data.m_sTypeOfDevice);
	m_sVendorOfDevice = std::move(data.m_sVendorOfDevice); 
	m_sManufacturer = std::move(data.m_sManufacturer);
	m_sProductName = std::move(data.m_sProductName); 
	m_sSNOfDevice = std::move(data.m_sSNOfDevice);
	m_vectChilds = std::move(data.m_vectChilds);
}

USBDevice::USBDevice(int iHub, int iPort, bool bIsHub, USB_ENGINE_CATEGORY usbCateg, const std::string& sTypeOfDevice)
	: m_iHub(iHub), m_iPort(iPort), m_kindOfDevice(USB_DEVICE_KIND::UNKNOWN),
	m_usbCateg(usbCateg), m_sTypeOfDevice(sTypeOfDevice)
{
}

USBDevice::USBDevice(int iHub, int iPort, bool bIsHub, USB_ENGINE_CATEGORY usbCateg,
	const std::string& sTypeOfDevice, const std::string& sDeviceId, const std::string& sVendor, const std::string& sProduct)
	: m_iHub(iHub), m_iPort(iPort), m_kindOfDevice(USB_DEVICE_KIND::UNKNOWN),
	m_usbCateg(usbCateg), m_sDeviceId(sDeviceId),
	m_sTypeOfDevice(sTypeOfDevice), m_sVendorOfDevice(sVendor), m_sProductName(sProduct)
{
}

USBDevice::~USBDevice()
{
	ClearData();	
}

void USBDevice::ClearData()
{
	m_iHub = 0;
	m_iPort = 0;
	
	m_usbCateg = USB_ENGINE_CATEGORY::GLOBAL; 
	m_kindOfDevice = USB_DEVICE_KIND::UNKNOWN;
	
	m_sTypeOfDevice = "";
	m_sVendorOfDevice = "";  
	m_sManufacturer = "";
	m_sProductName = ""; 
	m_sSNOfDevice = ""; 
	m_sGuidOfVolume = ""; 
	m_sDeviceId = "";

	m_vectVolumePath.clear();
	m_vectChilds.clear();
}

void USBDevice::SetDeviceKind(USB_DEVICE_KIND kindOfDevice)
{
	m_kindOfDevice = kindOfDevice;
}

USB_DEVICE_KIND USBDevice::GetDeviceKind() const
{
	return m_kindOfDevice;
}

void USBDevice::SetVectorVolumePath(const PATH_VECTOR& vectVolumePath)
{
	m_vectVolumePath = vectVolumePath;
}

PATH_VECTOR& USBDevice::GetVectorVolumePath() const
{
	return (PATH_VECTOR&)m_vectVolumePath;
}

std::string USBDevice::GetVolumePathString() const
{
	if (m_vectVolumePath.size() == 0)
		return "";

	return m_vectVolumePath[0];
}

std::string USBDevice::GetGuidOfVolume() const
{
	return m_sGuidOfVolume;
}

void USBDevice::SetGuidOfVolume(const std::string& sGuidOfVolume)
{
	m_sGuidOfVolume = sGuidOfVolume;
}

std::string USBDevice::GetDeviceId() const
{
	return m_sDeviceId;
}

void USBDevice::SetDeviceId(const std::string& sDeviceId)
{
	m_sDeviceId = sDeviceId;
}

std::string USBDevice::GetSNOfDevice() const
{
	return m_sSNOfDevice;
}

void USBDevice::SetSNOfDevice(const std::string& sSNOfDevice)
{
	m_sSNOfDevice = sSNOfDevice;
}

std::string USBDevice::GetVendorOfDevice() const
{
	return m_sVendorOfDevice;
}

void USBDevice::SetVendorOfDevice(const std::string& sVendorOfDevice)
{
	m_sVendorOfDevice = sVendorOfDevice;
}

std::string USBDevice::GetManufacturer() const
{
	return m_sManufacturer;
}

void USBDevice::SetManufacturer(const std::string& sManufacturer)
{
	m_sManufacturer = sManufacturer;
}

std::string USBDevice::GetProductName() const
{
	return m_sProductName;
}

void USBDevice::SetProductName(const std::string& sProductName)
{
	m_sProductName = sProductName;
}

void USBDevice::AddChild(SP_USB_DEV spData)
{		
	m_vectChilds.push_back(spData);
}

INDEX_OF_PORT USBDevice::GetPortIndex() const
{
	INDEX_OF_PORT nIPortIndex = m_iPort * 1000 + (m_iHub % 1000);
	return nIPortIndex;
}

int USBDevice::GetParentHub() const
{
	return m_iHub;
}

int USBDevice::GetPort() const
{
	return m_iPort;
}

USB_ENGINE_CATEGORY USBDevice::GetUSBCategory() const
{
	return m_usbCateg;
}

std::string USBDevice::GetTypeOfDevice() const
{
	return m_sTypeOfDevice;
}

VECT_SP_USB_DEV& USBDevice::GetVectChilds() const
{
	return (VECT_SP_USB_DEV&)m_vectChilds;
}