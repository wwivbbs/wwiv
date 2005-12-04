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

#define _DEFINE_GLOBALS_
#include "wwiv.h"
#include "log.h"
#include "dirs.h"
#include "users.h"

/****************************************************************************/
/*                      GLOBALS                                             */
/****************************************************************************/
bool force_yes;
int num_dirs = 0;
int num_subs = 0;

/****************************************************************************/
/*                      STUBS                                               */
/****************************************************************************/
void *BbsAllocA( size_t lNumBytes )
{
	return malloc(lNumBytes);
}

void giveup_timeslice()
{
}

void WWIV_Delay(unsigned long delay)
{
}
char *stripfn(const char *pszFileName)
{
    static char szStaticFileName[15];
    char szTempFileName[ MAX_PATH ];

	WWIV_ASSERT(pszFileName);

    int nSepIndex = -1;
    for (int i = 0; i < wwiv::stringUtils::GetStringLength(pszFileName); i++)
    {
        if ( pszFileName[i] == '\\' || pszFileName[i] == ':' || pszFileName[i] == '/' )
        {
            nSepIndex = i;
        }
    }
    if (nSepIndex != -1)
    {
        strcpy( szTempFileName, &( pszFileName[nSepIndex + 1] ) );
    }
    else
    {
        strcpy( szTempFileName, pszFileName );
    }
    for ( int i1 = 0; i1 < wwiv::stringUtils::GetStringLength( szTempFileName ); i1++ )
    {
        if ( szTempFileName[i1] >= 'A' && szTempFileName[i1] <= 'Z' )
        {
            szTempFileName[i1] = szTempFileName[i1] - 'A' + 'a';
        }
    }
    int j = 0;
    while ( szTempFileName[j] != 0 )
    {
        if ( szTempFileName[j] == SPACE )
        {
            strcpy( &szTempFileName[j], &szTempFileName[j + 1] );
        }
        else
        {
            ++j;
        }
    }
    strcpy( szStaticFileName, szTempFileName );
    return szStaticFileName;
}

#define LAST(s) s[strlen(s)-1]

bool mkdirs(char *s)
{
  char current_path[MAX_PATH], *p, *flp;

  p = flp = WWIV_STRDUP(s);
  getcwd(current_path, MAX_PATH);
  if(LAST(p) == WWIV_FILE_SEPERATOR_CHAR)
    LAST(p) = 0;
  if(*p == WWIV_FILE_SEPERATOR_CHAR) {
    chdir(WWIV_FILE_SEPERATOR_STRING);
    p++;
  }
  for(; (p = strtok(p, WWIV_FILE_SEPERATOR_STRING)) != 0; p = 0) {
    if(chdir(p)) {
      if(mkdir(p)) {
        chdir(current_path);
        return false;
      }
      chdir(p);
    }
  }
  chdir(current_path);
  if(flp)
  {
    BbsFreeMemory(flp);
  }
  return true;
}

/****************************************************************************/
/*                      LOCALS                                              */
/****************************************************************************/
void ShowBanner()
{
    printf("WWIV Bulletin Board System - %s\n", wwiv_version);
    printf("Copyright (c) 1998-2004, WWIV Software Services.\n");
    printf("All Rights Reserved.\n\n");
    printf("Compile Time : %s\n\n",wwiv_date);
}

void ShowHelp()
{
    printf("Command Line Usage:\n\n");
    printf("\t-Y\t= Force Yes to All Prompts\n");
    printf("\t-U\t= Skip User Record Check\n");
    printf("\n");
    exit(0);
}

void giveUp()
{
    Print(NOK, true, "Giving up.");
    exit(-1);
}

void maybeGiveUp(void)
{
  Print(OK, true, "Future expansion might try to fix this problem.");
  giveUp();
}

void checkFileSize(WFile &file, unsigned long len)
{
    if(!file.IsOpen())
    {
        int nFileMode = WFile::modeReadOnly | WFile::modeBinary;
        file.Open(nFileMode);
    }
    unsigned long actual = file.GetLength();
    file.Close();
    if(actual < len)
    {
        Print(NOK, true, "%s too short (%ld<%ld).", file.GetFullPathName(), actual, len);
        giveUp();
    }
    if(actual > len)
    {
        Print(NOK, true, "%s too long (%ld>%ld).", file.GetFullPathName(), actual, len);
        Print(NOK, true, "Attempting to continue.");
    }
}

