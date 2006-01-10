/**
*	ZMODEM - main logic parser for zmodem library
*
*	Routines provided here:
*
*	name (args)
*		Brief description.
*
*	int ZmodemRcv(u_char *buffer, int len, ZModem *info)
*		Call whenever characters are received.  If this function
*		returns ZmDone, previous function has completed successfully,
*		either call ZmodemTFile() to start next file, or call
*		ZmodemTFinish() to terminate the session.
*
*
*	int
*	ZmodemTimeout(ZModem *info)
*		Call whenever the timeout period expires and no
*		characters have been received.
*
*	int
*	ZmodemAttention(ZModem *info)
*		Call whenever the attention sequence has been received
*		from the remote end.  It is safe to call this function
*		from an interrupt handler.
*
*	int
*	ZmodemAbort(ZModem *info)
*		Call to abort transfer.  Physical connection remains
*		open until you close it.
*
*	Copyright (c) 1995 by Edward A. Falk
*	January, 1995
*/

#include <stdio.h>

//
// Constants,  typedefs, externals, globals, statics, macros, block data
//



// TODO: sample input before initial send
// TODO: more intelligent timeout dispatch
// TODO: read all pending input before sending next data packet out
// TODO: if received ZDATA while waiting for ZFILE/ZFIN, it's probably leftovers
// TODO: enable flow control for zmodem, disable for X/YModem

#include <ctype.h>
#include <errno.h>
#include <string.h>

#include "zmodem.h"
#include "crctab.h"

static	u_char	zeros[4] = {0,0,0,0};


extern	int	YrcvChar( char c, ZModem *info );
extern	int	YrcvTimeout( ZModem *info );
extern	void	ZIdleStr(u_char *buffer, int len, ZModem *info);

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4706 4127 4244 4100 )
#endif

/* LEXICAL BOX: handle characters received from remote end.
* These may be header, data or noise.
*
* This section is a finite state machine for parsing headers
* and reading input data.  The info->chrCount field is effectively
* the state variable.
*/

int	FinishChar( char c, ZModem *info );
int	DataChar( u_char c, ZModem *info );
int	HdrChar( u_char c, ZModem *info );
int	IdleChar(u_char c, ZModem *info);
extern int	YsendChar(char c, ZModem *info);

int	ZProtocol(ZModem *info);
int ZDataReceived(ZModem *info, int crcGood);


int ZmodemRcv( u_char *str, int len, ZModem *info )
{
	u_char	c;
	int	err;

	info->rcvlen = len;

	while( --info->rcvlen >= 0 )
	{
		c = *str++;

		if( c == CAN )
		{
			if( ++info->canCount >= 5 )
			{
				ZStatus(RmtCancel, 0, NULL);
				return ZmErrCancel;
			}
		}
		else
		{
			info->canCount = 0;
		}

		if( info->InputState == ZModem::Ysend )
		{
			if( (err = YsendChar(c, info)) )
			{
				return err;
			}
		}
		else if( info->InputState == ZModem::Yrcv )
		{
			if( (err = YrcvChar(c, info)) )
			{
				return err;
			}
		}

		else if( c != XON && c != XOFF )
		{
			/* now look at what we have */
			switch( info->InputState )
			{
			case ZModem::Idle:
				if( (err = IdleChar(c, info)) )
				{
					return err;
				}
				break;

			case ZModem::Inhdr:
				if( (err = HdrChar(c, info)) )
				{
					return err;
				}
				break;

			case ZModem::Indata:
				if( (err = DataChar(c, info)) )
				{
					return err;
				}
				break;

			case ZModem::Finish:
				if( (err = FinishChar(c, info)) )
				{
					return err;
				}
				break;

			default:
				break;
			}
		}
	}

	return 0;
}







/* handle character input while idling
* looking for ZPAD-ZDLE sequence which introduces a header
*/

