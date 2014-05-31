/**
*	ZMODEMR - receive side of zmodem protocol
*
* receive side of zmodem protocol
*
* This code is designed to be called from inside a larger
* program, so it is implemented as a state machine where
* practical.
*
* functions:
*
*	ZmodemRInit(ZModem *info)
*		Initiate a connection
*
*	ZmodemRAbort(ZModem *info)
*		abort transfer
*
* all functions return 0 on success, 1 on failure
*
*
*	Copyright (c) 1995 by Edward A. Falk
*
*	January, 1995
*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <errno.h>

#include "zmodem.h"
#include "crctab.h"
#include "wstringutils.h"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4706 4127 4244 4100 )
#endif

//extern	int	errno;

int	ZWriteFile(u_char *buffer, int len, FILE *, ZModem *);
int	ZCloseFile(ZModem *info);
void ZFlowControl(int onoff, ZModem *info);

static	u_char	zeros[4] = {0,0,0,0};


int ZmodemRInit(ZModem *info) {
	info->packetCount = 0;
	info->offset = 0;
	info->errCount = 0;
	info->escCtrl = info->escHibit = info->atSign = info->escape = 0;
	info->InputState = ZModem::Idle;
	info->canCount = info->chrCount = 0;
	info->filename = NULL;
	info->interrupt = 0;
	info->waitflag = 0;
	info->attn = NULL;
	info->file = NULL;

	info->buffer = (u_char *)malloc(8192);

	info->state = RStart;
	info->timeoutCount = 0;

	ZIFlush(info);

	/* Don't send ZRINIT right away, there might be a ZRQINIT in
	* the input buffer.  Instead, set timeout to zero and return.
	* This will allow ZmodemRcv() to check the input stream first.
	* If nothing found, a ZRINIT will be sent immediately.
	*/
	info->timeout = 0;
#if defined(_DEBUG)
	zmodemlog("ZmodemRInit[%s]: flush input, new state = RStart\n",
	          sname(info));
#endif
	return 0;
}

int YmodemRInit(ZModem *info) {
	info->errCount = 0;
	info->InputState = ZModem::Yrcv;
	info->canCount = info->chrCount = 0;
	info->noiseCount = 0;
	info->filename = NULL;
	info->file = NULL;

	if( info->buffer == NULL ) {
		info->buffer = (u_char *)malloc(1024);
	}

	info->state = YRStart;
	info->packetCount = -1;
	info->timeoutCount = 0;
	info->timeout = 10;
	info->offset = 0;

	ZIFlush(info);

	return ZXmitStr((u_char *)"C", 1, info);
}





extern	int	ZPF( ZModem *info );
extern	int	Ignore( ZModem *info );
extern	int	GotCommand( ZModem *info );
extern	int	GotStderr( ZModem *info );

int	SendRinit( ZModem *info );
int	GotSinit( ZModem *info );
int	GotFile( ZModem *info );
int	GotFin( ZModem *info );
int	GotData( ZModem *info );
int	GotEof( ZModem *info );
int	GotFreecnt( ZModem *info );
int	GotFileCrc( ZModem *info );
int	ResendCrcReq( ZModem *info );
int	ResendRpos( ZModem *info );

/* sent ZRINIT, waiting for ZSINIT or ZFILE */
StateTable	RStartOps[] = {
	{ZSINIT,GotSinit,0,1,RSinitWait},	/* SINIT, wait for attn str */
	{ZFILE,GotFile,0,0,RFileName},	/* FILE, wait for filename */
	{ZRQINIT,SendRinit,0,1,RStart},	/* sender confused, resend */
	{ZFIN,GotFin,1,0,RFinish},		/* sender shutting down */
	{ZNAK,SendRinit,1,0,RStart},		/* RINIT was bad, resend */
#ifdef	TODO
	{ZCOMPL,f,1,1,s},
#endif	/* TODO */
	{ZFREECNT,GotFreecnt,0,0,RStart},	/* sender wants free space */
	{ZCOMMAND,GotCommand,0,0,CommandData}, /* sender wants command */
	{ZSTDERR,GotStderr,0,0,StderrData},	/* sender wants to send msg */
	{99,ZPF,0,0,RStart},			/* anything else is an error */
};

StateTable	RSinitWaitOps[] = {	/* waiting for data */
	{99,ZPF,0,0,RSinitWait},
};

StateTable	RFileNameOps[] = {	/* waiting for file name */
	{99,ZPF,0,0,RFileName},
};