bool checkDirExists(WFile &dir, const char *desc)
{
    bool exist = dir.Exists();
    if(!exist)
    {
        Print(NOK, false, "Unable to find dir '%s'", dir.GetFullPathName());
        Print(NOK, false, "for '%s' dir", desc);
	printf("   Do you wish to CREATE it (y/N)?\n");
	char ch[128];
	std::cin >> ch;
	if(ch[0] == 'Y' || ch[0] == 'y')
	{
	    exist = mkdirs(dir.GetFullPathName());
	    if(!exist)
	    {
                Print(NOK, true, "Unable to create dir '%s' for %s dir.", dir.GetFullPathName(), desc);
	    }
	}
    }
    return exist;
}

/****************************************************************************/
/*                      CHECK METHODS                                       */
/****************************************************************************/

void initStatusDat()
{
    int nFileMode = WFile::modeReadOnly | WFile::modeBinary;
    bool update = false;
    WFile statusDat(syscfg.datadir, STATUS_DAT);
    if(!statusDat.Exists())
    {
        Print(NOK, true, "%s NOT FOUND.", statusDat.GetFullPathName());
        Print(OK, true, "Recreating %s.", statusDat.GetFullPathName());
	memset(&status, 0, sizeof(statusrec));
	strcpy(status.date1, "00/00/00");
	strcpy(status.date2, status.date1);
	strcpy(status.date3, status.date1);
	strcpy(status.log1, "000000.log");
	strcpy(status.log2, "000000.log");
	strcpy(status.gfiledate, "00/00/00");
	status.callernum = 65535;
        update = true;
    }
    else
    {
        checkFileSize(statusDat, sizeof(statusrec));
        Print(OK, true, "Reading %s...", statusDat.GetFullPathName());
        statusDat.Open(nFileMode);
        if (!statusDat.Open(nFileMode))
        {
            Print(NOK, true, "%s NOT FOUND.", statusDat.GetFullPathName());
            giveUp();
        }
        statusDat.Read(&status, sizeof(statusrec));
        statusDat.Close();

	// version check
	if(status.wwiv_version > wwiv_num_version)
	{
	    Print(NOK, true, "Incorrect version of fix (this is for %d, you need %d)", wwiv_num_version, status.wwiv_version);
	    giveUp();
	}

        time_t val = time(NULL);
        char *curDate = dateFromTimeT(val);
	if(strcmp(status.date1, curDate))
	{
	    strcpy(status.date1, curDate);
            update = true;
	    Print(OK, true, "Date error in STATUS.DAT (status.date1) corrected");
	}

        val -= 86400L;
        curDate = dateFromTimeT(val);
	if(strcmp(status.date2, curDate))
	{
	    strcpy(status.date2, curDate);
            update = true;
	    Print(OK, true, "Date error in STATUS.DAT (status.date2) corrected");
	}
	char logFile[512];
	snprintf(logFile, sizeof(logFile), "%s.log", dateFromTimeTForLog(val));
	if(strcmp(status.log1, logFile))
	{
	    strcpy(status.log1, logFile);
            update = true;
	    Print(OK, true, "Log filename error in STATUS.DAT (status.log1) corrected");
	}

        val -= 86400L;
        curDate = dateFromTimeT(val);
	if(strcmp(status.date3, curDate))
	{
	    strcpy(status.date3, curDate);
            update = true;
	    Print(OK, true, "Date error in STATUS.DAT (status.date3) corrected");
	}
	snprintf(logFile, sizeof(logFile), "%s.log", dateFromTimeTForLog(val));
	if(strcmp(status.log2, logFile))
	{
	    strcpy(status.log2, logFile);
            update = true;
	    Print(OK, true, "Log filename error in STATUS.DAT (status.log2) corrected");
	}
    }
    if(update)
    {
        statusDat.Open( WFile::modeReadWrite | WFile::modeBinary );
        statusDat.Write( &status, sizeof( statusrec ) );
        statusDat.Close();
    }
}

void initDirsDat()
{
    WFile dirsDat(syscfg.datadir, DIRS_DAT);
    if(!dirsDat.Exists())
    {
        Print(NOK, true, "%s NOT FOUND.", dirsDat.GetFullPathName());
	maybeGiveUp();
    }
    else
    {
        Print(OK, true, "Reading %s...", dirsDat.GetFullPathName());
        int nFileMode = WFile::modeReadOnly | WFile::modeBinary;
	dirsDat.Open(nFileMode);
        directories = (directoryrec *)bbsmalloc(dirsDat.GetLength() + 1);
	if(directories == NULL)
	{
	    Print(NOK, true, "Couldn't allocate %ld bytes for %s.", dirsDat.GetLength(), dirsDat.GetFullPathName());
	    giveUp();
	}
	num_dirs = (dirsDat.Read(directories, dirsDat.GetLength())) / sizeof(directoryrec);
	dirsDat.Close();
	Print(OK, true, "Found %d directories", num_dirs);
    }
}

