==============================================================================
                             WWIV 5.0 Source Code
              Copyright 2002-2005 WWIV Software Services
==============================================================================


GENERAL INFORMATION:

    WWIV 5.0 is can be built with the following compilers:
        MS Visual C++ .NET 2003 (Standard/Professional/Enterprise/Architect)
        MS Visual C++ .NET (Standard/Professional/Enterprise/Architect)
        MS Visual C++ .NET (Standard/Professional/Enterprise/Architect)
        MS Visual C++ 6.0 (Standard/Professional/Enterprise) [Not tested lately]
        Borland C++ Builder 5.0 (Standard/Professional/Enterprise) [Not tested lately]
        Borland C++ 5.5 (Free Command Line Tools Edition from [Not tested lately]
            http://community.borland.com)
        GCC 2.95.2-Mingw32 (You need to patch the libraries with the  [Not tested lately]
            binary release from 020300 or later, as well as add a
            few missing functions into winsock2.h)
            MinGW is available at: http://www.mingw.org/

    I recommend using MSVC .NET 2005. (Since there is a the "express" edition
    available for free.
    
    The command line parameters have changed quite a bit, I suggest
    running "WWIV50 -? | MORE" to see the list of changes.  Currently
    WWIV50 is command line compatable with EleBBS (for all of the
    major command line arguments)

    To run with the integrated Telnet Server, run "WWIV50 -TELSRV",
    this is mainly inlcuded so that you can debug a test telnet session,
    the recommend way of running WWIV Telnet only is from a Front End
    Program (such as Argus, or the internal wwiv telnet server included
    under the telsrv directory).  The command line for the DOOR in Argus
    is "X:\WWIV\WWIV50 -H%h -XT -N%n")

    So far, WWIV 5.0 is still fully compatable with your existing WWIV 4.30
    installation (Just drop it in and go).

    If you run a local node (i.e. -M) then WWIVnet callouts will still occur,
    (this is due to the fact that many use the PPP Project that that is not
    affected by Serial Communications for the node.  There is a bool setting
    at the top of BBS.CPP which you can set to false to disable this feature)

    Another note: If you want to use the FOSSIL emulation (thanks to Rob from
    Synchronet for the VXD/VDD and also tons of help using it), you MUST
    get the WWIVSYNC.ZIP and unzip it into your WWIV50 directory.


==============================================================================

LABEL: WWIV-5_0_61
DATE:  ??/??/2005

Rushfan - Fixed up more time_t != sizeof(int) issues in the code, mainly with localtime
Rushfan - Some const correctness work, also using ISO C++ functionanmes in more places
Rushfan - Changed WStatusMgr::Get to return a bool instead of calling AbortBBS directly
Rushfan - Made WFile::Write take a const void * parameter to help fix const correctness
Rushfan - Implemented windows trashcan support in WFile::Delete
Rushfan - Changed ANSI file code over to WFile in several places througout the code.
Rushfan - Got rid of filelength (use WFile::GetLength()) now.
Rushfan - renamed islname to GetTemporaryInstanceLogFileName
Rushfan - renamed slname to GetSysopLogFileName
Rushfan - Split up each of the different dropfile creation code into separate functions
Rushfan - renamed sl1 to AddLineToSysopLogImpl
Rushfan - Started making WStatusMgr support transactions so status can stop being global
Atani   - various linux compilation issues fixed
Rushfan - Added WFile::Exists(dir,filename), WFile::Remove(string)
Rushfan - Fixed issue with startup (not starting) and cleaned up some xinit code
Rushfan - Introduced WTextFile as a replacement for the CRT FILE* handle + our fsh_XXX code
Rushfan - Started making utility objects not depend on so much of the BBS so that
          WFile, WTextFile, WUser, WUserManager can be used outside of the BBS, eventually
          we will have classes for messagebase manipulation available from outside of WWIV.
Rushfan - Some more const-correctness in the transfer code to make life easier in other places
Rushfan - Removed almost all usages of status
Rushfan - fsh_open was moved into WTextFile (WTextFile::OpenImpl).
Rushfan - last usages of fsh_XXXX is migrated to WTextFile
Rushfan - Changed thisuser to GetCurrentUser
Rushfan - Moved WLocalIO::pr_Wait to WSession::DisplaySysopWorkingIndicator
Rushfan - nl => WOutStream::NewLine, ansic => WOutStream::Color, 
          setc => WOutStream::SystemColor, BackSpace & BackLine &
          DisplayLiteBar => WOutStream, reset_colors => WOutStream::ResetColors
          goxy => WOutStream::GotoXY, bputs => WOutStream::Write
          bprintf => WOutStream::WriteFormatted
Rushfan - Started working on WIniFile Class
Rushfan - ** IMPORTANT CHANGE ** In chat.ini, CH_PROMPT's allowable values
          for true and false are now Y and N (like the rest of WWIV), and
          not 1 and 0.  Please change this in CHAT.INI when you upgrade to
          this version.  I am standardizing the INI files as much as possible
          and making 1 set of INI code for WWIV and all utilities to share.
Rushfan - Started replacing ini_init/ini_get with WIniFile



==============================================================================

LABEL: WWIV-5_0_60
DATE:  11/28/2005

Rushfan - Fixed issue with message base corruption


==============================================================================

LABEL: WWIV-5_0_59
DATE:  11/20/2005

Atani   - sf-bug 1215436 - door32.sys contained extra "l" after baud rate
Rushfan - Started working on Mac OS X Support
Rushfan - Changed global app to GetApplication(), sess to GetSession().  Added
          GetComm(), GetUserManager(), GetLocalIO(), and GetStatusManager() 
          to WBbsApp and made the previously public member variables private 
          and updated all access through the accessors
Rushfan - Update size test code to handle Mac OS X as well as Linux/Win32 and
          MSDOS.
Rushfan - Updated Copyright statement in header files to 2005
Rushfan - removed unused files extrn.cpp and extrn1.cpp from CVS finally.
Rushfan - Updated code to compile under VS.NET 2005 (using VC++ Express)
Rushfan - Got rid of WWIV_Copy_File and moved it to WFile.  Also added WFile::MoveFile
          which needs to be implemented on UNIX still.
Rushfan - WFile no longer includes wwiv.h (and should now work outside of WWIV)


==============================================================================

LABEL: WWIV-5_0_58
DATE:  11/30/2004

Rushfan - Fixed a typo/bug in the sending the initial telnet sequences, look
          for echo being broken or linemode being enabled on some lesser
          telnet clients
Rushfan - More conversion of sprintf to snprintf and replacing hard coded sizes
          with sizeof for non-pointer strings.
Atani   - Fixed linux build, added MAX_PATH to WFile.h (if not defined)
Rushfan - Fixed bug [ 1059251 ] user can't login -- Issue with the phone number
          verification support.  This should be fixed now.

==============================================================================

LABEL: WWIV-5_0_57
DATE:  10/22/2004

Rushfan - Moved shutdown information and variables from WSession to WBbsApp.
Rushfan - Changed skey to take an int vs. unsigned char.
Rushfan - More ongoing work on removing unneeded unsigned data type usages.
Rushfan - Disabled remote.exe and remotes.dat support by default.
Rushfan - Fixed display bug in voting booth.
Rushfan - got rid of number_userrecs for WUserManager::GetNumberOfUserRecords
Rushfan - split out code from user.cpp to SmallRecord.cpp and FindUser.cpp
Rushfan - Moved function prototypes from fcns.h to WStringUtils.h for the
          string functions contained in that file.
Rushfan - Started making WUser and WUserManager self-contained so it can be
          used outside of the main bbs code more easily.

==============================================================================

LABEL: WWIV-5_0_56 (Beta-3)
DATE:  9/8/2004

Rushfan - Removed registration code from lilo


==============================================================================

LABEL: WWIV-5_0_55
DATE:  9/8/2004

Rushfan - Moved WFC initialization out of xinit and into wfc_init
Rushfan - Moved WFC variables out of WSession into wfc.cpp module.
Rushfan - Removed global use_forcescan as it's not used (it's an ini flag)
Rushfan - Got rid of INT32, made it all int.
Rushfan - Changed dir_dates from ULONG32 to UINT32 since that's what they are
Rushfan - Moved dir_dates and sub_dates into WSession class.
Rushfan - Implemented more accessors in WSession
Rushfan - Changed the length of all c-style strings with length 161 to 255
Rushfan - Moved PPP email information into WSession and made ppp_realname bool
Rushfan - Removed global variable in_fsed since we don't need it anymore.
Rushfan - Replaced lecho variable with calls to AllowLocalSysop
Rushfan - Removed unused global "ppp_domain_usernum"
Rushfan - Fixed chainedit/boardedit where unsigned chars were displaying
          as characters and not as numbers after the streams conversion.
