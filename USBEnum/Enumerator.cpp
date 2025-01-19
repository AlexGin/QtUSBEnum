#include <strsafe.h>

#include "USBEnum\Enumerator.h"
#include "USBEnum\DevNode.h"
#include "USBEnum\USBDesc.h"
#include "USBEnum\vndrlist.h"
#include "LogFile.h"

extern CLogFile g_log;
extern void Free(_Frees_ptr_opt_ HGLOBAL hMem);

GUID g_guidUsbDevice =
{ 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } }; 

#define ALLOC(dwBytes) GlobalAlloc(GPTR,(dwBytes))

#define MAX_DEVICE_PROP 200

#define RemoveHeadList(ListHead) \
    (ListHead)->Flink;\
    {RemoveEntryList((ListHead)->Flink)}

#define RemoveEntryList(Entry) {\
    PLIST_ENTRY _EX_Blink;\
    PLIST_ENTRY _EX_Flink;\
    _EX_Flink = (Entry)->Flink;\
    _EX_Blink = (Entry)->Blink;\
    _EX_Blink->Flink = _EX_Flink;\
    _EX_Flink->Blink = _EX_Blink;\
    }

#define InsertTailList(ListHead,Entry) {\
    PLIST_ENTRY _EX_Blink;\
    PLIST_ENTRY _EX_ListHead;\
    _EX_ListHead = (ListHead);\
    _EX_Blink = _EX_ListHead->Blink;\
    (Entry)->Flink = _EX_ListHead;\
    (Entry)->Blink = _EX_Blink;\
    _EX_Blink->Flink = (Entry);\
    _EX_ListHead->Blink = (Entry);\
    }

int CEnumerator::g_nTotalHubs; 
DEVICE_GUID_LIST CEnumerator::g_lsHubList;
DEVICE_GUID_LIST CEnumerator::g_lsDeviceList;

/*****************************************************************************

Oops()

*****************************************************************************/

void CEnumerator::Oops(const char* szFile, UINT nLine) // Added 07.04.2019
{
	DWORD dwGLE = GetLastError();
	g_log.SaveLogFile(LOG_MODES::ERRORLOG, "Oops: File: %s; Line: %u (Error code = %u)", szFile, nLine, dwGLE);
}

// List of enumerated host controllers.
//
LIST_ENTRY EnumeratedHCListHead =
{
	&EnumeratedHCListHead,
	&EnumeratedHCListHead
};

const char* CEnumerator::ConnectionStatuses[] =
{
	"",                   // 0  - NoDeviceConnected
	"",                   // 1  - DeviceConnected
	"FailedEnumeration",  // 2  - DeviceFailedEnumeration
	"GeneralFailure",     // 3  - DeviceGeneralFailure
	"Overcurrent",        // 4  - DeviceCausedOvercurrent
	"NotEnoughPower",     // 5  - DeviceNotEnoughPower
	"NotEnoughBandwidth", // 6  - DeviceNotEnoughBandwidth
	"HubNestedTooDeeply", // 7  - DeviceHubNestedTooDeeply
	"InLegacyHub",        // 8  - DeviceInLegacyHub
	"Enumerating",        // 9  - DeviceEnumerating
	"Reset"               // 10 - DeviceReset
};

///////////////////////////////////////////////////////////////////////
BOOL CEnumerator::g_bDoConfigDesc;

CEnumerator::CEnumerator(SP_USB_VOL spUsbVolume, const USB_ENUM_SETTINGS& usbEnumSettings)
	: m_nTotalDevicesConnected(0), m_spUsbVolume(spUsbVolume), m_usbEnumSettings(usbEnumSettings)
{
	g_nTotalHubs = 0;	
	CEnumerator::g_bDoConfigDesc = TRUE;
}

void CEnumerator::ClearDeviceList(PDEVICE_GUID_LIST DeviceList)
{
	if (DeviceList->DeviceInfo != INVALID_HANDLE_VALUE)
	{
		SetupDiDestroyDeviceInfoList(DeviceList->DeviceInfo);
		DeviceList->DeviceInfo = INVALID_HANDLE_VALUE;
	}

	while (!IsListEmpty(&DeviceList->ListHead))
	{
		PDEVICE_INFO_NODE pNode = NULL;
		PLIST_ENTRY pEntry;

		pEntry = RemoveHeadList(&DeviceList->ListHead);

		pNode = CONTAINING_RECORD(pEntry,
			DEVICE_INFO_NODE,
			ListEntry);

		FreeDeviceInfoNode(&pNode);
	}
}

