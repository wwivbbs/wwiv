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
#include "network.h"

void set_net_num(int nNetNumber)
{
	char szFileName[_MAX_PATH];
	
	sprintf(szFileName, "%sNETWORKS.DAT", syscfg.datadir);
	int f = sh_open1(szFileName, O_RDONLY | O_BINARY);
	if (f < 0)
	{
		return;
	}
	_lseek(f, ((long) nNetNumber) * sizeof(net_networks_rec), SEEK_SET);
	sh_read(f, &netcfg, sizeof(net_networks_rec));
	_close(f);
	g_nNetworkSystemNumber = netcfg.sysnum;
	strcpy(net_name, netcfg.name);
	strcpy(net_data, make_abs_path(netcfg.dir, maindir));
	if (LAST(net_data) != '\\')
	{
		strcat(net_data, "\\");
	}
}


void process_mail()
{
	char szCommand[_MAX_PATH];
	
	output("\n \xFE Network clean-up... please wait... ");
	sprintf(szCommand, "%s\\FLINK.EXE", maindir);
	if (exist(szCommand))
	{
		do_spawn(szCommand);
	}
	sprintf(szCommand, "%s\\LINKER.EXE", maindir);
	if (exist(szCommand))
	{
		do_spawn(szCommand);
	}
	output("done.");
}


bool valid_system( int nSystemNumber, int nNumSysList )
{
    for ( int i = 0; i < nNumSysList; i++ )
    {
        if ( netsys[i].sysnum == nSystemNumber )
        {
            return (netsys[i].numhops == 1) ? true : false;
        }
    }
    return false;
}


bool create_contact_ppp()
{
	char *ss, szFileName[_MAX_PATH], s[81];
	net_address_rec curaddr;
	FILE *fp, *fc;
	
	sprintf(szFileName, "%sCONTACT.PPP", net_data);
	if ((fc = fsh_open(szFileName, "wb+")) == NULL) 
	{
		output("\n ! %s %s!\n", strings[0], szFileName);
		return false;
	}
	int curfile = 1;
	g_nNumAddresses = 0;
	do 
	{
		if (curfile == 1) 
		{
			sprintf(szFileName, "%sADDRESS.1", net_data);
			if (!exist(szFileName)) 
			{
				sprintf(szFileName, "%sADDRESS.NET", net_data);
				curfile = 0;
			}
		} 
		else
		{
			sprintf(szFileName, "%sADDRESS.%d", net_data, curfile);
		}
		if ((fp = fsh_open(szFileName, "rt")) == NULL)
		{
			curfile = -1;
		}
		while ((curfile >= 0) && (fgets(s, 80, fp) != NULL)) 
		{
			if (s[0] != '@')
			{
				continue;
			}
			ss = strtok(s, " \t\r");
			int node = atoi(&ss[1]);
			if (node) 
			{
				ss = strtok(NULL, " \t\r");
				trimstr1(ss);
				curaddr.sysnum = ( unsigned short ) node;
				strcpy(curaddr.address, ss);
				if (ss[0]) 
				{
					fwrite(&curaddr, sizeof(net_address_rec), 1, fc);
					++g_nNumAddresses;
				}
			}
		}
		if (fp != NULL)
		{
			fclose(fp);
		}
		if (curfile)
		{
			++curfile;
		}
	} while (curfile > 0);

	fseek(fc, 0L, SEEK_SET);
	sprintf(szFileName, "%sADDRESS.0", net_data);
	if ((fp = fsh_open(szFileName, "rt")) == NULL) 
	{
		if (fc != NULL)
		{
			fclose(fc);
		}
		return true;
	}
	while (fgets(s, 80, fp) != NULL) 
	{
		if (s[0] != '@')
		{
			continue;
		}
		ss = strtok(s, " \t\r");
		int node = atoi(&ss[1]);
		do 
		{
			ss = strtok(NULL, " \t\r");
			if ( ss != NULL )
			{
				switch (ss[0]) 
				{
				case '-':
					for (int i = 0; i < g_nNumAddresses; i++) 
					{
						fseek(fc, (long) i * sizeof(net_address_rec), SEEK_SET);
						fread(&curaddr, sizeof(net_address_rec), 1, fc);
						if (node == curaddr.sysnum) 
						{
							curaddr.sysnum = 0;
							fseek(fc, (long) i * sizeof(net_address_rec), SEEK_SET);
							fwrite(&curaddr, sizeof(net_address_rec), 1, fc);
						}
					}
					break;
				}
			}
		} while (ss);
	}
	if (fp != NULL)
	{
		fclose(fp);
	}
	if (fc != NULL)
	{
		fclose(fc);
	}
	return true;
}

void good_addr(char *address, int nSystemNumber)
{
	char szFileName[_MAX_PATH];
	net_address_rec curaddr;
	FILE *fc;
	
	address[0] = 0;
	sprintf(szFileName, "%sCONTACT.PPP", net_data);
	if ((fc = fsh_open(szFileName, "rb+")) == NULL) 
    {
		log_it(1, "\n \xFE %s %s!\n", strings[1], szFileName);
		return;
	}
	for ( int i = 0; ( i < g_nNumAddresses && !address[0] ); i++ )
	{
		fseek( fc, (long) i * sizeof(net_address_rec), SEEK_SET );
		fread( &curaddr, sizeof(net_address_rec), 1, fc );
		if ( nSystemNumber == curaddr.sysnum )
		{
			strcpy( address, curaddr.address );
		}
	}
	if (fc != NULL)
	{
		fclose(fc);
	}
}