int IdleChar(u_char c,  ZModem *info)
{
	if( info->chrCount == 0 )
	{
		if( c == ZPAD )
		{
			++info->chrCount;
		}
		else if( info->state == Sending  &&  ++info->noiseCount > MaxNoise )
		{
			info->waitflag = 1;
		}
		else if( info->state == TStart && (c == 'C' || c == 'G' || c == NAK) )
		{
			/* switch to ymodem */
			info->state = YTStart;
			info->InputState = ZModem::Ysend;
			info->Protocol = ZModem::YMODEM;
			return YsendChar(c, info);
		}
		else
		{
			ZIdleStr(&c, 1, info);
		}
	}

	else
	{
		switch( c )
		{
		case ZPAD:
			++info->chrCount;
			break;
		case ZDLE:
			info->InputState = ZModem::Inhdr;
			info->chrCount=0;
			break;
		default:
			while( --info->chrCount >= 0 )
			{
				ZIdleStr((u_char *)"*", 1, info);
			}
			info->chrCount = 0;
			break;
		}
	}
	return 0;
}



u_int rcvHex( u_int i, char c )
{
	if( c <= '9' )
	{
		c -= '0';
	}
	else if( c <= 'F' )
	{
		c -= 'A'-10;
	}
	else
	{
		c -= 'a'-10;
	}
	return ( i << 4 ) + c;
}

/* handle character input in a header */

int HdrChar( u_char c,  ZModem *info )
{
	int	i;
	int	crc=0;

	if( c == ZDLE )
	{
		info->escape = 1;
		return 0;
	}

	if( info->escape )
	{
		info->escape = 0;
		switch( c ) {
		case ZRUB0: c = 0177; break;
		case ZRUB1: c = 0377; break;
		default: c ^= 0100; break;
		}
	}

	if( info->chrCount == 0 )
	{	/* waiting for format */
		switch( c )
		{
		case ZHEX:
		case ZBIN:
		case ZBIN32:
			info->DataType = c;
			info->chrCount = 1;
			info->crc = (info->DataType != ZBIN32) ? 0 : 0xffffffffL;
			memset(info->hdrData,0,sizeof(info->hdrData));
			break;
		default:
			info->InputState = ZModem::Idle;
			info->chrCount = 0;
			return ZXmitHdrHex(ZNAK, zeros, info);
		}
		return 0;
	}


	switch( info->DataType )
	{
		/* hex header is 14 hex digits, cr, lf.  Optional xon is ignored */
	  case ZHEX:
		  if( info->chrCount <= 14  &&   !isxdigit(c) )
		  {
			  info->InputState = ZModem::Idle;
			  info->chrCount = 0;
			  return ZXmitHdrHex(ZNAK, zeros, info);
		  }
		  if( info->chrCount <= 14 )
		  {
			  i = (info->chrCount-1)/2;
			  info->hdrData[i] = rcvHex(info->hdrData[i], c);
		  }
		  if( info->chrCount == 16 )
		  {
			  crc = 0;
			  for(i=0; i<7; ++i)
			  {
				  crc = updcrc(info->hdrData[i], crc);
			  }
			  info->InputState = ZModem::Idle;
			  info->chrCount = 0;
			  if( (crc&0xffff) != 0 )
			  {
				  return ZXmitHdrHex(ZNAK, zeros, info);
			  }
			  else
			  {
				  return ZProtocol(info);
			  }
		  }
		  else
			  ++info->chrCount;
		  break;
	  case ZBIN:
		  /* binary header is type, 4 bytes data, 2 bytes CRC */
		  info->hdrData[info->chrCount-1] = c;
		  info->crc = updcrc(c, info->crc);
		  if( ++info->chrCount > 7 )
		  {
			  info->InputState = ZModem::Idle;
			  info->chrCount = 0;
			  if( (crc&0xffff) != 0 )
			  {
				  return ZXmitHdrHex(ZNAK, zeros, info);
			  }
			  else
			  {
				  return ZProtocol(info);
			  }
		  }
		  break;
	  case ZBIN32:
		  /* binary32 header is type, 4 bytes data, 4 bytes CRC */
		  info->hdrData[info->chrCount-1] = c;
		  info->crc = UPDC32(c, info->crc);
		  if( ++info->chrCount > 9 )
		  {
			  info->InputState = ZModem::Idle;
			  info->chrCount = 0;
			  if( info->crc != 0xdebb20e3 )	/* see note below */
			  {
				  return ZXmitHdrHex(ZNAK, zeros, info);
			  }
			  else
			  {
				  return ZProtocol(info);
			  }
		  }
		  break;
	}
	return 0;
}