Rushfan - Removed extra nl in message header display
Rushfan - More streams work.  Fixed bug in WOutStream with a null parameter.
Atani   - Cleaned up warnings under linux
Atani   - Linux build fixes
Rushfan - Fixed message read going 1 line too far.
Rushfan - Removed trailing whitespace in all files.
Rushfan - Started to use stringstream vs. sprintf calls
Rushfan - replaced printf calls with streams to cout
Rushfan - removed EOL() as it did the same as clearline(), also renamed 
          clearline() to ClearEOL().  Renamed spin to SpinPuts, left to
          MoveLeft, and backprint to BackPrint, bempty to bkbhit,
          get1c to bgetchraw, comhit to bkbhitraw
Rushfan - Still working on making WSession use encapsulation.
Rushfan - Updated Copyrights and License Statements to reflect Apache License
          
==============================================================================

LABEL: WWIV-5_0_54
DATE:  01/19/2004

Rushfan - Merged system_operation_rec with WSession
Rushfan - Added Set/Has/Clear/Toggle+ConfigFlag to WBbsApp and use it 
          instead of (former sysinfo.flags)
Rushfan - Fixed MSVC6 build i.r.t STL usages.
Rushfan - cleaned up a couple of the /W4 warnings.
Rushfan - Found more places where I wasn't using control-code #defines - fixed
Rushfan - Created WSession.cpp and started moving code to it from  WSession.h
Rushfan - Created and Fixed a GPD caused by a memset call to WSession()
Rushfan - Started using iostreams instead of bputs/bprintf in a couple
          of places. Not ready for a full-scale conversion yet, but
          investigations are underway.
Rushfan - Fixed "(x years old)" display in newuser logon
Rushfan - Fixed Full Screen Editors

==============================================================================

LABEL: WWIV-5_0_53
DATE:  01/12/2004

Rushfan - Fixed litebar call in list files.
Rushfan - Fixed display of chains (missing part of the pipe color code)
Rushfan - Introduced WUser class for user record.
Rushfan - Fixed fast logon for users other than user #1
Rushfan - nam and nam1 now takes userrec as a const parameter.
Rushfan - Introduced WUser and WUserManager classes.  sess->thisuser is now
          a WUser object instead of a userrec structure.
Rushfan - Added WSession::ReadCurrentUser and WSession::WriteCurrentUser
Atani   - Fixes for linx build
Rushfan - Added broadcast( s, ... ) and switched to it.
Rushfan - More fleshing out of WUser C++ class.
Rushfan - Making more parameters in the message base, file areas, and 
          conferences code const so it can use the const char* return values 
          from WUser member functions.
Rushfan - Fixed bug in message base code that could assert in a debug build
          when the message being quoted had control characters
Rushfan - More constness fixes and variable name sanitization.
Rushfan - Finished WUser class and use it virtually everywhere.
Rushfan - Added input function that take string vs. char*
Rushfan - renamed input_pw1 to change_password
Rushfan - Added StringTrimBegin, StringTrimEnd, and StringTrim that works on
          STL string classes to WStringUtils.
Rushfan - Converted most hard-coded color codes back into WWIV color codes
          instead of hard color attributes (still uses pipe syntax though)
Rushfan - Added #defines to be used when calling inmsg and external_edit to 
          the list of other #defines at WConstants.h
          
==============================================================================

LABEL: WWIV-5_0_52 (Beta-2)
DATE:  01/03/2004

Rushfan - Use masked field editline (used in F1 user editor)
Rushfan - Changed the message scan prompt to be 1 line instead of 2.
          Does anyone like it?  dislike it?
Rushfan - Added litebar calls to voting booth and yourinfo
Rushfan - In UEdit, if you are editing user #1, you can increase that SL/DSL
          even if your SL/DSL is lower.
Rushfan - Telnet should now respond with a "IAC WILL SUPPRESS GA" after 
          receiving a "IAC DO SUPPRESS GA".
Rushfan - Now sends a IAC DON'T ECHO along with the IAC WILL ECHO at startup.
		  (these last 2 things makes putty happy)      
Rushfan - Wrapped WIOTelnet::purgeIn in a Mutex for good measure.	
Rushfan - Split up existing files into more files; bgetch.cpp, bputch.cpp, 
          pause.cpp, interpret.cpp, stuffin.cpp, execexternal.cpp, and 
          printfile.cpp
Rushfan - Moved editline to WLocalIO::LocalEditLine and editlinestrlen to 
          WLocalIO::EditLineStringLength
Rushfan - Replaced itoa with sprintf calls.
Rushfan - Replaced ltoa and ultoa with sprintf calls.
Rushfan - Renamed WLocalIO::LocalXYPrintf to WLocalIO::LocalXYAPrintf and
          created a new LocalXYPrintf, LocalPrintf and a new LocalXYPuts
Rushfan - Replaced many LocalGotoXY and LocalFastPuts combos with LocalXYPuts
          LocalXYPrintf, or LocalXYAPrintf calls
Rushfan - Fixed Pending Net Display.
Rushfan - WFC Cleanups (display issues and misc code cleanup)
Rushfan - print_net_listing now takes a bool argument instead of an int.
Rushfan - Added EFLAG_NONE to use instead of a 0
Rushfan - Added #defines to pass to usedit ( UEDIT_NONE, UEDIT_FULLINFO, and 
          UEDIT_CLEARSCREEN ) to use instead of hard coded numbers.
Rushfan - Updated Copyright Statements to 2004.


==============================================================================

LABEL: WWIV-5_0_51
DATE:  12/18/2003

Rushfan - Got rid of utoa macros, replaced with ultoa
Rushfan - Fixed %% parameter in stuff_in, this has never worked. (it added
          %s instead of % to the commandline)
Rushfan - Updated L&F of timebank.
Rushfan - renamed ok_local() to AllowLocalSysop()
Rushfan - renamed checkpw() to ValidateSysopPassword()
Rushfan - Changed play_sdf's abortable param to a bool & used the macro 
          versions of filenames that are passed to it.
Rushfan - moved more of the string functions into WStringUtils.cpp, they are:
          justify_string, trimstr1, strstr_nocase, single_space, and stptok
Rushfan - Improved the look of the online user editor (F1 Sysop Command)
Rushfan - strip_space and trimstr1 did the same thing, strip_space is now 
          gone, replaced with trimstr1 (which needs a more descriptive name!)
Rushfan - Renamed trimstr1 > StringTrim, trimstr > StringTrimEnd,
          justify_string > StringJustify, strstr_nocase > stristr (removed
          old stristr), stripspace > StringRemoveWhitespace, 
          strrepl > StringReplace, strip > StringRemoveChar,
          stripfn1 > stripfn_inplace
Rushfan - Added #defines for the various spawnopts
Rushfan - Fixed some keys (home, end, etc) that were not working in 
          get_kb_event and therefore not from Input1.
Rushfan - Consolidated get_kb_event1 into get_kb_event.
Rushfan - Fixed cursor staying invisible at the WFC screen when invoking 
          sysop commands (boardedit, chainedit, etc).          
Rushfan - Lowercased more filename parts          
Rushfan - constrain editor name in defaults to keep display nice
Rushfan - Fixed check_for_files_zip and check_for_files_lzh (wwiv wouldn't
          recognize a valid ZIP header)
Rushfan - COMIO support was broken though the Synchronet FOSSIL driver, this
          should be fixed again.
Rushfan - Fixed temp directories not getting cleaned up in remove_from_temp          
Rushfan - Added call to create_chain_file in extern_prog and removed it from
          most other places where it was being called and not used for getting
          the path to chain.txt.
