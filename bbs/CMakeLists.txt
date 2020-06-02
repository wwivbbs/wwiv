# CMake for WWIV
include_directories(../deps/cl342)
include_directories(../deps/cereal/include)
Include(FindWWIVCurses)

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
 context.cpp
 crc.cpp
 datetime.cpp
 defaults.cpp
 diredit.cpp
 dirlist.cpp
 dropfile.cpp
 dsz.cpp
 email.cpp
 execexternal.cpp
 external_edit.cpp
 external_edit_qbbs.cpp
 external_edit_wwiv.cpp
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
 lpfunc.cpp
 menu.cpp
 menuinterpretcommand.cpp
 menu_parser.cpp
 menuspec.cpp
 menusupp.cpp
 message_editor_data.cpp
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
 pause.cpp
 printfile.cpp
 save_qscan.cpp
 quote.cpp
 qwk.cpp
 qwk1.cpp
 readmail.cpp
 read_message.cpp
 remote_socket_io.cpp
 shortmsg.cpp
 showfiles.cpp
 sr.cpp
 srrcv.cpp
 srsend.cpp
 ssh.cpp
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
 ../deps/my_basic/core/my_basic.c
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
  set(PLATFORM_SOURCES 
    	exec_unix.cpp 
    	make_abs_cmd_unix.cpp 
  )
  find_package (Threads)
  set(FORKPTY_LIB util)
  
  if(CMAKE_SYSTEM_NAME MATCHES "SunOS.*")
    list(APPEND CMAKE_EXE_LINKER_FLAGS "-lsocket -lnsl -lkstat -lssp")
  endif()
endif()

if(WIN32)
  set(
    PLATFORM_SOURCES
		  exec_win32.cpp 
		  make_abs_cmd_win32.cpp 
  )
  SET_MSVC_WARNING_LEVEL_4()
endif()

add_library(
  bbs_lib 
  ${COMMON_SOURCES} 
  ${ZMODEM_SOURCES} 
  ${PLATFORM_SOURCES}
) 

target_link_libraries(
  bbs_lib 
  local_io 
  localui 
  core 
  sdk 
  CL342_LIB 
  ${CMAKE_THREAD_LIBS_INIT} 
  ${FORKPTY_LIB}
)

add_executable(bbs ${BBS_MAIN})
target_link_libraries(bbs bbs_lib fmt::fmt-header-only)
