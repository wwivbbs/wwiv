#ifndef	lint
static const char rcsid[] = "$Id$" ;
#endif

/* seriallog routines.  Write data to log file, tagged with timestamp
 * and channel.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>

#include "seriallog.h"

static	struct timeval	timestamp = {0,0} ;

#define	DTIME	1500		/* timeout, milliseconds */

static	int	which = -1 ;	/* last transmitted */

static	char	obuf[4096] ;
static	int	olen = -1 ;

	FILE	*SerialLogFile = NULL ;

	/* flush out buffered text */

void
SerialLogFlush()
{
	int	i ;

	if( SerialLogFile == NULL )
	  return ;

	if( which != -1 && olen > 0 )
	{
	  i = fwrite((char *)&which, sizeof(int), 1, SerialLogFile) ;
	  i = fwrite((char *)&timestamp, sizeof(timestamp), 1, SerialLogFile) ;
	  i = fwrite((char *)&olen, sizeof(int), 1, SerialLogFile) ;
	  i = fwrite(obuf, 1, olen, SerialLogFile) ;
	}
	fflush(SerialLogFile) ;

	which = -1 ;
	olen = 0 ;
}


void
SerialLog(const void *data, int len, int w)
{
	struct timezone	z ;
	struct timeval	time ;
	int	dt ;
	int	i ;

	if( SerialLogFile == NULL )
	  return ;

	gettimeofday(&time, &z) ;
	dt = (time.tv_sec - timestamp.tv_sec)*1000 +
	     (time.tv_usec - timestamp.tv_usec)/1000 ;

	if( w != which || dt > DTIME ) {
	  SerialLogFlush() ;
	  which = w ;
	  timestamp = time ;
	}

	while( len > 0 )
	{
	  if( olen+len > sizeof(obuf) )
	    SerialLogFlush() ;

	  i = len > sizeof(obuf) ? sizeof(obuf) : len ;
	  bcopy(data, obuf+olen, i) ;
	  olen += i ;
	  len -= i ;
	}
}
