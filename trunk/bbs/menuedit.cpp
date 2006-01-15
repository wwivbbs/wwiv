/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2006, WWIV Software Services             */
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


void EditMenus()
{
	char szFile[MAX_PATH];
	char szDirectoryName[15], szMenu[15];
	char szMenuDir[MAX_PATH];
	char szTemp1[21];
	char szPW[21];
	char szDesc[81];
	int nAmount = 0;
	MenuHeader header;
	MenuRec Menu;


	GetSession()->bout.ClearScreen();
	GetSession()->bout << "|#2WWIV Menu Editor|#0\r\n";

	if (!GetMenuDir(szDirectoryName))
	{
		return;
	}

	if (!GetMenuMenu(szDirectoryName, szMenu))
	{
		return;
	}

	sprintf(szFile, "%s%s%c%s.mnu", MenuDir(szMenuDir), szDirectoryName,
		WWIV_FILE_SEPERATOR_CHAR, szMenu);

	WFile fileEditMenu( szFile );
	if (!WFile::Exists(szFile))
	{
		GetSession()->bout << "Creating menu...\r\n";
		if ( !fileEditMenu.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareDenyNone, WFile::permReadWrite ) )
		{
			GetSession()->bout << "Unable to create menu.\r\n";
			return;
		}
		memset(&header, 0, sizeof(MenuHeader));
		memset(&Menu, 0, sizeof(MenuRec));
		strcpy(header.szSig, "WWIV430");

		header.nVersion = MENU_VERSION;
		header.nFlags = MENU_FLAG_MAINMENU;

		header.nHeadBytes = sizeof(MenuHeader);
		header.nBodyBytes = sizeof(MenuRec);

		/* ---------------------------------------- */
		/* Copy header into menu and write the menu */
		/* to ensure the record is 0 100% 0 filled  */
		/* ---------------------------------------- */
		memmove(&Menu, &header, sizeof(MenuHeader));
		fileEditMenu.Write( &Menu, sizeof( MenuRec ) );
		nAmount = 0;
	}
	else
	{
		if ( !fileEditMenu.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareDenyNone, WFile::permReadWrite ) )
		{
			MenuSysopLog("Unable to open menu.");
			MenuSysopLog(szFile);
			return;
		}
		nAmount = static_cast<INT16>(fileEditMenu.GetLength() / sizeof(MenuRec));
		--nAmount;
		if (nAmount < 0)
		{
			MenuSysopLog("Menu is corrupt.");
			MenuSysopLog(szFile);
			return;
		}
	}
	int nCur = 0;

	// read first record
	fileEditMenu.Seek( nCur * sizeof( MenuRec ), WFile::seekBegin );
	fileEditMenu.Read( &Menu, sizeof( MenuRec ) );

	bool done = false;
	while (!hangup && !done)
	{
		if (nCur == 0)
		{
			DisplayHeader((MenuHeader *) (&Menu), nCur, nAmount, szDirectoryName);
			char chKey = onek("Q[]Z012ABCDEFGHIJKLMNOP");
			switch (chKey)
			{
			case 'Q':
				WriteMenuRec(fileEditMenu, &Menu, nCur);
				done = true;
				break;
			case '[':
				WriteMenuRec(fileEditMenu, &Menu, nCur);
				nAmount = static_cast<INT16>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
				--nCur;
				if (nCur < 0)
				{
					nCur = nAmount;
				}
				ReadMenuRec(fileEditMenu, &Menu, nCur);
				break;
			case ']':
				WriteMenuRec(fileEditMenu, &Menu, nCur);
				nAmount = static_cast<INT16>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
				++nCur;
				if (nCur > nAmount)
				{
					nCur = 0;
				}
				ReadMenuRec(fileEditMenu, &Menu, nCur);
				break;
			case 'Z':
				WriteMenuRec(fileEditMenu, &Menu, nCur);
				memset(&Menu, 0, sizeof(MenuRec));

				nAmount = static_cast<INT16>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;

				nCur = nAmount + 1;
				memset(&Menu, 0, sizeof(MenuRec));
				Menu.iMaxSL = 255;
				Menu.iMaxDSL = 255;

				WriteMenuRec(fileEditMenu, &Menu, nCur);
				nAmount = static_cast<INT16>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
				ReadMenuRec(fileEditMenu, &Menu, nCur);
				break;
			case '0':
				OpenMenuDescriptions();
				GetMenuDescription(szDirectoryName, szDesc);

				GetSession()->bout << "|#5New desc     : ";
				GetSession()->bout.Color( 0 );
				inputl(szDesc, 60);
				if (szDesc[0])
				{
					SetMenuDescription(szDirectoryName, szDesc);
				}
				CloseMenuDescriptions();
				break;
			case '1':
				GetSession()->bout << "Is menu deleted? (N) ";
				if (yesno())
				{
					((MenuHeader *) (&Menu))->nFlags |= MENU_FLAG_DELETED;
				}
				else
				{
					((MenuHeader *) (&Menu))->nFlags &= ~MENU_FLAG_DELETED;
				}
				break;
			case '2':
				GetSession()->bout << "Is menu a main menu? (Y) ";
				if (noyes())
				{
					((MenuHeader *) (&Menu))->nFlags |= MENU_FLAG_MAINMENU;
				}
				else
				{
					((MenuHeader *) (&Menu))->nFlags &= ~MENU_FLAG_MAINMENU;
				}
				break;
			case 'A':
				((MenuHeader *) (&Menu))->nNumbers++;
				if (((MenuHeader *) (&Menu))->nNumbers == MENU_NUMFLAG_LAST)
				{
					((MenuHeader *) (&Menu))->nNumbers = 0;
				}
				break;
			case 'B':
				((MenuHeader *) (&Menu))->nLogging++;
				if (((MenuHeader *) (&Menu))->nLogging == MENU_LOGTYPE_LAST)
				{
					((MenuHeader *) (&Menu))->nLogging = 0;
				}
				break;
			case 'C':
				((MenuHeader *) (&Menu))->nForceHelp++;
				if (((MenuHeader *) (&Menu))->nForceHelp == MENU_HELP_LAST)
				{
					((MenuHeader *) (&Menu))->nForceHelp = 0;
				}
				break;
			case 'D':
				((MenuHeader *) (&Menu))->nAllowedMenu++;
				if (((MenuHeader *) (&Menu))->nAllowedMenu == MENU_ALLOWED_LAST)
				{
					((MenuHeader *) (&Menu))->nAllowedMenu = 0;
				}
				break;
			case 'E':
				GetSession()->bout << "Pulldown menu title : ";
				inputl(((MenuHeader *) (&Menu))->szMenuTitle, 20);
				break;
			case 'F':
				GetSession()->bout << "Command to execute : ";
				inputl(((MenuHeader *) (&Menu))->szScript, 100);
				break;
			case 'G':
				GetSession()->bout << "Script for when menu ends : ";
				inputl(((MenuHeader *) (&Menu))->szExitScript, 100);
				break;
			case 'H':
				GetSession()->bout << "Min SL : ";
				input(szTemp1, 3);
				if (szTemp1[0])
				{
                    ((MenuHeader *) (&Menu))->nMinSL = wwiv::stringUtils::StringToShort(szTemp1);
				}
				break;
			case 'I':
				GetSession()->bout << "Min DSL : ";
				input(szTemp1, 3);
				if (szTemp1[0])
				{
					((MenuHeader *) (&Menu))->nMinDSL = wwiv::stringUtils::StringToShort(szTemp1);
				}
				break;
			case 'J':
				GetSession()->bout << "AR : ";
				input(szTemp1, 5);
				if (szTemp1[0])
				{
                    ((MenuHeader *) (&Menu))->uAR = wwiv::stringUtils::StringToUnsignedShort(szTemp1);
				}
				break;
			case 'K':
				GetSession()->bout << "DAR : ";
				input(szTemp1, 5);
				if (szTemp1[0])
				{
					((MenuHeader *) (&Menu))->uDAR = wwiv::stringUtils::StringToUnsignedShort(szTemp1);
				}
				break;
			case 'L':
				GetSession()->bout << "Restrictions : ";
				input(szTemp1, 5);
				if (szTemp1[0])
				{
					((MenuHeader *) (&Menu))->uRestrict = wwiv::stringUtils::StringToUnsignedShort(szTemp1);
				}
				break;
			case 'M':
				((MenuHeader *) (&Menu))->nSysop = !((MenuHeader *) (&Menu))->nSysop;
				break;
			case 'N':
				((MenuHeader *) (&Menu))->nCoSysop = !((MenuHeader *) (&Menu))->nCoSysop;
				break;
			case 'O':
				if (incom && ((MenuHeader *) (&Menu))->szPassWord[0])
				{
					GetSession()->bout << "Current PW: ";
					input(szPW, 20);
					if ( !wwiv::stringUtils::IsEqualsIgnoreCase(szPW, ((MenuHeader *) (&Menu))->szPassWord ) )
					{
						MenuSysopLog("Unable to change PW");
						break;
					}
				}
				GetSession()->bout << "   New PW : ";
				input(((MenuHeader *) (&Menu))->szPassWord, 20);
				break;
			case 'P':
				EditPulldownColors(((MenuHeader *) (&Menu)));
				break;
			}
		}
		else
		{
			DisplayItem(&Menu, nCur, nAmount);
			char chKey = onek("Q[]Z1ABCDEFGKLMNOPRSTUVWX");

			switch ( chKey )
			{
			case 'Q':
				WriteMenuRec(fileEditMenu, &Menu, nCur);
				done = true;
				break;

			case '[':
				WriteMenuRec(fileEditMenu, &Menu, nCur);
				nAmount = static_cast<INT16>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
				--nCur;
				if (nCur < 0)
				{
					nCur = nAmount;
				}
				ReadMenuRec(fileEditMenu, &Menu, nCur);
				break;
			case ']':
				WriteMenuRec(fileEditMenu, &Menu, nCur);
				nAmount = static_cast<INT16>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
				++nCur;
				if (nCur > nAmount)
				{
					nCur = 0;
				}
				ReadMenuRec(fileEditMenu, &Menu, nCur);
				break;
			case 'Z':
				WriteMenuRec(fileEditMenu, &Menu, nCur);
				memset(&Menu, 0, sizeof(MenuRec));
				nAmount = static_cast<INT16>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
				nCur = nAmount + 1;
				memset(&Menu, 0, sizeof(MenuRec));
				Menu.iMaxSL = 255;
				Menu.iMaxDSL = 255;
				WriteMenuRec(fileEditMenu, &Menu, nCur);
				nAmount = static_cast<INT16>(fileEditMenu.GetLength() / sizeof(MenuRec)) - 1;
				ReadMenuRec(fileEditMenu, &Menu, nCur);
				break;
			case '1':
				GetSession()->bout << "Is record deleted? (N) ";
				if (yesno())
				{
					Menu.nFlags |= MENU_FLAG_DELETED;
				}
				else
				{
					Menu.nFlags &= ~MENU_FLAG_DELETED;
				}
				break;
			case 'A':
				GetSession()->bout << "Key to cause execution : ";
				input(Menu.szKey, MENU_MAX_KEYS);
				if (!(Menu.szSysopLog[0]))
				{
					strcpy(Menu.szSysopLog,Menu.szKey);
				}
				break;
			case 'B':
				GetSession()->bout << "Command to execute : ";
				inputl(Menu.szExecute, 100);
				if (!(Menu.szMenuText[0]))
				{
					strcpy(Menu.szMenuText, Menu.szExecute);
				}
				if (!(Menu.szPDText[0]))
				{
					strcpy(Menu.szPDText, Menu.szExecute);
				}
				break;
			case 'C':
				GetSession()->bout << "Menu Text : ";
				inputl(Menu.szMenuText, 40);
				if (!(Menu.szPDText[0]))
				{
					strcpy(Menu.szPDText, Menu.szMenuText);
				}
				break;
			case 'D':
				GetSession()->bout << "Pulldown Menu Text : ";
				inputl(Menu.szPDText, 40);
				break;
			case 'E':
				GetSession()->bout << "Help Text : ";
				inputl(Menu.szHelp, 80);
				break;
			case 'F':
				GetSession()->bout << "Instance Message : ";
				inputl(Menu.szInstanceMessage, 80);
				break;
			case 'G':
				GetSession()->bout << "Sysoplog Message : ";
				inputl(Menu.szSysopLog, 50);
				break;
			case 'K':
				GetSession()->bout << "Min SL : ";
				input(szTemp1, 3);
				if (szTemp1[0])
				{
					Menu.nMinSL = wwiv::stringUtils::StringToShort(szTemp1);
				}
				break;
			case 'L':
				GetSession()->bout << "Max SL : ";
				input(szTemp1, 3);
				if (szTemp1[0])
				{
					Menu.iMaxSL = wwiv::stringUtils::StringToShort(szTemp1);
				}
				break;
			case 'M':
				GetSession()->bout << "Min DSL : ";
				input(szTemp1, 3);
				if (szTemp1[0])
				{
					Menu.nMinDSL = wwiv::stringUtils::StringToShort(szTemp1);
				}
				break;
			case 'N':
				GetSession()->bout << "Max DSL : ";
				input(szTemp1, 3);
				if (szTemp1[0])
				{
					Menu.iMaxDSL = wwiv::stringUtils::StringToShort(szTemp1);
				}
				break;
			case 'O':
				GetSession()->bout << "AR : ";
				input(szTemp1, 5);
				if (szTemp1[0])
				{
					Menu.uAR = wwiv::stringUtils::StringToUnsignedShort(szTemp1);
				}
				break;
			case 'P':
				GetSession()->bout << "DAR : ";
				input(szTemp1, 5);
				if (szTemp1[0])
				{
					Menu.uDAR = wwiv::stringUtils::StringToUnsignedShort(szTemp1);
				}
				break;
			case 'R':
				GetSession()->bout << "Restrictions : ";
				input(szTemp1, 5);
				if (szTemp1[0])
				{
					Menu.uRestrict = wwiv::stringUtils::StringToUnsignedShort(szTemp1);
				}
				break;
			case 'S':
				Menu.nSysop = !Menu.nSysop;
				break;
			case 'T':
				Menu.nCoSysop = !Menu.nCoSysop;
				break;
			case 'U':
				if (incom && Menu.szPassWord[0])
				{
					GetSession()->bout << "Current PW: ";
					input(szPW, 20);
					if ( !wwiv::stringUtils::IsEqualsIgnoreCase( szPW, Menu.szPassWord ) )
					{
						MenuSysopLog("Unable to change PW");
						break;
					}
				}
				GetSession()->bout << "   New PW : ";
				input(Menu.szPassWord, 20);
				break;

			case 'V':
				++Menu.nHide;
				if (Menu.nHide >= MENU_HIDE_LAST)
				{
					Menu.nHide = MENU_HIDE_NONE;
				}
				break;
			case 'W':
				GetSession()->bout << "Clear screen before command is run? (Y) ";
				if (noyes())
				{
					Menu.nPDFlags &= ~PDFLAGS_NOCLEAR;
				}
				else
				{
					Menu.nPDFlags |= PDFLAGS_NOCLEAR;
				}

				GetSession()->bout << "Pause screen after command is run? (Y) ";
				if (noyes())
				{
					Menu.nPDFlags &= ~PDFLAGS_NOPAUSEAFTER;
				}
				else
				{
					Menu.nPDFlags |= PDFLAGS_NOPAUSEAFTER;
				}

				GetSession()->bout << "Restore screen after command is run? (Y) ";
				if (noyes())
				{
					Menu.nPDFlags &= ~PDFLAGS_NORESTORE;
				}
				else
				{
					Menu.nPDFlags |= PDFLAGS_NORESTORE;
				}
				break;
			case 'X':
				GetSession()->bout << "Filename for detailed help on item : ";
				input( Menu.szExtendedHelp, 12 );
				break;
			}
		}
	}
	GetApplication()->CdHome(); // make sure we are in the wwiv dir

	ReIndexMenu(fileEditMenu, szDirectoryName, szMenu);
	fileEditMenu.Close();
}


