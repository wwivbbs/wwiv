/**
*	ZMODEMT - transmit side of zmodem protocol
*
* transmit side of zmodem protocol
*
* Caller sets flags defined in zmodem.h as appropriate.
* (default is basic zmodem)
*
* functions:
*
*	ZmodemTInit(ZModem *info)
*	YmodemTInit(ZModem *info)
*	XmodemTInit(ZModem *info)
*		Initiate a connection
*
*	ZmodemTFile(char *filename, u_char flags[4], ZModem *info)
*		Initiate a file transfer.  Flags are as specified
*		under "ZCBIN" in zmodem.h
*
*	ZmodemTFinish(ZModem *info)
*		last file
*
*	ZmodemTAbort(ZModem *info)
*		abort transfer
*
* all functions return 0 on success, 1 on abort
*
*	Copyright (c) 1995 by Edward A. Falk
*	January, 1995
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "zmodem.h"
#include "crctab.h"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4706 4127 4244 4100 )
#endif

int	SendMoreFileData( ZModem *info );
extern	int	ZXmitData(int, int, u_char, u_char *data, ZModem *);
extern	void	ZFlowControl(int onoff, ZModem *info);

int	YXmitData( u_char *buffer, int len, ZModem *info );
int	YSendFilename( ZModem *info );
int	YSendData( ZModem *info );
int	YSendFin( ZModem *info );

int	sendFilename( ZModem *info );
int SendZSInit( ZModem *info );

static u_char	zeros[4] = {0,0,0,0};

int	ZPF( ZModem *info );
int	AnswerChallenge( ZModem *info );
int	GotAbort( ZModem *info );
int	Ignore( ZModem *info );
int	RetDone( ZModem *info );
int	GotCommand( ZModem *info );
int	GotStderr( ZModem *info );

int	GotRinit( ZModem *info );
int	SendZSInit( ZModem *info );
int	SendFileCrc( ZModem *info );
int	GotSendAck( ZModem *info );
int	GotSendDoneAck( ZModem *info );
int	GotSendNak( ZModem *info );
int	GotSendWaitAck( ZModem *info );
int	SkipFile( ZModem *info );
int	GotSendPos( ZModem *info );
int	SendFileData( ZModem *info );
int	ResendEof( ZModem *info );
int	OverAndOut( ZModem *info );

/* sent ZRQINIT, waiting for response */
StateTable	TStartOps[] = {
	{ZRINIT,GotRinit,1,1,TStart},
	{ZCHALLENGE,AnswerChallenge,1,0,TStart},
	{ZABORT,GotAbort,1,1,TFinish},
	{ZFERR,GotAbort,1,1,TFinish},
	{ZNAK,Ignore,0,0,TStart},
	{ZCOMMAND,GotCommand,0,0,CommandData},
	{ZSTDERR,GotStderr,0,0,StderrData},
	{99,ZPF,0,0,TStart},
};

/* sent ZSINIT, waiting for response */
StateTable	TInitOps[] = {
	{ZACK,RetDone,1,0,TInit},
	{ZNAK,SendZSInit,1,0,TInit},
	{ZRINIT,GotRinit,1,1,TInit},	/* redundant, but who cares */
	{ZCHALLENGE,AnswerChallenge,1,0,TInit},
	{ZABORT,GotAbort,1,1,TFinish},
	{ZFERR,GotAbort,1,1,TFinish},
	{ZCOMMAND,GotCommand,0,0,CommandData},
	{ZSTDERR,GotStderr,0,0,StderrData},
	{99,ZPF,0,0,TInit},
};

/* sent ZFILE, waiting for response */
StateTable	FileWaitOps[] = {
	{ZRPOS,SendFileData,1,0,Sending},
	{ZSKIP,SkipFile,1,0,FileWait},
	{ZCRC,SendFileCrc,1,0,FileWait},
	{ZNAK,sendFilename,1,0,FileWait},
	{ZRINIT,sendFilename,1,1,FileWait},	/* rcvr confused, retry file */
	{ZABORT,GotAbort,1,1,TFinish},
	{ZFERR,GotAbort,1,1,TFinish},
	{ZCHALLENGE,AnswerChallenge,1,0,FileWait},
	{ZCOMMAND,GotCommand,0,0,CommandData},
	{ZSTDERR,GotStderr,0,0,StderrData},
	{99,ZPF,0,0,FileWait},
};

