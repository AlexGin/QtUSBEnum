#include "USBEnumJson.h"
#include "QtUSBEnum.h"
#include <QApplication>
#include "HttpDaemon.h"
#include "LogFile.h"

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>    // std::find_if

extern CLogFile g_log;
extern QApplication* g_pApp;

// using namespace QtJson;

USBEnumJson::USBEnumJson()
{
	InitRequestList();
}

USBEnumJson::~USBEnumJson()
{
	m_slRequests.clear();
}

void USBEnumJson::InitRequestList()
{
	m_slRequests.clear();
	m_slRequests.append("last-update-dt"); 
	m_slRequests.append("total-nodes"); 
	m_slRequests.append("entire-nodes-tree"); 
	m_slRequests.append("entire-active-tree");
	m_slRequests.append("entire-hubs-tree"); 
}

bool USBEnumJson::ParseRequestString(const QString& strRequest, HTTP_API_REQUEST* pResult)
{
	bool bFound = false;
	int iResult = (-1);	
	int nCount = m_slRequests.count();
	for (int i = 0; i < nCount; i++)
	{
		QString str = m_slRequests[i];
		int nIndexOf = strRequest.indexOf(str);
		if (nIndexOf != (-1))
		{
			iResult = i + 1; // Base string in the HTTP_API_REQUEST-enum: 1 (one) 
			bFound = true;
			break;
		}
	}

	if (bFound && (pResult != nullptr))
	{
		*pResult = (HTTP_API_REQUEST)iResult;
	}

	return bFound;
}

void USBEnumJson::SetMainRootDevice(SP_USB_DEV spUSBDev)
{
	m_spUSBDev = spUSBDev;
}

QByteArray USBEnumJson::GetProductInformation()
{
	std::stringstream ss;
	ss << "Creation: Date " << __DATE__ << "; Time " << __TIME__;
	std::string sDateTime = ss.str();

	std::string sTypeOfBuild = "Release; x86";
#ifdef _DEBUG
	sTypeOfBuild = "Debug; x86";
#endif
	QString strLocalhostname;
	QString strHostIPAddr = HttpDaemon::PrepareHostInfo(strLocalhostname);
	std::string sLocalhostname = strLocalhostname.toStdString(); 
	std::string sHostIPAddr = strHostIPAddr.toStdString();

	QJsonObject infoObject;   
	infoObject.insert("appfile", QJsonValue::fromVariant("QtUSBEnum.exe"));
	infoObject.insert("appname", QJsonValue::fromVariant("Qt USB Enumerator"));	 
	infoObject.insert("appversion", QJsonValue::fromVariant(g_pApp->applicationVersion())); 
	infoObject.insert("datetime", QJsonValue::fromVariant(sDateTime.c_str()));
	infoObject.insert("hostname", QJsonValue::fromVariant(sLocalhostname.c_str())); 
	infoObject.insert("hostipaddr", QJsonValue::fromVariant(sHostIPAddr.c_str()));	 	
	infoObject.insert("winsdkver", QJsonValue::fromVariant("10.0.22621.0")); 
	infoObject.insert("qtver", QJsonValue::fromVariant("Qt 5.13.1"));
	infoObject.insert("toolset", QJsonValue::fromVariant("Visual Studio 2017 (v141)"));
	infoObject.insert("typeofbuild", QJsonValue::fromVariant(sTypeOfBuild.c_str()));
		
	QByteArray baOut = QJsonDocument(infoObject).toJson(QJsonDocument::Indented);
	return baOut; 
}

QByteArray USBEnumJson::GetDateTimeInfo(const QDateTime& dt)
{
	QDate date = dt.date();
	QTime time = dt.time();

	QString strDate = date.toString("yyyy.MM.dd");
	QString strTime = time.toString("HH:mm:ss");

	QJsonObject infoObject;
	infoObject.insert("lastupdetedate", QJsonValue::fromVariant(strDate));
	infoObject.insert("lastupdetetime", QJsonValue::fromVariant(strTime)); 

	QByteArray baOut = QJsonDocument(infoObject).toJson(QJsonDocument::Indented);
	return baOut;
}

