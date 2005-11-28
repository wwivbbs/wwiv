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

configrec syscfg;

char net_name[16], net_data[_MAX_PATH];

void name_chunk(char *chunkname)
{
	bool ok = false;
	for ( int i = 0; ((i < 1000) && (!ok)); i++ )
	{
		sprintf(chunkname, "%sSPOOL\\UNK-9%3.3d.UUE", net_data, i);
		struct _stat info;
		if ( _stat( chunkname, &info ) == -1 )
		{
			ok = true;
		}
	}
}


int chunk(char *fn)
{
	char s[255], outfn[_MAX_PATH];
	long textlen, curpos;
	size_t hdrlen = 0;
	FILE *in, *out;
	
	fprintf(stderr, "\n \xFE Splitting %s into 32K chunks.", fn);
	
	in = fopen(fn, "rb");
	if (in == NULL) 
	{
		fprintf(stderr, "\n ! Unable to open %s.", fn);
		return 1;
	}
	
	bool done = false;
	do 
	{
		fgets(s, 254, in);
		if (*s == '')
		{
			hdrlen = (size_t) ftell(in);
		}
		else
		{
			done = true;
		}
	} while (!done);

	if ( hdrlen == 0 )
	{
		fprintf( stderr, "\n ! Can not find the size for the header" );
		fclose( in );
		return 1;
	}
	
	char* hdrbuf = (char *) malloc(hdrlen + 1);
	if (hdrbuf == NULL) 
	{
		fprintf(stderr, "\n ! Unable to allocate %ld bytes for header.", hdrlen);
		fclose(in);
		return 1;
	}
	
	rewind(in);
	fread((void *) hdrbuf, sizeof(char), hdrlen, in);
	
	curpos = hdrlen;
	done = false;
	rewind(in);
	while (!done) 
	{
		name_chunk(outfn);
		if ((out = fopen(outfn, "wb+")) == NULL) 
		{
			free(hdrbuf);
			fclose(in);
			return 1;
		}
		fwrite((void *) hdrbuf, sizeof(char), hdrlen, out);
		fprintf(out, "\n");
		fseek(in, curpos, SEEK_SET);
		textlen = 0L;
		do 
		{
			fgets(s, 254, in);
			textlen += fprintf(out, s);
		} while ((textlen < (long) (32000 - hdrlen)) && (feof(in) == 0));
		
		if (feof(in) != 0)
		{
			done = true;
		}
		else 
		{
			fprintf(out, "\r\n\r\nContinued in next message...\r\n");
			curpos = ftell(in);
		}
		if (out != NULL)
		{
			fclose(out);
		}
	}
	if (in != NULL)
	{
		fclose(in);
	}
	if (hdrbuf)
	{
		free(hdrbuf);
	}
	return 0;
}


void purge_sent(int days)
{
	char s[121];
	int howmany = 0;
	time_t age = 0;
	struct _finddata_t ff;
	struct _stat fileinfo;
	
	sprintf( s, "%sSENT\\*.*", net_data );
	long hFind = _findfirst( s, &ff );
	int nFindNext = ( hFind != -1 ) ? 0 : -1;
	while ( nFindNext == 0 ) 
	{
		sprintf( s, "%sSENT\\%s", net_data, ff.name );
		if ( _stat( s, &fileinfo ) == 0 ) 
		{
			age = ( time(NULL) - fileinfo.st_atime );
			if ( age > ( SECONDS_PER_DAY * days ) ) 
			{
				++howmany;
				_unlink( s );
			}
		}
		nFindNext = _findnext( hFind, &ff );
	}
	fprintf( stderr, " %d packet%s deleted.", howmany, howmany == 1 ? "" : "s" );
}


#define MAX_LOG 1000

void trim_log(char *ol)
{
	int num_lines, total_lines, kill_lines;
	FILE *old_log, *new_log;
	char nl[_MAX_PATH], s[160];
	
	sprintf(ol, "%sNEWS.LOG", net_data);
	sprintf(nl, "%sNEWS.ZZZ", net_data);
	
	old_log = fopen(ol, "r");
	new_log = fopen(nl, "a");
	
	total_lines = 0;
	if (old_log != NULL) 
	{
		while (!(fgets(s, 160, old_log) == NULL))
		{
			++total_lines;
		}
		rewind(old_log);
		if (total_lines < MAX_LOG) 
		{
			fclose(old_log);
			if (new_log != NULL)
			{
				fclose(new_log);
			}
			_unlink(nl);
			return;
		}
		kill_lines = total_lines - MAX_LOG;
		num_lines = 0;
		while ((fgets(s, 160, old_log)) && (num_lines < kill_lines))
		{
			num_lines++;
		}

		while ((strstr(s, "WWIV Internet Network Support (WINS)") == NULL) &&
			(num_lines < total_lines)) 
		{
			fgets(s, 160, old_log);
			num_lines++;
		}
		fputs(s, new_log);
		while ((!(fgets(s, 160, old_log) == NULL)))
			fputs(s, new_log);
	}
	if (old_log != NULL)
	{
		fclose(old_log);
	}
	if (new_log != NULL)
	{
		fclose(new_log);
	}
	_unlink(ol);
	rename(nl, ol);
}


