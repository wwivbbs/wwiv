/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#ifdef _MSC_VER
#pragma once
#endif

#ifndef __INCLUDED_BBS_H__
#define __INCLUDED_BBS_H__


#include "WComm.h"
#include "WSession.h"
#include "WFile.h"
#include "WLocalIO.h"


/*!
 * @header WWIV 5.0 Main Application
 * Main Starting point of the WWIV 5.0 System.
 */


/*!
 * @class StatusMgr Manages STATUS.DAT
 */
class StatusMgr
{
private:
    WFile m_statusFile;
public:
	/*!
	 * @function StatusMgr Constructor
	 */
	StatusMgr() { }
	~StatusMgr() {}
	/*!
	 * @function Read Loads the contents of STATUS.DAT
	 */
	void Read();
	/*!
	 * @function Write Writes the contents of STATUS.DAT
	 */
	void Write();
	/*!
	 * @function Lock Aquires write lock on STATUS.DAT
	 */
	void Lock();
	/*!
	 * @function Get Loads the contents of STATUS.DAT with
	 *           control on failure and lock mode
	 * @param bFailOnFailure Exit the BBS if reading the file fails
	 * @param bLockFile Aquires write lock
	 */
	void Get(bool bFailOnFailure, bool bLockFile);
};



/*!
 * @class WBbsApp  Main Application object for WWIV 5.0
 */
class WBbsApp : public WLogger
{
public:
    // Constants
    static const int exitLevelOK;
    static const int exitLevelNotOK;
    static const int exitLevelQuit;

    static const int shutdownNone;
    static const int shutdownThreeMinutes;
    static const int shutdownTwoMinutes;
    static const int shutdownOneMinute;
    static const int shutdownImmediate;

public:
    // former global variables and system_operation_rec members
    // to be moved
    unsigned long flags;
    unsigned short spawn_opts[20];


private:
	unsigned short  m_unx;
	/*! @var m_szCurrentDirectory The current directory where WWIV lives */
	char            m_szCurrentDirectory[ MAX_PATH ];
    int             m_nOkLevel;
    int             m_nErrorLevel;
    int             m_nInstance;
    char            m_szNetworkExtension[ 5 ];
    double          last_time;
	bool            m_bUserAlreadyOn;
	bool            m_bNeedToCleanNetwork;
    int             m_nBbsShutdownStatus;
    double          m_fShutDownTime;


    WComm* comm;
    StatusMgr* statusMgr;
    WLocalIO *localIO;
    WUserManager* userManager;


protected:

    /*!
	 * @function GetCaller WFC Screen loop
	 */
    void GetCaller();

	int doWFCEvents();

    /*!
	 * @function GotCaller login routines
	 * @param ms Modem Speed (may be a locked speed)
	 * @param cs Connect Speed (real speed)
	 */
	void GotCaller(unsigned int ms, unsigned long cs);

    /*!
	 * @function BBSmain main bbs loop - Invoked from the application
	 *           main method.
     * @param argc The number of arguments
	 * @param argv arguments
	 */
	int BBSmain(int argc, char *argv[]);

    /*!
	 * @function TelnetMainLoop - waits for telnet connection,
	 *           then calls BBSmain on connection
	 */
	void TelnetMainLoop();
	/*!
	 * @function ShowUsage - Shows the help screen to the user listing
	 *           all of the command line arguments for WWIV
	 */
    void ShowUsage();

public:
    WBbsApp();
    WBbsApp( const WBbsApp& copy );
    virtual ~WBbsApp();

    /*!
	 * @function Run - Main BBS loop.. (old main functon)
	 */
    int  Run(int argc, char *argv[]);

    WComm* GetComm();

    StatusMgr* GetStatusManager();

    WLocalIO* GetLocalIO(); 

    WUserManager* GetUserManager();

    /*!
	 * @var m_szEnvironVarWwivNetworkNumber Environment variable style
     *      listing of WWIV net number, (only used for the xenviron)
	 */
	char m_szEnvironVarWwivNetworkNumber[20];

    /*!
	 * @var m_szWWIVEnvironmentVariable Environment variable for the WWIV
     *      version (set as BBS env variable)
	 */
    char m_szWWIVEnvironmentVariable[ 255 ];

	/*!
	 * @function GetHomeDir Returns the current home directory
	 */
	const char* GetHomeDir();

	/*! @function CdHome Changes directories back to the WWIV Home directory */
	void CdHome();

	/*! @function StartupComm Start up the communications subsystem */
	bool StartupComm(bool bUseSockets);

	/*! @function ShutdownComm Shutdown the communications subsystem */
	bool ShutdownComm();

    /*! @function AbortBBS - Shuts down the bbs at the not-ok error level */
    void AbortBBS( bool bSkipShutdown = false );

    /*! @function ShutdownBBS - Shuts down the bbs at the ok error level */
    void ShutdownBBS();

    /*! @function ShutdownBBS - Shuts down the bbs at the "QUIT" error level */
    void QuitBBS();

    int  GetInstanceNumber() { return m_nInstance; }

    const char* GetNetworkExtension() { return m_szNetworkExtension; }

    // From WLogger
    virtual bool LogMessage( const char* pszFormat, ... );

	bool SaveConfig();

    void SetConfigFlag( int nFlag )         { flags |= nFlag; }
    void ToggleConfigFlag( int nFlag )      { flags ^= nFlag; }
    void ClearConfigFlag( int nFlag )       { flags &= ~nFlag; }
    bool HasConfigFlag( int nFlag ) const   { return ( flags & nFlag ) != 0; }
    void SetConfigFlags( int nFlags )       { flags = nFlags; }
    unsigned long GetConfigFlags() const    { return flags; }

    unsigned short GetSpawnOptions( int nCmdID )    { return spawn_opts[ nCmdID ]; }

	bool IsCleanNetNeeded() const			{ return m_bNeedToCleanNetwork; }
	void SetCleanNetNeeded( bool b )		{ m_bNeedToCleanNetwork = b; }

    bool IsShutDownActive() const           { return m_nBbsShutdownStatus > 0; }
    int  GetShutDownStatus() const          { return m_nBbsShutdownStatus; }
    void SetShutDownStatus( int n )         { m_nBbsShutdownStatus = n; }

    double GetShutDownTime() const          { return m_fShutDownTime; }
    void   SetShutDownTime( double d )      { m_fShutDownTime = d; }

    bool read_subs();

private:
    void ExitBBSImpl( int nExitLevel );

    void InitializeBBS(); // old init() method
    bool ReadINIFile(); // from xinit.cpp
    bool ReadConfig();

    int LocalLogon();

    unsigned short str2spawnopt( const char *s );
    unsigned short str2restrict( const char *s );
    unsigned char stryn2tf( const char *s );
    void read_nextern();
    void read_arcs();
    void read_editors();
    void read_nintern();
    void read_networks();
    bool read_names();
    void read_voting();
    bool read_dirs();
    void read_chains();
    bool read_language();
    bool read_modem();
    void read_gfile();
    bool make_abs_path(char *checkdir);
    void check_phonenum();
    void create_phone_file();

};

// Function Prototypes
WBbsApp* GetApplication();


class WSession;

WSession* GetSession();



#endif // __INCLUDED_BBS_H__