void ReIndexMenu(WFile &fileEditMenu, const char *pszDirectoryName, const char *pszMenuName)
{
	char szFile[MAX_PATH];
	char szMenuDir[MAX_PATH];
	int nRec;
	MenuRecIndex MenuIndex;
	MenuRec Menu;

	sprintf( szFile, "%s%s%c%s.idx", MenuDir(szMenuDir), pszDirectoryName, WWIV_FILE_SEPERATOR_CHAR, pszMenuName );

	GetSession()->bout << "Indexing Menu...\r\n";

	WFile fileIdx( szFile );
	if ( !fileIdx.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeTruncate|WFile::modeReadWrite, WFile::shareDenyWrite, WFile::permReadWrite ) )
	{
		GetSession()->bout << "Unable to reindex\r\n";
		pausescr();
		return;
	}
	int nAmount = static_cast<INT16>(fileEditMenu.GetLength() / sizeof(MenuRec));

	for (nRec = 1; nRec < nAmount; nRec++)
	{
		fileEditMenu.Seek( nRec * sizeof( MenuRec ), WFile::seekBegin );
		fileEditMenu.Read( &Menu, sizeof( MenuRec ) );

		memset(&MenuIndex, 0, sizeof(MenuRecIndex));
		MenuIndex.nRec = static_cast<short>( nRec );
		MenuIndex.nFlags = Menu.nFlags;
		strcpy(MenuIndex.szKey, Menu.szKey);

		fileIdx.Seek( (nRec - 1) * sizeof( MenuRecIndex ), WFile::seekBegin );
		fileIdx.Write( &MenuIndex, sizeof( MenuRecIndex ) );
	}

	fileIdx.Close();
}


