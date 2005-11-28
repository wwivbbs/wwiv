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
#include "uu.h"


#define MAX_BUF 1024
#define MAX_DIZ_LINES 10

#define SETREC(f,i)  sh_lseek(f,((long) (i)) * ((long)sizeof(uploadsrec)), SEEK_SET);

char netname[21], net_name[21], netdata[_MAX_PATH], net_data[_MAX_PATH], fdldesc[81], fdlfn[40], fdlupby[40], edlfn[_MAX_PATH];
char net_pkt[21], temp_dir[_MAX_PATH], maindir[_MAX_PATH], dlfn[_MAX_PATH];
unsigned short fdltype;
int numf, net_num;
const char *version = "WWIV Internet Network Support (WINS) UUEncode/UUDecode " VERSION;

directoryrec *dirs;
configrec syscfg;
net_networks_rec netcfg;

#define MAX_XFER 61440L

long huge_xfer(int fd, void * buf, unsigned sz, unsigned nel, int wr)
{
	long nxfr = 0, len = ((long) sz) * ((long) nel);
	char  *xbuf = (char  *) buf;
	unsigned cur, cur1;

	while (len > 0) 
	{
		if (len > MAX_XFER)
		{
			cur = (unsigned int) MAX_XFER;
		}
		else
		{
			cur = (unsigned int) len;
		}

		if (wr)
		{
			cur1 = sh_write(fd, (char *) xbuf, cur);
		}
		else
		{
			cur1 = sh_read(fd, (char *) xbuf, cur);
		}

		if (cur1 != 65535L) 
		{
			len -= cur1;
			nxfr += cur1;
			xbuf = ((char  *) buf) + nxfr;
		}
		if (cur1 != cur)
		{
			break;
		}
	}

	return nxfr;
}


void *mallocx(unsigned long lNumberOfBytes)
{
	void * x = malloc( ( lNumberOfBytes > 0 ) ? lNumberOfBytes : 1 );
	if (!x) 
	{
		log_it(1, "\n ! Insufficient memory (%ld bytes) to read all directories.", lNumberOfBytes);
		return NULL;
	}
	memset( ( void *)  x, 0, ( size_t ) lNumberOfBytes );
	return x;
}


int scanfor(char *token, FILE * in)
{
	char buf[MAX_BUF];

	long pos = ftell(in);
	while (fgets(buf, MAX_BUF, in) && _strnicmp(buf, token, strlen(token))) 
	{
		pos = ftell(in);
	}
	rewind(in);
	fseek(in, pos, 0);
	pos = ftell(in);
	return (!_strnicmp(buf, "begin 6", 7));
}


void scanfdl(FILE * in)
{
	char *ss, buf[MAX_BUF];
	int done = 0;

	while ((fgets(buf, MAX_BUF, in)) && !done) 
	{
		ss = NULL;
		if (_strnicmp(buf, "subject:", 8) == 0) 
		{
			if (strstr(buf, "FILE TRANSFER")) 
			{
				ss = strtok(buf, "@");
				if (ss) 
				{
					ss = strtok(NULL, " ");
					strcpy(netname, ss);
					ss = strtok(NULL, ":");
					if (ss) 
					{
						ss = strtok(NULL, "\r\n");
						if (ss) 
						{
							strcpy(fdlfn, ss);
							trimstr1(fdlfn);
						}
					}
				}
			}
		} 
		else if (_strnicmp(buf, "Description:", 12) == 0) 
		{
			ss = strtok(buf, " ");
			if (ss) 
			{
				ss = strtok(NULL, "\r\n");
				strcpy(fdldesc, ss);
			}
		} 
		else if (_strnicmp(buf, "FDL Type:", 9) == 0) 
		{
			ss = strtok(buf, ":");
			if (ss) 
			{
				ss = strtok(NULL, "\r\n");
				trimstr1(ss);
				fdltype = ( unsigned short ) atoi(ss);
			}
		} 
		else if (_strnicmp(buf, "Originating System:", 19) == 0) 
		{
			ss = strtok(buf, ":");
			if (ss) 
			{
				ss = strtok(NULL, "\r\n");
				trimstr1(ss);
				strcpy(fdlupby, ss);
			}
		} 
		else if (_strnicmp(buf, "begin 600", 9) == 0)
		{
			done = 1;
		}
	}
	return;
}


