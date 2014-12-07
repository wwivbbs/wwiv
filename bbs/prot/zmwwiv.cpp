/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1999-2014, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include "bbs/wcomm.h"
#include "bbs/wwiv.h"
#include "bbs/prot/zmodem.h"
#include "core/os.h"
#include "core/strings.h"

using std::chrono::milliseconds;
using namespace wwiv::os;
using namespace wwiv::strings;

// Local Functions
int ZModemWindowStatus( const char *fmt, ... );
int ZModemWindowXferStatus( const char *fmt, ... );
int doIO( ZModem *info );
void ProcessLocalKeyDuringZmodem();

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4706 4127 4244 4100 )
#endif

bool NewZModemSendFile( const char *pszFileName ) {
	ZModem info;
	info.ifd = info.ofd = -1;
	info.zrinitflags = 0;
	info.zsinitflags = 0;
	info.attn = nullptr;
	info.packetsize = 0;
	info.windowsize = 0;
	info.bufsize = 0;

	sleep_for(milliseconds(500));	// Kludge -- Byte thinks this may help on his system

	ZmodemTInit( &info );
	int done = doIO( &info );
	if ( done != ZmDone ) {
		//puts( "Returning False from doIO After ZModemTInit\r\n" );
		return false;
	}

	int f0			= ZCBIN;
	int f1			= 0; // xferType | noloc
	int f2			= 0;
	int f3			= 0;
	int nFilesRem	= 0;
	int nBytesRem	= 0;
	done = ZmodemTFile( pszFileName, pszFileName, f0, f1, f2, f3, nFilesRem, nBytesRem, &info );
	switch ( done ) {
	case 0:
		ZModemWindowXferStatus( "Sending File: %s", pszFileName );
		break;
	case ZmErrCantOpen:
		ZModemWindowXferStatus( "ERROR Opening File: %s", pszFileName );
		break;
	case ZmFileTooLong:
		ZModemWindowXferStatus( "ERROR FileName \"%s\" is too long", pszFileName );
		break;
	case ZmDone:
		return true;
	default:
		return false;
	}

	if ( !done ) {
		done = doIO( &info );
#if defined(_DEBUG)
		zmodemlog( "Returning %d from doIO After ZmodemTFile\r\n", done );
#endif
	}
	if ( done != ZmDone ) {
		return false;
	}

	sleep_for(milliseconds(500));	// Kludge -- Byte thinks this may help on his system

	done = ZmodemTFinish( &info );
	if( !done ) {
		done = doIO( &info ) ;
	}

	return ( done == ZmDone ) ? true : false;
}


bool NewZModemReceiveFile( const char *pszFileName ) {
	ZModem info;
	info.ifd = info.ofd = -1;
	info.zrinitflags = 0;
	info.zsinitflags = 0;
	info.attn = nullptr;
	info.packetsize = 0;
	info.windowsize = 0;
	info.bufsize = 0;

	int nDone = ZmodemRInit( &info );
	nDone = doIO( &info );
	bool ret = ( nDone == ZmDone ) ? true : false;
	if ( ret ) {
		char szNewFileName[ MAX_PATH ];
		char szOldFileName[ MAX_PATH ];
		strcpy( szNewFileName, pszFileName );
		StringRemoveWhitespace( szNewFileName );

		sprintf( szOldFileName, "%s%s", syscfgovr.tempdir, stripfn( pszFileName ) );
		StringRemoveWhitespace( szOldFileName );
        std::cout << "szOldFileName=" << szOldFileName
            << "szNewFileName=" << szNewFileName;
		movefile( szOldFileName, szNewFileName, false );
	}
	return ret;
}



/**
 * printf sytle output function.  Most code should use this when writing
 * locally + remotely.
 */
int ZModemWindowStatus(const char *fmt,...) {
	va_list ap;
	char szBuffer[2048];

	va_start( ap, fmt );
	vsnprintf( szBuffer, sizeof( szBuffer ), fmt, ap );
	va_end( ap );
	int oldX = session()->localIO()->WhereX();
	int oldY = session()->localIO()->WhereY();
	session()->localIO()->LocalXYPrintf( 1, 10, "%s                           ", szBuffer );
	session()->localIO()->LocalGotoXY( oldX, oldY );
	return 0;
}


/**
 * printf sytle output function.  Most code should use this when writing
 * locally + remotely.
 */
int ZModemWindowXferStatus(const char *fmt,...) {
	va_list ap;
	char szBuffer[2048];

	va_start( ap, fmt );
	vsnprintf( szBuffer, sizeof( szBuffer ), fmt, ap );
	va_end( ap );
	int oldX = session()->localIO()->WhereX();
	int oldY = session()->localIO()->WhereY();
	session()->localIO()->LocalXYPrintf( 1, 1, "%s                           ", szBuffer );
	session()->localIO()->LocalGotoXY( oldX, oldY );
	return 0;
}

