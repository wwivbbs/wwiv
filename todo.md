# WWIV BBS TODO List
# Copyright 2002-2021, WWIV Software Services
***

# TODO debugger


# TODO Cleanups
* make a SideMenu class so we don't need statics in side_menu method.
* redo readmail(...) It's a friggin mess.
* make nightly event or fix check for mail forwarded to a deleted user, and then fix that.


# Things to add to fix.

* Check for no conferences
* Check for subs not part of any conference (dirs done in wwivutil fix)
* sub storage type != 2

# CONFIG.JSON

* make sure wwivconfig can edit wwivd and networks for legacy 4.24 and 4.30 format

# WWIVconfig

# MCI|PIPE
* Add tests for BbsMacroFilter (also make sure it works now)
* Use BbsMacroFilter in wwivutil print.

TODO Expression:
|{uppercase user.name)}

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
* Add ability to convert between a FidoPackedMessage and FidoStoredMessage.
  Then we can move dupes to a badmessage area as FidoStoredMessage (.msg)
* Update BBS list for network to use FTN nodelist.
* Doesn't seem that wwivd checks for crash status on callout since
  wwiv::sdk::callout never has the right file sizes for FTN, so it doesn't
  know if anything is waiting.


## Container work
***
* Create default directory structure for in containers
* move all binaries to bin, see what breaks


## Infrastructure Cleanup
***
* Make sure the Network.network_number is accurate, then try
  to get rid of the other network_numbers everywhere.
* Try to remove a()->current_net() and a()->net_num(), pass in the net 
  as much as possible.
* cleanup wfc since we aren't waiting for a call
* add tests for usermanager and user (in SDK)
* stop using strncpy, strcpy.
* Finish SDK QScan class (sdk/qscan.h UserQScan is experimental) and
  move BBS qscan handling onto it.
* Move to LibSSH on all platforms
* Run various asan/tsan's regularly with gcc via CMake
* start using clock more.
* move to datetime as much as possible.
* Stop calling input use the input_xxx routines instead.  Make Input1 
  just support a fixed set of characters (like numbers, A-Z, 
  high ascii too, etc).
* cleanup context class (doc comments, treat type-0 as email api
* Move the userrec from User class to heap from stack.
* add to_number<T> override that takes a default value, and then 
  change to_number<T>
  to return an std::optional<T> as the return type.
* Switch all BBS LocalIO subclasses to use curses KEY types not Win32 key code
  types for characters >0xff. (i.e. KEY_LEFT not LEFT)
* Finish making bbs only use UserManager that returns optional


## core::File Improvements
***
* File::lock is unfinished on non-Windows platforms


## Linux Work
***
* TopScreen doesn't work with CursesIO

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
* Add overload of AddFile, UpdateFile and handles external 
  descirptions too, i.e. add a 3rd parameter for the external
  description.  Or put it into the FileRecord
  class.

## Closed Questions
***

1) Guest user account. Is it useful?   - Yes, it just needs to be improved
   and documented.