/* sent file CRC, waiting for response */
StateTable	CrcWaitOps[] = {
	{ZRPOS,SendFileData,1,0,Sending},
	{ZSKIP,SkipFile,1,0,FileWait},
	{ZNAK,SendFileCrc,1,0,CrcWait},
	{ZRINIT,sendFilename,1,1,FileWait},	/* rcvr confused, retry file */
	{ZABORT,GotAbort,1,1,TFinish},
	{ZFERR,GotAbort,1,1,TFinish},
	{ZCRC,SendFileCrc,0,0,CrcWait},
	{ZCHALLENGE,AnswerChallenge,0,0,CrcWait},
	{ZCOMMAND,GotCommand,0,0,CommandData},
	{ZSTDERR,GotStderr,0,0,StderrData},
	{99,ZPF,0,0,CrcWait},
};

/* sending data, interruptable */
StateTable	SendingOps[] = {
	{ZACK,GotSendAck,0,0,Sending},
	{ZRPOS,GotSendPos,1,1,Sending},
	{ZSKIP,SkipFile,1,1,FileWait},
	{ZNAK,GotSendNak,1,1,Sending},
	{ZRINIT,sendFilename,1,1,FileWait},	/* rcvr confused, retry file */
	{ZABORT,GotAbort,1,1,TFinish},
	{ZFERR,GotAbort,1,1,TFinish},
	{99,ZPF,0,0,SendWait},
};

/* sent data, need to send EOF */
StateTable	SendDoneOps[] = {
	{ZACK,GotSendDoneAck,0,0,SendWait},
	{ZRPOS,GotSendPos,1,1,Sending},
	{ZSKIP,SkipFile,1,1,FileWait},
	{ZNAK,GotSendNak,1,1,Sending},
	{ZRINIT,sendFilename,1,1,FileWait},	/* rcvr confused, retry file */
	{ZABORT,GotAbort,1,1,TFinish},
	{ZFERR,GotAbort,1,1,TFinish},
	{99,ZPF,0,0,SendWait},
};

/* sending data, waiting for ACK */
StateTable	SendWaitOps[] = {
	{ZACK,GotSendWaitAck,0,0,Sending},
	{ZRPOS,GotSendPos,0,0,SendWait},
	{ZSKIP,SkipFile,1,1,FileWait},
	{ZNAK,GotSendNak,0,0,Sending},
	{ZRINIT,sendFilename,1,1,FileWait},	/* rcvr confused, retry file */
	{ZABORT,GotAbort,1,1,TFinish},
	{ZFERR,GotAbort,1,1,TFinish},
	{99,ZPF,0,0,SendWait},
};

/* sent ZEOF, waiting for new RINIT */
StateTable	SendEofOps[] = {
	{ZRINIT,SkipFile,1,0,TStart},	/* successful completion */
	{ZACK,Ignore,0,0,SendEof},	/* probably ACK from last packet */
	{ZRPOS,GotSendPos,1,1,SendWait},
	{ZSKIP,SkipFile,1,1,TStart},
	{ZNAK,ResendEof,1,0,SendEof},
	{ZRINIT,sendFilename,1,1,FileWait},	/* rcvr confused, retry file */
	{ZABORT,GotAbort,1,1,TFinish},
	{ZFERR,GotAbort,1,1,TFinish},
	{99,ZPF,0,0,SendEof},
};

StateTable	TFinishOps[] = {
	{ZFIN,OverAndOut,1,1,Done},
	{ZNAK,ZmodemTFinish,1,1,TFinish},
	{ZRINIT,ZmodemTFinish,1,1,TFinish},
	{ZABORT,GotAbort,1,1,TFinish},
	{ZFERR,GotAbort,1,1,TFinish},
	{99,ZPF,0,0,TFinish},
};


extern	char	*hdrnames[];



/* called by user to establish protocol */

