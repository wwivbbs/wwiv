#ifndef lint
static const char rcsid[] = "$Id$" ;
#endif

/*
 * Copyright (c) 1995 by Edward A. Falk
 */


/**********
 *
 *
 *	@   @   @@@    @@@   @   @  
 *	@@ @@  @   @    @    @@  @  
 *	@ @ @  @@@@@    @    @ @ @  
 *	@ @ @  @   @    @    @  @@  
 *	@ @ @  @   @   @@@   @   @  
 *
 *	MAIN - demonstration zmodem program
 *
 *	This program implements the x,y,z-modem protocols, using the
 *	zmodem library.
 *
 *	Edward A. Falk
 *
 *	Jan, 1995
 *
 *
 *
 **********/

static	char	usage [] =
"usage:	zmodem -r [options]\n\
	zmodem -s [options] file [file ...]\n\
	-r		receive files\n\
	-s		send files\n\
	- file		allows you to transmit files with '-' in their names\n\
	-v		verbose, more v's increase messages.\n\
	-l /dev/ttyX	use specified serial port instead of /dev/tty\n\
	-b baud		set baud rate\n\
	-x file		use xmodem, specify filename\n\
	-y		use ymodem\n\
	-k		use 1k, packets (ymodem, xmodem)\n\
	-win nn		set sliding window to nn bytes (send)\n\
	-buf nn		set input buffer size to nn bytes (receive)\n\
	-L nn		set packet length\n\
	-e		escape control characters\n\
	-a		ascii text\n\
	-i		image (binary data)\n\
	-resume		continue an interrupted transfer\n\
	-new		transfer only if newer\n\
	-newl		transfer only if newer or longer\n\
	-diff		transfer only if dates or lengths differ\n\
	-crc		transfer only if length or crc different\n\
	-apnd		append to existing file\n\
	-clob		force overwrite of existing files\n\
	-prot		do not overwrite existing files\n\
	-chng		change filename if destination exists\n\
	-noloc		do not transfer unless destination exists\n\
	-[hq]		this list\n" ;

#include <stdio.h>

/****
 *
 * Constants,  typedefs, externals, globals, statics, macros, block data
 *
 ****/





#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/termios.h>
#include <sys/param.h>

#include "zmodem.h"
#include "seriallog.h"

extern	int	errno ;
extern	char	*getenv() ;


	/* compile-time parameters */

#define	ESCCTRL	0	/* control characters need to be escaped */

#if !defined(CRTSCTS) && defined(CNEW_RTSCTS)
#define	CRTSCTS	CNEW_RTSCTS
#endif

static	int	baud = 0 ;		/* requested baud rate */
static	char	*line = NULL ;		/* requested serial line */
static	int	verbose = 0 ;
static	int	send = 0 ;		/* send files */
static	int	receive = 0 ;		/* receive */
static	int	xmodem = 0 ;		/* use xmodem */
static	int	ymodem = 0 ;		/* use ymodem */
static	int	escCtrl = ESCCTRL ;
static	int	ascii = 0 ;		/* translate line endings */
static	int	binary = 0 ;		/* don't translate line endings */
static	int	resume = 0 ;		/* resume interrupted transfers */
static	int	xferType = 0 ;		/* new,newl,diff,crc, etc. */
static	int	noloc = 0 ;
static	int	doCancel = 0 ;
static	int	doLogging = 0 ;

static	int	Corrupt = 0 ;		/* deliberately corrupt data */

static	int	fileErrs ;		/* used to track errors per file */
static	int	fileSent ;		/* track amount sent */

static	ZModem	info ;

static	int	begun = 0 ;
static	struct	termios	old_settings, new_settings ;

static	int	FinishXmit(ZModem *info) ;
static	int	DoReceive(ZModem *info) ;
static	int	InitXmit(ZModem *info) ;
static	int	XmitFile(char *filename, int f0, int f1, ZModem *info) ;
static	void	SendFile() ;
static	void	RcvFiles() ;
static	void	RcvXmodem() ;
static	int	getBaud() ;
static	void	resetCom() ;
static	char	*basename(char *name) ;

#ifdef	SVr4
static	void	sighandle(int) ;
#else
static	void	sighandle() ;
#endif

extern	FILE	*SerialLogFile ;
extern	FILE	*zmodemlogfile ;

