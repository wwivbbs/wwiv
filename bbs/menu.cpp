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

static user_config *pSecondUserRec;         // Userrec2 style setup
static int nSecondUserRecLoaded;            // Whos config is loaded

static FILE *hMenuDesc;

static char *pMenuStrings;
static char **ppMenuStringsIndex;
static int nNumMenuCmds;

//
// Local function prototypes
//
bool CheckMenuPassword( char* pszCorrectPassword );
int  GetMenuIndex( const char* pszCommand );
bool LoadMenuSetup( int nUserNum );
bool ValidateMenuSet( const char *pszMenuDir);
void ReadMenuSetup();
void StartMenus();
void Menus(MenuInstanceData * pMenuData, const char *pszDir, const char *pszMenu);
void CloseMenu(MenuInstanceData * pMenuData);
bool OpenMenu(MenuInstanceData * pMenuData);
bool CheckMenuSecurity(MenuHeader * pHeader, bool bCheckPassword );
bool LoadMenuRecord(MenuInstanceData * pMenuData, const char *pszCommand, MenuRec * pMenu);
void MenuExecuteCommand(MenuInstanceData * pMenuData, const char *pszCommand);
void LogUserFunction(MenuInstanceData * pMenuData, const char *pszCommand, MenuRec * pMenu);
void PrintMenuPrompt(MenuInstanceData * pMenuData);
void AMDisplayHelp(MenuInstanceData * pMenuData);
void TurnMCIOff();
void TurnMCIOn();
bool AMIsNumber(const char *pszBuf);
void QueryMenuSet();
void WriteMenuSetup(int nUserNum);
void UnloadMenuSetup();
void GetCommand(MenuInstanceData * pMenuData, char *pszBuf);
bool CheckMenuItemSecurity(MenuInstanceData * pMenuData, MenuRec * pMenu, bool bCheckPassword );
void GenerateMenu(MenuInstanceData * pMenuData);
char *MenuParseLine(char *pszSrc, char *pszCmd, char *pszParam1, char *pszParam2);
char *MenuDoParenCheck(char *pszSrc, int bMore, char *porig);
char *MenuGetParam(char *pszSrc, char *pszParam);
char *MenuSkipSpaces(char *pszSrc);
void InterpretCommand(MenuInstanceData * pMenuData, const char *pszScript);


bool CheckMenuPassword( char* pszCorrectPassword )
{
    std::string password;
    if ( wwiv::stringUtils::IsEqualsIgnoreCase( pszCorrectPassword, "*SYSTEM" ) )
    {
        password = syscfg.systempw;
    }
    else
    {
        password = pszCorrectPassword;
    }

    GetSession()->bout.NewLine();
    std::string passwordFromUser;
    input_password( "|#2SY: ", passwordFromUser, 20 );
    return ( passwordFromUser == password ) ? true : false;
}


// returns -1 of the item is not matched, this will fall though to the
// default case in InterpretCommand
int GetMenuIndex( const char* pszCommand )
{
    for( int i=0; i < nNumMenuCmds; i++ )
    {
        char* p = ppMenuStringsIndex[i];
        if ( p && *p && wwiv::stringUtils::IsEqualsIgnoreCase( pszCommand, p ) )
        {
            return i;
        }
    }
    return -1;
}


void ReadMenuSetup()
{
	if (pMenuStrings == NULL)
    {
		char szMenuCmdsFileName[MAX_PATH];
		sprintf(szMenuCmdsFileName, "%s%s", syscfg.datadir, MENUCMDS_DAT);
		FILE* fp = fopen(szMenuCmdsFileName, "rb");
		if (!fp)
        {
			sysoplog("Unable to open menucmds.dat");
			exit( 0 );
		}

		fseek(fp, 0, SEEK_END);
		long lLen = ftell(fp);
		fseek(fp, 0, SEEK_SET);

        short int nAmt = 0;
		fread(&nAmt, 2, 1, fp);
		if (nAmt == 0)
        {
			exit( 0 );
        }

		short int* index = static_cast<short *>( bbsmalloc( sizeof( short int ) * nAmt ) );
		fread(index, 2, nAmt, fp);
		lLen -= ftell(fp);
		pMenuStrings = static_cast<char *>( bbsmalloc( lLen ) );
		ppMenuStringsIndex = static_cast<char **>( bbsmalloc( sizeof( char ** ) * nAmt ) );
		fread(pMenuStrings, lLen, 1, fp);
		fclose(fp);

		for (int nX = 0; nX < nAmt; nX++)
		{
			ppMenuStringsIndex[nX] = pMenuStrings + index[nX];
		}
        nNumMenuCmds = nAmt;

		BbsFreeMemory(index);
	}
}

void mainmenu()
{
	if (pSecondUserRec)
    {
		BbsFreeMemory(pSecondUserRec);
	    pSecondUserRec = NULL;
    }
	pSecondUserRec = static_cast<user_config *>( bbsmalloc( sizeof( user_config ) ) );
	if (!pSecondUserRec)
    {
		return;
    }

	ReadMenuSetup();

	while (!hangup)
    {
		StartMenus();
    }

	BbsFreeMemory(pSecondUserRec);
	pSecondUserRec = NULL;
	nSecondUserRecLoaded = 0;

	BbsFreeMemory(pMenuStrings);
	pMenuStrings = NULL;
	BbsFreeMemory(ppMenuStringsIndex);
	ppMenuStringsIndex = NULL;
}


void StartMenus()
{
	MenuInstanceData* pMenuData = static_cast<MenuInstanceData *>( bbsmalloc( sizeof( MenuInstanceData ) ) );
	pMenuData->pMenuFile = NULL;
	if (!pMenuData)
	{
		sysoplog("Unable to allocate memory for pMenuData");
		hangup = true;
		return;
	}
	pMenuData->nReload = 1;                    // force loading of menu

	if (!LoadMenuSetup(GetSession()->usernum))
	{
		strcpy(pSecondUserRec->szMenuSet, "wwiv"); // default menu set name
		pSecondUserRec->cHotKeys = HOTKEYS_ON;
		pSecondUserRec->cMenuType = MENUTYPE_REGULAR;
		WriteMenuSetup(GetSession()->usernum);
	}
	while (pMenuData->nReload != 0 && !hangup)
	{
		if ( pMenuData->pMenuFile != NULL )
		{
			delete pMenuData->pMenuFile;
			pMenuData->pMenuFile = NULL;

		}
		memset(pMenuData, 0, sizeof(MenuInstanceData));

		pMenuData->nFinished = 0;
		pMenuData->nReload = 0;


		if (!LoadMenuSetup(GetSession()->usernum))
		{
			LoadMenuSetup( 1 );
			ConfigUserMenuSet();
		}
		if ( !ValidateMenuSet( pSecondUserRec->szMenuSet ) )
		{
			ConfigUserMenuSet();
		}

		Menus(pMenuData, pSecondUserRec->szMenuSet, "main"); // default starting menu
	}
	if (pMenuData)
	{
		BbsFreeMemory(pMenuData);
	}
}


