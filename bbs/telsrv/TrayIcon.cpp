////////////////////////////////////////////////////////////////
// CTrayIcon Copyright 1996 Microsoft Systems Journal.
//
// If this code works, it was written by Paul DiLascia.
// If not, I don't know who wrote it.

#include "stdafx.h"
#include "trayicon.h"
#include <afxpriv.h>		// for AfxLoadString
#include <shellapi.h>
//#define _WIN32_IE 0x0500
#include <commctrl.h>
#include <shlwapi.h>
#include ".\trayicon.h"

HRESULT CALLBACK DllGetVersion( DLLVERSIONINFO *pdvi );

#define PACKVERSION(major,minor) MAKELONG(minor,major)
DWORD GetDllVersion(LPCTSTR lpszDllName)
{
    DWORD dwVersion = 0;
    /* For security purposes, LoadLibrary should be provided with a 
       fully-qualified path to the DLL. The lpszDllName variable should be
       tested to ensure that it is a fully qualified path before it is used. */
    HINSTANCE hinstDll = LoadLibrary( lpszDllName );
	
    if( hinstDll )
    {
        DLLGETVERSIONPROC pDllGetVersion;
        pDllGetVersion = reinterpret_cast<DLLGETVERSIONPROC>( GetProcAddress( hinstDll, _T( "DllGetVersion" ) ) );

        /* Because some DLLs might not implement this function, you
        must test for it explicitly. Depending on the particular 
        DLL, the lack of a DllGetVersion function can be a useful
        indicator of the version. */

        if ( pDllGetVersion )
        {
            DLLVERSIONINFO dvi;

            ZeroMemory( &dvi, sizeof( dvi ) );
            dvi.cbSize = sizeof( dvi );

            HRESULT hr = ( *pDllGetVersion )( &dvi );

            if( SUCCEEDED( hr ) )
            {
               dwVersion = PACKVERSION( dvi.dwMajorVersion, dvi.dwMinorVersion );
            }
        }

        FreeLibrary( hinstDll );
    }
    return dwVersion;
}


IMPLEMENT_DYNAMIC( CTrayIcon, CCmdTarget )
CTrayIcon::CTrayIcon( UINT uID )
{
	// Initialize NOTIFYICONDATA
    ZeroMemory( &m_nid, sizeof( NOTIFYICONDATA ) );
    DWORD dwVersion = GetDllVersion( _T( "Shell32.dll" ) );
    if( dwVersion >= PACKVERSION( 5,0 ) )
    {
        m_nid.cbSize = sizeof( NOTIFYICONDATA );
    }
    else 
    {
        m_nid.cbSize = NOTIFYICONDATA_V1_SIZE;
    }
	m_nid.uID = uID;	// never changes after construction

	// Use resource string as tip if there is one
	AfxLoadString( uID, m_nid.szTip, sizeof( m_nid.szTip ) );
}


CTrayIcon::~CTrayIcon()
{
	// remove icon from system tray
	SetIcon( 0 ); 
}

// Set notification window. It must created already.
void CTrayIcon::SetNotificationWnd(CWnd* pNotifyWnd, UINT uCbMsg)
{
	// If the following assert fails, you're probably
	// calling me before you created your window. Oops.
	ASSERT(pNotifyWnd==NULL || ::IsWindow(pNotifyWnd->GetSafeHwnd()));
	m_nid.hWnd = pNotifyWnd->GetSafeHwnd();

	ASSERT( uCbMsg==0 || uCbMsg>=WM_USER );
	m_nid.uCallbackMessage = uCbMsg;
}


// This is the main variant for setting the icon.
// Sets both the icon and tooltip from resource ID
// To remove the icon, call SetIcon(0)
BOOL CTrayIcon::SetIcon( UINT uID, LPCTSTR lpszToolTip )
{ 
	HICON hicon = NULL;
	if (uID) 
	{
		AfxLoadString( uID, m_nid.szTip, sizeof( m_nid.szTip ) );
		hicon = AfxGetApp()->LoadIcon( uID );
	}
	return SetIcon( hicon, lpszToolTip );
}

