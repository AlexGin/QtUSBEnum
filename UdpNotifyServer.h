#pragma once

#include <map>
#include <vector>

#include <qobject.h>
#include <qstring.h> 

#include "UdpNotifyItem.h"

class UdpNotifyServer : public QObject
{
	Q_OBJECT
public: 
	UdpNotifyServer(const QString& strClientIpAddress, quint16 nPort, 
		int m_nRepeatCounter = 3, int nTimePeriodMs = 1000, QObject *pParent = nullptr);
	~UdpNotifyServer(); 
public:
	void Notify(DWORD nUdpMessageID, const QString& strTextToClient);
	void ClearNotifyItem(UINT nThreadID, int iIndex); 
private:
	void ClearNotifyItemByThreadID(UINT nThreadID);
	DWORD RetrieveTotalActiveThreads();
private: 
	QString m_strClientIpAddress;
	NOTIFY_MAP m_mapNotify;
	int m_nRepeatCounter; // Usually three (3) times 
	int m_nCurrentCount;
	int m_nTimePeriodMs;	// Usually 1000 ms (1sec)
	quint16 m_nPort;
private:
	bool StartNotifyThread(DWORD nUdpMessageID, const QString& strTextToClient);
	void ClearMapNotify();
	void ClearVectorNotify();
public:
	static UINT __stdcall NotifyThreadProc(LPVOID pParam);
	static HANDLE m_evntStopUdpNotifyThread;   // Event - for Stop UdpNotify-thread
	static std::vector<HANDLE> m_vectStopItems;
public slots:
	void OnReceiveFeedback();
};

