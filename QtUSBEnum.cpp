#include "QtUSBEnum.h"
#include "HttpDaemon.h"
#include "UdpNotifyServer.h"
#include "USBEnumEventFilter.h"

#include <QVBoxLayout>
#include <QSplitter>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QString>
#include <QStringList>
#include <QMessageBox>
#include <QFileDialog>
#include <QTimer>
#include <QLabel>
#include <QSettings>
#include <QtNetwork>
#include <QClipboard>

#include "FileUtils.h"
#include "LogFile.h"

extern CLogFile g_log;
extern QApplication* g_pApp;

QPixmap* g_pPixmapSmallIcons;
INDEX_OF_PORT g_nIndexOfPort;

extern std::shared_ptr<USBEnumEventFilter> g_spUSBEnumEventFilter;

QtUSBEnum::QtUSBEnum(QWidget *parent)
	: QMainWindow(parent), m_nHttpPort(5000), m_nUdpPort(5300), m_nUdpNotifyPeriod(1000),
	m_nUsbRemoveWaitTimeout(3500), m_nShowMainWindowOnStart(1), m_nUsbDevicechangeGuidFlags(0),
	m_nTotalNodes(0), m_nTotalHubs(0), m_nTotalRootHubs(0), m_nUdpMountvolNotify(0),
	m_dwCurrentCheckSumm(0), m_bChangeDecodedFromOS(false)
{
	ui.setupUi(this);

	// ui.mainToolBar->setVisible(false); 
	ui.mainToolBar->setMovable(false);

	g_pPixmapSmallIcons = new QPixmap(":/images/smallico.bmp");
	
	m_pVBoxLayout = new QVBoxLayout(this);
	
	m_pTreeWidget = new QTreeWidget(this);
	m_pTreeWidget->setColumnCount(11);
	m_pTreeWidget->setColumnWidth(0, 185);  // "Hub"
	m_pTreeWidget->setColumnWidth(1, 50);   // "Port"
	m_pTreeWidget->setColumnWidth(2, 50);   // "Is Hub"
	m_pTreeWidget->setColumnWidth(3, 145);  // "Type of Device"
	m_pTreeWidget->setColumnWidth(4, 175);  // "Device ID (Serial Number)"
	m_pTreeWidget->setColumnWidth(5, 50);   // "Volume"
	m_pTreeWidget->setColumnWidth(6, 135);  // "GUID of Volume" 
	m_pTreeWidget->setColumnWidth(7, 135);  // "Vendor" 
	m_pTreeWidget->setColumnWidth(8, 50);   // "Manufacturer"
	m_pTreeWidget->setColumnWidth(9, 135);  // "Product Name" 
	m_pTreeWidget->setColumnWidth(10, 135); // "Serial Number"

	m_pLabelPortHubInfo = new QLabel(this); 
	m_pLabelPortHubInfo->setMaximumWidth(397);
	m_pLabelPortHubInfo->setMinimumWidth(397);
	m_pLabelPortHubInfo->setText("Application Started!");
	ui.statusBar->addWidget(m_pLabelPortHubInfo);

	m_pPlainTextEdit = new QPlainTextEdit(this);

	m_pUSBEnumJson = new USBEnumJson();
	m_dtLastUpdate = QDateTime::currentDateTime(); // Set on start Application

	Init();
	g_log.SetLogSettings(m_logFileSettings); // Execute only one time - on start
	g_log.CleanUpLogFiles();				 // Execute only one time - on start

	DWORD nThreadID = ::GetCurrentThreadId();
	g_log.SaveLogFile(LOG_MODES::DEBUGING, "Application QtUSBEnum started (Thread ID = %u)!", nThreadID);

	m_Splitter = new QSplitter(m_bSplitModeHorizontal ? Qt::Horizontal : Qt::Vertical, this);
	m_Splitter->addWidget(m_pTreeWidget);
	m_Splitter->addWidget(m_pPlainTextEdit);
	m_pVBoxLayout->addWidget(m_Splitter);
	
	connect(m_pTreeWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QtUSBEnum::OnSelectionChanged); 

	ui.mainToolBar->addAction(ui.actionRefresh); 
	ui.mainToolBar->addSeparator();
	ui.mainToolBar->addAction(ui.actionEntire_Nodes_tree); 
	ui.mainToolBar->addAction(ui.actionEntire_active_Nodes_tree);
	ui.mainToolBar->addAction(ui.actionEntire_Hubs_tree); 
	ui.mainToolBar->addSeparator();
	ui.mainToolBar->addAction(ui.actionAbout_QtUSBEnum); 
	ui.mainToolBar->addAction(ui.actionMinimize_to_tray);
	ui.mainToolBar->addAction(ui.actionQuit);

	connect(ui.actionRefresh, SIGNAL(triggered()), this, SLOT(OnRefresh()));
	connect(ui.actionQuit, SIGNAL(triggered()), this, SLOT(OnExit()));
	connect(ui.actionAbout_QtUSBEnum, SIGNAL(triggered()), this, SLOT(OnAbout())); 
	connect(ui.actionShow_Full_Screen, SIGNAL(triggered()), this, SLOT(OnShowFullScreen()));
	connect(ui.actionShow_Maximized, SIGNAL(triggered()), this, SLOT(OnShowMaximized()));
	connect(ui.actionMinimize_to_tray, SIGNAL(triggered()), this, SLOT(OnMinimize()));
	connect(ui.actionEntire_Nodes_tree, SIGNAL(triggered()), this, SLOT(OnFileSaveEntireNodesTree())); 
	connect(ui.actionEntire_active_Nodes_tree, SIGNAL(triggered()), this, SLOT(OnFileSaveEntireActiveNodesTree())); 
	connect(ui.actionEntire_Hubs_tree, SIGNAL(triggered()), this, SLOT(OnFileSaveEntireHubsTree()));
	
	QList<int> sizes;
	sizes << m_nFirstSize << m_nSecondSize;
	m_Splitter->setSizes(sizes);

	m_pHttpDaemon = new HttpDaemon((qint16)m_nHttpPort, this); 
	m_pUdpNotifyServer = new UdpNotifyServer(m_strUdpClientIpAddr, m_nUdpPort, m_nUdpRepeatCounter, m_nUdpNotifyPeriod, this);
	connect(m_pHttpDaemon, &HttpDaemon::GETRequestRecived, m_pUdpNotifyServer, &UdpNotifyServer::OnReceiveFeedback);
	connect(m_pHttpDaemon, SIGNAL(PerformHttpRequest(QByteArray&, const QString&)),
		this, SLOT(OnPerformHttpRequest(QByteArray&, const QString&)));
	
	m_pTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeWidget, &QTreeWidget::customContextMenuRequested, this, &QtUSBEnum::OnShowContextMenu);

	m_trayIcon = new QSystemTrayIcon(this);
	
	m_pPlainTextEdit->setReadOnly(true);
	m_pPlainTextEdit->setStyleSheet("QPlainTextEdit {background: rgb(220, 220, 220); }");

	m_pMinimizeAction = new QAction(tr("Mi&nimize"), this);
	connect(m_pMinimizeAction, &QAction::triggered, this, &QWidget::hide);

	m_pMaximizeAction = new QAction(tr("Ma&ximize"), this);
	connect(m_pMaximizeAction, &QAction::triggered, this, &QWidget::showMaximized);

	m_pRestoreAction = new QAction(tr("&Restore"), this);
	connect(m_pRestoreAction, &QAction::triggered, this, &QWidget::showNormal); 

	m_pQuitAction = new QAction(tr("&Quit"), this);
	connect(m_pQuitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

	m_menuTrayIcon = new QMenu(this);
	m_menuTrayIcon->addAction(m_pMinimizeAction);
	m_menuTrayIcon->addAction(m_pMaximizeAction);
	m_menuTrayIcon->addAction(m_pRestoreAction);
	m_menuTrayIcon->addSeparator();
	m_menuTrayIcon->addAction(m_pQuitAction);
	m_trayIcon->setContextMenu(m_menuTrayIcon);

	QIcon iconMain = QIcon(GetMainIcon());
	m_trayIcon->setIcon(iconMain);
	m_trayIcon->setToolTip("QtUSBEnum Application.");
	setWindowIcon(iconMain);

	m_trayIcon->show();

	connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), 
		this, SLOT(OnActivated(QSystemTrayIcon::ActivationReason)));

	QTimer::singleShot(200, this, SLOT(OnTimerStartSingleShot())); // 0.2 sec
}