int ZmodemTInit( ZModem *info )
{
	info->state = TStart;
	info->Protocol = ZModem::ZMODEM;
	info->crc32 = 0;
	info->packetCount = 0;
	info->errCount = 0;
	info->escCtrl = info->escHibit = info->atSign = info->escape = 0;
	info->InputState = ZModem::Idle;
	info->canCount = info->chrCount = 0;
	info->windowCount = 0;
	info->filename = NULL;
	info->bufsize = 0;
	info->interrupt = 0;
	info->waitflag = 0;

	if( info->packetsize == 0 )
	{
		info->packetsize = 1024;
	}

	/* we won't be receiving much data, pick a reasonable buffer
	* size (largest packet will do)
	*/

	int i = info->packetsize*2;
	if( i < 1024 ) i = 1024;
	info->buffer = (u_char *)malloc(i);

	ZIFlush(info);

	/* optional: send "rz\r" to remote end */
	int err = 0;
	if( DoInitRZ )
	{
		if( (err = ZXmitStr((u_char *)"rz\r", 3, info)) )
		{
			return err;
		}
	}

	if( (err = ZXmitHdr(ZRQINIT, ZHEX, zeros, info)) ) /* nudge receiver */
	{
		return err;
	}

	info->timeout = 60;

#if defined(_DEBUG)
	zmodemlog("ZmodemTInit[%s]: sent ZRQINIT\n", sname(info));
#endif

	return 0;
}


/* called by user to establish Ymodem protocol */

int YmodemTInit( ZModem *info )
{
	info->state = YTStart;
	info->Protocol = ZModem::YMODEM;
	info->errCount = 0;
	info->InputState = ZModem::Ysend;
	info->canCount = info->chrCount = 0;
	info->windowCount = 0;
	info->filename = NULL;

	if( info->packetsize != 1024 )
	{
		info->packetsize = 128;
	}

	info->buffer = (u_char *)malloc(1024);

	ZIFlush(info);
	ZFlowControl(0, info);

	info->timeout = 60;

	return 0;
}



/* called by user to establish Xmodem protocol */
int XmodemTInit( ZModem *info )
{
	YmodemTInit( info );
	info->Protocol = ZModem::XMODEM;
	return 0;
}



/* called by user to begin transmission of a file */

int ZmodemTFile(
				char	*file,
				char	*rfile,
				u_int	f0,
				u_int	f1,
				u_int	f2,
				u_int	f3,
				int	filesRem,
				int	bytesRem,
				ZModem	*info )
{
	if( file == NULL || (info->file = fopen(file, "rb")) == NULL )
	{
		return ZmErrCantOpen;
	}

	info->fileEof = 0;
	info->filename = file;
	info->rfilename = (rfile != NULL) ? rfile : const_cast<char *>("noname");
	info->filesRem = filesRem;
	info->bytesRem = bytesRem;
	info->fileFlags[3] = f0;
	info->fileFlags[2] = f1;
	info->fileFlags[1] = f2;
	info->fileFlags[0] = f3;
	info->offset = info->lastOffset = 0;
	info->len = info->date = info->fileType = info->mode = 0;
	if( info->filename != NULL )
	{
		struct stat buf;
		if( stat(info->filename, &buf) == 0 ) {
			info->len = buf.st_size;
			info->date = buf.st_mtime;
			info->fileType = 0;
			info->mode = (buf.st_mode&0777)|0100000;
		}
	}

	if( info->Protocol == ZModem::XMODEM )
		return YSendData(info);

	if( info->Protocol == ZModem::YMODEM )
	{
		return YSendFilename(info);
	}

	info->state = FileWait;
#if defined(_DEBUG)
	zmodemlog("ZmodemTFile[%s]: send ZFILE(%s)\n",
		sname(info), info->rfilename);
#endif
	return sendFilename(info);
}



/* send ZFILE header and filename & info.  Wait for response
* from receiver.
*/
int sendFilename( ZModem *info )
{
	u_char	obuf[2048];
	u_char	*ptr = obuf;

	info->state = FileWait;

	int	err = ZXmitHdr(ZFILE, ZBIN, info->fileFlags, info);
	if( err )
	{
		return err;
	}

	int i = strlen(info->rfilename);
	memcpy(ptr, info->rfilename, i+1);  ptr += i+1;
	sprintf( reinterpret_cast<char*>( ptr ) , "%d %lo %o 0 %d %d 0", info->len, info->date,
		info->mode, info->filesRem, info->bytesRem);
	ptr += strlen( reinterpret_cast<char*>( ptr ) );
	*ptr++ = '\0';

	return ZXmitData(ZBIN, ptr-obuf, ZCRCW, obuf, info);
}



/* called by user when there are no more files to send */

