#include "USBEnum\UsbVolume.h"
#include "USBEnum\DevNode.h"

#include <process.h>	// _beginthreadex
#include <algorithm>    // std::find_if

// see:
// https://docs.microsoft.com/en-us/windows/desktop/fileio/displaying-volume-paths
// also:
// https://gist.github.com/gabonator/2499496
// http://www.codeproject.com/KB/system/RemoveDriveByLetter.aspx

#include "USBEnumEventFilter.h"
#include "LogFile.h"

extern CLogFile g_log;
extern void Free(_Frees_ptr_opt_ HGLOBAL hMem);
extern std::shared_ptr<USBEnumEventFilter> g_spUSBEnumEventFilter;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UINT CUsbVolume::m_nThreadID;
HANDLE CUsbVolume::m_evntStopMonitoringThread;
HANDLE CUsbVolume::m_evntDeviceChangeMonitorLock; 
HANDLE CUsbVolume::m_evntMainThreadLock;

UINT __stdcall CUsbVolume::UsbVolumeThreadProc(LPVOID pParam)
{
	try
	{
		CUsbVolume* pUsbVolume = reinterpret_cast<CUsbVolume*>(pParam);
		if (!pUsbVolume)
		{
			g_log.SaveLogFile(LOG_MODES::ERRORLOG, "CUsbVolume::UsbVolumeThreadProc - Invalid pParam");
			return 1;
		}
		UINT nPoolingWaitTimeMs = pUsbVolume->GetPoolingWaitTimeMs(); 
		/* UINT nChangeDriveWaitTimeMs = pUsbVolume->GetChangeDriveWaitTimeMs(); */
		bool bIsSaveTimeLogEnable = pUsbVolume->GetIsSaveTimeLogEnable();
		int iCode = -1; 		
		do
		{			
			int nCode = WaitForSingleObject(CUsbVolume::m_evntDeviceChangeMonitorLock, 0);
			if (nCode != WAIT_OBJECT_0)
			{
				DWORD dwTickStart = 0;
				if (bIsSaveTimeLogEnable)
					dwTickStart = ::timeGetTime();

				DWORD dwCurrentCheckSumm = pUsbVolume->CalculateVolumePathCheckSumm(true);
				if (bIsSaveTimeLogEnable)
				{
					DWORD dwTickStop = ::timeGetTime();
					DWORD dwTimeCount = (DWORD)(dwTickStop - dwTickStart);
					g_log.SaveLogFile(LOG_MODES::DEBUGING, "WORK-THREAD: Time of execute 'CalculateVolumePathCheckSumm' = %u ms (CS = %u)",
						dwTimeCount, dwCurrentCheckSumm);
				}
				if ((pUsbVolume->m_dwOldVolumePathCheckSumm > 0) && (dwCurrentCheckSumm > 0) &&
					(pUsbVolume->m_dwOldVolumePathCheckSumm != dwCurrentCheckSumm))
				{
					HWND hWnd = g_spUSBEnumEventFilter->GetMainHWnd();
					::SendMessageA(hWnd, MYWN_PATHCHANGE, (WPARAM)1L, (LPARAM)dwCurrentCheckSumm);
				}

				pUsbVolume->m_dwOldVolumePathCheckSumm = dwCurrentCheckSumm;
			}
			else // If CUsbVolume::m_evntDeviceChangeMonitorLock is setted:
			{				
				do
				{					
					nCode = WaitForSingleObject(CUsbVolume::m_evntDeviceChangeMonitorLock, 10);
				} while (nCode == WAIT_OBJECT_0); 
			}
			// Using 'nPoolingWaitTimeMs' waiting - to exclude "CPU overload":
			iCode = WaitForSingleObject(CUsbVolume::m_evntStopMonitoringThread, nPoolingWaitTimeMs);
		} while (iCode != WAIT_OBJECT_0);
		// Now reset event for stop this thread: 
		::ResetEvent(CUsbVolume::m_evntStopMonitoringThread);
		return 0;
	} 
	catch (...)
	{
		g_log.SaveLogFile(LOG_MODES::ERRORLOG, "CUsbVolume::UsbVolumeThreadProc - error occur!");
		return 1;
	}
}