int
main(int argc, char **argv)
{
	char	*progname ;

#ifdef	COMMENT
	printf("%d\n", getpid()) ;
#endif	/* COMMENT */

	info.ifd = info.ofd = -1 ;
	info.zrinitflags = 0 ;
	info.zsinitflags = 0 ;
	info.attn = NULL ;
	info.packetsize = 0 ;
	info.windowsize = 0 ;
	info.bufsize = 0 ;

	progname = basename(argv[0]) ;
	if( strcmp(progname,"rz") == 0 )
	  receive = 1 ;
	else if( strcmp(progname, "sz") == 0 )
	  send = 1 ;

	++argv, --argc ;	/* skip program name */

	/* parse options */

	for(; argc > 0  &&  **argv == '-'; ++argv, --argc )
	{
	  if( strcmp(*argv, "-r") == 0 )
	    receive = 1 ;
	  else if( strcmp(*argv, "-s") == 0 )
	    send = 1 ;
	  else if( strncmp(*argv, "-v", 2) == 0 )
	    verbose += strlen(*argv)+1 ;
	  else if( strcmp(*argv, "-l") == 0 && --argc > 0 )
	    line = *++argv ;
	  else if( strcmp(*argv, "-b") == 0 && --argc > 0 )
	    baud = getBaud(atoi(*++argv)) ;
	  else if( strcmp(*argv, "-x") == 0 )
	    xmodem = 1 ;
	  else if( strcmp(*argv, "-y") == 0 )
	    ymodem = 1 ;
	  else if( strcmp(*argv, "-win") == 0 && --argc > 0 )
	    info.windowsize = atoi(*++argv) ;
	  else if( strcmp(*argv, "-buf") == 0 && --argc > 0 )
	    info.bufsize = atoi(*++argv) ;
	  else if( strcmp(*argv, "-L") == 0 && --argc > 0 )
	    info.packetsize = atoi(*++argv) ;
	  else if( strcmp(*argv, "-k") == 0 )
	    info.packetsize = 1024 ;
	  else if( strcmp(*argv, "-e") == 0 )
	    escCtrl = ESCCTL ;
	  else if( strcmp(*argv, "-a") == 0 )
	    ascii = 1 ;
	  else if( strcmp(*argv, "-i") == 0 )
	    binary = 1 ;
	  else if( strcmp(*argv, "-resume") == 0 )
	    resume = 1 ;
	  else if( strcmp(*argv, "-new") == 0 )
	    xferType = ZMNEW ;
	  else if( strcmp(*argv, "-newl") == 0 )
	    xferType = ZMNEWL ;
	  else if( strcmp(*argv, "-diff") == 0 )
	    xferType = ZMDIFF ;
	  else if( strcmp(*argv, "-crc") == 0 )
	    xferType = ZMCRC ;
	  else if( strcmp(*argv, "-apnd") == 0 )
	    xferType = ZMAPND ;
	  else if( strcmp(*argv, "-clob") == 0 )
	    xferType = ZMCLOB ;
	  else if( strcmp(*argv, "-prot") == 0 )
	    xferType = ZMPROT ;
	  else if( strcmp(*argv, "-chng") == 0 )
	    xferType = ZMCHNG ;
	  else if( strcmp(*argv, "-noloc") == 0 )
	    noloc = ZMSKNOLOC ;
	  else if( strcmp(*argv, "-h") == 0 ||
		   strcmp(*argv, "-q") == 0 )
	  {
	    fprintf(stderr, usage) ;
	    exit(0) ;
	  }
	  else if( strcmp(*argv, "-corrupt") == 0 )
	    Corrupt = 1 ;
	  else if( strcmp(*argv, "-log") == 0 )
	    doLogging = 1 ;
	  else if( strcmp(*argv, "-") == 0 ) {
	    ++argv, --argc ; break ;
	  }
	  else {
	    fprintf(stderr, "unknown argument '%s' or missing value\n%s",
	      *argv, usage) ;
	    exit(2) ;
	  }
	}


	if( doLogging ) {
	  if( (SerialLogFile = fopen("xfer.log", "w")) == NULL )
	    perror("xfer.log") ;
	  if( (zmodemlogfile = fopen("zmodem.log","w")) == NULL )
	    perror("zmodem.log") ;
	}


	if( send ) {
	  for(; argc > 0; ++argv, --argc )	/* process filenames */
	    SendFile(*argv) ;
	  FinishXmit(&info) ;
	}

	else if( receive )
	{
	  if( !xmodem )
	    RcvFiles() ;
	  else
	    for(; argc > 0; ++argv, --argc )	/* process filenames */
	      RcvXmodem(*argv) ;
	}


	else {
	  fprintf(stderr,
	    "either -s (send) or -r (receive) must be specified\n%s", usage) ;
	  exit(2) ;
	}

	resetCom() ;
	exit(0) ;
}