/* handle character input in a data buffer */

int DataChar( u_char c,  ZModem *info )
{
	if( c == ZDLE )
	{
		info->escape = 1;
		return 0;
	}

	if( info->escape )
	{
		info->escape = 0;
		switch( c )
		{
		case ZCRCE:
		case ZCRCG:
		case ZCRCQ:
		case ZCRCW:
			info->PacketType = c;
			info->crcCount = (info->DataType == ZBIN32) ? 4 : 2;
			if( info->DataType == ZBIN )
			{
				info->crc = updcrc(c, info->crc);
			}
			else
			{
				info->crc = UPDC32(c, info->crc);
			}
			return 0;
		case ZRUB0: c = 0177; break;
		case ZRUB1: c = 0377; break;
		default: c ^= 0100; break;
		}
	}


	switch( info->DataType )
	{
		/* TODO: are hex data packets ever used? */
	  case ZBIN:
		  info->crc = updcrc(c, info->crc);
		  if( info->crcCount == 0 )
		  {
			  info->buffer[info->chrCount++] = c;
		  }
		  else if( --info->crcCount == 0 )
		  {
			  return ZDataReceived( info, ( info->crc&0xffff ) == 0 );
		  }
		  break;
	  case ZBIN32:
		  info->crc = UPDC32(c, info->crc);
		  if( info->crcCount == 0 )
		  {
			  info->buffer[info->chrCount++] = c;
		  }
		  else if( --info->crcCount == 0 )
		  {
			  return ZDataReceived(info, info->crc == 0xdebb20e3);
		  }
		  break;
	}
	return 0;
}


/* wait for "OO" */
int FinishChar( char c,  ZModem *info )
{
	if( c == 'O' )
	{
		if( ++info->chrCount >= 2 )
		{
			return ZmDone;
		}
	}
	else
	{
		info->chrCount = 0;
	}
	return 0;
}

int	ZPF( ZModem *info );
int	Ignore( ZModem *info );
int	AnswerChallenge( ZModem *info );
int	GotAbort( ZModem *info );
int	GotCancel( ZModem *info );
int	GotCommand( ZModem *info );
int	GotStderr( ZModem *info );
int	RetDone( ZModem *info );
int	GotCommandData( ZModem *info, int crcGood );
int	GotStderrData( ZModem *info );

/* PROTOCOL LOGIC:  This section of code handles the actual
* protocol.  This is also driven by a finite state machine
*
* State tables are sorted by approximate frequency order to
* reduce search time.
*/

/* Extra ZRINIT headers are the receiver trying to resync.  */



/* If compiling for Send Only or Receive Only, convert table
* entries to no-ops so we don't have to link zmodem[rt].o
*/

#if	SendOnly
#define	RStartOps	DoneOps
#define	RSinitWaitOps	DoneOps
#define	RFileNameOps	DoneOps
#define	RCrcOps		DoneOps
#define	RFileOps	DoneOps
#define	RDataOps	DoneOps
#define	RFinishOps	DoneOps
#define	GotFileName	Ignore
#define	ResendCrcReq	Ignore
#define	GotSinitData	Ignore
#define	ResendRpos	Ignore
#define	GotFileData	Ignore
#define	SendRinit	Ignore
#else
extern	StateTable	RStartOps[];
extern	StateTable	RSinitWaitOps[];
extern	StateTable	RFileNameOps[];
extern	StateTable	RCrcOps[];
extern	StateTable	RFileOps[];
extern	StateTable	RDataOps[];
extern	StateTable	RFinishOps[];
extern	int		GotFileName( ZModem *info, int crcGood );
extern	int		ResendCrcReq( ZModem *info );
extern	int		GotSinitData( ZModem *info, int crcGood );
extern	int		ResendRpos( ZModem *info );
extern	int		GotFileData( ZModem *info, int crcGood );
extern	int		SendRinit( ZModem *info );
#endif