#define ZMODEM_RECEIVE_BUFFER_SIZE 8132
//#define ZMODEM_RECEIVE_BUFFER_SIZE 1024
// 21 here is just an arbitrary number
#define ZMODEM_RECEIVE_BUFFER_PADDING 21

int doIO( ZModem *info ) {
	u_char	buffer[ ZMODEM_RECEIVE_BUFFER_SIZE + ZMODEM_RECEIVE_BUFFER_PADDING ];
	int	done = 0;
	bool doCancel = false;	// %%TODO: make this true if the user aborts.

	while(!done) {
		yield();
		if ( info->timeout > 0 ) {
			yield();
		}
		time_t tThen = time( nullptr );
#if defined(_DEBUG)
		if ( info->timeout > 0 ) {
			zmodemlog( "[%ld] Then.  Timeout = %ld\r\n", tThen, info->timeout );
		}
#endif
		// Don't loop/sleep if the timeout is 0 (which means streaming), this makes the
		// performance < 1k/second vs. 8-9k/second locally
		while ( ( info->timeout > 0 ) && !session()->remoteIO()->incoming() && !hangup ) {
			sleep_for(milliseconds(100));
			time_t tNow = time( nullptr );
			if ( ( tNow - tThen ) > info->timeout ) {
#if defined(_DEBUG)
				zmodemlog( "Break: [%ld] Now.  Timedout = %ld.  Time = %d\r\n", tNow, info->timeout, ( tNow - tThen ) );
#endif
				break;
			}
			ProcessLocalKeyDuringZmodem();
		}

		ProcessLocalKeyDuringZmodem();
		if ( hangup || hungup ) {
			return ZmErrCancel;
		}

		if( doCancel ) {
			ZmodemAbort(info);
			fprintf(stderr, "cancelled by user\n");
			//resetCom();
			//%%TODO: signal parent we aborted.
			return 1;
		}
		bool bIncomming = session()->remoteIO()->incoming();
		if( !bIncomming ) {
			done = ZmodemTimeout(info);
			//puts( "ZmodemTimeout\r\n" );
		} else {
			int len = session()->remoteIO()->read( reinterpret_cast<char*>( buffer ), ZMODEM_RECEIVE_BUFFER_SIZE );
			done = ZmodemRcv( buffer, len, info );
#if defined(_DEBUG)
			zmodemlog( "ZmodemRcv [%d chars] [done:%d]\r\n", len, done );
#endif
		}
	}
	return done;
}



int ZXmitStr(u_char *str, int len, ZModem *info) {
#if defined(_DEBUG)
	zmodemlog( "ZXmitStr Size=[%d]\r\n", len );
#endif
	session()->remoteIO()->write( reinterpret_cast<const char*>( str ),  len );
	return 0;
}


void ZIFlush(ZModem *info) {
	sleep_for(milliseconds(100));
	//puts( "ZIFlush" );
	//if( connectionType == ConnectionSerial )
	//  SerialFlush( 0 );
}

void ZOFlush(ZModem *info) {
	//if( connectionType == ConnectionSerial )
	//	SerialFlush( 1 );
}

int ZAttn(ZModem *info) {
	if ( info->attn == nullptr ) {
		return 0;
	}

	int cnt = 0;
	for( char *ptr = info->attn; *ptr != '\0'; ++ptr ) {
		if ( ( ( cnt++ ) % 10 ) == 0 ) {
			sleep_for(milliseconds(100));
		}
		if ( *ptr == ATTNBRK ) {
			//SerialBreak();
		} else if ( *ptr == ATTNPSE ) {
#if defined(_DEBUG)
			zmodemlog( "ATTNPSE\r\n" );
#endif
			sleep_for(milliseconds(100));
		} else {
			rputch( *ptr, true );
			//append_buffer(&outputBuf, ptr, 1, ofd);
		}
	}
	FlushOutComChBuffer();
	return 0;
}


/* set flow control as required by protocol.  If original flow
* control was hardware, do not change it.  Otherwise, toggle
* software flow control
*/
void ZFlowControl(int onoff, ZModem *info) {
	// I don't think there is anything to do here.
}