static	void
sighandle(int arg)
{
	doCancel = 1 ;
}



static	void
openCom()
{
	char	*ptr ;

	if( line == NULL )
	  line = getenv("RZSZLINE") ;

	if( baud == 0 && (ptr = getenv("RZSZBAUD")) != NULL )
	  baud = getBaud(atoi(ptr)) ;

	if( line == NULL ) {
	  info.ifd = 0 ;
	  info.ofd = 1 ;
	}
	else if( (info.ifd = info.ofd = open(line, O_RDWR)) == -1 ) {
	  fprintf(stderr,
	    "cannot open %s, %s, use \"zmodem -h\" for more info\n",
	    line, strerror(errno)) ;
	  exit(2) ;
	}

	/* assumption: setting attributes for one half of channel will
	 * change both halves.  This fails if different physical
	 * devices are used.
	 */
	tcgetattr(info.ifd,&old_settings) ;
	new_settings = old_settings ;

#ifdef	IUCLC
	new_settings.c_iflag &=
	  ~(ISTRIP|INLCR|IGNCR|ICRNL|IUCLC|IXON|IXOFF|IMAXBEL) ;
#else
	new_settings.c_iflag &=
	  ~(ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF|IMAXBEL) ;
#endif
	new_settings.c_oflag &= ~OPOST ;
	new_settings.c_cflag &= ~(CSIZE|PARENB) ;
	new_settings.c_cflag |= CS8 ;
	if( baud != 0 ) {
	  cfsetospeed(&new_settings, baud) ;
	  cfsetispeed(&new_settings, baud) ;
	}
	else
	  baud = cfgetospeed(&old_settings) ;
	new_settings.c_lflag = 0 ;
	new_settings.c_cc[VMIN] = 32 ;
	new_settings.c_cc[VTIME] = 1 ;
	tcsetattr(info.ifd,TCSADRAIN, &new_settings) ;

	info.zrinitflags = CANFDX|CANOVIO|CANBRK|CANFC32|escCtrl ;
	info.zsinitflags = escCtrl ;
	if( info.packetsize == 0 ) {
	  if( xmodem || ymodem )
	    info.packetsize = 128 ;
	  /* TODO: what about future high-speed interfaces? */
	  else if( baud < B2400 )
	    info.packetsize = 256 ;
	  else if( baud == B2400 )
	    info.packetsize = 512 ;
	  else
	    info.packetsize = 1024 ;
	}
}


static	void
resetCom()
{
	tcsetattr(info.ifd,TCSADRAIN, &old_settings) ;
}



static	void
SendFile(char *filename)
{
	int	f0, f1 ;

	fileErrs = 0 ;

	if( !begun )		/* establish connection */
	{
	  openCom() ;

	  signal(SIGINT, sighandle) ;
	  signal(SIGTERM, sighandle) ;
	  signal(SIGHUP, sighandle) ;

	  if( InitXmit(&info) ) {
	    fprintf(stderr, "connect failed\n") ;
	    resetCom() ;
	    exit(1) ;
	  }

	  begun = 1 ;
	}

	if( ascii )
	  f0 = ZCNL ;
	else if( binary )
	  f0 = ZCBIN ;
	else if( resume )
	  f0 = ZCRESUM ;
	else
	  f0 = 0 ;

	f1 = xferType | noloc ;

	if( XmitFile(filename, f0,f1, &info) ) {
	  fprintf(stderr, "connect failed\n") ;
	  resetCom() ;
	  exit(1) ;
	}
}



static	void
RcvFiles()
{
	openCom() ;

	signal(SIGINT, sighandle) ;
	signal(SIGTERM, sighandle) ;

	if( DoReceive(&info) ) {
	  fprintf(stderr, "connect failed\n") ;
	  resetCom() ;
	  exit(1) ;
	}
}



