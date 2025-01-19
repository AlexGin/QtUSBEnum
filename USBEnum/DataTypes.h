#pragma once

#include <windows.h>
#include <windowsx.h>
#include <initguid.h>
#include <devioctl.h>
#include <dbt.h>
#include <commctrl.h>
#include <usbioctl.h>
#include <usbiodef.h>
#include <usb.h>
#include <usbuser.h>
#include <setupapi.h>
#include <winioctl.h>
#include <string>
#include <tuple>
/*****************************************************************************
 T Y P E D E F S
*****************************************************************************/
using USB_ENUM_SETTINGS = std::tuple<int, int, bool>; // int nRootHubBaseOffset, int nExtHubBaseOffset, bool bExtHubUseDeviceAddr
using LOG_FILE_SETTINGS = std::tuple<std::string, bool, int>; // std::string strLogFilePath, bool nLogAutoRemoveEnable, int nLogRemoveDays
using MONITORING_THREAD_SETTINGS = std::tuple<bool, UINT, UINT, bool>; 
// bool bMonitoringThreadEnable, uint nPoolingWaitTimeMs, uint nStopWaitTimeMs, bool bSaveTimeLogEnable

typedef enum _TREEICON
{
    ComputerIcon,
    HubIcon,
    NoDeviceIcon,
    GoodDeviceIcon,
    BadDeviceIcon,
    GoodSsDeviceIcon,
    NoSsDeviceIcon
} TREEICON;

// Callback function for walking TreeView items
//
typedef VOID
(*LPFNTREECALLBACK)(
    HWND        hTreeWnd,
    HTREEITEM   hTreeItem,
    PVOID       pContext
);


// Callback notification function called at end of every tree depth
typedef VOID
(*LPFNTREENOTIFYCALLBACK)(PVOID pContext);

//
// Structure used to build a linked list of String Descriptors
// retrieved from a device.
//

typedef struct _STRING_DESCRIPTOR_NODE
{
    struct _STRING_DESCRIPTOR_NODE *Next;
    UCHAR                           DescriptorIndex;
    USHORT                          LanguageID;
    USB_STRING_DESCRIPTOR           StringDescriptor[1];
} STRING_DESCRIPTOR_NODE, *PSTRING_DESCRIPTOR_NODE;

//
// A collection of device properties. The device can be hub, host controller or usb device
//
typedef struct _USB_DEVICE_PNP_STRINGS
{
    PCHAR DeviceId;
    PCHAR DeviceDesc;
    PCHAR HwId;
    PCHAR Service;
    PCHAR DeviceClass;
    PCHAR PowerState;
} USB_DEVICE_PNP_STRINGS, *PUSB_DEVICE_PNP_STRINGS;

typedef struct _DEVICE_INFO_NODE {
    HDEVINFO							DeviceInfo;
    LIST_ENTRY							ListEntry;
    SP_DEVINFO_DATA						DeviceInfoData;
    SP_DEVICE_INTERFACE_DATA			DeviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA_A	DeviceDetailData;
    PSTR								DeviceDescName;
    ULONG								DeviceDescNameLength;
    PSTR								DeviceDriverName;
    ULONG								DeviceDriverNameLength;
    DEVICE_POWER_STATE					LatestDevicePowerState;
} DEVICE_INFO_NODE, *PDEVICE_INFO_NODE;

//
// Structures assocated with TreeView items through the lParam.  When an item
// is selected, the lParam is retrieved and the structure it which it points
// is used to display information in the edit control.
//

typedef enum _USBDEVICEINFOTYPE
{
    HostControllerInfo,
    RootHubInfo,
    ExternalHubInfo,
    DeviceInfo
} USBDEVICEINFOTYPE, *PUSBDEVICEINFOTYPE;

