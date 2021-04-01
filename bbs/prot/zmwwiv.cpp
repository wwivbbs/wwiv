/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1999-2021, WWIV Software Services             */
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
#include "bbs/bbs.h"
#include "bbs/prot/zmodem.h"
#include "bbs/prot/zmutil.h"
#include "common/input.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "local_io/local_io.h"
#include "sdk/files/file_record.h"

#include <chrono>
#include <cstring>
#include <filesystem>

using std::chrono::milliseconds;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::strings;

// Local Functions
int doIO(ZModem* info);

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4706 4127 4244 4100)
#endif

static void ZModemWindowStatusImpl(fmt::string_view format, fmt::format_args args) {
  const auto s = fmt::vformat(format, args);
  const auto oldX = bout.localIO()->WhereX();
  const auto oldY = bout.localIO()->WhereY();
  bout.localIO()->PutsXYA(0, 2, 12, s);
  bout.localIO()->ClrEol();
  bout.localIO()->PutsXYA(0, 3, 9, std::string(79, '='));
  bout.localIO()->GotoXY(oldX, oldY);

  zmodemlog("ZModemWindowStatus: [{}]\r\n", s);
}

template <typename S, typename... Args>
static void ZModemWindowStatus(const S& format, Args&&... args) {
  ZModemWindowStatusImpl(format, fmt::make_args_checked<Args...>(format, args...));
}

/**
 * fmtlib style output function.  Most code should use this when writing locally + remotely.
 */
static void ZModemWindowXferStatusImpl(fmt::string_view format, fmt::format_args args) {
  const auto s = fmt::vformat(format, args);
  const auto oldX = bout.localIO()->WhereX();
  const auto oldY = bout.localIO()->WhereY();
  bout.localIO()->PutsXYA(0, 0, 3, "ZModem Transfer Status: ");
  bout.localIO()->PutsXYA(0, 1, 14, s);
  bout.localIO()->ClrEol();
  bout.localIO()->PutsXYA(0, 3, 9, std::string(79, '='));
  bout.localIO()->GotoXY(oldX, oldY);
}

template <typename S, typename... Args>
static void ZModemWindowXferStatus(const S& format, Args&&... args) {
  ZModemWindowXferStatusImpl(format, fmt::make_args_checked<Args...>(format, args...));
}

static void ProcessLocalKeyDuringZmodem() {
  if (!bout.localIO()->KeyPressed()) {
    return;
  }
  const auto c = bout.localIO()->GetChar();
  bin.SetLastKeyLocal(true);
  if (!c) {
    a()->handle_sysop_key(bout.localIO()->GetChar());
  }
}

bool NewZModemSendFile(const std::filesystem::path& path) {
  ZModem info{};
  info.ifd = info.ofd = -1;
  info.zrinitflags = 0;
  info.zsinitflags = 0;
  info.attn = nullptr;
  info.packetsize = 0;
  info.windowsize = 0;
  info.bufsize = 0;

  sleep_for(milliseconds(500)); // Kludge -- Byte thinks this may help on his system

  ZmodemTInit(&info);
  auto done = doIO(&info);
  if (done != ZmDone) {
    zmodemlog("Returning False from doIO After ZModemTInit\r\n");
    return false;
  }

  uint8_t f0 = ZCBIN;
  uint8_t f1 = 0; // xferType | noloc
  uint8_t f2 = 0;
  uint8_t f3 = 0;
  int nFilesRem = 0;
  int nBytesRem = 0;
  char file_name[1024]; // was MAX_PATH
  to_char_array(file_name, path.string());
  zmodemlog("NewZModemSendFile: About to call ZmodemTFile\n");
  done = ZmodemTFile(file_name, file_name, f0, f1, f2, f3, nFilesRem, nBytesRem, &info);
  zmodemlog("NewZModemSendFile: After ZmodemTFile; [done: {}]\n", done);
  switch (done) {
  case 0:
    ZModemWindowXferStatus("Sending File: {}", file_name);
    break;
  case ZmErrCantOpen:
    ZModemWindowXferStatus("ERROR Opening File: {}", file_name);
    break;
  case ZmFileTooLong:
    ZModemWindowXferStatus("ERROR FileName '{}' is too long", file_name);
    break;
  case ZmDone:
    return true;
  default:
    return false;
  }

  if (!done) {
    done = doIO(&info);
#if defined(_DEBUG)
    zmodemlog("Returning {} from doIO After ZmodemTFile\n", done);
#endif
  }
  if (done != ZmDone) {
    return false;
  }

  sleep_for(milliseconds(500)); // Kludge -- Byte thinks this may help on his system

  done = ZmodemTFinish(&info);
  if (!done) {
    done = doIO(&info);
  }

  return done == ZmDone;
}