QtUSBEnum::~QtUSBEnum()
{
	delete m_pVBoxLayout;

	if (m_pUSBEnumJson)
	{
		delete m_pUSBEnumJson;
		m_pUSBEnumJson = nullptr;
	}

	if (m_pViewer)
	{
		delete m_pViewer; 
		m_pViewer = nullptr;
	}

	if (g_pPixmapSmallIcons)
	{
		delete g_pPixmapSmallIcons;
		g_pPixmapSmallIcons = nullptr;
	}
}

void QtUSBEnum::Init()
{
	std::string sConfigIni(CFileUtils::RetrieveParentPath() + "CFGUSBEnum.ini");
	QString strConfigINIFileName(sConfigIni.c_str());

	// .ini format example:
	// https://stackoverflow.com/questions/14365653/how-to-load-settings-in-qt-app-with-qsettings
	QSettings s(strConfigINIFileName, QSettings::IniFormat);
		
	m_nHttpPort = s.value("USB_ENUM_HTTP_SETTINGS/HTTP_PORT").toInt(); 

	m_nUdpPort = s.value("USB_ENUM_UDP_SETTINGS/UDP_PORT").toInt(); 
	m_strUdpClientIpAddr = s.value("USB_ENUM_UDP_SETTINGS/UDP_CLIENT_IP_ADDR").toString();
	m_nUdpNotifyPeriod = s.value("USB_ENUM_UDP_SETTINGS/UDP_NOTIFY_PERIOD").toInt();
	m_nUdpRepeatCounter = s.value("USB_ENUM_UDP_SETTINGS/UDP_REPEAT_COUNTER").toInt(); 
	m_nUdpMountvolNotify = s.value("USB_ENUM_UDP_SETTINGS/UDP_MOUNTVOL_NOTIFY").toInt();

	m_nUsbDevicechangeGuidFlags = s.value("USB_ENUM_ENGINE_SETTINGS/USB_DEVICECHANGE_GUID_FLAGS").toInt();
	m_nUsbRemoveWaitTimeout = s.value("USB_ENUM_ENGINE_SETTINGS/USB_REMOVE_WAIT_TIMEOUT_MS").toInt();
	m_nShowMainWindowOnStart = s.value("USB_ENUM_ENGINE_SETTINGS/SHOW_MAIN_WINDOW_ON_START").toInt(); 

	m_nFirstSize = s.value("USB_ENUM_GUI_SETTINGS/SPLIT_FIRST_SIZE").toInt();
	m_nSecondSize = s.value("USB_ENUM_GUI_SETTINGS/SPLIT_SECOND_SIZE").toInt();
	int nSplitModeHorizontal = s.value("USB_ENUM_GUI_SETTINGS/SPLIT_MODE_HORIZONTAL").toInt();
	m_bSplitModeHorizontal = (bool)(1 == nSplitModeHorizontal);

	std::string sLogFilePath = s.value("USB_ENUM_LOG_SETTINGS/LOG_FILE_PATH").toString().toStdString();
	int nLogFileAutoRemoveEnable = s.value("USB_ENUM_LOG_SETTINGS/LOG_AUTO_REMOVE_ENABLE").toInt();
	bool bLogFileAutoRemoveEnable = (bool)(1 == nLogFileAutoRemoveEnable);
	int nLogRemoveDays = s.value("USB_ENUM_LOG_SETTINGS/LOG_REMOVE_DAYS").toInt();
	m_logFileSettings = std::make_tuple(sLogFilePath, bLogFileAutoRemoveEnable, nLogRemoveDays);

	int nRootHubBaseOffset = s.value("USB_ENUM_ROOT_HUB_SETTINGS/ROOT_HUB_BASE_OFFSET").toInt();
	int nExtHubBaseOffset = s.value("USB_ENUM_EXT_HUB_SETTINGS/EXT_HUB_BASE_OFFSET").toInt();
	int nExtHubUseDeviceAddr = s.value("USB_ENUM_EXT_HUB_SETTINGS/EXT_HUB_USE_DEVICE_ADDR").toInt();
	bool bExtHubUseDeviceAddr = (bool)(1 == nExtHubUseDeviceAddr); 
	m_usbEnumSettings = std::make_tuple(nRootHubBaseOffset, nExtHubBaseOffset, bExtHubUseDeviceAddr); 

	UINT nMonitoringThreadEnable = s.value("PATH_MONITORING_THREAD_SETTINGS/MONITORING_THREAD_ENABLE").toInt(); 
	UINT nPoolingWaitTimeMs = s.value("PATH_MONITORING_THREAD_SETTINGS/POOLING_WAIT_TIME_MS").toInt(); 	
	// UINT nChengeDriveWaitTimeMs = s.value("PATH_MONITORING_THREAD_SETTINGS/CHANGE_DRIVE_WAIT_TIME_MS").toInt(); 
	UINT nStopWaitTimeMs = s.value("PATH_MONITORING_THREAD_SETTINGS/STOP_WAIT_TIME_MS").toInt();
	UINT nSaveTimeLogEnable = s.value("PATH_MONITORING_THREAD_SETTINGS/SAVE_TIME_LOG_ENABLE").toInt();
	bool bMonitoringThreadEnable = (bool)(1 == nMonitoringThreadEnable); 
	bool bSaveTimeLogEnable = (bool)(1 == nSaveTimeLogEnable);
	m_monitorThreadSettings = std::make_tuple(bMonitoringThreadEnable,
		nPoolingWaitTimeMs, nStopWaitTimeMs, bSaveTimeLogEnable);
}

