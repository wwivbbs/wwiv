#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curses.h>

#include "../wtypes.h"
#include "../platform/incl1.h"
#include "../vardec.h"
#include "../net.h"
#include "../filenames.h"

configrec config;
languagerec lang;
statusrec status;

int DirSanityCheck(int which)
{
	if(!which || (which == 1))
		if(config.msgsdir[strlen(config.msgsdir)-1] !=
		  WWIV_FILE_SEPERATOR_CHAR)
			strcat(config.msgsdir, WWIV_FILE_SEPERATOR_STRING);

	if(!which || (which == 2))
		if(config.gfilesdir[strlen(config.gfilesdir)-1] !=
		  WWIV_FILE_SEPERATOR_CHAR)
			strcat(config.gfilesdir, WWIV_FILE_SEPERATOR_STRING);

	if(!which || (which == 3))
		if(config.datadir[strlen(config.datadir)-1] !=
		  WWIV_FILE_SEPERATOR_CHAR)
			strcat(config.datadir, WWIV_FILE_SEPERATOR_STRING);

	if(!which || (which == 4))
		if(config.dloadsdir[strlen(config.dloadsdir)-1] !=
		  WWIV_FILE_SEPERATOR_CHAR)
			strcat(config.dloadsdir, WWIV_FILE_SEPERATOR_STRING);

	if(!which || (which == 5))
		if(config.tempdir[strlen(config.tempdir)-1] !=
		  WWIV_FILE_SEPERATOR_CHAR)
			strcat(config.tempdir, WWIV_FILE_SEPERATOR_STRING);

	if(!which || (which == 6))
		if(config.menudir[strlen(config.menudir)-1] !=
		  WWIV_FILE_SEPERATOR_CHAR)
			strcat(config.menudir, WWIV_FILE_SEPERATOR_STRING);

	if(!which || (which == 7))
		if(config.batchdir[strlen(config.batchdir)-1] !=
		  WWIV_FILE_SEPERATOR_CHAR)
			strcat(config.batchdir, WWIV_FILE_SEPERATOR_STRING);

	return 1;
}

int ReadConfig()
{
	int hFile = open(CONFIG_DAT, O_BINARY | O_RDONLY);
	if (hFile == -1)
	{
		printw("ERROR: Unable to open CONFIG.DAT");
		return 0;
	}
	read(hFile, &config, sizeof(configrec));
	DirSanityCheck(0);
	return 1;

}

int WriteConfig()
{
	int hFile = open(CONFIG_DAT, 
			O_WRONLY|O_CREAT|O_TRUNC, 
			S_IRUSR|S_IWUSR);
	if (hFile == -1)
	{
		printw("ERROR: Unable to create CONFIG.DAT");
		return 0;
	}
	int ret = write(hFile, &config, sizeof(configrec));
	printf(" ret = %d", ret);
	return 1;
}


int ReadLang()
{
	char szFn[260];
	sprintf(szFn, "%s%s", config.datadir, LANGUAGE_DAT);
	int hFile = open(szFn, O_BINARY | O_RDONLY);
	if (hFile == -1)
	{
		printw("ERROR: Unable to open LANGUAGE.DAT");
		return 0;
	}
	read(hFile, &lang, sizeof(languagerec));
	return 1;
}


int WriteLang()
{
	char szFn[260];
	sprintf(szFn, "%s%s", config.datadir, LANGUAGE_DAT);
	int hFile = open(szFn, 
			O_WRONLY|O_CREAT|O_TRUNC, 
			S_IRUSR|S_IWUSR);
	if (hFile == -1)
	{
		printw("ERROR: Unable to create LANGUAGE.DAT");
		return 0;
	}
	int ret = write(hFile, &lang, sizeof(languagerec));
	printf("ret = %d", ret);
	return 1;
}


int ReadStatus()
{
	char szFn[260];
	sprintf(szFn, "%s%s", config.datadir, STATUS_DAT);
	int hFile = open(szFn, O_BINARY | O_RDONLY);
	if (hFile == -1)
	{
		printw("ERROR: Unable to open STATUS.DAT");
		return 0;
	}
	read(hFile, &status, sizeof(statusrec));
	return 1;
}


