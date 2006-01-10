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

#include "pppproj.h"
#include "socketwrapper.h"


#define BIGSTR 4096
#define STRING 1025
#define STR 256


configrec syscfg;

char *version = "WWIV Internet Network Support (WINS) News Retrieval " VERSION;

static char g_szTempBuffer[STRING];
char POPNAME[25], POPDOMAIN[45], serverhost[65];
char cur_path[STRING], cur_from[STR], cur_subject[STR], cur_replyto[STR];
char cur_newsgroups[BIGSTR], cur_message_ID[STR], cur_organization[STR];
char cur_articleid[STR], cur_references[STRING], cur_lines[STR], cur_date[STR];
static char whichrc, g_szMainDirectory[_MAX_PATH];
int instance, crossposts, newsrc_upd, binxpost, MOREINFO, QUIET;
static int spooltodisk, NNTP_stat;
unsigned long reparticle, cur_numa, cur_first, cur_last;
time_t cur_daten;
unsigned short reply, sy;
static char NEWSNAME[60], NEWSPASS[60];
static int DEBUG = 1;
char net_data[MAX_PATH];


typedef struct 
{
	char groupname[60];
	unsigned long lastread;
	char subtype[8];
} GROUPFILEREC;


GROUPFILEREC *grouprec;

int ngroups;

typedef struct 
{
	char whichrc;
	char hostname[60];
	char newspass[60];
	char newsname[60];
} NEWSHOST;

NEWSHOST *newshosts;

#define NEWS_SOCK_READ_ERR(PROTOCOL)                                      \
  switch (PROTOCOL##_stat) {                                              \
    case 1 :                                                              \
      fprintf(stderr, "\n ! Session error : sockerr");                    \
      if (grouprec != NULL) write_groups(1);                              \
      fcloseall();                                                        \
      cursor('R');                                                        \
      WSACleanup();                                                       \
      exit(EXIT_FAILURE);                                                 \
    case -1:                                                              \
      fprintf(stderr, "\n ! Timeout : sockerr");                          \
      if (grouprec != NULL) write_groups(1);                              \
      fcloseall();                                                        \
      cursor('R');                                                        \
      WSACleanup();                                                       \
      exit(EXIT_FAILURE);                                                 \
  }                                                                       \

char *texth[] = {"th", "st", "nd", "rd"};

char *ordinal_text(int number)
{
	if (((number %= 100) > 9 && number < 20) || (number %= 10) > 3)
	{
		number = 0;
	}
	return texth[number];
}

static unsigned cursize;

void cursor(int tmp)
{
    switch (toupper(tmp)) 
    {
    case 'S':                               /* Save */
        break;
    case 'R':                               /* Restore */
        break;
    case 'H':                               /* Hide */
        break;
    case 'N':                               /* Normal */
        break;
    }
}


void rename_pend( char *pszFileName )
{
	char szOrigName[_MAX_PATH], szNewName[_MAX_PATH];
	
	sprintf( szOrigName, "%s%s", net_data, pszFileName );
	for ( int i = 0; i < 1000; i++ ) 
	{
		sprintf( szNewName, "%sP0-%u.NET", net_data, i );
		if ( !exist( szNewName ) ) 
		{
			rename( szOrigName, szNewName );
			return;
		}
	}
}


void check_packets(void)
{
	char szBuffer[_MAX_PATH], szTempBuffer[_MAX_PATH];
	struct _finddata_t ff;
	
	sprintf(szBuffer, "%sP0-*.%3.3d", net_data, instance);
	long hFind = _findfirst( szBuffer, &ff );
	int nFindNext = ( hFind != -1 ) ? 0 : -1;
	while ( nFindNext == 0) 
	{
		if ( ff.size == 0 ) 
		{
			sprintf( szTempBuffer, "%s%s", net_data, ff.name );
			if ( _unlink(szTempBuffer) == -1 )
			{
				rename_pend( ff.name );
			}
		} 
		else
		{
			rename_pend( ff.name );
		}
		nFindNext = _findnext( hFind, &ff );
	}
}


void write_groups( int display )
{
	char szFileName[ _MAX_PATH ];
	FILE *hGroup;
	
	sprintf( szFileName, "%sNEWS%c", net_data, whichrc );
    strcat( szFileName, ".RC" );
	
	if ( ( hGroup = fsh_open( szFileName, "wt+" ) ) == NULL ) 
	{
		output( "\n \xFE Unable to open %s!", szFileName );
		return;
	} 
	else 
	{
		if ( display && MOREINFO && !QUIET )
		{
			output( "\n \xFE Updating message pointers.." );
		}
	}
	for ( int i = 0; i < ngroups; i++ )
	{
		if ( *grouprec[i].groupname && stricmp(grouprec[i].groupname, "newsrc") != 0 )
		{
			fprintf(hGroup, "%s %lu %s\n", grouprec[i].groupname, grouprec[i].lastread, grouprec[i].subtype);
		}
	}
	if ( hGroup != NULL )
	{
		fclose( hGroup );
	}
}


void nntp_shutdown(SOCKET sock)
{
	if (sock != INVALID_SOCKET ) 
	{
		sock_puts(sock, "QUIT");
		closesocket(sock);
	}
}


int read_groups()
{
	int i = 0;
	char *ss, szFileName[ _MAX_PATH ], szFileName1[ _MAX_PATH ], tmp[STRING];
	FILE *groupfp;
	
	sprintf(szFileName1, "%sNEWS%c", net_data, whichrc);
    strcat( szFileName1, ".RC" );
	ngroups = count_lines(szFileName1);
	if (!ngroups)
	{
		return 0;
	}
	if (grouprec != NULL)
	{
		free(grouprec);
	}
	grouprec = NULL;
	grouprec = (GROUPFILEREC *) malloc((ngroups + 1) * sizeof(GROUPFILEREC));
    ::ZeroMemory( grouprec, ( ngroups + 1 ) * sizeof( GROUPFILEREC ) );
	if (!grouprec) 
	{
		ngroups = 0;
		output("\n \xFE Insufficient memory for %d groups in %s.", ngroups, szFileName1);
		return 0;
	}
	if ((groupfp = fsh_open(szFileName1, "rt")) == NULL) 
	{
		if (grouprec != NULL)
		{
			free(grouprec);
		}
		ngroups = 0;
		return 0;
	}
    ::ZeroMemory( tmp, 120 );
	while (fgets(tmp, 120, groupfp)) 
	{
		if (*tmp) 
		{
            trimstr1( tmp );
			if (strnicmp(tmp, "newsrc", 6) == 0) 
			{
				strcpy(grouprec[i].groupname, "newsrc");
				++i;
			} 
			else 
			{
				ss = strtok(tmp, " ");
				strcpy(grouprec[i].groupname, ss);
				ss = strtok(NULL, " ");
				if (ss) 
				{
					grouprec[i].lastread = atol(ss);
					ss = strtok(NULL, " \n");
					if (ss) 
					{
						strcpy(grouprec[i].subtype, strupr(ss));
						++i;
					}
				}
			}
		}
        ::ZeroMemory( tmp, 120 );
	}
	ngroups = i;
	if (groupfp != NULL)
    {
		fclose(groupfp);
    }
	for (i = 0; i < ngroups; i++) 
	{
		if ((grouprec[i].subtype) && (atoi(grouprec[i].subtype) != 0)) 
		{
			sprintf(szFileName, "%sN%s.NET", net_data, grouprec[i].subtype);
			if (exist(szFileName)) 
			{
				bool ok = false;
				if ((groupfp = fsh_open(szFileName, "rt")) != NULL) 
				{
					while ((fgets(tmp, 25, groupfp)) && !ok) 
					{
						int sn = atoi(tmp);
						if (sn == 32767)
						{
							ok = true;
						}
					}
					fclose(groupfp);
				}
				if (!ok)
				{
					log_it(1, "\n \xFE @32767 not listed as a subscriber in N%s.NET!",
					grouprec[i].subtype);
				}
			}
		}
	}
    return ngroups;
}