StateTable	RCrcOps[] = {		/* waiting for CRC */
	{ZCRC,GotFileCrc,0,0,RFile},		  /* sender sent it */
	{ZNAK,ResendCrcReq,0,0,RCrc},		  /* ZCRC was bad, resend */
	{ZRQINIT,SendRinit,1,1,RStart},	  /* sender confused, restart */
	{ZFIN,GotFin,1,1,RFinish},		  /* sender signing off */
	{99,ZPF,0,0,RCrc},
};

StateTable	RFileOps[] = {		/* waiting for ZDATA */
	{ZDATA,GotData,0,0,RData},		  /* got it */
	{ZNAK,ResendRpos,0,0,RFile},		  /* ZRPOS was bad, resend */
	{ZEOF,GotEof,0,0,RStart},		  /* end of file */
	{ZRQINIT,SendRinit,1,1,RStart},	  /* sender confused, restart */
	{ZFILE,ResendRpos,0,0,RFile},		  /* ZRPOS was bad, resend */
	{ZFIN,GotFin,1,1,RFinish},		  /* sender signing off */
	{99,ZPF,0,0,RFile},
};

/* waiting for data, but a packet could possibly arrive due
* to error recovery or something
*/
StateTable	RDataOps[] = {
	{ZRQINIT,SendRinit,1,1,RStart},	/* sender confused, restart */
	{ZFILE,GotFile,0,1,RFileName},	/* start a new file (??) */
	{ZNAK,ResendRpos,1,1,RFile},		/* ZRPOS was bad, resend */
	{ZFIN,GotFin,1,1,RFinish},		/* sender signing off */
	{ZDATA,GotData,0,1,RData},		/* file data follows */
	{ZEOF,GotEof,1,1,RStart},		/* end of file */
	{99,ZPF,0,0,RData},
};

/* here if we've sent ZFERR or ZABORT.  Waiting for ZFIN */

StateTable	RFinishOps[] = {
	{ZRQINIT,SendRinit,1,1,RStart},	/* sender confused, restart */
	{ZFILE,GotFile,1,1,RFileName},	/* start a new file */
	{ZNAK,GotFin,1,1,RFinish},		/* resend ZFIN */
	{ZFIN,GotFin,1,1,RFinish},		/* sender signing off */
	{99,ZPF,0,0,RFinish},
};




extern	const char	*hdrnames[];

extern	int	dataSetup( ZModem *info );


/* RECEIVE-RELATED STUFF BELOW HERE */


/* resend ZRINIT header in response to ZRQINIT or ZNAK header */

int SendRinit( ZModem *info ) {
	u_char	dbuf[4];

#ifdef	COMMENT
	if( info->timeoutCount >= 5 )
		/* TODO: switch to Ymodem */
#endif	/* COMMENT */
#if defined(_DEBUG)
		zmodemlog("SendRinit[%s]: send ZRINIT\n", sname(info));
#endif
	info->timeout = ResponseTime;
	dbuf[0] = info->bufsize&0xff;
	dbuf[1] = (info->bufsize>>8)&0xff;
	dbuf[2] = 0;
	dbuf[3] = info->zrinitflags;
	return ZXmitHdrHex(ZRINIT, dbuf, info);
}



/* received a ZSINIT header in response to ZRINIT */

int GotSinit( ZModem *info ) {
#if defined(_DEBUG)
	zmodemlog("GotSinit[%s]: call dataSetup\n", sname(info));
#endif
	info->zsinitflags = info->hdrData[4];
	info->escCtrl = info->zsinitflags & TESCCTL;
	info->escHibit = info->zsinitflags & TESC8;
	ZFlowControl(1, info);
	return dataSetup(info);
}

/* received rest of ZSINIT packet */

int GotSinitData( ZModem *info, int crcGood ) {
	info->InputState = ZModem::Idle;
	info->chrCount=0;
	info->state = RStart;

#if defined(_DEBUG)
	zmodemlog("GotSinitData[%s]: crcGood=%d\n", sname(info), crcGood);
#endif

	if( !crcGood ) {
		return ZXmitHdrHex(ZNAK, zeros, info);
	}

	if( info->attn != NULL ) {
		free(info->attn);
	}
	info->attn = NULL;
	if( info->buffer[0] != '\0' ) {
		info->attn = WWIV_STRDUP(reinterpret_cast<char*>( info->buffer ) );
	}
	return ZXmitHdrHex(ZACK, ZEnc4(SerialNo), info);
}


/* got ZFILE.  Cache flags and set up to receive filename */

