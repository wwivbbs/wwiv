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

#ifndef __INCLUDED_FCNS_H__
#define __INCLUDED_FCNS_H__

#ifdef _MSC_VER
#pragma once
#endif

#include "menu.h"
#include "instmsg.h"

// File: asv.cpp

void asv();
void set_autoval(int n);


// File: attach.cpp

void attach_file(int mode);


// File: automsg.cpp

void read_automessage();
void do_automessage();


// File: batch.cpp

void upload(int dn);
void delbatch(int nBatchEntryNum);
char *unalign(char *pszFileName);
void dszbatchdl(bool bHangupAfterDl, char *pszCommandLine, char *pszDescription);
int	 batchdl(int mode);
void didnt_upload(int nBatchIndex);
void ymbatchdl(bool bHangupAfterDl);
void zmbatchdl(bool bHangupAfterDl);


// File: bbslist.cpp

void BBSList();


// File: bbsovl1.cpp

void DisplayHorizontalBar( int nSize, int nColor );
void YourInfo();
int  GetMaxMessageLinesAllowed();
void upload_post();
void send_email();
void edit_confs();
void feedback( bool bNewUserFeedback );
void text_edit();


// File: bbsovl2.cpp

void OnlineUserEditor();
void BackPrint(const char *pszText, int nColorCode, int nCharDelay, int nStringDelay );
void MoveLeft( int nNumberOfChars );
void ClearEOL();
void SpinPuts( const char *pszText, int nColorCode );


// File: bbsovl3.cpp

int  get_kb_event( int nNumLockMode );
char onek_ncr( const char *pszAllowableChars );
bool do_sysop_command(int command);
bool copyfile(const char *pszSourceFileName, const char *pszDestFileName, bool stats);
bool movefile(const char *pszSourceFileName, const char *pszDestFileName, bool stats);
void ListAllColors();
//int  get_kb_event1();


// File: bbsutl.cpp

void copy_line(char *pszOutLine, char *pszWholeBuffer, long *plBufferPtr, long lBufferLength);
bool inli( char *pszBuffer, char *pszRollover, int nMaxLen, bool bAddCRLF, bool bAllowPrevious, bool bTwoColorChatMode = false );
bool so();
bool cs();
bool lcs();
void checka(bool *abort, bool *next);
void pla(const char *pszText, bool *abort);
void plal(const char *pszText, int limit, bool *abort);
bool sysop2();
bool checkcomp(const char *pszComputerType);
int  check_ansi();
bool set_language_1(int n);
bool set_language(int n);
char *mmkey( int dl, bool bListOption = false );
const char *YesNoString( bool bYesNo );


// File: bbsutl1.cpp

bool AllowLocalSysop();
void parse_email_info( const char *pszEmailAddress, int *pUserNumber, int *pSystemNumber );
bool ValidateSysopPassword();
void hang_it_up();
bool play_sdf( const char *pszSoundFileName, bool abortable );
void describe_area_code( int nAreaCode, char *pszDescription );
void describe_town( int nAreaCode, int town, char *pszDescription );


// File: bbsutl2.cpp

void ClearScreen();
void repeat_char( char x, int amount, int nColor, bool bAddNL );
const char *ctypes(int num);
void osan( const char *pszText, bool *abort, bool *next );
void plan( int nWWIVColor, const char *pszText, bool *abort, bool *next );
char *strip_to_node( const char *txt, char *pszOutBuffer );


// File: bgetch.cpp

char bgetch();


// File: bputch.cpp

int  bputch( char c, bool bUseInternalBuffer = false );


// File: callback.cpp

void wwivnode( WUser *pUser, int mode );
int  callback();
void dial(char *phone, int xlate);
//bool connect_to1(char *phone, int dy);


// File: chains.cpp

void run_chain(int nChainNumber);
void do_chains();


// File: chat.cpp

void toggle_avail();
void toggle_invis();
void chat_room();

// File: chnedit.cpp

void chainedit();


// File: colors.cpp

void get_colordata();
void save_colordata();
void list_ext_colors();
void color_config();
void buildcolorfile();