int ZmodemTFinish( ZModem *info )
{
	if( info->Protocol == ZModem::XMODEM )
	{
		return ZmDone;
	}

	if( info->Protocol == ZModem::YMODEM )
	{
		return YSendFin(info);
	}

	info->state = TFinish;
	if( info->buffer != NULL )
	{
		free(info->buffer);
		info->buffer = NULL;
	}
#if defined(_DEBUG)
	zmodemlog("ZmodemTFinish[%s]: send ZFIN\n", sname(info));
#endif
	return ZXmitHdr(ZFIN, ZHEX, zeros, info);
}





/* Received Rinit.  Usually received while in start state,
* this can also be an attempt to resync after a protocol
* failure
*/
int GotRinit( ZModem *info )
{
	info->bufsize = info->hdrData[1] + info->hdrData[2]*256;
	info->zrinitflags = info->hdrData[4] + info->hdrData[3]*256;
	info->crc32 = info->zrinitflags & CANFC32;
	info->escCtrl = info->zrinitflags & ESCCTL;
	info->escHibit = info->zrinitflags & ESC8;

	/* Full streaming: If receiver can overlap I/O, and if
	* the sender can sample the reverse channel without hanging,
	* and the receiver has not specified a buffer size, then we
	* can simply blast away with ZCRCG packets.  If the receiver
	* detects an error, it sends an attn sequence and a new ZRPOS
	* header to restart the file where the error occurred.
	*
	* [note that zmodem8.doc does not define how much noise is
	* required to trigger a ZCRCW packet.  We arbitrarily choose
	* 64 bytes]
	*
	* If 'windowsize' is nonzero, and the receiver can do full
	* duplex, ZCRCQ packets are sent instead of ZCRCG, to keep track
	* of the number of characters in the queue.  If the queue fills
	* up, we pause and wait for a ZACK.
	*
	*
	* Full Streaming with Reverse Interrupt:  If sender cannot
	* sample the input stream, then we define an Attn sequence
	* that will be used to interrupt transmission.
	*
	*
	* Full Streaming with Sliding Window:  If sender cannot
	* sample input stream or respond to Attn signal, we send
	* several ZCRCQ packets until we're sure the receiver must
	* have sent back at least one ZACK.  Then we stop sending and
	* read that ZACK.  Then we send one more packet and so on.
	*
	*
	* Segmented Streaming:  If receiver cannot overlap I/O or can't do
	* full duplex and has specified a maximum receive buffer size,
	* whenever the buffer size is reached, we send a ZCRCW packet.
	*
	* TODO: what if receiver can't overlap, but hasn't set a buffer
	* size?
	*
	* ZCRCE: CRC next, frame ends, header follows
	* ZCRCG: CRC next, frame continues nonstop
	* ZCRCQ: CRC next, send ZACK, frame continues nonstop
	* ZCRCW: CRC next, send ZACK, frame ends, header follows
	*/

	ZFlowControl(1,info);

	if( (info->zrinitflags & (CANFDX|CANOVIO)) == (CANFDX|CANOVIO) &&
		(SendSample||SendAttn)  &&
		info->bufsize == 0 )
	{
		if( info->windowsize == 0 )
		{
			info->Streaming = ZModem::Full;
		}
		else
		{
			info->Streaming = ZModem::StrWindow;
		}
	}

	else if( (info->zrinitflags & (CANFDX|CANOVIO)) == (CANFDX|CANOVIO) &&
		info->bufsize == 0 )
	{
		info->Streaming = ZModem::SlidingWindow;
	}

	else
	{
		info->Streaming = ZModem::Segmented;
	}

#if defined(_DEBUG)
	zmodemlog("GotRinit[%s]\n", sname(info));
#endif

	if( AlwaysSinit || info->zsinitflags != 0  ||  info->attn != NULL )
	{
		return SendZSInit(info);
	}
	else
	{
		return ZmDone;	/* caller now calls ZmodemTFile() */
	}
}


int SendZSInit( ZModem *info )
{
	int	err;
	char	*at = (info->attn != NULL) ? info->attn : const_cast<char *>("");
	u_char	fbuf[4];

	/* TODO: zmodem8.doc states: "If the ZSINIT header specifies
	* ESCCTL or ESC8, a HEX header is used, and the receiver
	* activates the specified ESC modes before reading the following
	* data subpacket." What does that mean?
	*/

#if defined(_DEBUG)
	zmodemlog("SendZSInit[%s]\n", sname(info));
#endif

	info->state = TInit;
	fbuf[0] = fbuf[1] = fbuf[2] = 0;
	fbuf[3] = info->zsinitflags;
	if( (err = ZXmitHdr(ZSINIT, ZBIN, fbuf, info))  ||
		(err = ZXmitData(ZBIN, strlen(at)+1, ZCRCW, (u_char *)at, info)) )
	{
		return err;
	}
	return 0;
}


