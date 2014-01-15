#define DEBUG_ZMODEMLOG
/**********
*	ZMUTIL - utilties used by zmodem protocol.
*
*	Routines provided here:
*
*	int ZXmitHdrHex(type, data, info)
*		int	type;
*		u_char	data[4];
*		ZModem	*info;
*
*	transmit zmodem header in hex.
*
*
*	int ZXmitHdrBin(type, data, info)
*		int	type;
*		u_char	data[4];
*		ZModem	*info;
*
*		transmit zmodem header in binary.
*
*
*	int ZXmitHdrBin32(type, data, info)
*		int	type;
*		u_char	data[4];
*		ZModem	*info;
*
*		transmit zmodem header in binary with 32-bit crc.
*
*
*	int ZXmitHdr(type, format, data, info)
*		int	type, format;
*		u_char	data[4];
*		ZModem	*info;
*
*		transmit zmodem header
*
*
*	int ZXmitData(format, data, len, term, info)
*		int	format, len;
*		u_char	term;
*		u_char	*data;
*		ZModem	*info;
*
*		transmit buffer of data.
*
*
*	u_long FileCrc(name)
*		char	*name;
*
*		compute 32-bit crc for a file, returns 0 on not found
*
*
*	u_char	*ZEnc4(n)
*		u_long	n;
*
*		convert u_long to 4 bytes.
*
*	u_long ZDec4(buf)
*		u_char	buf[4];
*
*		convert 4 bytes to u_long
*
*
*
*	Copyright (c) 1995 by Edward A. Falk
*	January, 1995
**********/


#if defined( _MSC_VER ) && !defined( _CRT_SECURE_NO_DEPRECATE )
#define _CRT_SECURE_NO_DEPRECATE
#endif	// _MSC_VER 

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctype.h>
#include <ctime>

#include "zmodem.h"
#include "crctab.h"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4706 4127 4244 4100 )
#endif

u_long ZDec4( u_char buf[4] );


static	char	hexChars[] = "0123456789abcdef";

extern	char	*hdrnames[];

FILE	*zmodemlogfile = NULL;


/** put a number as two hex digits */
u_char * putHex( u_char *ptr, u_char c ) {
	*ptr++ = hexChars[(c>>4)&0xf];
	*ptr++ = hexChars[c&0xf];
	return ptr;
}


/* put a number with ZDLE escape if needed */

u_char * putZdle( u_char *ptr, u_char c, ZModem *info ) {
	u_char	c2 = c & 0177;

	if( c == ZDLE || c2 == 020 || c2 == 021 || c2 == 023 ||
	        c2 == 0177  ||  (c2 == 015 && info->atSign)  ||
#ifdef	COMMENT
	        c2 == 035  ||  (c2 == '~' && info->lastCR)  ||
#endif	/* COMMENT */
	        c2 == 035  ||
	        (c2 < 040 && info->escCtrl) ) {
		*ptr++ = ZDLE;
		if( c == 0177 ) {
			*ptr = ZRUB0;
		} else if( c == 0377 ) {
			*ptr = ZRUB1;
		} else {
			*ptr = c^0100;
		}
	} else {
		*ptr = c;
	}

	info->atSign = c2 == '@';
	info->lastCR = c2 == '\r';

	return ++ptr;
}


int ZXmitHdrHex( int type, u_char data[4], ZModem *info ) {
	u_char	szBuffer[128];
	u_char	*ptr = szBuffer;

#if defined(_DEBUG)
	zmodemlog(	"ZXmitHdrHex: sending  %s: %2.2x %2.2x %2.2x %2.2x = %lx\n",
	            hdrnames[type], data[0], data[1], data[2], data[3],
	            ZDec4(data));
#endif

	*ptr++ = ZPAD;
	*ptr++ = ZPAD;
	*ptr++ = ZDLE;
	*ptr++ = ZHEX;

	ptr = putHex(ptr, type);
	u_int crc = updcrc(type, 0);
	for( int i = 4; --i >= 0; ++data ) {
		ptr = putHex(ptr, *data);
		crc = updcrc(*data, crc);
	}
	crc = updcrc(0,crc);
	crc = updcrc(0,crc);
	ptr = putHex(ptr, (crc>>8)&0xff);
	ptr = putHex(ptr, crc&0xff);
	*ptr++ = '\r';
	*ptr++ = '\n';
	if( type != ZACK  &&  type != ZFIN ) {
		*ptr++ = XON;
	}

	return ZXmitStr(szBuffer, ptr-szBuffer, info);
}


int ZXmitHdrBin( int type, u_char data[4], ZModem *info ) {
	u_char	szBuffer[128];
	u_char	*ptr = szBuffer;
	int	len;

#if defined(_DEBUG)
	zmodemlog("ZXmitHdrBin: sending  %s: %2.2x %2.2x %2.2x %2.2x = %lx\n",
	          hdrnames[type], data[0], data[1], data[2], data[3],
	          ZDec4(data));
#endif

	*ptr++ = ZPAD;
	*ptr++ = ZDLE;
	*ptr++ = ZBIN;

	ptr = putZdle(ptr, type, info);
	u_int crc = updcrc(type, 0);
	for( len=4; --len >= 0; ++data ) {
		ptr = putZdle(ptr, *data, info);
		crc = updcrc(*data, crc);
	}
	crc = updcrc(0,crc);
	crc = updcrc(0,crc);
	ptr = putZdle(ptr, (crc>>8)&0xff, info);
	ptr = putZdle(ptr, crc&0xff, info);

	len = ptr-szBuffer;
	return ZXmitStr(szBuffer, len, info);
}