bool sendauthinfo(SOCKET sock)
{
	char szBuffer[_MAX_PATH], buf[STR];
	
    memset( g_szTempBuffer, 0, sizeof( g_szTempBuffer ) );
	
	log_it(MOREINFO, "\n \xFE Server requested authentication information.");
	
	for( ;; ) 
	{
		sprintf(buf, "authinfo user %s", NEWSNAME);
		sock_puts(sock, buf);
		sock_gets(sock, g_szTempBuffer, sizeof(g_szTempBuffer));
		sscanf(g_szTempBuffer, "%hu %s", &reply, szBuffer);
		if (reply == 381) 
		{
			sprintf(buf, "authinfo pass %s", NEWSPASS);
			sock_puts(sock, buf);
			sock_gets(sock, g_szTempBuffer, sizeof(g_szTempBuffer));
			sscanf(g_szTempBuffer, "%hu %s", &reply, szBuffer);
			if (reply == 281) 
			{
				log_it(MOREINFO, "\n \xFE Authentication accepted.  Continuing news retrieval session.");
				return true;
			} 
			else 
			{
				log_it(1, "\n \xFE Authentication failed.  Aborting news session.");
				return false;
			}
		} 
		else 
		{
			log_it(1, "\n \xFE Unknown AUTHINFO response %s.", g_szTempBuffer);
			return false;
		}
	}
}

int cgroup(SOCKET sock, char *buf)
{
	char szTempBuffer[STRING];
	
    memset( g_szTempBuffer, 0, sizeof( g_szTempBuffer ) );
	
	sprintf( szTempBuffer, "GROUP %s", buf );
	sock_puts(sock, szTempBuffer);
	sock_gets(sock, g_szTempBuffer, sizeof(g_szTempBuffer));
	sscanf(g_szTempBuffer, "%hu %lu", &reply, &reparticle);
	switch (reply) 
	{
    case 411:
		return 0;
    case 211:
		sscanf(g_szTempBuffer, "%hu %lu %lu %lu %s",
			&reply, &cur_numa, &cur_first, &cur_last, szTempBuffer);
		return 1;
    case 480:
		return -1;
    default:
		log_it(MOREINFO, "\n \xFE Unknown GROUP response : %s", g_szTempBuffer);
		break;
	}
	NEWS_SOCK_READ_ERR(NNTP);
	return 0;
}


char *treat(char *string)
{
	char *buf;
	
	if ( !string || !*string )
	{
		return NULL;
	}
	
	for ( buf = string; *buf; ++buf ) 
	{
		if ((*buf < 32) || (*buf > 126)) 
		{
			if ((*buf != 9) && (*buf != 10)) 
			{
				memmove(buf, buf + 1, strlen(buf) + 1);
				--buf;
			}
		}
	}
	
	return string;
}


char *name_packet( char *pktname )
{
	for ( int i = 0; i < 1000; i++ ) 
	{
		sprintf( pktname, "%sP0-%5.5d.%3.3d", net_data, i, instance );
		if ( !exist( pktname ) )
		{
			break;
		}
	}
	return pktname;
}


int jgets(char *buf, int maxlen)
{
	char temp[STRING];
	int done = 0, pos = 0, length = 0, i, c, zeroflag;
	
	while (!done) 
	{
		zeroflag = 0;
		if ((c = getch()) == 0) 
		{
			zeroflag = 1;
			c = getch();
		}
		switch (c) 
		{
		case 27:
			return 0;
		case 8:
			if (c == 8) 
			{
				if (pos == 0)
				{
					break;
				}
				if (pos != length) 
				{
					for (i = pos - 1; i < length; i++)
					{
						temp[i] = temp[i + 1];
					}
					pos--;
					length--;
					putch(8);
					for (i = pos; i < length; i++)
					{
						putch(temp[i]);
					}
					for (i = length; i >= pos; i--)
					{
						putch(8);
					}
				} 
				else 
				{
					putch(8);
					pos = --length;
				}
				break;
			}
		case 13:
			if (c == 13) 
			{
				done = 1;
				break;
			}
		default:
			if (zeroflag)
			{
				break;
			}
			if (c == 0 || pos == maxlen)
			{
				break;
			}
			if (pos == length) 
			{
				temp[pos++] = ( char ) c;
				if (pos > length)
				{
					length++;
				}
				putch(c);
			} 
			else 
			{
				for (i = length++; i >= pos; i--)
				{
					temp[i + 1] = temp[i];
				}
				temp[pos++] = ( char ) c;
				putch(c);
				for (i = pos; i < length; i++)
				{
					putch(temp[i]);
				}
				for (i = length; i > pos; i--)
				{
					putch(8);
				}
			}
		}
	}
	temp[length] = '\0';
	strcpy(buf, temp);
	return length;
}