void ReadMenuRec(WFile &fileEditMenu, MenuRec * Menu, int nCur)
{
	if ( fileEditMenu.Seek( nCur * sizeof(MenuRec), WFile::seekBegin ) != -1 )
	{
		fileEditMenu.Read( Menu, sizeof( MenuRec ) );
	}
}


void WriteMenuRec(WFile &fileEditMenu, MenuRec * Menu, int nCur)
{
	// %%TODO Add in locking (_locking) support via WIN32 file api's

	long lRet = fileEditMenu.Seek( nCur * sizeof( MenuRec ), WFile::seekBegin );
	if ( lRet == -1 )
	{
		return;
	}

	/*
	if (lock(iEditMenu, nCur * sizeof(MenuRec), sizeof(MenuRec)) != 0) {
	GetSession()->bout << "Unable to lock record for write\r\n";
	pausescr();
	return;
	}
	*/

	lRet = fileEditMenu.Write( Menu, sizeof( MenuRec ) );
	if ( lRet != sizeof( MenuRec ) )
	{
		return;
	}

	fileEditMenu.Seek( nCur * sizeof(MenuRec), WFile::seekBegin );

	/*
	unlock(iEditMenu, nCur * sizeof(MenuRec), sizeof(MenuRec));
	*/
}


bool GetMenuDir( char *pszBuffer )
{
	char szPath[MAX_PATH];
	char szMenuDir[MAX_PATH];

	ListMenuDirs();

	while (!hangup)
	{
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#9Enter menuset to edit ?=List : |#0";
		input(pszBuffer, 8);

		if (pszBuffer[0] == '?')
		{
			ListMenuDirs();
		}
		else if (pszBuffer[0] == 0)
		{
			return false;
		}
		else
		{
			sprintf(szPath, "%s%s", MenuDir(szMenuDir), pszBuffer);
			WFile dir(szPath);
			if (!dir.Exists())
			{
				GetSession()->bout << "The path " << szPath << wwiv::endl <<
					          "does not exist, create it? (Y) : ";
				if (noyes())
				{
					GetApplication()->CdHome();	// go to the wwiv dir
					WWIV_make_path(szPath);                    // Create the new path
					if (dir.Exists())
					{
						GetApplication()->CdHome();
						GetSession()->bout << "Created\r\n";
						return true;
					}
					else
					{
						GetApplication()->CdHome();
						GetSession()->bout << "Unable to create\r\n";
						return true;
					}
				}
				else
				{
					GetSession()->bout << "Not created\r\n";
					return true;
				}
			}
			GetApplication()->CdHome();
			return true;
		}
	}
    // The only way to get here is to hangup
	return false;
}


