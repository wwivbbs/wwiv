/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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



char *date()
{
  static char szDateString[11];
  time_t t = time( NULL );
  struct tm * pTm = localtime( &t );

  snprintf( szDateString, sizeof( szDateString ), "%02d/%02d/%02d", pTm->tm_mon+1, pTm->tm_mday, pTm->tm_year % 100 );
  return szDateString;
}


char *fulldate()
{
  static char szDateString[11];
  time_t t = time( NULL );
  struct tm * pTm = localtime( &t );

  snprintf( szDateString, sizeof( szDateString ), "%02d/%02d/%4d", pTm->tm_mon+1, pTm->tm_mday, pTm->tm_year + 1900 );
  return szDateString;
}


char *times()
{
	static char szTimeString[9];

	time_t tim = time( NULL );
	struct tm *t = localtime( &tim );
	snprintf( szTimeString, sizeof( szTimeString ), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec );
	return szTimeString;
}


//
// This kludge will get us through 2019 and should not interfere anywhere
// else.
//

time_t date_to_daten(const char *datet)
{
	if ( strlen( datet ) != 8 )
	{
		return 0;
	}

	time_t t = time( NULL );
	struct tm * pTm = localtime( &t );
	pTm->tm_mon		= atoi( datet );
	pTm->tm_mday	= atoi( datet + 3 );
	pTm->tm_year	= 1900 + atoi( datet + 6 );       // fixed for 1920-2019
	if ( datet[6] < '2' )
	{
		pTm->tm_year += 100;
	}
	return mktime( pTm );
}


/*
 * Returns the date a file was last modified as a string
 *
 * The following line would show the date that your BBS.EXE was last changed:
 * char szBuffer[ 81 ];
 * bputs("BBS was last modified on %s at %s\r\n",filedate( "BBS.EXE", szBuffer ),
 *     filetime("BBS.EXE"));
 *
 */
char *filedate( const char *pszFileName, char *pszReturnValue )
{
	if ( !WFile::Exists( pszFileName ) )
	{
		return "";
	}

	int i = open( pszFileName, O_RDONLY );
	if ( i == -1)
	{
		return "";
	}

	struct stat buf;
	fstat( i, &buf );
	close( i );

	struct tm *ptm = localtime( &buf.st_mtime );
    
    // We use 9 here since that is the size of the date format MM/DD/YY + NUL
	snprintf( pszReturnValue, 9, "%02d/%02d/%02d", ptm->tm_mon, ptm->tm_mday, ( ptm->tm_year % 100 ) );

	return pszReturnValue;
}


double timer()
/* This function returns the time, in seconds since midnight. */
{

#ifdef NEVER


#define SECSINMINUTE 60
#define SECSINHOUR (60 * SECSINMINUTE)
  SYSTEMTIME st;
  GetLocalTime( &st );

  long l = ( st.wHour * SECSINHOUR ) + ( st.wMinute * SECSINMINUTE ) + st.wSecond;
  double cputim = static_cast<double>( l ) +
                 ( static_cast<double>( st.wMilliseconds ) / 1000 );
  return cputim;

#else
	time_t ti       = time( NULL );
	struct tm *t    = localtime( &ti );

	double cp = static_cast<double>( t->tm_hour * SECONDS_PER_HOUR_FLOAT ) +
		        static_cast<double>( t->tm_min * SECONDS_PER_MINUTE_FLOAT ) +
				static_cast<double>( t->tm_sec );

	return cp;
#endif

}


long timer1()
/* This function returns the time, in ticks since midnight. */
{
#define TICKS_PER_SECOND 18.2

	return static_cast<long>( timer() * TICKS_PER_SECOND );
}


