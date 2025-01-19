#pragma once

#include "USBEnum/UVCViewer.h"

class CDevNode
{
public:
	static BOOL DriverNameToDeviceInst(
		_In_reads_bytes_(cbDriverName) PCHAR DriverName,
		_In_ size_t cbDriverName,
		_Out_ HDEVINFO *pDevInfo,
		_Out_writes_bytes_(sizeof(SP_DEVINFO_DATA)) PSP_DEVINFO_DATA pDevInfoData
	);
	static PUSB_DEVICE_PNP_STRINGS DriverNameToDeviceProperties(
			_In_reads_bytes_(cbDriverName) PCHAR  DriverName,
			_In_ size_t cbDriverName
		);
	static void FreeDeviceProperties(_In_ PUSB_DEVICE_PNP_STRINGS *ppDevProps);
	static BOOL GetDeviceProperty(
			_In_    HDEVINFO         DeviceInfoSet,
			_In_    PSP_DEVINFO_DATA DeviceInfoData,
			_In_    DWORD            Property,
			_Outptr_  LPTSTR        *ppBuffer
		);
	static int ParseRootHub(char* lpszText);
	static int ParsePortNumber(char* lpszText);
	static short CalculateCheckSumm(const std::string& sInput);
};