bool GetMenuMenu( const char *pszDirectoryName, char *pszBuffer )
{
	char szPath[MAX_PATH];
	char szMenuDir[MAX_PATH];
	int x;

	ListMenuMenus(pszDirectoryName);

	while (!hangup)
	{
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#9Enter menu file to edit ?=List : |#0";
		input(pszBuffer, 8);

		if (pszBuffer[0] == '?')
		{
			ListMenuMenus(pszDirectoryName);
		}
		else if (pszBuffer[0] == 0)
		{
			return false;
		}
		else
		{
			sprintf(  szPath, "%s%s%c%s.mnu", MenuDir(szMenuDir), pszDirectoryName,
				WWIV_FILE_SEPERATOR_CHAR, pszBuffer);
			if (!WFile::Exists(szPath))
			{
				GetSession()->bout << "File does not exist, create it? (yNq) : ";
				x = ynq();

				if (x == 'Q')
				{
					return false;
				}

				if (x == 'Y')
				{
					return true;
				}

				if (x == 'N')
				{
					continue;
				}
			} else
				return true;
		}
	}
    // The only way to get here is to hangup
    return false;
}



void DisplayItem(MenuRec * Menu, int nCur, int nAmount)
{
	GetSession()->bout.ClearScreen();

	GetSession()->bout << "|02(|#9" << nCur << "|02/|#9" << nAmount << "|02)" << wwiv::endl;

	if (nCur > 0 && nCur <= nAmount)
    {
		GetSession()->bout << "|#91) Deleted        : |#2" << ( Menu->nFlags & MENU_FLAG_DELETED ? "Yes" : "No " ) << wwiv::endl;
		GetSession()->bout << "|#9A) Key            : |#2" << Menu->szKey << wwiv::endl;
		GetSession()->bout << "|#9B) Command        : |#2" << Menu->szExecute << wwiv::endl;
		GetSession()->bout << "|#9C) Menu Text      : |#2" << Menu->szMenuText << wwiv::endl;
		GetSession()->bout << "|#9D) PD Menu Text   : |#2" << Menu->szPDText << wwiv::endl;
		GetSession()->bout << "|#9E) Help Text      : |#2" << Menu->szHelp << wwiv::endl;
		GetSession()->bout << "|#9F) Inst Msg       : |#2" << Menu->szInstanceMessage << wwiv::endl;
		GetSession()->bout << "|#9G) Sysop Log      : |#2" << Menu->szSysopLog << wwiv::endl;
		GetSession()->bout << "|#9K) Min SL         : |#2" << Menu->nMinSL << wwiv::endl;
		GetSession()->bout << "|#9L) Max SL         : |#2" << Menu->iMaxSL << wwiv::endl;
		GetSession()->bout << "|#9M) Min DSL        : |#2" << Menu->nMinDSL << wwiv::endl;
		GetSession()->bout << "|#9N) Max DSL        : |#2" << Menu->iMaxDSL << wwiv::endl;
		GetSession()->bout << "|#9O) AR             : |#2" << Menu->uAR << wwiv::endl;
		GetSession()->bout << "|#9P) DAR            : |#2" << Menu->uDAR << wwiv::endl;
		GetSession()->bout << "|#9R) Restrictions   : |#2" << Menu->uRestrict << wwiv::endl;
		GetSession()->bout << "|#9S) Sysop          : |#2" << ( Menu->nSysop ? "Yes" : "No" )  << wwiv::endl;
		GetSession()->bout << "|#9T) Co-Sysop       : |#2" << ( Menu->nCoSysop ? "Yes" : "No" ) << wwiv::endl;
		GetSession()->bout << "|#9U) Password       : |#2" << ( incom ? "<Remote>" : Menu->szPassWord ) << wwiv::endl;
		GetSession()->bout << "|#9V) Hide text from : |#2" << ( Menu->nHide == MENU_HIDE_NONE ? "None" : Menu->nHide == MENU_HIDE_PULLDOWN ? "Pulldown Menus" : Menu->nHide == MENU_HIDE_REGULAR ? "Regular Menus" : Menu->nHide == MENU_HIDE_BOTH ? "Both Menus" : "Out of Range" ) << wwiv::endl;
		GetSession()->bout.WriteFormatted( "|#9W) Pulldown flags : |#5%-20.20s |#1%-18.18s |#6%-20.20s", Menu->nPDFlags & PDFLAGS_NOCLEAR ? "No Clear before run" : "Clear before run", Menu->nPDFlags & PDFLAGS_NOPAUSEAFTER ? "No Pause after run" : "Pause after run", Menu->nPDFlags & PDFLAGS_NORESTORE ? "No Restore after run" : "Restore after run" );
		GetSession()->bout << "|#9X) Extended Help  : |#2%s" << Menu->szExtendedHelp << wwiv::endl;

	}
	GetSession()->bout.NewLine( 2 );
	GetSession()->bout << "|041|#0,|04A|#0-|04F|#0,|04K|#0-|04U|#0, |04Z|#0=Add new record, |04[|#0=Prev, |04]|#0=Next, |04Q|#0=Quit : ";
}


