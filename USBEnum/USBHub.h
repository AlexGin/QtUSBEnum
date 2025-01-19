#pragma once
#include "IUSBDevice.h"

namespace USBEnum
{
	class USBHub : public USBEnum::IUSBDevice
	{
	public:
		USBHub();
		USBHub(const USBHub& data); 
		USBHub(USBHub&& data); 
		USBHub(int iParentHub, const std::string& sTypeOfDevice, bool bIsRootHub = false);
		USBHub(int iParentHub, const std::string& sTypeOfDevice,
			const std::string& sDeviceId, const std::string& sVendor, const std::string& sProduct, bool bIsRootHub = false);
		virtual ~USBHub();
	public:
		void AddChild(SP_USB_DEV spDev) override;
		void ClearData() override;
	public:
		int GetParentHub() const override;
		int GetPort() const override
		{
			return 0; // (-1); // No port present
		}		
	public:
		USB_ENGINE_CATEGORY GetUSBCategory() const override
		{
			return USB_ENGINE_CATEGORY::USB_HUB;
		}		
		void SetVectorVolumePath(const PATH_VECTOR&) override {}; 
		PATH_VECTOR& GetVectorVolumePath() const override
		{			
			return (PATH_VECTOR&)m_vect; // Empty collection
		}
		std::string GetVolumePathString() const override
		{
			return "";
		}
		std::string GetGuidOfVolume() const override
		{
			return "";
		}
		void SetGuidOfVolume(const std::string& sGuidOfVolume) override {};
		std::string GetDeviceId() const override;
		void SetDeviceId(const std::string& sDeviceId) override;
		std::string GetTypeOfDevice() const override;
		std::string GetVendorOfDevice() const override;
		void SetVendorOfDevice(const std::string& sVendorOfDevice) override;
		std::string GetManufacturer() const override;
		void SetManufacturer(const std::string& sManufacturer) override;
		std::string GetProductName() const override;
		void SetProductName(const std::string& sProductName) override;
		virtual void SetDeviceKind(USB_DEVICE_KIND kindOfDevice) override {};
		virtual USB_DEVICE_KIND GetDeviceKind() const override
		{
			return USB_DEVICE_KIND::UNKNOWN;
		}
	public:
		std::string GetSNOfDevice() const override
		{
			return "";
		}
		void SetSNOfDevice(const std::string& sSNOfDevice) override {};
		VECT_SP_USB_DEV& GetVectChilds() const override;
		INDEX_OF_PORT GetPortIndex() const override
		{
			return 0;
		}
	public:
		bool GetIsRootHub() const
		{
			return m_bIsRootHub;
		}
	private:
		int m_iParentHub;		// Identyfier of the PARENT Hub or Host-Controller (for RootHub devices)
		bool m_bIsRootHub; 
		PATH_VECTOR m_vect;				// Empty collection
		std::string m_sDeviceId;		// USBDEVICEINFO.UsbDeviceProperties.DeviceId - last section
		std::string m_sTypeOfDevice;	// Type of USB Device
		std::string m_sVendorOfDevice;	// Vendor's name  
		std::string m_sManufacturer;	// Manufacturer
		std::string m_sProductName;		// Name of particular product
		VECT_SP_USB_DEV m_vectChilds;	// Vector with childs (devices, which conect to this device)
	};
}