//*****************************************************************************
//
// EnumerateHostControllers()
//
// hTreeParent - Handle of the TreeView item under which host controllers
// should be added.
//
//*****************************************************************************
void CEnumerator::EnumerateHostControllers(SP_USB_DEV spTreeParent, ULONG* DevicesConnected)
{
    HANDLE								hHCDev = NULL;
    HDEVINFO							deviceInfo = NULL;
    SP_DEVINFO_DATA						deviceInfoData;
    SP_DEVICE_INTERFACE_DATA			deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA_A	deviceDetailData = NULL;
    ULONG								index = 0;
    ULONG								requiredLength = 0;
    BOOL								success;

	/* m_nHubIndex = 0; */
	m_nTotalDevicesConnected = 0;
	CEnumerator::g_nTotalHubs = 0;

    EnumerateAllDevices();

    // Iterate over host controllers using the new GUID based interface
    //
    deviceInfo = SetupDiGetClassDevsA((LPGUID)&GUID_CLASS_USB_HOST_CONTROLLER,
                                     NULL,
                                     NULL,
                                     (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (index=0;
         SetupDiEnumDeviceInfo(deviceInfo,
                               index,
                               &deviceInfoData);
         index++)
    {
        deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        success = SetupDiEnumDeviceInterfaces(deviceInfo,
                                              0,
                                              (LPGUID)&GUID_CLASS_USB_HOST_CONTROLLER,
                                              index,
                                              &deviceInterfaceData);

        if (!success)
        {
            Oops(__FILE__, __LINE__);
            break;
        }

        success = SetupDiGetDeviceInterfaceDetailA(deviceInfo,
                                                  &deviceInterfaceData,
                                                  NULL,
                                                  0,
                                                  &requiredLength,
                                                  NULL);

        if (!success && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            Oops(__FILE__, __LINE__);
            break;
        }

        deviceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)ALLOC(requiredLength);
        if (deviceDetailData == NULL)
        {
            Oops(__FILE__, __LINE__);
            break;
        }

        deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        success = SetupDiGetDeviceInterfaceDetailA(deviceInfo,
                                                  &deviceInterfaceData,
                                                  deviceDetailData,
                                                  requiredLength,
                                                  &requiredLength,
                                                  NULL);

        if (!success)
        {
            Oops(__FILE__, __LINE__);
            break;
        }

        hHCDev = CreateFileA(deviceDetailData->DevicePath,
                            GENERIC_WRITE,
                            FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            NULL);

        // If the handle is valid, then we've successfully opened a Host
        // Controller.  Display some info about the Host Controller itself,
        // then enumerate the Root Hub attached to the Host Controller.
        //
        if (hHCDev != INVALID_HANDLE_VALUE)
        {
            EnumerateHostController(spTreeParent,
                                    hHCDev,
                                    deviceDetailData->DevicePath,
                                    deviceInfo,
                                    &deviceInfoData);

            CloseHandle(hHCDev);
        }

		Free(deviceDetailData);
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);

    *DevicesConnected = m_nTotalDevicesConnected;

    return;
}

//*****************************************************************************
//
// EnumerateHostController()
//
// hTreeParent - Handle of the TreeView item under which host controllers
// should be added.
//
//*****************************************************************************
void CEnumerator::EnumerateHostController(SP_USB_DEV spTreeParent, HANDLE hHCDev,
	_Inout_ PCHAR            leafName,
    _In_    HANDLE           deviceInfo,
    _In_    PSP_DEVINFO_DATA deviceInfoData
)
{
    PCHAR                   driverKeyName = NULL;
    HTREEITEM               hHCItem = NULL;
    PCHAR                   rootHubName = NULL;
    PLIST_ENTRY             listEntry = NULL;
    PUSBHOSTCONTROLLERINFO  hcInfo = NULL;
    PUSBHOSTCONTROLLERINFO  hcInfoInList = NULL;
    DWORD                   dwSuccess;
    BOOL                    success = FALSE;
    ULONG                   deviceAndFunction = 0;
    PUSB_DEVICE_PNP_STRINGS DevProps = NULL;


    // Allocate a structure to hold information about this host controller.
    //
    hcInfo = (PUSBHOSTCONTROLLERINFO)ALLOC(sizeof(USBHOSTCONTROLLERINFO));

    // just return if could not alloc memory
    if (NULL == hcInfo)
        return;

    hcInfo->DeviceInfoType = HostControllerInfo;

    // Obtain the driver key name for this host controller.
    //
    driverKeyName = GetHCDDriverKeyName(hHCDev);

    if (NULL == driverKeyName)
    {
        // Failure obtaining driver key name.
        Oops(__FILE__, __LINE__);
		Free(hcInfo);
        return;
    }

    // Don't enumerate this host controller again if it already
    // on the list of enumerated host controllers.
    //
    listEntry = EnumeratedHCListHead.Flink;

    while (listEntry != &EnumeratedHCListHead)
    {
        hcInfoInList = CONTAINING_RECORD(listEntry,
                                         USBHOSTCONTROLLERINFO,
                                         ListEntry);

        if (strcmp(driverKeyName, hcInfoInList->DriverKey) == 0)
        {
            // Already on the list, exit
            //
			Free(driverKeyName);
			Free(hcInfo);
            return;
        }

        listEntry = listEntry->Flink;
    }

    // Obtain host controller device properties
    {
        size_t cbDriverName = 0;
        HRESULT hr = S_OK;

        hr = StringCbLengthA(driverKeyName, MAX_DRIVER_KEY_NAME, &cbDriverName);
        if (SUCCEEDED(hr))
        {
            DevProps = CDevNode::DriverNameToDeviceProperties(driverKeyName, cbDriverName);
        }
    }

    hcInfo->DriverKey = driverKeyName;

    if (DevProps)
    {
        ULONG   ven, dev, subsys, rev;
        ven = dev = subsys = rev = 0;

        if (sscanf_s(DevProps->DeviceId,
                   "PCI\\VEN_%x&DEV_%x&SUBSYS_%x&REV_%x",
                   &ven, &dev, &subsys, &rev) != 4)
        {
            Oops(__FILE__, __LINE__);
        }

        hcInfo->VendorID = ven;
        hcInfo->DeviceID = dev;
        hcInfo->SubSysID = subsys;
        hcInfo->Revision = rev;
        hcInfo->UsbDeviceProperties = DevProps;
    }
    else
    {
        Oops(__FILE__, __LINE__);
    }

    if (DevProps != NULL && DevProps->DeviceDesc != NULL)
    {
        leafName = DevProps->DeviceDesc;
    }
    else
    {
        Oops(__FILE__, __LINE__);
    }

    // Get the USB Host Controller power map
    dwSuccess = GetHostControllerPowerMap(hHCDev, hcInfo);

    if (ERROR_SUCCESS != dwSuccess)
    {
        Oops(__FILE__, __LINE__);
    }

    // Get bus, device, and function
    //
    hcInfo->BusDeviceFunctionValid = FALSE;

    success = SetupDiGetDeviceRegistryPropertyA(deviceInfo,
                                               deviceInfoData,
                                               SPDRP_BUSNUMBER,
                                               NULL,
                                               (PBYTE)&hcInfo->BusNumber,
                                               sizeof(hcInfo->BusNumber),
                                               NULL);

    if (success)
    {
        success = SetupDiGetDeviceRegistryPropertyA(deviceInfo,
                                                   deviceInfoData,
                                                   SPDRP_ADDRESS,
                                                   NULL,
                                                   (PBYTE)&deviceAndFunction,
                                                   sizeof(deviceAndFunction),
                                                   NULL);
    }

    if (success)
    {
        hcInfo->BusDevice = deviceAndFunction >> 16;
        hcInfo->BusFunction = deviceAndFunction & 0xffff;
        hcInfo->BusDeviceFunctionValid = TRUE;
    }

    // Get the USB Host Controller info
    dwSuccess = GetHostControllerInfo(hHCDev, hcInfo);

    if (ERROR_SUCCESS != dwSuccess)
    {
        Oops(__FILE__, __LINE__);
    }

    // Add this host controller to the USB device tree view.
    //
	SP_USB_DEV spHCItem = AddLeaf(spTreeParent,
		(LPARAM)hcInfo,
		(LPTSTR)leafName,
		USB_ENGINE_CATEGORY::USB_HOST_DEVICE);
		//hcInfo->Revision == UsbSuperSpeed ? USB_ENGINE_CATEGORY::USB_SS_GOOD_DEVICE : USB_ENGINE_CATEGORY::USB_GOOD_DEVICE);
        //hcInfo->Revision == UsbSuperSpeed ? GoodSsDeviceIcon : GoodDeviceIcon);

    if (spHCItem == nullptr)
    {
        // Failure adding host controller to USB device tree
        // view.

        Oops(__FILE__, __LINE__);
		Free(driverKeyName);
		Free(hcInfo);
        return;
    }

    // Add this host controller to the list of enumerated
    // host controllers.
    //
    InsertTailList(&EnumeratedHCListHead,
                   &hcInfo->ListEntry);

    // Get the name of the root hub for this host
    // controller and then enumerate the root hub.
    //
    rootHubName = GetRootHubName(hHCDev);

    if (rootHubName != NULL)
    {
        size_t cbHubName = 0;
        HRESULT hr = S_OK;

        hr = StringCbLengthA(rootHubName, MAX_DRIVER_KEY_NAME, &cbHubName);
        if (SUCCEEDED(hr))
        {
            EnumerateHub(spHCItem,
                         rootHubName,
                         cbHubName,
                         NULL,       // ConnectionInfo
                         NULL,       // ConnectionInfoV2
                         NULL,       // PortConnectorProps
                         NULL,       // ConfigDesc
                         NULL,       // BosDesc
                         NULL,       // StringDescs
                         NULL);      // We do not pass DevProps for RootHub
        }
    }
    else
    {
        // Failure obtaining root hub name.

        Oops(__FILE__, __LINE__);
    }

    return;
}

//*****************************************************************************
//
// EnumerateHub()
//
// hTreeParent - Handle of the TreeView item under which this hub should be
// added.
//
// HubName - Name of this hub.  This pointer is kept so the caller can neither
// free nor reuse this memory.
//
// ConnectionInfo - NULL if this is a root hub, else this is the connection
// info for an external hub.  This pointer is kept so the caller can neither
// free nor reuse this memory.
//
// ConfigDesc - NULL if this is a root hub, else this is the Configuration
// Descriptor for an external hub.  This pointer is kept so the caller can
// neither free nor reuse this memory.
//
// StringDescs - NULL if this is a root hub.
//
// DevProps - Device properties of the hub
//
//*****************************************************************************
void CEnumerator::EnumerateHub(SP_USB_DEV spTreeParent,
    _In_reads_(cbHubName) PCHAR                     HubName,
    _In_ size_t                                     cbHubName,
    _In_opt_ PUSB_NODE_CONNECTION_INFORMATION_EX    ConnectionInfo,
    _In_opt_ PUSB_NODE_CONNECTION_INFORMATION_EX_V2 ConnectionInfoV2,
    _In_opt_ PUSB_PORT_CONNECTOR_PROPERTIES         PortConnectorProps,
    _In_opt_ PUSB_DESCRIPTOR_REQUEST                ConfigDesc,
    _In_opt_ PUSB_DESCRIPTOR_REQUEST                BosDesc,
    _In_opt_ PSTRING_DESCRIPTOR_NODE                StringDescs,
    _In_opt_ PUSB_DEVICE_PNP_STRINGS                DevProps
    )
{
    // Initialize locals to not allocated state so the error cleanup routine
    // only tries to cleanup things that were successfully allocated.
    //
    PUSB_NODE_INFORMATION    hubInfo = NULL;
    PUSB_HUB_INFORMATION_EX  hubInfoEx = NULL;
    PUSB_HUB_CAPABILITIES_EX hubCapabilityEx = NULL;
    HANDLE                  hHubDevice = INVALID_HANDLE_VALUE;
	SP_USB_DEV             spItem = nullptr;
    PVOID                   info = NULL;
    PCHAR                   deviceName = NULL;
    ULONG                   nBytes = 0;
    BOOL                    success = 0;
    DWORD                   dwSizeOfLeafName = 0;
    CHAR                    leafName[512] = {0}; 
    HRESULT                 hr = S_OK;
    size_t                  cchHeader = 0;
    size_t                  cchFullHubName = 0;

    // Allocate some space for a USBDEVICEINFO structure to hold the
    // hub info, hub name, and connection info pointers.  GPTR zero
    // initializes the structure for us.
    //
    info = ALLOC(sizeof(USBEXTERNALHUBINFO));
    if (info == NULL)
    {
        Oops(__FILE__, __LINE__);
        goto EnumerateHubError;
    }

    // Allocate some space for a USB_NODE_INFORMATION structure for this Hub
    //
    hubInfo = (PUSB_NODE_INFORMATION)ALLOC(sizeof(USB_NODE_INFORMATION));
    if (hubInfo == NULL)
    {
        Oops(__FILE__, __LINE__);
        goto EnumerateHubError;
    }

    hubInfoEx = (PUSB_HUB_INFORMATION_EX)ALLOC(sizeof(USB_HUB_INFORMATION_EX));
    if (hubInfoEx == NULL)
    {
        Oops(__FILE__, __LINE__);
        goto EnumerateHubError;
    }

    hubCapabilityEx = (PUSB_HUB_CAPABILITIES_EX)ALLOC(sizeof(USB_HUB_CAPABILITIES_EX));
    if(hubCapabilityEx == NULL)
    {
        Oops(__FILE__, __LINE__);
        goto EnumerateHubError;
    }

    // Keep copies of the Hub Name, Connection Info, and Configuration
    // Descriptor pointers
    //
    ((PUSBROOTHUBINFO)info)->HubInfo   = hubInfo;
    ((PUSBROOTHUBINFO)info)->HubName   = HubName;

    if (ConnectionInfo != NULL)
    {
        ((PUSBEXTERNALHUBINFO)info)->DeviceInfoType = ExternalHubInfo;
        ((PUSBEXTERNALHUBINFO)info)->ConnectionInfo = ConnectionInfo;
        ((PUSBEXTERNALHUBINFO)info)->ConfigDesc = ConfigDesc;
        ((PUSBEXTERNALHUBINFO)info)->StringDescs = StringDescs;
        ((PUSBEXTERNALHUBINFO)info)->PortConnectorProps = PortConnectorProps;
        ((PUSBEXTERNALHUBINFO)info)->HubInfoEx = hubInfoEx;
        ((PUSBEXTERNALHUBINFO)info)->HubCapabilityEx = hubCapabilityEx;
        ((PUSBEXTERNALHUBINFO)info)->BosDesc = BosDesc;
        ((PUSBEXTERNALHUBINFO)info)->ConnectionInfoV2 = ConnectionInfoV2;
        ((PUSBEXTERNALHUBINFO)info)->UsbDeviceProperties = DevProps;
    }
    else
    {
        ((PUSBROOTHUBINFO)info)->DeviceInfoType = RootHubInfo;
        ((PUSBROOTHUBINFO)info)->HubInfoEx = hubInfoEx;
        ((PUSBROOTHUBINFO)info)->HubCapabilityEx = hubCapabilityEx;
        ((PUSBROOTHUBINFO)info)->PortConnectorProps = PortConnectorProps;
        ((PUSBROOTHUBINFO)info)->UsbDeviceProperties = DevProps;
    }

    // Allocate a temp buffer for the full hub device name.
    //
    hr = StringCbLengthA("\\\\.\\", MAX_DEVICE_PROP, &cchHeader);
    if (FAILED(hr))
    {
        goto EnumerateHubError;
    }
    cchFullHubName = cchHeader + cbHubName + 1;
    deviceName = (PCHAR)ALLOC((DWORD) cchFullHubName);
    if (deviceName == NULL)
    {
        Oops(__FILE__, __LINE__);
        goto EnumerateHubError;
    }

    // Create the full hub device name
    //
    hr = StringCchCopyNA(deviceName, cchFullHubName, "\\\\.\\", cchHeader);
    if (FAILED(hr))
    {
        goto EnumerateHubError;
    }
    hr = StringCchCatNA(deviceName, cchFullHubName, HubName, cbHubName);
    if (FAILED(hr))
    {
        goto EnumerateHubError;
    }

    // Try to hub the open device
    //
    hHubDevice = CreateFileA(deviceName,
                            GENERIC_WRITE,
                            FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            NULL);

    // Done with temp buffer for full hub device name
    //
	Free(deviceName);

    if (hHubDevice == INVALID_HANDLE_VALUE)
    {
        Oops(__FILE__, __LINE__);
        goto EnumerateHubError;
    }

    //
    // Now query USBHUB for the USB_NODE_INFORMATION structure for this hub.
    // This will tell us the number of downstream ports to enumerate, among
    // other things.
    //
    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_NODE_INFORMATION,
                              hubInfo,
                              sizeof(USB_NODE_INFORMATION),
                              hubInfo,
                              sizeof(USB_NODE_INFORMATION),
                              &nBytes,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        goto EnumerateHubError;
    }

    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_HUB_INFORMATION_EX,
                              hubInfoEx,
                              sizeof(USB_HUB_INFORMATION_EX),
                              hubInfoEx,
                              sizeof(USB_HUB_INFORMATION_EX),
                              &nBytes,
                              NULL);

    //
    // Fail gracefully for downlevel OS's from Win8
    //
    if (!success || nBytes < sizeof(USB_HUB_INFORMATION_EX))
    {
		Free(hubInfoEx);
        hubInfoEx = NULL;
        if (ConnectionInfo != NULL)
        {
            ((PUSBEXTERNALHUBINFO)info)->HubInfoEx = NULL;
        }
        else
        {
            ((PUSBROOTHUBINFO)info)->HubInfoEx = NULL;
        }
    }

    //
    // Obtain Hub Capabilities
    //
    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_HUB_CAPABILITIES_EX,
                              hubCapabilityEx,
                              sizeof(USB_HUB_CAPABILITIES_EX),
                              hubCapabilityEx,
                              sizeof(USB_HUB_CAPABILITIES_EX),
                              &nBytes,
                              NULL);

	if (!hubCapabilityEx->CapabilityFlags.HubIsRoot)
	{
		int m = 0;
	}
    //
    // Fail gracefully
    //
    if (!success || nBytes < sizeof(USB_HUB_CAPABILITIES_EX))
    {
		Free(hubCapabilityEx);
        hubCapabilityEx = NULL;
        if (ConnectionInfo != NULL)
        {
            ((PUSBEXTERNALHUBINFO)info)->HubCapabilityEx = NULL;
        }
        else
        {
            ((PUSBROOTHUBINFO)info)->HubCapabilityEx = NULL;
        }
    }

    // Build the leaf name from the port number and the device description
    //
    dwSizeOfLeafName = sizeof(leafName);
    if (ConnectionInfo)
    {
        StringCchPrintfA(leafName, dwSizeOfLeafName, "[Port%d] ", ConnectionInfo->ConnectionIndex);
        StringCchCatA(leafName, 
            dwSizeOfLeafName, 
            ConnectionStatuses[ConnectionInfo->ConnectionStatus]);
        StringCchCatNA(leafName, 
            dwSizeOfLeafName, 
            " :  ",
            sizeof(" :  "));
    }

    if (DevProps)
    {
        size_t cbDeviceDesc = 0;
        hr = StringCbLengthA(DevProps->DeviceDesc, MAX_DRIVER_KEY_NAME, &cbDeviceDesc);
        if(SUCCEEDED(hr))
        {
            StringCchCatNA(leafName, 
                    dwSizeOfLeafName, 
                    DevProps->DeviceDesc,
                    cbDeviceDesc);
        }
    }
    else
    {
        if(ConnectionInfo != NULL)
        {
            // External hub
            StringCchCatNA(leafName, 
                    dwSizeOfLeafName, 
                    HubName,
                    cbHubName);
        }
        else
        {
            // Root hub
            StringCchCatNA(leafName, 
                    dwSizeOfLeafName, 
                    "RootHub",
                    sizeof("RootHub")); 
        }
    }
	/* m_nHubIndex++; */ // Increment of Hub-index
    // Now add an item to the TreeView with the PUSBDEVICEINFO pointer info
    // as the LPARAM reference value containing everything we know about the
    // hub.
    //
	spItem = AddLeaf(spTreeParent,
		(LPARAM)info,
		(LPTSTR)leafName,
		USB_ENGINE_CATEGORY::USB_HUB,
		true); // It's Hub

    if (spItem == nullptr)
    {
        Oops(__FILE__, __LINE__);
        goto EnumerateHubError;
    }
	
    // Now recursively enumerate the ports of this hub.
    //
    EnumerateHubPorts(
		spItem,
        hHubDevice,
        hubInfo->u.HubInformation.HubDescriptor.bNumberOfPorts
        );