QByteArray USBEnumJson::GetTotalNodesInfo(int nTotalNodes, int nTotalHubs, int nTotalRootHubs)
{
	QJsonObject infoObject;
	infoObject.insert("totalnodes", QJsonValue::fromVariant(nTotalNodes));
	infoObject.insert("totalhubs", QJsonValue::fromVariant(nTotalHubs)); 
	infoObject.insert("totalroothubs", QJsonValue::fromVariant(nTotalRootHubs));

	QByteArray baOut = QJsonDocument(infoObject).toJson(QJsonDocument::Indented);
	return baOut;
}

QJsonObject USBEnumJson::PrepareHubJsonObject(SP_USB_DEV spUsbDeviceData)
{
	QJsonObject objResult;
	std::string sDeviceId = spUsbDeviceData->GetDeviceId(); 

	std::string sSN = spUsbDeviceData->GetSNOfDevice();
	QString strSerialNumber = (sSN.size() > 0) ? sSN.c_str() : "-";

	std::string sVendor = spUsbDeviceData->GetVendorOfDevice();
	QString strVendor = (sVendor.size() > 0) ? sVendor.c_str() : "-";

	std::string sManuf = spUsbDeviceData->GetManufacturer();
	QString strManuf = (sManuf.size() > 0) ? sManuf.c_str() : "-";

	auto categ = spUsbDeviceData->GetUSBCategory();
	bool bIsHub = (bool)(USB_ENGINE_CATEGORY::USB_HUB == categ);
	QString strIsHub = bIsHub ? "Hub" : "No hub";

	objResult["deviceid"] = spUsbDeviceData->GetDeviceId().c_str();
	objResult["devicevendor"] = strVendor; 
	objResult["itishub"] = strIsHub;
	objResult["locationhub"] = spUsbDeviceData->GetParentHub();
	objResult["locationport"] = spUsbDeviceData->GetPort();
	objResult["manufacturer"] = strManuf;
	objResult["productname"] = spUsbDeviceData->GetProductName().c_str();
	objResult["serialnumber"] = strSerialNumber; 

	return objResult;
}

QJsonObject USBEnumJson::PrepareNodeJsonObject(const QString& strCateg, const QString& strDeviceId) // For Hub's tree:
{
	QJsonObject objResult;
	
	QString strDevID = (!strDeviceId.isEmpty()) ? strDeviceId : "-";
	objResult["deviceid"] = strDevID;
	objResult["deviceusbcateg"] = strCateg;
	objResult["itishub"] = "No hub"; 

	return objResult;
}

QJsonObject USBEnumJson::PrepareNodeJsonObject(SP_USB_DEV spUsbDeviceData) // For Devices tree:
{
	QJsonObject objResult;
	std::string sDeviceId = spUsbDeviceData->GetDeviceId();

	auto categ = spUsbDeviceData->GetUSBCategory();
	QString strCateg = USBEnumJson::ConvertCategToString(categ);

	auto kind = spUsbDeviceData->GetDeviceKind();
	QString strKind = USBEnumJson::ConvertKindToString(kind);

	std::string sVolumeGuid;
	std::vector<std::string> vectPath = spUsbDeviceData->GetVectorVolumePath();
	bool bDriveExist = (bool)(vectPath.size() > 0);
	if (bDriveExist)
	{		
		sVolumeGuid = spUsbDeviceData->GetGuidOfVolume();
	}

	std::string sSN = spUsbDeviceData->GetSNOfDevice();
	QString strSerialNumber = (sSN.size() > 0) ? sSN.c_str() : "-";

	std::string sVendor = spUsbDeviceData->GetVendorOfDevice(); 
	QString strVendor = (sVendor.size() > 0) ? sVendor.c_str() : "-"; 

	std::string sManuf = spUsbDeviceData->GetManufacturer();
	QString strManuf = (sManuf.size() > 0) ? sManuf.c_str() : "-";

	bool bIsHub = (bool)(USB_ENGINE_CATEGORY::USB_HUB == categ);
	QString strIsHub = bIsHub ? "Hub" : "No hub";

	objResult["deviceid"] = spUsbDeviceData->GetDeviceId().c_str();
	objResult["devicekind"] = strKind; 	
	objResult["deviceusbcateg"] = strCateg; 
	objResult["devicevendor"] = strVendor;
	objResult["itishub"] = strIsHub;
	objResult["locationhub"] = spUsbDeviceData->GetParentHub();
	objResult["locationport"] = spUsbDeviceData->GetPort(); 
	objResult["manufacturer"] = strManuf;
	objResult["productname"] = spUsbDeviceData->GetProductName().c_str();
	objResult["serialnumber"] = strSerialNumber;
	
	if (bDriveExist)
	{		
		QJsonArray pathArray;
		for (std::string sPath : vectPath)
		{
			std::string sPathOut = QtUSBEnum::CorrectVolumePathString(sPath);
			QString strPathOut = sPathOut.c_str();
			pathArray.push_back((QJsonValue)strPathOut);
		}
		objResult["volume_path"] = pathArray;
		objResult["volume_guid"] = sVolumeGuid.c_str();
	}
	return objResult;
}