void ToggleScrollLockKey()
{
#if defined( _WIN32 )
    // Simulate a key press
    keybd_event( VK_SCROLL, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0 );
    // Simulate a key release
    keybd_event( VK_SCROLL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
#endif // _WIN32
}


bool sysop1()
/* This function returns the status of scoll lock.  If scroll lock is active
 * (ie, the user has hit scroll lock + the light is lit if there is a
 * scoll lock LED), the sysop is assumed to be available.
 */
{
#if defined (__OS2__)
#if !defined(KBDSTF_SCROLLLOCK_ON)
#define KBDSTF_SCROLLLOCK_ON 0x0010
#endif

  KBDINFO ki;
  KbdGetStatus( &ki, 0 );
  return ( ki.fsState & KBDSTF_SCROLLLOCK_ON );

#elif defined (_WIN32)
  return ( GetKeyState( VK_SCROLL ) & 0x1 );
#else
  return false;
#endif
}


bool isleap ( int nYear )
{
   WWIV_ASSERT( nYear >= 0 );
   return nYear % 400 == 0 || ( nYear % 4 == 0 && nYear % 100 != 0 );
}


/** returns day of week, 0=Sun, 6=Sat */
int dow()
{
#ifdef _WIN32
	struct tm * newtime;
	time_t long_time = time( &long_time );  // Get time as long integer.
	newtime = localtime( &long_time );      // Convert to local time.
	return newtime->tm_wday;
#else // _WIN32
	struct tm t;
	asctime(&t);
	return static_cast<unsigned char>( t.tm_wday );
#endif // _WIN32
}



/*
 * Returns current time as string formatted like HH:MM:SS (01:13:00).
 */
char *ctim( double d )
{
    static char szCurrentTime[10];

    if (d < 0)
    {
		d += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
    }
    long lHour = static_cast<long>( d / SECONDS_PER_HOUR_FLOAT );
    d -= static_cast<double>( lHour * HOURS_PER_DAY );
    long lMinute = static_cast<long>( d / MINUTES_PER_HOUR_FLOAT );
	d -= static_cast<double>( lMinute * MINUTES_PER_HOUR );
    long lSecond = static_cast<long>( d );
    snprintf( szCurrentTime, sizeof( szCurrentTime ), "%2.2ld:%2.2ld:%2.2ld", lHour, lMinute, lSecond );

    return szCurrentTime;
}

/*
* Returns current time as string formatted as HH hours, MM minutes, SS seconds
*/
char *ctim2( double d, char *ch2 )
{
	std::string result = "";

    char szHours[20], szMinutes[20], szSeconds[20];

    long h = static_cast<long>( d / SECONDS_PER_HOUR_FLOAT );
	d -= static_cast<double>( h * SECONDS_PER_HOUR );
    long m = static_cast<long>( d / SECONDS_PER_MINUTE_FLOAT );
    d -= static_cast<double>( m * SECONDS_PER_MINUTE );
    long s = static_cast<long>( d );

    if (h == 0)
    {
        strcpy(szHours, "");
    }
    else
    {
        snprintf( szHours, sizeof( szHours ), "|#1%ld |#9%s", h, (h > 1) ? "hours" : "hour" );
    }
    if (m == 0)
    {
        strcpy(szMinutes, "");
    }
    else
    {
        snprintf( szMinutes, sizeof( szMinutes ), "|#1%ld |#9%s", m, (m > 1) ? "minutes" : "minute" );
    }
    if (s == 0)
    {
        strcpy(szSeconds, "");
    }
    else
    {
        snprintf( szSeconds, sizeof( szSeconds ), "|#1%ld |#9%s", s, (s > 1) ? "seconds" : "second" );
    }

    if (h == 0)
    {
        if (m == 0)
        {
            if (s == 0)
            {
				result = " ";
            }
            else
            {
				result = szSeconds;
            }
        }
        else
        {
			result = szMinutes;
            if (s != 0)
            {
				result += ", ";
				result += szSeconds;
            }
        }
    }
    else
    {
		result = szHours;
        if (m == 0)
        {
            if (s != 0)
            {
				result += ", ";
                result += szSeconds;
            }
        }
        else
        {
			result += ", ";
			result += szMinutes;
            if (s != 0)
            {
				result += ", ";
				result += szSeconds;
            }
        }
    }
	strcpy( ch2, result.c_str() );
    return ch2;
}




/* This should not be a problem 'till 2005 or so */
int years_old( int nMonth, int nDay, int nYear )
{
    time_t t = time( NULL );
    struct tm * pTm = localtime( &t );

    if ( pTm->tm_year < nYear )
    {
        return 0;
    }
    if ( pTm->tm_year == nYear )
    {
        if ( pTm->tm_mon < nMonth )
        {
            return 0;
        }
        if ( pTm->tm_mon == nMonth )
        {
            if ( pTm->tm_mday < nDay )
            {
                return 0;
            }
        }
    }
    int nAge = pTm->tm_year - nYear;
    if ( pTm->tm_mon < nMonth )
    {
        --nAge;
    }
    else if ( ( pTm->tm_mon == nMonth ) && ( pTm->tm_mday < nDay ) )
    {
        --nAge;
    }
    return nAge;
}