//////////////////
// Common SetIcon for all overloads. 
//
BOOL CTrayIcon::SetIcon( HICON hicon, LPCSTR lpszToolTip, LPCTSTR lpszBalloonTitle, LPCTSTR lpszBalloonText )
{
	UINT msg;
	m_nid.uFlags = 0;

	// Set the icon
	if ( hicon )
	{
		// Add or replace icon in system tray
		msg = m_nid.hIcon ? NIM_MODIFY : NIM_ADD;
		m_nid.hIcon = hicon;
		m_nid.uFlags |= NIF_ICON;
	} 
	else 
	{ 
		// remove icon from tray
		if ( m_nid.hIcon==NULL )
		{
			// already deleted
			return TRUE;		
		}
		msg = NIM_DELETE;
	}

	// Tooltip Support
	if ( lpszToolTip )
	{
		strncpy( m_nid.szTip, lpszToolTip, sizeof( m_nid.szTip ) );
		m_nid.uFlags |= NIF_TIP;
	}

    // Balloon Support
    if ( lpszBalloonText && lpszBalloonTitle )
    {
        strncpy( m_nid.szInfoTitle, lpszBalloonTitle, sizeof( m_nid.szInfoTitle ) );
        strncpy( m_nid.szInfo, lpszBalloonText, sizeof( m_nid.szInfo ) );
        m_nid.uFlags |= NIF_INFO;
        m_nid.uTimeout = 2000; // 2 seconds.
    }

	// Use callback if any
	if ( m_nid.uCallbackMessage && m_nid.hWnd )
	{
		m_nid.uFlags |= NIF_MESSAGE;
	}

	// Let the taskbar know what we want it to do now.
	BOOL bRet = Shell_NotifyIcon( msg, &m_nid );
	if ( msg == NIM_DELETE || !bRet )
	{
		// failed
		m_nid.hIcon = NULL;	
	}
	return bRet;
}


// Default event handler handles right-menu and doubleclick.
// Call this function from your own notification handler.
LRESULT CTrayIcon::OnTrayNotification(WPARAM wID, LPARAM lEvent)
{
	if ( wID != m_nid.uID || ( lEvent != WM_RBUTTONUP && lEvent != WM_LBUTTONDBLCLK ) )
	{
		return 0;
	}

	// If there's a resource menu with the same ID as the icon, use it as 
	// the right-button popup menu. CTrayIcon will interprets the first
	// item in the menu as the default command for WM_LBUTTONDBLCLK
	CMenu menu;
	if (!menu.LoadMenu(m_nid.uID))
	{
		return 0;
	}
	CMenu* pSubMenu = menu.GetSubMenu( 0 );
	if (!pSubMenu) 
	{
		return 0;
	}

	if ( lEvent == WM_RBUTTONUP || lEvent == WM_CONTEXTMENU )
	{
		// Make first menu item the default (bold font)
		::SetMenuDefaultItem( pSubMenu->m_hMenu, 0, TRUE );

		// Display the menu at the current mouse location. There's a "bug"
		// (Microsoft calls it a feature) in Windows 95 that requires calling
		// SetForegroundWindow. To find out more, search for Q135788 in MSDN.
		//
		CPoint mouse;
		GetCursorPos( &mouse );
		::SetForegroundWindow( m_nid.hWnd );
		::TrackPopupMenu( pSubMenu->m_hMenu, 0, mouse.x, mouse.y, 0, m_nid.hWnd, NULL );

	} 
	else  
	{
		// double click: execute first menu item
		::SendMessage(m_nid.hWnd, WM_COMMAND, pSubMenu->GetMenuItemID(0), 0);
	}

	return 1; // handled
}


BOOL CTrayIcon::RefreshIcon()
{
    return SetIcon( m_nid.hIcon, m_nid.szTip );
}


BOOL CTrayIcon::ShowBalloon( LPCTSTR pszTitle, LPCTSTR pszText )
{
    return SetIcon( m_nid.hIcon, m_nid.szTip, pszTitle, pszText );
}