int GotFile( ZModem *info ) {
#if defined(_DEBUG)
	zmodemlog("GotFile[%s]: call dataSetup\n", sname(info));
#endif

	info->errCount = 0;
	info->f0 = info->hdrData[4];
	info->f1 = info->hdrData[3];
	info->f2 = info->hdrData[2];
	info->f3 = info->hdrData[1];
	return dataSetup(info);
}


/* utility: see if ZOpenFile wants this file, and if
* so, request it from sender.
*/

int requestFile( ZModem *info, u_long crc ) {
	info->file = ZOpenFile( reinterpret_cast<char*>( info->buffer ), crc, info);

	if( info->file == NULL ) {
#if defined(_DEBUG)
		zmodemlog("requestFile[%s]: send ZSKIP\n", sname(info));
#endif
		info->state = RStart;
		ZStatus(FileSkip, 0, info->filename);
		return ZXmitHdrHex(ZSKIP, zeros, info);
	} else {
#if defined(_DEBUG)
		zmodemlog("requestFile[%s]: send ZRPOS(%ld)\n",
		          sname(info), info->offset);
#endif
		info->offset = info->f0 == ZCRESUM ? ftell(info->file) : 0;
		info->state = RFile;
		ZStatus(FileBegin, 0, info->filename);
		return ZXmitHdrHex(ZRPOS, ZEnc4(info->offset), info);
	}
}


/* parse filename info. */

void parseFileName( ZModem *info, char *fileinfo ) {
	char	*ptr;
	int	serial=0;

	info->len = info->mode = info->filesRem = info->bytesRem = info->fileType = 0;
	ptr = fileinfo + strlen(fileinfo) + 1;
	if( info->filename != NULL ) {
		free(info->filename);
	}
	info->filename = WWIV_STRDUP(fileinfo);
	sscanf(ptr, "%d %lo %o %o %d %d %d", &info->len, &info->date,
	       &info->mode, &serial, &info->filesRem, &info->bytesRem,
	       &info->fileType);
}


/* got filename.  Parse arguments from it and execute
* policy function ZOpenFile(), provided by caller
*/

int GotFileName( ZModem *info, int crcGood ) {
	info->InputState = ZModem::Idle;
	info->chrCount=0;

//#ifdef CHECK_FILENAME_CRC
	if( !crcGood ) {
#if defined(_DEBUG)
		zmodemlog("GotFileName[%s]: bad crc, send ZNAK\n", sname(info));
#endif
		info->state = RStart;
		return ZXmitHdrHex(ZNAK, zeros, info);
	}
//#endif // CHECK_FILENAME_CRC

	parseFileName(info, reinterpret_cast<char*>( info->buffer ) );

	if( (info->f1 & ZMMASK) == ZMCRC ) {
		info->state = RCrc;
		return ZXmitHdrHex(ZCRC, zeros, info);
	}

#if defined(_DEBUG)
	zmodemlog(	"GotFileName[%s]: good crc, call requestFile\n",
	            sname(info));
#endif
	info->state = RFile;
	return requestFile(info,0);
}


int ResendCrcReq(ZModem *info) {
#if defined(_DEBUG)
	zmodemlog("ResendCrcReq[%s]: send ZCRC\n", sname(info));
#endif
	return ZXmitHdrHex(ZCRC, zeros, info);
}


/* received file CRC, now we're ready to accept or reject */

int GotFileCrc( ZModem *info ) {
#if defined(_DEBUG)
	zmodemlog("GotFileCrc[%s]: call requestFile\n", sname(info));
#endif
	return requestFile(info, ZDec4(info->hdrData+1));
}


/* last ZRPOS was bad, resend it */

int ResendRpos( ZModem *info ) {
#if defined(_DEBUG)
	zmodemlog("ResendRpos[%s]: send ZRPOS(%ld)\n",
	          sname(info), info->offset);
#endif
	return ZXmitHdrHex(ZRPOS, ZEnc4(info->offset), info);
}


/* recevied ZDATA header */

int GotData( ZModem *info ) {
	int	err;
#if defined(_DEBUG)
	zmodemlog("GotData[%s]:\n", sname(info));
#endif
	if( ZDec4(info->hdrData+1) != info->offset ) {
		if( info->attn != NULL  &&  (err=ZAttn(info)) != 0 )
			return err;
#if defined(_DEBUG)
		zmodemlog("  bad, send ZRPOS(%ld)\n", info->offset);
#endif
		return ZXmitHdrHex(ZRPOS, ZEnc4(info->offset), info);
	}

	/* Let's do it! */
#if defined(_DEBUG)
	zmodemlog("  call dataSetup\n");
#endif
	return dataSetup(info);
}


/* Utility: flush input, send attn, send specified header */