Rushfan - Removed the directory changing in create_drop_file, it's not needed
          anymore now that wwiv uses full pathnames for datadir and gfilesdir
          (plus it was just lame)
Rushfan - When running a true "chain", use the normal internal IO for writing
          text locally/remotely (heart codes aren't processed locally, but 
          they are remotely)
Rushfan - Moved archiver structions into vardec.h so we don't need to have the 
          pack #pragma around it.  Also, missed the .ARC structure, so that 
          bit didn't work before. Moved fedit_data into vardec.h too.  It 
          didn't work before either (again a padding issue)
Rushfan - In the message read prompt, '$' will move to the last message.
Rushfan - renamed InternalOutputString back to bputs, since we need it in
          several places outside of com.cpp
Rushfan - moved get_quote from extrn.cpp to quote.cpp
Rushfan - removed global variable list_option, added an optional parameter
          oo mmkey (bool bListOption)
Rushfan - Changed more filenames to lowercase.  If a file can't be found on 
          UNIX, try making the real file name lowercase.
Rushfan - Changed nam1 and nam to return char* vs. unsigned char*
Rushfan - removed wfcprt, added WLocalIO::LocalXYPrintf
Rushfan - //VER didn't show expired versions. it does now.
Rushfan - Added ClearScreen calls before the WFC commands that were missing it
          and still needed one.


==============================================================================

LABEL: WWIV-5_0_50 (Beta-1)
DATE:  12/16/2003

Rushfan - Build50 was branched to branch WWIV-5_0_BETA_1_BRANCH.  This was
          build 50 which was never released.




==============================================================================

LABEL: WWIV-5_0_49
DATE:  12/04/2003

Rushfan - merged in missing 4.30 code to disable pause while displaying the
          logoff ansi/ascii file.
Rushfan - We now disable the FSED during newuser logon.          

==============================================================================

LABEL: WWIV-5_0_48
DATE:  12/02/2003

Rushfan - removed parameter from wfc_update since 1 wasn't used.
Rushfan - Fixed check_ul_event (had logic broken) and changed it to return a 
		  bool
Rushfan - Removed paths in include statements (finally got around to it)
Rushfan - Added defaulted parameter (bAutoMpl) to input, inputl, and input1
          which will invoke mpl automatically to reduce clutter in the rest
          of the place.
Rushfan - added new method - input_password( password, length )
Rushfan - Added some pauses around the zmodem send code to see if that helps.
Rushfan - More cleaning up #include statements to not include any path info
		  since we should just make the compiler do that for us ;-)
Rushfan - Changed side_menu_colors members from unsigned to int.
Rushfan - Removed an odd if statement in msgbase.c which was always true in
          function OpenMessageArea.
Rushfan - email wasn't passing the force_title parameter to inmsg.
Rushfan - changed cache_start and last_msgnum from unsigned shorts to ints.
Rushfan - Cleaned up many of the /W4 warnings, now down to a dozen or so, 
          mostly in the ZModem code.


==============================================================================

LABEL: WWIV-5_0_47
DATE:  07/02/2003

Rushfan - Fixed extra space in login information before "Available" in the
          sysop chat status.
Rushfan - Changed all static variables into instance variables in WIOSerial
          and WIOTelnet since they really are not static by instance specific.
Rushfan - Added a new argument to printfile (bForcePause) to force the pause on
          screen even for ANSI files.
Rushfan - Commented some more functions.                    
Rushfan - renamed write_automessage to do_automessage.
Rushfan - A couple of fixed which were causing ValScan and scanning a message
          area when THREADED_SUBS was enabled to crash WWIV.         
Rushfan - Removed WWIV_ASSERT in get_post when the parameter is 0 since it is
          actually legal in some places.
Rushfan - Fixed extra space being added after getting quotes (/Q) in inmsg
Rushfan - Fixed bug reportd by Gremlin about userlist not working right when
          sorting by user number and you have gaping holes in the user numbers
          of the users on your bbs.
Rushfan - Updated header of email to match new messagebase headers
Rushfan - Fixed File list with full info and the extended description has
          a high-ascii character.
          
==============================================================================

LABEL: WWIV-5_0_46
DATE:  06/14/2003

Rushfan - Dropped cbuf_t (circbuf.cpp/h) in WIOTelnet in favor of the STL
          queue class (since cbuf_t has more issues than it should)
Rushfan - Dropped cbuf_t in WIOSerial (again replacing it with queue)
Rushfan - Removed IncommingSize and PeekBuffer from WComm          
Rushfan - Fixed bug in WIOTelnet where incomming would return false when the
          next character in the input buffer was a NULL.
Rushfan - Added INI_INIT_TF (works off of YesNoString, i,e, Y/N)
Rushfan - Changed InternalZmodem to use Y/N vs. 1/0.
Rushfan - Changed ExecLogSyncFoss and ExecUseWaitForInputIdle to use Y/N
Rushfan - Added commandline option -k (pacK message areas) to wwiv50.  This 
          will pack all message areas unless you specify the number of the
          areas to pack on the commandline. i.e. "wwiv50 -k 1 5 10" would
          pack areas 1, 5 and 10.
Rushfan - If you add "NEW_SCAN_AT_LOGIN=Y" to WWIV.INI, then when a user logs 
          in they will be asked to scan all message areas for new messages.
Rushfan - stryn2tf now recognizes YesNoString(true)[0], 1, '1', and 'T'
          as valid responses for "YES"
Rushfan - InternalZmodem is now enabled by default.
Rushfan - The display while packing message bases is now a bit nicer looking.

==============================================================================

LABEL: WWIV-5_0_45
DATE:  06/09/2003

Rushfan - Fixed GPF when changing menu sets. (reportd by Eli)
Rushfan - Got rid of the extra space between NN: and PW: after a failed login
Rushfan - Removed some more unused function prototypes from fcns.h
Rushfan - made ssm take variable arguments
Rushfan - Fixed a couple of typos and missing nl()'s
Rushfan - Removed E_C, it's always on now.
Rushfan - Changed the format of the message headers
Rushfan - Fixed user.log only having 1 user listed (reported by Frank)
Rushfan - Using C++ style casts more and more now.
Rushfan - Added missing \r\n's in confedit
Rushfan - Added IsEquals(char*,char*) and IsEqualsIgnoreCase(char*,char*)
          to WStringUtils.
Rushfan - Fixed problem making ini flags not work.          
Rushfan - npr is now bprintf ({both}printf).  Some other name changes will
          be [{local}|{remote}]puts.
Rushfan - outchr is now bputch.  inkey is now bgetch.  outcomch is now rputch.
          restorel is now RestoreCurrentLine (to more closely match
          WLocalIO::SaveCurrentLine -- changed in build 20 in 3/2000)
Rushfan - moved some functions from com.cpp to WStringUtils.cpp          
Rushfan - Fixed assertion in trimstr on a string of all spaces.
FCReid  - Fixed build on Visual Studio 6.0
Rushfan - Fixed reversed logic in subedit when a filename is already in use
          and wwiv asks if you want to use it anyway.
Rushfan - Fixed an INI file glitch where the following commands were off by 1
          BEGINDAYNODENUMBER, INTERNALZMODEM, EXEC_LOGSYNCFOSS  
          EXEC_USECWAITFORIDLE, EXEC_CHILDWAITTIME, EXEC_WAITFORIDLETIME
Rushfan - Added a new Zmodem implementation.  Internal Zmodem now works for
          1 file at a time downloads (nothing else has been tested yet)          
Rushfan - Fixed WIOS_TRACE when TRACE_DEBUGSTRING is enabled.  It was writing
          to the sysop log not to the debugger.
