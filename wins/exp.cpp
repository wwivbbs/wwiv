/*
 * Copyright 2001,2004 Frank Reid
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 

// 
// Use Structured Exception Handling Tracing.
//
#define __WWIV__SEH__

#include "pppproj.h"

#define MEGA_DEBUG_LOG_INFO
#if defined( MEGA_DEBUG_LOG_INFO )
#define DEBUG_PUTS(s) puts(s)
#else // MEGA_DEBUG_LOG_INFO
#define DEBUG_PUTS(s)
#endif // MEGA_DEBUG_LOG_INFO



struct msghdr 
{
	char fromUserName[205];
	char toUserName[81];
	char subject[81];
	char dateTime[81];
};

typedef struct 
{
	char ownername[60];
	char subtype[8];
	char opttext[30];
} MAILLISTREC;

MAILLISTREC *maillist;
configrec syscfg;

char net_name[31], postmaster[31], net_data[_MAX_PATH];
#define EMPTY_STRING_LEN 200
char szEmptyString[EMPTY_STRING_LEN];
char POPNAME[21], REPLYTO[81], DOMAIN[81], POPDOMAIN[81], deftagfile[_MAX_PATH], tagfile[_MAX_PATH], maindir[_MAX_PATH], spamname[81];
unsigned short g_nNetworkSystemNumber, defuser, use_alias, usermail, instance, use_usernum;
bool spam;
int curuser, num_users;
int nlists = 0, jdater, DIGEST;
char alphasubtype[8];
time_t cur_daten;

void get_list_addr(char *list_addr, char *list_name)
{
	SEH_PUSH("get_list_addr()");
	char *ss, szFileName[_MAX_PATH], s[101], szBuffer[60];
	FILE *fp;
	
	bool found = false;
	*list_addr = 0;
	sprintf(szFileName, "%sACCT.INI", net_data);
	if ((fp = fsh_open(szFileName, "rt")) != NULL) 
	{
		while (fgets(s, sizeof(s)-1, fp) && !found) 
		{
			if (_strnicmp(s, "LIST", 4) == 0) 
			{
				ss = strtok(s, "=");
				if (s[4] == '-') 
				{
					strcpy(szBuffer, &s[5]);
					trimstr1(szBuffer);
					if (_stricmp(szBuffer, list_name) == 0) 
					{
						ss = strtok(NULL, "\r\n");
						trimstr1(ss);
						strcpy(list_addr, ss);
						found = true;
					}
				}
			}
		}
		if (fp != NULL)
		{
			fclose(fp);
		}
	}
}


void properize( char *s )
{
	PPP_ASSERT( s );
	SEH_PUSH("properize()");
	for (unsigned int i = 0; i < strlen(s); i++) 
	{
		if ((i == 0) || ((i > 0) && ((s[i - 1] == ' ') || (s[i - 1] == '.') ||
			((s[i - 1] == 'c') && (s[i - 2] == 'M')) || ((s[i - 1] == 'c') &&
			(s[i - 2] == 'a') && (s[i - 3] == 'M')) || (s[i - 1] == '-') ||
			(s[i - 1] == '_') || ((s[i - 1] == '\'') && (s[i - 2] == 'O'))))) 
		{
			s[i] = static_cast< char >( toupper( s[i] ) );
		} 
		else
		{
			s[i] = static_cast< char >( tolower( s[i] ) );
		}
	}
}


static unsigned char translate_table[] = 
{
	"................................_!.#$%&.{}*+.-.{0123456789..{=}?"
	"_abcdefghijklmnopqrstuvwxyz{}}-_.abcdefghijklmnopqrstuvwxyz{|}~."
	"cueaaaaceeeiiiaaelaooouuyouclypfaiounnao?__..!{}................"
	"................................abfneouyo0od.0en=+}{fj*=oo..n2.."
};


char *valid_name( char *pszName )
{
	static char name[60];
	SEH_PUSH("validate_name()");
	PPP_ASSERT( pszName );
	ZeroMemory( name, 60 );
	if ( !pszName || !*pszName )
	{
		return name;
	}
	
	unsigned int j = 0;
	for (unsigned int i = 0; (i < strlen(pszName)) && (i < 81); i++)
	{
		if (pszName[i] != '.')
		{
			name[j++] = translate_table[pszName[i]];
		}
	}
	name[j] = '\0';
	return name;
}


bool num_to_name(char *user_addr, char *user_name, int whichuser, int alias)
{
	char *ss, szFileName[_MAX_PATH], s[101], szBuffer[60];
	long pos;
	FILE *fp;
	userrec ur;
	SEH_PUSH("num_to_name()");
	
	
	if (alias == 2 && user_addr) 
	{
		strcpy(user_name, "Anonymous");
		strcpy(user_addr, "user@wwivbbs.org");
		return true;
	}
	bool found = false;
	if (user_addr && *user_addr) 
	{
		*user_addr = 0;
		sprintf(szFileName, "%sACCT.INI", net_data);
		if ((fp = fsh_open(szFileName, "rt")) != NULL) 
		{
			while (fgets(s, sizeof(s)-1, fp) && !found)
			{
				if (_strnicmp(s, "USER", 4) == 0) 
				{
					ss = strtok(s, "=");
					strcpy(szBuffer, &s[4]);
					if (isdigit(szBuffer[0])) 
					{
						int i = atoi(szBuffer);
						if (i == whichuser) 
						{
							ss = strtok(NULL, "\r\n");
							if (ss) 
							{
								trimstr1(ss);
								strcpy(user_addr, ss);
								found = true;
							}
						}
					}
				}
			}
			if (fp != NULL)
			{
				fclose(fp);
			}
		}
	}
	found = false;
	sprintf( szFileName, "%suser.lst", syscfg.datadir );
	int hUserFile = sh_open1(szFileName, O_RDONLY | O_BINARY);
	if (hUserFile < 0) 
	{
		log_it( true, "\n * Cannot open %s.", szFileName);
		return found;
	}
	int num_users = static_cast<int>( _filelength( hUserFile ) / sizeof( userrec ) );
	
	if (whichuser > num_users) 
	{
		log_it( true, "\n \xFE User #%d out of range.", whichuser);
		return found;
	}
	pos = ((long) sizeof(userrec) * ((long) whichuser));
	_lseek(hUserFile, pos, SEEK_SET);
	sh_read(hUserFile, &ur, sizeof(userrec));
	if (ur.realname[0] == 0)
	{
		log_it( true, "\n \xFE User #%d has blank real name field!", whichuser);
	}
	else 
	{
		if (ur.inact == inact_deleted)
		{
			log_it( true, "\n \xFE User #%d is marked as deleted!", whichuser);
		}
		else 
		{
			if (!alias)
			{
				strcpy(user_name, ( char * ) ur.realname);
			}
			else 
			{
				strcpy(user_name, ( char * ) ur.name);
				properize(user_name);
			}
			found = true;
		}
	}
	DEBUG_PUTS("nn20");
	if (found && user_addr && *POPDOMAIN && (!(*user_addr)) ) 
	{
		if (use_usernum)
		{
			DEBUG_PUTS("nn21");
			sprintf(user_addr, "u%d@%s", whichuser, POPDOMAIN);
		}
		else
		{
			DEBUG_PUTS("nn22");
			sprintf(user_addr, "%s@%s", valid_name( ( char * ) ur.name), POPDOMAIN);
		}
	}
	DEBUG_PUTS("nn23");
	sh_close(hUserFile);
	DEBUG_PUTS("nn24");
	return found;
}


void parse_net_ini()
{
	char szFileName[_MAX_PATH], origline[121], line[121], *ss;
	SEH_PUSH("parse_net_ini()");
	
	defuser = 1;
	use_alias = 1;
	usermail = 1;
	nlists = 0;
	maillist = NULL;
	*REPLYTO = 0;
	*POPDOMAIN = 0;
	*spamname = 0;
	use_usernum = 0;
	sprintf(szFileName, "%snet.ini", maindir);
	FILE *fp;
	if ((fp = fsh_open(szFileName, "rt")) == NULL) 
	{
		log_it( true, "\n \xFE Unable to open %s.", szFileName);
		return;
	}

	bool inlist = false;
	while (fgets(line, sizeof(line)-1, fp)) 
	{
		ss = NULL;
		strcpy(origline, line);
		stripspace(line);
		if ((line[0] == ';') || (line[0] == '\n') || (line[0] == 0))
		{
			continue;
		}
		if (_strnicmp(line, "[MAILLIST]", 10) == 0) 
		{
			long fptr = ftell(fp);
			while ((fgets(line, sizeof(line)-1, fp)) && (line[0] != '[')) 
			{
				if ((line[0] != '[') && (line[0] != ';') && (line[0] != 0) &&
					(_strnicmp(line, "DIGEST", 6) != 0))
				{
					++nlists;
				}
			}
			fseek(fp, fptr, SEEK_SET);
			maillist = (MAILLISTREC *) malloc((nlists + 1) * sizeof(MAILLISTREC));
			if (maillist == NULL)
			{
				log_it( true, "\n \xFE Not enough memory to process %d mailing lists.", nlists);
			}
			else
			{
				inlist = true;
			}
			nlists = 0;
			continue;
		} 
		else if (line[0] == '[') 
		{
			inlist = false;
			continue;
		}
		if (inlist) 
		{
			if (line[0] != ';' && line[0] != 0 && _strnicmp(line, "DIGEST", 6) != 0)
			{
				ss = strtok(line, "\"");
				trimstr1(ss);
				if (ss && *ss) 
				{
					ss = strtok(NULL, "\"");
					if (ss && *ss) 
					{
						strncpy(maillist[nlists].opttext, ss, 29);
						maillist[nlists].opttext[30] = '\0';
					} 
					else
					{
						maillist[nlists].opttext[0] = '\0';
					}
				}
				ss = strtok(line, "*\n");
				trimstr1(ss);
				strcpy(maillist[nlists].ownername, ss);
				ss = strtok(NULL, "\"\n");
				trimstr1(ss);
				if (ss && *ss) 
				{
					_strlwr(maillist[nlists].ownername);
					strcpy(maillist[nlists++].subtype, ss);
				} 
				else
				{
					log_it( true, "\n \xFE Missing *subtype in maillist for %s.",
						maillist[nlists].ownername);
				}
			} 
			else 
			{
				if (_strnicmp(line, "DIGEST", 6) == 0) 
				{
					ss = strtok(line, "=");
					if (ss && *ss) 
					{
						ss = strtok(NULL, "\r\n");
						trimstr1(ss);
						if ((ss[0] == 'y') || (ss[0] == 'Y'))
						{
							DIGEST = 1;
						}
					}
				}
			}
			continue;
		}
		if (_strnicmp(line, "POSTMASTER", 10) == 0) 
		{
			ss = strtok(line, "=");
			if (ss) 
			{
				ss = strtok(NULL, "\n");
				if (ss)
				{
					defuser = ( short ) atoi(ss);
				}
			}
			continue;
		}
		if (_strnicmp(line, "SPAMCONTROL", 11) == 0) 
		{
			ss = strtok(line, "=");
			if (ss) 
			{
				ss = strtok(NULL, "\n");
				if ((ss[0] == 'y') || (ss[0] == 'Y'))
				{
					spam = true;
				}
			}
			continue;
		}
		if (_strnicmp(line, "USERMAIL", 8) == 0) 
		{
			ss = strtok(line, "=");
			if (ss) 
			{
				ss = strtok(NULL, "\n");
				if ((ss[0] == 'n') || (ss[0] == 'N'))
				{
					usermail = 0;
				}
			}
			continue;
		}
		if (_strnicmp(line, "USERNUM", 7) == 0) 
		{
			ss = strtok(line, "=");
			if (ss) 
			{
				ss = strtok(NULL, "\n");
				if ((ss[0] == 'y') || (ss[0] == 'Y'))
				{
					use_usernum = 1;
				}
			}
			continue;
		}
		if (_strnicmp(line, "SPAMADDRESS", 9) == 0) 
		{
			ss = strtok(line, "=");
			if (ss) 
			{
				ss = strtok(NULL, "\n");
				trimstr1(ss);
				strcpy(spamname, ss);
				if (_stricmp(spamname, "DEFAULT") == 0)
				{
					strcpy(spamname, "WWIV_BBS@nospam.net");
				}
			}
			continue;
		}
		if (_strnicmp(line, "POPDOMAIN", 9) == 0) 
		{
			ss = strtok(line, "=");
			if (ss) 
			{
				ss = strtok(NULL, "\n");
				trimstr1(ss);
				strcpy(POPDOMAIN, ss);
			}
			continue;
		}
		if (_strnicmp(line, "REPLYTO", 7) == 0) 
		{
			ss = strtok(origline, "=");
			if (ss) 
			{
				ss = strtok(NULL, "\n");
				trimstr1(ss);
				strcpy(REPLYTO, ss);
			}
			continue;
		}
		if (_strnicmp(line, "SIGNATURE", 9) == 0) 
		{
			ss = strtok(line, "=");
			if (ss) 
			{
				ss = strtok(NULL, "\n");
				trimstr1(ss);
				strcpy(tagfile, ss);
				if (!exist(tagfile)) 
				{
					log_it( true, "\n \xFE Default signature file %s not found!", tagfile);
					tagfile[0] = 0;
				}
				strcpy(deftagfile, tagfile);
			}
			continue;
		}
		if (_strnicmp(line, "REALNAME", 8) == 0) 
		{
			ss = strtok(line, "=");
			if (ss) 
			{
				ss = strtok(NULL, "\n");
				if ((ss[0] == 'y') || (ss[0] == 'Y'))
				{
					use_alias = 0;
				}
			}
		}
  }
  DEBUG_PUTS("5c-loop-17");
  num_to_name(szEmptyString, postmaster, defuser, 1);
  DEBUG_PUTS("5c-loop-18");
  if (fp != NULL)
  {
	  fclose(fp);
  }
  return;
}


unsigned int name_to_num(char *name)
{
	userrec ur;
	char szFileName[_MAX_PATH], ur_name[60], ur_realname[60];
	SEH_PUSH("name_to_num()");
	
	if ((stristr(name, "Multiple recipients of") != NULL) || (strlen(name) == 0))
	{
		return 0;
	}
	if ((strstr(name, " ") == NULL) && (strlen(name) < 8)) 
	{
		sprintf(szFileName, "%sM%s.NET", net_data, name);
		if (exist(szFileName)) 
		{
			log_it( true, "\n \xFE Matched \"%s\" mailing list.", name);
			strcpy(alphasubtype, name);
			return static_cast<unsigned int>( 65535 );
		}
	}
	if (((name[0] == 'u') || (name[0] == 'U')) && (isdigit(name[1])) && (*POPDOMAIN)) 
	{
		int nEmbeddedUserNumber = atoi(&name[1]);
		log_it( true, "\n \xFE User #%d addressed by domain.", nEmbeddedUserNumber);
		return nEmbeddedUserNumber;
	}
	sprintf(szFileName, "%sUSER.LST", syscfg.datadir);
	int hUserFile = sh_open1(szFileName, O_RDONLY | O_BINARY);
	if (hUserFile < 0) 
	{
		log_it( true, "\n \xFE Cannot open %s", szFileName);
		return 0;
	} 
	else
	{
		log_it( true, "\n \xFE Searching for user \"%s\"...", name);
	}
	num_users = ((int) (_filelength(hUserFile) / sizeof(userrec)));
	
	int usernum = 1;
	for (usernum = 1; usernum < num_users; usernum++) 
	{
		long lUserRecFilePos = ((long) sizeof(userrec) * ((long) usernum));
		_lseek(hUserFile, lUserRecFilePos, SEEK_SET);
		sh_read(hUserFile, &ur, sizeof(userrec));
		strcpy(ur_realname, valid_name( ( char * ) ur.realname));
		strcpy(ur_name, valid_name( ( char * ) ur.name));
		if ((_strcmpi(ur_realname, name) == 0) || (_strcmpi(ur_name, name) == 0)) 
		{
			if (ur.inact == inact_deleted) 
			{
				log_it( true, " user #%d is deleted account.", usernum);
				usernum = 0;
				break;
			} 
			else 
			{
				log_it( true, " matched to user #%d.", usernum);
				break;
			}
		}
	}
	hUserFile = sh_close(hUserFile);
	
	if (usernum >= num_users) 
	{
		log_it( true, "... no match found.");
		return 0;
	}
	return usernum;
}

char *find_focus(char *name)
{
	char focus[121];
	SEH_PUSH("find_focus()");
	
	char* ss = name;
	unsigned int i = strcspn(ss, "<");
	if (i < strlen(ss)) 
	{
		strcpy(focus, &ss[i + 1]);
		ss = strtok(focus, ">");
		trimstr1(ss);
	} 
	else 
	{
		i = strcspn(ss, "(");
		if (i < strlen(ss)) 
		{
			strcpy(focus, ss);
			ss = strtok(focus, "(");
			trimstr1(ss);
		}
	}
	return ss;
}

char *find_name(char *name)
{
	char *ss, hold[81];
	int focus = 0;
	SEH_PUSH("find_name()");
	
	ZeroMemory( hold, 81 );
	curuser = 0;
	strcpy(hold, name);
	char *ss1 = NULL;
	if ( (POPDOMAIN) && (*POPDOMAIN) && (stristr(name, POPDOMAIN) != NULL)) 
	{
		strcpy(hold, find_focus(name));
		ss1 = strtok(hold, "@");
	} 
	else if ((ss = strchr(name, '(')) != NULL) 
	{
		ss1 = strtok(name, "(");
		ss1 = strtok(NULL, ")");
	} 
	else if ((ss = strchr(name, '\"')) != NULL) 
	{
		ss1 = strtok(ss, "\"");
	} 
	else if ((ss = strchr(name, '<')) != NULL) 
	{
		ss1 = strtok(name, "<");
	} 
	else 
	{
		focus = 1;
	}

	trimstr1(ss1);
	if (focus) 
	{
		stripspace(hold);
		curuser = name_to_num(hold);
	} 
	else
	{
		curuser = name_to_num(ss1);
	}
	
	if (curuser == 0)
	{
		return ('\0');
	}
	else if (curuser == ((unsigned) 65535L))
	{
		return alphasubtype;
	}
	else
	{
		return ss1;
	}
}


void CreatePacketName( char *pszPacketName )
{
	SEH_PUSH("CreatePacketName()");
	
	for ( int i = 0; i < 1000; i++ )
	{
		sprintf( pszPacketName, "%sP0-%u.%3.3hu", net_data, i, instance );
		if ( !exist( pszPacketName ) )
		{
			break;
		}
	}
}


void CreateMessageName( char *pszFileName )
{
	SEH_PUSH("CreateMessageName()");
	
	for ( int i = 0; i < 1000; i++) 
	{
		sprintf( pszFileName, "%sINBOUND\\UNK-%3.3u.MSG", net_data, i );
		if ( !exist( pszFileName ) )
		{
			break;
		}
	}
}


void CreateCheckMessageName( char *pszFileName )
{
	SEH_PUSH("CreateCheckMessageName()");
	
	for ( int i = 0; i < 1000; i++ )
	{
		sprintf( pszFileName, "%sINBOUND\\CHK-%3.3u.MSG", net_data, i );
		if ( !exist( pszFileName ) )
		{
			break;
		}
	}
}



#define FROM_RETURN 0x01
#define FROM_FROM   0x02
#define FROM_REPLY  0x04

void ssm(char *s)
{
	char szFileName[_MAX_PATH];
	shortmsgrec sm;
	SEH_PUSH("ssm()");
	
	sprintf(szFileName, "%sSMW.DAT", syscfg.datadir);
	int hSMWfile = sh_open(szFileName, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	if (hSMWfile < 0)
	{
		return;
	}
	int i1 = (int) (_filelength(hSMWfile) / sizeof(shortmsgrec)) - 1;
	if (i1 >= 0) 
	{
		sh_lseek(hSMWfile, ((long) (i1)) * sizeof(shortmsgrec), SEEK_SET);
		sh_read(hSMWfile, (void *) &sm, sizeof(shortmsgrec));
		while ((sm.tosys == 0) && (sm.touser == 0) && (i1 > 0)) 
		{
			--i1;
			sh_lseek(hSMWfile, ((long) (i1)) * sizeof(shortmsgrec), SEEK_SET);
			sh_read(hSMWfile, (void *) &sm, sizeof(shortmsgrec));
		}
		if ((sm.tosys) || (sm.touser))
		{
			++i1;
		}
	} 
	else
	{
		i1 = 0;
	}
	sm.tosys = 0;
	sm.touser = 1;
	strncpy(sm.message, s, 80);
	sm.message[80] = 0;
	sh_lseek(hSMWfile, ((long) (i1)) * sizeof(shortmsgrec), SEEK_SET);
	sh_write(hSMWfile, (void *) &sm, sizeof(shortmsgrec));
	sh_close(hSMWfile);
}

int import(char *pszFileName)
{
    char *ss = NULL, *ss1, *p, *id, *name;
    char s[513], s1[121], pktname[_MAX_PATH], msgdate[61];
    char alphatype[21], recvdate[81], realfrom[205], s2[121];
    int i, from, towhom, match, subj, in_to, intext, done, tolist, mailuser, bounce;
    long textlen, reallen;
    struct msghdr mh;
    net_header_rec nh;
    FILE *fp;
    SEH_PUSH("import()");

    towhom = 0;
    tolist = 0;
    intext = 0;
    in_to = 0;
    mailuser = 0;
    bounce = 0;
    int hFile = sh_open1(pszFileName, O_RDONLY | O_BINARY);
    if (hFile < 0)
    {
        return 1;
    }
    textlen = _filelength(hFile);
    if (textlen > 32767L) 
    {
        sh_close(hFile);
        sprintf(s2, "\n \xFE Skipped UU/Base64 %s.", pszFileName);
        log_it( true, s2);
        return 1;
    }
    p = (char *) malloc((int) (textlen + 1));
    if (p == NULL) 
    {
        sh_close(hFile);
        log_it( true, "\n \xFE Unable to allocate %ld bytes.", textlen);
        return 1;
    }
    sh_read(hFile, p, (int) textlen);
    sh_close(hFile);

    nh.tosys = g_nNetworkSystemNumber;
    nh.fromsys = 32767;
    nh.fromuser = 0;
    nh.touser = defuser;
    nh.main_type = main_type_email;
    nh.minor_type = 0;
    nh.list_len = 0;
    ++cur_daten;
    nh.daten = cur_daten;
    strncpy(msgdate, ctime(&cur_daten), 24);
    msgdate[24] = '\0';
    sprintf(recvdate, "%c0RReceived: WINS %s on %s\r\n", 0x03, VERSION, msgdate);
    strcat(msgdate, "\r\n");
    nh.method = 0;

    strcpy(mh.fromUserName, "Unknown");
    strcpy(mh.toUserName, "Unknown");
    strcpy(mh.subject, "None");
    realfrom[0] = 0;

    if ((fp = fsh_open(pszFileName, "rb")) == NULL) 
    {
        free(p);
        return 1;
    }
    match = subj = done = from = 0;
    while (!done && fgets(s, sizeof(s)-1, fp))
    {
        if (s[0] == 4) 
        {
            ss = strtok(s, "R");
            ss = strtok(NULL, "\r\n");
            if (ss == NULL)
            {
                s[0] = 0;
            }
            else
            {
                strcpy(s, ss);
            }
        } 
        else 
        {
            intext = 1;
            in_to = 0;
        }
        if (!intext) 
        {
            if (strchr(s, ':') != NULL)
            {
                in_to = 0;
            }
            if (stristr(s, "x-Mailer: Internet Rex") != NULL) 
            {
                fclose(fp);
                free(p);
                return 1;
            }
            if (_strnicmp(s, "x-wwiv-user", 11) == 0) 
            {
                ss1 = strtok(s, "#");
                if (ss1) 
                {
                    ss1 = strtok(NULL, "\r\n");
                    mailuser = atoi(ss1);
                }
            } 
            else if (_strnicmp(s, "x-wwiv-list", 11) == 0) 
            {
                ss1 = strtok(s, "*");
                if (ss1) 
                {
                    ss1 = strtok(NULL, "\r\n");
                    strcpy(alphatype, ss1);
                    nh.main_type = main_type_new_post;
                    nh.minor_type = 0;
                    nh.touser = 0;
                    mailuser = -1;
                    tolist = 1;
                }
            } 
            else if (_strnicmp(s, "from:", 5) == 0)
            {
                from = FROM_FROM;
            }
            else if (_strnicmp(s, "return-path:", 12) == 0)
            {
                from = FROM_RETURN;
            }
            else if (_strnicmp(s, "sender:", 7) == 0)
            {
                from = FROM_RETURN;
            }
            else if (_strnicmp(s, "x-sender:", 9) == 0)
            {
                from = FROM_RETURN;
            }
            else if (_strnicmp(s, "x-to:", 5) == 0)
            {
                from = FROM_RETURN;
            }
            else if (_strnicmp(s, "x-mailing-list:", 15) == 0)
            {
                from = FROM_RETURN;
            }
            else if (_strnicmp(s, "reply-to:", 9) == 0)
            {
                from = FROM_REPLY;
            }
            else if (_strnicmp(s, "x-reply-to:", 11) == 0)
            {
                from = FROM_REPLY;
            }
            else if (s[0] != ' ')
            {
                from = 0;
            }
            if (from) 
            {
                if (s[0] != ' ') 
                {
                    ss = strtok(s, ": ");
                    ss = strtok(NULL, "\r\n");
                }
                trimstr1(ss);
            }
            if (from && (strchr(ss, '@') != NULL)) 
            {
                if (from && (nh.main_type == main_type_email)) 
                {
                    strcpy(s1, ss);
                    _strlwr(s1);
                    for (i = 0; (i < nlists) && (nh.main_type == main_type_email); i++) 
                    {
                        if (stristr(s1, maillist[i].ownername) != NULL) 
                        {
                            if (atoi(maillist[i].subtype)) 
                            {
                                nh.main_type = main_type_pre_post;
                                nh.minor_type = ( unsigned short ) atoi(maillist[i].subtype);
                            } 
                            else 
                            {
                                nh.main_type = main_type_new_post;
                                nh.minor_type = 0;
                                strcpy(alphatype, maillist[i].subtype);
                            }
                            strcpy(alphasubtype, maillist[i].subtype);
                            nh.touser = 0;
                            from = 0;
                        }
                    }
                }
                if ( from > match && (nh.main_type == main_type_email || from == FROM_FROM ) ) 
                {
                    match = from;
                    if (strcspn(ss, "<") != strlen(ss)) 
                    {
                        if ((strcspn(ss, " ")) < (strcspn(ss, "<"))) 
                        {
                            name = strtok(ss, "<");
                            trimstr1(name);
                            id = strtok(NULL, ">");
                            trimstr1(id);
                            sprintf(mh.fromUserName, "%s <%s>", name, id);
                        } 
                        else 
                        {
                            strncpy(mh.fromUserName, ss, 205);
                            trimstr1(mh.fromUserName);
                            if (strcspn(ss, " ") != strlen(ss))
                            {
                                log_it( true, "\n ! Error parsing return address \"%s\"", mh.fromUserName);
                            }
                        }
                    } 
                    else
                    {
                        strncpy(mh.fromUserName, ss, 205);
                    }
                    mh.fromUserName[190] = 0;
                    strcat(mh.fromUserName, "\r\n");
                    if (from == FROM_FROM)
                    {
                        strcpy(realfrom, mh.fromUserName);
                    }
                }
            } 
            else if ( _strnicmp(s, "subject:", 8) == 0 && !subj ) 
            {
                ss = strtok(s, ": ");
                ss = strtok(NULL, "\r\n");
                trimstr1(ss);
                strncpy(mh.subject, ss, 81);
                mh.subject[72] = 0;
                subj = 1;
            } 
            else if (_strnicmp(s, "date:", 5) == 0) 
            {
                ss = strtok(s, ": ");
                ss = strtok(NULL, "\r\n");
                trimstr1(ss);
                strncpy(msgdate, ss, 58);
                msgdate[58] = '\0';
                strcat(msgdate, "\r\n");
            } 
            else if (_strnicmp(s, "message-id:", 11) == 0) 
            {
                sprintf(s1, "%s@wwivbbs.org", POPNAME);
                if (stristr(s, s1) != NULL)
                {
                    bounce = 1;
                }
            } 
            else if (_strnicmp(s, "apparently-to:", 14) == 0) 
            {
                ss = strtok(s, ": ");
                ss = strtok(NULL, "\r\n");
                strncpy(mh.toUserName, ss, 81);
                mh.toUserName[80] = 0;
                curuser = 0;
                trimstr1(mh.toUserName);
                if (strstr(mh.toUserName, " "))
                {
                    find_name(mh.toUserName);
                }
                else
                {
                    mh.toUserName[0] = 0;
                }
                if ((curuser == (unsigned) 65535L) && (nh.main_type == main_type_email)) 
                {
                    strcpy(alphatype, alphasubtype);
                    nh.main_type = main_type_new_post;
                    nh.minor_type = 0;
                    nh.touser = 0;
                    tolist = 1;
                } 
                else if ((mh.toUserName[0] == 0) || (curuser == 0)) 
                {
                    nh.touser = defuser;
                    strcpy(mh.toUserName, postmaster);
                } 
                else
                {
                    nh.touser = ( unsigned short ) curuser;
                }
            } 
            else if ( _strnicmp(s, "to:", 3) == 0 || _strnicmp(s, "cc:", 3) == 0 )
            {
                in_to = 1;
            }
            if (in_to) 
            {
                if (mailuser == 0) 
                {
                    ss = strtok(s, ":");
                    ss = strtok(NULL, "\r\n");
                    if ( ss && *ss )
                    {
                        // added check to prevent GPF
                        strncpy(mh.toUserName, ss, 81);
                        mh.toUserName[80] = 0;
                        curuser = 0;
                        trimstr1(mh.toUserName);
                        find_name(mh.toUserName);
                    }
                } 
                else if ( mailuser > 0 && !towhom ) 
                {
                    curuser = mailuser;
                    if (!num_to_name(szEmptyString, mh.toUserName, curuser, use_alias)) 
                    {
                        curuser = 0;
                        mh.toUserName[0] = 0;
                        towhom = 1;
                    }
                }

                if ((stristr(mh.toUserName, "Multiple recipients of") != NULL) && (curuser != (unsigned) 65535L)) 
                {
                    for (i = 0; (i < nlists) && (nh.main_type == main_type_email); i++) 
                    {
                        if ((maillist[i].opttext) &&
                            (stristr(mh.toUserName, maillist[i].opttext) != NULL)) 
                        {
                            if (atoi(maillist[i].subtype)) 
                            {
                                nh.main_type = main_type_pre_post;
                                nh.minor_type = ( unsigned short ) atoi(maillist[i].subtype);
                            } 
                            else 
                            {
                                nh.main_type = main_type_new_post;
                                nh.minor_type = 0;
                                strcpy(alphatype, maillist[i].subtype);
                            }
                            strcpy(alphasubtype, maillist[i].subtype);
                            nh.touser = 0;
                        }
                    }
                }
                if ((curuser == (unsigned) 65535L) && (nh.main_type == main_type_email)) 
                {
                    strcpy(alphatype, alphasubtype);
                    nh.main_type = main_type_new_post;
                    nh.minor_type = 0;
                    nh.touser = 0;
                    tolist = 1;
                } 
                else if ((mh.toUserName[0] == 0) || (curuser == 0)) 
                {
                    nh.touser = defuser;
                    strcpy(mh.toUserName, postmaster);
                } 
                else 
                {
                    nh.touser = ( unsigned short ) curuser;
                }
            }
        } 
        else
        {
            done = 1;
        }
    }
    if (fp != NULL)
    {
        fclose(fp);
    }
    if (mailuser == -1) 
    {
        strcpy(alphasubtype, alphatype);
        curuser = (unsigned) 65535L;
    }
    trimstr1(mh.fromUserName);
    strcat(mh.fromUserName, "\r\n");
    log_it( true, "\n \xFE From    : %s", mh.fromUserName);
    if ( nh.main_type == main_type_pre_post ||
        nh.main_type == main_type_new_post ) 
    {
        nh.touser = 0;
        log_it( true, " \xFE Post to : Sub %s", alphasubtype);
        if (bounce) 
        {
            log_it( true, "\n \xFE Return Post from Listserv - not reposted.");
            free(p);
            return 0;
        }
        done = 0;
        for (i = 0; i < nlists; i++)
        {
            if (_strcmpi(alphasubtype, maillist[i].subtype) == 0)
            {
                done = 1;
            }
        }
        if (!done) 
        {
            sprintf(s, "%sM%s.NET", net_data, alphasubtype);
            if ((fp = fsh_open(s, "rb")) == NULL)
            {
                done = -1;
            }
            strcpy(s1, find_focus(realfrom));
            trimstr1(s1);
            while ( done == 0 && fgets(s, sizeof(s)-1, fp))
            {
                trimstr1(s);
                if (stristr(s, s1) != 0)
                {
                    done = 1;
                }
            }
            if (fp != NULL)
            {
                fclose(fp);
            }
        }
        if (done < 1) 
        {
            CreateCheckMessageName(pktname);
            rename(pszFileName, pktname);
            sprintf(s2, "\n \xFE No \"%s\" in M%s.NET... message %s not posted.",
                s1, alphasubtype, pktname);
            log_it( true, s2);
            ssm(s2);
            free(p);
            return 0;
        }
    } 
    else
    {
        log_it( true, " \xFE Sent to : %s #%hd", _strupr(mh.toUserName), nh.touser);
    }
    log_it( true, "\n \xFE Subject : %s", mh.subject);
    CreatePacketName(pktname);
    if ((fp = fsh_open(pktname, "wb")) == NULL) 
    {
        log_it( true, "\n \xFE Unable to create packet %s", pktname);
        free(p);
        return 1;
    }
    nh.length = textlen + strlen(mh.fromUserName) + strlen(mh.subject)
        + strlen(msgdate) + strlen(recvdate) + 1;
    if (nh.main_type == main_type_new_post)
    {
        nh.length += strlen(alphatype) + 1;
    }
    while (tolist >= 0) 
    {
        fsh_write(&nh, sizeof(net_header_rec), 1, fp);
        if (nh.main_type == main_type_new_post)
        {
            fsh_write(alphatype, sizeof(char), strlen(alphatype) +1, fp);
        }
        fsh_write(mh.subject, sizeof(char), strlen(mh.subject) +1, fp);
        fsh_write(mh.fromUserName, sizeof(char), strlen(mh.fromUserName), fp);
        fsh_write(msgdate, sizeof(char), strlen(msgdate), fp);
        fsh_write(recvdate, sizeof(char), strlen(recvdate), fp);
        reallen = fsh_write(p, sizeof(char), (int) textlen, fp);
        if (reallen != textlen)
        {
            log_it( true, "\n \xFE Expected %ld bytes, wrote %ld bytes.", textlen, reallen);
        }
        nh.tosys = 32767;
        --tolist;
    }
    if (fp != NULL)
    {
        fclose(fp);
    }
    free(p);
    return 0;
}


char *stripcolors(char *str)
{
//	char *nbuf;
	SEH_PUSH("stripcolors()");
	
	if (str) 
	{
		char *nbuf, *obuf;
		for (obuf = str, nbuf = str; *obuf; ++obuf) 
		{
			if (obuf && *obuf == 3)
			{
				++obuf;
			}
			else if (((*obuf < 32) && (*obuf != 9)) || (*obuf > 126))
			{
				continue;
			}
			else
			{
				*nbuf++ = *obuf;
			}
		}
		*nbuf = NULL;
	}
	return str;
}

void move_dead(net_header_rec * nh, char *text)
{
	char szDeadNetFileName[_MAX_PATH];
	SEH_PUSH("move_dead()");
	
	sprintf(szDeadNetFileName, "%sDEAD.NET", net_data);
	int hDeadNetFile = sh_open(szDeadNetFileName, O_RDWR | O_BINARY | SH_DENYRW | O_CREAT, S_IREAD | S_IWRITE);
	if (hDeadNetFile > 0) 
	{
		_lseek(hDeadNetFile, 0L, SEEK_END);
		long l, l1;
		l = l1 = _tell(hDeadNetFile);
		_write(hDeadNetFile, (void *) nh, sizeof(net_header_rec));
		l1 += sizeof(net_header_rec);
		_write(hDeadNetFile, (void *) text, (int) (nh->length));
		l1 += nh->length;
		if (l1 != _tell(hDeadNetFile))
		{
			_chsize(hDeadNetFile, l);
		}
		hDeadNetFile = sh_close(hDeadNetFile);
	} 
	else
	{
		log_it( true, "\n ! Couldn't open '%s'", szDeadNetFileName);
	}
}

void get_subtype(int sub, char *where)
{
	char szFileName[181], s[81], net_name[21], *ss;
	SEH_PUSH("get_subtype()");
	
	where[0] = '\0';
	sprintf(szFileName, "%sSUBS.XTR", syscfg.datadir);
	FILE *fp;
	if ((fp = fsh_open(szFileName, "r")) == NULL)
	{
		return;
	}
	bool ok = false;
	while (fgets(s, sizeof(s)-1, fp)) 
	{
		if (s && *s == '!') 
		{
			if (sub == atoi(&s[1]))
			{
				ok = true;
			}
		}
		if (ok && (*s == '$')) 
		{
			ss = strtok(s, " ");
			strcpy(net_name, &ss[1]);
			ss = strtok(NULL, " ");
			if (fp != NULL)
			{
				fclose(fp);
			}
			if ((_stricmp(net_name, "FILENET") == 0)) 
			{
				trimstr1(ss);
				strcpy(where, ss);
				return;
			} 
			else
			{
				return;
			}
		}
	}
	if (fp != NULL)
	{
		fclose(fp);
	}
}

unsigned find_anony(char *stype)
{
	char szFileName[_MAX_PATH], subtype[12];
	subboardrec sub;
	FILE *fp;
	SEH_PUSH("find_anony()");
	
	int result = 0;
	sprintf(szFileName, "%sSUBS.DAT", syscfg.datadir);
	if ((fp = fsh_open(szFileName, "rb")) == NULL) 
	{
		log_it( true, "\n \xFE Unable to _read %s.", szFileName);
		return 0;
	} 
	else 
	{
		fseek(fp, 0L, SEEK_END);
		int num_subs = (int) (ftell(fp) / sizeof(subboardrec));
		bool found = false;
		output("    ");
		for (int i = 0; (i < num_subs) && (!found); i++) 
		{
			output("\b\b\b\b%-4d", i);
			fseek(fp, (long) i * sizeof(subboardrec), SEEK_SET);
			fread(&sub, sizeof(subboardrec), 1, fp);
			get_subtype(i, subtype);
			if (_stricmp(subtype, stype) == 0) 
			{
				int anony = sub.anony & 0x0f;
				output("\b\b\b\b%s - ", sub.name);
				switch (anony) 
				{
				case anony_force_anony:
					output("forced anonymous.");
					result = 2;
					break;
				case 0:
					output("aliases.");
					result = 1;
					break;
				default:
					output("real names.");
					result = 0;
					break;
				}
				found = true;
			}
		}
		if (!found)
		{
			output("\b\b\b\bnot found.");
		}
	}
	fclose(fp);
	return result;
}

int ExportMessages(char *pszFileName)
{
	char fn1[121], tagfn[121], groupname[81], outfn[121], savefn[121], _temp_buffer[256], acct_addr[80], list_addr[80];
	char *ss = NULL, *buffer = NULL, *text = NULL, mytype[12], alphatype[21], hold[21], tempoutfn[21], savename[81], savesubj[81];
	char references[255], reftest[255];
	unsigned stype, ttype, rime;
	int infile, outfile, savefile = -1, inloc, outloc, term, a = -1, i, j, k, ns, i6, tolist, forlist, hdr;
	bool ok;
	net_header_rec nhr;
	struct msghdr mh;
	struct tm *time_msg;
	FILE *fp;
	time_t some;
	char *main_type[] =
	{
		"Network Update", 
		"email by usernum", 
		"post from sub host", 
		"file",
		"post to sub host", 
		"external message", 
		"email by name",
		"NetEdit message",
		"SUBS.LST", 
		"Extra Data", 
		"BBSLIST from GC",
		"CONNECT from GC", 
		"Unused_1", 
		"Info from GC", 
		"SSM", 
		"Sub Add Request",
		"Sub Drop Request", 
		"Sub Add Response", 
		"Sub Drop Response", 
		"Sub Info",
		"Unused 1", 
		"Unused 2", 
		"Unused 3", 
		"Unused 4", 
		"Unused 5", 
		"new post",
		"new external"
	};
	SEH_PUSH("Export()");
	
	if ((infile = sh_open1(pszFileName, O_RDONLY | O_BINARY)) == -1)
	{
		return 1;
	}
	
	if ((buffer = (char *) malloc(32 * 1024)) == NULL) 
	{
		sh_close(infile);
		log_it( true, "\n \xFE Out of memory allocating input buffer!");
		return 1;
	}
	if ((text = (char *) malloc(32 * 1024)) == NULL) 
	{
		log_it( true, "\n \xFE Out of memory allocating output buffer!");
		sh_close(infile);
		if (buffer != NULL)
		{
			free(buffer);
		}
		return 1;
	}
	while (sh_read(infile, &nhr, sizeof(nhr))) 
	{
		strcpy(tagfile, deftagfile);
		*alphasubtype = 0;
		rime = 0;
		sh_read(infile, buffer, (int) nhr.length);
		if (nhr.tosys != 32767) 
		{
			log_it( true, "\n \xFE System @%hd routing through @32767... moving to DEAD.NET.",
				nhr.fromsys);
			move_dead(&nhr, buffer);
			continue;
		}
		tolist = forlist = 0;
		hdr = 1;
		if (nhr.main_type == main_type_pre_post)
		{
			nhr.main_type = main_type_post;
		}
		if ((nhr.main_type == main_type_post) ||
			(nhr.main_type == main_type_new_post) ||
			(nhr.main_type == main_type_email_name) ||
			(nhr.main_type == main_type_ssm)) 
		{
			inloc = 0;
			sprintf(hold, "%hu", nhr.minor_type);
			if ((nhr.main_type == main_type_email_name) ||
				(nhr.main_type == main_type_ssm) ||
				(nhr.main_type == main_type_new_post)) 
			{
				stype = nhr.minor_type;
				inloc = strlen(buffer) + 1;
				if ((nhr.main_type == main_type_new_post) || ((nhr.fromsys != g_nNetworkSystemNumber) &&
					(nhr.fromsys != 32767))) 
				{
					strcpy(alphasubtype, buffer);
					strcpy(hold, alphasubtype);
				} 
				else
				{
					strcpy(mh.toUserName, buffer);
				}
			} 
			else if (nhr.main_type == main_type_post) 
			{
				stype = nhr.minor_type;
			} 
			else 
			{
				stype = atoi(&buffer[inloc]);
				sprintf(_temp_buffer, "%u", stype);
				inloc += strlen(_temp_buffer) + 1;
			}
				
			strncpy(mh.subject, &buffer[inloc], sizeof(mh.subject));
			stripcolors(mh.subject);
			inloc += strlen(&buffer[inloc]) + 1;
			
			for (term = inloc; buffer[term] != '\r'; term++)
				;
			buffer[term] = '\0';
			
			*acct_addr = 0;
			if ((nhr.fromsys == g_nNetworkSystemNumber) && (nhr.fromuser)) 
			{
				*acct_addr = 1;
				*mytype = 0;
				if (nhr.main_type == main_type_new_post)
				{
					strcpy(mytype, hold);
				}
				if (nhr.main_type == main_type_post)
				{
					sprintf(mytype, "%hu", nhr.minor_type);
				}
				if ((*mytype) && (!tolist)) 
				{
					output("\n \xFE Subtype : %s - ", mytype);
					a = find_anony(mytype);
				} 
				else
				{
					a = use_alias;
				}
				if (!num_to_name(acct_addr, mh.fromUserName, nhr.fromuser, a)) 
				{
					log_it( true, "\n \xFE No match for user #%hd... skipping message!", nhr.fromuser);
					continue;
				}
			} 
			else 
			{
				strncpy(mh.fromUserName, &buffer[inloc], sizeof(mh.fromUserName));
				stripcolors(mh.fromUserName);
				strtok(mh.fromUserName, "#");
				trimstr1(mh.fromUserName);
				if (nhr.fromsys != 32767) 
				{
					sprintf(_temp_buffer, " #%hd @%hu", nhr.fromuser, nhr.fromsys);
					strcat(mh.fromUserName, _temp_buffer);
				}
				/* Gating code will go here */
			}
			
			inloc = term + 2;
			
			while (buffer[inloc] != '\n')
			{
				inloc++;
			}
			inloc++;
			
			/* Look here */
			if (_strnicmp(&buffer[inloc], "RE: ", 4) == 0) 
			{
				for (term = inloc; buffer[term] != '\r'; term++);
				buffer[term] = '\0';
				strncpy(mh.subject, &buffer[inloc + 4], sizeof(mh.subject));
				if (_strnicmp(mh.subject, "RE: ", 4) != 0) 
				{
					strcpy(_temp_buffer, "Re: ");
					strcat(_temp_buffer, mh.subject);
				}
				strcpy(mh.subject, _temp_buffer);
				inloc = term + 2;
			}
			if ((strncmp(&buffer[inloc], "BY: ", 4) == 0) ||
				(strncmp(&buffer[inloc], "TO: ", 4) == 0)) 
			{
				for (term = inloc; buffer[term] != '\r'; term++);
				buffer[term] = '\0';
				strncpy(mh.toUserName, &buffer[inloc + 4], sizeof(mh.toUserName));
				stripcolors(mh.toUserName);
				if (strcspn(mh.toUserName, "<") != strlen(mh.toUserName)) 
				{
					if ((strstr(mh.toUserName, "\"") == 0) && (strstr(mh.toUserName, "(") == 0)) 
					{
						ss = strtok(mh.toUserName, "<");
						ss = strtok(NULL, ">");
						strcpy(mh.toUserName, ss);
					}
				}
				inloc = term + 2;
			} 
			else 
			{
				if (nhr.main_type != main_type_email_name) 
				{
					strcpy(mh.toUserName, "ALL");
				}
			}
			outloc = 0;
			do 
			{
				if (buffer[inloc] == 2) 
				{
					i = inloc + 1;
					for (j = 1; j < 255; j++) 
					{
						if (buffer[i] == 'ã')
						{
							buffer[i] = '\r';
						}
						if ((buffer[i] == '\r') || (i > (int) nhr.length))
						{
							break;
						}
						i++;
					}
					if (j < 80) 
					{
						i = (80 - j) / 2;
						for (j = 1; j <= i; j++)
						{
							text[outloc++] = ' ';
						}
					}
					inloc++;
				} else if (buffer[inloc] == 3)
				{
					inloc += 2;
				}
				else if ((buffer[inloc] == 124) && (isdigit(buffer[inloc + 1])) && (isdigit(buffer[inloc + 2])))
				{
					inloc += 3;
				}
				else if ((buffer[inloc] == 4) && (isdigit(buffer[inloc + 1]))) 
				{
					i = inloc;
					k = 0;
					for (j = 1; j < 255; j++) 
					{
						reftest[k++] = buffer[i];
						if ((buffer[i] == '\r') || (i > (int) nhr.length))
						{
							break;
						}
						i++;
						inloc++;
					}
					inloc++;
					reftest[k] = '\0';
					// Don't remove newlines          if (buffer[inloc] == '\n')
					//            inloc++;
					references[0] = 0;
					if (stristr(reftest, "References")) 
					{
						ss = strtok(reftest, ":");
						ss = strtok(NULL, "\r\n");
						if (ss) 
						{
							strcpy(references, ss);
							trimstr1(references);
							fprintf(stderr, "\n \xFE References: %s", references);
						}
					}
				} 
				else if (buffer[inloc] >= 127)
				{
					inloc++;
				}
				else if (buffer[inloc] == 1)
				{
					inloc++;
				}
				else
				{
					text[outloc++] = buffer[inloc++];
				}
			} while (inloc < (int) nhr.length);
			
			text[outloc] = '\0';
			
			if ((nhr.main_type == main_type_post) ||
				(nhr.main_type == main_type_pre_post) ||
				(nhr.main_type == main_type_new_post)) 
			{
				if (nhr.main_type == main_type_new_post) 
				{
					sprintf(fn1, "%sM%s.NET", net_data, alphasubtype);
					if (exist(fn1)) 
					{
						tolist = 1;
						nhr.main_type = main_type_email_name;
						get_list_addr(list_addr, alphasubtype);
					}
				}
				for (i = 0; (i < nlists) && (nhr.main_type != main_type_email_name); i++) 
				{
					if (nhr.main_type == main_type_new_post) 
					{
						if (_strcmpi(alphasubtype, maillist[i].subtype) == 0) 
						{
							nhr.main_type = main_type_email_name;
							forlist = 1;
							strcpy(mh.toUserName, maillist[i].ownername);
						}
					} 
					else 
					{
						ttype = atoi(maillist[i].subtype);
						if (ttype == stype) 
						{
							nhr.main_type = main_type_email_name;
							forlist = 1;
							strcpy(mh.toUserName, maillist[i].ownername);
						}
					}
				}
			}
			switch (nhr.main_type) 
			{
			case main_type_email:
			case main_type_email_name:
			case main_type_ssm:
				i = 1;
				if ((DIGEST) && (tolist)) 
				{
					sprintf(outfn, "%sMQUEUE\\DIGEST\\%s.%d", net_data, alphasubtype, jdater);
					if (exist(outfn))
					{
						hdr = 0;
					}
				} 
				else 
				{
					sprintf(outfn, "%sMQUEUE\\MSG.%d", net_data, i);
					while (exist(outfn))
					{
						sprintf(outfn, "%sMQUEUE\\MSG.%d", net_data, ++i);
					}
					i = 1;
					sprintf(savefn, "%sSENT\\MSG%04d.SNT", net_data, i);
					while (exist(savefn))
					{
						sprintf(savefn, "%sSENT\\MSG%04d.SNT", net_data, ++i);
					}
				}
				break;
			case main_type_new_post:
			case main_type_post:
			case main_type_pre_post:
				i = 1;
				strcpy(tempoutfn, hold);
				sprintf(outfn, "%sOUTBOUND\\%s.%d", net_data, tempoutfn, i);
				while (exist(outfn))
				{
					sprintf(outfn, "%sOUTBOUND\\%s.%d", net_data, tempoutfn, ++i);
				}
				break;
			default:
				continue;
			}
			
			log_it( true, "\n \xFE Creating: %s", outfn);
			if (nhr.fromsys != g_nNetworkSystemNumber) 
			{
				log_it( true, "\n \xFE From    : %s", mh.fromUserName);
			} 
			else 
			{
				if (usermail) 
				{
					if ((*acct_addr) && (!forlist))
					{
						log_it( true, "\n \xFE From    : \"%s\" <%s>", mh.fromUserName, acct_addr);
					}
					else
					{
						log_it( true, "\n \xFE From    : \"%s\" <%s@%s>", mh.fromUserName, POPNAME, DOMAIN);
					}
				} 
				else
				{
					log_it( true, "\n \xFE From    : <%s@%s>", POPNAME, DOMAIN);
				}
			}
			if ((nhr.main_type == main_type_post) ||
				(nhr.main_type == main_type_pre_post) ||
				(nhr.main_type == main_type_new_post)) 
			{
				sprintf(fn1, "%sNEWS.RC", net_data);
				if (exist(fn1)) 
				{
					if ((fp = fsh_open(fn1, "rt")) == NULL) 
					{
						log_it( true, "\n \xFE %s not found!", fn1);
						sh_close(infile);
						if (text)
						{
							free(text);
						}
						if (buffer)
						{
							free(buffer);
						}
						return 1;
					} 
					else 
					{
						ok = false;
						while ( (fgets(_temp_buffer, sizeof(_temp_buffer)-1, fp) != NULL) && !ok ) 
						{
							groupname[0] = 0;
							ss = strtok(_temp_buffer, " ");
							if (ss) 
							{
								strcpy(groupname, ss);
								ss = strtok(NULL, " ");
								ss = strtok(NULL, "\r");
								if (nhr.main_type == main_type_new_post) 
								{
									strcpy(alphatype, ss);
									if (_strnicmp(alphasubtype, alphatype, strlen(alphasubtype)) == 0)
									{
										ok = true;
									}
								} 
								else 
								{
									ttype = atoi(ss);
									if (ttype == stype)
									{
										ok = true;
									}
								}
							}
						}
						*ss = NULL;
						if (fp != NULL)
						{
							fclose(fp);
						}
					}
				} 
				else 
				{
					ok = false;
					for (k = 0; k < 10 && !ok; k++) 
					{
						sprintf(fn1, "%sNEWS%d", net_data, k);
						strcat(fn1, ".RC");
						if (exist(fn1)) 
						{
							fp = fsh_open(fn1, "rt");
							while ((fgets(_temp_buffer, 80, fp) != NULL) && !ok) 
							{
								groupname[0] = 0;
								ss = strtok(_temp_buffer, " ");
								if (ss) 
								{
									strcpy(groupname, ss);
									ss = strtok(NULL, " ");
									ss = strtok(NULL, "\r");
									if (nhr.main_type == main_type_new_post) 
									{
										strcpy(alphatype, ss);
										if (_strnicmp(alphasubtype, alphatype, strlen(alphasubtype)) == 0)
										{
											ok = true;
										}
									} 
									else 
									{
										ttype = atoi(ss);
										if (ttype == stype)
										{
											ok = true;
										}
									}
								}
							}
							*ss = NULL;
							if (fp != NULL)
							{
								fclose(fp);
							}
						} 
						else
						{
							fprintf(stderr, "\n ! %s not found.", fn1);
						}
					}
					if ((_strnicmp(groupname, "rime.", 5) == 0) ||
						(_strnicmp(groupname, "prime.", 6) == 0)) 
					{
						rime = 1;
						sprintf(tagfn, "%sRIME.TAG", syscfg.gfilesdir);
						if (exist(tagfn))
						{
							strcpy(tagfile, tagfn);
						}
						tagfn[0] = 0;
					}
				}
				if (!ok) 
				{
					if (nhr.main_type != main_type_new_post)
					{
						sprintf(alphatype, "%u", stype);
					}
					log_it( true, "\n \xFE Subtype %s not found in NEWS.RC!", alphatype);
					sh_close(infile);
					if (text)
					{
						free(text);
					}
					if (buffer)
					{
						free(buffer);
					}
					return 1;
				}
			}
			if ((nhr.main_type == main_type_email) ||
				(nhr.main_type == main_type_email_name) ||
				(nhr.main_type == main_type_ssm)) 
			{
				if (tolist)
				{
					log_it( true, "\n \xFE Sent to : %s Mailing List", alphasubtype);
				}
				else
				{
					log_it( true, "\n \xFE Rcpt to : %s", mh.toUserName);
				}
			} 
			else
			{
				log_it( true, "\n \xFE Post to : %s", groupname);
			}
			
			strcpy(_temp_buffer, mh.subject);
			j = 0;
			for (i = 0; i < (int) strlen(mh.subject); i++) 
			{
				if (_temp_buffer[i] == 3)
				{
					++i;
				}
				else
				{
					mh.subject[j++] = _temp_buffer[i];
				}
			}
			mh.subject[j] = '\0';
			
			log_it( true, "\n \xFE Subject : %s", mh.subject);
			
			if ((nhr.main_type == main_type_email) ||
				(nhr.main_type == main_type_email_name) ||
				(nhr.main_type == main_type_ssm)) 
			{
				if (tolist) 
				{
					sprintf(fn1, "%sM%s.NET", net_data, alphasubtype);
					int hFile = sh_open1(fn1, O_RDONLY | O_BINARY);
					if (_filelength(hFile) <= 0) 
					{
						log_it( true, "\n \xFE Mailing list %s has no subscribers.", alphasubtype);
						hFile = sh_close(hFile);
						continue;
					}
					hFile = sh_close(hFile);
				}
			}
			outfile = sh_open(outfn, O_RDWR | O_BINARY | O_CREAT | O_APPEND, S_IWRITE);
			if (outfile == -1) 
			{
				sh_close(infile);
				if (buffer)
				{
					free(buffer);
				}
				if (text)
				{
					free(text);
				}
				log_it( true, "\n \xFE Unable to open output file %s", outfn);
				return 1;
			}
			if (!tolist)
			{
				savefile = sh_open(savefn, O_RDWR | O_BINARY | O_CREAT | O_APPEND, S_IWRITE);
			}
			time(&some);
			time_msg = localtime(&some);
			strftime(mh.dateTime, 80, "%a, %d %b 20%y %H:%M:%S %Z", time_msg);
			if (!hdr) 
			{
				sprintf(_temp_buffer, "\n\n---- Next Message ----\n\n");
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
			}
			if ((nhr.fromsys == 32767) || (rime)) 
			{
				//        fprintf(stderr, "\nDBG: RelayNet(tm) message...");
				sprintf(_temp_buffer, "From: %s\n", _strupr(mh.fromUserName));
				rime = 0;
			} 
			else if (nhr.fromsys != g_nNetworkSystemNumber) 
			{
				sprintf(_temp_buffer, "From: \"%s\" <%s@%s>\n", mh.fromUserName, POPNAME, DOMAIN);
			} 
			else 
			{
				if (spam && ((nhr.main_type == main_type_post) || (nhr.main_type == main_type_new_post))) 
				{
					if ((!*acct_addr) || (forlist)) 
					{
						if (spamname[0] == 0)
						{
							sprintf(_temp_buffer, "From: \"%s\" <%s@dont.spam.%s>\n",
							mh.fromUserName, POPNAME, DOMAIN);
						}
						else
						{
							sprintf(_temp_buffer, "From: \"%s\" <%s>\n", mh.fromUserName, spamname);
						}
					} 
					else
					{
						sprintf(_temp_buffer, "From: \"%s\" <%s.NOSPAM>\n", mh.fromUserName, acct_addr);
					}
				} 
				else 
				{
					if (usermail) 
					{
						if ((*acct_addr) && (!forlist))
						{
							sprintf(_temp_buffer, "From: \"%s\" <%s>\n",
							mh.fromUserName, acct_addr);
						}
						else
						{
							sprintf(_temp_buffer, "From: \"%s\" <%s@%s>\n",
							mh.fromUserName, POPNAME, DOMAIN);
						}
					} 
					else
					{
						sprintf(_temp_buffer, "From: <%s@%s>\n", POPNAME, DOMAIN);
					}
				}
			}
			if ((tolist) && (DIGEST)) 
			{
				strcpy(savename, mh.fromUserName);
				sprintf(_temp_buffer, "From: \"%s Mailing List\" <%s@%s>\n",
					alphasubtype, POPNAME, DOMAIN);
			}
			++cur_daten;
			if (hdr) 
			{
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
				if ((!tolist) && (savefile != -1))
				{
					sh_write(savefile, _temp_buffer, strlen(_temp_buffer));
				}
				sprintf(_temp_buffer, "Message-ID: <%lx-%s@wwivbbs.org>\n", cur_daten, POPNAME);
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
			}
			if ((nhr.main_type == main_type_email) ||
				(nhr.main_type == main_type_email_name) ||
				(nhr.main_type == main_type_ssm)) 
			{
				if (!tolist) 
				{
					sprintf(_temp_buffer, "To: %s\n", mh.toUserName);
					sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
					if ((!tolist) && (savefile != -1))
					{
						sh_write(savefile, _temp_buffer, strlen(_temp_buffer));
					}
				} 
				else 
				{
					i = 0;
					if (hdr) 
					{
						sprintf(fn1, "%sM%s.NET", net_data, alphasubtype);
						if ((fp = fsh_open(fn1, "rt")) != NULL) 
						{
							while (fgets(_temp_buffer, sizeof(_temp_buffer)-1, fp) != NULL) 
							{
								if (strstr(_temp_buffer, "@")) 
								{
									strcpy(mh.toUserName, _temp_buffer);
									sprintf(_temp_buffer, "To: %s", mh.toUserName);
									if (_temp_buffer[strlen(_temp_buffer) - 1] != '\n')
									{
										strcat(_temp_buffer, "\n");
									}
									sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
									i = 1;
								}
							}
						}
						if (fp != NULL)
						{
							fclose(fp);
						}
						if (!i) 
						{
							sh_close(infile);
							sh_close(outfile);
							if ( savefile != -1 )
							{
								sh_close(savefile);
							}
							if (buffer)
							{
								free(buffer);
							}
							if (text)
							{
								free(text);
							}
							log_it( true, "\n \xFE Error processing mailing list %s.", alphasubtype);
							return 1;
						} 
						else 
						{
							if (list_addr && *list_addr)
							{
								sprintf(_temp_buffer, "Reply-To: \"%s\" <%s>\n", alphasubtype,
								list_addr);
							}
							else
							{
								sprintf(_temp_buffer, "Reply-To: \"%s\" <%s@%s>\n", alphasubtype,
								POPNAME, DOMAIN);
							}
							sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
						}
					}
				}
			} 
			else 
			{
				if (references[0] != 0) 
				{
					sprintf(_temp_buffer, "References: %s\n", references);
					sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
					references[0] = 0;
				}
				sprintf(_temp_buffer, "Newsgroups: %s\n", groupname);
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
			}
			sprintf(_temp_buffer, "Subject: %s\n", mh.subject);
			if ((DIGEST) && (tolist)) 
			{
				strcpy(savesubj, _temp_buffer);
				sprintf(_temp_buffer, "Subject: %s Daily Digest, Day %d\n", alphasubtype, jdater);
			}
			if (hdr) 
			{
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
				if ((!tolist) && (savefile != -1))
				{
					sh_write(savefile, _temp_buffer, strlen(_temp_buffer));
				}
				sprintf(_temp_buffer, "MIME-Version: 1.0\n");
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
				sprintf(_temp_buffer, "Content-Type: text/plain; charset=us-ascii\n");
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
				sprintf(_temp_buffer, "Content-Transfer-Encoding: 7bit\n");
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
			}
			sprintf(_temp_buffer, "Date: %s\n", mh.dateTime);
			sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
			if ((!tolist) && (savefile != -1))
			{
				sh_write(savefile, _temp_buffer, strlen(_temp_buffer));
			}
			if (nhr.main_type != main_type_email_name) 
			{
				if (hdr) 
				{
					if (a == 2)
					{
						sprintf(_temp_buffer, "Path: anonymous!wwivbbs.org\n");
					}
					else
					{
						sprintf(_temp_buffer, "Path: %s!%s\n", POPNAME, DOMAIN);
					}
					sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
					if (a == 2)
					{
						sprintf(_temp_buffer, "Organization: WWIV BBS\n");
					}
					else
					{
						sprintf(_temp_buffer, "Organization: %s * %s\n",
						syscfg.systemname, syscfg.systemphone);
					}
					sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
				}
			}
			if (tolist) 
			{
				if (hdr) 
				{
					if (list_addr && *list_addr)
					{
						sprintf(_temp_buffer, "X-Reply-To: \"%s\" <%s>\n",
						alphasubtype, list_addr);
					}
					else
					{
						sprintf(_temp_buffer, "X-Reply-To: \"%s\" <%s@%s>\n",
						alphasubtype, POPNAME, DOMAIN);
					}
					sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
					sprintf(_temp_buffer, "Source: %s Mail List\n", alphasubtype);
					sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
				}
			}
			if ((((nhr.main_type == main_type_post) || (nhr.main_type == main_type_new_post)) &&
				(!tolist) && (a != 2)) || ((*acct_addr) && (forlist))) 
			{
				if (!*acct_addr) 
				{
					if (REPLYTO && *REPLYTO)
					{
						sprintf(_temp_buffer, "Reply-to: \"%s\" <%s>\n", mh.fromUserName, REPLYTO);
					}
					else
					{
						sprintf(_temp_buffer, "Reply-to: \"%s\" <%s@%s>\n",
						mh.fromUserName, POPNAME, DOMAIN);
					}
				} 
				else
				{
					sprintf(_temp_buffer, "Reply-to: \"%s\" <%s>\n", mh.fromUserName, acct_addr);
				}
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
			}
			if (hdr) 
			{
				sprintf(_temp_buffer, "Version: WWIV Internet Network Support %s\n\n", VERSION);
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
			}
			if ((DIGEST) && (tolist)) 
			{
				sprintf(_temp_buffer, "%s", savesubj);
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
				sprintf(_temp_buffer, "From: %s\n\n", savename);
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
			}
			if ((nhr.main_type != main_type_email) &&
				(nhr.main_type != main_type_email_name) &&
				(strncmp(mh.toUserName, "ALL", 3) != 0)) 
			{
				sprintf(_temp_buffer, "Responding to: %s\n", mh.toUserName);
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
				if ((!tolist) && (savefile != -1))
				{
					sh_write(savefile, _temp_buffer, strlen(_temp_buffer));
				}
			}
			for (i = 0; i < (int) strlen(text); i++)
			{
				if (text[i] == 'ã')
				{
					text[i] = '\n';
				}
			}
				
			sh_write(outfile, text, strlen(text));
			if ((!tolist) && (savefile != -1))
			{
				sh_write(savefile, text, strlen(text));
			}
			if ((tolist) && (DIGEST)) 
			{
				sprintf(_temp_buffer, "\n\n");
				sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
			} 
			else 
			{
				sprintf(tagfn, "%sI%s.TAG", syscfg.gfilesdir, alphasubtype);
				if (exist(tagfn))
				{
					strcpy(tagfile, tagfn);
				}
				tagfn[0] = 0;
				ns = 0;
				for (i6 = 0; i6 < 99; i6++) 
				{
					sprintf(tagfn, "%sI%s.T%d", syscfg.gfilesdir, alphasubtype, i6);
					if (exist(tagfn))
					{
						ns++;
					}
					else
					{
						break;
					}
				}
				srand( cur_daten );
				sprintf( tagfn, "%sI%s.T%d", syscfg.gfilesdir, alphasubtype, ( rand() % (ns-1) ) );
				if (exist(tagfn))
				{
					strcpy(tagfile, tagfn);
				}
				if (tagfile[0] == 0) 
				{
					sprintf(_temp_buffer, "\n\nOrigin: %s * %s\n\n",
						syscfg.systemname, syscfg.systemphone);
					sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
				} 
				else 
				{
					if ((fp = fsh_open(tagfile, "rt")) == NULL)
					{
						log_it( true, "\n \xFE Error reading %s.", tagfile);
					}
					else 
					{
						sprintf(_temp_buffer, "\n\n");
						sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
						while (fgets(_temp_buffer, 120, fp)) 
						{
							for (i = 0; ((i < (int) strlen(_temp_buffer)) &&
								(_temp_buffer[i] != '\r') && (_temp_buffer[i] != '\n')); i++)
							{
								if ((_temp_buffer[i] < 32) || (_temp_buffer[i] > 126))
								{
									_temp_buffer[i] = 32;
								}
							}
							sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
						}
					}
					if (fp != NULL)
					{
						fclose(fp);
					}
					sprintf(_temp_buffer, "\n\n");
					sh_write(outfile, _temp_buffer, strlen(_temp_buffer));
				}
			}
			sh_close(outfile);
			sh_close(savefile);
		} 
		else 
		{
			if ((nhr.main_type >= 0x01) && (nhr.main_type <= 0x1b))
			{
				log_it( true, "\n \xFE %s message skipped", main_type[nhr.main_type - 1]);
			}
			else
			{
				log_it( true, "\n \xFE Unknown Main_type %hd skipped", nhr.main_type);
			}
		}
	}
	sh_close(infile);
	_unlink(pszFileName);
	if (text)
	{
		free(text);
	}
	if (buffer)
	{
		free(buffer);
	}
	return 0;
}



