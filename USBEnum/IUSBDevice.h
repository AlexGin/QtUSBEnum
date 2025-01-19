#pragma once

#include <vector>
#include <string>
#include <memory>

enum class USB_ENGINE_CATEGORY
{	
	GLOBAL = 0,			// It's "My Computer" (monitor)
	USB_HUB,			// Root Hub or other Hub
	USB_BAD_DEVICE,		// USB bad-device
	USB_GOOD_DEVICE,	// USB good-device
	USB_NO_DEVICE,		// USB Port (without any device)
	USB_NO_SS_DEVICE,	// USB Super-speed Port
	USB_SS_GOOD_DEVICE,	// USB Super-speed good-device 
	USB_HOST_DEVICE		// USB Host Controller
};

enum class USB_DEVICE_KIND
{
	UNKNOWN = 0,
	KIND_MEMORY,	// Memory (disk) kind of device 
	KIND_COMPOSITE,	// Composite kind of device 
	KIND_HID		// Human Interface Device kind of device
};

using INDEX_OF_PORT = unsigned __int64; // Using as: nPort * 1000 + nHub
using PATH_VECTOR = std::vector<std::string>;

namespace USBEnum
{
	class IUSBDevice
	{
	public:
		virtual ~IUSBDevice() { int m = 0; };
	public:
		virtual void AddChild(std::shared_ptr<USBEnum::IUSBDevice> spData) = 0;
		virtual void ClearData() = 0;
	public:
		virtual int GetParentHub() const = 0;
		virtual int GetPort() const = 0;

	public:		
		virtual void SetVectorVolumePath(const PATH_VECTOR&) = 0;
		virtual std::string GetVolumePathString() const = 0;
		virtual std::string GetGuidOfVolume() const = 0; 
		virtual PATH_VECTOR& GetVectorVolumePath() const = 0;
		virtual void SetGuidOfVolume(const std::string& sGuidOfVolume) = 0; 

	public:
		virtual std::string GetDeviceId() const = 0;
		virtual void SetDeviceId(const std::string& sDeviceId) = 0;
		virtual std::string GetTypeOfDevice() const = 0;
		virtual std::string GetVendorOfDevice() const = 0;
		virtual void SetVendorOfDevice(const std::string& sVendorOfDevice) = 0;
		virtual std::string GetManufacturer() const = 0;
		virtual void SetManufacturer(const std::string& sManufacturer) = 0;
		virtual std::string GetProductName() const = 0;
		virtual void SetProductName(const std::string& sProductName) = 0;
	public:
		virtual USB_ENGINE_CATEGORY GetUSBCategory() const = 0;
		virtual void SetDeviceKind(USB_DEVICE_KIND kindOfDevice) = 0;
		virtual USB_DEVICE_KIND GetDeviceKind() const = 0;
		// virtual void SetParentDevice(std::shared_ptr<USBEnum::IUSBDevice> spParentDevice) = 0;
		// virtual std::shared_ptr<USBEnum::IUSBDevice> GetParentDevice() const = 0;
	public:
		virtual std::string GetSNOfDevice() const = 0;
		virtual void SetSNOfDevice(const std::string& sSNOfDevice) = 0;
		virtual std::vector<std::shared_ptr<USBEnum::IUSBDevice>>& GetVectChilds() const = 0;
		virtual INDEX_OF_PORT GetPortIndex() const = 0;
	};
}
using SP_USB_DEV = std::shared_ptr<USBEnum::IUSBDevice>;
using VECT_SP_USB_DEV = std::vector<std::shared_ptr<USBEnum::IUSBDevice>>; // In contemporary MSVC no need extra space: "> >"