void initSubsDat()
{
    WFile subsDat(syscfg.datadir, SUBS_DAT);
    if(!subsDat.Exists())
    {
        Print(NOK, true, "%s NOT FOUND.", subsDat.GetFullPathName());
	maybeGiveUp();
    }
    else
    {
        Print(OK, true, "Reading %s...", subsDat.GetFullPathName());
        int nFileMode = WFile::modeReadOnly | WFile::modeBinary;
	subsDat.Open(nFileMode);
        subboards = (subboardrec *)bbsmalloc(subsDat.GetLength() + 1);
	if(subboards == NULL)
	{
	    Print(NOK, true, "Couldn't allocate %ld bytes for %s.", subsDat.GetLength(), subsDat.GetFullPathName());
	    giveUp();
	}
	num_subs = (subsDat.Read(subboards, subsDat.GetLength())) / sizeof(subboardrec);
	subsDat.Close();
	Print(OK, true, "Found %d subs", num_subs);
    }
}

void init()
{
    WFile configFile(CONFIG_DAT);
    if (!configFile.Exists())
    {
        Print(NOK, true, "%s NOT FOUND.", CONFIG_DAT);
        giveUp();
    }
    int nFileMode = WFile::modeReadOnly | WFile::modeBinary;

    checkFileSize(configFile, sizeof(configrec));
    Print(OK, true, "Reading %s...", configFile.GetFullPathName());
    configFile.Open(nFileMode);
    int readIn = configFile.Read(&syscfg, sizeof(configrec));
    configFile.Close();
    if(readIn != sizeof(configrec))
    {
        Print(NOK, true, "Failed to read %s (%d != %d)", configFile.GetFullPathName(), readIn, sizeof(configrec));
	giveUp();
    }
    syscfg.userreclen = sizeof(userrec);

    WFile dataDir(syscfg.datadir);
    if(!checkDirExists(dataDir, "Data"))
    {
        Print(NOK, true, "Must find DATA directory to continue.");
	giveUp();
    }
    
    initStatusDat();
    initDirsDat();
    initSubsDat();
}

void checkCriticalFiles()
{
    const char *dataFiles[] =
    {
        ARCHIVER_DAT,
	WFC_DAT,
	COLOR_DAT,
        NULL
    };

    Print(OK, true, "Checking for Critical DATA files...");
    int fileNo = 0;
    while(dataFiles[fileNo] != NULL)
    {
        WFile file(syscfg.datadir, dataFiles[fileNo]);
	if(!file.Exists())
	{
            Print(NOK, true, "Critical file: %s is missing", dataFiles[fileNo]);
	}
	fileNo++;
    }
}

void saveStatus()
{
    WFile statusDat(syscfg.datadir, STATUS_DAT);

    statusDat.Open( WFile::modeReadWrite | WFile::modeBinary );
    statusDat.Write( &status, sizeof( statusrec ) );
    statusDat.Close();
}

/****************************************************************************/
int main(int argc, char *argv[])
{
  int i;
  char *ss;
  time_t startTime;
  time_t endTime;
  bool usercheck = true;

  ShowBanner();
  for (i = 1; i < argc; i++) {
    ss = argv[i];
    if ((*ss == '/') || (*ss == '-')) {
      switch (toupper(ss[1])) {
    case 'Y':
      force_yes = true;
      break;
    case 'U':
      usercheck = false;
      break;
    case '?':
      ShowHelp();
      break;
      }
    } else {
      Print(NOK, false, "Unknown argument: '%s'", ss);
    }
  }

  // get the current time in seconds
  startTime = time(NULL);
  // open the log
  OpenLogFile("FIX.LOG");

  // read in all critical data
  init();

  checkAllDirs();
  checkCriticalFiles();
  checkUserList();

  saveStatus();

/*
  if (usercheck)
    check_nameslist();
  find_max_qscan();
  check_msg_consistency();
*/
  endTime = time(NULL);

  Print(OK, true, "FIX Completed.  Time elapsed: %d seconds\n\n", (endTime-startTime));
  CloseLogFile();

  return 0;
}
