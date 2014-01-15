#ifndef lint
static const char rcsid[] = "$Id$" ;
#endif lint

/*
 * Copyright (c) 1995 by Edward A. Falk
 */


/**********
 *
 *
 *	@   @  @   @   @@@   @@@@   @@@@@  @   @  @@@@
 *	 @ @   @@ @@  @   @  @   @  @      @@ @@  @   @
 *	  @    @ @ @  @   @  @   @  @@@    @ @ @  @@@@
 *	 @ @   @ @ @  @   @  @   @  @      @ @ @  @  @
 *	@   @  @ @ @   @@@   @@@@   @@@@@  @ @ @  @   @
 *
 *	XMODEMR - receiver side of xmodem/ymodem protocol
 *
 *	Caller sets flags defined in xmodem.h as appropriate.
 *	(default is basic xmodem)
 *
 *	This code is designed to be called from inside a larger
 *	program, so it is implemented as a state machine where
 *	practical.
 *
 *
 *	functions:
 *
 *	XmodemRInit(char *filename, Protocol p)
 *		Initiate a receive
 *
 *	XmodemRTimeout()
 *		called after timeout expired
 *
 *	XmodemRRcv(char c)
 *		called after character received
 *
 *	XmodemRAbort()
 *		abort transfer
 *
 *	all functions return 0 on success, 1 on abort
 *
 *
 *
 *	Edward A. Falk
 *
 *	January, 1995
 *
 *
 *
 **********/




#include <stdio.h>
#include <sys/types.h>
#include "xmodem.h"


/* TODO: WXmodem */


bool	xmodem1k = False ;
Protocol protocol = Xmodem ;
int	xmTfd = -1 ;
int	xmRfd = -1 ;

int	xmTimeout = 0 ;

char	xmDefPath[MAXPATHLEN] ;
char	xmFilename[MAXPATHLEN] ;

typedef	enum {
	Start,	/* waiting to begin */
	Init,		/* sent initial NAK, 'C' or 'W' */
	Packet,	/* receiving a packet */
	Wait,		/* wait for start of next packet */
} XmodemState ;

static	bool		ymodem ;
static	XmodemState	state = Start ;
static	int		errorCount = 0 ;
static	int		errorCount2 ;
static	int		ignoreCount ;
static	int		eotCount ;	/* count EOT's, reject first one */
static	int		inCount ;	/* characters received this packet */
static	int		pktLen ;	/* length of this packet data */
static	int		pktHdrLen ;	/* id, cmpl, checksum or crc */
static	char		packet[MAXPACKET+5], *optr ;
static	int		packetId ;	/* id of last received packet */
static	int		packetCount ;	/* # packets received */

static	FILE		*ofile ;	/* output file fd */
static	int		fileLen, fileDate, fileMode ;

static	int	XmodemRStart() ;
static	int	processPacket() ;
static	int	rejectPacket() ;
static	int	acceptPacket() ;


int
XmodemRInit( char *file, Protocol prot ) {
	int	err ;

	state = Start ;
	protocol = prot ;
	ymodem = prot == Ymodem || prot == YmodemG ;

	if( ymodem )
		strcpy(xmDefPath, file) ;
	else
		strcpy(xmFilename, file) ;

	eotCount = errorCount = errorCount2 = 0 ;

	if( err=XmodemRStart() )
		return err ;

	state = Init ;
	packetId = ymodem ? 255 : 0 ;
	packetCount = 0 ;

	pktHdrLen = protocol == Xmodem ? 3 : 4 ;

	return 0 ;
}


/* send startup character */

static	int
XmodemRStart() {
	static	char	pchars[5] = {NAK,'C','W','C','C'} ;
	static	int	timeouts[5] = {INITTO, INITTO2, INITTO2, INITTO, INITTO} ;
	char	c = pchars[(int)protocol] ;
	int	err ;

	if( err=sendFlush(c) )
		return err ;

	xmTimeout = timeouts[(int)protocol] ;

	return 0 ;
}


int
XmodemRRcv(char c) {
	errorCount = 0 ;

	switch( state ) {
	case Start:		/* shouldn't happen, ignore */
		if( c == CAN )
			return XmErrCancel ;
		break ;

	case Init:		/* waiting */
	case Wait:
		switch( c ) {
		case SOH:
		case STX:
			pktLen = c == STX ? 1024 : 128 ;
			inCount = 0 ;
			optr = packet ;
			state = Packet ;
			xmTimeout = PKTTO ;
			break ;

		case EOT:
			if( ++eotCount > 1 ) {
				sendFlush(ACK) ;
				if( ymodem )
					return XmodemRInit() ;	/* restart protocol */
				else
					return XmDone ;
			} else
				return rejectPacket() ;	/* make xmitter try again */

		case CAN:
			return XmErrCancel ;

		default:		/* ignore all others */
			if( ++ignoreCount > 1030 ) {
				ignoreCount = 0 ;
				return sendFlush(NAK) ;
			}
			break ;
		}
		break ;


	case Packet:		/* mid packet */
		*optr++ = c ;
		if( ++inCount >= pktLen + pktHdrLen )
			ProcessPacket() ;
		break ;
	}
	return 0 ;
}


