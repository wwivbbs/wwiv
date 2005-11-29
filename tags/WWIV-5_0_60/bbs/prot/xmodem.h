/*	$Id$	*/

//#include <sys/param.h>


//#define MAXPATHLEN 260

typedef	int	bool ;

typedef	enum	{
		  Xmodem=0,
		  XmodemCrc=1,
		  WXmodem=2,
		  Ymodem=3,
		  YmodemG=4,
		} Protocol ;


extern	bool	xmodem1k ;	/* 1k blocks supported */
extern	bool	fileInfo ;	/* ymodem: send file attributes? */
extern	Protocol protocol ;
extern	int	xmTfd ;		/* transmit file descriptor */
extern	int	xmRfd ;		/* receive file descriptor */

extern	int	xmTimeout ;	/* timeout, seconds */

extern	char	xmDefPath[MAXPATHLEN] ;		/* default location (ymodem) */
extern	char	xmFilename[MAXPATHLEN] ;	/* current filename */

	/* error code definitions */

#define	XmDone		-1	/* done */
#define	XmErrInt	-2	/* internal error */
#define	XmErrSys	-3	/* system error, see errno */
#define	XmErrNotOpen	-4	/* communication channel not open */
#define	XmErrCantOpen	-5	/* can't open file, see errno */
#define	XmErrInitTo	-10	/* transmitter failed to respond to init req. */
#define	XmErrSequence	-11	/* packet received out of sequence */
#define	XmErrCancel	-12	/* cancelled by remote end */
#define	XmErrRcvTo	-13	/* remote end timed out during transfer */


#ifdef	__STDC__

extern	int	XmodemRInit(char *path, Protocol p) ;
					/* start receive protocol */
extern	int	XmodemRRcv(char c) ;	/* call for each received char. */
extern	int	XmodemRTimeout() ;	/* call if xmTimeout expires */
extern	int	XmodemRAbort() ;	/* call to abort protocol */

extern	int	XmodemTInit(char *path, Protocol p) ;
					/* start transmit protocol */
extern	int	XmodemTRcv(char c) ;	/* call for each received char. */
extern	int	XmodemTTimeout() ;	/* call if xmTimeout expires */
extern	int	XmodemTAbort() ;	/* call to abort protocol */
extern	int	XmodemTFinish() ;	/* call after last file sent (ymodem) */

#else

extern	int	XmodemRInit() ;		/* start receive protocol */
extern	int	XmodemRRcv() ;		/* call for each received char. */
extern	int	XmodemRTimeout() ;	/* call if xmTimeout expires */
extern	int	XmodemRAbort() ;	/* call to abort protocol */

extern	int	XmodemTInit() ;		/* start transmit protocol */
extern	int	XmodemTRcv() ;		/* call for each received char. */
extern	int	XmodemTTimeout() ;	/* call if xmTimeout expires */
extern	int	XmodemTAbort() ;	/* call to abort protocol */

#endif



	/* INTERNAL */

#define	SOH	1	/* ^A */
#define	STX	2	/* ^B */
#define	EOT	4	/* ^D */
#define	ACK	6	/* ^F */
#define	DLE	16	/* ^P */
#define	XON	17	/* ^Q */
#define	XOFF	19	/* ^S */
#define	NAK	21	/* ^U */
#define	SYN	22	/* ^V */
#define	CAN	24	/* ^X */

#ifndef	False
#define	False	0
#define	True	1
#endif

#define	MAXERROR	10
#define	INITTO		10	/* initialization timeout, basic xmodem */
#define	INITTO2		3	/* initialization timeout */
#define	PKTTO		5	/* in-packet receive timeout */

#define	MAXPACKET	1024	/* max packet length */


#ifdef	__STDC__

extern	int	sendCancel(), sendFlush(char),
		sendChr(char), sendStr(char *,int) ;
extern	int	calcrc(char *ptr, int count) ;
extern	int	calcChecksum(char *ptr, int count) ;

#else

extern	int	sendCancel(), sendFlush(), sendChr(), sendStr() ;
extern	int	calcrc() ;
extern	int	calcChecksum() ;

#endif