int SendFileCrc( ZModem *info )
{
	u_long	crc = FileCrc(info->filename);
#if defined(_DEBUG)
	zmodemlog("SendFileCrc[%s]: %lx\n", sname(info), crc);
#endif
	return ZXmitHdrHex(ZCRC, ZEnc4(crc), info);
}



/* Utility: start sending data. */

int startFileData( ZModem *info )
{
	int	err;

	info->offset =
		info->lastOffset =
		info->zrposOffset = ZDec4(info->hdrData+1);

	fseek(info->file, info->offset, 0);

	/* TODO: what if fseek fails?  Send EOF? */

#if defined(_DEBUG)
	zmodemlog("startFileData[%s]: %ld\n", sname(info), info->offset);
#endif

	if( (err = ZXmitHdr(ZDATA, ZBIN, info->hdrData+1, info)) )
	{
#if defined(_DEBUG)
		zmodemlog( "startFileData[%s]: return err", sname( info ) );
#endif
		return err;
	}
	return SendMoreFileData(info);
}



/* send a chunk of file data in response to a ZRPOS.  Whether this
* is followed by a ZCRCE, ZCRCG, ZCRCQ or ZCRCW depends on all
* sorts of protocol flags
*/

int SendFileData( ZModem *info )
{
	info->waitflag = 0;
	return startFileData(info);
}

/* here if an ACK arrived while transmitting data.  Update
* last known receiver offset, and try to send some more
* data.
*/
int GotSendAck( ZModem *info )
{
	u_long offset = ZDec4(info->hdrData+1);

	if( offset > info->lastOffset )
	{
		info->lastOffset = offset;
	}

#if defined(_DEBUG)
	zmodemlog("GotSendAck[%s]: %ld\n", sname(info), info->offset);
#endif

	return 0;		/* DONT send more data, that will happen later anyway */
}



/* here if an ACK arrived after last file packet sent.  Send
* the EOF.
*/

int GotSendDoneAck( ZModem *info )
{
	u_long offset = ZDec4(info->hdrData+1);

	if( offset > info->lastOffset )
	{
		info->lastOffset = offset;
	}

#if defined(_DEBUG)
	zmodemlog("GotSendDoneAck[%s]: %ld\n", sname(info), info->offset);
#endif

	info->state = SendEof;
	info->timeout = 60;
	return ZXmitHdrHex(ZEOF, ZEnc4(info->offset), info);
}


/* off to a bad start; ZDATA header was corrupt.  Start
* from beginning
*/

int GotSendNak( ZModem *info )
{
	info->offset = info->zrposOffset;

	fseek(info->file, info->offset, 0);

	/* TODO: what if fseek fails?  Send EOF? */

#if defined(_DEBUG)
	zmodemlog("GotSendNak[%s]: %ld\n", sname(info), info->offset);
#endif

	return SendMoreFileData(info);
}


/* got a ZSKIP, receiver doesn't want this file.  */

int SkipFile( ZModem *info )
{
#if defined(_DEBUG)
	zmodemlog("SkipFile[%s]\n", sname(info));
#endif
	fclose(info->file);
	return ZmDone;
}


/* got a ZRPOS packet in the middle of sending a file,
* set new offset and try again
*/

int GotSendPos( ZModem *info )
{
	ZStatus(DataErr, ++info->errCount, NULL);
	info->waitflag = 1;		/* next pkt should wait, to resync */
#if defined(_DEBUG)
	zmodemlog("GotSendPos[%s]\n", sname(info), info->offset);
#endif
	return startFileData(info);
}



/* here if an ACK arrived while waiting while transmitting data.
* Update last known receiver offset, and try to send some more
* data.
*/

int GotSendWaitAck( ZModem *info )
{
	int	err;

	u_long	offset = ZDec4(info->hdrData+1);

	if( offset > info->lastOffset )
	{
		info->lastOffset = offset;
	}

#if defined(_DEBUG)
	zmodemlog("GotSendWaitAck[%s]\n", sname(info), offset);
#endif

	if( (err = ZXmitHdr(ZDATA, ZBIN, ZEnc4(info->offset), info)) )
	{
		return err;
	}

	return SendMoreFileData(info);
}




