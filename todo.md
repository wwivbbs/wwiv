# WWIV BBS TODO List
# Copyright 2002-2020, WWIV Software Services
***

## Build Cleanup.
* add some cmake install targets
  cmake .. -DCMAKE_INSTALL_PREFIX=<location>
  cmake --build . --target install


# Install
curl https://build.wwivbbs.org/jenkins/job/wwiv/lastStableBuild/label=linux-debian10/api/json
?pretty=true > jenkins.json
jq ".url+.artifacts[].relativePath" jenkins.json
jq ".number" jenkins.json

## Networking Cleanup
***
* Figure out why new networking stack doesn't work for mark. 

## FTN
***
* add option to save packets
* ability to put more than 1 message per packet.
* zone:region/node is acceptible (not just zone:net/node)
* Add ability to convert between a FidoPackedMessage and FidoStoredMessage.  Then 
  we can move dupes to a badmessage area as FidoStoredMessage (.msg)
* Update BBS list for network to use FTN nodelist.


## Ini Files and Configuration
***

## Container work
***
* Add WWIV_CONFIG_DIR to point to where the config.json file lives (default to config.dat's data)
* Create default directory structure for in containers
* move all binaries to bin, see what breaks


## Infrastructure Cleanup
***
* Try to remove a()->current_net() and a()->net_num(), pass in the net as much as possible.
* make container that holds net_networks_rec *and* the current network number

* cleanup wfc since we aren't waiting for a call
* add tests for usermanager and user (in SDK)
* stop using strncpy, strcpy.
* Create a SDK QScan class to wrap interacting with the WWIV qscan
  variables and also load/save user.qsc
* Move to LibSSH on all platforms
* Run various asan/tsan's regularly with gcc via CMake
* start using clock more.
* move to datetime as much as possible.
* Stop calling input use the input_xxx routines instead.  Make Input1 just support a
  fixed set of characters (like numbers, A-Z, high ascii too, etc).
* cleanup context class (doc comments, treat type-0 as email api
* Move the userrec from User class to heap from stack.
* simplify names in WUser
* add to_number<T> override that takes a default value, and then change to_number<T>
  to return an std::optional<T> as the return type.
* Switch all BBS LocalIO subclasses to use curses KEY types not Win32 key code
  types for characters >0xff. (i.e. KEY_LEFT not LEFT)


## core::File Improvements
***
* File::lock doesn't seem to be finished


## Feature Improvements
***
* Add pipe code and heart code support to wwivutil print.
  - This needs a context class to hold current session state
    that is now spit across Application class and other
    global variables.
* make a common ACS system (like TG, vs. having separate 
  SL DSL AR fields everywhere for menus)
* cleanup menu system. It's awful.

## Linux Work
***
* TopScreen doesn't work with CursesIO
* stop using localIO()->WhereX to determine remote position

## Net52
***

## Message Areas
***
* detangle global net_email_name (maybe it needs to be added to EmailData?)
* Make the emailmessage extend the message so most things are common
  between them.
* Use read_type2_message everywhere
* make read_type2_message use the msgapi
* use the msgapi more in WWIV code.

# File Areas
***
* cleanup char[] usage
* support moving file in sdk. (Maybe FileApi::MoveFile(old_area, new_area))
* Move more batch code into batch class.
* move FileList into context (for tagged files)
* get rid of stripfn
* move core logic from get_file_idz into sdk
* make wwivutil fix use filesapi for extended descriptions.
* Add overload of AddFile, UpdateFile and handles external descirptions too, i.e.
  add a 3rd parameter for the external description.  Or put it into the FileRecord
  class.

## Parser
***

## Open Questions
***

## Closed Questions
***

1) Guest user account. Is it useful?   - Yes, it just needs to be improved
   and documented.
