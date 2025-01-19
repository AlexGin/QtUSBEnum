#pragma once

#include "USBEnum\DataTypes.h"
#include "USBEnum\USBDevice.h"
#include "USBEnum\USBHub.h"
#include "USBEnum\UsbVolume.h"

#define NUM_STRING_DESC_TO_GET 32
#define MAX_DRIVER_KEY_NAME 256

//#ifdef  _DEBUG
//#undef  DBG
//#define DBG 1
//#endif
//
//#if DBG
//#define OOPS() Oops(__FILE__, __LINE__)
//#else
//#define  OOPS()
//#endif

#define IsListEmpty(ListHead) \
    ((ListHead)->Flink == (ListHead))

class CEnumerator
{
public:
	CEnumerator(SP_USB_VOL spUsbVolume, const USB_ENUM_SETTINGS& usbEnumSettings);
private:
	ULONG m_nTotalDevicesConnected; 
	// int m_nHubIndex; 
	SP_USB_VOL m_spUsbVolume; 
private: // Settings (from the file "CFGUSBEnum.ini"):
	USB_ENUM_SETTINGS m_usbEnumSettings;
	int GetRootHubBaseOffset() const
	{
		return std::get<0>(m_usbEnumSettings);
	}
	int GetExtHubBaseOffset() const
	{
		return std::get<1>(m_usbEnumSettings);
	}
	bool GetIsExtHubUseDeviceAddr() const
	{
		return std::get<2>(m_usbEnumSettings);
	}
public:
	void EnumerateHostControllers(SP_USB_DEV spTreeParent, ULONG* DevicesConnected);
	void EnumerateHostController(SP_USB_DEV spTreeParent, HANDLE hHCDev,
		_Inout_ PCHAR leafName, _In_ HANDLE deviceInfo, _In_ PSP_DEVINFO_DATA deviceInfoData);
	void EnumerateHub(SP_USB_DEV spTreeParent,
		_In_reads_(cbHubName) PCHAR HubName, _In_ size_t cbHubName,
		_In_opt_ PUSB_NODE_CONNECTION_INFORMATION_EX    ConnectionInfo,
		_In_opt_ PUSB_NODE_CONNECTION_INFORMATION_EX_V2 ConnectionInfoV2,
		_In_opt_ PUSB_PORT_CONNECTOR_PROPERTIES         PortConnectorProps,
		_In_opt_ PUSB_DESCRIPTOR_REQUEST                ConfigDesc,
		_In_opt_ PUSB_DESCRIPTOR_REQUEST                BosDesc,
		_In_opt_ PSTRING_DESCRIPTOR_NODE                StringDescs,
		_In_opt_ PUSB_DEVICE_PNP_STRINGS                DevProps);
	void EnumerateHubPorts(SP_USB_DEV spTreeParent, HANDLE hHubDevice, ULONG NumPorts);  
	SP_USB_DEV AddLeaf(SP_USB_DEV spTreeParent, LPARAM lParam, _In_ LPTSTR lpszText, 
		USB_ENGINE_CATEGORY usbCateg, bool bIsHub = false);
	PCHAR GetRootHubName(HANDLE HostController);
	PCHAR GetExternalHubName(HANDLE  Hub, ULONG ConnectionIndex);
	PCHAR GetHCDDriverKeyName(HANDLE  HCD);
	PCHAR GetDriverKeyName(HANDLE  Hub, ULONG ConnectionIndex);
	PUSB_DESCRIPTOR_REQUEST GetConfigDescriptor(HANDLE hHubDevice, ULONG ConnectionIndex, UCHAR DescriptorIndex);
	PUSB_DESCRIPTOR_REQUEST GetBOSDescriptor(HANDLE  hHubDevice, ULONG   ConnectionIndex);
	DWORD GetHostControllerPowerMap(HANDLE hHCDev, PUSBHOSTCONTROLLERINFO hcInfo);
	DWORD GetHostControllerInfo(HANDLE hHCDev, PUSBHOSTCONTROLLERINFO hcInfo); 
	const char* GetVendorString(USHORT idVendor);
private:
	PCHAR WideStrToMultiStr( _In_reads_bytes_(cbWideStr) PWCHAR WideStr, _In_ size_t cbWideStr);
	BOOL AreThereStringDescriptors(PUSB_DEVICE_DESCRIPTOR DeviceDesc, PUSB_CONFIGURATION_DESCRIPTOR ConfigDesc);

	PSTRING_DESCRIPTOR_NODE GetAllStringDescriptors(HANDLE hHubDevice, ULONG ConnectionIndex,
		PUSB_DEVICE_DESCRIPTOR DeviceDesc, PUSB_CONFIGURATION_DESCRIPTOR ConfigDesc);

	PSTRING_DESCRIPTOR_NODE GetStringDescriptor(HANDLE  hHubDevice, ULONG ConnectionIndex, UCHAR DescriptorIndex, USHORT LanguageID);

	HRESULT GetStringDescriptors(_In_ HANDLE hHubDevice,
		_In_ ULONG ConnectionIndex, _In_ UCHAR DescriptorIndex,
		_In_ ULONG NumLanguageIDs, _In_reads_(NumLanguageIDs) USHORT  *LanguageIDs,
		_In_ PSTRING_DESCRIPTOR_NODE StringDescNodeHead);

	void EnumerateAllDevices();
	void EnumerateAllDevicesWithGuid(PDEVICE_GUID_LIST DeviceList, LPGUID Guid );
		
	PDEVICE_INFO_NODE FindMatchingDeviceNodeForDriverName(_In_ PSTR DriverKeyName, _In_ BOOLEAN IsHub); 
	USB_DEVICE_KIND PrepareKindOfDevice(bool bIsMemoryDevice, bool bIsCompositeDevice, bool bIsHIDUsbDevice);
private:
	void Oops(const char* szFile, UINT nLine);
public:
	static void FreeDeviceInfoNode(_In_ PDEVICE_INFO_NODE *ppNode); 
	static void ClearDeviceList(PDEVICE_GUID_LIST DeviceList);
	static std::string ConvertDeviceId(const std::string& sRawDeviceId);  
	static std::string ConvertHostId(ULONG nHostID);
	static std::string RetrieveUSEnglishStringDescriptor(UCHAR Index, PSTRING_DESCRIPTOR_NODE USStringDescs);
public:
	static int g_nTotalHubs; 
	static DEVICE_GUID_LIST g_lsHubList;
	static DEVICE_GUID_LIST g_lsDeviceList;  
	static const char* ConnectionStatuses[]; 
	static BOOL g_bDoConfigDesc;
};