int ZXmitHdrBin32( int type, u_char data[4], ZModem *info ) {
	u_char	szBuffer[128];
	u_char	*ptr = szBuffer;
	u_long	crc;
	int	len;

#if defined(_DEBUG)
	zmodemlog("ZXmitHdrBin32: sending  %s: %2.2x %2.2x %2.2x %2.2x = %lx\n",
	          hdrnames[type], data[0], data[1], data[2], data[3],
	          ZDec4(data));
#endif

	*ptr++ = ZPAD;
	*ptr++ = ZDLE;
	*ptr++ = ZBIN32;
	ptr = putZdle(ptr, type, info);
	crc = UPDC32(type, 0xffffffffL);
	for( len=4; --len >= 0; ++data ) {
		ptr = putZdle(ptr, *data, info);
		crc = UPDC32(*data, crc);
	}
	crc = ~crc;
	for( len = 4; --len >= 0; crc >>= 8 ) {
		ptr = putZdle( ptr, static_cast<u_char>( crc & 0xff ), info );
	}

	len = ptr-szBuffer;
	return ZXmitStr( szBuffer, len, info );
}


int ZXmitHdr( int type, int format, u_char data[4], ZModem *info) {
	if( format == ZBIN && info->crc32 ) {
		format = ZBIN32;
	}

	switch( format ) {
	case ZHEX:
		return ZXmitHdrHex(type, data, info);
	case ZBIN:
		return ZXmitHdrBin(type, data, info);
	case ZBIN32:
		return ZXmitHdrBin32(type, data, info);
	default:
		return 0;
	}
}




/* TODO: if input is not a file, need to keep old data
* for possible retransmission */

int ZXmitData( int format, int len, u_char term, u_char *data, ZModem *info) {
	u_char	*ptr = info->buffer;

	if( format == ZBIN && info->crc32 ) {
		format = ZBIN32;
	}

#if defined(_DEBUG)
	zmodemlog("ZXmiteData: fmt=%c, len=%d, term=%c\n", format, len, term);
#endif

	u_int crc = (format == ZBIN) ? 0 : 0xffffffff;

	while( --len >= 0 ) {
		if( format == ZBIN ) {
			crc = updcrc(*data, crc);
		} else {
			crc = UPDC32(*data, crc);
		}
		ptr = putZdle(ptr, *data++, info);
	}

	*ptr++ = ZDLE;
	if( format == ZBIN ) {
		crc = updcrc(term, crc);
	} else {
		crc = UPDC32(term, crc);
	}
	*ptr++ = term;
	if( format == ZBIN ) {
		crc = updcrc(0,crc);
		crc = updcrc(0,crc);
		ptr = putZdle(ptr, (crc>>8)&0xff, info);
		ptr = putZdle(ptr, crc&0xff, info);
	} else {
		crc = ~crc;
		for(len=4; --len >= 0; crc >>= 8) {
			ptr = putZdle(ptr, crc&0xff, info);
		}
	}

	return ZXmitStr(info->buffer, ptr-info->buffer, info);
}


/* compute 32-bit crc for a file, returns 0 on not found */
u_long FileCrc( char *name ) {
	FILE	*ifile = fopen(name, "r");
	int	i;

	if( ifile == NULL ) {	/* shouldn't happen, since we did access( 2 ) */
		return 0;
	}

	u_long crc = 0xffffffff;

	while( (i=fgetc(ifile)) != EOF ) {
		crc = UPDC32(i, crc);
	}

	fclose(ifile);
	return ~crc;
}


u_char	* ZEnc4( u_long n ) {
	static	u_char	buf[4];
	buf[0] = static_cast<u_char>( n&0xff );

	n >>= 8;
	buf[1] = static_cast<u_char>( n&0xff );

	n >>= 8;
	buf[2] = static_cast<u_char>( n&0xff );

	n >>= 8;
	buf[3] = static_cast<u_char>( n&0xff );

	return buf;
}


u_long ZDec4( u_char buf[4] ) {
	return buf[0] | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24);
}


char * sname2(ZMState state) {
	static char *names[] = {
		"RStart", "RSinitWait", "RFileName", "RCrc", "RFile", "RData",
		"RDataErr", "RFinish", "TStart", "TInit", "FileWait", "CrcWait",
		"Sending", "SendWait", "SendDone", "SendEof", "TFinish",
		"CommandData", "CommandWait", "StderrData", "Done", "YTStart",
		"YTFile", "YTDataWait", "YTData", "YTEOF", "YTFin", "YRStart",
		"YRDataWait", "YRData", "YREOF"
	};
	return names[state];
}


char * sname(ZModem *info) {
	return sname2(info->state);
}


#ifdef	_DEBUG
void zmodemlog(const char *fmt, ... ) {
#if defined( DEBUG_ZMODEMLOG )
	va_list ap;
	struct tm *tm;
	static	int	do_ts = 1;

	zmodemlogfile = fopen( "zmodem_log.txt", "at" );
	if( zmodemlogfile == NULL ) {
		zmodemlogfile = stderr;
	}

	if( do_ts ) {
		time_t t = time( NULL );
		tm = localtime( &t );
		fprintf( zmodemlogfile, "%2.2d:%2.2d:%2.2d: ", tm->tm_hour, tm->tm_min, tm->tm_sec );
	}
	do_ts = strchr( fmt, '\n' ) != NULL;

	va_start( ap, fmt );
	vfprintf( zmodemlogfile, fmt, ap );
	va_end( ap );

	if ( zmodemlogfile != stderr ) {
		fclose( zmodemlogfile );
	}
#endif // DEBUG_ZMODEMLOG
}
#endif	/* DEBUG */

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