/* utility: send a chunk of file data.  Whether this is followed
* by a ZCRCE, ZCRCG, ZCRCQ or ZCRCW depends on all
* sorts of protocol flags, plus 'waitflag'.  Exact amount of file
* data transmitted is variable, as escape sequences may pad out
* the buffer.
*/

int SendMoreFileData( ZModem *info )
{
	int	type;
	int	qfull = 0;
	int	err;
	int	len;		/* max # chars to send this packet */
	long	pending;	/* # of characters sent but not acknowledged */

	/* ZCRCE: CRC next, frame ends, header follows
	* ZCRCG: CRC next, frame continues nonstop
	* ZCRCQ: CRC next, send ZACK, frame continues nonstop
	* ZCRCW: CRC next, send ZACK, frame ends, header follows
	*/

	if( info->interrupt )
	{
		/* Bugger, receiver sent an interrupt.  Enter a wait state
		* and see what they want.  Next header *should* be ZRPOS.
		*/
		info->state = SendWait;
		info->timeout = 60;
		return 0;
	}

	/* Find out how many bytes we can transfer in the next packet */

	len = info->packetsize;

	pending = info->offset - info->lastOffset;

	if( info->windowsize != 0 && info->windowsize - pending <= len )
	{
		len = info->windowsize - pending;
		qfull = 1;
	}
	if( info->bufsize != 0  &&  info->bufsize - pending <= len )
	{
		len = info->bufsize - pending;
		qfull = 1;
	}

	if( len == 0 )
	{
		/* window still full, keep waiting */
		info->state = SendWait;
		info->timeout = 60;
		return 0;
	}


	/* OK, we can safely transmit 'len' bytes of data.  Start reading
	* file until buffer is full.
	*/

	//len -= 10;	/* Pre-deduct 10 bytes for trailing CRC */


	/* find out what kind of packet to send */
	if( info->waitflag )
	{
		type = ZCRCW;
		info->waitflag = 0;
	}
#ifdef	COMMENT
	else if( info->fileEof )
	{
		type = ZCRCE;
	}
#endif	/* COMMENT */
	else if( qfull )
	{
		type = ZCRCW;
	}
	else
	{
		switch( info->Streaming )
		{
		case ZModem::Full:
		case ZModem::Segmented: type = ZCRCG; break;

		case ZModem::StrWindow:
			if( (info->windowCount += len) < info->windowsize/4 )
			{
				type = ZCRCG;
			}
			else
			{
				type = ZCRCQ;
				info->windowCount = 0;
			}
			break;

		default:
		case ZModem::SlidingWindow:
			type = ZCRCQ;
			break;
		}
	}


	int	crc32 = info->crc32;
	int c=0, c2, atSign=0;
	u_long crc;
	u_char *ptr = info->buffer;

	crc = crc32 ? 0xffffffff : 0;

	/* read characters from file and put into buffer until buffer is
	* full or file is exhausted
	*/

	while( len > 0  && (c = getc(info->file)) != EOF )
	{
		if( !crc32 )
		{
			crc = updcrc(c, crc);
		}
		else
		{
			crc = UPDC32(c, crc);
		}

		/* zmodem protocol requires that CAN(ZDLE), DLE, XON, XOFF and
		* a CR following '@' be escaped.  In addition, I escape '^]'
		* to protect telnet, "<CR>~." to protect rlogin, and ESC for good
		* measure.
		*/
		c2 = c & 0177;
		if( c == ZDLE || c2 == 020 || c2 == 021 || c2 == 023 ||
			c2 == 0177  ||  c2 == '\r'  ||  c2 == '\n'  ||  c2 == 033  ||
			c2 == 035  || (c2 < 040 && info->escCtrl) )
		{
			*ptr++ = ZDLE;
			if( c == 0177 )
			{
				*ptr = ZRUB0;
			}
			else if( c == 0377 )
			{
				*ptr = ZRUB1;
			}
			else
			{
				*ptr = c^0100;
			}
			len -= 2;
		}
		else
		{
			*ptr = c;
			--len;
		}
		++ptr;

		atSign = c2 == '@';
		++info->offset;
	}

	/* if we've reached file end, a ZEOF header will follow.  If
	* there's room in the outgoing buffer for it, end the packet
	* with ZCRCE and append the ZEOF header.  If there isn't room,
	* we'll have to do a ZCRCW
	*/
	if( (info->fileEof = (c == EOF)) )
	{
		if( qfull  ||  (info->bufsize != 0 && len < 24) )
		{
			type = ZCRCW;
		}
		else
		{
			type = ZCRCE;
		}
	}

	*ptr++ = ZDLE;
	if( !crc32 )
	{
		crc = updcrc(type, crc);
	}
	else
	{
		crc = UPDC32(type, crc);
	}
	*ptr++ = type;

	if( !crc32 )
	{
		crc = updcrc(0,crc); crc = updcrc(0,crc);
		ptr = putZdle( ptr, static_cast<u_char>( ( crc >> 8 ) & 0xff ), info );
		ptr = putZdle( ptr, static_cast<u_char>( crc & 0xff ), info);
	}
	else
	{
		crc = ~crc;
		for(len=4; --len >= 0; crc >>= 8)
		{
			ptr = putZdle( ptr, static_cast<u_char>( crc & 0xff ), info );
		}
	}

	len = ptr - info->buffer;


	ZStatus(SndByteCount, info->offset, NULL);

	if( (err = ZXmitStr(info->buffer, len, info)) )
	{
		return err;
	}

#ifdef	COMMENT
	if( (err = ZXmitData(ZBIN, info->buffer, len, type, info)) )
	{
		return err;
	}
#endif	/* COMMENT */

	/* finally, do we want to wait after this packet? */

	switch( type )
	{
	case ZCRCE:
		info->state = SendEof;
		info->timeout = 60;
		return ZXmitHdrHex(ZEOF, ZEnc4(info->offset), info);
	case ZCRCW:
		info->state = info->fileEof ? SendDone : SendWait;
		info->timeout = 60;
		break;
	default:
		info->state = Sending;
		info->timeout = 0;
		break;
	}


#ifdef	COMMENT
	if( info->fileEof )
	{
		/* Yes, file is done, send EOF and wait */
		info->state = SendEof;
		info->timeout = 60;
		return ZXmitHdrHex(ZEOF, ZEnc4(info->offset), info);
	}
	else if( type == ZCRCW )
	{
		info->state = SendWait;
		info->timeout = 60;
	}
	else
	{
		info->state = Sending;
		info->timeout = 0;
	}
#endif	/* COMMENT */
	return 0;
}