// File: com.cpp
void RestoreCurrentLine(const char *cl, const char *atr, const char *xl, const char *cc);
void rputch(char ch, bool bUseInternalBuffer = false);
char rpeek_wfconly();
char bgetchraw();
bool bkbhitraw();
void dump();
bool CheckForHangup();
void makeansi( int attr, char *pszOutBuffer, bool forceit);
void nl(int nNumLines = 1);
void BackSpace();
void setc( int nColor );
void resetnsp();
bool bkbhit();
void mpl( int nNumberOfChars );
char getkey();
bool yesno();
bool noyes();
char ynq();
void ansic(int wwivColor);
char onek( const char *pszAllowableChars, bool bAutoMpl = false );
void reset_colors();
void goxy(int x, int y);
char onek1(const char *pszAllowableChars);
void DisplayLiteBar(const char *fmt,...);
void BackLine();
void FlushOutComChBuffer();
int  bputs(const char *pszText );
int  bprintf(const char *fmt,...);
namespace wwiv
{
    template<class charT, class traits>
    std::basic_ostream<charT, traits>&
    endl (std::basic_ostream<charT, traits>& strm )
    {
        strm.write( "\r\n", 2 );
        return strm;
    }
}


// File: conf.cpp

int  get_conf_info( int conftype, int *num, confrec ** cpp, char *pszFileName, int *num_s, userconfrec ** uc );
void jump_conf( int conftype );
void update_conf( int conftype, SUBCONF_TYPE * sub1, SUBCONF_TYPE * sub2, int action );
int str_to_arword(const char *arstr);
char *word_to_arstr(int ar);
char first_available_designator(int conftype);
int  in_conference(int subnum, confrec * c);
void save_confs(int conftype, int whichnum, confrec * c);
void showsubconfs(int conftype, confrec * c);
SUBCONF_TYPE *str_to_numrange(const char *pszNumbersText, int *numinlist);
void addsubconf(int conftype, confrec * c, SUBCONF_TYPE * which);
void delsubconf(int conftype, confrec * c, SUBCONF_TYPE * which);
int  modify_conf(int conftype, int which);
void insert_conf(int conftype, int n);
void delete_conf(int conftype, int n);
void conf_edit(int conftype);
void list_confs(int conftype, int ssc);
int  select_conf(const char *pszPromptText, int conftype, int listconfs);
int  create_conf_file(int conftype);
confrec *read_conferences(const char *pszFileName, int *nc, int max);
void read_in_conferences(int conftype);
void read_all_conferences();
int get_num_conferences(const char *pszFileName);
int wordcount(const char *instr, const char *delimstr);
char *extractword(int ww,  const char *instr, char *delimstr);
void sort_conf_str( char *pszConferenceStr );


// File: confutil.cpp

void setuconf(int nConferenceType, int num, int nOldSubNumber);
void changedsl();


// File: connect1.cpp

void zap_call_out_list();
void read_call_out_list();
void zap_bbs_list();
void read_bbs_list_index();
bool valid_system( int ts );
net_system_list_rec *next_system(  int ts );
void zap_contacts();
void read_contacts();
void set_net_num( int nNetworkNumber );


// File: crc.cpp

unsigned long int crc32buf(const char *buf, size_t len);


// File: datetime.cpp

char *dateFromTimeTForLog(time_t t);
char *dateFromTimeT(time_t t);
char *date();
char *fulldate();
char *times();
time_t date_to_daten(const char *datet);
char *filedate(const char *fpath, char *rtn);
double timer();
long timer1();
bool isleap( int nYear );
int dow();
char *ctim(double d);
char *ctim2(double d, char *ch2);
int years_old( int nMonth, int nDay, int nYear );


// File: defaults.cpp

void select_editor();
void print_cur_stat();
const char *DescribeColorCode( int nColorCode );
void color_list();
void change_colors();
void l_config_qscan();
void config_qscan();
void make_macros();
void list_macro(const char *pszMacroText);
char *macroedit(char *pszMacroText);
void change_password();
void modify_mailbox();
void optional_lines();
void enter_regnum();
void defaults(MenuInstanceData * MenuData);
void list_config_scan_plus(int first, int *amount, int type);
void config_scan_plus(int type);
void drawscan(int filepos, long tagged);
void undrawscan(int filepos, long tagged);
long is_inscan(int dir);


