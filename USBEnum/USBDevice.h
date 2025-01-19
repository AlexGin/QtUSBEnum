#pragma once

#include "USBEnum\IUSBDevice.h"

#include <map>
#include <vector>
#include <string> 
#include <sstream>
#include <memory>

namespace USBEnum
{
	class USBDevice : public USBEnum::IUSBDevice
	{
	public:
		USBDevice();
		USBDevice(const USBDevice& data);
		USBDevice(USBDevice&& data);
		USBDevice(int iHub, int iPort, bool bIsHub, USB_ENGINE_CATEGORY usbCateg, const std::string& sTypeOfDevice);
		USBDevice(int iHub, int iPort, bool bIsHub, USB_ENGINE_CATEGORY usbCateg,
			const std::string& sTypeOfDevice, const std::string& sDeviceId, const std::string& sVendor, const std::string& sProduct);
		virtual ~USBDevice();
	public:
		void AddChild(SP_USB_DEV spDev) override;
		void ClearData() override;
	public:
		int GetParentHub() const override;
		int GetPort() const override; 
		// void SetParentDevice(SP_USB_DATA spParentDevice) override;
		// SP_USB_DATA GetParentDevice() const override;
	public:	
		USB_ENGINE_CATEGORY GetUSBCategory() const override;
		void SetVectorVolumePath(const PATH_VECTOR& vectVolumePath) override; 
		PATH_VECTOR& GetVectorVolumePath() const override;
		std::string GetVolumePathString() const override;
		std::string GetGuidOfVolume() const override;
		void SetGuidOfVolume(const std::string& sGuidOfVolume) override;
		std::string GetDeviceId() const override;
		void SetDeviceId(const std::string& sDeviceId) override;
		std::string GetTypeOfDevice() const override;
		std::string GetVendorOfDevice() const override;
		void SetVendorOfDevice(const std::string& sVendorOfDevice) override;
		std::string GetManufacturer() const override;
		void SetManufacturer(const std::string& sManufacturer) override;
		std::string GetProductName() const override;
		void SetProductName(const std::string& sProductName) override;
		virtual void SetDeviceKind(USB_DEVICE_KIND kindOfDevice) override;
		virtual USB_DEVICE_KIND GetDeviceKind() const override;
	public:	
		std::string GetSNOfDevice() const override;
		void SetSNOfDevice(const std::string& sSNOfDevice) override;
		VECT_SP_USB_DEV& GetVectChilds() const override;
		INDEX_OF_PORT GetPortIndex() const override;
	private:
		int m_iHub;		// Identyfier of the PARENT Hub
		int m_iPort;	// Identyfier of the current Port
		USB_DEVICE_KIND m_kindOfDevice;
		USB_ENGINE_CATEGORY m_usbCateg; 
		SP_USB_DEV m_spParentDevice;	// Usually: pointer to the Parent-HUB 
		// std::string m_sVolumePathString; // Letter of volume (of drive) for example: "I:","J:","K:","H:" or path: "D:\test1" 
		PATH_VECTOR m_vectVolumePath;   // Using instead of m_sVolumePathString (main path - as a value from: m_vectVolumePath[0])
		std::string m_sGuidOfVolume;	// GUID of volume (for example: "{55fdafa3-45a0-11e9-a68c-309c23cd6ea9}")
		std::string m_sDeviceId;		// USBDEVICEINFO.UsbDeviceProperties.DeviceId - last section (Serial Number of particular USB Device)
		std::string m_sTypeOfDevice;	// Type of USB Device 	
		std::string m_sVendorOfDevice;	// Vendor's name (for example: "Silicon Motion, Inc. - Taiwan") 
		std::string m_sManufacturer;	// Manufacturer, or additional info about device (for example: "USB 2.0")
		std::string m_sProductName;		// Name of particular product (for example: "Silicon-Power8G")	
		std::string m_sSNOfDevice;		// Serial Number of particular USB Device (for example: "1601809941515142")
		VECT_SP_USB_DEV m_vectChilds;	// Vector with childs (devices, which conect to this device)
	};
}