int ResendEof( ZModem *info )
{
	return ZXmitHdrHex(ZEOF, ZEnc4(info->offset), info);
}


int OverAndOut( ZModem *info )
{
	ZXmitStr((u_char *)"OO", 2, info);
	return ZmDone;
}



/* YMODEM */


static	u_char	eotstr[1] = {EOT};

/* ymodem parser */

int YsendChar( char c, ZModem *info )
{
	int	err;

	if( info->canCount >= 2 )
	{
		ZStatus(RmtCancel, 0, NULL);
		return ZmErrCancel;
	}

	switch( info->state )
	{
	case YTStart:		/* wait for 'G', 'C' or NAK */
		switch( c )
		{
		case 'G':		/* streaming YModem */
		case 'C':		/* CRC YModem */
		case NAK:		/* checksum YModem */
			info->PacketType = c;
			return ZmDone;
		default:
			return 0;
		}

	case YTFile:		/* sent filename, waiting for ACK or NAK */
		switch( c )
		{
		case NAK:		/* resend */
		case 'C':
		case 'G':
			ZStatus(DataErr, ++info->errCount, NULL);
			return YSendFilename(info);
		case ACK:
			info->state = YTDataWait;
		default:
			return 0;
		}

	case YTDataWait:	/* sent filename, waiting for G,C or NAK */
		switch( c )
		{
		case NAK:
		case 'C':
		case 'G':
			info->chrCount = 0;
			if( info->PacketType == 'G' )	/* send it all at once */
			{
				while( info->state == YTData )
					if( (err = YSendData(info)) )
						return err;
				return 0;
			}
			else
				return YSendData(info);
		default:
			return 0;
		}

	case YTData:		/* sent data, waiting for ACK or NAK */
		switch( c )
		{
		case 'C':
		case 'G':		/* protocol failure, resend filename */
			if( info->Protocol == ZModem::YMODEM )
			{
				ZStatus(DataErr, ++info->errCount, NULL);
				info->state = YTFile;
				rewind(info->file);
				return YSendFilename(info);
			}
			/* else XModem, treat it like a NAK */
		case NAK:
			ZStatus(DataErr, ++info->errCount, NULL);
			return YXmitData(info->buffer + info->bufp, info->ylen, info);
		case ACK:
			info->offset += info->ylen;
			info->bufp += info->ylen;
			info->chrCount -= info->ylen;
			ZStatus(SndByteCount, info->offset, NULL);
			return YSendData(info);
		default:
			return 0;
		}

	case YTEOF:		/* sent EOF, waiting for ACK or NAK */
		switch( c )
		{
		case NAK:
			return ZXmitStr(eotstr, 1, info);
		case ACK:
			info->state = info->Protocol == ZModem::YMODEM ? YTStart : Done;
			return ZmDone;
		default:
			return 0;
		}

	case YTFin:		/* sent Fin, waiting for ACK or NAK */
		switch( c )
		{
		case NAK:
			return YSendFin(info);
		case ACK:
			return ZmDone;
		default:
			return 0;
		}
	default:
		return 0;
	}
}

