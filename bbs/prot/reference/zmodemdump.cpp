#ifndef	lint
static const char rcsid[] = "$Id$" ;
#endif


	/* variation on serialmon companion program serialdump, which
	 * interprets data as a zmodem dialog
	 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
//#include <sys/termios.h>
#include <sys/types.h>
//#include <sys/time.h>

#include <assert.h>

#include "crctab.h"

#define	MAXBUF	2048
#define	MAXLINE	16
#define	MAXHBUF	128
#define	OLINELEN (MAXLINE*2)

#define	CAN	030
#define	XON	021

#define	ZDLE	030
#define	ZPAD	'*'
#define	ZBIN	'A'
#define	ZHEX	'B'
#define	ZBIN32	'C'
#define	ZBINR32	'D'
#define	ZVBIN	'a'
#define	ZVHEX	'b'
#define	ZVBIN32	'c'
#define	ZVBINR32 'd'
#define	ZRESC	0177


#define	ZRQINIT	0	/* request receive init */
#define	ZRINIT	1	/* receive init */
#define	ZSINIT	2	/* send init sequence, define Attn */
#define	ZACK	3	/* ACK */
#define	ZFILE	4	/* file name, from sender */
#define	ZSKIP	5	/* skip file command, from receiver */
#define	ZNAK	6	/* last packet was garbled */
#define	ZABORT	7	/* abort */
#define	ZFIN	8	/* finish session */
#define	ZRPOS	9	/* resume file from this position, from receiver */
#define	ZDATA	10	/* data packets to follow, from sender */
#define	ZEOF	11	/* end of file, from sender */
#define	ZFERR	12	/* fatal i/o error, from receiver */
#define	ZCRC	13	/* request for file crc, from receiver */
#define	ZCHALLENGE 14	/* "send this number back to me", from receiver */
#define	ZCOMPL	15	/* request is complete */
#define	ZCAN	16	/* other end cancelled with CAN-CAN-CAN-CAN-CAN */
#define	ZFREECNT 17	/* request for free bytes on filesystem */
#define	ZCOMMAND 18	/* command, from sending program */
#define	ZSTDERR	19	/* output this message to stderr */

#define	ZCRCE	'h'
#define	ZCRCG	'i'
#define	ZCRCQ	'j'
#define	ZCRCW	'k'
#define	ZRUB0	'l'
#define	ZRUB1	'm'

typedef	enum {Idle, Padding, HexHeader, Header16, Header32,
		InData, InCrc} ZState ;

typedef	struct {
	  ZState	state ;
	  int		headertype ;
	  int		data[4] ;
	  int		crcBytes[4] ;
	  int		count ;
	  int		zdlePend ;	/* ZDLE received */
	  int		crcCmd ;
	  int		crclen ;
	  u_long	crc ;
	} Zinfo ;

	u_char	buffer[MAXBUF] ;
	u_char	line[MAXLINE] ;
	u_char	hbuffer[MAXHBUF] ;
	int	linecnt = 0 ;
	int	hbufcnt = 0 ;


main( int argc, char **argv )
{
	int	i,j ;
	int	len ;
	int	which ;
	struct timeval	timestamp, oldtime ;
	struct tm *tm ;

	Zinfo	Ainfo, Binfo ;

	printf("serial log.  'A' is application, 'B' is serial port\n\n") ;

	oldtime.tv_sec = 0 ;

	Ainfo.state = Binfo.state = Idle ;
	Ainfo.zdlePend = Binfo.zdlePend = 0 ;

	while( (i=fread((char *)&which, sizeof(which), 1, stdin)) > 0 )
	{
	  i = fread((char *)&timestamp, sizeof(timestamp), 1, stdin) ;
	  i = fread((char *)&len, sizeof(len), 1, stdin) ;
	  if( timestamp.tv_sec != oldtime.tv_sec  ||
	      timestamp.tv_usec != oldtime.tv_usec )
	  {
	    if( linecnt > 0 )
	      dumpLine() ;

	    tm = localtime(&timestamp.tv_sec) ;
	    printf("%c: %2d:%2.2d:%2.2d.%2.2d\n", 'A'+which,
	      tm->tm_hour,tm->tm_min,tm->tm_sec, timestamp.tv_usec/10000 ) ;

	    oldtime = timestamp ;
	  }


	  while( len > 0 )
	  {
	    i = MAXBUF ;
	    if( len < i ) i = len ;
	    j = fread(buffer, 1, i, stdin) ;
assert(j <= MAXBUF) ;
	    len -= j ;
	    parseData(which ? &Binfo : &Ainfo, j) ;
	  }

	  while( len > 0 )
	  {
	    i = MAXLINE - linecnt ;
	    if( len < i ) i = len ;
assert(linecnt+i <= MAXLINE) ;
	    j = fread(line+linecnt, 1, i, stdin) ;
assert(linecnt+j <= MAXLINE) ;
	    len -= j ;
	    linecnt += j ;
	    if( linecnt >= MAXLINE )
	      dumpLine() ;
	  }
	}
	if( linecnt > 0 )
	  dumpLine() ;

	exit(0) ;
}