void QtUSBEnum::OnAbout()
{
	if (g_pApp)
	{
		QString strVersion = "Version: " + g_pApp->applicationVersion();
		std::string sDate = __DATE__;
		std::string sTime = __TIME__;
		QString strDateTime = QString::asprintf("Generated: '%s' %s",sDate.c_str(), sTime.c_str());
		QMessageBox msgBoxAbout;
		QString cInfoText = "Qt USB Enumerator - " + strVersion + "\r\n" + strDateTime;
		msgBoxAbout.setText("USB Enumerator Application");
		msgBoxAbout.setInformativeText(cInfoText);
		msgBoxAbout.setIcon(QMessageBox::Information);
		msgBoxAbout.setStandardButtons(QMessageBox::Ok);
		msgBoxAbout.setDefaultButton(QMessageBox::Ok);
		msgBoxAbout.exec();
	}
}

void QtUSBEnum::OnExit()
{
	this->close();
	g_log.SaveLogFile(LOG_MODES::DEBUGING, "Application exited!");
}

void QtUSBEnum::OnActivated(QSystemTrayIcon::ActivationReason reason)
{
	switch (reason)
	{
	case QSystemTrayIcon::Trigger:
	case QSystemTrayIcon::DoubleClick:
	case QSystemTrayIcon::MiddleClick:
		this->showMaximized();
		// this->showFullScreen();
		// this->show();
	}
}

void QtUSBEnum::OnMinimize()
{
	this->hide();
}

void QtUSBEnum::OnShowMaximized()
{
	this->showMaximized();
}

void QtUSBEnum::OnShowFullScreen()
{
	this->showFullScreen();
}