void Menus(MenuInstanceData * pMenuData, const char *pszDir, const char *pszMenu)
{
    strcpy(pMenuData->szPath, pszDir);
    strcpy(pMenuData->szMenu, pszMenu);

    if (OpenMenu(pMenuData))
    {
        if ((pMenuData->header.nNumbers == MENU_NUMFLAG_DIRNUMBER) &&
            (udir[0].subnum==-1)) {
            GetSession()->bout << "\r\nYou cannot currently access the file section.\r\n\n";
            CloseMenu(pMenuData);
            return;
        }
        // if flagged to display help on entrance, then do so
        if ( GetSession()->GetCurrentUser()->IsExpert() && pMenuData->header.nForceHelp == MENU_HELP_ONENTRANCE )
        {
            AMDisplayHelp(pMenuData);
        }

        char szCommand[51];
        while ( !hangup && pMenuData->nFinished == 0 )
        {
            PrintMenuPrompt( pMenuData );
            GetCommand( pMenuData, szCommand );
            MenuExecuteCommand( pMenuData, szCommand );
        }
    }
    else if ( wwiv::stringUtils::IsEqualsIgnoreCase( pszMenu, "main" ) ) // default menu name
    {
        hangup = true;
    }
    CloseMenu( pMenuData );
}


void CloseMenu(MenuInstanceData * pMenuData)
{
	if ( pMenuData->pMenuFile != NULL && pMenuData->pMenuFile->IsOpen() )
	{
		pMenuData->pMenuFile->Close();
		delete pMenuData->pMenuFile;
		pMenuData->pMenuFile = NULL;
	}

	if (pMenuData->index != NULL)
	{
		BbsFreeMemory(pMenuData->index);
		pMenuData->index = NULL;
	}

	if (pMenuData->szPrompt != NULL)
	{
		BbsFreeMemory(pMenuData->szPrompt);
		pMenuData->szPrompt = NULL;
	}
}


bool OpenMenu(MenuInstanceData * pMenuData)
{
	char szMenuFileName[MAX_PATH + 1];

	CloseMenu(pMenuData);

	// --------------------------
	// Open up the main data file
	// --------------------------
	sprintf(szMenuFileName, "%s%s%c%s.mnu", GetMenuDirectory(), pMenuData->szPath,
			WWIV_FILE_SEPERATOR_CHAR, pMenuData->szMenu);
	pMenuData->pMenuFile = new WFile( szMenuFileName );
	pMenuData->pMenuFile->Open( WFile::modeBinary|WFile::modeReadOnly, WFile::shareDenyNone, WFile::permReadWrite );

	// -----------------------------------
	// Find out how many records there are
	// -----------------------------------
	if ( pMenuData->pMenuFile->IsOpen() )
    {
		long lSize = pMenuData->pMenuFile->GetLength();
		pMenuData->nAmountRecs = static_cast<INT16>(lSize / sizeof(MenuRec));
	}
    else
    {                                  // Unable to open menu
		MenuSysopLog("Unable to open Menu");
		pMenuData->nAmountRecs = 0;
		return false;
	}

	// -------------------------
	// Read the header (control)
	// record into memory
	// -------------------------
	pMenuData->pMenuFile->Seek( 0L, WFile::seekBegin );
	pMenuData->pMenuFile->Read( &pMenuData->header, sizeof( MenuHeader ) );

	// version numbers can be checked here

	// ------------------------------
	// Open/Read/Close the index file
	// ------------------------------
    char szIndexFileName[ MAX_PATH ];
	sprintf( szIndexFileName, "%s%s%c%s.idx", GetMenuDirectory(), pMenuData->szPath,
			 WWIV_FILE_SEPERATOR_CHAR, pMenuData->szMenu );
	WFile fileIndex( szIndexFileName );
	if ( fileIndex.Open( WFile::modeBinary|WFile::modeReadOnly, WFile::shareDenyNone, WFile::permReadWrite ) )
	{
		if ( fileIndex.GetLength() > static_cast<long>( pMenuData->nAmountRecs * sizeof( MenuRecIndex ) ) )
		{
			MenuSysopLog("Index is corrupt");
			MenuSysopLog(szIndexFileName);
			return false;
		}
		pMenuData->index = static_cast<MenuRecIndex *>( bbsmalloc( pMenuData->nAmountRecs * sizeof( MenuRecIndex ) + TEST_PADDING ) );
		if (pMenuData->index != NULL)
		{
			fileIndex.Read( pMenuData->index, pMenuData->nAmountRecs * sizeof( MenuRecIndex ) );
		}
		fileIndex.Close();					// close the file
	}
	else
	{                                  // Unable to open menu index
		MenuSysopLog("Unable to open Menu Index");
		return false;
	}


	// ----------------------------
	// Open/Rease/Close Prompt file
	// ----------------------------
    char szPromptFileName[ MAX_PATH ];
	sprintf( szPromptFileName, "%s%s%c%s.pro", GetMenuDirectory(), pMenuData->szPath,
			 WWIV_FILE_SEPERATOR_CHAR, pMenuData->szMenu );
	WFile filePrompt( szPromptFileName );
	if ( filePrompt.Open( WFile::modeBinary|WFile::modeReadOnly, WFile::shareDenyNone, WFile::permReadWrite ) )
    {
		long lSize = filePrompt.GetLength();
		pMenuData->szPrompt = static_cast<char *>( bbsmalloc( lSize + 10 + TEST_PADDING ) );
		if (pMenuData->szPrompt != NULL)
        {
			lSize = filePrompt.Read( pMenuData->szPrompt, lSize );
			pMenuData->szPrompt[lSize] = 0;

			char* sp = strstr(pMenuData->szPrompt, ".end.");
			if ( sp )
            {
				sp[0] = '\0';
            }
		}
		filePrompt.Close();
	}
	if (!CheckMenuSecurity(&pMenuData->header, true))
    {
		MenuSysopLog("< Menu Sec");
		return false;
	}
	if (pMenuData->header.szScript[0])
    {
		InterpretCommand(pMenuData, pMenuData->header.szScript);
    }

	return true;
}


bool CheckMenuSecurity(MenuHeader * pHeader, bool bCheckPassword )
{
    if ( ( pHeader->nFlags & MENU_FLAG_DELETED ) ||
        ( GetSession()->GetEffectiveSl() < pHeader->nMinSL ) ||
        ( GetSession()->GetCurrentUser()->GetDsl() < pHeader->nMinDSL ) )
    {
        return false;
    }

    // All AR bits specified must match
    for (short int x = 0; x < 16; x++)
    {
        if (pHeader->uAR & (1 << x))
        {
            if ( !GetSession()->GetCurrentUser()->HasArFlag( 1 << x ) )
            {
                return false;
            }
        }
    }

    // All DAR bits specified must match
    for (short int x = 0; x < 16; x++)
    {
        if (pHeader->uDAR & (1 << x))
        {
            if ( !GetSession()->GetCurrentUser()->HasDarFlag( 1 << x ) )
            {
                return ( GetSession()->GetCurrentUser()->GetDsl() < pHeader->nMinDSL );
            }
        }
    }

    // If any restrictions match, then they arn't allowed
    for (short int x = 0; x < 16; x++)
    {
        if (pHeader->uRestrict & (1 << x))
        {
            if ( GetSession()->GetCurrentUser()->HasRestrictionFlag( 1 << x ) )
            {
                return ( GetSession()->GetCurrentUser()->GetDsl() < pHeader->nMinDSL );
            }
        }
    }

    if ( ( pHeader->nSysop && !so() ) ||
         ( pHeader->nCoSysop && !cs() ) )
    {
        return false;
    }

    if (pHeader->szPassWord[0] && bCheckPassword )
    {
        if ( !CheckMenuPassword( pHeader->szPassWord ) )
        {
            return false;
        }
    }
    return true;
}