Rushfan - Fixed color of horizontal bar in LastCallers (it wasn't always #7)
Rushfan - Removed some unnecessary sleeps in the zmodem code which made it 
          go from ~1k/second locally to about ~500k/second locally.                    
Rushfan - found some serious synchonization issues in the telnet code, fixed 
          them, but it's new code.  let me know if there's any performance
          issues with the socket code vs. the last release.
Rushfan - Fixed issue with the socket buffer, we were stopping at a NULL and
          adding garbage into the input buffer (which was giving Zmodem
          quite a bit of gas)
Rushfan - Fixed header truncation in new mail prompt.

          
==============================================================================

LABEL: WWIV-5_0_44
DATE:  05/20/2003

Rushfan - More simple code cleanup.
Rushfan - Upgraded to Visual Studio.NET 2003

Rushfan - Added new files (split up existing files).  The new files are 
          xinitini.h which has the INI defines and such from xinit.cpp
		  and input.cpp which now has input* and Input1.
Rushfan - More new style casting and other misc changes.
Rushfan - Added new WWIV.INI parameters for customizing the launching of 
          chains/doors under Win32. (Notational note [ X | Y ] means X or 
          Y, for example "[ 1 | 0 ]" means that the value can be 1 or 0.
    New WWIV.INI Parameters
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	EXEC_LOGSYNCFOSS = [ 1 | 0 ] - If non-zero then wwivsync.log will 
					   be generated.  The default setting is 1 and this
					   is currently ignored.
	EXEC_USECWAITFORIDLE = [ 1 | 0 ] - Under WindowsNT/2K/XP when launching
						the child process WWIV uses WaitForInputIdle to wait
						for the child process to process all normal input 
						before starting the Fossil handling code.  Setting
						this to 0 will disable that (behaving like Win9x where
						we just wait for a bit (See EXEC_CHILDWAITTIME).  The
						default value of this is 1.
	EXEC_CHILDWAITTIME = #### (time to wait in milliseconds, this parameter is 
						only used on Win9X unless EXEC_USEWAITFORIDLE is
						set to 0.  The default value for this is 500 
						(1/2 second)
	EXEC_WAITFORIDLETIME = #### (time to wait in milliseconds, only used on 
						Windows NT/2K/XP unless EXEC_USEWAITFORIDLE=0).  The
						default value for this is 2000 (2 seconds)
Rushfan - Added new file quote.cpp and moved auto_quote, grab_quotes, and           
          the associated helper functions into this module.
Rushfan - added a pile of #defines for dealing with time to make the math easy
          to understand (also make WWIV use them everywhere I could find) 
          The new #defines are:          
			HOURS_PER_DAY, HOURS_PER_DAY_FLOAT, MINUTES_PER_HOUR, 
			MINUTES_PER_HOUR_FLOAT, SECONDS_PER_MINUTE, SECONDS_PER_MINUTE_FLOAT, 
			SECONDS_PER_HOUR_FLOAT, SECONDS_PER_DAY_FLOAT
Rushfan - Replaced numerous hard coded ascii values with #defines (CU, CX, 
          ESC, TAB, RETURN, CP, ...)
Atani   - Reacting to latest win32->linux changes, fixing some warnings.
Rushfan - removed function "pl", everyone should use npr now.
Rushfan - in printfile if the filesize is 0, return false (since it's not 
          really anything in there to display)


==============================================================================

LABEL: WWIV-5_0_43
DATE:  04/28/2003


Rushfan - Split out 2 new files from batch.cpp -- trytoul.cpp and normupld.cpp
Rushfan - Split out new files from listplus.cpp - lpfunc.cpp
Rushfan - save_config() is now app->SaveConfig()
Rushfan - Changed hungarian notation in menu code to conform with wwiv50
Rushfan - Found more pulldown menu stuff to remove.
Rushfan - Move conversions from sh_XXXXX calls to using the WFile class
Rushfan - Changed WriteBuf to take a WFile& vs. int (file handle)
Rushfan - Changed WFile::GetFileTime to open the file with WFile::modeReadOnly
          when it auto-opens the file to get the file date/time.
Rushfan - Removed WWIV_GetFileTime
Rushfan - Modified send_b to take a WFile& vs. file handle
Rushfan - Added WFile *OpenEmailFile( bool bAllowWrite ) to replace open_email
Rushfan - Removed old version of FileAreaSetRecord as it all uses WFile now
Rushfan - Removed open_email(int) and delmail(int, int)
Rushfan - Removed sh_open25
Rushfan - Changed filename global.txt to global-<node number>.txt
Rushfan - Removed sh_open, sh_open1, sh_open_internal, sh_lseek, sh_read, 
          sh_write, sh_close
Rushfan - Changed open_sub to return a bool (succes/fail) vs. file handle.
Rushfan - Using std::min/max vs. our defines now.
Rushfan - Upped the size of all the filename char[]'s I could find to MAX_PATH
Atani   - Fixed compilation issues on linux, fixed "slowness" on linux
          caused by funky permissions being set during WFile::Open.
Rushfan - Fixed last login date always being today (reported by Taz), which 
          was broken in 5.0 build 14.
Rushfan - Added code to wtypes.h to make min/max part of the std namespace
          (like it is SUPPOSED TO BE) under MSVC6
Rushfan - Added function "VerifyStatusDates" into sysopf.cpp which is executed
          during the beginday event which ensures the dates are only 8 chars
          long (since I seem to get corrupted dates on fresh installs often
          enough and that causes big problems for me)
Rushfan - Make onek call onek1 to get rid of duplicate logic.
Rushfan - Some tweaks to allow the serial code to at least init the modem
          built into my dell laptop.
Rushfan - added TRACE_DEBUGSTRING to WIN32/Wios.cpp
Rushfan - More cleaning up of warnings when at warning level 4
Rushfan - introducing c++ strings a little bit here and there.
Rushfan - Added new wwiv::WStringUtils namespace to add some extra 
          functionality when using c++ strings
Rushfan - Added WFile constructor which takes a C++ string
Rushfan - renamed forwardm to ForwardMessage and changed the ret type to bool
Rushfan - renamed isr to InsertSmallRecord and dsr to DeleteSmallRecord
Rushfan - Fixes for MSVC6 compatability from Frank (thx!)
Rushfan - Making sure done, ok, (bool variables) really are bool
Rushfan - Added 2 parameter version of static WFile::remove which takes
          a directory name and a filename
Rushfan - made setconf return a bool vs. int, also made most of the functions
          in confutil.cpp local functions.
Rushfan - made some of the conf code use int vs. usigned long (since it really
          just needs a 32bit value)

==============================================================================

LABEL: WWIV-5_0_42
DATE:  03/01/2003

Rushfan - Fixed problem with WFile and not getting the read permission
Rushfan - Also fixed a couple of GPF's with a fresh install
Rushfan - Fixed write_user opening the user list in read only mode
Rushfan - Fixed issues with status.dat not getting updated properly
Rushfan - Fixed WFC display issue where the # of new sysop msgs was always 0,
          also changed color to red w/o +128 since Win2k/etc doesn't blink.
Rushfan - Changed ClearScreen to CLS to make defaults look better and fit
Rushfan - Removed ansiec from xfer.cpp and replaced with ansic calls
Rushfan - Changed the SETREC macro to the FileAreaSetRecord function which
          is contained in xfer.cpp (vs. the macro which is defined in each 
		  file that it is used)
Rushfan - Cleaned up display in events editor. Also fixed problem with blue
          hilight bar in sub/diredit/etc only being 73 chars wide vs. 79
Rushfan - Merged the menu commands LoadText and LoadTextFile since they both
          did the same thing.  Currently both menu command work, however go
		  ahead and update your menus to use LoadTextFile
Rushfan - Started replacing prt usages with npr calls (more consolidation of
          the output code used in WWIV)
Rushfan - Finished removing all usages of prt
Rushfan - Changed most usages of outstr to npr
Rushfan - Changed almost all "typedef struct {...} X;" declarations to just 
          "struct X {...}" since the typedef is no longer needed for simplified
		  notation in C++. See http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vclang98/html/_pluslang_use_of_typedef_with_class_types.asp
          for more information (if you are curious)  Now things aren't listed
          as "unnamed_X3948328945234" in the class browsers in MSVC 6/7
Rushfan - changed num_lines to g_num_lines
Rushfan - Changed side_menu param "redraw" from int to bool
Rushfan - in listplus.cpp -- Much parameter bool'izing and code cleanup
Rushfan - started to cleanup defaults.cpp
Rushfan - Fixed File NewScan
Rushfan - Attempt to fix a GPF in show_chains when you abort the listing
Rushfan - Added fix in DateTime.cpp to make sure we don't get a 9 
          digit date
Rushfan - Now removed and restores topscreen when using Input1 so the line
          offsets match.
Rushfan - Now guard against going past the screen size in function goxy(x,y)
==============================================================================

LABEL: WWIV-5_0_41
DATE:  01/10/2003

Rushfan - Added BEGINDAYNODENUMBER to wwiv.ini, it defaults to 1.  This must 
          be set to the node number which will run the beginday events.
Rushfan - Also, the -E commandline parameter won't allow the beginday event
          to run multiple times in one day anymore. If you try, you will see
          a nasty message in the sysop log.
Rushfan - Removed the '-W' commandline parameter since it doesn't seem usefull
          and just makes wwiv crash.
Rushfan - WLocalIO::LocalCls will save and restore the default attribute to 7
Rushfan - When exiting WWIV the cursor will be restored and the screen won't
          be yellow
Rushfan - Replaced cursor constants with static const int's off WLocalIO
Rushfan - Cleaned up header files a bit
Rushfan - Removed defines for OK_LEVEL, NOK_LEVEL in bbs.h and moved them into
          WBbsApp as static constants.
Rushfan - Added WFile::modeTruncate
Rushfan - Removed exist(char* filename), replaced all usages with WFile::Exists
Rushfan - Added WFile::ExistsWildcard( char* wildcard ), and removed exists
          (replaced with WFile method)
Rushfan - Added Assertions to WFile::Exists looking for wildcards.
Rushfan - Many more conversions to use WFile for all file I/O
Rushfan - Code cleanup in gfiles and other places
Rushfan - Changed wwiv_version to not include "WWIV" since we all know this
          is WWIV (plus it saves us room on the WFC and other places)
Rushfan - Added exit code to sysoplog and screen when WWIV is exiting.
Rushfan - Put debuglevel into sysinfo structure as nGlobalDebugLevel
Rushfan - Misc cleanup in Platform sections of the code and in the header files.
Rushfan - Changed MSDOS typedef of "bool" to unsigned char to match what MSVC
          actually uses for it.
Rushfan - Reformatted code in netsup.cpp to make it easier to read
Rushfan - Removed usages of "BOOL" outside of the win32 platform code.
Rushfan - Replaces GOTO_XY with the real function "goxy"
Rushfan - Started some code cleanup/reformatting in listplus.cpp, also 
          removed TOBOOL and the BOOL variables there (replaced with "bool")
Atani   - Fixed many compilation issues on linux with gcc 3.2
Rushfan - Globally replaces unlink calls with WFile::Remove
Rushfan - Added WFile::SetFilePermissions( int nPermissions ) and changed all
          usages of _chmod to it. Also removed the UNIX #define for _chmod.
Rushfan - Replaced all calls to rename with WFile::Rename
Rushfan - Removed old version of extract_mod and made the changes in WWIV to
          use the new one which was added in 4.31build2 (somehow this got
          lost in the 4.31b2 merge many moons ago)
Rushfan - Split up some files into smaller logical units.  New source files
          are: confutil.cpp, dirlist.cpp, extract.cpp, and sublist.cpp
Rushfan - Added "time_t WFile::GetFileTime"
Rushfan - More converting to WFile from sh_XXXX calls
Rushfan - Replaced topdata numbers (0-2) with WLocalIO::topdataNone,
          WLocalIO::topdataSystem, and WLocalIO::topdataUser constants.
Rushfan - Fixed problem where tleft would write one too many topscreen lines
Rushfan - The title of the console now displays the username who is online.
Rushfan - Replaced the "PRINTER" option in wwiv.ini with "TITLEBAR", if this
          is 'Y' then the username will be displayed in the console windows
          titlebar (on Win32 only)
Rushfan - Cleaned up this readme so it fits in 80 columns.          
Rushfan - GuestCheck now returns bool (true = ok, false = not ok)
Rushfan - Merged some functions from menusupp.cpp with the real ones if all 
          the stub in menusupp did was to call the real one.
Rushfan - Added hop.cpp, valscan.cpp, inmsg.cpp, and msgscan.cpp
Atani   - Fixed new platform/linux/WFile.cpp changes for GetFileTime method
Atani   - Cleanup/reorder building of objects


==============================================================================

LABEL: WWIV-5_0_40
DATE:  01/02/2003

Rushfan - Created admin dir with some of the .MSG, ANS, .MNU, etc..., files 
          that are part of the stock wwiv50 install.
Rushfan - removed unused platform/$PLATFORM/datetime.cpp
Rushfan - Updates to the MSVC6 workspace + fixes in exec.cpp for MSVC6
Rushfan - Made most constants in version.cpp const.
Rushfan - Added const long lWWIVVersionNumbe to version.cpp
Rushfan - Updated Copyright Statement to include 2003
Rushfan - Added control-U for who's online hotkey.  Also changed 
          HandleControlKey to use the Control code macros vs. numbers
Rushfan - Fixed ivotes from not showing the 1st question at the WFC          
Rushfan - Fixed laston edge case where the header didn't match the data when
          EXTENDED_USERINFO = N but SHOW_CITY_ST = Y.
Rushfan - Mods in exec.cpp to combine the 2 log files into one.

==============================================================================

LABEL: WWIV-5_0_39
DATE:  12/15/2002

Added "Intercept DOS Interrupts" back to chainedit and enabled COMIO in the
wwiv.ini SPAWNOPT's again.  We're using SyncFOSS for Dos interrupts (just like
we do for FOSSIL support) -- Thanks Rob from Synchronet!!! 
One thing to note: WWIVEDIT doesn't do local ANSI support, so it looks bad 
locally.

Now we zero the ansiptr when makeansi is called with an invalid ansi sequence
so that the ansistr can't be overrun causing memory corruption.  We also 
check the ansiptr on bputch to ensure no overruns are going to occur (as is
the case when you have a text file with an escape char (27) and then a lot
of garbage after it (or a corrupt mail packet as in my case).  Anyway, be on 
the lookout for local ANSI interpretation not working as well after this 
change, it seems to work fine on my machine in all the tests I've tried.

=======
Atani - Cleanup of WZmodem, WProtocol to allow them to compile on linux.
Also removed more hard-coded names to filenames.h (lowercased as well)

Rushfan - Changed Win9x loop to not check exit code till 500 loops after no
data. This should improve the performance of doors under Win9x slightly.

Removed the function huge_xfer since it's really not needed in 32-bits
Removed W_Proper function (not used)

Rushfan - Fixed but in WFile asserting on Open.
Rushfan - 1st real use of WFile class in xinit.cpp
Rushfan - Added new spawnopt flag in WWIV.INI (NETWORK is the key name) so
          that Eli can add "FOSSIL" to that ;-)
          
Rushfan - Made WFile::IsOpen public
          Added WFile::GetFullPathName
          
Rushfan - Rewrote stripcolors to be about 1/3 of it's original size (and it
          even works right now)         