void DisplayHeader(MenuHeader * pHeader, int nCur, int nAmount, const char *pszDirectoryName)
{
	char szDesc[101];

	GetSession()->bout.ClearScreen();

	OpenMenuDescriptions();

	GetSession()->bout << "(" << nCur << "/" << nAmount << ")" << wwiv::endl;

	if ( nCur == 0 )
    {
		GetSession()->bout << "   Menu Version         : " <<
			          static_cast<int>( HIBYTE(pHeader->nVersion ) ) <<
					  static_cast<int>( LOBYTE( pHeader->nVersion ) ) << wwiv::endl;
		GetSession()->bout << "0) Menu Description     : " << GetMenuDescription( pszDirectoryName, szDesc ) << wwiv::endl;
		GetSession()->bout << "1) Deleted              : " << ( ( pHeader->nFlags & MENU_FLAG_DELETED ) ? "Yes" : "No" ) << wwiv::endl;
		GetSession()->bout << "2) Main Menu            : " << ( ( pHeader->nFlags & MENU_FLAG_MAINMENU ) ? "Yes" : "No" ) << wwiv::endl;;
		GetSession()->bout << "A) What do Numbers do   : " << ( pHeader->nNumbers == MENU_NUMFLAG_NOTHING ? "Nothing" : pHeader->nNumbers == MENU_NUMFLAG_SUBNUMBER ? "Set sub number" : pHeader->nNumbers == MENU_NUMFLAG_DIRNUMBER ? "Set dir number" : "Out of range" ) << wwiv::endl;
		GetSession()->bout << "B) What type of logging : " << ( pHeader->nLogging == MENU_LOGTYPE_KEY ? "Key entered" : pHeader->nLogging == MENU_LOGTYPE_NONE ? "No logging" : pHeader->nLogging == MENU_LOGTYPE_COMMAND ? "Command being executeed" : pHeader->nLogging == MENU_LOGTYPE_DESC ? "Desc of Command" : "Out of range" ) << wwiv::endl;
		GetSession()->bout << "C) Force help to be on  : " << ( pHeader->nForceHelp == MENU_HELP_DONTFORCE ? "Not forced" : pHeader->nForceHelp == MENU_HELP_FORCE ? "Forced On" : pHeader->nForceHelp == MENU_HELP_ONENTRANCE ? "Forced on entrance" : "Out of range" ) << wwiv::endl;
		GetSession()->bout << "D) Allowed menu type    : " << ( pHeader->nAllowedMenu == MENU_ALLOWED_BOTH ? "Regular/Pulldown" : pHeader->nAllowedMenu == MENU_ALLOWED_PULLDOWN ? "Pulldown" : pHeader->nAllowedMenu == MENU_ALLOWED_REGULAR ? "Regular" : "Out of range" ) << wwiv::endl;
		GetSession()->bout << "E) Pulldown menu title  : " << pHeader->szMenuTitle << wwiv::endl;
		GetSession()->bout << "F) Enter Script         : " << pHeader->szScript << wwiv::endl;
		GetSession()->bout << "G) Exit Script          : " << pHeader->szExitScript << wwiv::endl;
		GetSession()->bout << "H) Min SL               : " << static_cast<unsigned int>( pHeader->nMinSL ) << wwiv::endl;
		GetSession()->bout << "I) Min DSL              : " << static_cast<unsigned int>( pHeader->nMinDSL ) << wwiv::endl;
		GetSession()->bout << "J) AR                   : " << static_cast<unsigned int>( pHeader->uAR ) << wwiv::endl;
		GetSession()->bout << "K) DAR                  : " << static_cast<unsigned int>( pHeader->uDAR ) << wwiv::endl;
		GetSession()->bout << "L) Restrictions         : " << static_cast<unsigned int>( pHeader->uRestrict ) << wwiv::endl;
		GetSession()->bout << "M) Sysop                : " << ( pHeader->nSysop ? "Yes" : "No" ) << wwiv::endl;
		GetSession()->bout << "N) Co-Sysop             : " << ( pHeader->nCoSysop ? "Yes" : "No" ) << wwiv::endl;
		GetSession()->bout << "O) Password             : " << ( incom ? "<Remote>" : pHeader->szPassWord ) << wwiv::endl;
		GetSession()->bout << "P) Change pulldown colors" << wwiv::endl;
	}
	GetSession()->bout.NewLine( 2 );
	GetSession()->bout << "0-2, A-O, Z=Add new Record, [=Prev, ]=Next, Q=Quit : ";

	CloseMenuDescriptions();
}