bool LoadMenuRecord( MenuInstanceData * pMenuData, const char *pszCommand, MenuRec * pMenu )
{
	memset(pMenu, 0, sizeof(MenuRec));

	// ------------------------------------------------
	// If we have 'numbers set the sub #' turned on
	// then create a command to do so if a # is entered
	// ------------------------------------------------
	if (AMIsNumber(pszCommand))
    {
		if (pMenuData->header.nNumbers == MENU_NUMFLAG_SUBNUMBER)
        {
			memset(pMenu, 0, sizeof(MenuRec));
			sprintf(pMenu->szExecute, "SetSubNumber %d", atoi(pszCommand));
			return true;
		}
		if (pMenuData->header.nNumbers == MENU_NUMFLAG_DIRNUMBER)
        {
			memset(pMenu, 0, sizeof(MenuRec));
			sprintf(pMenu->szExecute, "SetDirNumber %d", atoi(pszCommand));
			return true;
		}
	}
	for (int x = 0; x < pMenuData->nAmountRecs; x++)
    {
		if ( wwiv::stringUtils::IsEqualsIgnoreCase( pMenuData->index[x].szKey, pszCommand ) )
        {
			if ((pMenuData->index[x].nFlags & MENU_FLAG_DELETED) == 0)
            {
				if (pMenuData->index[x].nRec != 0)
                {
                    // Dont include control record
					pMenuData->pMenuFile->Seek( pMenuData->index[x].nRec * sizeof( MenuRec ), WFile::seekBegin );
					pMenuData->pMenuFile->Read( pMenu, sizeof( MenuRec ) );

					if ( CheckMenuItemSecurity( pMenuData, pMenu, 1 ) )
                    {
						return true;
                    }
					else
                    {
						std::ostringstream msg;
						msg << "< item security : %s" << pszCommand;
						MenuSysopLog(msg.str().c_str());
						return false;
					}
				}
			}
		}
	}
	return false;
}


void MenuExecuteCommand(MenuInstanceData * pMenuData, const char *pszCommand)
{
	MenuRec menu;

	if (LoadMenuRecord(pMenuData, pszCommand, &menu))
    {
		LogUserFunction(pMenuData, pszCommand, &menu);
		InterpretCommand(pMenuData, menu.szExecute);
	}
    else
    {
		LogUserFunction(pMenuData, pszCommand, &menu);
    }
}


void LogUserFunction(MenuInstanceData * pMenuData, const char *pszCommand, MenuRec * pMenu)
{
    switch ( pMenuData->header.nLogging )
    {
    case MENU_LOGTYPE_KEY:
        sysopchar( pszCommand );
        break;
    case MENU_LOGTYPE_COMMAND:
        sysoplog( pMenu->szExecute );
        break;
    case MENU_LOGTYPE_DESC:
        sysoplog( pMenu->szMenuText[0] ? pMenu->szMenuText : pMenu->szExecute );
        break;
    case MENU_LOGTYPE_NONE:
    default:
        break;
    }
}



void MenuSysopLog(const char *pszMsg)
{
    std::ostringstream logStream;
    logStream << "*MENU* : " << pszMsg;

    sysopchar( logStream.str().c_str() );
    GetSession()->bout << logStream.str();
	GetSession()->bout.NewLine();
}


void PrintMenuPrompt( MenuInstanceData * pMenuData )
{
    if ( !GetSession()->GetCurrentUser()->IsExpert() || pMenuData->header.nForceHelp == MENU_HELP_FORCE )
    {
		AMDisplayHelp( pMenuData );
    }

	TurnMCIOn();

	if ( pMenuData->szPrompt )
    {
		GetSession()->bout << pMenuData->szPrompt;
    }

	TurnMCIOff();
}


void AMDisplayHelp( MenuInstanceData * pMenuData )
{
	char szFileName[MAX_PATH];

	sprintf( szFileName, "%s%s%c%s", GetMenuDirectory(), pMenuData->szPath,
			 WWIV_FILE_SEPERATOR_CHAR, pMenuData->szMenu );

	char * pszTemp = szFileName + strlen(szFileName);

	if ( GetSession()->GetCurrentUser()->HasAnsi() )
    {
		if ( GetSession()->GetCurrentUser()->HasColor() )
        {
			strcpy(pszTemp, ".ans");
			if (!WFile::Exists(szFileName))
            {
				pszTemp[0] = '\0';
            }
		}
		if ( !*pszTemp )
        {
			strcpy(pszTemp, ".b&w");
			if (!WFile::Exists(szFileName))
            {
				strcpy(pszTemp, ".msg");
            }
		}
	}
    else
    {
		strcpy(pszTemp, ".msg");
    }

	if ( printfile(szFileName, 1) == false )
    {
		GenerateMenu(pMenuData);
    }
}


void TurnMCIOff()
{
	if (!(syscfg.sysconfig & sysconfig_enable_mci))
    {
		g_flags |= g_flag_disable_mci;
    }
}


void TurnMCIOn()
{
    g_flags &= ~g_flag_disable_mci;
}



bool AMIsNumber(const char *pszBuf)
{
	int nSize = strlen(pszBuf);

	if (!nSize)
    {
		return false;
    }

	for (int nPos = 0; nPos < nSize; ++nPos)
    {
		if (isdigit(pszBuf[nPos]) == 0)
        {
			return false;
        }
	}
	return true;
}


