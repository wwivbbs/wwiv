#ifndef __INCLUDED_WSTATUS_H__
#define __INCLUDED_WSTATUS_H__
/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2005, WWIV Software Services             */
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

extern statusrec status;

class WStatus
{
    friend class StatusMgr;
public:
    static const int fileChangeNames;
    static const int fileChangeUpload;    
    static const int fileChangePosts;
    static const int fileChangeEmail;
    static const int fileChangeNet;

private:
    statusrec* m_pStatusRecord;

public:
    WStatus(statusrec* pStatusRecord) { m_pStatusRecord = pStatusRecord; }
    WStatus() { m_pStatusRecord = &status; }
    ~WStatus() {};

    /** If the caller number variable is using the old (pre 4.2) location, move it to the new location */
    void EnsureCallerNumberIsValid();
    /** Checks for corruption in the date strings, and try to fix if the date strings are corrupt */
    void ValidateAndFixDates();
    
    /** Updates the status object for a new day.  This zeros out daily stats and updates the log file names */
    bool NewDay();

    const char* GetLastDate( int nDaysAgo = 0 ) const;
    const char* GetLogFileName( int nDaysAgo = 0 ) const;
    const char* GetGFileDate() const { return m_pStatusRecord->gfiledate; }
    void SetGFileDate( const char *s ) { strcpy(  m_pStatusRecord->gfiledate, s ); }
    const char  GetFileChangedFlag( int nFlag ) const { return m_pStatusRecord->filechange[ nFlag ]; }
    void IncrementFileChangedFlag(int nFlag ) { m_pStatusRecord->filechange[nFlag]++; }
    
    const int GetNumLocalPosts() const { return m_pStatusRecord->localposts; }
    const int  IncrementNumLocalPosts() { return m_pStatusRecord->localposts++; }
    void SetNumLocalPosts( int n ) { m_pStatusRecord->localposts = static_cast<unsigned short>(n); }

    const int GetNumUsers() const { return m_pStatusRecord->users; }
    const int IncrementNumUsers() { return m_pStatusRecord->users++; }
    const int DecrementNumUsers() { return m_pStatusRecord->users--; }
    void SetNumUsers( int n ) { m_pStatusRecord->users = static_cast<unsigned short>(n); }

    const unsigned long GetCallerNumber() const { return m_pStatusRecord->callernum1; }
    const unsigned long  IncrementCallerNumber() { return m_pStatusRecord->callernum1++; }
    void SetCallerNumber( unsigned long l ) { m_pStatusRecord->callernum1 = l; }

    const int GetNumCallsToday() const { return m_pStatusRecord->callstoday; }
    const int IncrementNumCallsToday() { return m_pStatusRecord->callstoday++; }
    void SetNumCallsToday( int n ) { m_pStatusRecord->callstoday = static_cast<unsigned short>(n); }

    const int GetNumMessagesPostedToday() const { return m_pStatusRecord->msgposttoday; }
    const int IncrementNumMessagesPostedToday() { return m_pStatusRecord->msgposttoday++; }
    void SetNumMessagesPostedToday( int n ) { m_pStatusRecord->msgposttoday = static_cast<unsigned short>(n); }

    const int GetNumEmailSentToday() const { return m_pStatusRecord->emailtoday; }
    const int IncrementNumEmailSentToday() { return m_pStatusRecord->emailtoday++; }
    void SetNumEmailSentToday( int n ) { m_pStatusRecord->emailtoday = static_cast<unsigned short>(n); }

    const int GetNumFeedbackSentToday() const { return m_pStatusRecord->fbacktoday; }
    const int IncrementNumFeedbackSentToday() { return m_pStatusRecord->fbacktoday++; }
    void SetNumFeedbackSentToday( int n ) { m_pStatusRecord->fbacktoday = static_cast<unsigned short>(n); }

    const int GetNumUploadsToday() const { return m_pStatusRecord->uptoday; }
    const int IncrementNumUploadsToday() { return m_pStatusRecord->uptoday++; }
    void SetNumUploadsToday( int n ) { m_pStatusRecord->uptoday = static_cast<unsigned short>(n); }

    const int GetMinutesActiveToday() const { return m_pStatusRecord->activetoday; }
    const int IncrementMinutesActiveToday(int nMinutes) { return m_pStatusRecord->uptoday += static_cast<unsigned short>(nMinutes); }
    void SetMinutesActiveToday( int n ) { m_pStatusRecord->activetoday = static_cast<unsigned short>(n); }

    const unsigned long GetQScanPointer() const { return m_pStatusRecord->qscanptr; }
    const unsigned long IncrementQScanPointer() { return m_pStatusRecord->qscanptr++; }
    void SetQScanPointer( unsigned long l) { m_pStatusRecord->qscanptr = l; }

    const bool IsAutoMessageAnonymous() const { return m_pStatusRecord->amsganon ? true : false; }
    void SetAutoMessageAnonymous(bool b) { m_pStatusRecord->amsganon = (b) ? 1 : 0; }

    const int GetAutoMessageAuthorUserNumber() const { return m_pStatusRecord->amsguser; }
    void SetAutoMessageAuthorUserNumber( int n ) { m_pStatusRecord->amsguser = static_cast<unsigned short>(n); }

    const bool IsUsingNetEdit() const { return m_pStatusRecord->net_edit_stuff ? true : false; }
    
    const int GetWWIVVersion() const { return m_pStatusRecord->wwiv_version; }
    void SetWWIVVersion( int n ) { m_pStatusRecord->wwiv_version = static_cast<unsigned short>(n); }

    const int GetNetworkVersion() const { return m_pStatusRecord->net_version; }
    void SetNetworkVersion( int n ) { m_pStatusRecord->net_version = static_cast<unsigned short>(n); }

    const int GetDays() const { return m_pStatusRecord->days; }
    void SetDays( int n ) { m_pStatusRecord->days = static_cast<unsigned short>(n); }
};



/*!
 * @class StatusMgr Manages STATUS.DAT
 */
class StatusMgr
{
private:
    WFile m_statusFile;
    bool Write(statusrec *pStatus);
	/*!
	 * @function Get Loads the contents of STATUS.DAT with
	 *           control on failure and lock mode
	 * @param bLockFile Aquires write lock
     * @return true on success
	 */
	bool Get(bool bLockFile);

public:
	/*!
	 * @function StatusMgr Constructor
	 */
	StatusMgr() { }
	~StatusMgr() {}
	/*!
	 * @function Read Loads the contents of STATUS.DAT
	 */
    bool RefreshStatusCache();

    /*!
	 * @function Write Writes the contents of STATUS.DAT
     * @deprecated - Use BeginTransaction and CommitTransaction 
	 */
	bool Write();
	/*!
	 * @function Lock Aquires write lock on STATUS.DAT
     * @deprecated - Use BeginTransaction and CommitTransaction 
	 */
	bool Lock();

    /**
     * Gets the status object with no locks.  Just delete it when finished
     */
    WStatus* GetStatus();

    /**
     * Replacement for Lock
     */
    WStatus* BeginTransaction();

    /**
     * Replacement for Write
     */
    bool CommitTransaction( WStatus* pStatus );

	const int GetUserCount();
};




#endif // __INCLUDED_WSTATUS_H__

