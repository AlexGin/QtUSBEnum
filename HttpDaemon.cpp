#include "HttpDaemon.h"
#include <QHostInfo>
#include <Windows.h>

#include "USBEnumJson.h"
#include "LogFile.h"

extern CLogFile g_log;

// see:
// https://github.com/qtproject/qt-solutions/blob/master/qtservice/examples/server/main.cpp

HttpDaemon::HttpDaemon(quint16 port, QObject* pParent)
	    : QTcpServer(pParent), m_bDisabled(false), m_pMain(pParent)
{
        listen(QHostAddress::Any, port);
}

void HttpDaemon::incomingConnection(qintptr nSocket)
{
	if (m_bDisabled)
		return;

	// When a new client connects, the server constructs a QTcpSocket and all
	// communication with the client is done over this QTcpSocket. QTcpSocket
	// works asynchronously, this means that all the communication is done
	// in the two slots readClient() and discardClient().
/*	SP_USB_ENUM_THREAD spUSBEnumThread(new USBEnumThread(nSocket, m_pMain));
	spUSBEnumThread->start();
	m_mapThreadsOfClient[nSocket] = spUSBEnumThread; */

	QTcpSocket* s = new QTcpSocket(this);
	connect(s, SIGNAL(readyRead()), this, SLOT(OnReadClient()));
	connect(s, SIGNAL(disconnected()), this, SLOT(OnDiscardClient()));
	s->setSocketDescriptor(nSocket);  

	g_log.SaveLogFile(LOG_MODES::DEBUGING, "HttpDaemon::incomingConnection: New Connection");
}

void HttpDaemon::OnReadClient()
{
	if (m_bDisabled)
		return;
 
	// This slot is called when the client sent data to the server. The
	// server looks if it was a get request and sends a very simple HTML
	// document back.
	QTcpSocket* socket = (QTcpSocket*)sender();
	if (socket->canReadLine()) 
	{
		QStringList tokens = QString(socket->readLine()).split(QRegExp("[ \r\n][ \r\n]*"));
		if (tokens[0] == "GET") 
		{
			DWORD nThreadID = ::GetCurrentThreadId();
			g_log.SaveLogFile(LOG_MODES::DEBUGING, "HttpDaemon::OnReadClient: (Thread ID = %u)!", nThreadID);

			emit GETRequestRecived();

			QByteArray ba;
			QString strRequest = tokens[1];
			if (strRequest.length() > 1) // Exclude '/' symbol
			{				
				std::string sRequest = strRequest.toStdString();
				g_log.SaveLogFile(LOG_MODES::DEBUGING, "HttpDaemon::OnReadClient_1: request: '%s'", sRequest.c_str()); 
				emit PerformHttpRequest(ba, strRequest);
			}
			else // If strRequest is empty, show Product's Information:
			{
				ba = USBEnumJson::GetProductInformation();
			}
			QTextStream os(socket);
			os.setAutoDetectUnicode(true);
			os << "HTTP/1.0 200 Ok\r\n"
				"Content-Type: application/json; charset=\"utf-8\"\r\n"
				"\r\n";
			os << ba.toStdString().c_str() << "\r\n";
				
			socket->close();

			g_log.SaveLogFile(LOG_MODES::DEBUGING, "HttpDaemon::OnReadClient_2: Wrote to client");

			if (socket->state() == QTcpSocket::UnconnectedState) 
			{
				delete socket;
				g_log.SaveLogFile(LOG_MODES::DEBUGING, "HttpDaemon::OnReadClient_3: Connection closed, socked deleted");
			}
		}
	}
}

void HttpDaemon::OnDiscardClient()
{
	QTcpSocket* socket = (QTcpSocket*)sender();
	socket->deleteLater();

	g_log.SaveLogFile(LOG_MODES::DEBUGING, "HttpDaemon::OnDiscardClient: Connection closed (delete later)");
}

void HttpDaemon::pause()
{
	m_bDisabled = true;
}

void HttpDaemon::resume()
{
	m_bDisabled = false;
}

QString HttpDaemon::PrepareHostInfo(QString& strHostName)
{
	QString strLocalhostname = QHostInfo::localHostName();
	QString strLocalhostIP;
	QList<QHostAddress> hostList = QHostInfo::fromName(strLocalhostname).addresses();
	foreach(const QHostAddress& address, hostList)
	{
		if (address.protocol() == QAbstractSocket::IPv4Protocol && address.isLoopback() == false) 
		{
			strLocalhostIP = address.toString();
		}
	}
	strHostName = strLocalhostname;
	return strLocalhostIP;
}