void QtUSBEnum::OnShowContextMenu(const QPoint &pos)
{
	QPoint ptPos(pos.x(), pos.y() + 55);

	QTreeWidgetItem *pTreeItem = m_pTreeWidget->currentItem();

	QString strDeviceID = pTreeItem->text(4); // Column 4 -> Device ID

	QString strPath = pTreeItem->text(5); // Column 5 -> Path
	QString strGuid = pTreeItem->text(6); // Column 6 -> Guid

	if (!strPath.isEmpty() && !strGuid.isEmpty())
	{
		m_strDriverPath = strPath;
		m_strDriverGuid = strGuid;
		m_strDeviceID = strDeviceID;

		QMenu contextMenu("Context_menu_1", m_pTreeWidget);

		QAction action1("Copy Driver's Path to Clipboard", m_pTreeWidget);
		QAction action2("Copy Driver's GUID to Clipboard", m_pTreeWidget); 
		QAction action3("Copy Divice-ID to Clipboard", m_pTreeWidget);

		connect(&action1, SIGNAL(triggered()), this, SLOT(OnCopyDriverPathToClipboard()));
		connect(&action2, SIGNAL(triggered()), this, SLOT(OnCopyDriverGuidToClipboard())); 
		connect(&action3, SIGNAL(triggered()), this, SLOT(OnCopyDiviceIdToClipboard()));

		contextMenu.addAction(&action1);
		contextMenu.addAction(&action2); 
		contextMenu.addSeparator(); 
		contextMenu.addAction(&action3);

		contextMenu.exec(mapToGlobal(ptPos/*pos*/));
	}
	else if (!strDeviceID.isEmpty())
	{
		m_strDeviceID = strDeviceID; 

		QMenu contextMenu("Context_menu_2", m_pTreeWidget);

		QAction action11("Copy Divice-ID to Clipboard", m_pTreeWidget);

		connect(&action11, SIGNAL(triggered()), this, SLOT(OnCopyDiviceIdToClipboard())); 

		contextMenu.addAction(&action11);
		
		contextMenu.exec(mapToGlobal(ptPos/*pos*/));
	}
	else
	{
		QMessageBox::information(this, "No Drive-info or Device ID present", 
			"For this node Node NOT exist Path, GUID and Device ID info!");
	}
}

void QtUSBEnum::OnChangeDecoded(DWORD dwCoolCheckSumm)
{
	m_dwCurrentCheckSumm = dwCoolCheckSumm;
}

void QtUSBEnum::OnCopyDriverPathToClipboard()
{
	QClipboard *ptrClipboard = QGuiApplication::clipboard(); 
	ptrClipboard->setText(m_strDriverPath);
}

void QtUSBEnum::OnCopyDriverGuidToClipboard()
{
	QClipboard *ptrClipboard = QGuiApplication::clipboard();
	ptrClipboard->setText(m_strDriverGuid);
}

void QtUSBEnum::OnCopyDiviceIdToClipboard()
{
	QClipboard *ptrClipboard = QGuiApplication::clipboard();
	ptrClipboard->setText(m_strDeviceID);
}

void QtUSBEnum::OnTimerStartSingleShot()
{
	if (m_nShowMainWindowOnStart != 1)
		this->hide();
	
	g_log.SaveLogFile(LOG_MODES::DEBUGING, "QtUSBEnum::OnTimerStartSingleShot");
	RefreshUSBTreeEnum(true, true);
}

void QtUSBEnum::OnTimerDeviceRemoveSingleShot()
{
	g_log.SaveLogFile(LOG_MODES::DEBUGING, "QtUSBEnum::OnTimerDeviceRemoveSingleShot: OnRefresh");
	OnRefreshDeviceChange();
	//if (m_pUdpNotifyServer != nullptr)
	//{
	//	m_pUdpNotifyServer->Notify("QtUSBEnum::OnDeviceChange");
	//}
}

void QtUSBEnum::OnDeviceChange(WPARAM wParam, LPARAM lParam)
{
  m_dtLastUpdate = QDateTime::currentDateTime(); // Set if Device Change execute
  uint uEventID = (uint)(wParam);
  if ((uEventID == DBT_DEVICEARRIVAL) || (uEventID == DBT_DEVICEREMOVECOMPLETE))
  {
	  // emit DeviceChange();
	  m_bChangeDecodedFromOS = true;
  }
  switch (uEventID)
  {
	case DBT_DEVICEARRIVAL: 
	{		
		OnRefresh(); 
		//if (m_pUdpNotifyServer != nullptr)
		//{
		//	m_pUdpNotifyServer->Notify("QtUSBEnum::OnDeviceChange");
		//}
		g_log.SaveLogFile(LOG_MODES::DEBUGING, "QtUSBEnum::OnDeviceChange: DBT_DEVICEARRIVAL");
	}
	break;
	case DBT_DEVICEREMOVECOMPLETE: 
	{		
		g_log.SaveLogFile(LOG_MODES::DEBUGING, "QtUSBEnum::OnDeviceChange: DBT_DEVICEREMOVECOMPLETE"); 
		if (m_nUsbRemoveWaitTimeout > 0)
			QTimer::singleShot(m_nUsbRemoveWaitTimeout, this, SLOT(OnTimerDeviceRemoveSingleShot())); // Timeout - msec
		else
			OnTimerDeviceRemoveSingleShot();
	}
	break;
  }
}

void QtUSBEnum::OnRefresh()
{
	emit DeviceChange();
	m_pTreeWidget->clear();
	RefreshUSBTreeEnum(false, true);
}

void QtUSBEnum::OnRefreshDeviceChange()
{
	emit DeviceChange();
	m_pTreeWidget->clear();
	RefreshUSBTreeEnum(false, true);
}

void QtUSBEnum::OnRefreshOnlyTree()
{
	m_pTreeWidget->clear();
	RefreshUSBTreeEnum(false, false);
}

void QtUSBEnum::OnPathChange(WPARAM wParam, LPARAM lParam)
{
	// OnRefreshOnlyTree(); 
	// OnRefresh(); 
	if (wParam == 1)
	{
		if (m_nUdpMountvolNotify == 1)
			m_dwCurrentCheckSumm = (DWORD)lParam; 

		QTimer::singleShot(50, this, SLOT(OnTimerPathChangeSingleShot())); // 0.05 sec
	}
}

void QtUSBEnum::OnTimerPathChangeSingleShot()
{
	// OnRefresh(); 
	OnRefreshOnlyTree();
}