int uu(char *pszFileName, char *pktname, char *dest)
{
	char szFileName[_MAX_PATH], szEncodeCommand[_MAX_PATH], szMessageHeader[121];
	FILE *fp;
	
	sprintf(szFileName, "%sMQUEUE\\%s.UUE", net_data, pktname);
	if ((fp = fsh_open(szFileName, "wt")) == NULL) 
	{
		log_it(1, "\n \xFE %s %s!", strings[0], szFileName);
		do_exit(EXIT_FAILURE);
	} 
	else 
	{
		if (BBSNODE == 2)
		{
			sprintf(szMessageHeader, "From: n%hd@filenet.wwiv.net\n", g_nNetworkSystemNumber);
		}
		else
		{
			sprintf(szMessageHeader, "From: %s@%s\n", POPNAME, DOMAIN);
		}
		fprintf(fp, szMessageHeader);
		sprintf(szMessageHeader, "To: %s\n", dest);
		fprintf(fp, szMessageHeader);
		fprintf(fp, "MIME-Version: 1.0\n");
		fprintf(fp, "Content-Type: text/plain; charset=us-ascii\n");
		fprintf(fp, "Content-Transfer-Encoding: x-uue\n");
		sprintf(szMessageHeader, "Message-ID: <%s-%s@%s>\n", pktname, POPNAME, DOMAIN);
		fprintf(fp, szMessageHeader);
		sprintf(szMessageHeader, "Subject: NET PACKET : %s.UUE\n\n", pktname);
		fprintf(fp, szMessageHeader);
		sprintf(szMessageHeader, "NET: %s\n\n", net_name);
		fprintf(fp, szMessageHeader);
	}

	if (fp != NULL)
	{
		fclose(fp);
	}
	sprintf(szEncodeCommand, "%s\\UU.EXE -encode %s %s", maindir, pszFileName, szFileName);

	int nRet = do_spawn(szEncodeCommand);
	if (MOREINFO)
	{
		output("\n");
	}
	return nRet;
}


int update_contact(int sy, unsigned long sb, unsigned long rb, int set_time)
{
	char s[_MAX_PATH];
	int i, i1 = 0;
	
	sprintf(s, "%sCONTACT.NET", net_data);
	int f = sh_open1(s, O_RDONLY | O_BINARY);
	if (f >= 0) 
	{
		long l = _filelength(f);
		netcfg.num_ncn = (short) (l / sizeof(net_contact_rec));
		if ((netcfg.ncn = (net_contact_rec *) malloc((netcfg.num_ncn + 2) *
			sizeof(net_contact_rec))) == NULL) 
		{
			f = sh_close(f);
			return 1;
		}
		sh_lseek(f, 0L, SEEK_SET);
		sh_read(f, (void *) netcfg.ncn, netcfg.num_ncn * sizeof(net_contact_rec));
		f = sh_close(f);
	}
	for (i = 0; i < netcfg.num_ncn; i++)
	{
		if (netcfg.ncn[i].systemnumber == sy)
		{
			i1 = i;
		}
	}
	if (set_time) 
	{
		time_t cur_time;
		time(&cur_time);
		netcfg.ncn[i1].lasttry = static_cast<unsigned long>(cur_time);
		++netcfg.ncn[i1].numcontacts;
		if (!netcfg.ncn[i1].firstcontact)
		{
			netcfg.ncn[i1].firstcontact = cur_time;
		}
		netcfg.ncn[i1].lastcontact = cur_time;
		netcfg.ncn[i1].lastcontactsent = cur_time;
	}
	netcfg.ncn[i1].bytes_sent += sb;
	if (rb)
	{
		netcfg.ncn[i1].bytes_received += rb;
	}
	netcfg.ncn[i1].bytes_waiting = 0L;
	sprintf(s, "%sS%hd.net", net_data, sy);
	if (!exist(s))
	{
		sprintf(s, "%sZ%hd.net", net_data, sy);
	}
	f = sh_open1(s, O_RDONLY | O_BINARY);
	if (f > 0) 
	{
		netcfg.ncn[i1].bytes_waiting += (long) _filelength(f);
		_close(f);
	}
	sprintf(s, "%sCONTACT.NET", net_data);
	f = sh_open1(s, O_RDWR | O_CREAT | O_BINARY);
	if (f > 0) 
	{
		sh_lseek(f, 0L, SEEK_SET);
		sh_write(f, (void *) netcfg.ncn, netcfg.num_ncn * sizeof(net_contact_rec));
		f = sh_close(f);
	}
	if (netcfg.ncn)
	{
		free(netcfg.ncn);
	}
	return 0;
}


// Return code 1 is error, 0 is success.
int update_contacts(int sy, unsigned long sb, unsigned long rb)
{
	FILE *fc = NULL;
	
	int rc = 0;
	char szFileName[_MAX_PATH];
	sprintf(szFileName, "%sCONTACT.PPP", net_data);
	if ((fc = fsh_open(szFileName, "rb+")) == NULL) 
	{
		log_it(1, "\n \xFE %s %s!\n", strings[1], szFileName);
		return 1;
	}
	for ( int i = 0; i < g_nNumAddresses; i++ ) 
	{
		net_address_rec curaddr;
		fseek(fc, (long) i * sizeof(net_address_rec), SEEK_SET);
		fread(&curaddr, sizeof(net_address_rec), 1, fc);
		if (curaddr.sysnum != 0) 
		{
			int i1;
			if (curaddr.sysnum == sy)
			{
				i1 = update_contact(curaddr.sysnum, sb, rb, 1);
			}
			else
			{
				i1 = update_contact(curaddr.sysnum, 0, 0, 1);
			}
			if (i1)
			{
				rc = 1;
			}
		}
	}
	if (fc != NULL)
	{
		fclose(fc);
	}
	return rc;
}