int WriteStatus()
{
	char szFn[260];
	sprintf(szFn, "%s%s", config.datadir, STATUS_DAT);
	int hFile = open(szFn, 
			O_WRONLY|O_CREAT|O_TRUNC, 
			S_IRUSR|S_IWUSR);
	if (hFile == -1)
	{
		printw("ERROR: Unable to create STATUS.DAT");
		return 0;
	}
	int ret = write(hFile, &status, sizeof(statusrec));
	printf("ret = %d", ret);
	return 1;
}



int EditPaths()
{
	char szLine[263];
	clear();
	int bDone = 0;
	while (!bDone)
	{
		clear();
		mvprintw(0, 0, "Init Lite version 5.0");
		mvprintw(1, 0, "1) Message Dir   : %s", config.msgsdir);
		mvprintw(2, 0, "2) GFiles Dir    : %s", config.gfilesdir);
		mvprintw(3, 0, "3) Data Dir      : %s", config.datadir);
		mvprintw(4, 0, "4) Downloads Dir : %s", config.dloadsdir);
		mvprintw(5, 0, "5) Temp Dir      : %s", config.tempdir);
		mvprintw(6, 0, "6) Menu Dir      : %s", config.menudir);
		mvprintw(7, 0, "7) Batch Dir     : %s", config.batchdir);
		mvprintw(9, 0, "(Q=Quit) Enter Command : ");
		int key = getch();
		key = toupper(key);
		switch (key)
		{
		case '1':
			mvprintw(10, 0, "Enter Msgs Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.msgsdir, szLine);
			}
			DirSanityCheck(1);	// messages dir
			break;
		case '2':
			mvprintw(10, 0, "Enter GFiles Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.gfilesdir, szLine);
			}
			DirSanityCheck(2);	// gfiles dir
			break;
		case '3':
			mvprintw(10, 0, "Enter Data Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.datadir, szLine);
			}
			DirSanityCheck(3);	// data dir
			break;
		case '4':
			mvprintw(10, 0, "Enter DLoads Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.dloadsdir, szLine);
			}
			DirSanityCheck(4);	// dloads dir
			break;
		case '5':
			mvprintw(10, 0, "Enter Temp Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.tempdir, szLine);
				strcpy(config.batchdir, szLine);
			}
			DirSanityCheck(5);	// tempdir
			DirSanityCheck(7);	// batchdir
			break;
		case '6':
			mvprintw(10, 0, "Enter Menu Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.menudir, szLine);
			}
			DirSanityCheck(6);	// menu dir
			break;
		case '7':
			mvprintw(10, 0, "Enter Batch Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.batchdir, szLine);
			}
			DirSanityCheck(7);	// batch dir
			break;
		case 'Q': 
			bDone = 1;
			break;
		default:
			break;
		}
	}
	DirSanityCheck(0);			// all dirs
	return 0;
}


int EditLanguages()
{
	char szLine[263];
	clear();
	ReadLang();
	int bDone = 0;
	while (!bDone)
	{
		clear();
		mvprintw(0, 0, "Init Lite version 5.0");
		mvprintw(2, 0, "1) Name:  %s", lang.name);
		mvprintw(3, 0, "2) Num:   %d", lang.num);
		mvprintw(4, 0, "3) Dir:   %s", lang.dir);
		mvprintw(5, 0, "4) MDir:  %s", lang.mdir);
		mvprintw(8, 0, "(Q=Quit) Enter Command : ");
		int key = getch();
		key = toupper(key);
		switch (key)
		{
		case '1':
			mvprintw(10, 0, "Enter Language Name:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(lang.name, szLine);
			}
			break;
		case '2':
			mvprintw(10, 0, "Enter Language Number:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				lang.num = atoi(szLine);
			}
			break;
		case '3':
			mvprintw(10, 0, "Enter Language Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(lang.dir, szLine);
			}
			break;
		case '4':
			mvprintw(10, 0, "Enter Language Menu Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(lang.mdir, szLine);
			}
			break;
		case 'Q': 
			bDone = 1;
			break;
		default:
			break;
		}
	}
	WriteLang();
	return 1;
}


