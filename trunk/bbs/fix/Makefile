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
PLATFORM_DIR=../platform/win32
PLATFORM_ALT_DIR=../platform/win32
PLATFORM_LINKFLAGS=
PATH_SEPERATOR=;
EXE_EXT         = .exe
OBJ_EXT         = .o
else
OS=UNIX
PLATFORM_CFLAGS=
PLATFORM_DIR=../platform/unix
PLATFORM_LINKFLAGS=
ifeq (Darwin,$(shell uname))
PLATFORM_ALT_DIR=../platform/osx
PLATFORM_CFLAGS+=-I$(PLATFORM_ALT_DIR)
PLATFORM_LINKFLAGS+=-lobjc -framework CoreFoundation -framework Foundation
else
PLATFORM_ALT_DIR=platform/unix
endif
PATH_SEPERATOR=:
EXE_EXT         = 
OBJ_EXT         = .o
endif

CC              = gcc
CXX             = g++

OBJ		= obj
BIN		= ../bin

BIN_NAME        = $(BIN)/fix$(EXE_EXT)
RM		= rm
MKDIR		= mkdir
CFLAGS		= -I../ -I../platform -I$(PLATFORM_DIR) -Wall -g $(PLATFORM_CFLAGS) -DINIT -DNOT_BBS
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

$(OBJ)/%.o : ../platform/%.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.o : %.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.o : ../%.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

#############################################################################
#
# Target "ALL" Rebuilds WWIV
#

COMMON_BBS_OBJS =	       	\
	$(OBJ)/fix.o      	\
	$(OBJ)/log.o      	\
	$(OBJ)/dirs.o      	\
	$(OBJ)/users.o      	\
	$(OBJ)/DateTime.o      	\
	$(OBJ)/wfndfile.o      	\
	$(OBJ)/WFile.o      	\
	$(OBJ)/WUser.o      	\
	$(OBJ)/WStringUtils.o   \
	$(OBJ)/version.o      	\

PLATFORM_UNIX_OBJS =		\
	$(OBJ)/stringstuff.o	\
	$(OBJ)/filestuff.o	\

PLATFORM_WIN32_OBJS =		\
	$(OBJ)/filesupp.o

BBS_OBJS = $(COMMON_BBS_OBJS) $(PLATFORM_$(OS)_OBJS)
BBS_SOURCE =	$(patsubst %.o, %.cpp, $(BBS_OBJS))

###############################################################################
#
# Makefile Targets
#
#
#

all: $(BIN_NAME)

$(OBJ):
	-$(MKDIR) $(OBJ)

$(BIN):
	-$(MKDIR) $(BIN)

$(BIN_NAME): $(BIN) $(OBJ) $(BBS_OBJS)
	@echo Linking $@
	@$(CXX) -o $(BIN_NAME) $(LINKFLAGS) $(BBS_OBJS)

clean:
	@echo Cleaning...
	-$(RM) -f $(BIN_NAME)
	-$(RM) -f $(BBS_OBJS)
	-$(RM) -f make.log
	-$(RM) -f *~

spotless:
	-$(RM) -rf $(BIN)/
	-$(RM) -rf $(OBJ)/

