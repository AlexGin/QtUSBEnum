#pragma once
#include <memory>

#include <qstring.h>
#include <qudpsocket.h>
#include <qhostaddress.h>

#include <Windows.h>

const int TIME_SPAN_FOR_ITEM = 100;

class UdpNotifyItem
{
	friend class UdpNotifyServer;
public:
	UdpNotifyItem(const QString& strClientIpAddress, quint16 nPort, int nRepeatCounter, int nTimePeriodMs);
	~UdpNotifyItem(); 
	void SetThreadID(UINT nThreadID);
	void SetTextToClient(const QString& strTextToClient);
	int  GetStopVectIndex() const;
	UINT GetThreadID() const;
	void SendDatagram(); 
	UINT CountDecrement();
private: 
	QString m_strClientIpAddress;
	QHostAddress* m_pHostAddr;
	quint16 m_nPort;
private:
	QUdpSocket* m_pUdp;
	QString m_strTextToClient;
private:
	DWORD m_nID;		  // Equal CheckSumm-value
	int m_nRepeatCounter; // Usually three (3) times 
	int m_nTimePeriodMs;  // Usually 1000 ms (1sec)
private: // Inner variables:
	int m_nCurrentCount; 
	int m_nStopVectIndex;
	int m_nCurrentTimeSpan;
private:
	UINT m_nThreadID;
};
using NOTIFY_PTR = std::shared_ptr<UdpNotifyItem>;
using NOTIFY_MAP = std::map<DWORD, NOTIFY_PTR>; // Key DWORD (nUdpMessageID) equal as CheckSumm-value