bool NewZModemReceiveFile(const std::filesystem::path& path){
  ZModem info{};
  info.ifd = info.ofd = -1;
  info.zsinitflags = 0;
  info.attn = nullptr;
  info.packetsize = 0;
  info.windowsize = 0;
  info.bufsize = 0;
  // This is what receive uses.
  info.zrinitflags = CANFDX|CANOVIO|CANBRK|CANFC32;

  ZmodemRInit(&info);
  const auto ret = doIO(&info) == ZmDone;
  if (ret) {
    const auto fn = wwiv::sdk::files::unalign(path.filename().string());
    const auto old_fn = FilePath(a()->sess().dirs().temp_directory(), fn);

    if (auto o = FindFile(old_fn)) {
      // make case match.
      LOG(INFO) << "Move file: " << o.value().string();
      LOG(INFO) << "Move to  : " << path;
      File::Move(o.value(), path);
    }

  }
  return ret;
}

static constexpr int ZMODEM_RECEIVE_BUFFER_SIZE = 8132;
// 21 here is just an arbitrary number
static constexpr int ZMODEM_RECEIVE_BUFFER_PADDING = 21;

int doIO(ZModem* info) {
  u_char buffer[ZMODEM_RECEIVE_BUFFER_SIZE + ZMODEM_RECEIVE_BUFFER_PADDING];
  auto done = 0;
  auto doCancel = false; // %%TODO: make this true if the user aborts.

  while (!done) {
    const auto tThen = time(nullptr);
    if (info->timeout > 0) {
      zmodemlog("doIO: [{}] [timeout: {}] [state: {}]\n", tThen, info->timeout, info->state);
    }
    // Don't loop/sleep if the timeout is 0 (which means streaming), this makes the
    // performance < 1k/second vs. 8-9k/second locally
    while (info->timeout > 0 && !bout.remoteIO()->incoming() && !a()->sess().hangup()) {
      sleep_for(milliseconds(100));
      const auto tNow = time(nullptr);
      if ((tNow - tThen) > info->timeout) {
        zmodemlog("Break: [time: {}] Now.  Timedout: {}.  Time: {}\r\n", tNow, info->timeout,
                  tNow - tThen);
        break;
      }
      ProcessLocalKeyDuringZmodem();
    }

    ProcessLocalKeyDuringZmodem();
    if (a()->sess().hangup()) {
      return ZmErrCancel;
    }
    if (!bout.remoteIO()->connected()) {
      LOG(INFO) << "Lost connection during ZModem transfer.";
      return ZmErrCancel;
    }

    if (doCancel) {
      ZmodemAbort(info);
      fprintf(stderr, "cancelled by user\n");
      zmodemlog("cancelled by user");
      // resetCom();
      //%%TODO: signal parent we aborted.
      return 1;
    }
    if (const auto incomming = bout.remoteIO()->incoming(); !incomming) {
      done = ZmodemTimeout(info);
      // zmodemlog("ZmodemTimeout State [{}] [done:{}]\n", sname(info), done);
    } else {
      const int len = bout.remoteIO()->read(reinterpret_cast<char*>(buffer), ZMODEM_RECEIVE_BUFFER_SIZE);
      zmodemlog("ZmodemRcv Before [{}:{}] [{} chars] [done: {}]\n", sname(info), info->state, len, done);
      done = ZmodemRcv(buffer, len, info);
      //zmodemlog("ZmodemRcv After [{}] [{} chars] [done: {}]\n", sname(info), len, done);
    }
  }
  zmodemlog("doIO: [done: {}]\n", done);
  return done;
}

int ZXmitStr(const u_char* str, int len, ZModem* info) {
  bout.remoteIO()->write(reinterpret_cast<const char*>(str), len);
  return 0;
}

void ZIFlush(ZModem* info) {
  zmodemlog("ZIFlush\n");
  bout.remoteIO()->purgeIn();
  //sleep_for(milliseconds(100));
  // puts( "ZIFlush" );
  // if( connectionType == ConnectionSerial )
  //  SerialFlush( 0 );
}

void ZOFlush(ZModem* info) {
  zmodemlog("ZOFlush\n");
  // if( connectionType == ConnectionSerial )
  //	SerialFlush( 1 );
}

int ZAttn(ZModem* info) {
  zmodemlog("ZAttn\n");
  if (info->attn == nullptr) {
    return 0;
  }

  int cnt = 0;
  for (char* ptr = info->attn; *ptr != '\0'; ++ptr) {
    if (((cnt++) % 10) == 0) {
      sleep_for(milliseconds(1));
    }
    if (*ptr == ATTNBRK) {
      // SerialBreak();
    } else if (*ptr == ATTNPSE) {
#if defined(_DEBUG)
      zmodemlog("ATTNPSE\r\n");
#endif
      sleep_for(milliseconds(1));
    } else {
      bout.rputch(*ptr, true);
      // append_buffer(&outputBuf, ptr, 1, ofd);
    }
  }
  bout.flush();
  return 0;
}

/* set flow control as required by protocol.  If original flow
 * control was hardware, do not change it.  Otherwise, toggle
 * software flow control
 */
void ZFlowControl(int /* onoff */, ZModem* /* info */) {
  // I don't think there is anything to do here.
}

