#include "USBHub.h"

using namespace USBEnum;

USBHub::USBHub()
	: m_iParentHub(0), m_bIsRootHub(false)
{
}

USBHub::USBHub(const USBHub& data)
{
	m_iParentHub = data.m_iParentHub;
	m_bIsRootHub = data.m_bIsRootHub;
	m_sDeviceId = data.m_sDeviceId;
	m_sTypeOfDevice = data.m_sTypeOfDevice;
	m_sVendorOfDevice = data.m_sVendorOfDevice;
	m_sManufacturer = data.m_sManufacturer;
	m_sProductName = data.m_sProductName; 
	m_vectChilds = data.m_vectChilds;
}

USBHub::USBHub(USBHub&& data)
{
	m_iParentHub = data.m_iParentHub;
	m_bIsRootHub = data.m_bIsRootHub;
	m_sDeviceId = std::move(data.m_sDeviceId);
	m_sTypeOfDevice = std::move(data.m_sTypeOfDevice);
	m_sVendorOfDevice = std::move(data.m_sVendorOfDevice);
	m_sManufacturer = std::move(data.m_sManufacturer);
	m_sProductName = std::move(data.m_sProductName); 
	m_vectChilds = std::move(data.m_vectChilds);
}

USBHub::USBHub(int iParentHub, const std::string& sTypeOfDevice, bool bIsRootHub)
	: m_iParentHub(iParentHub), m_sTypeOfDevice(sTypeOfDevice), m_bIsRootHub(bIsRootHub)
{
}

USBHub::USBHub(int iParentHub, const std::string& sTypeOfDevice,
	const std::string& sDeviceId, const std::string& sVendor, const std::string& sProduct, bool bIsRootHub)
	: m_iParentHub(iParentHub), m_sDeviceId(sDeviceId), m_sTypeOfDevice(sTypeOfDevice), 
	m_sVendorOfDevice(sVendor), m_sProductName(sProduct), m_bIsRootHub(bIsRootHub)
{
}

USBHub::~USBHub()
{
	ClearData();
}

void USBHub::ClearData()
{
	m_iParentHub = 0;

	m_sTypeOfDevice = "";
	m_sVendorOfDevice = "";
	m_sManufacturer = "";
	m_sProductName = "";
	m_sDeviceId = "";

	m_vectChilds.clear();
}

//void USBHub::SetParentDevice(SP_USB_DATA spParentDevice)
//{
//	m_spParentDevice = spParentDevice;
//}
//
//SP_USB_DATA USBHub::GetParentDevice() const
//{
//	return m_spParentDevice;   // Usually: pointer to the Parent-HUB 
//}

int USBHub::GetParentHub() const
{
	return m_iParentHub;
}

std::string USBHub::GetDeviceId() const
{
	return m_sDeviceId;
}

void USBHub::SetDeviceId(const std::string& sDeviceId)
{
	m_sDeviceId = sDeviceId;
}

std::string USBHub::GetVendorOfDevice() const
{
	return m_sVendorOfDevice;
}

void USBHub::SetVendorOfDevice(const std::string& sVendorOfDevice)
{
	m_sVendorOfDevice = sVendorOfDevice;
}

std::string USBHub::GetManufacturer() const
{
	return m_sManufacturer;
}

void USBHub::SetManufacturer(const std::string& sManufacturer)
{
	m_sManufacturer = sManufacturer;
}

std::string USBHub::GetProductName() const
{
	return m_sProductName;
}

void USBHub::SetProductName(const std::string& sProductName)
{
	m_sProductName = sProductName;
}

std::string USBHub::GetTypeOfDevice() const
{
	return m_sTypeOfDevice;
}

void USBHub::AddChild(SP_USB_DEV spDev)
{
	m_vectChilds.push_back(spDev);
}

VECT_SP_USB_DEV& USBHub::GetVectChilds() const
{
	return (VECT_SP_USB_DEV&)m_vectChilds;
}