int savebody(SOCKET sock, char *pszFileName, int cug, unsigned long cur_article, int *abort)
{
	int i,  place, length, curpos, count, part;
	char _big_temp_buffer[BIGSTR + 1];
	char ch, *spin, buf[STRING];
	unsigned long msgsize;
	
	place = count = 0;
    memset( g_szTempBuffer, 0, sizeof( g_szTempBuffer ) );
	part = 1;
	
	if (!QUIET)
	{
		spin = "||//--\\\\";
	}
	else
	{
		spin = "";
	}
	length = strlen(spin);
	
    int hFile = -1;
    if (spooltodisk)
	{
		hFile = sh_open(pszFileName, O_RDWR | O_APPEND | O_TEXT | O_CREAT, S_IREAD | S_IWRITE);
	}
	else
	{
		hFile = sh_open(pszFileName, O_RDWR | O_TEXT | O_CREAT | O_TRUNC, S_IWRITE);
	}
	
	if (hFile < 0) 
	{
		log_it(MOREINFO, "\n \xFE Unable to write to [%s], error = [%d].", pszFileName, errno );
		return 0;
	}
	char szTemp[STRING];
	sprintf( szTemp, "BODY %ld", cur_article );
	sock_puts( sock, szTemp );
    memset( _big_temp_buffer, 0, sizeof( _big_temp_buffer ) );
	sock_gets(sock, _big_temp_buffer, sizeof(_big_temp_buffer) - 1);
	sscanf(_big_temp_buffer, "%hu %lu", &reply, &reparticle);
	switch (reply) {
    case 423:
		return 0;
    case 222:
		if (QUIET)
		{
			output("[-]");
		}
		sh_write(hFile, "\n", sizeof(char));
		if (spooltodisk) 
		{
			if ((MOREINFO) && (!QUIET))
			{
				output("\n \xFE NEWS%d.UUE - [Esc], [Space], [Tab], ] = +10, } = +100, + = User Input [-]", cug);
			}
			for (i = 0; i < 79; i++)
			{
				sh_write(hFile, "-", sizeof(char));
			}
			sh_write(hFile, "\n", sizeof(char));
			sprintf(buf, "Art  : %lu\n", cur_article);
			sh_write(hFile, buf, strlen(buf));
			if (*cur_lines) 
			{
				if ((atol(cur_lines) > 0L) && (atol(cur_lines) < 99999L))
				{
					sprintf(buf, "Lines: %s\n", cur_lines);
				}
				else
				{
					sprintf(buf, "Lines: Unknown\n");
				}
			}
			sh_write(hFile, buf, strlen(buf));
			sprintf(buf, "Group: %s\n", grouprec[cug].groupname);
			sh_write(hFile, buf, strlen(buf));
			if (*cur_date)
			{
				sprintf(buf, "Date : %s\n", cur_date);
			}
			else
			{
				sprintf(buf, "Date : Unknown\n");
			}
			sh_write(hFile, buf, strlen(buf));
			if (*cur_replyto)
			{
				sprintf(buf, "From : %s\n", cur_replyto);
			}
			else
			{
				sprintf(buf, "From : Unknown\n");
			}
			sh_write(hFile, buf, strlen(buf));
			if (*cur_subject)
			{
				sprintf(buf, "Subj : %s\n\n", cur_subject);
			}
			else
			{
				sprintf(buf, "Subj : Unknown\n");
			}
			sh_write(hFile, buf, strlen(buf));
		} 
		else 
		{
			if (*cur_replyto) 
			{
				sprintf(buf, "0RReply-To: %s\n", cur_replyto);
				sh_write(hFile, buf, strlen(buf));
			}
			if (*cur_message_ID) 
			{
				sprintf(buf, "0RMessage-ID: %s\n", cur_message_ID);
				sh_write(hFile, buf, strlen(buf));
			}
			if (*cur_references) 
			{
				sprintf(buf, "0RReferences: %s\n", cur_references);
				sh_write(hFile, buf, strlen(buf));
			}
			if ( MOREINFO && !QUIET )
			{
				output("\n \xFE Receiving message - [Esc] Abort - [Space] Skip Group - [Tab] Catch Up [-]");
			}
		}
		msgsize = 0L;
		curpos = 0L;
		for( ;; ) 
		{
			while (kbhit()) 
			{
				ch = ( char ) (getch());
				spin = "..oOOOo.";
				length = strlen(spin);
				switch (ch) 
				{
				case '+':
					if (spooltodisk) 
					{
						if (MOREINFO) 
						{
							backline();
							log_it(MOREINFO, "\r \xFE Jumping forward... wait until message is completed [.]");
						}
						*abort = 6;
					}
					break;
				case '}':
					if (spooltodisk) 
					{
						if (MOREINFO) 
						{
							backline();
							log_it(MOREINFO, "\r \xFE Jumping 100 articles... wait until message is completed [.]");
						}
						*abort = 5;
					}
					break;
				case ']':
					if (spooltodisk) 
					{
						if (MOREINFO) 
						{
							backline();
							log_it(MOREINFO, "\r \xFE Jumping 10 articles... wait until message is completed [.]");
						}
						*abort = 4;
					}
					break;
				case 9:
					if (MOREINFO) 
					{
						backline();
						log_it(MOREINFO, "\r \xFE Catching up on group... wait until message is completed [.]");
					}
					*abort = 3;
					break;
				case 32:
					if (MOREINFO) 
					{
						backline();
						log_it(MOREINFO, "\r \xFE Skipping group... wait until message is completed [.]");
					}
					*abort = 2;
					break;
				case 27:
					if (MOREINFO) 
					{
						backline();
						log_it(MOREINFO, "\r \xFE Aborting session... wait until message is completed [.]");
					}
					*abort = 1;
					break;
				default:
					break;
				}
			}
			if (count++ > 5) 
			{
				output("\b\b%c]", spin[place++]);
				if (length)
				{
					place %= length;
				}
				count = 0;
			}
            memset( _big_temp_buffer, 0, sizeof( _big_temp_buffer ) );
			sock_gets( sock, _big_temp_buffer, sizeof( _big_temp_buffer ) - 1 );
			if (_big_temp_buffer[0] == '.' && _big_temp_buffer[1] == 0)
			{
				break;
			}
			else 
			{
				treat(_big_temp_buffer);
				if (*_big_temp_buffer) 
				{
					strcat(_big_temp_buffer, "\n");
					curpos = strlen(_big_temp_buffer);
					sh_write(hFile, _big_temp_buffer, strlen(_big_temp_buffer));
					msgsize += curpos;
				}
			}
			if ((msgsize > 30000L) && (!spooltodisk)) 
			{
				strcpy(buf, "\nContinued in next message...\n");
				sh_write(hFile, buf, strlen(buf));
				sh_close(hFile);
				sprintf(pszFileName, "%sSPOOL\\INPUT%d.MSG", net_data, ++part);
				unlink(pszFileName);
				if ((hFile = sh_open(pszFileName, O_WRONLY | O_TEXT | O_CREAT | O_TRUNC, S_IWRITE)) < 0) 
				{
					log_it(MOREINFO, "\n \xFE Unable to create %s", pszFileName);
					return 0;
				}
				if ((MOREINFO) && (!QUIET)) 
				{
					backline();
					output("\r \xFE Breaking into %d%s part [ ]", (part), ordinal_text(part));
				}
				strcpy(buf, "\nContinued from previous message...\n");
				sh_write(hFile, buf, strlen(buf));
				msgsize = strlen(buf);
			}
		}
		sh_close(hFile);
		if (!QUIET)
		{
			output("\b\bX]");
		}
		else
		{
			output("\b\b\xFE]");
		}
		return 1;
    default:
		log_it(MOREINFO, "\n \xFE Unknown BODY error : %s", g_szTempBuffer);
		break;
  }
  sh_close(hFile);
  NEWS_SOCK_READ_ERR(NNTP);
  return 0;
}

bool extract( char *to, char *key, char *from )
{
    int nKeyLen = strlen( key );
    //printf( "[Extract: keylen=%d]", nKeyLen );
	if ( !strnicmp( from, key, strlen( key ) ) ) 
	{
		from += strlen( key );
		while ( *from == ' ' )
		{
			from++;
		}
		strcpy( to, from );
		return true;
	} 
	return false;
}


int chead( SOCKET sock, unsigned long which)
{
	char _big_temp_buffer[BIGSTR];
	
	*cur_path = 0;
	*cur_from = 0;
	*cur_replyto = 0;
	*cur_subject = 0;
	*cur_newsgroups = 0;
	*cur_message_ID = 0;
	*cur_references = 0;
	*cur_date = 0;
    memset( g_szTempBuffer, 0, sizeof( g_szTempBuffer ) );
	
	char szTemp[STRING];
	sprintf( szTemp, "HEAD %lu", which );
	sock_puts(sock, szTemp );
    memset( _big_temp_buffer, 0, sizeof( _big_temp_buffer ) );
	sock_gets(sock, _big_temp_buffer, sizeof(_big_temp_buffer) - 1 );
	sscanf(_big_temp_buffer, "%hu %lu", &reply, &reparticle);
	switch (reply) 
	{
    case 423:
		if (!QUIET)
		{
			output("%-53.53s", "Expired article.");
		}
		return 0;
    case 221:
		for ( ;; ) 
		{
            memset( _big_temp_buffer, 0, sizeof( _big_temp_buffer ) );
			sock_gets( sock, _big_temp_buffer, sizeof(_big_temp_buffer) - 1 );
			if (_big_temp_buffer[0] == '.' && _big_temp_buffer[1] == 0) 
			{
				if (cur_replyto[0] == 0)
				{
					strcpy(cur_replyto, cur_from);
				}
				if (cur_subject[0] == 0)
				{
					strcpy(cur_subject, "No Subject");
				}
				return 1;
			}
			if ((strlen(_big_temp_buffer)) > STRING)
			{
				_big_temp_buffer[STRING] = '\0';
			}
			if (extract(cur_path, "Path:", _big_temp_buffer) ||
				extract(cur_from, "From:", _big_temp_buffer) ||
				extract(cur_subject, "Subject:", _big_temp_buffer) ||
				extract(cur_replyto, "Reply-To:", _big_temp_buffer) ||
				extract(cur_newsgroups, "Newsgroups:", _big_temp_buffer) ||
				extract(cur_organization, "Organization:", _big_temp_buffer) ||
				extract(cur_message_ID, "Message-ID:", _big_temp_buffer) ||
				extract(cur_references, "References:", _big_temp_buffer) ||
				extract(cur_lines, "Lines:", _big_temp_buffer) ||
				extract(cur_date, "Date:", _big_temp_buffer))
			{
				continue;
			}
		}
    default:
		log_it(MOREINFO, "\n \xFE Unknown chead error : %s", _big_temp_buffer);
		break;
	}
	NEWS_SOCK_READ_ERR(NNTP);
	return 0;
}