void QtUSBEnum::OnSendDGTerminate(WPARAM wParam, LPARAM lParam)
{
	if (wParam >= 1000)
	{
		int iIndex = (int)(wParam - 1000);
		UINT nThreadID = (UINT)lParam;
		m_pUdpNotifyServer->ClearNotifyItem(nThreadID, iIndex);
	}
}

void QtUSBEnum::resizeEvent(QResizeEvent* event)
{
	QMainWindow::resizeEvent(event);

	QRect rect = this->geometry();
	int iHeihgt = rect.height();
	int iWidth = rect.width(); 
	int iStatusBarHeight = ui.statusBar->height(); 
	int iMenuBarHeight = ui.menuBar->height(); 
	int iToolBarHeight = ui.mainToolBar->height();
	iHeihgt -= (iToolBarHeight + iStatusBarHeight + iMenuBarHeight);
	/*int iToolBarHeight = ui.mainToolBar->height();
	
	iHeihgt -= (iToolBarHeight + iStatusBarHeight + iMenuBarHeight);

	int iWidth = rect.width();
	QRect rectView(0, (iToolBarHeight + iMenuBarHeight), iWidth, iHeihgt);*/
	QRect rectView(0, (iToolBarHeight + iMenuBarHeight), iWidth, iHeihgt);
	if (m_pVBoxLayout != nullptr)
		m_pVBoxLayout->setGeometry(rectView);
}

void QtUSBEnum::OnSelectionChanged()
{
	bool bHasCurrent = m_pTreeWidget->selectionModel()->currentIndex().isValid();
	if (bHasCurrent)
	{
		QTreeWidgetItem *pTreeItem = m_pTreeWidget->currentItem();

		QVariant varVal_hub = pTreeItem->data(0, Qt::UserRole);
		INDEX_OF_PORT nPortIndex = varVal_hub.toULongLong();
		uint nHubID = nPortIndex % 1000;
		uint nPortID = (uint)(nPortIndex / 1000);

		HWND hWnd = g_spUSBEnumEventFilter->GetMainHWnd(); 
		
		::SendMessageA(hWnd, MYWN_NODECHANGE, (WPARAM)nHubID, (LPARAM)nPortID);
	}
}

void QtUSBEnum::OnNodeChange(uint nHubID, uint nPortID)
{
	std::string sHubPort = QtUSBEnum::PrepareHubPortString(nHubID, nPortID);
	QString strHubAndPortId = QString::asprintf("Selected Port/Hub IDs: %s", sHubPort.c_str());
	m_pLabelPortHubInfo->setText(strHubAndPortId);

//	QMessageBox msgBoxInfo;
//	QString strInfoText = "Show Port/Hub IDs: ";
/*	msgBoxInfo.setText(strInfoText);
	msgBoxInfo.setInformativeText(strHubAndPortId);
	msgBoxInfo.setIcon(QMessageBox::Information);
	msgBoxInfo.setStandardButtons(QMessageBox::Ok);
	msgBoxInfo.setDefaultButton(QMessageBox::Ok);
	msgBoxInfo.exec(); */

}

void QtUSBEnum::OnSizeMainWnd(uint wParam, uint lParam)
{
	if (wParam == SIZE_MINIMIZED)
		this->hide();
}

void QtUSBEnum::OnCloseMainWnd()
{
	m_spUsbTree->ClearData();
	m_pViewer->ListsClear(); 
	m_pViewer->StopMonitoringThread();
}

QPixmap QtUSBEnum::GetIconByIndex(int nIconIndex)
{
	QPixmap pm;
	if (!g_pPixmapSmallIcons)
	{
		return pm;
	}

	pm = g_pPixmapSmallIcons->copy(16 * nIconIndex, 0, 16, 16);
	return pm;
}

QPixmap QtUSBEnum::GetCategoryIcon(USB_ENGINE_CATEGORY usbCateg)
{
	int iImageIndex = (int)(usbCateg); // Category's images starts form position "zero"
	return GetIconByIndex(iImageIndex);
}

QPixmap QtUSBEnum::GetIconOfKind(USB_DEVICE_KIND kind)
{
	if (USB_DEVICE_KIND::UNKNOWN == kind)
	{
		QPixmap pm;
		return pm;
	}
	// Category's images starts form position 8 (eight), but first - is not used:
	int iImageIndex = (int)(kind) + 8 - 1;  
	return GetIconByIndex(iImageIndex);
}

QPixmap  QtUSBEnum::GetMainIcon()
{
	return GetIconByIndex(11);
}

std::string QtUSBEnum::CorrectVolumePathString(const std::string& sVolumePathString)
{
	int iLen = (int)sVolumePathString.size();
	if (iLen > 0)
	{
		char* szVolumePath = new char[MAX_PATH];
		::ZeroMemory(szVolumePath, MAX_PATH);
		strcpy(szVolumePath, sVolumePathString.c_str());

		int nIndex = iLen - 1;
		szVolumePath[nIndex] = '\0';

		std::string strVolumePathString = szVolumePath;

		if (szVolumePath != nullptr)
			delete[] szVolumePath;

		return strVolumePathString;
	}
	else
		return "";
}

QString QtUSBEnum::ConvertVectorVolumePath(const PATH_VECTOR& vectVolumePath)
{
	QString strResult; 
	size_t nSize = vectVolumePath.size();
	if (nSize > 1)
	{		
		for (size_t i = 0; i < nSize; i++)
		{			
			QString strPath = QString::fromLocal8Bit(CorrectVolumePathString(vectVolumePath[i]).c_str());
			if (i != (nSize - 1))
				strResult += strPath + ", ";
			else
				strResult += strPath;
		}
	}
	else
	{
		if (nSize > 0)
			strResult = QString::fromLocal8Bit(CorrectVolumePathString(vectVolumePath[0]).c_str());
	}
	return strResult;
}

