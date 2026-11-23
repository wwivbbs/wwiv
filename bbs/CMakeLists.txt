# CMake for WWIV

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
  basic/debug_model.cpp
  basic/debug_state.cpp
  basic/debugger.cpp
  basic/util.cpp
  basic/wwiv.cpp
  basic/wwiv_data.cpp
  basic/wwiv_file.cpp
  basic/wwiv_io.cpp
  basic/wwiv_os.cpp
  basic/wwiv_time.cpp
  basic/script_engine.cpp
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


if(UNIX) 
  message(STATUS "UNIX ROCKS")
  target_sources(bbs_lib PRIVATE
      exec_unix.cpp 
      "exec_socket.cpp"
      make_abs_cmd_unix.cpp 
  )  
  if(CMAKE_SYSTEM_NAME MATCHES "SunOS.*")
    list(APPEND CMAKE_EXE_LINKER_FLAGS "-lsocket -lnsl -lkstat -lssp")
  endif()

  find_package (Threads)
  target_link_libraries(bbs_lib z util ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} nlohmann_json::nlohmann_json)

endif()

if(WIN32)
  target_sources(bbs_lib PRIVATE
		exec_win32.cpp 
		"exec_socket.cpp"
		make_abs_cmd_win32.cpp 
  )

  set_max_warnings(bbs_lib)
endif()

if(OS2)
  target_sources(bbs_lib PRIVATE
		exec_os2.cpp 
		make_abs_cmd_os2.cpp 
  )
endif()
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(
  bbs_lib 
  local_io 
  localui 
  common
  core 
  fsed
  my_basic
  sdk 
  nlohmann_json::nlohmann_json

)

 find_path(CPP_HTTPLIB_INCLUDE_DIRS "httplib.h")
target_include_directories(bbs_lib PRIVATE ${CPP_HTTPLIB_INCLUDE_DIRS})

# The new BASIC engine (wwivbasic) and its wwiv.* bindings. Without
# WWIV_WWIVBASIC_NEXT only the legacy (my_basic) engine is built.
if (WWIV_WWIVBASIC_NEXT)
  target_sources(bbs_lib PRIVATE
    basic/next/next_engine.cpp
  )
  target_link_libraries(bbs_lib wwivbasic_interpreter)
  target_compile_definitions(bbs_lib PUBLIC WWIV_WWIVBASIC_NEXT)
endif()

if (WWIV_SSH_CRYPTLIB AND NOT OS2) 
  message(STATUS "WWIV_SSH_CRYPTLIB is ON")
  target_sources(bbs_lib PRIVATE ssh.cpp)
  target_link_libraries(bbs_lib cryptlib)
  target_compile_definitions(bbs_lib PRIVATE WWIV_HAS_SSH_CRYPTLIB)
endif()


add_executable(bbs bbs_main.cpp)
target_link_libraries(bbs bbs_lib fmt::fmt-header-only)
if (WWIV_SSH_CRYPTLIB AND NOT OS2)
  # Copies cl32.dll next to bbs.exe on Windows.
  wwiv_copy_runtime_dlls(bbs)
endif()

## Tests
if (WWIV_BUILD_TESTS)

    set(test_sources
      bbs_test_main.cpp
      bbs_helper.cpp
      bbs_macro_context_test.cpp
      bbslist_test.cpp
      bputs_test.cpp
      bputch_test.cpp
      datetime_test.cpp
      dropfile_test.cpp
      dsz_test.cpp
      email_test.cpp
      input_test.cpp
      make_abs_test.cpp
      message_file_test.cpp
      msgbase1_test.cpp
      netsup_test.cpp
      pause_test.cpp
      printfile_test.cpp
      quote_test.cpp
      qwk_test.cpp
      stuffin_test.cpp
      trashcan_test.cpp
      utility_test.cpp
      wutil_test.cpp
      xfer_test.cpp
      xferovl_test.cpp
      basic/basic_test.cpp
      basic/script_engine_test.cpp
      basic/util_test.cpp
    )
    add_executable(bbs_tests ${test_sources})

    if (WWIV_SSH_CRYPTLIB AND NOT OS2)
      # Copies cl32.dll next to bbs_tests.exe on Windows, so the tests
      # can be discovered and run without copying it by hand.
      wwiv_copy_runtime_dlls(bbs_tests)
    endif()

    if(APPLE)
      target_link_libraries(program "-framework CoreFoundation" "-framework Security" " -framework Foundation")
    endif()

    if(UNIX) 
      if(CMAKE_SYSTEM_NAME MATCHES "SunOS.*")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -lnsl -lkstat -lssp")
        endif()
      find_package(ZLIB REQUIRED)
    endif()

  include(GoogleTest)

  target_link_libraries(bbs_tests core bbs_lib core_fixtures common_fixtures GTest::gtest)
  gtest_discover_tests(bbs_tests)

  # End-to-end SSH test: OpenSSH client -> bbs -XS (cryptlib SSH server).
  # Windows uses cryptlib's prebuilt DLL and has no ssh in this setup.
  if (WWIV_SSH_CRYPTLIB AND NOT WIN32 AND NOT OS2)
    find_package(Python3 COMPONENTS Interpreter)
    if (Python3_Interpreter_FOUND)
      add_test(NAME ssh_logon_test
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/ssh_logon_test.py"
          --bbs "$<TARGET_FILE:bbs>"
          --wwivconfig "$<TARGET_FILE:wwivconfig>"
          --source "${PROJECT_SOURCE_DIR}")
      # 77: no ssh client on this machine.
      set_tests_properties(ssh_logon_test PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 180 LABELS integration)
    endif()
  endif()

endif() # WWIV_BUILD_TESTS
