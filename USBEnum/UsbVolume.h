//#define DISABLE_USB
#pragma once
#include <windows.h>

#include <string>
#include <vector>
#include <memory>
#include <tuple>

#include "USBEnum/DriveVolume.h"

// see:
// https://docs.microsoft.com/en-us/windows/desktop/fileio/displaying-volume-paths
// and:
// https://gist.github.com/gabonator/2499496

using SP_DRIVE_VOLUME = std::shared_ptr<CDriveVolume>;
using VECT_DRIVES_VOLUME = std::vector<std::shared_ptr<CDriveVolume>>;
using MONITORING_THREAD_SETTINGS = std::tuple<bool, UINT, UINT, bool>;
// bool bMonitoringThreadEnable, uint nPoolingWaitTimeMs, uint nStopWaitTimeMs, bool bSaveTimeLogEnable

using USB_DISK_VOLUME_DATA = std::pair<std::string, PATH_VECTOR>; // First-GUID of Volume; Second-Pathes for this volume
class CUsbVolume
{
public:
	CUsbVolume(const MONITORING_THREAD_SETTINGS& thrSettings);
	~CUsbVolume();
public: 
	USB_DISK_VOLUME_DATA RetrieveDiskVolumeData(const std::string& sDeviceId); 	
	bool PrepareDrivesAndVolumeNames(VECT_DRIVES_VOLUME &vectDrives); // Added 04.04.2019
	bool PrepareStringDrivesData(VECT_DRIVES_VOLUME &vectDrives); // Added 05.04.2019
	bool RemovableEnumerate(DWORD* pCoolCheckSummValue);
	void Clear(); 
public:
	void OnDeviceChangeMonitorLock();
	void OnRefreshComplete();
public: // It is for Monitoring-thread support:
	static UINT __stdcall UsbVolumeThreadProc(LPVOID pParam); 
	void TimerMonitorProc();
	void StopMonitoringThread(); 
private:
	bool StartThread(); 
	DWORD CalculateVolumePathCheckSumm(bool bFromWorkThread);
	DWORD m_dwOldVolumePathCheckSumm; 
	int GenerateDeviceNumber(const std::string& sVolumeGuid);
public: // Monitoring-thread support:
	static UINT m_nThreadID;
	static HANDLE m_evntStopMonitoringThread;		// Event - for Stop UsbVolume Monitoring-thread
	static HANDLE m_evntDeviceChangeMonitorLock;	// Event - for disable work thread function during Device Change processing
	static HANDLE m_evntMainThreadLock;				// Event - for disable function the main thread
private:
	MONITORING_THREAD_SETTINGS m_thrSettings;
public:
	bool GetIsMonitoringThreadEnable() const;
	UINT GetPoolingWaitTimeMs() const; 
	// UINT GetChangeDriveWaitTimeMs() const;
	UINT GetStopWaitTimeMs() const;
	bool GetIsSaveTimeLogEnable() const;
private:
	std::string GetDevicePathByDeviceNumber(long DeviceNumber, UINT DriveType, char* szDosDeviceName);
	bool PrepareDeviceStringData(CDriveVolume &volDrive);
	bool PrepareVolumePathVector(CDriveVolume &volDrive);
private:
	VECT_DRIVES_VOLUME m_vectDrives; 
public:
	static std::string ConvertGuidOfVolume(const std::string& sRawGuidOfVolume); 	
};
using SP_USB_VOL = std::shared_ptr<CUsbVolume>;
