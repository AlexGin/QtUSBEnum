#include <process.h>	// _beginthreadex

#include "UdpNotifyServer.h"
#include "USBEnumEventFilter.h"
#include "LogFile.h"

extern CLogFile g_log;
extern std::shared_ptr<USBEnumEventFilter> g_spUSBEnumEventFilter;

HANDLE UdpNotifyServer::m_evntStopUdpNotifyThread;
std::vector<HANDLE> UdpNotifyServer::m_vectStopItems;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT __stdcall UdpNotifyServer::NotifyThreadProc(LPVOID pParam)
{
	try
	{
		UdpNotifyItem* pNotifyItem = reinterpret_cast<UdpNotifyItem*>(pParam);
		if (!pNotifyItem)
		{
			g_log.SaveLogFile(LOG_MODES::ERRORLOG, "UdpNotifyServer::NotifyThreadProc - Invalid pParam");
			return 1;
		}

		pNotifyItem->m_pHostAddr = new QHostAddress(pNotifyItem->m_strClientIpAddress);
		pNotifyItem->m_pUdp = new QUdpSocket();

		do
		{			
			pNotifyItem->SendDatagram();
		
			UINT nResult = pNotifyItem->CountDecrement();
			if (1 == nResult)
			{
				HWND hWnd = g_spUSBEnumEventFilter->GetMainHWnd(); 
				int nStopIndex = pNotifyItem->GetStopVectIndex() + 1000;
				::PostMessageA(hWnd, MYWN_SEND_DG_TERMINATE, (WPARAM)nStopIndex, (LPARAM)pNotifyItem->m_nThreadID);
				return 0;
			}
						 
			int nCode = WaitForSingleObject(UdpNotifyServer::m_evntStopUdpNotifyThread, pNotifyItem->m_nTimePeriodMs);
			if (nCode == WAIT_OBJECT_0)
			{
				int nIndex = pNotifyItem->GetStopVectIndex();
				::SetEvent(UdpNotifyServer::m_vectStopItems[nIndex]);
				return 0;
			}			
		} while (1);
		return 0;
	}
	catch (...)
	{
		g_log.SaveLogFile(LOG_MODES::ERRORLOG, "UdpNotifyServer::NotifyThreadProc - error occur!");
		return 1;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
UdpNotifyServer::UdpNotifyServer(const QString& strClientIpAddress, quint16 nPort, 
	int nRepeatCounter, int nTimePeriodMs, QObject *pParent)
	: QObject(pParent),
	m_strClientIpAddress(strClientIpAddress), m_nPort(nPort), 
	m_nCurrentCount(0), m_nRepeatCounter(nRepeatCounter), m_nTimePeriodMs(nTimePeriodMs)
{
	// Create event (for manual use):
	UdpNotifyServer::m_evntStopUdpNotifyThread = ::CreateEventA(NULL, TRUE, FALSE, "StopUdpNotifyThread");
}

UdpNotifyServer::~UdpNotifyServer()
{
	m_mapNotify.clear();
}

void UdpNotifyServer::ClearNotifyItem(UINT nThreadID, int iIndex)
{
	int iTotalThreads = (int)UdpNotifyServer::m_vectStopItems.size();
	if (iTotalThreads > iIndex)
	{
		HANDLE h = UdpNotifyServer::m_vectStopItems[iIndex];
		if (h)
		{
			CloseHandle(h); 
			UdpNotifyServer::m_vectStopItems[iIndex] = 0;
		}
	}
	ClearNotifyItemByThreadID(nThreadID);
}

void UdpNotifyServer::ClearNotifyItemByThreadID(UINT nThreadID)
{
	DWORD dwCurrentKey = 0; // The current value
	DWORD dwToEraseKey = 0; // For remove from map (m_mapNotify)
	for (auto it = m_mapNotify.begin(); it != m_mapNotify.end(); it++)
	{
		auto pair = *it;
		dwCurrentKey = pair.first;
		if (dwCurrentKey > 0)
		{
			UINT nCurrentThreadID = pair.second->GetThreadID();
			if (nThreadID == nCurrentThreadID)
			{
				dwToEraseKey = dwCurrentKey;
				pair.second.reset(); 
			}
		}
	}
	if (dwToEraseKey > 0)
	{
		m_mapNotify.erase(dwToEraseKey);
	}
}

void UdpNotifyServer::Notify(DWORD nUdpMessageID, const QString& strTextToClient)
{
	auto it = m_mapNotify.find(nUdpMessageID);
	if (it == m_mapNotify.end()) // The value of nUdpMessageID is NOT present
	{
		StartNotifyThread(nUdpMessageID, strTextToClient);
	}
	else // The value of nUdpMessageID already exist in map
	{
		g_log.SaveLogFile(LOG_MODES::ERRORLOG, "UdpNotifyServer::Notify - error: try to send extra Notification Datagram!");
	}
}

bool UdpNotifyServer::StartNotifyThread(DWORD nUdpMessageID, const QString& strTextToClient)
{
	// see:
	// https://msdn.microsoft.com/en-us/library/kdzttdcb.aspx
	// http://www.codeproject.com/Articles/14746/Multithreading-Tutorial 

	NOTIFY_PTR spNotify(new UdpNotifyItem(m_strClientIpAddress, m_nPort, m_nRepeatCounter, m_nTimePeriodMs));
	UINT nThreadID = 0;
	HANDLE hthr = (HANDLE)_beginthreadex(NULL, // security
		0,             // stack size
		&UdpNotifyServer::NotifyThreadProc, // entry-point-function
		spNotify.get(),           // arg list holding the "UdpNotifyItem" pointer
		0, // NORMAL_PRIORITY_CLASS, // CREATE_SUSPENDED,
		&nThreadID);

	if (!hthr || !nThreadID)
	{
		g_log.SaveLogFile(LOG_MODES::ERRORLOG, "UdpNotifyServer::StartNotifyThread - error occur!");
		return false;
	}
	spNotify->SetThreadID(nThreadID); 
	spNotify->SetTextToClient(strTextToClient);
	m_mapNotify[nUdpMessageID] = spNotify;

	g_log.SaveLogFile(LOG_MODES::DEBUGING, "UdpNotifyServer::StartNotifyThread - nThreadID = %u (UDPNotify-thread)", nThreadID);
	return true;
}

DWORD UdpNotifyServer::RetrieveTotalActiveThreads()
{
	DWORD dwResult = 0;
	for (auto it = UdpNotifyServer::m_vectStopItems.begin(); it != UdpNotifyServer::m_vectStopItems.end(); it++)
	{
		HANDLE h = *it;
		if (h)
		{
			dwResult++;
		}
	}
	return dwResult;
}

void UdpNotifyServer::OnReceiveFeedback()
{	
	::SetEvent(UdpNotifyServer::m_evntStopUdpNotifyThread);

	DWORD dwTotalThreads = RetrieveTotalActiveThreads();
	if (dwTotalThreads > 0)
	{
		int nCode = -1;
		do
		{
			nCode = WaitForMultipleObjects(dwTotalThreads,
				UdpNotifyServer::m_vectStopItems.data(),
				TRUE,
				100);  // Using 0.1 sec waiting - to exclude "CPU overload"  
		} while (nCode != WAIT_OBJECT_0);
	}

	ClearMapNotify();
	ClearVectorNotify();
	 
	::ResetEvent(UdpNotifyServer::m_evntStopUdpNotifyThread);
}

void UdpNotifyServer::ClearVectorNotify()
{
	for (auto it = UdpNotifyServer::m_vectStopItems.begin(); it != UdpNotifyServer::m_vectStopItems.end(); it++)
	{
		HANDLE h = *it;
		if (h)
		{
			CloseHandle(h);
		}
	}
	UdpNotifyServer::m_vectStopItems.clear();
}

void UdpNotifyServer::ClearMapNotify()
{
	for (auto it = m_mapNotify.begin(); it != m_mapNotify.end(); it++)
	{
		auto pair = *it;
		if (pair.first > 0)
		{
			pair.second.reset();
		}
	}
	m_mapNotify.clear(); 
}
