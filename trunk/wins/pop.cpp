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
#include "pop.h"
#include "socketwrapper.h"

#define MEGA_DEBUG_LOG_INFO

#if defined ( MEGA_DEBUG_LOG_INFO )
#define DEBUG_LOG_IT(s) log_it( true, s )
#define DEBUG_LOG_IT_1(s, a) log_it( true, s, a )
#define DEBUG_LOG_IT_2(s, a, b) log_it( true, s, a, b )
#define DEBUG_LOG_IT_3(s, a, b, c) log_it( true, s, a, b, c )
#define DEBUG_LOG_IT_4(s, a, b, c, d) log_it( true, s, a, b, c, d );
#define DEBUG_LOG_IT_5(s, a, b, d, e) log_it( true, s, a, b, c, d, e );
#else	// MEGA_DEBUG_LOG_INFO
#define DEBUG_LOG_IT(s) 
#define DEBUG_LOG_IT_1(s, a)
#define DEBUG_LOG_IT_2(s, a, b)
#define DEBUG_LOG_IT_3(s, a, b, c)
#define DEBUG_LOG_IT_4(s, a, b, c, d)
#define DEBUG_LOG_IT_5(s, a, b, d, e)
#endif	// MEGA_DEBUG_LOG_INFO


char *fix_quoted_commas(char *string)
{
    int quoted = 0;

    char* ptr = string;
    if (ptr) 
    {
        while (*ptr != 0) 
        {
            if (*ptr == '\"')
            {
                quoted = (!quoted);
            }
            if (*ptr == ',' && quoted)
            {
                *ptr = ( char ) '\xb3';
            }
            ptr = &ptr[1];
        }
    }
    return string;
}

void statusbar(statusbarrec * sb, int now, int tot)
{
    if (DEBUG)
    {
        return;
    }

    if (sb->current_item == 0) 
    {
        int x = 0;
        go_back(WhereX(), 1);
        output(" * File %3.3d/%3.3d ", now, tot);
        _putch(sb->side_char1);
        while (x < sb->width) 
        {
            _putch(sb->empty_space);
            ++x;
        }
        _putch(sb->side_char2);
        x = 0;
        while (x < sb->width) 
        {
            _putch('\b');
            ++x;
        }
        sb->last_maj_pos = 0;
        sb->last_min_pos = 0;
        return;
    }

    float pos = (static_cast<float>(sb->current_item) / sb->total_items);
    const int total_fractions = (sb->width) * sb->amount_per_square;
    pos = pos * total_fractions;
    int maj_pos = static_cast<int>(pos / sb->amount_per_square);
    int min_pos = static_cast<int>(pos - (maj_pos * sb->amount_per_square));

    if (min_pos == 0)
    {
        min_pos = sb->amount_per_square - 1;
    }
    else
    {
        --min_pos;
    }

    if (maj_pos == sb->last_maj_pos) 
    {
        if (min_pos == sb->last_min_pos)
        {
            return;
        }
        _putch('\b');
        _putch(sb->square_list[min_pos]);
        sb->last_min_pos = min_pos;
        return;
    }
    _putch('\b');
    _putch(sb->square_list[sb->amount_per_square - 1]);
    sb->last_min_pos = min_pos;

    ++sb->last_maj_pos;
    while (sb->last_maj_pos < maj_pos) 
    {
        ++sb->last_maj_pos;
        if (WhereX() < 80)
        {
            _putch(sb->square_list[sb->amount_per_square - 1]);
        }
    }
    if (WhereX() < 80)
    {
        _putch(sb->square_list[min_pos]);
    }

    sb->last_maj_pos = maj_pos;
}