SOCKET netsocket(char *host)
{
    SOCKET sock;
	sock = socket(AF_INET, SOCK_STREAM, 0 );
	if ( sock == INVALID_SOCKET )
	{
		return INVALID_SOCKET;
	}
	
	SOCKADDR_IN sockAddr;
	ZeroMemory(&sockAddr, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons((u_short) NNTP_PORT );
	sockAddr.sin_addr.s_addr = inet_addr( host );
	if (sockAddr.sin_addr.s_addr == INADDR_NONE)
	{
		LPHOSTENT lphost;
		lphost = gethostbyname( host );
		if (lphost != NULL)
		{
			sockAddr.sin_addr.s_addr = ((LPIN_ADDR)lphost->h_addr)->s_addr;
		}
		else
		{
			log_it(1, "\n \xFE Error : Cannot resolve host %s", host);
			return INVALID_SOCKET;
		}
	}
	if ( connect( sock, ( SOCKADDR *) &sockAddr, sizeof ( sockAddr ) ) == SOCKET_ERROR )
	{
		log_it(1, "\n \xFE Error : Unable to connect to %s", host);
		return INVALID_SOCKET;
	}

	sock_gets( sock, g_szTempBuffer, sizeof(g_szTempBuffer) );
	log_it(DEBUG, "\n - NNTP> %s", g_szTempBuffer);
	sscanf(g_szTempBuffer, "%hu %lu", &reply, &reparticle);
	switch (reply) 
	{
    case 200:
		log_it(MOREINFO, "\n \xFE Connection to %s accepted...", host);
		return sock;
    case 502:
		log_it(1, "\n \xFE Connection to %s refused... try again later.", host);
		break;
    case 503:
		log_it(1, "\n \xFE NNTP service unavailable. Connection to %s refused.", host);
		break;
    default:
		log_it(1, "\n \xFE Unknown NNTP error. Connection to %s failed.", host);
		break;
	}
	NEWS_SOCK_READ_ERR(NNTP);
	return INVALID_SOCKET;
}

int cnext( SOCKET sock)
{
	char *p, *q;
	
    memset( g_szTempBuffer, 0, sizeof( g_szTempBuffer ) );
	
	sock_puts(sock, "NEXT");
	sock_gets( sock, g_szTempBuffer, sizeof(g_szTempBuffer) );
	sscanf(g_szTempBuffer, "%hu %lu", &reply, &reparticle);
    //printf("\n {DEBUG: Reply = %d [%s]} \n", reply, g_szTempBuffer );
	switch (reply) 
    {
    case 421:
		return 0;
    case 223:
		p = g_szTempBuffer;
		q = cur_articleid;
		while ((p < g_szTempBuffer + STRING - 2) && (*p != '<'))
		{
			p++;
		}
		while ((p < g_szTempBuffer + STRING - 2) && (*p != '>'))
		{
			*q++ = *p++;
		}
		*q++ = '>';
		*q++ = '\0';
		return 1;
    default:
		log_it(MOREINFO, "\n \xFE Unknown NEXT error : %s", g_szTempBuffer);
		break;
	}
	NEWS_SOCK_READ_ERR(NNTP);
	return 0;
}


void cslave( SOCKET sock )
{
    char szTempBuffer[STRING];

    memset( g_szTempBuffer, 0, sizeof( g_szTempBuffer ) );

    sock_puts( sock, "SLAVE");
    sock_gets( sock, g_szTempBuffer, sizeof(g_szTempBuffer));
    sscanf(g_szTempBuffer, "%hu %s", &reply, szTempBuffer);
    NEWS_SOCK_READ_ERR(NNTP);
}


int saveactive(SOCKET sock, int forced)
{
	char s[181], prev_s[181], szFileName[_MAX_PATH], group[256], act;
	unsigned long to, from;
	unsigned fnday = 0, fnmonth = 0, fnyear = 0, fnhour = 0, fnminute = 0, fnsecond = 0;
	int done, inloop, update;
	unsigned long count;
	FILE *fp;
	struct tm *ft;
	struct tm *d;
	
    memset( g_szTempBuffer, 0, sizeof( g_szTempBuffer ) );
	
	update = inloop = 0;
	
	sprintf(szFileName, "%sNEWSRC%c", net_data, whichrc);
	
	time_t time_t_now;
	time( &time_t_now );
	int hFile = sh_open1(szFileName, O_RDONLY);
	if (hFile < 0)
	{
		forced = 1;
		ft = gmtime( &time_t_now );
	}
	else 
	{
		struct _stat statbuf;
		_fstat( hFile, &statbuf );
		ft = gmtime( &statbuf.st_mtime );
		sh_close(hFile);
	}
	
	d = gmtime( &time_t_now );
	if ( !forced && ( ft->tm_mon+1 )== 12 && (d->tm_mday == 1 && d->tm_mon == 0 ) ) 
	{
		_unlink( szFileName );
		forced = 1;
	}
	if (!forced) 
	{
		fnsecond = ft->tm_sec+1;
		fnminute = ft->tm_min+1;
		fnhour = ft->tm_hour+1;
		fnday = ft->tm_mday+1;
		fnmonth = ft->tm_mon+1;
		fnyear = ft->tm_year + 1900;
		log_it( 1, "\n \xFE NEWSRC last updated %02u/%02u/%04u %.2u:%.2u:%.2u... ", fnmonth, fnday, fnyear, fnhour, fnminute, fnsecond );
		if ( d->tm_mday == ft->tm_mday && d->tm_mon == ft->tm_mon && d->tm_year == ft->tm_year ) 
		{
			log_it(1, "no update required.");
			return 0;
		} 
		else
		{
			log_it(1, "updating.");
		}
		if ( d->tm_mon == ft->tm_mon && d->tm_mday != ft->tm_mday )
		{
			update = 1;
		}
	}
	if (update)
	{
		fp = fsh_open(szFileName, "a+");
	}
	else
	{
		fp = fsh_open(szFileName, "w+");
	}
	
	if (fp == NULL) 
	{
		perror(szFileName);
		return 0;
	}
	done = 0;
	
	while (!done) 
    {
		if (update) 
		{
			fnyear %= 100;
			sprintf(s, "NEWGROUPS %02.02u%02.02u%02.02u %02.02u%02.02u%02.02u",
				fnyear, fnmonth, fnday, fnhour, fnminute, fnsecond);
			sock_puts(sock, s);
			output("\n \xFE Refreshing NEWSRC newsgroup listing :           ");
		} 
		else 
		{
			sock_puts(sock, "LIST");
			output("\n \xFE Retrieving current newsgroup listing :           ");
		}
		sock_gets(sock, g_szTempBuffer, sizeof(g_szTempBuffer));
		sscanf(g_szTempBuffer, "%hu %lu", &reply, &reparticle);
		if ((reply == 215) || (reply == 231)) 
		{
			count = 0L;
			for ( ;; ) 
            {
				strcpy(prev_s, g_szTempBuffer);
				sock_gets(sock, g_szTempBuffer, sizeof(g_szTempBuffer));
				trimstr1(g_szTempBuffer);
				if (*g_szTempBuffer == '.' && g_szTempBuffer[1] == 0)
                {
					break;
                }
				if (strcmp(g_szTempBuffer, prev_s) == 0)
                {
					++inloop;
                }
				else
                {
					inloop = 0;
                }
				if (inloop > 10) 
                {
					log_it(1, "Looping problem at %s", g_szTempBuffer);
					if (fp != NULL)
                    {
						fclose(fp);
                    }
					break;
				}
				if (strlen(g_szTempBuffer) < 180) 
                {
					if (count++ % 25 == 24L)
                    {
						output("\b\b\b\b\b\b\b%-7lu", count);
                    }
					sscanf(g_szTempBuffer, "%s %lu %lu %c", group, &to, &from, &act);
					sprintf(s, "%s %lu\n", group, to);
					fsh_write(s, sizeof(char), strlen(s), fp);
				}
			}
			if (update) 
            {
				if (count == 0)
                {
					log_it(1, "\b\b\b\b\b\b\bNo new groups.");
                }
				else
                {
					log_it(1, "\b\b\b\b\b\b\b%lu group%s added to NEWSRC.", count,
					count == 1 ? "" : "s");
                }
			} 
            else
            {
				output("\b\b\b\b\b\b\b%lu total groups.", count);
            }
			
			done = 1;
			if (fp != NULL)
            {
				fclose(fp);
            }
			if (update) 
			{
				SetFileToCurrentTime( szFileName );
			}
			return 1;
		} 
        else 
        {
			if (reply == 480) 
            {
				if (fp != NULL)
                {
					fclose(fp);
                }
				return 0;
			} 
            else 
            {
				log_it(MOREINFO, "\n \xFE Unknown saveactive error : %s", g_szTempBuffer);
				if (fp != NULL)
                {
					fclose(fp);
                }
				done = 1;
			}
		}
	}
	if (fp != NULL)
    {
		fclose(fp);
    }
	NEWS_SOCK_READ_ERR(NNTP);
	return 0;
}

int checkx(int cug)
{
	char *ptr, *ptr2, *ptr3, szFileName[_MAX_PATH], buf[256], tmp[STR];
	int i, spam, max;
	FILE *fp;
	
	if ((stristr(grouprec[cug].groupname, "binaries")) && (binxpost))
	{
		return 1;
	}
	
	sprintf(buf, "%s!%s", POPNAME, POPDOMAIN);
	if (stristr(cur_path, buf) != 0) 
	{
		sprintf(buf, "Local post - %s - skipped.", cur_subject);
		if (!QUIET)
		{
			output("%-53.53s", buf);
		}
		return 0;
	}
	if (stristr(cur_organization, syscfg.systemname) != 0) 
	{
		sprintf(buf, "Local post - %s - skipped.", cur_subject);
		if (!QUIET)
		{
			output("%-53.53s", buf);
		}
		return 0;
	}
	bool ok = true;
	spam = 0;
	sprintf(szFileName, "%sNOSPAM.TXT", net_data);
	if ((fp = fsh_open(szFileName, "r")) != NULL) 
	{
		while ((!feof(fp)) && (!spam)) 
		{
			fgets(buf, 80, fp);
			trimstr1(buf);
			if (strlen(buf) > 2) 
			{
				if (buf[0] == '\"') 
				{
					strcpy(tmp, &(buf[1]));
					LAST(tmp) = '\0';
					strcpy(buf, tmp);
				}
				if (buf[0] == '[') 
				{
					if ((strnicmp(buf, "[GLOBAL]", 8) == 0) || (strnicmp(buf, "[NEWS]", 6) == 0))
					{
						ok = true;
					}
					else
					{
						ok = false;
					}
				}
				if ((ok) && ((stristr(cur_subject, buf)) || (stristr(cur_from, buf)))) 
				{
					sprintf(tmp, "SPAM - %s - skipped.", buf);
					if (!QUIET)
					{
						output("%-53.53s", tmp);
					}
					spam = 1;
				}
			}
		}
		fclose(fp);
		if (spam)
		{
			return 0;
		}
	}
	ptr = cur_newsgroups;
	
	while (*ptr == ' ')
	{
		ptr++;
	}
	max = 0;
	for ( ;; ) 
	{
		if (*ptr == 0)
		{
			break;
		}
		if (*ptr == ',') 
		{
			ptr++;
			++max;
		}
		while (*ptr == ' ')
		{
			ptr++;
		}
		ptr2 = ptr;
		ptr3 = buf;
		while (*ptr2 != 0 && *ptr2 != ',')
		{
			*ptr3++ = *ptr2++;
		}
		*ptr3 = 0;
		while (*(--ptr3) == ' ')
		{
			*ptr3 = 0;
		}
		ptr = ptr2;
		for (i = 0; i < cug; i++) 
		{
			if (strcmpi(buf, grouprec[i].groupname) == 0) 
			{
				sprintf(buf, "Already posted in %s.", grouprec[i].groupname);
				if (!QUIET)
				{
					output("%-53.53s", buf);
				}
				return 0;
			}
		}
		if ((crossposts > 0) && (max > crossposts)) 
		{
			sprintf(buf, "Crosspost to %d newsgroups.", crossposts);
			if (!QUIET)
			{
				output("%-53.53s", buf);
			}
			return 0;
		}
	}
	return 1;
}


bool verify_server(char *stype)
{
	for (int i = 0; i < ngroups; i++) 
	{
		long l = strlen(grouprec[i].subtype);
		if (strnicmp(grouprec[i].subtype, stype, l) == 0)
		{
			return true;
		}
	}
	return false;
}


int postnews( SOCKET sock, int cug )
{
	char s[STR], s1[12], s2[5], szFileName[_MAX_PATH], *ss;
	int nlines;
	FILE *fp;
	struct _finddata_t ff;
	
    memset( g_szTempBuffer, 0, sizeof( g_szTempBuffer ) );
	*g_szTempBuffer = 0;
	
	sprintf(szFileName, "%sOUTBOUND\\%s.*", net_data, grouprec[cug].subtype);
	long hFind = _findfirst( szFileName, &ff );
	int nFindNext = ( hFind != -1 ) ? 0 : -1;
	if ( nFindNext != 0 )
	{
		log_it(!QUIET, "\r\n \xFE No outbound news articles to post...");
		return 1;
	}
	while ( nFindNext == 0 ) 
	{
		if (!verify_server(grouprec[cug].subtype)) 
		{
			nFindNext = _findnext( hFind, &ff );
			continue;
		}
		sock_puts(sock, "POST");
		sock_gets(sock, g_szTempBuffer, sizeof(g_szTempBuffer));
		if (*g_szTempBuffer != '3') 
		{
			if (atoi(g_szTempBuffer) == 440)
			{
				log_it(MOREINFO, "\n \xFE No posting allowed to %s!", grouprec[cug].groupname);
			}
			else
			{
				log_it(1, "\n \xFE Remote error: %s", g_szTempBuffer);
			}
			return 0;
		}
		if ( MOREINFO  &&  !QUIET )
		{
			output("\n");
		}
		else
		{
			backline();
		}
		log_it(!QUIET, " \xFE Posting article to %s\n", grouprec[cug].groupname);
		sprintf(s, "%sOUTBOUND\\%s", net_data, ff.name);
		if ((fp = fsh_open(s, "r")) == NULL) 
		{
			log_it(MOREINFO, "\n \xFE Error reading %s.", s);
			return 1;
		}
		nlines = 0;
		while (!feof(fp)) 
		{
			fgets(g_szTempBuffer, sizeof(g_szTempBuffer), fp);
			strrep(g_szTempBuffer, '\n', '\0');
			strrep(g_szTempBuffer, '\r', '\0');
			if (*g_szTempBuffer == '.') 
			{
				memmove(g_szTempBuffer, g_szTempBuffer + 1, sizeof(g_szTempBuffer) - 1);
				*g_szTempBuffer = '.';
			}
			sock_puts(sock, g_szTempBuffer);
			if ( MOREINFO )
			{
				backline();
				log_it(!QUIET, "\r \xFE Lines sent : %d", ++nlines);
			}
		}
		fclose(fp);
		sock_puts(sock, ".");
		if ((MOREINFO) && (!QUIET))
		{
			output("\n \xFE Awaiting acknowledgement - may take several minutes...");
		}
		log_it(MOREINFO, "\n \xFE Server response : ");
		sock_gets(sock, g_szTempBuffer, sizeof(g_szTempBuffer));
		log_it(MOREINFO, "%s", g_szTempBuffer);
		if (*g_szTempBuffer != '2') 
		{
			sscanf(g_szTempBuffer, "%hu %s", &reply, s);
			if (reply == 441) 
			{
				ss = strtok(g_szTempBuffer, " ");
				ss = strtok(NULL, " ");
				if (atoi(ss) == 435)
				{
					unlink(s);
				}
				_splitpath(s, NULL, NULL, s1, s2);
				log_it(MOREINFO, "\n \xFE %s%s not accepted by server - nothing posted", s1, s2);
				if (unlink(s) == 0)
				{
					log_it(MOREINFO, "\n \xFE Deleted message - %s", s);
				}
			} 
			else
			{
				log_it(MOREINFO, "\n \xFE Remote error: %s", g_szTempBuffer);
			}
		} 
		else 
		{
			if (unlink(s) == 0)
			{
				log_it(MOREINFO, "\n \xFE Deleted sent message - %s", s);
			}
		}
		nFindNext = _findnext( hFind, &ff );
	}
	NEWS_SOCK_READ_ERR(NNTP);
	return 0;
}

void get_subtype(int sub, char *where)
{
	int which;
	char szFileName[_MAX_PATH], s[STR], net_name[21], *ss;
	FILE *fp;
	
	where[0] = 0;
	which = sub;
	sprintf(szFileName, "%sSUBS.XTR", syscfg.datadir);
	if ((fp = fsh_open(szFileName, "r")) == NULL)
    {
		return;
    }
	bool ok = false;
	while (fgets(s, 80, fp)) 
    {
		if (*s == '!') 
        {
			if (which == atoi(&s[1]))
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
			if ((stricmp(net_name, "FILENET") == 0)) 
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

unsigned long max_on_sub(int cug)
{
	int i, num_subs;
	char szFileName[_MAX_PATH], subtype[12];
	subboardrec sub;
	FILE *fp;
	
	unsigned long max = 0;
	if ((strlen(grouprec[cug].subtype) == 1) && (*grouprec[cug].subtype == '0'))
	{
		return max;
	}
	if (!QUIET) 
	{
		if (MOREINFO)
		{
			output("\n \xFE Max articles -     ");
		}
		else
		{
			output(" -     ");
		}
	}
	sprintf(szFileName, "%sSUBS.DAT", syscfg.datadir);
	if ((fp = fsh_open(szFileName, "rb")) == NULL) 
	{
		log_it(MOREINFO, "\n \xFE Unable to read %s.", szFileName);
		return max;
	} 
	else 
	{
		fseek(fp, 0L, SEEK_END);
		num_subs = (int) (ftell(fp) / sizeof(subboardrec));
		for (i = 0; i < num_subs; i++) 
		{
			if (!QUIET)
			{
				output("\b\b\b\b%-4d", i);
			}
			fseek(fp, (long) i * sizeof(subboardrec), SEEK_SET);
			fread(&sub, sizeof(subboardrec), 1, fp);
			get_subtype(i, subtype);
			if (stricmp(subtype, grouprec[cug].subtype) == 0) 
			{
				max = (unsigned long) sub.maxmsgs;
				if (!QUIET) 
				{
					if (MOREINFO)
					{
						output("\b\b\b\b%s (%s) - %lu posts.", sub.name, subtype, max);
					}
					else
					{
						output("\b\b\b\b [%lu max]", max);
					}
				}
				break;
			}
		}
	}
	fclose(fp);
	return max;
}


int getnews(SOCKET sock)
{
    int nup, nug, cug, abort, ok, done, firstrun, parts, skipped, bogus;
    char *p, szFileName[_MAX_PATH], mailname[121], reline[121], pktname[_MAX_PATH];
    char ch, orig_subj[STRING], buf[STR];
    unsigned int text_len;
    unsigned short temptype;
    unsigned long new_articles, max_articles, skip_articles;
    net_header_rec nh;
    struct _finddata_t ff;
    FILE *fp;

    abort = cug = bogus = 0;

    if (stricmp(grouprec[0].groupname, "newsrc") == 0) 
    {
        if (!saveactive(sock, 1)) 
        {
            if (sendauthinfo(sock))
            {
                saveactive(sock, 1);
            }
        }
        ++cug;
    } 
    else if (newsrc_upd)
    {
        saveactive(sock, 0);
    }

    nug = nup = 1;
    skipped = 0;
    cslave(sock);
    name_packet(pktname);
    if ((!MOREINFO) && (!QUIET))
    {
        output("\n \xFE [Esc] aborts session - [Space] skips group - [Tab] catches up group");
    }
    while ((cug < ngroups) && (!abort)) 
    {
        if (*grouprec[cug].groupname == ';') 
        {
            log_it(!QUIET, "\n \xFE Skipped group - %s", &(grouprec[cug].groupname[1]));
            ++cug;
            continue;
        }
        spooltodisk = (*grouprec[cug].subtype == '0') ? 1 : 0;

        if (nup) 
        {
            nup = 0;
            write_groups(1);
            if (*grouprec[cug].subtype != '0')
            {
                name_packet(pktname);
            }
        }
        if (nug) 
        {
            nug = 0;
            for ( ;; ) 
            {
                if (cgroup(sock, grouprec[cug].groupname) < 1) 
                {
                    if (cgroup(sock, grouprec[cug].groupname) == -1) 
                    {
                        if (sendauthinfo(sock))
                        {
                            continue;
                        }
                        return false;
                    }
                    if (++bogus > 10) 
                    {
                        log_it(1, "\n \xFE Too many invalid newsgroups... NEWS.RC error?");
                        return false;
                    }
                    log_it(!QUIET, "\n \xFE Error accessing - %s", grouprec[cug].groupname);
                    nug = 1;
                    break;
                } 
                else
                {
                    break;
                }
            }
            if (!nug) 
            {
                output("   ");
                backline();
                log_it(MOREINFO || !QUIET, "\r \xFE Requesting %s ", grouprec[cug].groupname);
                if (QUIET)
                {
                    fprintf(stderr, "\r \xFE Retrieving newsgroup #%d from %s ", cug + 1, serverhost);
                }
                if (grouprec[cug].lastread < cur_first)
                {
                    grouprec[cug].lastread = cur_first;
                }
                if (grouprec[cug].lastread >= cur_last) 
                {
                    grouprec[cug].lastread = cur_last;
                    log_it(!QUIET, "- [No new articles]");
                    nug = 1;
                } 
                else 
                {
                    new_articles = cur_last - grouprec[cug].lastread;
                    if (new_articles) 
                    {
                        log_it(!QUIET, "- [%lu new article%s]", new_articles,
                            new_articles == 1 ? "" : "s");
                    } 
                    else 
                    {
                        log_it(!QUIET, "- [No new articles]");
                        nug = 1;
                    }
                    if (new_articles > 250L) 
                    {
                        max_articles = max_on_sub(cug);
                        if (max_articles && (new_articles > max_articles)) 
                        {
                            log_it(!QUIET, "- [latest %lu articles]", max_articles);
                            grouprec[cug].lastread = cur_last - max_articles;
                        }
                    } 
                    else
                    {
                        ++grouprec[cug].lastread;
                    }
                }
                postnews(sock, cug);
                if ( !nug && cur_numa == 0 )
                {
                    log_it(!QUIET, "\n \xFE No articles in %s...", grouprec[cug].groupname);
                    nug = 1;
                    continue;
                }
            }
        } 
        else if ( !cnext(sock) || grouprec[cug].lastread > cur_last ) 
        {
            log_it(!QUIET, "\n \xFE End of articles in %s", grouprec[cug].groupname);
            nug = 1;
            write_groups(0);
        }
        if (!nug) 
        {
            if (!QUIET) 
            {
                if (skipped)
                {
                    backline();
                }
                else
                {
                    output("\n");
                }
            }
            if (!QUIET) 
            {
                backline();
                output(" \xFE [%lu/%lu] : ", grouprec[cug].lastread, cur_last);
            }
            if ( chead(sock, grouprec[cug].lastread ) && checkx( cug ) ) 
            {
                skipped = 0;
                if (*cur_subject) 
                {
                    if (strlen(cur_subject) > 78)
                    {
                        cur_subject[78] = '\0';
                    }
                    treat(cur_subject);
                }
                strcpy(orig_subj, cur_subject);
                if (strlen(orig_subj) > 70)
                {
                    orig_subj[70] = 0;
                }
                if (*cur_from) 
                {
                    treat(cur_from);
                    if (strlen(cur_from) > 45)
                    {
                        cur_from[45] = 0;
                    }
                } 
                else
                {
                    strcpy(cur_from, "Unknown");
                }
                if (*cur_replyto) 
                {
                    if (strlen(cur_replyto) > 45)
                    {
                        cur_replyto[45] = 0;
                    }
                } 
                else
                {
                    strcpy(cur_replyto, "Unknown");
                }
                if (!QUIET)
                {
                    output("%-48.48s", orig_subj);
                }
                if (spooltodisk)
                {
                    sprintf(szFileName, "%sSPOOL\\NEWS%d.UUE", net_data, cug);
                }
                else
                {
                    sprintf(szFileName, "%sSPOOL\\INPUT1.MSG", net_data);
                }
                abort = 0;
                fcloseall();
                ok = savebody(sock, szFileName, cug, grouprec[cug].lastread, &abort);
                if (QUIET)
                {
                    output("\b\b\b");
                }
                if (ok && (*grouprec[cug].subtype != '0')) 
                {
                    if ((fp = fsh_open(pktname, "ab+")) == NULL) 
                    {
                        log_it(MOREINFO, "\n \xFE Unable to create %s!", pktname);
                        return false;
                    }
                    sprintf(szFileName, "%sSPOOL\\INPUT*.MSG", net_data);
                    done = 1;
                    parts = 0;
                    long hFind = _findfirst( szFileName, &ff );
                    int nFindNext = ( hFind != -1 ) ? 0 : -1;
                    if ( nFindNext == 0) 
                    {
                        parts = 1;
                        done = 0;
                    }
                    firstrun = 1;
                    while (!done) 
                    {
                        sprintf(szFileName, "%sSPOOL\\%s", net_data, ff.name);
                        int hInputFile = sh_open1(szFileName, O_RDONLY | O_BINARY);
                        text_len = (unsigned int) filelength( hInputFile );
                        if (text_len > 32000L) 
                        {
                            log_it(MOREINFO, "\n \xFE Truncating %lu bytes from input file",
                                text_len - 32000L);
                            text_len = 32000;
                        }
                        if ((p = (char *) malloc(32767)) == NULL) 
                        {
                            log_it(MOREINFO, "\n \xFE Insufficient memory to read entire message");
                            return false;
                        }
                        sh_read( hInputFile, (void *) p, text_len );
                        sh_close( hInputFile );
                        temptype = ( unsigned short ) atoi(grouprec[cug].subtype);
                        nh.tosys = sy;
                        nh.touser = 0;
                        nh.fromsys = 32767;
                        nh.fromuser = 0;
                        if (!temptype) 
                        {
                            nh.main_type = main_type_new_post;
                            nh.minor_type = 0;
                        } 
                        else 
                        {
                            nh.main_type = main_type_pre_post;
                            nh.minor_type = temptype;
                        }
                        nh.list_len = 0;
                        ++cur_daten;
                        nh.daten = cur_daten;
                        nh.method = 0;
                        if (parts > 1) 
                        {
                            sprintf(buf, "%d%s ", parts, ordinal_text(parts));
                            strncat(buf, orig_subj, 72);
                            strcpy(cur_subject, buf);
                            cur_subject[72] = '\0';
                        }
                        nh.length = text_len + strlen(cur_subject) + 1;
                        strncpy(mailname, cur_replyto, 46);
                        strcat(mailname, "\r\n");
                        nh.length += strlen(mailname);
                        if (nh.main_type == main_type_new_post)
						{
                            nh.length += strlen(grouprec[cug].subtype) + 1;
						}
                        if (firstrun) 
                        {
                            firstrun = 0;
                            if (strnicmp(cur_articleid, "re: ", 4) == 0) 
                            {
                                strncpy(reline, cur_articleid, 60);
                                sprintf(cur_articleid, "Re: %s\r\n\r\n", reline);
                            }
                        }
                        if (!*cur_date) 
                        {
                            time_t cur_daten = nh.daten;
                            strncpy(cur_date, ctime(&cur_daten), 24);
                            cur_date[24] = '\0';
                        }
                        strcat(cur_date, "\r\n");
                        nh.length += strlen(cur_date);
                        nh.length += strlen(cur_articleid);
                        fsh_write(&nh, sizeof(net_header_rec), 1, fp);
                        if (nh.main_type == main_type_new_post)
                        {
                            fsh_write(grouprec[cug].subtype, sizeof(char), strlen(grouprec[cug].subtype) +1, fp);
                        }
                        fsh_write(cur_subject, sizeof(char), strlen(cur_subject) +1, fp);
                        fsh_write(mailname, sizeof(char), strlen(mailname), fp);
                        fsh_write(cur_date, sizeof(char), strlen(cur_date), fp);
                        fsh_write(cur_articleid, sizeof(char), strlen(cur_articleid), fp);
                        fsh_write(p, sizeof(char), text_len, fp);
                        if ( p )
                        {
                            free(p);
                        }
                        unlink(szFileName);
                        nFindNext = _findnext( hFind, &ff );
                        if ( nFindNext == 0 ) 
                        {
                            done = 0;
                            ++parts;
                        } 
                        else
                        {
                            done = 1;
                        }
                    }
                    if (filelength(fileno(fp)) > 250000L)
                    {
                        nup = 1;
                    }
                    if (fp != NULL)
                    {
                        fclose(fp);
                    }
                }
            } 
            else
            {
                skipped = 1;
            }
            ++grouprec[cug].lastread;
            while (kbhit()) 
            {
                ch = ( char ) (getch());
                switch (ch) 
                {
                case '+':
                    if (spooltodisk) 
                    {
                        if (MOREINFO)
                        {
                            backline();
                        }
                        abort = 6;
                    }
                    break;
                case '}':
                    if (spooltodisk) 
                    {
                        if (MOREINFO)
                        {
                            backline();
                        }
                        abort = 5;
                    }
                    break;
                case ']':
                    if (spooltodisk) 
                    {
                        if (MOREINFO)
                        {
                            backline();
                        }
                        abort = 4;
                    }
                    break;
                case 9:
                    abort = 3;
                    break;
                case 32:
                    abort = 2;
                    break;
                case 27:
                    abort = 1;
                    break;
                default:
                    break;
                }
            }
            switch (abort) 
            {
            case 6:
                backline();
                output(" ? Skip how many articles? ");
                if (jgets(buf, 10)) 
                {
                    skip_articles = atol(buf);
                    if ((grouprec[cug].lastread += skip_articles) < cur_last) 
                    {
                        abort = 0;
                        break;
                    }
                } else
                    break;
            case 5:
                if ((grouprec[cug].lastread += 100) < cur_last) 
                {
                    abort = 0;
                    break;
                }
            case 4:
                if ((grouprec[cug].lastread += 10) < cur_last) 
                {
                    abort = 0;
                    break;
                }
            case 3:
                grouprec[cug].lastread = cur_last;
            case 2:
                nug = 1;
                abort = 0;
                break;
            default:
                break;
            }
        }
        if (nug) 
        {
            ++cug;
            if (*grouprec[cug].subtype == '0')
            {
                nup = 1;
            }
        }
    }
    write_groups(1);
    nntp_shutdown(sock);
    if (abort)
    {
        log_it(1, "\n \xFE Session aborted from keyboard");
    }
    return true;
}


int read_newshosts()
{
    char chk, *ss, szFileName[_MAX_PATH], line[STRING];
    int num = 0;
    fpos_t pos;
    FILE *fp;

    sprintf(szFileName, "%s\\NET.INI", g_szMainDirectory);
    if ((fp = fsh_open(szFileName, "rt")) == NULL) 
    {
        output("\n ! Unable to read %s.", szFileName);
        return 0;
    }
    while (fgets(line, 80, fp)) 
    {
        stripspace(line);
        ss = NULL;
        ss = strtok(line, "=");
        if (ss)
        {
            ss = strtok(NULL, "\n");
        }
        if ((line[0] == ';') || (line[0] == '\n') || (strlen(line) == 0))
        {
            continue;
        }
        if (strnicmp(line, "NEWSHOST", 8) == 0) 
        {
            if (isdigit(line[8])) 
            {
                if (ss) 
                {
                    strcpy(newshosts[num].hostname, ss);
                    newshosts[num].whichrc = line[8];
                    newshosts[num].newsname[0] = 0;
                    newshosts[num].newspass[0] = 0;
                    chk = line[8];
                    fgetpos(fp, &pos);
                    rewind(fp);
                    while (fgets(line, 80, fp)) 
                    {
                        if ((strnicmp(line, "NEWSNAME", 8) == 0) && (line[8] == chk)) 
                        {
                            ss = strtok(line, "=");
                            if (ss) 
                            {
                                ss = strtok(NULL, "\n");
                                trimstr1(ss);
                                strcpy(newshosts[num].newsname, ss);
                            }
                        }
                        if ((strnicmp(line, "NEWSPASS", 8) == 0) && (line[8] == chk)) 
                        {
                            ss = strtok(line, "=");
                            if (ss) 
                            {
                                ss = strtok(NULL, "\n");
                                trimstr1(ss);
                                strcpy(newshosts[num].newspass, ss);
                            }
                        }
                    }
                }
                ++num;
                fsetpos(fp, &pos);
            }
            continue;
        }
    }
    if (fp != NULL)
    {
        fclose(fp);
    }
    return num;
}


int GetInstanceNumber()
{
    int nInstanceNumber = 1;
    char* ss = getenv("WWIV_INSTANCE");
    if (ss) 
    {
        nInstanceNumber = atoi(ss);
        if ( nInstanceNumber <= 0 || nInstanceNumber >= 1000 )
        {
            log_it(1, "\n \xFE WWIV_INSTANCE set to %d.  Can only be 1..999!", nInstanceNumber);
            nInstanceNumber = 1;
        }
    } 
    return nInstanceNumber;
}

bool ReadConfig()
{
    char szFileName[_MAX_PATH];
    sprintf(szFileName, "%s\\CONFIG.DAT", g_szMainDirectory);
    int hFile = sh_open1(szFileName, O_RDONLY | O_BINARY);
    if (hFile < 0) 
    {
        output("\n \xFE %s NOT FOUND.", szFileName);
        cursor('R');
        return false;
    }
    sh_read(hFile, (void *) (&syscfg), sizeof(configrec));
    sh_close(hFile);
    return true;
}


bool ReadNetIniFile( bool bUseMultiNewsHosts )
{
    char s[255], szFileName[_MAX_PATH];
    FILE *fp;
    
    sprintf(szFileName, "%s\\NET.INI", g_szMainDirectory);
    if ((fp = fsh_open(szFileName, "rt")) == NULL)
    {
        output("\n \xFE Unable to read %s", szFileName);
        log_it( 1, "\n \xFE Unable to read %s", szFileName);
        return false;
    }

    while (fgets(s, 100, fp)) 
    {
        char* ss = NULL;
        stripspace(s);
        if ((*s == ';') || (*s == '\n') || (*s == '['))
        {
            continue;
        }
        if (strlen(s) == 0)
        {
            continue;
        }
        if ((strnicmp(s, "POPNAME", 7) == 0) && (POPNAME[0] == 0)) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                strcpy(POPNAME, ss);
            }
        }
        if (strnicmp(s, "FWDNAME", 7) == 0) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                strcpy(POPNAME, ss);
            }
        }
        if ((strnicmp(s, "DOMAIN", 6) == 0) && (POPDOMAIN[0] == 0)) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                strcpy(POPDOMAIN, ss);
            }
        }
        if (strnicmp(s, "FWDDOM", 6) == 0) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                strcpy(POPDOMAIN, ss);
            }
        }
        if (strnicmp(s, "XPOSTS", 6) == 0) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                crossposts = atoi(ss);
                if (crossposts > 99)
                {
                    crossposts = 10;
                }
            }
        }
        if (strnicmp(s, "MOREINFO", 8) == 0) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                if ((ss[0] == 'n') || (ss[0] == 'N'))
                {
                    MOREINFO = 0;
                }
            }
        }
        if (strnicmp(s, "QUIET", 5) == 0) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                if ((ss[0] == 'y') || (ss[0] == 'Y'))
                {
                    QUIET = 1;
                }
            }
        }
        if (strnicmp(s, "BINXPOST", 8) == 0) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                if ((ss[0] == 'y') || (ss[0] == 'Y'))
                {
                    binxpost = 1;
                }
            }
        }
        if (strnicmp(s, "NEWSRC_UPD", 9) == 0) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                if ((ss[0] == 'y') || (ss[0] == 'Y'))
                {
                    newsrc_upd = 1;
                }
            }
        }
        if ((strnicmp(s, "NEWSNAME", 8) == 0) && (!bUseMultiNewsHosts)) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                strcpy(NEWSNAME, ss);
            }
        }
        if ((strnicmp(s, "NEWSPASS", 8) == 0) && (!bUseMultiNewsHosts)) 
        {
            ss = strtok(s, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\n");
                trimstr1(ss);
                strcpy(NEWSPASS, ss);
            }
        }
    }
    if (fp != NULL)
    {
        fclose(fp);
    }
    return true;
}


