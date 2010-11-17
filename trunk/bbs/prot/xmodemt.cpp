#ifndef	lint
static const char rcsid[] = "$Id$" ;
#endif

/*
 * Copyright (c) 1995 by Edward A. Falk
 */


/**********
 *
 *
 *	@   @  @   @   @@@   @@@@   @@@@@  @   @  @@@@@  
 *	 @ @   @@ @@  @   @  @   @  @      @@ @@    @    
 *	  @    @ @ @  @   @  @   @  @@@    @ @ @    @    
 *	 @ @   @ @ @  @   @  @   @  @      @ @ @    @    
 *	@   @  @ @ @   @@@   @@@@   @@@@@  @ @ @    @    
 *
 *	XMODEMT - transmit side of xmodem protocol
 *
 * Caller sets flags defined in xmodem.h as appropriate.
 * (default is basic xmodem)
 *
 * This code is designed to be called from inside a larger
 * program, so it is implemented as a state machine where
 * practical.
 *
 * functions:
 *
 *	XmodemTInit(char *filename, Protocol p)
 *		Initiate a receive
 *
 *	XmodemTTimeout()
 *		called after timeout expired
 *
 *	XmodemTRcv(char c)
 *		called after character received
 *
 *	XmodemTFinish()
 *		last file (ymodem)
 *
 *	XmodemTAbort()
 *		abort transfer
 *
 * all functions return 0 on success, 1 on abort
 *
 *	Edward A. Falk
 *
 *	January, 1995
 *
 *
 *
 **********/


#include <cstring>
#include <stdio.h>
#include <sys/types.h>
#include "xmodem.h"


	bool	fileInfo = False ;

	/* TODO: WXmodem, YmodemG */

typedef	enum {
	  File,		/* waiting for initial protocol character */
	  FileWait,	/* sent file header, waiting for ACK */
	  Start,	/* waiting to begin */
	  Wait,		/* sent a packet, waiting for ACK */
	  Eot,		/* sent an EOT, waiting for ACK */
	  EndWait,	/* sent null filename, waiting for ACK */
	} XmodemState ;

static	XmodemState	state = Start ;
static	bool		ymodem ;
static	bool		useCrc ;	/* receiver wants crc */
static	int		pktLen ;	/* length of this packet data */
static	int		pktMaxLen ;
static	char		packet[MAXPACKET] ;
static	char		pktHdr[3], pktCrc[2] ;
static	int		packetId ;	/* id of last received packet */
static	int		packetCount ;	/* # packets received */

static	FILE		*ifile = NULL ;	/* input file fd */
static	int		fileLen, fileDate, fileMode ;

static	int	sendFilename() ;
static	int	sendPacket() ;
static	int	buildPacket() ;
static	int	resendPacket() ;


int
XmodemTInit( char *file, Protocol prot )
{
	int	err ;

	protocol = prot ;
	ymodem = prot == Ymodem || prot == YmodemG ;
	state = ymodem ? File : Start ;

	strcpy(xmFilename, file) ;

	if( (ifile = fopen(xmFilename, "r")) == NULL ) {
	  sendCancel() ;
	  return XmErrCantOpen ;
	}

	packetId = 1 ;
	packetCount = 0 ;
	pktMaxLen = xmodem1k ? 1024 : 128 ;

	xmTimeout = 60 ;

	return 0 ;
}


int
XmodemTRcv(char c)
{
	if( c == CAN ) {
	  if( ifile != NULL )
	    fclose(ifile) ;
	  return XmErrCancel ;
	}

	switch( state ) {
	  case File:		/* waiting for command, ymodem */
	    switch( c ) {
	      case NAK: useCrc = False ; return sendFilename() ;
	      case 'C': useCrc = True ; return sendFilename() ;
	    }
	    break ;

	  case FileWait:	/* waiting for filename ACK */
	    switch( c ) {
	      case NAK: return resendPacket() ;
	      case ACK: state = Start ; return 0 ;
	    }

	  case Start:		/* waiting for command, data */
	    switch( c ) {
	      case NAK:		/* wants checksums */
		if( !ymodem )
		  protocol = Xmodem ;
		useCrc = False ;
		return sendPacket() ;

	      case 'C': useCrc = True ; return sendPacket() ;

	      case 'W':
		if( !ymodem ) {
		  protocol = WXmodem ;
		  useCrc = True ;
		  /* TODO: WXmodem */
		}
	    }
	    break ;


	  case Wait:		/* waiting for ACK */
	    switch( c ) {
	      case ACK: return sendPacket() ;
	      case NAK: return resendPacket() ;
	    }
	    break ;		/* ignore all other characters */

	  case Eot:		/* waiting for ACK after EOT */
	    switch( c ) {
	      case ACK: return XmDone ;
	      case NAK: return sendChr(EOT) ;
	    }
	    break ;

	  case EndWait:		/* waiting for filename ACK */
	    switch( c ) {
	      case NAK: return resendPacket() ;
	      case ACK: return XmDone ;
	    }
	}
	return 0 ;
}