/*	goto EnumerateHubExit; */
	CloseHandle(hHubDevice);
	/* m_nHubIndex--; */ // Decrement of Hub-index (during correct execution)
	return;

EnumerateHubError:
	g_log.SaveLogFile(LOG_MODES::ERRORLOG, "CEnumerator::EnumerateHub: 'EnumerateHubError' occur!");
    //
    // Clean up any stuff that got allocated
    //
// EnumerateHubExit:
    if (hHubDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hHubDevice);
        hHubDevice = INVALID_HANDLE_VALUE;
    }

    if (hubInfo)
    {
		Free(hubInfo);
    }

    if (hubInfoEx)
    {
		Free(hubInfoEx);
    }

    if (info)
    {
		Free(info);
    }

    if (HubName)
    {
		Free(HubName);
    }

    if (ConnectionInfo)
    {
		Free(ConnectionInfo);
    }

    if (ConfigDesc)
    {
		Free(ConfigDesc);
    }

    if (BosDesc)
    {
		Free(BosDesc);
    }

    if (StringDescs != NULL)
    {
        PSTRING_DESCRIPTOR_NODE Next;

        do {

            Next = StringDescs->Next;
			Free(StringDescs);
            StringDescs = Next;

        } while (StringDescs != NULL);
    }
	/* m_nHubIndex--; */ // Decrement of Hub-index (during error)
}

//*****************************************************************************
//
// EnumerateHubPorts()
//
// hTreeParent - Handle of the TreeView item under which the hub port should
// be added.
//
// hHubDevice - Handle of the hub device to enumerate.
//
// NumPorts - Number of ports on the hub.
//
//*****************************************************************************
void CEnumerator::EnumerateHubPorts (SP_USB_DEV spTreeParent, HANDLE hHubDevice, ULONG NumPorts)
{
    ULONG       index = 0;
    BOOL        success = 0;
    HRESULT     hr = S_OK;
    PCHAR       driverKeyName = NULL;
    PUSB_DEVICE_PNP_STRINGS DevProps;
    DWORD       dwSizeOfLeafName = 0;
    CHAR        leafName[512];
	USB_ENGINE_CATEGORY   usbCateg = USB_ENGINE_CATEGORY::GLOBAL;

    PUSB_NODE_CONNECTION_INFORMATION_EX    connectionInfoEx;
    PUSB_PORT_CONNECTOR_PROPERTIES         pPortConnectorProps;
    USB_PORT_CONNECTOR_PROPERTIES          portConnectorProps;
    PUSB_DESCRIPTOR_REQUEST                configDesc;
    PUSB_DESCRIPTOR_REQUEST                bosDesc;
    PSTRING_DESCRIPTOR_NODE                stringDescs;
    PUSBDEVICEINFO                         info;
    PUSB_NODE_CONNECTION_INFORMATION_EX_V2 connectionInfoExV2;
    PDEVICE_INFO_NODE                      pNode;

    // Loop over all ports of the hub.
    //
    // Port indices are 1 based, not 0 based.
    //
    for (index = 1; index <= NumPorts; index++)
    {
        ULONG nBytesEx;
        ULONG nBytes = 0;

        connectionInfoEx = NULL;
        pPortConnectorProps = NULL;
        ZeroMemory(&portConnectorProps, sizeof(portConnectorProps));
        configDesc = NULL;
        bosDesc = NULL;
        stringDescs = NULL;
        info = NULL;
        connectionInfoExV2 = NULL;
        pNode = NULL;
        DevProps = NULL;
        ZeroMemory(leafName, sizeof(leafName));

        //
        // Allocate space to hold the connection info for this port.
        // For now, allocate it big enough to hold info for 30 pipes.
        //
        // Endpoint numbers are 0-15.  Endpoint number 0 is the standard
        // control endpoint which is not explicitly listed in the Configuration
        // Descriptor.  There can be an IN endpoint and an OUT endpoint at
        // endpoint numbers 1-15 so there can be a maximum of 30 endpoints
        // per device configuration.
        //
        // Should probably size this dynamically at some point.
        //

        nBytesEx = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) +
                 (sizeof(USB_PIPE_INFO) * 30);

        connectionInfoEx = (PUSB_NODE_CONNECTION_INFORMATION_EX)ALLOC(nBytesEx);

        if (connectionInfoEx == NULL)
        {
            Oops(__FILE__, __LINE__);
            break;
        }

        connectionInfoExV2 = (PUSB_NODE_CONNECTION_INFORMATION_EX_V2) 
                                    ALLOC(sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2));

        if (connectionInfoExV2 == NULL)
        {
            Oops(__FILE__, __LINE__);
            Free(connectionInfoEx);
            break;
        }
        
        //
        // Now query USBHUB for the structures
        // for this port.  This will tell us if a device is attached to this
        // port, among other things.
        // The fault tolerate code is executed first.
        //

        portConnectorProps.ConnectionIndex = index;

        success = DeviceIoControl(hHubDevice,
                                  IOCTL_USB_GET_PORT_CONNECTOR_PROPERTIES,
                                  &portConnectorProps,
                                  sizeof(USB_PORT_CONNECTOR_PROPERTIES),
                                  &portConnectorProps,
                                  sizeof(USB_PORT_CONNECTOR_PROPERTIES),
                                  &nBytes,
                                  NULL);

        if (success && nBytes == sizeof(USB_PORT_CONNECTOR_PROPERTIES)) 
        {
            pPortConnectorProps = (PUSB_PORT_CONNECTOR_PROPERTIES)
                                        ALLOC(portConnectorProps.ActualLength);

            if (pPortConnectorProps != NULL)
            {
                pPortConnectorProps->ConnectionIndex = index;
                
                success = DeviceIoControl(hHubDevice,
                                          IOCTL_USB_GET_PORT_CONNECTOR_PROPERTIES,
                                          pPortConnectorProps,
                                          portConnectorProps.ActualLength,
                                          pPortConnectorProps,
                                          portConnectorProps.ActualLength,
                                          &nBytes,
                                          NULL);

                if (!success || nBytes < portConnectorProps.ActualLength)
                {
                    Free(pPortConnectorProps);
                    pPortConnectorProps = NULL;
                }
            }
        }
        
        connectionInfoExV2->ConnectionIndex = index;
        connectionInfoExV2->Length = sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2);
        connectionInfoExV2->SupportedUsbProtocols.Usb300 = 1;

        success = DeviceIoControl(hHubDevice,
                                  IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX_V2,
                                  connectionInfoExV2,
                                  sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2),
                                  connectionInfoExV2,
                                  sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2),
                                  &nBytes,
                                  NULL);

        if (!success || nBytes < sizeof(USB_NODE_CONNECTION_INFORMATION_EX_V2)) 
        {
            Free(connectionInfoExV2);
            connectionInfoExV2 = NULL;
        }

        connectionInfoEx->ConnectionIndex = index;

        success = DeviceIoControl(hHubDevice,
                                  IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                                  connectionInfoEx,
                                  nBytesEx,
                                  connectionInfoEx,
                                  nBytesEx,
                                  &nBytesEx,
                                  NULL);

        if (success)
        {
            //
            // Since the USB_NODE_CONNECTION_INFORMATION_EX is used to display
            // the device speed, but the hub driver doesn't support indication
            // of superspeed, we overwrite the value if the super speed
            // data structures are available and indicate the device is operating
            // at SuperSpeed.
            // 
            
            if (connectionInfoEx->Speed == UsbHighSpeed 
                && connectionInfoExV2 != NULL 
                && (connectionInfoExV2->Flags.DeviceIsOperatingAtSuperSpeedOrHigher)) // ||
                   // connectionInfoExV2->Flags.DeviceIsOperatingAtSuperSpeedPlusOrHigher))
            {
                connectionInfoEx->Speed = UsbSuperSpeed;
            }
        } 
        else 
        {
            PUSB_NODE_CONNECTION_INFORMATION    connectionInfo = NULL;

            // Try using IOCTL_USB_GET_NODE_CONNECTION_INFORMATION
            // instead of IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX
            //

            nBytes = sizeof(USB_NODE_CONNECTION_INFORMATION) +
                     sizeof(USB_PIPE_INFO) * 30;

            connectionInfo = (PUSB_NODE_CONNECTION_INFORMATION)ALLOC(nBytes);

            if (connectionInfo == NULL) 
            {
                Oops(__FILE__, __LINE__);

                Free(connectionInfoEx);
                if (pPortConnectorProps != NULL)
                {
                    Free(pPortConnectorProps);
                }
                if (connectionInfoExV2 != NULL)
                {
                    Free(connectionInfoExV2);
                }
                continue;                
            }

            connectionInfo->ConnectionIndex = index;

            success = DeviceIoControl(hHubDevice,
                                      IOCTL_USB_GET_NODE_CONNECTION_INFORMATION,
                                      connectionInfo,
                                      nBytes,
                                      connectionInfo,
                                      nBytes,
                                      &nBytes,
                                      NULL);

            if (!success)
            {
                Oops(__FILE__, __LINE__);

                Free(connectionInfo);
                Free(connectionInfoEx);
                if (pPortConnectorProps != NULL)
                {
                    Free(pPortConnectorProps);
                }
                if (connectionInfoExV2 != NULL)
                {
                    Free(connectionInfoExV2);
                }
                continue;
            }

            // Copy IOCTL_USB_GET_NODE_CONNECTION_INFORMATION into
            // IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX structure.
            //
            connectionInfoEx->ConnectionIndex = connectionInfo->ConnectionIndex;
            connectionInfoEx->DeviceDescriptor = connectionInfo->DeviceDescriptor;
            connectionInfoEx->CurrentConfigurationValue = connectionInfo->CurrentConfigurationValue;
            connectionInfoEx->Speed = connectionInfo->LowSpeed ? UsbLowSpeed : UsbFullSpeed;
            connectionInfoEx->DeviceIsHub = connectionInfo->DeviceIsHub;
            connectionInfoEx->DeviceAddress = connectionInfo->DeviceAddress;
            connectionInfoEx->NumberOfOpenPipes = connectionInfo->NumberOfOpenPipes;
            connectionInfoEx->ConnectionStatus = connectionInfo->ConnectionStatus;

            memcpy(&connectionInfoEx->PipeList[0],
                   &connectionInfo->PipeList[0],
                   sizeof(USB_PIPE_INFO) * 30);

            Free(connectionInfo);
        }

        // Update the count of connected devices
        //
        if (connectionInfoEx->ConnectionStatus == DeviceConnected)
        {
			m_nTotalDevicesConnected++;
        }

        if (connectionInfoEx->DeviceIsHub)
        {
			g_nTotalHubs++;
        }

        // If there is a device connected, get the Device Description
        //
        if (connectionInfoEx->ConnectionStatus != NoDeviceConnected)
        {
            driverKeyName = GetDriverKeyName(hHubDevice, index);

            if (driverKeyName)
            {
                size_t cbDriverName = 0;

                hr = StringCbLengthA(driverKeyName, MAX_DRIVER_KEY_NAME, &cbDriverName);
                if (SUCCEEDED(hr))
                {
                    DevProps = CDevNode::DriverNameToDeviceProperties(driverKeyName, cbDriverName);
                    pNode = FindMatchingDeviceNodeForDriverName(driverKeyName, connectionInfoEx->DeviceIsHub);
                }
                Free(driverKeyName);
            }

        }

        // If there is a device connected to the port, try to retrieve the
        // Configuration Descriptor from the device.
        // Variable g_bDoConfigDesc <=> gDoConfigDesc
        if (g_bDoConfigDesc &&
            connectionInfoEx->ConnectionStatus == DeviceConnected)
        {
            configDesc = GetConfigDescriptor(hHubDevice,
                                             index,
                                             0);
        }
        else
        {
            configDesc = NULL;
        }

        if (configDesc != NULL &&
            connectionInfoEx->DeviceDescriptor.bcdUSB > 0x0200)
        {
            bosDesc = GetBOSDescriptor(hHubDevice,
                                       index);
        }
        else
        {
            bosDesc = NULL;
        }

        if (configDesc != NULL &&
            AreThereStringDescriptors(&connectionInfoEx->DeviceDescriptor,
                                      (PUSB_CONFIGURATION_DESCRIPTOR)(configDesc+1)))
        {
            stringDescs = GetAllStringDescriptors (
                              hHubDevice,
                              index,
                              &connectionInfoEx->DeviceDescriptor,
                              (PUSB_CONFIGURATION_DESCRIPTOR)(configDesc+1));
        }
        else
        {
            stringDescs = NULL;
        }

        // If the device connected to the port is an external hub, get the
        // name of the external hub and recursively enumerate it.
        //
        if (connectionInfoEx->DeviceIsHub)
        {
            PCHAR extHubName;
            size_t cbHubName = 0;

            extHubName = GetExternalHubName(hHubDevice, index);
            if (extHubName != NULL)
            {
                hr = StringCbLengthA(extHubName, MAX_DRIVER_KEY_NAME, &cbHubName);
                if (SUCCEEDED(hr))
                {
                    EnumerateHub(spTreeParent, //hPortItem,
                            extHubName,
                            cbHubName,
                            connectionInfoEx,
                            connectionInfoExV2,
                            pPortConnectorProps,
                            configDesc,
                            bosDesc,
                            stringDescs,
                            DevProps);
                }
            }
        }
        else
        {
            // Allocate some space for a USBDEVICEINFO structure to hold the
            // hub info, hub name, and connection info pointers.  GPTR zero
            // initializes the structure for us.
            //
            info = (PUSBDEVICEINFO) ALLOC(sizeof(USBDEVICEINFO));

            if (info == NULL)
            {
                Oops(__FILE__, __LINE__);
                if (configDesc != NULL)
                {
                    Free(configDesc);
                }
                if (bosDesc != NULL)
                {
                    Free(bosDesc);
                }
                Free(connectionInfoEx);
                
                if (pPortConnectorProps != NULL)
                {
                    Free(pPortConnectorProps);
                }
                if (connectionInfoExV2 != NULL)
                {
                    Free(connectionInfoExV2);
                }
                break;
            }

            info->DeviceInfoType = DeviceInfo;
            info->ConnectionInfo = connectionInfoEx;
            info->PortConnectorProps = pPortConnectorProps;
            info->ConfigDesc = configDesc;
            info->StringDescs = stringDescs;
            info->BosDesc = bosDesc;
            info->ConnectionInfoV2 = connectionInfoExV2;
            info->UsbDeviceProperties = DevProps;
            info->DeviceInfoNode = pNode;

            StringCchPrintfA(leafName, sizeof(leafName), "[Port%d] ", index);

            // Add error description if ConnectionStatus is other than NoDeviceConnected / DeviceConnected
            StringCchCatA(leafName, 
                sizeof(leafName), 
                ConnectionStatuses[connectionInfoEx->ConnectionStatus]);

            if (DevProps)
            {
                size_t cchDeviceDesc = 0;

                hr = StringCbLengthA(DevProps->DeviceDesc, MAX_DEVICE_PROP, &cchDeviceDesc);
                if (FAILED(hr))
                {
                    Oops(__FILE__, __LINE__);
                }
                dwSizeOfLeafName = sizeof(leafName);
                StringCchCatNA(leafName, 
                    dwSizeOfLeafName - 1, 
                    " :  ",
                    sizeof(" :  "));
                StringCchCatNA(leafName, 
                    dwSizeOfLeafName - 1, 
                    DevProps->DeviceDesc,
                    cchDeviceDesc );
            }

            if (connectionInfoEx->ConnectionStatus == NoDeviceConnected)
            {
                if (connectionInfoExV2 != NULL &&
                    connectionInfoExV2->SupportedUsbProtocols.Usb300 == 1)
                {
                    // icon = NoSsDeviceIcon;
					usbCateg = USB_ENGINE_CATEGORY::USB_NO_SS_DEVICE; 
                }
                else
                {
                    // icon = NoDeviceIcon; 
					usbCateg = USB_ENGINE_CATEGORY::USB_NO_DEVICE; 
                }
            }
            else if (connectionInfoEx->CurrentConfigurationValue)
            {
                if (connectionInfoEx->Speed == UsbSuperSpeed)
                {
                    // icon = GoodSsDeviceIcon; 
					usbCateg = USB_ENGINE_CATEGORY::USB_SS_GOOD_DEVICE;
                }
                else
                {
                    // icon = GoodDeviceIcon; 
					usbCateg = USB_ENGINE_CATEGORY::USB_GOOD_DEVICE;
                }
            }
            else
            {
                // icon = BadDeviceIcon;
				usbCateg = USB_ENGINE_CATEGORY::USB_BAD_DEVICE;
            }

            AddLeaf(spTreeParent, //hPortItem,
                (LPARAM)info,
                (LPTSTR)leafName,
				usbCateg);
        }
    } // for
}