void ConfigUserMenuSet()
{
    char szMsg[101], szDesc[101];

    ReadMenuSetup();

    if (GetSession()->usernum != nSecondUserRecLoaded)
    {
        if ( !LoadMenuSetup(GetSession()->usernum) )
        {
            LoadMenuSetup( 1 );
        }
    }

    nSecondUserRecLoaded = GetSession()->usernum;

    GetSession()->bout.ClearScreen();
    printfile(MENUWEL_NOEXT);
    bool bDone = false;
    while (!bDone && !hangup)
    {
        GetSession()->bout << "   |#1WWIV |#6Menu |#1Editor|#0\r\n\r\n";
		GetSession()->bout << "|#21|06) |#1Menuset      |06: |15" << pSecondUserRec->szMenuSet << wwiv::endl;
		GetSession()->bout << "|#22|06) |#1Use hot keys |06: |15" << ( pSecondUserRec->cHotKeys == HOTKEYS_ON ? "Yes" : "No ") << wwiv::endl;
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#9[|0212? |08Q|02=Quit|#9] :|#0 ";

        char chKey = onek( "Q12?" );

        switch (chKey)
        {
        case 'Q':
            bDone = 1;
            break;

        case '1': 
        {
            ListMenuDirs();
            GetSession()->bout.NewLine( 2 );
            GetSession()->bout << "|15Enter the menu set to use : |#0";
            std::string menuSetName;
            inputl( menuSetName, 8 );
            if ( ValidateMenuSet( menuSetName.c_str() ) )
            {
                OpenMenuDescriptions();
                GetSession()->bout.NewLine();
				GetSession()->bout << "|#1Menu Set : |#2" <<  menuSetName.c_str() << "  -  |15" << GetMenuDescription(  menuSetName, szDesc ) << wwiv::endl;
                GetSession()->bout << "|#5Use this menu set? ";
                if (noyes())
                {
                    strcpy(pSecondUserRec->szMenuSet, menuSetName.c_str());
                    break;
                }
                CloseMenuDescriptions();
            }
            GetSession()->bout.NewLine();
            GetSession()->bout << "|#8That menu set does not exists, resetting to the default menu set" << wwiv::endl;
            pausescr();
            if (pSecondUserRec->szMenuSet[0] == '\0')
            {
                strcpy(pSecondUserRec->szMenuSet, "wwiv");
            }
        }
        break;

        case '2':
            pSecondUserRec->cHotKeys = !pSecondUserRec->cHotKeys;
            break;

        case '3':
            pSecondUserRec->cMenuType = !pSecondUserRec->cMenuType;
            break;

        case '?':
            printfile(MENUWEL_NOEXT);
            continue;                           // bypass the below cls()
        }

        GetSession()->bout.ClearScreen();
    }

    // If menu is invalid, it picks the first one it finds
    if ( !ValidateMenuSet( pSecondUserRec->szMenuSet ) )
    {
        if ( GetSession()->num_languages > 1 && GetSession()->GetCurrentUser()->GetLanguage() != 0 )
        {
            GetSession()->bout << "|#6No menus for " << languages[GetSession()->GetCurrentUser()->GetLanguage()].name << " language.";
            input_language();
        }
    }

    WriteMenuSetup(GetSession()->usernum);

    sprintf(szMsg, "Menu in use : %s - %s - %s", pSecondUserRec->szMenuSet, pSecondUserRec->cHotKeys == HOTKEYS_ON ? "Hot" : "Off", pSecondUserRec->cMenuType == MENUTYPE_REGULAR ? "REG" : "PD");
    MenuSysopLog(szMsg);
    GetSession()->bout.NewLine( 2 );
}


void QueryMenuSet()
{
	user_config tmpcfg;
    bool bSecondUserRecLoaded = false;

	if (!pSecondUserRec)
    {
		bSecondUserRecLoaded = true;
		pSecondUserRec = &tmpcfg;
	}
	ReadMenuSetup();

	if (GetSession()->usernum != nSecondUserRecLoaded)
    {
		if ( !LoadMenuSetup(GetSession()->usernum) )
        {
			LoadMenuSetup( 1 );
        }
    }

	nSecondUserRecLoaded = GetSession()->usernum;

	ValidateMenuSet( pSecondUserRec->szMenuSet );

	GetSession()->bout.NewLine( 2 );
	if (pSecondUserRec->szMenuSet[0] == 0)
    {
		strcpy(pSecondUserRec->szMenuSet, "WWIV");
    }
    GetSession()->bout << "|#7Configurable menu set status:\r\n\r\n";
	GetSession()->bout << "|#8Menu in use  : |#9" << pSecondUserRec->szMenuSet << wwiv::endl;
	GetSession()->bout << "|#8Hot keys are : |#9" << ( pSecondUserRec->cHotKeys == HOTKEYS_ON ? "On" : "Off" ) << wwiv::endl;
	GetSession()->bout.NewLine();

    GetSession()->bout << "|#7Would you like to change these? (N) ";
	if ( yesno() )
    {
		ConfigUserMenuSet();
    }

	if ( bSecondUserRecLoaded )
    {
		pSecondUserRec = NULL;
		nSecondUserRecLoaded = 0;
	}
}



bool ValidateMenuSet( const char *pszMenuDir )
{
	if (GetSession()->usernum != nSecondUserRecLoaded)
    {
		if ( !LoadMenuSetup( GetSession()->usernum ) )
        {
			LoadMenuSetup( 1 );
        }
    }

	nSecondUserRecLoaded = GetSession()->usernum;

	// ensure the entry point exists
	std::ostringstream path;
	path << GetMenuDirectory() << pszMenuDir << "main.mnu";
	return WFile::Exists(path.str());
}



bool LoadMenuSetup( int nUserNum )
{
	if (!pSecondUserRec)
    {
		MenuSysopLog("Mem Error");
		return false;
	}
	UnloadMenuSetup();


	if (!nUserNum)
    {
		return false;
    }

    WFile userConfig( syscfg.datadir, CONFIG_USR );
    if ( userConfig.Exists() )
    {
        WUser user;
        GetApplication()->GetUserManager()->ReadUser( &user, nUserNum );
        if ( userConfig.Open( WFile::modeReadOnly | WFile::modeBinary ) )
        {
            userConfig.Seek( nUserNum * sizeof(user_config), WFile::seekBegin );

            int len = userConfig.Read( pSecondUserRec, sizeof( user_config ) );
            userConfig.Close();

			if ( len != sizeof( user_config ) ||
                 !wwiv::stringUtils::IsEqualsIgnoreCase( reinterpret_cast<char*>( pSecondUserRec->name ),
                                                         user.GetName() ) )
            {
				memset(pSecondUserRec, 0, sizeof(user_config));
				strcpy( reinterpret_cast<char*>( pSecondUserRec->name ), user.GetName() );
				return 0;
			}
			nSecondUserRecLoaded = nUserNum;

			return true;
		}
	}
	return false;
}


void WriteMenuSetup(int nUserNum)
{
	if (!nUserNum)
    {
		return;
    }

    WUser user;
    GetApplication()->GetUserManager()->ReadUser( &user, nUserNum );
    strcpy(pSecondUserRec->name, user.GetName() );

    WFile userConfig( syscfg.datadir, CONFIG_USR );
    if ( !userConfig.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile ) )
    {
		return;
    }

    userConfig.Seek( nUserNum * sizeof(user_config), WFile::seekBegin );
    userConfig.Write( pSecondUserRec, sizeof( user_config ) );
    userConfig.Close();
}


void UnloadMenuSetup()
{
	nSecondUserRecLoaded = 0;
	memset(pSecondUserRec, 0, sizeof(user_config));
}

