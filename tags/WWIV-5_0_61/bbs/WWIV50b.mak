##############################################################################
#
# WWIV 5.0 MAKEFILE for Borland C++ 5.5 and Borland C++ Builder 5.0
# Copyright (c) 2000-2006 WWIV Software Services
# All Rights Reserved.
#
# Written by Rushfan
#
#



##############################################################################
#
# This part of the makefile should not need to be changed unless files
# are added or removed.  All the customization settings are above.
#



OS=WIN32
PLATFORM_CFLAGS=-tWC -tWM
PLATFORM_DIRS=.\platform\WIN32
PATH_SEPERATOR=;
EXE_EXT         = .exe
OBJ_EXT         = .obj


CC              = bcc32

OBJ		= obj
BIN		= bin
#
# Note to user, set this to be the LIB dir in BCC5.5
#
LIB		= D:\Program Files\Borland\CBuilder5\Lib
LIB_PSDK	= $(LIB)\PSDK
LIBPATH		= "$(LIB);$(LIB_PSDK)"

BIN_NAME        = $(BIN)\wwiv$(EXE_EXT)
RM		= rm
MKDIR		= mkdir
CFLAGS		= $(PLATFORM_CFLAGS)
LINKFLAGS	= -lm -L$(LIBPATH)
PROJECT_DIR	= wwiv50
TAR		= tar czvf 

.path.cpp	= .;.\platform;$(PLATFORM_DIRS)
.path.obj	= .;$(OBJ)
.path.exe	= .;$(BIN)

##############################################################################
#
# Implicit Rules
#
#
#

.cpp.obj:
	$(CC) $(CFLAGS) -c -n$(OBJ) $<

#############################################################################
#
# Target "ALL" Rebuilds WWIV
#

COMMON_BBS_OBJS = 		       	\
        $(OBJ)\batch.obj         	\
        $(OBJ)\bbs.obj           	\
        $(OBJ)\bbsovl1.obj       	\
        $(OBJ)\bbsovl2.obj       	\
        $(OBJ)\bbsovl3.obj       	\
        $(OBJ)\bbsutl.obj        	\
        $(OBJ)\bbsutl1.obj       	\
        $(OBJ)\bbsutl2.obj       	\
        $(OBJ)\callback.obj      	\
        $(OBJ)\chat.obj          	\
        $(OBJ)\chnedit.obj       	\
        $(OBJ)\com.obj           	\
        $(OBJ)\conf.obj          	\
        $(OBJ)\connect1.obj      	\
        $(OBJ)\crc.obj           	\
        $(OBJ)\defaults.obj      	\
        $(OBJ)\diredit.obj       	\
        $(OBJ)\events.obj        	\
        $(OBJ)\tcpip.obj         	\
        $(OBJ)\gfiles.obj        	\
        $(OBJ)\gfledit.obj       	\
        $(OBJ)\ini.obj           	\
        $(OBJ)\instmsg.obj       	\
        $(OBJ)\lilo.obj          	\
        $(OBJ)\listplus.obj      	\
        $(OBJ)\misccmd.obj       	\
        $(OBJ)\modem.obj         	\
        $(OBJ)\msgbase.obj       	\
        $(OBJ)\msgbase1.obj      	\
        $(OBJ)\multinst.obj      	\
        $(OBJ)\multmail.obj      	\
        $(OBJ)\netsup.obj        	\
        $(OBJ)\newuser.obj       	\
        $(OBJ)\readmail.obj      	\
        $(OBJ)\sr.obj            	\
        $(OBJ)\srrcv.obj         	\
        $(OBJ)\srsend.obj        	\
        $(OBJ)\strings.obj       	\
        $(OBJ)\subacc.obj        	\
        $(OBJ)\subedit.obj       	\
        $(OBJ)\subreq.obj        	\
        $(OBJ)\subxtr.obj        	\
        $(OBJ)\sysopf.obj        	\
        $(OBJ)\uedit.obj         	\
        $(OBJ)\version.obj       	\
        $(OBJ)\voteedit.obj      	\
        $(OBJ)\xfer.obj          	\
        $(OBJ)\xferovl.obj       	\
        $(OBJ)\xferovl1.obj      	\
        $(OBJ)\xfertmp.obj       	\
        $(OBJ)\xinit.obj         	\
	$(OBJ)\circbuf.obj       	\
	$(OBJ)\wutil.obj         	\
	$(OBJ)\automsg.obj	\
	$(OBJ)\syschat.obj	\
	$(OBJ)\vote.obj		\
	$(OBJ)\dupphone.obj	\
	$(OBJ)\chains.obj		\
	$(OBJ)\sysoplog.obj	\
	$(OBJ)\status.obj		\
	$(OBJ)\user.obj		\
	$(OBJ)\asv.obj		\
	$(OBJ)\wqscn.obj		\
	$(OBJ)\shortmsg.obj	\
	$(OBJ)\colors.obj		\
	$(OBJ)\attach.obj		\
	$(OBJ)\inetmsg.obj	\
	$(OBJ)\memory.obj		\
	$(OBJ)\bbslist.obj	\
	$(OBJ)\datetime.obj	\
	$(OBJ)\wfndfile.obj	\
	$(OBJ)\utility2.obj	\
        $(OBJ)\utility.obj       	\
	$(OBJ)\WLocalIO.obj	\
	$(OBJ)\reboot.obj		\
	$(OBJ)\wshare.obj		\
	$(OBJ)\WComm.obj		\
	$(OBJ)\wfc.obj		\
        $(OBJ)\menuedit.obj      	\
        $(OBJ)\menuspec.obj      	\
        $(OBJ)\menusupp.obj      	\
        $(OBJ)\menu.obj          



BBS_OBJS 	= $(COMMON_BBS_OBJS) $(OBJ)\Wios.obj 	\
		  $(OBJ)\Wiot.obj $(OBJ)\filesupp.obj

WIN32LIBS = advapi32.lib ws2_32.lib user32.lib kernel32.lib cw32mt.lib import32.lib

###############################################################################
#
# Makefile Targets
#
#
#


all: $(OBJ) $(BIN) $(BIN_NAME) $(BBS_OBJS)



$(OBJ):
	-$(MKDIR) $(OBJ)


$(BIN):
	-$(MKDIR) $(BIN)


$(BIN_NAME): $(BIN) $(BBS_OBJS)
	$(CC) -e$(BIN_NAME) $(LINKFLAGS) $(BBS_OBJS) $(WIN32LIBS) 

clean:
	-$(RM) -f $(BIN_NAME)
	-$(RM) -f $(BBS_OBJS)
	-$(RM) -f make.log

spotless:
	-$(RM) -rf $(BIN)\
	-$(RM) -rf $(OBJ)\


# Included for convension
distclean: spotless

.PHONY: clean clobberall 