QStringList QtUSBEnum::ConvertUSBDeviceData(SP_USB_DEV spDev)
{
	QStringList listResult; 
	if (spDev != nullptr)
	{
		listResult.append(QString::number(spDev->GetParentHub()));
		listResult.append(QString::number(spDev->GetPort()));

		auto categ = spDev->GetUSBCategory();
		bool bIsHub = (bool)(USB_ENGINE_CATEGORY::USB_HUB == categ);
		listResult.append(bIsHub ? "Hub" : "No hub");
		
		listResult.append(QString::fromLocal8Bit(spDev->GetTypeOfDevice().c_str())); // The: "QString::fromLocal8Bit" it's a Cyrillic font 
		listResult.append(QString::fromLocal8Bit(spDev->GetDeviceId().c_str()));
		QString strVolumePath = ConvertVectorVolumePath(spDev->GetVectorVolumePath());
		listResult.append(strVolumePath);
		// listResult.append(QString::fromLocal8Bit(spDev->GetVolumePathString().c_str())); 
		listResult.append(QString::fromLocal8Bit(spDev->GetGuidOfVolume().c_str()));

		listResult.append(QString::fromLocal8Bit(spDev->GetVendorOfDevice().c_str()));
		listResult.append(QString::fromLocal8Bit(spDev->GetManufacturer().c_str()));
		listResult.append(QString::fromLocal8Bit(spDev->GetProductName().c_str()));
		listResult.append(QString::fromLocal8Bit(spDev->GetSNOfDevice().c_str()));
	}
	return listResult;
}

void QtUSBEnum::PrepareTestTree()
{
	m_spUsbTree.reset(new USBEnum::USBDevice(
		0, 0, false, USB_ENGINE_CATEGORY::GLOBAL, "Computer", "123456789", "", ""));

	SP_USB_DEV spData1(new USBEnum::USBDevice(
		1, 1, true, USB_ENGINE_CATEGORY::USB_HUB, "USB Hub", "1755", "", ""));
	m_spUsbTree->AddChild(spData1);

	SP_USB_DEV spData11(new USBEnum::USBDevice(
		11, 11, false, USB_ENGINE_CATEGORY::USB_GOOD_DEVICE, "USB Memory", "5795", "", ""));
	spData1->AddChild(spData11);

	SP_USB_DEV spData12(new USBEnum::USBDevice(
		11, 12, false, USB_ENGINE_CATEGORY::USB_NO_DEVICE, "USB-Port", "6735", "", ""));
	spData1->AddChild(spData12);

	SP_USB_DEV spData13(new USBEnum::USBDevice(
		11, 13, false, USB_ENGINE_CATEGORY::USB_NO_SS_DEVICE, "USB-SS-Port", "7713XF", "", ""));
	spData1->AddChild(spData13);

	SP_USB_DEV spData2(new USBEnum::USBDevice(
		0, 2, false, USB_ENGINE_CATEGORY::USB_SS_GOOD_DEVICE, "USB Memory-stick", "2713", "", ""));
	m_spUsbTree->AddChild(spData2);

	SP_USB_DEV spData3(new USBEnum::USBDevice(
		0, 3, false, USB_ENGINE_CATEGORY::USB_GOOD_DEVICE, "USB Memory-stick", "3734916AF", "", ""));
	m_spUsbTree->AddChild(spData3);

	SP_USB_DEV spData4(new USBEnum::USBDevice(
		4, 4, true, USB_ENGINE_CATEGORY::USB_HUB, "USB Hub", "97468GH", "", ""));
	m_spUsbTree->AddChild(spData4);

	SP_USB_DEV spData41(new USBEnum::USBDevice(
		44, 41, false, USB_ENGINE_CATEGORY::USB_SS_GOOD_DEVICE, "USB Memory", "57841WZ", "", ""));
	spData4->AddChild(spData41);

	SP_USB_DEV spData42(new USBEnum::USBDevice(
		44, 42, false, USB_ENGINE_CATEGORY::USB_GOOD_DEVICE, "USB Memory", "86497LX", "", ""));
	spData4->AddChild(spData42);

	SP_USB_DEV spData43(new USBEnum::USBDevice(
		44, 43, false, USB_ENGINE_CATEGORY::USB_GOOD_DEVICE, "USB Memory", "474532", "", ""));
	spData4->AddChild(spData43); 

	SP_USB_DEV spData44(new USBEnum::USBDevice(
		44, 44, false, USB_ENGINE_CATEGORY::USB_NO_DEVICE, "USB-port", "972ER", "", ""));
	spData4->AddChild(spData44);

	SP_USB_DEV spData45(new USBEnum::USBDevice(
		44, 45, false, USB_ENGINE_CATEGORY::USB_NO_SS_DEVICE, "USB-ss-Port", "812WTR", "", ""));
	spData4->AddChild(spData45);

	SP_USB_DEV spData5(new USBEnum::USBDevice(
		0, 5, false, USB_ENGINE_CATEGORY::USB_BAD_DEVICE, " "));
	m_spUsbTree->AddChild(spData5);
}