Atani - various cleanups and reactions to WFile changes.

Rushfan - Replaces hard coded numbers used to identify internal protocols 
          with #defines

          Added WProtocol/WZmodem to the list of files built.
          
          WFile::GetLength will now open the file if its not already open.
          
Frank   - Fix in chains.cpp when CHAINS_REG is not set.

Rushfan - try to fix CR/LF bug in input1.
        - Misc cleanup in com.cpp        

==============================================================================

LABEL: WWIV-5_0_38
DATE:  11/02/2002

we now unalign filenames in the transfer area before checking their existance
so that //UPLOAD works on filenames with the base < 8 characters.

made hack in listplus.cpp to unalign every name before checking if it exists
( needs to be reworked with the rest of the xfer code... <sigh> )

added sysoplogf( bool indent, char*, ... ) and sysoplogf( char*, ... ) which
are printf style versions of the sysoplog function so that there are
fewer places in the source where temp variables are made just for a sprintf
and then a sysoplog call.

changed extern_prot and maybe_upload to use a bool for a boolean vs. an int.

code cleanup in utility.cpp

tweaked the inputting directory path in diredit to be nicer

Changes from Atani to make it build under GCC 3.2 on Linux.

replaced cd_to and get_dir with the WWIV functions

Fixed nrecno which was starting at record 2 instead of 1 (bugger!)

Fixed not stripping spaces before a move file operation.