char *stripfn(char *pszFileName)
{
	static char ofn[15];
	int i1;
	unsigned int i;
	char s[81];

	i1 = -1;
	for (i = 0; i < strlen(pszFileName); i++)
	{
		if ((pszFileName[i] == '\\') || (pszFileName[i] == ':') || (pszFileName[i] == '/'))
		{
			i1 = i;
		}
	}
	if (i1 != -1)
	{
		strcpy(s, &(pszFileName[i1 + 1]));
	}
	else
	{
		strcpy(s, pszFileName);
	}
	for (i = 0; i < strlen(s); i++)
	{
		if ((s[i] >= 'A') && (s[i] <= 'Z'))
		{
			s[i] = ( char ) ( s[i] - 'A' + 'a' );
		}
	}
	i = 0;
	while (s[i] != 0) 
	{
		if (s[i] == 32)
		{
			strcpy(&s[i], &s[i + 1]);
		}
		else
		{
			++i;
		}
	}
	strcpy(ofn, s);

	return ofn;
}


void ssm(char *s)
{
	char szFileName[_MAX_PATH];
	shortmsgrec sm;

	sprintf(szFileName, "%sSMW.DAT", syscfg.datadir);
	int f = sh_open(szFileName, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	if (f < 0)
	{
		return;
	}
	int i = (int) (_filelength(f) / sizeof(shortmsgrec));
	int i1 = i - 1;
	if (i1 >= 0) 
	{
		sh_lseek(f, ((long) (i1)) * sizeof(shortmsgrec), SEEK_SET);
		sh_read(f, (void *) &sm, sizeof(shortmsgrec));
		while ((sm.tosys == 0) && (sm.touser == 0) && (i1 > 0)) 
		{
			--i1;
			sh_lseek(f, ((long) (i1)) * sizeof(shortmsgrec), SEEK_SET);
			sh_read(f, (void *) &sm, sizeof(shortmsgrec));
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
	sh_lseek(f, ((long) (i1)) * sizeof(shortmsgrec), SEEK_SET);
	sh_write(f, (void *) &sm, sizeof(shortmsgrec));
	sh_close(f);
}


void align(char *s)
{
	char f[40], e[40], s1[20], *s2;
	int i2;
	unsigned int i;

	int i1 = 0;
	if (s[0] == '.')
	{
		i1 = 1;
	}
	for (i = 0; i < strlen(s); i++)
	{
		if ((s[i] == '\\') || (s[i] == '/') || (s[i] == ':') || (s[i] == '<') ||
			(s[i] == '>') || (s[i] == '|'))
		{
			i1 = 1;
		}
	}
	if (i1) 
	{
		strcpy(s, "!");
		return;
	}
	s2 = strchr(s, '.');
	if (s2 == NULL) 
	{
		e[0] = 0;
	} 
	else 
	{
		strcpy(e, &(s2[1]));
		e[3] = 0;
		s2[0] = 0;
	}
	strcpy(f, s);

	for (i = strlen(f); i < 8; i++)
	{
		f[i] = 32;
	}
	f[8] = 0;
	i1 = 0;
	i2 = 0;
	for (i = 0; i < 8; i++) 
	{
		if (f[i] == '*')
		{
			i1 = 1;
		}
		if (f[i] == ' ')
		{
			i2 = 1;
		}
		if (i2)
		{
			f[i] = ' ';
		}
		if (i1)
		{
			f[i] = '?';
		}
	}

	for (i = strlen(e); i < 3; i++)
	{
		e[i] = 32;
	}
	e[3] = 0;
	i1 = 0;
	for (i = 0; i < 3; i++) 
	{
		if (e[i] == '*')
		{
			i1 = 1;
		}
		if (i1)
		{
			e[i] = '?';
		}
	}

	for (i = 0; i < 12; i++)
	{
		s1[i] = 32;
	}
	strcpy(s1, f);
	s1[8] = '.';
	strcpy(&(s1[9]), e);
	strcpy(s, s1);
	_strupr(s);
}


void stuff_in(char *s, char *s1, char *f1, char *f2, char *f3, char *f4, char *f5)
{
	int r = 0, w = 0;

	while (s1[r] != 0) 
	{
		if (s1[r] == '%') 
		{
			++r;
			s[w] = 0;
			switch (s1[r]) 
			{
			case '1':
				strcat(s, f1);
				break;
			case '2':
				strcat(s, f2);
				break;
			case '3':
				strcat(s, f3);
				break;
			case '4':
				strcat(s, f4);
				break;
			case '5':
				strcat(s, f5);
				break;
			}
			w = strlen(s);
			r++;
		} 
		else
		{
			s[w++] = s1[r++];
		}
	}
	s[w] = 0;
}


char upcase(char ch)
{
	if ((ch > '`') && (ch < '{'))
	{
		ch = ( char ) ( ch - 32 );
	}
	return ch;
}


char *make_abs_cmd(char *out)
{
	char s[_MAX_PATH], s1[_MAX_PATH], s2[_MAX_PATH], *ss, *ss1;
	char *exts[] = {"", ".COM", ".EXE", ".BAT", ".CMD", 0};
	int i;

	strcpy(s1, out);

	if (s1[1] == ':') 
	{
		if (s1[2] != '\\') 
		{
			_getdcwd( toupper( s1[0] ) - 'A' + 1, s, _MAX_PATH );
			sprintf(out, "%c:\\%s\\%s", s1[0], s, s1 + 2);
		}
		goto got_cmd;
	}
	if (out[0] == '\\') 
	{
		sprintf(out, "%c:%s", maindir[0], s1);
		goto got_cmd;
	}
	ss = strchr(s1, ' ');
	if (ss) 
	{
		*ss = 0;
		sprintf(s2, " %s", ss + 1);
	} 
	else 
	{
		s2[0] = 0;
	}
	for (i = 0; exts[i]; i++) 
	{
		if (i == 0) 
		{
			ss1 = strrchr(s1, '\\');
			if (!ss1)
			{
				ss1 = s1;
			}
			if (strchr(ss1, '.') == 0)
			{
				continue;
			}
		}
		sprintf(s, "%s%s", s1, exts[i]);
		if (exist(s)) 
		{
			sprintf(out, "%s%s%s", maindir, s, s2);
			goto got_cmd;
		} 
		else 
		{
			char *p;
			char szFoundFile[_MAX_PATH];
			DWORD dwLen = ::SearchPath(NULL, s, NULL, _MAX_PATH, szFoundFile, &p );
			if (dwLen > 0) 
			{
				sprintf(out, "%s%s", szFoundFile, s2);
				goto got_cmd;
			}
		}
	}

	sprintf(out, "%s%s%s", maindir, s1, s2);

got_cmd:
	return out;
}


void get_arc_cmd(char *out, char *arcfn, int cmd, char *ofn)
{
	char s[161];
	int i;

	out[0] = 0;
	char *ss = strchr(arcfn, '.');
	if (ss == NULL)
	{
		return;
	}
	++ss;
	for (i = 0; i < 4; i++)
	{
		if (_stricmp(ss, syscfg.arcs[i].extension) == 0) 
		{
			switch (cmd) 
			{
			case 0:
				strcpy(s, syscfg.arcs[i].arcl);
				break;
			case 1:
				strcpy(s, syscfg.arcs[i].arce);
				break;
			case 2:
				strcpy(s, syscfg.arcs[i].arca);
				break;
			}
			if (s[0] == 0)
			{
				return;
			}
			stuff_in(out, s, arcfn, ofn, "", "", "");
			make_abs_cmd(out);
			return;
		}
	}
}


void add_extended_description(char *pszFileName, char *desc)
{
	ext_desc_type ed;

	strcpy(ed.name, pszFileName);
	ed.len = ( short ) strlen(desc);
	int f = sh_open(edlfn, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	sh_lseek(f, 0L, SEEK_END);
	sh_write(f, &ed, sizeof(ext_desc_type));
	sh_write(f, desc, ed.len);
	f = sh_close(f);
}



int get_file_idz(uploadsrec * u, directoryrec * d)
{
	char *b, *ss, cmd[_MAX_PATH], cmdline[_MAX_PATH], s3[_MAX_PATH], s4[_MAX_PATH];
	int f, i, ok = 0;

	ss = strchr(stripfn(u->filename), '.');
	if (ss == NULL)
	{
		return 1;
	}
	++ss;
	for (i = 0; i < 4 && !ok; i++)
	{
		if (!ok)
		{
			ok = (_stricmp(ss, syscfg.arcs[i].extension) == 0);
		}
	}
	if (!ok)
	{
		return 1;
	}
	sprintf(s4, "%sFILE_ID.DIZ", temp_dir);
	_unlink(s4);
	sprintf(s4, "%sDESC.SDI", temp_dir);
	_unlink(s4);
	strcpy(s3, d->path);
	cd_to(s3);
	get_dir(s4, true);
	strcat(s4, stripfn(u->filename));
	cd_to(maindir);
	get_arc_cmd(cmd, s4, 1, "FILE_ID.DIZ DESC.SDI");
	sprintf(cmdline, "%s", cmd);
	cd_to(temp_dir);
	do_spawn(cmdline);
	cd_to(maindir);
	sprintf(s4, "%sFILE_ID.DIZ", temp_dir);
	if (!exist(s4))
	{
		sprintf(s4, "%sDESC.SDI", temp_dir);
	}
	if (exist(s4)) 
	{
		stripfn(s4);
		if ((b = (char *) malloc((long) MAX_DIZ_LINES * 256 + 1)) == NULL)
		{
			return 1;
		}
		f = sh_open1(s4, O_RDONLY | O_BINARY);
		if (_filelength(f) < (MAX_DIZ_LINES * 256)) 
		{
			sh_read(f, b, (int) _filelength(f));
			b[(int)_filelength(f)] = 0;
		} 
		else 
		{
			sh_read(f, b, (int) MAX_DIZ_LINES * 256);
			b[(int) MAX_DIZ_LINES * 256] = 0;
		}
		sh_close(f);
		ss = strtok(b, "\n");
		if (LAST(ss) == '\r')
		{
			LAST(ss) = '\0';
		}
		sprintf(u->description, "%.58s", ss);
		ss = strtok(NULL, "");
		if (ss) 
		{
			for (i = strlen(ss) - 1; i > 0; i--) 
			{
				if ((ss[i] == 26) || (ss[i] == 12))
				{
					ss[i] = 32;
				}
			}
			add_extended_description(u->filename, ss);
			u->mask |= mask_extended;
		}
		if (b)
		{
			free(b);
		}
		sprintf(s4, "%sFILE_ID.DIZ", temp_dir);
		_unlink(s4);
		sprintf(s4, "%sDESC.SDI", temp_dir);
		_unlink(s4);
	}
	return 0;
}


static char *fdldate()
{
	static char ds[9];
	time_t t;
	time(&t);
	struct tm * pTm = localtime(&t);

	sprintf(ds, "%02d/%02d/%02d", pTm->tm_mon+1, pTm->tm_mday, pTm->tm_year % 100);
	return ds;
}


int compare(char *s1, char *s2)
{
	for (int i = 0; i < 12; i++)
	{
		if ((s1[i] != s2[i]) && (s1[i] != '?') && (s2[i] != '?'))
		{
			return 0;
		}
	}
	return 1;
}

void dliscan(int dn)
{
	time_t tNow;
	uploadsrec u;
	directoryrec d = dirs[dn];
	sprintf(dlfn, "%s%s.DIR", syscfg.datadir, d.filename);
	sprintf(edlfn, "%s%s.EXT", syscfg.datadir, d.filename);
	int f = sh_open(dlfn, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	int i = (int)(_filelength(f) / sizeof(uploadsrec));
	if (i == 0) 
	{
		log_it(1, "\n \xFE Creating new directory %s.", dlfn);
		memset(&u, 0, sizeof(uploadsrec));
		strcpy(u.filename, "|MARKER|");
		time(&tNow);
		u.daten = static_cast<unsigned long>(tNow);
		SETREC(f, 0);
		sh_write(f, (void *) &u, sizeof(uploadsrec));
	} 
	else 
	{
		SETREC(f, 0);
		sh_read(f, (void *) &u, sizeof(uploadsrec));
		if (strcmp(u.filename, "|MARKER|")) 
		{
			numf = (int)u.numbytes;
			memset(&u, 0, sizeof(uploadsrec));
			strcpy(u.filename, "|MARKER|");
			time(&tNow);
			u.daten = static_cast<unsigned long>(tNow);
			u.numbytes = numf;
			SETREC(f, 0);
			sh_write(f, &u, sizeof(uploadsrec));
		}
	}
	if (f > -1)
	{
		f = sh_close(f);
	}
	numf = (int)u.numbytes;
}




int recno(char *s)
{
	uploadsrec u;

	int i = 1;
	if (numf < 1)
	{
		return -1;
	}
	int f = sh_open1(dlfn, O_RDONLY | O_BINARY);
	SETREC(f, i);
	sh_read(f, (void *) &u, sizeof(uploadsrec));
	while ((i < numf) && (compare(s, u.filename) == 0)) 
	{
		++i;
		SETREC(f, i);
		sh_read(f, (void *) &u, sizeof(uploadsrec));
	}
	f = sh_close(f);
	return (compare(s, u.filename)) ? i : -1;
}


void upload_file(int dn, char *pszFileName, char *desc, char *upby)
{
	char s1[_MAX_PATH], ff[_MAX_PATH], temppath[_MAX_PATH];
	int f;
	uploadsrec u, u1;

	directoryrec d = dirs[dn];

	strcpy(u.filename, pszFileName);
	align(u.filename);
	u.ownerusr = 1;
	u.ownersys = 0;
	u.numdloads = 0;
	u.filetype = 0;
	u.mask = 0;
	strcpy(temppath, d.path);
	make_abs_path(temppath, maindir);
	sprintf(ff, "%s%s", temppath, pszFileName);
	f = sh_open1(ff, O_RDONLY | O_BINARY);
	long lFileSize = _filelength(f);
	u.numbytes = lFileSize;
	sh_close(f);
	strcpy(u.upby, upby);
	strcpy(u.date, fdldate());
	strcpy(u.description, desc);
	time_t tNow;
	time(&tNow);
	u.daten = static_cast<unsigned long>(tNow);
	get_file_idz(&u, &d);
	f = sh_open(dlfn, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	SETREC(f, 0);
	sh_read(f, &u1, sizeof(uploadsrec));
	numf = (int)u1.numbytes;
	++numf;
	SETREC(f, numf);
	sh_write(f, (void *) &u, sizeof(uploadsrec));
	SETREC(f, 0);
	sh_read(f, &u1, sizeof(uploadsrec));
	u1.numbytes = numf;
	u1.daten = static_cast<unsigned long>(tNow);
	SETREC(f, 0);
	sh_write(f, (void *) &u1, sizeof(uploadsrec));
	f = sh_close(f);
	sprintf(s1, "FDL : %s uploaded on %s", u.filename, d.name);
	ssm(s1);
	log_it(1, "\n \xFE %s", s1);
}


void free_dirs(void)
{
	if (dirs != NULL)
	{
		free(dirs);
	}
	dirs = NULL;
}


void name_file(int msg, char *newname)
{
	struct stat info;

	for (int i = 0; i < 10000; i++) 
	{
		sprintf(newname, "%s%s\\%s-%04.04d.MSG",
			netdata, msg ? "CHECKNET" : "SPOOL",
			msg ? "CHK" : "UNK", i);
		if (stat(newname, &info) == -1)
		{
			break;
		}
	}
}


void move_bad(char *src)
{
	char dest[_MAX_PATH], file[12], ext[5];
	int i = 0;

	_splitpath(src, NULL, NULL, file, ext);

	sprintf(dest, "%sCHECKNET\\%s%s", netdata, file, ext);
	while (exist(dest))
	{
		sprintf(dest, "%sCHECKNET\\%s.%03d", netdata, file, i++);
	}

	if (copyfile(src, dest))
	{
		_unlink(src);
	}
}


int init(void)
{
	char szFileName[_MAX_PATH];

	sprintf(szFileName, "%sCONFIG.DAT", maindir);
	int f = sh_open1(szFileName, O_RDONLY | O_BINARY);
	if (f < 0) 
	{
		output("\n \xFE Unable to read %s.", szFileName);
		return 1;
	}
	sh_read(f, (void *) (&syscfg), sizeof(configrec));
	sh_close(f);
	return 0;
}

int match_net(char *pszFileName)
{
	char line[161];
	int found = 0;
	FILE *fp;

	if ((fp = fsh_open(pszFileName, "rt")) != NULL) {
		while (!feof(fp) && (!found)) {
			fgets(line, 160, fp);
			if (_strnicmp(line, "NET: ", 5) == 0) {
				strcpy(net_pkt, &line[5]);
				trimstr1(net_pkt);
				found = 1;
			}
		}
		fclose(fp);
	}
	return found;
}

int read_networks(void)
{
	int f, max;
	char s[121];

	sprintf(s, "%sNETWORKS.DAT", syscfg.datadir);
	f = sh_open1(s, O_RDONLY | O_BINARY);
	max = 0;
	if (f > 0)
		max = (int) (_filelength(f) / sizeof(net_networks_rec));
	sh_close(f);
	return max;
}

void set_net_num(int n)
{
	char s[121];
	int f;

	sprintf(s, "%sNETWORKS.DAT", syscfg.datadir);
	f = sh_open1(s, O_RDONLY | O_BINARY);
	if (f < 0)
		return;
	_lseek(f, ((long) (n)) * sizeof(net_networks_rec), SEEK_SET);
	sh_read(f, &netcfg, sizeof(net_networks_rec));
	_close(f);
	net_num = n;
	//  net_sysnum = netcfg.sysnum;
	strcpy(net_name, netcfg.name);
	strcpy(net_data, make_abs_path(netcfg.dir, maindir));
	if (LAST(net_data) != '\\')
		strcat(net_data, "\\");
}


int main(int argc, char *argv[])
{
	char outname[181], buf[_MAX_PATH], dirfn[21], dirpath[_MAX_PATH], junk[80];
	char *ss, argfile[21], s1[181], s2[181], s3[21], s4[21], s5[21], szFileName[_MAX_PATH];
	int i, f, ok = 0, found = 0, ok1, dn, result, num_dirs, net_num_max;
	long lFileSize = 0;
	unsigned short dirtype;
	FILE *in = NULL, *out = NULL, *fp = NULL;

	ok1 = 0;

	if (argc == 4 || argc == 5) 
	{
		if (_strcmpi(argv[1], "-encode") == 0 && argc == 4) 
		{
			if (((in = fsh_open(argv[2], "r")) != NULL) && ((out = fsh_open(argv[3], "at+")) != NULL)) {
				output(" %ld bytes.", uuencode(in, out, argv[2]));
				if (in != NULL)
					fclose(in);
				if (out != NULL)
					fclose(out);
				ok1 = EXIT_SUCCESS;
			} else
				ok1 = EXIT_FAILURE;
		} 
		else if (_strcmpi(argv[1], "-decode") == 0 && argc == 5) 
		{
			strcpy(argfile, argv[2]);
			strcpy(temp_dir, argv[3]);
			strcpy(netdata, argv[4]);
			strcpy(maindir, argv[0]);
			while(LAST(maindir) != '\\')
				LAST(maindir) = 0;
			if (init()) {
				log_it(1, "\n \xFE Failed to initialize UU variables.");
				ok1 = EXIT_FAILURE;
			}
			cd_to(temp_dir);

			if ((in = fsh_open(argfile, "r")) != NULL) 
			{
				netname[0] = 0;
				fdldesc[0] = 0;
				fdltype = 0;
				fdlfn[0] = 0;
				scanfdl(in);
				rewind(in);
				scanfor("begin", in);
				result = uudecode(in, temp_dir, outname);
				if (in != NULL)
					fclose(in);
				switch (result) 
				{
				case UU_NO_MEM:
					sprintf(s1, "%s%s", temp_dir, argfile);
					move_bad(s1);
					cd_to(maindir);
					sprintf(s1, "Error decoding %s.", argfile);
					log_it(1, "\n \xFE %s", s1);
					ssm(s1);
					sprintf(s1, "%s%s", netdata, outname);
					_unlink(s1);
					ok1 = EXIT_FAILURE;
					break;
				case UU_BAD_BEGIN:
					sprintf(s1, "%s%s", temp_dir, argfile);
					name_file(0, s2);
					if (!copyfile(s1, s2))
						move_bad(s2);
					else
						_unlink(s1);
					cd_to(maindir);
					sprintf(s1, "%s%s", netdata, outname);
					_unlink(s1);
					ok1 = EXIT_FAILURE;
					break;
				case UU_CANT_OPEN:
					log_it(1, "\n \xFE Cannot open %s%s", netdata, outname);
					cd_to(maindir);
					ok1 = EXIT_FAILURE;
					break;
				case UU_CHECKSUM:
					sprintf(s1, "%s%s", temp_dir, argfile);
					move_bad(s1);
					cd_to(maindir);
					sprintf(s1, "Checksum error decoding %s - moved to CHECKNET!", argfile);
					log_it(1, "\n \xFE %s", s1);
					ssm(s1);
					sprintf(s1, "%s%s", netdata, outname);
					_unlink(s1);
					ok1 = EXIT_FAILURE;
					break;
				case UU_BAD_END:
					sprintf(s1, "%s%s", temp_dir, argfile);
					move_bad(s1);
					cd_to(maindir);
					sprintf(s1, "No \'end\' found in %s - moved to CHECKNET!", argfile);
					log_it(1, "\n \xFE %s", s1);
					ssm(s1);
					sprintf(s1, "%s%s", netdata, outname);
					_unlink(s1);
					ok1 = EXIT_FAILURE;
					break;
				case UU_SUCCESS:
					sprintf(s1, "%s%s", temp_dir, argfile);
					found = (match_net(s1));
					_unlink(s1);
					stripfn(outname);
#ifdef MEGA_DEBUG_LOG_INFO
					output( "\nDEBUG: outname = %s\n", outname );
#endif // #ifdef MEGA_DEBUG_LOG_INFO
					_splitpath(outname, NULL, NULL, s4, s3);
					sprintf(s5, "%s%s", s4, s3);
#ifdef MEGA_DEBUG_LOG_INFO
					output( "\nDEBUG: filename = %s", s4 );
					output( "\nDEBUG: ext = %s\n", s3 );
					output( "\nDEBUG: argfile = %s\n", argfile );
					output( "\nDEBUG: net_name = %s\n", net_name );
#endif // #ifdef MEGA_DEBUG_LOG_INFO
					if ((_strcmpi(s3, ".NET") == NULL) && (_strnicmp(argfile, "PKT", 3) == NULL)) 
					{
						if (found) {
							found = 0;
							net_num_max = read_networks();
							for (i = 0; ((i < net_num_max) && (!found)); i++) 
							{
								set_net_num(i);
								if (_strnicmp(net_name, net_pkt, strlen(net_name)) == 0) 
								{
									strcpy(netname, net_name);
									strcpy(netdata, net_data);
									found = 1;
								}
							}
							if (!found) {
								strcat(netdata, "CHECKNET\\");
								log_it(1, "\n ! Unknown network %s in %s - moved to CHECKNET.", net_pkt, s5);
							}
						} else
							log_it(0, "\n \xFE No network specified in %s packet.", s5);
						i = 0;
						sprintf(s2, "%sP1-%03d.NET", netdata, i);
						while (exist(s2))
							sprintf(s2, "%sP1-%03d.NET", netdata, ++i);
						sprintf(s1, "%s%s", temp_dir, stripfn(outname));
						if (!copyfile(s1, s2)) {
							log_it(1, "\n \xFE Error creating %s", s2);
							cd_to(maindir);
							ok1 = EXIT_FAILURE;
							break;
						} else
							_unlink(s1);
					} 
					else 
					{
						if (fdltype)
							sprintf(s1, "Received : %s (FDL %hu)", s5, fdltype);
						else
							sprintf(s1, "Received transferred file : %s.", s5);
						ssm(s1);
						log_it(1, "\n \xFE %s", s1);
						num_dirs = 0;
						dirs = (directoryrec *) mallocx(((long) syscfg.max_dirs) *
							((long) sizeof(directoryrec)));
						if (dirs == NULL)
						{
							log_it(0, "\n - Insufficient memory to read directories!");
						}
						else 
						{
							sprintf(szFileName, "%sDIRS.DAT", syscfg.datadir);
							f = sh_open1(szFileName, O_RDONLY | O_BINARY);
							if (f < 0)
							{
								log_it(1, "\n ! Unable to read %s", szFileName);
							}
							else 
							{
								num_dirs = (int) huge_xfer(f, dirs, sizeof(directoryrec),
									syscfg.max_dirs, 0) / sizeof(directoryrec);
								log_it(0, "\n - Total dirs %d", num_dirs);
								if (num_dirs) 
								{
									lFileSize = _filelength(f) / num_dirs;
								}
								if (lFileSize != sizeof(directoryrec))
								{
									num_dirs = 0;
								}
							}
							f =sh_close(f);
						}
						sprintf(s1, "%s%s", temp_dir, s5);
						if (num_dirs) 
						{
							dn = 0;
							if (fdltype) 
							{
								for (i = 0; (i < num_dirs && !dn); i++) 
								{
									if (dirs[i].type == fdltype) {
										dn = i;
										break;
									}
								}
								if (!dn) {
									log_it(1, "\n \xFE FDL %hu not in DIREDIT... checking FDLFTS.CFG.",
										fdltype);
									sprintf(szFileName, "%sFDLFTS.CFG", netdata);
									if ((fp = fsh_open(szFileName, "rt")) == NULL) {
										log_it(1, "\n \xFE FDLFTS.CFG does not exist... uploading to Sysop.");
									} else {
										ok = 0;
										while ((fgets(buf, 80, fp)) && !ok) {
											if (buf[0] == ':')
												continue;
											if (_strnicmp(buf, "FDL", 3) == 0) {
												sscanf(buf, "%s %hu %s %s", junk, &dirtype, dirfn, dirpath);
												trimstr1(dirfn);
												if (dirtype == fdltype) {
													for (i = 0; i < num_dirs; i++) 
													{
														if (_strnicmp((char *)dirs[i].filename, dirfn, strlen(dirfn)) == 0) {
															dn = i;
															ok = 1;
															break;
														}
													}
												}
											}
										}
										if (fp != NULL)
											fclose(fp);
									}
								}
								if ((!ok) && (fdltype))
									log_it(1, "\n \xFE FDL %hu not defined in FDLFTS.CFG... uploading to Sysop.",
									fdltype);
							} else {
								dn = 0;
								strcpy(fdldesc, "[No Description Found]");
								strcpy(fdlupby, "FILEnet Transfer");
								sprintf(szFileName, "%sFDLFTS.CFG", netdata);
								if ((fp = fsh_open(szFileName, "rt")) == NULL) {
									log_it(1, "\n \xFE %s does not exist... upload to Sysop.", szFileName);
								} else {
									ok = 0;
									while ((fgets(buf, 80, fp)) && !ok) {
										if (_strnicmp(buf, "NOREQUEST_DIR", 13) == 0) {
											ss = strtok(buf, " ");
											if (ss) {
												ss = strtok(NULL, "\r\n");
												if (ss) {
													strcpy(dirfn, ss);
													trimstr1(dirfn);
													ok = 1;
													for (i = 0; i < num_dirs; i++) 
													{
														if (_strnicmp(dirs[i].filename, dirfn, strlen(dirfn)) == 0) {
															dn = i;
															break;
														}
													}
												}
											}
										}
									}
									if (fp != NULL)
										fclose(fp);
								}
								if (!ok)
									log_it(1, "\n \xFE NOREQUEST_DIR not defined in %s.", szFileName);
							}
							sprintf(s1, "%s%s", temp_dir, s5);
							strcpy(buf, (char *)dirs[dn].path);
							make_abs_path(buf, maindir);
							sprintf(s2, "%s%s", buf, s5);
							if (exist(s2)) {
								log_it(1, "\n \xFE %s already exists", s2);
								move_bad(s1);
							} else {
								if (!copyfile(s1, s2)) {
									log_it(1, "\n \xFE Error copying %s", s2);
									move_bad(s1);
								} else {
									dliscan(dn);
									strcpy(szFileName, s5);
									align(szFileName);
									ok = recno(szFileName);
									if (ok == -1)
										upload_file(dn, s5, fdldesc, fdlupby);
									else
										log_it(1, "\n \xFE %s already in %s.", szFileName, dirs[dn].name);
								}
							}
							_unlink(s1);
						} else {
							log_it(1, "\n \xFE Moving %s to CHECKNET", s1);
							move_bad(s1);
						}
						fdltype = 0;
						fdlfn[0] = 0;
						fdldesc[0] = 0;
						free_dirs();
					}
					cd_to(maindir);
					ok1 = EXIT_SUCCESS;
					break;
				default:
					sprintf(s1, "%s%s", temp_dir, argfile);
					name_file(1, s2);
					log_it(1, "\n \xFE Unknown error %s...", s2);
					if (!copyfile(s1, s2))
						log_it(1, "\n \xFE Error during copy...");
					else
						_unlink(s1);
					cd_to(maindir);
					sprintf(s1, "Unknown error %s...", s2);
					ssm(s1);
					ok1 = EXIT_FAILURE;
					break;
				}
			} 
			else 
			{
				cd_to(maindir);
				log_it(1, "\n \xFE Input file %s not found.", argfile);
				ok1 = EXIT_FAILURE;
			}
		}		
	} 
	else
	{
		output("\n \xFE %s\n", version);
		// %%TODO: Add usage here
	}
	if (in != NULL)
		fclose(in);
	if (out != NULL)
		fclose(out);
	return ok1;
}
