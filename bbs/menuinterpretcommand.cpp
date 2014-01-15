/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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

#include "wwiv.h"
#include "menu.h"
#include "menusupp.h"


void InterpretCommand( MenuInstanceData * pMenuData, const char *pszScript ) {
	char szCmd[31], szParam1[51], szParam2[51];
	char szTempScript[ 255 ];
	memset( szTempScript, 0, sizeof( szTempScript ) );
	strncpy( szTempScript, pszScript, 250 );

	if (pszScript[0] == 0) {
		return;
	}

	char* pszScriptPointer = szTempScript;
	while (pszScriptPointer && !hangup) {
		pszScriptPointer = MenuParseLine(pszScriptPointer, szCmd, szParam1, szParam2);

		if ( szCmd[0] == 0 ) {  // || !pszScriptPointer || !*pszScriptPointer
			break;
		}

		// -------------------------
		// Run a new menu instance

		int nCmdID = GetMenuIndex( szCmd );
		switch ( nCmdID ) {
		case 0: {
			// "MENU"
			// Spawn a new menu
			MenuInstanceData *pNewMenuData = static_cast<MenuInstanceData *>( bbsmalloc( sizeof( MenuInstanceData ) ) );

			memset(pNewMenuData, 0, sizeof(MenuInstanceData));
			pNewMenuData->nFinished = 0;
			pNewMenuData->nReload = 0;

			Menus(pNewMenuData, pMenuData->szPath, szParam1);
			BbsFreeMemory(pNewMenuData);
		}
		break;
		case 1: {
			// -------------------------
			// Exit out of this instance
			// of the menu
			// -------------------------
			// "ReturnFromMenu"
			InterpretCommand(pMenuData, pMenuData->header.szExitScript);
			pMenuData->nFinished = 1;
		}
		break;
		case 2: {
			// "EditMenuSet"
			EditMenus();           // flag if we are editing this menu
			pMenuData->nFinished = 1;
			pMenuData->nReload = 1;
		}
		break;
		case 3: {
			// "DLFreeFile"
			align(szParam2);
			MenuDownload(szParam1, szParam2, true, true);
		}
		break;
		case 4: {
			// "DLFile"
			align(szParam2);
			MenuDownload(szParam1, szParam2, false, true);
		}
		break;
		case 5: {
			// "RunDoor"
			MenuRunDoorName(szParam1, false);
		}
		break;
		case 6: {
			// "RunDoorFree"
			MenuRunDoorName(szParam1, true);
		}
		break;
		case 7: {
			// "RunDoorNumber"
			int nTemp = atoi(szParam1);
			MenuRunDoorNumber(nTemp, false);
		}
		break;
		case 8: {
			// "RunDoorNumberFree"
			int nTemp = atoi(szParam1);
			MenuRunDoorNumber(nTemp, true);
		}
		break;
		case 9: {
			// "PrintFile"
			printfile(szParam1, true);
		}
		break;
		case 10: {
			// "PrintFileNA"
			printfile(szParam1, false);
		}
		break;
		case 11: {
			// "SetSubNumber"
			SetSubNumber(szParam1);
		}
		break;
		case 12: {
			// "SetDirNumber"
			SetDirNumber(szParam1);
		}
		break;
		case 13: {
			// "SetMsgConf"
			SetMsgConf(szParam1[0]);
		}
		break;
		case 14: {
			// "SetDirConf"
			SetDirConf(szParam1[0]);
		}
		break;
		case 15: {
			// "EnableConf"
			EnableConf();
		}
		break;
		case 16: {
			// "DisableConf"
			DisableConf();
		}
		break;
		case 17: {
			// "Pause"
			pausescr();
		}
		break;
		case 18: {
			// "ConfigUserMenuSet"
			ConfigUserMenuSet();
			pMenuData->nFinished = 1;
			pMenuData->nReload = 1;
		}
		break;
		case 19: {
			// "DisplayHelp"
			if ( GetSession()->GetCurrentUser()->IsExpert() ) {
				AMDisplayHelp( pMenuData );
			}
		}
		break;
		case 20: {
			// "SelectSub"
			ChangeSubNumber();
		}
		break;
		case 21: {
			// "SelectDir"
			ChangeDirNumber();
		}
		break;
		case 22: {
			// "SubList"
			SubList();
		}
		break;
		case 23: {
			// "UpSubConf"
			UpSubConf();
		}
		break;
		case 24: {
			// "DownSubConf"
			DownSubConf();
		}
		break;
		case 25: {
			// "UpSub"
			UpSub();
		}
		break;
		case 26: {
			// "DownSub"
			DownSub();
		}
		break;
		case 27: {
			// "ValidateUser"
			ValidateUser();
		}
		break;
		case 28: {
			// "Doors"
			Chains();
		}
		break;
		case 29: {
			// "TimeBank"
			TimeBank();
		}
		break;
		case 30: {
			// "AutoMessage"
			AutoMessage();
		}
		break;
		case 31: {
			// "BBSList"
			BBSList();
		}
		break;
		case 32: {
			// "RequestChat"
			RequestChat();
		}
		break;
		case 33: {
			// "Defaults"
			Defaults(pMenuData);
		}
		break;
		case 34: {
			// "SendEMail"
			SendEMail();
		}
		break;
		case 35: {
			// "Feedback"
			FeedBack();
		}
		break;
		case 36: {
			// "Bulletins"
			Bulletins();
		}
		break;
		case 37: {
			// "HopSub"
			HopSub();
		}
		break;
		case 38: {
			// "SystemInfo"
			SystemInfo();
		}
		break;
		case 39: {
			// "JumpSubConf"
			JumpSubConf();
		}
		break;
		case 40: {
			// "KillEMail"
			KillEMail();
		}
		break;
		case 41: {
			// "LastCallers"
			LastCallers();
		}
		break;
		case 42: {
			// "ReadEMail"
			ReadEMail();
		}
		break;
		case 43: {
			// "NewMessageScan"
			NewMessageScan();
		}
		break;
		case 44: {
			// "Goodbye"
			GoodBye();
		}
		break;
		case 45: {
			// "PostMessage"
			WWIV_PostMessage();
		}
		break;
		case 46: {
			// "NewMsgScanCurSub"
			ScanSub();
		}
		break;
		case 47: {
			// "RemovePost"
			RemovePost();
		}
		break;
		case 48: {
			// "TitleScan"
			TitleScan();
		}
		break;
		case 49: {
			// "ListUsers"
			ListUsers();
		}
		break;
		case 50: {
			// "Vote"
			Vote();
		}
		break;
		case 51: {
			// "ToggleExpert"
			ToggleExpert();
		}
		break;
		case 52: {
			// "YourInfo"
			YourInfo();
		}
		break;
		case 53: {
			// "ExpressScan"
			ExpressScan();
		}
		break;
		case 54: {
			// "WWIVVer"
			WWIVVersion();
		}
		break;
		case 55: {
			// "InstanceEdit"
			InstanceEdit();
		}
		break;
		case 56: {
			// "ConferenceEdit"
			JumpEdit();
		}
		break;
		case 57: {
			// "SubEdit"
			BoardEdit();
		}
		break;
		case 58: {
			// "ChainEdit"
			ChainEdit();
		}
		break;
		case 59: {
			// "ToggleAvailable"
			ToggleChat();
		}
		break;
		case 60: {
			// "ChangeUser"
			ChangeUser();
		}
		break;
		case 61: {
			// "CLOUT"
			CallOut();
		}
		break;
		case 62: {
			// "Debug"
			Debug();
		}
		break;
		case 63: {
			// "DirEdit"
			DirEdit();
		}
		break;
		case 65: {
			// "Edit"
			EditText();
		}
		break;
		case 66: {
			// "BulletinEdit"
			EditBulletins();
		}
		break;
		case 67: {
			// "LoadText"
			// LoadText and LoadTextFile are the same, so they are now merged.
			LoadTextFile();
		}
		break;
		case 68: {
			// "ReadAllMail"
			ReadAllMail();
		}
		break;
		case 69: {
			// "Reboot"
			RebootComputer();
		}
		break;
		case 70: {
			// "ReloadMenus"
			ReloadMenus();
		}
		break;
		case 71: {
			// "ResetUserIndex"
			ResetFiles();
		}
		break;
		case 72: {
			// "ResetQScan"
			ResetQscan();
		}
		break;
		case 73: {
			// "MemStat"
			MemoryStatus();
		}
		break;
		case 74: {
			// "PackMsgs"
			PackMessages();
		}
		break;
		case 75: {
			// "VoteEdit"
			InitVotes();
		}
		break;
		case 76: {
			// "Log"
			ReadLog();
		}
		break;
		case 77: {
			// "NetLog"
			ReadNetLog();
		}
		break;
		case 78: {
			// "Pending"
			PrintPending();
		}
		break;
		case 79: {
			// "Status"
			PrintStatus();
		}
		break;
		case 80: {
			// "TextEdit"
			TextEdit();
		}
		break;
		case 81: {
			// "UserEdit"
			UserEdit();
		}
		break;
		case 82: {
			// "VotePrint"
			VotePrint();
		}
		break;
		case 83: {
			// "YLog"
			YesturdaysLog();
		}
		break;
		case 84: {
			// "ZLog"
			ZLog();
		}
		break;
		case 85: {
			// "ViewNetDataLog"
			ViewNetDataLog();
		}
		break;
		case 86: {
			// "UploadPost"
			UploadPost();
		}
		break;
		case 87: {
			// "ClearScreen"
			GetSession()->bout.ClearScreen();
		}
		break;
		case 88: {
			// "NetListing"
			NetListing();
		}
		break;
		case 89: {
			// "WHO"
			WhoIsOnline();
		}
		break;
		case 90: {
			// /A "NewMsgsAllConfs"
			NewMsgsAllConfs();
		}
		break;
		case 91: {
			// /E "MultiEMail"
			MultiEmail();
		}
		break;
		case 92: {
			// "NewMsgScanFromHere"
			NewMsgScanFromHere();
		}
		break;
		case 93: {
			// "ValidatePosts"
			ValidateScan();
		}
		break;
		case 94: {
			// "ChatRoom"
			ChatRoom();
		}
		break;
		case 95: {
			// "DownloadPosts"
			DownloadPosts();
		}
		break;
		case 96: {
			// "DownloadFileList"
			DownloadFileList();
		}
		break;
		case 97: {
			// "ClearQScan"
			ClearQScan();
		}
		break;
		case 98: {
			// "FastGoodBye"
			FastGoodBye();
		}
		break;
		case 99: {
			// "NewFilesAllConfs"
			NewFilesAllConfs();
		}
		break;
		case 100: {
			// "ReadIDZ"
			ReadIDZ();
		}
		break;
		case 101: {
			// "UploadAllDirs"
			UploadAllDirs();
		}
		break;
		case 102: {
			// "UploadCurDir"
			UploadCurDir();
		}
		break;
		case 103: {
			// "RenameFiles"
			RenameFiles();
		}
		break;
		case 104: {
			// "MoveFiles"
			MoveFiles();
		}
		break;
		case 105: {
			// "SortDirs"
			SortDirs();
		}
		break;
		case 106: {
			// "ReverseSortDirs"
			ReverseSort();
		}
		break;
		case 107: {
			// "AllowEdit"
			AllowEdit();
		}
		break;
		case 109: {
			// "UploadFilesBBS"
			UploadFilesBBS();
		}
		break;
		case 110: {
			// "DirList"
			DirList();
		}
		break;
		case 111: {
			// "UpDirConf"
			UpDirConf();
		}
		break;
		case 112: {
			// "UpDir"
			UpDir();
		}
		break;
		case 113: {
			// "DownDirConf"
			DownDirConf();
		}
		break;
		case 114: {
			// "DownDir"
			DownDir();
		}
		break;
		case 115: {
			// "ListUsersDL"
			ListUsersDL();
		}
		break;
		case 116: {
			// "PrintDSZLog"
			PrintDSZLog();
		}
		break;
		case 117: {
			// "PrintDevices"
			PrintDevices();
		}
		break;
		case 118: {
			// "ViewArchive"
			ViewArchive();
		}
		break;
		case 119: {
			// "BatchMenu"
			BatchMenu();
		}
		break;
		case 120: {
			// "Download"
			Download();
		}
		break;
		case 121: {
			// "TempExtract"
			TempExtract();
		}
		break;
		case 122: {
			// "FindDescription"
			FindDescription();
		}
		break;
		case 123: {
			// "ArchiveMenu"
			TemporaryStuff();
		}
		break;
		case 124: {
			// "HopDir"
			HopDir();
		}
		break;
		case 125: {
			// "JumpDirConf"
			JumpDirConf();
		}
		break;
		case 126: {
			// "ListFiles"
			ListFiles();
		}
		break;
		case 127: {
			// "NewFileScan"
			NewFileScan();
		}
		break;
		case 128: {
			// "SetNewFileScanDate"
			SetNewFileScanDate();
		}
		break;
		case 129: {
			// "RemoveFiles"
			RemoveFiles();
		}
		break;
		case 130: {
			// "SearchAllFiles"
			SearchAllFiles();
		}
		break;
		case 131: {
			// "XferDefaults"
			XferDefaults();
		}
		break;
		case 132: {
			// "Upload"
			Upload();
		}
		break;
		case 133: {
			// "YourInfoDL"
			YourInfoDL();
		}
		break;
		case 134: {
			// "UploadToSysop"
			UploadToSysop();
		}
		break;
		case 135: {
			// "ReadAutoMessage"
			ReadAutoMessage();
		}
		break;
		case 136: {
			// "SetNewScanMsg"
			SetNewScanMsg();
		}
		break;
		case 137: {
			// "ReadMessages"
			ReadMessages();
		}
		break;
		/*
		case 138:
		{ // "RUN"
		ExecuteBasic(szParam1);
		} break;
		*/
		case 139: {
			// "EventEdit"
			EventEdit();
		}
		break;
		case 140: {
			// "LoadTextFile"
			LoadTextFile();
		}
		break;
		case 141: {
			// "GuestApply"
			GuestApply();
		}
		break;
		case 142: {
			// "ConfigFileList"
			ConfigFileList();
		}
		break;
		case 143: {
			// "ListAllColors"
			ListAllColors();
		}
		break;
#ifdef QUESTIONS
		case 144: {
			// "EditQuestions"
			EditQuestions();
		}
		break;
		case 145: {
			// "Questions"
			Questions();
		}
		break;
#endif
		case 146: {
			// "RemoveNotThere"
			RemoveNotThere();
		}
		break;
		case 147: {
			// "AttachFile"
			AttachFile();
		}
		break;
		case 148: {
			// "InternetEmail"
			InternetEmail();
		}
		break;
		case 149: {
			// "UnQScan"
			UnQScan();
		}
		break;
		// ppMenuStringsIndex[150] thru ppMenuStringsIndex[153] not used.....
		case 154: {
			// "Packers"
			Packers();
		}
		break;
		case 155: {
			// Color_Config
			color_config();
		}
		break;
		//------------------------------------------------------------------
		//  ppMenuStringsIndex[156] and [157] are reserved for SDS Systems and systems
		//  that distribute modifications.  DO NOT reuse these strings for
		//  other menu options.
		//------------------------------------------------------------------
		//    case 156:
		//    { // ModAccess
		//        ModsAccess();
		//    } break;
		//    case 157:
		//    { // SDSAccess
		//        SDSAccess();
		//      } break;
		//------------------------------------------------------------------
		case 158: {
			// InitVotes
			InitVotes();
		}
		break;
		case 161: {
			// TurnMCIOn
			TurnMCIOn();
		}
		break;
		case 162: {
			// TurnMCIOff
			TurnMCIOff();
		}
		break;
		default: {
			MenuSysopLog("The following command was not recognized");
			MenuSysopLog(szCmd);
		}
		break;
		}
	}
}


