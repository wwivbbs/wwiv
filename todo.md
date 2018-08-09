# WWIV BBS TODO List
# Copyright 2002-2018, WWIV Software Services
***

## FTN
***
* add option to save packets
* ability to put more than 1 message per packet.
* zone:region/node is acceptible (not just zone:net/node)

## Ini Files and Configuration
***

## Infrastructure Cleanup
***
* Try to remove a()->current_net() and a()->net_num(), pass in the net as much as possible.
* make container that holds net_networks_rec *and* the current network number

* cleanup wfc since we aren't waiting for a call
* add tests for usermanager and user (in SDK)
* Update syscfg.sysconfig in init and save it back, since
  wwiv never saves config.dat anymore.
* stop using strncpy, strcpy, strcasestr.
* Create a SDK QScan class to wrap interacting with the WWIV qscan
  variables and also load/save user.qsc
* Move to LibSSH on all platforms
* Run various asan/tsan's regularly with gcc via CMake
* Drop Debian8
* start using clock more.
* move to datetime as much as possible.

## Networking Cleanup
***
* Figure out why new networking stack doesn't work for mark.

## core::File Improvements
***
* start migrating core::File towards fs::filesystem and fs::path naming. (in progress)
  - also todo is to figure out if we want to add some interoperability with
    iostreams here.  Note: This needs C++17 (ish) support, so needs debian9 as the
    baseline since GCC 4.x only suppors std=gnu++14 as the latest.
* File::lock doesn't seem to be finished


## Feature Improvements
***
* Create wwivutil print command to print files in ANSI with 
  heart and pipe codes
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

## Open Questions
***

## Closed Questions
***

1) Guest user account. Is it useful?   - Yes, it just needs to be improved
   and documented.