SOCKET smtp_start( char *host, char *dom )
{
    SOCKET sock = socket( AF_INET, SOCK_STREAM, 0 );
	if ( sock == INVALID_SOCKET )
	{
		int nWSAErrorCode = WSAGetLastError();
		log_it( true, "\n [SMTP] INVALID_SOCKET trying to create socket WSAGetLastError = [%d]", nWSAErrorCode );
		return NULL;
	}
	
	SOCKADDR_IN sockAddr;
	ZeroMemory(&sockAddr, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons( SMTPPORT );
	sockAddr.sin_addr.s_addr = inet_addr( host );
	if (sockAddr.sin_addr.s_addr == INADDR_NONE)
	{
		LPHOSTENT lphost = gethostbyname( host );
		if (lphost != NULL)
		{
			sockAddr.sin_addr.s_addr = ((LPIN_ADDR)lphost->h_addr)->s_addr;
		}
		else
		{
			SMTP_Err_Cond = SMTP_FAILED;
			log_it( true, "\n \xFE Error : Cannot resolve host %s", host);
			return NULL;
		}
	}

	if ( connect( sock, ( SOCKADDR *) &sockAddr, sizeof ( sockAddr ) ) == SOCKET_ERROR )
	{
		SMTP_Err_Cond = SMTP_FAILED;
		log_it( true, "\n \xFE Error : Unable to connect to %s : %d", host, SMTPPORT);
		return NULL;
	}

	DEBUG_LOG_IT( "\n [SMTP] Past SMTP connect" );
	
	sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
	log_it(DEBUG, "\n - SMTP> %s", _temp_buffer);
	
	sprintf(_temp_buffer, "HELO %s", dom);
	sock_puts(sock, _temp_buffer);
	while (sock_tbused(sock) > 0) 
	{
		SOCK_GETS(SMTP);
		SMTP_FAIL_ON(SMTP_OOPS,NOOP);
		SMTP_FAIL_ON(SMTP_SYNTAX,NOOP);
		SMTP_FAIL_ON(SMTP_PARAM,NOOP);
		SMTP_FAIL_ON(SMTP_ACCESS,NOOP);
		SMTP_FAIL_ON(SMTP_BAD_PARM,NOOP);
	}
	SOCK_READ_ERR(SMTP,NOOP);
	return sock;

}

char *smtp_parse_from_line(FILE * fpMessageFile)
{
	char s[161];
	bool found = false, done = false;
	
	rewind( fpMessageFile );
	while ( !feof(fpMessageFile) && !done ) 
	{
		fgets( s, sizeof(s)-1, fpMessageFile );
		if ( s && *s == '\n' )
		{
			done = true;
		}
		else if ( _strnicmp(s, "from:", 5) == 0 && strchr(s, '@') != NULL ) 
		{
			found = true;
			done = true;
		}
	}
	if ( found )
	{
		unsigned int beginfocus = 0, endfocus = 0;
		if ( ( beginfocus = strcspn( s, "<" ) ) != strlen( s ) ) 
		{
			++beginfocus;
			endfocus = strcspn( s, ">" );
			s[endfocus] = NULL;
		} 
		else
		{
			beginfocus = 5;
		}
		return trim( _strdup( &s[beginfocus] ) );
	}
	return NULL;
}


bool find_listname(FILE * fpMessageFile)
{
	char *ss = NULL, s[161];
	bool found = false, done = false;
	
	*LISTNAME = 0;
	rewind(fpMessageFile);
	while (!feof(fpMessageFile) && !done) 
	{
		fgets(s, sizeof(s)-1, fpMessageFile);
		if (*s == '\n')
		{
			done = true;
		}
		else if ((_strnicmp(s, "x-reply-to", 10) == 0) && (strchr(s, '@') != 0) && (strchr(s, '\"') != 0)) 
		{
			found = true;
			done = true;
		}
	}
	if (found) 
	{
		ss = strtok(s, "\"");
		if (ss) 
		{
			ss = strtok(NULL, "\"");
			trimstr1(ss);
			strcpy(LISTNAME, ss);
		}
		if (ss)
		{
			return true;
		}
	}
	return false;
}


char **smtp_parse_to_line(FILE * fpMessageFile)
{
	int current = 0;
	char **list = NULL;
	char *addr, _temp_addr[120], buf[120];
	bool done = false;
	
	rewind(fpMessageFile);
	while (!feof(fpMessageFile) && !done) 
	{
		fgets(_temp_buffer, sizeof(_temp_buffer), fpMessageFile);
		if (*_temp_buffer == '\n')
		{
			done = true;
		}
		else if ((_strnicmp(_temp_buffer, "to:", 3) == 0) ||
			(_strnicmp(_temp_buffer, "cc:", 3) == 0) ||
			(_strnicmp(_temp_buffer, "bcc:", 4) == 0)) 
		{
			fix_quoted_commas(_temp_buffer);
			addr = strtok(_temp_buffer, ":");
			addr = strtok(NULL, "\r\n");
			trimstr1(addr);
			strcpy(_temp_addr, addr);
			if ((strchr(_temp_addr, ' ')) || (strchr(_temp_addr, ')')) || (strchr(_temp_addr, '\"'))) 
			{
				*buf = 0;
				int i1 = 0;
				int i = strcspn(_temp_addr, "@");
				while (i > 0 && _temp_addr[i - 1] != ' ' && _temp_addr[i - 1] != '<')
				{
					--i;
				}
				while (*_temp_addr && _temp_addr[i] != ' ' && _temp_addr[i] != '>')
				{
					buf[i1++] = _temp_addr[i++];
				}
				buf[i1] = '\0';
				addr = buf;
			}
			list = (char **) realloc(list, sizeof(char *) * ((current) + 2));
			list[current] = _strdup(addr);
			list[current + 1] = NULL;
			current++;
		}
	}
	return list;
}


int smtp_send_MAIL_FROM_line(SOCKET sock, FILE * fpMessageFile)
{
	char *from = smtp_parse_from_line(fpMessageFile);
	if (from) 
	{
		sprintf(_temp_buffer, "RSET");
		sock_puts(sock, _temp_buffer);
		while (sock_tbused(sock) > 0) 
		{
			SOCK_GETS(SMTP);
			SMTP_FAIL_ON(SMTP_SYNTAX,NOOP);
			SMTP_FAIL_ON(SMTP_PARAM,NOOP);
			SMTP_FAIL_ON(SMTP_BAD_PARM,NOOP);
			SMTP_FAIL_ON(SMTP_OOPS,NOOP);
		}
		if (DEBUG)
		{
			output("\n - SMTP> Mail From:<%s>", from);
		}
		sprintf(_temp_buffer, "MAIL FROM:<%s>", from);
		strcpy(MAILFROM, from);
		sock_puts(sock, _temp_buffer);
		free(from);
		while (sock_tbused(sock) > 0) 
		{
			SOCK_GETS(SMTP);
			SMTP_FAIL_ON(SMTP_OOPS,NOOP);
			SMTP_FAIL_ON(SMTP_FULL,NOOP);
			SMTP_FAIL_ON(SMTP_ERROR,NOOP);
			SMTP_FAIL_ON(SMTP_SQUEEZED,NOOP);
			SMTP_FAIL_ON(SMTP_SYNTAX,NOOP);
		}
	}
	SOCK_READ_ERR(SMTP,NOOP);
	return 1;
}

#define FREE_ALL for (int i=0; to_list[i]!=NULL; i++) if (to_list[i]) free(to_list[i]); if (to_list) free(to_list);

int smtp_send_RCPT_TO_line(SOCKET sock, FILE * fpMessageFile)
{
    char **to_list;
    bool done = false;

    to_list = smtp_parse_to_line(fpMessageFile);
    for (int i = 0; (to_list[i] != NULL && !done); i++) 
    {
        if ((strchr(to_list[i], '@') == NULL) || (strchr(to_list[i], '.') == NULL)) 
        {
            log_it( true, "\n ! Invalid recipient - %s - aborting message.", to_list[i]);
            sock_puts(sock, "RSET");
            done = true;
        } 
        else 
        {
            log_it(DEBUG, "\n - SMTP> Rcpt To:<%s>", to_list[i]);
            sprintf(_temp_buffer, "RCPT TO:<%s>", to_list[i]);
            sock_puts(sock, _temp_buffer);
        }
        while (sock_tbused(sock) > 0) 
        {
            SOCK_GETS(SMTP);
            SMTP_FAIL_ON(SMTP_OOPS, FREE_ALL);
            SMTP_RESET_ON(SMTP_SYNTAX, FREE_ALL);
            SMTP_RESET_ON(SMTP_PARAM, FREE_ALL);
            SMTP_RESET_ON(SMTP_ACCESS, FREE_ALL);
            SMTP_RESET_ON(SMTP_BAD_SEQ, FREE_ALL);
        }
    }

    SOCK_READ_ERR(SMTP, FREE_ALL);

    FREE_ALL;

    return 1;
}

#undef FREE_ALL

bool smtp_sendf(SOCKET sock, FILE * fp, long cb, long tb, int cf, int tf, bool bSkip)
{
	statusbarrec sb;
	
	sb.width = 59;
	sb.amount_per_square = 2;
	sb.square_list[0] = ( char ) '.';
	sb.square_list[1] = ( char ) '\xFE';
	sb.empty_space = ( char ) '.';
	sb.side_char1 = '[';
	sb.side_char2 = ']';
	sb.current_item = 0;
	sb.total_items = tb;
	statusbar(&sb, cf, tf);
	sb.current_item = cb;
	statusbar(&sb, cf, tf);
	
	fseek(fp, 0L, SEEK_END);
	long obytes = ftell(fp);
	rewind(fp);
	sock_puts(sock, "DATA");
	while (sock_tbused(sock) > 0) 
	{
		SOCK_GETS(SMTP);
		if (DEBUG)
		{
			log_it(DEBUG, "\n - SMTP> %s", _temp_buffer);
		}
		SMTP_FAIL_ON(SMTP_OOPS,NOOP);
		SMTP_RESET_ON(SMTP_BAD_SEQ,NOOP);
		SMTP_RESET_ON(SMTP_SYNTAX,NOOP);
		SMTP_RESET_ON(SMTP_PARAM,NOOP);
		SMTP_RESET_ON(SMTP_ACCESS,NOOP);
		SMTP_RESET_ON(SMTP_COM_NI,NOOP);
		SMTP_RESET_ON(SMTP_FAILED,NOOP);
		SMTP_RESET_ON(SMTP_ERROR,NOOP);
	}
	long nbytes = 0L;
	long rbytes = 256L, cbytes = 256L;
	int pos = WhereX();
	if (DEBUG) 
	{
		output("               ");
		go_back(WhereX(), pos);
	}
	bool in_header = true;
	bool sent_from = false;
	while (feof(fp) == 0 && fgets(_temp_buffer, sizeof(_temp_buffer), fp))
	{
		sb.current_item += strlen(_temp_buffer);
		rip(_temp_buffer);
		char *pszTemp = _strdup(_temp_buffer);
		trim(pszTemp);
		if (strlen(pszTemp) == 0)
		{
			in_header = false;
		}
		free(pszTemp);
		
		if (*_temp_buffer == '.') 
		{
			memmove(_temp_buffer, _temp_buffer + 1, sizeof(_temp_buffer) - 1);
			*_temp_buffer = '.';
		}
		
		if (bSkip && *LISTNAME && (_strnicmp(_temp_buffer, "to:", 3) == 0) && in_header ) 
		{
			if (!sent_from) 
			{
				sprintf(_temp_buffer, "To: \"Multiple Recipients of Mailing List %s\" <%s>",
					LISTNAME, MAILFROM);
				sent_from = true;
			} 
			else
			{
				continue;
			}
		}
		
		nbytes += sock_puts(sock, _temp_buffer) + 2;
		
		if (nbytes > 2000L)
		{
			cbytes = 512L;
		}
		if (nbytes > 4000L)
		{
			cbytes = 1024L;
		}
		if (nbytes > 16000L)
		{
			cbytes = 2048L;
		}
		if (nbytes > 32000L)
		{
			cbytes = 4096L;
		}
		if (nbytes > 64000L)
		{
			cbytes = 8192L;
		}
		if ((nbytes > rbytes) && (DEBUG)) 
		{
			go_back(WhereX(), pos);
			output("%ld/%ld", nbytes, obytes);
			rbytes += cbytes;
		} 
		else
		{
			statusbar(&sb, cf, tf);
		}
		
		if (_kbhit()) 
		{
			go_back(WhereX(), pos);
			output(" aborted.");
			aborted = true;
			return false;
		}
	}
	sock_puts(sock, "");
	sock_puts(sock, ".");
	while (sock_tbused(sock) > 0) 
	{
		SOCK_GETS(SMTP);
		SMTP_FAIL_ON(SMTP_OOPS,NOOP);
		SMTP_RESET_ON(SMTP_ERROR,NOOP);
		SMTP_RESET_ON(SMTP_SQUEEZED,NOOP);
		SMTP_RESET_ON(SMTP_FULL,NOOP);
		SMTP_RESET_ON(SMTP_FAILED,NOOP);
	}
	if (*_temp_buffer == '2') 
	{
		log_it(DEBUG, "\n - SMTP> %s", _temp_buffer);
	}
	if (DEBUG) 
	{
		go_back(WhereX(), pos);
		output("accepted.");
	}
	return true;
	
}


void smtp_shutdown(SOCKET sock)
{
	if (sock) 
	{
		sock_puts(sock, "QUIT");
		sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
		closesocket(sock);
		
	}
	output(" ");
	SOCK_READ_ERR(SMTP, closesocket(sock));
}


SOCKET pop_init(char *host)
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0 );
	if ( sock == INVALID_SOCKET )
	{
		return NULL;
	}
	
	SOCKADDR_IN sockAddr;
	ZeroMemory(&sockAddr, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(POPPORT );
	sockAddr.sin_addr.s_addr = inet_addr( host );
	if (sockAddr.sin_addr.s_addr == INADDR_NONE)
	{
		LPHOSTENT lphost = gethostbyname( host );
		if (lphost != NULL)
		{
			sockAddr.sin_addr.s_addr = ((LPIN_ADDR)lphost->h_addr)->s_addr;
		}
		else
		{
			SMTP_Err_Cond = SMTP_FAILED;
			log_it( true, "\n \xFE Error : Cannot resolve host %s", host);
			return NULL;
		}
	}

	if ( connect( sock, ( SOCKADDR *) &sockAddr, sizeof ( sockAddr ) ) == SOCKET_ERROR )
	{
		SMTP_Err_Cond = SMTP_FAILED;
		log_it( true, "\n \xFE Error : Unable to connect to %s", host);
		return NULL;
	}
	

	sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
	log_it(DEBUG, "\n - POP> %s", _temp_buffer);
	if (*_temp_buffer != '+') 
	{
		POP_Err_Cond = POP_HOST_UNAVAILABLE;
		log_it( true, "\n \xFE Error : Host %s is unavailable.", host);
		return NULL;
	} 
	else 
	{
		POP_Err_Cond = POP_OK;
		log_it(DEBUG, "\n - POP socket initialized.");
		return sock;
	}
}

