/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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
/**************************************************************************/

#ifndef __INCLUDED_MENUSUPP_H__
#define __INCLUDED_MENUSUPP_H__

#ifdef _MSC_VER
#pragma once
#endif

#include "menu.h" // Only needed by Defaults

void UnQScan();
void DirList();
void UpSubConf();
void DownSubConf();
void DownSub();
void UpSub();
void ValidateUser();
void Chains();
void TimeBank();
void AutoMessage();
void Defaults(MenuInstanceData * MenuData);
void SendEMail();
void FeedBack();
void Bulletins();
void SystemInfo();
void JumpSubConf();
void KillEMail();
void LastCallers();
void ReadEMail();
void NewMessageScan();
void GoodBye();
void WWIV_PostMessage();
void ScanSub();
void RemovePost();
void TitleScan();
void ListUsers();
void Vote();
void ToggleExpert();
void ExpressScan();
void WWIVVersion();
void InstanceEdit();
void JumpEdit();
void BoardEdit();
void ChainEdit();
void ToggleChat();
void ChangeUser();
void CallOut();
void Debug();
void DirEdit();
void EventEdit();
void LoadTextFile();
void EditText();
void EditBulletins();
void ReadAllMail();
void RebootComputer();
void ReloadMenus();
void ResetFiles();
void ResetQscan();
void MemoryStatus();
void PackMessages();
void InitVotes();
void ReadLog();
void ReadNetLog();
void PrintPending();
void PrintStatus();
void TextEdit();
void UserEdit();
void VotePrint();
void YesturdaysLog();
void ZLog();
void ViewNetDataLog();
void UploadPost();
void NetListing();
void WhoIsOnline();
void NewMsgsAllConfs();
void MultiEmail();
void InternetEmail();
void NewMsgScanFromHere();
void ValidateScan();
void ChatRoom();
void DownloadPosts();
void DownloadFileList();
void ClearQScan();
void FastGoodBye();
void NewFilesAllConfs();
void ReadIDZ();
void RemoveNotThere();
void UploadAllDirs();
void UploadCurDir();
void RenameFiles();
void MoveFiles();
void SortDirs();
void ReverseSort();
void AllowEdit();
void UploadFilesBBS();
void UpDirConf();
void UpDir();
void DownDirConf();
void DownDir();
void ListUsersDL();
void PrintDSZLog();
void PrintDevices();
void ViewArchive();
void BatchMenu();
void Download();
void TempExtract();
void FindDescription();
void TemporaryStuff();
void JumpDirConf();
void ConfigFileList();
void ListFiles();
void NewFileScan();
void RemoveFiles();
void SearchAllFiles();
void XferDefaults();
void Upload();
void YourInfoDL();
void UploadToSysop();
void ReadAutoMessage();
void GuestApply();
void AttachFile();
bool GuestCheck();
void SetSubNumber(char *pszSubKeys);
void SetDirNumber(char *pszDirectoryKeys);

#endif // __INCLUDED_MENUSUPP_H__