void ZStatus(int type, int value, char *msg) {
	switch( type ) {
	case RcvByteCount:
		ZModemWindowXferStatus("ZModemWindowXferStatus: %d bytes received", value);
		// WindowXferGauge(value);
		break;

	case SndByteCount:
		ZModemWindowXferStatus("ZModemWindowXferStatus: %d bytes sent", value);
		// WindowXferGauge(value);
		break;

	case RcvTimeout:
		ZModemWindowStatus("ZModemWindowStatus: Receiver did not respond, aborting");
		break;

	case SndTimeout:
		ZModemWindowStatus("ZModemWindowStatus: %d send timeouts", value);
		break;

	case RmtCancel:
		ZModemWindowStatus("ZModemWindowStatus: Remote end has cancelled");
		break;

	case ProtocolErr:
		ZModemWindowStatus("ZModemWindowStatus: Protocol error, header=%d", value);
		break;

	case RemoteMessage:	/* message from remote end */
		ZModemWindowStatus("ZModemWindowStatus: MESSAGE: %s", msg);
		break;

	case DataErr:		/* data error, val=error count */
		ZModemWindowStatus("ZModemWindowStatus: %d data errors", value);
		break;

	case FileErr:		/* error writing file, val=errno */
		ZModemWindowStatus("ZModemWindowStatus: Cannot write file: %s", strerror(errno));
		break;

	case FileBegin:	/* file transfer begins, str=name */
		ZModemWindowStatus("ZModemWindowStatus: Transfering %s", msg);
		break;

	case FileEnd:		/* file transfer ends, str=name */
		ZModemWindowStatus("ZModemWindowStatus: %s finished", msg);
		break;

	case FileSkip:	/* file transfer ends, str=name */
		ZModemWindowStatus("ZModemWindowStatus: Skipping %s", msg);
		break;
	}
}


FILE * ZOpenFile(char *pszFileName, u_long crc, ZModem *info) {
	char szTempFileName[ MAX_PATH ];
	sprintf( szTempFileName, "%s%s", syscfgovr.tempdir, pszFileName );
#if defined(_DEBUG)
	zmodemlog( "ZOpenFile filename=%s %s\r\n", pszFileName, szTempFileName );
#endif
	return fopen( szTempFileName, "wb" );

	//	struct stat	buf;
	//	bool		exists;	/* file already exists */
	//	static	int		changeCount = 0;
	//	char		szFileName2[MAXPATHLEN];
	//	int		apnd = 0;
	//	int		f0,f1;
	//	FILE		*ofile;
	//	char		path[1024];
	//
	//	/* TODO: if absolute path, do we want to allow it?
	//	 * if relative path, do we want to prepend something?
	//	 */
	//
	//	if( *pszFileName == '/' )	/* for now, disallow absolute paths */
	//	  return nullptr;
	//
	//	buf.st_size = 0;
	//	if( stat(pszFileName, &buf) == 0 )
	//	  exists = True;
	//	else if( errno == ENOENT )
	//	  exists = False;
	//	else
	//	  return nullptr;
	//
	//
	//	/* if remote end has not specified transfer flags, we can
	//	 * accept the local definitions
	//	 */
	//
	//	if( (f0=info->f0) == 0 ) {
	//	  if( xferResume )
	//	    f0 = ZCRESUM;
	//	  else if( xferAscii )
	//	    f0 = ZCNL;
	//#ifdef	COMMENT
	//	  else if( binary )
	//	    f0 = ZCBIN;
	//#endif	/* COMMENT */
	//	  else
	//	    f0 = 0;
	//	}
	//
	//	if( (f1=info->f1) == 0 ) {
	//	  f1 = xferType;
	//	  if( noLoc )
	//	    f1 |= ZMSKNOLOC;
	//	}
	//
	//	zmodemlog("ZOpenFile: %s, f0=%x, f1=%x, exists=%d, size=%d/%d\n",
	//	  pszFileName, f0,f1, exists, buf.st_size, info->len);
	//
	//	if( f0 == ZCRESUM ) {	/* if exists, and we already have it, return */
	//	  if( exists  &&  buf.st_size == info->len )
	//	    return nullptr;
	//	  apnd = 1;
	//	}
	//
	//	/* reject if file not found and it must be there (ZMSKNOLOC) */
	//	if( !exists && (f1 & ZMSKNOLOC) )
	//	  return nullptr;
	//
	//	switch( f1 & ZMMASK ) {
	//	  case 0:	/* Implementation-dependent.  In this case, we
	//			 * reject if file exists (and ZMSKNOLOC not set) */
	//	    if( exists && !(f1 & ZMSKNOLOC) )
	//	      return nullptr;
	//	    break;
	//
	//	  case ZMNEWL:	/* take if newer or longer than file on disk */
	//	    if( exists  &&  info->date <= buf.st_mtime  &&
	//		info->len <= buf.st_size )
	//	      return nullptr;
	//	    break;
	//
	//	  case ZMCRC:		/* take if different CRC or length */
	//	    zmodemlog("  ZMCRC: crc=%x, FileCrc=%x\n", crc, FileCrc(pszFileName) );
	//	    if( exists  &&  info->len == buf.st_size && crc == FileCrc(pszFileName) )
	//	      return nullptr;
	//	    break;
	//
	//	  case ZMAPND:	/* append */
	//	    apnd = 1;
	//	  case ZMCLOB:	/* unconditional replace */
	//	    break;
	//
	//	  case ZMNEW:	/* take if newer than file on disk */
	//	    if( exists  &&  info->date <= buf.st_mtime )
	//	      return nullptr;
	//	    break;
	//
	//	  case ZMDIFF:	/* take if different date or length */
	//	    if( exists  &&  info->date == buf.st_mtime  &&
	//		info->len == buf.st_size )
	//	      return nullptr;
	//	    break;
	//
	//	  case ZMPROT:	/* only if dest does not exist */
	//	    if( exists )
	//	      return nullptr;
	//	    break;
	//
	//	  case ZMCHNG:	/* invent new filename if exists */
	//	    if( exists ) {
	//	      while( exists ) {
	//		sprintf(szFileName2, "%s_%d", pszFileName, changeCount++);
	//		exists = stat(szFileName2, &buf) == 0 || errno != ENOENT;
	//	      }
	//	      pszFileName = szFileName2;
	//	    }
	//	    break;
	//	}
	//
	//	/* here if we've decided to accept */
	//	if( exists && !apnd && unlink(pszFileName) != 0 )
	//	  return nullptr;
	//
	//	/* TODO: build directory path if needed */
	//
	//	ZModemWindowStatus("Receiving: \"%s\"", pszFileName);
	//
	//	WindowXferGaugeMax(info->len);
	//
	//	ofile = fopen(pszFileName, apnd ? "a" : "w");
	//
	//	zmodemlog("  ready to open %s/%s: apnd = %d, file = %lx\n",
	//	  getcwd(path,sizeof(path)), pszFileName, apnd, (long)ofile);
	//
	//	return ofile;
	//return nullptr;
}