void ZStatus(int type, int value, char* msg) {
  switch (type) {
  case RcvByteCount:
    ZModemWindowXferStatus("ZModemWindowXferStatus: {} bytes received", value);
    // WindowXferGauge(value);
    break;

  case SndByteCount:
    ZModemWindowXferStatus("ZModemWindowXferStatus: {} bytes sent", value);
    // WindowXferGauge(value);
    break;

  case RcvTimeout:
    ZModemWindowStatus("ZModemWindowStatus: Receiver did not respond, aborting");
    break;

  case SndTimeout:
    ZModemWindowStatus("ZModemWindowStatus: {} send timeouts", value);
    break;

  case RmtCancel:
    ZModemWindowStatus("ZModemWindowStatus: Remote end has cancelled");
    break;

  case ProtocolErr:
    ZModemWindowStatus("ZModemWindowStatus: Protocol error, header={}", value);
    break;

  case RemoteMessage: /* message from remote end */
    ZModemWindowStatus("ZModemWindowStatus: MESSAGE: {}", msg);
    break;

  case DataErr: /* data error, val=error count */
    ZModemWindowStatus("ZModemWindowStatus: {} data errors", value);
    break;

  case FileErr: /* error writing file, val=errno */
    ZModemWindowStatus("ZModemWindowStatus: Cannot write file: {}", strerror(errno));
    break;

  case FileBegin: /* file transfer begins, str=name */
    ZModemWindowStatus("ZModemWindowStatus: Transfering {}", msg);
    break;

  case FileEnd: /* file transfer ends, str=name */
    ZModemWindowStatus("ZModemWindowStatus: {} finished", msg);
    break;

  case FileSkip: /* file transfer ends, str=name */
    ZModemWindowStatus("ZModemWindowStatus: Skipping {}", msg);
    break;
  }
}

FILE* ZOpenFile(char* file_name, uint32_t /* crc */, ZModem* /* info */) {
  const auto tfn = FilePath(a()->sess().dirs().temp_directory(), file_name).string();
#if defined(_DEBUG)
  zmodemlog("ZOpenFile filename='{}' [full path: {}]\r\n", file_name, tfn);
#endif
  return fopen(tfn.c_str(), "wb");

  //	struct stat	buf;
  //	bool		exists;	/* file already exists */
  //	static	int		changeCount = 0;
  //	char		file_name2[MAXPATHLEN];
  //	int		apnd = 0;
  //	int		f0,f1;
  //	FILE		*ofile;
  //	char		path[1024];
  //
  //	/* TODO: if absolute path, do we want to allow it?
  //	 * if relative path, do we want to prepend something?
  //	 */
  //
  //	if( *file_name == '/' )	/* for now, disallow absolute paths */
  //	  return nullptr;
  //
  //	buf.st_size = 0;
  //	if( stat(file_name, &buf) == 0 )
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
  //	zmodemlog("ZOpenFile: {}, f0={:x}, f1={:x}, exists={}, size={}/{}\n",
  //	  file_name, f0,f1, exists, buf.st_size, info->len);
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
  //	    zmodemlog("  ZMCRC: crc={:x}, FileCrc={:x}\n", crc, FileCrc(file_name) );
  //	    if( exists  &&  info->len == buf.st_size && crc == FileCrc(file_name) )
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
  //		sprintf(file_name2, "%s_%d", file_name, changeCount++);
  //		exists = stat(file_name2, &buf) == 0 || errno != ENOENT;
  //	      }
  //	      file_name = file_name2;
  //	    }
  //	    break;
  //	}
  //
  //	/* here if we've decided to accept */
  //	if( exists && !apnd && unlink(file_name) != 0 )
  //	  return nullptr;
  //
  //	/* TODO: build directory path if needed */
  //
  //	ZModemWindowStatus("Receiving: \"{}\"", file_name);
  //
  //	WindowXferGaugeMax(info->len);
  //
  //	ofile = fopen(file_name, apnd ? "a" : "w");
  //
  //	zmodemlog("  ready to open {}/{}: apnd = {}, file = {:x}\n",
  //	  getcwd(path,sizeof(path)), file_name, apnd, (long)ofile);
  //
  //	return ofile;
  // return nullptr;
}

int ZWriteFile(u_char* buffer, int len, FILE* file, ZModem* info) {
  /* If ZCNL set in info->f0, convert
   * newlines to unix convention
   */
  // if( info->f0 == ZCNL )
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
  // else
  if (info->f0 == ZCNL) {
#ifdef _DEBUG
    zmodemlog("ZCNL\n");
#endif // _WIN32
  }
  return fwrite(buffer, 1, len, file) == static_cast<unsigned int>(len) ? 0 : ZmErrSys;
}

int ZCloseFile(ZModem* info) {
  fclose(info->file);
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

void ZIdleStr(unsigned char* buf, int len, ZModem* info) {
  // PutTerm(buf, len);
  return;
#if 0
	char szBuffer[1024];
	strcpy( szBuffer, reinterpret_cast<const char *>( buf  ) );
	szBuffer[len] = '\0';
	if ( strlen( szBuffer ) == 1 ) {
		zmodemlog( "ZIdleStr: #[{}]\r\n", static_cast<unsigned int>( (unsigned char ) szBuffer[0] ) );
	} else {
		zmodemlog( "ZIdleStr: [{}]\r\n", szBuffer );
	}
#endif
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
