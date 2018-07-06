# WWIV BBS TODO List
# Copyright 2002-2018, WWIV Software Services
***

## FTN
***
* improve addressing for outbound netmail
* add option to save packets
* ability to put more than 1 message per packet.
* zone:region/node is acceptible (not just zone:net/node)
* dupe checking code is a mess and probably won't work,
  need to check for both address and serial number or 
  crc of headers too.

## Ini Files and Configuration
***
* Add net.ini support into network{1,2,3}

## Infrastructure Cleanup
***
* get rid of more char[] variables from vars.h, move to wsession as strings.
* cleanup wfc since we aren't waiting for a call
* add tests for usermanager and user (in SDK)
* Maybe make the hangup variable a static in the Application class.
* Try to remove a()->current_net(), pass in the net as much as possible.
* Update syscfg.sysconfig in init and save it back, since
  wwiv never saves config.dat anymore.
* stop using strncpy, strcpy, strcasestr.
* get rid of g_flags, make it a boolean in application somewhere else
  that's not global scoped.
* get rid of curatr as a global variable. Looks like it should be
  part of Output class.
* Create a SDK QScan class to wrap interacting with the WWIV qscan
  variables and also load/save user.qsc
* Move to LibSSH on all platforms
* Run various asan/tsan's regularly with gcc via CMake
* Drop Debian8


## Networking Cleanup
***
* Figure out why new networking stack doesn't work for mark.
* set status_pending_net on posts like wwivtoss does if needed.

## core::File Improvements
***
* move File to namespace wwiv::core
* start migrating core::File towards fs::filesystem and fs::path naming. (in progress)
  - also todo is to figure out if we want to add some interoperability with
    iostreams here.
* Create SemaphoreFile class,later on also add support for creating semaphores for network
  and other places in wwiv.

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