// File: diredit.cpp

void dlboardedit();


// File: dirlist.cpp

void dirlist(int mode);


// File: dropfile.cpp

void create_filename( int nDropFileType, char *pszOutputFileName );
char *create_chain_file();


// File: dupphone.cpp

void add_phone_number(int usernum, const char *phone);
void delete_phone_number(int usernum, const char *phone);
int  find_phone_number(const char *phone);


// File: events.cpp

int  t_now();
char *ttc(int d);
void sort_events();
void init_events();
void get_next_forced_event();
void cleanup_events();
void check_event();
void run_event(int evnt);
void show_events();
void select_event_days(int evnt);
void modify_event(int evnt);
void insert_event();
void delete_event(int n);
void eventedit();


// File: execexternal.cpp

int  ExecuteExternalProgram( const char *pszCommandLine, int nFlags );


// File: extract.cpp

void extract_mod(const char *b, long len, time_t tDateTime);
void extract_out(char *b, long len, const char *pszTitle, time_t tDateTime);
bool upload_mod(int nDirectoryNumber, const char *pszFileName, const char *pszDescription);


// File: finduser.cpp

int  finduser( char *pszSearchString );
int  finduser1( const char *pszSearchString );


// File: gfiles.cpp

char *get_file(const char *pszFileName, long *len);
gfilerec *read_sec(int sn, int *nf);
void gfiles();


// File: gfledit.cpp

void modify_sec(int n);
void gfileedit();
bool fill_sec(int sn);
void pack_all_subs( bool bFromCommandline );
void pack_sub(int si);


// File: hop.cpp

void HopSub();
void HopDir();


// File: ini.cpp

void ini_done();
bool ini_init(const char *pszFileName, const char *prim, const char *sec);
char *ini_get(const char *key, int index = -1, char *index1 = NULL);


// File: inmsg.cpp

void inmsg( messagerec * pMessageRecord, char *pszTitle, int *anony, bool needtitle, const char *aux, int fsed, const char *pszDestination, int flags, bool force_title = false );


// File: input.cpp

void input1( char *pszOutText, int maxlen, int lc, bool crend, bool bAutoMpl = false );
void input1( std::string &strOutText, int maxlen, int lc, bool crend, bool bAutoMpl = false );
void input( char *pszOutText, int len, bool bAutoMpl = false );
void input( std::string &strOutText, int len, bool bAutoMpl = false );
void inputl( char *pszOutText, int len, bool bAutoMpl = false);
void inputl( std::string &strOutText, int len, bool bAutoMpl = false);
int  Input1( char *pszOutText, const char *pszOrigText, int maxlen, bool bInsert, int mode );
int  Input1( std::string &strOutText, const char *pszOrigText, int maxlen, bool bInsert, int mode );
void input_password( const char *pszPromptText, char *pszOutPassword, int len );
void input_password( const char *pszPromptText, std::string &strOutPassword, int len );

// File: inetmsg.cpp

void get_user_ppp_addr();
void send_inet_email();
bool check_inet_addr(const char *inetaddr);
char *read_inet_addr(char *pszInternetEmailAddress, int nUserNumber);
void write_inet_addr(const char *pszInternetEmailAddress, int nUserNumber);


// File: instmsg.cpp

void send_inst_msg( inst_msg_header * ih, const char *msg );
void send_inst_str1( int m, int whichinst, const char *pszSendString );
void send_inst_str( int whichinst, const char *pszSendString );
void send_inst_sysstr( int whichinst, const char *pszSendString );
void send_inst_shutdown( int whichinst );
void send_inst_cleannet();
void broadcast( const char *fmt, ... );
int  handle_inst_msg( inst_msg_header * ih, const char *msg );
void process_inst_msgs();
bool get_inst_info(int nInstanceNum, instancerec * ir);
bool inst_available(instancerec * ir);
bool inst_available_chat(instancerec * ir);
int  num_instances();
bool  user_online(int nUserNumber, int *wi);
void instance_edit();
void write_inst( int loc, int subloc, int flags );
bool inst_msg_waiting();
int  setiia(int poll_ticks);


// File: interpret.cpp

char *interpret( char chKey );


// File: lilo.cpp

