#include <qdatetime.h>

#include "UdpNotifyItem.h" 
#include "UdpNotifyServer.h"
#include "LogFile.h"

extern CLogFile g_log;

UdpNotifyItem::UdpNotifyItem(const QString& strClientIpAddress, quint16 nPort, int nRepeatCounter, int nTimePeriodMs)
	: m_strClientIpAddress(strClientIpAddress), m_nPort(nPort), 
	m_nRepeatCounter(nRepeatCounter), m_nTimePeriodMs(nTimePeriodMs)
{
	m_nCurrentCount = m_nRepeatCounter;

	m_nStopVectIndex = (int)UdpNotifyServer::m_vectStopItems.size(); 

	// Create event (for manual use):
	HANDLE hHandle = ::CreateEventA(NULL, TRUE, FALSE, NULL); 
	UdpNotifyServer::m_vectStopItems.push_back(hHandle);
}

UdpNotifyItem::~UdpNotifyItem()
{
}

void UdpNotifyItem::SetTextToClient(const QString& strTextToClient)
{
	m_strTextToClient = strTextToClient;
}

void UdpNotifyItem::SetThreadID(UINT nThreadID)
{
	m_nThreadID = nThreadID;
}

UINT UdpNotifyItem::GetThreadID() const
{
	return m_nThreadID;
}

int UdpNotifyItem::GetStopVectIndex() const
{
	return m_nStopVectIndex;
}

void UdpNotifyItem::SendDatagram()
{
	if (m_nCurrentCount != 0)
	{
		QDateTime dt = QDateTime::currentDateTime();
		QString strThreadID = QString::number(m_nThreadID);
		QString str = m_strTextToClient + "_ThreadID=" + strThreadID + "_Time_Curr=" + dt.toString("HH:mm:ss");
		m_pUdp->writeDatagram(str.toUtf8(), *m_pHostAddr, m_nPort); 		
	}
}

UINT UdpNotifyItem::CountDecrement()
{
	if (m_nCurrentCount > 0)
		m_nCurrentCount--;

	if (!m_nCurrentCount)
	{
		g_log.SaveLogFile(LOG_MODES::ERRORLOG, "UdpNotifyItem::CountDecrement: Client not ready - timeout error");
		return 1;
	}
	return 0;
}