static	int
sendFilename()
{
	int	i ;
	char	*ptr ;

	pktLen = 128 ;

	/* TODO: protect against long filenames */
	strcpy(packet, xmFilename) ;
	ptr = packet + strlen(packet) + 1 ;
	/* TODO: get file info */
	if( fileInfo ) {
	  sprintf(ptr, "%d %o %o %o", 0,0,0100644) ;
	  ptr += strlen(ptr) + 1 ;
	}

	/* TODO: what if file desc buffer too big? */

	if( ptr > packet+128 )
	  pktLen = 1024 ;

	i = pktLen - (ptr-packet) ;
	bzero(ptr, i) ;

	state = FileWait ;
	packetId = 0 ;
	return buildPacket() ;
}


int
XmodemTFinish()
{
	int	i ;
	char	*ptr ;

	pktLen = 128 ;
	bzero(packet, pktLen) ;

	state = EndWait ;
	packetId = 0 ;
	return buildPacket() ;
}


static	char	*bufptr ;
static	int	buflen = 0 ;

static	int
sendPacket()
{
	int	i ;

	/* This code assumes that a incomplete reads can only happen
	 * after EOF.  This will fail with pipes.
	 * TODO: try to make pipes work.
	 */

	state = Wait ;

	if( buflen > 0 )		/* previous incomplete packet */
	{
	  memcpy(packet, bufptr, 128) ;
	  bufptr += 128 ;
	  if( buflen < 128 )
	    for(i=buflen; i<128; ++i)
	      packet[i] = 0x1a ;
	  buflen -= 128 ;
	  pktLen = 128 ;
	  return buildPacket() ;
	}

	if( (i=fread(packet, 1,pktMaxLen, ifile)) <= 0 )	/* EOF */
	{
	  state = Eot ;
	  return sendChr(EOT) ;
	}

	else if( i == pktMaxLen )		/* full buffer */
	{
	  pktLen = i ;
	  return buildPacket() ;
	}

	buflen = i ;
	bufptr = packet ;
	pktLen = 128 ;
	return buildPacket() ;
}

static	int
buildPacket()
{
	int	i ;

	pktHdr[0] = pktLen == 128 ? SOH : STX ;
	pktHdr[1] = (char)packetId ;
	pktHdr[2] = (char)(255-packetId) ;
	++packetId ;

	if( useCrc ) {
	  i = calcrc(packet, pktLen) ;
	  pktCrc[0] = (char) (i>>8) ;
	  pktCrc[1] = (char) (i & 0xff) ;
	}
	else
	  pktCrc[0] = (char) calcChecksum(packet, pktLen) ;

	return resendPacket() ;
}


static	int
resendPacket()
{
	int	i ;

	(i=sendStr(pktHdr, 3)) || (i=sendStr(packet, pktLen)) ||
		(i=sendStr(pktCrc, useCrc?2:1)) ;
	return i ;
}


int
XmodemTTimeout()
{
	switch( state ) {
	  case File:
	  case Start:
	    return XmErrInitTo ;
	  case FileWait:
	  case Wait:
	  case Eot:
	    return XmErrRcvTo ;
	}
}

int
XmodemTAbort()
{
	  return sendCancel() ;
}



//#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
//#include <sys/time.h>
//#include <sys/termios.h>


main(argc,argv)
	int	argc ;
	char	**argv ;
{
	struct	termios	old_settings, new_settings ;
	fd_set	readfds ;
	struct timeval timeout ;
	int	i ;
	int	len ;
	char	buffer[1024] ;
	bool	done = False ;
	int	filecount = 0 ;

	if( argc < 2 )
	  exit(2) ;

	xmTfd = xmRfd = open(argv[1], O_RDWR) ;

	if( xmTfd == -1 )
	  exit(1) ;

	tcgetattr(xmTfd,&old_settings) ;
	new_settings = old_settings ;

	new_settings.c_iflag &=
	  ~(ISTRIP|INLCR|IGNCR|ICRNL|IUCLC|IXON|IXOFF|IMAXBEL) ;
	new_settings.c_oflag = 0 ;
	new_settings.c_cflag = B300|CS8|CREAD|CLOCAL ;
	new_settings.c_lflag = 0 ;
	new_settings.c_cc[VMIN] = 32 ;
	new_settings.c_cc[VTIME] = 1 ;
	tcsetattr(xmTfd,TCSADRAIN, &new_settings) ;


	xmodem1k = 1 ;
	done = XmodemTInit("xmodem.h", Ymodem) != 0 ;

	while(!done)
	{
	  FD_ZERO(&readfds) ;
	  FD_SET(xmTfd, &readfds) ;
	  timeout.tv_sec = xmTimeout ;
	  timeout.tv_usec = 0 ;
	  i = select(xmTfd+1, &readfds,NULL,NULL, &timeout) ;
	  if( i<0 )
	    perror("select") ;
	  else if( i==0 )
	    done = XmodemTTimeout() != 0 ;
	  else {
	    len = read(xmTfd, buffer, sizeof(buffer)) ;
	    for(i=0; !done && i<len; ++i)
	      done = XmodemTRcv(buffer[i]) != 0 ;
	  }
	  if( done ) {
	    switch( ++filecount ) {
	      case 1:
		done = XmodemTInit("crc.c", Ymodem) != 0 ;
		break ;
	      case 2:
		done = XmodemTFinish() ;
		break ;
	      case 3: break ;
	    }
	  }
	}




	tcsetattr(xmTfd,TCSADRAIN, &old_settings) ;
	exit(0) ;
}