void QtUSBEnum::RefreshUSBTreeEnum(bool bInttializeView, bool bRefreshWithPathes)
{	
//	PrepareTestTree(); // It is for Testing purposes // DEBUG !!!
	m_nTotalNodes = 0;
	m_nTotalHubs = 0;
	m_nTotalRootHubs = 0;
	HWND hWnd = g_spUSBEnumEventFilter->GetMainHWnd();

	if (bInttializeView)
	{
		m_spUsbTree.reset(new USBEnum::USBDevice(
			0, 0, false, USB_ENGINE_CATEGORY::GLOBAL, "My Computer"));
	}
	else
	{
		m_spUsbTree->ClearData();
	}

	if (bInttializeView)
	{
		m_pViewer = new CUVCViewer(m_spUsbTree, m_nUsbDevicechangeGuidFlags, m_usbEnumSettings, m_monitorThreadSettings);
		connect(this, SIGNAL(DeviceChange()), m_pViewer, SLOT(OnDeviceChangeMonitorLock()));
		connect(this, SIGNAL(RefreshComplete()), m_pViewer, SLOT(OnRefreshComplete()));
		connect(m_pViewer, SIGNAL(ChangeDecoded(DWORD)), this, SLOT(OnChangeDecoded(DWORD)));
		m_pViewer->InitViewer(hWnd);
	}
	else
	{	
		m_pViewer->RefreshTree(bRefreshWithPathes);
	}
		
	g_log.SaveLogFile(LOG_MODES::DEBUGING, "QtUSBEnum::RefreshUSBTreeEnum: Refresh TreeView-Collection is completed!"); 
	if ((!bInttializeView) && (m_dwCurrentCheckSumm > 0)) // && m_bChangeDecodedFromOS)
	{		
		if (!m_nUdpMountvolNotify)
		{
			if (m_bChangeDecodedFromOS)
			{
				m_bChangeDecodedFromOS = false;
				if (m_pUdpNotifyServer != nullptr)
				{
					m_pUdpNotifyServer->Notify(m_dwCurrentCheckSumm, "UDP(MN-0)");
				}
				m_dwCurrentCheckSumm = 0;
			}
		}
		else
		{
			m_bChangeDecodedFromOS = false;
			if (m_pUdpNotifyServer != nullptr)
			{
				m_pUdpNotifyServer->Notify(m_dwCurrentCheckSumm, "UDP(MN-1)");
			}
			m_dwCurrentCheckSumm = 0;
		}
	}

	QStringList slCaption;
	slCaption << "Tree (Hubs and other)" << "Port" << "Is Hub" << "Type of Device"
		<< "Device ID" << "Volume" << "GUID of Volume" << "Vendor" << "Manuf." << "Product Name" << "Serial Number";

	RefreshTreeView(slCaption);

	m_pTreeWidget->expandAll();

	RefreshHubList();

	emit RefreshComplete();
}

void QtUSBEnum::RefreshTreeView(const QStringList &slCaption)
{	
	g_nIndexOfPort = 0;
	m_pTreeWidget->clear();
	m_pTreeWidget->setHeaderLabels(slCaption);
	AddTreeItem(nullptr, m_spUsbTree); // Adding - start from the Root-node
}

void QtUSBEnum::AddTreeItem(QTreeWidgetItem *pParent, SP_USB_DEV spUsbDeviceData)
{
	m_nTotalNodes++;
	QStringList list = ConvertUSBDeviceData(spUsbDeviceData);

	QTreeWidgetItem *pTreeItem = (pParent == nullptr) ?
		new QTreeWidgetItem(m_pTreeWidget) :
		new QTreeWidgetItem(pParent, list);

	INDEX_OF_PORT nPortIndex = spUsbDeviceData->GetPortIndex();
	if (pParent == nullptr) // It's Root node
	{
		int nTotalInList = list.size();
		pTreeItem->setData(0, Qt::UserRole, (QVariant)0L); 
		for (int i = 0; i < nTotalInList; i++)
			pTreeItem->setText(i, list[i]);
	}
	else // It's Child node
	{
		auto categ = spUsbDeviceData->GetUSBCategory();
		if (categ != USB_ENGINE_CATEGORY::USB_HOST_DEVICE)
		{
			if (categ != USB_ENGINE_CATEGORY::USB_HUB)
				pTreeItem->setData(0, Qt::UserRole, (QVariant)nPortIndex);
			else				 
				pTreeItem->setData(0, Qt::UserRole, (QVariant)(spUsbDeviceData->GetParentHub())); 							
		}
		else
			pTreeItem->setData(0, Qt::UserRole, (QVariant)9999000L);
	}

	QIcon iconCateg = QIcon(GetCategoryIcon(spUsbDeviceData->GetUSBCategory()));
	pTreeItem->setIcon(0, iconCateg);
		
	QIcon iconOfKind = QIcon(GetIconOfKind(spUsbDeviceData->GetDeviceKind()));
	pTreeItem->setIcon(5, iconOfKind);
	
	for (auto cit = spUsbDeviceData->GetVectChilds().cbegin(); cit != spUsbDeviceData->GetVectChilds().cend(); cit++)
	{
		SP_USB_DEV spData = *cit; 
		if (spData != nullptr)
		{
			QStringList list2 = ConvertUSBDeviceData(spData);

			if (list2.size() > 0)
			{   // Recursive call:
				AddTreeItem(pTreeItem, spData);
			}
		}
	}
}

void QtUSBEnum::RefreshHubList()
{
	m_pPlainTextEdit->clear();
	QStringList slOutput;

	int nTreeLevel = 0;
	AddHubItem(slOutput, &nTreeLevel, m_spUsbTree);

	QString strOutput;
	for (QString s : slOutput)
	{
		strOutput += s;
	}
	m_pPlainTextEdit->insertPlainText(strOutput);
}