#define MAX_LEN 12288L

int open_netlog(char *fn)
{
	int f, count = 0;
	
	do 
	{
		f = _open(fn, O_RDWR | O_BINARY | SH_DENYRW | O_CREAT, S_IREAD | S_IWRITE);
	} while ((f < 0) && (errno == EACCES) && (count++ < 500));
	
	return (f);
}


int write_netlog(int sn, long sent, long recd, char *tmused)
{
	int f;
	char s[101], s1[81], s2[81], fn[121];
	long l;
	struct tm *time_now;
	time_t some;

	//printf(" write_netlog: %d %ld %ld %s", sn, sent, recd, tmused );
	
	time(&some);
	time_now = localtime(&some);
	strftime(s1, 35, "%m/%d/%y %H:%M:%S", time_now);
	
	if ((sent) || (recd)) 
	{
		if ((recd) && (!sent))
		{
			sprintf(s2, "       , R:%4ldk,", recd);
		}
		else 
		{
			if ((recd) && (sent))
			{
				sprintf(s2, "S:%4ldk, R:%4ldk,", sent, recd);
			}
			else
			{
				sprintf(s2, "S:%4ldk,         ", sent);
			}
		}
	} 
	else
	{
		strcpy(s2, "                 ");
	}

	if ( strlen( tmused ) > 10 )
	{
		printf(" Bad time pased to tmused\r\n" );
		tmused = "0.0";
	}
	sprintf(s, "%s To %5d, %s         %5s min  %s\r\n", s1, sn, s2, tmused, net_name);
	
	char* ss = (char *) malloc(MAX_LEN + 1024L);
	if (ss == NULL)
	{
		return 1;
	}
	strcpy(ss, s);
	
	sprintf(fn, "%sNET.LOG", syscfg.gfilesdir);
	f = open_netlog(fn);
	_lseek(f, 0L, SEEK_SET);
	l = (long) (_read(f, (void *) (&(ss[strlen(s)])), (int) MAX_LEN) + strlen(s));
	while ((l > 0L) && (ss[(int) l] != '\n'))
	{
		--l;
	}
	_lseek(f, 0L, SEEK_SET);
	_write(f, (void *) ss, (int) l + 1);
	_chsize(f, l + 1);
	if (ss) 
	{
		free(ss);
		ss = NULL;
	}
	_close(f);
	return 0;
}


int main(int argc, char *argv[])
{
	UNREFERENCED_PARAMETER( argc );

    char szConfigFileName[ _MAX_PATH ];
	strcpy(szConfigFileName, "CONFIG.DAT");
	int f = _open(szConfigFileName, O_RDONLY | O_BINARY);
	if (f < 0)
	{
		return 1;
	}
	_read(f, (void *) (&syscfg), sizeof(configrec));
	_close(f);
	
	if (strncmp(argv[1], "NETLOG", 6) == 0) 
	{
		strcpy(net_name, argv[6]);
		unsigned int sy = atoi(argv[2]);
		unsigned long sent = atol(argv[3]);
		unsigned long recd = atol(argv[4]);
		if (write_netlog(sy, sent, recd, argv[5]))
		{
			return 1;
		}
	}
	
	if (strncmp(argv[1], "TRIM", 4) == 0) 
	{
		strcpy(net_data, argv[2]);
        char szBuffer[ 255 ];
		sprintf(szBuffer, "%s%s", net_data, argv[3]);
		trim_log(szBuffer);
	}
	
	if (strncmp(argv[1], "PURGE", 5) == 0) 
	{
		strcpy(net_data, argv[2]);
		int i = atoi(argv[3]);
		purge_sent( i );
	}
	
	if (strncmp(argv[1], "CHUNK", 5) == 0) 
	{
		strcpy(net_data, argv[2]);
        char szBuffer[ 255 ];
		sprintf(szBuffer, "%sINBOUND\\%s", net_data, argv[3]);
		if (!chunk(szBuffer))
		{
			_unlink(szBuffer);
		}
	}
	
	return 0;
}