int pop_login(SOCKET sock, char *userid, char *password, char *host, bool wingate)
{
    if (wingate)
    {
        sprintf(_temp_buffer, "USER %s#%s", userid, host);
    }
    else
    {
        sprintf(_temp_buffer, "USER %s", userid);
    }
    sock_puts(sock, _temp_buffer);
    sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
    log_it(DEBUG, "\n - POP> %s", _temp_buffer);
    if (*_temp_buffer != '+') 
    {
        POP_Err_Cond = POP_BAD_MBOX;
        log_it( true, "\n \xFE Error : host report mailbox %s does not exist", userid);
        if (sock) 
        {
            sock_puts(sock, "QUIT");
            closesocket(sock);
        }
        return 0;
    }
    sprintf(_temp_buffer, "PASS %s", password);
    sock_puts(sock, _temp_buffer);
    sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
    log_it(DEBUG, "\n - POP> %s", _temp_buffer);
    if (*_temp_buffer != '+') 
    {
        POP_Err_Cond = POP_BAD_PASS;
        log_it( true, "\n \xFE Error : Host reports password incorrect or account locked.");
        if (sock) 
        {
            sock_puts(sock, "QUIT");
            closesocket(sock);
        }
        return 0;
    }
    SOCK_READ_ERR(POP,NOOP);
    return 1;
}

int pop_status(SOCKET sock, unsigned int *count, unsigned long *totallength)
{
    char junk[12];

    sock_puts(sock, "STAT");
    sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
    if (*_temp_buffer != '+') 
    {
        POP_Err_Cond = POP_UNKNOWN;
        log_it(DEBUG, "\n \xFE Error : Unknown POP error.");
        return 0;
    } 
    else
    {
        sscanf(_temp_buffer, "%s %u %lu", junk, count, totallength);
    }

    SOCK_READ_ERR(POP,NOOP);
    return 1;
}

long pop_length(SOCKET sock, unsigned int msg_num, unsigned long *size)
{
    char junk[21];
    unsigned int dummy;

    sock_flushbuffer( sock );
    sprintf( _temp_buffer, "LIST %u", msg_num );
    sock_puts(sock, _temp_buffer);
    sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));

    if ( *_temp_buffer == '.' ) 
    {
        // work around problem with mdaemon and messages with an
        // extra '.' character at the end.
        Sleep( 100 );
        sock_flushbuffer( sock );
        sprintf( _temp_buffer, "LIST %u", msg_num );
        sock_puts(sock, _temp_buffer);
        Sleep( 100 );
        sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
    }
    
    if ( *_temp_buffer != '+' ) 
    {
        POP_Err_Cond = POP_NOT_MSG;
        log_it( DEBUG, "\n \xFE Error : No message #%u", msg_num );
        log_it( DEBUG, "\n \xFE Debug : _temp_buffer=[%s]", _temp_buffer );
        return 0;
    } 
    else
    {
        sscanf(_temp_buffer, "%s %u %lu", &junk, &dummy, size);
    }

    SOCK_READ_ERR(POP,NOOP);
    if (*size == 0L) 
    {
        log_it( true, "\n \xFE Mailbox contains a zero byte file -- deleting Message #%u!", msg_num);
        sprintf(_temp_buffer, "DELE %u", msg_num);
        sock_puts(sock, _temp_buffer);
        sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
        log_it(DEBUG, "\n - POP> %s", _temp_buffer);
        if (*_temp_buffer != '+') 
        {
            POP_Err_Cond = POP_NOT_MSG;
            log_it( true, "\n \xFE Error : No message #%u", msg_num);
        }
        sock_puts(sock, "QUIT");
        sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
        if (*_temp_buffer != '+') 
        {
            POP_Err_Cond = POP_UNKNOWN;
            log_it( true, "\n \xFE Error : Unable to update mailbox.");
        } 
        else
        {
            log_it(DEBUG, "\n \xFE Close and Updated mailbox.");
        }
        closesocket(sock);
    }
    return (*size);
}


bool checkspam(char *pszText)
{
	char szFileName[_MAX_PATH], buf[81], tmp[81];
	bool spam = false;
	sprintf(szFileName, "%sNOSPAM.TXT", net_data);
    FILE *fp = fsh_open(szFileName, "r");
	if (fp != NULL) 
	{
		bool ok = false;
		while (!feof(fp) && !spam) 
		{
			fgets(buf, sizeof(buf)-1, fp);
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
					ok = (_strnicmp(buf, "[GLOBAL]", 8) == 0 || _strnicmp(buf, "[MAIL]", 6) == 0);
				}
				if (ok && stristr(pszText, buf))
				{
					spam = true;
				}
			}
		}
		fclose(fp);
	}
	return spam;
}

bool checkfido(char *pszText)
{
    char szFileName[_MAX_PATH];

    bool spam = false;
    sprintf(szFileName, "%sFIWPKT.TXT", net_data);
    FILE *fp = fsh_open(szFileName, "r");
    if (fp != NULL)
    {
        while (!feof(fp) && !spam) 
        {
            char szCurrentLine[255];
            fgets(szCurrentLine, sizeof(szCurrentLine)-1, fp);
            trimstr1(szCurrentLine);
            if (strlen(szCurrentLine) > 2) 
            {
                if (szCurrentLine[0] == '\"') 
                {
                    char szTemp[255];
                    strcpy(szTemp, &(szCurrentLine[1]));
                    LAST(szTemp) = '\0';
                    strcpy(szCurrentLine, szTemp);
                }
                if (stristr(pszText, szCurrentLine))
                {
                    spam = true;
                }
            }
        }
    }
    if (fp != NULL)
    {
        fclose(fp);
    }
    return spam;
}