//*****************************************************************************
//
// WideStrToMultiStr()
//
//*****************************************************************************
PCHAR CEnumerator::WideStrToMultiStr( _In_reads_bytes_(cbWideStr) PWCHAR WideStr, _In_ size_t cbWideStr)
{
    ULONG  nBytes = 0;
    PCHAR  MultiStr = NULL;
    PWCHAR pWideStr = NULL;

    // Use local string to guarantee zero termination
    pWideStr = (PWCHAR) ALLOC((DWORD) cbWideStr + sizeof(WCHAR));
    if (NULL == pWideStr)
    {
        return NULL;
    }
    memset(pWideStr, 0, cbWideStr + sizeof(WCHAR));
    memcpy(pWideStr, WideStr, cbWideStr);

    // Get the length of the converted string
    //
    nBytes = WideCharToMultiByte(
                 CP_ACP,
                 WC_NO_BEST_FIT_CHARS,
                 pWideStr,
                 -1,
                 NULL,
                 0,
                 NULL,
                 NULL);

    if (nBytes == 0)
    {
        Free(pWideStr);
        return NULL;
    }

    // Allocate space to hold the converted string
    //
    MultiStr = (PCHAR)ALLOC(nBytes);
    if (MultiStr == NULL)
    {
        Free(pWideStr);
        return NULL;
    }

    // Convert the string
    //
    nBytes = WideCharToMultiByte(
                 CP_ACP,
                 WC_NO_BEST_FIT_CHARS,
                 pWideStr,
                 -1,
                 MultiStr,
                 nBytes,
                 NULL,
                 NULL);

    if (nBytes == 0)
    {
        Free(MultiStr);
        Free(pWideStr);
        return NULL;
    }

    Free(pWideStr);
    return MultiStr;
}

//*****************************************************************************
//
// GetRootHubName()
//
//*****************************************************************************

PCHAR CEnumerator::GetRootHubName(HANDLE HostController)
{
    BOOL                success = 0;
    ULONG               nBytes = 0;
    USB_ROOT_HUB_NAME   rootHubName;
    PUSB_ROOT_HUB_NAME  rootHubNameW = NULL;
    PCHAR               rootHubNameA = NULL;

    // Get the length of the name of the Root Hub attached to the
    // Host Controller
    //
    success = DeviceIoControl(HostController,
                              IOCTL_USB_GET_ROOT_HUB_NAME,
                              0,
                              0,
                              &rootHubName,
                              sizeof(rootHubName),
                              &nBytes,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        goto GetRootHubNameError;
    }

    // Allocate space to hold the Root Hub name
    //
    nBytes = rootHubName.ActualLength;

    rootHubNameW = (PUSB_ROOT_HUB_NAME)ALLOC(nBytes);
    if (rootHubNameW == NULL)
    {
        Oops(__FILE__, __LINE__);
        goto GetRootHubNameError;
    }

    // Get the name of the Root Hub attached to the Host Controller
    //
    success = DeviceIoControl(HostController,
                              IOCTL_USB_GET_ROOT_HUB_NAME,
                              NULL,
                              0,
                              rootHubNameW,
                              nBytes,
                              &nBytes,
                              NULL);
    if (!success)
    {
        Oops(__FILE__, __LINE__);
        goto GetRootHubNameError;
    }

    // Convert the Root Hub name
    //
    rootHubNameA = WideStrToMultiStr(rootHubNameW->RootHubName, nBytes - sizeof(USB_ROOT_HUB_NAME) + sizeof(WCHAR));

    // All done, free the uncoverted Root Hub name and return the
    // converted Root Hub name
    //
    Free(rootHubNameW);

    return rootHubNameA;

GetRootHubNameError:
    // There was an error, free anything that was allocated
    //
    if (rootHubNameW != NULL)
    {
        Free(rootHubNameW);
        rootHubNameW = NULL;
    }
    return NULL;
}

//*****************************************************************************
//
// GetExternalHubName()
//
//*****************************************************************************

PCHAR CEnumerator::GetExternalHubName(HANDLE Hub, ULONG ConnectionIndex)
{
    BOOL                        success = 0;
    ULONG                       nBytes = 0;
    USB_NODE_CONNECTION_NAME    extHubName;
    PUSB_NODE_CONNECTION_NAME   extHubNameW = NULL;
    PCHAR                       extHubNameA = NULL;

    // Get the length of the name of the external hub attached to the
    // specified port.
    //
    extHubName.ConnectionIndex = ConnectionIndex;

    success = DeviceIoControl(Hub,
                              IOCTL_USB_GET_NODE_CONNECTION_NAME,
                              &extHubName,
                              sizeof(extHubName),
                              &extHubName,
                              sizeof(extHubName),
                              &nBytes,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        goto GetExternalHubNameError;
    }

    // Allocate space to hold the external hub name
    //
    nBytes = extHubName.ActualLength;

    if (nBytes <= sizeof(extHubName))
    {
        Oops(__FILE__, __LINE__);
        goto GetExternalHubNameError;
    }

    extHubNameW = (PUSB_NODE_CONNECTION_NAME)ALLOC(nBytes);

    if (extHubNameW == NULL)
    {
        Oops(__FILE__, __LINE__);
        goto GetExternalHubNameError;
    }

    // Get the name of the external hub attached to the specified port
    //
    extHubNameW->ConnectionIndex = ConnectionIndex;

    success = DeviceIoControl(Hub,
                              IOCTL_USB_GET_NODE_CONNECTION_NAME,
                              extHubNameW,
                              nBytes,
                              extHubNameW,
                              nBytes,
                              &nBytes,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        goto GetExternalHubNameError;
    }

    // Convert the External Hub name
    //
    extHubNameA = WideStrToMultiStr(extHubNameW->NodeName, nBytes - sizeof(USB_NODE_CONNECTION_NAME) + sizeof(WCHAR));

    // All done, free the uncoverted external hub name and return the
    // converted external hub name
    //
    Free(extHubNameW);

    return extHubNameA;


GetExternalHubNameError:
    // There was an error, free anything that was allocated
    //
    if (extHubNameW != NULL)
    {
        Free(extHubNameW);
        extHubNameW = NULL;
    }

    return NULL;
}

