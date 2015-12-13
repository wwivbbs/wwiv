/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_XFER_H__
#define __INCLUDED_BBS_XFER_H__

#include <string>

void zap_ed_info();
void get_ed_info();
unsigned long bytes_to_k(unsigned long lBytes);
int  check_batch_queue(const char *file_name);
bool check_ul_event(int nDirectoryNum, uploadsrec * pUploadRecord);
bool okfn(const std::string& fileName);
void print_devices();
void get_arc_cmd(char *pszOutBuffer, const char *pszArcFileName, int cmd, const char *ofn);
int  list_arc_out(const char *file_name, const char *pszDirectory);
bool ratio_ok();
bool dcs();
void dliscan1(int nDirectoryNum);
void dliscan();
void add_extended_description(const char *file_name, const char *description);
void delete_extended_description(const char *file_name);
char *read_extended_description(const char *file_name);
void print_extended(const char *file_name, bool *abort, int numlist, int indent);
void align(char *file_name);
bool compare(const char *pszFileName1, const char *pszFileName2);
void printinfo(uploadsrec * pUploadRecord, bool *abort);
void printtitle(bool *abort);
void file_mask(char *pszFileMask);
void listfiles();
void nscandir(int nDirNum, bool *abort);
void nscanall();
void searchall();
int  recno(const char *pszFileMask);
int  nrecno(const char *pszFileMask, int nStartingRec);
int  printfileinfo(uploadsrec * pUploadRecord, int nDirectoryNum);
void remlist(const char *file_name);
int  FileAreaSetRecord(File &file, int nRecordNumber);

#endif  // __INCLUDED_BBS_XFER_H__