void GetCommand(MenuInstanceData * pMenuData, char *pszBuf)
{
	if (pSecondUserRec->cHotKeys == HOTKEYS_ON)
    {
		if (pMenuData->header.nNumbers == MENU_NUMFLAG_DIRNUMBER)
        {
			GetSession()->SetMMKeyArea( WSession::mmkeyFileAreas );
			write_inst(INST_LOC_XFER,udir[GetSession()->GetCurrentFileArea()].subnum,INST_FLAGS_NONE);
			strcpy(pszBuf, mmkey( 1 ));
		}
        else if (pMenuData->header.nNumbers == MENU_NUMFLAG_SUBNUMBER)
        {
			GetSession()->SetMMKeyArea( WSession::mmkeyMessageAreas );
			write_inst(INST_LOC_MAIN,usub[GetSession()->GetCurrentMessageArea()].subnum,INST_FLAGS_NONE);
			strcpy(pszBuf, mmkey( 0 ));
		}
        else
        {
			odc[0] = '/';
			odc[1] = 0;
			strcpy(pszBuf, mmkey( 2 ));
		}
	}
    else
    {
		input( pszBuf, 50 );
    }
}


bool CheckMenuItemSecurity(MenuInstanceData * pMenuData, MenuRec * pMenu, bool bCheckPassword )
{
    // Looks like this is here just to keep a compiler warning away
    if (pMenuData != pMenuData)
    {
        pMenuData = pMenuData;
    }

    // if deleted, return as failed
    if ( ( pMenu->nFlags & MENU_FLAG_DELETED ) ||
         ( GetSession()->GetEffectiveSl() < pMenu->nMinSL ) ||
         ( GetSession()->GetEffectiveSl() > pMenu->iMaxSL && pMenu->iMaxSL != 0 ) ||
         ( GetSession()->GetCurrentUser()->GetDsl() < pMenu->nMinDSL ) ||
         ( GetSession()->GetCurrentUser()->GetDsl() > pMenu->iMaxDSL && pMenu->iMaxDSL != 0 ) )
    {
        return false;
    }

    int x;

    // All AR bits specified must match
    for (x = 0; x < 16; x++)
    {
        if (pMenu->uAR & (1 << x))
        {
            if ( !GetSession()->GetCurrentUser()->HasArFlag( 1 << x ) )
            {
                return false;
            }
        }
    }

    // All DAR bits specified must match
    for (x = 0; x < 16; x++)
    {
        if (pMenu->uDAR & (1 << x))
        {
            if ( !GetSession()->GetCurrentUser()->HasDarFlag( 1 << x ) )
            {
                return false;
            }
        }
    }

    // If any restrictions match, then they arn't allowed
    for (x = 0; x < 16; x++)
    {
        if (pMenu->uRestrict & (1 << x))
        {
            if ( GetSession()->GetCurrentUser()->HasRestrictionFlag( 1 << x ) )
            {
                return false;
            }
        }
    }

    if ( ( pMenu->nSysop && !so() ) ||
         ( pMenu->nCoSysop && !cs() ) )
    {
        return false;
    }

    if (pMenu->szPassWord[0] && bCheckPassword )
    {
        if ( !CheckMenuPassword( pMenu->szPassWord ) )
        {
            return false;
        }
    }

    // If you made it past all of the checks
    // then you may execute the menu record
    return true;
}



void OpenMenuDescriptions()
{
	std::ostringstream name;
	name << GetMenuDirectory() << DESCRIPT_ION;
	hMenuDesc = fopen(name.str().c_str(), "r");
}

void CloseMenuDescriptions()
{
	if (hMenuDesc)
    {
		fclose(hMenuDesc);
    }

	hMenuDesc = NULL;
}


char *GetMenuDescription(const std::string& name, char *pszDesc )
{
	if (!hMenuDesc)
    {
		*pszDesc = 0;
		return NULL;
	}
	fseek(hMenuDesc, 0, SEEK_SET);

	char szLine[201];
	while (!hangup)
    {
		if (!fgets(szLine, 200, hMenuDesc))
        {
			*pszDesc = 0;
			return pszDesc;
		}
		char* pszTemp = strchr(szLine, ' ');
		if (!pszTemp)
        {
			continue;
        }

		pszTemp[0] = 0;
		++pszTemp;

		if ( wwiv::stringUtils::IsEqualsIgnoreCase( name.c_str(), szLine ) )
        {
			strcpy(pszDesc, pszTemp);
			int x = strlen(pszDesc);
			--x;

			if (x > 55)
            {
				x = 54;
				pszDesc[x + 1] = 0;
			}
			if (x >= 0 && isspace(pszDesc[x]))
            {
				while (x > 0 && isspace(pszDesc[x]) && !hangup)
                {
					--x;
                }
				if (!isspace(pszDesc[x]))
                {
					++x;
                }
				pszDesc[x] = 0;
			}
			return pszDesc;
		}
	}
	return pszDesc;
}


void SetMenuDescription(const char *pszName, const char *pszDesc)
{
	char szLine[MAX_PATH], szTok[26];
	int bWritten = 0;
	bool bMenuOpen = false;

	if (!hMenuDesc)
    {
		bMenuOpen = false;
		OpenMenuDescriptions();
	}
    else
    {
		bMenuOpen = true;
    }

    WTextFile tempDescriptionFile( GetMenuDirectory(), TEMP_ION, "wt" );

    if (!tempDescriptionFile.IsOpen())
    {
		MenuSysopLog("Unable to write description");
		return;
	}
	if (hMenuDesc)
    {
		fseek(hMenuDesc, 0, SEEK_SET);

		while (!hangup)
        {
			if (!fgets(szLine, 200, hMenuDesc))
            {
				break;
            }

			stptok(szLine, szTok, 25, " ");

			if ( wwiv::stringUtils::IsEqualsIgnoreCase( pszName, szTok ) )
            {
                tempDescriptionFile.WriteFormatted( "%s %s\n", pszName, pszDesc );
				bWritten = 1;
			}
            else
            {
                tempDescriptionFile.WriteFormatted( "%s", szLine );
            }
		}
	}
	if (!bWritten)
    {
        tempDescriptionFile.WriteFormatted( "%s %s\n", pszName, pszDesc );
    }

    tempDescriptionFile.Close();

	CloseMenuDescriptions();

    WFile descriptionFile( GetMenuDirectory(), DESCRIPT_ION );
    WFile::Remove( descriptionFile.GetFullPathName() );
    WFile::Rename( tempDescriptionFile.GetFullPathName(), descriptionFile.GetFullPathName() );

	if (bMenuOpen)
    {
		OpenMenuDescriptions();
    }
}


const char *GetMenuDirectory()
{
	static char szMenuDir[MAX_PATH];
	sprintf(szMenuDir, "%smenus%c", GetSession()->pszLanguageDir, WWIV_FILE_SEPERATOR_CHAR);
	return szMenuDir;
}