int YXmitData( u_char *buffer, int len, ZModem *info )
{
	u_char	hdr[3];
	u_char	trail[2];
	u_long	crc = 0;
	int	i, err;

	hdr[0] = len == 1024 ? STX : SOH;
	hdr[1] = info->packetCount;
	hdr[2] = ~hdr[1];
	if( (err = ZXmitStr(hdr, 3, info)) ||
		(err = ZXmitStr(buffer, len, info)) )
	{
		return err;
	}

	if( info->PacketType == NAK )
	{	/* checksum */
		for(i=0; i<len; ++i)
		{
			crc += buffer[i];
		}
		trail[0] = static_cast<u_char>( crc % 256 );
		return ZXmitStr(trail,1, info);
	}
	else
	{
		for(i=0; i<len; ++i)
		{
			crc = updcrc(buffer[i], crc);
		}
		crc = updcrc(0,crc); crc = updcrc(0,crc);
		trail[0] = static_cast<u_char>( crc / 256 );
		trail[1] = static_cast<u_char>( crc % 256 );
		return ZXmitStr(trail,2, info);
	}
}



int YSendFilename( ZModem *info )
{
	u_char	obuf[1024];
	u_char	*ptr = obuf;

	info->state = info->PacketType != 'G' ? YTFile : YTDataWait;
	info->packetCount = 0;
	info->offset = 0;

	int i = strlen(info->rfilename);
	memcpy(ptr, info->rfilename, i+1);  ptr += i+1;
	sprintf( reinterpret_cast<char*>( ptr ), "%d %lo %o 0", info->len, info->date, info->mode);
	ptr += strlen( reinterpret_cast<char*>( ptr ) );
	*ptr++ = '\0';
	/* pad out to 128 bytes or 1024 bytes */
	i = ptr-obuf;
	int len = i>128 ? 1024 : 128;
	for(; i<len; ++i)
	{
		*ptr++ = '\0';
	}

	return YXmitData(obuf, len, info);
}


/* send next buffer of data */

int YSendData( ZModem *info )
{
	/* are there characters still in the read buffer? */

	if( info->chrCount <= 0 )
	{
		info->bufp = 0;
		info->chrCount = fread(info->buffer, 1, info->packetsize, info->file);
		info->fileEof = feof(info->file);
	}

	if( info->chrCount <= 0 )
	{
		fclose(info->file);
		info->state = YTEOF;
		return ZXmitStr(eotstr, 1, info);
	}

	/* pad out to 128 bytes if needed */
	if( info->chrCount < 128 )
	{
		int i = 128 - info->chrCount;
		memset(info->buffer + info->bufp + info->chrCount, 0x1a, i);
		info->chrCount = 128;
	}

	info->ylen = info->chrCount >= 1024 ? 1024 : 128;
	++info->packetCount;

	info->state = YTData;

	return YXmitData(info->buffer + info->bufp, info->ylen, info);
}



int YSendFin( ZModem *info )
{
	u_char	obuf[128];

	info->state = YTFin;
	info->packetCount = 0;

	memset(obuf,0,128);

	return YXmitData(obuf, 128, info);
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