/*
  action: 1 = unsubscribed,
          2 = subscribed,
          3 = already subscribed,
          4 = not subscribed
          5 = invalid list
          6 = sending MAILLIST.TXT
*/

void send_note(int action, char *mailname, char *listname)
{
	char s[81], szFileName[_MAX_PATH];
	FILE *fp, *rlz;
	SEH_PUSH("send_note()");
	
	int i = 1;
	sprintf(szFileName, "%sMQUEUE\\MSG.%d", net_data, i);
	while (exist(szFileName))
	{
		sprintf(szFileName, "%sMQUEUE\\MSG.%d", net_data, ++i);
	}
	
	if ((fp = fsh_open(szFileName, "wt+")) == NULL) 
	{
		log_it( true, "\n ! Unable to create %s.", szFileName);
		return;
	}
	if (action != 5)
	{
		fprintf(fp, "From: \"%s\" <%s@%s>\n", listname, POPNAME, DOMAIN);
	}
	else
	{
		fprintf(fp, "From: <%s@%s>\n", POPNAME, DOMAIN);
	}
	fprintf(fp, "To: %s\n", mailname);
	fprintf(fp, "Subject: %s Mailing List\n\n", listname);
	
	switch (action) 
	{
    case 1:
		sprintf(s, "%s removed from mailing list: %s", mailname, listname);
		fprintf(fp, "\n%s\n\n", s);
		log_it( true, "\n \xFE %s", s);
		ssm(s);
		break;
    case 2:
		sprintf(szFileName, "%sR%s.RLZ", net_data, listname);
		if (!exist(szFileName))
		{
			sprintf(szFileName, "%sGLOBAL.RLZ", net_data);
		}
		if ((rlz = fsh_open(szFileName, "rt")) != NULL) 
		{
			while (fgets(s, sizeof(s)-1, rlz))
			{
				fprintf(fp, s);
			}
			fprintf(fp, "\n\n");
			fclose(rlz);
			sprintf(s, "%s added to mailing list: %s", mailname, listname);
			log_it( true, "\n%s", s);
			ssm(s);
		} 
		else 
		{
			sprintf(s, "%s added to mailing list: %s", mailname, listname);
			fprintf(fp, "\n%s\n\n", s);
			log_it( true, "\n \xFE %s", s);
			ssm(s);
		}
		break;
    case 3:
		sprintf(szFileName, "%sR%s.RLZ", net_data, listname);
		if (!exist(szFileName))
		{
			sprintf(szFileName, "%sGLOBAL.RLZ", net_data);
		}
		if ((rlz = fsh_open(szFileName, "rt")) != NULL) 
		{
			while (fgets(s, sizeof(s)-1, rlz))
			{
				fprintf(fp, s);
			}
			fprintf(fp, "\n\n");
			fclose(rlz);
		} 
		else
		{
			fprintf(fp, "\n%s was already subscribed to mailing list:\n\n   %s\n\n",
                mailname, listname);
		}
		break;
    case 4:
		fprintf(fp, "\n%s not subscribed to mailing list:\n\n   %s\n\n",
			mailname, listname);
		break;
    case 5:
		sprintf(s, "%s requested an invalid mailing list: %s", mailname, listname);
		fprintf(fp, "\n%s\n\n", s);
		log_it( true, "\n \xFE %s", s);
		ssm(s);
		break;
    case 6:
		sprintf(szFileName, "%sMAILLIST.TXT", syscfg.gfilesdir);
		if ((rlz = fsh_open(szFileName, "rt")) != NULL) 
		{
			while (fgets(s, sizeof(s)-1, rlz))
			{
				fprintf(fp, s);
			}
			fprintf(fp, "\n\n");
			fclose(rlz);
			sprintf(s, "Sent MAILLIST.TXT to %s.", mailname);
		} 
		else
		{
			sprintf(s, "No mailing list file found.  Notify system operator.");
		}
		fprintf(fp, "\n%s\n\n", s);
		log_it( true, "\n \xFE %s", s);
		ssm(s);
		break;
	}
	if (fp != NULL)
	{
		fclose(fp);
	}
}