void USBEnumJson::AddJsonHubObject(QJsonObject& infoObj, SP_USB_DEV spUsbDeviceData)
{
	QString strKey; 
	int nHubID = spUsbDeviceData->GetParentHub();
	int nPortID = spUsbDeviceData->GetPort();
	std::string sHubPort = QtUSBEnum::PrepareHubPortString(nHubID, nPortID);
	strKey = sHubPort.c_str();
		
	QJsonObject objFirst;
	auto spHub1 = std::dynamic_pointer_cast<USBEnum::USBHub>(spUsbDeviceData);
	if (spHub1) // It is Hub:
	{
		objFirst = USBEnumJson::PrepareHubJsonObject(spUsbDeviceData);		
	}
	else // It is NO Hub:
	{
		auto categ = spUsbDeviceData->GetUSBCategory(); 
		QString strCateg = USBEnumJson::ConvertCategToString(categ);
		QString strDeviceId = spUsbDeviceData->GetDeviceId().c_str();
		objFirst = USBEnumJson::PrepareNodeJsonObject(strCateg, strDeviceId);
	}
	
	QJsonArray childUsbHubsHostsArray;
	for (auto cit = spUsbDeviceData->GetVectChilds().cbegin(); cit != spUsbDeviceData->GetVectChilds().cend(); cit++)
	{
		SP_USB_DEV spData = *cit;
		if (spData != nullptr)
		{
			auto spHub2 = std::dynamic_pointer_cast<USBEnum::USBHub>(spData);
			if (spHub2)
			{
				auto sDevId = spData->GetDeviceId();
				if (!sDevId.empty())
				{
					QJsonObject objSecond;
					// Recursive call:
					AddJsonHubObject(objSecond, spData);
					childUsbHubsHostsArray.append(objSecond);
				}
			}
			else
			{
				auto categ = spData->GetUSBCategory();
				if (categ == USB_ENGINE_CATEGORY::USB_HOST_DEVICE)
				{
					QJsonObject objSecond;
					// Recursive call:
					AddJsonHubObject(objSecond, spData);
					childUsbHubsHostsArray.append(objSecond);
				}
			}
		}
	}
	
	if (IsHubOrHostPresent(spUsbDeviceData->GetVectChilds()))
		objFirst["usb_hubs_and_hosts"] = childUsbHubsHostsArray;

	infoObj.insert(strKey, QJsonValue::fromVariant(objFirst));
}

