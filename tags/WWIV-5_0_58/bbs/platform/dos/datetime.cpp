/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2003 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/
#include "../../wwiv.h"



char *date()
{
  static char ds[9];
  time_t t;
  time(&t);
  struct tm * pTm = localtime(&t);
  
  sprintf(ds, "%02d/%02d/%02d", pTm->tm_mon+1, pTm->tm_mday, pTm->tm_year % 100);
  return (ds);
}

char *fulldate()
{
  static char ds[11];
  time_t t;
  time(&t);
  struct tm * pTm = localtime(&t);
  
  sprintf(ds, "%02d/%02d/%4d", pTm->tm_mon+1, pTm->tm_mday, pTm->tm_year + 1900);
  return (ds);
}

char *times()
{
  static char ti[9];
  int h, m, s;
  double t;

  t = timer();
  h = (int) (t / 3600.0);
  t -= ((double) (h)) * 3600.0;
  m = (int) (t / 60.0);
  t -= ((double) (m)) * 60.0;
  s = (int) (t);
  sprintf(ti, "%02d:%02d:%02d", h, m, s);
  return (ti);
}


/****************************************************************************/
/* This kludge will get us through 2019 and should not interfere anywhere   */
/* else.                                                                    */
/****************************************************************************/

time_t date_to_daten(const char *datet)
{
	if (strlen(datet) != 8)
	{
		return (0);
	}

	time_t t;
	time(&t);
	struct tm * pTm = localtime(&t);
	pTm->tm_mon		= atoi(datet)+1;
	pTm->tm_mday	= atoi(datet + 3)+1;
	pTm->tm_year	= 1900 + atoi(datet + 6);       /* fixed for 1920-2019 */
	if (datet[6] < '2')
	{
		pTm->tm_year += 100;
	}
	return mktime(pTm);
}


/*
 * Example(s):
 *
 * The following line would show the date that your BBS.EXE was last changed:
 *
 * pl(filedate("BBS.EXE"));
 *
 * Or to be a little fancier:
 *
 * npr("BBS was last modified on %s at %s\r\n",filedate("BBS.EXE"),
 *     filetime("BBS.EXE"));
 *
 */

char *filedate(const char *fpath, char *rtn)
{
	
	if (!exist(fpath))
	{
		return ("");
	}
	
	int i = open(fpath, O_RDONLY);
	if (i == -1)
	{
		return ("");
	}
	
	struct stat buf;
	fstat(i,&buf);
	close(i);

	struct tm *ptm;
	ptm = localtime(&buf.st_mtime);
	sprintf(rtn, "%02d/%02d/%02d", ptm->tm_mon, ptm->tm_mday, ptm->tm_year);

	return (rtn);
}


double timer()
/* This function returns the time, in seconds since midnight. */
{
  double cputim;
  struct dostime_t t;
  long l;
#define SECSINMINUTE 60
#define SECSINHOUR (60 * SECSINMINUTE)
 
  _dos_gettime(&t);
  l = (t.hour * SECSINHOUR) + (t.minute * SECSINMINUTE) + t.second;
  cputim = (double)l + (((double) t.hsecond) / 100);
  return (cputim);
}


long timer1()
/* This function returns the time, in ticks since midnight. */
{
#define TICKS_PER_SECOND 18.2

	return (long) (timer() * TICKS_PER_SECOND);
}


int sysop1()
/* This function returns the status of scoll lock.  If scroll lock is active
 * (ie, the user has hit scroll lock + the light is lit if there is a
 * scoll lock LED), the sysop is assumed to be available.
 */
{
  if ((peekb(0, 1047) & 0x10) == 0)
      return (0);
  else
      return (1);

}


int isleap (unsigned yr)
{
   return yr % 400 == 0 || (yr % 4 == 0 && yr % 100 != 0);
}


/****************************************************************************/

unsigned char dow()
/* returns day of week, 0=Sun, 6=Sat */
{
  time_t long_time;
  struct tm * newtime;

  time( &long_time );                /* Get time as long integer. */
  newtime = localtime( &long_time ); /* Convert to local time. */

  return newtime->tm_wday;
}



/*
* Returns current time as string formatted like HH:MM:SS (01:13:00).
*/

char *ctim(double d)
{
    static char ch[10];
    long h, m, s;
    
    if (d < 0)
    {
        d += 24.0 * 3600.0;
    }
    h = (long) (d / 3600.0);
    d -= (double) (h * 3600);
    m = (long) (d / 60.0);
    d -= (double) (m * 60);
    s = (long) (d);
    sprintf(ch, "%02.2ld:%02.2ld:%02.2ld", h, m, s);
    
    return (ch);
}

/*
* Returns current time as string formatted as HH hours, MM minutes, SS seconds
*/

char *ctim2(double d, char *ch2)
{
    char ht[20], mt[20], st[20];
    long h, m, s;
    
    h = (long) (d / 3600.0);
    d -= (double) (h * 3600);
    m = (long) (d / 60.0);
    d -= (double) (m * 60);
    s = (long) (d);
    if (h == 0)
        strcpy(ht, "");
    else
        sprintf(ht, ".1%ld .9%s", h, (h > 1) ? "hours" : "hour");
    if (m == 0)
        strcpy(mt, "");
    else
        sprintf(mt, ".1%ld .9%s", m, (m > 1) ? "minutes" : "minute");
    if (s == 0)
        strcpy(st, "");
    else
        sprintf(st, ".1%ld .9%s", s, (s > 1) ? "seconds" : "second");
    
    if (h == 0) {
        if (m == 0) {
            if (s == 0) {
                strcpy(ch2, " ");
            } else {
                strcpy(ch2, st);
            }
        } else {
            strcpy(ch2, mt);
            if (s != 0) {
                strcat(ch2, ", ");
                strcat(ch2, st);
            }
        }
    } else {
        strcpy(ch2, ht);
        if (m == 0) {
            if (s != 0) {
                strcat(ch2, ", ");
                strcat(ch2, st);
            }
        } else {
            strcat(ch2, ", ");
            strcat(ch2, mt);
            if (s != 0) {
                strcat(ch2, ", ");
                strcat(ch2, st);
            }
        }
    }
    return (ch2);
}




/* This should not be a problem 'till 2005 or so.                          */

unsigned char years_old(unsigned char m, unsigned char d, unsigned char y)
{
  int a;
  time_t t;
  time(&t);
  struct tm * pTm = localtime(&t);

  if (pTm->tm_year < y)
    return (0);
  if (pTm->tm_year == y) {
    if (pTm->tm_mon < m)
      return (0);
    if (pTm->tm_mon == m) {
      if (pTm->tm_mday < d)
        return (0);
    }
  }
  a = (int) (pTm->tm_year - y);
  if (pTm->tm_mon < m)
    --a;
  else
    if ((pTm->tm_mon == m) && (pTm->tm_mday < d))
    --a;
  return ((unsigned char) a);
}