bool okfn(char *s)
{
	SEH_PUSH("okfn()");
	
	if ( !s || !*s )
	{
		return false;
	}
	if ((s[0] == '-') || (s[0] == ' ') || (s[0] == '.') || (s[0] == '@'))
	{
		return false;
	}
	int l = strlen(s);
	for (int i = 0; i < l; i++) 
	{
		unsigned char ch = s[i];
		if ((ch == ' ') || (ch == '/') || (ch == '\\') || (ch == ':') ||
			(ch == '>') || (ch == '<') || (ch == '|') || (ch == '+') ||
			(ch == ',') || (ch == ';') || (ch == '^') || (ch == '\"') ||
			(ch == '\'') || (ch > 126))
		{
			return false;
		}
	}
	
	return true;
}

int subscribe(char *pszFileName)
{
	char *ss, s[161], s1[161], mailname[81], corename[81], subtype[81];
	FILE *fp, *oldfp, *newfp;
	SEH_PUSH("subscribe()");
	
	if ((fp = fsh_open(pszFileName, "rt")) == NULL) 
	{
		log_it( true, "\n ! Unable to open %s.", pszFileName);
		return 1;
	}
	bool done = false, unsubscribe = false;
	
	while (fgets(s1, sizeof(s)-1, fp) && !done) 
	{
		if ((s1[0] == '') && (s1[3] != 0))
		{
			strcpy(s, &(s1[3]));
		}
		else
		{
			strcpy(s, s1);
		}
		if (_strnicmp(s, "from:", 5) == 0) 
		{
			ss = strtok(s, ":");
			if (ss) 
			{
				ss = strtok(NULL, "\r\n");
				trimstr1(ss);
				strcpy(mailname, ss);
				if (strstr(ss, "<") != 0) 
				{
					ss = strtok(ss, "<");
					ss = strtok(NULL, ">");
					if (ss)
					{
						strcpy(corename, ss);
					}
				}
			}
		}
		if (_strnicmp(s, "subject:", 8) == 0) 
		{
			done = true;
			ss = strtok(s, ":");
			if (ss) 
			{
				ss = strtok(NULL, "\r\n");
				trimstr1(ss);
				strcpy(s1, ss);
				if (_strnicmp(s1, "subscribe", 9) == 0) 
				{
					ss = strtok(s1, " ");
					if (ss) 
					{
						ss = strtok(NULL, "\r\n");
						trimstr1(ss);
						strcpy(subtype, ss);
					}
				}
				if (_strnicmp(s1, "unsubscribe", 11) == 0) 
				{
					unsubscribe = true;
					ss = strtok(s1, " ");
					if (ss) 
					{
						ss = strtok(NULL, "\r\n");
						trimstr1(ss);
						strcpy(subtype, ss);
					}
				}
			}
			ss = NULL;
		}
	}
	
	if (fp != NULL)
	{
		fclose(fp);
	}
	
	if ((mailname[0] == 0) || (subtype[0] == 0)) 
	{
		log_it( true, "\n ! Invalid subscription request %s.", pszFileName);
		return 1;
	}
	if ((strlen(subtype) == 1) || (strlen(subtype) > 7)) 
	{
		sprintf(s, "\n ! %s attempted to write to M%s.NET!", mailname, subtype);
		log_it( true, s);
		ssm(s);
		return 1;
	}
	if (_strnicmp(subtype, "LISTS", 5) == 0) 
	{
		send_note(6, mailname, subtype);
		return 0;
	}
	sprintf(s1, "M%s.NET", subtype);
	if (!okfn(s1)) 
	{
		sprintf(s, "\n ! %s attempted to create invalid filename %s!", mailname, s1);
		log_it( true, s);
		ssm(s);
		return 1;
	}
	sprintf(s1, "%sM%s.NET", net_data, subtype);
	if (!exist(s1)) 
	{
		sprintf(s, "\n ! Mail list %s not found (subtype %s).", s1, subtype);
		log_it( true, s);
		ssm(s);
		send_note(5, mailname, subtype);
		return 1;
	}
	if ((oldfp = fsh_open(s1, "rt")) == NULL) 
	{
		log_it( true, "\n ! Unable to open input file %s.", s1);
		return 1;
	}
	sprintf(s1, "%sM%s.TMP", net_data, subtype);
	if (exist(s1))
		_unlink(s1);
	
	if ((newfp = fsh_open(s1, "wt+")) == NULL) 
	{
		log_it( true, "\n ! Unable to open output file %s.", s1);
		if (oldfp != NULL)
		{
			fclose(oldfp);
		}
		return 1;
	}
	bool found = false;
	while (fgets(s, sizeof(s)-1, oldfp)) 
	{
		trimstr1(s);
		if (unsubscribe) 
		{
			if (stristr(s, corename) != 0) 
			{
				log_it( true, "\n \xFE Removing %s from %s mailing list.", mailname, subtype);
				send_note(1, mailname, subtype);
				found = true;
			} 
			else
			{
				fprintf(newfp, "%s\n", s);
			}
		} 
		else 
		{
			if (stristr(s, corename) != 0) 
			{
				sprintf(s1, "\n \xFE %s already in %s mailing list.", mailname, subtype);
				log_it( true, s1);
				ssm(s1);
				send_note(3, mailname, subtype);
				found = true;
			}
			fprintf(newfp, "%s\n", s);
		}
	}
	if (oldfp != NULL)
	{
		fclose(oldfp);
	}
	if (!found) 
	{
		if (unsubscribe) 
		{
			sprintf(s, "\n \xFE Unsubscribe %s - not a member of %s list.", mailname, subtype);
			log_it( true, s);
			ssm(s);
			send_note(4, mailname, subtype);
		} 
		else 
		{
			sprintf(s, "\n \xFE Added new subscriber %s to %s list.", mailname, subtype);
			log_it( true, s);
			ssm(s);
			fprintf(newfp, "%s\n", mailname);
			send_note(2, mailname, subtype);
		}
	}
	if (newfp != NULL)
	{
		fclose(newfp);
	}
	sprintf(s, "%sM%s.TMP", net_data, subtype);
	sprintf(s1, "%sM%s.NET", net_data, subtype);
	if (exist(s1))
	{
		_unlink(s1);
	}
	copyfile(s, s1);
	if (exist(s))
	{
		_unlink(s);
	}
	return 0;
}