bool random_screen(const char *mpfn);
bool IsPhoneNumberUSAFormat( WUser *pUser );
void getuser();
void logon();
void logoff();
void logon_guest();


// File: listplus.cpp

void colorize_foundtext(char *text, struct search_record * search_rec, int color);
void printtitle_plus_old();
void printtitle_plus();
void build_header();
int  first_file_pos();
int  lp_configured_lines();
void print_searching(struct search_record * search_rec);
void catch_divide_by_zero(int x);
int  listfiles_plus(int type);
void drawfile(int filepos, int filenum);
void undrawfile(int filepos, int filenum);
int  lp_add_batch(const char *pszFileName, int dn, long fs);
int  printinfo_plus(uploadsrec *pUploadRecord, int filenum, int marked, int LinesLeft, struct search_record * search_rec);
void CheckLPColors();
int  load_config_listing(int config);
void write_config_listing(int config);
void unload_config_listing();
int  print_extended_plus( const char *pszFileName, int numlist, int indent, int color, struct search_record * search_rec );
void show_fileinfo(uploadsrec *pUploadRecord );
int  check_lines_needed(uploadsrec * pUploadRecord);
char *lp_read_extended_description(const char *pszFileName);
void lp_zap_ed_info();
void lp_get_ed_info();
void prep_menu_items(char **menu_items);
int  prep_search_rec(struct search_record * search_rec, int type);
int  calc_max_lines();
void sysop_configure();
short SelectColor(int which);
void check_listplus();
void config_file_list();
void update_user_config_screen(uploadsrec * pUploadRecord, int which);
int  rename_filename(const char *pszFileName, int dn);
int  remove_filename(const char *pszFileName, int dn);
int  move_filename(const char *pszFileName, int dn);
void do_batch_sysop_command(int mode, const char *pszFileName);
void load_lp_config();
void save_lp_config();
int  search_criteria(struct search_record * sr);
void load_listing();
void view_file(const char *pszFileName);
int  lp_try_to_download(const char *pszFileMask, int dn);
void download_plus(const char *pszFileName);
void request_file(const char *pszFileName);
bool ok_listplus();
void check_lp_colors();


// File: lpfunc.cpp

int  listfiles_plus_function( int type );


// File: memory.cpp

void *BbsAllocA( size_t lNumBytes);
void *BbsAllocWithComment( size_t lNumBytes, char *pszComment);
char **BbsAlloc2D( int nRow, int nCol, int nSize );
void BbsFree2D( char **pa );


// File: menu.cpp

void ReadMenuSetup();
void mainmenu();
void StartMenus();
void Menus(MenuInstanceData * pMenuData, const char *pszDir, const char *pszMenu);
void CloseMenu(MenuInstanceData * pMenuData);
bool OpenMenu(MenuInstanceData * pMenuData);
bool CheckMenuSecurity(MenuHeader * pHeader, bool bCheckPassword );
bool LoadMenuRecord(MenuInstanceData * pMenuData, const char *pszCommand, MenuRec * pMenu);
void MenuExecuteCommand(MenuInstanceData * pMenuData, const char *pszCommand);
void LogUserFunction(MenuInstanceData * pMenuData, const char *pszCommand, MenuRec * pMenu);
void MenuSysopLog(const char *pszMsg);
void PrintMenuPrompt(MenuInstanceData * pMenuData);
void AMDisplayHelp(MenuInstanceData * pMenuData);
void TurnMCIOff();
void TurnMCIOn();
bool AMIsNumber(const char *pszBuf);
void ConfigUserMenuSet();
void QueryMenuSet();
bool ValidateMenuSet( char *pszMenuDir, bool bSetIt );
void WriteMenuSetup(int nUserNum);
void UnloadMenuSetup();
void GetCommand(MenuInstanceData * pMenuData, char *pszBuf);
bool CheckMenuItemSecurity(MenuInstanceData * pMenuData, MenuRec * pMenu, bool bCheckPassword );
void OpenMenuDescriptions();
void CloseMenuDescriptions();
char *GetMenuDescription( const char *pszName, char *pszDesc );
void SetMenuDescription(const char *pszName, const char *pszDesc);
char *MenuDir(char *pszDir);
void GenerateMenu(MenuInstanceData * pMenuData);
char *MenuParseLine(char *pszSrc, char *pszCmd, char *pszParam1, char *pszParam2);
char *MenuDoParenCheck(char *pszSrc, int bMore, char *porig);
char *MenuGetParam(char *pszSrc, char *pszParam);
char *MenuSkipSpaces(char *pszSrc);
void InterpretCommand(MenuInstanceData * pMenuData, const char *pszScript);


