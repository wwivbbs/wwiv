OBJ		= obj
BIN		= ../bin
INIT_APP        = $(BIN)/init
CC              = g++
CFLAGS          = -I$(CURDIR)/../../textui/include -I$(CURDIR)/../ -I$(CURDIR)/../platform -DINIT

ifneq (,$(findstring win32,$(MAKEFLAGS)))
OS=WIN32
PLATFORM_CFLAGS=
PLATFORM_DIR=../platform/WIN32
PATH_SEPERATOR=;
EXE_EXT         = .exe
OBJ_EXT         = obj
PLATFORM_TARGETS= 
TEXTUI_LIBRARY  = $(CURDIR)/../../textui/bin/textui.lib
else
OS=UNIX
# no need to define _UNIX since platform/testos.h does it now if "linux" is defined
PLATFORM_CFLAGS=-Wall
PLATFORM_DIR=../platform/linux
PATH_SEPERATOR=:
EXE_EXT         = 
OBJ_EXT         = o
PLATFORM_TARGETS=$(NODEMGR)
TEXTUI_LIBRARY  = $(CURDIR)/../../textui/bin/libtextui.a
endif

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

$(OBJ)/%.$(OBJ_EXT) : %.cpp
	@echo Compiling $<
	@$(CC) -c $< $(CFLAGS) -o $@

$(OBJ)/%.$(OBJ_EXT) : ../%.cpp
	@echo Compiling $<
	@$(CC) -c $< $(CFLAGS) -o $@

$(OBJ)/%.$(OBJ_EXT) : ../platform/%.cpp
	@echo Compiling $<
	@$(CC) -c $< $(CFLAGS) -o $@

$(OBJ)/%.$(OBJ_EXT) : $(PLATFORM_DIR)/%.cpp
	@echo Compiling $<
	@$(CC) -c $< $(CFLAGS) -o $@

#############################################################################
#
# Target "ALL" Rebuilds WWIV
#

INIT_OBJS =	       				\
	$(OBJ)/main.$(OBJ_EXT)			\
	$(OBJ)/sys_paths.$(OBJ_EXT)		\
	$(OBJ)/bbsfuncs.$(OBJ_EXT)		\
	$(OBJ)/WFile.$(OBJ_EXT)			\
	$(OBJ)/WStringUtils.$(OBJ_EXT)		\
	$(OBJ)/wfndfile.$(OBJ_EXT)		\
	$(OBJ)/wshare.$(OBJ_EXT)		\
	$(OBJ)/version.$(OBJ_EXT)		\

all: $(OBJ) $(BIN) $(INIT_APP)

clean:
	@echo Cleaning...
	-rm -f $(INIT_APP)
	-rm -f $(INIT_OBJS)

$(OBJ) $(BIN):
	-mkdir $@

$(TEXTUI_LIBRARY):
	$(MAKE) -C $(CURDIR)/../../textui all

$(INIT_APP): $(INIT_OBJS) $(TEXTUI_LIBRARY)
	@echo Linking $(INIT_APP)
	$(CC) -o $(INIT_APP) $(INIT_OBJS) $(TEXTUI_LIBRARY) -lcurses