#if	RcvOnly
#define	TStartOps	DoneOps
#define	TInitOps	DoneOps
#define	FileWaitOps	DoneOps
#define	CrcWaitOps	DoneOps
#define	SendingOps	DoneOps
#define	SendDoneOps	DoneOps
#define	SendWaitOps	DoneOps
#define	SendEofOps	DoneOps
#define	TFinishOps	DoneOps
#define	SendMoreFileData Ignore
#else
extern	StateTable	TStartOps[];
extern	StateTable	TInitOps[];
extern	StateTable	FileWaitOps[];
extern	StateTable	CrcWaitOps[];
extern	StateTable	SendingOps[];
extern	StateTable	SendDoneOps[];
extern	StateTable	SendWaitOps[];
extern	StateTable	SendEofOps[];
extern	StateTable	TFinishOps[];
extern	int		SendMoreFileData( ZModem *info );
#endif



static	StateTable	CommandDataOps[] = {
#ifdef	COMMENT
	{ZRQINIT,f,1,1},
	{ZRINIT,f,1,1},
	{ZSINIT,f,1,1},
	{ZACK,f,1,1},
	{ZFILE,f,1,1},
	{ZSKIP,f,1,1},
	{ZNAK,f,1,1},
	{ZABORT,f,1,1,TFinish},
	{ZFIN,f,1,1},
	{ZRPOS,f,1,1},
	{ZDATA,f,1,1},
	{ZEOF,f,1,1},
	{ZFERR,f,1,1,TFinish},
	{ZCRC,f,1,1},
	{ZCHALLENGE,f,1,1},
	{ZCOMPL,f,1,1},
	{ZCAN,f,1,1},
	{ZFREECNT,f,1,1},
	{ZCOMMAND,f,1,1},
	{ZSTDERR,f,1,1},
#endif	/* COMMENT */
	{99,ZPF,0,0,CommandData},
};

static	StateTable	CommandWaitOps[] =
{
#ifdef	COMMENT
	{ZRQINIT,f,1,1},
	{ZRINIT,f,1,1},
	{ZSINIT,f,1,1},
	{ZACK,f,1,1},
	{ZFILE,f,1,1},
	{ZSKIP,f,1,1},
	{ZNAK,f,1,1},
	{ZABORT,f,1,1,TFinish},
	{ZFIN,f,1,1},
	{ZRPOS,f,1,1},
	{ZDATA,f,1,1},
	{ZEOF,f,1,1},
	{ZFERR,f,1,1,TFinish},
	{ZCRC,f,1,1},
	{ZCHALLENGE,f,1,1},
	{ZCOMPL,f,1,1},
	{ZCAN,f,1,1},
	{ZFREECNT,f,1,1},
	{ZCOMMAND,f,1,1},
	{ZSTDERR,f,1,1},
#endif	/* COMMENT */
	{99,ZPF,0,0,CommandWait},
};

static	StateTable	StderrDataOps[] = {
#ifdef	COMMENT
	{ZRQINIT,f,1,1},
	{ZRINIT,f,1,1},
	{ZSINIT,f,1,1},
	{ZACK,f,1,1},
	{ZFILE,f,1,1},
	{ZSKIP,f,1,1},
	{ZNAK,f,1,1},
	{ZABORT,f,1,1,TFinish},
	{ZFIN,f,1,1},
	{ZRPOS,f,1,1},
	{ZDATA,f,1,1},
	{ZEOF,f,1,1},
	{ZFERR,f,1,1,TFinish},
	{ZCRC,f,1,1},
	{ZCHALLENGE,f,1,1},
	{ZCOMPL,f,1,1},
	{ZCAN,f,1,1},
	{ZFREECNT,f,1,1},
	{ZCOMMAND,f,1,1},
	{ZSTDERR,f,1,1},
#endif	/* COMMENT */
	{99,ZPF,0,0,StderrData},
};

static	StateTable	DoneOps[] =
{
	{99,ZPF,0,0,Done},
};


