#include "QtUSBEnum.h"
#include <QtWidgets/QApplication>
#include <QSystemSemaphore>
#include <QSharedMemory>
#include <QMessageBox>
#include <QSplashScreen>

#include "USBEnumEventFilter.h"
#include <memory>
#include "LogFile.h"

//#ifdef _DEBUG
//#include "vld.h"
//#endif

CLogFile g_log("QtUSBEnum");
QApplication* g_pApp;

void Free(_Frees_ptr_opt_ HGLOBAL hMem) {
	::GlobalFree(hMem); return;
};

std::shared_ptr<USBEnumEventFilter> g_spUSBEnumEventFilter;

int main(int argc, char *argv[])
{
	QApplication a(argc, argv); 
	QSplashScreen splash(QPixmap(":/images/USBEnumer1.jpg"));
	splash.show();
	a.setApplicationVersion("1.1.19.25");
	g_pApp = &a;
	
	// Single instance support:
	// see: http://blog.aeguana.com/2015/10/15/how-to-run-a-single-app-instance-in-qt/
	QSystemSemaphore sema("QtUSBEnum", 1); // In the original text: "<unique identifier>"
	sema.acquire();

#ifndef Q_OS_WIN32
	// on linux/unix shared memory is not freed upon crash
	// so if there is any trash from previous instance, clean it
	QSharedMemory nix_fix_shmem("QtUSBEnumApplication"); // In the original text: "<unique identifier 2>"
	if (nix_fix_shmem.attach())
	{
		nix_fix_shmem.detach();
	}
#endif

	QSharedMemory shmem("QtUSBEnumApplication"); // In the original text: "<unique identifier 2>"
	bool is_running;
	if (shmem.attach())
	{
		is_running = true;
	}
	else
	{
		shmem.create(1);
		is_running = false;
	}
	sema.release();

	if (is_running)
	{
		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Warning);
		msgBox.setText("You already have the USB Enumerator (QtUSBEnum.exe) running."
			"\r\nOnly one instance is allowed.");
		msgBox.exec();
		return 1;
	}
	// End of Single instance support 
	/* ::Sleep(3500); */ // Splash-screen testing
	
	QtUSBEnum mainWindow;
	g_spUSBEnumEventFilter.reset(new USBEnumEventFilter()); 
	g_spUSBEnumEventFilter->SetMainWndWidget(&mainWindow); 
	a.installNativeEventFilter(g_spUSBEnumEventFilter.get());
	mainWindow.show();
	splash.finish(&mainWindow);
	return a.exec();
}