removed unused i1 variable in uploadall and also removed unneeded nl(2) call
from there, also changed sig of upload to return a bool vs. int, and false 
means that it was aborted (which actually works now)

Changed the who's online list to be a list, also removed the WFC hack there
and made it a 3rd parameter to the function

Atani - Various fixes to warnings on compiling under GCC 3.2 on Linux.
Atani - initlite updates to allow further config.dat settings, #1 SL/DSL twiddle.

Rushfan - Tweaks to the SyncFoss support

Atani - Moved more hard-coded filenames to filenames.h
Atani - Added platform/linux/WFile.cpp
Atani - Added nodemgr for linux, Makefile was modifed to have a "PLATFORM_TARGETS"
option which is for targets that should only run on certain platforms.

Rushfan - Fixed voting booth.

Rushfan - More tweaks to the SyncFOSS support

==============================================================================

LABEL: WWIV-5_0_37
DATE:  09/29/2002

Added new parameter to read_user (bForceRead) so that you don't have to jump
though hoops setting the WFCStatus and stuff.

Removed a pile of code not used anymore in extrn1.cpp (some of it wouldn't 
even work if it were used such as the strtok nonsense

removed in_extern variable.

Changed last parameter of write_qscn and read_qscn from int to bool.

Fixed "random_screen" in lilo since it accidentally got .0 replaced with \0030

Fixed the sublist from not displaying the horizontal high-bit ascii bars

changed most \003's to |# to use the simpler form

changed Input1 to return an int vs. unsigned char (noone uses the retval 
anyway, and the cast there is just not needed)

Fixed //CHAT from GPF'ing. (it actually toggles the scroll lock on Win32 too)

Fixed /C from GPF'ing (no promises it works yet, but it does on my machine)

Fixed possible GPF in beginday since it's possible for it to execute an 
event if beginday_c[1] is not NULL, which is true in debug builds, it's
some weird number.

Code cleanup and reformatting in chat.cpp and ini.cpp, getting ready for C++
overhaul of the ini code.

Added default parameters to param 2 and 3 from ini_get

Changed %%TODO: comments to TODO so MSVC.NET sees them right.

Code Cleanup in syschat.cpp

Changed 3rd parameter of inli from "int" to "bool"

Code cleanup in defaults.cpp and mulinst.cpp

Started code cleanup in transfer area code

made recno(s) just call nrecno(s, 1)

removed nete, added app->GetNetworkExtension()

renamed dlfn to g_szDownloadFileName, and edlfn to g_szExtDescrFileName

started cleanup in sr.cpp and srrcv.cpp

changed osan, plan, pla, pla1, printtitle, printinfo, nscandir, print_extended,
read_message, and read_message1 to use bool's for abort and next instead of
ints.

changed had (HangupAfterDownload) from int to bool in batch.cpp globally

code cleanup in attach.cpp and automsg.cpp

removed global force_title, added new optional parameter to inmsg and email.

refactored bbslist.cpp into smaller chunks.

started code cleanup in uedit.cpp

moved statusfile in status.cpp into class

started refactorign in newuser.cpp

removed ClearScreen function, and renamed CLS to ClearScreen

codecleanup in menu.cpp

Updated wwiv50 info in readme.txt, also added URL for Mingw

removed numerous calls to ansic... replaced with pipe codes.

replaced usages of strcmpi with stricmp ( so we only had 1 fn doing this )

defaulted parameter in "nscan" to 0

changed osan and plan to use buffered version of bputch.

modified read_message1 to put more data in each packet (like 1 line vs 1 word)

Now use CreateProcess to execute a command vs. spawnvpe

Moved GetLastErrorText() into WComm from Wios (since I needed it in Wiot)

Changed thread signaling to use Events instead of being lame and using a bool.

renamed ansir_no_300 to ansir_emulate_fossil since 300 baud is so not an
issue anymore.

Moved function ExecExternalProgram into an platform package to get rid of most of
the platform defines in extrn1.cpp

Added new flag in ChainEdit (Emulate FOSSIL), enable this, and if you have
the DOSXTRN.EXE, SBBSEXEC.DLL, and SBBSEXEC.VXD in your wwiv home directory,
then DOS doors will work, just make sure your comport number is set to 1.
(yes, Eli, this works on Win9x and WinNT/2k/XP, at least on my Win98 machine
it seems to work as well as Win98 ever does)

FOSSIL can now be specified in WWIV.INI as a valid flag for spawn options

Added new parameter to write in WComm to spefify NoTranslation (defaults to 
false) when broadcasting IAC chars over telnet.

In WIOTelnet::write, it now excapes IAC chars so that FDSZ will work with the
emulated fossil. (Just add FOSSIL for SPAWNOPT[PROT_SINGLE] and 
SPAWNOPT[PROT_BATCH].

Started stuffing the envionment entries we need into the processes environ
since passing xenviron wasn't working with the emulated fossil.  Maybe
xenviron should just go away.  Doing this, downloads now work as expected 
usign fdsz.


==============================================================================

LABEL: WWIV-5_0_36
DATE:  09/08/2002

CHANGES:

Moved non-class functions in wiot.cpp into the class as static member 
functions along with moving variables into class as statics as well.

Fixed bug with ctypes looping.

Fixed bug with displaying subs when you had exactly 17 of them.

Added quasi-buffered writing to the IO code, it should be a tad bit
faster, especially for telnet connections.

Fixed problem with XXX's appearing in newuser password prompt on non-ansi 
connections

fixed problem with some telnet clients hanging after the 1st CR/LF pair
was entered.

Synced the version of the telnet server with the bbs's version number

Moved the global variable instance into the WBbsApp class

Moved init() into the WBbsApp class

removed two_color global variable, made it a defaulted parameter to inli

moved the global variabl elastcon into the WSession class

Added function const char* YesNoString(bool) to return the Yes or No String

made sure all malloc/free went through bbsmalloc/BbsFreeMemory to facilitate dropping
in a debugging malloc

made a couple of functions return const char* vs char* where possible.

Replaced nln function with overloaded version of nl which can optionally
take an argument

Code cleanup in WLocalIO class.  

moved initport into modem.cpp and renamed it to InitializeComPort

replaced set_baud with the call to app->comm->setup

removed setfgc and setgbc, inlined the methods into execute_ansi

Added new pipe codes:
~~~~~~~~~~~~~~~~~~~~~
|#<wwiv color code 0-9> as an alternative to the "heart" codes

|@<macro character> as an alternative to ^O^O<macro char>

Just FYI: Existing pipe codes are |B<background char>, and |<2 digit code>
for foreground colors.


==============================================================================

LABEL: WWIV-5_0_35
DATE:  08/18/2002

CHANGES:

Added [R]estore default colors as an option in Defaults under the color
configuration.

Changed defaults to be 2 columns instead of one, also changed the colors
of it so it looks nicer and added a header line.

Changed colors used in Your Info, also added a header

Fixed problem where print_local_file wasnt' abortable even when it was told
that it should be.

Cosmetic improvements in the voting booth code.

Changed the return type of more functions from int to bool when they were
really returning a boolean type value.

The included telnet server will now minimize to a system tray icon.

Refactored the getuser function in lilo.cpp -- It now has numerous helper
functions which do most of the work, and getuser is just left to control
the flow.  This makes the code ALOT easier to maintain and spot problems.

Started refactoring the rest of the code in lilo.cpp since before it was only
a couple of really long nasty looking functions.

Removed function "pln" since npr should really be used instead.

Merged alot of code from multiple outstr calls into 1 call to npr

Merged the two places which calculate how many lines a message can be into
one function "void GetMaxMessageLinesAllowed()"

Added parameter "%E" to stuff_in which can be used in doors as the path to the
door32.sys file.

Some work on the WIOTelnet class.  The read thread is now started/stopped 
whereas before it was not.  There was a problem with it running during a
DOOR32.SYS door.  Also, close doesn't close the socket when bIsTemporary
is true (new parameter WComm::close takes).  In short, now PimpWars and
other door32.sys doors should work under the native telnet handling.

Fixed the problem where WWIV would not handle an IAC telnet command and
treat the command as text to process.... We still don't handle the command
properly, but at least we know it is a command. (hey, it's a start)

Now lists the replacable parameters when editing the command in ChainEdit. 
Also, we use the Input1 routines so you don't have to always retype 
everything.

==============================================================================

LABEL: WWIV-5_0_34
DATE:  08/04/2002

CHANGES:
Tons since the last update, check CVS sources for full list. (I know,
I should be better about writing these dcwn... but I get sidetracked 
easily... maybe I just have ADHD... Hmm... What was I talking about again?)

Better OS detection (again).  Detects XP properly now.
Building with Microsoft Visual C++.NET now, so this has allowed me to find
numerous stack corruptions and fix them.

Found issue where the WFC status was not set to zero when a caller was
passed in from the telnet server, therefore the load/save user code
was trashing the sysop's user record (yuck)

Source code cleanup particularly in the message base areas.

Changed several functions to use bool vs. int/BOOL for return codes.

More using of bool internally for control flags, instead of using
int values ( as booleans )

Removed a few unused global variables ( I wish they would all get removed! ),
also added parameters to functions to get rid of globals.

==============================================================================

LABEL: WWIV-5_0_33
DATE:  06/01/2002

CHANGES:
Tons since the last update, check CVS sources for full list.

Improved OS detection
/A, /H, /? Added to internal message editor
many tweaks to prompts
Screen size set on launch, and reset on exit.
blah blah blah


LABEL:  WWIV-5_0_25
DATE:   04/21/2000

CHANGES:

* Many changes for Linux.  It now compiles on Linux with GCC

* New FindFirst/FindNext class

Changed all instances of findfirst()/findnext() to use WFindFile class
Changed all instances of WIN32 specific _findfirst()/_findnext() to use 
WFindFile class removed functions findfirst/findnext

WFileFile usage:
~~~~~~~~~~~~~~~~
WFindFile fnd;
BOOL bFound = fnd.open(<filespec>, 0)
while (bFound)
{
	char *name = fnd.GetFileName();
	long size = fnd.GetFileSize();
	BOOL isFile = fnd.IsFile()
	BOOL isDir = fnd.IsDirectory();
	bFound = fnd.next();
}

Optionally you can call fnd.close() to free resources allocated by the object,
however the resources are also freed in the destructor for the class.  (when 
it goes out of scope if it's on the stack, or when delete is called if it was
created with new)


* Moved Wios, Wiot, dosemu to platform/WIN32 since they are not generic source.

* ADD int WLocalIO::LocalGetChar() - same as getch() or getchar()

* Merged all the different functions to copy a file

	1) Changed copy_file(x, y) to copyfile(x, y, FALSE)
	2) Removed copyfile2
	3) Changed all calls to copyfile2 to use copyfile
	4) Modified copyfile to invoke WWIV_CopyFile to do the actual copying

