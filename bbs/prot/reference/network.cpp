#ifndef	lint
static const char rcsid[] = "$Id$" ;
#endif

/*
 * Copyright (c) 1995 by Edward A. Falk
 */


/**********
 *
 *
 *	@   @  @@@@@  @@@@@  @   @   @@@   @@@@   @   @  
 *	@@  @  @        @    @   @  @   @  @   @  @  @   
 *	@ @ @  @@@      @    @ @ @  @   @  @@@@   @@@    
 *	@  @@  @        @    @ @ @  @   @  @  @   @  @   
 *	@   @  @@@@@    @     @ @    @@@   @   @  @   @  
 *
 *	NETWORK - Make network connection
 *
 *	Routines provided here:
 *
 *
 *	void
 *	IpConnect()
 *		Make IP connection according to parameters in gcomm.h
 *
 *	void
 *	IpDisConnect()
 *		Close IP connection.
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
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <varargs.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef	COMMENT
#include <sys/ttold.h>
#include <sys/termio.h>
#endif	/* COMMENT */
#include <termio.h>
#include <sys/stat.h>

#include <netdb.h>		/* stuff used by tcp/ip */
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/wait.h>
#include <sys/signal.h>

#include "gcomm.h"

extern	int	errno ;
#ifdef	COMMENT
extern	char	*sys_errlist[] ;
#endif	/* COMMENT */



void
IpConnect()
{
	int	sockfd ;
	int	i,j ;
	struct hostent *hostent ;
	struct sockaddr_in sockaddr ;
	int	port ;

	WindowStatus("Connecting...") ;

	port = PortLookup(hostPort) ;
	if( port == -1 )
	  return ;

	bzero(&sockaddr, sizeof(sockaddr)) ;
	sockaddr.sin_family = AF_INET ;
	sockaddr.sin_port = htons(port) ;

	hostent = gethostbyname(hostId) ;
	if( hostent != NULL )
	{
	  sockaddr.sin_addr = *(struct in_addr *) hostent->h_addr ;
	}
	else if( isdigit(hostId[0]) )
	{
	  i = sscanf(hostId, "%d.%d.%d.%d",
		&sockaddr.sin_addr.S_un.S_un_b.s_b1,
		&sockaddr.sin_addr.S_un.S_un_b.s_b2,
		&sockaddr.sin_addr.S_un.S_un_b.s_b3,
		&sockaddr.sin_addr.S_un.S_un_b.s_b4) ;
	  if( i != 4 ) {
	    WindowStatus("Host address must be in dd.dd.dd.dd format") ;
	    return ;
	  }
	}
	else {
	  WindowStatus("Hostname not found") ;
	  return ;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0) ;
	if( sockfd < 0 ) {
	  WindowStatus("Cannot open socket: %s", sys_errlist[errno]) ;
	  return ;
	}

	i = connect(sockfd, &sockaddr, sizeof(sockaddr)) ;
	if( i < 0 ) {
	  WindowStatus("Cannot connect to host: %s", sys_errlist[errno]) ;
	  close(sockfd) ;
	  return ;
	}

	ifd = ofd = sockfd ;

	i = fcntl(ifd, F_GETFL, 0) ;
	j = fcntl(ifd, F_SETFL, i|O_NDELAY) ;

	connectionActive = 1 ;
	connectTime0 = time(NULL) ;
	WindowStatus("Connected") ;
}


void
IpDisConnect()
{
	if( ifd == -1 )
	  return ;

	close(ifd) ;
	ifd = ofd = -1 ;

	connectionActive = 0 ;
}
