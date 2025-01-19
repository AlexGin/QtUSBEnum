#pragma once

#include <windows.h>
#include <QWidget>
#include <QAbstractNativeEventFilter>

#include "QtUSBEnum.h"

// see:
// http://doc.qt.io/qt-5/qabstractnativeeventfilter.html
// http://www.qtcentre.org/threads/56438-help-with-QAbstractNativeEventFilter
// http://stackoverflow.com/questions/26652783/qtnativeevent-calls
// http://stackoverflow.com/questions/37071142/get-raw-mouse-movement-in-qt

const UINT MYWN_NODECHANGE = WM_USER + 0x101; 
const UINT MYWN_PATHCHANGE = WM_USER + 0x102; 
const UINT MYWN_SEND_DG_TERMINATE = WM_USER + 0x103;

class USBEnumEventFilter : public QAbstractNativeEventFilter
{
    QWidget* m_pMainWndWidget;

public:
	USBEnumEventFilter()
        : m_pMainWndWidget(nullptr)
    {
    }

    void SetMainWndWidget(QWidget* pMainWndWidget)
    {
		m_pMainWndWidget = pMainWndWidget;
    }

    bool IsFilterValid()
    {
        return (bool)(m_pMainWndWidget && GetMainHWnd());
    }

    HWND GetMainHWnd()
    {
        HWND hwnd = (HWND)m_pMainWndWidget->winId();
        return hwnd;
    }

	QtUSBEnum* GetMainWidget() const
    {
        return ((QtUSBEnum*)m_pMainWndWidget);
    }

    virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *) Q_DECL_OVERRIDE
    {
        if (eventType == "windows_generic_MSG")
        {
            MSG *msg = static_cast<MSG*>(message);            
            if (m_pMainWndWidget)
            {
				switch (msg->message)
				{
					case MYWN_NODECHANGE: ((QtUSBEnum*)m_pMainWndWidget)->OnNodeChange((uint)msg->wParam, (uint)msg->lParam); break;
					case MYWN_PATHCHANGE: ((QtUSBEnum*)m_pMainWndWidget)->OnPathChange((uint)msg->wParam, (uint)msg->lParam); break;
					case MYWN_SEND_DG_TERMINATE: ((QtUSBEnum*)m_pMainWndWidget)->OnSendDGTerminate((uint)msg->wParam, (uint)msg->lParam); break;
					case WM_DEVICECHANGE: ((QtUSBEnum*)m_pMainWndWidget)->OnDeviceChange((uint)msg->wParam, (uint)msg->lParam); break;
					case WM_SIZE:  ((QtUSBEnum*)m_pMainWndWidget)->OnSizeMainWnd((uint)msg->wParam, (uint)msg->lParam); break;
					case WM_CLOSE: ((QtUSBEnum*)m_pMainWndWidget)->OnCloseMainWnd(); break;
				}
            }
        }
        return false;
    }
};
