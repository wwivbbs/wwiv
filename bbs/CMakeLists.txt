# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (BBS)
include_directories(..)
include_directories(../deps/cl342)
include_directories(../deps/cereal/include)

set(COMMON_SOURCES 
 application.cpp
 archivers.cpp
 arword.cpp
 asv.cpp
 attach.cpp
 automsg.cpp
 basic.cpp
 batch.cpp
 bbs.cpp
 bbsovl1.cpp
 bbsovl2.cpp
 bbsovl3.cpp
 bbsutl.cpp
 bbsutl1.cpp
 bbsutl2.cpp
 bgetch.cpp
 bputch.cpp
 chains.cpp
 chat.cpp
 chnedit.cpp
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
 email.cpp
 events.cpp
 execexternal.cpp
 external_edit.cpp
 extract.cpp
 finduser.cpp
 full_screen.cpp
 gfiles.cpp
 gfileedit.cpp
 hop.cpp
 inetmsg.cpp
 inmsg.cpp
 input.cpp
 instmsg.cpp
 interpret.cpp
 lilo.cpp
 listplus.cpp
 local_io.cpp
 local_io_curses.cpp
 lpfunc.cpp
 menu.cpp
 menuinterpretcommand.cpp
 menu_parser.cpp
 menuspec.cpp
 menusupp.cpp
 message_file.cpp
 misccmd.cpp
 mmkey.cpp
 msgbase1.cpp
 msgscan.cpp
 multinst.cpp
 multmail.cpp
 netsup.cpp
 newuser.cpp
 new_bbslist.cpp
 normupld.cpp
 null_local_io.cpp
 pause.cpp
 printfile.cpp
 qscan.cpp
 quote.cpp
 qwk.cpp
 qwk1.cpp
 readmail.cpp
 read_message.cpp
 remote_socket_io.cpp
 shortmsg.cpp
 showfiles.cpp
 smallrecord.cpp
 sr.cpp
 srrcv.cpp
 srsend.cpp
 ssh.cpp
 stdio_local_io.cpp
 stuffin.cpp
 subacc.cpp
 subedit.cpp
 sublist.cpp
 subreq.cpp
 syschat.cpp
 sysopf.cpp
 sysoplog.cpp
 trashcan.cpp
 trytoul.cpp
 uedit.cpp
 utility.cpp
 valscan.cpp
 vote.cpp
 voteedit.cpp
 remote_io.cpp
 output.cpp
 wqscn.cpp
 xfer.cpp
 xferovl.cpp
 xferovl1.cpp
 xfertmp.cpp
 xinit.cpp
 wfc.cpp
 workspace.cpp
 ../deps/my_basic-master/core/my_basic.c
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
  # Enable if you are cleaning them up actively.
  # add_definitions ("-Wall")
  set(PLATFORM_SOURCES 
    	platform/unix/exec.cpp 
    	platform/unix/utility2.cpp 
  )
  find_package (Threads)
  if(APPLE)
    set(PLATFORM_SOURCES ${PLATFORM_SOURCES} platform/osx/macversioninfo.mm)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework Foundation")
    include_directories(platform/osx)
  endif()
  if(CMAKE_SYSTEM_NAME MATCHES "SunOS.*")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lsocket -lnsl -lkstat -lssp")
  endif()
endif()

add_library(bbs_lib ${COMMON_SOURCES} ${ZMODEM_SOURCES} ${PLATFORM_SOURCES}) 
target_link_libraries(bbs_lib localui core sdk ${CL342_LIB} ${CMAKE_THREAD_LIBS_INIT})
add_executable(bbs ${BBS_MAIN})
target_link_libraries(bbs bbs_lib)