int
XmodemRTimeout() {
	if( ++errorCount > MAXERROR )
		return state == Init ? XmErrInitTo : XmErrRcvTo ;

	switch( state ) {
	case Start:
		return -1 ;		/* shouldn't happen */
	case Init:
		if( ++errorCount2 >= 3 )
			switch( protocol ) {
			case WXmodem:
				protocol = XmodemCrc ;
				errorCount2 = 0 ;
				break ;
			case XmodemCrc:
				protocol = Xmodem ;
				pktHdrLen = 3 ;
				break ;
			}
		return XmodemRStart() ;

	case Wait:			/* timeout while waiting */
	case Packet:			/* timeout in mid packet */
		return rejectPacket() ;
	}
}

int
XmodemRAbort() {
	return sendCancel() ;
}


static	int
ProcessPacket() {
	int	id = (u_char)packet[0] ;
	int	idc = (u_char)packet[1] ;
	int	i ;

	if( idc != 255-id )
		return rejectPacket() ;

	if( id == packetId )		/* duplicate */
		return acceptPacket() ;

	if( id != (packetId+1)%256 ) {	/* out of sequence */
		(void) sendCancel() ;
		return XmErrSequence ;
	}

	if( protocol == Xmodem ) {
		/* compute checksum */
		register int csum = calcChecksum(packet+2, pktLen) ;
		if( csum != (u_char) packet[2+pktLen] )
			return rejectPacket() ;
	} else {
		int crc0 = (u_char)packet[pktLen+2] << 8 | (u_char)packet[pktLen+3] ;
		int crc1 = calcrc(packet+2, pktLen) ;
		if( crc0 != crc1 )
			return rejectPacket() ;
	}

	/* it's a good packet */
	packetId = (packetId+1)%256 ;


	/* is this the first packet? */

	if( packetCount == 0 ) {
		if( ymodem ) {
			if( packet[2] == '\0' ) {	/* last file */
				(void) acceptPacket() ;
				return XmDone ;
			}

			if( packet[2] == '/' )
				strcpy(xmFilename, packet+2) ;
			else {
				strcpy(xmFilename, xmDefPath) ;
				strcat(xmFilename, packet+2) ;
			}

			fileLen = fileDate = fileMode = -1 ;
			sscanf(packet+2+strlen(packet)+1, "%d %o %o",
			       &fileLen, &fileDate, &fileMode) ;
		}

		if( (ofile = fopen(xmFilename, "w")) == NULL ) {
			sendCancel() ;
			return XmErrCantOpen ;
		}

		if( ymodem ) {
			packetCount = 1 ;
			(void) acceptPacket() ;
			return sendFlush('C') ;
		} else
			state = Packet ;
	}

	++packetCount ;

	/* TODO: ymodem: if this is last packet, truncate it */
	if( (i=fwrite(packet+2, 1, pktLen, ofile)) != pktLen ) {
		sendCancel() ;
		return XmErrSys ;
	} else
		return acceptPacket() ;
}


static	int
rejectPacket() {
	state = Wait ;
	xmTimeout = INITTO ;
	return sendFlush(NAK) ;
}

static	int
acceptPacket() {
	state = Wait ;
	xmTimeout = INITTO ;
	return sendFlush(ACK) ;
}



#ifdef	COMMENT		/* stand-alone testing */

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/termios.h>


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


	xmodem1k = 0 ;
	done = XmodemRInit("foo", XmodemCrc) != 0 ;
#ifdef	COMMENT
	xmodem1k = 1 ;
	done = XmodemRInit("./", Ymodem) != 0 ;
#endif	/* COMMENT */

	while(!done) {
		FD_ZERO(&readfds) ;
		FD_SET(xmTfd, &readfds) ;
		timeout.tv_sec = xmTimeout ;
		timeout.tv_usec = 0 ;
		i = select(xmTfd+1, &readfds,NULL,NULL, &timeout) ;
		if( i<0 )
			perror("select") ;
		else if( i==0 )
			done = XmodemRTimeout() != 0 ;
		else {
			len = read(xmTfd, buffer, sizeof(buffer)) ;
			for(i=0; !done && i<len; ++i)
				done = XmodemRRcv(buffer[i]) != 0 ;
		}
	}




	tcsetattr(xmTfd,TCSADRAIN, &old_settings) ;
	exit(0) ;
}
#endif	/* COMMENT */