// File: menuedit.cpp

void EditMenus();
void ReIndexMenu( WFile &fileEditMenu, const char *pszDirectoryName, const char *pszMenuName );
void ReadMenuRec(WFile &fileEditMenu, MenuRec * Menu, int nCur);
void WriteMenuRec(WFile &fileEditMenu, MenuRec * Menu, int nCur);
bool GetMenuDir(char *pszBuffer);
bool GetMenuMenu(const char *pszDirectoryName, char *pszBuffer);
void DisplayItem(MenuRec * Menu, int nCur, int nAmount);
void DisplayHeader(MenuHeader * Header, int nCur, int nAmount, const char *pszDirectoryName );
void EditPulldownColors(MenuHeader * H);
void ListMenuDirs();
void ListMenuMenus(const char *pszDirectoryName );


// File: menuspec.cpp

int  MenuDownload( char *pszDirFName, char *pszFName, bool bFreeDL, bool bTitle );
int  FindDN(char *pszFName);
bool MenuRunDoorName(char *pszDoor, int bFree);
bool MenuRunDoorNumber(int nDoorNumber, int bFree);
int  FindDoorNo(char *pszDoor);
bool ValidateDoorAccess(int nDoorNumber);
void ChangeSubNumber();
void ChangeDirNumber();
void SetMsgConf(int iConf);
void SetDirConf(int iConf);
void EnableConf();
void DisableConf();
void SetNewScanMsg();
void ReadMessages();
void ReadSelectedMessages(int iWhich, int iWhere);


// File: menusupp.cpp

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


// File: misccmd.cpp

void kill_old_email();
void list_users(int mode);
void time_bank();
int  getnetnum( const char *pszNetworkName );
void uudecode( const char *pszInputFileName, const char *pszOutputFileName );
void Packers();


// File: modem.cpp

void rputs(const char *pszText);
void get_modem_line(char *pszLine, double d, bool allowa);
void do_result(result_info * ri);
void process_full_result( char *pszResultCode );
int  mode_switch(double d, bool allowa);
void holdphone(bool bPickUpPhone);
void imodem(bool bSetup);
void answer_phone();


// File: msgbase.cpp

void remove_link(messagerec * pMessageRecord, const char *aux);
void savefile( char *b, long lMessageLength, messagerec * pMessageRecord, const char *aux );
char *readfile(messagerec * pMessageRecord, const char *aux, long *plMessageLength);
void LoadFileIntoWorkspace( const char *pszFileName, bool bNoEditAllowed );
bool ForwardMessage( int *pUserNumber, int *pSystemNumber );
WFile *OpenEmailFile( bool bAllowWrite );
void sendout_email(const char *title, messagerec * msg, int anony, int nUserNumber, int nSystemNumber, int an, int nFromUser, int nFromSystem, int nForwardedCode, int nFromNetworkNumber );
bool ok_to_mail( int nUserNumber, int nSystemNumber, bool bForceit );
void email( int nUserNumber, int nSystemNumber, bool forceit, int anony, bool force_title = false, bool bAllowFSED = true );
void imail( int nUserNumber, int nSystemNumber );
void read_message1(messagerec * pMessageRecord, char an, bool readit, bool *next, const char *pszFileName, int nFromSystem, int nFromUser );
void read_message(int n, bool *next, int *val);
void lineadd(messagerec * pMessageRecord, const char *sx, const char *aux);


// File: msgbase1.cpp

void send_net_post( postrec * pPostRecord, const char *extra, int nSubNumber );
void post();
void grab_user_name(messagerec * pMessageRecord, const char *pszFileName);
void scan(int nMessageNumber, int nScanOptionType, int *nextsub, bool bTitleScan);
void qscan( int nBeginSubNumber, int *pnNextSubNumber );
void nscan( int nStartingSubNum = 0 );
void ScanMessageTitles();
void delmail(WFile *pFile, int loc);
void remove_post();
bool external_edit( const char *pszEditFileName, const char *pszNewDirectory, int ednum, int numlines, const char *pszDestination, const char *pszTitle, int flags );