int ZWriteFile( u_char *buffer, int len, FILE *file, ZModem *info ) {
	/* If ZCNL set in info->f0, convert
	* newlines to unix convention
	*/
	//if( info->f0 == ZCNL )
	//{
	//  int	i, j, c;
	//  static int lastc = '\0';
	//  for(i=0; i < len; ++i)
	//  {
	//    switch( (c=*buffer++) ) {
	//      case '\n': if( lastc != '\r' ) j = putc('\n', file); break;
	//      case '\r': if( lastc != '\n' ) j = putc('\n', file); break;
	//      default: j = putc(c, file); break;
	//    }
	//    lastc = c;
	//    if( j == EOF )
	//      return ZmErrSys;
	//  }
	//  return 0;
	//}
	//else
	if( info->f0 == ZCNL ) {
#ifdef _WIN32
		OutputDebugString( "ZCNL\r\n" );
#endif // _WIN32
	}
	return ( fwrite( buffer, 1, len, file ) == static_cast<unsigned int>( len ) ) ? 0 : ZmErrSys;
}


int ZCloseFile(ZModem *info) {
	fclose( info->file );
	//	struct timeval tvp[2];
	//	fclose(info->file);
	//#ifdef	TODO
	//	if( ymodem )
	//	  truncate(info->filename, info->len);
	//#endif	/* TODO */
	//	if( info->date != 0 ) {
	//	  tvp[0].tv_sec = tvp[1].tv_sec = info->date;
	//	  tvp[0].tv_usec = tvp[1].tv_usec = 0;
	//	  utimes(info->filename, tvp);
	//	}
	//	if( info->mode & 01000000 )
	//	  chmod(info->filename, info->mode&0777);
	//	return 0;
	return 0;
}


void ZIdleStr(unsigned char *buf, int len, ZModem *info) {
	//PutTerm(buf, len);
	return;
#if 0
	char szBuffer[ 1024 ];
	strcpy( szBuffer, reinterpret_cast<const char *>( buf  ) );
	szBuffer[len] = '\0';
	if ( strlen( szBuffer ) == 1 ) {
		zmodemlog( "ZIdleStr: #[%d]\r\n", static_cast<unsigned int>( (unsigned char ) szBuffer[0] ) );
	} else {
		zmodemlog( "ZIdleStr: [%s]\r\n", szBuffer );
	}
#endif
}


void ProcessLocalKeyDuringZmodem() {
	if ( session()->localIO()->LocalKeyPressed() ) {
		char localChar = session()->localIO()->LocalGetChar();
		session()->SetLastKeyLocal( true );
		if (!(g_flags & g_flag_allow_extended)) {
			if (!localChar) {
				localChar = session()->localIO()->LocalGetChar();
				session()->localIO()->skey(localChar);
			}
		}
	}
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