// It is same as the 'CUsbVolume::UsbVolumeThreadProc' in the TimerMode:
void CUsbVolume::TimerMonitorProc()
{
	try
	{
		int nCode = WaitForSingleObject(CUsbVolume::m_evntDeviceChangeMonitorLock, 0);
		if (nCode == WAIT_OBJECT_0)
		{			
			return;
		}

		DWORD dwCurrentCheckSumm = CalculateVolumePathCheckSumm(true); // Same as in the WorkThread

		if ((m_dwOldVolumePathCheckSumm > 0) && (dwCurrentCheckSumm > 0) &&
			(m_dwOldVolumePathCheckSumm != dwCurrentCheckSumm))
		{
			HWND hWnd = g_spUSBEnumEventFilter->GetMainHWnd();
			::SendMessageA(hWnd, MYWN_PATHCHANGE, (WPARAM)1L, (LPARAM)dwCurrentCheckSumm);
		}

		m_dwOldVolumePathCheckSumm = dwCurrentCheckSumm;
		return;
	}
	catch (...)
	{
		g_log.SaveLogFile(LOG_MODES::ERRORLOG, "CUsbVolume::TimerMonitorProc - error occur!");
		return;
	}
}

void CUsbVolume::OnDeviceChangeMonitorLock()
{
	::SetEvent(CUsbVolume::m_evntDeviceChangeMonitorLock);
}

void CUsbVolume::OnRefreshComplete()
{
	::ResetEvent(CUsbVolume::m_evntDeviceChangeMonitorLock);
}

bool CUsbVolume::StartThread()
{
	// see:
	// https://msdn.microsoft.com/en-us/library/kdzttdcb.aspx
	// http://www.codeproject.com/Articles/14746/Multithreading-Tutorial 

	HANDLE hthr = (HANDLE)_beginthreadex(NULL, // security
		0,             // stack size
		&CUsbVolume::UsbVolumeThreadProc, // entry-point-function
		this,           // arg list holding the "this" pointer
		0, // NORMAL_PRIORITY_CLASS, // CREATE_SUSPENDED,
		&CUsbVolume::m_nThreadID);

	if (!hthr || !CUsbVolume::m_nThreadID)
	{
		g_log.SaveLogFile(LOG_MODES::ERRORLOG, "CUsbVolume::StartThread - error occur!");
		return false;
	}
	g_log.SaveLogFile(LOG_MODES::DEBUGING, "CUsbVolume::StartThread - m_nThreadID = %u (USBEnum Monitoring-thread)",
		CUsbVolume::m_nThreadID);
	return true;
}