int ExportMain( int argc, char *argv[] )
{
	char szFileName[_MAX_PATH], s[201], szFileExtension[5], *ss;
	SEH_PUSH("main()");
	
	output("\n \xFE WWIV Internet Network Support (WINS) Import/Export %s", VERSION);
	if (argc != 7) 
	{
		printf(" ! EXP <filename> <net_data> <net_sysnum> <POPNAME> <DOMAIN> <net_name>\n\n");
		return 1;
	}

	ZeroMemory( szEmptyString, EMPTY_STRING_LEN );
	strcpy(net_data, argv[2]);
	int hFile = sh_open1("CONFIG.DAT", O_RDONLY | O_BINARY);
	if (hFile < 0) 
	{
		puts( "Could not open CONFIG.DAT!\n\n" );
		log_it( false, "Could not open CONFIG.DAT!\n\n");
		return 1;
	}
	sh_read(hFile, &syscfg, sizeof(configrec));
	sh_close(hFile);
	
	get_dir(maindir, true);
	sprintf(szFileName, "%s%s", net_data, argv[1]);
	strcpy(net_name, argv[6]);
	strcpy(POPNAME, argv[4]);
	strcpy(DOMAIN, argv[5]);
	g_nNetworkSystemNumber = ( unsigned short ) atoi(argv[3]);
	DIGEST = 0;
	tagfile[0] = 0;
	spam = false;
	
	ss = getenv("WWIV_INSTANCE");
	if (ss) 
	{
		instance = ( unsigned short ) atoi(ss);
		if (instance >= 1000) 
		{
			log_it( true, "\n \xFE WWIV_INSTANCE set to %hd.  Can only be 1..999!", instance);
			instance = 1;
		}
	} 
	else
	{
		instance = 1;
	}
	
	parse_net_ini();
	DEBUG_PUTS( "main(6) " );

	struct tm * today;
	time( &cur_daten );
	today = localtime( &cur_daten );
	jdater = jdate( today->tm_year, today->tm_mday, today->tm_wday );
	
	_strupr(postmaster);
	DEBUG_PUTS( "main(7) " );
	ExportMessages(szFileName);
	DEBUG_PUTS( "main(8) " );
	
	_snprintf(szFileName, _MAX_PATH, "%sSPOOL\\UNK*.*", net_data);
	struct _finddata_t ff;
	long hFind = _findfirst( szFileName, &ff );
	int nFindNext = ( hFind != -1 ) ? 0 : -1;
	while ( nFindNext == 0) 
	{
		sprintf(szFileName, "%sSPOOL\\%s", net_data, ff.name);
		char szRob[512];
		sprintf( szRob, "\n\nImporting %s\n", szFileName );
		log_it( true, szRob );
		if (!import(szFileName) || ff.size == 0L)
		{
			_unlink(szFileName);
		}
		nFindNext = _findnext( hFind, &ff );
	}
	_findclose( hFind );
	hFind = -1;
	DEBUG_PUTS( "main(9) " );
	
	sprintf(szFileName, "%sINBOUND\\SUB*.*", net_data);
	hFind = _findfirst( szFileName, &ff );
	nFindNext = ( hFind != -1 ) ? 0 : -1;
	while ( nFindNext == 0 )
	{
		sprintf(szFileName, "%sINBOUND\\%s", net_data, ff.name);
		_splitpath(szFileName, NULL, NULL, NULL, szFileExtension);
		if (_strnicmp(szFileExtension, ".BAD", 4) == 0) 
		{
			sprintf(s, "\n! * Unvalidated subscribe request %s.", szFileName);
			log_it( true, s);
			ssm(s);
		} 
		else 
		{
			sprintf( s, "\n * Processing subscribe request %s.", szFileName );
			log_it( true, s );
			if ( !subscribe( szFileName ) || ff.size == 0 )
			{
				_unlink(szFileName);
			}
			else 
			{
				char newfn[_MAX_PATH];
				CreateMessageName(newfn);
				rename(szFileName, newfn);
			}
		}
		nFindNext = _findnext( hFind, &ff );
	}
	_findclose( hFind );
	hFind = -1;
	DEBUG_PUTS( "main(a) " );
	
	if (maillist != NULL)
	{
		free(maillist);
	}
	
	DEBUG_PUTS( "main(b) " );
	return 0;
}


// Command line: S32767.NET C:\BBS\FILENET\ 561 n561 home.robsite.org FILEnet
int main(int argc, char* argv[])
{
	__try
	{
		SEH_Init();
		return ExportMain( argc, argv );
	}
	__except( GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION )
	{
		printf( "EXCEPTION_ACCESS_VIOLATION \n" );
		SEH_DumpStack();
	}
	return 0;
}