int uu_packets(void)
{
	char pktname[21], s[121], s1[121], s2[121], s3[121], temp[121];
	int sz, f, f1, i;
	net_address_rec curaddr;
	FILE *fc = NULL;
	
	int num_sys_list = 0;
	bool bFound = false;
	time_t basename = time(NULL);
	sprintf(s, "%sBBSDATA.NET", net_data);
	f = sh_open1(s, O_RDONLY | O_BINARY);
	if (f > 0) 
	{
		num_sys_list = (int) (_filelength(f) / sizeof(net_system_list_rec));
		if ((netsys = (net_system_list_rec *) malloc((num_sys_list + 2) * sizeof(net_system_list_rec))) == NULL) 
		{
			log_it(1, "\n \xFE Unable to allocate %d bytes of memory for BBSDATA.NET",
				(int) (num_sys_list + 2) * sizeof(net_system_list_rec));
			sh_close(f);
			return false;
		}
		sh_lseek(f, 0L, SEEK_SET);
		sh_read(f, (void *) netsys, num_sys_list * sizeof(net_system_list_rec));
		sh_close(f);
	} 
	else
	{
		return false;
	}
	
	sprintf(s1, "%sCONTACT.PPP", net_data);
	if ((fc = fsh_open(s1, "rb+")) == NULL) 
	{
		log_it(1, "\n \xFE %s %s!\n", strings[1], s1);
		free(netsys);
		return false;
	}
	for (i = 0; i < g_nNumAddresses; i++) 
	{
		fseek(fc, (long) i * sizeof(net_address_rec), SEEK_SET);
		fread(&curaddr, sizeof(net_address_rec), 1, fc);
		if ( valid_system( curaddr.sysnum, num_sys_list ) && curaddr.sysnum != 32767 ) 
		{
			sprintf(s1, "%sS%hd.NET", net_data, curaddr.sysnum);
			if (exist(s1)) 
			{
				sz = 0;
				bFound = true;
			} 
			else 
			{
				sprintf(s1, "%sZ%hd.NET", net_data, curaddr.sysnum);
				if (exist(s1)) 
				{
					sz = 1;
					bFound = true;
				} 
				else
				{
					continue;
				}
			}
			do 
			{
				sprintf(pktname, "%8.8lx", ++basename);
				sprintf(temp, "%sMQUEUE\\%s.UUE", net_data, pktname);
			} 
			while (exist(temp));
			backline();
			output("\r \xFE Encoding %s%hd.NET as %s.UUE -",
				sz ? "Z" : "S", curaddr.sysnum, _strupr(pktname));
			if ((uu(s1, pktname, curaddr.address)) != EXIT_SUCCESS) 
			{
				output("\n \xFE %s %sMQUEUE\\%s.UUE", strings[0], net_data, pktname);
				if (fc != NULL)
				{
					fclose(fc);
				}
				free(netsys);
				do_exit(EXIT_FAILURE);
			} 
			else 
			{
				unsigned long sbytes = 0;
				sprintf(s3, "%sMQUEUE\\%s.UUE", net_data, pktname);
				f1 = sh_open1(s3, O_RDONLY | O_BINARY);
				if (f1 > 0) 
				{
					sbytes = (long) _filelength(f1);
					f = sh_close(f1);
				}
				if (KEEPSENT) 
				{
					sprintf(s2, "%sSENT\\%s.SNT", net_data, pktname);
					if (!copyfile(s3, s2))
					{
						output("\n \xFE %s %s%hd.NET", strings[0], sz ? "Z" : "S", curaddr.sysnum);
					}
				}
				_unlink(s1);
				update_contact(curaddr.sysnum, sbytes, 0, 0);
			}
		}
	}
	if (fc != NULL)
	{
		fclose(fc);
	}
	free(netsys);
	return bFound;
}