* Started working on file path issues for Linux compatability.

	1) define WWIV_FILE_SEPERATOR_CHAR and WWIV_FILE_SEPERATOR_STRING as 
	   the proper file seperator for the platforms

	2) changed \\ to WWIV_FILE_SEPERATOR_CHAR

	3) Made WWIV_ChangeDirTo, and WWIV_GetDir just simply call into platform 
	   routines to perform these functions.

* Other general code cleanup for Linux and WIN32
	- Started removing those little annoying warnings because 
  	  the variable and the formatting placeholder didn't match.
	- Moved some functions out of wfc.cpp because they really
  	  shouldn't be in there.
	- Changed a putenv() to a setenv() because putenv() doesn't 
  	  work sometimes.
	- Changed setftime to char * instead of int because it works 
  	  on Linux that way.
	- Moved function show_files from utility to platform/$PLATFORM/utility2.cpp


* Moved more code into the WLocalIO class

	1) Changed WWIV_MakeLocalWindow(...) to WLocalIO::MakeLocalWindow(...)
	2) Changed _setcursortype(UINT) to WLocalIO::SetCursor(UINT)




==============================================================================

LABEL:  WWIV-5_0_24 
DATE:   04/04/2000

CHANGES:

* Makefile is now the Linux makefile.  makefile-gcc.w32 is the Win32 GCC 
  makefile

* More of the WWIV code compiles on Linux now

* platform/incl1.h and platform/incl2.h have been moved from wwiv.h

* all Linux files are now lower or proper case (not all upper case)

* Many makefile changes for Linux

* More code is making it's way into the platform directories



==============================================================================

LABEL:  WWIV_5.0_23 (Includes changes in 21 and 22)
DATE:   03/19/2000

CHANGES:

* Changed WComm::setup()'s return type to BOOL

* Added hCommHandle to system_operation_rec

* Removed function call to ReadIniInfo from the Menu Commands

* Changed initport() to return TRUE on success (not 0)

* Got rough version of Serial IO working (Win95/Win98)

* Made WWIV not open and close the comport on check_comport

* Got rough version of Serial IO working (WinNT/Win2000)

* Implemented function WIOSerial::carrer() finally

* Removed functions from WComm classes [ set(), brk(), status() ] - unused

* Removed fossil_set(), fossil_brk(), fossil_status() - unused

* Moved function wfcprt() to sysopf.cpp  (with the rest of the WFC stuff)

* Fixed Serial input thread not exiting under WinNT/Win2000

* Fixed display of chains list.

* put #ifdef _WIN32 code around telnet server code in bbs.cpp

* Modified the header files to compile under GCC/EMX v0.9d on OS/2.




==============================================================================

LABEL:  WWIV_5.0_20
DATE:   03/12/2000

CHANGES:

* Moved makewindow into platform/$PLATFORM & renamed it WWIV_MakeLocalWindow

* Moved code to copy a file out of a function in attach.cpp into a new
	file platform/$PLATFORM/filesupp.cpp as WWIV_CopyFile(char*, char*)
	(There are still 2 other functions that copy a file that need
	to be converted into calling this one)

* Moved most (hopefully all) of the Local IO code into a C++ class
	WLocalIO.  The header file is platform/WLocalIO.h, and the source
	file is platform/$PLATFORM/WLocalIO.h.  Currently only a WIN32
	and partial OS2 implementation exist.  The Linux implementation
	needs to be written.

* Removed "Control-O" processing in skey, since it was only falling through
	to the "Control-T" handler.

* Changed savel() into WLocalIO::SaveCurrentLine()

* Updated Borland C++ Builder 5.0 Project and Borland C++ 5.5/Builder 
	makefile to include the new files added.



==============================================================================

LABEL:  WWIV_5.0_19
DATE:   03/10/2000

CHANGES:

* Moved OS testing / #define verification code into platform/testos.h

* Moved the platform code from wwiv.h into platform/incl1.h and platform/incl2.h
	platform/incl1.h - This is for platform code to be included before the
	common set of standard C runtime libraries.
	platform/incl2.h - This is for platform code to be included before 
	WWIV specific header files

* Created new file - platform/platformfcns.h 
	If you add a new routine into the platform code area, please be sure
	that you add the prototype into platform/platformfcns.h NOT fcns.h.  Also
	notate the build note so that the other platform groups can make
	their own platform version of that function.





==============================================================================

LABEL:  WWIV_5.0_18
DATE:   03/06/2000

CHANGES:
* New files added (split up larger files into smaller ones)
    asv.cpp
    attach.cpp
    automsg.cpp
    bbslist.cpp
    chains.cpp
    colors.cpp
    datetime.cpp
    dupphone.cpp
    inetmsg.cpp
    memory.cpp
    shortmsg.cpp
    status.cpp
    sysoplog.cpp
    user.cpp
    vote.cpp
    wqscn.cpp


* Renamed clrscrb() to LocalCls()

* Fixed F1 (UserEdit) from not looking right if the TopScreen is on

* Sorted FCNS.H to list the files in alphabetical order

* Removed function gotoxy() (it was a duplicate of movecsr)

* Renamed movecsr() to LocalGotoXY()

* Renamed kbhitb() to LocalKeyPressed()

* Updated Borland C++ Builder Project file with new files

* Removed MAKEFILE.BCC - Now you should use WWIV50b.mak (b = Borland)
    To change the output path, edit the file and look for "PROJECT"
    and change the location of the EXE that will be generated

* Removed build.bat - Wasn't used

* The makefile for GCC 2.95.2 - Mingw32 is in the root directory, and is
    called makefile-win32.gcc.  It is no longer in the platform/WIN32 directory
    (the whole make file thing needs to be reorganized quite a bit)