int EditStatus()
{
	char szLine[263];
	clear();
	ReadStatus();
	int bDone = 0;
	while (!bDone)
	{
		clear();
		mvprintw(0, 0, "Init Lite version 5.0");
		mvprintw(2, 0, "1) Num Calls:  %d", status.callernum1);
		mvprintw(8, 0, "(Q=Quit) Enter Command : ");
		int key = getch();
		key = toupper(key);
		switch (key)
		{
		case '1':
			mvprintw(10, 0, "Enter new number of calls:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				status.callernum1 = atoi(szLine);
			}
			break;
		case 'Q': 
			bDone = 1;
			break;
		default:
			break;
		}
	}
	WriteStatus();
	return 1;
}

int EditSystemInfo()
{
	char szLine[263];
	clear();
	ReadConfig();
	int bDone = 0;
	while (!bDone)
	{
		clear();
		mvprintw(0, 0, "Init Lite version 5.0");
		mvprintw(2, 0, "1) System Name :  %s", config.systemname );
		mvprintw(3, 0, "2) System Phone:  %s", config.systemphone );
		mvprintw(4, 0, "3) Sysop Name  :  %s", config.sysopname );
		mvprintw(5, 0, "4) RegCode     :  %s", config.regcode );
		mvprintw(6, 0, "5) System Pass :  %s", config.systempw );
		mvprintw(8, 0, "(Q=Quit) Enter Command : ");
		int key = getch();
		key = toupper(key);
		switch (key)
		{
		case '1':
			mvprintw(10, 0, "Enter new System Name:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.systemname, szLine);
			}
			break;
		case '2':
			mvprintw(10, 0, "Enter new System Phone:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.systemphone, szLine);
			}
			break;
		case '3':
			mvprintw(10, 0, "Enter new Sysop Name:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.sysopname, szLine);
			}
			break;
		case '4':
			mvprintw(10, 0, "Enter new Registration Code:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				strcpy(config.regcode, szLine);
				config.wwiv_reg_number = atoi(szLine);
			}
			break;
		case '5':
			mvprintw(10, 0, "Enter new Password:");
			move(11, 0); 
			getnstr(szLine, 81);
			mvprintw(15, 0, "Line entered = '%s'", szLine);
			if (strlen(szLine)>0)
			{
				for(unsigned int i =0; i < strlen(szLine); i++)
				{
				  szLine[i] = toupper(szLine[i]);
				}
				strcpy(config.systempw, szLine);
			}
			break;
		case 'Q': 
			bDone = 1;
			break;
		default:
			break;
		}
	}
	WriteConfig();
	return 1;
}

void TwiddleSysopRec()
{
	userrec userFirst;
	userrec user;
	char szFn[260];
	sprintf(szFn, "%s%s", config.datadir, USER_LST);
	int hFile = open(szFn, O_BINARY | O_RDONLY);
	if (hFile == -1)
	{
		printw("ERROR: Unable to open CONFIG.DAT");
		return;
	}
	read(hFile, &userFirst, sizeof(userrec));
	read(hFile, &user, sizeof(userrec));

	close(hFile);
	user.ar = user.dar = 0xFFFF;
	user.sl = user.dsl = 255;
	user.restrict = user.exempt = 0;
	user.language = 1;

	hFile = open(szFn, O_RDWR | O_BINARY);
	if (hFile == -1)
	{
		printw("ERROR: Unable to create CONFIG.DAT");
		return;
	}
	write(hFile, &userFirst, sizeof(userrec));
	write(hFile, &user, sizeof(userrec));
	close(hFile);
}