void CUsbVolume::StopMonitoringThread()
{
	if (CUsbVolume::m_nThreadID > 0)
	{
		::ResetEvent(CUsbVolume::m_evntDeviceChangeMonitorLock); 
		::ResetEvent(CUsbVolume::m_evntMainThreadLock);
		::SetEvent(CUsbVolume::m_evntStopMonitoringThread);
		UINT nStopWaitTimeMs = GetStopWaitTimeMs();
		int iCode = -1;
		do
		{
			// Using nStopWaitTimeMs sec waiting - to exclude "CPU overload":
			iCode = WaitForSingleObject(CUsbVolume::m_evntStopMonitoringThread, nStopWaitTimeMs); // For example: 500 == 0.5 sec
		} while (iCode == WAIT_OBJECT_0);
		g_log.SaveLogFile(LOG_MODES::DEBUGING, "Application exit.");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CUsbVolume::CUsbVolume(const MONITORING_THREAD_SETTINGS& thrSettings)
	: m_dwOldVolumePathCheckSumm(0), m_thrSettings(thrSettings)
{	
	CUsbVolume::m_nThreadID = 0;
	// Create event (for manual use):
	CUsbVolume::m_evntStopMonitoringThread = ::CreateEventA(NULL, TRUE, FALSE, "StopMonitoringThread");
	CUsbVolume::m_evntDeviceChangeMonitorLock = ::CreateEventA(NULL, TRUE, FALSE, "DeviceChangeMonitorLock"); 
	CUsbVolume::m_evntMainThreadLock = ::CreateEventA(NULL, TRUE, FALSE, "MainThreadLock"); 
}

CUsbVolume::~CUsbVolume()
{
	Clear();
}

void CUsbVolume::Clear()
{
	m_vectDrives.clear();
}

bool CUsbVolume::GetIsMonitoringThreadEnable() const
{
	return std::get<0>(m_thrSettings);
}

UINT CUsbVolume::GetPoolingWaitTimeMs() const
{
	return std::get<1>(m_thrSettings);
}

//UINT CUsbVolume::GetChangeDriveWaitTimeMs() const
//{
//	return std::get<2>(m_thrSettings);
//}

UINT CUsbVolume::GetStopWaitTimeMs() const
{
	return std::get<2>(m_thrSettings); // <3>
}

bool CUsbVolume::GetIsSaveTimeLogEnable() const
{
	return std::get<3>(m_thrSettings); // <4>
}

USB_DISK_VOLUME_DATA CUsbVolume::RetrieveDiskVolumeData(const std::string& sDeviceId)
{
	USB_DISK_VOLUME_DATA pairData;
	std::string sDeviceToFind(sDeviceId);
	std::transform(sDeviceToFind.begin(), sDeviceToFind.end(), sDeviceToFind.begin(), ::tolower);

	find_if(m_vectDrives.begin(), m_vectDrives.end(),
		[=, &pairData](std::shared_ptr<CDriveVolume> spDriverVolume)
	{	
		std::size_t found = spDriverVolume->GetStrDevice().find(sDeviceToFind);
		if (found != std::string::npos)
		{			
			std::string sGuidOfVolume = CUsbVolume::ConvertGuidOfVolume(spDriverVolume->GetStrVolume());
			pairData.first = sGuidOfVolume; 
			pairData.second = spDriverVolume->GetVectorVolumePath();
			return true;
		}
		else
			return false;
	});
	return pairData;
}

std::string CUsbVolume::GetDevicePathByDeviceNumber(long DeviceNumber, UINT DriveType, char* szDosDeviceName)
{
	bool IsFloppy = (strstr(szDosDeviceName, "\\Floppy") != NULL); // is there a better way?

	GUID* guid;

	switch (DriveType) 
	{
	case DRIVE_NO_ROOT_DIR: // Added 04.04.2019
		guid = (GUID*)&GUID_DEVINTERFACE_DISK;
		break;
	case DRIVE_REMOVABLE:
	  if ( IsFloppy ) 
	  {
		  guid = (GUID*)&GUID_DEVINTERFACE_FLOPPY;
	  } else 
	  {
		  guid = (GUID*)&GUID_DEVINTERFACE_DISK;
	  }
	  break;
	case DRIVE_FIXED:
	  guid = (GUID*)&GUID_DEVINTERFACE_DISK;
	  break;
	case DRIVE_CDROM:
	  guid = (GUID*)&GUID_DEVINTERFACE_CDROM;
	  break;
	default:
	  return ("");
	}

	// Get device interface info set handle
	// for all devices attached to system
	HDEVINFO hDevInfo = SetupDiGetClassDevsA(guid, NULL, NULL, 
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (hDevInfo == INVALID_HANDLE_VALUE) 
	{
		return ("");
	}

	// Retrieve a context structure for a device interface
	// of a device information set.
	DWORD dwIndex = 0;
	BOOL bRet = FALSE;

	BYTE Buf[1024];
	PSP_DEVICE_INTERFACE_DETAIL_DATA_A pspdidd = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)Buf;
	SP_DEVICE_INTERFACE_DATA           spdid;
	SP_DEVINFO_DATA                    spdd;
	DWORD                              dwSize;

	spdid.cbSize = sizeof(spdid);

	while ( true )  {
		bRet = SetupDiEnumDeviceInterfaces(hDevInfo, NULL,
			guid, dwIndex, &spdid);
		if (!bRet) {
			break;
		}

		dwSize = 0;
		SetupDiGetDeviceInterfaceDetailA(hDevInfo,
			&spdid, NULL, 0, &dwSize, NULL);

		if ( dwSize!=0 && dwSize<=sizeof(Buf) ) {
			pspdidd->cbSize = sizeof(*pspdidd); // 5 Bytes!

			ZeroMemory((PVOID)&spdd, sizeof(spdd));
			spdd.cbSize = sizeof(spdd);

			long res =
				SetupDiGetDeviceInterfaceDetailA(hDevInfo, &

				spdid, pspdidd,
				dwSize, &dwSize,
				&spdd);
			if ( res ) {
				HANDLE hDrive = CreateFileA(pspdidd->DevicePath,0,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, NULL, NULL);
				if ( hDrive != INVALID_HANDLE_VALUE ) {
					STORAGE_DEVICE_NUMBER sdn;
					DWORD dwBytesReturned = 0;
					res = DeviceIoControl(hDrive,
						IOCTL_STORAGE_GET_DEVICE_NUMBER,
						NULL, 0, &sdn, sizeof(sdn),
						&dwBytesReturned, NULL);
					if ( res ) {
						if ( DeviceNumber == (long)sdn.DeviceNumber ) {
							CloseHandle(hDrive);
							SetupDiDestroyDeviceInfoList(hDevInfo);
							return std::string(pspdidd->DevicePath);
						}
					}
					CloseHandle(hDrive);
				}
			}
		}
		dwIndex++;
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	return ("");
}

int CUsbVolume::GenerateDeviceNumber(const std::string& sVolumeGuid)
{
	HANDLE hDevice = CreateFileA(sVolumeGuid.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, NULL, NULL);

	if (hDevice == NULL || hDevice == INVALID_HANDLE_VALUE)
		return -1; // Error occur

	STORAGE_DEVICE_NUMBER deviceNumber;
	DWORD dwOutBytes = 0;
	BOOL bRes = DeviceIoControl(hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER,
		NULL, 0, &deviceNumber, sizeof(deviceNumber), &dwOutBytes, (LPOVERLAPPED)NULL);

	CloseHandle(hDevice);

	if (!bRes)
		return -1; // Error occur

	int iDeviceNumber = deviceNumber.DeviceNumber;
	return iDeviceNumber;
}

bool CUsbVolume::PrepareDeviceStringData(CDriveVolume &volDrive)
{
	int nSize = volDrive.GetVectorVolumePath().size(); 
	if (!nSize)
		return false;

	int iLen = volDrive.GetStrVolumePath().length();
	// if (!iLen)
	//	return false;
			
	char* szVolumePath = new char[MAX_PATH];
	::ZeroMemory(szVolumePath, MAX_PATH);
	strcpy(szVolumePath, volDrive.GetStrVolumePath().c_str());
	int nIndex = iLen - 1;
	szVolumePath[nIndex] = '\0';
		
	std::string sVolume = volDrive.GetStrVolume();

	//int nDevNumber = GenerateDeviceNumber(sVolume);
	//if (nDevNumber < 0)
	//{
	//	if (szVolumePath != nullptr)
	//		delete[] szVolumePath;
	//
	//	return false;
	//}
	// volDrive.SetDeviceNumber(nDevNumber);

	UINT nDriveType = GetDriveTypeA(szVolumePath); 
	std::string sDosDeviceName = volDrive.GetStrDosDeviceName();	
	volDrive.m_strDevice = GetDevicePathByDeviceNumber(volDrive.GetDeviceNumber()/*nDevNumber*/, nDriveType, (char*)sDosDeviceName.c_str());
	// g_log.SaveLogFile(LOG_MODES::DEBUGING, "VOLUME_PATH: %s", szVolumePath);
	// g_log.SaveLogFile(LOG_MODES::DEBUGING, "DOS_DEVICE_NAME: %s", sDosDeviceName.c_str()); 
	// g_log.SaveLogFile(LOG_MODES::DEBUGING, "DEVICE: %s", volDrive.m_strDevice.c_str()); 

	if (szVolumePath != nullptr)
		delete[] szVolumePath;
		
	return true;
}

// see:
// https://docs.microsoft.com/en-us/windows/desktop/fileio/displaying-volume-paths
bool CUsbVolume::PrepareVolumePathVector(CDriveVolume &volDrive)
{
	std::string sVolumeNameGuid = volDrive.GetStrVolume();
	DWORD  dwCharCount = MAX_PATH + 1;
	char*  szNames = NULL;
	char*  szNameIdx = NULL;
	BOOL   bSuccess = FALSE;

	std::string sVolGuidCorrect = sVolumeNameGuid;
	sVolGuidCorrect += "\\"; 
	
	std::string sOutput;
	for (;;)
	{		
		//  Allocate a buffer to hold the paths.
		szNames = (char*)new BYTE[dwCharCount * sizeof(char)];

		if (!szNames)
		{			
			//  If memory can't be allocated, return.
			return "";
		}
				
		//  Obtain all of the paths
		//  for this volume.
		bSuccess = GetVolumePathNamesForVolumeNameA(sVolGuidCorrect.c_str(), szNames, dwCharCount, &dwCharCount);
		DWORD dwError = GetLastError();
		if (bSuccess)
		{
			break;
		}

		if (GetLastError() != ERROR_MORE_DATA)
		{
			break;
		}
				
		//  Try again with the
		//  new suggested size.
		delete[] szNames;
		szNames = NULL;
	}

	if (bSuccess)
	{
		std::vector<std::string> vect;
		//  Processing the various paths:
		for (szNameIdx = szNames;
			szNameIdx[0] != '\0';
			szNameIdx += strlen(szNameIdx) + 1)
		{
			std::string s1(szNameIdx); 
			vect.push_back(s1);
		}
		volDrive.SetVectorVolumePath(std::move(vect));		
	}

	if (szNames != NULL)
	{
		delete[] szNames;
		szNames = NULL;
	}

	return true;
}

// see:
// https://docs.microsoft.com/en-us/windows/desktop/fileio/displaying-volume-paths
bool CUsbVolume::PrepareDrivesAndVolumeNames(VECT_DRIVES_VOLUME &vectDrives)
{	
	char* szVolumeName = new char[MAX_PATH];
	ZeroMemory(szVolumeName, MAX_PATH);

	HANDLE hFindHandle = INVALID_HANDLE_VALUE;

	//  Enumerate all volumes in the system.
	hFindHandle = FindFirstVolumeA(szVolumeName, MAX_PATH);

	if (hFindHandle == INVALID_HANDLE_VALUE)
	{
		DWORD dwError = GetLastError();
		g_log.SaveLogFile(LOG_MODES::ERRORLOG, "CUsbVolume::PrepareVolumeNames: 'FindFirstVolumeA' failed with error code %u",
			dwError); 		

		if (szVolumeName)
			delete[] szVolumeName;

		return false; 
	}
		
	DWORD  dwCharCount = 0; 
	char*  szDeviceName = new char[MAX_PATH]; 
	ZeroMemory(szDeviceName, MAX_PATH);
		
	BOOL bSuccess = false;
	size_t nIndex = 0;
	for (;;)
	{
		//  Skip the \\?\ prefix and remove the trailing backslash.
		nIndex = strlen(szVolumeName) - 1;

		DWORD dwError = 0;
		if (szVolumeName[0] != '\\' ||
			szVolumeName[1] != '\\' ||
			szVolumeName[2] != '?' ||
			szVolumeName[3] != '\\' ||
			szVolumeName[nIndex] != '\\')
		{
			dwError = ERROR_BAD_PATHNAME;
			g_log.SaveLogFile(LOG_MODES::ERRORLOG, "CUsbVolume::PrepareVolumeNames: FindFirstVolume/FindNextVolume returned a bad path: %s", 
				szVolumeName); 
			
			break;
		}

		//  QueryDosDeviceA does not allow a trailing backslash,
		//  so temporarily remove it.
		
		szVolumeName[nIndex] = '\0';
		dwCharCount = QueryDosDeviceA(&szVolumeName[4], szDeviceName, MAX_PATH);

		std::string sVolumeName(szVolumeName); // For example: "\\?\Volume{ee0ff618-0000-0000-0000-100000000000}"
		std::string sDeviceName(szDeviceName); // For example: "\Device\HarddiskVolume1"
		SP_DRIVE_VOLUME spDriveVolume(new CDriveVolume(sDeviceName, sVolumeName)); 
		// Device-number generating (from the volumr-GUID):		
		int nDeviceNumber = GenerateDeviceNumber(sVolumeName);
		spDriveVolume->SetDeviceNumber(nDeviceNumber);
				 
		// PrepareVolumePathVector(*spDriveVolume.get()); // DEBUG !!!
		// PrepareDeviceStringData(*spDriveVolume.get()); // DEBUG !!!

		vectDrives.push_back(spDriveVolume);

		bSuccess = FindNextVolumeA(hFindHandle, szVolumeName, MAX_PATH);
		if (!bSuccess)
		{
			dwError = GetLastError();

			if (dwError != ERROR_NO_MORE_FILES)
			{
				g_log.SaveLogFile(LOG_MODES::ERRORLOG, "FindNextVolume failed with error code %d\n", dwError);

				if (szVolumeName)
					delete[] szVolumeName;

				if (szDeviceName)
					delete[] szDeviceName; 

				return false;
			}

			//  Finished iterating
			//  through all the volumes.
			dwError = ERROR_SUCCESS; 
			break;
		}		 		
	}
	if (szVolumeName)
		delete[] szVolumeName;

	if (szDeviceName)
		delete[] szDeviceName;

	return true; // Return list with Volume's names (GUIDs of Volumes)
}

DWORD CUsbVolume::CalculateVolumePathCheckSumm(bool bFromWorkThread)
{	
	if (bFromWorkThread)
		::SetEvent(CUsbVolume::m_evntMainThreadLock);
	
	bool bResult = PrepareStringDrivesData(m_vectDrives);

	if (bFromWorkThread)
		::ResetEvent(CUsbVolume::m_evntMainThreadLock);
	
	DWORD dwResult = 0;
	if (bResult)
	{		
		for (auto spDriveVolume : m_vectDrives)
		{
			if (spDriveVolume != nullptr)
			{
				PATH_VECTOR vect = spDriveVolume->GetVectorVolumePath();
				for (std::string sPath : vect)
				{
					short nCheckSumm = CDevNode::CalculateCheckSumm(sPath); 
					dwResult += nCheckSumm;
				}
			}
		}
	}
	return dwResult;
}

bool CUsbVolume::PrepareStringDrivesData(VECT_DRIVES_VOLUME &vectDrives)
{
	try
	{
		bool bResult = false;
		for (auto it = vectDrives.begin(); it != vectDrives.end(); it++)
		{
			SP_DRIVE_VOLUME spDriveVolume = *it;
			if (spDriveVolume)
			{
				PrepareVolumePathVector(*spDriveVolume.get());
				PrepareDeviceStringData(*spDriveVolume.get());
				bResult = true;
			}
		}
		return bResult;
	}
	catch (...)
	{
		g_log.SaveLogFile(LOG_MODES::ERRORLOG, "CUsbVolume::PrepareStringDrivesData: Unlnown error occur!");
		return false;
	}
}

bool CUsbVolume::RemovableEnumerate(DWORD* pCoolCheckSummValue)
{
	// bool bRes2 = false;
	bool bRes1 = PrepareDrivesAndVolumeNames(m_vectDrives);
	if (bRes1)
	{
		int nCode = -1;
		do
		{
			nCode = WaitForSingleObject(CUsbVolume::m_evntMainThreadLock, 0); // 10);
		} while (nCode == WAIT_OBJECT_0);
		DWORD dwVolumePathCheckSummTemp = CalculateVolumePathCheckSumm(false);
		if (m_dwOldVolumePathCheckSumm != dwVolumePathCheckSummTemp)
		{
			if ((m_dwOldVolumePathCheckSumm > 0) && (dwVolumePathCheckSummTemp > 0))
			{
				if (pCoolCheckSummValue)
					*pCoolCheckSummValue = dwVolumePathCheckSummTemp;
			}
			m_dwOldVolumePathCheckSumm = dwVolumePathCheckSummTemp;
		}
		/* bRes2 = PrepareStringDrivesData(m_vectDrives); */ 
	}
	bool bVolumeVectReady = (bool)(bRes1); // && bRes2);
	bool bIsMonitoringThreadEnable = GetIsMonitoringThreadEnable();
	if (bVolumeVectReady && bIsMonitoringThreadEnable && (CUsbVolume::m_nThreadID == 0))
	{
		StartThread();
	}
	return bVolumeVectReady;
}

std::string CUsbVolume::ConvertGuidOfVolume(const std::string& sRawGuidOfVolume)
{
	int nLength = sRawGuidOfVolume.length();

	char charForSearch1 = 0x7B; // Symbol: "{"
	char charForSearch2 = 0x7D; // Symbol: "}"
	int iFirstIndex = sRawGuidOfVolume.find(&charForSearch1, 0, 1);
	int iLastIndex = sRawGuidOfVolume.rfind(&charForSearch2, (nLength - 1), 1); 

	std::string sResult;
	for (int iIndex = iFirstIndex; iIndex <= iLastIndex; iIndex++)
	{
		char chOut = sRawGuidOfVolume[iIndex]; 
		sResult.append(&chOut, 1);
	}

	return sResult;
}