int parse_net_ini( int &ini_section )
{
    char szFileName[_MAX_PATH], line[121], vanline[121], *ss;
    int cursect = 0;
    FILE *fp;

    sprintf(szFileName, "%s\\NET.INI", maindir);
    if ((fp = fsh_open(szFileName, "rt")) == NULL) 
    {
        output("\n ! Unable to read %s.", szFileName);
        return 0;
    }
    while (fgets(line, 80, fp)) 
    {
        strcpy(vanline, line);
        stripspace(line);
        ss = NULL;
        ss = strtok(line, "=");
        if (ss)
        {
            ss = strtok(NULL, "\n");
        }
        if ( line[0] == ';' || line[0] == '\n' || strlen(line) == 0 )
        {
            continue;
        }
        if ( _strnicmp(line, "[FILENET]", 9) == 0  ||
            _strnicmp(line, "[PPPNET]", 8) == 0   ||
            _strnicmp(line, "[NETWORK]", 8) == 0 )
        {
            cursect = INI_NETWORK;
            ini_section |= INI_NETWORK;
            continue;
        }
        if (_strnicmp(line, "[GENERAL]", 9) == 0) 
        {
            cursect = INI_GENERAL;
            ini_section |= INI_GENERAL;
            continue;
        }
        if (_strnicmp(line, "[NEWS]", 6) == 0) 
        {
            cursect = INI_NEWS;
            ini_section |= INI_NEWS;
            continue;
        }
        if (cursect & INI_NETWORK) 
        {
            if (_strnicmp(line, "TIMEHOST", 8) == 0) 
            {
                if (ss)
                {
                    strcpy(TIMEHOST, ss);
                }
                continue;
            }
            if (_strnicmp(line, "QOTDHOST", 8) == 0) 
            {
                if (ss)
                {
                    strcpy(QOTDHOST, ss);
                }
                continue;
            }
            if (_strnicmp(line, "QOTDFILE", 8) == 0) 
            {
                if (ss)
                {
                    strcpy(QOTDFILE, ss);
                }
                continue;
            }
            if (_strnicmp(line, "PRIMENET", 8) == 0) 
            {
                if (ss)
                {
                    strcpy(PRIMENET, ss);
                }
                continue;
            }
            if (_strnicmp(line, "SMTPHOST", 8) == 0) 
            {
                if (ss)
                {
                    strcpy(SMTPHOST, ss);
                }
                continue;
            }
            if (_strnicmp(line, "POPHOST", 7) == 0) 
            {
                if (ss)
                {
                    strcpy(POPHOST, ss);
                }
                continue;
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
				}
				continue;
			}
            if (_strnicmp(line, "PROXY", 5) == 0) 
            {
                if (ss)
                {
                    strcpy(PROXY, ss);
                }
                continue;
            }
            if (_strnicmp(line, "POPNAME", 7) == 0) 
            {
                if (ss)
                {
                    strcpy(POPNAME, ss);
                }
                continue;
            }
            if (_strnicmp(line, "POPPASS", 7) == 0) 
            {
                if (ss)
                {
                    strcpy(POPPASS, ss);
                }
                continue;
            }
        }
        if (_strnicmp(line, "NODEPASS", 8) == 0) 
        {
            if (ss) 
            {
                strcpy(NODEPASS, ss);
                BBSNODE = 2;
            }
            continue;
        }
        if (cursect & INI_GENERAL) 
        {
            if (_strnicmp(line, "KEEPSENT", 8) == 0) 
            {
                if (atoi(ss))
                {
                    KEEPSENT = atoi(ss);
                }
                continue;
            }
            if (_strnicmp(line, "PURGE", 5) == 0) 
            {
                if (toupper(ss[0]) == 'N')
                {
                    PURGE = 0;
                }
                continue;
            }
            if (_strnicmp(line, "ALLMAIL", 7) == 0) 
            {
                if (toupper(ss[0]) == 'N')
                {
                    ALLMAIL = 0;
                }
                continue;
            }
            if (_strnicmp(line, "ONECALL", 7) == 0) 
            {
                if (toupper(ss[0]) == 'Y')
                {
                    ONECALL = 1;
                }
                continue;
            }
            if (_strnicmp(line, "RASDIAL", 7) == 0) 
            {
                if (toupper(ss[0]) == 'Y')
                {
                    RASDIAL = 1;
                }
                continue;
            }
            if (_strnicmp(line, "MOREINFO", 8) == 0) 
            {
                if (toupper(ss[0]) == 'Y')
                {
                    MOREINFO = 1;
                }
                continue;
            }
            if (_strnicmp(line, "NOLISTNAMES", 11) == 0) 
            {
                if (toupper(ss[0]) == 'Y')
                {
                    NOLISTNAMES = 1;
                }
                continue;
            }
            if (_strnicmp(line, "CLEANUP", 7) == 0) 
            {
                if (toupper(ss[0]) == 'Y')
                {
                    CLEANUP = 1;
                }
                continue;
            }
            if (_strnicmp(line, "DOMAIN", 6) == 0) 
            {
                if (ss)
                {
                    strcpy(DOMAIN, ss);
                }
                continue;
            }
            if (_strnicmp(line, "FWDNAME", 7) == 0) 
            {
                if (ss)
                {
                    strcpy(FWDNAME, ss);
                }
                continue;
            }
            if (_strnicmp(line, "FWDDOM", 6) == 0) 
            {
                if (ss)
                {
                    strcpy(FWDDOM, ss);
                }
                continue;
            }
        }
        if (cursect & INI_NEWS) 
        {
            if (_strnicmp(line, "NEWSHOST", 8) == 0) 
            {
                if (!isdigit(line[8])) 
                {
                    if (ss)
                    {
                        strcpy(NEWSHOST, ss);
                    }
                } 
                else
                {
                    MULTINEWS = 1;
                }
                continue;
            }
        }
    }
    if (!FWDNAME[0])
    {
        strcpy(FWDNAME, POPNAME);
    }
    if (!FWDDOM[0])
    {
        strcpy(FWDDOM, DOMAIN);
    }
    if (fp != NULL)
    {
        fclose(fp);
    }
    return 1;
}


int read_networks(void)
{
	char s[121];
	
	sprintf( s, "%sNETWORKS.DAT", syscfg.datadir );
	int hFile = sh_open1( s, O_RDONLY | O_BINARY );
	int nMaxNetworkNumber = ( hFile > 0 ) ? ( int ) ( _filelength( hFile ) / sizeof( net_networks_rec ) ) : 0;
	sh_close(hFile);
	return nMaxNetworkNumber;
}


bool check_encode( char *pszFileName )
{
	char s[121], s1[121];
	FILE *phFile;
	
	if ( ( phFile = fsh_open(pszFileName, "r" ) ) == NULL )
	{
		return false;
	}
	
	int lines = 0;
	bool bEncoded = false;
	
	while ( fgets( s, 120, phFile ) &&  ++lines < 200 ) 
	{
		if ( s &&  *s == '' )
		{
			strcpy(s1, &s[3]);
		}
		else
		{
			strcpy( s1, s );
		}
		if ( _strnicmp( s1, "begin 6", 7 ) == 0 ) 
		{
			bEncoded = true;
			break;
		}
		if ( _strnicmp( s1, "Content-Type", 12 ) == 0 ) 
		{
			if ( stristr( s1, "text/plain" ) == 0 )
			{
				bEncoded = true;
			}
		}
		if ( _strnicmp( s1, "Content-Transfer-Encoding", 25 ) == 0 ) 
		{
			if (stristr(s1, "8bit")) 
			{
				bEncoded = true;
				break;
			}
		}
		if ( stristr( s1, "This is a multi-part message in MIME format." ) ) 
		{
			bEncoded = true;
			break;
		}
	}
	
	if ( phFile != NULL )
	{
		fclose( phFile );
	}
	
	if ( bEncoded )
	{
		log_it( MOREINFO, "\n \xFE %s appears to contain encoded data.", pszFileName );
	}
	
	return bEncoded;
}