char
toprintable(char c)
{
	c &= 0177 ;
	if( c >= 0x20 && c <= 0x7e )
	  return c ;
	else
	  return '.' ;
}

dumpLine()
{
	int	i,j ;

	if( linecnt <= 0 )
	  return ;

	if( linecnt > MAXLINE ) linecnt = MAXLINE ;
	printf("     ") ;
	for(i=0; i<linecnt; ++i) {
assert(i <= MAXLINE) ;
	  printf("%2.2x ", line[i]) ;
	}
	for(; i<MAXLINE; ++i)
	  printf("   ") ;
	printf("\t|") ;
	for(i=0; i<linecnt; ++i) {
assert(i <= MAXLINE) ;
	  printf("%c",toprintable(line[i])) ;
	}
	printf("|\n") ;

	linecnt = 0 ;
}


static	u_char	hexHeaderStart[] = {ZPAD,ZPAD,ZDLE,ZHEX} ;
static	u_char	header16Start[] = {ZPAD,ZDLE,ZBIN} ;
static	u_char	header32Start[] = {ZPAD,ZDLE,ZBIN32} ;

parseData( Zinfo *info, int len )
{
	u_char	*ptr, c ;
	int	idx ;

	for(ptr = buffer; --len >= 0; ptr++) {
assert(ptr >= buffer && ptr < buffer+MAXBUF) ;
	  c = *ptr ;
	  if( c != XON )
	    switch( info->state ) {
	      case Idle:
		if( c != ZPAD )
		  dataChar(c) ;
		else {
		  info->state = Padding ;
		  info->count = 1 ;
		  hbuffer[0] = c ;
		}
		break ;

	      case Padding:
		if( c == ZDLE ) {
		  info->zdlePend = 1 ;
		}
		else if( info->zdlePend ) {
		  info->zdlePend = 0 ;
		  info->count = 0 ;
		  switch(c) {
		    case ZHEX:
		      info->state = HexHeader ;
		      info->crclen=2 ;
		      info->crc = 0 ;
		      break ;
		    case ZBIN:
		      info->state = Header16 ;
		      info->crclen=2 ;
		      info->crc = 0 ;
		      break ;
		    case ZBIN32:
		      info->state = Header32 ;
		      info->crclen=4 ;
		      info->crc = 0xffffffff ;
		      break ;
		    default:
		      cancelHeader(info) ; break ;
		  }
		}
		else if( c == ZPAD )
		{
		  if( info->count < 2 ) {
assert(info->count < MAXHBUF) ;
		    hbuffer[info->count++] = c ;
		  }
		  else
		    dataChar(c) ;
		}
		else
		  cancelHeader(info) ; break ;
		break ;

	      case HexHeader:
		if( c == ZDLE && !info->zdlePend ) {
		  info->zdlePend = 1 ;
		  break ;
		}

		if( info->zdlePend ) {
		  c = zdle(c) ;
		  info->zdlePend = 0 ;
		}

		idx = info->count++ ;
assert(idx < MAXHBUF) ;
		hbuffer[idx] = c ;
		if( info->count >= 16 ) {		/* end of header */
		  info->headertype = hex2(hbuffer+0) ;
		  info->data[0] = hex2(hbuffer+2) ;
		  info->data[1] = hex2(hbuffer+4) ;
		  info->data[2] = hex2(hbuffer+6) ;
		  info->data[3] = hex2(hbuffer+8) ;
		  info->crcBytes[0] = hex2(hbuffer+10) ;
		  info->crcBytes[1] = hex2(hbuffer+12) ;
		  displayHeader(info) ;
		}
		break ;

	      case Header16:
		if( c == ZDLE && !info->zdlePend ) {
		  info->zdlePend = 1 ;
		  break ;
		}

		if( info->zdlePend ) {
		  c = zdle(c) ;
		  info->zdlePend = 0 ;
		}

		idx = info->count++ ;
assert(idx < MAXHBUF) ;
		hbuffer[idx] = c ;
		if( info->count >= 7 ) {		/* end of header */
		  info->headertype = hbuffer[0] ;
		  info->data[0] = hbuffer[1] ;
		  info->data[1] = hbuffer[2] ;
		  info->data[2] = hbuffer[3] ;
		  info->data[3] = hbuffer[4] ;
		  info->crcBytes[0] = hbuffer[5] ;
		  info->crcBytes[1] = hbuffer[6] ;
		  displayHeader(info) ;
		}
		break ;

	      case Header32:
		if( c == ZDLE && !info->zdlePend ) {
		  info->zdlePend = 1 ;
		  break ;
		}

		if( info->zdlePend ) {
		  c = zdle(c) ;
		  info->zdlePend = 0 ;
		}

		idx = info->count++ ;
assert(idx < MAXHBUF) ;
		hbuffer[idx] = c ;
		if( info->count >= 9 ) {		/* end of header */
		  info->headertype = hbuffer[0] ;
		  info->data[0] = hbuffer[1] ;
		  info->data[1] = hbuffer[2] ;
		  info->data[2] = hbuffer[3] ;
		  info->data[3] = hbuffer[4] ;
		  info->crcBytes[0] = hbuffer[5] ;
		  info->crcBytes[1] = hbuffer[6] ;
		  info->crcBytes[2] = hbuffer[7] ;
		  info->crcBytes[3] = hbuffer[8] ;
		  displayHeader(info) ;
		}
		break ;

	      case InData:
		if( info->zdlePend )
		{
		  info->zdlePend = 0 ;
		  switch( c ) {
		    case ZCRCE:
		    case ZCRCW:
		    case ZCRCG:
		    case ZCRCQ:
		      info->crcCmd = c ;
		      dumpLine() ;
		      info->state = InCrc ;
		      info->count = 0 ;
		      break ;
		    default:
		      c = zdle(c) ;
		      dataChar(c) ;
		      break ;
		  }
		}
		else if( c == ZDLE )
		  info->zdlePend = 1 ;
		else
		  dataChar(c) ;
		break ;

	      case InCrc:
		if( info->zdlePend ) {
		  c = zdle(c) ;
		  info->zdlePend = 0 ;
		}

		if( c == ZDLE )
		  info->zdlePend = 1 ;
		else
		{
		  dataChar(c) ;
		  if( ++info->count >= info->crclen )
		  {
		    dumpCrc() ;
		    switch( info->crcCmd ) {
		      case ZCRCE:
			printf("     ZCRCE: end of frame, header follows\n") ;
			info->state = Idle ;
			break ;
		      case ZCRCW:
			printf("     ZCRCW: end of frame, send ZACK\n") ;
			info->state = Idle ;
			break ;
		      case ZCRCG:
			printf("     ZCRCG: more data follows:\n") ;
			info->state = InData ;
			break ;
		      case ZCRCQ:
			printf("     ZCRCQ: send ZACK, more data follows:\n") ;
			info->state = InData ;
			break ;
		    }
		  }
		}
		break ;
	    }
	}
}


	/* handle a character that's not part of the protocol */