typedef struct _USBHOSTCONTROLLERINFO
{
    USBDEVICEINFOTYPE                   DeviceInfoType;
    LIST_ENTRY                          ListEntry;
    PCHAR                               DriverKey;
    ULONG                               VendorID;
    ULONG                               DeviceID;
    ULONG                               SubSysID;
    ULONG                               Revision;
    USB_POWER_INFO                      USBPowerInfo[6];
    BOOL                                BusDeviceFunctionValid;
    ULONG                               BusNumber;
    USHORT                              BusDevice;
    USHORT                              BusFunction;
    PUSB_CONTROLLER_INFO_0              ControllerInfo;
    PUSB_DEVICE_PNP_STRINGS             UsbDeviceProperties;
} USBHOSTCONTROLLERINFO, *PUSBHOSTCONTROLLERINFO;

typedef struct _USBROOTHUBINFO
{
    USBDEVICEINFOTYPE                   DeviceInfoType;
    PUSB_NODE_INFORMATION               HubInfo;
    PUSB_HUB_INFORMATION_EX             HubInfoEx;
    PCHAR                               HubName;
    PUSB_PORT_CONNECTOR_PROPERTIES      PortConnectorProps;
    PUSB_DEVICE_PNP_STRINGS             UsbDeviceProperties;
    PDEVICE_INFO_NODE                   DeviceInfoNode;
    PUSB_HUB_CAPABILITIES_EX            HubCapabilityEx;

} USBROOTHUBINFO, *PUSBROOTHUBINFO;

typedef struct _USBEXTERNALHUBINFO
{
    USBDEVICEINFOTYPE                      DeviceInfoType;
    PUSB_NODE_INFORMATION                  HubInfo;
    PUSB_HUB_INFORMATION_EX                HubInfoEx;
    PCHAR                                  HubName;
    PUSB_NODE_CONNECTION_INFORMATION_EX    ConnectionInfo;
    PUSB_PORT_CONNECTOR_PROPERTIES         PortConnectorProps;
    PUSB_DESCRIPTOR_REQUEST                ConfigDesc;
    PUSB_DESCRIPTOR_REQUEST                BosDesc;
    PSTRING_DESCRIPTOR_NODE                StringDescs;
    PUSB_NODE_CONNECTION_INFORMATION_EX_V2 ConnectionInfoV2; // NULL if root HUB
    PUSB_DEVICE_PNP_STRINGS                UsbDeviceProperties;
    PDEVICE_INFO_NODE                      DeviceInfoNode;
    PUSB_HUB_CAPABILITIES_EX               HubCapabilityEx;
} USBEXTERNALHUBINFO, *PUSBEXTERNALHUBINFO;


// HubInfo, HubName may be in USBDEVICEINFOTYPE, so they can be removed
typedef struct
{
    USBDEVICEINFOTYPE                      DeviceInfoType;
    PUSB_NODE_INFORMATION                  HubInfo;          // NULL if not a HUB
    PUSB_HUB_INFORMATION_EX                HubInfoEx;        // NULL if not a HUB
    PCHAR                                  HubName;          // NULL if not a HUB
    PUSB_NODE_CONNECTION_INFORMATION_EX    ConnectionInfo;   // NULL if root HUB
    PUSB_PORT_CONNECTOR_PROPERTIES         PortConnectorProps;
    PUSB_DESCRIPTOR_REQUEST                ConfigDesc;       // NULL if root HUB
    PUSB_DESCRIPTOR_REQUEST                BosDesc;          // NULL if root HUB
    PSTRING_DESCRIPTOR_NODE                StringDescs;
    PUSB_NODE_CONNECTION_INFORMATION_EX_V2 ConnectionInfoV2; // NULL if root HUB
    PUSB_DEVICE_PNP_STRINGS                UsbDeviceProperties;
    PDEVICE_INFO_NODE                      DeviceInfoNode;
    PUSB_HUB_CAPABILITIES_EX               HubCapabilityEx;  // NULL if not a HUB
} USBDEVICEINFO, *PUSBDEVICEINFO;

typedef struct _STRINGLIST
{
#ifdef H264_SUPPORT
    ULONGLONG       ulFlag;
#else
    ULONG           ulFlag;
#endif
    PCHAR     pszString;
    PCHAR     pszModifier;

} STRINGLIST, * PSTRINGLIST;

typedef struct _DEVICE_GUID_LIST {
    HDEVINFO   DeviceInfo;
    LIST_ENTRY ListHead;
} DEVICE_GUID_LIST, *PDEVICE_GUID_LIST;