#define MAX_IDS 250

bool compact_msgid()
{
    char szNewFileName[_MAX_PATH], szOldFileName[_MAX_PATH];
    Message_ID messageid;

    sprintf(szOldFileName, "%sMSGID.OLD", net_data);
    _unlink(szOldFileName);
    sprintf(szNewFileName, "%sMSGID.DAT", net_data);
    rename(szNewFileName, szOldFileName);
    int hOldFile = sh_open(szOldFileName, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
    if (hOldFile < 0) 
    {
        log_it( true, "\n ! Unable to read %s.", szOldFileName);
        return false;
    }

    int hNewFile = sh_open(szNewFileName, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
    if (hNewFile < 0) 
    {
        log_it( true, "\n ! Unable to create %s.", szNewFileName);
        return false;
    }
    int nNewLocation = 0;
    for (int nOldLocation = 50; nOldLocation < MAX_IDS; nOldLocation++) 
    {
        sh_lseek(hOldFile, nOldLocation * sizeof(Message_ID), SEEK_SET);
        sh_read(hOldFile, &messageid, sizeof(Message_ID));
        sh_lseek(hNewFile, nNewLocation++ * sizeof(Message_ID), SEEK_SET);
        sh_write(hNewFile, &messageid, sizeof(Message_ID));
    }
    hOldFile = sh_close(hOldFile);
    hNewFile = sh_close(hNewFile);
    _unlink(szOldFileName);
    return true;
}

/** returns false if the message is a dupe, true otherwise */
bool check_messageid(bool add, char *msgid)
{
    char szFileName[_MAX_PATH];
	_snprintf(szFileName, _MAX_PATH, "%sMSGID.DAT", net_data);
    int hMsgIdFile = sh_open(szFileName, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
    if (hMsgIdFile < 0) 
    {
        log_it( true, "\n ! Unable to create %s.", szFileName);
        return true;	// -1
    }

	int nNumberOfMessageIDs = static_cast<int>(_filelength(hMsgIdFile) / sizeof(Message_ID));
    if (nNumberOfMessageIDs > MAX_IDS)
    {
        compact_ids = true;
    }

    Message_ID messageid;
	bool messageNotDupe = true;
    if (!add) 
    {
        log_it(DEBUG, "\n - Scanning previous %d Message-IDs.", nNumberOfMessageIDs);
        for (int i = 0; i < nNumberOfMessageIDs && messageNotDupe; i++) 
        {
            sh_lseek(hMsgIdFile, i * sizeof(Message_ID), SEEK_SET);
            sh_read(hMsgIdFile, &messageid, sizeof(Message_ID));
            if (strcmp(messageid.msgid, msgid) == 0)
            {
                messageNotDupe = false;
            }
        }
    } 
    else 
    {
        strncpy(messageid.msgid, msgid, 80);
        messageid.msgid[80] = '\0';
        log_it(DEBUG, "\n \xFE Adding new Message-ID:%s", messageid.msgid);
        sh_lseek(hMsgIdFile, nNumberOfMessageIDs * sizeof(Message_ID), SEEK_SET);
        sh_write(hMsgIdFile, &messageid, sizeof(Message_ID));
    }
    hMsgIdFile = sh_close(hMsgIdFile);
	return messageNotDupe;
}

int pop_top(SOCKET sock, unsigned int msg_num, int usernum)
{
    bool found_from = false, found_subj = false, dupe = false;
    char *ss, subject[255];

    sprintf(_temp_buffer, "TOP %u 40", msg_num);
    sock_puts(sock, _temp_buffer);
    sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
    int nPacketState = STATE_ERROR;
    if (*_temp_buffer != '+') 
    {
        POP_Err_Cond = POP_NOT_MSG;
        log_it( true, "\n \xFE Error : No message #%u.", msg_num);
        return nPacketState;
    }

    fdl = false;
    net_pkt[0] = '\0';
    int nEmptyCount = 0;
    for( ;; )
    {
        sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
        if ( *_temp_buffer == '.' && _temp_buffer[1] == '\0' )
        {
            break;
        }
        if ( *_temp_buffer == '\0' )
        {
            ++nEmptyCount;
            if ( nEmptyCount > 5 )
            {
                break;
            }
        }
        _temp_buffer[_TEMP_BUFFER_LEN-1]=0;
#ifdef MEGA_DEBUG_LOG_INFO
        output("\n DEBUG: ");
        puts( _temp_buffer );
#endif // MEGA_DEBUG_LOG_INFO
        if (usernum == 0) 
        {
            if (_strnicmp(_temp_buffer, "NET: ", 5) == 0)
            {
                strcpy(net_pkt, &_temp_buffer[5]);
            }
            if ( _strnicmp(_temp_buffer, "begin 6", 7) == 0 &&
                stristr(_temp_buffer, "WINMAIL") == NULL ) 
            {
                if (nPacketState != STATE_BAD)
                {
                    nPacketState = STATE_UUENCODE;
                }
                if ( stristr(_temp_buffer, ".ZIP") != NULL ||
                    stristr(_temp_buffer, ".ARJ") != NULL ||
                    stristr(_temp_buffer, ".LZH") != NULL )
                {
                    nPacketState = STATE_ARCHIVE;
                }
                if ( stristr(_temp_buffer, ".GIF") != NULL ||
                    stristr(_temp_buffer, ".JPG") != NULL )
                {
                    nPacketState = STATE_IMAGE;
                }
                if ( nPacketState == STATE_ARCHIVE || nPacketState == STATE_IMAGE || fdl) 
                {
                    ss = strtok(_temp_buffer, "6");
                    if (ss) 
                    {
                        ss = strtok(NULL, " ");
                        if (ss)
                        {
                            ss = strtok(NULL, "\r\n");
                        }
                    }
                    if (ss) 
                    {
                        strcpy(fdlfn, ss);
                        trimstr1(fdlfn);
                    }
                }
            }
            if (_strnicmp(_temp_buffer, "FDL Type:", 9) == 0)
            {
                fdl = true;
            }
        }
        if ( _strnicmp(_temp_buffer, "from:", 5) == 0 && !found_from )
        {
            if ( ( stristr(_temp_buffer, "mailer-daemon") != NULL ||
                stristr(_temp_buffer, "mail delivery") != NULL ||
                stristr(_temp_buffer, "mdaemon") != NULL       ||
                stristr(_temp_buffer, "administrator") != NULL ||
                stristr(_temp_buffer, from_user) != NULL ) 
                && usernum == 0 )
            {
                nPacketState = STATE_BAD;
            }
            else 
            {
                if (_temp_buffer[6] != 0) 
                {
                    strncpy(pktowner, &_temp_buffer[6], 35);
                    trimstr1(pktowner);
                    pktowner[25] = '\0';
                } 
                else
                {
                    strcpy(pktowner, "Unknown");
                }
            }
            found_from = true;
        }
        if (_strnicmp(_temp_buffer, "subject:", 8) == 0 && !found_subj)
        {
            if (_temp_buffer[9] != 0)
            {
                strncpy(subject, &_temp_buffer[9], 60);
            }
            else
            {
                strcpy(subject, "Unknown");
            }
            found_subj = true;
        }
        if (usernum == 0) 
        {
            if ((_strnicmp(_temp_buffer, "Message-ID:", 11) == 0) && !found_subj )
            {
                if (_temp_buffer[11] != 0) 
                {
                    strncpy(id, &_temp_buffer[11], 80);
                    id[80] = '\0';
                    if (!check_messageid(false, id))
                    {
                        dupe = true;
                    }
                }
            }
        }
    }
    if (found_from && found_subj) 
    {
        if (nPacketState == STATE_UNKNOWN) 
        {
            if ( checkspam( pktowner ) || checkspam( subject ) )
            {
                nPacketState = STATE_SPAM;
            }
            if ( checkfido( subject ) )
            {
                nPacketState = STATE_FIDONET;
            }
        }
    }
    if (found_subj) 
    {
        if ((_strnicmp(subject, "subscribe", 9) == 0) ||
            (_strnicmp(subject, "unsubscribe", 11) == 0))
        {
            nPacketState = STATE_SUBSCRIBE;
        }
    }
    if (dupe)
    {
		nPacketState = STATE_DUPLICATE;
    }
    SOCK_READ_ERR(POP,NOOP);
    
    // try to fix the extra '.' at the end problem.
    Sleep( 100 );
    sock_flushbuffer( sock );

    return nPacketState;
}

int pop_getf(SOCKET sock, char *pszFileName, unsigned int msg_num, int usernum)
{
	int ctld, length;
	
	unsigned long size;
	if (!pop_length(sock, msg_num, &size))
	{
		log_it( true, "\n \xFE Error : Unable to get size of message #%u", msg_num);
		return STATE_ERROR_RETREIVE;
	}
	sprintf(_temp_buffer, "RETR %u", msg_num);
	sock_puts(sock, _temp_buffer);
	sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
	if (*_temp_buffer != '+') 
	{
		POP_Err_Cond = POP_NOT_MSG;
		log_it( true, "\n \xFE Error : No message #%u", msg_num);
		return STATE_ERROR_RETREIVE;
	}
	long nbytes = 0L, rbytes = 1024L;
	output(" : ");
	int pos = WhereX();
	FILE *fp = fsh_open(pszFileName, "w");
	if ( fp == NULL )
	{
		log_it( true, "\n \xFE Unable to create %s... aborting!", pszFileName);
		return STATE_ERROR_RETREIVE;
	}
	if (usernum > 0)
	{
		fprintf(fp, "\0040RX-WWIV-User: #%d\n", usernum);
	}
	else if (usernum == -1)
	{
		fprintf(fp, "\0040RX-WWIV-List: *%s\n", listaddr);
	}
	ctld = 1;
	for ( ;; ) 
	{
		length = sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
		if (ctld == 1 && length == 0)
		{
			ctld = 0;
		}
		if (_strnicmp(_temp_buffer, "begin ", 6) == 0 &&
			stristr(_temp_buffer, "WINMAIL") != NULL)
		{
			ctld = 2;
		}
		if (ctld == 2 && _strnicmp(_temp_buffer, "end", 3) == 0)
		{
			ctld = 0;
		}
		if (_temp_buffer[0] == '.' && _temp_buffer[1] == 0)
		{
			break;
		}
		if (EOF == (nbytes += fprintf(fp, "%s%s\n", ctld ? "\0040R" : "", _temp_buffer))) 
		{
			if (fp != NULL)
			{
				fclose(fp);
			}
			return STATE_ERROR_RETREIVE;
		}
		if (nbytes > rbytes) 
		{
			go_back(WhereX(), pos);
			output("%ld/%ld", nbytes, size);
			output(".");
			rbytes += 512L;
		}
	}
	if (fp != NULL)
	{
		fclose(fp);
	}
	go_back(WhereX(), pos);
	output("%lu bytes.", size);
	SOCK_READ_ERR(POP,NOOP);
	return STATE_SUCCESS;
}

int pop_delete(SOCKET sock, unsigned int msg_num)
{
#ifndef __NO_POP_DELETE__
    sprintf(_temp_buffer, "DELE %u", msg_num);
    sock_puts(sock, _temp_buffer);
    sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
    log_it(DEBUG, "\n - POP> %s", _temp_buffer);
    if (*_temp_buffer != '+') 
    {
        POP_Err_Cond = POP_NOT_MSG;
        log_it( true, "\n \xFE Error : No message #%u", msg_num);
        return STATE_ERROR_DELETE;
    }
    SOCK_READ_ERR(POP,NOOP);
#endif // __NO_POP_DELETE__
	return STATE_SUCCESS;
}


bool pop_shutdown(SOCKET sock)
{
    if ( sock != INVALID_SOCKET ) 
    {
        sock_puts(sock, "QUIT");
        sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));
        if (*_temp_buffer != '+') 
        {
            POP_Err_Cond = POP_UNKNOWN;
            log_it( true, "\n \xFE Error : Unable to update mailbox.");
            closesocket(sock);
            return false;
        } 
        log_it(DEBUG, "\n \xFE Closed and updated mailbox.");
        closesocket(sock);
        return true;
    }
    closesocket(sock);
    return false;
}


int pop_get_nextf(SOCKET sock, char *pszFileName, int msgnum, int usernum)
{
    if (!pop_getf(sock, pszFileName, msgnum, usernum)) 
    {
        _unlink(pszFileName);
        return STATE_ERROR_RETREIVE;
    }
    return pop_delete(sock, msgnum);
}


int find_acct(char *username, char *hostname, char *password)
{
    char szFileName[_MAX_PATH], s[121];
    FILE *fp;

    int num = 0;
    sprintf(szFileName, "%sACCT.INI", net_data);
    if ((fp = fsh_open(szFileName, "rt")) == NULL)
    {
        return 0;
    }

    while (fgets(s, sizeof(s)-1, fp) && num == 0)
    {
        if (_strnicmp(s, "ACCT", 4) == 0) 
        {
            if ( strstr(s, username) != 0 && 
                strstr(s, hostname) != 0 &&
                strstr(s, password) != 0 ) 
            {
                char *ss = strtok(s, "=");
                if (ss)
                {
                    trimstr1(s);
                }
                if (s[4] == '-') 
                {
                    num = -1;
                    strcpy(listaddr, &(s[5]));
                    log_it(DEBUG, "\n \xFE Checking mailbox %s on %s for list %s.", username,
                        hostname, listaddr);
                } 
                else 
                {
                    num = atoi(&(s[4]));
                    log_it(DEBUG, "\n \xFE Checking mailbox %s on %s for user #%d.", username,
                        hostname, num);
                }
            }
        }
    }
    if (fp != NULL)
    {
        fclose(fp);
    }
    return num;
}


int count_accts(int build)
{
    FILE *fp;
    char s[101], szFileName[_MAX_PATH];
    int accts = 0;

    sprintf(szFileName, "%sACCT.INI", net_data);
    if ((fp = fsh_open(szFileName, "rt")) == NULL)
    {
        return 0;
    }

    while (fgets(s, sizeof(s)-1, fp)) 
    {
        if (_strnicmp(s, "ACCT", 4) == 0) 
        {
            if (build) 
            {
                char *ss = strtok(s, "=");
                if (ss) 
                {
                    ss = strtok(NULL, "@");
                    trimstr1(ss);
                    if (ss) 
                    {
                        strcpy(acct[accts].popname, ss);
                        ss = strtok(NULL, " ");
                        trimstr1(ss);
                        if (ss) 
                        {
                            strcpy(acct[accts].pophost, ss);
                            ss = strtok(NULL, " \r\n");
                            trimstr1(ss);
                            if (ss)
                            {
                                strcpy(acct[accts].poppass, ss);
                            }
                        }
                    }
                    log_it(DEBUG, "\n - Account : %s - %s - %s", acct[accts].pophost,
                        acct[accts].popname, acct[accts].poppass);
                }
            }
            ++accts;
        }
    }
    if (fp != NULL)
    {
        fclose(fp);
    }
    return accts;
}


bool parse_net_ini()
{
    char s[_MAX_PATH], line[121], *ss;
    FILE *fp;

    sprintf(s, "%sNET.INI", maindir);
    if ((fp = fsh_open(s, "rt")) == NULL) 
    {
        output("\n \xFE Unable to open %s.", s);
        return false;
    }
    *POPHOST = *PROXY = *POPNAME = *POPPASS = *DOMAIN = *NODEPASS = 0;
	POPPORT = DEFAULT_POP_PORT;
	SMTPPORT = DEFAULT_SMTP_PORT;
    while (fgets(line, sizeof(s)-1, fp)) 
    {
        ss = NULL;
        stripspace(line);
        if ((line[0] != ';') && (line[0] != 0) && (line[0] != '\n')) 
        {
            ss = strtok(line, "=");
            if (ss) 
            {
                ss = strtok(NULL, "\r\n");
                trimstr1(ss);
                if (_strnicmp(line, "POPHOST", 7) == 0) 
                {
                    if (ss) 
                    {
                        strcpy(POPHOST, ss);
                        continue;
                    }
                }
				if (_strnicmp(line, "POPPORT", 7) == 0) 
				{
					if (atoi(ss))
					{
						POPPORT = atoi(ss);
					}
					continue;
				}
				if (_strnicmp(line, "SMTPPORT", 8) == 0) 
				{
					if (atoi(ss))
					{
						SMTPPORT = atoi(ss);
						DEBUG_LOG_IT_1("\n [DEBUG] SMTPPORT=%d", SMTPPORT);
					}
					continue;
				}
                if (_strnicmp(line, "PROXY", 5) == 0) 
                {
                    if (ss) 
                    {
                        strcpy(PROXY, ss);
                        continue;
                    }
                }
                if (_strnicmp(line, "POPNAME", 7) == 0) 
                {
                    if (ss) 
                    {
                        strcpy(POPNAME, ss);
                        continue;
                    }
                }
                if (_strnicmp(line, "POPPASS", 7) == 0) 
                {
                    if (ss) 
                    {
                        strcpy(POPPASS, ss);
                        continue;
                    }
                }
                if (_strnicmp(line, "DOMAIN", 6) == 0) 
                {
                    if (ss) 
                    {
                        strcpy(DOMAIN, ss);
                        continue;
                    }
                }
                if (_strnicmp(line, "NODEPASS", 8) == 0) 
                {
                    if (ss) 
                    {
                        strcpy(NODEPASS, ss);
                        continue;
                    }
                }
            }
        }
    }
    if (fp != NULL)
    {
        fclose(fp);
    }
    if ( !*POPHOST || !*POPNAME || !*POPPASS || !*DOMAIN )
    {
        return false;
    }
    return true;
}


int move_bad(char *path, char *pszFileName, int why)
{
    char src[_MAX_PATH], dest[_MAX_PATH];

    log_it( true, "\n \xFE %s failed - SMTP error condition %d", pszFileName, why);
    sprintf(src, "%s%s", path, pszFileName);
    sprintf(dest, "%sFAILED\\%s", path, pszFileName);
    trimstr1(src);
    trimstr1(dest);
    fprintf(stderr, "\n! \"%s\" for src\n \"%s\" for dest\n", src, dest);
    return ( copyfile( src, dest, true ) );
}


int SendMail(int argc, char *argv[])
{
	if (argc < 5) 
	{
		output("\n \xFE %s", version);
		log_it( true, "\n \xFE Invalid arguments for %s\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (argc >= 6)
	{
		DEBUG = atoi(argv[5]) ? true : false;
	}
	// %%HACK: Rushfan to keep debug mode enabled.
	if (!DEBUG) DEBUG = true;
	bool bSkip = ( argc == 7 && atoi(argv[6]) == 1 ) ? true : false;

	char szMessageQueueDirectory[_MAX_PATH];
	strcpy(szMessageQueueDirectory, argv[4]);
	strcpy(net_data, argv[4]);
	LAST(net_data) = '\0';
	while ( LAST(net_data) != '\\' )
	{
		LAST(net_data) = '\0';
	}
	output("\n");
	DEBUG_LOG_IT_1( "\n [DEBUG] net_data = [%s]", net_data );
	DEBUG_LOG_IT_2( "\n [DEBUG] Before smtp_start [%s] [%s]", argv[2], argv[3] );
	SOCKET smtp_sock;
	if ((smtp_sock = smtp_start(argv[2], argv[3])) != NULL) 
	{
		long total_bytes = 0, current_bytes = 0, total_files = 0, current_files = 0, nTimesFailed = 0;
		unsigned int count = 0;
		bool skipit = false, firstrun = true;
		aborted = false;
	
		char szFileName[_MAX_PATH];
		struct _finddata_t ff;

		sprintf(szFileName, "%s*.*", szMessageQueueDirectory);
		DEBUG_LOG_IT_1( "\n [DEBUG] Looking in [%s]", szFileName );

		long hFind = _findfirst( szFileName, &ff ); //, FA_ARCH);
		int nFindNext = ( hFind != -1 ) ? 0 : -1;
		while (nFindNext == 0) 
		{
			if ( ff.attrib & _A_ARCH )
			{
				total_bytes += ff.size;
				++total_files;
			}
			nFindNext = _findnext( hFind, &ff );
		}
		_findclose( hFind );
		hFind = _findfirst(szFileName, &ff ); // , FA_ARCH);
		nFindNext = ( hFind != -1 ) ? 0 : -1;
		time_t ltime;
		_tzset();
		time( &ltime );
		struct tm *today = localtime( &ltime );
		int jdater = jdate(today->tm_year, today->tm_mday, today->tm_wday);
		while ( count < 3 && nFindNext == 0 && nTimesFailed < 5 && !aborted )
		{
			DEBUG_LOG_IT_1( "\n [DEBUG] Thinking about file [%s]", ff.name );
			bool bDoThisFile = false;
			if ( ff.attrib & _A_ARCH ) bDoThisFile=true;
			if ( ff.attrib & _A_SUBDIR) bDoThisFile=false;
			if ( bDoThisFile )
			{
				DEBUG_LOG_IT_1( "\n [DEBUG] Doing file [%s]", ff.name );
				if (count > 1) 
				{
					DEBUG = true;
					output("\n \xFE SMTP pass %d...\n", count);
				}
				sprintf(szFileName, "%s%s", szMessageQueueDirectory, ff.name);
				FILE *fp = NULL;
				if ((fp = fsh_open(szFileName, "r")) != NULL) 
				{
					SMTP_Err_Cond = SMTP_OK;
					bool ok = find_listname(fp);
					if (DEBUG) 
					{
						output("\n");
						if (!ok)
						{
							output("\r \xFE SND : %-12s : %-18.18s : [Space] aborts : ", ff.name, argv[2]);
						}
						else
						{
							output("\r \xFE SND : %-12s : %-18.18s : [Space] aborts : ", LISTNAME, argv[2]);
						}
					}
					ok = true;
					if (!smtp_send_MAIL_FROM_line(smtp_sock, fp))
					{
						DEBUG_LOG_IT( "\n [DEBUG] Bad smtp_send_MAIL_FROM_line");
						ok = false;
					}
					if (!smtp_send_RCPT_TO_line(smtp_sock, fp))
					{
						DEBUG_LOG_IT( "\n [DEBUG] Bad smtp_send_RCPT_TO_line");
						ok = false;
					}
					aborted = false;
					if (ok) 
					{
						++current_files;
						DEBUG_LOG_IT( "\n [DEBUG] About to call smtp_sendf");
						int result = smtp_sendf(smtp_sock, fp, current_bytes, total_bytes, current_files, total_files, bSkip);
						if ( !result || aborted )
						{
							if (fp != NULL)
							{
								fclose(fp);
							}
							if ( SMTP_Err_Cond == SMTP_FULL    || 
								SMTP_Err_Cond == SMTP_FAILED  ||
								SMTP_Err_Cond == SMTP_ACCESS  || 
								SMTP_Err_Cond == SMTP_BAD_NAM ||
								SMTP_Err_Cond == SMTP_YOU_FWD )
							{
								if (move_bad(szMessageQueueDirectory, ff.name, SMTP_Err_Cond))
								{
									_unlink( szFileName );
								}
							}
							++nTimesFailed;
						} 
						else 
						{
							if (fp != NULL)
							{
								fclose(fp);
							}
							if (!skipit)
							{
								_unlink( szFileName );
							}
							current_bytes += ff.size;
						}
					} 
					else if (fp != NULL)
					{
						fclose(fp);
					}
				} 
				else
				{
					log_it( true, "\n ! Unable to open %s.", szFileName );
				}
				nFindNext = _findnext( hFind, &ff );

				if ( nFindNext != 0 && firstrun ) 
				{
					sprintf( szFileName, "%s*.*", szMessageQueueDirectory );
					_findclose( hFind );
					hFind = -1;
					hFind = _findfirst( szFileName, &ff ); // , FA_ARCH);
					nFindNext = ( hFind != -1 ) ? 0 : -1;
					if (nFindNext == 0 && ff.attrib & _A_ARCH )
					{
						++count;
					}
				}
				if (nFindNext != 0) 
				{
					if (firstrun) 
					{
						strcat(szMessageQueueDirectory, "DIGEST\\");
						firstrun = false;
					}
					sprintf( szFileName, "%s*.*", szMessageQueueDirectory );
					hFind = _findfirst( szFileName, &ff );	// FA_ARCH);
					nFindNext = ( hFind != -1 ) ? 0 : -1;
					skipit = true;
					while ( nFindNext == 0 && skipit )
					{
						if ( ff.attrib & _A_ARCH )
						{
							char szFileExtension[_MAX_EXT];
							_splitpath(ff.name, NULL, NULL, NULL, szFileExtension);
							int jdatec = atoi(&(szFileExtension[1]));
							if (jdatec < jdater) 
							{
								skipit = false;
								break;
							} 
							else 
							{
								skipit = true;
								log_it( false, "\n \xFE Digest %s not ready.", ff.name);
							}
						}
						nFindNext = _findnext(hFind, &ff);
					}
				}
			}
			else
			{
				nFindNext = _findnext( hFind, &ff );
			}
		}
		if ( hFind != -1 )
		{
			_findclose( hFind );
		}
		if (nTimesFailed >= 5)
		{
			log_it( true, "\n \xFE Too many SMTP failures.  Try again later.");
		}
		smtp_shutdown(smtp_sock);
	} 
	else
	{
		log_it( true, "\n \xFE SMTP connection failed.");
	}
	_fcloseall();
	return EXIT_SUCCESS;
}

int ReceiveMail(int argc, char *argv[])
{
	char szFileName[181], s[21], s1[21];
	char nodepass[40], nodename[20], host[60];
	char pophost[60], poppass[20], popname[40];
	int i, usernum, num_accts, accts;
	bool once, checknode;
	SOCKET pop_sock;

	strcpy(szFileName, argv[0]);
	while (LAST(szFileName) != '\\')
	{
		LAST(szFileName) = '\0';
	}
	strcpy(maindir, szFileName);

	bool wingate = false;
	strcpy(pophost, POPHOST);
	strcpy(popname, POPNAME);
	strcpy(poppass, POPPASS);
	if (argc < 6) 
	{
		output("\n \xFE %s", version);
		output("\n \xFE Invalid arguments for %s\n", argv[0]);
		return EXIT_FAILURE;
	}
	sprintf(from_user, "%s@%s", popname, pophost);
	DEBUG = atoi(argv[4]) ? true : false;
	ALLMAIL = (atoi(argv[3]) > 0 );
	strcpy(net_data, argv[2]);
	LAST(net_data) = '\0';
	while (LAST(net_data) != '\\')
	{
		LAST(net_data) = '\0';
	}
	POP_Err_Cond = POP_OK;
	num_accts = accts = usernum = 0;
	checknode = once = false;
	*nodepass = *nodename = 0;
	if (*NODEPASS) 
	{
		strcpy(nodepass, NODEPASS);
		strcpy(nodename, argv[5]);
		checknode = once = true;
	}
	while ( num_accts >= 0 || once ) 
	{
		if (*PROXY) 
		{
			wingate = true;
			strcpy(host, PROXY);
		} 
		else
		{
			strcpy(host, pophost);
		}
		log_it( true, "\n \xFE Checking %s... ", pophost);
		if ((pop_sock = pop_init(host)) != NULL) 
		{
			if (pop_login(pop_sock, popname, poppass, pophost, wingate)) 
			{
				unsigned long size;
				unsigned int count;
				if (pop_status(pop_sock, &count, &size)) 
				{
					output("%s has %u message%s (%luK).", popname, count,
						count == 1 ? "" : "s", ((size + 1023) / 1024));
					int i1 = 1;
					pktowner[0] = 0;
					while ( i1 <= ( int ) count ) 
					{
						int nPacketState = pop_top(pop_sock, i1, usernum);
						if ( nPacketState != STATE_UUENCODE )
						{
							log_it( true, "** Pop top returned %d", nPacketState );
						}
						switch (nPacketState) 
						{
						case STATE_UNKNOWN:
							if (!ALLMAIL && !fdl)
							{
								log_it( true, "\n \xFE Non-network message %d left on server.", i1);
							}
							else 
							{
								i = 0;
								sprintf(szFileName, "%sUNK-%03d.MSG", argv[2], i);
								while (exist(szFileName))
								{
									sprintf(szFileName, "%sUNK-%03d.MSG", argv[2], ++i);
								}
								_splitpath(szFileName, NULL, NULL, s, s1);
								log_it( true, pktowner[0] == 0 ?
									"non-network packet" : pktowner, s, s1);
								int result = pop_get_nextf(pop_sock, szFileName, i1, usernum);
								switch (result) 
								{
								case STATE_ERROR_RETREIVE:
									log_it( true, "\n \xFE Unable to retrieve message %d.", i1);
									_fcloseall();
									return EXIT_FAILURE;
								case STATE_SUCCESS:
									if (usernum == 0)
									{
										check_messageid(true, id);
									}
									break;
								case STATE_ERROR_DELETE:
									log_it( true, "\n \xFE Unable to delete message %d from host!", i1);
									return EXIT_FAILURE;
								}
							}
							break;
						case STATE_ERROR:
							log_it( true, "\n \xFE Error accessing message %d", i1);
							_fcloseall();
							return EXIT_FAILURE;
						case STATE_UUENCODE:
							{
								i = 0;
								sprintf(szFileName, "%sPKT-%03d.UUE", argv[2], i);
								while (exist(szFileName))
								{
									sprintf(szFileName, "%sPKT-%03d.UUE", argv[2], ++i);
								}
								_splitpath(szFileName, NULL, NULL, s, s1);
								log_it( true, "\n \xFE %s : %3.3d : %-20.20s : %s%s",
									*net_pkt ? net_pkt : "Receive", i1, pktowner, s, s1);
								int result = pop_get_nextf(pop_sock, szFileName, i1, usernum);
								switch (result) 
								{
								case STATE_ERROR_RETREIVE:
									log_it( true, "\n \xFE Unable to retrieve message %d.", i1);
									_fcloseall();
									return EXIT_FAILURE;
								case STATE_SUCCESS:
									if (usernum == 0)
									{
										check_messageid(true, id);
									}
									break;
								case STATE_ERROR_DELETE:
									log_it( true, "\n \xFE Unable to delete message %d on host!", i1);
									return EXIT_FAILURE;
								}
							}
							break;
						case STATE_ARCHIVE:
							if ( !ALLMAIL && !fdl )
							{
								log_it( true, "\n \xFE Non-network message %d left on server.", i1);
							}
							else 
							{
								i = 0;
								sprintf(szFileName, "%sARC-%03d.UUE", argv[2], i);
								while (exist(szFileName))
								{
									sprintf(szFileName, "%sARC-%03d.UUE", argv[2], ++i);
								}
								_splitpath(szFileName, NULL, NULL, s, s1);
								if (*fdlfn)
								{
									log_it( true, "archived file", fdlfn);
								}
								else
								{
									log_it( true, "archived file", s, s1);
								}
								int result = pop_get_nextf(pop_sock, szFileName, i1, usernum);
								switch (result) 
								{
								case STATE_ERROR_RETREIVE:
									log_it( true, "\n \xFE Unable to retrieve message %d.", i1);
									_fcloseall();
									return EXIT_FAILURE;
								case STATE_SUCCESS:
									if (usernum == 0)
									{
										check_messageid(true, id);
									}
									break;
								case STATE_ERROR_DELETE:
									log_it( true, "\n \xFE Unable to delete message %d on host!", i1);
									return EXIT_FAILURE;
								}
							}
							break;
						case STATE_IMAGE:
							if ( !ALLMAIL && !fdl )
							{
								log_it( true, "\n \xFE Non-network message %d left on server.", i1);
							}
							else 
							{
								i = 0;
								sprintf(szFileName, "%sGIF-%03d.UUE", argv[2], i);
								while ( exist( szFileName ) )
								{
									sprintf(szFileName, "%sGIF-%03d.UUE", argv[2], ++i);
								}
								_splitpath(szFileName, NULL, NULL, s, s1);
								log_it( true, "graphic/image file", s, s1);
								int result = pop_get_nextf(pop_sock, szFileName, i1, usernum);
								switch (result) 
								{
								case STATE_ERROR_RETREIVE:
									log_it( true, "\n \xFE Unable to retrieve message %d.", i1);
									_fcloseall();
									return EXIT_FAILURE;
								case STATE_SUCCESS:
									if (usernum == 0)
									{
										check_messageid(true, id);
									}
									break;
								case STATE_ERROR_DELETE:
									log_it( true, "\n \xFE Unable to delete message %d from host!", i1);
									return EXIT_FAILURE;
								}
							}
							break;
						case STATE_BAD:
							{
								i = 0;
								sprintf(szFileName, "%sBAD-%03d.UUE", argv[2], i);
								while (exist(szFileName))
								{
									sprintf(szFileName, "%sBAD-%03d.UUE", argv[2], ++i);
								}
								_splitpath(szFileName, NULL, NULL, s, s1);
								log_it( true, "mailer-daemon/bounced", s, s1);
								int result = pop_get_nextf(pop_sock, szFileName, i1, usernum);
								switch (result) 
								{
								case STATE_ERROR_RETREIVE:
									log_it( true, "\n \xFE Unable to retrieve message %d.", i1);
									_fcloseall();
									return EXIT_FAILURE;
								case STATE_SUCCESS:
									if (usernum == 0)
									{
										check_messageid(true, id);
									}
									break;
								case STATE_ERROR_DELETE:
									log_it( true, "\n \xFE Unable to delete message %d from host!", i1);
									return EXIT_FAILURE;
								}
							}
							break;
						case STATE_SPAM:
							if ( !ALLMAIL && !fdl )
							{
								log_it( true, "\n \xFE Non-network message %d left on server.", i1);
							}
							else 
							{
								i = 0;
								sprintf(szFileName, "%sSPM-%03d.MSG", argv[2], i);
								while (exist(szFileName))
								{
									sprintf(szFileName, "%sSPM-%03d.MSG", argv[2], ++i);
								}
								_splitpath(szFileName, NULL, NULL, s, s1);
								log_it( true, "matched NOSPAM.TXT", s, s1);
								int result = pop_get_nextf(pop_sock, szFileName, i1, usernum);
								switch (result) 
								{
								case STATE_ERROR_RETREIVE:
									log_it( true, "\n \xFE Unable to retrieve message %d.", i1);
									_fcloseall();
									return EXIT_FAILURE;
								case STATE_SUCCESS:
									if (usernum == 0)
									{
										check_messageid(true, id);
									}
									break;
								case STATE_ERROR_DELETE:
									log_it( true, "\n \xFE Unable to delete message %d from host!", i1);
									return EXIT_FAILURE;
								}
							}
							break;
						case STATE_SUBSCRIBE:
							{
								if ( !ALLMAIL && !fdl )
								{
									log_it( true, "\n \xFE Non-network message %d left on server.", i1);
								}
								else 
								{
									i = 0;
									sprintf(szFileName, "%sSUB-%03d.MSG", argv[2], i);
									while (exist(szFileName))
									{
										sprintf(szFileName, "%sSUB-%03d.MSG", argv[2], ++i);
									}
									_splitpath(szFileName, NULL, NULL, s, s1);
									log_it( true, "subscribe request", s, s1);
									int result = pop_get_nextf(pop_sock, szFileName, i1, usernum);
									switch (result) 
									{
									case STATE_ERROR_RETREIVE:
										log_it( true, "\n \xFE Unable to retrieve message %d.", i1);
										_fcloseall();
										return EXIT_FAILURE;
									case STATE_SUCCESS:
										if (usernum == 0)
										{
											check_messageid(true, id);
										}
										break;
									case STATE_ERROR_DELETE:
										log_it( true, "\n \xFE Unable to delete message %d from host!", i1);
										return EXIT_FAILURE;
									}
								}
							}
							break;
						case STATE_DUPLICATE:
							{
								if ( !ALLMAIL && !fdl )
								{
									log_it( true, "\n \xFE Duplicate message %d left on server.", i1);
								}
								else 
								{
									int result = pop_delete(pop_sock, i1);
									if (result == STATE_ERROR_DELETE) {
										log_it( true, "\n \xFE Unable to delete message %d from host!", i1);
										return EXIT_FAILURE;
									}
								}
							}
							break;
						case STATE_FIDONET:
							{
								i = 0;
								sprintf(szFileName, "%sFIW-%03d.MSG", argv[2], i);
								while (exist(szFileName))
								{
									sprintf(szFileName, "%sFIW-%03d.MSG", argv[2], ++i);
								}
								_splitpath(szFileName, NULL, NULL, s, s1);
								log_it( true, pktowner[0] == 0 ?
									"non-network packet" : pktowner, s, s1);
								int result = pop_get_nextf(pop_sock, szFileName, i1, usernum);
								switch (result) 
								{
								case STATE_ERROR_RETREIVE:
									log_it( true, "\n \xFE Unable to retrieve message %d.", i1);
									_fcloseall();
									return EXIT_FAILURE;
								case STATE_SUCCESS:
									break;
								case STATE_ERROR_DELETE:
									log_it( true, "\n \xFE Unable to delete message %d from host!", i1);
									return EXIT_FAILURE;
								}
							}
							break;
						}
						i1++;
						_fcloseall();
					}
					if (compact_ids) 
					{
						log_it( true, "\n \xFE Compacting Message-ID database...");
						compact_msgid();
						compact_ids = false;
					}
				} 
				else
				{
					log_it( true, "\n \xFE Unknown POP _access error - try again later.");
				}
				pop_shutdown(pop_sock);
			} 
			else 
			{
				log_it( true, "\n \xFE Unable to log into POP server!");
				pop_shutdown(pop_sock);
			}
		} 
		else
		{
			log_it( true, "\n \xFE POP socket connect failed.");
		}
		if ( checknode && once ) 
		{
			strcpy(pophost, "mail.filenet.wwiv.net");
			strcpy(popname, nodename);
			strcpy(poppass, nodepass);
			ALLMAIL = true;
			once = false;
		} 
		else 
		{
			if (!accts) 
			{
				num_accts = count_accts(0);
				log_it(DEBUG, "\n - Found %d extra account%s.", num_accts, num_accts == 1 ? "" : "s");
				if (num_accts) 
				{
					acct = (ACCT *) malloc(sizeof(ACCT) * num_accts);
					if (acct != NULL) 
					{
						num_accts = count_accts(1);
					} 
					else 
					{
						log_it(DEBUG, "\n ! Insufficient memory for extra accounts.");
						num_accts = 0;
					}
					accts = 1;
				}
			}
			if (num_accts) 
			{
				strcpy(pophost, acct[num_accts - 1].pophost);
				strcpy(popname, acct[num_accts - 1].popname);
				strcpy(poppass, acct[num_accts - 1].poppass);
				ALLMAIL = true;
				usernum = find_acct(popname, pophost, poppass);
				--num_accts;
			} 
			else
			{
				num_accts = -1;
			}
		}
	}
	if (acct != NULL) 
	{
		free(acct);
		acct = NULL;
	}
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	DEBUG_LOG_IT_1( "\n [DEBUG] CommandLine=[%s]", GetCommandLine() );

    if (!parse_net_ini()) 
    {
        output("\n ! Missing critical NET.INI settings!");
        WSACleanup();
        return EXIT_FAILURE;
    }

	if ( !InitializeWinsock() )
    {
        log_it( true, "\n Unable to initialize WinSock, Exiting...\n" );
        WSACleanup();
        return EXIT_FAILURE;
    }

	int nResultCode = EXIT_SUCCESS;
    if (argc > 1 && _strnicmp(argv[1], "-send", strlen(argv[1])) == 0) 
	{
		nResultCode = SendMail(argc, argv);
	}
    else if (argc > 1 && _strnicmp(argv[1], "-r", strlen(argv[1])) == 0) 
    {
		nResultCode = ReceiveMail(argc, argv);
    }
    WSACleanup();
    return nResultCode;
}




/*
test command lines
-send mail.filenet.wwiv.net home.robsite.org D:\BBS\FILENET\MQUEUE\ 0 1
-r D:\BBS\FILENET\INBOUND\ 1 0 n561
*/
