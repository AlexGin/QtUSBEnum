#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QStringList>
#include <QDateTime>

// see:
// https://github.com/qtproject/qt-solutions/blob/master/qtservice/examples/server/main.cpp

// HttpDaemon is the the class that implements the simple HTTP server.
class HttpDaemon : public QTcpServer
{
    Q_OBJECT
public:
    HttpDaemon(quint16 port, QObject* pParent = nullptr);
protected:
    void incomingConnection(qintptr nSocket) override;

public:
	void pause();
	void resume();
private:
	bool m_bDisabled;
	QObject* m_pMain;

private slots:
	void OnReadClient();
 	void OnDiscardClient();
public:
	static QString PrepareHostInfo(QString& strHostName); // Return the host's IP-address
signals:
	void GETRequestRecived();
	void PerformHttpRequest(QByteArray& ba, const QString& strRequest);
};