static	void
RcvXmodem(char *filename)
{
	/* TODO: */
}


static	int
getBaud(int b)
{
	switch(b) {
	  case 50: return B50 ;
	  case 75: return B75 ;
	  case 110: return B110 ;
	  case 134: return B134 ;
	  case 150: return B150 ;
	  case 200: return B200 ;
	  case 300: return B300 ;
	  case 600: return B600 ;
	  case 1200: return B1200 ;
	  case 1800: return B1800 ;
	  case 2400: return B2400 ;
	  case 4800: return B4800 ;
	  case 9600: return B9600 ;
	  case 19200: return B19200 ;
	  case 38400: return B38400 ;
	  default: return 0 ;
	}
}



	/* TEST: randomly corrupt every 1000th byte */
static	void
corrupt(u_char *buffer, int len)
{
	extern double drand48() ;

	while( --len >= 0 ) {
	  if( drand48() < 1./1000. )
	    *buffer ^= 0x81 ;
	  ++buffer ;
	}
}


static	int
doIO(ZModem *info)
{
	fd_set	readfds ;
	struct timeval timeout ;
	int	i ;
	int	len ;
	u_char	buffer[1024] ;
	int	done = 0 ;

	while(!done)
	{
	  FD_ZERO(&readfds) ;
	  FD_SET(info->ifd, &readfds) ;
	  timeout.tv_sec = info->timeout ;
	  timeout.tv_usec = 0 ;
	  i = select(info->ifd+1, &readfds,NULL,NULL, &timeout) ;
	  if( doCancel ) {
	    ZmodemAbort(info) ;
	    fprintf(stderr, "cancelled by user\n") ;
	    resetCom() ;
	    exit(3) ;
	  }
	  if( i<0 )
	    perror("select") ;
	  else if( i==0 )
	    done = ZmodemTimeout(info) ;
	  else {
	    len = read(info->ifd, buffer, sizeof(buffer)) ;
	    if( Corrupt )
	      corrupt(buffer, len) ;
	    if( SerialLogFile != NULL )
	      SerialLog(buffer, len, 1) ;
	    done = ZmodemRcv(buffer, len, info) ;
	  }
	}
	if( SerialLogFile != NULL )
	  SerialLogFlush() ;
	return done ;
}


static	int
InitXmit(ZModem *info)
{
	int	done ;

	if( xmodem )
	  done = XmodemTInit(info) ;
	else if( ymodem )
	  done = YmodemTInit(info) ;
	else
	  done = ZmodemTInit(info) ;
	if( !done )
	  done = doIO(info) ;

	return done != ZmDone ;
}


static	int
XmitFile(char *filename, int f0, int f1, ZModem *info)
{
	int	done ;

	done = ZmodemTFile(filename,filename, f0,f1,0,0,
		0,0, info) ;
	switch( done ) {
	  case 0:
	    if( verbose )
	      fprintf(stderr, "Sending: \"%s\"\n", filename) ;
	    break ;

	  case ZmErrCantOpen:
	    fprintf(stderr, "cannot open file \"%s\": %s\n",
	      filename, strerror(errno)) ;
	    return 0 ;

	  case ZmFileTooLong:
	    fprintf(stderr, "filename \"%s\" too long, skipping...\n",
	      filename) ;
	    return 0 ;

	  case ZmDone:
	    return 0 ;

	  default:
	    return 1 ;
	}

	if( !done )
	  done = doIO(info) ;

	return done != ZmDone ;
}



static	int
FinishXmit(ZModem *info)
{
	int	done ;

	done = ZmodemTFinish(info) ;
	if( !done )
	  done = doIO(info) ;

	return done != ZmDone ;
}



static	int
DoReceive(ZModem *info)
{
	int	done ;

	if( ymodem )
	  done = YmodemRInit(info) ;
	else
	  done = ZmodemRInit(info) ;

	if( !done )
	  done = doIO(info) ;

	return done != ZmDone ;
}


int
ZXmitStr(u_char *buffer, int len, ZModem *info)
{
	u_char	b2[1024] ;

	/* TEST: randomly corrupt every 1000th byte */
	if( Corrupt )
	{
	  bcopy(buffer, b2, len) ;
	  corrupt(b2, len) ;
	  buffer = b2 ;
	}

	if( SerialLogFile != NULL )
	  SerialLog(buffer, len, 0) ;

	if( write(info->ofd, buffer, len) != len )
	  return ZmErrSys ;

	return 0 ;
}