void GenerateMenu(MenuInstanceData * pMenuData)
{
	MenuRec menu;
	int x, iDisplayed = 0;
	char szKey[30];

	memset(&menu, 0, sizeof(MenuRec));

	GetSession()->bout.Color( 0 );
	GetSession()->bout.NewLine();

	if (pMenuData->header.nNumbers != MENU_NUMFLAG_NOTHING)
    {
		GetSession()->bout.WriteFormatted("|#1%-8.8s  |#2%-25.25s  ", "[#]", "Change Sub/Dir #");
		++iDisplayed;
	}
	for (x = 0; x < pMenuData->nAmountRecs - 1; x++)
    {
		if ((pMenuData->index[x].nFlags & MENU_FLAG_DELETED) == 0)
        {
			if (pMenuData->index[x].nRec != 0)
            {
                // Dont include control record
				pMenuData->pMenuFile->Seek( pMenuData->index[x].nRec * sizeof( MenuRec ), WFile::seekBegin );
				pMenuData->pMenuFile->Read( &menu, sizeof( MenuRec ) );

				if ( CheckMenuItemSecurity(pMenuData, &menu, 0) &&
                     menu.nHide != MENU_HIDE_REGULAR &&
                     menu.nHide != MENU_HIDE_BOTH )
                {
					if (strlen(menu.szKey) > 1 && menu.szKey[0] != '/' && pSecondUserRec->cHotKeys == HOTKEYS_ON)
                    {
						sprintf(szKey, "//%s", menu.szKey);
                    }
					else
                    {
						sprintf(szKey, "[%s]", menu.szKey);
                    }

					GetSession()->bout.WriteFormatted("|#1%-8.8s  |#2%-25.25s  ", szKey, menu.szMenuText[0] ? menu.szMenuText : menu.szExecute);

					if (iDisplayed % 2)
                    {
						GetSession()->bout.NewLine();
                    }

					++iDisplayed;
				}
			}
		}
	}
	if ( wwiv::stringUtils::IsEquals( GetSession()->GetCurrentUser()->GetName(), "GUEST" ) )
    {
		if ( iDisplayed % 2 )
        {
			GetSession()->bout.NewLine();
        }
		GetSession()->bout.WriteFormatted( "|#1%-8.8s  |#2%-25.25s  ",
			     pSecondUserRec->cHotKeys == HOTKEYS_ON ? "//APPLY" : "[APPLY]",
                 "Guest Account Application");
		++iDisplayed;
	}
	GetSession()->bout.NewLine( 2 );
	return;
}


// MenuParseLine(szSrc, szCmd, szParam1, szParam2)
//
// szSrc    = Line to parse
// szCmd    = Returns the command to be executed
// szParam1 = 1st parameter - if it exists
// szParam2 = 2cd paramater - if it exists
//
// return value is where to continue parsing this line
//
// szSrc to be interpreted can be formated like this
// either  cmd param1 param2
// or     cmd(param1, param2)
//   multiple lines are seperated with the ~ key
//   enclose multi word text in quotes
//
char *MenuParseLine(char *pszSrc, char *pszCmd, char *pszParam1, char *pszParam2)
{
	char *porig = pszSrc;

	pszCmd[0] = 0;
	pszParam1[0] = 0;
	pszParam2[0] = 0;

	pszSrc = MenuSkipSpaces(pszSrc);
	while (pszSrc[0] == '~' && !hangup)
    {
		++pszSrc;
		pszSrc = MenuSkipSpaces(pszSrc);
	}

	int nLen = 0;
	while (isalnum(*pszSrc) && !hangup)
    {
		if (nLen < 30)
        {
			pszCmd[nLen++] = *pszSrc;
        }
		++pszSrc;
	}
	pszCmd[nLen] = 0;

	pszSrc = MenuSkipSpaces(pszSrc);

	bool bParen = false;
	if (*pszSrc == '(')
    {
		++pszSrc;
		bParen = true;
		pszSrc = MenuSkipSpaces(pszSrc);
	}
	pszSrc = MenuGetParam(pszSrc, pszParam1);
	pszSrc = MenuSkipSpaces(pszSrc);

	if ( bParen )
    {
		pszSrc = MenuDoParenCheck(pszSrc, 1, porig);
    }

	pszSrc = MenuGetParam(pszSrc, pszParam2);
	pszSrc = MenuSkipSpaces(pszSrc);

	if ( bParen )
    {
		pszSrc = MenuDoParenCheck(pszSrc, 0, porig);
    }

	pszSrc = MenuSkipSpaces(pszSrc);

	if (pszSrc[0] != 0 && pszSrc[0] != '~')
    {
		MenuSysopLog("Expected EOL");
		MenuSysopLog(porig);
	}
	return pszSrc;
}



char *MenuDoParenCheck(char *pszSrc, int bMore, char *porig)
{
    if (pszSrc[0] == ',')
    {
        if (bMore == 0)
        {
            MenuSysopLog("Too many paramaters in line of code");
            MenuSysopLog(porig);
        }
        ++pszSrc;
        pszSrc = MenuSkipSpaces(pszSrc);
    }
    else if (pszSrc[0] == ')')
    {
        ++pszSrc;
        pszSrc = MenuSkipSpaces(pszSrc);
        if (pszSrc[0] != 0 && pszSrc[0] != '~')
        {
            MenuSysopLog("Invalid Code, exptected EOL after close parentheses");
            MenuSysopLog(porig);
        }
    }
    else if (pszSrc[0] == 0 || pszSrc[0] == '~')
    {
        MenuSysopLog("Unexpected end of line (wanted a close parentheses)");
    }

    return pszSrc;
}


char *MenuGetParam(char *pszSrc, char *pszParam)
{
    pszSrc = MenuSkipSpaces(pszSrc);
    pszParam[0] = 0;

    if (pszSrc[0] == 0 || pszSrc[0] == '~')
    {
        return pszSrc;
    }

    int nLen = 0;
    if (*pszSrc == '"')
    {
        ++pszSrc;
        nLen = 0;
        while (*pszSrc != '"' && *pszSrc != 0)
        {
            if (nLen < 50)
            {
                pszParam[nLen++] = *pszSrc;
            }
            ++pszSrc;
        }
        if (*pszSrc)                             // We should either be on NULL or the CLOSING QUOTE
        {
            ++pszSrc;                              // If on the Quote, advance one
        }
        pszParam[nLen] = '\0';
    }
    else
    {
        nLen = 0;
        while (*pszSrc != ' ' && *pszSrc != 0 && *pszSrc != '~')
        {
            if (nLen < 50)
            {
                pszParam[nLen++] = *pszSrc;
            }
            ++pszSrc;
        }
        pszParam[nLen] = '\0';
    }
    return pszSrc;
}


char *MenuSkipSpaces(char *pszSrc)
{
    while ( isspace(pszSrc[0]) && pszSrc[0] != '\0' )
    {
        ++pszSrc;
    }
    return pszSrc;
}


