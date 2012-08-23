
###ifneq (,$(findstring win32,$(MAKEFLAGS)))
###OS=WIN32
###PLATFORM_CFLAGS=
###PLATFORM_DIR=../platform/WIN32
###PATH_SEPERATOR=;
###EXE_EXT         = .exe
###OBJ_EXT         = obj
###TEXTUI_LIBRARY  = $(CURDIR)/../../textui/bin/textui.lib
###else
OS=UNIX
PLATFORM_CFLAGS=-Wall
PLATFORM_DIR=../platform/unix
PLATFORM_ALT_DIR=../platform/unix
###ifeq (Darwin,$(shell uname))
###PLATFORM_ALT_DIR=../platform/osx
###PLATFORM_CFLAGS+=-I$(PLATFORM_ALT_DIR)
###PLATFORM_LINKFLAGS+=-lobjc -framework CoreFoundation -framework Foundation
###else
###PLATFORM_ALT_DIR=platform/unix
###endif
PATH_SEPERATOR=:
EXE_EXT         = 
OBJ_EXT         = o
TEXTUI_LIBRARY  = $(CURDIR)/../../textui/bin/libtextui.a
###endif

OBJ		= obj
BIN		= ../bin
INIT_APP        = $(BIN)/init
CC              = gcc
CXX             = g++
CFLAGS          = -gdwarf-2 -g3 -I$(CURDIR)/../../textui/include -I$(CURDIR)/../ -I$(CURDIR)/../platform -DINIT $(PLATFORM_CFLAGS)
LINKFLAGS       = $(PLATFORM_LINKFLAGS) -lcurses

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
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.$(OBJ_EXT) : ../%.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.$(OBJ_EXT) : ../platform/%.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.$(OBJ_EXT) : $(PLATFORM_DIR)/%.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.$(OBJ_EXT) : $(PLATFORM_DIR)/%.m
	@echo Compiling $<
	@$(CC) -c $< $(CFLAGS) -o $@

$(OBJ)/%.$(OBJ_EXT) : $(PLATFORM_ALT_DIR)/%.cpp
	@echo Compiling $<
	@$(CXX) -c $< $(CFLAGS) -o $@

$(OBJ)/%.$(OBJ_EXT) : $(PLATFORM_ALT_DIR)/%.m
	@echo Compiling $<
	@$(CC) -c $< $(CFLAGS) -o $@

#############################################################################
#
# Target "ALL" Rebuilds WWIV
#

INIT_OBJS =	       				\
	$(OBJ)/main.$(OBJ_EXT)			\
	$(OBJ)/sys_paths.$(OBJ_EXT)		\
	$(OBJ)/sys_general.$(OBJ_EXT)		\
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
		@$(CXX) $(LINKFLAGS)  -o $(INIT_APP) $(INIT_OBJS) $(TEXTUI_LIBRARY) -lcurses