void
ZIFlush(ZModem *info)
{
	if( SerialLogFile != NULL )
	  SerialLogFlush() ;

	if( tcflush(info->ifd, TCIFLUSH) != 0 )
	  perror("TCIFLUSH") ;
}

void
ZOFlush(ZModem *info)
{
	if( SerialLogFile != NULL )
	  SerialLogFlush() ;

	if( tcflush(info->ifd, TCOFLUSH) != 0 )
	  perror("TCOFLUSH") ;
}

void
ZStatus(int type, int j, char *str)
{
	switch( type ) {
	  case RcvByteCount:
	    if( verbose >= 2 )
	      fprintf(stderr, "received: %6d bytes\n", j) ;
	    break ;
	  case SndByteCount:
	    fileSent = j ;
	    if( verbose >= 2 )
	      fprintf(stderr, "sent: %6d bytes\n", j) ;
	    break ;
	  case RcvTimeout:
	    if( verbose )
	      fprintf(stderr, "receiver timeouts: %2d\n", j) ;
	    break ;
	  case SndTimeout:
	    if( verbose )
	      fprintf(stderr, "sender timeouts: %2d\n", j) ;
	    break ;
	  case RmtCancel:
	    fprintf(stderr, "transfer cancelled by remote\n") ;
	    break ;
	  case ProtocolErr:
	    if( verbose >= 3 )
	      fprintf(stderr, "protocol error: %2.2d\n", j) ;
	    break ;
	  case RemoteMessage:
	    fprintf(stderr, "remote message: %s\n",str) ;
	    break ;
	  case DataErr:
	    if( verbose >= 3 )
	      fprintf(stderr, "data errors: %2d\n", j) ;
#ifdef	COMMENT
	    if( ++fileErrs > 20 )
	    {
	      /* something's wrong */
	      ZmodemAbort(&info) ;
	    }
#endif	/* COMMENT */
	    break ;
	  case FileErr:
	    fprintf(stderr, "cannot write file: %s\n", strerror(errno)) ;
	    break ;
	}
}


int
ZAttn(ZModem *info)
{
	char	*ptr ;

	if( info->attn == NULL )
	  return 0 ;

	for(ptr = info->attn; *ptr != '\0'; ++ptr) {
	  if( *ptr == ATTNBRK )
	    tcsendbreak(info->ifd, 0) ;
	  else if( *ptr == ATTNPSE )
	    sleep(1) ;
	  else
	    write(info->ifd, ptr, 1) ;
	}
	return 0 ;
}