bool USBEnumJson::IsHubOrHostPresent(const VECT_SP_USB_DEV& vect) // Finding
{
	if (vect.size() == 0)
		return false; 

	auto it = find_if(vect.begin(), vect.end(),
		[](std::shared_ptr<USBEnum::IUSBDevice> spDev) -> bool
	{
		auto spHub1 = std::dynamic_pointer_cast<USBEnum::USBHub>(spDev);
		if (spHub1) // It is Hub:
		{
			return true;
		}
		else // It is NO Nub:
		{
			auto categ = spDev->GetUSBCategory();
			if ((categ == USB_ENGINE_CATEGORY::GLOBAL) ||
				(categ == USB_ENGINE_CATEGORY::USB_HOST_DEVICE))
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	});

	if (it == vect.end())
		return false;

	return true;
}

void USBEnumJson::AddJsonNodeObject(QJsonObject& infoObj, SP_USB_DEV spUsbDeviceData, bool bShowActive)
{
	int nHubID = spUsbDeviceData->GetParentHub(); 
	int nPortID = spUsbDeviceData->GetPort();
	std::string sHubPort = QtUSBEnum::PrepareHubPortString(nHubID, nPortID);
	QString strKey(sHubPort.c_str());  

	QJsonObject objFirst = USBEnumJson::PrepareNodeJsonObject(spUsbDeviceData);

	QJsonArray childUsbItemsArray;
	for (auto cit = spUsbDeviceData->GetVectChilds().cbegin(); cit != spUsbDeviceData->GetVectChilds().cend(); cit++)
	{
		SP_USB_DEV spData = *cit;
		if (spData != nullptr)
		{			
			auto sDevId = spData->GetDeviceId();
			if (!sDevId.empty() || !bShowActive)
			{
				QJsonObject objSecond;
				// Recursive call:
				AddJsonNodeObject(objSecond, spData, bShowActive);
				childUsbItemsArray.append(objSecond);
			}
		}
		
	}
	size_t nSize = spUsbDeviceData->GetVectChilds().size();
	if (nSize > 0)
		objFirst["usb_items"] = childUsbItemsArray;

	infoObj.insert(strKey, QJsonValue::fromVariant(objFirst));
}

QByteArray USBEnumJson::GetEntireNodesTree(SP_USB_DEV spUsbDev, bool bShowActive, QFile* ptrOutFile)
{
	QJsonObject infoTreeObject; 

	AddJsonNodeObject(infoTreeObject, spUsbDev, bShowActive);

	if (ptrOutFile)
	{
		ptrOutFile->write(QJsonDocument(infoTreeObject).toJson(QJsonDocument::Indented));
	}

	QByteArray baOut = QJsonDocument(infoTreeObject).toJson(QJsonDocument::Indented);
	return baOut;
}

QByteArray USBEnumJson::GetEntireHubsTree(SP_USB_DEV spUsbDev, QFile* ptrOutFile)
{
	QJsonObject infoTreeObject;

	AddJsonHubObject(infoTreeObject, spUsbDev);

	if (ptrOutFile)
	{
		ptrOutFile->write(QJsonDocument(infoTreeObject).toJson(QJsonDocument::Indented));
	}

	QByteArray baOut = QJsonDocument(infoTreeObject).toJson(QJsonDocument::Indented);
	return baOut;
}

QString USBEnumJson::ConvertKindToString(USB_DEVICE_KIND usbDeviceKind)
{
	QString strResult;
	switch (usbDeviceKind)
	{
	case USB_DEVICE_KIND::KIND_MEMORY:
		strResult = "Memory or Disk";
		break; 
	case USB_DEVICE_KIND::KIND_COMPOSITE: 
		strResult = "Composite";
		break; 
	case USB_DEVICE_KIND::KIND_HID:
		strResult = "Human Interface Device";
		break;
	default:
		strResult = "Unknown";
	}
	return strResult;
}

QString USBEnumJson::ConvertCategToString(USB_ENGINE_CATEGORY usbCateg)
{
	QString strResult;
	switch (usbCateg)
	{
	case USB_ENGINE_CATEGORY::GLOBAL:
		strResult = "Computer (Global)";
		break;
	case USB_ENGINE_CATEGORY::USB_HUB:
		strResult = "USB Hub";
		break;
	case USB_ENGINE_CATEGORY::USB_BAD_DEVICE:
		strResult = "Bad Device";
		break;
	case USB_ENGINE_CATEGORY::USB_GOOD_DEVICE:
		strResult = "USB Device";
		break;
	case USB_ENGINE_CATEGORY::USB_NO_DEVICE:
		strResult = "USB Port";
		break;
	case USB_ENGINE_CATEGORY::USB_NO_SS_DEVICE:
		strResult = "USB SS Port";
		break;
	case USB_ENGINE_CATEGORY::USB_SS_GOOD_DEVICE:
		strResult = "USB SS Device";
		break;
	case USB_ENGINE_CATEGORY::USB_HOST_DEVICE:
		strResult = "Host USB Controller";
		break;
	default:
		strResult = "Unknown Device";
	}
	return strResult;
}