int fileError( ZModem *info, int type, int data ) {
	int	err;

	info->InputState = ZModem::Idle;
	info->chrCount=0;

	if( info->attn != NULL  &&  (err=ZAttn(info)) != 0 ) {
		return err;
	}
	return ZXmitHdrHex(type, ZEnc4(data), info);
}

/* received file data */

int GotFileData( ZModem *info, int crcGood ) {
	/* OK, now what?  Fushing the buffers and executing the
	* attn sequence has likely chopped off the input stream
	* mid-packet.  Now we switch to idle mode and treat all
	* incoming stuff like noise until we get a new valid
	* packet.
	*/

	if( !crcGood ) {
		/* oh bugger, an error. */
#if defined(_DEBUG)
		zmodemlog( "GotFileData[%s]: bad crc, send ZRPOS(%ld), new state = RFile\n",
		           sname(info), info->offset);
#endif
		ZStatus(DataErr, ++info->errCount, NULL);
		if( info->errCount > MaxErrs ) {
			ZmodemAbort(info);
			return ZmDataErr;
		} else {
			info->state = RFile;
			info->InputState = ZModem::Idle;
			info->chrCount=0;
			return fileError(info, ZRPOS, info->offset);
		}
	}

	if( ZWriteFile(info->buffer, info->chrCount, info->file, info) ) {
		/* RED ALERT!  Could not write the file. */
		ZStatus(FileErr, errno, NULL);
		info->state = RFinish;
		info->InputState = ZModem::Idle;
		info->chrCount=0;
		return fileError(info, ZFERR, errno);
	}

#if defined(_DEBUG)
	zmodemlog("GotFileData[%s]: %ld.%d,",
	          sname(info), info->offset, info->chrCount);
#endif
	info->offset += info->chrCount;
	ZStatus(RcvByteCount, info->offset, NULL);

	/* if this was the last data subpacket, leave data mode */
	if( info->PacketType == ZCRCE  ||  info->PacketType == ZCRCW ) {
#if defined(_DEBUG)
		zmodemlog("  ZCRCE|ZCRCW, new state RFile");
#endif
		info->state = RFile;
		info->InputState = ZModem::Idle;
		info->chrCount=0;
	} else {
#if defined(_DEBUG)
		zmodemlog("  call dataSetup");
#endif
		dataSetup(info);
	}

	if( info->PacketType == ZCRCQ || info->PacketType == ZCRCW ) {
#if defined(_DEBUG)
		zmodemlog(",  send ZACK\n");
#endif
		return ZXmitHdrHex(ZACK, ZEnc4(info->offset), info);
	}
#if defined(_DEBUG)
	else {
		zmodemlog("\n");
	}
#endif

	return 0;
}


/* received ZEOF packet, file is now complete */

int GotEof( ZModem *info ) {
#if defined(_DEBUG)
	zmodemlog("GotEof[%s]: offset=%ld\n", sname(info), info->offset);
#endif
	if( ZDec4(info->hdrData+1) != info->offset ) {
#if defined(_DEBUG)
		zmodemlog("  bad length, state = RFile\n");
#endif
		info->state = RFile;
		return 0;		/* it was probably spurious */
	}

	/* TODO: if we can't close the file, send a ZFERR */
	ZCloseFile(info);
	info->file = NULL;
	ZStatus(FileEnd, 0, info->filename);
	if( info->filename != NULL ) {
		free(info->filename);
		info->filename = NULL;
	}
	return SendRinit(info);
}



/* got ZFIN, respond in kind */

int GotFin( ZModem *info ) {
#if defined(_DEBUG)
	zmodemlog("GotFin[%s]: send ZFIN\n", sname(info));
#endif
	info->InputState = ZModem::Finish;
	info->chrCount = 0;
	if( info->filename != NULL ) {
		free(info->filename);
	}
	return ZXmitHdrHex(ZFIN, zeros, info);
}



int GotFreecnt( ZModem *info ) {
	/* TODO: how do we find free space on system? */
	return ZXmitHdrHex(ZACK, ZEnc4(0xffffffff), info);
}



/* YMODEM */

static	u_char	AckStr[1] = {ACK};
static	u_char	NakStr[1] = {NAK};
static	u_char	CanStr[2] = {CAN,CAN};

int	ProcessPacket( ZModem *info );
int	acceptPacket( ZModem *info );
int	rejectPacket( ZModem *info );
int	calcCrc( u_char *str, int len );

