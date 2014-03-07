/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#ifndef __INCLUDED_INIT_H__
#define __INCLUDED_INIT_H__

/*!
 * @class InitStatusMgr Manages STATUS.DAT
 */
class InitStatusMgr
{
public:
	/*!
	 * @function InitStatusMgr Constructor
	 */
	InitStatusMgr() {}
	~InitStatusMgr() {}
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

	void RefreshStatusCache() {
		this->Get(false, false);
	}
};

/*!
 * @class WInitApp  Main Application object for WWIV 5.0
 */
class WInitApp
{
private:
public:	
	WInitApp() {}
	virtual ~WInitApp() {}

	int main(int argc, char* argv[]);
	WLocalIO *localIO;

	InitStatusMgr* GetStatusManager() {
		return statusMgr;
	}

	/*!
	 * @var statusMgr pointer to the InitStatusMgr class.
	 */
	InitStatusMgr* statusMgr;

};
	

#endif