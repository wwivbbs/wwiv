
==============================================================================
                             WWIV 5.0 Telnet Server
                      Copyright 2002-2004 Rushfan
==============================================================================

GENERAL INFORMATION:

    WWIV 5.0 Telnet Server is can be built with the following compilers:
        MS Visual C++ .NET 2013 (Standard/Pro/Enterprise/Architect)

    The arguments for WWIV in the telnet server are "-XT -H@H -N@N" 


==============================================================================

LABEL: WWIV-5_0_??
DATE:  

Upgrade compiler to VC2013


==============================================================================

LABEL: WWIV-5_0_57
DATE:  10/22/2004


Moved WSAStartup to the Document Initialization (in preparation for more than
one socket listener being in place)

Added a call to WSACleanup in the Document destructor (this wasn't being done
before and could have been a potential memory/resource leak)

Fixed default logging style - It used to be FileSystem (with an empty 
filename), but now it's None.

Fixed help file generation under Visual Studio.NET 2003

Created an XML manifest for Windows XP so it will use the ComCtrl6 if present.

Added listener to respawn the icon when the taskbar is recreated (i.e. when
explorer crashes)

Added support for showing a popup balloon when WTS is minimized.

Added new preference to support showing balloon popups on connect/disconnect 
when WTS is minimized (it defaults to ON)

Note: the toolbar balloon support requires WTS be compiled against a Platform
SDK that supports Windows XP *AND* you are on Windows XP.  MSVC.NET 2003
includes a Platform SDK supports XP, however MSVC6 does NOT out of the box.

A couple of UNICODE fixes.

Fixed build under MSVC 6.0

==============================================================================

LABEL: WWIV-5_0_41
DATE:  01/10/2003

Added Preferences option to have the telnet server run the beginday event, If
checked, each day WWIVTelnetServer (WTS) will run the "local node" (as defined
in the WTS preferences) with the -E command to execute the beginday event.  If
the Local node is not 1, then edit wwiv.ini and add/edit the following line,
chanign the 1 to be the node number defined as your local node:
BEGINDAYNODENUMBER = 1


==============================================================================

LABEL: WWIV-5_0_40
DATE:  01/02/2003

Fixed garbage from getting logged where the command line should have been.

Quit scrolling to the top when nothing in the log has changed

Now the log scrolls to the end of the log when a new line is added



==============================================================================

LABEL: WWIV-5_0_39
DATE:  12/15/2002

More work on the help file.

Added "Launch WWIV Minimized" option to the main preferences page.  If this 
is checked then wwiv will be launched in a minimized console window.  This
is so that bbs windows don't pop up while you are on the machine.  Under 
Windows 9x performance is not as good for minimized windows, however this is
a good option to enable for Windows NT/2000/XP as mimized windows are no 
slower than normal windows.

Started on check for updates functionality.

==============================================================================

LABEL: WWIV-5_0_39
DATE:  11/05/2002

Added support for seeing the bbs status when a user is online in the telnet 
server

Fixed the version number from not showing up in the about box

Added code to read config.dat instead of assuming ${WWIV_HOME}\DATA is the 
data directory for the BBS.

Created a basic help file.

Added support for sounds on logon and logoff (configurable Bunder "Sounds" 
in the Preferences Dialog)

Added About and Exit commands to the popup of the system tray

Added "Run Local Node" to the file menu, this will execute the node number
defined in the General page of the preferences (using the same exe as 
defined for wwiv above it).  Also put this menu item on the context menu
of the system tray

Added shell for Help file