QString QtUSBEnum::PrepareHubInfoAsText(std::shared_ptr<USBEnum::USBHub> spHub, int nLvl)
{
	if (!spHub)
		return "";

	QString strPrefix;
	for (int i = 0; i <= nLvl; i++)
		strPrefix += "    "; // It is 4-x space symbols

	bool bIsRootHub = spHub->GetIsRootHub();

	QString str;
	if (bIsRootHub)
	{
		str = QString::asprintf("HUB-%u ROOT, Device-ID '%s'\r\n", spHub->GetParentHub(), spHub->GetDeviceId().c_str()); 
		m_nTotalRootHubs++;
	}
	else
		str = QString::asprintf("HUB-%u, Device-ID '%s'\r\n", spHub->GetParentHub(), spHub->GetDeviceId().c_str());

	QString strResult = strPrefix + str;
	return strResult;
}

void QtUSBEnum::AddHubItem(QStringList& sl, int* pTreeLevel, SP_USB_DEV spUsbDeviceData)
{
	(*pTreeLevel)++;
	
	auto spHub = std::dynamic_pointer_cast<USBEnum::USBHub>(spUsbDeviceData);
	if (spHub)
	{		
		m_nTotalHubs++;
		QString str = PrepareHubInfoAsText(spHub, *pTreeLevel); 
		sl.append(str);
	}

	for (auto cit = spUsbDeviceData->GetVectChilds().cbegin(); cit != spUsbDeviceData->GetVectChilds().cend(); cit++)
	{
		SP_USB_DEV spData = *cit;
		// Recursive call:
		AddHubItem(sl, pTreeLevel, spData);
	}
	(*pTreeLevel)--;
}

std::string QtUSBEnum::ConvertHubPortNumber(int nNumber)
{
	if (nNumber < 0)
		nNumber = 9999; // Special value

	std::stringstream ssNumber;
	ssNumber << std::setw(4) << std::setfill('0') << nNumber;
	std::string sResult = ssNumber.str();
	return sResult;
}

std::string QtUSBEnum::PrepareHubPortString(int nHubID, int nPortID)
{
	std::string sPort = ConvertHubPortNumber(nPortID);
	std::string sHub = ConvertHubPortNumber(nHubID);
	QString strHubAndPort = QString::asprintf("Port_#%s.Hub_#%s", sPort.c_str(), sHub.c_str()); 
	return strHubAndPort.toStdString();
}

void QtUSBEnum::OnPerformHttpRequest(QByteArray& ba, const QString& strRequest)
{
	HTTP_API_REQUEST request = HTTP_API_REQUEST::UNKNOWN;
	bool bRequestValid = m_pUSBEnumJson->ParseRequestString(strRequest, &request);
	if (bRequestValid)
	{
		if (request == HTTP_API_REQUEST::REQ_LAST_UPDATE_DT)
		{
			auto byteArray = m_pUSBEnumJson->GetDateTimeInfo(m_dtLastUpdate);
			ba = byteArray; 
			return;
		}
		if (request == HTTP_API_REQUEST::REQ_TOTAL_NODES)
		{
			auto byteArray = m_pUSBEnumJson->GetTotalNodesInfo(m_nTotalNodes, m_nTotalHubs, m_nTotalRootHubs);
			ba = byteArray;
			return;
		}
		if ( (request == HTTP_API_REQUEST::REQ_ENTIRE_NODES_TREE) || 
			(request == HTTP_API_REQUEST::REQ_ENTIRE_ACTIVE_TREE) )
		{
			bool bShowActive = (request == HTTP_API_REQUEST::REQ_ENTIRE_ACTIVE_TREE); // If true: show ONLY active nodes
			auto byteArray = m_pUSBEnumJson->GetEntireNodesTree(m_spUsbTree, bShowActive);
			ba = byteArray;
			return;
		}
		if (request == HTTP_API_REQUEST::REQ_ENTIRE_HUBS_TREE)
		{
			auto byteArray = m_pUSBEnumJson->GetEntireHubsTree(m_spUsbTree);
			ba = byteArray;
			return;
		}
	}
}

QString QtUSBEnum::PrepareFileSaveName(const QString& strTitle)
{
	QFileDialog dialog(this, strTitle);
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	dialog.setFileMode(QFileDialog::AnyFile);

	dialog.setNameFilter(tr("Json files(*.json)")); 

	QStringList fileNames;
	if (dialog.exec())
		fileNames = dialog.selectedFiles();

	int fnSize = fileNames.size();
	if (fnSize == 1)
	{
		QString strOutTextFileName = fileNames.at(0);
		return strOutTextFileName;
	}
	return "";
}

void QtUSBEnum::OnFileSaveEntireNodesTree()
{
	QString strFileName = PrepareFileSaveName("Save Entire Nodes Tree");

	QFile fileJson(strFileName);
	fileJson.open(QFile::WriteOnly);

	m_pUSBEnumJson->GetEntireNodesTree(m_spUsbTree, false, &fileJson); // bShowActive = false
}

void QtUSBEnum::OnFileSaveEntireActiveNodesTree()
{
	QString strFileName = PrepareFileSaveName("Save Entire Active Nodes Tree");

	QFile fileJson(strFileName);
	fileJson.open(QFile::WriteOnly);
		
	m_pUSBEnumJson->GetEntireNodesTree(m_spUsbTree, true, &fileJson); // bShowActive = true;
}

void QtUSBEnum::OnFileSaveEntireHubsTree()
{
	QString strFileName = PrepareFileSaveName("Save Entire Hubs Tree");

	QFile fileJson(strFileName);
	fileJson.open(QFile::WriteOnly);

	m_pUSBEnumJson->GetEntireHubsTree(m_spUsbTree, &fileJson);
}