void check_unk(void)
{
	char s[_MAX_PATH], szFileName[_MAX_PATH], szNewFileName[_MAX_PATH];
	
	sprintf( szFileName, "%sINBOUND\\UNK-0*.*", net_data );
	
	_finddata_t ff;
	long hFind = _findfirst( szFileName, &ff );
	int nFindNext = ( hFind != -1 ) ? 0 : -1;
	while ( nFindNext == 0 ) 
	{
		sprintf( szFileName, "%sINBOUND\\%s", net_data, ff.name );
		int f = sh_open1(szFileName, O_RDONLY | O_BINARY);
		long l = _filelength(f);
		f = sh_close(f);
		if (l > 32767L) 
		{
			if (!check_encode(szFileName)) 
			{
				sprintf(s, "%s\\PPPUTIL.EXE CHUNK %s %s", maindir, net_data, ff.name);
				do_spawn(s);
			} 
			else
			{
				log_it(MOREINFO, "\n ! %s contains encoded data.", ff.name);
			}
		} 
		else 
		{
			int i = 0;
			_splitpath(szFileName, NULL, NULL, s, NULL);
			sprintf(szNewFileName, "%sSPOOL\\%s.%03d", net_data, s, ++i);
			while ( exist( szNewFileName ) )
			{
				sprintf(szNewFileName, "%sSPOOL\\%s.%03d", net_data, s, ++i);
			}
			if ( copyfile( szFileName, szNewFileName ) )
			{
				_unlink(szFileName);
			}
		}
		nFindNext = _findnext( hFind, &ff );
	}
	_findclose( hFind );
}


void check_exp()
{
	char s[_MAX_PATH];
	
	cd_to(maindir);
	sprintf(s, "%s\\EXP.EXE", maindir);
	if (!exist(s)) 
	{
		log_it(1, "\n ! EXPort/Import module not found!");
		return;
	}
	bool ok = false;
	sprintf(s, "%sS32767.NET", net_data);
	if (exist(s))
	{
		ok = true;
	}
	sprintf(s, "%sINBOUND\\SUB*.*", net_data);
	if (exists(s))
	{
		ok = true;
	}
	sprintf(s, "%sINBOUND\\UNK*.*", net_data);
	if (exists(s)) 
	{
		ok = true;
		sprintf(s, "%sINBOUND\\UNK-0*.*", net_data);
		if (exists(s))
		{
			check_unk();
		}
	}
	sprintf(s, "%sSPOOL\\UNK*.*", net_data);
	if (exists(s))
	{
		ok = true;
	}
	
	if ( ok ) 
	{
        char szCommand[ _MAX_PATH ];
		sprintf(szCommand, "%s\\EXP.EXE S32767.NET %s %hu %s %s %s", maindir, net_data,
            g_nNetworkSystemNumber, FWDNAME, FWDDOM, net_name);
		if (do_spawn(szCommand)) 
		{
			log_it(1, "\n ! %s during export!", strings[2]);
			return;
		}
	} 
	else
	{
		log_it(MOREINFO, "\n \xFE No Internet mail or newsgroup posts to process.");
	}
}



void LaunchWWIVnetSoftware( int nNetNum, int argc, char* argv[] )
{
	char szBuffer[_MAX_PATH];
	output( "\n" );
	set_net_num( nNetNum );
	strcpy( szBuffer, "NETWORK0.EXE" );
	for ( int i = 1; i < argc; i++ ) 
	{
		strcat( szBuffer, " " );
		strcat( szBuffer, argv[i] );
	}
	szBuffer[strlen(szBuffer) + 1] = '\0';
	output( "NOTE: Launching legacy WWIV networking network0.exe\n" );
	do_spawn( szBuffer );
	exit( EXIT_SUCCESS );
}



bool MakeNetDataPath( const char * pszPathName )
{
	char s[_MAX_PATH];

	sprintf( s, "%s%s", net_data, pszPathName );
	if (make_path(s) < 0) 
	{
		log_it( 1, "\n \xFE %s \"%s\"", strings[0], s );
		return false;
	}
	return true;
}


// Ensures the paths required by the application are created, by creating them if required.
bool EnsurePaths()
{
	return (MakeNetDataPath( "SPOOL" ) && 
			MakeNetDataPath( "MQUEUE" ) &&
			MakeNetDataPath( "MQUEUE\\FAILED" ) &&
			MakeNetDataPath( "MQUEUE\\DIGEST" ) &&
			MakeNetDataPath( "OUTBOUND" ) &&
			MakeNetDataPath( "INBOUND" ) &&
			MakeNetDataPath( "SENT" ) &&
			MakeNetDataPath( "CHECKNET" ));
}


bool ReadConfigDat( configrec *pData )
{
	char szFileName[_MAX_PATH];

	strcpy(szFileName, "CONFIG.DAT");
	int hConfigFile = sh_open1(szFileName, O_RDONLY | O_BINARY);
	if ( hConfigFile < 0 ) 
	{
		output( "\n ! %s NOT FOUND.", szFileName );
		return false;
	}
	sh_read( hConfigFile, (void *) (pData), sizeof( configrec ) );
	sh_close( hConfigFile );
	return true;
}


int GetWWIVInstanceNum()
{
	char * ss = getenv("WWIV_INSTANCE");
	if ( ss != NULL ) 
	{
		int instance = atoi(ss);
		if ( instance <= 0  || instance > 999 ) 
		{
			output("\n ! WWIV_INSTANCE must be 1..999!");
			return 1;
		}
		return instance;
	} 
	return 1;
}


bool ReadConfigOvr( configoverrec * pData, int nInstance )
{
	char szFileName[_MAX_PATH];

	strcpy( szFileName, "CONFIG.OVR" );
	int hConfigFile = sh_open1( szFileName, O_RDONLY | O_BINARY);
	if ( hConfigFile < 0 ) 
	{
		output( "\n ! %s NOT FOUND.", szFileName );
		return false;
	}
	sh_lseek( hConfigFile, ( nInstance - 1 ) * sizeof( configoverrec ), SEEK_SET );
	sh_read( hConfigFile, pData, sizeof( configoverrec ) );
	sh_close( hConfigFile );
	
	sprintf( szFileName, "%sINSTANCE.DAT", syscfg.datadir );
	if ( !exist( szFileName ) ) 
	{
		output( "\n ! %s NOT FOUND.", szFileName );
		return false;
	}
	return true;
}