static	StateTable	*tables[] =
{
	RStartOps,
		RSinitWaitOps,
		RFileNameOps,
		RCrcOps,
		RFileOps,
		RDataOps,
		RDataOps,	/* RDataErr is the same as RData */
		RFinishOps,

		TStartOps,
		TInitOps,
		FileWaitOps,
		CrcWaitOps,
		SendingOps,
		SendWaitOps,
		SendDoneOps,
		SendEofOps,
		TFinishOps,

		CommandDataOps,
		CommandWaitOps,
		StderrDataOps,
		DoneOps,
};


char	*hdrnames[] =
{
	"ZRQINIT",
		"ZRINIT",
		"ZSINIT",
		"ZACK",
		"ZFILE",
		"ZSKIP",
		"ZNAK",
		"ZABORT",
		"ZFIN",
		"ZRPOS",
		"ZDATA",
		"ZEOF",
		"ZFERR",
		"ZCRC",
		"ZCHALLENGE",
		"ZCOMPL",
		"ZCAN",
		"ZFREECNT",
		"ZCOMMAND",
		"ZSTDERR",
};



/* This function is called (indirectly) by the ZmodemRcv()
* function when a full header has been received.
*/

int ZProtocol(  ZModem *info )
{
	StateTable *table;
#if defined(_DEBUG)
	zmodemlog(	"received %s: %2.2x %2.2x %2.2x %2.2x = %lx\n",
				hdrnames[info->hdrData[0]], info->hdrData[1],
				info->hdrData[2], info->hdrData[3], info->hdrData[4],
				ZDec4(info->hdrData+1));
#endif
	/* Flags are sent in F3 F2 F1 F0 order.  Data is sent in P0 P1 P2 P3 */

	info->timeoutCount = 0;
	info->noiseCount = 0;

	table = tables[(int)info->state];
	while( table->type != 99  &&  table->type != info->hdrData[0] )
	{
		++table;
	}
#if defined(_DEBUG)
	zmodemlog(	"  state %s => %s, iflush=%d, oflush=%d, call %x\n",
				sname(info), sname2(table->newstate), table->IFlush,
				table->OFlush, table->func);
#endif
	info->state = table->newstate;
	if( table->IFlush )
	{
		info->rcvlen = 0;
		ZIFlush(info);
	}
	if( table->OFlush )
	{
		ZOFlush(info);
	}

	return table->func(info);
}


int ZDataReceived(  ZModem *info, int crcGood )
{
	switch( info->state )
	{
	case RSinitWait: return GotSinitData(info, crcGood);
	case RFileName: return GotFileName(info, crcGood);
	case RData: return GotFileData(info, crcGood);
	case CommandData: return GotCommandData(info, crcGood);
	case StderrData: return GotStderrData(info /* , crcGood */ );
	default: return ZPF(info);
	}
}


int ZmodemTimeout(  ZModem *info )
{
	/* timed out while waiting for input */
	++info->timeoutCount;
#if defined(_DEBUG)
	zmodemlog("timeout %d [%s]\n", info->timeoutCount, sname(info) );
#endif
	switch( info->state )
	{
		/* receive */
	  case RStart:		/* waiting for INIT frame from other end */
		  if( info->timeoutCount > 4 )
		  {
			  return YmodemRInit(info);
		  }
	  case RSinitWait:
	  case RFileName:
		  if( info->timeout > 0 )
		  {
			  ZStatus(SndTimeout, info->timeoutCount, NULL);
		  }
		  if( info->timeoutCount > 4 )
		  {
			  return ZmErrRcvTo;
		  }
		  info->state = RStart;
		  return SendRinit(info);
	  case RCrc:
	  case RFile:
	  case RData:
		  ZStatus(SndTimeout, info->timeoutCount, NULL);
		  if( info->timeoutCount > 2 )
		  {
			  info->timeoutCount = 0;
			  info->state = RStart;
			  return SendRinit(info);
		  }
		  return info->state == RCrc ? ResendCrcReq(info) : ResendRpos(info);
	  case RFinish:
		  ZStatus(SndTimeout, info->timeoutCount, NULL);
		  return ZmDone;
	  case YRStart:
	  case YRDataWait:
	  case YRData:
	  case YREOF:
		  return YrcvTimeout(info);
		  /* transmit */
	  case TStart:		/* waiting for INIT frame from other end */
	  case TInit:		/* sent INIT, waiting for ZACK */
	  case FileWait:	/* sent file header, waiting for ZRPOS */
	  case CrcWait:		/* sent file crc, waiting for ZRPOS */
	  case SendWait:	/* waiting for ZACK */
	  case SendEof:		/* sent EOF, waiting for ZACK */
	  case TFinish:		/* sent ZFIN, waiting for ZFIN */
	  case YTStart:
	  case YTFile:
	  case YTDataWait:
	  case YTData:
	  case YTEOF:
	  case YTFin:
		  ZStatus(RcvTimeout,0,NULL);
		  return ZmErrRcvTo;
	  case Sending:		/* sending data subpackets, ready for int */
		  return SendMoreFileData(info);
		  /* general */
	  case CommandData:	/* waiting for command data */
	  case StderrData:	/* waiting for stderr data */
		  return ZmErrSndTo;
	  case CommandWait:	/* waiting for command to execute */
		  return ZmErrCmdTo;
	  case Done:
		  return ZmDone;
	  default:
		  return 0;
	}
}


