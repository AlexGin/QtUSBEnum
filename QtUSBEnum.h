#pragma once

#include <QtWidgets/QMainWindow> 
#include <QPixmap>
#include <QSystemTrayIcon>
#include "ui_QtUSBEnum.h"
#include "USBEnum\USBHub.h"
#include <vector> 
#include <string>
#include <sstream>
#include <iomanip> // std::setfill, std::setw support
#include "USBEnum\USBHub.h"
#include "USBEnum\USBDevice.h"
#include "USBEnum\UVCViewer.h"
#include "USBEnumJson.h"

class QVBoxLayout;
class QSplitter;
class QLabel;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class HttpDaemon;
class UdpNotifyServer;

class QtUSBEnum : public QMainWindow
{
	Q_OBJECT
public:
	QtUSBEnum(QWidget *parent = nullptr); 
	~QtUSBEnum();
	void RefreshUSBTreeEnum(bool bInttializeView, bool bRefreshWithPathes);
public:
	void OnPathChange(WPARAM wParam, LPARAM lParam);
	void OnSendDGTerminate(WPARAM wParam, LPARAM lParam);
	void OnDeviceChange(WPARAM wParam, LPARAM lParam);
	void OnCloseMainWnd();
	void OnNodeChange(uint nHubID, uint nPortID); 
	void OnSizeMainWnd(uint wParam, uint lParam);
public: // Add USB Enum (devices) items:
	void RefreshTreeView(const QStringList &slCaption); 
	void RefreshHubList();
private:
	// Recursive methods:
	void AddTreeItem(QTreeWidgetItem *pParent, SP_USB_DEV spUsbDeviceData); // If pParent is "nullptr" -> Root
	void AddHubItem(QStringList& sl, int* pTreeLevel, SP_USB_DEV spUsbDeviceData);
	
private:	
	QString PrepareHubInfoAsText(std::shared_ptr<USBEnum::USBHub> spHub, int nLvl);
	QStringList ConvertUSBDeviceData(SP_USB_DEV spDev); 
	QString ConvertVectorVolumePath(const PATH_VECTOR& vectVolumePath);
private:
	CUVCViewer* m_pViewer; 
	USBEnumJson* m_pUSBEnumJson;
	QDateTime m_dtLastUpdate;
	int m_nTotalNodes;
	int m_nTotalHubs;
	int m_nTotalRootHubs;
private:
	QTreeWidget* m_pTreeWidget;
	QPlainTextEdit* m_pPlainTextEdit; 
	QSplitter* m_Splitter;
	QVBoxLayout* m_pVBoxLayout; 
	QLabel* m_pLabelPortHubInfo;
protected:
	void resizeEvent(QResizeEvent* event) override;
	
private:
	Ui::QtUSBEnumClass ui;
	void PrepareTestTree(); 
	
private:
	SP_USB_DEV m_spUsbTree; 
	int m_nHttpPort;
	int m_nUdpPort; 
	QString m_strUdpClientIpAddr;
	int m_nUdpNotifyPeriod;
	int m_nUdpRepeatCounter;
	HttpDaemon* m_pHttpDaemon;
	UdpNotifyServer* m_pUdpNotifyServer; 
	int m_nUsbDevicechangeGuidFlags;
	int m_nUsbRemoveWaitTimeout; // ms
	int m_nShowMainWindowOnStart;
	int m_nUdpMountvolNotify;
private: // Settings (from the file "CFGUSBEnum.ini"): 
	LOG_FILE_SETTINGS m_logFileSettings;
	USB_ENUM_SETTINGS m_usbEnumSettings; 
	MONITORING_THREAD_SETTINGS m_monitorThreadSettings;
	int  m_nFirstSize;
	int  m_nSecondSize;
	bool m_bSplitModeHorizontal;
	QString PrepareFileSaveName(const QString& strTitle);
private:
	void Init();
	
private:
	QSystemTrayIcon *m_trayIcon; 
	QMenu *m_menuTrayIcon;
	QAction *m_pMinimizeAction;
	QAction *m_pMaximizeAction;
	QAction *m_pRestoreAction; 
	QAction *m_pQuitAction;
	// For copy to Clipboard:
	QString m_strDriverPath;
	QString m_strDriverGuid;
	QString m_strDeviceID;
	bool m_bChangeDecodedFromOS; // Support for Notify (from the UdpNotifyServer)
	DWORD m_dwCurrentCheckSumm;  // Support for Notify (from the UdpNotifyServer)
signals:
	void DeviceChange(); 
	void RefreshComplete();
public slots:
	void OnChangeDecoded(DWORD dwCoolCheckSumm);
private slots: 
	void OnShowContextMenu(const QPoint &pos);
	void OnSelectionChanged(); 
	void OnAbout();
	void OnExit(); 
	void OnRefresh(); 
	void OnRefreshDeviceChange();
	void OnRefreshOnlyTree();
	void OnMinimize();
	void OnShowMaximized(); 
	void OnShowFullScreen();
	void OnFileSaveEntireNodesTree(); 
	void OnFileSaveEntireActiveNodesTree(); 
	void OnFileSaveEntireHubsTree();
	void OnTimerStartSingleShot(); 
	void OnTimerDeviceRemoveSingleShot(); 
	void OnTimerPathChangeSingleShot(); 
	void OnCopyDriverPathToClipboard();
	void OnCopyDriverGuidToClipboard(); 
	void OnCopyDiviceIdToClipboard();
	void OnActivated(QSystemTrayIcon::ActivationReason reason);
	void OnPerformHttpRequest(QByteArray& ba, const QString& strRequest);
private:	
	QPixmap GetCategoryIcon(USB_ENGINE_CATEGORY usbCateg); 
	QPixmap GetIconOfKind(USB_DEVICE_KIND kind);
	QPixmap GetIconByIndex(int nIconIndex);
	QPixmap GetMainIcon();
public:
	static std::string CorrectVolumePathString(const std::string& sVolumePathString);
	static std::string ConvertHubPortNumber(int nNumber);
	static std::string PrepareHubPortString(int nHubID, int nPortID);
};
