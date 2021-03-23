# CMake for WWIV 5

set(WWIVUTIL_MAIN wwivutil.cpp)

set(COMMAND_SOURCES
  "command.cpp"
  "acs/acs.cpp"
  "conf/conf.cpp"
  "config/config.cpp"
  "email/email.cpp"
  "fido/fido.cpp"
  "fido/dump_fido_packet.cpp"
  "fido/dump_nodelist.cpp"
  "fido/dump_fido_subscribers.cpp"
  "files/allow.cpp"
  "files/arc.cpp"
  "files/files.cpp"
  "files/tic.cpp"
  "fix/dirs.cpp"
  "fix/fix.cpp"
  "fix/users.cpp"
  "instance/instance.cpp"
  "messages/messages.cpp"
  "menus/menus.cpp"
  "net/dump_bbsdata.cpp"
  "net/dump_callout.cpp"
  "net/dump_connect.cpp"
  "net/dump_contact.cpp"
  "net/dump_subscribers.cpp"
  "net/dump_packet.cpp"
  "net/list.cpp"
  "net/net.cpp"
  "net/req.cpp"
  "net/send.cpp"
  "print/print.cpp"
  "status/status.cpp"
  "subs/import.cpp"
  "subs/subs.cpp"
  "users/users.cpp"
  "help.cpp"
  "util.cpp"
  )

set_max_warnings()

add_executable(wwivutil ${WWIVUTIL_MAIN} ${COMMAND_SOURCES})
target_link_libraries(wwivutil common core binkp_lib sdk)