// File: multinst.cpp

void make_inst_str( int nInstanceNum, char *pszOutInstanceString, int nInstanceFormat );
void multi_instance();
int  inst_ok( int loc, int subloc );


// File: multmail.cpp

void multimail(int *nUserNumber, int numu);
void slash_e();


// File: netsup.cpp

void rename_pend( const char *pszDirectory, const char *pszFileName );
int  checkup2( const time_t tFileTime, const char *pszFileName );
int  check_bbsdata();
void cleanup_net();
int  cleanup_net1();
void do_callout(int sn);
bool ok_to_call(int i);
void free_vars(float **weight, int **try1);
void attempt_callout();
void print_pending_list();
void gate_msg( net_header_rec * nh, char *pszMessageText, int nNetNumber, const char *pszAuthorName, unsigned short int *pList, int nFromNetworkNumber );
void print_call(int sn, int nn, int i2);
void fill_call(int color, int row, int netmax, int *nodenum);
int  ansicallout();
void force_callout(int dw);
long *next_system_reg( int ts );
void run_exp();


// File: newuser.cpp

void input_phone();
void input_dataphone();
void input_language();
bool check_name(char *nn);
void input_name();
void input_realname();
void input_callsign();
bool valid_phone( const char *phone );
void input_street();
void input_city();
void input_state();
void input_country();
void input_zipcode();
void input_sex();
void input_age( WUser *pUser );
void input_comptype();
void input_screensize();
void input_pw( WUser *pUser );
void input_ansistat();
int  find_new_usernum( const WUser *pUser, unsigned long *qsc );
void newuser();
void properize( char *pszText );
void cln_nu();


// File: pause.cpp

void pausescr();


// File: printfile.cpp

void print_local_file( const char *ss, const char *ss1 );
bool printfile( const char *pszFileName, bool bAbortable = true, bool bForcePause = true );


// File: quote.cpp

void grab_quotes(messagerec * m, const char *aux);
void auto_quote(char *org, long len, int type, time_t tDateTime);
void get_quote(int fsed);


// File: readmail.cpp

void readmail(int mode);
int  check_new_mail(int nUserNumber);


// File: shortmsg.cpp

void rsm( int nUserNum, WUser * pUser, bool bAskToSaveMsgs );
void ssm( int nUserNum, int nSystemNum, const char *pszFormat, ... );


// File: SmallRecord.cpp

void InsertSmallRecord(int nUserNumber, const char *name);
void DeleteSmallRecord(const char *name);


// File: sr.cpp

void calc_CRC(unsigned char b);
char gettimeout(double d, bool *abort);
int  extern_prot( int nProtocolNum, char *pszFileNameToSend, bool bSending );
bool ok_prot(int nProtocolNum, xfertype xt);
char *prot_name(int nProtocolNum);
int  get_protocol(xfertype xt);
void ascii_send(char *pszFileName, bool *sent, double *percent);
void maybe_internal(char *pszFileName, bool *xferred, double *percent, char ft, char *ftp, bool bSend, int prot);
void send_file(char *pszFileName, bool *sent, bool *abort, char ft, char *sfn, int dn, long fs);
void receive_file(char *pszFileName, int *received, char *ft, char *sfn, int dn);
char end_batch1();
void endbatch();


// File: srrcv.cpp

char modemkey(int *tout);
int  receive_block(char *b, unsigned char *bln, bool bUseCRC);
void xymodem_receive(char *pszFileName, char *ft, bool *received, bool bUseCRC);
void zmodem_receive(char *pszFileName, char *ft, bool *received );


// File: srsend.cpp

void send_block(char *b, int nBlockType, bool bUseCRC, char byBlockNumber);
char send_b(WFile &file, long pos, int nBlockType, char byBlockNumber, bool *bUseCRC, char *pszFileName, int *terr, bool *abort);
bool okstart(bool *bUseCRC, bool *abort);
void xymodem_send(char *pszFileName, bool *sent, double *percent, char ft, bool bUseCRC, bool bUseYModem, bool bUseYModemBatch );
void zmodem_send(char *pszFileName, bool *sent, double *percent, char ft  );