int YrcvChar( char c, ZModem *info ) {
	int	err;

	if( info->canCount >= 2 ) {
		ZStatus(RmtCancel, 0, NULL);
		return ZmErrCancel;
	}

	switch( info->state ) {
	case YREOF:
		if( c == EOT ) {
			ZCloseFile(info);
			info->file = NULL;
			ZStatus(FileEnd, 0, info->filename);
			if( info->filename != NULL ) {
				free(info->filename);
			}
			if( (err = acceptPacket(info)) != 0 ) {
				return err;
			}
			info->packetCount = -1;
			info->offset = 0;
			info->state = YRStart;
			return ZXmitStr((u_char *)"C", 1, info);
		}
	/* else, drop through */

	case YRStart:
	case YRDataWait:
		switch( c ) {
		case SOH:
		case STX:
			info->pktLen = c == SOH ? (128+4) : (1024+4);
			info->state = YRData;
			info->chrCount = 0;
			info->timeout = 1;
			info->noiseCount = 0;
			info->crc = 0;
			break;
		case EOT:
			/* ignore first EOT to protect against false eot */
			info->state = YREOF;
			return rejectPacket(info);
		default:
			if( ++info->noiseCount > 135 ) {
				return ZXmitStr(NakStr, 1, info);
			}
			break;
		}
		break;
	case YRData:
		info->buffer[info->chrCount++] = c;
		if( info->chrCount >= info->pktLen ) {
			return ProcessPacket(info);
		}
		break;
	default:
		break;
	}

	return 0;
}


int YrcvTimeout( ZModem *info ) {
	switch( info->state ) {
	case YRStart:
		if( info->timeoutCount >= 10 ) {
			ZXmitStr(CanStr, 2, info);
			return ZmErrInitTo;
		}
		return ZXmitStr((u_char *)"C", 1, info);

	case YRDataWait:
	case YREOF:
	case YRData:
		if( info->timeoutCount >= 10 ) {
			ZXmitStr(CanStr, 2, info);
			return ZmErrRcvTo;
		}
		return ZXmitStr(NakStr, 1, info);
	default:
		return 0;
	}
}



int ProcessPacket( ZModem *info ) {
	int	idx = (u_char) info->buffer[0];
	int	idxc = (u_char) info->buffer[1];
	int	crc0, crc1;
	int	err;

	info->state = YRDataWait;

	if( idxc != 255 - idx ) {
		ZStatus(DataErr, ++info->errCount, NULL);
		return rejectPacket(info);
	}

	if( idx == (info->packetCount%256) ) {	/* quietly ignore dup */
		return acceptPacket(info);
	}

	if( idx != (info->packetCount+1)%256 ) {
		/* out of sequence */
		ZXmitStr(CanStr, 2, info);
		return ZmErrSequence;
	}

	crc0 =	(u_char)info->buffer[info->pktLen-2] << 8 |
	        (u_char)info->buffer[info->pktLen-1];
	crc1 = calcCrc(info->buffer+2, info->pktLen-4);
	if( crc0 != crc1 ) {
		ZStatus(DataErr, ++info->errCount, NULL);
		return rejectPacket(info);
	}

	++info->packetCount;

	if( info->packetCount == 0 ) {	/* packet 0 is filename */
		if( info->buffer[2] == '\0' ) {
			/* null filename is FIN */
			acceptPacket(info);
			return ZmDone;
		}

		parseFileName(info, reinterpret_cast<char*>( info->buffer + 2 ) );
		info->file = ZOpenFile(info->filename, 0, info);
		if( info->file == NULL ) {
			ZXmitStr(CanStr, 2, info);
			return ZmErrCantOpen;
		}
		if( (err = acceptPacket(info)) != 0 ) {
			return err;
		}
		return ZXmitStr((u_char *)"C", 1, info);
	}


	if( ZWriteFile(info->buffer+2, info->pktLen-4, info->file, info) ) {
		ZStatus(FileErr, errno, NULL);
		ZXmitStr(CanStr, 2, info);
		return ZmErrSys;
	}
	info->offset += info->pktLen-4;
	ZStatus(RcvByteCount, info->offset, NULL);

	acceptPacket(info);
	return 0;
}


int rejectPacket( ZModem *info ) {
	info->timeout = 10;
	return ZXmitStr(NakStr, 1, info);
}


int acceptPacket( ZModem *info ) {
	info->state = YRDataWait;
	info->timeout = 10;
	return ZXmitStr(AckStr, 1, info);
}


int calcCrc( u_char *str, int len ) {
	int	crc = 0;
	while( --len >= 0 ) {
		crc = updcrc(*str++, crc);
	}
	crc = updcrc(0,crc);
	crc = updcrc(0,crc);
	return crc & 0xffff;
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