void EditPulldownColors(MenuHeader * pMenuHeader)
{
	char szTemp[15];

	bool done = false;
	while (!done && !hangup)
	{
		GetSession()->bout.ClearScreen();
		GetSession()->bout.Color( 0 );

		GetSession()->bout.WriteFormatted("%-35.35s", "A) Title color");
		if ( pMenuHeader->nTitleColor )
		{
			GetSession()->bout.SystemColor( pMenuHeader->nTitleColor );
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nTitleColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "B) Main border color");
		if (pMenuHeader->nMainBorderColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nMainBorderColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nMainBorderColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "C) Main box color");
		if (pMenuHeader->nMainBoxColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nMainBoxColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nMainBoxColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "D) Main text color");
		if (pMenuHeader->nMainTextColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nMainTextColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nMainTextColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "E) Main text highlight color");
		if (pMenuHeader->nMainTextHLColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nMainTextHLColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nMainTextHLColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "F) Main selected color");
		if (pMenuHeader->nMainSelectedColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nMainSelectedColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nMainSelectedColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "G) Main selected hightlight color");
		if (pMenuHeader->nMainSelectedHLColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nMainSelectedHLColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nMainSelectedHLColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );

		GetSession()->bout.WriteFormatted("%-35.35s", "K) Item border color");
		if (pMenuHeader->nItemBorderColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nItemBorderColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nItemBorderColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "L) Item box color");
		if (pMenuHeader->nItemBoxColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nItemBoxColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nItemBoxColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "M) Item text color");
		if (pMenuHeader->nItemTextColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nItemTextColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nItemTextColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "N) Item text highlight color");
		if (pMenuHeader->nItemTextHLColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nItemTextHLColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nItemTextHLColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "O) Item selected color");
		if (pMenuHeader->nItemSelectedColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nItemSelectedColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nItemSelectedColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );
		GetSession()->bout.WriteFormatted("%-35.35s", "P) Item selected hightlight color");
		if (pMenuHeader->nItemSelectedHLColor)
		{
			GetSession()->bout.SystemColor(pMenuHeader->nItemSelectedHLColor);
		}
		GetSession()->bout << static_cast<int>( pMenuHeader->nItemSelectedHLColor ) << wwiv::endl;
		GetSession()->bout.Color( 0 );

		GetSession()->bout.NewLine( 2 );
		GetSession()->bout << "A-G,K-P, Q=Quit : ";
		char chKey = onek("QABCDEFGKLMNOP");

		if (chKey != 'Q')
		{
			ListAllColors();
			GetSession()->bout.NewLine();
			GetSession()->bout << "Select a color : ";
		}
		switch (chKey)
		{
	  case 'A':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nTitleColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'B':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nMainBorderColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'C':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nMainBoxColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'D':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nMainTextColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'E':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nMainTextHLColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'F':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nMainSelectedColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'G':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nMainSelectedHLColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'K':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nItemBorderColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'L':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nItemBoxColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'M':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nItemTextColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'N':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nItemTextHLColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'O':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nItemSelectedColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'P':
		  input(szTemp, 3);
		  if (szTemp[0])
		  {
			  pMenuHeader->nItemSelectedHLColor = wwiv::stringUtils::StringToChar(szTemp);
		  }
		  break;
	  case 'Q':
		  done = true;
		  break;
		}
	}
}