void BackupNewsFile()
{
    char szFileName[_MAX_PATH], szTempFileName[_MAX_PATH];

    // NOTE: whichrc is NULL when there is only 1 newsserver, hence the odd looking
    // sprintf followed by a strcat.  
    sprintf(szFileName, "%sNEWS%c", net_data, whichrc);
    strcat( szFileName, ".RC" );
    sprintf(szTempFileName, "%sNEWS%c", net_data, whichrc);
    strcat( szTempFileName, ".BAK" );
    if (exist(szTempFileName))
    {
        unlink(szTempFileName);
    }
    copyfile(szFileName, szTempFileName);
}

int main(int argc, char *argv[])
{
    SOCKET sock;
    bool bUseMultiNewsHosts = false;
    bool ok = false;

    crossposts = 10;
    binxpost = 0;
    newsrc_upd = 0;
    MOREINFO = 1;
    QUIET = 0;
    POPNAME[0] = 0;
    POPDOMAIN[0] = 0;
    whichrc = '\0';

    cursor('S');
    cursor('H');
    output("\n \xFE %s", version);

    if (argc != 4) 
    {
        output("\n \xFE Invalid arguments for %s\n", argv[0]);
        cursor('R');
        WSACleanup();
        return EXIT_FAILURE;
    }
    sprintf( net_data, "%s\\", argv[1] );

    sy = ( unsigned short ) atoi(argv[3]);

    get_dir(g_szMainDirectory, false);

    time( &cur_daten );

    int nNumNewsHosts = 0;
    if (argv[2][0] != '-') 
    {
        strcpy(serverhost, argv[2]);
        nNumNewsHosts = 1;
    } 
    else 
    {
        bUseMultiNewsHosts = true;
        newshosts = (NEWSHOST *) malloc(10 * sizeof(NEWSHOST));
        ::ZeroMemory( newshosts, 10 * sizeof( NEWSHOST ) );
        if (newshosts)
        {
            nNumNewsHosts = read_newshosts();
        }
    }

    instance = GetInstanceNumber();

    if ( !ReadConfig() )
    {
        return EXIT_FAILURE;
    }

    if ( !ReadNetIniFile( bUseMultiNewsHosts ) )
    {
        return EXIT_FAILURE;
    }


    if ( !InitializeWinsock() )
    {
        log_it( 1, "\n Unable to initialize WinSock, Exiting...\n" );
        WSACleanup();
        return EXIT_FAILURE;
    }


    for (int nCurrentHost = 0; nCurrentHost < nNumNewsHosts; nCurrentHost++) 
    {
        if (bUseMultiNewsHosts) 
        {
            strcpy(serverhost, newshosts[nCurrentHost].hostname);
            whichrc = newshosts[nCurrentHost].whichrc;
            strcpy(NEWSNAME, newshosts[nCurrentHost].newsname);
            strcpy(NEWSPASS, newshosts[nCurrentHost].newspass);
        }

        if (read_groups() == 0) 
        {
            output("\n \xFE Unable to access newsgroup file NEWS%c.RC!", whichrc);
            cursor('R');
            WSACleanup();
            return EXIT_FAILURE;
        } 

        BackupNewsFile();

        if (stricmp(grouprec[0].groupname, "newsrc") == 0)
        {
            log_it(1, "\n \xFE Forced retrieval of newsgroup listing from %s.",
                serverhost);
        }
        else
        {
            log_it(0, "\n \xFE %u newsgroup%s defined in NEWS.RC.", ngroups,
                ngroups == 1 ? "" : "s");
        }

        ok = false;
		sock = netsocket(serverhost);
        if ( sock != NULL && sock != INVALID_SOCKET ) 
        {
            bool bAuth = true;
            if ((NEWSNAME[0]) && (whichrc != NULL)) 
            {
                if (!sendauthinfo(sock))
                {
                    bAuth = false;
                }
            }
            if (bAuth)
            {
                ok = ( getnews(sock) ) ? true : false;
            }
            if (!ok)
            {
                log_it(1, "\n \xFE NEWS exited with an error.");
            }
            sock = INVALID_SOCKET;
        }
        cd_to(g_szMainDirectory);
        log_it(1, "\n \xFE NEWS completed processing %d newsgroups", ngroups);
        if (grouprec)
        {
            free(grouprec);
        }
        grouprec = NULL;
        NEWSNAME[0] = '\0';
        NEWSPASS[0] = '\0';
    }
    check_packets();
    fcloseall();
    cursor('R');
    return ( ok ) ? 0 : 1;
}
