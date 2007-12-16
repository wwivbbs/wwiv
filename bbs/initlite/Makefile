##############################################################################
#
# WWIV 5.0 INIT LITE MAKEFILE
# Copyright (c) 2000 WWIV Software Services
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



ifneq (,$(findstring win32,$(MAKEFLAGS)))
OS=WIN32
PLATFORM_CFLAGS=
PLATFORM_DIRS=../platform/WIN32
PATH_SEPERATOR=;
EXE_EXT		= .exe
OBJ_EXT		= .o
else
OS=UNIX
PLATFORM_CFLAGS=
PLATFORM_DIRS=../platform/linux
PATH_SEPERATOR=:
EXE_EXT		= 
OBJ_EXT		= .o
endif


CC              = g++

OBJ		= ../obj
BIN		= ../bin

BIN_NAME        = $(BIN)/initlite$(EXE_EXT)
RM		= rm
MKDIR		= mkdir
CFLAGS		= -Wall -g $(PLATFORM_CFLAGS)
LINKFLAGS	= -lm -lncurses
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

$(OBJ)/%.o : %.cpp
	@echo Compiling $<
	@$(CC) -c $< $(CFLAGS) -o $@

#############################################################################
#
# Target "ALL" Rebuilds WWIV
#

BBS_OBJS =		\
	$(OBJ)/initlite.o

BBS_SOURCE =	$(patsubst %.o, %.cpp, $(BBS_OBJS))


###############################################################################
#
# Makefile Targets
#
#
#


all: $(BIN_NAME)

$(OBJ):
	-$(MKDIR) $@

$(BIN):
	-$(MKDIR) $@

$(BIN_NAME): $(BIN) $(OBJ) $(BBS_OBJS)
	@echo Linking $@
	@$(CC) -o $(BIN_NAME) $(LINKFLAGS) $(BBS_OBJS)

clean:
	@echo Cleaning initlite...
	-$(RM) -f $(BIN_NAME)
	-$(RM) -f $(BBS_OBJS)
	-$(RM) -f make.log
	-$(RM) -f *~

spotless:
	-$(RM) -rf $(BIN)/
	-$(RM) -rf $(OBJ)/

tarball:
	cd ..; make tarball

cp:
	su -c 'cp $(BIN)/initlite$(EXE_EXT) ~bbs/'

# Included for convension
distclean: spotless

.PHONY: clean clobberall 