FILE *
ZOpenFile(char *name, u_long crc, ZModem *info)
{
	struct stat	buf ;
	int		exists ;	/* file already exists */
static	int		changeCount = 0 ;
	char		name2[MAXPATHLEN] ;
	int		apnd = 0 ;
	int		f0,f1 ;
	FILE		*rval ;

	/* TODO: if absolute path, do we want to allow it?
	 * if relative path, do we want to prepend something?
	 */

	if( *name == '/' )	/* for now, disallow absolute paths */
	  return NULL ;

	if( stat(name, &buf) == 0 )
	  exists = 1 ;
	else if( errno == ENOENT )
	  exists = 0 ;
	else
	  return NULL ;


	/* if remote end has not specified transfer flags, we can
	 * accept the local definitions
	 */

	if( (f0=info->f0) == 0 ) {
	  if( ascii )
	    f0 = ZCNL ;
	  else if( binary )
	    f0 = ZCBIN ;
	  else if( resume )
	    f0 = ZCRESUM ;
	  else
	    f0 = 0 ;
	}

	if( (f1=info->f1) == 0 )
	  f1 = xferType | noloc ;

	if( f0 == ZCRESUM ) {	/* if exists, and we already have it, return */
	  if( exists  &&  buf.st_size == info->len )
	    return NULL ;
	  apnd = 1 ;
	}

	/* reject if file not found and it most be there (ZMSKNOLOC) */
	if( !exists && (f1 & ZMSKNOLOC) )
	  return NULL ;

	switch( f1 & ZMMASK ) {
	  case 0:	/* Implementation-dependent.  In this case, we
			 * reject if file exists (and ZMSKNOLOC not set) */
	    if( exists && !(f1 & ZMSKNOLOC) ) {
	      fprintf(stderr, "%s already exists\n", name) ;
	      return NULL ;
	    }
	    break ;

	  case ZMNEWL:	/* take if newer or longer than file on disk */
	    if( exists  &&  info->date <= buf.st_mtime  &&
		info->len <= buf.st_size ) {
	      fprintf(stderr, "%s already exists\n", name) ;
	      return NULL ;
	    }
	    break ;

	  case ZMCRC:		/* take if different CRC or length */
	    if( exists  &&  info->len == buf.st_size && crc == FileCrc(name) )
	    {
	      fprintf(stderr, "%s already exists\n", name) ;
	      return NULL ;
	    }
	    break ;

	  case ZMAPND:	/* append */
	    apnd = 1 ;
	  case ZMCLOB:	/* unconditional replace */
	    break ;

	  case ZMNEW:	/* take if newer than file on disk */
	    if( exists  &&  info->date <= buf.st_mtime ) {
	      fprintf(stderr, "%s already exists and is newer\n", name) ;
	      return NULL ;
	    }
	    break ;

	  case ZMDIFF:	/* take if different date or length */
	    if( exists  &&  info->date == buf.st_mtime  &&
		info->len == buf.st_size )
	    {
	      fprintf(stderr, "%s already exists\n", name) ;
	      return NULL ;
	    }
	    break ;

	  case ZMPROT:	/* only if dest does not exist */
	    if( exists )
	    {
	      fprintf(stderr, "%s already exists\n", name) ;
	      return NULL ;
	    }
	    break ;

	  case ZMCHNG:	/* invent new filename if exists */
	    if( exists ) {
	      while( exists ) {
		sprintf(name2, "%s_%d", name, changeCount++) ;
		exists = stat(name2, &buf) == 0 || errno != ENOENT ;
	      }
	      name = name2 ;
	    }
	    break ;
	}

	/* here if we've decided to accept */
#ifdef	COMMENT
	if( exists && !apnd && unlink(name) != 0 )
	  return NULL ;
#endif	/* COMMENT */

	/* TODO: build directory path if needed */

	if( verbose )
	  fprintf(stderr, "Receiving: \"%s\"\n", name) ;

	rval = fopen(name, apnd ? "a" : "w") ;
	if( rval == NULL )
	  perror(name) ;
	return rval ;
}


int
ZWriteFile(u_char *buffer, int len, FILE *file, ZModem *info)
{
	/* TODO: if ZCNL set in info->f0, convert
	 * newlines to local convention
	 */

	return fwrite(buffer, 1, len, file) == len ? 0 : ZmErrSys ;
}



int
ZCloseFile(ZModem *info)
{
	struct timeval tvp[2] ;
	fclose(info->file) ;
	if( ymodem )
	  truncate(info->filename, info->len) ;
	if( info->date != 0 ) {
	  tvp[0].tv_sec = tvp[1].tv_sec = info->date ;
	  tvp[0].tv_usec = tvp[1].tv_usec = 0 ;
	  utimes(info->filename, tvp) ;
	}
	if( info->mode & 01000000 )
	  chmod(info->filename, info->mode&0777) ;
	return 0 ;
}


void
ZIdleStr(u_char *buffer, int len, ZModem *info)
{
#ifdef	COMMENT
	fwrite(buffer, 1,len, stdout) ;
#endif	/* COMMENT */
}

void
ZFlowControl(int onoff, ZModem *info)
{
	if( onoff ) {
	  new_settings.c_iflag |= IXON|IXANY|IXOFF ;
#ifdef	COMMENT
	  new_settings.c_cflag |= CRTSCTS ;
#endif	/* COMMENT */
	}
	else {
	  new_settings.c_iflag &= ~(IXON|IXANY|IXOFF) ;
	  new_settings.c_cflag &= ~CRTSCTS ;
	}
	tcsetattr(info->ifd,TCSADRAIN, &new_settings) ;
}



static	char *
basename(char *name)
{
	char	*ptr ;

	if( (ptr = strrchr(name, '/')) == NULL )
	  return name ;
	else
	  return ++ptr ;
}
