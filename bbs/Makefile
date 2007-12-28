##############################################################################
#
# WWIV 5.0 MAKEFILE for GCC
# Copyright (c) 2000 WWIV Software Services
# All Rights Reserved.
#
# Written by Rushfan
#
# Modified for Linux by Ben "Knight Walker" Carner
#

##############################################################################
#
# This part of the makefile should not need to be changed unless files
# are added or removed.  All the customization settings are above.
#

ifneq (,$(findstring win32,$(MAKEFLAGS)))
OS=WIN32
PLATFORM_CFLAGS=
PLATFORM_LINKFLAGS=
PLATFORM_DIR=platform/win32
PLATFORM_ALT_DIR=platform/win32
PATH_SEPERATOR=;
EXE_EXT         = .exe
OBJ_EXT         = .o
PLATFORM_TARGETS= 
else
OS=UNIX
PLATFORM_CFLAGS=-Wall
PLATFORM_LINKFLAGS=
PLATFORM_DIR=platform/unix
ifeq (Darwin,$(shell uname))
PLATFORM_ALT_DIR=platform/osx
PLATFORM_CFLAGS+=-I$(PLATFORM_ALT_DIR)
PLATFORM_LINKFLAGS+=-lobjc -framework CoreFoundation -framework Foundation
else
PLATFORM_ALT_DIR=platform/unix
endif
PATH_SEPERATOR=:
EXE_EXT         = 
OBJ_EXT         = .o
PLATFORM_TARGETS=$(NODEMGR)
endif

CXX             = g++
CC              = gcc

OBJ		= obj
BIN		= bin

BIN_NAME        = $(BIN)/wwiv$(EXE_EXT)
INIT    	= $(BIN)/init$(EXE_EXT)
NODEMGR 	= $(BIN)/nodemgr$(EXE_EXT)
FIX             = $(BIN)/fix$(EXE_EXT)
RM		= rm
MKDIR		= mkdir
CFLAGS		= -I$(CURDIR) -Wall -g $(PLATFORM_CFLAGS) -Iplatform -I$(PLATFORM_DIR)
LINKFLAGS	= -lm $(PLATFORM_LINKFLAGS)
PROJECT_DIR	= wwiv50
TAR		= tar czvf 

##############################################################################
#
# Implicit Rules
#
#
#
# This whole substitution bit is because gcc doesn't like files
# with a 'CPP' extension, but it does like 'cpp'
#
#
#.SUFFIXES:
#.SUFFIXES: .o .c .cpp .C .CPP
#

$(OBJ)/%.o : ${PLATFORM_DIR}/%.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.o : ${PLATFORM_DIR}/%.m
	@echo Compiling $<
	@$(CC) -c $< $(CFLAGS) -o $@

$(OBJ)/%.o : ${PLATFORM_ALT_DIR}/%.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.o : ${PLATFORM_ALT_DIR}/%.m
	@echo Compiling $<
	@$(CC) -c $< $(CFLAGS) -o $@

$(OBJ)/%.o : platform/%.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.o : prot/%.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.o : %.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

#############################################################################
#
# Target "ALL" Rebuilds WWIV
#

COMMON_BBS_OBJS =	       	\
	$(OBJ)/asv.o		\
	$(OBJ)/attach.o		\
	$(OBJ)/automsg.o	\
	$(OBJ)/batch.o         	\
	$(OBJ)/bbs.o           	\
	$(OBJ)/bbslist.o	\
	$(OBJ)/bbsovl1.o       	\
	$(OBJ)/bbsovl2.o       	\
	$(OBJ)/bbsovl3.o       	\
	$(OBJ)/bbsutl.o        	\
	$(OBJ)/bbsutl1.o       	\
	$(OBJ)/bbsutl2.o       	\
	$(OBJ)/bgetch.o       	\
	$(OBJ)/bputch.o       	\
	$(OBJ)/callback.o      	\
	$(OBJ)/chains.o		\
	$(OBJ)/chat.o          	\
	$(OBJ)/chnedit.o       	\
	$(OBJ)/colors.o		\
	$(OBJ)/com.o           	\
	$(OBJ)/conf.o          	\
	$(OBJ)/confutil.o	\
	$(OBJ)/connect1.o      	\
	$(OBJ)/crc.o           	\
	$(OBJ)/DateTime.o	\
	$(OBJ)/defaults.o      	\
	$(OBJ)/diredit.o       	\
	$(OBJ)/dirlist.o	\
	$(OBJ)/dropfile.o	\
	$(OBJ)/dupphone.o	\
	$(OBJ)/events.o        	\
	$(OBJ)/exec.o        	\
	$(OBJ)/execexternal.o   \
	$(OBJ)/extract.o	\
	$(OBJ)/FindUser.o       \
	$(OBJ)/gfiles.o        	\
	$(OBJ)/gfledit.o       	\
	$(OBJ)/hop.o      	\
	$(OBJ)/inetmsg.o	\
	$(OBJ)/ini.o           	\
	$(OBJ)/inmsg.o       	\
	$(OBJ)/input.o       	\
	$(OBJ)/instmsg.o       	\
	$(OBJ)/interpret.o      \
	$(OBJ)/lilo.o          	\
	$(OBJ)/listplus.o      	\
	$(OBJ)/lpfunc.o      	\
	$(OBJ)/memory.o		\
	$(OBJ)/menu.o      	\
	$(OBJ)/menuinterpretcommand.o \
	$(OBJ)/menuedit.o      	\
	$(OBJ)/menuspec.o      	\
	$(OBJ)/menusupp.o      	\
	$(OBJ)/msgbase.o	\
	$(OBJ)/msgbase1.o	\
	$(OBJ)/msgscan.o	\
	$(OBJ)/misccmd.o       	\
	$(OBJ)/modem.o         	\
	$(OBJ)/multinst.o      	\
	$(OBJ)/multmail.o      	\
	$(OBJ)/netsup.o        	\
	$(OBJ)/newuser.o       	\
	$(OBJ)/normupld.o       \
	$(OBJ)/pause.o       	\
	$(OBJ)/printfile.o      \
	$(OBJ)/quote.o      	\
	$(OBJ)/readmail.o      	\
	$(OBJ)/reboot.o		\
	$(OBJ)/shortmsg.o	\
	$(OBJ)/showfiles.o	\
	$(OBJ)/SmallRecord.o	\
	$(OBJ)/sr.o            	\
	$(OBJ)/srrcv.o         	\
	$(OBJ)/srsend.o        	\
	$(OBJ)/status.o		\
	$(OBJ)/strings.o       	\
	$(OBJ)/stuffin.o       	\
	$(OBJ)/subacc.o        	\
	$(OBJ)/subedit.o       	\
	$(OBJ)/sublist.o      	\
	$(OBJ)/subreq.o        	\
	$(OBJ)/subxtr.o        	\
	$(OBJ)/sysopf.o        	\
	$(OBJ)/sysoplog.o	\
	$(OBJ)/syschat.o	\
	$(OBJ)/trytoul.o	\
	$(OBJ)/uedit.o         	\
	$(OBJ)/unittests.o	\
	$(OBJ)/user.o		\
	$(OBJ)/utility.o       	\
	$(OBJ)/utility2.o	\
	$(OBJ)/valscan.o      	\
	$(OBJ)/version.o       	\
	$(OBJ)/vote.o		\
	$(OBJ)/voteedit.o      	\
	$(OBJ)/WComm.o		\
	$(OBJ)/wfc.o		\
	$(OBJ)/WFile.o		\
	$(OBJ)/wfndfile.o	\
	$(OBJ)/WLocalIO.o	\
	$(OBJ)/WOutStream.o	\
	$(OBJ)/WOutStreamBuffer.o \
	$(OBJ)/wqscn.o		\
	$(OBJ)/wshare.o		\
	$(OBJ)/WSession.o	\
	$(OBJ)/WStringUtils.o	\
	$(OBJ)/WTextFile.o	\
	$(OBJ)/WUser.o		\
	$(OBJ)/wutil.o         	\
	$(OBJ)/xfer.o          	\
	$(OBJ)/xferovl.o       	\
	$(OBJ)/xferovl1.o      	\
	$(OBJ)/xfertmp.o       	\
	$(OBJ)/xinit.o         	\

