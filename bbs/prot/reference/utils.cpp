#ifndef	lint
static const char rcsid[] = "$Id$" ;
#endif

/*
 * Copyright (c) 1995 by Edward A. Falk
 */


/**********
 *
 *
 *	@   @  @@@@@   @@@   @       @@@@  
 *	@   @    @      @    @      @      
 *	@   @    @      @    @       @@@   
 *	@   @    @      @    @          @  
 *	 @@@     @     @@@   @@@@@  @@@@   
 *
 *	UTILS - utilities used by xmodem library.
 *
 *	Routines provided here:
 *
 *
 *	name (args)
 *		Brief description.
 *
 *	int sendCancel()
 *
 *		send cancel string <CAN><CAN>
 *
 *
 *	int sendFlush(c)
 *		char	c ;
 *
 *		flush input, send one character, return nonzero on error
 *
 *
 *	int sendChr(c)
 *		char	c ;
 *
 *		send one character, return nonzero on error
 *
 *
 *	int sendStr(str, len)
 *		char	*str ;
 *		int	len ;
 *
 *		send string, return nonzero on error
 *
 *
 *	int calcChecksum(ptr, count)
 *		char	*ptr ;
 *		int	count ;
 *
 *		compute checksum (used by xmodem)
 *
 *
 *	Edward A. Falk
 *
 *	January, 1995
 *
 *
 *
 **********/




//#include <sys/termios.h>
//#include <sys/ioctl.h>
#include "xmodem.h"


int
sendCancel()
{
	return sendFlush(CAN) || sendFlush(CAN) ;
}


	/* send one character, return nonzero on error */
int
sendFlush(char c)
{
	/* first, flush input port */
	/* TODO: caller provide a way to do this? */

	if( xmRfd == -1 )
	  return XmErrNotOpen ;

	/* TODO: caller provides flush */
	if( ioctl(xmRfd, TCFLSH, TCIFLUSH) == -1 )
	  return XmErrSys ;

	return sendChr(c) ;
}


	/* send one character, return nonzero on error */

int
sendChr(char c)
{
	/* TODO: caller provide character output func? */
	if( xmTfd == -1 )
	  return XmErrNotOpen ;

	return write(xmTfd, &c, 1) ==1 ? 0 : XmErrSys ;
}


	/* send multiple characters, return nonzero on error */

int
sendStr(char *str, int len)
{
	/* TODO: caller provide character output func? */
	if( xmTfd == -1 )
	  return XmErrNotOpen ;

	return write(xmTfd, str, len) == len ? 0 : XmErrSys ;
}

	/* compute checksum */

int
calcChecksum(char *ptr, int count)
{
	register int csum = 0 ;
	while( --count >= 0 )
	  csum += (u_char) *ptr++ ;
	return csum & 255 ;
}