//*****************************************************************************
//
// GetDriverKeyName()
//
//*****************************************************************************
PCHAR CEnumerator::GetDriverKeyName (HANDLE Hub, ULONG ConnectionIndex
)
{
    BOOL                                success = 0;
    ULONG                               nBytes = 0;
    USB_NODE_CONNECTION_DRIVERKEY_NAME  driverKeyName;
    PUSB_NODE_CONNECTION_DRIVERKEY_NAME driverKeyNameW = NULL;
    PCHAR                               driverKeyNameA = NULL;

    // Get the length of the name of the driver key of the device attached to
    // the specified port.
    //
    driverKeyName.ConnectionIndex = ConnectionIndex;

    success = DeviceIoControl(Hub,
                              IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
                              &driverKeyName,
                              sizeof(driverKeyName),
                              &driverKeyName,
                              sizeof(driverKeyName),
                              &nBytes,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        goto GetDriverKeyNameError;
    }

    // Allocate space to hold the driver key name
    //
    nBytes = driverKeyName.ActualLength;

    if (nBytes <= sizeof(driverKeyName))
    {
        Oops(__FILE__, __LINE__);
        goto GetDriverKeyNameError;
    }

    driverKeyNameW = (PUSB_NODE_CONNECTION_DRIVERKEY_NAME)ALLOC(nBytes);
    if (driverKeyNameW == NULL)
    {
        Oops(__FILE__, __LINE__);
        goto GetDriverKeyNameError;
    }

    // Get the name of the driver key of the device attached to
    // the specified port.
    //
    driverKeyNameW->ConnectionIndex = ConnectionIndex;

    success = DeviceIoControl(Hub,
                              IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
                              driverKeyNameW,
                              nBytes,
                              driverKeyNameW,
                              nBytes,
                              &nBytes,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        goto GetDriverKeyNameError;
    }

    // Convert the driver key name
    //
    driverKeyNameA = WideStrToMultiStr(driverKeyNameW->DriverKeyName, nBytes - sizeof(USB_NODE_CONNECTION_DRIVERKEY_NAME) + sizeof(WCHAR));

    // All done, free the uncoverted driver key name and return the
    // converted driver key name
    //
    Free(driverKeyNameW);

    return driverKeyNameA;


GetDriverKeyNameError:
    // There was an error, free anything that was allocated
    //
    if (driverKeyNameW != NULL)
    {
        Free(driverKeyNameW);
        driverKeyNameW = NULL;
    }

    return NULL;
}

//*****************************************************************************
//
// GetHCDDriverKeyName()
//
//*****************************************************************************

PCHAR CEnumerator::GetHCDDriverKeyName (HANDLE  HCD)
{
    BOOL                    success = 0;
    ULONG                   nBytes = 0;
    USB_HCD_DRIVERKEY_NAME  driverKeyName = {0};
    PUSB_HCD_DRIVERKEY_NAME driverKeyNameW = NULL;
    PCHAR                   driverKeyNameA = NULL;

    ZeroMemory(&driverKeyName, sizeof(driverKeyName));

    // Get the length of the name of the driver key of the HCD
    //
    success = DeviceIoControl(HCD,
                              IOCTL_GET_HCD_DRIVERKEY_NAME,
                              &driverKeyName,
                              sizeof(driverKeyName),
                              &driverKeyName,
                              sizeof(driverKeyName),
                              &nBytes,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        goto GetHCDDriverKeyNameError;
    }

    // Allocate space to hold the driver key name
    //
    nBytes = driverKeyName.ActualLength;
    if (nBytes <= sizeof(driverKeyName))
    {
        Oops(__FILE__, __LINE__);
        goto GetHCDDriverKeyNameError;
    }

    driverKeyNameW = (PUSB_HCD_DRIVERKEY_NAME)ALLOC(nBytes);
    if (driverKeyNameW == NULL)
    {
        Oops(__FILE__, __LINE__);
        goto GetHCDDriverKeyNameError;
    }

    // Get the name of the driver key of the device attached to
    // the specified port.
    //

    success = DeviceIoControl(HCD,
                              IOCTL_GET_HCD_DRIVERKEY_NAME,
                              driverKeyNameW,
                              nBytes,
                              driverKeyNameW,
                              nBytes,
                              &nBytes,
                              NULL);
    if (!success)
    {
        Oops(__FILE__, __LINE__);
        goto GetHCDDriverKeyNameError;
    }

    //
    // Convert the driver key name
    // Pass the length of the DriverKeyName string
    // 

    driverKeyNameA = WideStrToMultiStr(driverKeyNameW->DriverKeyName, nBytes - sizeof(USB_HCD_DRIVERKEY_NAME) + sizeof(WCHAR));

    // All done, free the uncoverted driver key name and return the
    // converted driver key name
    //
    Free(driverKeyNameW);

    return driverKeyNameA;

GetHCDDriverKeyNameError:
    // There was an error, free anything that was allocated
    //
    if (driverKeyNameW != NULL)
    {
        Free(driverKeyNameW);
        driverKeyNameW = NULL;
    }

    return NULL;
}

//*****************************************************************************
//
// GetConfigDescriptor()
//
// hHubDevice - Handle of the hub device containing the port from which the
// Configuration Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the Configuration Descriptor will be requested.
//
// DescriptorIndex - Configuration Descriptor index, zero based.
//
//*****************************************************************************
PUSB_DESCRIPTOR_REQUEST CEnumerator::GetConfigDescriptor(HANDLE hHubDevice, ULONG ConnectionIndex, UCHAR DescriptorIndex)
{
    BOOL    success = 0;
    ULONG   nBytes = 0;
    ULONG   nBytesReturned = 0;

    UCHAR   configDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST) +
                             sizeof(USB_CONFIGURATION_DESCRIPTOR)];

    PUSB_DESCRIPTOR_REQUEST         configDescReq = NULL;
    PUSB_CONFIGURATION_DESCRIPTOR   configDesc = NULL;


    // Request the Configuration Descriptor the first time using our
    // local buffer, which is just big enough for the Cofiguration
    // Descriptor itself.
    //
    nBytes = sizeof(configDescReqBuf);

    configDescReq = (PUSB_DESCRIPTOR_REQUEST)configDescReqBuf;
    configDesc = (PUSB_CONFIGURATION_DESCRIPTOR)(configDescReq+1);

    // Zero fill the entire request structure
    //
    memset(configDescReq, 0, nBytes);

    // Indicate the port from which the descriptor will be requested
    //
    configDescReq->ConnectionIndex = ConnectionIndex;

    //
    // USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
    // IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
    //
    // USBD will automatically initialize these fields:
    //     bmRequest = 0x80
    //     bRequest  = 0x06
    //
    // We must inititialize these fields:
    //     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
    //     wIndex    = Zero (or Language ID for String Descriptors)
    //     wLength   = Length of descriptor buffer
    //
    configDescReq->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8)
                                        | DescriptorIndex;

    configDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

    // Now issue the get descriptor request.
    //
    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                              configDescReq,
                              nBytes,
                              configDescReq,
                              nBytes,
                              &nBytesReturned,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        return NULL;
    }

    if (nBytes != nBytesReturned)
    {
        Oops(__FILE__, __LINE__);
        return NULL;
    }

    if (configDesc->wTotalLength < sizeof(USB_CONFIGURATION_DESCRIPTOR))
    {
        Oops(__FILE__, __LINE__);
        return NULL;
    }

    // Now request the entire Configuration Descriptor using a dynamically
    // allocated buffer which is sized big enough to hold the entire descriptor
    //
    nBytes = sizeof(USB_DESCRIPTOR_REQUEST) + configDesc->wTotalLength;

    configDescReq = (PUSB_DESCRIPTOR_REQUEST)ALLOC(nBytes);

    if (configDescReq == NULL)
    {
        Oops(__FILE__, __LINE__);
        return NULL;
    }

    configDesc = (PUSB_CONFIGURATION_DESCRIPTOR)(configDescReq+1);

    // Indicate the port from which the descriptor will be requested
    //
    configDescReq->ConnectionIndex = ConnectionIndex;

    //
    // USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
    // IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
    //
    // USBD will automatically initialize these fields:
    //     bmRequest = 0x80
    //     bRequest  = 0x06
    //
    // We must inititialize these fields:
    //     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
    //     wIndex    = Zero (or Language ID for String Descriptors)
    //     wLength   = Length of descriptor buffer
    //
    configDescReq->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8)
                                        | DescriptorIndex;

    configDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

    // Now issue the get descriptor request.
    //

    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                              configDescReq,
                              nBytes,
                              configDescReq,
                              nBytes,
                              &nBytesReturned,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        Free(configDescReq);
        return NULL;
    }

    if (nBytes != nBytesReturned)
    {
        Oops(__FILE__, __LINE__);
        Free(configDescReq);
        return NULL;
    }

    if (configDesc->wTotalLength != (nBytes - sizeof(USB_DESCRIPTOR_REQUEST)))
    {
        Oops(__FILE__, __LINE__);
        Free(configDescReq);
        return NULL;
    }

    return configDescReq;
}

//*****************************************************************************
//
// GetBOSDescriptor()
//
// hHubDevice - Handle of the hub device containing the port from which the
// Configuration Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the BOS Descriptor will be requested.
//
//*****************************************************************************
PUSB_DESCRIPTOR_REQUEST CEnumerator::GetBOSDescriptor(HANDLE hHubDevice, ULONG ConnectionIndex)
{
    BOOL    success = 0;
    ULONG   nBytes = 0;
    ULONG   nBytesReturned = 0;

    UCHAR   bosDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST) +
                          sizeof(USB_BOS_DESCRIPTOR)];

    PUSB_DESCRIPTOR_REQUEST bosDescReq = NULL;
    PUSB_BOS_DESCRIPTOR     bosDesc = NULL;


    // Request the BOS Descriptor the first time using our
    // local buffer, which is just big enough for the BOS
    // Descriptor itself.
    //
    nBytes = sizeof(bosDescReqBuf);

    bosDescReq = (PUSB_DESCRIPTOR_REQUEST)bosDescReqBuf;
    bosDesc = (PUSB_BOS_DESCRIPTOR)(bosDescReq+1);

    // Zero fill the entire request structure
    //
    memset(bosDescReq, 0, nBytes);

    // Indicate the port from which the descriptor will be requested
    //
    bosDescReq->ConnectionIndex = ConnectionIndex;

    //
    // USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
    // IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
    //
    // USBD will automatically initialize these fields:
    //     bmRequest = 0x80
    //     bRequest  = 0x06
    //
    // We must inititialize these fields:
    //     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
    //     wIndex    = Zero (or Language ID for String Descriptors)
    //     wLength   = Length of descriptor buffer
    //
    bosDescReq->SetupPacket.wValue = (USB_BOS_DESCRIPTOR_TYPE << 8);

    bosDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

    // Now issue the get descriptor request.
    //
    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                              bosDescReq,
                              nBytes,
                              bosDescReq,
                              nBytes,
                              &nBytesReturned,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        return NULL;
    }

    if (nBytes != nBytesReturned)
    {
        Oops(__FILE__, __LINE__);
        return NULL;
    }

    if (bosDesc->wTotalLength < sizeof(USB_BOS_DESCRIPTOR))
    {
        Oops(__FILE__, __LINE__);
        return NULL;
    }

    // Now request the entire BOS Descriptor using a dynamically
    // allocated buffer which is sized big enough to hold the entire descriptor
    //
    nBytes = sizeof(USB_DESCRIPTOR_REQUEST) + bosDesc->wTotalLength;

    bosDescReq = (PUSB_DESCRIPTOR_REQUEST)ALLOC(nBytes);

    if (bosDescReq == NULL)
    {
        Oops(__FILE__, __LINE__);
        return NULL;
    }

    bosDesc = (PUSB_BOS_DESCRIPTOR)(bosDescReq+1);

    // Indicate the port from which the descriptor will be requested
    //
    bosDescReq->ConnectionIndex = ConnectionIndex;

    //
    // USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
    // IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
    //
    // USBD will automatically initialize these fields:
    //     bmRequest = 0x80
    //     bRequest  = 0x06
    //
    // We must inititialize these fields:
    //     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
    //     wIndex    = Zero (or Language ID for String Descriptors)
    //     wLength   = Length of descriptor buffer
    //
    bosDescReq->SetupPacket.wValue = (USB_BOS_DESCRIPTOR_TYPE << 8);

    bosDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

    // Now issue the get descriptor request.
    //

    success = DeviceIoControl(hHubDevice,
                              IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                              bosDescReq,
                              nBytes,
                              bosDescReq,
                              nBytes,
                              &nBytesReturned,
                              NULL);

    if (!success)
    {
        Oops(__FILE__, __LINE__);
        Free(bosDescReq);
        return NULL;
    }

    if (nBytes != nBytesReturned)
    {
        Oops(__FILE__, __LINE__);
        Free(bosDescReq);
        return NULL;
    }

    if (bosDesc->wTotalLength != (nBytes - sizeof(USB_DESCRIPTOR_REQUEST)))
    {
        Oops(__FILE__, __LINE__);
        Free(bosDescReq);
        return NULL;
    }

    return bosDescReq;
}