==============================================================================

        
WWIV 5.0 Build 16

NOTE: Build 15 was never distributed as the changes weren't significant


Major changes since Build 14
============================

Removed pull down menu code (This was all of build 15)

Fixed warning in Release Build related to #pragma component(Browser, on)

Fixed remaining warnings under MSVC 6.0 (BC++ Builder 5.0 still reports a few
warnings, and most of them are flat-wrong)

Fixed auto network callouts from not happening while running WWIV50 -m

renamed out1chx -> LocalPutchRaw
renamed out1ch  -> LocalPutch
renamed lf -> LocalLf
renamed bs -> LocalBs
renamed cr -> LocalCr
renamed outfast -> LocalFastPuts
renamed outs -> LocalPuts

Changed most code that called outs to use the fast version 
(minor speed increase)

Changed function makewindow() to use the fast outs routine under __OS2__

Wrote WIN32 optimized version of makewindow() that write to a buffer then 
writes the entire buffer to screen with 1 call (twice as fast under NT/2000, 
10 times faster under 95/98)

Modifed the wfc_screen() function to only load WFC.DAT one time from disk, 
and cache it from there on out.  (The was causing HD access every time the WFC
screen had to redraw itself)  And since we have the memory (whereas DOS apps 
don't) , it's better to do this.



==============================================================================


WWIV 5.0 Build 14

Major changes since Build 13a
==============================


Changed "thisuser" to "sess->thisuser" (moved to WSession class)
Removed code for writing STAT.WWV and RESTORE.WWV
Removed extern_prog flag "EFLAG_SHRINK" and "EFLAG_FILES"
Removed defines for sysconfig_shrink_term and ansir_shrink
Removed global variables
	xdate (moved to only function using it)
	cur_lang_idx (set but never used)
	cursor_move
	daysmin
	daysmax
	endday (always = to 0, even in 4.30)
	pend_num (always = to 0, even in 4.30)
	tempio
	commport (used only in fossil.cpp)
	curloc
	ver_no2 (used for WWIV_FP environment which isn't with us anymore)
	
Moved ooneuser & event_only to bbs.cpp
Moved inst_num to sysopf.cpp
Moved screenlen to only function in comio.cpp that used it
Moved xtime to function "holdphone"
Moved sp to uedit.cpp as file level static

Removed GetHostID() in bbs.cpp.  Now the internal telnet listener binds to any
local address (otherwise, it wouldn't work right on non-networked machines)

Fixed the topscreen from scrolling away
Fixed topscreen display (now shows proper flags (comm disabled, etc.))
moved default_ctyles to bbsutl2.cpp
made translate_letters, valid_letters, and invalid_chars const.
moved valid_letters to com.cpp
removed unused function SetNet_Name()

optimized WFC drawing code to write to a buffer then blit to to the screen 
(now it's about 5-10 times faster at drawing the WFC)

removed unused global variable "abortext"
moved variable "search_pattern" to uedit.cpp as file level static
Removed variable "force_newuser".. It was used but never set (even in 4.30)
Removed unused structure "resultrec"
Fixed problem with xenviron[] not being zeroed to NULL before use.
Disabled part of the EFLAG_NETPROG processing in extern_prog since it was 
hosing any attempts at running the network software

Added "BOOL bUsingPppProject = TRUE" to bbs.cpp.  If this is set, you can do 
a wwivnet callout even if ok_modem_stuf == FALSE. 

added back in a #pragma (pack, 1) to net.h so WWIVnet packets are the correct 
size again

fixed remaining structures in net.h to be the right size under WIN32
Fixed network pending list under WIN32
Fixed arrow keys not working in function ansicallout() under WIN32

(Now you can use the PPP project to do callouts from WWIV 5.0)





==============================================================================


WWIV 5.0 Build 13a Source.


Major changes since Build 12
============================

1)  If you do not specify an instance number, 1 is assumed instead of 0
2)  List of command line parameters is now sorted
3)  SystemInfo() now calls WWIVVersion() for version information 
    (removed redundant code)
4)  At logon time, the real OS detection code is used, (vs, #ifdef's)
5)  At logon time, "Multitasker" is now just "OS"
6)  Removed some older code guarded with #ifdef OLD and #ifdef __NEVER__
7)  It now builds with GCC/Mingw32 again
8)  It now builds and runs with Borland C++ 5.5 Free Command Line Tools. 
    (see makefile.bcc)
9)  Changed WApplication to WBbsApp (Main Application)9) 
10) Started adding C++ objects
	WSession	- Current user session information
	WComm		- Base class for serial/socket communications
	WIos		- Win32 Serial IO (not implemented yet)
	WIot		- Win32 Telnet/Socket IO

	To access a low-level comm routine, instead of "fossil_write()", you 
	should call app->comm->write().

11) Fixed ListPlus from not working (structures in listplus.h were wrong size)
12) more source code reformatting/cleanup.






==============================================================================



WWIV 5.0 Build 12 


Major changes include:
======================
1) native Telnet support.  (however it won't build under GCC anymore since 
they don't include Winsock2 headers, etc...).. Right now you need MSVC to 
build it.  (I'll get it working under Borland's Free C++ 5.5 compiler soon)

2) Now includes it's own quasi-function telnet server (run WWIV50 -TELSRV)...
After one connection, it exits to DOS.  (But it works)

3) It now can run under EleBBS's telsrv.exe (Their telnet server process)...
Just rename this to elebbs.exe, and copy over ELEONFIG.EXE and TELSRV.EXE 
from an EleBBS distribution.  Then modify it in ELCONFIG to start with Node #1.

4) You no longer have to set WWIV_INSTANCE to do multi-node (just passing the
node number on the command line is enough).. WWIV will set the environment 
variable by itself now (had to make this change to get it to work with telnet 
servers)

5) WWIV Networking works (fixed a bug where it was generating corrupt 
outgoing packets)

6) BBS.CPP now contains a C++ class (minor code changes, however I wanted to 
start adding the C++ classes into the framework)

7) Command line parameter changes (to match what we get from EleBBS's 
telsrv).. -N is now -Q, -I is not -N (but -I still works), -TELSRV tells 
WWIV to use internal telnet server (not needed if running under EleBBS's 
telnet server).. However with this option, you can only run single node, 
whereas you can run multi-node under EleBBS's server

8) All sorts of other random fixes/tweaks to make things work...




==============================================================================

WWIV 5.0 Build 6
================

* Fixed several ANSI errors (i.e. external foo, without the type, etc.)

* Changed some functions parameters to "const char*" if they really are 
(warnings under GCC)

* Added some GCC specific code in port.h to enable compiles under 
GCC 2.95 on WIN32

==============================================================================


01-06-2000 Build 3
==================
* Made compiles 10 times faster with VC's pre-compiled headers.  Also 
  reorganized header file setup so that the only header file to include per 
  .C/.CPP file is "wwiv.h".  Precompiled headers are set to work through wwiv.h

* Sped up compile time by defining WIN32_LEAN_AND_MEAN

* Added 1st OS/2 conditional code segment to port.c

* removed #pragma hdrstop code (since MSVC doesn't use it)

* changed all header file guards to "__INCLUDED_XXXXX_H__" and test define 
  before #include statement (this is many times faster than making the compiler 
  parse the entire header file looking for
  the #endif at the end)

* Removed #include statements from most of the header file since they are 
  only included from "wwiv.h"

==============================================================================


01-05-2000 Build 2
==================
Fixed save and restore screen.  They use the right screen sizes now.



==============================================================================

01-04-2000 Build 2
==================

* Fixed INI files not working right, apparently, WIN32 wasn't working w/o 
  full pathnames to files, and the INI files were relative.

* Fixed bug where "char* ctypes(int num)" was freeing the data before 
  returning it and not working.

* Fixed WFC.  Added "void DisplayWFCScreen(char *pszScreenBuffer)" to sysopf.c

* Fixed Null-Pointer Exceptions where 0 was being passed to inmsg from 
  newuser.c where a pointer is being used. (bad!!)



==============================================================================


01-04-2000 Build 1
==================

Initial Revision, Created initial port




==============================================================================

[End of revision history]

