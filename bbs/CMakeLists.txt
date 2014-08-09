# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (WWIV5)
include_directories(.)
include_directories(..)
include_directories(platform)

set(COMMON_SOURCES 
 archivers.cpp
 asv.cpp
 attach.cpp
 automsg.cpp
 batch.cpp
 bbs.cpp
 bbslist.cpp
 bbsovl1.cpp
 bbsovl2.cpp
 bbsovl3.cpp
 bbsutl.cpp
 bbsutl1.cpp
 bbsutl2.cpp
 bgetch.cpp
 bputch.cpp
 callback.cpp
 chains.cpp
 chat.cpp
 chnedit.cpp
 colors.cpp
 com.cpp
 conf.cpp
 confutil.cpp
 connect1.cpp
 crc.cpp
 datetime.cpp
 defaults.cpp
 diredit.cpp
 dirlist.cpp
 dropfile.cpp
 dupphone.cpp
 events.cpp
 execexternal.cpp
 extract.cpp
 finduser.cpp
 gfiles.cpp
 gfledit.cpp
 hop.cpp
 inetmsg.cpp
 inmsg.cpp
 input.cpp
 instmsg.cpp
 interpret.cpp
 lilo.cpp
 listplus.cpp
 lpfunc.cpp
 memory.cpp
 menu.cpp
 menuedit.cpp
 menuinterpretcommand.cpp
 menuspec.cpp
 menusupp.cpp
 misccmd.cpp
 msgbase.cpp
 msgbase1.cpp
 msgscan.cpp
 multinst.cpp
 multmail.cpp
 netsup.cpp
 newuser.cpp
 normupld.cpp
 pause.cpp
 printfile.cpp
 qscan.cpp
 quote.cpp
 qwk.cpp
 qwk1.cpp
 readmail.cpp
 shortmsg.cpp
 showfiles.cpp
 smallrecord.cpp
 sr.cpp
 srrcv.cpp
 srsend.cpp
 status.cpp
 strings.cpp
 stuffin.cpp
 subacc.cpp
 subedit.cpp
 sublist.cpp
 subreq.cpp
 subxtr.cpp
 syschat.cpp
 sysopf.cpp
 sysoplog.cpp
 trytoul.cpp
 uedit.cpp
 user.cpp
 utility.cpp
 valscan.cpp
 version.cpp
 vote.cpp
 voteedit.cpp
 wcomm.cpp
 wfc.cpp
 woutstream.cpp
 woutstreambuffer.cpp
 wqscn.cpp
 wsession.cpp
 wuser.cpp
 xfer.cpp
 xferovl.cpp
 xferovl1.cpp
 xfertmp.cpp
 xinit.cpp
)

set(BBS_MAIN bbs_main.cpp)

include_directories(prot)
set(ZMODEM_SOURCES 
  prot/crctab.cpp
  prot/zmodem.cpp
  prot/zmodemcrc.cpp
  prot/zmodemr.cpp
  prot/zmodemt.cpp
  prot/zmutil.cpp
  prot/zmwwiv.cpp
)

if(UNIX) 
  include_directories(platform/unix)
  set(PLATFORM_SOURCES 
	platform/wutil.cpp 
	platform/unix/exec.cpp 
	platform/unix/utility2.cpp 
	platform/unix/wlocal_io.cpp
	platform/unix/stringstuff.cpp 
	platform/unix/filestuff.cpp 
	platform/unix/wiou.cpp)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  if(APPLE)
    set(PLATFORM_SOURCES ${PLATFORM_SOURCES} platform/osx/macversioninfo.mm)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework Foundation")
    include_directories(platform/osx)
    message("Detected OS as APPLE.")
  endif()
endif()


add_library(bbs_lib ${COMMON_SOURCES} ${ZMODEM_SOURCES} ${PLATFORM_SOURCES}) 
add_executable(bbs ${BBS_MAIN})
target_link_libraries(bbs bbs_lib core)