int main(int argc, char *argv[])
{
	char s[201], s1[201], destaddr[255], temp[512], ttotal[21];
	int nNetNumber = 0;
	bool ok = false;
	FILE *fp;
	clock_t starttime, mailtime, newstime;
	struct tm *time_now;
	time_t some;
	const char * tasker = "Win32";
	
	get_dir(maindir, false);

	if ( !ReadConfigDat( &syscfg ) )
	{
		return 1;
	}
	
	int nInstanceNum = GetWWIVInstanceNum();
	
	if ( !ReadConfigOvr( &syscfgovr, nInstanceNum ) )
	{
		return 1;
	}
	int net_num_max = read_networks();
	
	bool bHandleNews = false;
	g_nNumAddresses = 0;
	char * ss = argv[argc - 1];
	if ( ss && strstr((char *) ss, ".") != NULL ) 
	{
		if ( atoi(&ss[1]) < net_num_max ) 
		{
			nNetNumber = atoi(&ss[1]);
			ok = true;
		}
	} 
	else 
	{
		ss = (getenv("WWIV_NET"));
		if ( ss != NULL )
		{
			nNetNumber = atoi(ss);
			if (nNetNumber < net_num_max)
			{
				ok = true;
			}
		}
	}
	if (!ok) 
	{
		strcpy(s, "WWIV_NET.DAT");
		if ((fp = fsh_open(s, "rb")) != NULL) 
		{
			fgets(s, 3, fp);
			nNetNumber = atoi(s);
			if (nNetNumber < net_num_max)
			{
				ok = true;
			}
		}
		if (fp != NULL)
		{
			fclose(fp);
		}
	}
	if ( argc > 1 )
	{
		ss = argv[1];
		if (_strnicmp(ss, "/N", 2))
		{
			ok = false;
		}
	}
	set_net_num(nNetNumber);
	sprintf(s, "%sADDRESS.NET", net_data);
	if (!exist(s)) 
	{
		sprintf(s, "%sADDRESS.1", net_data);
		if (!exist(s))
		{
			ok = false;
		}
	}
	if ( !ok )
	{
		LaunchWWIVnetSoftware( nNetNumber, argc, argv );
		return 0;
	}
	*SMTPHOST = *POPHOST = *NEWSHOST = *TIMEHOST = *QOTDHOST = *QOTDFILE = *POPNAME = *POPPASS = *PROXY = 0;
	*DOMAIN = *FWDNAME = *FWDDOM = 0;
	KEEPSENT = CLEANUP = MOREINFO = NOLISTNAMES = ONECALL = MULTINEWS = 0;
	BBSNODE = ALLMAIL = PURGE = 1;
	POPPORT = DEFAULT_POP_PORT;
	SMTPPORT = DEFAULT_SMTP_PORT;
	int ini_section = 0;
	if (parse_net_ini( ini_section )) 
	{
		set_net_num(nNetNumber);
		if (ini_section & INI_NETWORK) 
		{
			if (!(ini_section & INI_GENERAL)) 
			{
				output("\n ! [GENERAL] tag missing from NET.INI!");
				ok = false;
			}
		} 
		else 
		{
			ok = false;
			output("\n ! [NETWORK] tag missing from NET.INI!");
		}
	} 
	else 
	{
		output("\n ! %s NET.INI!", strings[1]);
		ok = false;
	}

	if ( !ok )
	{
		LaunchWWIVnetSoftware( nNetNumber, argc, argv );
		return 0;
	}
	
	if ( !create_contact_ppp() )
	{
		LaunchWWIVnetSoftware( nNetNumber, argc, argv );
		return 0;
	}

	log_it( 1, "\n\n%s", version );
    output( "\nLicensed under the Apache License." );
	output( "\n%s\n", version_url );
	time(&some);
	time_now = localtime(&some);
	sprintf(s1, "\n \xFE Running under %s on instance %d ", tasker, nInstanceNum );
	strftime(s, 80, "at %I:%M%p, %d %b %Y", time_now);
	strcat(s1, s);
	log_it(1, s1);
	int i = ( net_data[1] == ':' ) ? toupper( net_data[0] ) - 'A' + 1 : toupper( maindir[0] ) - 'A' + 1;
	double dspace = WOSD_FreeSpaceForDriveLetter( i );
	if (dspace < 1000.00) 
	{
		log_it(1, "\n \xFE Only %.1fK available on drive %c:... aborting!", dspace, i + '@');
		do_exit(EXIT_FAILURE);
	} 
	else
	{
        double dMegsFree = (dspace / 1024L);
        if ( dMegsFree > 2048.0 )
        {
		    log_it(1, "\n   Disk space : %c:\\ - %.1fGB", i + '@', ( dMegsFree / 1024L ) );
        }
        else
        {
		    log_it(1, "\n   Disk space : %c:\\ - %.1fMB", i + '@', dMegsFree );
        }
	}
	
	set_net_num(nNetNumber);
	ss = argv[1];
	int sy = ( ss != NULL ) ? atoi(&ss[2]) : 0;
	destaddr[0] = 0;
	if (sy != 32767)
	{
		good_addr(destaddr, sy);
	}
	
	if ( destaddr[0] == 0 && sy != 32767 ) 
	{
		output("\n \xFE Using direct dial for @%hd.%s\n\n", sy, net_name);
		ok = false;
	} 
	else 
	{
		ok = true;
		if (sy == 32767) 
		{
			if (ini_section & INI_NEWS)
			{
				output("\n \xFE Initiating newsgroup session...");
			}
			else 
			{
				output("\n \xFE [NEWS] missing from NET.INI!");
				do_exit(EXIT_FAILURE);
			}
		}
	}
	
	if ( !ok )
	{
		LaunchWWIVnetSoftware( nNetNumber, argc, argv );
		return 0;
	}
	
	/* -- This BLOWS up under Win32.
	if (!MOREINFO)
	freopen(NULL, "w", stdout);
	*/
	if (CLEANUP)
	{
		process_mail();
	}
	set_net_num(nNetNumber);
	
	if ( !EnsurePaths() )
	{
		do_exit( EXIT_FAILURE );
	}

	if ( PURGE && KEEPSENT && sy != 32767 ) 
	{
		log_it(1, "\n \xFE Purging sent packets older than %d day%s...", KEEPSENT, KEEPSENT == 1 ? "" : "s");
		sprintf(s, "%s\\PPPUTIL PURGE %s %d", maindir, net_data, KEEPSENT);
		do_spawn(s);
	}

	bool bNeedToSend = true;
	
	check_exp();
	
	cd_to(maindir);
	if (sy != 32767 || ONECALL ) 
	{
		output("\n \xFE Preparing outbound packets...");
		if ( !uu_packets() ) 
		{
			sprintf(s, "%sMQUEUE\\*.*", net_data);
			_finddata_t ff;
			long hFind = _findfirst( s, &ff );
			if ( hFind != -1 ) 
			{
				output(" already in MQUEUE to be sent.");
			} 
			else 
			{
				output(" no mail or packets to send.");
				bNeedToSend = false;
			}
			_findclose( hFind );
		} 
		else 
		{
			backline();
			output(" \xFE All packets prepared for transfer.");
		}
	}
	starttime = clock();
	
	unsigned long sentbytes, recdbytes, rnewsbytes, snewsbytes;
	sentbytes = recdbytes = snewsbytes = rnewsbytes = 0L;
	cd_to(maindir);
	
	
#ifdef __USE_DIALUP_NETWORK__
	if ( RASDIAL )
	{
		if ( !InternetAutodial( INTERNET_AUTODIAL_FORCE_UNATTENDED, NULL ) )
		{
			log_it( 1, "\n \xFE Unable to connect using InternetAutodial" );
			do_exit( EXIT_FAILURE );
		}
	}
	// %%TODO: Run RAS to dial up connect
#endif	// __USE_DIALUP_NETWORK__
	
	sprintf(temp_dir, "%sINBOUND\\", net_data);
	
	ok = false;
	time(&some);
	time_now = localtime(&some);
	strftime(s, 80, "\n - SMTP/POP on %A, %B %d, %Y at %H:%M %p", time_now);
	log_it(0, s);
	sentbytes = recdbytes = 0L;
	if ( bNeedToSend  && ( sy != 32767 || ONECALL ) ) 
	{
		int nNumMessagesSent = 0;
		sprintf(temp, "%sMQUEUE\\*.*", net_data);
		struct _finddata_t ff;
		long hFind = _findfirst(temp, &ff );
		int nFindNext = ( hFind != -1 ) ? 0 : -1;
		while ( nFindNext == 0 )
		{
			if ( ff.attrib & _A_ARCH ) 
			{
				ok = true;
				++nNumMessagesSent;
				sentbytes += (long) ff.size;
			}
			nFindNext = _findnext( hFind, &ff );
		}
		_findclose( hFind );
		hFind = -1;
		sprintf(s1, "%sMQUEUE\\", net_data);
		sprintf(s, "%s\\POP.EXE -send %s %s %s %d %d", maindir, SMTPHOST, DOMAIN,
			s1, MOREINFO, NOLISTNAMES);
		do_spawn(s);
		
		hFind = _findfirst( temp, &ff );
		nFindNext = ( hFind != -1 ) ? 0 : -1;
		while ( nFindNext == 0 ) 
		{
			if ( ff.attrib & _A_ARCH )
			{
				--nNumMessagesSent;
				sentbytes -= (long) ff.size;
			}
			nFindNext = _findnext( hFind, &ff );
		}
		_findclose( hFind );
		hFind = -1;
		cd_to(maindir);
		if (ok) 
		{
			output(" ");
			backline();
			log_it(0, "\n");
			log_it(1, " \xFE Outbound mail transfer completed : %d message%s (%ldK).",
				nNumMessagesSent, nNumMessagesSent == 1 ? "" : "s", ((sentbytes + 1023) / 1024));
		}
	}
	if ( sy != 32767 || ONECALL ) 
	{
		if ( *PRIMENET && _strnicmp(net_name, PRIMENET, strlen(net_name) != 0 ) ) 
		{
			if (MOREINFO)
			{
				output("\n ! Comparing %s (PRIMENET) to %s.", PRIMENET, net_name);
			}
			output("\n \xFE No packet receipt for %s.", net_name);
		} 
		else 
		{
			sprintf(s, "%s\\POP.EXE -r %s %d %d n%hd", maindir, temp_dir, ALLMAIL, MOREINFO, g_nNetworkSystemNumber);
			sprintf(s1, "%s*.*", temp_dir);
			if ( do_spawn( s ) == EXIT_SUCCESS || exists( s1 ) ) 
			{
				sprintf(temp, "%s*.*", temp_dir);
				struct _finddata_t ff;
				int hFind = _findfirst( temp, &ff );
				int nFindNext = ( hFind != -1 ) ? 0 : -1;
				i = 0;
				while ( nFindNext == 0) 
				{
					if ((strncmp(ff.name, "BAD", 3) != 0) && (strncmp(ff.name, "SPM", 3) != 0) &&
						(strncmp(ff.name, "SUB", 3) != 0) && (strncmp(ff.name, "DUP", 3) != 0) &&
						(strncmp(ff.name, "FIW", 3) != 0) && (ff.size)) 
					{
						recdbytes += (long) ff.size;
						sprintf(s1, "%s%s", temp_dir, ff.name);
						cd_to(maindir);
						sprintf(temp, "%s\\UU.EXE -decode %s %s %s", maindir, ff.name, temp_dir, net_data);
						if (!do_spawn(temp))
						{ 
							_unlink(s1);
						}
					}
					nFindNext = _findnext( hFind, &ff);
				}
				_findclose( hFind );
				hFind = -1;
			} 
			else
			{
				output("\n \xFE No network packets to process");
			}
		}
	}
	check_exp();
	
	mailtime = clock() - starttime + 1;
	if ( sy == 32767 || ONECALL ) 
	{
		time(&some);
		time_now = localtime(&some);
		strftime(s, 80, "\n - NEWS session beginning on %A, %B %d, %Y at %H:%M %p", time_now);
		log_it(0, s);
		bHandleNews = true;
		starttime = clock();
		set_net_num(nNetNumber);
		sprintf(temp, "%sOUTBOUND\\*.*", net_data);
		struct _finddata_t ff;
		long hFind = _findfirst( temp, &ff );
		int nFindNext = ( hFind != -1 ) ? 0 : -1;
		while ( nFindNext == 0 ) 
		{
			if ( ff.attrib & _A_ARCH )
			{
				snewsbytes += (long) ff.size;
			}
			nFindNext = _findnext( hFind, &ff );
		}
		_findclose( hFind );
		hFind = -1;
		cd_to(maindir);
		strcpy(s1, net_data);
		if (LAST(s1) == '\\')
		{
			LAST(s1) = 0;
		}
		sprintf(s, "%s\\NEWS.EXE %s %s %hu", maindir, s1, MULTINEWS ? "-" : NEWSHOST, g_nNetworkSystemNumber);
		ok = true;
		do_spawn(s);
		set_net_num(nNetNumber);
		sprintf(temp, "%sP0*.*", net_data);
		hFind = _findfirst( temp, &ff );
		nFindNext = ( hFind != -1 ) ? 0 : -1;
		while (nFindNext == 0) 
		{
			rnewsbytes += (long) ff.size;
			nFindNext = _findnext( hFind, &ff );
		}
		_findclose( hFind );
		hFind = -1;
	}


	if ( !ok )
	{
		LaunchWWIVnetSoftware( nNetNumber, argc, argv );
		return 0;
	}

	sprintf(s1, "%s\\NTIME.EXE", maindir);
	if ((exist(s1)) && (*TIMEHOST)) 
	{
		output("\n \xFE Polling time from %s...  ", TIMEHOST);
		sprintf(s, "%s %s", s1, TIMEHOST);
		do_spawn(s);
	}

	sprintf(s1, "%s\\QOTD.EXE", maindir);
	if ((exist(s1)) && (*QOTDHOST) && (*QOTDFILE)) 
	{
		output("\n \xFE Getting quote from %s...  ", QOTDHOST);
		if (QOTDFILE && *QOTDFILE)
		{
			sprintf(temp, "%s%s", syscfg.gfilesdir, QOTDFILE);
		}
		else
		{
			temp[0] = 0;
		}
		sprintf(s, "%s %s %s", s1, QOTDHOST, temp);
		do_spawn(s);
	}

#ifdef __USE_DIALUP_NETWORK__
	if ( RASDIAL )
	{
		if ( !InternetAutodialHangup( static_cast<DWORD>( 0 ) ) )
		{
			log_it( 1, "\n \xFE ERROR disconnecting from the internet" );
		}
	}
// %%TODO: Disconnect from RAS
#endif

	output("\n \xFE Updating network connection records...");

	if ((sy != 32767) || ((sy == 32767) && (ONECALL))) 
	{
		if (update_contacts(sy, sentbytes, recdbytes))
		{
			log_it(1, "\n \xFE %s", strings[3]);
		}
		if (mailtime && ( mailtime > starttime ) )
		{
			float timef = ( float ) (mailtime / CLOCKS_PER_SEC / 60);
			sprintf(ttotal, "%3.1f", timef);
		}
		else
		{
			strcpy(ttotal, "0.1");
		}
		sprintf(s, "%s\\PPPUTIL.EXE NETLOG %hd %lu %lu %s %s",
		  maindir, sy, (recdbytes + 1023) / 1024, (sentbytes + 1023) / 1024, ttotal, net_name);
		if (do_spawn(s))
		{
			log_it(1, "\n ! %s", strings[4]);
		}
	}

	if ( bHandleNews ) 
	{
		if ((newstime = clock()) == (clock_t) - 1)
		{
		  log_it(1, "\n \xFE NEWS time invalid.");
		}
		else 
		{
			if (update_contact(32767, snewsbytes, rnewsbytes, 1))
			{
				log_it(1, "\n \xFE %s", strings[3]);
			}
			newstime -= starttime;
			if (newstime && ( newstime > starttime ) )
			{
				float timef = ( float ) (newstime / CLOCKS_PER_SEC / 60);
				sprintf(ttotal, "%3.1f", timef );
			}
			else
			{
				strcpy(ttotal, "0.1");
			}
			rnewsbytes = ((rnewsbytes + 1023) / 1024);
			if (rnewsbytes > 9999L)
			{
				rnewsbytes = 9999L;
			}
			snewsbytes = ((snewsbytes + 1023) / 1024);
			sprintf(s, "%s\\PPPUTIL.EXE NETLOG 32767 %lu %lu %s %s", maindir, snewsbytes, rnewsbytes, ttotal, net_name);
			if (do_spawn(s))
			{
				log_it(1, "\n ! %s", strings[4]);
			}
		}
	}

	if (CLEANUP) 
	{
		process_mail();
		set_net_num(nNetNumber);
	}

	sprintf( s, "%s\\PPPUTIL.EXE TRIM %s NEWS.LOG", maindir, net_data );
	do_spawn( s );

	cd_to( maindir );
	log_it( 1, "\n \xFE %s completed!\n\n", version );
	return 0;
}