int ZmodemAttention(  ZModem *info )
{
	/* attention received from remote end */
	if( info->state == Sending )
	{
		ZOFlush(info);
		info->interrupt = 1;
	}
	return 0;
}


int ZmodemAbort(  ZModem *info )
{
	static	u_char	canistr[] =
	{
		CAN,CAN,CAN,CAN,CAN,CAN,CAN,CAN,8,8,8,8,8,8,8,8,8,8
	};
	info->state = Done;
	ZIFlush(info);
	ZOFlush(info);
	return ZXmitStr(canistr, sizeof(canistr), info);
}



/* used to completely ignore headers */

int Ignore( ZModem *info )
{
	return 0;
}


/* ignore header contents, return ZmDone */

int RetDone( ZModem *info )
{
	return ZmDone;
}


/* ignore header contents, return ZmErrCancel */

int GotCancel( ZModem *info )
{
	return ZmErrCancel;
}


/* utility: set up to receive a data packet */

int dataSetup(  ZModem *info )
{
	info->InputState = ZModem::Indata;
	info->chrCount = 0;
	info->crcCount = 0;
	info->crc = (info->DataType != ZBIN32) ? 0 : 0xffffffffL;
	return 0;
}


/* called when a remote command received.  For now, we
* refuse to execute commands.  Send EPERM and ignore.
*/
int GotCommand( ZModem *info )
{
	u_char	rbuf[4];
	/* TODO: add command capability */

	rbuf[0] = EPERM;
	rbuf[1] = rbuf[2] = rbuf[3] = 0;
	return ZXmitHdrHex(ZCOMPL, rbuf, info);
}

int GotCommandData(  ZModem *info, int crcGood )
{
	/* TODO */
	return 0;
}



/* called when the remote system wants to put something to
* stderr
*/
int GotStderr(  ZModem *info )
{
	info->InputState = ZModem::Indata;
	info->chrCount = 0;
	return 0;
}

int GotStderrData(  ZModem *info )
{
	info->buffer[info->chrCount] = '\0';
	ZStatus(RemoteMessage, info->chrCount, reinterpret_cast<char*>( info->buffer ) );
	return 0;
}


/* Protocol failure:  An unexpected packet arrived.  This could
* be from many sources, such as old pipelined info finally arriving
* or a serial line with echo enabled.  Report it and ignore it.
*/

int ZPF( ZModem *info )
{
	info->waitflag = 1;	/* pause any in-progress transmission */
	ZStatus(ProtocolErr, info->hdrData[0], NULL);
	return 0;
}


int AnswerChallenge(  ZModem *info )
{
	return ZXmitHdrHex(ZACK, info->hdrData+1, info);
}


int GotAbort(  ZModem *info )
{
	ZStatus(RmtCancel, 0, NULL);
	return ZXmitHdrHex(ZFIN, zeros, info);
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