// File: strings.cpp

int  set_strings_fn(int filen, char *pszDirectoryName, char *pszFileName, int allowwrite);
void put_string(int filen, int n, char *pszText);
int  cachestat();
int  num_strings(int filen);
char *getrandomstring(int filen);
void close_strfiles();


// File: stuffin.cpp

void stuff_in( char *pszOutCommandLine, const char *pszInCommandLine,
               const char *pszFlag1, const char *pszFlag2,
               const char *pszFlag3, const char *pszFlag4,
               const char *pszFlag5 );


// File: subacc.cpp

void    close_sub();
bool    open_sub(bool wr);
bool    iscan1(int si, bool quick);
int     iscan(int b);
postrec *get_post( int mn );
void    delete_message(int mn);
void    write_post(int mn, postrec * pp);
void    add_post(postrec * pp);
void    resynch(int subnum, int *msgnum, postrec * pp);


// File: subedit.cpp

void boardedit();


// File: sublist.cpp

void old_sublist();
void SubList();


// File: subreq.cpp

void sub_req(int main_type, int minor_type, int tosys, char *extra);
int  find_hostfor(char *type, short *ui, char *pszDescription, short *opt);
void sub_xtr_del(int n, int nn, int f);
void sub_xtr_add(int n, int nn);
int  display_sub_categories();
int  amount_of_subscribers( const char *pszNetworkFileName );


// File: subxtr.cpp

bool read_subs_xtr(int nMaxSubs, int nNumSubs, subboardrec * subboards);


// File: syschat.cpp

void RequestChat();
void select_chat_name(char *pszSysopName);
void two_way_chat(char *pszRollover, int maxlen, bool crend, char *pszSysopName);
void chat1(char *pszChatLine, bool two_way);


// File: sysoplog.cpp

void slname(const char *d, char *pszLogFileName);
void islname(char *pszInstanceLogFileName);
void catsl();
void sysopchar(const char *pszLogText);
void sysoplog(const char *pszLogText, bool bIndent = true);
void sysoplogf( const char *pszFormat, ... );
void sysoplogfi( bool bIndent, const char *pszFormat, ... );


// File: sysopf.cpp

void isr1( int nUserNumber, const char *pszName );
void reset_files();
void prstatus(bool bIsWFC);
void valuser(int nUserNumber);
void print_net_listing( bool bForcePause );
void read_new_stuff();
void mailr();
void chuser();
void zlog();
void auto_purge();
void beginday(bool displayStatus);
void set_user_age();


// File: uedit.cpp

void deluser( int nUserNumber );
void print_data( int nUserNumber, WUser *pUser, bool bLongFormat, bool bClearScreen );
int  matchuser( int nUserNumber );
int  matchuser( WUser * pUser );
void changeopt();
void auto_val( int n, WUser *pUser );
void uedit(int usern, int other);
void print_affil( WUser *pUser );


// File: user.cpp

bool okconf( WUser *pUser );
void add_ass(int nNumPoints, const char *pszReason);



// File: utility.cpp

void remove_from_temp( const char *pszFileName, const char *pszDirectoryName, bool bPrintStatus );
bool sysop1();
void ToggleScrollLockKey();
bool okansi();
void reset_disable_conf();
void tmp_disable_conf(bool disable);
void tmp_disable_pause(bool disable);
void frequent_init();
double ratio();
double post_ratio();
double nsl();
void wait1(long l);
void Wait(double d);
double freek1(const char *pszPathName);
void send_net(net_header_rec * nh, unsigned short int *list, const char *text, const char *byname);
void giveup_timeslice();
char *stripfn(const char *pszFileName);
void stripfn_inplace(char *pszFileName);
void preload_subs();
char *get_wildlist(char *pszFileMask);
int side_menu(int *menu_pos, bool redraw, char *menu_items[], int xpos, int ypos, struct side_menu_colors * smc);
slrec getslrec(int nSl);
void shut_down( int nShutDownStatus );
void WWIV_SetFileTime(const char* pszFileName, const time_t tTime);
bool okfsed();
char* W_DateString(time_t tDateTime, char* mode , char* delim);
void WriteBuf( WFile &file, const char *pszText);


