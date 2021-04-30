# CMake for WWIV

if (WWIV_SSH_CRYPTLIB AND NOT OS2) 
  include_directories(../deps/cl345)
endif()

include_directories(../deps/cereal/include)
include_directories(prot)

add_library(
  bbs_lib 
  acs.cpp
  application.cpp
  archivers.cpp
  attach.cpp
  automsg.cpp
  batch.cpp
  bbs.cpp
  bbs_event_handlers.cpp
  bbslist.cpp
  bbsovl1.cpp
  bbsovl2.cpp
  bbsovl3.cpp
  bbsutl.cpp
  bbsutl1.cpp
  bbsutl2.cpp
  chains.cpp
  chat.cpp
  chnedit.cpp
  conf.cpp
  confutil.cpp
  connect1.cpp
  crc.cpp
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
  gfiles.cpp
  gfileedit.cpp
  hop.cpp
  inetmsg.cpp
  inmsg.cpp
  instmsg.cpp
  interpret.cpp
  lilo.cpp
  listplus.cpp
  lpfunc.cpp
  misccmd.cpp
  message_file.cpp
  message_find.cpp
  mmkey.cpp
  msgbase1.cpp
  msgscan.cpp
  multinst.cpp
  multmail.cpp
  netsup.cpp
  newuser.cpp
  normupld.cpp
  save_qscan.cpp
  readmail.cpp
  read_message.cpp
  shortmsg.cpp
  showfiles.cpp
  sr.cpp
  srrcv.cpp
  srsend.cpp
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
  wqscn.cpp
  xfer.cpp
  xferovl.cpp
  xferovl1.cpp
  xfertmp.cpp
  xinit.cpp
  wfc.cpp
  basic/basic.cpp
  basic/util.cpp
  basic/wwiv.cpp
  basic/wwiv_data.cpp
  basic/wwiv_file.cpp
  basic/wwiv_io.cpp
  basic/wwiv_os.cpp
  basic/wwiv_time.cpp
  ../deps/my_basic/core/my_basic.c
  menus/config_menus.cpp
  menus/mainmenu.cpp
  menus/menucommands.cpp
  menus/menuspec.cpp
  menus/menusupp.cpp
  menus/printcommands.cpp
  qwk/qwk.cpp
  qwk/qwk_email.cpp
  qwk/qwk_mail_packet.cpp
  qwk/qwk_reply.cpp
  qwk/qwk_text.cpp
  qwk/qwk_ui.cpp
  qwk/qwk_util.cpp
  prot/crctab.cpp
  prot/zmodem.cpp
  prot/zmodemcrc.cpp
  prot/zmodemr.cpp
  prot/zmodemt.cpp
  prot/zmutil.cpp
  prot/zmwwiv.cpp
)

if (WWIV_SSH_CRYPTLIB AND NOT OS2) 
  target_sources(bbs_lib PRIVATE ssh.cpp)
  target_link_libraries(bbs_lib CL345_LIB)

endif()

if(UNIX) 
  target_sources(bbs_lib PRIVATE
      exec_unix.cpp 
    	make_abs_cmd_unix.cpp 
  )  
  if(CMAKE_SYSTEM_NAME MATCHES "SunOS.*")
    list(APPEND CMAKE_EXE_LINKER_FLAGS "-lsocket -lnsl -lkstat -lssp")
  endif()

  find_package (Threads)
  target_link_libraries(bbs_lib z util ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

endif()

if(WIN32)
  target_sources(bbs_lib PRIVATE
		exec_win32.cpp 
		make_abs_cmd_win32.cpp 
  )

  SET_MSVC_WARNING_LEVEL_4()
endif()


if (WWIV_SSH_CRYPTLIB AND NOT OS2) 
  target_compile_definitions(bbs_lib PRIVATE WWIV_HAS_SSH_CRYPTLIB)
  target_link_libraries( bbs_lib CL345_LIB )
endif()

target_link_libraries(
  bbs_lib 
  local_io 
  localui 
  common
  core 
  fsed
  sdk 
)

add_executable(bbs bbs_main.cpp)
target_link_libraries(bbs bbs_lib fmt::fmt-header-only)
