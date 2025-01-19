#pragma once

#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "USBEnum\USBDevice.h"

enum class HTTP_API_REQUEST
{
	UNKNOWN = 0,
	REQ_LAST_UPDATE_DT,		// Last update Date-time
	REQ_TOTAL_NODES,		// Total quantity of nodes (include Hubs and RootHubs)
	REQ_ENTIRE_NODES_TREE,	// Entire Tree of Nodes (show ALL nodes)
	REQ_ENTIRE_ACTIVE_TREE,	// Entire Tree of Nodes (show ONLY active nodes)
	REQ_ENTIRE_HUBS_TREE	// Entire Tree of Hubs (include RootHubs)
};

class USBEnumJson
{
public:
	USBEnumJson();
	~USBEnumJson();
public:
	void SetMainRootDevice(SP_USB_DEV spUSBDev);
	bool ParseRequestString(const QString& strRequest, HTTP_API_REQUEST* pResult); 
	QByteArray GetDateTimeInfo(const QDateTime& dt); 
	QByteArray GetTotalNodesInfo(int nTotalNodes, int nTotalHubs, int nTotalRootHubs); 
	QByteArray GetEntireNodesTree(SP_USB_DEV spUsbDev, bool bShowActive, QFile* ptrOutFile = nullptr);
	QByteArray GetEntireHubsTree(SP_USB_DEV spUsbDev, QFile* ptrOutFile = nullptr);
private:
	SP_USB_DEV m_spUSBDev; // Main Root Device 
	QStringList m_slRequests; // List with strings of the requests
private:
	void InitRequestList(); 
	void AddJsonNodeObject(QJsonObject& infoObj, SP_USB_DEV spUsbDeviceData, bool bShowActive); 
	void AddJsonHubObject(QJsonObject& infoObj, SP_USB_DEV spUsbDeviceData);
public:
	static QByteArray GetProductInformation(); 
	static QString ConvertCategToString(USB_ENGINE_CATEGORY usbCateg); 
	static QString ConvertKindToString(USB_DEVICE_KIND usbDeviceKind);
	static QJsonObject PrepareNodeJsonObject(SP_USB_DEV spUsbDeviceData);  // For Devices tree
	static QJsonObject PrepareNodeJsonObject(const QString& strCateg, const QString& strDeviceId); // For Hub's tree
	static QJsonObject PrepareHubJsonObject(SP_USB_DEV spUsbDeviceData);
	static bool IsHubOrHostPresent(const VECT_SP_USB_DEV& vect);
};

