The USB bus Enumerator for Windows

Scanning and enumeration whole USB bus in computer, with Windows. \
This application perform preparing Tree-view of the list of USB devices. \
In the application support export results into the JSON output file. 

Also this application include the http-server functional. 

When application is runned, type in your browser: 
http://localhost:5200  \
where 5200 - it is numbar of port (file CFGUSBEnum.ini - section [USB_ENUM_HTTP_SETTINGS]) 

You can see response: 

{  \
    "appfile": "QtUSBEnum.exe", \
    "appname": "Qt USB Enumerator", \
    "appversion": "1.0.7.29",  \
    "datetime": "Creation: Date Jan 19 2025; Time 08:46:42", \
    "hostipaddr": "192.168.100.2",  \
    "hostname": "ALEX-PC",   \
    "qtver": "Qt 5.13.1",    \
    "toolset": "Visual Studio 2017 (v141)",  \
    "typeofbuild": "Release; x86",  \
    "winsdkver": "10.0.22621.0"  \
}  
This is example: fields "appversion" and "datetime" can store differ values.

This requests are supported: \
http://localhost:5200/entire-hubs-tree  \
http://localhost:5200/entire-active-tree  
etc... 

In the file CFGUSBEnum.ini - section [USB_ENUM_UDP_SETTINGS] - defined rules of \
notifivations over UDP to client, when the USB device attach or detach.  