dataChar(int c)
{
assert(linecnt < MAXLINE) ;
	line[linecnt++] = c ;
	if( linecnt >= MAXLINE )
	  dumpLine() ;
}


	/* here if we thought we were in a header, but were wrong */

cancelHeader( Zinfo *info )
{
	int	i ;

	for(i=0; i<info->count; ++i) {
assert(i < MAXHBUF) ;
	  dataChar(hbuffer[i]) ;
	}

	info->state = Idle ;
}


	/* here to display a full header.  CRC's not currently checked */

displayHeader( Zinfo *info )
{
	int	i ;
	u_long	crc ;
	int	h32 = info->state == Header32 ;
static	char	*names[] = {
	  "ZRQINIT", "ZRINIT", "ZSINIT", "ZACK", "ZFILE", "ZSKIP",
	  "ZNAK", "ZABORT", "ZFIN", "ZRPOS", "ZDATA", "ZEOF", "ZFERR",
	  "ZCRC", "ZCHALLENGE", "ZCOMPL", "ZCAN", "ZFREECNT",
	  "ZCOMMAND", "ZSTDERR",} ;

	dumpLine() ;

	printf("     ") ;
	switch( info->state ) {
	  case HexHeader: printf("hex header") ; break ;
	  case Header16: printf("bin header") ; break ;
	  case Header32: printf("bin32 header") ; break ;
	}
	printf(" %d: %s: d=[%x %x %x %x]", info->headertype,
	  info->headertype <= ZSTDERR ? names[info->headertype] : "BAD HEADER",
	  info->data[0], info->data[1], info->data[2], info->data[3]) ;
	switch( info->state ) {
	  case HexHeader:
	  case Header16:
	    printf(" crc=[%x %x]", info->crcBytes[0], info->crcBytes[1]) ;
	    break ;
	  case Header32:
	    printf(" crc=[%x %x %x %x]",
	      info->crcBytes[0], info->crcBytes[1],
	      info->crcBytes[2], info->crcBytes[3]) ;
	    break ;
	}

	switch( info->headertype ) {
	  case ZRQINIT:
	  case ZRINIT:
	  case ZACK:
	  case ZSKIP:
	  case ZNAK:
	  case ZABORT:
	  case ZFIN:
	  case ZRPOS:
	  case ZEOF:
	  case ZFERR:
	  case ZCRC:
	  case ZCHALLENGE:
	  case ZCOMPL:
	  case ZCAN:
	  case ZFREECNT:
	  case ZCOMMAND:
	    printf("\n") ;
	    info->state = Idle ;
	    break ;

	  case ZSINIT:
	  case ZFILE:
	  case ZDATA:
	  case ZSTDERR:
	    printf(", data follows:\n") ;
	    info->state = InData ;
	    info->count = 0 ;
	    break ;
	}

	if( !h32 )
	{
	  crc = 0 ;
	  crc = updcrc(info->headertype, crc) ;
	  crc = updcrc(info->data[0], crc) ;
	  crc = updcrc(info->data[1], crc) ;
	  crc = updcrc(info->data[2], crc) ;
	  crc = updcrc(info->data[3], crc) ;
	  crc = updcrc(info->crcBytes[0], crc) ;
	  crc = updcrc(info->crcBytes[1], crc) ;
	  if( crc&0xffff != 0 )
	    printf("     CRC ERROR\n") ;
	}
	else
	{
	  crc = 0xffffffff ;
	  crc = UPDC32(info->headertype, crc) ;
	  crc = UPDC32(info->data[0], crc) ;
	  crc = UPDC32(info->data[1], crc) ;
	  crc = UPDC32(info->data[2], crc) ;
	  crc = UPDC32(info->data[3], crc) ;
	  crc = UPDC32(info->crcBytes[0], crc) ;
	  crc = UPDC32(info->crcBytes[1], crc) ;
	  crc = UPDC32(info->crcBytes[2], crc) ;
	  crc = UPDC32(info->crcBytes[3], crc) ;
	  if( crc != 0xdebb20e3 )
	    printf("     CRC ERROR\n") ;
	}
}

dumpCrc()
{
	int	i,j ;

	if( linecnt <= 0 )
	  return ;

	if( linecnt > MAXLINE ) linecnt = MAXLINE ;
	printf("     crc: ") ;
	for(i=0; i<linecnt; ++i) {
assert(i < MAXLINE) ;
	  printf("%2.2x ", line[i]) ;
	}
	printf("\n") ;

	linecnt = 0 ;
}


	/* return value of 2 hex digits */

int
hex2(char *chrs)
{
	return	(hex1(chrs[0]) << 4) + hex1(chrs[1]) ;
}

	/* return value of a hex digit */

int
hex1(int chr)
{
	chr -= '0' ;
	if( chr > 9 )
	  chr -= 'A'-'0'-10 ;
	if( chr > 15 )
	  chr -= 'a' - 'A' ;

	return	chr ;
}


	/* apply ZDLE to chr */

int
zdle(int chr)
{
	switch( chr ) {
	  case ZRUB0: return 0177 ;
	  case ZRUB1: return 0377 ;
	  default:
	    if( (chr & 0140) == 0100 )
	      return chr ^ 0100 ;
	    return -1 ;
	}
}