//*****************************************************************************
//
// AreThereStringDescriptors()
//
// DeviceDesc - Device Descriptor for which String Descriptors should be
// checked.
//
// ConfigDesc - Configuration Descriptor (also containing Interface Descriptor)
// for which String Descriptors should be checked.
//
//*****************************************************************************
BOOL CEnumerator::AreThereStringDescriptors(PUSB_DEVICE_DESCRIPTOR DeviceDesc, PUSB_CONFIGURATION_DESCRIPTOR ConfigDesc)
{
	PUCHAR                  descEnd = NULL;
	PUSB_COMMON_DESCRIPTOR  commonDesc = NULL;

	//
	// Check Device Descriptor strings
	//

	if (DeviceDesc->iManufacturer ||
		DeviceDesc->iProduct ||
		DeviceDesc->iSerialNumber
		)
	{
		return TRUE;
	}


	//
	// Check the Configuration and Interface Descriptor strings
	//

	descEnd = (PUCHAR)ConfigDesc + ConfigDesc->wTotalLength;

	commonDesc = (PUSB_COMMON_DESCRIPTOR)ConfigDesc;

	while ((PUCHAR)commonDesc + sizeof(USB_COMMON_DESCRIPTOR) < descEnd &&
		(PUCHAR)commonDesc + commonDesc->bLength <= descEnd)
	{
		switch (commonDesc->bDescriptorType)
		{
		case USB_CONFIGURATION_DESCRIPTOR_TYPE:
		case USB_OTHER_SPEED_CONFIGURATION_DESCRIPTOR_TYPE:
			if (commonDesc->bLength != sizeof(USB_CONFIGURATION_DESCRIPTOR))
			{
				Oops(__FILE__, __LINE__);
				break;
			}
			if (((PUSB_CONFIGURATION_DESCRIPTOR)commonDesc)->iConfiguration)
			{
				return TRUE;
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;

		case USB_INTERFACE_DESCRIPTOR_TYPE:
			if (commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR) &&
				commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR2))
			{
				Oops(__FILE__, __LINE__);
				break;
			}
			if (((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->iInterface)
			{
				return TRUE;
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;

		default:
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;
		}
		break;
	}

	return FALSE;
}


//*****************************************************************************
//
// GetAllStringDescriptors()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptors will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptors will be requested.
//
// DeviceDesc - Device Descriptor for which String Descriptors should be
// requested.
//
// ConfigDesc - Configuration Descriptor (also containing Interface Descriptor)
// for which String Descriptors should be requested.
//
//*****************************************************************************
PSTRING_DESCRIPTOR_NODE CEnumerator::GetAllStringDescriptors(HANDLE hHubDevice, ULONG ConnectionIndex,
	PUSB_DEVICE_DESCRIPTOR DeviceDesc, PUSB_CONFIGURATION_DESCRIPTOR ConfigDesc)
{
	PSTRING_DESCRIPTOR_NODE supportedLanguagesString = NULL;
	ULONG                   numLanguageIDs = 0;
	USHORT                  *languageIDs = NULL;

	PUCHAR                  descEnd = NULL;
	PUSB_COMMON_DESCRIPTOR  commonDesc = NULL;
	UCHAR                   uIndex = 1;
	UCHAR                   bInterfaceClass = 0;
	BOOL                    getMoreStrings = FALSE;
	HRESULT                 hr = S_OK;

	//
	// Get the array of supported Language IDs, which is returned
	// in String Descriptor 0
	//
	supportedLanguagesString = GetStringDescriptor(hHubDevice,
		ConnectionIndex,
		0,
		0);

	if (supportedLanguagesString == NULL)
	{
		return NULL;
	}

	numLanguageIDs = (supportedLanguagesString->StringDescriptor->bLength - 2) / 2;

	languageIDs = (USHORT*)&supportedLanguagesString->StringDescriptor->bString[0];

	//
	// Get the Device Descriptor strings
	//

	if (DeviceDesc->iManufacturer)
	{
		GetStringDescriptors(hHubDevice,
			ConnectionIndex,
			DeviceDesc->iManufacturer,
			numLanguageIDs,
			languageIDs,
			supportedLanguagesString);
	}

	if (DeviceDesc->iProduct)
	{
		GetStringDescriptors(hHubDevice,
			ConnectionIndex,
			DeviceDesc->iProduct,
			numLanguageIDs,
			languageIDs,
			supportedLanguagesString);
	}

	if (DeviceDesc->iSerialNumber)
	{
		GetStringDescriptors(hHubDevice,
			ConnectionIndex,
			DeviceDesc->iSerialNumber,
			numLanguageIDs,
			languageIDs,
			supportedLanguagesString);
	}

	//
	// Get the Configuration and Interface Descriptor strings
	//

	descEnd = (PUCHAR)ConfigDesc + ConfigDesc->wTotalLength;

	commonDesc = (PUSB_COMMON_DESCRIPTOR)ConfigDesc;

	while ((PUCHAR)commonDesc + sizeof(USB_COMMON_DESCRIPTOR) < descEnd &&
		(PUCHAR)commonDesc + commonDesc->bLength <= descEnd)
	{
		switch (commonDesc->bDescriptorType)
		{
		case USB_CONFIGURATION_DESCRIPTOR_TYPE:
			if (commonDesc->bLength != sizeof(USB_CONFIGURATION_DESCRIPTOR))
			{
				Oops(__FILE__, __LINE__);
				break;
			}
			if (((PUSB_CONFIGURATION_DESCRIPTOR)commonDesc)->iConfiguration)
			{
				GetStringDescriptors(hHubDevice,
					ConnectionIndex,
					((PUSB_CONFIGURATION_DESCRIPTOR)commonDesc)->iConfiguration,
					numLanguageIDs,
					languageIDs,
					supportedLanguagesString);
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;

		case USB_IAD_DESCRIPTOR_TYPE:
			if (commonDesc->bLength < sizeof(USB_IAD_DESCRIPTOR))
			{
				Oops(__FILE__, __LINE__);
				break;
			}
			if (((PUSB_IAD_DESCRIPTOR)commonDesc)->iFunction)
			{
				GetStringDescriptors(hHubDevice,
					ConnectionIndex,
					((PUSB_IAD_DESCRIPTOR)commonDesc)->iFunction,
					numLanguageIDs,
					languageIDs,
					supportedLanguagesString);
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;

		case USB_INTERFACE_DESCRIPTOR_TYPE:
			if (commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR) &&
				commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR2))
			{
				Oops(__FILE__, __LINE__);
				break;
			}
			if (((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->iInterface)
			{
				GetStringDescriptors(hHubDevice,
					ConnectionIndex,
					((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->iInterface,
					numLanguageIDs,
					languageIDs,
					supportedLanguagesString);
			}

			//
			// We need to display more string descriptors for the following
			// interface classes
			//
			bInterfaceClass = ((PUSB_INTERFACE_DESCRIPTOR)commonDesc)->bInterfaceClass;
			if (bInterfaceClass == USB_DEVICE_CLASS_VIDEO)
			{
				getMoreStrings = TRUE;
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;

		default:
			commonDesc = (PUSB_COMMON_DESCRIPTOR)((PUCHAR)commonDesc + commonDesc->bLength);
			continue;
		}
		break;
	}

	if (getMoreStrings)
	{
		//
		// We might need to display strings later that are referenced only in
		// class-specific descriptors. Get String Descriptors 1 through 32 (an
		// arbitrary upper limit for Strings needed due to "bad devices"
		// returning an infinite repeat of Strings 0 through 4) until one is not
		// found.
		//
		// There are also "bad devices" that have issues even querying 1-32, but
		// historically USBView made this query, so the query should be safe for
		// video devices.
		//
		for (uIndex = 1; SUCCEEDED(hr) && (uIndex < NUM_STRING_DESC_TO_GET); uIndex++)
		{
			hr = GetStringDescriptors(hHubDevice,
				ConnectionIndex,
				uIndex,
				numLanguageIDs,
				languageIDs,
				supportedLanguagesString);
		}
	}

	return supportedLanguagesString;
}

//*****************************************************************************
//
// GetStringDescriptor()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptor will be requested.
//
// DescriptorIndex - String Descriptor index.
//
// LanguageID - Language in which the string should be requested.
//
//*****************************************************************************

PSTRING_DESCRIPTOR_NODE CEnumerator::GetStringDescriptor(HANDLE hHubDevice, ULONG ConnectionIndex, UCHAR DescriptorIndex, USHORT LanguageID)
{
	BOOL    success = 0;
	ULONG   nBytes = 0;
	ULONG   nBytesReturned = 0;

	UCHAR   stringDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST) +
		MAXIMUM_USB_STRING_LENGTH];

	PUSB_DESCRIPTOR_REQUEST stringDescReq = NULL;
	PUSB_STRING_DESCRIPTOR  stringDesc = NULL;
	PSTRING_DESCRIPTOR_NODE stringDescNode = NULL;

	nBytes = sizeof(stringDescReqBuf);

	stringDescReq = (PUSB_DESCRIPTOR_REQUEST)stringDescReqBuf;
	stringDesc = (PUSB_STRING_DESCRIPTOR)(stringDescReq + 1);

	// Zero fill the entire request structure
	//
	memset(stringDescReq, 0, nBytes);

	// Indicate the port from which the descriptor will be requested
	//
	stringDescReq->ConnectionIndex = ConnectionIndex;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	stringDescReq->SetupPacket.wValue = (USB_STRING_DESCRIPTOR_TYPE << 8)
		| DescriptorIndex;

	stringDescReq->SetupPacket.wIndex = LanguageID;

	stringDescReq->SetupPacket.wLength = (USHORT)(nBytes - sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//
	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
		stringDescReq,
		nBytes,
		stringDescReq,
		nBytes,
		&nBytesReturned,
		NULL);

	//
	// Do some sanity checks on the return from the get descriptor request.
	//

	if (!success)
	{
		Oops(__FILE__, __LINE__);
		return NULL;
	}

	if (nBytesReturned < 2)
	{
		Oops(__FILE__, __LINE__);
		return NULL;
	}

	if (stringDesc->bDescriptorType != USB_STRING_DESCRIPTOR_TYPE)
	{
		Oops(__FILE__, __LINE__);
		return NULL;
	}

	if (stringDesc->bLength != nBytesReturned - sizeof(USB_DESCRIPTOR_REQUEST))
	{
		Oops(__FILE__, __LINE__);
		return NULL;
	}

	if (stringDesc->bLength % 2 != 0)
	{
		Oops(__FILE__, __LINE__);
		return NULL;
	}

	//
	// Looks good, allocate some (zero filled) space for the string descriptor
	// node and copy the string descriptor to it.
	//

	stringDescNode = (PSTRING_DESCRIPTOR_NODE)ALLOC(sizeof(STRING_DESCRIPTOR_NODE) +
		stringDesc->bLength);

	if (stringDescNode == NULL)
	{
		Oops(__FILE__, __LINE__);
		return NULL;
	}

	stringDescNode->DescriptorIndex = DescriptorIndex;
	stringDescNode->LanguageID = LanguageID;

	memcpy(stringDescNode->StringDescriptor,
		stringDesc,
		stringDesc->bLength);

	return stringDescNode;
}

//*****************************************************************************
//
// GetStringDescriptors()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptor will be requested.
//
// DescriptorIndex - String Descriptor index.
//
// NumLanguageIDs -  Number of languages in which the string should be
// requested.
//
// LanguageIDs - Languages in which the string should be requested.
//
// StringDescNodeHead - First node in linked list of device's string descriptors
//
// Return Value: HRESULT indicating whether the string is on the list
//
//*****************************************************************************
HRESULT CEnumerator::GetStringDescriptors(
	_In_ HANDLE                         hHubDevice,
	_In_ ULONG                          ConnectionIndex,
	_In_ UCHAR                          DescriptorIndex,
	_In_ ULONG                          NumLanguageIDs,
	_In_reads_(NumLanguageIDs) USHORT  *LanguageIDs,
	_In_ PSTRING_DESCRIPTOR_NODE        StringDescNodeHead
)
{
	PSTRING_DESCRIPTOR_NODE tail = NULL;
	PSTRING_DESCRIPTOR_NODE trailing = NULL;
	ULONG i = 0;

	//
	// Go to the end of the linked list, searching for the requested index to
	// see if we've already retrieved it
	//
	for (tail = StringDescNodeHead; tail != NULL; tail = tail->Next)
	{
		if (tail->DescriptorIndex == DescriptorIndex)
		{
			return S_OK;
		}

		trailing = tail;
	}

	tail = trailing;

	//
	// Get the next String Descriptor. If this is NULL, then we're done (return)
	// Otherwise, loop through all Language IDs
	//
	for (i = 0; (tail != NULL) && (i < NumLanguageIDs); i++)
	{
		tail->Next = GetStringDescriptor(hHubDevice,
			ConnectionIndex,
			DescriptorIndex,
			LanguageIDs[i]);

		tail = tail->Next;
	}

	if (tail == NULL)
	{
		return E_FAIL;
	}
	else {
		return S_OK;
	}
}

//*****************************************************************************
//
// GetHostControllerPowerMap()
//
// HANDLE hHCDev
//      - handle to USB Host Controller
//
// PUSBHOSTCONTROLLERINFO hcInfo
//      - data structure to receive the Power Map Info
//
// return DWORD dwError
//      - return ERROR_SUCCESS or last error
//
//*****************************************************************************
DWORD CEnumerator::GetHostControllerPowerMap(HANDLE hHCDev, PUSBHOSTCONTROLLERINFO hcInfo)
{
	USBUSER_POWER_INFO_REQUEST UsbPowerInfoRequest;
	PUSB_POWER_INFO            pUPI = &UsbPowerInfoRequest.PowerInformation;
	DWORD                      dwError = 0;
	DWORD                      dwBytes = 0;
	BOOL                       bSuccess = FALSE;
	int                        nIndex = 0;
	int                        nPowerState = WdmUsbPowerSystemWorking;

	for (; nPowerState <= WdmUsbPowerSystemShutdown; nIndex++, nPowerState++)
	{
		// zero initialize our request
		memset(&UsbPowerInfoRequest, 0, sizeof(UsbPowerInfoRequest));

		// set the header and request sizes
		UsbPowerInfoRequest.Header.UsbUserRequest = USBUSER_GET_POWER_STATE_MAP;
		UsbPowerInfoRequest.Header.RequestBufferLength = sizeof(UsbPowerInfoRequest);
		UsbPowerInfoRequest.PowerInformation.SystemState = (WDMUSB_POWER_STATE)nPowerState;

		//
		// Now query USBHUB for the USB_POWER_INFO structure for this hub.
		// For Selective Suspend support
		//
		bSuccess = DeviceIoControl(hHCDev,
			IOCTL_USB_USER_REQUEST,
			&UsbPowerInfoRequest,
			sizeof(UsbPowerInfoRequest),
			&UsbPowerInfoRequest,
			sizeof(UsbPowerInfoRequest),
			&dwBytes,
			NULL);

		if (!bSuccess)
		{
			dwError = GetLastError();
			Oops(__FILE__, __LINE__);
		}
		else
		{
			// copy the data into our USB Host Controller's info structure
			memcpy(&(hcInfo->USBPowerInfo[nIndex]), pUPI, sizeof(USB_POWER_INFO));
		}
	}

	return dwError;
}

void CEnumerator::EnumerateAllDevices()
{
	EnumerateAllDevicesWithGuid(&CEnumerator::g_lsDeviceList,
		(LPGUID)&GUID_DEVINTERFACE_USB_DEVICE);

	EnumerateAllDevicesWithGuid(&CEnumerator::g_lsHubList,
		(LPGUID)&GUID_DEVINTERFACE_USB_HUB); 

	// It is added - for Hmarka's project specific:
//	EnumerateAllDevicesWithGuid(&CEnumerator::g_lsVolumeList,
//		(LPGUID)&GUID_DEVINTERFACE_VOLUME);
}

//*****************************************************************************
//
// GetHostControllerInfo()
//
// HANDLE hHCDev
//      - handle to USB Host Controller
//
// PUSBHOSTCONTROLLERINFO hcInfo
//      - data structure to receive the Power Map Info
//
// return DWORD dwError
//      - return ERROR_SUCCESS or last error
//
//*****************************************************************************
DWORD CEnumerator::GetHostControllerInfo(HANDLE hHCDev, PUSBHOSTCONTROLLERINFO hcInfo)
{
	USBUSER_CONTROLLER_INFO_0 UsbControllerInfo;
	DWORD                      dwError = 0;
	DWORD                      dwBytes = 0;
	BOOL                       bSuccess = FALSE;

	memset(&UsbControllerInfo, 0, sizeof(UsbControllerInfo));

	// set the header and request sizes
	UsbControllerInfo.Header.UsbUserRequest = USBUSER_GET_CONTROLLER_INFO_0;
	UsbControllerInfo.Header.RequestBufferLength = sizeof(UsbControllerInfo);

	//
	// Query for the USB_CONTROLLER_INFO_0 structure
	//
	bSuccess = DeviceIoControl(hHCDev,
		IOCTL_USB_USER_REQUEST,
		&UsbControllerInfo,
		sizeof(UsbControllerInfo),
		&UsbControllerInfo,
		sizeof(UsbControllerInfo),
		&dwBytes,
		NULL);

	if (!bSuccess)
	{
		dwError = GetLastError();
		Oops(__FILE__, __LINE__);
	}
	else
	{
		hcInfo->ControllerInfo = (PUSB_CONTROLLER_INFO_0)ALLOC(sizeof(USB_CONTROLLER_INFO_0));
		if (NULL == hcInfo->ControllerInfo)
		{
			dwError = GetLastError();
			Oops(__FILE__, __LINE__);
		}
		else
		{
			// copy the data into our USB Host Controller's info structure
			memcpy(hcInfo->ControllerInfo, &UsbControllerInfo.Info0, sizeof(USB_CONTROLLER_INFO_0));
		}
	}
	return dwError;
}

void CEnumerator::EnumerateAllDevicesWithGuid(PDEVICE_GUID_LIST DeviceList, LPGUID Guid)
{
	if (DeviceList->DeviceInfo != INVALID_HANDLE_VALUE)
	{
		ClearDeviceList(DeviceList);
	}

	DeviceList->DeviceInfo = SetupDiGetClassDevsA(Guid,
		NULL,
		NULL,
		(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

	if (DeviceList->DeviceInfo != INVALID_HANDLE_VALUE)
	{
		ULONG                    index;
		DWORD error;

		error = 0;
		index = 0;

		while (error != ERROR_NO_MORE_ITEMS)
		{
			BOOL success;
			PDEVICE_INFO_NODE pNode;

			pNode = (PDEVICE_INFO_NODE)ALLOC(sizeof(DEVICE_INFO_NODE));
			if (pNode == NULL)
			{
				Oops(__FILE__, __LINE__);
				break;
			}
			pNode->DeviceInfo = DeviceList->DeviceInfo;
			pNode->DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
			pNode->DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

			success = SetupDiEnumDeviceInfo(DeviceList->DeviceInfo,
				index,
				&pNode->DeviceInfoData);

			index++;

			if (success == FALSE)
			{
				error = GetLastError();

				if (error != ERROR_NO_MORE_ITEMS)
				{
					Oops(__FILE__, __LINE__);
				}

				FreeDeviceInfoNode(&pNode);
			}
			else
			{
				BOOL   bResult;
				ULONG  requiredLength;

				bResult = CDevNode::GetDeviceProperty(DeviceList->DeviceInfo,
					&pNode->DeviceInfoData,
					SPDRP_DEVICEDESC,
					(LPTSTR*)&pNode->DeviceDescName);
				if (bResult == FALSE)
				{
					FreeDeviceInfoNode(&pNode);
					Oops(__FILE__, __LINE__);
					break;
				}

				bResult = CDevNode::GetDeviceProperty(DeviceList->DeviceInfo,
					&pNode->DeviceInfoData,
					SPDRP_DRIVER,
					(LPTSTR*)&pNode->DeviceDriverName);
				if (bResult == FALSE)
				{
					FreeDeviceInfoNode(&pNode);
					Oops(__FILE__, __LINE__);
					break;
				}

				pNode->DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

				success = SetupDiEnumDeviceInterfaces(DeviceList->DeviceInfo,
					0,
					Guid,
					index - 1,
					&pNode->DeviceInterfaceData);
				if (!success)
				{
					FreeDeviceInfoNode(&pNode);
					Oops(__FILE__, __LINE__);
					break;
				}

				success = SetupDiGetDeviceInterfaceDetailA(DeviceList->DeviceInfo,
					&pNode->DeviceInterfaceData,
					NULL,
					0,
					&requiredLength,
					NULL);

				error = GetLastError();

				if (!success && error != ERROR_INSUFFICIENT_BUFFER)
				{
					FreeDeviceInfoNode(&pNode);
					Oops(__FILE__, __LINE__);
					break;
				}

				pNode->DeviceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)ALLOC(requiredLength);

				if (pNode->DeviceDetailData == NULL)
				{
					FreeDeviceInfoNode(&pNode);
					Oops(__FILE__, __LINE__);
					break;
				}

				pNode->DeviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

				success = SetupDiGetDeviceInterfaceDetailA(DeviceList->DeviceInfo,
					&pNode->DeviceInterfaceData,
					pNode->DeviceDetailData,
					requiredLength,
					&requiredLength,
					NULL);
				if (!success)
				{
					FreeDeviceInfoNode(&pNode);
					Oops(__FILE__, __LINE__);
					break;
				}

				InsertTailList(&DeviceList->ListHead, &pNode->ListEntry);
			}
		}
	}
}

void CEnumerator::FreeDeviceInfoNode(_In_ PDEVICE_INFO_NODE *ppNode)
{
	if (ppNode == NULL)
	{
		return;
	}

	if (*ppNode == NULL)
	{
		return;
	}

	if ((*ppNode)->DeviceDetailData != NULL)
	{
		Free((*ppNode)->DeviceDetailData);
	}

	if ((*ppNode)->DeviceDescName != NULL)
	{
		Free((*ppNode)->DeviceDescName);
	}

	if ((*ppNode)->DeviceDriverName != NULL)
	{
		Free((*ppNode)->DeviceDriverName);
	}

	Free(*ppNode);
	*ppNode = NULL;
}

PDEVICE_INFO_NODE CEnumerator::FindMatchingDeviceNodeForDriverName(_In_ PSTR DriverKeyName, _In_ BOOLEAN IsHub)
{
	PDEVICE_INFO_NODE pNode = NULL;
	PDEVICE_GUID_LIST pList = NULL;
	PLIST_ENTRY       pEntry = NULL;

	pList = IsHub ? &CEnumerator::g_lsHubList : &CEnumerator::g_lsDeviceList;

	pEntry = pList->ListHead.Flink;

	while (pEntry != &pList->ListHead)
	{
		pNode = CONTAINING_RECORD(pEntry,
			DEVICE_INFO_NODE,
			ListEntry);
		if (_stricmp(DriverKeyName, pNode->DeviceDriverName) == 0)
		{
			return pNode;
		}

		pEntry = pEntry->Flink;
	}

	return NULL;
}

USB_DEVICE_KIND CEnumerator::PrepareKindOfDevice(bool bIsMemoryDevice, bool bIsCompositeDevice, bool bIsHIDUsbDevice)
{
	USB_DEVICE_KIND kindOfDevice = USB_DEVICE_KIND::UNKNOWN;
	if (bIsMemoryDevice)
	{
		kindOfDevice = USB_DEVICE_KIND::KIND_MEMORY;
		return kindOfDevice;
	}
	else if (bIsCompositeDevice)
	{
		kindOfDevice = USB_DEVICE_KIND::KIND_COMPOSITE;
		return kindOfDevice;
	}
	else if (bIsHIDUsbDevice)
	{
		kindOfDevice = USB_DEVICE_KIND::KIND_HID;
		return kindOfDevice;
	}
	return kindOfDevice;
}

/*****************************************************************************

AddLeaf()

*****************************************************************************/
SP_USB_DEV CEnumerator::AddLeaf(SP_USB_DEV spTreeParent, LPARAM lParam, _In_ LPTSTR lpszText, USB_ENGINE_CATEGORY usbCateg, bool bIsHub)
{	
	int nRootHub = 0;
	int iPort = CDevNode::ParsePortNumber((char*)lpszText);
	if (iPort == (-1))
		nRootHub = CDevNode::ParseRootHub((char*)lpszText);

	std::string sName((char*)lpszText); // Use as a "TypeOfDevice"

	int nHubIndex = 0;
	if (!bIsHub)
		nHubIndex = spTreeParent->GetParentHub(); 

	bool bIsRootHub = false;
	std::vector<std::string> vectVolumePath;	// Drive letter, or path (if mounted disk)
	std::string sGuidOfVolume;					// GUID of drive or path
	std::string sDeviceId, sVendor, sManufacturer, sProduct, sSNOfDevice;
	bool bIsMemoryDevice = false; // "USBSTOR"
	bool bIsCompositeDevice = false; // "usbccgp"
	bool bIsHIDUsbDevice = false; // "HidUsb"
	auto* ptrDeviceInfo = reinterpret_cast<USBDEVICEINFO*>(lParam);
	if (ptrDeviceInfo != nullptr && iPort != (-1))
	{
		if (ptrDeviceInfo->DeviceInfoNode != nullptr)
		{
			auto* pDINode = static_cast<PDEVICE_INFO_NODE>(ptrDeviceInfo->DeviceInfoNode);
			if (pDINode != nullptr)
			{
				GUID guid = ptrDeviceInfo->DeviceInfoNode->DeviceInterfaceData.InterfaceClassGuid;
				if (g_guidUsbDevice == guid)
				{
					if (ptrDeviceInfo->UsbDeviceProperties)
					{
						g_log.SaveLogFile(LOG_MODES::DEBUGING, "CEnumerator::AddLeaf_1: UsbDeviceProperties->Service: '%s'",
							ptrDeviceInfo->UsbDeviceProperties->Service);

						if (0 == strcmp(ptrDeviceInfo->UsbDeviceProperties->Service, "USBSTOR")) // It is MEMORY!
							bIsMemoryDevice = true;

						if (0 == strcmp(ptrDeviceInfo->UsbDeviceProperties->Service, "usbccgp")) // It is CompositeDevice!
							bIsCompositeDevice = true; 

						if (0 == strcmp(ptrDeviceInfo->UsbDeviceProperties->Service, "HidUsb")) // USB human interface device class!
							bIsHIDUsbDevice = true;
					}
				}
			}
		}
		if (ptrDeviceInfo->UsbDeviceProperties != nullptr)
		{
			sDeviceId = CEnumerator::ConvertDeviceId(ptrDeviceInfo->UsbDeviceProperties->DeviceId);
			if (!bIsHub) // Do not log HUBs at this point:
				g_log.SaveLogFile(LOG_MODES::DEBUGING, "CEnumerator::AddLeaf_2: Device ID (S/N): '%s'", sDeviceId.c_str());
			
			if (bIsMemoryDevice || bIsCompositeDevice)
			{
				auto pairData = m_spUsbVolume->RetrieveDiskVolumeData(sDeviceId); 
				sGuidOfVolume = pairData.first;
				vectVolumePath = pairData.second;				
			}
		}
		if (ptrDeviceInfo->ConnectionInfo != nullptr)
		{			
			auto* pDeviceInfo = reinterpret_cast<USBDEVICEINFO*>(lParam);
			PSTRING_DESCRIPTOR_NODE pStringDescs = ((PUSBDEVICEINFO)pDeviceInfo)->StringDescs;
				
			sManufacturer = RetrieveUSEnglishStringDescriptor(ptrDeviceInfo->ConnectionInfo->DeviceDescriptor.iManufacturer, pStringDescs);
			sProduct = RetrieveUSEnglishStringDescriptor(ptrDeviceInfo->ConnectionInfo->DeviceDescriptor.iProduct, pStringDescs);
			sSNOfDevice = RetrieveUSEnglishStringDescriptor(ptrDeviceInfo->ConnectionInfo->DeviceDescriptor.iSerialNumber, pStringDescs);

			if (ptrDeviceInfo->ConnectionInfo->DeviceDescriptor.idVendor != 0)
				sVendor = GetVendorString(ptrDeviceInfo->ConnectionInfo->DeviceDescriptor.idVendor);
				// Do NOT use: RetrieveUSEnglishStringDescriptor(ptrDeviceInfo->ConnectionInfo->DeviceDescriptor.idVendor, pStringDescs);
			else
				sVendor = "-";
		}
	}
	if (iPort == (-1)) // Undefined Port for nodes: Host-Controller or RootHub:
	{
		if (!bIsHub) // It is Host-Controller:
		{
			auto* ptrHostControllerInfo = reinterpret_cast<USBHOSTCONTROLLERINFO*>(lParam);
			if (ptrHostControllerInfo != nullptr) 
			{		
				ULONG nDeviceID = ptrHostControllerInfo->DeviceID;
				sDeviceId = CEnumerator::ConvertHostId(nDeviceID); 
				nHubIndex = nDeviceID % 1000;
				nHubIndex -= GetRootHubBaseOffset();
			}
		}
		else if (nRootHub > 0) // It is RootHub:
		{
			auto* ptrUsbRootHubInfo = reinterpret_cast<USBROOTHUBINFO*>(lParam);
			if (ptrUsbRootHubInfo != nullptr)
			{
				sDeviceId = spTreeParent->GetDeviceId() + "ROOT";	
				nHubIndex = spTreeParent->GetParentHub(); 
				bIsRootHub = true;
			}
		}
	}
	if (bIsHub && (nRootHub == 0)) // It is External Hub (NO "RootHub"):
	{
		bool bExtHubUseDeviceAddr = GetIsExtHubUseDeviceAddr();
		if (bExtHubUseDeviceAddr) // Use "DeviceAddress":
		{
			USHORT nDeviceAddress = 0;
			auto* ptrUsbExtHubInfo = reinterpret_cast<USBEXTERNALHUBINFO*>(lParam);
			if (ptrUsbExtHubInfo != nullptr)
			{
				if (ptrUsbExtHubInfo->ConnectionInfo)
				{
					nDeviceAddress = ptrUsbExtHubInfo->ConnectionInfo->DeviceAddress;
					nHubIndex = nDeviceAddress;
				}
			}
		}
		else // Use check-summ calculation:
		{
			short nCkeckSumm = CDevNode::CalculateCheckSumm(sDeviceId);
			nHubIndex = nCkeckSumm % 1000;
			nHubIndex -= GetExtHubBaseOffset();
		}
	}
	// The var 'sName' - it is 'sTypeOfDevice' for USBEnum::USBDevice and USBEnum::USBHub c-tor:
	SP_USB_DEV spData(bIsHub ?
		dynamic_cast<USBEnum::IUSBDevice*>(new USBEnum::USBHub(nHubIndex, sName, bIsRootHub)) :
		dynamic_cast<USBEnum::IUSBDevice*>(new USBEnum::USBDevice(nHubIndex, iPort, bIsHub, usbCateg, sName))); 

	/* spData->SetParentDevice(spTreeParent); */ // VLD-errors (if use this string)!!!
	spData->SetDeviceKind(PrepareKindOfDevice(bIsMemoryDevice, bIsCompositeDevice, bIsHIDUsbDevice));
	spData->SetDeviceId(sDeviceId); 

	if (bIsMemoryDevice || bIsCompositeDevice)
	{
		spData->SetVectorVolumePath(vectVolumePath);
		spData->SetGuidOfVolume(sGuidOfVolume);
	}

	spData->SetVendorOfDevice(sVendor); 
	spData->SetManufacturer(sManufacturer);
	spData->SetProductName(sProduct); 
	spData->SetSNOfDevice(sSNOfDevice);

	spTreeParent->AddChild(spData);
	return spData;
}

std::string CEnumerator::ConvertDeviceId(const std::string& sRawDeviceId)
{
	int nLength = sRawDeviceId.length(); 
		
	char charForSearch = 0x5C; // Symbol: "\"
	int iIndex = sRawDeviceId.rfind(&charForSearch, (nLength - 1), 1); 
	
	std::string sResult;
	while (iIndex < (nLength - 1))
	{
		char chOut = sRawDeviceId[++iIndex];
		sResult.append(&chOut, 1);
	}
	
	return sResult;
}

std::string CEnumerator::ConvertHostId(ULONG nHostID)
{	// Convertion from Decimal to hex:
	std::stringstream ss;
	ss << std::hex << (int)(nHostID);
	std::string sResult(ss.str());
	std::transform(sResult.begin(), sResult.end(), sResult.begin(), ::toupper);
	return sResult;
}

/*****************************************************************************

GetVendorString()

idVendor - USB Vendor ID

Return Value - Vendor name string associated with idVendor, or NULL if
no vendor name string is found which is associated with idVendor.

*****************************************************************************/
const char* CEnumerator::GetVendorString(USHORT idVendor)
{
	PVENDOR_ID vendorID = NULL;

	if (idVendor == 0x0000)
	{
		return NULL;
	}

	vendorID = USBVendorIDs;

	while (vendorID->usVendorID != 0x0000)
	{
		if (vendorID->usVendorID == idVendor)
		{
			break;
		}
		vendorID++;
	}

	return (vendorID->szVendor);
}

/*****************************************************************************
RetrieveUSEnglishStringDescriptor(
Former: DisplayUSEnglishStringDescriptor() 
//@@DisplayUSEnglishStringDescriptor - String Descriptor
*****************************************************************************/
std::string CEnumerator::RetrieveUSEnglishStringDescriptor(UCHAR Index, PSTRING_DESCRIPTOR_NODE USStringDescs)
{
	ULONG nBytes = 0;
	BOOLEAN FoundMatchingString = FALSE;
	char  pString[512];
	std::string sResult;
	for (; USStringDescs; USStringDescs = USStringDescs->Next)
	{
		if (USStringDescs->DescriptorIndex == Index && USStringDescs->LanguageID == 0x0409)
		{
			FoundMatchingString = TRUE;
				
			memset(pString, 0, 512);
			nBytes = WideCharToMultiByte(
				CP_ACP,     // CodePage
				WC_NO_BEST_FIT_CHARS,
				USStringDescs->StringDescriptor->bString,
				(USStringDescs->StringDescriptor->bLength - 2) / 2,
				pString,
				512,
				NULL,       // lpDefaultChar
				NULL);      // pUsedDefaultChar
			if (nBytes)
				sResult = pString;
			else
				sResult = "-";
			return sResult;
		}
	}

	sResult = "-";
	/*if (!FoundMatchingString)
	{
		if (LatestDevicePowerState == PowerDeviceD0)
		{
			AppendTextBuffer("*!*ERROR:  No String Descriptor for index %d!\r\n", Index);
			Oops(__FILE__, __LINE__);
		}
		else
		{
			AppendTextBuffer("String Descriptor for index %d not available while device is in low power state.\r\n", Index);
		}
	}
	else
	{
		AppendTextBuffer("*!*ERROR:  The index selected does not support English(US)\r\n");
		Oops(__FILE__, __LINE__);
	}*/
	return sResult;
}