void InterpretCommand( MenuInstanceData * pMenuData, const char *pszScript )
{
    char szCmd[31], szParam1[51], szParam2[51];
    char szTempScript[ 255 ];
    memset( szTempScript, 0, sizeof( szTempScript ) );
    strncpy( szTempScript, pszScript, 250 );

    if (pszScript[0] == 0)
    {
        return;
    }

    char* pszScriptPointer = szTempScript;
    while (pszScriptPointer && !hangup)
    {
        pszScriptPointer = MenuParseLine(pszScriptPointer, szCmd, szParam1, szParam2);

        if ( szCmd[0] == 0 )    // || !pszScriptPointer || !*pszScriptPointer
        {
            break;
        }

        // -------------------------
        // Run a new menu instance

        int nCmdID = GetMenuIndex( szCmd );
        switch ( nCmdID )
        {
        case 0:
            {   // "MENU"
                // Spawn a new menu
                MenuInstanceData *pNewMenuData = static_cast<MenuInstanceData *>( bbsmalloc( sizeof( MenuInstanceData ) ) );

                memset(pNewMenuData, 0, sizeof(MenuInstanceData));
                //pNewMenuData->hMenuFile = -1;
                pNewMenuData->nFinished = 0;
                pNewMenuData->nReload = 0;

                Menus(pNewMenuData, pMenuData->szPath, szParam1);
                BbsFreeMemory(pNewMenuData);
            } break;
        case 1:
            {
                // -------------------------
                // Exit out of this instance
                // of the menu
                // -------------------------
                // "ReturnFromMenu"
                InterpretCommand(pMenuData, pMenuData->header.szExitScript);
                pMenuData->nFinished = 1;
            } break;
        case 2:
            { // "EditMenuSet"
                EditMenus();           // flag if we are editing this menu
                pMenuData->nFinished = 1;
                pMenuData->nReload = 1;
            }
            break;
        case 3:
            {   // "DLFreeFile"
                align(szParam2);
                MenuDownload(szParam1, szParam2, 1, 1);
            } break;
        case 4:
            {   // "DLFile"
                align(szParam2);
                MenuDownload(szParam1, szParam2, 0, 1);
            } break;
        case 5:
            {   // "RunDoor"
                MenuRunDoorName(szParam1, 0);
            } break;
        case 6:
            {   // "RunDoorFree"
                MenuRunDoorName(szParam1, 0);
            } break;
        case 7:
            {   // "RunDoorNumber"
                int nTemp = atoi(szParam1);
                MenuRunDoorNumber(nTemp, 0);
            } break;
        case 8:
            {   // "RunDoorNumberFree"
                int nTemp = atoi(szParam1);
                MenuRunDoorNumber(nTemp, 1);
            } break;
        case 9:
            {   // "PrintFile"
                printfile(szParam1, 1);
            } break;
        case 10:
            {  // "PrintFileNA"
                printfile(szParam1, 0);
            } break;
        case 11:
            {  // "SetSubNumber"
                SetSubNumber(szParam1);
            } break;
        case 12:
            {  // "SetDirNumber"
                SetDirNumber(szParam1);
            } break;
        case 13:
            {  // "SetMsgConf"
                SetMsgConf(szParam1[0]);
            } break;
        case 14:
            {  // "SetDirConf"
                SetDirConf(szParam1[0]);
            } break;
        case 15:
            {  // "EnableConf"
                EnableConf();
            } break;
        case 16:
            {  // "DisableConf"
                DisableConf();
            } break;
        case 17:
            {  // "Pause"
                pausescr();
            } break;
        case 18:
            {  // "ConfigUserMenuSet"
                ConfigUserMenuSet();
                pMenuData->nFinished = 1;
                pMenuData->nReload = 1;
            } break;
        case 19:
            {  // "DisplayHelp"
                if ( GetSession()->GetCurrentUser()->IsExpert() )
                {
                    AMDisplayHelp( pMenuData );
                }
            } break;
        case 20:
            {  // "SelectSub"
                ChangeSubNumber();
            } break;
        case 21:
            {  // "SelectDir"
                ChangeDirNumber();
            } break;
        case 22:
            {  // "SubList"
                SubList();
            } break;
        case 23:
            {  // "UpSubConf"
                UpSubConf();
            } break;
        case 24:
            {  // "DownSubConf"
                DownSubConf();
            } break;
        case 25:
            {  // "UpSub"
                UpSub();
            } break;
        case 26:
            {  // "DownSub"
                DownSub();
            } break;
        case 27:
            {  // "ValidateUser"
                ValidateUser();
            } break;
        case 28:
            {  // "Doors"
                Chains();
            } break;
        case 29:
            {  // "TimeBank"
                TimeBank();
            } break;
        case 30:
            {  // "AutoMessage"
                AutoMessage();
            } break;
        case 31:
            {  // "BBSList"
                BBSList();
            } break;
        case 32:
            {  // "RequestChat"
                RequestChat();
            } break;
        case 33:
            {  // "Defaults"
                Defaults(pMenuData);
            } break;
        case 34:
            {  // "SendEMail"
                SendEMail();
            } break;
        case 35:
            {  // "Feedback"
                FeedBack();
            } break;
        case 36:
            {  // "Bulletins"
                Bulletins();
            } break;
        case 37:
            {  // "HopSub"
                HopSub();
            } break;
        case 38:
            {  // "SystemInfo"
                SystemInfo();
            } break;
        case 39:
            {  // "JumpSubConf"
                JumpSubConf();
            } break;
        case 40:
            {  // "KillEMail"
                KillEMail();
            } break;
        case 41:
            {  // "LastCallers"
                LastCallers();
            } break;
        case 42:
            {  // "ReadEMail"
                ReadEMail();
            } break;
        case 43:
            {  // "NewMessageScan"
                NewMessageScan();
            } break;
        case 44:
            {  // "Goodbye"
                GoodBye();
            } break;
        case 45:
            {  // "PostMessage"
                WWIV_PostMessage();
            } break;
        case 46:
            {  // "NewMsgScanCurSub"
                ScanSub();
            } break;
        case 47:
            {  // "RemovePost"
                RemovePost();
            } break;
        case 48:
            {  // "TitleScan"
                TitleScan();
            } break;
        case 49:
            {  // "ListUsers"
                ListUsers();
            } break;
        case 50:
            {  // "Vote"
                Vote();
            } break;
        case 51:
            {  // "ToggleExpert"
                ToggleExpert();
            } break;
        case 52:
            {  // "YourInfo"
                YourInfo();
            } break;
        case 53:
            {  // "ExpressScan"
                ExpressScan();
            } break;
        case 54:
            {  // "WWIVVer"
                WWIVVersion();
            } break;
        case 55:
            {  // "InstanceEdit"
                InstanceEdit();
            } break;
        case 56:
            {  // "ConferenceEdit"
                JumpEdit();
            } break;
        case 57:
            {  // "SubEdit"
                BoardEdit();
            } break;
        case 58:
            {  // "ChainEdit"
                ChainEdit();
            } break;
        case 59:
            {  // "ToggleAvailable"
                ToggleChat();
            } break;
        case 60:
            {  // "ChangeUser"
                ChangeUser();
            } break;
        case 61:
            {  // "CLOUT"
                CallOut();
            } break;
        case 62:
            {  // "Debug"
                Debug();
            } break;
        case 63:
            {  // "DirEdit"
                DirEdit();
            } break;
        case 65:
            {  // "Edit"
                EditText();
            } break;
        case 66:
            {  // "BulletinEdit"
                EditBulletins();
            } break;
        case 67:
            {
				// "LoadText"
				// LoadText and LoadTextFile are the same, so they are now merged.
                LoadTextFile();
            } break;
        case 68:
            {  // "ReadAllMail"
                ReadAllMail();
            } break;
        case 69:
            {  // "Reboot"
                RebootComputer();
            } break;
        case 70:
            {  // "ReloadMenus"
                ReloadMenus();
            } break;
        case 71:
            {  // "ResetUserIndex"
                ResetFiles();
            } break;
        case 72:
            {  // "ResetQScan"
                ResetQscan();
            } break;
        case 73:
            {  // "MemStat"
                MemoryStatus();
            } break;
        case 74:
            {  // "PackMsgs"
                PackMessages();
            } break;
        case 75:
            {  // "VoteEdit"
                InitVotes();
            } break;
        case 76:
            {  // "Log"
                ReadLog();
            } break;
        case 77:
            {  // "NetLog"
                ReadNetLog();
            } break;
        case 78:
            {  // "Pending"
                PrintPending();
            } break;
        case 79:
            {  // "Status"
                PrintStatus();
            } break;
        case 80:
            {  // "TextEdit"
                TextEdit();
            } break;
        case 81:
            {  // "UserEdit"
                UserEdit();
            } break;
        case 82:
            {  // "VotePrint"
                VotePrint();
            } break;
        case 83:
            {  // "YLog"
                YesturdaysLog();
            } break;
        case 84:
            {  // "ZLog"
                ZLog();
            } break;
        case 85:
            {  // "ViewNetDataLog"
                ViewNetDataLog();
            } break;
        case 86:
            {  // "UploadPost"
                UploadPost();
            } break;
        case 87:
            {  // "ClearScreen"
                GetSession()->bout.ClearScreen();
            } break;
        case 88:
            {  // "NetListing"
                NetListing();
            } break;
        case 89:
            {  // "WHO"
                WhoIsOnline();
            } break;
        case 90:
            {  // /A "NewMsgsAllConfs"
                NewMsgsAllConfs();
            } break;
        case 91:
            {  // /E "MultiEMail"
                MultiEmail();
            } break;
        case 92:
            {  // "NewMsgScanFromHere"
                NewMsgScanFromHere();
            } break;
        case 93:
            {  // "ValidatePosts"
                ValidateScan();
            } break;
        case 94:
            {  // "ChatRoom"
                ChatRoom();
            } break;
        case 95:
            {  // "DownloadPosts"
                DownloadPosts();
            } break;
        case 96:
            {  // "DownloadFileList"
                DownloadFileList();
            } break;
        case 97:
            {  // "ClearQScan"
                ClearQScan();
            } break;
        case 98:
            {  // "FastGoodBye"
                FastGoodBye();
            } break;
        case 99:
            {  // "NewFilesAllConfs"
                NewFilesAllConfs();
            } break;
        case 100:
            { // "ReadIDZ"
                ReadIDZ();
            } break;
        case 101:
            { // "UploadAllDirs"
                UploadAllDirs();
            } break;
        case 102:
            { // "UploadCurDir"
                UploadCurDir();
            } break;
        case 103:
            { // "RenameFiles"
                RenameFiles();
            } break;
        case 104:
            { // "MoveFiles"
                MoveFiles();
            } break;
        case 105:
            { // "SortDirs"
                SortDirs();
            } break;
        case 106:
            { // "ReverseSortDirs"
                ReverseSort();
            } break;
        case 107:
            { // "AllowEdit"
                AllowEdit();
            } break;
        case 109:
            { // "UploadFilesBBS"
                UploadFilesBBS();
            } break;
        case 110:
            { // "DirList"
                DirList();
            } break;
        case 111:
            { // "UpDirConf"
                UpDirConf();
            } break;
        case 112:
            { // "UpDir"
                UpDir();
            } break;
        case 113:
            { // "DownDirConf"
                DownDirConf();
            } break;
        case 114:
            { // "DownDir"
                DownDir();
            } break;
        case 115:
            { // "ListUsersDL"
                ListUsersDL();
            } break;
        case 116:
            { // "PrintDSZLog"
                PrintDSZLog();
            } break;
        case 117:
            { // "PrintDevices"
                PrintDevices();
            } break;
        case 118:
            { // "ViewArchive"
                ViewArchive();
            } break;
        case 119:
            { // "BatchMenu"
                BatchMenu();
            } break;
        case 120:
            { // "Download"
                Download();
            } break;
        case 121:
            { // "TempExtract"
                TempExtract();
            } break;
        case 122:
            { // "FindDescription"
                FindDescription();
            } break;
        case 123:
            { // "ArchiveMenu"
                TemporaryStuff();
            } break;
        case 124:
            { // "HopDir"
                HopDir();
            } break;
        case 125:
            { // "JumpDirConf"
                JumpDirConf();
            } break;
        case 126:
            { // "ListFiles"
                ListFiles();
            } break;
        case 127:
            { // "NewFileScan"
                NewFileScan();
            } break;
        case 128:
            { // "SetNewFileScanDate"
                SetNewFileScanDate();
            } break;
        case 129:
            { // "RemoveFiles"
                RemoveFiles();
            } break;
        case 130:
            { // "SearchAllFiles"
                SearchAllFiles();
            } break;
        case 131:
            { // "XferDefaults"
                XferDefaults();
            } break;
        case 132:
            { // "Upload"
                Upload();
            } break;
        case 133:
            { // "YourInfoDL"
                YourInfoDL();
            } break;
        case 134:
            { // "UploadToSysop"
                UploadToSysop();
            } break;
        case 135:
            { // "ReadAutoMessage"
                ReadAutoMessage();
            } break;
        case 136:
            { // "SetNewScanMsg"
                SetNewScanMsg();
            } break;
        case 137:
            { // "ReadMessages"
                ReadMessages();
            } break;
            /*
            case 138:
            { // "RUN"
            ExecuteBasic(szParam1);
            } break;
            */
        case 139:
            { // "EventEdit"
                EventEdit();
            } break;
        case 140:
            { // "LoadTextFile"
                LoadTextFile();
            } break;
        case 141:
            { // "GuestApply"
                GuestApply();
            } break;
        case 142:
            { // "ConfigFileList"
                ConfigFileList();
            } break;
        case 143:
            { // "ListAllColors"
                ListAllColors();
            } break;
#ifdef QUESTIONS
        case 144:
            { // "EditQuestions"
                EditQuestions();
            } break;
        case 145:
            { // "Questions"
                Questions();
            } break;
#endif
        case 146:
            { // "RemoveNotThere"
                RemoveNotThere();
            } break;
        case 147:
            { // "AttachFile"
                AttachFile();
            } break;
        case 148:
            { // "InternetEmail"
                InternetEmail();
            } break;
        case 149:
            { // "UnQScan"
                UnQScan();
            } break;
            // ppMenuStringsIndex[150] thru ppMenuStringsIndex[153] not used.....
        case 154:
            { // "Packers"
                Packers();
            } break;
        case 155:
            { // Color_Config
                color_config();
            } break;
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
        case 158:
            { // InitVotes
                InitVotes();
            } break;
        case 161:
            { // TurnMCIOn
                TurnMCIOn();
            } break;
        case 162:
            { // TurnMCIOff
                TurnMCIOff();
            } break;
        default:
            {
                MenuSysopLog("The following command was not recognized");
                MenuSysopLog(szCmd);
            } break;
        }
    }
}


