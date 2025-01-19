#pragma once

#include <windows.h>
#include <windowsx.h>
#include <initguid.h>
#include <devioctl.h>
#include <dbt.h>
#include <stdio.h>
#include <stddef.h>
#include <commctrl.h>
#include <usbioctl.h>
#include <usbiodef.h>
#include <intsafe.h>
#include <strsafe.h>
#include <specstrings.h>
#include <usb.h>
#include <usbuser.h>
#include <basetyps.h>
#include <wtypes.h>
#include <objbase.h>
#include <io.h>
#include <conio.h>
#include <shellapi.h>
#include <cfgmgr32.h>
#include <shlwapi.h>
#include <setupapi.h>
#include <devpkey.h>
#include <math.h>
#include <memory>
#include <tuple>

#include <QObject> 
#include <QString> 
#include <QTimer>
#include <QUuid>

#include "USBEnum\DataTypes.h"
#include "USBEnum\USBDevice.h"
#include "USBEnum\UsbVolume.h" 

using MONITORING_THREAD_SETTINGS = std::tuple<bool, UINT, UINT, bool>;
// bool bMonitoringThreadEnable, uint nPoolingWaitTimeMs, uint nStopWaitTimeMs, bool bSaveTimeLogEnable

FORCEINLINE
void InitializeListHead(_Out_ PLIST_ENTRY ListHead)
{
	ListHead->Flink = ListHead->Blink = ListHead;
}

class CEnumerator;

class CUVCViewer : public QObject
{
	Q_OBJECT
public:
	CUVCViewer(SP_USB_DEV spUsbTree, int nGuidFlags,
		const USB_ENUM_SETTINGS& usbEnumSettings, const MONITORING_THREAD_SETTINGS& thrSettings);
	~CUVCViewer();
	void InitViewer(HWND hWnd); // USBView_OnInitDialog
	void ListsInit();
	void ListsClear();
	void RefreshTree(bool bRefreshWithPathes);
	void StopMonitoringThread();
private: 
	QTimer* m_pTimer;
	int m_nGuidFlags;
	SP_USB_VOL m_spUsbVolume;
	CEnumerator* m_pEnumerator;
	HDEVNOTIFY m_hNotifyDevHandle;
	HDEVNOTIFY m_hNotifyHubHandle; 
	SP_USB_DEV m_spUsbTree;
public slots: 
	void OnTimerMonitorProc();
	void OnDeviceChangeMonitorLock();
	void OnRefreshComplete();
signals:
	void ChangeDecoded(DWORD dwCheckSumm);
};

// Copy from HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\DeviceClasses
static const GUID GUID_DEVINTERFACE_LIST[] =
{
	// GUID_DEVINTERFACE_USB_DEVICE
	{ 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } },

	// GUID_DEVINTERFACE_DISK
	{ 0x53f56307, 0xb6bf, 0x11d0, { 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b } },

	// GUID_DEVINTERFACE_USB_HUB: {F18A0E88-C30C-11D0-8815-00A0C906BED8}
	{0xF18A0E88, 0xC30C, 0x11D0,  { 0x88, 0x15, 0x00, 0xA0, 0xC9, 0x06, 0xBE, 0xD8 } },

	// GUID_DEVINTERFACE_HID, 
	{ 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } },

	// GUID_NDIS_LAN_CLASS
	{ 0xad498944, 0x762f, 0x11d0, { 0x8d, 0xcb, 0x00, 0xc0, 0x4f, 0xc3, 0x35, 0x8c } }

	//// GUID_DEVINTERFACE_COMPORT
	//{ 0x86e0d1e0, 0x8089, 0x11d0, { 0x9c, 0xe4, 0x08, 0x00, 0x3e, 0x30, 0x1f, 0x73 } },

	//// GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR
	//{ 0x4D36E978, 0xE325, 0x11CE, { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 } },

	//// GUID_DEVINTERFACE_PARALLEL
	//{ 0x97F76EF0, 0xF883, 0x11D0, { 0xAF, 0x1F, 0x00, 0x00, 0xF8, 0x00, 0x84, 0x5C } },

	//// GUID_DEVINTERFACE_PARCLASS
	//{ 0x811FC6A5, 0xF728, 0x11D0, { 0xA5, 0x37, 0x00, 0x00, 0xF8, 0x75, 0x3E, 0xD1 } }
};