#ifndef	lint
static const char rcsid[] = "$Id$" ;
#endif


#define	TEST	/* standalone test */

#include <unistd.h>
#include <string.h>
#include "zmodem.h"
#include <sys/stat.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/termios.h>
#include <sys/termio.h>		/* define TCSBRK */
#include <sys/param.h>

extern	int	errno ;

static	u_char	zeros[4] = {0,0,0,0} ;

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
	int	done = 0 ;
	int	filecount = 0 ;
	ZModem	info ;

#ifdef	TEST

static	u_char	Amsg0[] = {
	  0x43, 
	} ;
static	u_char	Amsg1[] = {
	  0x06, 0x43, 
	} ;
static	u_char	Amsg2[] = {
	  0x06, 
	} ;
static	u_char	Amsg3[] = {
	  0x06, 
	} ;
static	u_char	Amsg4[] = {
	  0x06, 0x43, 
	} ;
static	u_char	Amsg5[] = {
	  0x06, 0x43, 
	} ;
static	u_char	Amsg6[] = {
	  0x06, 
	} ;
static	u_char	Amsg7[] = {
	  0x06, 
	} ;
static	u_char	Amsg8[] = {
	  0x06, 
	} ;
static	u_char	Amsg9[] = {
	  0x06, 0x43, 
	} ;


#define	E(b)	{b, sizeof(b)}
static	struct {u_char *bm; int len} Bmsgs[] = {
	  E(Amsg0),
	  E(Amsg1),
	  E(Amsg2),
	  E(Amsg3),
	  E(Amsg4),
	  E(Amsg5),
	  E(Amsg6),
	  E(Amsg7),
	  E(Amsg8),
	  E(Amsg9),} ;


#endif	/* TEST */

	/* try to trap uninit. variables */
	memset(&info,0xa5,sizeof(info)) ;

	info.zsinitflags = 0 ;
	info.attn = "\17\336" ;
	info.packetsize = 1024 ;
	info.windowsize = 4096 ;

#ifdef	TEST
	done = YmodemTInit(&info) ;
	for(i=0; !done; ++i )
	  done = ZmodemRcv(Bmsgs[i].bm, Bmsgs[i].len, &info) ;

	done = ZmodemTFile("utils.c","utils.c", 0,0,0,0, 1,0, &info) ;
	for(; !done; ++i )
	  done = ZmodemRcv(Bmsgs[i].bm, Bmsgs[i].len, &info) ;

	done = ZmodemTFile("foo.c","foo.c", 0,0,0,0, 1,0, &info) ;
	for(; !done; ++i )
	  done = ZmodemRcv(Bmsgs[i].bm, Bmsgs[i].len, &info) ;

	done = ZmodemTFinish(info) ;

#else
	if( argc < 2 )
	  exit(2) ;

	info.ifd = open(argv[1], O_RDWR) ;

	if( info.ifd == -1 )
	  exit(1) ;

	tcgetattr(info.ifd,&old_settings) ;
	new_settings = old_settings ;

	new_settings.c_iflag &=
	  ~(ISTRIP|INLCR|IGNCR|ICRNL|IUCLC|IXON|IXOFF|IMAXBEL) ;
	new_settings.c_oflag = 0 ;
	new_settings.c_cflag = B300|CS8|CREAD|CLOCAL ;
	new_settings.c_lflag = 0 ;
	new_settings.c_cc[VMIN] = 32 ;
	new_settings.c_cc[VTIME] = 1 ;
	tcsetattr(info.ifd,TCSADRAIN, &new_settings) ;

	InitXmit(&info) ;
	XmitFile("utils.c",1,0,&info) ;
#ifdef	COMMENT
	XmitFile("foo",1,0,&info) ;
	XmitFile("foo.c",1,0,&info) ;
	XmitFile("raw",1,0,&info) ;
#endif	/* COMMENT */
	FinishXmit(&info) ;

	tcsetattr(info.ifd,TCSADRAIN, &old_settings) ;
#endif	/* TEST */
	exit(0) ;
}