void ListMenuDirs()
{
	char szPath[MAX_PATH], szName[20];
	char szMenuDir[MAX_PATH];
	char szDesc[101];
	char szFileName[MAX_PATH];
	WFindFile fnd;
	bool bFound;

	sprintf(szPath, "%s*.*", MenuDir(szMenuDir));

	OpenMenuDescriptions();

	GetSession()->bout.NewLine();
	GetSession()->bout << "|#7Available Menus Sets\r\n";
	GetSession()->bout << "|#5============================\r\n";

	bFound = fnd.open(szPath, 0);
	while (bFound && !hangup)
	{
		strcpy(szFileName, fnd.GetFileName());
		if ((szFileName[0] != '.') && (fnd.IsDirectory()))
		{
			WWIV_GetFileNameFromPath(szFileName, szName);
			GetSession()->bout.WriteFormatted("|#2%-8.8s |15%-60.60s\r\n", szName, GetMenuDescription(szFileName, szDesc));
		}
		bFound = fnd.next();
	}
	GetSession()->bout.NewLine();

	CloseMenuDescriptions();
	GetSession()->bout.Color( 0 );
}


void ListMenuMenus( const char *pszDirectoryName )
{
	char szPath[MAX_PATH];
	char szMenuDir[MAX_PATH];
	char *ss;
	char szFileName[MAX_PATH];
	WFindFile fnd;
	bool bFound;

	sprintf(szPath, "%s%s%c*.MNU", MenuDir(szMenuDir), pszDirectoryName,
		WWIV_FILE_SEPERATOR_CHAR);

	GetSession()->bout.NewLine();
	GetSession()->bout << "|#7Available Menus\r\n";
	GetSession()->bout << "|#5===============|06\r\n";

	bFound = fnd.open(szPath, 0);
	while (bFound && !hangup)
	{
		strcpy(szFileName, fnd.GetFileName());
		if (fnd.IsFile())
		{
			ss = strtok(szFileName, ".");
			GetSession()->bout << ss << wwiv::endl;
		}
		bFound = fnd.next();
	}
	GetSession()->bout.NewLine();
	GetSession()->bout.Color( 0 );
}