void NetworkSetup()
{
	int netNumber = 0;
	int netCount = 0;
	int bDone = 0;
	net_networks_rec netrec = {0};
	char szFn[260];
	char szLine[263];
	FILE *fp;

	sprintf(szFn, "%s%s", config.datadir, NETWORKS_DAT);
	fp = fopen(szFn, "r+b");
	if(fp == NULL)
	{
		printw("ERROR: Unable to read NETWORKS.DAT\n");
		printw(szFn);
		getch();
		return;
	}
	fseek(fp, 0L, SEEK_END);
	netCount = (ftell(fp) / sizeof(net_networks_rec));
	fseek(fp, 0L, SEEK_SET);
	fread(&netrec, 1, sizeof(net_networks_rec), fp);
	while (!bDone)
	{
		clear();
		mvprintw(0, 0, "Network Setup");
		mvprintw(2, 0, "Net Number      :  %u", netNumber);
		mvprintw(3, 0, "1) Network Name :  %s", netrec.name);
		mvprintw(4, 0, "2) Network Dir  :  %s", netrec.dir);
		mvprintw(5, 0, "3) Network Type :  %u", netrec.type);
		mvprintw(6, 0, "4) Node Number  :  %u", netrec.sysnum);
		mvprintw(8, 0, "(Q=Quit, [, ], N) Enter Command : ");
		int key = getch();
		key = toupper(key);
		switch (key)
		{
		case '1':
			mvprintw(10, 0, "Enter new Network Name:");
			move(11, 0); 
			getnstr(szLine, 81);
			if (strlen(szLine)>0)
			{
				strcpy(netrec.name, szLine);
			}
			break;
		case '2':
			mvprintw(10, 0, "Enter new Network Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			if (strlen(szLine)>0)
			{
				strcpy(netrec.dir, szLine);
			}
			break;
		case '3':
			mvprintw(10, 0, "WWIVNet == 0");
			mvprintw(11, 0, "FidoNet == 1");
			mvprintw(12, 0, "Internet == 3");
			mvprintw(13, 0, "Enter Network Type:");
			move(14, 0); 
			getnstr(szLine, 81);
			if (strlen(szLine)>0)
			{
				netrec.type = atoi(szLine);
			}
			break;
		case '4':
			mvprintw(10, 0, "Enter Node Number:");
			move(11, 0); 
			getnstr(szLine, 81);
			if (strlen(szLine)>0)
			{
				netrec.sysnum = atoi(szLine);
			}
			break;
		case '[':
			if(netNumber)
			{
				fseek(fp, netNumber * sizeof(net_networks_rec), SEEK_SET);
				fwrite(&netrec, 1, sizeof(net_networks_rec), fp);
				netNumber--;
				fseek(fp, netNumber * sizeof(net_networks_rec), SEEK_SET);
				fread(&netrec, 1, sizeof(net_networks_rec), fp);
			}
			break;
		case ']':
			if(netNumber < netCount)
			{
				fseek(fp, netNumber * sizeof(net_networks_rec), SEEK_SET);
				fwrite(&netrec, 1, sizeof(net_networks_rec), fp);
				netNumber++;
				fseek(fp, netNumber * sizeof(net_networks_rec), SEEK_SET);
				fread(&netrec, 1, sizeof(net_networks_rec), fp);
			}
			break;
		case 'N':
			if(netCount != 0)
			{
				fseek(fp, netNumber * sizeof(net_networks_rec), SEEK_SET);
				fwrite(&netrec, 1, sizeof(net_networks_rec), fp);
			}
			netNumber = netCount;
			netCount++;
			break;
		case 'Q': 
			bDone = 1;
			break;
		default:
			break;
		}
	}
	fseek(fp, netNumber * sizeof(net_networks_rec), SEEK_SET);
	fwrite(&netrec, 1, sizeof(net_networks_rec), fp);
	fclose(fp);
}


void NodeSetup()
{
	int nodeNumber = 1;
	int nodeCount = 1;
	int bDone = 0;
	configoverrec cfgOverRec;
	char szLine[263];
	FILE *fp;

	memset(&cfgOverRec, 0L, sizeof(configoverrec));

	fp = fopen(CONFIG_OVR, "r+b");
	if(fp == NULL)
	{
		fp = fopen(CONFIG_OVR, "wb");
	}
	fseek(fp, 0L, SEEK_END);
	nodeCount = (ftell(fp) / sizeof(configoverrec));
	fseek(fp, 0L, SEEK_SET);
	fread(&cfgOverRec, 1, sizeof(configoverrec), fp);
	while (!bDone)
	{
		clear();
		mvprintw(0, 0, "Node Setup");
		mvprintw(2, 0, "Node Number     :  %u", nodeNumber);
		mvprintw(3, 0, "1) Temp Dir     :  %s", cfgOverRec.tempdir);
		mvprintw(4, 0, "2) Batch Dir    :  %s", cfgOverRec.batchdir);
		mvprintw(8, 0, "(Q=Quit, [, ], N) Enter Command : ");
		int key = getch();
		key = toupper(key);
		switch (key)
		{
		case '1':
			mvprintw(10, 0, "Enter new Temp Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			if (strlen(szLine)>0)
			{
				strcpy(cfgOverRec.tempdir, szLine);
			}
			break;
		case '2':
			mvprintw(10, 0, "Enter new Batch Dir:");
			move(11, 0); 
			getnstr(szLine, 81);
			if (strlen(szLine)>0)
			{
				strcpy(cfgOverRec.batchdir, szLine);
			}
			break;
		case '[':
			if(nodeNumber > 1)
			{
				fseek(fp, (nodeNumber-1) * sizeof(configoverrec), SEEK_SET);
				fwrite(&cfgOverRec, 1, sizeof(configoverrec), fp);
				nodeNumber--;
				fseek(fp, (nodeNumber-1) * sizeof(configoverrec), SEEK_SET);
				fread(&cfgOverRec, 1, sizeof(configoverrec), fp);
			}
			break;
		case ']':
			if(nodeNumber < nodeCount)
			{
				fseek(fp, (nodeNumber-1) * sizeof(configoverrec), SEEK_SET);
				fwrite(&cfgOverRec, 1, sizeof(configoverrec), fp);
				nodeNumber++;
				fseek(fp, (nodeNumber-1) * sizeof(configoverrec), SEEK_SET);
				fread(&cfgOverRec, 1, sizeof(configoverrec), fp);
			}
			break;
		case 'N':
			if(nodeCount != 1)
			{
				fseek(fp, (nodeNumber-1) * sizeof(configoverrec), SEEK_SET);
				fwrite(&cfgOverRec, 1, sizeof(configoverrec), fp);
			}
			nodeNumber = nodeCount;
			nodeCount++;
			break;
		case 'Q': 
			bDone = 1;
			break;
		default:
			break;
		}
	}
	fseek(fp, (nodeNumber-1) * sizeof(configoverrec), SEEK_SET);
	fwrite(&cfgOverRec, 1, sizeof(configoverrec), fp);
	fclose(fp);
}

int main(int argc, char* argv[])
{
	initscr();
	int bDone = 0;
	ReadConfig();
	while (!bDone)
	{
		clear();
		mvprintw(0, 0, "Init Lite version 5.0");
		mvprintw(2, 0, "1) Edit Paths");
		mvprintw(3, 0, "2) Edit Languages");
		mvprintw(4, 0, "3) Edit Status");
		mvprintw(5, 0, "4) Edit System Info");
		mvprintw(6, 0, "5) SL255 User #1");
		mvprintw(7, 0, "6) Network Setup");
		mvprintw(8, 0, "7) Node Setup");
		mvprintw(9, 0, "(Q=Quit) Enter Command : ");
		int key = getch();
		key = toupper(key);
		switch (key)
		{
		case '1':
			EditPaths();
			WriteConfig();
			break;
		case '2':
			EditLanguages();
			WriteConfig();
			break;
		case '3':
			EditStatus();
			WriteConfig();
			break;
		case '4':
			EditSystemInfo();
			WriteConfig();
			break;
		case '5':
			TwiddleSysopRec();
			break;
		case '6':
			NetworkSetup();
			break;
		case '7':
			NodeSetup();
			break;
		case 'Q': 
			bDone = 1;
			break;
		default:
			break;
		}
	}
	endwin();
	return 0;
	

}