static	int
doIO(info)
	ZModem	*info ;
{
	fd_set	readfds ;
	struct timeval timeout ;
	int	i ;
	int	len ;
	char	buffer[1024] ;
	int	done = 0 ;

	while(!done)
	{
	  FD_ZERO(&readfds) ;
	  FD_SET(info->ifd, &readfds) ;
	  timeout.tv_sec = info->timeout ;
	  timeout.tv_usec = 0 ;
	  i = select(info->ifd+1, &readfds,NULL,NULL, &timeout) ;
	  if( i<0 )
	    perror("select") ;
	  else if( i==0 )
	    done = ZmodemTimeout(info) ;
	  else {
	    len = read(info->ifd, buffer, sizeof(buffer)) ;
	    done = ZmodemRcv(buffer, len, info) ;
	  }
	}
	return done ;
}


int
InitXmit(info)
	ZModem	*info ;
{
	int	done ;

	done = ZmodemTInit(info) ;
	if( !done )
	  done = doIO(info) ;

	return done != ZmDone ;
}


XmitFile(filename,nfiles,nbytes,info)
	char	*filename ;
	int	nfiles, nbytes ;
	ZModem	*info ;
{
	int	done ;

	done = ZmodemTFile(filename,filename, 0,ZMCLOB,0,0,
		nfiles,nbytes, info) ;
	if( done == ZmErrCantOpen || done == ZmFileTooLong )
	  return 0 ;

	if( !done )
	  done = doIO(info) ;

	return done != ZmDone ;
}



FinishXmit(info)
	ZModem	*info ;
{
	int	done ;

	done = ZmodemTFinish(info) ;
	if( !done )
	  done = doIO(info) ;

	return done != ZmDone ;
}


int
ZXmitStr(buffer, len, info)
	u_char	*buffer ;
	int	len ;
	ZModem	*info ;
{
	int	i,j ;
	u_char	c ;
extern	double	drand48() ;

#ifdef	TEST
	for(i=0; i<len; i += 16)
	{
	  printf("   ") ;
	  for(j=0; j<16 && i+j<len; ++j)
	    printf("%2.2x ", buffer[i+j]) ;
	  for(; j<16; ++j)
	    printf("   ") ;
	  printf("  |") ;
	  for(j=0; j<16 && i+j<len; ++j)
	    putchar(((c=buffer[i+j]) < 040 || c >= 0177) ? '.' : c ) ;
	  printf("|\n") ;
	}
#else

#ifdef	COMMENT
/* TEST: randomly corrupt one out of every 300 bytes */
for(i=0; i<len; ++i)
  if( drand48() < 1./300. ) {
    fprintf(stderr, "byte %d was %2.2x, is", i, buffer[i]) ;
    buffer[i] ^= 1<<(lrand48()&7) ;
    fprintf(stderr, " %2.2x\n", buffer[i]) ;
  }
#endif	/* COMMENT */

	if( write(info->ifd, buffer, len) != len )
	  return ZmErrSys ;
#endif	/* TEST */

	return 0 ;
}


void
ZIFlush(info)
	ZModem	*info ;
{
}

void
ZOFlush(info)
	ZModem	*info ;
{
}

void
ZStatus(i,j,str)
{
	fprintf(stderr,"status %d=%d\n",i,j) ;
}


int
ZAttn(info)
	ZModem	*info ;
{
	char	*ptr ;
	int	i = 0 ;

	for(ptr = info->attn; *ptr != '\0'; ++ptr) {
	  if( *ptr == ATTNBRK )
	    ioctl(info->ifd, TCSBRK, 0) ;
	  else if( *ptr == ATTNPSE )
	    sleep(1) ;
	  else
	    write(info->ifd, ptr, 1) ;
	}
	return 0 ;
}


FILE *
ZOpenFile(name, f0,f1,f2,f3, len, date, mode, filesRem, bytesRem, crc)
	char	*name ;
	int	f0,f1,f2,f3, len, mode, filesRem, bytesRem ;
	long	date ;
	u_long	crc ;
{
}


int
ZWriteFile()
{}


ZCloseFile(info)
	ZModem *info ;
{}

ZFlowControl(info)
	ZModem *info ;
{}