// File: wqscn.cpp

bool open_qscn();
void close_qscn();
void read_qscn( int nUserNumber, unsigned long *qscn, bool bStayOpen, bool bForceRead = false );
void write_qscn( int nUserNumber, unsigned long *qscn, bool bStayOpen);

// File: valscan.cpp

void valscan();


// File: vote.cpp

void print_quest(int mapp, int map[21]);
bool print_question(int i, int ii);
void vote_question(int i, int ii);
void vote();


// File: voteedit.cpp

void print_quests();
void set_question(int ii);
void ivotes();
void voteprint();


// File: wfc.cpp

void wfc_cls();
void wfc_init();
void wfc_screen();


// File: WStringUtils.cpp

// Moved to WStringUtils.h

// File: xfer.cpp

void zap_ed_info();
void get_ed_info();
unsigned long bytes_to_k(unsigned long lBytes);
int  check_batch_queue( const char *pszFileName );
bool check_ul_event( int nDirectoryNum, uploadsrec * pUploadRecord );
void finddevs(char (*devs)[9], int *count);
void find_devices();
bool okfn( const char *pszFileName );
void print_devices();
void get_arc_cmd(char *pszOutBuffer, const char *pszArcFileName, int cmd, const char *ofn);
int  list_arc_out(const char *pszFileName, const char *pszDirectory);
bool ratio_ok();
bool dcs();
void dliscan1(int nDirectoryNum);
void dliscan_hash(int nDirectoryNum);
void dliscan();
void add_extended_description(const char *pszFileName, const char *pszDescription);
void delete_extended_description(const char *pszFileName);
char *read_extended_description(const char *pszFileName);
void print_extended(const char *pszFileName, bool *abort, int numlist, int indent);
void align(char *pszFileName);
bool compare(const char *pszFileName1, const char *pszFileName2 );
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
void remlist(const char *pszFileName);
int  FileAreaSetRecord( WFile &file, int nRecordNumber );


// File: xferovl.cpp

void move_file();
void sortdir(int nDirectoryNum, int type);
void sort_all(int type);
void rename_file();
bool maybe_upload(const char *pszFileName, int nDirectoryNum, const char *pszDescription);
void upload_files(const char *pszFileName, int nDirectoryNum, int type);
bool uploadall(int nDirectoryNum);
void relist();
void edit_database();
void modify_database(const char *pszFileName, bool add);
bool is_uploadable(const char *pszFileName);
void xfer_defaults();
void finddescription();
void arc_l();


// File: xferovl1.cpp

void modify_extended_description(char **sss, const char *dest, const char *title);
bool valid_desc( const char *pszDescription );
bool get_file_idz(uploadsrec * pUploadRecord, int dn);
int  read_idz_all();
int  read_idz(int mode, int tempdir);
void tag_it();
void tag_files();
int  add_batch(char *pszDescription, const char *pszFileName, int dn, long fs);
int  try_to_download( const char *pszFileMask, int dn );
void download();
char fancy_prompt( const char *pszPrompt, const char *pszAcceptChars );
void endlist(int mode);
void SetNewFileScanDate();
void removefilesnotthere(int dn, int *autodel);
void removenotthere();
int  find_batch_queue( const char *pszFileName );
void remove_batch(const char *pszFileName);


// File: xfertmp.cpp

bool bad_filename(const char *pszFileName);
int  check_for_files_arc(const char *pszFileName);
int  check_for_files_zip(const char *pszFileName);
int  check_for_files_lzh(const char *pszFileName);
int  check_for_files_arj(const char *pszFileName);
int  check_for_files(const char *pszFileName);
void download_temp_arc(const char *pszFileName, int xfer);
void add_arc( const char *arc, const char *pszFileName, int dos );
void add_temp_arc();
void del_temp();
void list_temp_dir();
void temp_extract();
void list_temp_text();
void list_temp_arc();
void temporary_stuff();
void move_file_t();
void removefile();


// File: xinit.cpp





// WWIV Namespace inlined Functions





#endif // __INCLUDED_FCNS_H__
