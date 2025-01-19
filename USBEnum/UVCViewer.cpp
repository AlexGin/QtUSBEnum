#include "USBEnum\UVCViewer.h"
#include "USBEnum\Enumerator.h"
#include "USBEnum\DevNode.h"
#include "LogFile.h"

extern CLogFile g_log;
extern LIST_ENTRY EnumeratedHCListHead;

#define RemoveEntryList(Entry) {\
    PLIST_ENTRY _EX_Blink;\
    PLIST_ENTRY _EX_Flink;\
    _EX_Flink = (Entry)->Flink;\
    _EX_Blink = (Entry)->Blink;\
    _EX_Blink->Flink = _EX_Flink;\
    _EX_Flink->Blink = _EX_Blink;\
    }

///////////////////////////////////////////////////////////////////////////////
CUVCViewer::CUVCViewer(SP_USB_DEV spUsbTree, int nGuidFlags,
	const USB_ENUM_SETTINGS& usbEnumSettings, const MONITORING_THREAD_SETTINGS& thrSettings)
	: m_spUsbTree(spUsbTree), m_nGuidFlags(nGuidFlags)
{	
	m_spUsbVolume.reset(new CUsbVolume(thrSettings));
	m_pEnumerator = new CEnumerator(m_spUsbVolume, usbEnumSettings); 

	bool bMonitorThread = m_spUsbVolume->GetIsMonitoringThreadEnable();
	if (!bMonitorThread) // Use TimerMode (instead of mutithreading mode):
	{
		UINT nPoolintTimePeriod = m_spUsbVolume->GetPoolingWaitTimeMs();
		m_pTimer = new QTimer(this);
		connect(m_pTimer, SIGNAL(timeout()), this, SLOT(OnTimerMonitorProc()));
		m_pTimer->start((int)nPoolintTimePeriod);
	}
}

CUVCViewer::~CUVCViewer()
{
	m_spUsbVolume->Clear();

	if (m_pEnumerator != nullptr)
	{
		delete m_pEnumerator;
		m_pEnumerator = nullptr;
	}
}

void CUVCViewer::OnTimerMonitorProc()
{
	if (m_spUsbVolume)
	{
		m_spUsbVolume->TimerMonitorProc();
	}
}

//////////////////////////////////////////////////////////////////////////////////
void CUVCViewer::InitViewer(HWND hWnd)
{
	//DEV_BROADCAST_DEVICEINTERFACE_A   broadcastInterface; 

	//// Register to receive notification when a USB device is plugged in.
	//broadcastInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	//broadcastInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE; 

	//memcpy(&(broadcastInterface.dbcc_classguid),
	//	&(GUID_DEVINTERFACE_USB_DEVICE),
	//	sizeof(struct _GUID));

	//m_hNotifyDevHandle = RegisterDeviceNotificationA(hWnd,
	//	&broadcastInterface,
	//	DEVICE_NOTIFY_WINDOW_HANDLE); 

	//// Now register for Hub notifications.
	//memcpy(&(broadcastInterface.dbcc_classguid),
	//	&(GUID_CLASS_USBHUB),
	//	sizeof(struct _GUID));

	//m_hNotifyHubHandle = RegisterDeviceNotificationA(hWnd,
	//	&broadcastInterface,
	//	DEVICE_NOTIFY_WINDOW_HANDLE); 

	HDEVNOTIFY hDevNotify;
	DEV_BROADCAST_DEVICEINTERFACE_A NotificationFilter;
	ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
	NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	int nBitFlag = 1;
	for (int i = 0; i < sizeof(GUID_DEVINTERFACE_LIST) / sizeof(GUID); i++) 
	{
		if (m_nGuidFlags & nBitFlag)
		{
			NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_LIST[i];
			QUuid guid(NotificationFilter.dbcc_classguid);
			hDevNotify = RegisterDeviceNotificationA(hWnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
			if (!hDevNotify)
			{
				DWORD dwErrorCode = GetLastError();
				g_log.SaveLogFile(LOG_MODES::ERRORLOG, "CUVCViewer::InitViewer: Can't register device notification: ErrorCode = %u",
					dwErrorCode);
				return;
			}
			else
			{
				QString strGuid = guid.toString(); // QUuid::WithBraces);
				std::string sGuid = strGuid.toStdString();
				g_log.SaveLogFile(LOG_MODES::DEBUGING, "CUVCViewer::InitViewer: GUID %s register success!", sGuid.c_str());
			}
		}
		nBitFlag = nBitFlag << 1;
	}

	ListsInit();

	RefreshTree(true); // Refresh with Paths
}

void CUVCViewer::ListsInit()
{
	CEnumerator::g_lsHubList.DeviceInfo = INVALID_HANDLE_VALUE;
	InitializeListHead(&CEnumerator::g_lsHubList.ListHead);
	CEnumerator::g_lsDeviceList.DeviceInfo = INVALID_HANDLE_VALUE;
	InitializeListHead(&CEnumerator::g_lsDeviceList.ListHead);
}

void CUVCViewer::ListsClear()
{
	CEnumerator::ClearDeviceList(&CEnumerator::g_lsDeviceList);
	CEnumerator::ClearDeviceList(&CEnumerator::g_lsHubList); 
	
	::EnumeratedHCListHead.Blink = &::EnumeratedHCListHead;
	::EnumeratedHCListHead.Flink = &::EnumeratedHCListHead;
}

void CUVCViewer::OnDeviceChangeMonitorLock()
{
	m_spUsbVolume->OnDeviceChangeMonitorLock();
}

void CUVCViewer::OnRefreshComplete()
{
	m_spUsbVolume->OnRefreshComplete();
}

void CUVCViewer::StopMonitoringThread()
{
	m_spUsbVolume->StopMonitoringThread();
}

void CUVCViewer::RefreshTree(bool bRefreshWithPathes)
{	
	if (bRefreshWithPathes)
	{
		m_spUsbVolume->Clear(); 
		DWORD dwCoolCheckSumm = 0;
		m_spUsbVolume->RemovableEnumerate(&dwCoolCheckSumm);
		if (dwCoolCheckSumm > 0)
		{
			emit ChangeDecoded(dwCoolCheckSumm);
		}
	}

	ListsClear();
	ULONG devicesConnected;
	m_pEnumerator->EnumerateHostControllers(m_spUsbTree, &devicesConnected);
}