# Internal ZModem protocol

COMMON_BBS_OBJS +=	       	\
	$(OBJ)/crctab.o       	\
	$(OBJ)/zmodem.o       	\
	$(OBJ)/zmodemcrc.o    	\
	$(OBJ)/zmodemr.o       	\
	$(OBJ)/zmodemt.o       	\
	$(OBJ)/zmutil.o       	\
	$(OBJ)/zmwwiv.o       	\

PLATFORM_UNIX_OBJS =		\
	$(OBJ)/stringstuff.o	\
	$(OBJ)/filestuff.o	\
	$(OBJ)/Wiou.o		\

PLATFORM_OSX_OBJS =		\
	$(OBJ)/macversioninfo.o	\

PLATFORM_WIN32_OBJS =		\
	$(OBJ)/Wios.o		\
	$(OBJ)/Wiot.o		\
	$(OBJ)/filesupp.o

BBS_OBJS = $(COMMON_BBS_OBJS) $(PLATFORM_$(OS)_OBJS)

ifeq (Darwin,$(shell uname))
BBS_OBJS += $(PLATFORM_OSX_OBJS)
endif

###############################################################################
#
# Makefile Targets
#
#
#

all: $(BIN_NAME) $(INIT) $(FIX) $(PLATFORM_TARGETS)

init: $(INIT)

nodemgr: $(NODEMGR)

fix: $(FIX)

$(OBJ):
	-$(MKDIR) $(OBJ)

$(BIN):
	-$(MKDIR) $(BIN)

$(BIN_NAME): $(BIN) $(OBJ) $(BBS_OBJS)
	@echo Linking $@
	@$(CXX) -o $(BIN_NAME) $(LINKFLAGS) $(BBS_OBJS)

$(INIT):
	@echo Building $@
	@$(MAKE) -C init  --no-print-directory

$(NODEMGR):
	@echo Building $@
	@$(MAKE) -C nodemgr  --no-print-directory

$(FIX):
	@echo Building $@
	@$(MAKE) -C fix --no-print-directory

clean:
	@echo Cleaning...
	-$(RM) -f $(BIN_NAME)
	-$(RM) -f $(BBS_OBJS)
	-$(RM) -f make.log
	-$(RM) -f *~
	@$(MAKE) -C init clean --no-print-directory
	@$(MAKE) -C nodemgr clean --no-print-directory
	@$(MAKE) -C fix clean --no-print-directory

spotless:
	-$(RM) -rf $(BIN)/
	-$(RM) -rf $(OBJ)/
	@$(MAKE) -C nodemgr spotless --no-print-directory
	@$(MAKE) -C fix spotless --no-print-directory

dist: tarball all
ifeq "$(OS)" "UNIX"
	cd $(BIN); strip -s *
endif
	cd $(BIN); $(TAR) wwiv50-bin-`date +%Y%m%d`.tar.gz *

tarball: clean
	cd ..; $(TAR) wwiv50-src-`date +%Y%m%d`.tar.gz $(PROJECT_DIR)/*

cp:
	su -c 'cp $(BIN)/wwiv$(EXE_EXT) ~bbs/'

# Included for convension
distclean: spotless

.PHONY: clean clobberall
